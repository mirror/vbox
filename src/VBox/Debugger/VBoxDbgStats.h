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


#ifndef __VBoxDbgStats_h__
#define __VBoxDbgStats_h__

#include "VBoxDbgBase.h"

#include <qlistview.h>
#include <qvbox.h>
#include <qtimer.h>
#include <qcombobox.h>
#include <qpopupmenu.h>

class VBoxDbgStats;

/**
 * A statistics item.
 *
 * This class represent can be both a leaf and a branch item.
 */
class VBoxDbgStatsItem : public QListViewItem
{
public:
    /**
     * Constructor.
     *
     * @param   pszName     The name of this item.
     * @param   pParent     The parent view item.
     * @param   fBranch     Set if this is a branch.
     */
    VBoxDbgStatsItem(const char *pszName, VBoxDbgStatsItem *pParent, bool fBranch = true);

    /**
     * Constructor.
     *
     * @param   pszName     The name of this item.
     * @param   pParent     The parent list view.
     * @param   fBranch     Set if this is a branch.
     */
    VBoxDbgStatsItem(const char *pszName, QListView *pParent, bool fBranch = true);

    /** Destructor. */
    virtual ~VBoxDbgStatsItem();

    /**
     * Gets the STAM name of the item.
     * @returns STAM Name.
     */
    const char *getName() const
    {
        return m_pszName;
    }

    /**
     * Branch item?
     * @returns true if branch, false if leaf.
     */
    bool isBranch() const
    {
        return m_fBranch;
    }

    /**
     * Leaf item?
     * @returns true if leaf, false if branch.
     */
    bool isLeaf() const
    {
        return !m_fBranch;
    }

    /**
     * Gets the parent item.
     * @returns Pointer to parent item, NULL if this is the root item.
     */
    VBoxDbgStatsItem *getParent()
    {
        return m_pParent;
    }

    /**
     * Get sort key.
     *
     * @returns The sort key.
     * @param   iColumn         The column to sort.
     * @param   fAscending      The sorting direction.
     */
    virtual QString key(int iColumn, bool fAscending) const
    {
        return QListViewItem::key(iColumn, fAscending);
    }

    /**
     * Logs the tree starting at this item to one of the default logs.
     * @param   fReleaseLog     If set use RTLogRelPrintf instead of RTLogPrintf.
     */
    virtual void logTree(bool fReleaseLog) const;

    /**
     * Converts the tree starting at this item into a string and adds it to
     * the specified string object.
     * @param   String          The string to append the stringified tree to.
     */
    virtual void stringifyTree(QString &String) const;

    /**
     * Copies the stringified tree onto the clipboard.
     */
    void copyTreeToClipboard(void) const;


protected:
    /** The name of this item. */
    char       *m_pszName;
    /** Branch (true) / Leaf (false) indicator */
    bool        m_fBranch;
    /** Parent item.
     * This is NULL for the root item. */
    VBoxDbgStatsItem *m_pParent;
};


/**
 * A statistics item.
 *
 * This class represent one statistical item from STAM.
 */
class VBoxDbgStatsLeafItem : public VBoxDbgStatsItem
{
public:
    /**
     * Constructor.
     *
     * @param   pszName     The name of this item.
     * @param   pParent     The parent view item.
     */
    VBoxDbgStatsLeafItem(const char *pszName, VBoxDbgStatsItem *pParent);

    /** Destructor. */
    virtual ~VBoxDbgStatsLeafItem();

    /**
     * Updates the item when current data.
     *
     * @param   enmType         The current type of the object.
     * @param   pvSample        Pointer to the sample (may change).
     * @param   enmUnit         The current unit.
     * @param   enmVisibility   The current visibility settings.
     * @param   pszDesc         The current description.
     */
    void update(STAMTYPE enmType, void *pvSample, STAMUNIT enmUnit, STAMVISIBILITY enmVisibility, const char *pszDesc);

    /**
     * Get sort key.
     *
     * @returns The sort key.
     * @param   iColumn         The column to sort.
     * @param   fAscending      The sorting direction.
     */
    virtual QString key(int iColumn, bool fAscending) const;

    /**
     * Logs the tree starting at this item to one of the default logs.
     * @param   fReleaseLog     If set use RTLogRelPrintf instead of RTLogPrintf.
     */
    virtual void logTree(bool fReleaseLog) const;

    /**
     * Converts the tree starting at this item into a string and adds it to
     * the specified string object.
     * @param   String          The string to append the stringified tree to.
     */
    virtual void stringifyTree(QString &String) const;

    /** Pointer to the next item in the list.
     * The list is maintained by the creator of the object, not the object it self. */
    VBoxDbgStatsLeafItem *m_pNext;
    /** Pointer to the previous item in the list. */
    VBoxDbgStatsLeafItem *m_pPrev;


protected:

    /** The data type. */
    STAMTYPE    m_enmType;
    /** The data at last update. */
    union
    {
        /** STAMTYPE_COUNTER. */
        STAMCOUNTER     Counter;
        /** STAMTYPE_PROFILE. */
        STAMPROFILE     Profile;
        /** STAMTYPE_PROFILE_ADV. */
        STAMPROFILEADV  ProfileAdv;
        /** STAMTYPE_RATIO_U32. */
        STAMRATIOU32    RatioU32;
        /** STAMTYPE_U8 & STAMTYPE_U8_RESET. */
        uint8_t         u8;
        /** STAMTYPE_U16 & STAMTYPE_U16_RESET. */
        uint16_t        u16;
        /** STAMTYPE_U32 & STAMTYPE_U32_RESET. */
        uint32_t        u32;
        /** STAMTYPE_U64 & STAMTYPE_U64_RESET. */
        uint64_t        u64;
    }           m_Data;
    /** The unit. */
    STAMUNIT    m_enmUnit;
    /** The description string. */
    QString     m_DescStr;
};


/**
 * The VM statistics tree view.
 *
 * A tree represenation of the STAM statistics.
 */
class VBoxDbgStatsView : public QListView, public VBoxDbgBase
{
    Q_OBJECT

public:
    /**
     * Creates a VM statistics list view widget.
     *
     * @param   pVM         The VM which STAM data is being viewed.
     * @param   pParent     Parent widget.
     * @param   pszName     Widget name.
     * @param   f           Widget flags.
     */
    VBoxDbgStatsView(PVM pVM, VBoxDbgStats *pParent = NULL, const char *pszName = NULL, WFlags f = 0);

    /** Destructor. */
    virtual ~VBoxDbgStatsView();

    /**
     * Updates the view with current information from STAM.
     * This will indirectly update the m_PatStr.
     *
     * @param   rPatStr     Selection pattern. NULL means everything, see STAM for further details.
     */
    void update(const QString &rPatStr);

    /**
     * Resets the stats items matching the specified pattern.
     * This pattern doesn't have to be the one used for update, thus m_PatStr isn't updated.
     *
     * @param   rPatStr     Selection pattern. NULL means everything, see STAM for further details.
     */
    void reset(const QString &rPatStr);

    /**
     * Expand all items in the view.
     */
    void expandAll();

    /**
     * Collaps all items in the view.
     */
    void collapsAll();

private:
    /**
     * Callback function for the STAMR3Enum() made by update().
     *
     * @returns 0 (i.e. never halt enumeration).
     *
     * @param   pszName         The name of the sample.
     * @param   enmType         The type.
     * @param   pvSample        Pointer to the data. enmType indicates the format of this data.
     * @param   enmUnit         The unit.
     * @param   enmVisibility   The visibility.
     * @param   pszDesc         The description.
     * @param   pvUser          Pointer to the VBoxDbgStatsView object.
     */
    static DECLCALLBACK(int) updateCallback(const char *pszName, STAMTYPE enmType, void *pvSample, STAMUNIT enmUnit,
                                            STAMVISIBILITY enmVisibility, const char *pszDesc, void *pvUser);

protected:
    /**
     * Creates / finds the path to the specified stats item and makes is visible.
     *
     * @returns Parent node.
     * @param   pszName     Path to a stats item.
     */
    VBoxDbgStatsItem *createPath(const char *pszName);

protected slots:
    /** Context menu. */
    void contextMenuReq(QListViewItem *pItem, const QPoint &rPoint, int iColumn);
    /** Leaf context. */
    void leafMenuActivated(int iId);
    /** Branch context. */
    void branchMenuActivated(int iId);
    /** View context. */
    void viewMenuActivated(int iId);

protected:
    typedef enum { eRefresh = 1, eReset, eExpand, eCollaps, eCopy, eLog, eLogRel } MenuId;

protected:
    /** The current selection pattern. */
    QString m_PatStr;
    /** The parent widget. */
    VBoxDbgStats *m_pParent;
    /** Head of the items list.
     * This list is in the order that STAMR3Enum() uses.
     * Access seralization should not be required, and is therefore omitted. */
    VBoxDbgStatsLeafItem *m_pHead;
    /** Tail of the items list (see m_pHead). */
    VBoxDbgStatsLeafItem *m_pTail;
    /** The current position in the enumeration.
     * If NULL we've reached the end of the list and are adding elements. */
    VBoxDbgStatsLeafItem *m_pCur;
    /** The root item. */
    VBoxDbgStatsItem *m_pRoot;
    /** Leaf item menu. */
    QPopupMenu *m_pLeafMenu;
    /** Branch item menu. */
    QPopupMenu *m_pBranchMenu;
    /** View menu. */
    QPopupMenu *m_pViewMenu;
    /** The pointer to the context menu item which is the focus of a context menu. */
    VBoxDbgStatsItem *m_pContextMenuItem;
};



/**
 * The VM statistics window.
 *
 * This class displays the statistics of a VM. The UI contains
 * a entry field for the selection pattern, a refresh interval
 * spinbutton, and the tree view with the statistics.
 */
class VBoxDbgStats : public QVBox, public VBoxDbgBase
{
    Q_OBJECT

public:
    /**
     * Creates a VM statistics list view widget.
     *
     * @param   pVM             The VM this is hooked up to.
     * @param   pszPat          Initial selection pattern. NULL means everything. (See STAM for details.)
     * @param   uRefreshRate    The refresh rate. 0 means not to refresh and is the default.
     * @param   pParent         Parent widget.
     * @param   pszName         Widget name.
     * @param   f               Widget flags.
     */
    VBoxDbgStats(PVM pVM, const char *pszPat = NULL, unsigned uRefreshRate= 0, QWidget *pParent = NULL, const char *pszName = NULL, WFlags f = 0);

    /** Destructor. */
    virtual ~VBoxDbgStats();

protected slots:
    /** Apply the activated combobox pattern. */
    void apply(const QString &Str);
    /** The "All" button was pressed. */
    void applyAll();
    /** Refresh the data on timer tick and pattern changed. */
    void refresh();
    /**
     * Set the refresh rate.
     *
     * @param   iRefresh        The refresh interval in seconds.
     */
    void setRefresh(int iRefresh);

protected:

    /** The current selection pattern. */
    QString             m_PatStr;
    /** The pattern combo box. */
    QComboBox          *m_pPatCB;
    /** The refresh rate in seconds.
     * 0 means not to refresh. */
    unsigned            m_uRefreshRate;
    /** The refresh timer .*/
    QTimer             *m_pTimer;
    /** The tree view widget. */
    VBoxDbgStatsView   *m_pView;
};


#endif
