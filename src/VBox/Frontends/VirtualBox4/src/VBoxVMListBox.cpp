/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxVMListItem class implementation
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

#include "VBoxVMListBox.h"

#include "VBoxProblemReporter.h"

#include <qpainter.h>
#include <qfontmetrics.h>
#include <qdrawutil.h>
#include <qtooltip.h>
#include <qmetaobject.h>
#include <qstyle.h>

#include <qfileinfo.h>
//Added by qt3to4:
#include <QPixmap>
#include <q3mimefactory.h>
#include <QFocusEvent>

#if defined (Q_WS_MAC)
# include <Carbon/Carbon.h>
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

////////////////////////////////////////////////////////////////////////////////
// VBoxVMListBoxItem class
////////////////////////////////////////////////////////////////////////////////

#warning port me
//class VBoxVMListBoxTip : public QToolTip
//{
//public:
//
//    VBoxVMListBoxTip (VBoxVMListBox *aLB, QToolTipGroup *aTG = 0)
//        : QToolTip (aLB, aTG)
//    {}
//
//    virtual ~VBoxVMListBoxTip()
//    {
//        remove (parentWidget());
//    }
//
//    void maybeTip (const QPoint &aPnt);
//};
//
//void VBoxVMListBoxTip::maybeTip (const QPoint &aPnt)
//{
//    const VBoxVMListBox *lb = static_cast <VBoxVMListBox *> (parentWidget());
//
//    VBoxVMListBoxItem *vmi = static_cast <VBoxVMListBoxItem *> (lb->itemAt (aPnt));
//    if (!vmi)
//        return;
//
//    if (parentWidget()->topLevelWidget()->inherits ("QMainWindow"))
//    {
//        /*
//         *  Ensure the main window doesn't show the text from the previous
//         *  tooltip in the status bar.
//         */
//        QToolTipGroup *toolTipGroup =
//            (::qt_cast <Q3MainWindow *> (parentWidget()->topLevelWidget()))->
//                toolTipGroup();
//        if (toolTipGroup)
//        {
//            int index = toolTipGroup->metaObject()->findSignal("removeTip()", false);
//            toolTipGroup->qt_emit (index, 0);
//        }
//    }
//
//    tip (lb->itemRect (vmi), vmi->toolTipText());
//}

////////////////////////////////////////////////////////////////////////////////
// VBoxVMListBox class
////////////////////////////////////////////////////////////////////////////////

/**
 *  Creates a box with the list of all virtual machines in the
 *  VirtualBox installation.
 */
VBoxVMListBox::
VBoxVMListBox (QWidget *aParent /* = 0 */, const char *aName /* = NULL */,
               Qt::WFlags aFlags /* = 0 */)
    : Q3ListBox (aParent, aName, aFlags)
{
    mVBox = vboxGlobal().virtualBox();

    mNameFont = QFont (font().family(), font().pointSize() + 1, QFont::Bold);
    mShotFont = QFont (font().family(), font().pointSize() + 1);
    mStateBusyFont = font();
    mStateBusyFont.setItalic (true);

    mMargin = QMAX (fontMetrics().width (' ') * 2, 8);

#warning port me
//    mToolTip = new VBoxVMListBoxTip (this);

    mGaveFocusToPopup = false;

    refresh();
}

VBoxVMListBox::~VBoxVMListBox()
{
#warning port me
//    delete mToolTip;
}

// public members
////////////////////////////////////////////////////////////////////////////////

/**
 *  Refreshes the box contents by rereading the list of VM's using
 *  the IVirtualBox instance.
 */
void VBoxVMListBox::refresh()
{
    clear();

    CMachineVector vec = mVBox.GetMachines2();
    for (CMachineVector::ConstIterator m = vec.begin();
         m != vec.end(); ++ m)
        new VBoxVMListBoxItem (this, *m);

    sort();
    setCurrentItem (0);
}

/**
 *  Refreshes the item corresponding to the given UUID.
 */
void VBoxVMListBox::refresh (const QUuid &aID)
{
    for (uint i = 0; i < count(); i++)
    {
        VBoxVMListBoxItem *vmi = (VBoxVMListBoxItem *) Q3ListBox::item (i);
        if (vmi->id() == aID)
        {
            vmi->recache();
            /* we call triggerUpdate() instead of updateItem() to cause the
             * layout to be redone for the case when the item width changes */
            triggerUpdate (true);
            break;
        }
    }
}

/**
 *  Returns the list item with the given UUID.
 */
VBoxVMListBoxItem *VBoxVMListBox::item (const QUuid &aID)
{
    for (uint i = 0; i < count(); i++)
    {
        VBoxVMListBoxItem *vmi = (VBoxVMListBoxItem *) Q3ListBox::item (i);
        if (vmi->id() == aID)
            return vmi;
    }
    return 0;
}

/**
 *  Returns the color group QListBox uses to prepare a place for painting
 *  a list box item (for example, to draw the highlighted rectangle of the
 *  currently selected item, etc.). Useful in item's QListBoxItem::paint()
 *  reimplementations to select correct colors that will contrast with the
 *  item rectangle.
 */
const QColorGroup &VBoxVMListBox::activeColorGroup() const
{
    /* Too bad QListBox doesn't allow to determine what color group it uses
     * to draw the item's rectangle before calling its paint() method. Not
     * knowing that, we cannot choose correct colors that will contrast with
     * that rectangle. So, we have to use the logic of QListBox itself.
     * Here is a modified extract from qlistbox.cpp: */

#warning port me
//    bool drawActiveSelection =
//        !style().styleHint (QStyle::SH_ItemView_ChangeHighlightOnFocus, this) ||
//        hasFocus() ||
//        mGaveFocusToPopup;
//
//    return (drawActiveSelection ? colorGroup() : palette().inactive());
    return colorGroup();
}

// protected members
////////////////////////////////////////////////////////////////////////////////

void VBoxVMListBox::focusInEvent (QFocusEvent *aE)
{
    mGaveFocusToPopup = false;
    Q3ListBox::focusInEvent (aE);
}

void VBoxVMListBox::focusOutEvent (QFocusEvent *aE)
{
    /* A modified extract from qlistbox.cpp (see #activeColorGroup()): */
    mGaveFocusToPopup =
#warning port me: check this
        aE->reason() == QFocusEvent::Popup ||
//        QFocusEvent::reason() == QFocusEvent::Popup ||
        (qApp->focusWidget() && qApp->focusWidget()->inherits ("QMenuBar"));
    Q3ListBox::focusOutEvent (aE);
}

// private members
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// VBoxVMListBoxItem class
////////////////////////////////////////////////////////////////////////////////

VBoxVMListBoxItem::VBoxVMListBoxItem (VBoxVMListBox *aLB, const CMachine &aM)
    : Q3ListBoxItem (aLB), mMachine (aM)
{
    recache();
}

VBoxVMListBoxItem::~VBoxVMListBoxItem()
{
}

// public members
////////////////////////////////////////////////////////////////////////////////

void VBoxVMListBoxItem::setMachine (const CMachine &aM)
{
    Assert (!aM.isNull());

    mMachine = aM;
    recache();
    vmListBox()->triggerUpdate (true);
}

void VBoxVMListBoxItem::recache()
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
        QString name = fi.extension().lower() == "xml" ?
                       fi.baseName (true) : fi.fileName();
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

    if (needsResort)
        vmListBox()->sort();
}

QString VBoxVMListBoxItem::toolTipText() const
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
        toolTip = QString (VBoxVMListBox::tr (
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
        toolTip = QString (VBoxVMListBox::tr (
            "<nobr><b>%1</b><br></nobr>"
            "<nobr>Inaccessible since %2</nobr>",
            "Inaccessible VM tooltip (name, last state change)"))
            .arg (mSettingsFile)
            .arg (dateTime);
    }

    return toolTip;
}

int VBoxVMListBoxItem::width (const Q3ListBox *) const
{
    /* see the picture below for dimensions */

    const VBoxVMListBox *lb = vmListBox();

    QFontMetrics fmName = QFontMetrics (lb->nameFont());
    QFontMetrics fmShot = QFontMetrics (lb->shotFont());
    QFontMetrics fmState = QFontMetrics (lb->stateFont (mSessionState));
    const int marg = lb->margin();

    QPixmap pmOSType;
    QPixmap pmState;
    QString strState;

    if (mAccessible)
    {
        pmOSType = vboxGlobal().vmGuestOSTypeIcon (mOSTypeId);
        pmState = vboxGlobal().toIcon (mState);
        strState = vboxGlobal().toString (mState);
    }
    else
    {
        /// @todo (r=dmik) temporary
        pmOSType = QPixmap (":/os_other.png");
        pmState = QPixmap (":/state_aborted_16px.png");
        strState = VBoxVMListBox::tr ("Inaccessible");
    }

    int nameWidth = fmName.width (mName);
    if (!mSnapshotName.isNull())
        nameWidth += fmShot.width (QString (" (%1)").arg (mSnapshotName));
    int stateWidth = pmState.width() + fmState.width (' ') +
                     fmState.width (strState);

    return marg + pmOSType.width() + marg * 3 / 2 +
           QMAX (nameWidth, stateWidth) + marg;
}

int VBoxVMListBoxItem::height (const Q3ListBox *) const
{
    /* see the picture below for dimensions */

    const VBoxVMListBox *lb = vmListBox();

    QFontMetrics fmName = QFontMetrics (lb->nameFont());
    QFontMetrics fmState = QFontMetrics (lb->stateFont (mSessionState));
    const int marg = lb->margin();

    QPixmap pmOSType;
    QPixmap pmState;

    if (mAccessible)
    {
        pmOSType = vboxGlobal().vmGuestOSTypeIcon (mOSTypeId);
        pmState = vboxGlobal().toIcon (mState);
    }
    else
    {
        /// @todo (r=dmik) temporary
        pmOSType = QPixmap (":/os_other.png");
        pmState = QPixmap (":/state_aborted_16px.png");
    }

    int strHeight = fmName.lineSpacing() +
                    QMAX (pmState.height(), fmState.height());

    return marg + QMAX (pmOSType.height(), strHeight) + marg;
}

/**
 * Returns @a true if we can activate and bring the VM console window to
 * foreground, and @a false otherwise.
 */
bool VBoxVMListBoxItem::canSwitchTo() const
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
bool VBoxVMListBoxItem::switchTo()
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

// protected members
////////////////////////////////////////////////////////////////////////////////

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

void VBoxVMListBoxItem::paint (QPainter *aP)
{
    const VBoxVMListBox *lb = vmListBox();

    QFontMetrics fmName = QFontMetrics (lb->nameFont());
    QFontMetrics fmShot = QFontMetrics (lb->shotFont());
    QFontMetrics fmState = QFontMetrics (lb->stateFont (mSessionState));
    const int marg = lb->margin();

    QPixmap pmOSType;
    QPixmap pmState;
    QString strState;

    if (mAccessible)
    {
        pmOSType = vboxGlobal().vmGuestOSTypeIcon (mOSTypeId);
        pmState = vboxGlobal().toIcon (mState);
        strState = vboxGlobal().toString (mState);
    }
    else
    {
        /// @todo (r=dmik) temporary
        pmOSType = QPixmap (":/os_other.png");
        pmState = QPixmap (":/state_aborted_16px.png");
        strState = VBoxVMListBox::tr ("Inaccessible");
    }

    const QPalette &pal = lb->palette();

    int osTypeY = marg;
    int strHeight = fmName.lineSpacing() +
                    QMAX (pmState.height(), fmState.height());
    if (strHeight > pmOSType.height())
        osTypeY += (strHeight - pmOSType.height()) / 2;

    /* draw the OS type icon with border, vertically centered */
    aP->drawPixmap (marg, osTypeY, pmOSType);

    aP->setPen (isSelected() ?
                pal.color (QPalette::Active, QPalette::HighlightedText) :
                pal.color (QPalette::Active, QPalette::Text));

    int textX = marg + pmOSType.width() + marg * 3 / 2;

    /* draw the VM name */
    aP->setFont (lb->nameFont());
    aP->drawText (textX, marg + fmName.ascent(), mName);

    if (!mSnapshotName.isNull())
    {
        int nameWidth = fmName.width (mName);
        aP->setFont (lb->shotFont());
        QString shotName = QString (" (%1)").arg (mSnapshotName);
        aP->drawText (textX + nameWidth, marg + fmName.ascent(), shotName);
    }

    int stateY = marg + fmName.lineSpacing();
    int stateH = QMAX (pmState.height(), fmState.height());

    /* draw the VM state icon */
    aP->drawPixmap (textX, stateY + (stateH - pmState.height()) / 2,
                    pmState);

    /* draw the VM state text */
    aP->setFont (lb->stateFont (mSessionState));
    aP->drawText (textX + pmState.width() + fmState.width(' '),
                  stateY + (stateH - fmState.height()) / 2 + fmState.ascent(),
                  strState);
}
