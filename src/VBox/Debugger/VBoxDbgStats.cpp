/** @file
 *
 * VBox Debugger GUI - Statistics.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGG
#include "VBoxDbgStats.h"
#include <qlocale.h>
#include <qpushbutton.h>
#include <qspinbox.h>
#include <qlabel.h>
#include <qclipboard.h>
#include <qapplication.h>

#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/string.h>
#include <iprt/assert.h>




/**
 * Gets the last component of a statistics name string.
 *
 * @returns the last component in the name string.
 */
const char *getNodeName(const char *pszName)
{
    const char *pszRet = strrchr(pszName, '/');
    return pszRet && pszName[1] ? pszRet + 1 : pszName;
}






/*
 *
 *      V B o x D b g S t a t s I t e m
 *      V B o x D b g S t a t s I t e m
 *      V B o x D b g S t a t s I t e m
 *
 */


VBoxDbgStatsItem::VBoxDbgStatsItem(const char *pszName, VBoxDbgStatsItem *pParent, bool fBranch /*= true*/)
    : QListViewItem(pParent, QString(getNodeName(pszName))), m_pszName(RTStrDup(pszName)), m_fBranch(fBranch), m_pParent(pParent)

{
}

VBoxDbgStatsItem::VBoxDbgStatsItem(const char *pszName, QListView *pParent, bool fBranch/* = true*/)
    : QListViewItem(pParent, QString(getNodeName(pszName))), m_pszName(RTStrDup(pszName)), m_fBranch(fBranch), m_pParent(NULL)

{
}

VBoxDbgStatsItem::~VBoxDbgStatsItem()
{
    RTStrFree(m_pszName);
    m_pszName = NULL;
}

void VBoxDbgStatsItem::logTree(bool fReleaseLog) const
{
    /* Iterate and print our children. */
    QListViewItem *pItem;
    for (pItem = firstChild(); pItem; pItem = pItem->nextSibling())
    {
        VBoxDbgStatsItem *pMyItem = (VBoxDbgStatsItem *)pItem;
        pMyItem->logTree(fReleaseLog);
    }
}

void VBoxDbgStatsItem::stringifyTree(QString &String) const
{
    /* Iterate and stringify our children. */
    QListViewItem *pItem;
    for (pItem = firstChild(); pItem; pItem = pItem->nextSibling())
    {
        VBoxDbgStatsItem *pMyItem = (VBoxDbgStatsItem *)pItem;
        pMyItem->stringifyTree(String);
    }
}

void VBoxDbgStatsItem::copyTreeToClipboard(void) const
{
    QString String;
    stringifyTree(String);

    QClipboard *pClipboard = QApplication::clipboard();
    if (pClipboard)
        pClipboard->setText(String, QClipboard::Clipboard);
}




/*
 *
 *      V B o x D b g S t a t s L e a f I t e m
 *      V B o x D b g S t a t s L e a f I t e m
 *      V B o x D b g S t a t s L e a f I t e m
 *
 */


VBoxDbgStatsLeafItem::VBoxDbgStatsLeafItem(const char *pszName, VBoxDbgStatsItem *pParent)
    : VBoxDbgStatsItem(pszName, pParent, false),
      m_pNext(NULL), m_pPrev(NULL), m_enmType(STAMTYPE_INVALID),
      m_enmUnit(STAMUNIT_INVALID), m_DescStr()
{
    memset(&m_Data, 0, sizeof(m_Data));
}


VBoxDbgStatsLeafItem::~VBoxDbgStatsLeafItem()
{
}


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

void VBoxDbgStatsLeafItem::logTree(bool fReleaseLog) const
{
    /*
     * Generic printing.
     */
    if (isVisible())
    {
        if (fReleaseLog)
            RTLogRelPrintf("%-50s  %-10s %18s %18s %18s %18s %16s %s\n",
                           getName(), (const char *)text(1), (const char *)text(2), (const char *)text(3),
                           (const char *)text(4), (const char *)text(5), (const char *)text(7), (const char *)text(8));
        else
            RTLogPrintf("%-50s  %-10s %18s %18s %18s %18s %16s %s\n",
                        getName(), (const char *)text(1), (const char *)text(2), (const char *)text(3),
                        (const char *)text(4), (const char *)text(5), (const char *)text(7), (const char *)text(8));
    }

    /*
     * Let the super class to do the rest.
     */
    VBoxDbgStatsItem::logTree(fReleaseLog);
}

void VBoxDbgStatsLeafItem::stringifyTree(QString &String) const
{
    /*
     * Generic printing.
     */
    if (isVisible())
    {
        QString ItemString;
        ItemString.sprintf("%-50s  %-10s %18s %18s %18s %18s %16s %s\n",
                           getName(), (const char *)text(1), (const char *)text(2), (const char *)text(3),
                           (const char *)text(4), (const char *)text(5), (const char *)text(7), (const char *)text(8));
        String += ItemString;
    }

    /*
     * Let the super class to do the rest.
     */
    VBoxDbgStatsItem::stringifyTree(String);
}





/*
 *
 *      V B o x D b g S t a t s V i e w
 *      V B o x D b g S t a t s V i e w
 *      V B o x D b g S t a t s V i e w
 *
 *
 */


VBoxDbgStatsView::VBoxDbgStatsView(PVM pVM, VBoxDbgStats *pParent/* = NULL*/, const char *pszName/* = NULL*/, WFlags f/* = 0*/)
    : QListView(pParent, pszName, f),  VBoxDbgBase(pVM),
    m_pParent(pParent), m_pHead(NULL), m_pTail(NULL), m_pCur(NULL), m_pRoot(NULL),
    m_pLeafMenu(NULL), m_pBranchMenu(NULL), m_pViewMenu(NULL), m_pContextMenuItem(NULL)

{
    /*
     * Create the columns.
     */
    addColumn("Name");                  // 0
    addColumn("Unit");                  // 1
    setColumnAlignment(1, Qt::AlignCenter);
    addColumn("Value/Times");           // 2
    setColumnAlignment(2, Qt::AlignRight);
    addColumn("Min");                   // 3
    setColumnAlignment(3, Qt::AlignRight);
    addColumn("Average");               // 4
    setColumnAlignment(4, Qt::AlignRight);
    addColumn("Max");                   // 5
    setColumnAlignment(5, Qt::AlignRight);
    addColumn("Total");                 // 6
    setColumnAlignment(6, Qt::AlignRight);
    addColumn("dInt");                  // 7
    setColumnAlignment(7, Qt::AlignRight);
    int i = addColumn("Description");   // 8
    NOREF(i);
    Assert(i == 8);
    setShowSortIndicator(true);

    /*
     * Create the root node.
     */
    setRootIsDecorated(true);
    m_pRoot = new VBoxDbgStatsItem("/", this);
    m_pRoot->setOpen(true);

    /*
     * We've got three menus to populate and link up.
     */
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
}

VBoxDbgStatsView::~VBoxDbgStatsView()
{
    /* Who frees the items? What happens to the reference in QListView? Does the parent free things in some way? */
#if 0
    VBoxDbgStatsLeafItem *pCur = m_pHead;
    while (pCur)
    {
        VBoxDbgStatsLeafItem *pFree = pCur;
        pCur = pCur->m_pNext;
        delete pFree;
    }

    delete m_pRoot;
#endif
    m_pHead = NULL;
    m_pTail = NULL;
    m_pCur  = NULL;
    m_pRoot = NULL;
}

/**
 * Hides all parent branches which doesn't have any visible leafs.
 */
static void hideParentBranches(VBoxDbgStatsLeafItem *pItem)
{
    for (VBoxDbgStatsItem *pParent = pItem->getParent(); pParent; pParent = pParent->getParent())
    {
        QListViewItem *pChild = pParent->firstChild();
        while (pChild && !pChild->isVisible())
            pChild = pChild->nextSibling();
        if (pChild)
            return;
        pParent->setVisible(false);
    }
}

/**
 * Shows all parent branches
 */
static void showParentBranches(VBoxDbgStatsLeafItem *pItem)
{
    for (VBoxDbgStatsItem *pParent = pItem->getParent(); pParent; pParent = pParent->getParent())
        pParent->setVisible(true);
}

void VBoxDbgStatsView::update(const QString &rPatStr)
{
    m_pCur = m_pHead;
    m_PatStr = rPatStr;
    int rc = stamEnum(m_PatStr.isEmpty() ? NULL : m_PatStr, updateCallback, this);
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
}

void VBoxDbgStatsView::reset(const QString &rPatStr)
{
    stamReset(rPatStr.isEmpty() ? NULL : rPatStr);
}

static void setOpenTree(QListViewItem *pItem, bool f)
{
    pItem->setOpen(f);
    for (pItem = pItem->firstChild(); pItem; pItem = pItem->nextSibling())
        setOpenTree(pItem, f);
}

void VBoxDbgStatsView::expandAll()
{
    setOpenTree(m_pRoot, true);
}

void VBoxDbgStatsView::collapsAll()
{
    setOpenTree(m_pRoot, false);
}

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
        QListViewItem *pChild = pParent->firstChild();
        while (pChild && pChild->text(0) != NameStr)
            pChild = pChild->nextSibling();
        if (pChild)
            pParent = (VBoxDbgStatsItem *)pChild;
        else
        {
            Log3(("createPath: %.*s\n", pszEnd - pszFullName, pszFullName));
            NameStr = QString::fromUtf8(pszFullName, pszEnd - pszFullName);
            pParent = new VBoxDbgStatsItem(NameStr, pParent);
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
            m_pLeafMenu->setItemEnabled(eReset, isVMOk());
            m_pLeafMenu->setItemEnabled(eRefresh, isVMOk());
            m_pLeafMenu->popup(rPoint);
        }
        else
        {
            m_pBranchMenu->setItemEnabled(eReset, isVMOk());
            m_pBranchMenu->setItemEnabled(eRefresh, isVMOk());
            m_pBranchMenu->popup(rPoint);
        }
    }
    else
    {
        m_pContextMenuItem = NULL;
        m_pViewMenu->setItemEnabled(eReset, isVMOk());
        m_pViewMenu->setItemEnabled(eRefresh, isVMOk());
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






/*
 *
 *      V B o x D b g S t a t s
 *      V B o x D b g S t a t s
 *      V B o x D b g S t a t s
 *
 *
 */


VBoxDbgStats::VBoxDbgStats(PVM pVM, const char *pszPat/* = NULL*/, unsigned uRefreshRate/* = 0*/,
                           QWidget *pParent/* = NULL*/, const char *pszName/* = NULL*/, WFlags f/* = 0*/)
    : QVBox(pParent, pszName, f), VBoxDbgBase(pVM),
    m_PatStr(pszPat), m_uRefreshRate(0)
{
    setCaption("VBoxDbg - Statistics");

    /*
     * On top, a horizontal box with the pattern field, buttons and refresh interval.
     */
    QHBox *pHBox = new QHBox(this);

    QLabel *pLabel = new QLabel(NULL, " Pattern ", pHBox);
    pLabel->setMaximumSize(pLabel->sizeHint());
    pLabel->setAlignment(AlignHCenter | AlignVCenter);

    m_pPatCB = new QComboBox(true, pHBox, "Pattern");
    if (pszPat && *pszPat)
        m_pPatCB->insertItem(pszPat);
    m_pPatCB->setDuplicatesEnabled(false);
    connect(m_pPatCB, SIGNAL(activated(const QString &)), this, SLOT(apply(const QString &)));

    QPushButton *pPB = new QPushButton("&All", pHBox);
    connect(pPB, SIGNAL(clicked()), this, SLOT(applyAll()));
    pPB->setMaximumSize(pPB->sizeHint());

    pLabel = new QLabel(NULL, "  Interval ", pHBox);
    pLabel->setMaximumSize(pLabel->sizeHint());
    pLabel->setAlignment(AlignRight | AlignVCenter);

    QSpinBox *pSB = new QSpinBox(0, 60, 1, pHBox, "Interval");
    pSB->setValue(m_uRefreshRate);
    pSB->setSuffix(" s");
    pSB->setWrapping(false);
    pSB->setButtonSymbols(QSpinBox::PlusMinus);
    pSB->setMaximumSize(pSB->sizeHint());
    connect(pSB, SIGNAL(valueChanged(int)), this, SLOT(setRefresh(int)));


    /*
     * Place the view at the top.
     */
    m_pView = new VBoxDbgStatsView(pVM, this, pszName, f);

    /*
     * Perform the first refresh to get a good window size.
     * We do this with sorting disabled because it's horribly slow otherwise.
     */
    int iColumn = m_pView->sortColumn();
    m_pView->setSortColumn(-1);
    refresh();
    m_pView->setSortColumn(iColumn);
    m_pView->sort();

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
        if (!m_uRefreshRate)
            m_pTimer->start(iRefresh * 1000);
        else if (iRefresh)
            m_pTimer->changeInterval(iRefresh * 1000);
        else
            m_pTimer->stop();
        m_uRefreshRate = iRefresh;
    }
}

