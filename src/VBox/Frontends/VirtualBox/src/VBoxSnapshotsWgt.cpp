/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxSnapshotsWgt class implementation
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

/* Global includes */
#include <QDateTime>
#include <QHeaderView>
#include <QMenu>
#include <QScrollBar>

/* Local includes */
#include <VBoxSnapshotsWgt.h>
#include <VBoxProblemReporter.h>
#include <VBoxSnapshotDetailsDlg.h>
#include <VBoxTakeSnapshotDlg.h>
#include <VBoxToolBar.h>

/**
 *  QTreeWidgetItem subclass for snapshots items
 */
class SnapshotWgtItem : public QTreeWidgetItem
{
public:

    enum { ItemType = QTreeWidgetItem::UserType + 1 };

    /* Normal snapshot item (child of tree-widget) */
    SnapshotWgtItem (QTreeWidget *aTreeWidget, const CSnapshot &aSnapshot)
        : QTreeWidgetItem (aTreeWidget, ItemType)
        , mSnapshot (aSnapshot)
    {
    }

    /* Normal snapshot item (child of tree-widget-item) */
    SnapshotWgtItem (QTreeWidgetItem *aRootItem, const CSnapshot &aSnapshot)
        : QTreeWidgetItem (aRootItem, ItemType)
        , mSnapshot (aSnapshot)
    {
    }

    /* Current state item (child of tree-widget) */
    SnapshotWgtItem (QTreeWidget *aTreeWidget, const CMachine &aMachine)
        : QTreeWidgetItem (aTreeWidget, ItemType)
        , mMachine (aMachine)
    {
        updateCurrentState (mMachine.GetState());
    }

    /* Current state item (child of tree-widget-item) */
    SnapshotWgtItem (QTreeWidgetItem *aRootItem, const CMachine &aMachine)
        : QTreeWidgetItem (aRootItem, ItemType)
        , mMachine (aMachine)
    {
        updateCurrentState (mMachine.GetState());
    }

    QVariant data (int aColumn, int aRole) const
    {
        switch (aRole)
        {
            case Qt::DisplayRole:
                return QVariant (QString ("%1%2").arg (QTreeWidgetItem::data (aColumn, aRole).toString()).arg (mAge));
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

    CSnapshot snapshot() const { return mSnapshot; }
    QString snapshotId() const { return mId; }

    void recache()
    {
        if (!mSnapshot.isNull())
        {
            mId = mSnapshot.GetId();
            setText (0, mSnapshot.GetName());
            mOnline = mSnapshot.GetOnline();
            setIcon (0, vboxGlobal().snapshotIcon (mOnline));
            mDesc = mSnapshot.GetDescription();
            mTimestamp.setTime_t (mSnapshot.GetTimeStamp() / 1000);
            mCurStateModified = false;
        }
        else
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
        adjustText();
        recacheToolTip();
    }

    void updateCurrentState (KMachineState aState)
    {
        if (mMachine.isNull())
            return;

        setIcon (0, vboxGlobal().toIcon (aState));
        mMachineState = aState;
        mTimestamp.setTime_t (mMachine.GetLastStateChange() / 1000);
    }

    SnapshotAgeFormat updateAge()
    {
        QString oldAge (mAge);

        /* Age: [date time|%1d ago|%1h ago|%1min ago|%1sec ago] */
        SnapshotAgeFormat ageFormat;
        if (mTimestamp.daysTo (QDateTime::currentDateTime()) > 30)
        {
            mAge = VBoxSnapshotsWgt::tr (" [%1]").arg (mTimestamp.toString (Qt::LocalDate));
            ageFormat = AgeMax;
        }
        else if (mTimestamp.secsTo (QDateTime::currentDateTime()) > 60 * 60 * 24)
        {
            mAge = VBoxSnapshotsWgt::tr (" [%1d ago]").arg (mTimestamp.secsTo (QDateTime::currentDateTime()) / 60 / 60 / 24);
            ageFormat = AgeInDays;
        }
        else if (mTimestamp.secsTo (QDateTime::currentDateTime()) > 60 * 60)
        {
            mAge = VBoxSnapshotsWgt::tr (" [%1h ago]").arg (mTimestamp.secsTo (QDateTime::currentDateTime()) / 60 / 60);
            ageFormat = AgeInHours;
        }
        else if (mTimestamp.secsTo (QDateTime::currentDateTime()) > 60)
        {
            mAge = VBoxSnapshotsWgt::tr (" [%1min ago]").arg (mTimestamp.secsTo (QDateTime::currentDateTime()) / 60);
            ageFormat = AgeInMinutes;
        }
        else
        {
            mAge = VBoxSnapshotsWgt::tr (" [%1sec ago]").arg (mTimestamp.secsTo (QDateTime::currentDateTime()));
            ageFormat = AgeInSeconds;
        }

        if (mAge != oldAge)
            emitDataChanged();

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
                .arg (vboxGlobal().toString (mMachineState)).arg (dateTime);
        }

        QString toolTip = QString ("<nobr><b>%1</b>%2</nobr><br><nobr>%3</nobr>")
            .arg (name) .arg (details).arg (dateTime);

        if (!mDesc.isEmpty())
            toolTip += "<hr>" + mDesc;

        setToolTip (0, toolTip);
    }

    CSnapshot mSnapshot;
    CMachine mMachine;

    QString mId;
    bool mOnline;
    QString mDesc;
    QDateTime mTimestamp;
    QString mAge;

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
{
    /* Apply UI decorations */
    Ui::VBoxSnapshotsWgt::setupUi (this);

    mTreeWidget->header()->hide();

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

    /* ToolBar creation */
    VBoxToolBar *toolBar = new VBoxToolBar (this);
    toolBar->setUsesTextLabel (false);
    toolBar->setIconSize (QSize (22, 22));
    toolBar->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);

    toolBar->addAction (mTakeSnapshotAction);
    toolBar->addSeparator();
    toolBar->addAction (mRestoreSnapshotAction);
    toolBar->addAction (mDeleteSnapshotAction);
    toolBar->addSeparator();
    toolBar->addAction (mShowSnapshotDetailsAction);

    ((QVBoxLayout*)layout())->insertWidget (0, toolBar);

    /* Setup actions */
    mRestoreSnapshotAction->setIcon (VBoxGlobal::iconSetFull (
        QSize (22, 22), QSize (16, 16),
        ":/discard_cur_state_22px.png", ":/discard_cur_state_16px.png", // TODO: Update Icons!
        ":/discard_cur_state_dis_22px.png", ":/discard_cur_state_dis_16px.png")); // TODO: Update Icons!
    mDeleteSnapshotAction->setIcon (VBoxGlobal::iconSetFull (
        QSize (22, 22), QSize (16, 16),
        ":/delete_snapshot_22px.png", ":/delete_snapshot_16px.png",
        ":/delete_snapshot_dis_22px.png", ":/delete_snapshot_dis_16px.png"));
    mShowSnapshotDetailsAction->setIcon (VBoxGlobal::iconSetFull (
        QSize (22, 22), QSize (16, 16),
        ":/show_snapshot_details_22px.png", ":/show_snapshot_details_16px.png",
        ":/show_snapshot_details_dis_22px.png", ":/show_snapshot_details_dis_16px.png"));
    mTakeSnapshotAction->setIcon (VBoxGlobal::iconSetFull (
        QSize (22, 22), QSize (16, 16),
        ":/take_snapshot_22px.png", ":/take_snapshot_16px.png",
        ":/take_snapshot_dis_22px.png", ":/take_snapshot_dis_16px.png"));

    mRestoreSnapshotAction->setShortcut (QString ("Ctrl+Shift+R"));
    mDeleteSnapshotAction->setShortcut (QString ("Ctrl+Shift+D"));
    mShowSnapshotDetailsAction->setShortcut (QString ("Ctrl+Space"));
    mTakeSnapshotAction->setShortcut (QString ("Ctrl+Shift+S"));

    mAgeUpdateTimer.setSingleShot (true);

    /* Setup connections */
    connect (mTreeWidget, SIGNAL (currentItemChanged (QTreeWidgetItem*, QTreeWidgetItem*)),
             this, SLOT (onCurrentChanged (QTreeWidgetItem*)));
    connect (mTreeWidget, SIGNAL (customContextMenuRequested (const QPoint&)),
             this, SLOT (onContextMenuRequested (const QPoint&)));
    connect (mTreeWidget, SIGNAL (itemChanged (QTreeWidgetItem*, int)),
             this, SLOT (onItemChanged (QTreeWidgetItem*)));

    connect (mRestoreSnapshotAction, SIGNAL (triggered()), this, SLOT (restoreSnapshot()));
    connect (mDeleteSnapshotAction, SIGNAL (triggered()), this, SLOT (deleteSnapshot()));
    connect (mShowSnapshotDetailsAction, SIGNAL (triggered()), this, SLOT (showSnapshotDetails()));
    connect (mTakeSnapshotAction, SIGNAL (triggered()), this, SLOT (takeSnapshot()));

    connect (&vboxGlobal(), SIGNAL (machineDataChanged (const VBoxMachineDataChangeEvent&)),
             this, SLOT (machineDataChanged (const VBoxMachineDataChangeEvent&)));
    connect (&vboxGlobal(), SIGNAL (machineStateChanged (const VBoxMachineStateChangeEvent&)),
             this, SLOT (machineStateChanged (const VBoxMachineStateChangeEvent&)));
    connect (&vboxGlobal(), SIGNAL (sessionStateChanged (const VBoxSessionStateChangeEvent&)),
             this, SLOT (sessionStateChanged (const VBoxSessionStateChangeEvent&)));

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
    }
    else
    {
        mMachineId = aMachine.GetId();
        mSessionState = aMachine.GetSessionState();
    }

    refreshAll();
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
    bool busy = mSessionState != KSessionState_Closed;

    /* Enable/disable snapshot actions */
    mSnapshotActionGroup->setEnabled (!busy && mCurSnapshotItem && item && !item->isCurrentStateItem());

    /* Enable/disable the details action regardles of the session state */
    mShowSnapshotDetailsAction->setEnabled (mCurSnapshotItem && item && !item->isCurrentStateItem());

    /* Enable/disable current state actions */
    mCurStateActionGroup->setEnabled ((!busy && mCurSnapshotItem && item && item->isCurrentStateItem()) ||
                                      (item && !mCurSnapshotItem));
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
        menu.addSeparator();
        menu.addAction (mShowSnapshotDetailsAction);
    }
    else
        menu.addAction (mTakeSnapshotAction);

    menu.exec (mTreeWidget->viewport()->mapToGlobal (aPoint));
}

void VBoxSnapshotsWgt::onItemChanged (QTreeWidgetItem *aItem)
{
    if (mEditProtector)
        return;

    SnapshotWgtItem *item = aItem ? static_cast <SnapshotWgtItem*> (aItem) : 0;

    if (item)
    {
        CSnapshot snap = item->snapshotId().isNull() ? CSnapshot() : mMachine.GetSnapshot (item->snapshotId());
        if (!snap.isNull() && snap.isOk() && snap.GetName() != item->text (0))
            snap.SetName (item->text (0));
    }
}

void VBoxSnapshotsWgt::restoreSnapshot()
{
    SnapshotWgtItem *item = !mTreeWidget->currentItem() ? 0 :
        static_cast <SnapshotWgtItem*> (mTreeWidget->currentItem());
    AssertReturn (item, (void) 0);

    QString snapId = item->snapshotId();
    AssertReturn (!snapId.isNull(), (void) 0);
    CSnapshot snapshot = mMachine.GetSnapshot (snapId);

    if (!vboxProblem().askAboutSnapshotRestoring (snapshot.GetName()))
        return;

    /* Open a direct session (this call will handle all errors) */
    CSession session = vboxGlobal().openSession (mMachineId);
    if (session.isNull())
        return;

    CConsole console = session.GetConsole();
    CProgress progress = console.RestoreSnapshot (snapshot);
    if (console.isOk())
    {
        /* Show the progress dialog */
        vboxProblem().showModalProgressDialog (progress, mMachine.GetName(),
                                               vboxProblem().mainWindowShown());

        if (progress.GetResultCode() != 0)
            vboxProblem().cannotRestoreSnapshot (progress, snapshot.GetName());
    }
    else
        vboxProblem().cannotRestoreSnapshot (progress, snapshot.GetName());

    session.Close();
}

void VBoxSnapshotsWgt::deleteSnapshot()
{
    SnapshotWgtItem *item = !mTreeWidget->currentItem() ? 0 :
        static_cast <SnapshotWgtItem*> (mTreeWidget->currentItem());
    AssertReturn (item, (void) 0);

    QString snapId = item->snapshotId();
    AssertReturn (!snapId.isNull(), (void) 0);
    CSnapshot snapshot = mMachine.GetSnapshot (snapId);

    if (!vboxProblem().askAboutSnapshotDeleting (snapshot.GetName()))
        return;

    /* Open a direct session (this call will handle all errors) */
    CSession session = vboxGlobal().openSession (mMachineId);
    if (session.isNull())
        return;

    CConsole console = session.GetConsole();
    CProgress progress = console.DeleteSnapshot (snapId);
    if (console.isOk())
    {
        /* Show the progress dialog */
        vboxProblem().showModalProgressDialog (progress, mMachine.GetName(),
                                               vboxProblem().mainWindowShown());

        if (progress.GetResultCode() != 0)
            vboxProblem().cannotDeleteSnapshot (progress,  snapshot.GetName());
    }
    else
        vboxProblem().cannotDeleteSnapshot (console,  snapshot.GetName());

    session.Close();
}

void VBoxSnapshotsWgt::showSnapshotDetails()
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

void VBoxSnapshotsWgt::takeSnapshot()
{
    SnapshotWgtItem *item = !mTreeWidget->currentItem() ? 0 :
        static_cast <SnapshotWgtItem*> (mTreeWidget->currentItem());
    AssertReturn (item, (void) 0);

    VBoxTakeSnapshotDlg dlg (this);

    QString typeId = mMachine.GetOSTypeId();
    dlg.mLbIcon->setPixmap (vboxGlobal().vmGuestOSTypeIcon (typeId));

    /* Search for the max available filter index */
    int maxSnapShotIndex = 0;
    QString snapShotName = tr ("Snapshot %1");
    QRegExp regExp (QString ("^") + snapShotName.arg ("([0-9]+)") + QString ("$"));
    QTreeWidgetItemIterator iterator (mTreeWidget);
    while (*iterator)
    {
        QString snapShot = static_cast <SnapshotWgtItem*> (*iterator)->text (0);
        int pos = regExp.indexIn (snapShot);
        if (pos != -1)
            maxSnapShotIndex = regExp.cap (1).toInt() > maxSnapShotIndex ?
                               regExp.cap (1).toInt() : maxSnapShotIndex;
        ++ iterator;
    }
    dlg.mLeName->setText (snapShotName.arg (maxSnapShotIndex + 1));

    if (dlg.exec() == QDialog::Accepted)
    {
        /* Open a direct session (this call will handle all errors) */
        CSession session = vboxGlobal().openSession (mMachineId);
        if (session.isNull())
            return;

        CConsole console = session.GetConsole();
        CProgress progress = console.TakeSnapshot (dlg.mLeName->text().trimmed(),
                                                   dlg.mTeDescription->toPlainText());
        if (console.isOk())
        {
            /* Show the progress dialog */
            vboxProblem().showModalProgressDialog (progress, mMachine.GetName(),
                                                   vboxProblem().mainWindowShown());

            if (progress.GetResultCode() != 0)
                vboxProblem().cannotTakeSnapshot (progress);
        }
        else
            vboxProblem().cannotTakeSnapshot (console);

        session.Close();
    }
}


void VBoxSnapshotsWgt::machineDataChanged (const VBoxMachineDataChangeEvent &aEvent)
{
    SnapshotEditBlocker guardBlock (mEditProtector);

    if (aEvent.id != mMachineId)
        return;

    curStateItem()->recache();
}

void VBoxSnapshotsWgt::machineStateChanged (const VBoxMachineStateChangeEvent &aEvent)
{
    SnapshotEditBlocker guardBlock (mEditProtector);

    if (aEvent.id != mMachineId)
        return;

    curStateItem()->recache();
    curStateItem()->updateCurrentState (aEvent.state);
}

void VBoxSnapshotsWgt::sessionStateChanged (const VBoxSessionStateChangeEvent &aEvent)
{
    SnapshotEditBlocker guardBlock (mEditProtector);

    if (aEvent.id != mMachineId)
        return;

    mSessionState = aEvent.state;
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

void VBoxSnapshotsWgt::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxSnapshotsWgt::retranslateUi (this);

    mRestoreSnapshotAction->setText (tr ("&Restore Snapshot"));
    mDeleteSnapshotAction->setText (tr ("&Delete Snapshot"));
    mShowSnapshotDetailsAction->setText (tr ("S&how Details"));
    mTakeSnapshotAction->setText (tr ("Take &Snapshot"));

    mRestoreSnapshotAction->setStatusTip (tr ("Restore the selected snapshot of the virtual machine"));
    mDeleteSnapshotAction->setStatusTip (tr ("Delete the selected snapshot of the virtual machine"));
    mShowSnapshotDetailsAction->setStatusTip (tr ("Show details of the selected snapshot"));
    mTakeSnapshotAction->setStatusTip (tr ("Take a snapshot of the current virtual machine state"));

    mRestoreSnapshotAction->setToolTip (mRestoreSnapshotAction->text().remove ('&').remove ('.') +
        QString (" (%1)").arg (mRestoreSnapshotAction->shortcut().toString()));
    mDeleteSnapshotAction->setToolTip (mDeleteSnapshotAction->text().remove ('&').remove ('.') +
        QString (" (%1)").arg (mDeleteSnapshotAction->shortcut().toString()));
    mShowSnapshotDetailsAction->setToolTip (mShowSnapshotDetailsAction->text().remove ('&').remove ('.') +
        QString (" (%1)").arg (mShowSnapshotDetailsAction->shortcut().toString()));
    mTakeSnapshotAction->setToolTip (mTakeSnapshotAction->text().remove ('&').remove ('.') +
        QString (" (%1)").arg (mTakeSnapshotAction->shortcut().toString()));
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
        CSnapshot snapshot = mMachine.GetSnapshot (QString::null);

        populateSnapshots (snapshot, 0);
        Assert (mCurSnapshotItem);

        /* Add the "current state" item */
        SnapshotWgtItem *csi = new SnapshotWgtItem (mCurSnapshotItem, mMachine);
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
        SnapshotWgtItem *csi = new SnapshotWgtItem (mTreeWidget, mMachine);
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
    Assert (csi);
    return static_cast <SnapshotWgtItem*> (csi);
}

void VBoxSnapshotsWgt::populateSnapshots (const CSnapshot &aSnapshot, QTreeWidgetItem *aItem)
{
    SnapshotWgtItem *item = aItem ? new SnapshotWgtItem (aItem, aSnapshot) :
                                    new SnapshotWgtItem (mTreeWidget, aSnapshot);
    item->recache();

    if (mMachine.GetCurrentSnapshot().GetId() == aSnapshot.GetId())
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

