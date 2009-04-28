/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxSnapshotsWgt class implementation
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

#include <VBoxSnapshotsWgt.h>
#include <VBoxProblemReporter.h>
#include <VBoxSnapshotDetailsDlg.h>
#include <VBoxTakeSnapshotDlg.h>
#include <VBoxToolBar.h>

/* Qt includes */
#include <QMenu>
#include <QHeaderView>
#include <QScrollBar>
#include <QDateTime>

/** QListViewItem subclass for snapshots items */
class SnapshotWgtItem : public QTreeWidgetItem
{
public:

    /** Normal snapshot item */
    SnapshotWgtItem (QTreeWidget *aTreeWidget, const CSnapshot &aSnapshot)
        : QTreeWidgetItem (aTreeWidget)
        , mSnapshot (aSnapshot)
    {
        recache();
    }

    /** Normal snapshot item */
    SnapshotWgtItem (QTreeWidgetItem *aRootItem, const CSnapshot &aSnapshot)
        : QTreeWidgetItem (aRootItem)
        , mSnapshot (aSnapshot)
    {
        recache();
    }

    /** Current state item */
    SnapshotWgtItem (QTreeWidget *aTreeWidget, const CMachine &aMachine)
        : QTreeWidgetItem (aTreeWidget)
        , mMachine (aMachine)
    {
        updateCurrentState (mMachine.GetState());
        recache();
    }

    /** Current state item */
    SnapshotWgtItem (QTreeWidgetItem *aRootItem, const CMachine &aMachine)
        : QTreeWidgetItem (aRootItem)
        , mMachine (aMachine)
    {
        updateCurrentState (mMachine.GetState());
        recache();
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
        myFont.setBold (aItalic);
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
                VBoxSnapshotsWgt::tr ("Current State (changed)",
                                      "Current State (Modified)") :
                VBoxSnapshotsWgt::tr ("Current State",
                                      "Current State (Unmodified)"));
            mDesc = mCurStateModified ?
                VBoxSnapshotsWgt::tr ("The current state differs from the state "
                                      "stored in the current snapshot") :
                parent() != 0 ? /* we're not the only item in the view */
                    VBoxSnapshotsWgt::tr ("The current state is identical to the state "
                                          "stored in the current snapshot") :
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

private:

    void adjustText()
    {
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
            /* the current snapshot is always bold */
            if (bold())
                details = VBoxSnapshotsWgt::tr (" (current, ", "Snapshot details");
            else
                details = " (";
            details += mOnline ? VBoxSnapshotsWgt::tr ("online)", "Snapshot details")
                               : VBoxSnapshotsWgt::tr ("offline)", "Snapshot details");

            if (dateTimeToday)
                dateTime = VBoxSnapshotsWgt::tr ("Taken at %1", "Snapshot (time)")
                    .arg (dateTime);
            else
                dateTime = VBoxSnapshotsWgt::tr ("Taken on %1", "Snapshot (date + time)")
                    .arg (dateTime);
        }
        else
        {
            dateTime = VBoxSnapshotsWgt::tr ("%1 since %2", "Current State (time or date + time)")
                .arg (vboxGlobal().toString (mMachineState))
                .arg (dateTime);
        }

        QString toolTip = QString ("<nobr><b>%1</b>%2</nobr><br><nobr>%3</nobr>")
            .arg (name) .arg (details)
            .arg (dateTime);

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
    : QIWithRetranslateUI<QWidget> (aParent)
    , mCurSnapshotItem (0)
    , mContextMenu (new QMenu (this))
    , mContextMenuDirty (true)
    , mEditProtector (false)
    , mSnapshotActionGroup (new QActionGroup (this))
    , mCurStateActionGroup (new QActionGroup (this))
    , mDiscardSnapshotAction (new QAction (mSnapshotActionGroup))
    , mTakeSnapshotAction (new QAction (this))
    , mRevertToCurSnapAction (new QAction (mCurStateActionGroup))
    , mDiscardCurSnapAndStateAction (new QAction (mCurStateActionGroup))
    , mShowSnapshotDetailsAction (new QAction (this))
{
    /* Apply UI decorations */
    Ui::VBoxSnapshotsWgt::setupUi (this);

    mTreeWidget->header()->hide();

    /* ToolBar creation */
    VBoxToolBar *toolBar = new VBoxToolBar (this);
    toolBar->setUsesTextLabel (false);
    toolBar->setIconSize (QSize (22, 22));
    toolBar->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);

    toolBar->addAction (mTakeSnapshotAction);
    toolBar->addActions (mCurStateActionGroup->actions());
    toolBar->addSeparator();
    toolBar->addActions (mSnapshotActionGroup->actions());
    toolBar->addSeparator();
    toolBar->addAction (mShowSnapshotDetailsAction);

    ((QVBoxLayout*)layout())->insertWidget (0, toolBar);

    /* Setup actions */
    mDiscardSnapshotAction->setIcon (VBoxGlobal::iconSetFull (
        QSize (22, 22), QSize (16, 16),
        ":/discard_snapshot_22px.png", ":/discard_snapshot_16px.png",
        ":/discard_snapshot_dis_22px.png", ":/discard_snapshot_dis_16px.png"));
    mTakeSnapshotAction->setIcon (VBoxGlobal::iconSetFull (
        QSize (22, 22), QSize (16, 16),
        ":/take_snapshot_22px.png", ":/take_snapshot_16px.png",
        ":/take_snapshot_dis_22px.png", ":/take_snapshot_dis_16px.png"));
    mRevertToCurSnapAction->setIcon (VBoxGlobal::iconSetFull (
        QSize (22, 22), QSize (16, 16),
        ":/discard_cur_state_22px.png", ":/discard_cur_state_16px.png",
        ":/discard_cur_state_dis_22px.png", ":/discard_cur_state_dis_16px.png"));
    mDiscardCurSnapAndStateAction->setIcon (VBoxGlobal::iconSetFull (
        QSize (22, 22), QSize (16, 16),
        ":/discard_cur_state_snapshot_22px.png", ":/discard_cur_state_snapshot_16px.png",
        ":/discard_cur_state_snapshot_dis_22px.png", ":/discard_cur_state_snapshot_dis_16px.png"));
    mShowSnapshotDetailsAction->setIcon (VBoxGlobal::iconSetFull (
        QSize (22, 22), QSize (16, 16),
        ":/show_snapshot_details_22px.png", ":/show_snapshot_details_16px.png",
        ":/show_snapshot_details_dis_22px.png", ":/show_snapshot_details_dis_16px.png"));

    mDiscardSnapshotAction->setShortcut (QString ("Ctrl+Shift+D"));
    mTakeSnapshotAction->setShortcut (QString ("Ctrl+Shift+S"));
    mRevertToCurSnapAction->setShortcut (QString ("Ctrl+Shift+R"));
    mDiscardCurSnapAndStateAction->setShortcut (QString ("Ctrl+Shift+B"));
    mShowSnapshotDetailsAction->setShortcut (QString ("Ctrl+Space"));

    /* Setup connections */
    connect (mTreeWidget, SIGNAL (currentItemChanged (QTreeWidgetItem*, QTreeWidgetItem*)),
             this, SLOT (onCurrentChanged (QTreeWidgetItem*, QTreeWidgetItem*)));
    connect (mTreeWidget, SIGNAL (customContextMenuRequested (const QPoint&)),
             this, SLOT (onContextMenuRequested (const QPoint&)));
    connect (mTreeWidget, SIGNAL (itemChanged (QTreeWidgetItem*, int)),
             this, SLOT (onItemChanged (QTreeWidgetItem*, int)));

    connect (mDiscardSnapshotAction, SIGNAL (triggered()),
             this, SLOT (discardSnapshot()));
    connect (mTakeSnapshotAction, SIGNAL (triggered()),
             this, SLOT (takeSnapshot()));
    connect (mRevertToCurSnapAction, SIGNAL (triggered()),
             this, SLOT (discardCurState()));
    connect (mDiscardCurSnapAndStateAction, SIGNAL (triggered()),
             this, SLOT (discardCurSnapAndState()));
    connect (mShowSnapshotDetailsAction, SIGNAL (triggered()),
             this, SLOT (showSnapshotDetails()));

    connect (&vboxGlobal(), SIGNAL (machineDataChanged (const VBoxMachineDataChangeEvent&)),
             this, SLOT (machineDataChanged (const VBoxMachineDataChangeEvent&)));
    connect (&vboxGlobal(), SIGNAL (machineStateChanged (const VBoxMachineStateChangeEvent&)),
             this, SLOT (machineStateChanged (const VBoxMachineStateChangeEvent&)));
    connect (&vboxGlobal(), SIGNAL (sessionStateChanged (const VBoxSessionStateChangeEvent&)),
             this, SLOT (sessionStateChanged (const VBoxSessionStateChangeEvent&)));
#if 0
    connect (&vboxGlobal(), SIGNAL (snapshotChanged (const VBoxSnapshotEvent&)),
             this, SLOT (snapshotChanged (const VBoxSnapshotEvent&)));
#endif

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


void VBoxSnapshotsWgt::onCurrentChanged (QTreeWidgetItem *aItem,
                                         QTreeWidgetItem* /* old */)
{
    /* Make the selected item visible */
    if (aItem)
    {
        SnapshotWgtItem *item = static_cast<SnapshotWgtItem*> (aItem);
        mTreeWidget->horizontalScrollBar()->setValue (0);
        mTreeWidget->scrollToItem (item);
        mTreeWidget->horizontalScrollBar()->setValue (
            mTreeWidget->indentation() * item->level());
    }

    /* Whether another direct session is open or not */
    bool busy = mSessionState != KSessionState_Closed;

    /* Enable/disable snapshot actions */
    mSnapshotActionGroup->setEnabled (!busy &&
        aItem && mCurSnapshotItem && aItem != mCurSnapshotItem->child (0));

    /* Enable/disable the details action regardles of the session state */
    mShowSnapshotDetailsAction->setEnabled (
        aItem && mCurSnapshotItem && aItem != mCurSnapshotItem->child (0));

    /* Enable/disable current state actions */
    mCurStateActionGroup->setEnabled (!busy &&
        aItem && mCurSnapshotItem && aItem == mCurSnapshotItem->child (0));

    /* The Take Snapshot action is always enabled for the current state */
    mTakeSnapshotAction->setEnabled ((!busy && mCurStateActionGroup->isEnabled()) ||
                                     (aItem && !mCurSnapshotItem));

    mContextMenuDirty = true;
}

void VBoxSnapshotsWgt::onContextMenuRequested (const QPoint &aPoint)
{
    QTreeWidgetItem *item = mTreeWidget->itemAt (aPoint);
    if (!item)
        return;

    if (mContextMenuDirty)
    {
        mContextMenu->clear();

        if (!mCurSnapshotItem)
        {
            /* We have only one item -- current state */
            mContextMenu->addAction (mTakeSnapshotAction);
            mContextMenu->addActions (mCurStateActionGroup->actions());
        }
        else
        {
            if (item == mCurSnapshotItem->child (0))
            {
                /* Current state is selected */
                mContextMenu->addAction (mTakeSnapshotAction);
                mContextMenu->addActions (mCurStateActionGroup->actions());
            }
            else
            {
                /* Snapshot is selected */
                mContextMenu->addActions (mSnapshotActionGroup->actions());
                mContextMenu->addSeparator();
                mContextMenu->addAction (mShowSnapshotDetailsAction);
            }
        }

        mContextMenuDirty = false;
    }

    mContextMenu->exec (mTreeWidget->viewport()->mapToGlobal (aPoint));
}

void VBoxSnapshotsWgt::onItemChanged (QTreeWidgetItem *aItem, int)
{
    if (mEditProtector)
        return;

    SnapshotWgtItem *item = aItem ? static_cast<SnapshotWgtItem*> (aItem) : 0;

    if (item)
    {
        CSnapshot snap = mMachine.GetSnapshot (item->snapshotId());
        if (!snap.isNull() && snap.isOk() &&
            snap.GetName() != item->text (0))
            snap.SetName (item->text (0));
    }
}


void VBoxSnapshotsWgt::discardSnapshot()
{
    SnapshotWgtItem *item = mTreeWidget->selectedItems().isEmpty() ? 0 :
        static_cast<SnapshotWgtItem*> (mTreeWidget->selectedItems() [0]);
    AssertReturn (item, (void) 0);

    QString snapId = item->snapshotId();
    AssertReturn (!snapId.isNull(), (void) 0);

    /* Open a direct session (this call will handle all errors) */
    CSession session = vboxGlobal().openSession (mMachineId);
    if (session.isNull())
        return;

    QString snapName = mMachine.GetSnapshot (snapId).GetName();

    CConsole console = session.GetConsole();
    CProgress progress = console.DiscardSnapshot (snapId);
    if (console.isOk())
    {
        /* Show the progress dialog */
        vboxProblem().showModalProgressDialog (progress, mMachine.GetName(),
                                               vboxProblem().mainWindowShown());

        if (progress.GetResultCode() != 0)
            vboxProblem().cannotDiscardSnapshot (progress, snapName);
    }
    else
        vboxProblem().cannotDiscardSnapshot (console, snapName);

    session.Close();
}

void VBoxSnapshotsWgt::takeSnapshot()
{
    SnapshotWgtItem *item = mTreeWidget->selectedItems().isEmpty() ? 0 :
        static_cast<SnapshotWgtItem*> (mTreeWidget->selectedItems() [0]);
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
        QString snapShot = (*iterator)->text (0);
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
        CProgress progress =
                console.TakeSnapshot (dlg.mLeName->text().trimmed(),
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

void VBoxSnapshotsWgt::discardCurState()
{
    SnapshotWgtItem *item = mTreeWidget->selectedItems().isEmpty() ? 0 :
        static_cast<SnapshotWgtItem*> (mTreeWidget->selectedItems() [0]);
    AssertReturn (item, (void) 0);

    /* Open a direct session (this call will handle all errors) */
    CSession session = vboxGlobal().openSession (mMachineId);
    if (session.isNull())
        return;

    CConsole console = session.GetConsole();
    CProgress progress = console.DiscardCurrentState();
    if (console.isOk())
    {
        /* show the progress dialog */
        vboxProblem().showModalProgressDialog (progress, mMachine.GetName(),
                                               vboxProblem().mainWindowShown());

        if (progress.GetResultCode() != 0)
            vboxProblem().cannotDiscardCurrentState (progress);
    }
    else
        vboxProblem().cannotDiscardCurrentState (console);

    session.Close();
}

void VBoxSnapshotsWgt::discardCurSnapAndState()
{
    if (!vboxProblem().askAboutSnapshotAndStateDiscarding())
        return;

    SnapshotWgtItem *item = mTreeWidget->selectedItems().isEmpty() ? 0 :
        static_cast<SnapshotWgtItem*> (mTreeWidget->selectedItems() [0]);
    AssertReturn (item, (void) 0);

    /* Open a direct session (this call will handle all errors) */
    CSession session = vboxGlobal().openSession (mMachineId);
    if (session.isNull())
        return;

    CConsole console = session.GetConsole();
    CProgress progress = console.DiscardCurrentSnapshotAndState();
    if (console.isOk())
    {
        /* Show the progress dialog */
        vboxProblem().showModalProgressDialog (progress, mMachine.GetName(),
                                               vboxProblem().mainWindowShown());

        if (progress.GetResultCode() != 0)
            vboxProblem().cannotDiscardCurrentSnapshotAndState (progress);
    }
    else
        vboxProblem().cannotDiscardCurrentSnapshotAndState (console);

    session.Close();
}

void VBoxSnapshotsWgt::showSnapshotDetails()
{
    SnapshotWgtItem *item = mTreeWidget->selectedItems().isEmpty() ? 0 :
        static_cast<SnapshotWgtItem*> (mTreeWidget->selectedItems() [0]);
    AssertReturn (item, (void) 0);

    CSnapshot snap = item->snapshot();
    AssertReturn (!snap.isNull(), (void) 0);

    CMachine snapMachine = snap.GetMachine();

    VBoxSnapshotDetailsDlg dlg (this);
    dlg.getFromSnapshot (snap);

    if (dlg.exec() == QDialog::Accepted)
        dlg.putBackToSnapshot();
}


void VBoxSnapshotsWgt::machineDataChanged (const VBoxMachineDataChangeEvent &aE)
{
    SnapshotEditBlocker guardBlock (mEditProtector);

    if (aE.id != mMachineId)
        return; /* not interested in other machines */

    curStateItem()->recache();
}

void VBoxSnapshotsWgt::machineStateChanged (const VBoxMachineStateChangeEvent &aE)
{
    SnapshotEditBlocker guardBlock (mEditProtector);

    if (aE.id != mMachineId)
        return; /* not interested in other machines */

    curStateItem()->recache();
    curStateItem()->updateCurrentState (aE.state);
}

void VBoxSnapshotsWgt::sessionStateChanged (const VBoxSessionStateChangeEvent &aE)
{
    SnapshotEditBlocker guardBlock (mEditProtector);

    if (aE.id != mMachineId)
        return; /* not interested in other machines */

    mSessionState = aE.state;
    onCurrentChanged (mTreeWidget->selectedItems().isEmpty() ? 0 :
                      mTreeWidget->selectedItems() [0]);
}

#if 0
void VBoxSnapshotsWgt::snapshotChanged (const VBoxSnapshotEvent &aE)
{
    SnapshotEditBlocker guardBlock (mEditProtector);

    if (aE.machineId != mMachineId)
        return; /* not interested in other machines */

    switch (aE.what)
    {
        case VBoxSnapshotEvent::Taken:
        case VBoxSnapshotEvent::Discarded:
        {
            refreshAll();
            break;
        }
        case VBoxSnapshotEvent::Changed:
        {
            SnapshotWgtItem *lvi = findItem (aE.snapshotId);
            if (!lvi)
                refreshAll (false);
            else
                lvi->recache();
            break;
        }
    }
}
#endif

void VBoxSnapshotsWgt::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxSnapshotsWgt::retranslateUi (this);

    mDiscardSnapshotAction->setText (tr ("&Discard Snapshot"));
    mTakeSnapshotAction->setText (tr ("Take &Snapshot"));
    mRevertToCurSnapAction->setText (tr ("&Revert to Current Snapshot"));
    mDiscardCurSnapAndStateAction->setText (tr ("D&iscard Current Snapshot and State"));
    mShowSnapshotDetailsAction->setText (tr ("S&how Details"));

    mDiscardSnapshotAction->setStatusTip (tr ("Discard the selected snapshot of the virtual machine"));
    mTakeSnapshotAction->setStatusTip (tr ("Take a snapshot of the current virtual machine state"));
    mRevertToCurSnapAction->setStatusTip (tr ("Restore the virtual machine state from the state stored in the current snapshot"));
    mDiscardCurSnapAndStateAction->setStatusTip (tr ("Discard the current snapshot and revert the machine to the state it had before the snapshot was taken"));
    mShowSnapshotDetailsAction->setStatusTip (tr ("Show details of the selected snapshot"));

    mDiscardSnapshotAction->setToolTip (mDiscardSnapshotAction->text().remove ('&').remove ('.') +
        QString (" (%1)").arg (mDiscardSnapshotAction->shortcut().toString()));
    mTakeSnapshotAction->setToolTip (mTakeSnapshotAction->text().remove ('&').remove ('.') +
        QString (" (%1)").arg (mTakeSnapshotAction->shortcut().toString()));
    mRevertToCurSnapAction->setToolTip (mRevertToCurSnapAction->text().remove ('&').remove ('.') +
        QString (" (%1)").arg (mRevertToCurSnapAction->shortcut().toString()));
    mDiscardCurSnapAndStateAction->setToolTip (mDiscardCurSnapAndStateAction->text().remove ('&').remove ('.') +
        QString (" (%1)").arg (mDiscardCurSnapAndStateAction->shortcut().toString()));
    mShowSnapshotDetailsAction->setToolTip (mShowSnapshotDetailsAction->text().remove ('&').remove ('.') +
        QString (" (%1)").arg (mShowSnapshotDetailsAction->shortcut().toString()));
}

void VBoxSnapshotsWgt::refreshAll (bool aKeepSelected /* = true */)
{
    SnapshotEditBlocker guardBlock (mEditProtector);

    QString selected, selectedFirstChild;
    if (aKeepSelected)
    {
        SnapshotWgtItem *cur = mTreeWidget->selectedItems().isEmpty() ? 0 :
            static_cast<SnapshotWgtItem*> (mTreeWidget->selectedItems() [0]);
        if (cur)
        {
            selected = cur->snapshotId();
            if (cur->child (0))
                selectedFirstChild =
                    static_cast<SnapshotWgtItem*> (cur->child (0))->snapshotId();
        }
    }

    mTreeWidget->clear();

    if (mMachine.isNull())
    {
        onCurrentChanged (NULL);
        return;
    }

    /* get the first snapshot */
    if (mMachine.GetSnapshotCount() > 0)
    {
        CSnapshot snapshot = mMachine.GetSnapshot (QString::null);

        populateSnapshots (snapshot, 0);
        Assert (mCurSnapshotItem);

        /* add the "current state" item */
        new SnapshotWgtItem (mCurSnapshotItem, mMachine);

        SnapshotWgtItem *cur = 0;
        if (aKeepSelected)
        {
            cur = findItem (selected);
            if (cur == 0)
                cur = findItem (selectedFirstChild);
        }
        if (cur == 0)
            cur = curStateItem();
        mTreeWidget->scrollToItem (cur);
        mTreeWidget->setCurrentItem (cur);
        onCurrentChanged (cur);
    }
    else
    {
        mCurSnapshotItem = 0;

        /* add the "current state" item */
        SnapshotWgtItem *csi = new SnapshotWgtItem (mTreeWidget, mMachine);
        mTreeWidget->setCurrentItem (csi);
        onCurrentChanged (csi);
    }

    mTreeWidget->resizeColumnToContents (0);
}

SnapshotWgtItem* VBoxSnapshotsWgt::findItem (const QString &aSnapshotId)
{
    QTreeWidgetItemIterator it (mTreeWidget);
    while (*it)
    {
        SnapshotWgtItem *lvi = static_cast<SnapshotWgtItem*> (*it);
        if (lvi->snapshotId() == aSnapshotId)
            return lvi;
        ++ it;
    }

    return 0;
}

SnapshotWgtItem *VBoxSnapshotsWgt::curStateItem()
{
    QTreeWidgetItem *csi = mCurSnapshotItem ? mCurSnapshotItem->child (0)
                         : mTreeWidget->invisibleRootItem()->child (0);
    Assert (csi);
    return static_cast<SnapshotWgtItem*> (csi);
}

void VBoxSnapshotsWgt::populateSnapshots (const CSnapshot &aSnapshot, QTreeWidgetItem *item)
{
    SnapshotWgtItem *si = 0;
    si = item ? new SnapshotWgtItem (item, aSnapshot) :
                new SnapshotWgtItem (mTreeWidget, aSnapshot);

    if (mMachine.GetCurrentSnapshot().GetId() == aSnapshot.GetId())
    {
        si->setBold (true);
        mCurSnapshotItem = si;
    }

    CSnapshotVector snapvec = aSnapshot.GetChildren();
    for (int i = 0; i < snapvec.size(); ++i)
    {
        CSnapshot sn = snapvec[i];
        populateSnapshots (sn, si);
    }

    si->setExpanded (true);
    si->setFlags (si->flags() | Qt::ItemIsEditable);
}

