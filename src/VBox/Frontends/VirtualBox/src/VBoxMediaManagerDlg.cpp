/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxMediaManagerDlg class implementation
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

#include "VBoxMediaManagerDlg.h"
#include "VBoxToolBar.h"
#include "QILabel.h"
#include "VBoxNewHDWzd.h"
#include "VBoxProblemReporter.h"
#include "VBoxGlobal.h"

/* Qt includes */
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QMenuBar>
#include <QPushButton>
#include <QUrl>
#include <QProgressBar>
#include <QTimer>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>

class AddVDMUrlsEvent: public QEvent
{
public:

    AddVDMUrlsEvent (const QList <QUrl> &aUrls)
        : QEvent (static_cast <QEvent::Type> (VBoxDefs::AddVDMUrlsEventType))
        , mUrls (aUrls)
    {}

    const QList <QUrl> &urls() const { return mUrls; }

private:

    const QList <QUrl> mUrls;
};

class MediaItem : public QTreeWidgetItem
{
public:

    MediaItem (MediaItem *aParent, const VBoxMedium &aMedium,
               const VBoxMediaManagerDlg *aManager)
        : QTreeWidgetItem (aParent, QITreeWidget::BasicItemType)
        , mMedium (aMedium)
        , mManager (aManager)
    { refresh(); }


    MediaItem (QTreeWidget *aParent, const VBoxMedium &aMedium,
               const VBoxMediaManagerDlg *aManager)
        : QTreeWidgetItem (aParent, QITreeWidget::BasicItemType)
        , mMedium (aMedium)
        , mManager (aManager)
    { refresh(); }

    const VBoxMedium &medium() const { return mMedium; }

    void setMedium (const VBoxMedium &aMedium)
    {
        mMedium = aMedium;
        refresh();
    }

    void refreshAll();

    VBoxDefs::MediaType type() const { return mMedium.type(); }

    KMediaState state() const { return mMedium.state (!mManager->showDiffs()); }

    QString id() const { return mMedium.id(); }
    QString location() const { return mMedium.location (!mManager->showDiffs()); }

    QString hardDiskFormat() const
        { return mMedium.hardDiskFormat (!mManager->showDiffs()); }
    QString hardDiskType() const
        { return mMedium.hardDiskType (!mManager->showDiffs()); }

    QString usage() const { return mMedium.usage (!mManager->showDiffs()); }

    QString toolTip() const
    {
        return mMedium.toolTip (!mManager->showDiffs(),
                                mManager->inAttachMode());
    }

    bool isUsed() const { return mMedium.isUsed(); }
    bool isUsedInSnapshots() const { return mMedium.isUsedInSnapshots(); }

    bool operator< (const QTreeWidgetItem &aOther) const
    {
        int column = treeWidget()->sortColumn();
        ULONG64 thisValue = vboxGlobal().parseSize (       text (column));
        ULONG64 thatValue = vboxGlobal().parseSize (aOther.text (column));
        if (thisValue && thatValue)
            return thisValue < thatValue;
        else
            return QTreeWidgetItem::operator< (aOther);
    }

//    void paintCell (QPainter *aPainter, const QColorGroup &aColorGroup,
//                    int aColumn, int aWidth, int aSlign)
//    {
//        QColorGroup cGroup (aColorGroup);
//        if (mStatus == KMediaState_NotCreated)
//            cGroup.setColor (QColorGroup::Text, cGroup.mid());
//        Q3ListViewItem::paintCell (aPainter, cGroup, aColumn, aWidth, aSlign);
//    }
protected:

    void refresh();

private:

    /* Private member vars */
    VBoxMedium mMedium;
    const VBoxMediaManagerDlg *mManager;
};

/**
 * Refreshes the underlying medium (see VBoxMedium::refresh()) and then
 * refreshes this item's columns based on the new values.
 */
void MediaItem::refreshAll()
{
    mMedium.refresh();
    refresh();
}

/**
 * Refreshes the item representation according to the associated medium.
 *
 * Note that the underlying medium itself is not refreshed.
 */
void MediaItem::refresh()
{
    /* Fill in columns */
    setIcon (0, mMedium.icon (!mManager->showDiffs(),
                              mManager->inAttachMode()));
    /* Set the text */
    setText (0, mMedium.name (!mManager->showDiffs()));
    setText (1, mMedium.logicalSize (!mManager->showDiffs()));
    setText (2, mMedium.size (!mManager->showDiffs()));
    /* All columns get the same tooltip */
    QString tt = mMedium.toolTip (!mManager->showDiffs());
    for (int i = 0; i < treeWidget()->columnCount(); ++i)
        setToolTip (i, tt);
}


/**
 * Iterator for MediaItem.
 */
class MediaItemIterator : public QTreeWidgetItemIterator
{
public:

    MediaItemIterator (QTreeWidget* aTree)
        : QTreeWidgetItemIterator (aTree) {}

    MediaItem* operator*()
    {
        QTreeWidgetItem *item = QTreeWidgetItemIterator::operator*();
        return item && item->type() == QITreeWidget::BasicItemType ?
            static_cast <MediaItem*> (item) : 0;
    }

    MediaItemIterator& operator++()
    {
        return static_cast <MediaItemIterator&> (QTreeWidgetItemIterator::operator++());
    }
};


class VBoxProgressBar: public QWidget
{
    Q_OBJECT;

public:

    VBoxProgressBar (QWidget *aParent)
        : QWidget (aParent)
    {
        mText = new QLabel (this);
        mProgressBar = new QProgressBar (this);
        mProgressBar->setTextVisible (false);

        QHBoxLayout *layout = new QHBoxLayout (this);
        VBoxGlobal::setLayoutMargin (layout, 0);
        layout->addWidget (mText);
        layout->addWidget (mProgressBar);
    }

    void setText (const QString &aText) { mText->setText (aText); }
    void setValue (int aValue) { mProgressBar->setValue (aValue); }
    void setMaximum (int aValue) { mProgressBar->setMaximum (aValue); }

    int value() const { return mProgressBar->value(); }

private:

    /* Private member vars */
    QLabel *mText;
    QProgressBar *mProgressBar;
};


VBoxMediaManagerDlg* VBoxMediaManagerDlg::mModelessDialog = 0;

VBoxMediaManagerDlg::VBoxMediaManagerDlg (QWidget *aParent /* = 0 */,
                                          Qt::WindowFlags aFlags /* = Qt::Dialog */)
    : QIWithRetranslateUI2 <QIMainDialog> (aParent, aFlags)
    , mType (VBoxDefs::MediaType_Invalid)
    , mShowDiffs (true)
    , mSetupMode (false)
{
    /* Apply UI decorations */
    Ui::VBoxMediaManagerDlg::setupUi (this);

    /* Apply window icons */
    setWindowIcon (vboxGlobal().iconSetFull (QSize (32, 32), QSize (16, 16),
                                             ":/diskimage_32px.png", ":/diskimage_16px.png"));

    mVBox = vboxGlobal().virtualBox();
    Assert (!mVBox.isNull());

    mHardDisksInaccessible =
        mDVDImagesInaccessible =
            mFloppyImagesInaccessible = false;

    mHardDiskIcon    = VBoxGlobal::iconSet (":/hd_16px.png", ":/hd_disabled_16px.png");
    mDVDImageIcon    = VBoxGlobal::iconSet (":/cd_16px.png", ":/cd_disabled_16px.png");
    mFloppyImageIcon = VBoxGlobal::iconSet (":/fd_16px.png", ":/fd_disabled_16px.png");

    /* Setup tab widget icons */
    mTwImages->setTabIcon (HDTab, mHardDiskIcon);
    mTwImages->setTabIcon (CDTab, mDVDImageIcon);
    mTwImages->setTabIcon (FDTab, mFloppyImageIcon);

    connect (mTwImages, SIGNAL (currentChanged (int)),
             this, SLOT (processCurrentChanged (int)));

    /* Setup the tree view widgets */
    mHardDiskView->sortItems (0, Qt::AscendingOrder);
    mHardDiskView->header()->setResizeMode (0, QHeaderView::Fixed);
    mHardDiskView->header()->setResizeMode (1, QHeaderView::ResizeToContents);
    mHardDiskView->header()->setResizeMode (2, QHeaderView::ResizeToContents);
    mHardDiskView->header()->setStretchLastSection (false);
    mHardDiskView->setSortingEnabled (true);
    mHardDiskView->setSupportedDropActions (Qt::LinkAction);
    mHardDiskView->installEventFilter (this);
    connect (mHardDiskView, SIGNAL (currentItemChanged (QTreeWidgetItem *, QTreeWidgetItem *)),
             this, SLOT (processCurrentChanged (QTreeWidgetItem *, QTreeWidgetItem *)));
    connect (mHardDiskView, SIGNAL (itemDoubleClicked (QTreeWidgetItem *, int)),
             this, SLOT (processDoubleClick (QTreeWidgetItem *, int)));
    connect (mHardDiskView, SIGNAL (customContextMenuRequested (const QPoint &)),
             this, SLOT (showContextMenu (const QPoint &)));
    connect (mHardDiskView, SIGNAL (resized (const QSize&, const QSize&)),
             this, SLOT (makeRequestForAdjustTable()));
    connect (mHardDiskView->header(), SIGNAL (sectionResized (int, int, int)),
             this, SLOT (makeRequestForAdjustTable()));

    mDVDView->sortItems (0, Qt::AscendingOrder);
    mDVDView->header()->setResizeMode (0, QHeaderView::Fixed);
    mDVDView->header()->setResizeMode (1, QHeaderView::ResizeToContents);
    mDVDView->header()->setStretchLastSection (false);
    mDVDView->setSortingEnabled (true);
    mDVDView->setSupportedDropActions (Qt::LinkAction);
    mDVDView->installEventFilter (this);
    connect (mDVDView, SIGNAL (currentItemChanged (QTreeWidgetItem *, QTreeWidgetItem *)),
             this, SLOT (processCurrentChanged (QTreeWidgetItem *, QTreeWidgetItem *)));
    connect (mDVDView, SIGNAL (itemDoubleClicked (QTreeWidgetItem *, int)),
             this, SLOT (processDoubleClick (QTreeWidgetItem *, int)));
    connect (mDVDView, SIGNAL (customContextMenuRequested (const QPoint &)),
             this, SLOT (showContextMenu (const QPoint &)));
    connect (mDVDView, SIGNAL (resized (const QSize&, const QSize&)),
             this, SLOT (makeRequestForAdjustTable()));
    connect (mDVDView->header(), SIGNAL (sectionResized (int, int, int)),
             this, SLOT (makeRequestForAdjustTable()));

    mFloppyView->sortItems (0, Qt::AscendingOrder);
    mFloppyView->header()->setResizeMode (0, QHeaderView::Fixed);
    mFloppyView->header()->setResizeMode (1, QHeaderView::ResizeToContents);
    mFloppyView->header()->setStretchLastSection (false);
    mFloppyView->setSortingEnabled (true);
    mFloppyView->setSupportedDropActions (Qt::LinkAction);
    mFloppyView->installEventFilter (this);
    connect (mFloppyView, SIGNAL (currentItemChanged (QTreeWidgetItem *, QTreeWidgetItem *)),
             this, SLOT (processCurrentChanged (QTreeWidgetItem *, QTreeWidgetItem *)));
    connect (mFloppyView, SIGNAL (itemDoubleClicked (QTreeWidgetItem *, int)),
             this, SLOT (processDoubleClick (QTreeWidgetItem *, int)));
    connect (mFloppyView, SIGNAL (customContextMenuRequested (const QPoint &)),
             this, SLOT (showContextMenu (const QPoint &)));
    connect (mFloppyView, SIGNAL (resized (const QSize&, const QSize&)),
             this, SLOT (makeRequestForAdjustTable()));
    connect (mFloppyView->header(), SIGNAL (sectionResized (int, int, int)),
             this, SLOT (makeRequestForAdjustTable()));

    /* Context menu composing */
    mActionsContextMenu = new QMenu (this);

    mNewAction     = new QAction (this);
    mAddAction     = new QAction (this);
//  mEditAction    = new QAction (this);
    mRemoveAction  = new QAction (this);
    mReleaseAction = new QAction (this);
    mRefreshAction = new QAction (this);

    connect (mNewAction, SIGNAL (triggered()),
             this, SLOT (doNewMedium()));
    connect (mAddAction, SIGNAL (triggered()),
             this, SLOT (doAddMedium()));
//  connect (mEditAction, SIGNAL (triggered()),
//           this, SLOT (editImage()));
    connect (mRemoveAction, SIGNAL (triggered()),
             this, SLOT (doRemoveMedium()));
    connect (mReleaseAction, SIGNAL (triggered()),
             this, SLOT (doReleaseMedium()));
    connect (mRefreshAction, SIGNAL (triggered()),
             this, SLOT (refreshAll()));

    mNewAction->setIcon (VBoxGlobal::iconSetFull (
        QSize (22, 22), QSize (16, 16),
        ":/vdm_new_22px.png", ":/vdm_new_16px.png",
        ":/vdm_new_disabled_22px.png", ":/vdm_new_disabled_16px.png"));
    mAddAction->setIcon (VBoxGlobal::iconSetFull (
        QSize (22, 22), QSize (16, 16),
        ":/vdm_add_22px.png", ":/vdm_add_16px.png",
        ":/vdm_add_disabled_22px.png", ":/vdm_add_disabled_16px.png"));
//  mEditAction->setIcon (VBoxGlobal::iconSet (":/guesttools_16px.png", ":/guesttools_disabled_16px.png"));
    mRemoveAction->setIcon (VBoxGlobal::iconSetFull (
        QSize (22, 22), QSize (16, 16),
        ":/vdm_remove_22px.png", ":/vdm_remove_16px.png",
        ":/vdm_remove_disabled_22px.png", ":/vdm_remove_disabled_16px.png"));
    mReleaseAction->setIcon (VBoxGlobal::iconSetFull (
        QSize (22, 22), QSize (16, 16),
        ":/vdm_release_22px.png", ":/vdm_release_16px.png",
        ":/vdm_release_disabled_22px.png", ":/vdm_release_disabled_16px.png"));
    mRefreshAction->setIcon (VBoxGlobal::iconSetFull (
        QSize (22, 22), QSize (16, 16),
        ":/refresh_22px.png", ":/refresh_16px.png",
        ":/refresh_disabled_22px.png", ":/refresh_disabled_16px.png"));

//  mActionsContextMenu->addAction (mEditAction);
    mActionsContextMenu->addAction (mRemoveAction);
    mActionsContextMenu->addAction (mReleaseAction);

    /* Toolbar composing */
    mActionsToolBar = new VBoxToolBar (this);
    mActionsToolBar->setIconSize (QSize (22, 22));
    mActionsToolBar->setToolButtonStyle (Qt::ToolButtonTextUnderIcon);
    mActionsToolBar->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Preferred);

    /* Really messy what the uic produce here: a vbox layout in an hbox layout */
    QHBoxLayout *centralLayout = qobject_cast <QHBoxLayout*> (centralWidget()->layout());
    Assert (VALID_PTR (centralLayout));
    QVBoxLayout *mainLayout = static_cast <QVBoxLayout*> (centralLayout->itemAt(0));
    Assert (VALID_PTR (mainLayout));
#if MAC_LEOPARD_STYLE
    /* Enable unified toolbars on Mac OS X. Available on Qt >= 4.3 */
    addToolBar (mActionsToolBar);
    mActionsToolBar->setMacToolbar();
    /* No spacing/margin on the mac */
    VBoxGlobal::setLayoutMargin (centralLayout, 0);
    mainLayout->insertSpacing (0, 10);
    VBoxGlobal::setLayoutMargin (mainLayout, 0);
#else /* MAC_LEOPARD_STYLE */
    /* Add the toolbar */
    mainLayout->insertWidget (0, mActionsToolBar);
    /* Set spacing/margin like in the selector window */
    centralLayout->setSpacing (0);
    VBoxGlobal::setLayoutMargin (centralLayout, 0);
    mainLayout->setSpacing (5);
    VBoxGlobal::setLayoutMargin (mainLayout, 5);
#endif /* MAC_LEOPARD_STYLE */

    mActionsToolBar->addAction (mNewAction);
    mActionsToolBar->addAction (mAddAction);
    mActionsToolBar->addSeparator();
//  mActionsToolBar->addAction (mEditAction);
    mActionsToolBar->addAction (mRemoveAction);
    mActionsToolBar->addAction (mReleaseAction);
    mActionsToolBar->addSeparator();
    mActionsToolBar->addAction (mRefreshAction);

    /* Menu bar */
    mActionsMenu = menuBar()->addMenu (QString::null);
    mActionsMenu->addAction (mNewAction);
    mActionsMenu->addAction (mAddAction);
    mActionsMenu->addSeparator();
//  mActionsMenu->addAction (mEditAction);
    mActionsMenu->addAction (mRemoveAction);
    mActionsMenu->addAction (mReleaseAction);
    mActionsMenu->addSeparator();
    mActionsMenu->addAction (mRefreshAction);

    /* Setup information pane */
    QList <QILabel*> paneList = findChildren <QILabel*>();
    foreach (QILabel *infoPane, paneList)
        infoPane->setFullSizeSelection (true);

    /* Enumeration progressbar creation */
    mProgressBar = new VBoxProgressBar (this);
    /* Add to the dialog button box */
    mButtonBox->addExtraWidget (mProgressBar);
    /* Default is invisible */
    mProgressBar->setVisible (false);

    /* Set the default button */
    mButtonBox->button (QDialogButtonBox::Ok)->setDefault (true);

    /* Connects for the button box */
    connect (mButtonBox, SIGNAL (accepted()),
             this, SLOT (accept()));
    connect (mButtonBox, SIGNAL (rejected()),
             this, SLOT (reject()));
    connect (mButtonBox, SIGNAL (helpRequested()),
             &vboxProblem(), SLOT (showHelpHelpDialog()));
}

/**
 * Sets up the dialog.
 *
 * @param aType             Media type to display (either one type or all).
 * @param aDoSelect         Whether to enable the select action (OK button).
 * @param aRefresh          Whether to do a media refresh.
 * @param aSessionMachine   Session machine object if this dialog is opened for
 *                          a machine from its settings dialog.
 * @param aSelectId         Which medium to make selected? (ignored when @a
 *                          aType is VBoxDefs::MediaType_All)
 * @param aShowDiffs        @c true to show differencing hard disks initially
 *                          (ignored if @a aSessionMachine is null assuming
 *                          @c true).
 */
void VBoxMediaManagerDlg::setup (VBoxDefs::MediaType aType, bool aDoSelect,
                                 bool aRefresh /* = true */,
                                 const CMachine &aSessionMachine /* = CMachine() */,
                                 const QString &aSelectId /* = QString() */,
                                 bool aShowDiffs /* = true */)
{
    mSetupMode = true;

    mType = aType;

    mDoSelect = aDoSelect;

    mSessionMachine = aSessionMachine;
    mSessionMachineId = mSessionMachine.isNull() ? QString::null : mSessionMachine.GetId();
    mShowDiffs = mSessionMachine.isNull() ? true : aShowDiffs;

    switch (aType)
    {
        case VBoxDefs::MediaType_HardDisk: mHDSelectedId = aSelectId; break;
        case VBoxDefs::MediaType_DVD:      mDVDSelectedId = aSelectId; break;
        case VBoxDefs::MediaType_Floppy:   mFloppySelectedId = aSelectId; break;
        case VBoxDefs::MediaType_All: break;
        default:
            AssertFailedReturnVoid();
    }

    mTwImages->setTabEnabled (HDTab,
                              aType == VBoxDefs::MediaType_All ||
                              aType == VBoxDefs::MediaType_HardDisk);
    mTwImages->setTabEnabled (CDTab,
                              aType == VBoxDefs::MediaType_All ||
                              aType == VBoxDefs::MediaType_DVD);
    mTwImages->setTabEnabled (FDTab,
                              aType == VBoxDefs::MediaType_All ||
                              aType == VBoxDefs::MediaType_Floppy);

    mDoSelect = aDoSelect;

    mButtonBox->button (QDialogButtonBox::Cancel)->setVisible (mDoSelect);

    /* Listen to "media enumeration started" signals */
    connect (&vboxGlobal(), SIGNAL (mediumEnumStarted()),
             this, SLOT (mediumEnumStarted()));
    /* Listen to "media enumeration" signals */
    connect (&vboxGlobal(), SIGNAL (mediumEnumerated (const VBoxMedium &)),
             this, SLOT (mediumEnumerated (const VBoxMedium &)));
    /* Listen to "media enumeration finished" signals */
    connect (&vboxGlobal(), SIGNAL (mediumEnumFinished (const VBoxMediaList &)),
             this, SLOT (mediumEnumFinished (const VBoxMediaList &)));

    /* Listen to "media add" signals */
    connect (&vboxGlobal(), SIGNAL (mediumAdded (const VBoxMedium &)),
             this, SLOT (mediumAdded (const VBoxMedium &)));
    /* Listen to "media update" signals */
    connect (&vboxGlobal(), SIGNAL (mediumUpdated (const VBoxMedium &)),
             this, SLOT (mediumUpdated (const VBoxMedium &)));
    /* Listen to "media remove" signals */
    connect (&vboxGlobal(), SIGNAL (mediumRemoved (VBoxDefs::MediaType, const QString &)),
             this, SLOT (mediumRemoved (VBoxDefs::MediaType, const QString &)));

    if (aRefresh && !vboxGlobal().isMediaEnumerationStarted())
        vboxGlobal().startEnumeratingMedia();
    else
    {
        /* Insert already enumerated media */
        const VBoxMediaList &list = vboxGlobal().currentMediaList();
        prepareToRefresh (list.size());
        VBoxMediaList::const_iterator it;
        int index = 0;
        for (it = list.begin(); it != list.end(); ++ it)
        {
            mediumAdded (*it);
            if ((*it).state() != KMediaState_NotCreated)
                mProgressBar->setValue (++ index);
        }

        /* Emulate the finished signal to reuse the code */
        if (!vboxGlobal().isMediaEnumerationStarted())
            mediumEnumFinished (list);
    }

    /* For a newly opened dialog, select the first item */
    if (mHardDiskView->selectedItems().isEmpty())
        if (QTreeWidgetItem *item = mHardDiskView->topLevelItem (0))
            setCurrentItem (mHardDiskView, item);
    if (mDVDView->selectedItems().isEmpty())
        if (QTreeWidgetItem *item = mDVDView->topLevelItem (0))
            setCurrentItem (mDVDView, item);
    if (mFloppyView->selectedItems().isEmpty())
        if (QTreeWidgetItem *item = mFloppyView->topLevelItem (0))
            setCurrentItem (mFloppyView, item);

    /* Applying language settings */
    retranslateUi();

    mSetupMode = false;
}

/* static */
void VBoxMediaManagerDlg::showModeless (QWidget *aCenterWidget /* = 0 */,
                                        bool aRefresh /* = true */)
{
    if (!mModelessDialog)
    {
        mModelessDialog = new VBoxMediaManagerDlg (0, Qt::Window);
        mModelessDialog->centerAccording (aCenterWidget);
        connect (vboxGlobal().mainWindow(), SIGNAL (closing()), mModelessDialog, SLOT (close()));
        mModelessDialog->setAttribute (Qt::WA_DeleteOnClose);
        mModelessDialog->setup (VBoxDefs::MediaType_All,
                                false /* aDoSelect */, aRefresh);

        /* listen to events that may change the media status and refresh
         * the contents of the modeless dialog */
        /// @todo refreshAll() may be slow, so it may be better to analyze
        //  event details and update only what is changed */
        connect (&vboxGlobal(), SIGNAL (machineDataChanged (const VBoxMachineDataChangeEvent &)),
                 mModelessDialog, SLOT (refreshAll()));
        connect (&vboxGlobal(), SIGNAL (machineRegistered (const VBoxMachineRegisteredEvent &)),
                 mModelessDialog, SLOT (refreshAll()));
        connect (&vboxGlobal(), SIGNAL (snapshotChanged (const VBoxSnapshotEvent &)),
                 mModelessDialog, SLOT (refreshAll()));
    }

    mModelessDialog->show();
    mModelessDialog->setWindowState (mModelessDialog->windowState() &
                                     ~Qt::WindowMinimized);
    mModelessDialog->activateWindow();
}

QString VBoxMediaManagerDlg::selectedId() const
{
    QTreeWidget *tree = currentTreeWidget();
    QString uuid;

    MediaItem *item = toMediaItem (selectedItem (tree));
    if (item)
        uuid = item->id();

    return uuid;
}

QString VBoxMediaManagerDlg::selectedLocation() const
{
    QTreeWidget *tree = currentTreeWidget();
    QString loc;

    MediaItem *item = toMediaItem (selectedItem (tree));
    if (item)
        loc = item->location().trimmed();

    return loc;
}

void VBoxMediaManagerDlg::refreshAll()
{
    /* Start enumerating media */
    vboxGlobal().startEnumeratingMedia();
}

void VBoxMediaManagerDlg::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxMediaManagerDlg::retranslateUi (this);

    mActionsMenu->setTitle (tr ("&Actions"));

    mNewAction->setText (tr ("&New..."));
    mAddAction->setText (tr ("&Add..."));
    // mEditAction->setText (tr ("&Edit..."));
    mRemoveAction->setText (tr ("R&emove"));
    mReleaseAction->setText (tr ("Re&lease"));
    mRefreshAction->setText (tr ("Re&fresh"));

    mNewAction->setShortcut (QKeySequence (QKeySequence::New));
    mAddAction->setShortcut (QKeySequence ("Ins"));
    // mEditAction->setShortcut (QKeySequence ("Ctrl+E"));
    mRemoveAction->setShortcut (QKeySequence (QKeySequence::Delete));
    mReleaseAction->setShortcut (QKeySequence ("Ctrl+L"));
    mRefreshAction->setShortcut (QKeySequence (QKeySequence::Refresh));

    mNewAction->setStatusTip (tr ("Create a new virtual hard disk"));
    mAddAction->setStatusTip (tr ("Add an existing medium"));
    // mEditAction->setStatusTip (tr ("Edit the properties of the selected medium"));
    mRemoveAction->setStatusTip (tr ("Remove the selected medium"));
    mReleaseAction->setStatusTip (tr ("Release the selected medium by detaching it from the machines"));
    mRefreshAction->setStatusTip (tr ("Refresh the media list"));

    mNewAction->setToolTip (mNewAction->text().remove ('&') +
        QString (" (%1)").arg (mNewAction->shortcut().toString()));
    mAddAction->setToolTip (mAddAction->text().remove ('&') +
        QString (" (%1)").arg (mAddAction->shortcut().toString()));
    // mEditAction->setToolTip (mEditAction->text().remove ('&') +
    //     QString (" (%1)").arg (mEditAction->shortcut().toString()));
    mRemoveAction->setToolTip (mRemoveAction->text().remove ('&') +
        QString (" (%1)").arg (mRemoveAction->shortcut().toString()));
    mReleaseAction->setToolTip (mReleaseAction->text().remove ('&') +
        QString (" (%1)").arg (mReleaseAction->shortcut().toString()));
    mRefreshAction->setToolTip (mRefreshAction->text().remove ('&') +
        QString (" (%1)").arg (mRefreshAction->shortcut().toString()));

    mLbHD1->setText (QString ("<nobr>%1:</nobr>").arg (tr ("Location")));
    mLbHD2->setText (QString ("<nobr>%1:</nobr>").arg (tr ("Type (Format)")));
    mLbHD3->setText (QString ("<nobr>%1:</nobr>").arg (tr ("Attached to")));

    mLbCD1->setText (QString ("<nobr>%1:</nobr>").arg (tr ("Location")));
    mLbCD2->setText (QString ("<nobr>%1:</nobr>").arg (tr ("Attached to")));

    mLbFD1->setText (QString ("<nobr>%1:</nobr>").arg (tr ("Location")));
    mLbFD2->setText (QString ("<nobr>%1:</nobr>").arg (tr ("Attached to")));

    mProgressBar->setText (tr ("Checking accessibility"));
#ifdef Q_WS_MAC
    /* Make sure that the widgets aren't jumping around while the progress bar
     * get visible. */
    mProgressBar->adjustSize();
    int h = mProgressBar->height();
    mButtonBox->setMinimumHeight (h + 12);
#endif

    if (mDoSelect)
        mButtonBox->button (QDialogButtonBox::Ok)->setText (tr ("&Select"));

    if (mHardDiskView->model()->rowCount() || mDVDView->model()->rowCount() || mFloppyView->model()->rowCount())
        refreshAll();
}

void VBoxMediaManagerDlg::closeEvent (QCloseEvent *aEvent)
{
    mModelessDialog = 0;
    aEvent->accept();
}

bool VBoxMediaManagerDlg::eventFilter (QObject *aObject, QEvent *aEvent)
{
    /* Check for interesting objects */
    if (!(aObject == mHardDiskView ||
          aObject == mDVDView ||
          aObject == mFloppyView))
        return QIMainDialog::eventFilter (aObject, aEvent);

    switch (aEvent->type())
    {
        case QEvent::DragEnter:
        {
            QDragEnterEvent *deEvent = static_cast <QDragEnterEvent*> (aEvent);
            if (deEvent->mimeData()->hasUrls())
            {
                QList <QUrl> urls = deEvent->mimeData()->urls();
                /* Sometimes urls has an empty Url entry. Filter them out. */
                urls.removeAll (QUrl());
                if (checkDndUrls (urls))
                {
                    deEvent->setDropAction (Qt::LinkAction);
                    deEvent->acceptProposedAction();
                }
            }
            return true;
            break;
        }
        case QEvent::Drop:
        {
            QDropEvent *dEvent = static_cast <QDropEvent*> (aEvent);
            if (dEvent->mimeData()->hasUrls())
            {
                QList <QUrl> urls = dEvent->mimeData()->urls();
                /* Sometimes urls has an empty Url entry. Filter them out. */
                urls.removeAll (QUrl());
                AddVDMUrlsEvent *event = new AddVDMUrlsEvent (urls);
                QApplication::postEvent (currentTreeWidget(), event);
                dEvent->acceptProposedAction();
            }
            return true;
            break;
        }
        case VBoxDefs::AddVDMUrlsEventType:
        {
            if (aObject == currentTreeWidget())
            {
                AddVDMUrlsEvent *addEvent = static_cast <AddVDMUrlsEvent*> (aEvent);
                addDndUrls (addEvent->urls());
                return true;
            }
            break;
        }
        default:
            break;
    }
    return QIMainDialog::eventFilter (aObject, aEvent);
}

void VBoxMediaManagerDlg::mediumAdded (const VBoxMedium &aMedium)
{
    /* Ignore non-interesting aMedium */
    if (mType != VBoxDefs::MediaType_All && mType != aMedium.type())
        return;

    if (!mShowDiffs && aMedium.type() == VBoxDefs::MediaType_HardDisk)
    {
        if (aMedium.parent() && !mSessionMachineId.isNull())
        {
            /* in !mShowDiffs mode, we ignore all diffs except ones that are
             * directly attached to the related VM in the current state */
            if (!aMedium.isAttachedInCurStateTo (mSessionMachineId))
                return;

            /* Since the base hard disk of this diff has been already appended,
             * we want to replace it with this diff to avoid duplicates in
             * !mShowDiffs mode. */
            MediaItem *item = searchItem (mHardDiskView, aMedium.root().id());
            AssertReturnVoid (item);

            item->setMedium (aMedium);
            updateTabIcons (item, ItemAction_Updated);
            return;
        }
    }

    MediaItem *item = 0;

    switch (aMedium.type())
    {
        case VBoxDefs::MediaType_HardDisk:
        {
            item = createHardDiskItem (mHardDiskView, aMedium);
            AssertReturnVoid (item);

            /* Damn Qt4 didn't notifies the table's QHeaderView on adding
             * new tree-widget items, so initialize the header adjustment
             * by calling resizeSections() slot... */
            QTimer::singleShot (0, mHardDiskView->header(), SLOT (resizeSections()));

            if (item->id() == mHDSelectedId)
            {
                setCurrentItem (mHardDiskView, item);
                mHDSelectedId = QString::null;
            }
            break;
        }
        case VBoxDefs::MediaType_DVD:
        {
            item = new MediaItem (mDVDView, aMedium, this);
            AssertReturnVoid (item);

            /* Damn Qt4 didn't notifies the table's QHeaderView on adding
             * new tree-widget items, so initialize the header adjustment
             * by calling resizeSections() slot... */
            QTimer::singleShot (0, mDVDView->header(), SLOT (resizeSections()));

            if (item->id() == mDVDSelectedId)
            {
                setCurrentItem (mDVDView, item);
                mDVDSelectedId = QString::null;
            }
            break;
        }
        case VBoxDefs::MediaType_Floppy:
        {
            item = new MediaItem (mFloppyView, aMedium, this);
            AssertReturnVoid (item);

            /* Damn Qt4 didn't notifies the table's QHeaderView on adding
             * new tree-widget items, so initialize the header adjustment
             * by calling resizeSections() slot... */
            QTimer::singleShot (0, mFloppyView->header(), SLOT (resizeSections()));

            if (item->id() == mFloppySelectedId)
            {
                setCurrentItem (mFloppyView, item);
                mFloppySelectedId = QString::null;
            }
            break;
        }
        default:
            AssertFailed();
    }

    AssertReturnVoid (item);

    updateTabIcons (item, ItemAction_Added);

    /* If the media enumeration process is not started we have to select the
     * newly added item as the current one for the case of new image was added
     * or created. But we have to avoid this in case of we are adding already
     * enumerated medias in setup() function when the media enumeration is not
     * running. So the mSetupMode variable reflects the setup status for it. */
    if (!mSetupMode && !vboxGlobal().isMediaEnumerationStarted())
        setCurrentItem (treeWidget (aMedium.type()), item);
    if (item == currentTreeWidget()->currentItem())
        processCurrentChanged (item);
}

void VBoxMediaManagerDlg::mediumUpdated (const VBoxMedium &aMedium)
{
    /* Ignore non-interesting aMedium */
    if (mType != VBoxDefs::MediaType_All && mType != aMedium.type())
        return;

    MediaItem *item = 0;

    switch (aMedium.type())
    {
        case VBoxDefs::MediaType_HardDisk:
        {
            item = searchItem (mHardDiskView, aMedium.id());
            break;
        }
        case VBoxDefs::MediaType_DVD:
        {
            item = searchItem (mDVDView, aMedium.id());
            break;
        }
        case VBoxDefs::MediaType_Floppy:
        {
            item = searchItem (mFloppyView, aMedium.id());
            break;
        }
        default:
            AssertFailed();
    }

    /* There may be unwanted items due to !mShowDiffs */
    if (!item)
        return;

    item->setMedium (aMedium);

    updateTabIcons (item, ItemAction_Updated);

    /* Note: current items on invisible tabs are not updated because
     * it is always done in processCurrentChanged() when the user switches
     * to an invisible tab */
    if (item == currentTreeWidget()->currentItem())
        processCurrentChanged (item);
}

void VBoxMediaManagerDlg::mediumRemoved (VBoxDefs::MediaType aType,
                                         const QString &aId)
{
    /* Ignore non-interesting aMedium */
    if (mType != VBoxDefs::MediaType_All && mType != aType)
        return;

    QTreeWidget *tree = currentTreeWidget();
    MediaItem *item = toMediaItem (searchItem (tree, aId));

    /* There may be unwanted items due to !mShowDiffs */
    if (!item)
        return;

    updateTabIcons (item, ItemAction_Removed);

    delete item;

    /* Note: current items on invisible tabs are not updated because
     * it is always done in processCurrentChanged() when the user switches
     * to an invisible tab */
    setCurrentItem (tree, tree->currentItem());
}

void VBoxMediaManagerDlg::mediumEnumStarted()
{
    /* Reset inaccessible flags */
    mHardDisksInaccessible =
        mDVDImagesInaccessible =
            mFloppyImagesInaccessible = false;

    /* Load default tab icons */
    mTwImages->setTabIcon (HDTab, mHardDiskIcon);
    mTwImages->setTabIcon (CDTab, mDVDImageIcon);
    mTwImages->setTabIcon (FDTab, mFloppyImageIcon);

    /* Load current media list */
    const VBoxMediaList &list = vboxGlobal().currentMediaList();
    prepareToRefresh (list.size());
    VBoxMediaList::const_iterator it;
    for (it = list.begin(); it != list.end(); ++ it)
        mediumAdded (*it);

    /* Select the first item to be the current one if the previous saved item
     * was not selected yet. */
    if (!mHardDiskView->currentItem())
        if (QTreeWidgetItem *item = mHardDiskView->topLevelItem (0))
            setCurrentItem (mHardDiskView, item);
    if (!mDVDView->currentItem())
        if (QTreeWidgetItem *item = mDVDView->topLevelItem (0))
            setCurrentItem (mDVDView, item);
    if (!mFloppyView->currentItem())
        if (QTreeWidgetItem *item = mFloppyView->topLevelItem (0))
            setCurrentItem (mFloppyView, item);

    processCurrentChanged();
}

void VBoxMediaManagerDlg::mediumEnumerated (const VBoxMedium &aMedium)
{
    AssertReturnVoid (aMedium.state() != KMediaState_NotCreated);

    mediumUpdated (aMedium);

    mProgressBar->setValue (mProgressBar->value() + 1);
}

void VBoxMediaManagerDlg::mediumEnumFinished (const VBoxMediaList &/* aList */)
{
    mProgressBar->setVisible (false);

    mRefreshAction->setEnabled (true);
    unsetCursor();

    processCurrentChanged();
}

void VBoxMediaManagerDlg::doNewMedium()
{
    AssertReturnVoid (currentTreeWidgetType() == VBoxDefs::MediaType_HardDisk);

    VBoxNewHDWzd dlg (this);

    if (dlg.exec() == QDialog::Accepted)
    {
        CHardDisk hd = dlg.hardDisk();
        /* Select the newly created hard disk */
        MediaItem *item = searchItem (mHardDiskView, hd.GetId());
        AssertReturnVoid (item);
        mHardDiskView->setCurrentItem (item);
    }
}

void VBoxMediaManagerDlg::doAddMedium()
{
    QTreeWidget *tree = currentTreeWidget();
    MediaItem *item = toMediaItem (tree->currentItem());

    QString title;
    QString filter;
    VBoxDefs::MediaType type = currentTreeWidgetType();

    QString dir;
    if (item && item->state() != KMediaState_Inaccessible
             && item->state() != KMediaState_NotCreated)
        dir = QFileInfo (item->location().trimmed()).absolutePath();

    if (dir.isEmpty())
        if (type == VBoxDefs::MediaType_HardDisk)
            dir = mVBox.GetSystemProperties().GetDefaultHardDiskFolder();

    if (dir.isEmpty() || !QFileInfo (dir).exists())
        dir = mVBox.GetHomeFolder();

    switch (type)
    {
        case VBoxDefs::MediaType_HardDisk:
        {
            /// @todo NEWMEDIA use CSystemProperties::GetHardDIskFormats to detect
            /// possible hard disk extensions
            QList < QPair <QString, QString> > filterList = vboxGlobal().HDDBackends();
            QStringList backends;
            QStringList allPrefix;
            for (int i = 0; i < filterList.count(); ++i)
            {
                QPair <QString, QString> item = filterList.at (i);
                /* Create one backend filter string */
                backends << QString ("%1 (%2)").arg (item.first). arg (item.second);
                /* Save the suffix's for the "All" entry */
                allPrefix << item.second;
            }
            if (!allPrefix.isEmpty())
                backends.insert (0, tr ("All hard disk images (%1)").arg (allPrefix.join (" ").trimmed()));
            backends << tr ("All files (*)");
            filter = backends.join (";;").trimmed();

            title = tr ("Select a hard disk image file");
            break;
        }
        case VBoxDefs::MediaType_DVD:
        {
            filter = tr ("CD/DVD-ROM images (*.iso);;"
                         "All files (*)");
            title = tr ("Select a CD/DVD-ROM disk image file");
            break;
        }
        case VBoxDefs::MediaType_Floppy:
        {
            filter = tr ("Floppy images (*.img);;"
                         "All files (*)");
            title = tr ("Select a floppy disk image file");
            break;
        }
        default:
            AssertMsgFailed (("Selected tree should be equal to one item in VBoxDefs::MediaType.\n"));
            break;
    }

    QStringList files = VBoxGlobal::getOpenFileNames (dir, filter, this, title);
    foreach (QString loc, files)
    {
        loc =  QDir::convertSeparators (loc);

        if (!loc.isEmpty())
            addMediumToList (loc, type);
    }
}

void VBoxMediaManagerDlg::doRemoveMedium()
{
    QTreeWidget *tree = currentTreeWidget();
    MediaItem *item = toMediaItem (tree->currentItem());
    AssertMsgReturnVoid (item, ("Current item must not be null"));

    /* Remember ID/type as they may get lost after the closure/deletion */
    QString id = item->id();
    AssertReturnVoid (!id.isNull());
    VBoxDefs::MediaType type = item->type();

    if (!vboxProblem().confirmRemoveMedium (this, item->medium()))
        return;

    COMResult result;

    switch (type)
    {
        case VBoxDefs::MediaType_HardDisk:
        {
            bool deleteStorage = false;

            /// @todo NEWMEDIA use CHardDiskFormat to find out if the format
            /// supports storage deletion

            /* We don't want to try to delete inaccessible storage as it will
             * most likely fail. Note that
             * VBoxProblemReporter::confirmRemoveMedium() is aware of that and
             * will give a corresponding hint. Therefore, once the code is
             * changed below, the hint should be re-checked for validity. */
            if (item->state() != KMediaState_Inaccessible)
            {
                int rc = vboxProblem().
                    confirmDeleteHardDiskStorage (this, item->location());
                if (rc == QIMessageBox::Cancel)
                    return;
                deleteStorage = rc == QIMessageBox::Yes;
            }

            CHardDisk hardDisk = item->medium().hardDisk();

            if (deleteStorage)
            {
                bool success = false;

                CProgress progress = hardDisk.DeleteStorage();
                if (hardDisk.isOk())
                {
                    vboxProblem().showModalProgressDialog (progress, windowTitle(),
                                                           parentWidget());
                    if (progress.isOk() && progress.GetResultCode() == S_OK)
                        success = true;
                }

                if (success)
                    vboxGlobal().removeMedium (VBoxDefs::MediaType_HardDisk, id);
                else
                    vboxProblem().cannotDeleteHardDiskStorage (this, hardDisk,
                                                               progress);

                /* We don't want to close the hard disk because it was
                 * implicitly closed and removed from the list of known media
                 * on storage deletion */
                return;
            }

            hardDisk.Close();
            result = hardDisk;
            break;
        }
        case VBoxDefs::MediaType_DVD:
        {
            CDVDImage image = item->medium().dvdImage();
            image.Close();
            result = image;
            break;
        }
        case VBoxDefs::MediaType_Floppy:
        {
            CFloppyImage image = item->medium().floppyImage();
            image.Close();
            result = image;
            break;
        }
        default:
            AssertFailedReturnVoid();
    }

    if (result.isOk())
        vboxGlobal().removeMedium (type, id);
    else
        vboxProblem().cannotCloseMedium (this, item->medium(), result);
}

void VBoxMediaManagerDlg::doReleaseMedium()
{
    QTreeWidget *tree = currentTreeWidget();
    MediaItem *item = toMediaItem (tree->currentItem());
    AssertMsgReturnVoid (item, ("Current item must not be null"));

    AssertReturnVoid (!item->id().isNull());

    /* Get the fresh attached VM list */
    item->refreshAll();

    QString usage;
    CMachineVector machines;

    const QList <QString> &machineIds = item->medium().curStateMachineIds();
    for (QList <QString>::const_iterator it = machineIds.begin();
         it != machineIds.end(); ++ it)
    {
        CMachine m = mVBox.GetMachine (*it);
        if (!mVBox.isOk())
            continue;

        if (!usage.isEmpty())
            usage += ", ";
        usage += m.GetName();

        machines.push_back (m);
    }

    if (machineIds.size() == 0)
    {
        /* It may happen that the new machine list is empty (medium was already
         * released by a third party); update the details and silently return.*/
        processCurrentChanged (item);
        return;
    }

    AssertReturnVoid (machines.size() > 0);

    if (!vboxProblem().confirmReleaseMedium (this, item->medium(), usage))
        return;

    for (QList <QString>::const_iterator it = machineIds.begin();
         it != machineIds.end(); ++ it)
    {
        if (!releaseMediumFrom (item->medium(), *it))
            break;
    }

    /* Inform others about medium changes (use a copy since data owning is not
     * clean there (to be fixed one day using shared_ptr)) */
    VBoxMedium newMedium = item->medium();
    newMedium.refresh();
    vboxGlobal().updateMedium (newMedium);
}

bool VBoxMediaManagerDlg::releaseMediumFrom (const VBoxMedium &aMedium,
                                             const QString &aMachineId)
{
    CSession session;
    CMachine machine;

    /* Is this medium is attached to the VM we are setting up */
    if (mSessionMachineId == aMachineId)
    {
        machine = mSessionMachine;
    }
    /* or to some other */
    else
    {
        session = vboxGlobal().openSession (aMachineId);
        if (session.isNull())
            return false;

        machine = session.GetMachine();
    }

    bool success = true;

    switch (aMedium.type())
    {
        case VBoxDefs::MediaType_HardDisk:
        {
            CHardDiskAttachmentVector vec =machine.GetHardDiskAttachments();
            for (int i = 0; i < vec.size(); ++ i)
            {
                CHardDiskAttachment hda = vec [i];
                if (hda.GetHardDisk().GetId() == aMedium.id())
                {
                    machine.DetachHardDisk(hda.GetController(),
                                           hda.GetPort(),
                                           hda.GetDevice());
                    if (!machine.isOk())
                    {
                        CStorageController ctl = machine.GetStorageControllerByName(hda.GetController());

                        vboxProblem().cannotDetachHardDisk (
                            this, machine, aMedium.location(), ctl.GetBus(),
                            hda.GetPort(), hda.GetDevice());
                        success = false;
                        break;
                    }
                }
            }
            break;
        }
        case VBoxDefs::MediaType_DVD:
        {
            CDVDDrive drive = machine.GetDVDDrive();
            drive.Unmount();
            if (!drive.isOk())
            {
                vboxProblem().cannotUnmountMedium (this, machine, aMedium,
                                                   COMResult (drive));
                success = false;
            }
            break;
        }
        case VBoxDefs::MediaType_Floppy:
        {
            CFloppyDrive drive = machine.GetFloppyDrive();
            drive.Unmount();
            if (!drive.isOk())
            {
                vboxProblem().cannotUnmountMedium (this, machine, aMedium,
                                                   COMResult (drive));
                success = false;
            }
            break;
        }
        default:
            AssertFailedBreakStmt (success = false);
    }

    if (success)
    {
        machine.SaveSettings();
        if (!machine.isOk())
        {
            vboxProblem().cannotSaveMachineSettings (machine);
            success = false;
        }
    }

    /* If a new session was opened, we must close it */
    if (!session.isNull())
        session.Close();

    return success;
}

QTreeWidget* VBoxMediaManagerDlg::treeWidget (VBoxDefs::MediaType aType) const
{
    QTreeWidget* tree = 0;
    switch (aType)
    {
        case VBoxDefs::MediaType_HardDisk:
            tree = mHardDiskView;
            break;
        case VBoxDefs::MediaType_DVD:
            tree = mDVDView;
            break;
        case VBoxDefs::MediaType_Floppy:
            tree = mFloppyView;
            break;
        default:
            AssertMsgFailed (("Disk type %d unknown!\n", aType));
            break;
    }
    return tree;
}

VBoxDefs::MediaType VBoxMediaManagerDlg::currentTreeWidgetType() const
{
    VBoxDefs::MediaType type = VBoxDefs::MediaType_Invalid;
    switch (mTwImages->currentIndex ())
    {
        case HDTab:
            type = VBoxDefs::MediaType_HardDisk;
            break;
        case CDTab:
            type = VBoxDefs::MediaType_DVD;
            break;
        case FDTab:
            type = VBoxDefs::MediaType_Floppy;
            break;
        default:
            AssertMsgFailed (("Page type %d unknown!\n", mTwImages->currentIndex ()));
            break;
    }
    return type;
}

QTreeWidget* VBoxMediaManagerDlg::currentTreeWidget() const
{
    return treeWidget (currentTreeWidgetType());
}

QTreeWidgetItem *VBoxMediaManagerDlg::selectedItem (const QTreeWidget *aTree) const
{
    /* Return the current selected item. The user can select one item at the
     * time only, so return the first element always. */
    QList <QTreeWidgetItem*> selItems = aTree->selectedItems();
    return selItems.isEmpty() ? 0 : selItems.first();
}


MediaItem* VBoxMediaManagerDlg::toMediaItem (QTreeWidgetItem *aItem) const
{
    /* Convert the QTreeWidgetItem to a MediaItem if it is valid. */
    MediaItem *item = 0;
    if (aItem && aItem->type() == QITreeWidget::BasicItemType)
        item = static_cast <MediaItem*> (aItem);
    return item;
}

void VBoxMediaManagerDlg::setCurrentItem (QTreeWidget *aTree,
                                          QTreeWidgetItem *aItem)
{
    if (aTree && aItem)
    {
        aItem->setSelected (true);
        aTree->setCurrentItem (aItem);
        aTree->scrollToItem (aItem, QAbstractItemView::EnsureVisible);
    }
}

void VBoxMediaManagerDlg::processCurrentChanged (int /* index = -1 */)
{
    QTreeWidget *tree = currentTreeWidget();
    tree->setFocus();
    processCurrentChanged (tree->currentItem());
}

void VBoxMediaManagerDlg::processCurrentChanged (QTreeWidgetItem *aItem,
                                                 QTreeWidgetItem *aPrevItem /* = 0 */)
{
    MediaItem *item = toMediaItem (aItem);

    if (!item && aPrevItem)
    {
        MediaItem *itemOld = toMediaItem (aPrevItem);
        /* We have to make sure that one item is selected always. If the new
         * item is 0, set the old item again. */
        setCurrentItem (currentTreeWidget(), itemOld);
    }

    if (item)
    {
        /* Set the file for the proxy icon */
        setFileForProxyIcon (item->location());
        /* Ensures current item visible every time we are switching page */
        item->treeWidget()->scrollToItem (item, QAbstractItemView::EnsureVisible);
    }

    bool notInEnum = !vboxGlobal().isMediaEnumerationStarted();

    /* New and Add are now enabled even when enumerating since it should be safe */
    bool newEnabled     = currentTreeWidgetType() == VBoxDefs::MediaType_HardDisk;
    bool addEnabled     = true;
//  bool editEnabled    = notInEnum && item && checkMediumFor (item, Action_Edit);
    bool removeEnabled  = notInEnum && item && checkMediumFor (item, Action_Remove);
    bool releaseEnabled = item && checkMediumFor (item, Action_Release);

    mNewAction->setEnabled (newEnabled);
    mAddAction->setEnabled (addEnabled);
//  mEditAction->setEnabled (editEnabled);
    mRemoveAction->setEnabled (removeEnabled);
    mReleaseAction->setEnabled (releaseEnabled);

    if (mDoSelect)
    {
        bool selectEnabled = item && checkMediumFor (item, Action_Select);
        mButtonBox->button (QDialogButtonBox::Ok)->setEnabled (selectEnabled);
    }

    if (item)
    {
        QString usage = item->usage().isNull() ?
                        formatPaneText (tr ("<i>Not&nbsp;Attached</i>"), false) :
                        formatPaneText (item->usage());

        if (item->treeWidget() == mHardDiskView)
        {
            mHdsPane1->setText (formatPaneText (item->location(), true, "end"));
            mHdsPane2->setText (formatPaneText (QString ("%1 (%2)").arg (item->hardDiskType())
                                                                   .arg (item->hardDiskFormat()), false));
            mHdsPane3->setText (usage);
        }
        else if (item->treeWidget() == mDVDView)
        {
            mCdsPane1->setText (formatPaneText (item->location(), true, "end"));
            mCdsPane2->setText (usage);
        }
        else if (item->treeWidget() == mFloppyView)
        {
            mFdsPane1->setText (formatPaneText (item->location(), true, "end"));
            mFdsPane2->setText (usage);
        }
    }
    else
        clearInfoPanes();

    mHdsContainer->setEnabled (item);
    mCdsContainer->setEnabled (item);
    mFdsContainer->setEnabled (item);
}

void VBoxMediaManagerDlg::processDoubleClick (QTreeWidgetItem * /* aItem */, int /* aColumn */)
{
    QTreeWidget *tree = currentTreeWidget();

    if (mDoSelect && selectedItem (tree) && mButtonBox->button (QDialogButtonBox::Ok)->isEnabled())
        accept();
}

void VBoxMediaManagerDlg::showContextMenu (const QPoint &aPos)
{
    QTreeWidget *curWidget = currentTreeWidget();
    QTreeWidgetItem *curItem = curWidget->itemAt (aPos);
    if (curItem)
    {
        /* Make sure the item is selected and current */
        setCurrentItem (curWidget, curItem);
        mActionsContextMenu->exec (curWidget->viewport()->mapToGlobal (aPos));
    }
}

void VBoxMediaManagerDlg::machineStateChanged (const VBoxMachineStateChangeEvent &aEvent)
{
    /// @todo (r=dmik) IVirtualBoxCallback::OnMachineStateChange
    //  must also expose the old state! In this case we won't need to cache
    //  the state value in every class in GUI that uses this signal.

    switch (aEvent.state)
    {
        case KMachineState_PoweredOff:
        case KMachineState_Aborted:
        case KMachineState_Saved:
        case KMachineState_Starting:
        case KMachineState_Restoring:
        {
            refreshAll();
            break;
        }
        default:
            break;
    }
}

void VBoxMediaManagerDlg::makeRequestForAdjustTable()
{
    /* We have to perform table adjustment only after all the [auto]resize
     * events processed for every column of this table. */
    QTimer::singleShot (0, this, SLOT (performTablesAdjustment()));
}

void VBoxMediaManagerDlg::performTablesAdjustment()
{
    /* Get all the tree widgets */
    QList <QITreeWidget*> widgetList;
    widgetList << mHardDiskView;
    widgetList << mDVDView;
    widgetList << mFloppyView;

    /* Calculate deduction for every header */
    QList <int> deductionsList;
    foreach (QITreeWidget *widget, widgetList)
    {
        int deduction = 0;
        for (int i = 1; i < widget->header()->count(); ++ i)
            deduction += widget->header()->sectionSize (i);
        deductionsList << deduction;
    }

    /* Adjust the table's first column */
    for (int i = 0; i < widgetList.size(); ++ i)
    {
        int size0 = widgetList [i]->viewport()->width() - deductionsList [i];
        if (widgetList [i]->header()->sectionSize (0) != size0)
            widgetList [i]->header()->resizeSection (0, size0);
    }
}

void VBoxMediaManagerDlg::addMediumToList (const QString &aLocation,
                                           VBoxDefs::MediaType aType)
{
    AssertReturnVoid (!aLocation.isEmpty());

    QString uuid;
    VBoxMedium medium;

    switch (aType)
    {
        case VBoxDefs::MediaType_HardDisk:
        {
            CHardDisk hd = mVBox.OpenHardDisk(aLocation, KAccessMode_ReadWrite);
            if (mVBox.isOk())
            {
                medium = VBoxMedium (CMedium (hd),
                                     VBoxDefs::MediaType_HardDisk,
                                     KMediaState_Created);
            }
            break;
        }
        case VBoxDefs::MediaType_DVD:
        {
            CDVDImage image = mVBox.OpenDVDImage (aLocation, uuid);
            if (mVBox.isOk())
            {
                medium = VBoxMedium (CMedium (image),
                                     VBoxDefs::MediaType_DVD,
                                     KMediaState_Created);
            }
            break;
        }
        case VBoxDefs::MediaType_Floppy:
        {
            CFloppyImage image = mVBox.OpenFloppyImage (aLocation, uuid);
            if (mVBox.isOk())
            {
                medium = VBoxMedium (CMedium (image),
                                     VBoxDefs::MediaType_Floppy,
                                     KMediaState_Created);
            }
            break;
        }
        default:
            AssertMsgFailedReturnVoid (("Invalid aType %d\n", aType));
    }

    if (!mVBox.isOk())
        vboxProblem().cannotOpenMedium (this, mVBox, aType, aLocation);
    else
        vboxGlobal().addMedium (medium);
}

MediaItem* VBoxMediaManagerDlg::createHardDiskItem (QTreeWidget *aTree,
                                                    const VBoxMedium &aMedium) const
{
    AssertReturn (!aMedium.hardDisk().isNull(), 0);

    MediaItem *item = 0;

    CHardDisk parent = aMedium.hardDisk().GetParent();
    if (parent.isNull())
    {
        item = new MediaItem (aTree, aMedium, this);
    }
    else
    {
        MediaItem *root = searchItem (aTree, parent.GetId());
        AssertReturn (root, 0);
        item = new MediaItem (root, aMedium, this);
    }

    return item;
}

void VBoxMediaManagerDlg::updateTabIcons (MediaItem *aItem, ItemAction aAction)
{
    AssertReturnVoid (aItem);

    int tab = -1;
    QIcon *icon = 0;
    bool *inaccessible = 0;

    switch (aItem->type())
    {
        case VBoxDefs::MediaType_HardDisk:
            tab = HDTab;
            icon = &mHardDiskIcon;
            inaccessible = &mHardDisksInaccessible;
            break;
        case VBoxDefs::MediaType_DVD:
            tab = CDTab;
            icon = &mDVDImageIcon;
            inaccessible = &mDVDImagesInaccessible;
            break;
        case VBoxDefs::MediaType_Floppy:
            tab = FDTab;
            icon = &mFloppyImageIcon;
            inaccessible = &mFloppyImagesInaccessible;
            break;
        default:
            AssertFailed();
    }

    AssertReturnVoid (tab != -1 && icon && inaccessible);

    switch (aAction)
    {
        case ItemAction_Added:
        {
            /* Does it change the overall state? */
            if (*inaccessible ||
                aItem->state() != KMediaState_Inaccessible)
                break; /* no */

            *inaccessible = true;

            mTwImages->setTabIcon (tab, vboxGlobal().warningIcon());

            break;
        }
        case ItemAction_Updated:
        case ItemAction_Removed:
        {
            bool checkRest = false;

            if (aAction == ItemAction_Updated)
            {
                /* Does it change the overall state? */
                if ((*inaccessible && aItem->state() == KMediaState_Inaccessible) ||
                    (!*inaccessible && aItem->state() != KMediaState_Inaccessible))
                    break; /* no */

                /* Is the given item in charge? */
                if (!*inaccessible && aItem->state() == KMediaState_Inaccessible)
                    *inaccessible = true; /* yes */
                else
                    checkRest = true; /* no */
            }
            else
                checkRest = true;

            if (checkRest)
            {
                *inaccessible = false;

                QTreeWidget *tree = aItem->treeWidget();

                /* Find the first inaccessible item to be in charge */
                MediaItemIterator it (tree);
                for (; *it; ++ it)
                {
                    if (*it != aItem &&
                        (*it)->state() == KMediaState_Inaccessible)
                    {
                        *inaccessible = true;
                        break;
                    }
                }
            }

            if (*inaccessible)
                mTwImages->setTabIcon (tab, vboxGlobal().warningIcon());
            else
                mTwImages->setTabIcon (tab, *icon);

            break;
        }
    }
}

MediaItem* VBoxMediaManagerDlg::searchItem (QTreeWidget *aTree,
                                            const QString &aId) const
{
    if (aId.isNull())
        return 0;

    MediaItemIterator iterator (aTree);
    while (*iterator)
    {
        if ((*iterator)->id() == aId)
            return *iterator;
        ++ iterator;
    }
    return 0;
}

/**
 * Checks if a specific action is valid for the given medium at its current
 * state.
 *
 * @param aItem     Media item.
 * @param aAction   Action to check.
 *
 * @return @c true if the action is possible and false otherwise.
 */
bool VBoxMediaManagerDlg::checkMediumFor (MediaItem *aItem, Action aAction)
{
    AssertReturn (aItem, false);

    switch (aAction)
    {
        case Action_Select:
        {
            /* In the current implementation, any known media can be attached
             * (either directly, or indirectly), so always return true */
            return true;
        }
        case Action_Edit:
        {
            /* Edit means changing the description and alike; any media that is
             * not being read to or written from can be altered in these
             * terms */
            switch (aItem->state())
            {
                case KMediaState_NotCreated:
                case KMediaState_Inaccessible:
                case KMediaState_LockedRead:
                case KMediaState_LockedWrite:
                    return false;
                default:
                    break;
            }
            return true;
        }
        case Action_Remove:
        {
            /* Removable if not attached to anything */
            return !aItem->isUsed();
        }
        case Action_Release:
        {
            /* Releasable if attached but not in snapshots */
            return aItem->isUsed() && !aItem->isUsedInSnapshots();
        }
    }

    AssertFailedReturn (false);
}

bool VBoxMediaManagerDlg::checkDndUrls (const QList <QUrl> &aUrls) const
{
    bool err = false;
    /* Check that all file extensions fit to the current
     * selected tree view and the files are valid. */
    foreach (QUrl u, aUrls)
    {
        QFileInfo fi (u.toLocalFile());
        /* Check dropped media type */
        /// @todo On OS/2 and windows (and mac?) extension checks should be case
        /// insensitive, as OPPOSED to linux and the rest where case matters.
        QString suffix = fi.suffix().toLower();
        switch (currentTreeWidgetType())
        {
            case VBoxDefs::MediaType_HardDisk:
            {
                QList < QPair <QString, QString> > filterList = vboxGlobal().HDDBackends();
                bool match = false;
                for (int i = 0; i < filterList.count(); ++i)
                {
                    QPair <QString, QString> item = filterList.at (i);
                    if (QString ("*.%1").arg (suffix) == item.second)
                    {
                        match = true;
                        break;
                    }
                }
                err |= !match;
                break;
            }
            case VBoxDefs::MediaType_DVD:
                err |= (suffix != "iso");
                break;
            case VBoxDefs::MediaType_Floppy:
                err |= (suffix != "img");
                break;
            default:
                AssertMsgFailed (("Selected tree should be equal to one item in VBoxDefs::MediaType.\n"));
                break;
        }
    }
    return !err;
}

void VBoxMediaManagerDlg::addDndUrls (const QList <QUrl> &aUrls)
{
    foreach (QUrl u, aUrls)
    {
        QString file = u.toLocalFile();
        VBoxDefs::MediaType type = currentTreeWidgetType();
        addMediumToList (file, type);
    }
}

void VBoxMediaManagerDlg::clearInfoPanes()
{
    mHdsPane1->clear(); mHdsPane2->clear(); mHdsPane3->clear();
    mCdsPane1->clear(); mCdsPane2->clear();
    mFdsPane1->clear(); mFdsPane2->clear();
}

void VBoxMediaManagerDlg::prepareToRefresh (int aTotal)
{
    /* Info panel clearing */
    clearInfoPanes();

    /* Prepare progressbar */
    if (mProgressBar)
    {
        mProgressBar->setMaximum (aTotal);
        mProgressBar->setValue (0);
        mProgressBar->setVisible (true);
    }

    mRefreshAction->setEnabled (false);
    setCursor (QCursor (Qt::BusyCursor));

    /* Store the current list selections */
    MediaItem *mi;

    mi = toMediaItem (mHardDiskView->currentItem());
    if (mHDSelectedId.isNull())
        mHDSelectedId = mi ? mi->id() : QString::null;

    mi = toMediaItem (mDVDView->currentItem());
    if (mDVDSelectedId.isNull())
        mDVDSelectedId = mi ? mi->id() : QString::null;

    mi = toMediaItem (mFloppyView->currentItem());
    if (mFloppySelectedId.isNull())
        mFloppySelectedId = mi ? mi->id() : QString::null;

    /* Finally, clear all the lists...
     * Qt4 has interesting bug here. It sends the currentChanged (cur, prev)
     * signal during list clearing with 'cur' set to null and 'prev' pointing
     * to already excluded element if this element is not topLevelItem
     * (has parent). Cause the Hard Disk list has such elements there is
     * seg-fault when trying to make 'prev' element the current due to 'cur'
     * is null and at least one element have to be selected (by policy).
     * So just blocking any signals outgoing from the list during clearing. */
    mHardDiskView->blockSignals (true);
    mHardDiskView->clear();
    mHardDiskView->blockSignals (false);
    mDVDView->clear();
    mFloppyView->clear();
}

/**
 * Modifies the given text string for QILabel so that it optionally uses the
 * <compact> syntax controlling visual text "compression" with elipsis.
 *
 * @param aText     Original text string.
 * @param aCompact  @c true if "compression" should be activated.
 * @param aElipsis  Where to put the elipsis (see <compact> in QILabel).
 *
 * @return Modified text string.
 */
QString VBoxMediaManagerDlg::formatPaneText (const QString &aText, bool aCompact /* = true */,
                                             const QString &aElipsis /* = "middle" */)
{
    QString compactString = QString ("<compact elipsis=\"%1\">").arg (aElipsis);
    QString info = QString ("<nobr>%1%2%3</nobr>")
        .arg (aCompact ? compactString : "")
        .arg (aText.isEmpty() ?
              QApplication::translate ("VBoxMediaManagerDlg", "--", "no info") :
              aText)
        .arg (aCompact ? "</compact>" : "");
    return info;
}

#include "VBoxMediaManagerDlg.moc"

