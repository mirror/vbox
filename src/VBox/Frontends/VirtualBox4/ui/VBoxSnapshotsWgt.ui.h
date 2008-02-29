//Added by qt3to4:
#include <Q3PopupMenu>
/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Snapshot details dialog (Qt Designer)
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

/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/

/** QListViewItem subclass for snapshots */
class VBoxSnapshotsWgt::ListViewItem : public Q3ListViewItem
{
public:

    /** Normal snapshot item */
    ListViewItem (Q3ListView *lv, const CSnapshot &aSnapshot)
        : Q3ListViewItem (lv)
        , mBld (false), mItal (false)
        , mSnapshot (aSnapshot)
    {
        recache();
    }

    /** Normal snapshot item */
    ListViewItem (Q3ListViewItem *lvi, const CSnapshot &aSnapshot)
        : Q3ListViewItem (lvi)
        , mBld (false), mItal (false)
        , mSnapshot (aSnapshot)
    {
        recache();
    }

    /** Current state item */
    ListViewItem (Q3ListView *lv, const CMachine &aMachine)
        : Q3ListViewItem (lv)
        , mBld (false), mItal (true)
        , mMachine (aMachine)
    {
        recache();
        updateCurrentState (mMachine.GetState());
    }

    /** Current state item */
    ListViewItem (Q3ListViewItem *lvi, const CMachine &aMachine)
        : Q3ListViewItem (lvi)
        , mBld (false), mItal (true)
        , mMachine (aMachine)
    {
        recache();
        updateCurrentState (mMachine.GetState());
    }

    bool bold() const { return mBld; }
    void setBold (bool bold)
    {
        mBld = bold;
        repaint();
    }

    bool italic() const { return mItal; }
    void setItalic (bool italic)
    {
        mItal = italic;
        repaint();
    }

    void paintCell (QPainter *p, const QColorGroup &cg, int column, int width, int align)
    {
        QFont font = p->font();
        if (font.bold() != mBld)
            font.setBold (mBld);
        if (font.italic() != mItal)
            font.setItalic (mItal);
        if (font != p->font())
            p->setFont (font);
        Q3ListViewItem::paintCell (p, cg, column, width, align);
    }

    int width (const QFontMetrics &fm, const Q3ListView *lv, int c) const
    {
        QFont font = lv->font();
        if (font.bold() != mBld)
            font.setBold (mBld);
        if (font.italic() != mItal)
            font.setItalic (mItal);
        if (font != lv->font())
            return Q3ListViewItem::width (QFontMetrics (font), lv, c);
        return Q3ListViewItem::width (fm, lv, c);
    }

    CSnapshot snapshot() const { return mSnapshot; }
    QUuid snapshotId() const { return mId; }

    KMachineState machineState() const { return mMachineState; }

    void recache()
    {
        if (!mSnapshot.isNull())
        {
            mId = mSnapshot.GetId();
            setText (0, mSnapshot.GetName());
            mOnline = mSnapshot.GetOnline();
            setPixmap (0, vboxGlobal().snapshotIcon (mOnline));
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
    }

    void updateCurrentState (KMachineState aState)
    {
        AssertReturn (!mMachine.isNull(), (void) 0);
        setPixmap (0, vboxGlobal().toIcon (aState));
        mMachineState = aState;
        mTimestamp.setTime_t (mMachine.GetLastStateChange() / 1000);
    }

    QString toolTipText() const
    {
        QString name = !mSnapshot.isNull() ?
                       text (0) : text (0);

        bool dateTimeToday = mTimestamp.date() == QDate::currentDate();
        QString dateTime = dateTimeToday ?
                           mTimestamp.time().toString (Qt::LocalDate) :
                           mTimestamp.toString (Qt::LocalDate);

        QString details;
        if (!mSnapshot.isNull())
        {
            /* the current snapshot is always bold */
            if (mBld)
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
            toolTip += "<br><hr>" + mDesc;

        return toolTip;
    }

    void okRename (int aCol)
    {
        Q3ListViewItem::okRename (aCol);
        AssertReturn (aCol == 0 && !mSnapshot.isNull(), (void) 0);
        mSnapshot.SetName (text (0));
    }

private:

    bool mBld : 1;
    bool mItal : 1;

    CSnapshot mSnapshot;
    CMachine mMachine;

    QUuid mId;
    bool mOnline;
    QString mDesc;
    QDateTime mTimestamp;

    bool mCurStateModified;
    KMachineState mMachineState;
};

////////////////////////////////////////////////////////////////////////////////

/** Tooltips for snapshots */
#warning port me
//class VBoxSnapshotsWgt::ToolTip : public QToolTip
//{
//public:
//
//    ToolTip (Q3ListView *aLV, QWidget *aParent, QToolTipGroup *aTG = 0)
//        : QToolTip (aParent, aTG), mLV (aLV)
//    {}
//
//    virtual ~ToolTip()
//    {
//        remove (parentWidget());
//    }
//
//    void maybeTip (const QPoint &aPnt);
//
//private:
//
//    Q3ListView *mLV;
//};

//void VBoxSnapshotsWgt::ToolTip::maybeTip (const QPoint &aPnt)
//{
//    ListViewItem *lvi = static_cast <ListViewItem *> (mLV->itemAt (aPnt));
//    if (!lvi)
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
//	    int index = toolTipGroup->metaObject()->findSignal("removeTip()", false);
//	    toolTipGroup->qt_emit (index, 0);
//        }
//    }
//
//    tip (mLV->itemRect (lvi), lvi->toolTipText());
//}

////////////////////////////////////////////////////////////////////////////////

void VBoxSnapshotsWgt::init()
{
    mCurSnapshotItem = 0;

    listView->setItemMargin (2);
    listView->header()->hide();
    listView->setRootIsDecorated (true);
    /* we have our own tooltips */
    listView->setShowToolTips (false);
    /* disable sorting */
    listView->setSorting (-1);
    /* disable unselecting items by clicking in the unused area of the list */
    new QIListViewSelectionPreserver (this, listView);

    /* toolbar */
    VBoxToolBar *toolBar = new VBoxToolBar (0, this, "snapshotToolBar");

    curStateActionGroup->addTo (toolBar);
    toolBar->addSeparator();
    snapshotActionGroup->addTo (toolBar);
    toolBar->addSeparator();
    showSnapshotDetailsAction->addTo (toolBar);

    toolBar->setUsesTextLabel (false);
    toolBar->setUsesBigPixmaps (true);
    toolBar->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);
#warning port me
//    VBoxSnapshotsWgtLayout->insertWidget (0, toolBar);
#ifdef Q_WS_MAC
    toolBar->setMacStyle();
#endif

    /* context menu */
    mContextMenu = new Q3PopupMenu (this);
    mContextMenuDirty = true;

    /* icons */
    discardSnapshotAction->setIconSet (VBoxGlobal::iconSetEx (
        "discard_snapshot_22px.png", "discard_snapshot_16px.png",
        "discard_snapshot_dis_22px.png", "discard_snapshot_dis_16px.png"));
    takeSnapshotAction->setIconSet (VBoxGlobal::iconSetEx (
        "take_snapshot_22px.png", "take_snapshot_16px.png",
        "take_snapshot_dis_22px.png", "take_snapshot_dis_16px.png"));
    revertToCurSnapAction->setIconSet (VBoxGlobal::iconSetEx (
        "discard_cur_state_22px.png", "discard_cur_state_16px.png",
        "discard_cur_state_dis_22px.png", "discard_cur_state_dis_16px.png"));
    discardCurSnapAndStateAction->setIconSet (VBoxGlobal::iconSetEx (
        "discard_cur_state_snapshot_22px.png", "discard_cur_state_snapshot_16px.png",
        "discard_cur_state_snapshot_dis_22px.png", "discard_cur_state_snapshot_dis_16px.png"));
    showSnapshotDetailsAction->setIconSet (VBoxGlobal::iconSetEx (
        "show_snapshot_details_22px.png", "show_snapshot_details_16px.png",
        "show_snapshot_details_dis_22px.png", "show_snapshot_details_dis_16px.png"));

    /* tooltip */
#warning port me
//    mToolTip = new ToolTip (listView, listView->viewport());
}

void VBoxSnapshotsWgt::destroy()
{
    delete mToolTip;
}

void VBoxSnapshotsWgt::setMachine (const CMachine &aMachine)
{
    mMachine = aMachine;

    if (aMachine.isNull())
    {
        mMachineId = QUuid();
        mSessionState = KSessionState_Null;
    }
    else
    {
        mMachineId = aMachine.GetId();
        mSessionState = aMachine.GetSessionState();
    }

    refreshAll();
}

VBoxSnapshotsWgt::ListViewItem *VBoxSnapshotsWgt::findItem (const QUuid &aSnapshotId)
{
    Q3ListViewItemIterator it (listView);
    while (it.current())
    {
        ListViewItem *lvi = static_cast <ListViewItem *> (it.current());
        if (lvi->snapshotId() == aSnapshotId)
            return lvi;
        ++ it;
    }

    return 0;
}

void VBoxSnapshotsWgt::refreshAll (bool aKeepSelected /* = false */)
{
    QUuid selected, selectedFirstChild;
    if (aKeepSelected)
    {
        ListViewItem *cur = static_cast <ListViewItem *> (listView->selectedItem());
        Assert (cur);
        if (cur)
        {
            selected = cur->snapshotId();
            if (cur->firstChild())
                selectedFirstChild =
                    static_cast <ListViewItem *> (cur->firstChild())->snapshotId();
        }
    }

    listView->clear();

    if (mMachine.isNull())
    {
        listView_currentChanged (NULL);
        return;
    }

    /* get the first snapshot */
    if (mMachine.GetSnapshotCount() > 0)
    {
        CSnapshot snapshot = mMachine.GetSnapshot (QUuid());

        populateSnapshots (snapshot, 0);
        Assert (mCurSnapshotItem);

        /* add the "current state" item */
        new ListViewItem (mCurSnapshotItem, mMachine);

        ListViewItem *cur = 0;
        if (aKeepSelected)
        {
            cur = findItem (selected);
            if (cur == 0)
                cur = findItem (selectedFirstChild);
        }
        if (cur == 0)
            cur = curStateItem();
        listView->setSelected (cur, true);
        listView->ensureItemVisible (cur);
    }
    else
    {
        mCurSnapshotItem = NULL;

        /* add the "current state" item */
        ListViewItem *csi = new ListViewItem (listView, mMachine);

        listView->setSelected (csi, true);
        /*
         *  stupid Qt doesn't issue this signal when only one item is added
         *  to the empty list -- do it manually
         */
        listView_currentChanged (csi);
    }

    listView->adjustColumn (0);
}

VBoxSnapshotsWgt::ListViewItem *VBoxSnapshotsWgt::curStateItem()
{
    Q3ListViewItem *csi = mCurSnapshotItem ? mCurSnapshotItem->firstChild()
                                          : listView->firstChild();
    Assert (csi);
    return static_cast <ListViewItem *> (csi);
}

void VBoxSnapshotsWgt::populateSnapshots (const CSnapshot &snapshot, Q3ListViewItem *item)
{
    ListViewItem *si = 0;
    if (item)
        si = new ListViewItem (item, snapshot);
    else
        si = new ListViewItem (listView, snapshot);

    if (mMachine.GetCurrentSnapshot().GetId() == snapshot.GetId())
    {
        si->setBold (true);
        mCurSnapshotItem = si;
    }

    CSnapshotEnumerator en = snapshot.GetChildren().Enumerate();
    while (en.HasMore())
    {
        CSnapshot sn = en.GetNext();
        populateSnapshots (sn, si);
    }

    si->setOpen (true);
    si->setRenameEnabled (0, true);
}

void VBoxSnapshotsWgt::listView_currentChanged (Q3ListViewItem *item)
{
    /* Make the selected item visible */
    if (item)
    {
        QPoint oldPos (listView->contentsX(), listView->contentsY());
        listView->ensureItemVisible (item);
        listView->setContentsPos (listView->treeStepSize() * item->depth(),
                                  listView->contentsY());
        /* Sometimes (here, when both X and Y are changed), affected items
         * are not properly updated after moving the contents. Fix it. */
        if (oldPos != QPoint (listView->contentsX(), listView->contentsY()))
            listView->triggerUpdate();
    }

    /* whether another direct session is open or not */
    bool busy = mSessionState != KSessionState_Closed;

    /* enable/disable snapshot actions */
    snapshotActionGroup->setEnabled (!busy &&
        item && mCurSnapshotItem && item != mCurSnapshotItem->firstChild());

    /* enable/disable the details action regardles of the session state */
    showSnapshotDetailsAction->setEnabled (
        item && mCurSnapshotItem && item != mCurSnapshotItem->firstChild());

    /* enable/disable current state actions */
    curStateActionGroup->setEnabled (!busy &&
        item && mCurSnapshotItem && item == mCurSnapshotItem->firstChild());

    /* the Take Snapshot action is always enabled for the current state */
    takeSnapshotAction->setEnabled ((!busy && curStateActionGroup->isEnabled()) ||
                                    (item && !mCurSnapshotItem));

    mContextMenuDirty = true;
}

void VBoxSnapshotsWgt::
listView_contextMenuRequested (Q3ListViewItem *item, const QPoint &pnt,
                               int /* col */)
{
    if (!item)
        return;

    if (mContextMenuDirty)
    {
        mContextMenu->clear();

        if (!mCurSnapshotItem)
        {
            /* we have only one item -- current state */
            curStateActionGroup->addTo (mContextMenu);
        }
        else
        {
            if (item == mCurSnapshotItem->firstChild())
            {
                /* current state is selected */
                curStateActionGroup->addTo (mContextMenu);
            }
            else
            {
                /* snapshot is selected */
                snapshotActionGroup->addTo (mContextMenu);
                mContextMenu->insertSeparator();
                showSnapshotDetailsAction->addTo (mContextMenu);
            }
        }

        mContextMenuDirty = false;
    }

    mContextMenu->exec (pnt);
}

void VBoxSnapshotsWgt::discardSnapshot()
{
    ListViewItem *item = static_cast <ListViewItem *> (listView->currentItem());
    AssertReturn (item, (void) 0);

    QUuid snapId = item->snapshotId();
    AssertReturn (!snapId.isNull(), (void) 0);

    /* open a direct session (this call will handle all errors) */
    CSession session = vboxGlobal().openSession (mMachineId);
    if (session.isNull())
        return;

    CConsole console = session.GetConsole();
    CProgress progress = console.DiscardSnapshot (snapId);
    if (console.isOk())
    {
        /* show the progress dialog */
        vboxProblem().showModalProgressDialog (progress, mMachine.GetName(),
                                               vboxProblem().mainWindowShown());

        if (progress.GetResultCode() != 0)
            vboxProblem().cannotDiscardSnapshot (progress, mMachine.GetSnapshot (snapId));
    }
    else
        vboxProblem().cannotDiscardSnapshot (console, mMachine.GetSnapshot (snapId));

    session.Close();
}

void VBoxSnapshotsWgt::takeSnapshot()
{
    ListViewItem *item = static_cast <ListViewItem *> (listView->currentItem());
    AssertReturn (item, (void) 0);

    VBoxTakeSnapshotDlg dlg (this, "VBoxTakeSnapshotDlg");

    QString typeId = mMachine.GetOSTypeId();
    dlg.pmIcon->setPixmap (vboxGlobal().vmGuestOSTypeIcon (typeId));

    /* search for the max available filter index */
    int maxSnapShotIndex = 0;
    QString snapShotName = tr ("Snapshot %1");
    QRegExp regExp (QString ("^") + snapShotName.arg ("([0-9]+)") + QString ("$"));
    Q3ListViewItemIterator iterator (listView);
    while (*iterator)
    {
        QString snapShot = (*iterator)->text (0);
        int pos = regExp.search (snapShot);
        if (pos != -1)
            maxSnapShotIndex = regExp.cap (1).toInt() > maxSnapShotIndex ?
                               regExp.cap (1).toInt() : maxSnapShotIndex;
        ++ iterator;
    }
    dlg.leName->setText (snapShotName.arg (maxSnapShotIndex + 1));

    if (dlg.exec() == QDialog::Accepted)
    {
        /* open a direct session (this call will handle all errors) */
        CSession session = vboxGlobal().openSession (mMachineId);
        if (session.isNull())
            return;

        CConsole console = session.GetConsole();
        CProgress progress =
                console.TakeSnapshot (dlg.leName->text().stripWhiteSpace(),
                                      dlg.txeDescription->text());
        if (console.isOk())
        {
            /* show the progress dialog */
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
    ListViewItem *item = static_cast <ListViewItem *> (listView->currentItem());
    AssertReturn (item, (void) 0);

    /* open a direct session (this call will handle all errors) */
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
    ListViewItem *item = static_cast <ListViewItem *> (listView->currentItem());
    AssertReturn (item, (void) 0);

    /* open a direct session (this call will handle all errors) */
    CSession session = vboxGlobal().openSession (mMachineId);
    if (session.isNull())
        return;

    CConsole console = session.GetConsole();
    CProgress progress = console.DiscardCurrentSnapshotAndState();
    if (console.isOk())
    {
        /* show the progress dialog */
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
    ListViewItem *item = static_cast <ListViewItem *> (listView->currentItem());
    AssertReturn (item, (void) 0);

    CSnapshot snap = item->snapshot();
    AssertReturn (!snap.isNull(), (void) 0);

    CMachine snapMachine = snap.GetMachine();

    VBoxSnapshotDetailsDlg dlg (this);
    dlg.getFromSnapshot (snap);

    if (dlg.exec() == QDialog::Accepted)
    {
        dlg.putBackToSnapshot();
    }
}

void VBoxSnapshotsWgt::machineDataChanged (const VBoxMachineDataChangeEvent &aE)
{
    if (aE.id != mMachineId)
        return; /* not interested in other machines */

    curStateItem()->recache();
}

void VBoxSnapshotsWgt::machineStateChanged (const VBoxMachineStateChangeEvent &aE)
{
    if (aE.id != mMachineId)
        return; /* not interested in other machines */

    curStateItem()->recache();
    curStateItem()->updateCurrentState (aE.state);
}

void VBoxSnapshotsWgt::sessionStateChanged (const VBoxSessionStateChangeEvent &aE)
{
    if (aE.id != mMachineId)
        return; /* not interested in other machines */

    mSessionState = aE.state;
    listView_currentChanged (listView->currentItem());
}

void VBoxSnapshotsWgt::snapshotChanged (const VBoxSnapshotEvent &aE)
{
    if (aE.machineId != mMachineId)
        return; /* not interested in other machines */

    switch (aE.what)
    {
        case VBoxSnapshotEvent::Taken:
        case VBoxSnapshotEvent::Discarded:
        {
            refreshAll (true);
            break;
        }
        case VBoxSnapshotEvent::Changed:
        {
            ListViewItem *lvi = findItem (aE.snapshotId);
            if (!lvi)
                refreshAll();
            else
            {
                lvi->recache();
                lvi->repaint();
            }
            break;
        }
    }
}

