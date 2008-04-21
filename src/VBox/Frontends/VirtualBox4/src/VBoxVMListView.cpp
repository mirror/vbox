/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxVMItem, VBoxVMModel, VBoxVMListView, VBoxVMItemPainter class implementation
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#include "VBoxVMListView.h"
#include "VBoxProblemReporter.h"

/* Qt includes */
#include <QPainter>
#include <QFileInfo>

#if defined (Q_WS_MAC)
# include <Carbon/Carbon.h>
# if MAC_LEOPARD_STYLE
#  include <qmacstyle_mac.h>
# endif /* MAC_LEOPARD_STYLE */
#endif


// Helpers
////////////////////////////////////////////////////////////////////////////////

/// @todo Remove. See @c todo in #switchTo() below.
#if 0

#if defined (Q_WS_WIN32)

struct EnumWindowsProcData
{
    ULONG pid;
    WId wid;
};

BOOL CALLBACK EnumWindowsProc (HWND hwnd, LPARAM lParam)
{
    EnumWindowsProcData *d = (EnumWindowsProcData *) lParam;

    DWORD pid = 0;
    GetWindowThreadProcessId (hwnd, &pid);

    if (d->pid == pid)
    {
        WINDOWINFO info;
        if (!GetWindowInfo (hwnd, &info))
            return TRUE;

#if 0
        LogFlowFunc (("pid=%d, wid=%08X\n", pid, hwnd));
        LogFlowFunc (("  parent=%08X\n", GetParent (hwnd)));
        LogFlowFunc (("  owner=%08X\n", GetWindow (hwnd, GW_OWNER)));
        TCHAR buf [256];
        LogFlowFunc (("  rcWindow=%d,%d;%d,%d\n",
                      info.rcWindow.left, info.rcWindow.top,
                      info.rcWindow.right, info.rcWindow.bottom));
        LogFlowFunc (("  dwStyle=%08X\n", info.dwStyle));
        LogFlowFunc (("  dwExStyle=%08X\n", info.dwExStyle));
        GetClassName (hwnd, buf, 256);
        LogFlowFunc (("  class=%ls\n", buf));
        GetWindowText (hwnd, buf, 256);
        LogFlowFunc (("  text=%ls\n", buf));
#endif

        /* we are interested in unowned top-level windows only */
        if (!(info.dwStyle & (WS_CHILD | WS_POPUP)) &&
            info.rcWindow.left < info.rcWindow.right &&
            info.rcWindow.top < info.rcWindow.bottom &&
            GetParent (hwnd) == NULL &&
            GetWindow (hwnd, GW_OWNER) == NULL)
        {
            d->wid = hwnd;
            /* if visible, stop the search immediately */
            if (info.dwStyle & WS_VISIBLE)
                return FALSE;
            /* otherwise, give other top-level windows a chance
             * (the last one wins) */
        }
    }

    return TRUE;
}

#endif

/**
 * Searches for a main window of the given process.
 *
 * @param aPid process ID to search for
 *
 * @return window ID on success or <tt>(WId) ~0</tt> otherwise.
 */
static WId FindWindowIdFromPid (ULONG aPid)
{
#if defined (Q_WS_WIN32)

    EnumWindowsProcData d = { aPid, (WId) ~0 };
    EnumWindows (EnumWindowsProc, (LPARAM) &d);
    LogFlowFunc (("SELECTED wid=%08X\n", d.wid));
    return d.wid;

#elif defined (Q_WS_X11)

    NOREF (aPid);
    return (WId) ~0;

#elif defined (Q_WS_MAC)

    /** @todo Figure out how to get access to another windows of another process...
     * Or at least check that it's not a VBoxVRDP process. */
    NOREF (aPid);
    return (WId) 0;

#else

    return (WId) ~0;

#endif
}

#endif

VBoxVMItem::VBoxVMItem (const CMachine &aM)
    : mMachine (aM)
{
    recache();
}

VBoxVMItem::~VBoxVMItem()
{
}

// public members
////////////////////////////////////////////////////////////////////////////////

QString VBoxVMItem::sessionStateName() const 
{ 
    return mAccessible ? vboxGlobal().toString (mState) : VBoxVMListView::tr ("Inaccessible"); 
}

QString VBoxVMItem::toolTipText() const
{
    QString dateTime = (mLastStateChange.date() == QDate::currentDate()) ?
                        mLastStateChange.time().toString (Qt::LocalDate) :
                        mLastStateChange.toString (Qt::LocalDate);

    QString toolTip;

    if (mAccessible)
    {
        toolTip = QString ("<b>%1</b>").arg (mName);
        if (!mSnapshotName.isNull())
            toolTip += QString (" (%1)").arg (mSnapshotName);
        toolTip = QString (VBoxVMListView::tr (
            "<nobr>%1<br></nobr>"
            "<nobr>%2 since %3</nobr><br>"
            "<nobr>Session %4</nobr>",
            "VM tooltip (name, last state change, session state)"))
            .arg (toolTip)
            .arg (vboxGlobal().toString (mState))
            .arg (dateTime)
            .arg (vboxGlobal().toString (mSessionState));
    }
    else
    {
        toolTip = QString (VBoxVMListView::tr (
            "<nobr><b>%1</b><br></nobr>"
            "<nobr>Inaccessible since %2</nobr>",
            "Inaccessible VM tooltip (name, last state change)"))
            .arg (mSettingsFile)
            .arg (dateTime);
    }

    return toolTip;
}

bool VBoxVMItem::recache()
{
    bool needsResort = true;

    mId = mMachine.GetId();
    mSettingsFile = mMachine.GetSettingsFilePath();

    mAccessible = mMachine.GetAccessible();
    if (mAccessible)
    {
        QString name = mMachine.GetName();
        CSnapshot snp = mMachine.GetCurrentSnapshot();
        mSnapshotName = snp.isNull() ? QString::null : snp.GetName();
        needsResort = name != mName;
        mName = name;

        mState = mMachine.GetState();
        mLastStateChange.setTime_t (mMachine.GetLastStateChange() / 1000);
        mSessionState = mMachine.GetSessionState();
        mOSTypeId = mMachine.GetOSTypeId();
        mSnapshotCount = mMachine.GetSnapshotCount();

        if (mState >= KMachineState_Running)
        {
            mPid = mMachine.GetSessionPid();
    /// @todo Remove. See @c todo in #switchTo() below.
#if 0
            mWinId = FindWindowIdFromPid (mPid);
#endif
        }
        else
        {
            mPid = (ULONG) ~0;
    /// @todo Remove. See @c todo in #switchTo() below.
#if 0
            mWinId = (WId) ~0;
#endif
        }
    }
    else
    {
        mAccessError = mMachine.GetAccessError();

        /* this should be in sync with
         * VBoxProblemReporter::confirmMachineDeletion() */
        QFileInfo fi (mSettingsFile);
        QString name = fi.completeSuffix().toLower() == "xml" ?
                       fi.completeBaseName() : fi.fileName();
        needsResort = name != mName;
        mName = name;
        mState = KMachineState_Null;
        mSessionState = KSessionState_Null;
        mLastStateChange = QDateTime::currentDateTime();
        mOSTypeId = QString::null;
        mSnapshotCount = 0;

        mPid = (ULONG) ~0;
    /// @todo Remove. See @c todo in #switchTo() below.
#if 0
        mWinId = (WId) ~0;
#endif
    }

    return needsResort;
}

/**
 * Returns @a true if we can activate and bring the VM console window to
 * foreground, and @a false otherwise.
 */
bool VBoxVMItem::canSwitchTo() const
{
    return const_cast <CMachine &> (mMachine).CanShowConsoleWindow();

    /// @todo Remove. See @c todo in #switchTo() below.
#if 0
    return mWinId != (WId) ~0;
#endif
}

/**
 * Tries to switch to the main window of the VM process.
 *
 * @return true if successfully switched and false otherwise.
 */
bool VBoxVMItem::switchTo()
{
#ifdef Q_WS_MAC
    ULONG64 id = mMachine.ShowConsoleWindow();
#else
    WId id = (WId) mMachine.ShowConsoleWindow();
#endif
    AssertWrapperOk (mMachine);
    if (!mMachine.isOk())
        return false;

    /* winId = 0 it means the console window has already done everything
     * necessary to implement the "show window" semantics. */
    if (id == 0)
        return true;

#if defined (Q_WS_WIN32) || defined (Q_WS_X11)

    return vboxGlobal().activateWindow (id, true);

#elif defined (Q_WS_MAC)
    /*
     * This is just for the case were the other process cannot steal
     * the focus from us. It will send us a PSN so we can try.
     */
    ProcessSerialNumber psn;
    psn.highLongOfPSN = id >> 32;
    psn.lowLongOfPSN = (UInt32)id;
    OSErr rc = ::SetFrontProcess (&psn);
    if (!rc)
        Log (("GUI: %#RX64 couldn't do SetFrontProcess on itself, the selector (we) had to do it...\n", id));
    else
        Log (("GUI: Failed to bring %#RX64 to front. rc=%#x\n", id, rc));
    return !rc;

#endif

    return false;

    /// @todo Below is the old method of switching to the console window
    //  based on the process ID of the console process. It should go away
    //  after the new (callback-based) method is fully tested.
#if 0

    if (!canSwitchTo())
        return false;

#if defined (Q_WS_WIN32)

    HWND hwnd = mWinId;

    /* if there are ownees (modal and modeless dialogs, etc), find the
     * topmost one */
    HWND hwndAbove = NULL;
    do
    {
        hwndAbove = GetNextWindow (hwnd, GW_HWNDPREV);
        HWND hwndOwner;
        if (hwndAbove != NULL &&
            ((hwndOwner = GetWindow (hwndAbove, GW_OWNER)) == hwnd ||
             hwndOwner  == hwndAbove))
            hwnd = hwndAbove;
        else
            break;
    }
    while (1);

    /* first, check that the primary window is visible */
    if (IsIconic (mWinId))
        ShowWindow (mWinId, SW_RESTORE);
    else if (!IsWindowVisible (mWinId))
        ShowWindow (mWinId, SW_SHOW);

#if 0
    LogFlowFunc (("mWinId=%08X hwnd=%08X\n", mWinId, hwnd));
#endif

    /* then, activate the topmost in the group */
    AllowSetForegroundWindow (mPid);
    SetForegroundWindow (hwnd);

    return true;

#elif defined (Q_WS_X11)

    return false;

#elif defined (Q_WS_MAC)

    ProcessSerialNumber psn;
    OSStatus rc = ::GetProcessForPID (mPid, &psn);
    if (!rc)
    {
        rc = ::SetFrontProcess (&psn);

        if (!rc)
        {
            ShowHideProcess (&psn, true);
            return true;
        }
    }
    return false;

#else

    return false;

#endif

#endif
}

/* VBoxVMModel class */

void VBoxVMModel::addItem (VBoxVMItem *aItem) 
{ 
    Assert (aItem);
    int row = mVMItemList.count();
    emit layoutAboutToBeChanged();
    beginInsertRows (QModelIndex(), row, row);
    mVMItemList << aItem; 
    endInsertRows();
    refreshItem (aItem);
}

void VBoxVMModel::removeItem (VBoxVMItem *aItem)
{
    Assert (aItem);
    int row = mVMItemList.indexOf (aItem);
    removeRows (row, 1);
}

bool VBoxVMModel::removeRows (int aRow, int aCount, const QModelIndex &aParent /* = QModelIndex() */)
{
    emit layoutAboutToBeChanged();
    beginRemoveRows (aParent, aRow, aRow + aCount);
    mVMItemList.erase (mVMItemList.begin() + aRow, mVMItemList.begin() + aRow + aCount);
    endRemoveRows();
    emit layoutChanged();
    return true;
}

/**
 *  Refreshes the item corresponding to the given UUID.
 */
void VBoxVMModel::refreshItem (VBoxVMItem *aItem)
{
    Assert (aItem);
    if (aItem->recache())
        sort();
    itemChanged (aItem);
}

void VBoxVMModel::itemChanged (VBoxVMItem *aItem)
{
    Assert (aItem);
    int row = mVMItemList.indexOf (aItem);
    /* Emit an layout change signal for the case some dimensions of the item
     * has changed also. */
    emit layoutChanged();
    /* Emit an data changed signal. */
    emit dataChanged (index (row), index (row));
}

/**
 *  Refreshes the model contents by rereading the list of VM's using the
 *  IVirtualBox instance.
 */
void VBoxVMModel::refresh()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    CMachineVector vec = vbox.GetMachines2();
    for (CMachineVector::ConstIterator m = vec.begin();
         m != vec.end(); ++ m)
        addItem(new VBoxVMItem (*m));

    sort();
}

/**
 *  Returns the list item with the given UUID.
 */
VBoxVMItem *VBoxVMModel::itemById (const QUuid &aId) const
{
    foreach (VBoxVMItem *item, mVMItemList)
        if (item->id() == aId)
            return item;
    return NULL;
}

QModelIndex VBoxVMModel::indexById (const QUuid &aId) const
{
    int row = rowById (aId);
    if (row >= 0)
        return index (row);
    else
        return QModelIndex();
}

int VBoxVMModel::rowById (const QUuid &aId) const
{
    for (int i=0; i < mVMItemList.count(); ++i)
    {
        VBoxVMItem *item = mVMItemList.at (i);
        if (item->id() == aId)
            return i;
    }
    return -1;
}

void VBoxVMModel::sort (int /* aColumn */, Qt::SortOrder aOrder /* = Qt::AscendingOrder */)
{
    emit layoutAboutToBeChanged();
    switch (aOrder)
    {
        case Qt::AscendingOrder: qSort (mVMItemList.begin(), mVMItemList.end(), VBoxVMItemNameCompareLessThan); break;
        case Qt::DescendingOrder: qSort (mVMItemList.begin(), mVMItemList.end(), VBoxVMItemNameCompareGreaterThan); break;
    }
    emit layoutChanged();
}

int VBoxVMModel::rowCount(const QModelIndex & /* aParent = QModelIndex() */) const 
{ 
    return mVMItemList.count(); 
}

QVariant VBoxVMModel::data(const QModelIndex &aIndex, int aRole) const
{
    if (!aIndex.isValid())
        return QVariant();

    if (aIndex.row() >= mVMItemList.size())
        return QVariant();

    QVariant v;
    switch (aRole)
    {
        case Qt::DisplayRole:
            {
                v = mVMItemList.at (aIndex.row())->name();
                break;
            }
        case Qt::DecorationRole:
            {
                v = mVMItemList.at (aIndex.row())->osIcon();
                break;
            }
        case Qt::ToolTipRole:
            {
                v = mVMItemList.at (aIndex.row())->toolTipText();
                break;
            }
        case Qt::FontRole:
            {
                QFont f = qApp->font();
                f.setPointSize (f.pointSize() + 1);
                f.setWeight (QFont::Bold);
                v = f;
                break;
            }
        case Qt::AccessibleTextRole:
            {
                VBoxVMItem *item = mVMItemList.at (aIndex.row());
                v = QString (VBoxVMListView::tr ("%1 (%2)\n %3", "Accessible string of the list view item")
                             .arg (item->name())
                             .arg (item->snapshotName())
                             .arg (item->sessionStateName()));
                break;
            }
        case SnapShotDisplayRole:
            {
                v = mVMItemList.at (aIndex.row())->snapshotName();
                break;
            }
        case SnapShotFontRole:
            {
                QFont f = qApp->font();
                v = f;
                break;
            }
        case SessionStateDisplayRole:
            {
                v = mVMItemList.at (aIndex.row())->sessionStateName();
                break;
            }
        case SessionStateDecorationRole:
            {
                v = mVMItemList.at (aIndex.row())->sessionStateIcon();
                break;
            }
        case SessionStateFontRole:
            {
                QFont f = qApp->font();
                f.setPointSize (f.pointSize());
                if (mVMItemList.at (aIndex.row())->sessionState() != KSessionState_Closed)
                    f.setItalic (true);
                v = f;
                break;
            }
        case VBoxVMItemPtrRole:
            {
                v = qVariantFromValue (mVMItemList.at (aIndex.row()));
            }
    }
    return v;
}

QVariant VBoxVMModel::headerData(int aSection, Qt::Orientation aOrientation,
                                 int aRole /* = Qt::DisplayRole */) const
{
    if (aRole != Qt::DisplayRole)
        return QVariant();

    if (aOrientation == Qt::Horizontal)
        return QString (VBoxVMListView::tr ("VM", "Horizontal header description"));
    else
        return QString ("%1").arg (aSection);
}

bool VBoxVMModel::VBoxVMItemNameCompareLessThan (VBoxVMItem* aItem1, VBoxVMItem* aItem2)
{
    Assert (aItem1);
    Assert (aItem2);
    return aItem1->name().toLower() < aItem2->name().toLower();
}

bool VBoxVMModel::VBoxVMItemNameCompareGreaterThan (VBoxVMItem* aItem1, VBoxVMItem* aItem2)
{
    Assert (aItem1);
    Assert (aItem2);
    return aItem2->name().toLower() < aItem1->name().toLower();
}

/* VBoxVMListView class */

VBoxVMListView::VBoxVMListView (QWidget *aParent /* = 0 */)
    :QListView (aParent)
{
    /* Create & set our delegation class */
    VBoxVMItemPainter *delegate = new VBoxVMItemPainter(this);
    setItemDelegate(delegate);
    /* Default icon size */
    setIconSize (QSize (32, 32));
    /* Publish the activation of items */
    connect (this, SIGNAL (activated (const QModelIndex &)),
             this, SIGNAL (activated ()));
    /* Track if the application lost the focus */
#if MAC_LEOPARD_STYLE
    connect (QCoreApplication::instance(), SIGNAL (focusChanged (QWidget *, QWidget *)),
             this, SLOT (focusChanged (QWidget *, QWidget *)));
    /* 1 pixel line frame */
    setMidLineWidth (1);
    setLineWidth (0);
    setFrameShape (QFrame::Box);
    focusChanged (NULL, this);
    /* Nesty hack to disable the focus rect on the list view. This interface
     * may change at any time! */
    static_cast<QMacStyle *> (style())->setFocusRectPolicy (this, QMacStyle::FocusDisabled);
#endif /* MAC_LEOPARD_STYLE */
}

void VBoxVMListView::selectItemByRow (int row)
{
    setCurrentIndex (model()->index (row, 0));
}

void VBoxVMListView::selectItemById (const QUuid &aID)
{
    if (VBoxVMModel *m = qobject_cast<VBoxVMModel*> (model()))
    {
        QModelIndex i = m->indexById (aID);
        if (i.isValid())
            setCurrentIndex (i);
    }
}

void VBoxVMListView::ensureSomeRowSelected (int aRowHint)
{
    VBoxVMItem *item = selectedItem();
    if(!item)
    {
        aRowHint = qBound (0, aRowHint, model()->rowCount() - 1);
        selectItemByRow (aRowHint);
        item = selectedItem ();
        if (!item)
            selectItemByRow (0);
    }
}

VBoxVMItem * VBoxVMListView::selectedItem() const
{
    QModelIndexList indexes = selectedIndexes();
    if (indexes.isEmpty())
        return NULL;
    return model()->data (indexes.first(), VBoxVMModel::VBoxVMItemPtrRole).value<VBoxVMItem *>();
}

void VBoxVMListView::ensureCurrentVisible()
{
    scrollTo (currentIndex(), QAbstractItemView::EnsureVisible);
}

void VBoxVMListView::selectionChanged (const QItemSelection &aSelected, const QItemSelection &aDeselected) 
{
    QListView::selectionChanged (aSelected, aDeselected);
    selectCurrent();
    ensureCurrentVisible();
    emit currentChanged(); 
}

void VBoxVMListView::currentChanged (const QModelIndex &aCurrent, const QModelIndex &aPrevious) 
{ 
    QListView::currentChanged (aCurrent, aPrevious);
    selectCurrent();
    ensureCurrentVisible();
    emit currentChanged(); 
} 

void VBoxVMListView::dataChanged (const QModelIndex &aTopLeft, const QModelIndex &aBottomRight)
{
    QListView::dataChanged (aTopLeft, aBottomRight);
    selectCurrent();
    ensureCurrentVisible();
    emit currentChanged(); 
}

void VBoxVMListView::focusChanged (QWidget * /* aOld */, QWidget *aNow)
{
#if MAC_LEOPARD_STYLE
    QColor bgColor (212, 221, 229);
    if (aNow == NULL)
        bgColor.setRgb (232, 232, 232);
    QPalette pal = viewport()->palette();
    pal.setColor (QPalette::Base, bgColor);
    viewport()->setPalette (pal);
    viewport()->setAutoFillBackground (true);
#else /* MAC_LEOPARD_STYLE */
    NOREF (aNow);
#endif /* MAC_LEOPARD_STYLE */
}

void VBoxVMListView::mousePressEvent (QMouseEvent *aEvent)
{
    /* First process event, in the case the user select a new item */
    QListView::mousePressEvent (aEvent);
    if (aEvent->button() == Qt::RightButton)
    {
        /* Send a context menu request if necessary */
        const QModelIndex &index = indexAt (aEvent->pos());
        if (index.isValid())
        {
            VBoxVMItem *item = model()->data (index, VBoxVMModel::VBoxVMItemPtrRole).value<VBoxVMItem *>();
            emit contextMenuRequested (item, aEvent->globalPos());
        }
    }
}

bool VBoxVMListView::selectCurrent()
{
    QModelIndexList indexes = selectionModel()->selectedIndexes();
    if (indexes.isEmpty() ||
        indexes.first() != currentIndex())
    {
        /* Make sure that the current is always selected */
        selectionModel()->select (currentIndex(), QItemSelectionModel::Current | QItemSelectionModel::ClearAndSelect);
        return true;
    }
    return false;
}

/* VBoxVMItemPainter class */
/*
 +----------------------------------------------+
 |       marg                                   |
 |   +----------+   m                           |
 | m |          | m a  name_string___________ m |
 | a |  OSType  | a r                         a |
 | r |  icon    | r g  +--+                   r |
 | g |          | g /  |si|  state_string     g |
 |   +----------+   2  +--+                     |
 |       marg                                   |
 +----------------------------------------------+

 si = state icon

*/

/* Little helper class for layout calculation */
class QRectList: public QList<QRect *>
{
public:
    void alignVCenterTo (QRect* aWhich)
    {
        QRect b;
        foreach (QRect *rect, *this)
            if(rect != aWhich)
                b |= *rect;
        if (b.width() > aWhich->width())
            aWhich->moveCenter (QPoint (aWhich->center().x(), b.center().y()));
        else
        {
            foreach (QRect *rect, *this)
                if(rect != aWhich)
                    rect->moveCenter (QPoint (rect->center().x(), aWhich->center().y()));
        }
    }
};

QSize VBoxVMItemPainter::sizeHint (const QStyleOptionViewItem &aOption,
                                   const QModelIndex &aIndex) const
{
    /* Get the size of every item */
    QRect osTypeRT = rect (aOption, aIndex, Qt::DecorationRole);
    QRect vmNameRT = rect (aOption, aIndex, Qt::DisplayRole);
    QRect shotRT = rect (aOption, aIndex, VBoxVMModel::SnapShotDisplayRole);
    QRect stateIconRT = rect (aOption, aIndex, VBoxVMModel::SessionStateDecorationRole);
    QRect stateRT = rect (aOption, aIndex, VBoxVMModel::SessionStateDisplayRole);
    /* Calculate the position for every item */
    calcLayout (aIndex, &osTypeRT, &vmNameRT, &shotRT, &stateIconRT, &stateRT);
    /* Calc the bounding rect */
    const QRect boundingRect = osTypeRT | vmNameRT | shotRT | stateIconRT | stateRT;
    /* Return + left/top/right/bottom margin */
    return (boundingRect.size() + QSize (2 * mMargin, 2 * mMargin));
}

void VBoxVMItemPainter::paint (QPainter *aPainter, const QStyleOptionViewItem &aOption,
                               const QModelIndex &aIndex) const
{
    // Name and decoration
    const QString vmName = aIndex.data(Qt::DisplayRole).toString();
    const QFont nameFont = aIndex.data (Qt::FontRole).value<QFont>();
    const QPixmap osType = aIndex.data(Qt::DecorationRole).value<QIcon>().pixmap (aOption.decorationSize, iconMode (aOption.state), iconState (aOption.state));

    const QString shot = aIndex.data(VBoxVMModel::SnapShotDisplayRole).toString();
    const QFont shotFont = aIndex.data (VBoxVMModel::SnapShotFontRole).value<QFont>();

    const QString state = aIndex.data(VBoxVMModel::SessionStateDisplayRole).toString();
    const QFont stateFont = aIndex.data (VBoxVMModel::SessionStateFontRole).value<QFont>();
    const QPixmap stateIcon = aIndex.data(VBoxVMModel::SessionStateDecorationRole).value<QIcon>().pixmap (QSize (16, 16), iconMode (aOption.state), iconState (aOption.state));

    /* Get the sizes for all items */
    QRect osTypeRT = rect (aOption, aIndex, Qt::DecorationRole);
    QRect vmNameRT = rect (aOption, aIndex, Qt::DisplayRole);
    QRect shotRT = rect (aOption, aIndex, VBoxVMModel::SnapShotDisplayRole);
    QRect stateIconRT = rect (aOption, aIndex, VBoxVMModel::SessionStateDecorationRole);
    QRect stateRT = rect (aOption, aIndex, VBoxVMModel::SessionStateDisplayRole);

    /* Calculate the positions for all items */
    calcLayout (aIndex, &osTypeRT, &vmNameRT, &shotRT, &stateIconRT, &stateRT);

    /* Get the appropiate pen for the current state */
    QPalette pal = aOption.palette;
    QPen pen = pal.color (QPalette::Active, QPalette::Text);
    if (aOption.state & QStyle::State_Selected)
        pen =  pal.color (QPalette::Active, QPalette::HighlightedText);
    /* Start drawing */
    drawBackground (aPainter, aOption, aIndex);
    aPainter->save();
    /* Set the current pen */
    aPainter->setPen (pen);
    /* Move the painter to the initial position */
    aPainter->translate (aOption.rect.x(), aOption.rect.y());
    /* os type icon */
    aPainter->drawPixmap (osTypeRT, osType);
    /* vm name */
    aPainter->setFont (nameFont);
    aPainter->drawText (vmNameRT, vmName);
    /* current snapshot in braces */
    if (!shot.isEmpty())
    {
        aPainter->setFont (shotFont);
        aPainter->drawText (shotRT, QString ("(%1)").arg (shot));
    }
    /* state icon */
    aPainter->drawPixmap (stateIconRT, stateIcon);
    /* textual state */
    aPainter->setFont (stateFont);
    aPainter->drawText (stateRT, state);
    /* For debugging */
//    QRect boundingRect = osTypeRT | vmNameRT | shotRT | stateIconRT | stateRT;
//    aPainter->drawRect (boundingRect);
    aPainter->restore();
    drawFocus(aPainter, aOption, aOption.rect);
}

void VBoxVMItemPainter::drawBackground (QPainter *aPainter, const QStyleOptionViewItem &aOption, 
                                        const QModelIndex &aIndex) const
{   
#if MAC_LEOPARD_STYLE
    NOREF (aIndex);
    /* Macify for Leopard */
    if (aOption.state & QStyle::State_Selected)
    {   
        /* Standard color for selected items and focus on the widget */
        QColor topLineColor (69, 128, 200);
        QColor topGradColor (92, 147, 214);
        QColor bottomGradColor (21, 83, 169);
        /* Color for selected items and no focus on the widget */
        if (QWidget *p = qobject_cast<QWidget *> (parent()))
            if (!p->hasFocus())
            {
                topLineColor.setRgb (145, 160, 192);
                topGradColor.setRgb (162, 177, 207);
                bottomGradColor.setRgb (110, 129, 169);
            }
        /* Color for selected items and no focus on the application at all */
        if (qApp->focusWidget() == NULL)
            {
                topLineColor.setRgb (151, 151, 151);
                topGradColor.setRgb (180, 180, 180);
                bottomGradColor.setRgb (137, 137, 137);
            }
        /* Paint the background */
        QRect r = aOption.rect;
        r.setTop (r.top() + 1);
        QLinearGradient linearGrad (QPointF(0, r.top()), QPointF(0, r.bottom()));
        linearGrad.setColorAt (0, topGradColor);
        linearGrad.setColorAt (1, bottomGradColor);
        aPainter->setPen (topLineColor);
        aPainter->drawLine (r.left(), r.top() - 1, r.right(), r.top() - 1);
        aPainter->fillRect (r, linearGrad);
    }
#else /* MAC_LEOPARD_STYLE */
    QItemDelegate::drawBackground (aPainter, aOption, aIndex);
#endif /* MAC_LEOPARD_STYLE */
}

QRect VBoxVMItemPainter::rect (const QStyleOptionViewItem &aOption,
                               const QModelIndex &aIndex, int aRole) const
{
    switch (aRole)
    {
        case Qt::DisplayRole:
            {
                QString text = aIndex.data (Qt::DisplayRole).toString();
                QFontMetrics fm (fontMetric (aIndex, Qt::FontRole));
                return QRect (QPoint (0, 0), fm.size (0, text));
                break;
            }
        case Qt::DecorationRole:
            {
                QIcon icon = aIndex.data (Qt::DecorationRole).value<QIcon>();
                return QRect (QPoint (0, 0), icon.actualSize (aOption.decorationSize, iconMode (aOption.state), iconState (aOption.state)));
                break;
            }
        case VBoxVMModel::SnapShotDisplayRole:
            {
                QString text = aIndex.data (VBoxVMModel::SnapShotDisplayRole).toString();
                if (!text.isEmpty())
                {
                    QFontMetrics fm (fontMetric (aIndex, VBoxVMModel::SnapShotFontRole));
                    return QRect (QPoint (0, 0), fm.size (0, QString ("(%1)").arg (text)));
                }else
                    return QRect();
                break;
            }
        case VBoxVMModel::SessionStateDisplayRole:
            {
                QString text = aIndex.data (VBoxVMModel::SessionStateDisplayRole).toString();
                QFontMetrics fm (fontMetric (aIndex, VBoxVMModel::SessionStateFontRole));
                return QRect (QPoint (0, 0), fm.size (0, text));
                break;
            }
        case VBoxVMModel::SessionStateDecorationRole:
            {
                QIcon icon = aIndex.data (VBoxVMModel::SessionStateDecorationRole).value<QIcon>();
                return QRect (QPoint (0, 0), icon.actualSize (QSize (16, 16), iconMode (aOption.state), iconState (aOption.state)));
                break;
            }
    }
    return QRect();
}

void VBoxVMItemPainter::calcLayout (const QModelIndex &aIndex,
                                    QRect *aOSType, QRect *aVMName, QRect *aShot,
                                    QRect *aStateIcon, QRect *aState) const
{
    const int nameSpaceWidth = fontMetric (aIndex, Qt::FontRole).width (' ');
    const int stateSpaceWidth = fontMetric (aIndex, VBoxVMModel::SessionStateFontRole).width (' ');
    /* Really basic layout managment. 
     * First layout as usual */
    aOSType->moveTo (mMargin, mMargin);
    aVMName->moveTo (mMargin + aOSType->width() + mSpacing, mMargin);
    aShot->moveTo (aVMName->right() + nameSpaceWidth, aVMName->top());
    aStateIcon->moveTo (aVMName->left(), aVMName->bottom());
    aState->moveTo (aStateIcon->right() + stateSpaceWidth, aStateIcon->top());
    /* Do grouping for the automatic center routine.
     * First the states group: */
    QRectList statesLayout;
    statesLayout << aStateIcon << aState;
    /* All items in the layout: */
    QRectList allLayout;
    allLayout << aOSType << aVMName << aShot << statesLayout;
    /* Now verticaly center the items based on the reference item */
    statesLayout.alignVCenterTo (aStateIcon);
    allLayout.alignVCenterTo (aOSType);
}

