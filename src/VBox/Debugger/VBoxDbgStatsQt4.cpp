/* $Id$ */
/** @file
 * VBox Debugger GUI - Statistics.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGG
#include "VBoxDbgStatsQt4.h"

#include <QLocale>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QClipboard>
#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/assert.h>


#include <stdio.h> //remove me


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The number of column. */
#define DBGGUI_STATS_COLUMNS    9



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The state of a statistics sample node.
 *
 * This is used for two pass refresh (1. get data, 2. update the view) and
 * for saving the result of a diff.
 */
typedef enum DBGGUISTATSNODESTATE
{
    /** The typical invalid zeroth entry. */
    kDbgGuiStatsNodeState_kInvalid = 0,
    /** The node is the root node. */
    kDbgGuiStatsNodeState_kRoot,
    /** The node is visible. */
    kDbgGuiStatsNodeState_kVisible,
    /** The node is invisible. */
    kDbgGuiStatsNodeState_kInvisible,
    /** The node should be made visble. */
    kDbgGuiStatsNodeState_kMakeVisible,
    /** The node should be made invisble. */
    kDbgGuiStatsNodeState_kMakeInvisible,
    /** The node should be refreshed. */
    kDbgGuiStatsNodeState_kRefresh,
    /** diff: The node equals. */
    kDbgGuiStatsNodeState_kDiffEqual,
    /** diff: The node in set 1 is less than the one in set 2. */
    kDbgGuiStatsNodeState_kDiffSmaller,
    /** diff: The node in set 1 is greater than the one in set 2. */
    kDbgGuiStatsNodeState_kDiffGreater,
    /** diff: The node is only in set 1. */
    kDbgGuiStatsNodeState_kDiffOnlyIn1,
    /** diff: The node is only in set 2. */
    kDbgGuiStatsNodeState_kDiffOnlyIn2,
    /** The end of the valid state values. */
    kDbgGuiStatsNodeState_kEnd
} DBGGUISTATENODESTATE;


/**
 * A tree node representing a statistic sample.
 *
 * The nodes carry a reference to the parent and to its position among its
 * siblings. Both of these need updating when the grand parent or parent adds a
 * new child. This will hopefully not be too expensive but rather pay off when
 * we need to create a parent index.
 */
typedef struct DBGGUISTATSNODE
{
    /** Pointer to the parent. */
    PDBGGUISTATSNODE        pParent;
    /** Array of pointers to the child nodes. */
    PDBGGUISTATSNODE       *papChildren;
    /** The number of children. */
    uint32_t                cChildren;
    /** Our index among the parent's children. */
    uint32_t                iSelf;
    /** The unit. */
    STAMUNIT                enmUnit;
    /** The data type. */
    STAMTYPE                enmType;
    /** The data at last update. */
    union
    {
        /** STAMTYPE_COUNTER. */
        STAMCOUNTER         Counter;
        /** STAMTYPE_PROFILE. */
        STAMPROFILE         Profile;
        /** STAMTYPE_PROFILE_ADV. */
        STAMPROFILEADV      ProfileAdv;
        /** STAMTYPE_RATIO_U32. */
        STAMRATIOU32        RatioU32;
        /** STAMTYPE_U8 & STAMTYPE_U8_RESET. */
        uint8_t             u8;
        /** STAMTYPE_U16 & STAMTYPE_U16_RESET. */
        uint16_t            u16;
        /** STAMTYPE_U32 & STAMTYPE_U32_RESET. */
        uint32_t            u32;
        /** STAMTYPE_U64 & STAMTYPE_U64_RESET. */
        uint64_t            u64;
        /** STAMTYPE_CALLBACK - buffer of 256 bytes. */
        char               *psz;
    } Data;
    /** The name. */
    char                   *pszName;
    /** The description string. */
    QString                *pDescStr;
    /** The node state. */
    DBGGUISTATENODESTATE    enmState;
    /** The delta. */
    char                    szDelta[32];
} DBGGUISTATSNODE;


/**
 * Recursion stack.
 */
typedef struct DBGGUISTATSSTACK
{
    /** The top stack entry. */
    int32_t iTop;
    /** The stack array. */
    struct DBGGUISTATSSTACKENTRY
    {
        /** The node. */
        PDBGGUISTATSNODE    pNode;
        /** The current child. */
        int32_t             iChild;
    } a[32];
} DBGGUISTATSSTACK;




/**
 * The item model for the statistics tree view.
 *
 * This manages the DBGGUISTATSNODE trees.
 */
class VBoxDbgStatsModel : public QAbstractItemModel
{
protected:
    /** The root of the sample tree. */
    PDBGGUISTATSNODE m_pRoot;

public:
    /**
     * Constructor.
     *
     * @param   a_pParent       The parent object. See QAbstractItemModel in the Qt
     *                          docs for details.
     */
    VBoxDbgStatsModel(QObject *a_pParent);

    /**
     * Destructor.
     *
     * This will free all the the data the model holds.
     */
    virtual ~VBoxDbgStatsModel();

    /**
     * Updates the data matching the specified pattern.
     *
     * Nodes not matched by the pattern will become invisible.
     *
     * @param   a_rPatStr       The selection pattern.
     */
    virtual void update(const QString &a_rPatStr) = 0;

    /**
     * Gets the model index of the root node.
     *
     * @returns root index.
     */
    QModelIndex getRootIndex(void) const;

protected:
    /**
     * Set the root node.
     *
     * This will free all the current data before taking the ownership of the new
     * root node and its children.
     *
     * @param   a_pRoot         The new root node.
     */
    void setRootNode(PDBGGUISTATSNODE a_pRoot);

    /** Creates the root node. */
    static PDBGGUISTATSNODE createRootNode(void);

    /** Creates and insert a node under the given parent. */
    static PDBGGUISTATSNODE createAndInsertNode(PDBGGUISTATSNODE pParent, const char *pszName, size_t cchName, uint32_t iPosition = UINT32_MAX);

    /**
     * Destroys a statistics tree.
     *
     * @param   a_pRoot         The root of the tree. NULL is fine.
     */
    static void destroyTree(PDBGGUISTATSNODE a_pRoot);

    /**
     * Stringifies exactly one node, no children.
     *
     * This is for logging and clipboard.
     *
     * @param   a_pNode     The node.
     * @param   a_rString   The string to append the strigified node to.
     */
    static void stringifyNodeNoRecursion(PDBGGUISTATSNODE a_pNode, QString &a_rString);

    /**
     * Stringifies a node and its children.
     *
     * This is for logging and clipboard.
     *
     * @param   a_pNode     The node.
     * @param   a_rString   The string to append the strigified node to.
     */
    static void stringifyNode(PDBGGUISTATSNODE a_pNode, QString &a_rString);

public:
    /**
     * Converts the specified tree to string.
     *
     * This is for logging and clipboard.
     *
     * @param   a_rRoot         Where to start. Use QModelIndex() to start at the root.
     * @param   a_rString       Where to return to return the string dump.
     */
    void stringifyTree(QModelIndex &a_rRoot, QString &a_rString) const;

    /**
     * Dumps the given (sub-)tree as XML.
     *
     * @param   a_rRoot         Where to start. Use QModelIndex() to start at the root.
     * @param   a_rString       Where to return to return the XML dump.
     */
    void xmlifyTree(QModelIndex &a_rRoot, QString &a_rString) const;

    /**
     * Puts the stringified tree on the clipboard.
     *
     * @param   a_rRoot         Where to start. Use QModelIndex() to start at the root.
     */
    void copyTreeToClipboard(QModelIndex &a_rRoot) const;


protected:
    /** Worker for logTree. */
    static void logNode(PDBGGUISTATSNODE a_pNode, bool a_fReleaseLog);

public:
    /** Logs a (sub-)tree.
     *
     * @param   a_rRoot         Where to start. Use QModelIndex() to start at the root.
     * @param   a_fReleaseLog   Whether to use the release log (true) or the debug log (false).
     */
    void logTree(QModelIndex &a_rRoot, bool a_fReleaseLog) const;

protected:
    /** Gets the unit. */
    static QString strUnit(PCDBGGUISTATSNODE pNode);
    /** Gets the value. */
    static QString strValue(PCDBGGUISTATSNODE pNode);

    /**
     * Destroys a node and all its children.
     *
     * @param   a_pNode         The node to destroy.
     */
    static void destroyNode(PDBGGUISTATSNODE a_pNode);

    /**
     * Converts an index to a node pointer.
     *
     * @returns Pointer to the node, NULL if invalid reference.
     * @param   a_rIndex        Reference to the index
     */
    inline PDBGGUISTATSNODE nodeFromIndex(const QModelIndex &a_rIndex) const
    {
        if (RT_LIKELY(a_rIndex.isValid()))
            return (PDBGGUISTATSNODE)a_rIndex.internalPointer();
        return NULL;
    }

public:

    /** @name Overriden QAbstractItemModel methods
     * @{ */
    virtual int columnCount(const QModelIndex &a_rParent) const;
    virtual QVariant data(const QModelIndex &a_rIndex, int a_eRole) const;
    virtual Qt::ItemFlags flags(const QModelIndex &a_rIndex) const;
    virtual bool hasChildren(const QModelIndex &a_rParent) const;
    virtual QVariant headerData(int a_iSection, Qt::Orientation a_ePrientation, int a_eRole) const;
    virtual QModelIndex index(int a_iRow, int a_iColumn, const QModelIndex &a_rParent) const;
    virtual QModelIndex parent(const QModelIndex &a_rChild) const;
    virtual int rowCount(const QModelIndex &a_rParent) const;
    ///virtual void sort(int a_iColumn, Qt::SortOrder a_eOrder);
    /** @}  */
};


/**
 * Model using the VM / STAM interface as data source.
 */
class VBoxDbgStatsModelVM : public VBoxDbgStatsModel, public VBoxDbgBase
{
public:
    /**
     * Constructor.
     *
     * @param   a_pVM           The VM handle.
     * @param   a_pParent       The parent object. NULL is fine.
     */
    VBoxDbgStatsModelVM(PVM a_pVM, QObject *a_pParent);

    /** Destructor */
    virtual ~VBoxDbgStatsModelVM();

    /**
     * Updates the data matching the specified pattern.
     *
     * Nodes not matched by the pattern will become invisible.
     *
     * @param   a_rPatStr       The selection pattern.
     */
    virtual void update(const QString &a_rPatStr);

private:
    /**
     * Enumeration callback used by update.
     */
    static DECLCALLBACK(int) updateCallback(const char *pszName, STAMTYPE enmType, void *pvSample, STAMUNIT enmUnit,
                                            STAMVISIBILITY enmVisibility, const char *pszDesc, void *pvUser);

    /**
     * Initializes a new node.
     */
    static int initNode(PDBGGUISTATSNODE pNode, STAMTYPE enmType, void *pvSample, STAMUNIT enmUnit, STAMVISIBILITY enmVisibility, const char *pszDesc);

    /**
     * Enumeration callback used by createNewTree.
     */
    static DECLCALLBACK(int) createNewTreeCallback(const char *pszName, STAMTYPE enmType, void *pvSample, STAMUNIT enmUnit,
                                                   STAMVISIBILITY enmVisibility, const char *pszDesc, void *pvUser);

    /**
     * Constructs a new statistics tree by query data from the VM.
     *
     * @returns Pointer to the root of the tree we've constructed. This will be NULL
     *          if the STAM API throws an error or we run out of memory.
     */
    PDBGGUISTATSNODE createNewTree(void);
};


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/


/**
 * Formats a number into a 64-byte buffer.
 */
static char *formatNumber(char *psz, uint64_t u64)
{
    static const char s_szDigits[] = "0123456789";
    psz += 63;
    *psz-- = '\0';
    unsigned cDigits = 0;
    for (;;)
    {
        const unsigned iDigit = u64 % 10;
        u64 /= 10;
        *psz = s_szDigits[iDigit];
        if (!u64)
            break;
        psz--;
        if (!(++cDigits % 3))
            *psz-- = ',';
    }
    return psz;
}


/**
 * Formats a number into a 64-byte buffer.
 * (18.446.744.073.709.551.615)
 */
static char *formatNumberSigned(char *psz, int64_t i64)
{
    static const char s_szDigits[] = "0123456789";
    psz += 63;
    *psz-- = '\0';
    const bool fNegative = i64 < 0;
    uint64_t u64 = fNegative ? -i64 : i64;
    unsigned cDigits = 0;
    for (;;)
    {
        const unsigned iDigit = u64 % 10;
        u64 /= 10;
        *psz = s_szDigits[iDigit];
        if (!u64)
            break;
        psz--;
        if (!(++cDigits % 3))
            *psz-- = ',';
    }
    if (fNegative)
        *--psz = '-';
    return psz;
}


/**
 * Formats a unsigned hexadecimal number into a into a 64-byte buffer.
 */
static char *formatHexNumber(char *psz, uint64_t u64, unsigned cZeros)
{
    static const char s_szDigits[] = "0123456789abcdef";
    psz += 63;
    *psz-- = '\0';
    unsigned cDigits = 0;
    for (;;)
    {
        const unsigned iDigit = u64 % 16;
        u64 /= 16;
        *psz = s_szDigits[iDigit];
        ++cDigits;
        if (!u64 && cDigits >= cZeros)
            break;
        psz--;
        if (!(cDigits % 8))
            *psz-- = '\'';
    }
    return psz;
}


/**
 * Formats a sort key number.
 */
static void formatSortKey(char *psz, uint64_t u64)
{
    static const char s_szDigits[] = "0123456789abcdef";
    /* signed */
    *psz++ = '+';

    /* 16 hex digits */
    psz[16] = '\0';
    unsigned i = 16;
    while (i-- > 0)
    {
        if (u64)
        {
            const unsigned iDigit = u64 % 16;
            u64 /= 16;
            psz[i] = s_szDigits[iDigit];
        }
        else
            psz[i] = '0';
    }
}


#if 0/* unused */
/**
 * Formats a sort key number.
 */
static void formatSortKeySigned(char *psz, int64_t i64)
{
    static const char s_szDigits[] = "0123456789abcdef";

    /* signed */
    uint64_t u64;
    if (i64 >= 0)
    {
        *psz++ = '+';
        u64 = i64;
    }
    else
    {
        *psz++ = '-';
        u64 = -i64;
    }

    /* 16 hex digits */
    psz[16] = '\0';
    unsigned i = 16;
    while (i-- > 0)
    {
        if (u64)
        {
            const unsigned iDigit = u64 % 16;
            u64 /= 16;
            psz[i] = s_szDigits[iDigit];
        }
        else
            psz[i] = '0';
    }
}
#endif



/*
 *
 *      V B o x D b g S t a t s M o d e l
 *      V B o x D b g S t a t s M o d e l
 *      V B o x D b g S t a t s M o d e l
 *
 *
 */


VBoxDbgStatsModel::VBoxDbgStatsModel(QObject *a_pParent)
    : QAbstractItemModel(a_pParent), m_pRoot(NULL)
{
}



VBoxDbgStatsModel::~VBoxDbgStatsModel()
{
    destroyTree(m_pRoot);
    m_pRoot = NULL;
}


/*static*/ void
VBoxDbgStatsModel::destroyTree(PDBGGUISTATSNODE a_pRoot)
{
    if (!a_pRoot)
        return;
    Assert(!a_pRoot->pParent);
    Assert(!a_pRoot->iSelf);

    destroyNode(a_pRoot);
}


/* static*/ void
VBoxDbgStatsModel::destroyNode(PDBGGUISTATSNODE a_pNode)
{
    /* destroy all our children */
    uint32_t i = a_pNode->cChildren;
    while (i-- > 0)
    {
        destroyNode(a_pNode->papChildren[i]);
        a_pNode->papChildren[i] = NULL;
    }
    a_pNode->cChildren = 0;

    /* free the resources we're using */
    a_pNode->pParent = NULL;

    RTMemFree(a_pNode->papChildren);
    a_pNode->papChildren = NULL;

    a_pNode->cChildren = 0;
    a_pNode->iSelf = UINT32_MAX;
    a_pNode->enmUnit = STAMUNIT_INVALID;
    a_pNode->enmType = STAMTYPE_INVALID;

    RTMemFree(a_pNode->pszName);
    a_pNode->pszName = NULL;

    if (a_pNode->pDescStr)
    {
        delete a_pNode->pDescStr;
        a_pNode->pDescStr = NULL;
    }

    /* Finally ourselves */
    a_pNode->enmState = kDbgGuiStatsNodeState_kInvalid;
    RTMemFree(a_pNode);
}


/*static*/ PDBGGUISTATSNODE
VBoxDbgStatsModel::createRootNode(void)
{
    PDBGGUISTATSNODE pRoot = (PDBGGUISTATSNODE)RTMemAllocZ(sizeof(DBGGUISTATSNODE));
    if (!pRoot)
        return NULL;
    pRoot->iSelf = 0;
    pRoot->enmType = STAMTYPE_INVALID;
    pRoot->enmUnit = STAMUNIT_INVALID;
    pRoot->pszName = (char *)RTMemDup("/", sizeof("/"));
    pRoot->enmState = kDbgGuiStatsNodeState_kRoot;

    return pRoot;
}


/*static*/ PDBGGUISTATSNODE
VBoxDbgStatsModel::createAndInsertNode(PDBGGUISTATSNODE pParent, const char *pszName, size_t cchName, uint32_t iPosition /*= UINT32_MAX*/)
{
    /*
     * Create it.
     */
    PDBGGUISTATSNODE pNode = (PDBGGUISTATSNODE)RTMemAllocZ(sizeof(DBGGUISTATSNODE));
    if (!pNode)
        return NULL;
    pNode->iSelf = UINT32_MAX;
    pNode->enmType = STAMTYPE_INVALID;
    pNode->enmUnit = STAMUNIT_INVALID;
    pNode->pszName = (char *)RTMemDupEx(pszName, cchName, 1);
    pNode->enmState = kDbgGuiStatsNodeState_kVisible;

    /*
     * Do we need to expand the array?
     */
    if (!(pParent->cChildren & 31))
    {
        void *pvNew = RTMemRealloc(pParent->papChildren, sizeof(*pParent->papChildren) * (pParent->cChildren + 32));
        if (!pvNew)
        {
            destroyNode(pNode);
            return NULL;
        }
        pParent->papChildren = (PDBGGUISTATSNODE *)pvNew;
    }

    /*
     * Insert it.
     */
    pNode->pParent = pParent;
    if (iPosition >= pParent->cChildren)
        /* Last. */
        iPosition = pParent->cChildren;
    else
    {
        /* Shift all the items after ours. */
        uint32_t iShift = pParent->cChildren;
        while (iShift-- > iPosition)
        {
            PDBGGUISTATSNODE pChild = pParent->papChildren[iShift];
            pParent->papChildren[iShift + 1] = pChild;
            pChild->iSelf = iShift + 1;
        }
    }

    /* Insert ours */
    pNode->iSelf = iPosition;
    pParent->papChildren[iPosition] = pNode;
    pParent->cChildren++;

    return pNode;
}


#if 0
/*static*/ PDBGGUISTATSNODE
VBoxDbgStatsModel::createNewTree(IMachineDebugger *a_pIMachineDebugger)
{
    /** @todo  */
    return NULL;
}
#endif


#if 0
/*static*/ PDBGGUISTATSNODE
VBoxDbgStatsModel::createNewTree(const char *pszFilename)
{
    /** @todo  */
    return NULL;
}
#endif


#if 0
/*static*/ PDBGGUISTATSNODE
VBoxDbgStatsModel::createDiffTree(PDBGGUISTATSNODE pTree1, PDBGGUISTATSNODE pTree2)
{
    /** @todo  */
    return NULL;
}
#endif


QModelIndex
VBoxDbgStatsModel::getRootIndex(void) const
{
    if (!m_pRoot)
        return QModelIndex();
    return createIndex(0, 0, m_pRoot);
}


void
VBoxDbgStatsModel::setRootNode(PDBGGUISTATSNODE a_pRoot)
{
    PDBGGUISTATSNODE pOldTree = m_pRoot;
    m_pRoot = a_pRoot;
    destroyTree(pOldTree);
    reset();
}


Qt::ItemFlags
VBoxDbgStatsModel::flags(const QModelIndex &a_rIndex) const
{
    Qt::ItemFlags fFlags = QAbstractItemModel::flags(a_rIndex);

    PDBGGUISTATSNODE pNode = nodeFromIndex(a_rIndex);
    if (pNode)
    {
        if (pNode->enmState == kDbgGuiStatsNodeState_kInvisible)
            fFlags &= ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    }
    return fFlags;
}


int
VBoxDbgStatsModel::columnCount(const QModelIndex &a_rParent) const
{
    PDBGGUISTATSNODE pParent = nodeFromIndex(a_rParent);
    if (    pParent
        &&  pParent->enmState == kDbgGuiStatsNodeState_kInvisible)
        return 0;
    return DBGGUI_STATS_COLUMNS;
}


int
VBoxDbgStatsModel::rowCount(const QModelIndex &a_rParent) const
{
    PDBGGUISTATSNODE pParent = nodeFromIndex(a_rParent);
    if (    !pParent
        ||  pParent->enmState == kDbgGuiStatsNodeState_kInvisible)
        return 0;
    return pParent->cChildren;
}


bool
VBoxDbgStatsModel::hasChildren(const QModelIndex &a_rParent) const
{
    PDBGGUISTATSNODE pParent = nodeFromIndex(a_rParent);
    return pParent
        && pParent->cChildren > 0
        && pParent->enmState != kDbgGuiStatsNodeState_kInvisible;
}


QModelIndex
VBoxDbgStatsModel::index(int iRow, int iColumn, const QModelIndex &r_pParent) const
{
    PDBGGUISTATSNODE pParent = nodeFromIndex(r_pParent);
    if (!pParent)
    {
        printf("index: iRow=%d iColumn=%d invalid parent\n", iRow, iColumn);
        return QModelIndex(); /* bug? */
    }
    if ((unsigned)iRow >= pParent->cChildren)
    {
        printf("index: iRow=%d >= cChildren=%u (iColumn=%d)\n", iRow, (unsigned)pParent->cChildren, iColumn);
        return QModelIndex(); /* bug? */
    }
    if ((unsigned)iColumn >= DBGGUI_STATS_COLUMNS)
    {
        printf("index: iColumn=%d (iRow=%d)\n", iColumn, iRow);
        return QModelIndex(); /* bug? */
    }
    PDBGGUISTATSNODE pChild = pParent->papChildren[iRow];
    if (pChild->enmState == kDbgGuiStatsNodeState_kInvisible)
    {
        printf("index: invisible (iColumn=%d iRow=%d)\n", iColumn, iRow);
        return QModelIndex();
    }

    //printf("index: iRow=%d iColumn=%d %p %s/%s\n", iRow, iColumn, pParent->papChildren[iRow], pParent->pszName, pParent->papChildren[iRow]->pszName);
    return createIndex(iRow, iColumn, pParent->papChildren[iRow]);
}


QModelIndex
VBoxDbgStatsModel::parent(const QModelIndex &a_rChild) const
{
    PDBGGUISTATSNODE pChild = nodeFromIndex(a_rChild);
    if (!pChild)
    {
        printf("parent: invalid child\n");
        return QModelIndex(); /* bug */
    }
    PDBGGUISTATSNODE pParent = pChild->pParent;
    if (!pParent)
    {
        printf("parent: root\n");
        return QModelIndex(); /* we're root */
    }

    //printf("parent: iSelf=%d iColumn=%d %p %s(/%s)\n", pParent->iSelf, a_rChild.column(), pParent, pParent->pszName, pChild->pszName);
    return createIndex(pParent->iSelf, a_rChild.column(), pParent);
}


QVariant
VBoxDbgStatsModel::headerData(int a_iSection, Qt::Orientation a_eOrientation, int a_eRole) const
{
    if (    a_eOrientation != Qt::Horizontal
        ||  a_eRole != Qt::DisplayRole)
        return QVariant();
    switch (a_iSection)
    {
        case 0: return tr("Name");
        case 1: return tr("Unit");
        case 2: return tr("Value/Times");
        case 3: return tr("Min");
        case 4: return tr("Average");
        case 5: return tr("Max");
        case 6: return tr("Total");
        case 7: return tr("dInt");
        case 8: return tr("Description");
        default:
            AssertCompile(DBGGUI_STATS_COLUMNS == 9);
            return QVariant(); /* bug */
    }
}


/*static*/ QString
VBoxDbgStatsModel::strUnit(PCDBGGUISTATSNODE pNode)
{
    if (pNode->enmUnit == STAMUNIT_INVALID)
        return "";
    return STAMR3GetUnit(pNode->enmUnit);
}


/*static*/ QString
VBoxDbgStatsModel::strValue(PCDBGGUISTATSNODE pNode)
{
    char sz[128];

    switch (pNode->enmType)
    {
        case STAMTYPE_COUNTER:
            return formatNumber(sz, pNode->Data.Counter.c);

        case STAMTYPE_PROFILE:
        case STAMTYPE_PROFILE_ADV:
            if (!pNode->Data.Profile.cPeriods)
                return "0";
            return formatNumber(sz, pNode->Data.Profile.cPeriods);

        case STAMTYPE_RATIO_U32:
        case STAMTYPE_RATIO_U32_RESET:
        {
            formatNumber(sz, pNode->Data.RatioU32.u32A);
            char *psz = strchr(sz, '\0');
            *psz++ = ':';
            formatNumber(psz, pNode->Data.RatioU32.u32B);
            return psz;
        }

        case STAMTYPE_CALLBACK:
            return pNode->Data.psz;

        case STAMTYPE_U8:
        case STAMTYPE_U8_RESET:
            return formatNumber(sz, pNode->Data.u8);

        case STAMTYPE_X8:
        case STAMTYPE_X8_RESET:
            return formatHexNumber(sz, pNode->Data.u8, 2);

        case STAMTYPE_U16:
        case STAMTYPE_U16_RESET:
            return formatNumber(sz, pNode->Data.u16);

        case STAMTYPE_X16:
        case STAMTYPE_X16_RESET:
            return formatHexNumber(sz, pNode->Data.u16, 4);

        case STAMTYPE_U32:
        case STAMTYPE_U32_RESET:
            return formatNumber(sz, pNode->Data.u32);

        case STAMTYPE_X32:
        case STAMTYPE_X32_RESET:
            return formatHexNumber(sz, pNode->Data.u32, 8);

        case STAMTYPE_U64:
        case STAMTYPE_U64_RESET:
            return formatNumber(sz, pNode->Data.u64);

        case STAMTYPE_X64:
        case STAMTYPE_X64_RESET:
            return formatHexNumber(sz, pNode->Data.u64, 16);

        default:
            AssertMsgFailed(("%d\n", pNode->enmType));
        case STAMTYPE_INVALID:
            return "";
    }
}


QVariant
VBoxDbgStatsModel::data(const QModelIndex &a_rIndex, int a_eRole) const
{
    unsigned iCol = a_rIndex.column();
    if (    iCol >= DBGGUI_STATS_COLUMNS
        ||  (   a_eRole != Qt::DisplayRole
             /*&& a_eRole != Qt::XxxRole*/) )
        return QVariant();

    PDBGGUISTATSNODE pNode = nodeFromIndex(a_rIndex);
    if (    !pNode
        ||  pNode->enmState == kDbgGuiStatsNodeState_kInvisible)
        return QVariant();

    switch (iCol)
    {
        case 0:
            return QString(pNode->pszName);

        case 1:
            return strUnit(pNode);
        case 2:
            return strValue(pNode);
        case 3:
        case 4:
        case 5:
        case 6:
            return QString("todo"); /** @todo the rest of the data formatting... */
        case 7:
            return pNode->szDelta;
        case 8:
            return pNode->pDescStr ? QString(*pNode->pDescStr) : QString("");

        default:
            return QVariant();
    }
}


/*static*/ void
VBoxDbgStatsModel::stringifyNodeNoRecursion(PDBGGUISTATSNODE a_pNode, QString &a_rString)
{
    NOREF(a_pNode);
    a_rString = "todo";
}


/*static*/ void
VBoxDbgStatsModel::stringifyNode(PDBGGUISTATSNODE a_pNode, QString &a_rString)
{
    /* this node */
    if (!a_rString.isEmpty())
        a_rString += "\n";
    stringifyNodeNoRecursion(a_pNode, a_rString);

    /* the children */
    uint32_t const cChildren = a_pNode->cChildren;
    for (uint32_t i = 0; i < cChildren; i++)
        stringifyNode(a_pNode->papChildren[i], a_rString);
}


void
VBoxDbgStatsModel::stringifyTree(QModelIndex &a_rRoot, QString &a_rString) const
{
    PDBGGUISTATSNODE pRoot = a_rRoot.isValid() ? nodeFromIndex(a_rRoot) : m_pRoot;
    if (pRoot)
        stringifyNode(pRoot, a_rString);
}


void
VBoxDbgStatsModel::copyTreeToClipboard(QModelIndex &a_rRoot) const
{
    QString String;
    stringifyTree(a_rRoot, String);

    QClipboard *pClipboard = QApplication::clipboard();
    if (pClipboard)
        pClipboard->setText(String, QClipboard::Clipboard);
}


/*static*/ void
VBoxDbgStatsModel::logNode(PDBGGUISTATSNODE a_pNode, bool a_fReleaseLog)
{
    /* this node */
    QString SelfStr;
    stringifyNodeNoRecursion(a_pNode, SelfStr);
    QByteArray SelfByteArray = SelfStr.toUtf8();
    if (a_fReleaseLog)
        RTLogRelPrintf("%s\n", SelfByteArray.constData());
    else
        RTLogPrintf("%s\n", SelfByteArray.constData());

    /* the children */
    uint32_t const cChildren = a_pNode->cChildren;
    for (uint32_t i = 0; i < cChildren; i++)
        logNode(a_pNode->papChildren[i], a_fReleaseLog);
}


void
VBoxDbgStatsModel::logTree(QModelIndex &a_rRoot, bool a_fReleaseLog) const
{
    PDBGGUISTATSNODE pRoot = a_rRoot.isValid() ? nodeFromIndex(a_rRoot) : m_pRoot;
    if (pRoot)
        logNode(pRoot, a_fReleaseLog);
}




/*
 *
 *      V B o x D b g S t a t s M o d e l V M
 *      V B o x D b g S t a t s M o d e l V M
 *      V B o x D b g S t a t s M o d e l V M
 *
 *
 */


VBoxDbgStatsModelVM::VBoxDbgStatsModelVM(PVM a_pVM, QObject *a_pParent)
    : VBoxDbgStatsModel(a_pParent), VBoxDbgBase(a_pVM)
{
    /*
     * Create a model containing *all* the STAM entries since this is is
     * faster than adding stuff later. (And who cares about wasing a few MBs anyway...)
     */
    PDBGGUISTATSNODE pTree = createNewTree();
    setRootNode(pTree);
}


VBoxDbgStatsModelVM::~VBoxDbgStatsModelVM()
{
    /* nothing to do here. */
}


/*static*/ DECLCALLBACK(int)
VBoxDbgStatsModelVM::updateCallback(const char *pszName, STAMTYPE enmType, void *pvSample, STAMUNIT enmUnit,
                                    STAMVISIBILITY enmVisibility, const char *pszDesc, void *pvUser)
{
    VBoxDbgStatsModelVM *pThis = (VBoxDbgStatsModelVM *)pvUser;
    Log3(("updateCallback: %s\n", pszName));

    /*
     * Skip the ones which shouldn't be visible in the GUI.
     */
    if (enmVisibility == STAMVISIBILITY_NOT_GUI)
        return 0;

/** @todo a much much faster approach would be to link all items into a doubly chained list. */
#if 0
    /*
     * Locate the node in the tree, creating whatever doesn't exist.
     * Because of strict input ordering we can assume that any parent that is
     * still in an undetermined state should be wing clipped and show no data.
     */
    AssertReturn(*pszName == '/' && pszName[1] != '/', VERR_INTERNAL_ERROR);
    PDBGGUISTATSNODE pNode = pRoot;
    const char *pszCur = pszName + 1;
    while (*pszCur)
    {
        /* find the end of this component. */
        const char *pszNext = strchr(pszCur, '/');
        if (!pszNext)
            pszNext = strchr(pszCur, '\0');
        size_t cchCur = pszNext - pszCur;

        /* look it up, create it if it not found. */


        /* Create it if it doesn't exist (it will be last if it exists). */
        if (    !pNode->cChildren
            ||  strncmp(pNode->papChildren[pNode->cChildren - 1]->pszName, pszCur, cchCur)
            ||  pNode->papChildren[pNode->cChildren - 1]->pszName[cchCur])
        {
            pNode = createAndInsertNode(pNode, pszCur, pszNext - pszCur, UINT32_MAX);
            if (!pNode)
                return VERR_NO_MEMORY;
        }
        else
            pNode = pNode->papChildren[pNode->cChildren - 1];

        /* Advance */
        pszCur = *pszNext ? pszNext + 1 : pszNext;
    }

    /*
     * Save the data.
     */
    return initNode(pNode, enmType, pvSample, enmUnit, enmVisibility, pszDesc);
#endif
#if 0
    /*
     * Advance to the matching item.
     */
    VBoxDbgStatsLeafItem *pCur = pThis->m_pCur;
    while (pCur)
    {
        /*
         * ASSUMES ascending order of STAM items.
         */
        int iDiff = strcmp(pszName, pCur->getName());
        if (!iDiff)
            break;
        if (iDiff > 0)
        {
            /*
             * Removed / filtered out.
             */
            Log2(("updateCallback: %s - filtered out\n", pCur->getName()));
            if (pCur->isVisible())
            {
                pCur->setVisible(false);
                hideParentBranches(pCur);
            }

            pCur = pCur->m_pNext;
        }
        else if (iDiff < 0)
        {
            /*
             * New item, insert before pCur.
             */
            Log2(("updateCallback: %s - new\n", pszName));
            VBoxDbgStatsLeafItem *pNew = new VBoxDbgStatsLeafItem(pszName, pThis->createPath(pszName));
            pNew->m_pNext = pCur;
            pNew->m_pPrev = pCur->m_pPrev;
            if (pNew->m_pPrev)
                pNew->m_pPrev->m_pNext = pNew;
            else
                pThis->m_pHead = pNew;
            pCur->m_pPrev = pNew;
            pCur = pNew;
            Assert(!strcmp(pszName, pCur->getName()));
            break;
        }
    }

    /*
     * End of items, insert it at the tail.
     */
    if (!pCur)
    {
        Log2(("updateCallback: %s - new end\n", pszName));
        pCur = new VBoxDbgStatsLeafItem(pszName, pThis->createPath(pszName));
        pCur->m_pNext = NULL;
        pCur->m_pPrev = pThis->m_pTail;
        if (pCur->m_pPrev)
            pCur->m_pPrev->m_pNext = pCur;
        else
            pThis->m_pHead = pCur;
        pThis->m_pTail = pCur;
    }
    Assert(pThis->m_pHead);
    Assert(pThis->m_pTail);

    /*
     * Update it and move on.
     */
    if (!pCur->isVisible())
        showParentBranches(pCur);
    pCur->update(enmType, pvSample, enmUnit, enmVisibility, pszDesc);
    pThis->m_pCur = pCur->m_pNext;

#endif
    return VINF_SUCCESS;
}


void
VBoxDbgStatsModelVM::update(const QString &a_rPatStr)
{
    /*
     * Mark all nodes that aren't yet invisble as invisible.
     * (Depth first)
     */
    DBGGUISTATSSTACK    Stack;
    Stack.a[0].pNode = m_pRoot;
    Stack.a[0].iChild = -1;
    Stack.iTop = 0;

    while (Stack.iTop >= 0)
    {
        /* get top element */
        PDBGGUISTATSNODE pNode  = Stack.a[Stack.iTop].pNode;
        uint32_t         iChild = ++Stack.a[Stack.iTop].iChild;
        if (iChild < pNode->cChildren)
        {
            /* push */
            Stack.iTop++;
            Assert(Stack.iTop < (int32_t)RT_ELEMENTS(Stack.a));
            Stack.a[Stack.iTop].pNode = pNode->papChildren[iChild];
            Stack.a[Stack.iTop].iChild = 0;
        }
        else
        {
            /* pop */
            Stack.iTop--;

            /* do the actual work. */
            switch (pNode->enmState)
            {
                case kDbgGuiStatsNodeState_kVisible:
                    pNode->enmState = kDbgGuiStatsNodeState_kMakeInvisible;
                    break;
                case kDbgGuiStatsNodeState_kInvisible:
                case kDbgGuiStatsNodeState_kRoot:
                    break;
                default:
                    AssertMsgFailedBreak(("%d\n", pNode->enmState));
            }
        }
    }


    /*
     * Perform the update.
     */
    stamEnum(a_rPatStr, updateCallback, this);


    /*
     * Send dataChanged events.
     */
    Stack.a[0].pNode = m_pRoot;
    Stack.a[0].iChild = -1;
    Stack.iTop = 0;

    while (Stack.iTop >= 0)
    {
        /* get top element */
        PDBGGUISTATSNODE pNode  = Stack.a[Stack.iTop].pNode;
        uint32_t         iChild = ++Stack.a[Stack.iTop].iChild;
        if (iChild < pNode->cChildren)
        {
            /* push */
            Stack.iTop++;
            Assert(Stack.iTop < (int32_t)RT_ELEMENTS(Stack.a));
            Stack.a[Stack.iTop].pNode = pNode->papChildren[iChild];
            Stack.a[Stack.iTop].iChild = 0;
        }
        else
        {
            /* pop */
            Stack.iTop--;

            /* do the actual work. */
            iChild = 0;
            while (iChild < pNode->cChildren)
            {
                /* skip to the first needing updating. */
                while (     iChild < pNode->cChildren
                       &&   (   pNode->papChildren[iChild]->enmState != kDbgGuiStatsNodeState_kMakeVisible
                             && pNode->papChildren[iChild]->enmState != kDbgGuiStatsNodeState_kMakeInvisible
                             && pNode->papChildren[iChild]->enmState != kDbgGuiStatsNodeState_kRefresh))
                    iChild++;
                if (iChild >= pNode->cChildren)
                    break;
                QModelIndex TopLeft = createIndex(iChild, 0, pNode->papChildren[iChild]);

                /* collect any subsequent nodes that also needs refreshing. */
                while (++iChild < pNode->cChildren)
                {
                    DBGGUISTATENODESTATE enmState = pNode->papChildren[iChild]->enmState;
                    if (    enmState == kDbgGuiStatsNodeState_kRefresh
                        ||  enmState == kDbgGuiStatsNodeState_kMakeVisible)
                        enmState = kDbgGuiStatsNodeState_kVisible;
                    else if (enmState == kDbgGuiStatsNodeState_kMakeInvisible)
                        enmState = kDbgGuiStatsNodeState_kInvisible;
                    else
                        break;
                    pNode->papChildren[iChild]->enmState = enmState;
                }
                QModelIndex BottomRight = createIndex(iChild - 1, DBGGUI_STATS_COLUMNS - 1, pNode->papChildren[iChild - 1]);

                /* emit the refresh signal */
                emit dataChanged(TopLeft, BottomRight);
            }
        }
    }
}


/*static*/ int
VBoxDbgStatsModelVM::initNode(PDBGGUISTATSNODE pNode, STAMTYPE enmType, void *pvSample, STAMUNIT enmUnit, STAMVISIBILITY enmVisibility, const char *pszDesc)
{
    /*
     * Copy the data.
     */
    bool fVisible = true;
    pNode->enmUnit = enmUnit;
    Assert(pNode->enmType == STAMTYPE_INVALID);
    pNode->enmType = enmType;
    if (pszDesc)
        pNode->pDescStr = new QString(pszDesc); /* ignore allocation failure (well, at least up to the point we can ignore it) */

    switch (enmType)
    {
        case STAMTYPE_COUNTER:
            pNode->Data.Counter = *(PSTAMCOUNTER)pvSample;
            fVisible = enmVisibility != STAMVISIBILITY_NOT_GUI
                    && (enmVisibility == STAMVISIBILITY_ALWAYS || pNode->Data.Counter.c);
            break;

        case STAMTYPE_PROFILE:
        case STAMTYPE_PROFILE_ADV:
            pNode->Data.Profile = *(PSTAMPROFILE)pvSample;
            fVisible = enmVisibility != STAMVISIBILITY_NOT_GUI
                    && (enmVisibility == STAMVISIBILITY_ALWAYS || pNode->Data.Profile.cPeriods);
            break;

        case STAMTYPE_RATIO_U32:
        case STAMTYPE_RATIO_U32_RESET:
            pNode->Data.RatioU32 = *(PSTAMRATIOU32)pvSample;
            fVisible = enmVisibility != STAMVISIBILITY_NOT_GUI
                    && (enmVisibility == STAMVISIBILITY_ALWAYS || pNode->Data.RatioU32.u32A || pNode->Data.RatioU32.u32B);
            break;

        case STAMTYPE_CALLBACK:
        {
            const char *pszString = (const char *)pvSample;
            size_t cchString = strlen(pszString);
            pNode->Data.psz = (char *)RTMemAlloc(256);
            if (pNode->Data.psz)
                return VERR_NO_MEMORY;
            memcpy(pNode->Data.psz, pszString, RT_MIN(256, cchString + 1));

            fVisible = enmVisibility != STAMVISIBILITY_NOT_GUI
                    && (enmVisibility == STAMVISIBILITY_ALWAYS || cchString);
            break;
        }

        case STAMTYPE_U8:
        case STAMTYPE_U8_RESET:
        case STAMTYPE_X8:
        case STAMTYPE_X8_RESET:
            pNode->Data.u8 = *(uint8_t *)pvSample;
            fVisible = enmVisibility != STAMVISIBILITY_NOT_GUI
                    && (enmVisibility == STAMVISIBILITY_ALWAYS || pNode->Data.u8);
            break;

        case STAMTYPE_U16:
        case STAMTYPE_U16_RESET:
        case STAMTYPE_X16:
        case STAMTYPE_X16_RESET:
            pNode->Data.u16 = *(uint16_t *)pvSample;
            fVisible = enmVisibility != STAMVISIBILITY_NOT_GUI
                    && (enmVisibility == STAMVISIBILITY_ALWAYS || pNode->Data.u16);
            break;

        case STAMTYPE_U32:
        case STAMTYPE_U32_RESET:
        case STAMTYPE_X32:
        case STAMTYPE_X32_RESET:
            pNode->Data.u32 = *(uint32_t *)pvSample;
            fVisible = enmVisibility != STAMVISIBILITY_NOT_GUI
                    && (enmVisibility == STAMVISIBILITY_ALWAYS || pNode->Data.u32);
            break;

        case STAMTYPE_U64:
        case STAMTYPE_U64_RESET:
        case STAMTYPE_X64:
        case STAMTYPE_X64_RESET:
            pNode->Data.u64 = *(uint64_t *)pvSample;
            fVisible = enmVisibility != STAMVISIBILITY_NOT_GUI
                    && (enmVisibility == STAMVISIBILITY_ALWAYS || pNode->Data.u64);
            break;

        default:
            AssertMsgFailed(("%d\n", enmType));
            break;
    }

    /*
     * Set the state according to the visibility.
     */
    pNode->enmState = fVisible
                    ? kDbgGuiStatsNodeState_kVisible
                    : kDbgGuiStatsNodeState_kInvisible;
    return VINF_SUCCESS;
}


/*static*/ DECLCALLBACK(int)
VBoxDbgStatsModelVM::createNewTreeCallback(const char *pszName, STAMTYPE enmType, void *pvSample, STAMUNIT enmUnit,
                                           STAMVISIBILITY enmVisibility, const char *pszDesc, void *pvUser)
{
    PDBGGUISTATSNODE pRoot = (PDBGGUISTATSNODE)pvUser;
    Log3(("createNewTreeCallback: %s\n", pszName));

    /*
     * Skip the ones which shouldn't be visible in the GUI.
     */
    if (enmVisibility == STAMVISIBILITY_NOT_GUI)
        return 0;

    /*
     * Perform a mkdir -p like operation till we've walked / created the entire path down
     * to the node specfied node. Remember the last node as that will be the one we will
     * stuff the data into.
     */
    AssertReturn(*pszName == '/' && pszName[1] != '/', VERR_INTERNAL_ERROR);
    PDBGGUISTATSNODE pNode = pRoot;
    const char *pszCur = pszName + 1;
    while (*pszCur)
    {
        /* find the end of this component. */
        const char *pszNext = strchr(pszCur, '/');
        if (!pszNext)
            pszNext = strchr(pszCur, '\0');
        size_t cchCur = pszNext - pszCur;

        /* Create it if it doesn't exist (it will be last if it exists). */
        if (    !pNode->cChildren
            ||  strncmp(pNode->papChildren[pNode->cChildren - 1]->pszName, pszCur, cchCur)
            ||  pNode->papChildren[pNode->cChildren - 1]->pszName[cchCur])
        {
            pNode = createAndInsertNode(pNode, pszCur, pszNext - pszCur, UINT32_MAX);
            if (!pNode)
                return VERR_NO_MEMORY;
        }
        else
            pNode = pNode->papChildren[pNode->cChildren - 1];

        /* Advance */
        pszCur = *pszNext ? pszNext + 1 : pszNext;
    }

    /*
     * Save the data.
     */
    return initNode(pNode, enmType, pvSample, enmUnit, enmVisibility, pszDesc);
}


PDBGGUISTATSNODE
VBoxDbgStatsModelVM::createNewTree(void)
{
    PDBGGUISTATSNODE pRoot = createRootNode();
    if (pRoot)
    {
        int rc = stamEnum(QString(), createNewTreeCallback, pRoot);
        if (VBOX_SUCCESS(rc))
            return pRoot;

        /* failed, cleanup. */
        destroyTree(pRoot);
    }

    return NULL;
}




#if 0 /* save for later */

void VBoxDbgStatsLeafItem::update(STAMTYPE enmType, void *pvSample, STAMUNIT enmUnit, STAMVISIBILITY enmVisibility, const char *pszDesc)
{
    /*
     * Detect changes.
     * This path will be taken on the first update and if a item
     * is reregistred with a different unit/type (unlikely).
     */
    if (    enmType != m_enmType
        ||  enmUnit != m_enmUnit)
    {
        m_enmType = enmType;
        m_enmUnit = enmUnit;

        /*
         * Unit.
         */
        setText(1, STAMR3GetUnit(enmUnit));

        /**
         * Update the description.
         * Insert two spaces as gap after the last left-aligned field.
         * @todo dmik: How to make this better?
         */
        m_DescStr = QString("  ") + QString(pszDesc);

        /*
         * Clear the content.
         */
        setText(2, "");
        setText(3, "");
        setText(4, "");
        setText(5, "");
        setText(6, "");
        setText(8, m_DescStr);
    }

    /*
     * Update the data.
     */
    char sz[64];
    switch (enmType)
    {
        case STAMTYPE_COUNTER:
        {
            const uint64_t cPrev = m_Data.Counter.c;
            m_Data.Counter = *(PSTAMCOUNTER)pvSample;
            setText(2, formatNumber(sz, m_Data.Counter.c));
            setText(7, formatNumberSigned(sz, m_Data.Counter.c - cPrev));
            setVisible(   enmVisibility != STAMVISIBILITY_NOT_GUI
                       && (enmVisibility == STAMVISIBILITY_ALWAYS || m_Data.Counter.c));
            break;
        }

        case STAMTYPE_PROFILE:
        case STAMTYPE_PROFILE_ADV:
        {
            const uint64_t cPeriodsPrev = m_Data.Profile.cPeriods;
            m_Data.Profile = *(PSTAMPROFILE)pvSample;
            if (m_Data.Profile.cPeriods)
            {
                setText(2, formatNumber(sz, m_Data.Profile.cPeriods));
                setText(3, formatNumber(sz, m_Data.Profile.cTicksMin));
                setText(4, formatNumber(sz, m_Data.Profile.cTicks / m_Data.Profile.cPeriods));
                setText(5, formatNumber(sz, m_Data.Profile.cTicksMax));
                setText(6, formatNumber(sz, m_Data.Profile.cTicks));
                setText(7, formatNumberSigned(sz, m_Data.Profile.cPeriods - cPeriodsPrev));
                setVisible(enmVisibility != STAMVISIBILITY_NOT_GUI);
            }
            else
            {
                setText(2, "0");
                setText(3, "0");
                setText(4, "0");
                setText(5, "0");
                setText(6, "0");
                setText(7, "0");
                setVisible(enmVisibility != STAMVISIBILITY_NOT_GUI && enmVisibility == STAMVISIBILITY_ALWAYS);
            }
            break;
        }

        case STAMTYPE_RATIO_U32:
        case STAMTYPE_RATIO_U32_RESET:
        {
            const STAMRATIOU32 RatioU32 = m_Data.RatioU32;
            m_Data.RatioU32 = *(PSTAMRATIOU32)pvSample;

            char sz2[64];
            char sz3[128];
            strcat(strcat(strcpy(sz3, formatNumber(sz, m_Data.RatioU32.u32A)), " : "), formatNumber(sz2, m_Data.RatioU32.u32B));
            setText(2, sz3);
            ///@todo ratio: setText(7, formatNumberSigned(sz, m_Data.u64 - u64Prev));
            setVisible(   enmVisibility != STAMVISIBILITY_NOT_GUI
                       && (enmVisibility == STAMVISIBILITY_ALWAYS || m_Data.RatioU32.u32A || m_Data.RatioU32.u32B));
            break;
        }

        case STAMTYPE_CALLBACK:
        {
            const char *pszString = (const char *)pvSample;
            setText(2, pszString);
            setVisible(   enmVisibility != STAMVISIBILITY_NOT_GUI
                       && (enmVisibility == STAMVISIBILITY_ALWAYS || *pszString));
            break;
        }

        case STAMTYPE_U8:
        case STAMTYPE_U8_RESET:
        {
            const uint8_t u8Prev = m_Data.u8;
            m_Data.u8 = *(uint8_t *)pvSample;
            setText(2, formatNumber(sz, m_Data.u8));
            setText(7, formatNumberSigned(sz, (int32_t)m_Data.u8 - u8Prev));
            setVisible(   enmVisibility != STAMVISIBILITY_NOT_GUI
                       && (enmVisibility == STAMVISIBILITY_ALWAYS || m_Data.u8));
            break;
        }

        case STAMTYPE_X8:
        case STAMTYPE_X8_RESET:
        {
            const uint8_t u8Prev = m_Data.u8;
            m_Data.u8 = *(uint8_t *)pvSample;
            setText(2, formatHexNumber(sz, m_Data.u8, 2));
            setText(7, formatHexNumber(sz, m_Data.u8 - u8Prev, 1));
            setVisible(   enmVisibility != STAMVISIBILITY_NOT_GUI
                       && (enmVisibility == STAMVISIBILITY_ALWAYS || m_Data.u8));
            break;
        }

        case STAMTYPE_U16:
        case STAMTYPE_U16_RESET:
        {
            const uint16_t u16Prev = m_Data.u16;
            m_Data.u16 = *(uint16_t *)pvSample;
            setText(2, formatNumber(sz, m_Data.u16));
            setText(7, formatNumberSigned(sz, (int32_t)m_Data.u16 - u16Prev));
            setVisible(   enmVisibility != STAMVISIBILITY_NOT_GUI
                       && (enmVisibility == STAMVISIBILITY_ALWAYS || m_Data.u16));
            break;
        }

        case STAMTYPE_X16:
        case STAMTYPE_X16_RESET:
        {
            const uint16_t u16Prev = m_Data.u16;
            m_Data.u16 = *(uint16_t *)pvSample;
            setText(2, formatHexNumber(sz, m_Data.u16, 4));
            setText(7, formatHexNumber(sz, m_Data.u16 - u16Prev, 1));
            setVisible(   enmVisibility != STAMVISIBILITY_NOT_GUI
                       && (enmVisibility == STAMVISIBILITY_ALWAYS || m_Data.u16));
            break;
        }

        case STAMTYPE_U32:
        case STAMTYPE_U32_RESET:
        {
            const uint32_t u32Prev = m_Data.u32;
            m_Data.u32 = *(uint32_t *)pvSample;
            setText(2, formatNumber(sz, m_Data.u32));
            setText(7, formatNumberSigned(sz, (int64_t)m_Data.u32 - u32Prev));
            setVisible(   enmVisibility != STAMVISIBILITY_NOT_GUI
                       && (enmVisibility == STAMVISIBILITY_ALWAYS || m_Data.u32));
            break;
        }

        case STAMTYPE_X32:
        case STAMTYPE_X32_RESET:
        {
            const uint32_t u32Prev = m_Data.u32;
            m_Data.u32 = *(uint32_t *)pvSample;
            setText(2, formatHexNumber(sz, m_Data.u32, 8));
            setText(7, formatHexNumber(sz, m_Data.u32 - u32Prev, 1));
            setVisible(   enmVisibility != STAMVISIBILITY_NOT_GUI
                       && (enmVisibility == STAMVISIBILITY_ALWAYS || m_Data.u32));
            break;
        }

        case STAMTYPE_U64:
        case STAMTYPE_U64_RESET:
        {
            const uint64_t u64Prev = m_Data.u64;
            m_Data.u64 = *(uint64_t *)pvSample;
            setText(2, formatNumber(sz, m_Data.u64));
            setText(7, formatNumberSigned(sz, m_Data.u64 - u64Prev));
            setVisible(   enmVisibility != STAMVISIBILITY_NOT_GUI
                       && (enmVisibility == STAMVISIBILITY_ALWAYS || m_Data.u64));
            break;
        }

        case STAMTYPE_X64:
        case STAMTYPE_X64_RESET:
        {
            const uint64_t u64Prev = m_Data.u64;
            m_Data.u64 = *(uint64_t *)pvSample;
            setText(2, formatHexNumber(sz, m_Data.u64, 16));
            setText(7, formatHexNumber(sz, m_Data.u64 - u64Prev, 1));
            setVisible(   enmVisibility != STAMVISIBILITY_NOT_GUI
                       && (enmVisibility == STAMVISIBILITY_ALWAYS || m_Data.u64));
            break;
        }

        default:
            break;
    }
}


QString VBoxDbgStatsLeafItem::key(int iColumn, bool /*fAscending*/) const
{
    /* name and description */
    if (iColumn <= 1 || iColumn >= 8)
        return text(iColumn);

    /* the number columns */
    char sz[128];
    switch (m_enmType)
    {
        case STAMTYPE_COUNTER:
            switch (iColumn)
            {
                case 2: formatSortKey(sz, m_Data.Counter.c); break;
                case 7: return text(iColumn);
                default: sz[0] = '\0'; break;
            }
            break;

        case STAMTYPE_PROFILE:
        case STAMTYPE_PROFILE_ADV:
            if (m_Data.Profile.cPeriods)
            {
                switch (iColumn)
                {
                    case 2: formatSortKey(sz, m_Data.Profile.cPeriods); break;
                    case 3: formatSortKey(sz, m_Data.Profile.cTicksMin); break;
                    case 4: formatSortKey(sz, m_Data.Profile.cTicks / m_Data.Profile.cPeriods); break;
                    case 5: formatSortKey(sz, m_Data.Profile.cTicksMax); break;
                    case 6: formatSortKey(sz, m_Data.Profile.cTicks); break;
                    case 7: return text(iColumn);
                    default: sz[0] = '\0'; break;
                }
            }
            else
                sz[0] = '\0';
            break;

        case STAMTYPE_RATIO_U32:
        case STAMTYPE_RATIO_U32_RESET:
            if (m_Data.RatioU32.u32B)
                formatSortKey(sz, (m_Data.RatioU32.u32A * (uint64_t)1000) / m_Data.RatioU32.u32B);
            else if (m_Data.RatioU32.u32A)
                formatSortKey(sz, m_Data.RatioU32.u32A * (uint64_t)1000);
            else
                formatSortKey(sz, 1000);
            break;

        case STAMTYPE_U8:
        case STAMTYPE_U8_RESET:
            switch (iColumn)
            {
                case 2: formatSortKey(sz, m_Data.u8); break;
                case 7: return text(iColumn);
                default: sz[0] = '\0'; break;
            }
            break;

        case STAMTYPE_U16:
        case STAMTYPE_U16_RESET:
            switch (iColumn)
            {
                case 2: formatSortKey(sz, m_Data.u16); break;
                case 7: return text(iColumn);
                default: sz[0] = '\0'; break;
            }
            break;

        case STAMTYPE_U32:
        case STAMTYPE_U32_RESET:
            switch (iColumn)
            {
                case 2: formatSortKey(sz, m_Data.u32); break;
                case 7: return text(iColumn);
                default: sz[0] = '\0'; break;
            }
            break;

        case STAMTYPE_U64:
        case STAMTYPE_U64_RESET:
            switch (iColumn)
            {
                case 2: formatSortKey(sz, m_Data.u64); break;
                case 7: return text(iColumn);
                default: sz[0] = '\0'; break;
            }
            break;

        case STAMTYPE_CALLBACK:
        default:
            return text(iColumn);
    }

    return QString(sz);
}

#endif /* saved for later reuse */





/*
 *
 *      V B o x D b g S t a t s V i e w
 *      V B o x D b g S t a t s V i e w
 *      V B o x D b g S t a t s V i e w
 *
 *
 */


VBoxDbgStatsView::VBoxDbgStatsView(PVM a_pVM, VBoxDbgStatsModel *a_pModel, VBoxDbgStats *a_pParent/* = NULL*/)
    : QTreeView(a_pParent), VBoxDbgBase(a_pVM), m_pModel(a_pModel), m_pParent(a_pParent),
    m_pLeafMenu(NULL), m_pBranchMenu(NULL), m_pViewMenu(NULL), m_pContextNode(NULL)

{
    /*
     * Set the model and view defaults.
     */
    setModel(m_pModel);
    setRootIndex(m_pModel->getRootIndex());
    //setRootIsDecorated(true);
    setItemsExpandable(true);
    setSortingEnabled(true);

    /*
     * We've got three menus to populate and link up.
     */
#ifdef VBOXDBG_USE_QT4
    /** @todo */

#else  /* QT3 */
    m_pLeafMenu = new QPopupMenu(this);
    m_pLeafMenu->insertItem("Rese&t", eReset);
    m_pLeafMenu->insertItem("&Refresh", eRefresh);
    m_pLeafMenu->insertItem("&Copy", eCopy);
    m_pLeafMenu->insertItem("To &Log", eLog);
    m_pLeafMenu->insertItem("T&o Release Log", eLogRel);
    connect(m_pLeafMenu, SIGNAL(activated(int)), this, SLOT(leafMenuActivated(int)));

    m_pBranchMenu = new QPopupMenu(this);
    m_pBranchMenu->insertItem("&Expand Tree", eExpand);
    m_pBranchMenu->insertItem("&Collaps Tree", eCollaps);
    m_pBranchMenu->insertItem("&Refresh", eRefresh);
    m_pBranchMenu->insertItem("Rese&t", eReset);
    m_pBranchMenu->insertItem("&Copy", eCopy);
    m_pBranchMenu->insertItem("To &Log", eLog);
    m_pBranchMenu->insertItem("T&o Release Log", eLogRel);
    connect(m_pBranchMenu, SIGNAL(activated(int)), this, SLOT(branchMenuActivated(int)));

    m_pViewMenu = new QPopupMenu(this);
    m_pViewMenu->insertItem("&Expand All", eExpand);
    m_pViewMenu->insertItem("&Collaps All", eCollaps);
    m_pViewMenu->insertItem("&Refresh", eRefresh);
    m_pViewMenu->insertItem("Rese&t", eReset);
    m_pViewMenu->insertItem("&Copy", eCopy);
    m_pViewMenu->insertItem("To &Log", eLog);
    m_pViewMenu->insertItem("T&o Release Log", eLogRel);
    connect(m_pViewMenu, SIGNAL(activated(int)), this, SLOT(viewMenuActivated(int)));

    connect(this, SIGNAL(contextMenuRequested(QListViewItem *, const QPoint &, int)), this,
            SLOT(contextMenuReq(QListViewItem *, const QPoint &, int)));
#endif /* QT3 */
}


VBoxDbgStatsView::~VBoxDbgStatsView()
{
#if 0 /// @todo check who has to delete the model...
    setModel(NULL);
    delete m_pModel;
#endif
    m_pModel = NULL;
}


#if 0
/**
 * Hides all parent branches which doesn't have any visible leafs.
 */
static void hideParentBranches(VBoxDbgStatsLeafItem *pItem)
{
#ifdef VBOXDBG_USE_QT4
    /// @todo
    NOREF(pItem);
#else
    for (VBoxDbgStatsItem *pParent = pItem->getParent(); pParent; pParent = pParent->getParent())
    {
        QListViewItem *pChild = pParent->firstChild();
        while (pChild && !pChild->isVisible())
            pChild = pChild->nextSibling();
        if (pChild)
            return;
        pParent->setVisible(false);
    }
#endif
}

/**
 * Shows all parent branches
 */
static void showParentBranches(VBoxDbgStatsLeafItem *pItem)
{
    for (VBoxDbgStatsItem *pParent = pItem->getParent(); pParent; pParent = pParent->getParent())
        pParent->setVisible(true);
}
#endif


void VBoxDbgStatsView::update(const QString &rPatStr)
{
    m_pModel->update(rPatStr);

#if 0 /* later */
    m_pCur = m_pHead;
    m_PatStr = rPatStr;
    int rc = stamEnum(m_PatStr, updateCallback, this);
    if (VBOX_SUCCESS(rc))
    {
        /* hide what's left */
        for (VBoxDbgStatsLeafItem *pCur = m_pCur; pCur; pCur = pCur->m_pNext)
            if (pCur->isVisible())
            {
                pCur->setVisible(false);
                hideParentBranches(pCur);
            }
    }
    m_pCur = NULL;
#endif
    NOREF(rPatStr);
}

void VBoxDbgStatsView::reset(const QString &rPatStr)
{
    stamReset(rPatStr);
}


#if 0 /* later */

static void setOpenTree(QListViewItem *pItem, bool f)
{
#ifdef VBOXDBG_USE_QT4
    pItem->setExpanded(f);
    int cChildren = pItem->childCount();
    for (int i = 0; i < cChildren; i++)
        pItem->child(i)->setExpanded(f);
#else
    pItem->setOpen(f);
    for (pItem = pItem->firstChild(); pItem; pItem = pItem->nextSibling())
        setOpenTree(pItem, f);
#endif
}

#ifndef VBOXDBG_USE_QT4
void VBoxDbgStatsView::expandAll()
{
    setOpenTree(m_pRoot, true);
}

void VBoxDbgStatsView::collapsAll()
{
    setOpenTree(m_pRoot, false);
}
#endif /* QT3 */


/*static*/ DECLCALLBACK(int) VBoxDbgStatsView::updateCallback(const char *pszName, STAMTYPE enmType, void *pvSample, STAMUNIT enmUnit,
                                                              STAMVISIBILITY enmVisibility, const char *pszDesc, void *pvUser)
{
    Log3(("updateCallback: %s\n", pszName));
    VBoxDbgStatsView *pThis = (VBoxDbgStatsView *)pvUser;

    /*
     * Skip the ones which shouldn't be visible in the GUI.
     */
    if (enmVisibility == STAMVISIBILITY_NOT_GUI)
        return 0;

    /*
     * Advance to the matching item.
     */
    VBoxDbgStatsLeafItem *pCur = pThis->m_pCur;
    while (pCur)
    {
        /*
         * ASSUMES ascending order of STAM items.
         */
        int iDiff = strcmp(pszName, pCur->getName());
        if (!iDiff)
            break;
        if (iDiff > 0)
        {
            /*
             * Removed / filtered out.
             */
            Log2(("updateCallback: %s - filtered out\n", pCur->getName()));
            if (pCur->isVisible())
            {
                pCur->setVisible(false);
                hideParentBranches(pCur);
            }

            pCur = pCur->m_pNext;
        }
        else if (iDiff < 0)
        {
            /*
             * New item, insert before pCur.
             */
            Log2(("updateCallback: %s - new\n", pszName));
            VBoxDbgStatsLeafItem *pNew = new VBoxDbgStatsLeafItem(pszName, pThis->createPath(pszName));
            pNew->m_pNext = pCur;
            pNew->m_pPrev = pCur->m_pPrev;
            if (pNew->m_pPrev)
                pNew->m_pPrev->m_pNext = pNew;
            else
                pThis->m_pHead = pNew;
            pCur->m_pPrev = pNew;
            pCur = pNew;
            Assert(!strcmp(pszName, pCur->getName()));
            break;
        }
    }

    /*
     * End of items, insert it at the tail.
     */
    if (!pCur)
    {
        Log2(("updateCallback: %s - new end\n", pszName));
        pCur = new VBoxDbgStatsLeafItem(pszName, pThis->createPath(pszName));
        pCur->m_pNext = NULL;
        pCur->m_pPrev = pThis->m_pTail;
        if (pCur->m_pPrev)
            pCur->m_pPrev->m_pNext = pCur;
        else
            pThis->m_pHead = pCur;
        pThis->m_pTail = pCur;
    }
    Assert(pThis->m_pHead);
    Assert(pThis->m_pTail);

    /*
     * Update it and move on.
     */
    if (!pCur->isVisible())
        showParentBranches(pCur);
    pCur->update(enmType, pvSample, enmUnit, enmVisibility, pszDesc);
    pThis->m_pCur = pCur->m_pNext;

    return 0;
}

VBoxDbgStatsItem *VBoxDbgStatsView::createPath(const char *pszName)
{
    const char * const pszFullName = pszName;

    /*
     * Start at root.
     */
    while (*pszName == '/')
        pszName++;
    VBoxDbgStatsItem *pParent = m_pRoot;

    /*
     * Walk down the path creating what's missing.
     */
    for (;;)
    {
        /*
         * Extract the path component.
         */
        const char *pszEnd = strchr(pszName, '/');
        if (!pszEnd)
            return pParent;
        QString NameStr = QString::fromUtf8(pszName, pszEnd - pszName);
        /* advance */
        pszName = pszEnd + 1;

        /*
         * Try find the name among the children of that parent guy.
         */
#ifdef VBOXDBG_USE_QT4
        QListViewItem *pChild = NULL;
        int cChildren = pParent->childCount();
        for (int i = 0; i < cChildren; i++)
        {
            pChild = pParent->child(i);
            if (pChild->text(0) == NameStr)
                break;
        }
#else
        QListViewItem *pChild = pParent->firstChild();
        while (pChild && pChild->text(0) != NameStr)
            pChild = pChild->nextSibling();
#endif

        if (pChild)
            pParent = (VBoxDbgStatsItem *)pChild;
        else
        {
            Log3(("createPath: %.*s\n", pszEnd - pszFullName, pszFullName));
            NameStr = QString::fromUtf8(pszFullName, pszEnd - pszFullName);
#ifdef VBOXDBG_USE_QT4
            QByteArray NameArray = NameStr.toUtf8();
            pParent = new VBoxDbgStatsItem(NameArray.constData(), pParent);
#else
            pParent = new VBoxDbgStatsItem(NameStr, pParent);
#endif
        }
        pParent->setVisible(true);
    }
}

void VBoxDbgStatsView::contextMenuReq(QListViewItem *pItem, const QPoint &rPoint, int /*iColumn*/)
{
    if (pItem)
    {
        m_pContextMenuItem = (VBoxDbgStatsItem *)pItem;
        if (m_pContextMenuItem->isLeaf())
        {
#ifdef VBOXDBG_USE_QT4
#else
            m_pLeafMenu->setItemEnabled(eReset, isVMOk());
            m_pLeafMenu->setItemEnabled(eRefresh, isVMOk());
#endif
            m_pLeafMenu->popup(rPoint);
        }
        else
        {
#ifdef VBOXDBG_USE_QT4
#else
            m_pBranchMenu->setItemEnabled(eReset, isVMOk());
            m_pBranchMenu->setItemEnabled(eRefresh, isVMOk());
#endif
            m_pBranchMenu->popup(rPoint);
        }
    }
    else
    {
        m_pContextMenuItem = NULL;
#ifdef VBOXDBG_USE_QT4
#else
        m_pViewMenu->setItemEnabled(eReset, isVMOk());
        m_pViewMenu->setItemEnabled(eRefresh, isVMOk());
#endif
        m_pViewMenu->popup(rPoint);
    }
}

void VBoxDbgStatsView::leafMenuActivated(int iId)
{
    VBoxDbgStatsLeafItem *pItem = (VBoxDbgStatsLeafItem *)m_pContextMenuItem;
    AssertReturn(pItem, (void)0);

    switch ((MenuId)iId)
    {
        case eReset:
            stamReset(m_pContextMenuItem->getName());
            /* fall thru */

        case eRefresh:
            m_pCur = pItem;
            stamEnum(m_pContextMenuItem->getName(), updateCallback, this);
            break;

        case eCopy:
            m_pContextMenuItem->copyTreeToClipboard();
            break;

        case eLog:
            m_pContextMenuItem->logTree(false /* !release log */);
            break;

        case eLogRel:
            m_pContextMenuItem->logTree(true /* release log */);
            break;

        default: /* keep gcc quite */
            break;
    }
    m_pContextMenuItem = NULL;
}

void VBoxDbgStatsView::branchMenuActivated(int iId)
{
    AssertReturn(m_pContextMenuItem, (void)0);

    /** @todo make enum for iId */
    switch ((MenuId)iId)
    {
        case eExpand:
            setOpenTree(m_pContextMenuItem, true);
            break;

        case eCollaps:
            setOpenTree(m_pContextMenuItem, false);
            break;

        case eReset:
        {
            QString Str = QString::fromUtf8(m_pContextMenuItem->getName());
            Str.append((Str != "/") ? "/*" : "*");
            stamReset(Str);
        }
        /* fall thru */

        case eRefresh:
        {
            const char *psz = m_pContextMenuItem->getName();
            QString Str = QString::fromUtf8(psz);
            if (strcmp(psz, "/"))
            {
                int cch = strlen(psz);
                m_pCur = m_pHead;
                while (     m_pCur
                       &&   (   strncmp(psz, m_pCur->getName(), cch)
                             || m_pCur->getName()[cch] != '/'))
                {
                    m_pCur = m_pCur->m_pNext;
                }
                if (!m_pCur)
                    return;
                Str.append("/*");
            }
            else
            {
                m_pCur = m_pHead;
                Str.append("*");
            }
            stamEnum(Str, updateCallback, this);
            m_pCur = NULL;
            break;
        }

        case eCopy:
            m_pContextMenuItem->copyTreeToClipboard();
            break;

        case eLog:
            m_pContextMenuItem->logTree(false /* !release log */);
            break;

        case eLogRel:
            m_pContextMenuItem->logTree(true /* release log */);
            break;

    }
    m_pContextMenuItem = NULL;
}

void VBoxDbgStatsView::viewMenuActivated(int iId)
{
    switch ((MenuId)iId)
    {
        case eExpand:
            setOpenTree(m_pRoot, true);
            break;

        case eCollaps:
            setOpenTree(m_pRoot, false);
            break;

        case eReset:
            reset(m_PatStr);
            /* fall thru */

        case eRefresh:
            update(QString(m_PatStr));
            break;

        case eCopy:
            m_pRoot->copyTreeToClipboard();
            break;

        case eLog:
            m_pRoot->logTree(false /* !release log */);
            break;

        case eLogRel:
            m_pRoot->logTree(true /* release log */);
            break;
    }
}

#endif /* later */




/*
 *
 *      V B o x D b g S t a t s
 *      V B o x D b g S t a t s
 *      V B o x D b g S t a t s
 *
 *
 */


VBoxDbgStats::VBoxDbgStats(PVM pVM, const char *pszPat/* = NULL*/, unsigned uRefreshRate/* = 0*/, QWidget *pParent/* = NULL*/)
    : QWidget(pParent), VBoxDbgBase(pVM), m_PatStr(pszPat), m_uRefreshRate(0)
{
    setWindowTitle("VBoxDbg - Statistics");

    /*
     * On top, a horizontal box with the pattern field, buttons and refresh interval.
     */
    QHBoxLayout *pHLayout = new QHBoxLayout;

    QLabel *pLabel = new QLabel(" Pattern ");
    pHLayout->addWidget(pLabel);
    pLabel->setMaximumSize(pLabel->sizeHint());
    pLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    m_pPatCB = new QComboBox();
    pHLayout->addWidget(m_pPatCB);
    if (pszPat && *pszPat)
        m_pPatCB->addItem(pszPat);
    m_pPatCB->setDuplicatesEnabled(false);
    m_pPatCB->setEditable(true);
    connect(m_pPatCB, SIGNAL(activated(const QString &)), this, SLOT(apply(const QString &)));

    QPushButton *pPB = new QPushButton("&All");
    pHLayout->addWidget(pPB);
    pPB->setMaximumSize(pPB->sizeHint());
    connect(pPB, SIGNAL(clicked()), this, SLOT(applyAll()));

    pLabel = new QLabel("  Interval ");
    pHLayout->addWidget(pLabel);
    pLabel->setMaximumSize(pLabel->sizeHint());
    pLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    QSpinBox *pSB = new QSpinBox();
    pHLayout->addWidget(pSB);
    pSB->setMinimum(0);
    pSB->setMaximum(60);
    pSB->setSingleStep(1.0);
    pSB->setValue(m_uRefreshRate);
    pSB->setSuffix(" s");
    pSB->setWrapping(false);
    pSB->setButtonSymbols(QSpinBox::PlusMinus);
    pSB->setMaximumSize(pSB->sizeHint());
    connect(pSB, SIGNAL(valueChanged(int)), this, SLOT(setRefresh(int)));


    /*
     * Create the tree view and setup the layout.
     */
    VBoxDbgStatsModelVM *pModel = new VBoxDbgStatsModelVM(pVM, NULL);
    m_pView = new VBoxDbgStatsView(pVM, pModel, this);

    QWidget *pHBox = new QWidget;
    pHBox->setLayout(pHLayout);

    QVBoxLayout *pVLayout = new QVBoxLayout;
    pVLayout->addWidget(pHBox);
    pVLayout->addWidget(m_pView);
    this->setLayout(pVLayout);

#if 0 //fixme
    /*
     * Perform the first refresh to get a good window size.
     * We do this with sorting disabled because it's horribly slow otherwise.
     */
//later:    int iColumn = m_pView->sortColumn();
    m_pView->setUpdatesEnabled(false);
    m_pView->setSortingEnabled(false);
    refresh();
//later:    m_pView->sortItems(iColumn, Qt::AscendingOrder);
 //   QTreeView::expandAll
#endif
    m_pView->expandAll();
    for (int i = 0; i <= 8; i++)
    {
        printf("%#x: %d", i, m_pView->columnWidth(i));
        m_pView->resizeColumnToContents(i);
        printf(" -> %d\n", m_pView->columnWidth(i));
    }

    /*
     * Create a refresh timer and start it.
     */
    m_pTimer = new QTimer(this);
    connect(m_pTimer, SIGNAL(timeout()), this, SLOT(refresh()));
    setRefresh(uRefreshRate);
}

VBoxDbgStats::~VBoxDbgStats()
{
    //????
}

void VBoxDbgStats::apply(const QString &Str)
{
    m_PatStr = Str;
    refresh();
}

void VBoxDbgStats::applyAll()
{
    apply("");
}

void VBoxDbgStats::refresh()
{
    m_pView->update(m_PatStr);
}

void VBoxDbgStats::setRefresh(int iRefresh)
{
    if ((unsigned)iRefresh != m_uRefreshRate)
    {
        if (!m_uRefreshRate || iRefresh)
            m_pTimer->start(iRefresh * 1000);
        else
            m_pTimer->stop();
        m_uRefreshRate = iRefresh;
    }
}

