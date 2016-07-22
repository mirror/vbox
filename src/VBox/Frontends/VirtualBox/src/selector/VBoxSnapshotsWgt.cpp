/* $Id$ */
/** @file
 * VBox Qt GUI - VBoxSnapshotsWgt class implementation.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QDateTime>
# include <QHeaderView>
# include <QMenu>
# include <QScrollBar>
# include <QPointer>
# include <QApplication>

/* GUI includes: */
# include "UIIconPool.h"
# include "UIMessageCenter.h"
# include "VBoxSnapshotDetailsDlg.h"
# include "VBoxSnapshotsWgt.h"
# include "VBoxTakeSnapshotDlg.h"
# include "UIWizardCloneVM.h"
# include "UIToolBar.h"
# include "UIVirtualBoxEventHandler.h"
# include "UIConverter.h"
# include "UIModalWindowManager.h"
# include "UIExtraDataManager.h"

/* COM includes: */
# include "CConsole.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
#if QT_VERSION < 0x050000
# include <QWindowsStyle>
#endif /* QT_VERSION < 0x050000 */


/**
 *  QTreeWidgetItem subclass for snapshots items
 */
class SnapshotWgtItem : public QTreeWidgetItem
{
public:

    enum { ItemType = QTreeWidgetItem::UserType + 1 };

    /* Normal snapshot item (child of tree-widget) */
    SnapshotWgtItem (VBoxSnapshotsWgt *pSnapshotWidget, QTreeWidget *aTreeWidget, const CSnapshot &aSnapshot)
        : QTreeWidgetItem (aTreeWidget, ItemType)
        , m_pSnapshotWidget(pSnapshotWidget)
        , mIsCurrentState (false)
        , mSnapshot (aSnapshot)
    {
    }

    /* Normal snapshot item (child of tree-widget-item) */
    SnapshotWgtItem (VBoxSnapshotsWgt *pSnapshotWidget, QTreeWidgetItem *aRootItem, const CSnapshot &aSnapshot)
        : QTreeWidgetItem (aRootItem, ItemType)
        , m_pSnapshotWidget(pSnapshotWidget)
        , mIsCurrentState (false)
        , mSnapshot (aSnapshot)
    {
    }

    /* Current state item (child of tree-widget) */
    SnapshotWgtItem (VBoxSnapshotsWgt *pSnapshotWidget, QTreeWidget *aTreeWidget, const CMachine &aMachine)
        : QTreeWidgetItem (aTreeWidget, ItemType)
        , m_pSnapshotWidget(pSnapshotWidget)
        , mIsCurrentState (true)
        , mMachine (aMachine)
    {
        updateCurrentState (mMachine.GetState());
    }

    /* Current state item (child of tree-widget-item) */
    SnapshotWgtItem (VBoxSnapshotsWgt *pSnapshotWidget, QTreeWidgetItem *aRootItem, const CMachine &aMachine)
        : QTreeWidgetItem (aRootItem, ItemType)
        , m_pSnapshotWidget(pSnapshotWidget)
        , mIsCurrentState (true)
        , mMachine (aMachine)
    {
        updateCurrentState (mMachine.GetState());
    }

    QVariant data (int aColumn, int aRole) const
    {
        switch (aRole)
        {
            case Qt::DisplayRole:
                return mIsCurrentState ? QTreeWidgetItem::data (aColumn, aRole) : QVariant (QString ("%1%2")
                                         .arg (QTreeWidgetItem::data (aColumn, Qt::DisplayRole).toString())
                                         .arg (QTreeWidgetItem::data (aColumn, Qt::UserRole).toString()));
            case Qt::SizeHintRole:
            {
                /* Determine the icon metric: */
                const QStyle *pStyle = QApplication::style();
                const int iIconMetric = pStyle->pixelMetric(QStyle::PM_SmallIconSize);
                /* Determine the minimum size-hint for this tree-widget-item: */
                const QSize baseSizeHint = QTreeWidgetItem::data(aColumn, aRole).toSize();
                /* Determine the effective height-hint for this tree-widget-item: */
                const int iEffectiveHeightHint = qMax(baseSizeHint.height(),
                                                      iIconMetric + 2 * 2 /* margins */);
                /* Return size-hint for this tree-widget-item: */
                return QSize(baseSizeHint.width(), iEffectiveHeightHint);
            }
            default:
                break;
        }
        return QTreeWidgetItem::data (aColumn, aRole);
    }

    QString text (int aColumn) const
    {
        return QTreeWidgetItem::data (aColumn, Qt::DisplayRole).toString();
    }

    bool isCurrentStateItem() const
    {
        return mSnapshot.isNull();
    }

    int level()
    {
        QTreeWidgetItem *item = this;
        int result = 0;
        while (item->parent())
        {
            ++ result;
            item = item->parent();
        }
        return result;
    }

    bool bold() const { return font (0).bold(); }
    void setBold (bool aBold)
    {
        QFont myFont = font (0);
        myFont.setBold (aBold);
        setFont (0, myFont);
        adjustText();
    }

    bool italic() const { return font (0).italic(); }
    void setItalic (bool aItalic)
    {
        QFont myFont = font (0);
        myFont.setItalic (aItalic);
        setFont (0, myFont);
        adjustText();
    }

    CMachine machine() const { return mMachine; }
    CSnapshot snapshot() const { return mSnapshot; }
    QString snapshotId() const { return mId; }

    void recache()
    {
        if (mIsCurrentState)
        {
            Assert (!mMachine.isNull());
            mCurStateModified = mMachine.GetCurrentStateModified();
            setText (0, mCurStateModified ?
                        VBoxSnapshotsWgt::tr ("Current State (changed)", "Current State (Modified)") :
                        VBoxSnapshotsWgt::tr ("Current State", "Current State (Unmodified)"));
            mDesc = mCurStateModified ?
                    VBoxSnapshotsWgt::tr ("The current state differs from the state stored in the current snapshot") :
                    parent() != 0 ?
                    VBoxSnapshotsWgt::tr ("The current state is identical to the state stored in the current snapshot") :
                    QString::null;
        }
        else
        {
            Assert (!mSnapshot.isNull());
            mId = mSnapshot.GetId();
            setText (0, mSnapshot.GetName());
            mOnline = mSnapshot.GetOnline();
            setIcon(0, m_pSnapshotWidget->snapshotItemIcon(mOnline));
            mDesc = mSnapshot.GetDescription();
            mTimestamp.setTime_t (mSnapshot.GetTimeStamp() / 1000);
            mCurStateModified = false;
        }
        adjustText();
        recacheToolTip();
    }

    KMachineState getCurrentState()
    {
        if (mMachine.isNull())
            return KMachineState_Null;

        return mMachineState;
    }

    void updateCurrentState (KMachineState aState)
    {
        if (mMachine.isNull())
            return;

        setIcon(0, gpConverter->toIcon(aState));
        mMachineState = aState;
        mTimestamp.setTime_t (mMachine.GetLastStateChange() / 1000);
    }

    SnapshotAgeFormat updateAge()
    {
        QString age;

        /* Age: [date time|%1d ago|%1h ago|%1min ago|%1sec ago] */
        SnapshotAgeFormat ageFormat;
        QDateTime now = QDateTime::currentDateTime();
        QDateTime then = mTimestamp;
        if (then > now)
            then = now; /* can happen if the host time is wrong */
        if (then.daysTo (now) > 30)
        {
            age = VBoxSnapshotsWgt::tr (" (%1)").arg (then.toString (Qt::LocalDate));
            ageFormat = AgeMax;
        }
        else if (then.secsTo (now) > 60 * 60 * 24)
        {
            age = VBoxSnapshotsWgt::tr (" (%1 ago)").arg(VBoxGlobal::daysToString(then.secsTo (now) / 60 / 60 / 24));
            ageFormat = AgeInDays;
        }
        else if (then.secsTo (now) > 60 * 60)
        {
            age = VBoxSnapshotsWgt::tr (" (%1 ago)").arg(VBoxGlobal::hoursToString(then.secsTo (now) / 60 / 60));
            ageFormat = AgeInHours;
        }
        else if (then.secsTo (now) > 60)
        {
            age = VBoxSnapshotsWgt::tr (" (%1 ago)").arg(VBoxGlobal::minutesToString(then.secsTo (now) / 60));
            ageFormat = AgeInMinutes;
        }
        else
        {
            age = VBoxSnapshotsWgt::tr (" (%1 ago)").arg(VBoxGlobal::secondsToString(then.secsTo (now)));
            ageFormat = AgeInSeconds;
        }

        /* Update data */
        setData (0, Qt::UserRole, age);

        return ageFormat;
    }

private:

    void adjustText()
    {
        if (!treeWidget()) return; /* only for initialised items */
        QFontMetrics metrics (font (0));
        int hei0 = (metrics.height() > 16 ?
                   metrics.height() /* text */ : 16 /* icon */) +
                   2 * 2 /* 2 pixel per margin */;
        int wid0 = metrics.width (text (0)) /* text */ +
                   treeWidget()->indentation() /* indent */ +
                   16 /* icon */;
        setSizeHint (0, QSize (wid0, hei0));
    }

    void recacheToolTip()
    {
        QString name = text (0);

        bool dateTimeToday = mTimestamp.date() == QDate::currentDate();
        QString dateTime = dateTimeToday ?
                           mTimestamp.time().toString (Qt::LocalDate) :
                           mTimestamp.toString (Qt::LocalDate);

        QString details;

        if (!mSnapshot.isNull())
        {
            /* The current snapshot is always bold */
            if (bold())
                details = VBoxSnapshotsWgt::tr (" (current, ", "Snapshot details");
            else
                details = " (";
            details += mOnline ? VBoxSnapshotsWgt::tr ("online)", "Snapshot details")
                               : VBoxSnapshotsWgt::tr ("offline)", "Snapshot details");

            if (dateTimeToday)
                dateTime = VBoxSnapshotsWgt::tr ("Taken at %1", "Snapshot (time)").arg (dateTime);
            else
                dateTime = VBoxSnapshotsWgt::tr ("Taken on %1", "Snapshot (date + time)").arg (dateTime);
        }
        else
        {
            dateTime = VBoxSnapshotsWgt::tr ("%1 since %2", "Current State (time or date + time)")
                .arg (gpConverter->toString (mMachineState)).arg (dateTime);
        }

        QString toolTip = QString ("<nobr><b>%1</b>%2</nobr><br><nobr>%3</nobr>")
            .arg (name) .arg (details).arg (dateTime);

        if (!mDesc.isEmpty())
            toolTip += "<hr>" + mDesc;

        setToolTip (0, toolTip);
    }

    /** Holds pointer to snapshot-widget 'this' item belongs to. */
    QPointer<VBoxSnapshotsWgt> m_pSnapshotWidget;

    bool mIsCurrentState;

    CSnapshot mSnapshot;
    CMachine mMachine;

    QString mId;
    bool mOnline;
    QString mDesc;
    QDateTime mTimestamp;

    bool mCurStateModified;
    KMachineState mMachineState;
};

/**
 *  Simple guard block to prevent cyclic call caused by:
 *  changing tree-widget item content (rename) leads to snapshot update &
 *  snapshot update leads to changing tree-widget item content.
 */
class SnapshotEditBlocker
{
public:

    SnapshotEditBlocker (bool &aProtector)
        : mProtector (aProtector)
    {
        mProtector = true;
    }

   ~SnapshotEditBlocker()
    {
        mProtector = false;
    }

private:

    bool &mProtector;
};

VBoxSnapshotsWgt::VBoxSnapshotsWgt (QWidget *aParent)
    : QIWithRetranslateUI <QWidget> (aParent)
    , mCurSnapshotItem (0)
    , mEditProtector (false)
    , mSnapshotActionGroup (new QActionGroup (this))
    , mCurStateActionGroup (new QActionGroup (this))
    , mRestoreSnapshotAction (new QAction (mSnapshotActionGroup))
    , mDeleteSnapshotAction (new QAction (mSnapshotActionGroup))
    , mShowSnapshotDetailsAction (new QAction (mSnapshotActionGroup))
    , mTakeSnapshotAction (new QAction (mCurStateActionGroup))
    , mCloneSnapshotAction(new QAction(mCurStateActionGroup))
    , m_fShapshotOperationsAllowed(false)
{
    /* Apply UI decorations */
    Ui::VBoxSnapshotsWgt::setupUi (this);

    mTreeWidget->header()->hide();

#if QT_VERSION < 0x050000
    /* The snapshots widget is not very useful if there are a lot
     * of snapshots in a tree and the current Qt style decides not
     * to draw lines (branches) between the snapshot nodes; it is
     * then often unclear which snapshot is a child of another.
     * So on platforms whose styles do not normally draw branches,
     * we use QWindowsStyle which is present on every platform and
     * draws required thing like we want. */
// #if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined(RT_OS_SOLARIS)
    QWindowsStyle *treeWidgetStyle = new QWindowsStyle;
    mTreeWidget->setStyle (treeWidgetStyle);
    connect (mTreeWidget, SIGNAL (destroyed (QObject *)), treeWidgetStyle, SLOT (deleteLater()));
// #endif
#endif /* QT_VERSION < 0x050000 */

    /* Cache pixmaps: */
    m_offlineSnapshotIcon = UIIconPool::iconSet(":/snapshot_offline_16px.png");
    m_onlineSnapshotIcon = UIIconPool::iconSet(":/snapshot_online_16px.png");

    /* Determine icon metric: */
    const QStyle *pStyle = QApplication::style();
    const int iIconMetric = (int)(pStyle->pixelMetric(QStyle::PM_SmallIconSize) * 1.375);

    /* ToolBar creation */
    UIToolBar *toolBar = new UIToolBar (this);
    toolBar->setIconSize (QSize (iIconMetric, iIconMetric));
    toolBar->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);

    toolBar->addAction (mTakeSnapshotAction);
    toolBar->addSeparator();
    toolBar->addAction (mRestoreSnapshotAction);
    toolBar->addAction (mDeleteSnapshotAction);
    toolBar->addAction (mShowSnapshotDetailsAction);
    toolBar->addSeparator();
    toolBar->addAction(mCloneSnapshotAction);

    ((QVBoxLayout*)layout())->insertWidget (0, toolBar);

    /* Setup actions */
    mRestoreSnapshotAction->setIcon(UIIconPool::iconSetFull(
        ":/snapshot_restore_22px.png", ":/snapshot_restore_16px.png",
        ":/snapshot_restore_disabled_22px.png", ":/snapshot_restore_disabled_16px.png"));
    mDeleteSnapshotAction->setIcon(UIIconPool::iconSetFull(
        ":/snapshot_delete_22px.png", ":/snapshot_delete_16px.png",
        ":/snapshot_delete_disabled_22px.png", ":/snapshot_delete_disabled_16px.png"));
    mShowSnapshotDetailsAction->setIcon(UIIconPool::iconSetFull(
        ":/snapshot_show_details_22px.png", ":/snapshot_show_details_16px.png",
        ":/snapshot_show_details_disabled_22px.png", ":/snapshot_details_show_disabled_16px.png"));
    mTakeSnapshotAction->setIcon(UIIconPool::iconSetFull(
        ":/snapshot_take_22px.png", ":/snapshot_take_16px.png",
        ":/snapshot_take_disabled_22px.png", ":/snapshot_take_disabled_16px.png"));
    mCloneSnapshotAction->setIcon(UIIconPool::iconSetFull(
        ":/vm_clone_22px.png", ":/vm_clone_16px.png",
        ":/vm_clone_disabled_22px.png", ":/vm_clone_disabled_16px.png"));

    mRestoreSnapshotAction->setShortcut (QString ("Ctrl+Shift+R"));
    mDeleteSnapshotAction->setShortcut (QString ("Ctrl+Shift+D"));
    mShowSnapshotDetailsAction->setShortcut (QString ("Ctrl+Space"));
    mTakeSnapshotAction->setShortcut (QString ("Ctrl+Shift+S"));
    mCloneSnapshotAction->setShortcut(QString ("Ctrl+Shift+C"));

    mAgeUpdateTimer.setSingleShot (true);

    /* Setup connections */
    connect (mTreeWidget, SIGNAL (currentItemChanged (QTreeWidgetItem*, QTreeWidgetItem*)),
             this, SLOT (onCurrentChanged (QTreeWidgetItem*)));
    connect (mTreeWidget, SIGNAL (customContextMenuRequested (const QPoint&)),
             this, SLOT (onContextMenuRequested (const QPoint&)));
    connect (mTreeWidget, SIGNAL (itemChanged (QTreeWidgetItem*, int)),
             this, SLOT (onItemChanged (QTreeWidgetItem*)));
    connect(mTreeWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)),
            this, SLOT (sltItemDoubleClicked(QTreeWidgetItem*)));

    connect (mTakeSnapshotAction, SIGNAL (triggered()), this, SLOT (sltTakeSnapshot()));
    connect (mRestoreSnapshotAction, SIGNAL (triggered()), this, SLOT (sltRestoreSnapshot()));
    connect (mDeleteSnapshotAction, SIGNAL (triggered()), this, SLOT (sltDeleteSnapshot()));
    connect (mShowSnapshotDetailsAction, SIGNAL (triggered()), this, SLOT (sltShowSnapshotDetails()));
    connect (mCloneSnapshotAction, SIGNAL(triggered()), this, SLOT(sltCloneSnapshot()));

    connect (gVBoxEvents, SIGNAL(sigMachineDataChange(QString)),
             this, SLOT(machineDataChanged(QString)));
    connect (gVBoxEvents, SIGNAL(sigMachineStateChange(QString, KMachineState)),
             this, SLOT(machineStateChanged(QString, KMachineState)));
    connect (gVBoxEvents, SIGNAL(sigSessionStateChange(QString, KSessionState)),
             this, SLOT(sessionStateChanged(QString, KSessionState)));

    connect (&mAgeUpdateTimer, SIGNAL (timeout()), this, SLOT (updateSnapshotsAge()));

    retranslateUi();
}

void VBoxSnapshotsWgt::setMachine (const CMachine &aMachine)
{
    mMachine = aMachine;

    if (aMachine.isNull())
    {
        mMachineId = QString::null;
        mSessionState = KSessionState_Null;
        m_fShapshotOperationsAllowed = false;
    }
    else
    {
        mMachineId = aMachine.GetId();
        mSessionState = aMachine.GetSessionState();
        m_fShapshotOperationsAllowed = gEDataManager->machineSnapshotOperationsEnabled(mMachineId);
    }

    refreshAll();
}

void VBoxSnapshotsWgt::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::VBoxSnapshotsWgt::retranslateUi(this);

    mRestoreSnapshotAction->setText(tr("&Restore Snapshot"));
    mDeleteSnapshotAction->setText(tr("&Delete Snapshot"));
    mShowSnapshotDetailsAction->setText(tr("S&how Details"));
    mTakeSnapshotAction->setText(tr("Take &Snapshot"));
    mCloneSnapshotAction->setText(tr("&Clone..."));

    mRestoreSnapshotAction->setStatusTip(tr("Restore selected snapshot of the virtual machine"));
    mDeleteSnapshotAction->setStatusTip(tr("Delete selected snapshot of the virtual machine"));
    mShowSnapshotDetailsAction->setStatusTip(tr("Display a window with selected snapshot details"));
    mTakeSnapshotAction->setStatusTip(tr("Take a snapshot of the current virtual machine state"));
    mCloneSnapshotAction->setStatusTip(tr("Clone selected virtual machine"));

    mRestoreSnapshotAction->setToolTip(mRestoreSnapshotAction->statusTip() +
        QString(" (%1)").arg(mRestoreSnapshotAction->shortcut().toString()));
    mDeleteSnapshotAction->setToolTip(mDeleteSnapshotAction->statusTip() +
        QString(" (%1)").arg(mDeleteSnapshotAction->shortcut().toString()));
    mShowSnapshotDetailsAction->setToolTip(mShowSnapshotDetailsAction->statusTip() +
        QString(" (%1)").arg(mShowSnapshotDetailsAction->shortcut().toString()));
    mTakeSnapshotAction->setToolTip(mTakeSnapshotAction->statusTip() +
        QString(" (%1)").arg(mTakeSnapshotAction->shortcut().toString()));
    mCloneSnapshotAction->setToolTip(mCloneSnapshotAction->statusTip() +
        QString(" (%1)").arg(mCloneSnapshotAction->shortcut().toString()));
}

void VBoxSnapshotsWgt::onCurrentChanged (QTreeWidgetItem *aItem)
{
    /* Make the selected item visible */
    SnapshotWgtItem *item = aItem ? static_cast <SnapshotWgtItem*> (aItem) : 0;
    if (item)
    {
        mTreeWidget->horizontalScrollBar()->setValue (0);
        mTreeWidget->scrollToItem (item);
        mTreeWidget->horizontalScrollBar()->setValue (mTreeWidget->indentation() * item->level());
    }

    /* Whether another direct session is open or not */
    bool busy = mSessionState != KSessionState_Unlocked;

    /* Machine state of the current state item */
    KMachineState s = KMachineState_Null;
    if (curStateItem())
        s = curStateItem()->getCurrentState();

    /* Whether taking or deleting snapshots is possible right now */
    bool canTakeDeleteSnapshot =    !busy
                                 || s == KMachineState_PoweredOff
                                 || s == KMachineState_Saved
                                 || s == KMachineState_Aborted
                                 || s == KMachineState_Running
                                 || s == KMachineState_Paused;

    /* Enable/disable restoring snapshot */
    mRestoreSnapshotAction->setEnabled (!busy && mCurSnapshotItem && item && !item->isCurrentStateItem());

    /* Enable/disable deleting snapshot */
    mDeleteSnapshotAction->setEnabled (m_fShapshotOperationsAllowed &&
                                       canTakeDeleteSnapshot && mCurSnapshotItem && item && !item->isCurrentStateItem());

    /* Enable/disable the details action regardless of the session state */
    mShowSnapshotDetailsAction->setEnabled (mCurSnapshotItem && item && !item->isCurrentStateItem());

    /* Enable/disable taking snapshots */
    mTakeSnapshotAction->setEnabled (m_fShapshotOperationsAllowed &&
                                     ((canTakeDeleteSnapshot && mCurSnapshotItem && item && item->isCurrentStateItem()) ||
                                      (item && !mCurSnapshotItem)));

    /* Enable/disable cloning snapshots */
    mCloneSnapshotAction->setEnabled(!busy && item);
}

void VBoxSnapshotsWgt::onContextMenuRequested (const QPoint &aPoint)
{
    QTreeWidgetItem *item = mTreeWidget->itemAt (aPoint);
    SnapshotWgtItem *snapshotItem = item ? static_cast <SnapshotWgtItem*> (item) : 0;
    if (!snapshotItem)
        return;

    QMenu menu;

    if (mCurSnapshotItem && !snapshotItem->isCurrentStateItem())
    {
        menu.addAction (mRestoreSnapshotAction);
        menu.addAction (mDeleteSnapshotAction);
        menu.addAction (mShowSnapshotDetailsAction);
        menu.addSeparator();
        menu.addAction(mCloneSnapshotAction);
    }
    else
    {
        menu.addAction (mTakeSnapshotAction);
        menu.addSeparator();
        menu.addAction(mCloneSnapshotAction);
    }

    menu.exec (mTreeWidget->viewport()->mapToGlobal (aPoint));
}

void VBoxSnapshotsWgt::onItemChanged (QTreeWidgetItem *aItem)
{
    if (mEditProtector)
        return;

    SnapshotWgtItem *item = aItem ? static_cast <SnapshotWgtItem*> (aItem) : 0;

    if (item)
    {
        CSnapshot snap = item->snapshotId().isNull()    ? CSnapshot() : mMachine.FindSnapshot(item->snapshotId());
        if (!snap.isNull() && snap.isOk() && snap.GetName() != item->text (0))
            snap.SetName (item->text (0));
    }
}

void VBoxSnapshotsWgt::sltItemDoubleClicked(QTreeWidgetItem *pItem)
{
    /* Make sure *nothing* is being edited currently: */
    if (mEditProtector)
        return;

    /* Make sure some *valid* item was *really* double-clicked: */
    SnapshotWgtItem *pValidItem = pItem ? static_cast<SnapshotWgtItem*>(pItem) : 0;
    if (!pValidItem)
        return;

    /* Handle Ctrl+DoubleClick: */
    if (QApplication::keyboardModifiers() == Qt::ControlModifier)
    {
        /* As call for snapshot-restore procedure: */
        sltRestoreSnapshot(true /* suppress non-critical warnings */);
    }
}

void VBoxSnapshotsWgt::sltTakeSnapshot()
{
    takeSnapshot();
}

void VBoxSnapshotsWgt::sltRestoreSnapshot(bool fSuppressNonCriticalWarnings /* = false*/)
{
    /* Get currently chosen item: */
    SnapshotWgtItem *pItem = mTreeWidget->currentItem() ? static_cast<SnapshotWgtItem*>(mTreeWidget->currentItem()) : 0;
    AssertReturn(pItem, (void)0);
    /* Detemine snapshot id: */
    QString strSnapshotId = pItem->snapshotId();
    AssertReturn(!strSnapshotId.isNull(), (void)0);
    /* Get currently desired snapshot: */
    CSnapshot snapshot = mMachine.FindSnapshot(strSnapshotId);

    /* Ask the user if he really wants to restore the snapshot: */
    int iResultCode = AlertButton_Ok;
    if (!fSuppressNonCriticalWarnings || mMachine.GetCurrentStateModified())
    {
        iResultCode = msgCenter().confirmSnapshotRestoring(snapshot.GetName(), mMachine.GetCurrentStateModified());
        if (iResultCode & AlertButton_Cancel)
            return;
    }

    /* If user also confirmed new snapshot creation: */
    if (iResultCode & AlertOption_CheckBox)
    {
        /* Take snapshot of changed current state: */
        mTreeWidget->setCurrentItem(curStateItem());
        if (!takeSnapshot())
            return;
    }

    /* Open a direct session (this call will handle all errors): */
    CSession session = vboxGlobal().openSession(mMachineId);
    if (session.isNull())
        return;

    /* Restore chosen snapshot: */
    CMachine machine = session.GetMachine();
    CProgress progress = machine.RestoreSnapshot(snapshot);
    if (machine.isOk())
    {
        msgCenter().showModalProgressDialog(progress, mMachine.GetName(), ":/progress_snapshot_restore_90px.png");
        if (progress.GetResultCode() != 0)
            msgCenter().cannotRestoreSnapshot(progress, snapshot.GetName(), mMachine.GetName());
    }
    else
        msgCenter().cannotRestoreSnapshot(machine, snapshot.GetName(), mMachine.GetName());

    /* Unlock machine finally: */
    session.UnlockMachine();
}

void VBoxSnapshotsWgt::sltDeleteSnapshot()
{
    SnapshotWgtItem *item = !mTreeWidget->currentItem() ? 0 :
        static_cast <SnapshotWgtItem*> (mTreeWidget->currentItem());
    AssertReturn (item, (void) 0);

    QString snapId = item->snapshotId();
    AssertReturn (!snapId.isNull(), (void) 0);
    CSnapshot snapshot = mMachine.FindSnapshot(snapId);

    if (!msgCenter().confirmSnapshotRemoval(snapshot.GetName()))
        return;

    /** @todo check available space on the target filesystem etc etc. */
#if 0
    if (!msgCenter().warnAboutSnapshotRemovalFreeSpace(snapshot.GetName(),
                                                       "/home/juser/.VirtualBox/Machines/SampleVM/Snapshots/{01020304-0102-0102-0102-010203040506}.vdi",
                                                       "59 GiB",
                                                       "15 GiB"))
        return;
#endif

    /* Open a direct session (this call will handle all errors) */
    bool busy = mSessionState != KSessionState_Unlocked;
    CSession session;
    if (busy)
        session = vboxGlobal().openExistingSession(mMachineId);
    else
        session = vboxGlobal().openSession(mMachineId);
    if (session.isNull())
        return;

    /* Remove chosen snapshot: */
    CMachine machine = session.GetMachine();
    CProgress progress = machine.DeleteSnapshot(snapId);
    if (machine.isOk())
    {
        /* Show the progress dialog */
        msgCenter().showModalProgressDialog(progress, mMachine.GetName(), ":/progress_snapshot_discard_90px.png");

        if (progress.GetResultCode() != 0)
            msgCenter().cannotRemoveSnapshot(progress,  snapshot.GetName(), mMachine.GetName());
    }
    else
        msgCenter().cannotRemoveSnapshot(machine,  snapshot.GetName(), mMachine.GetName());

    session.UnlockMachine();
}

void VBoxSnapshotsWgt::sltShowSnapshotDetails()
{
    SnapshotWgtItem *item = !mTreeWidget->currentItem() ? 0 :
        static_cast <SnapshotWgtItem*> (mTreeWidget->currentItem());
    AssertReturn (item, (void) 0);

    CSnapshot snap = item->snapshot();
    AssertReturn (!snap.isNull(), (void) 0);

    CMachine snapMachine = snap.GetMachine();

    VBoxSnapshotDetailsDlg dlg (this);
    dlg.getFromSnapshot (snap);

    if (dlg.exec() == QDialog::Accepted)
        dlg.putBackToSnapshot();
}

void VBoxSnapshotsWgt::sltCloneSnapshot()
{
    SnapshotWgtItem *item = !mTreeWidget->currentItem() ? 0 :
        static_cast <SnapshotWgtItem*> (mTreeWidget->currentItem());
    AssertReturn (item, (void) 0);

    CMachine machine;
    CSnapshot snapshot;
    if (item->isCurrentStateItem())
        machine = item->machine();
    else
    {
        snapshot = item->snapshot();
        AssertReturn(!snapshot.isNull(), (void)0);
        machine = snapshot.GetMachine();
    }
    AssertReturn(!machine.isNull(), (void)0);

    /* Show Clone VM wizard: */
    UISafePointerWizard pWizard = new UIWizardCloneVM(this, machine, snapshot);
    pWizard->prepare();
    pWizard->exec();
    if (pWizard)
        delete pWizard;
}

void VBoxSnapshotsWgt::machineDataChanged(QString strId)
{
    SnapshotEditBlocker guardBlock (mEditProtector);

    if (strId != mMachineId)
        return;

    curStateItem()->recache();
}

void VBoxSnapshotsWgt::machineStateChanged(QString strId, KMachineState state)
{
    SnapshotEditBlocker guardBlock (mEditProtector);

    if (strId != mMachineId)
        return;

    curStateItem()->recache();
    curStateItem()->updateCurrentState(state);
}

void VBoxSnapshotsWgt::sessionStateChanged(QString strId, KSessionState state)
{
    SnapshotEditBlocker guardBlock (mEditProtector);

    if (strId != mMachineId)
        return;

    mSessionState = state;
    onCurrentChanged (mTreeWidget->currentItem());
}

void VBoxSnapshotsWgt::updateSnapshotsAge()
{
    if (mAgeUpdateTimer.isActive())
        mAgeUpdateTimer.stop();

    SnapshotAgeFormat age = traverseSnapshotAge (mTreeWidget->invisibleRootItem());

    switch (age)
    {
        case AgeInSeconds:
            mAgeUpdateTimer.setInterval (5 * 1000);
            break;
        case AgeInMinutes:
            mAgeUpdateTimer.setInterval (60 * 1000);
            break;
        case AgeInHours:
            mAgeUpdateTimer.setInterval (60 * 60 * 1000);
            break;
        case AgeInDays:
            mAgeUpdateTimer.setInterval (24 * 60 * 60 * 1000);
            break;
        default:
            mAgeUpdateTimer.setInterval (0);
            break;
    }

    if (mAgeUpdateTimer.interval() > 0)
        mAgeUpdateTimer.start();
}

bool VBoxSnapshotsWgt::takeSnapshot()
{
    /* Prepare result: */
    bool fIsValid = true;

    /* Get currently chosen item: */
    SnapshotWgtItem *pItem = mTreeWidget->currentItem() ? static_cast<SnapshotWgtItem*>(mTreeWidget->currentItem()) : 0;
    AssertReturn(pItem, (bool)0);

    /* Open a session to work with corresponding VM: */
    CSession session;
    if (mSessionState != KSessionState_Unlocked)
        session = vboxGlobal().openExistingSession(mMachineId);
    else
        session = vboxGlobal().openSession(mMachineId);
    fIsValid = !session.isNull();

    if (fIsValid)
    {
        /* Get corresponding machine object also: */
        CMachine machine = session.GetMachine();

        /* Create take-snapshot dialog: */
        QWidget *pDlgParent = windowManager().realParentWindow(this);
        QPointer<VBoxTakeSnapshotDlg> pDlg = new VBoxTakeSnapshotDlg(pDlgParent, mMachine);
        windowManager().registerNewParent(pDlg, pDlgParent);

        /* Assign corresponding icon: */
        pDlg->mLbIcon->setPixmap(vboxGlobal().vmGuestOSTypeIcon(mMachine.GetOSTypeId()));

        /* Search for the max available snapshot index: */
        int iMaxSnapShotIndex = 0;
        QString snapShotName = tr("Snapshot %1");
        QRegExp regExp(QString("^") + snapShotName.arg("([0-9]+)") + QString("$"));
        QTreeWidgetItemIterator iterator(mTreeWidget);
        while (*iterator)
        {
            QString snapShot = static_cast<SnapshotWgtItem*>(*iterator)->text(0);
            int pos = regExp.indexIn(snapShot);
            if (pos != -1)
                iMaxSnapShotIndex = regExp.cap(1).toInt() > iMaxSnapShotIndex ? regExp.cap(1).toInt() : iMaxSnapShotIndex;
            ++iterator;
        }
        pDlg->mLeName->setText(snapShotName.arg(iMaxSnapShotIndex + 1));

        /* Exec the dialog: */
        bool fDialogAccepted = pDlg->exec() == QDialog::Accepted;

        /* Is the dialog still valid? */
        if (pDlg)
        {
            /* Acquire variables: */
            QString strSnapshotName = pDlg->mLeName->text().trimmed();
            QString strSnapshotDescription = pDlg->mTeDescription->toPlainText();

            /* Destroy dialog early: */
            delete pDlg;

            /* Was the dialog accepted? */
            if (fDialogAccepted)
            {
                QString strSnapshotId;
                /* Prepare the take-snapshot progress: */
                CProgress progress = machine.TakeSnapshot(strSnapshotName, strSnapshotDescription, true, strSnapshotId);
                if (machine.isOk())
                {
                    /* Show the take-snapshot progress: */
                    msgCenter().showModalProgressDialog(progress, mMachine.GetName(), ":/progress_snapshot_create_90px.png");
                    if (!progress.isOk() || progress.GetResultCode() != 0)
                    {
                        msgCenter().cannotTakeSnapshot(progress, mMachine.GetName());
                        fIsValid = false;
                    }
                }
                else
                {
                    msgCenter().cannotTakeSnapshot(machine, mMachine.GetName());
                    fIsValid = false;
                }
            }
            else
                fIsValid = false;
        }
        else
            fIsValid = false;

        /* Unlock machine finally: */
        session.UnlockMachine();
    }

    /* Return result: */
    return fIsValid;
}

void VBoxSnapshotsWgt::refreshAll()
{
    SnapshotEditBlocker guardBlock (mEditProtector);

    if (mMachine.isNull())
    {
        onCurrentChanged();
        return;
    }

    QString selectedItem, firstChildOfSelectedItem;
    SnapshotWgtItem *cur = !mTreeWidget->currentItem() ? 0 :
        static_cast <SnapshotWgtItem*> (mTreeWidget->currentItem());
    if (cur)
    {
        selectedItem = cur->snapshotId();
        if (cur->child (0))
            firstChildOfSelectedItem = static_cast <SnapshotWgtItem*> (cur->child (0))->snapshotId();
    }

    mTreeWidget->clear();

    /* Get the first snapshot */
    if (mMachine.GetSnapshotCount() > 0)
    {
        CSnapshot snapshot = mMachine.FindSnapshot(QString::null);

        populateSnapshots (snapshot, 0);
        Assert (mCurSnapshotItem);

        /* Add the "current state" item */
        SnapshotWgtItem *csi = new SnapshotWgtItem(this, mCurSnapshotItem, mMachine);
        csi->setBold (true);
        csi->recache();

        SnapshotWgtItem *cur = findItem (selectedItem);
        if (cur == 0)
            cur = findItem (firstChildOfSelectedItem);
        if (cur == 0)
            cur = curStateItem();

        mTreeWidget->scrollToItem (cur);
        mTreeWidget->setCurrentItem (cur);
        onCurrentChanged (cur);
    }
    else
    {
        mCurSnapshotItem = 0;

        /* Add the "current state" item */
        SnapshotWgtItem *csi = new SnapshotWgtItem(this, mTreeWidget, mMachine);
        csi->setBold (true);
        csi->recache();

        mTreeWidget->setCurrentItem (csi);
        onCurrentChanged (csi);
    }

    /* Updating age */
    updateSnapshotsAge();

    mTreeWidget->resizeColumnToContents (0);
}

SnapshotWgtItem* VBoxSnapshotsWgt::findItem (const QString &aSnapshotId)
{
    QTreeWidgetItemIterator it (mTreeWidget);
    while (*it)
    {
        SnapshotWgtItem *lvi = static_cast <SnapshotWgtItem*> (*it);
        if (lvi->snapshotId() == aSnapshotId)
            return lvi;
        ++ it;
    }

    return 0;
}

SnapshotWgtItem *VBoxSnapshotsWgt::curStateItem()
{
    QTreeWidgetItem *csi = mCurSnapshotItem ?
                           mCurSnapshotItem->child (mCurSnapshotItem->childCount() - 1) :
                           mTreeWidget->invisibleRootItem()->child (0);
    return static_cast <SnapshotWgtItem*> (csi);
}

void VBoxSnapshotsWgt::populateSnapshots (const CSnapshot &aSnapshot, QTreeWidgetItem *aItem)
{
    SnapshotWgtItem *item = aItem ? new SnapshotWgtItem(this, aItem, aSnapshot) :
                                    new SnapshotWgtItem(this, mTreeWidget, aSnapshot);
    item->recache();

    CSnapshot curSnapshot = mMachine.GetCurrentSnapshot();
    if (!curSnapshot.isNull() && curSnapshot.GetId() == aSnapshot.GetId())
    {
        item->setBold (true);
        mCurSnapshotItem = item;
    }

    CSnapshotVector snapshots = aSnapshot.GetChildren();
    foreach (const CSnapshot &snapshot, snapshots)
        populateSnapshots (snapshot, item);

    item->setExpanded (true);
    item->setFlags (item->flags() | Qt::ItemIsEditable);
}

SnapshotAgeFormat VBoxSnapshotsWgt::traverseSnapshotAge (QTreeWidgetItem *aParentItem)
{
    SnapshotWgtItem *parentItem = aParentItem->type() == SnapshotWgtItem::ItemType ?
                                  static_cast <SnapshotWgtItem*> (aParentItem) : 0;

    SnapshotAgeFormat age = parentItem ? parentItem->updateAge() : AgeMax;
    for (int i = 0; i < aParentItem->childCount(); ++ i)
    {
        SnapshotAgeFormat newAge = traverseSnapshotAge (aParentItem->child (i));
        age = newAge < age ? newAge : age;
    }

    return age;
}

