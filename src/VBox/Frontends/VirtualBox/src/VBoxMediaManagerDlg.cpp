/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxMediaManagerDlg class implementation
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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
# include "precomp.h"
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */
/* Global includes */
#include <QCloseEvent>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QHeaderView>
#include <QMenuBar>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QUrl>

/* Local includes */
#include "VBoxGlobal.h"
#include "VBoxMediaManagerDlg.h"
#include "UINewHDWizard.h"
#include "VBoxProblemReporter.h"
#include "UIToolBar.h"
#include "QIFileDialog.h"
#include "QILabel.h"
#include "UIIconPool.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIMediumTypeChangeDialog.h"
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

#ifdef Q_WS_MAC
# include "UIWindowMenuManager.h"
#endif /* Q_WS_MAC */

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

    enum { MediaItemType = QTreeWidgetItem::UserType + 1 };

    MediaItem (MediaItem *aParent, const VBoxMedium &aMedium, const VBoxMediaManagerDlg *aManager)
        : QTreeWidgetItem (aParent, MediaItemType)
        , mMedium (aMedium)
        , mManager (aManager)
    { refresh(); }

    MediaItem (QTreeWidget *aParent, const VBoxMedium &aMedium, const VBoxMediaManagerDlg *aManager)
        : QTreeWidgetItem (aParent, MediaItemType)
        , mMedium (aMedium)
        , mManager (aManager)
    { refresh(); }

    void refreshAll()
    {
        mMedium.refresh();
        refresh();
    }

    void setMedium (const VBoxMedium &aMedium)
    {
        mMedium = aMedium;
        refresh();
    }

    const VBoxMedium& medium() const { return mMedium; }

    VBoxDefs::MediumType type() const { return mMedium.type(); }

    KMediumState state() const { return mMedium.state (!mManager->showDiffs()); }

    QString id() const { return mMedium.id(); }

    QString location() const { return mMedium.location (!mManager->showDiffs()); }

    QString hardDiskFormat() const { return mMedium.hardDiskFormat (!mManager->showDiffs()); }
    QString hardDiskType() const { return mMedium.hardDiskType (!mManager->showDiffs()); }

    QString details() const { return mMedium.storageDetails(); }

    QString usage() const { return mMedium.usage (!mManager->showDiffs()); }

    QString toolTip() const { return mMedium.toolTip (!mManager->showDiffs(), mManager->inAttachMode()); }

    bool isUsed() const { return mMedium.isUsed(); }
    bool isUsedInSnapshots() const { return mMedium.isUsedInSnapshots(); }

    bool operator< (const QTreeWidgetItem &aOther) const
    {
        int column = treeWidget()->sortColumn();
        ULONG64 thisValue = vboxGlobal().parseSize (       text (column));
        ULONG64 thatValue = vboxGlobal().parseSize (aOther.text (column));
        return thisValue && thatValue ? thisValue < thatValue : QTreeWidgetItem::operator< (aOther);
    }

private:

    void refresh()
    {
        /* Fill in columns */
        setIcon (0, mMedium.icon (!mManager->showDiffs(), mManager->inAttachMode()));
        /* Set the text */
        setText (0, mMedium.name (!mManager->showDiffs()));
        setText (1, mMedium.logicalSize (!mManager->showDiffs()));
        setText (2, mMedium.size (!mManager->showDiffs()));
        /* All columns get the same tooltip */
        QString tt = mMedium.toolTip (!mManager->showDiffs());
        for (int i = 0; i < treeWidget()->columnCount(); ++ i)
            setToolTip (i, tt);
    }

    VBoxMedium mMedium;
    const VBoxMediaManagerDlg *mManager;
};

class MediaItemIterator : public QTreeWidgetItemIterator
{
public:

    MediaItemIterator (QTreeWidget* aTree)
        : QTreeWidgetItemIterator (aTree) {}

    MediaItem* operator*()
    {
        QTreeWidgetItem *item = QTreeWidgetItemIterator::operator*();
        return item && item->type() == MediaItem::MediaItemType ?
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

    QLabel *mText;
    QProgressBar *mProgressBar;
};


VBoxMediaManagerDlg* VBoxMediaManagerDlg::mModelessDialog = 0;

VBoxMediaManagerDlg::VBoxMediaManagerDlg (QWidget *aParent /* = 0 */, Qt::WindowFlags aFlags /* = Qt::Dialog */)
    : QIWithRetranslateUI2 <QIMainDialog> (aParent, aFlags)
    , mType (VBoxDefs::MediumType_Invalid)
    , mShowDiffs (true)
    , mSetupMode (false)
{
    /* Apply UI decorations */
    Ui::VBoxMediaManagerDlg::setupUi (this);

    /* Apply window icons */
    setWindowIcon(UIIconPool::iconSetFull(QSize (32, 32), QSize (16, 16),
                                          ":/diskimage_32px.png", ":/diskimage_16px.png"));

    mVBox = vboxGlobal().virtualBox();
    Assert (!mVBox.isNull());

    mHardDisksInaccessible =
        mDVDImagesInaccessible =
            mFloppyImagesInaccessible = false;

    mHardDiskIcon    = UIIconPool::iconSet(":/hd_16px.png", ":/hd_disabled_16px.png");
    mDVDImageIcon    = UIIconPool::iconSet(":/cd_16px.png", ":/cd_disabled_16px.png");
    mFloppyImageIcon = UIIconPool::iconSet(":/fd_16px.png", ":/fd_disabled_16px.png");

    /* Setup tab-widget icons */
    mTabWidget->setTabIcon (HDTab, mHardDiskIcon);
    mTabWidget->setTabIcon (CDTab, mDVDImageIcon);
    mTabWidget->setTabIcon (FDTab, mFloppyImageIcon);

    connect (mTabWidget, SIGNAL (currentChanged (int)), this, SLOT (processCurrentChanged (int)));

    /* Setup the tree-widgets */
    mTwHD->sortItems (0, Qt::AscendingOrder);
    mTwHD->header()->setResizeMode (0, QHeaderView::Fixed);
    mTwHD->header()->setResizeMode (1, QHeaderView::ResizeToContents);
    mTwHD->header()->setResizeMode (2, QHeaderView::ResizeToContents);
    mTwHD->header()->setStretchLastSection (false);
    mTwHD->setSortingEnabled (true);
    mTwHD->installEventFilter (this);
    connect (mTwHD, SIGNAL (currentItemChanged (QTreeWidgetItem *, QTreeWidgetItem *)),
             this, SLOT (processCurrentChanged (QTreeWidgetItem *, QTreeWidgetItem *)));
    connect (mTwHD, SIGNAL (itemDoubleClicked (QTreeWidgetItem *, int)),
             this, SLOT (processDoubleClick (QTreeWidgetItem *, int)));
    connect (mTwHD, SIGNAL (customContextMenuRequested (const QPoint &)),
             this, SLOT (showContextMenu (const QPoint &)));
    connect (mTwHD, SIGNAL (resized (const QSize &, const QSize &)),
             this, SLOT (makeRequestForAdjustTable()));
    connect (mTwHD->header(), SIGNAL (sectionResized (int, int, int)),
             this, SLOT (makeRequestForAdjustTable()));

    mTwCD->sortItems (0, Qt::AscendingOrder);
    mTwCD->header()->setResizeMode (0, QHeaderView::Fixed);
    mTwCD->header()->setResizeMode (1, QHeaderView::ResizeToContents);
    mTwCD->header()->setStretchLastSection (false);
    mTwCD->setSortingEnabled (true);
    mTwCD->installEventFilter (this);
    connect (mTwCD, SIGNAL (currentItemChanged (QTreeWidgetItem *, QTreeWidgetItem *)),
             this, SLOT (processCurrentChanged (QTreeWidgetItem *, QTreeWidgetItem *)));
    connect (mTwCD, SIGNAL (itemDoubleClicked (QTreeWidgetItem *, int)),
             this, SLOT (processDoubleClick (QTreeWidgetItem *, int)));
    connect (mTwCD, SIGNAL (customContextMenuRequested (const QPoint &)),
             this, SLOT (showContextMenu (const QPoint &)));
    connect (mTwCD, SIGNAL (resized (const QSize&, const QSize&)),
             this, SLOT (makeRequestForAdjustTable()));
    connect (mTwCD->header(), SIGNAL (sectionResized (int, int, int)),
             this, SLOT (makeRequestForAdjustTable()));

    mTwFD->sortItems (0, Qt::AscendingOrder);
    mTwFD->header()->setResizeMode (0, QHeaderView::Fixed);
    mTwFD->header()->setResizeMode (1, QHeaderView::ResizeToContents);
    mTwFD->header()->setStretchLastSection (false);
    mTwFD->setSortingEnabled (true);
    mTwFD->installEventFilter (this);
    connect (mTwFD, SIGNAL (currentItemChanged (QTreeWidgetItem *, QTreeWidgetItem *)),
             this, SLOT (processCurrentChanged (QTreeWidgetItem *, QTreeWidgetItem *)));
    connect (mTwFD, SIGNAL (itemDoubleClicked (QTreeWidgetItem *, int)),
             this, SLOT (processDoubleClick (QTreeWidgetItem *, int)));
    connect (mTwFD, SIGNAL (customContextMenuRequested (const QPoint &)),
             this, SLOT (showContextMenu (const QPoint &)));
    connect (mTwFD, SIGNAL (resized (const QSize&, const QSize&)),
             this, SLOT (makeRequestForAdjustTable()));
    connect (mTwFD->header(), SIGNAL (sectionResized (int, int, int)),
             this, SLOT (makeRequestForAdjustTable()));

    /* Setup information pane: */
    m_pChangeMediumTypeButton->setIcon(UIIconPool::iconSet(":/arrow_down_10px.png"));
    connect(m_pChangeMediumTypeButton, SIGNAL(clicked()), this, SLOT(sltOpenMediumTypeChangeDialog()));
    m_pChangeMediumTypeButton->setIconSize(QSize(10, 10));

    /* Context menu composing */
    mActionsContextMenu = new QMenu (this);

    mNewAction     = new QAction (this);
    mAddAction     = new QAction (this);
    mCopyAction    = new QAction (this);
    mRemoveAction  = new QAction (this);
    mReleaseAction = new QAction (this);
    mRefreshAction = new QAction (this);

    connect (mNewAction, SIGNAL (triggered()), this, SLOT (doNewMedium()));
    connect (mAddAction, SIGNAL (triggered()), this, SLOT (doAddMedium()));
    connect (mCopyAction, SIGNAL (triggered()), this, SLOT (doCopyMedium()));
    connect (mRemoveAction, SIGNAL (triggered()), this, SLOT (doRemoveMedium()));
    connect (mReleaseAction, SIGNAL (triggered()), this, SLOT (doReleaseMedium()));
    connect (mRefreshAction, SIGNAL (triggered()), this, SLOT (refreshAll()));

    mNewAction->setIcon(UIIconPool::iconSetFull (
        QSize (22, 22), QSize (16, 16),
        ":/hd_new_22px.png", ":/hd_new_16px.png",
        ":/hd_new_disabled_22px.png", ":/hd_new_disabled_16px.png"));
    mAddAction->setIcon(UIIconPool::iconSetFull (
        QSize (22, 22), QSize (16, 16),
        ":/hd_add_22px.png", ":/hd_add_16px.png",
        ":/hd_add_disabled_22px.png", ":/hd_add_disabled_16px.png"));
    mCopyAction->setIcon(UIIconPool::iconSetFull (
        QSize (22, 22), QSize (16, 16),
        ":/hd_new_22px.png", ":/hd_new_16px.png",
        ":/hd_new_disabled_22px.png", ":/hd_new_disabled_16px.png"));
    mRemoveAction->setIcon(UIIconPool::iconSetFull (
        QSize (22, 22), QSize (16, 16),
        ":/hd_remove_22px.png", ":/hd_remove_16px.png",
        ":/hd_remove_disabled_22px.png", ":/hd_remove_disabled_16px.png"));
    mReleaseAction->setIcon(UIIconPool::iconSetFull (
        QSize (22, 22), QSize (16, 16),
        ":/hd_release_22px.png", ":/hd_release_16px.png",
        ":/hd_release_disabled_22px.png", ":/hd_release_disabled_16px.png"));
    mRefreshAction->setIcon(UIIconPool::iconSetFull (
        QSize (22, 22), QSize (16, 16),
        ":/refresh_22px.png", ":/refresh_16px.png",
        ":/refresh_disabled_22px.png", ":/refresh_disabled_16px.png"));

    mActionsContextMenu->addAction (mCopyAction);
    mActionsContextMenu->addAction (mRemoveAction);
    mActionsContextMenu->addAction (mReleaseAction);

    /* Toolbar composing */
    mToolBar = new UIToolBar (this);
    mToolBar->setIconSize (QSize (22, 22));
    mToolBar->setToolButtonStyle (Qt::ToolButtonTextUnderIcon);
    mToolBar->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Preferred);

    QVBoxLayout *mainLayout = qobject_cast <QVBoxLayout*> (centralWidget()->layout());
    Assert (mainLayout);
#if MAC_LEOPARD_STYLE
    /* Enable unified toolbars on Mac OS X. Available on Qt >= 4.3 */
    addToolBar (mToolBar);
    mToolBar->setMacToolbar();
    /* No spacing/margin on the mac */
    VBoxGlobal::setLayoutMargin (mainLayout, 0);
    mainLayout->insertSpacing (0, 10);
#else /* MAC_LEOPARD_STYLE */
    /* Add the toolbar */
    mainLayout->insertWidget (0, mToolBar);
    /* Set spacing/margin like in the selector window */
    mainLayout->setSpacing (5);
    VBoxGlobal::setLayoutMargin (mainLayout, 5);
#endif /* MAC_LEOPARD_STYLE */

//    mToolBar->addAction (mNewAction);
//    mToolBar->addAction (mAddAction);
    mToolBar->addAction (mCopyAction);
//    mToolBar->addSeparator();
    mToolBar->addAction (mRemoveAction);
    mToolBar->addAction (mReleaseAction);
//    mToolBar->addSeparator();
    mToolBar->addAction (mRefreshAction);

    /* Menu bar */
    mActionsMenu = menuBar()->addMenu (QString::null);
//    mActionsMenu->addAction (mNewAction);
//    mActionsMenu->addAction (mAddAction);
    mActionsMenu->addAction (mCopyAction);
//    mActionsMenu->addSeparator();
    mActionsMenu->addAction (mRemoveAction);
    mActionsMenu->addAction (mReleaseAction);
//    mActionsMenu->addSeparator();
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
    connect (mButtonBox, SIGNAL (accepted()), this, SLOT (accept()));
    connect (mButtonBox, SIGNAL (rejected()), this, SLOT (reject()));
    connect (mButtonBox, SIGNAL (helpRequested()), &vboxProblem(), SLOT (showHelpHelpDialog()));
}

VBoxMediaManagerDlg::~VBoxMediaManagerDlg()
{
#ifdef Q_WS_MAC
    if (!mDoSelect)
    {
        UIWindowMenuManager::instance()->removeWindow(this);
        UIWindowMenuManager::instance()->destroyMenu(this);
    }
#endif /* Q_WS_MAC */
    delete mToolBar;
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
 *                          aType is VBoxDefs::MediumType_All)
 * @param aShowDiffs        @c true to show differencing hard disks initially
 *                          (ignored if @a aSessionMachine is null assuming
 *                          @c true).
 * @param aUsedMediaIds     List containing IDs of mediums used in other
 *                          attachments to restrict selection.
 */
void VBoxMediaManagerDlg::setup (VBoxDefs::MediumType aType, bool aDoSelect,
                                 bool aRefresh /* = true */,
                                 const CMachine &aSessionMachine /* = CMachine() */,
                                 const QString &aSelectId /* = QString() */,
                                 bool aShowDiffs /* = true */,
                                 const QStringList &aUsedMediaIds /* = QStringList() */)
{
    mSetupMode = true;

    mType = aType;

    mDoSelect = aDoSelect;

    mSessionMachine = aSessionMachine;
    mSessionMachineId = mSessionMachine.isNull() ? QString::null : mSessionMachine.GetId();
    mShowDiffs = mSessionMachine.isNull() ? true : aShowDiffs;

    switch (aType)
    {
        case VBoxDefs::MediumType_HardDisk: mHDSelectedId = aSelectId; break;
        case VBoxDefs::MediumType_DVD:      mCDSelectedId = aSelectId; break;
        case VBoxDefs::MediumType_Floppy:   mFDSelectedId = aSelectId; break;
        case VBoxDefs::MediumType_All: break;
        default:
            AssertFailedReturnVoid();
    }

    mTabWidget->setTabEnabled (HDTab,
                               aType == VBoxDefs::MediumType_All ||
                               aType == VBoxDefs::MediumType_HardDisk);
    mTabWidget->setTabEnabled (CDTab,
                               aType == VBoxDefs::MediumType_All ||
                               aType == VBoxDefs::MediumType_DVD);
    mTabWidget->setTabEnabled (FDTab,
                               aType == VBoxDefs::MediumType_All ||
                               aType == VBoxDefs::MediumType_Floppy);

    mDoSelect = aDoSelect;
    mUsedMediaIds = aUsedMediaIds;

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
    connect (&vboxGlobal(), SIGNAL (mediumRemoved (VBoxDefs::MediumType, const QString &)),
             this, SLOT (mediumRemoved (VBoxDefs::MediumType, const QString &)));

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
            if ((*it).state() != KMediumState_NotCreated)
                mProgressBar->setValue (++ index);
        }

        /* Emulate the finished signal to reuse the code */
        if (!vboxGlobal().isMediaEnumerationStarted())
            mediumEnumFinished (list);
    }

    /* For a newly opened dialog, select the first item */
    if (mTwHD->selectedItems().isEmpty())
        if (QTreeWidgetItem *item = mTwHD->topLevelItem (0))
            setCurrentItem (mTwHD, item);
    if (mTwCD->selectedItems().isEmpty())
        if (QTreeWidgetItem *item = mTwCD->topLevelItem (0))
            setCurrentItem (mTwCD, item);
    if (mTwFD->selectedItems().isEmpty())
        if (QTreeWidgetItem *item = mTwFD->topLevelItem (0))
            setCurrentItem (mTwFD, item);

    /* Applying language settings */
    retranslateUi();

#ifdef Q_WS_MAC
    if (!mDoSelect)
    {
        menuBar()->addMenu(UIWindowMenuManager::instance()->createMenu(this));
        UIWindowMenuManager::instance()->addWindow(this);
    }
#endif /* Q_WS_MAC */

    mSetupMode = false;
}

/* static */
void VBoxMediaManagerDlg::showModeless (QWidget *aCenterWidget /* = 0 */, bool aRefresh /* = true */)
{
    if (!mModelessDialog)
    {
        mModelessDialog = new VBoxMediaManagerDlg (0, Qt::Window);
        mModelessDialog->centerAccording (aCenterWidget);
        mModelessDialog->setAttribute (Qt::WA_DeleteOnClose);
        mModelessDialog->setup (VBoxDefs::MediumType_All, false /* aDoSelect */, aRefresh);

        /* Setup 'closing' connection if main window is VBoxSelectorWnd: */
        if (vboxGlobal().mainWindow() && vboxGlobal().mainWindow()->inherits("VBoxSelectorWnd"))
            connect(vboxGlobal().mainWindow(), SIGNAL(closing()), mModelessDialog, SLOT(close()));

        /* listen to events that may change the media status and refresh
         * the contents of the modeless dialog */
        /// @todo refreshAll() may be slow, so it may be better to analyze
        //  event details and update only what is changed */
        connect (gVBoxEvents, SIGNAL(sigMachineDataChange(QString)),
                 mModelessDialog, SLOT(refreshAll()));
        connect (gVBoxEvents, SIGNAL(sigMachineRegistered(QString, bool)),
                 mModelessDialog, SLOT(refreshAll()));
        connect (gVBoxEvents, SIGNAL(sigSnapshotChange(QString, QString)),
                 mModelessDialog, SLOT(refreshAll()));
    }

    mModelessDialog->show();
    mModelessDialog->setWindowState (mModelessDialog->windowState() & ~Qt::WindowMinimized);
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
    mCopyAction->setText (tr ("&Copy..."));
    mRemoveAction->setText (tr ("R&emove"));
    mReleaseAction->setText (tr ("Re&lease"));
    mRefreshAction->setText (tr ("Re&fresh"));

    mNewAction->setShortcut (QKeySequence (QKeySequence::New));
    mAddAction->setShortcut (QKeySequence ("Ins"));
    mCopyAction->setShortcut (QKeySequence ("Ctrl+C"));
    mRemoveAction->setShortcut (QKeySequence (QKeySequence::Delete));
    mReleaseAction->setShortcut (QKeySequence ("Ctrl+L"));
    mRefreshAction->setShortcut (QKeySequence (QKeySequence::Refresh));

    mNewAction->setStatusTip (tr ("Create a new virtual hard disk"));
    mAddAction->setStatusTip (tr ("Add an existing medium"));
    mCopyAction->setStatusTip (tr ("Copy an existing medium"));
    mRemoveAction->setStatusTip (tr ("Remove the selected medium"));
    mReleaseAction->setStatusTip (tr ("Release the selected medium by detaching it from the machines"));
    mRefreshAction->setStatusTip (tr ("Refresh the media list"));

    mNewAction->setToolTip (mNewAction->text().remove ('&') +
        QString (" (%1)").arg (mNewAction->shortcut().toString()));
    mAddAction->setToolTip (mAddAction->text().remove ('&') +
        QString (" (%1)").arg (mAddAction->shortcut().toString()));
    mCopyAction->setToolTip (mCopyAction->text().remove ('&') +
        QString (" (%1)").arg (mCopyAction->shortcut().toString()));
    mRemoveAction->setToolTip (mRemoveAction->text().remove ('&') +
        QString (" (%1)").arg (mRemoveAction->shortcut().toString()));
    mReleaseAction->setToolTip (mReleaseAction->text().remove ('&') +
        QString (" (%1)").arg (mReleaseAction->shortcut().toString()));
    mRefreshAction->setToolTip (mRefreshAction->text().remove ('&') +
        QString (" (%1)").arg (mRefreshAction->shortcut().toString()));

    mProgressBar->setText (tr ("Checking accessibility"));
#ifdef Q_WS_MAC
    /* Make sure that the widgets aren't jumping around while the progress bar get visible. */
    mProgressBar->adjustSize();
    int h = mProgressBar->height();
    mButtonBox->setMinimumHeight (h + 12);
#endif
#ifdef QT_MAC_USE_COCOA
    /* There is a bug in Qt Cocoa which result in showing a "more arrow" when
       the necessary size of the toolbar is increased. Also for some languages
       the with doesn't match if the text increase. So manually adjust the size
       after changing the text. */
    mToolBar->updateLayout();
#endif /* QT_MAC_USE_COCOA */

    mButtonBox->button(QDialogButtonBox::Ok)->setText(mDoSelect ? tr("&Select") : tr("C&lose"));

    if (mTwHD->model()->rowCount() || mTwCD->model()->rowCount() || mTwFD->model()->rowCount())
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
    if (!(aObject == mTwHD || aObject == mTwCD || aObject == mTwFD))
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
    if ((aMedium.isNull()) ||
        (mType != VBoxDefs::MediumType_All && mType != aMedium.type()) ||
        (aMedium.isHostDrive()))
        return;

    if (!mShowDiffs && aMedium.type() == VBoxDefs::MediumType_HardDisk)
    {
        if (aMedium.parent() && !mSessionMachineId.isNull())
        {
            /* In !mShowDiffs mode, we ignore all diffs except ones that are
             * directly attached to the related VM in the current state */
            if (!aMedium.isAttachedInCurStateTo (mSessionMachineId))
                return;

            /* Since the base hard disk of this diff has been already appended,
             * we want to replace it with this diff to avoid duplicates in
             * !mShowDiffs mode. */
            MediaItem *item = searchItem (mTwHD, aMedium.root().id());
            AssertReturnVoid (item);

            item->setMedium (aMedium);

            /* Check if swapped diff disk is required one */
            if (item->id() == mHDSelectedId)
            {
                setCurrentItem (mTwHD, item);
                mHDSelectedId = QString::null;
            }

            updateTabIcons (item, ItemAction_Updated);
            return;
        }
    }

    MediaItem *item = 0;

    switch (aMedium.type())
    {
        case VBoxDefs::MediumType_HardDisk:
        {
            item = createHardDiskItem (mTwHD, aMedium);
            AssertReturnVoid (item);

            /* Damn Qt4 didn't notifies the table's QHeaderView on adding
             * new tree-widget items, so initialize the header adjustment
             * by calling resizeSections() slot... */
            QTimer::singleShot (0, mTwHD->header(), SLOT (resizeSections()));

            if (item->id() == mHDSelectedId)
            {
                setCurrentItem (mTwHD, item);
                mHDSelectedId = QString::null;
            }
            break;
        }
        case VBoxDefs::MediumType_DVD:
        {
            item = new MediaItem (mTwCD, aMedium, this);
            AssertReturnVoid (item);

            /* Damn Qt4 didn't notifies the table's QHeaderView on adding
             * new tree-widget items, so initialize the header adjustment
             * by calling resizeSections() slot... */
            QTimer::singleShot (0, mTwCD->header(), SLOT (resizeSections()));

            if (item->id() == mCDSelectedId)
            {
                setCurrentItem (mTwCD, item);
                mCDSelectedId = QString::null;
            }
            break;
        }
        case VBoxDefs::MediumType_Floppy:
        {
            item = new MediaItem (mTwFD, aMedium, this);
            AssertReturnVoid (item);

            /* Damn Qt4 didn't notifies the table's QHeaderView on adding
             * new tree-widget items, so initialize the header adjustment
             * by calling resizeSections() slot... */
            QTimer::singleShot (0, mTwFD->header(), SLOT (resizeSections()));

            if (item->id() == mFDSelectedId)
            {
                setCurrentItem (mTwFD, item);
                mFDSelectedId = QString::null;
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
    if ((aMedium.isNull()) ||
        (mType != VBoxDefs::MediumType_All && mType != aMedium.type()) ||
        (aMedium.isHostDrive()))
        return;

    MediaItem *item = 0;

    switch (aMedium.type())
    {
        case VBoxDefs::MediumType_HardDisk:
        {
            item = searchItem (mTwHD, aMedium.id());
            break;
        }
        case VBoxDefs::MediumType_DVD:
        {
            item = searchItem (mTwCD, aMedium.id());
            break;
        }
        case VBoxDefs::MediumType_Floppy:
        {
            item = searchItem (mTwFD, aMedium.id());
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

void VBoxMediaManagerDlg::mediumRemoved (VBoxDefs::MediumType aType, const QString &aId)
{
    /* Ignore non-interesting aMedium */
    if (mType != VBoxDefs::MediumType_All && mType != aType)
        return;

    QTreeWidget *tree = currentTreeWidget();
    MediaItem *item = toMediaItem (searchItem (tree, aId));

    /* There may be unwanted items due to !mShowDiffs */
    if (!item)
        return;

    updateTabIcons (item, ItemAction_Removed);

    /* We need to silently delete item without selecting
     * the new one because of complex selection mechanism
     * which could provoke a segfault choosing the new
     * one item during last item deletion routine. So blocking
     * the tree-view for the time of item removing. */
    tree->blockSignals (true);
    delete item;
    tree->blockSignals (false);

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
    mTabWidget->setTabIcon (HDTab, mHardDiskIcon);
    mTabWidget->setTabIcon (CDTab, mDVDImageIcon);
    mTabWidget->setTabIcon (FDTab, mFloppyImageIcon);

    /* Load current media list */
    const VBoxMediaList &list = vboxGlobal().currentMediaList();
    prepareToRefresh (list.size());
    VBoxMediaList::const_iterator it;
    for (it = list.begin(); it != list.end(); ++ it)
        mediumAdded (*it);

    /* Select the first item to be the current one if the previous saved item
     * was not selected yet. */
    if (!mTwHD->currentItem())
        if (QTreeWidgetItem *item = mTwHD->topLevelItem (0))
            setCurrentItem (mTwHD, item);
    if (!mTwCD->currentItem())
        if (QTreeWidgetItem *item = mTwCD->topLevelItem (0))
            setCurrentItem (mTwCD, item);
    if (!mTwFD->currentItem())
        if (QTreeWidgetItem *item = mTwFD->topLevelItem (0))
            setCurrentItem (mTwFD, item);

    processCurrentChanged();
}

void VBoxMediaManagerDlg::mediumEnumerated (const VBoxMedium &aMedium)
{
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
    AssertReturnVoid (currentTreeWidgetType() == VBoxDefs::MediumType_HardDisk);

    UINewHDWizard dlg (this);

    if (dlg.exec() == QDialog::Accepted)
    {
        CMedium hd = dlg.hardDisk();
        /* Select the newly created hard disk */
        MediaItem *item = searchItem (mTwHD, hd.GetId());
        AssertReturnVoid (item);
        mTwHD->setCurrentItem (item);
    }
}

void VBoxMediaManagerDlg::doAddMedium()
{
    QTreeWidget *tree = currentTreeWidget();
    MediaItem *item = toMediaItem (tree->currentItem());

    QString title;
    QString filter;
    VBoxDefs::MediumType type = currentTreeWidgetType();

    QString dir;
    if (item && item->state() != KMediumState_Inaccessible
             && item->state() != KMediumState_NotCreated)
        dir = QFileInfo (item->location().trimmed()).absolutePath();

    if (dir.isEmpty() || !QFileInfo (dir).exists())
        dir = mVBox.GetHomeFolder();

    QList < QPair <QString, QString> > filterList;
    QStringList backends;
    QStringList allPrefix;
    QString     allType;

    switch (type)
    {
        case VBoxDefs::MediumType_DVD:
        {
            filterList = vboxGlobal().DVDBackends();
            title = tr ("Select a CD/DVD-ROM disk image file");
            allType = tr ("CD/DVD-ROM disk");
            break;
        }
        case VBoxDefs::MediumType_HardDisk:
        {
            filterList = vboxGlobal().HDDBackends();
            title = tr ("Select a hard disk image file");
            allType = tr ("hard disk");
            break;
        }
        case VBoxDefs::MediumType_Floppy:
        {
            filterList = vboxGlobal().FloppyBackends();
            title = tr ("Select a floppy disk image file");
            allType = tr ("floppy disk");
            break;
        }
        default:
            AssertMsgFailed (("Selected tree should be equal to one item in VBoxDefs::MediumType.\n"));
            break;
    }

    for (int i = 0; i < filterList.count(); ++i)
    {
        QPair <QString, QString> item = filterList.at (i);
        /* Create one backend filter string */
        backends << QString ("%1 (%2)").arg (item.first). arg (item.second);
        /* Save the suffix's for the "All" entry */
        allPrefix << item.second;
    }
    if (!allPrefix.isEmpty())
        backends.insert (0, tr ("All %1 images (%2)").arg (allType). arg (allPrefix.join (" ").trimmed()));
    backends << tr ("All files (*)");
    filter = backends.join (";;").trimmed();

    QStringList files = QIFileDialog::getOpenFileNames (dir, filter, this, title);
    foreach (QString loc, files)
    {
        loc =  QDir::convertSeparators (loc);

        if (!loc.isEmpty())
            addMediumToList (loc, type);
    }
}

void VBoxMediaManagerDlg::doCopyMedium()
{
    /* Get current tree: */
    QTreeWidget *pTree = currentTreeWidget();
    /* Get current item of current tree: */
    MediaItem *pItem = toMediaItem(pTree->currentItem());

    UINewHDWizard wizard(this /* parent dialog */,
                         UINewHDWizard::tr("%1_copy", "copied virtual disk name").arg(QFileInfo(pItem->text(0)).baseName()) /* default name */,
                         QFileInfo(pItem->location()).absolutePath() /* default path */,
                         0 /* default size, not important for copying */,
                         pItem->medium().medium() /* base medium for copying */);
    wizard.exec();
}

void VBoxMediaManagerDlg::doRemoveMedium()
{
    QTreeWidget *tree = currentTreeWidget();
    MediaItem *item = toMediaItem (tree->currentItem());
    AssertMsgReturnVoid (item, ("Current item must not be null"));

    /* Remember ID/type as they may get lost after the closure/deletion */
    QString id = item->id();
    AssertReturnVoid (!id.isNull());
    VBoxDefs::MediumType type = item->type();

    if (!vboxProblem().confirmRemoveMedium (this, item->medium()))
        return;

    COMResult result;

    switch (type)
    {
        case VBoxDefs::MediumType_HardDisk:
        {
            bool deleteStorage = false;

            /* We don't want to try to delete inaccessible storage as it will
             * most likely fail. Note that
             * VBoxProblemReporter::confirmRemoveMedium() is aware of that and
             * will give a corresponding hint. Therefore, once the code is
             * changed below, the hint should be re-checked for validity. */
            if (item->state() != KMediumState_Inaccessible &&
                item->medium().medium().GetMediumFormat().GetCapabilities() & MediumFormatCapabilities_File)
            {
                int rc = vboxProblem().
                    confirmDeleteHardDiskStorage (this, item->location());
                if (rc == QIMessageBox::Cancel)
                    return;
                deleteStorage = rc == QIMessageBox::Yes;
            }

            CMedium hardDisk = item->medium().medium();

            if (deleteStorage)
            {
                CProgress progress = hardDisk.DeleteStorage();
                if (hardDisk.isOk())
                {
                    vboxProblem().showModalProgressDialog(progress, windowTitle(), ":/progress_media_delete_90px.png", this, true);
                    if (!(progress.isOk() && progress.GetResultCode() == S_OK))
                    {
                        vboxProblem().cannotDeleteHardDiskStorage(this, hardDisk, progress);
                        return;
                    }
                }
            }

            hardDisk.Close();
            result = hardDisk;
            break;
        }
        case VBoxDefs::MediumType_DVD:
        {
            CMedium image = item->medium().medium();
            image.Close();
            result = image;
            break;
        }
        case VBoxDefs::MediumType_Floppy:
        {
            CMedium image = item->medium().medium();
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
    for (QList <QString>::const_iterator it = machineIds.begin(); it != machineIds.end(); ++ it)
    {
        CMachine m = mVBox.FindMachine (*it);
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

    for (QList <QString>::const_iterator it = machineIds.begin(); it != machineIds.end(); ++ it)
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

bool VBoxMediaManagerDlg::releaseMediumFrom (const VBoxMedium &aMedium, const QString &aMachineId)
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
        session = vboxGlobal().openSession(aMachineId);
        if (session.isNull())
            return false;

        machine = session.GetMachine();
    }

    bool success = true;

    switch (aMedium.type())
    {
        case VBoxDefs::MediumType_HardDisk:
        {
            CMediumAttachmentVector attachments = machine.GetMediumAttachments();
            foreach (const CMediumAttachment &attachment, attachments)
            {
                if (attachment.GetType() != KDeviceType_HardDisk) continue;

                if (attachment.GetMedium().GetId() == aMedium.id())
                {
                    machine.DetachDevice (attachment.GetController(), attachment.GetPort(), attachment.GetDevice());
                    if (!machine.isOk())
                    {
                        CStorageController controller = machine.GetStorageControllerByName (attachment.GetController());
                        vboxProblem().cannotDetachDevice (this, machine, VBoxDefs::MediumType_HardDisk, aMedium.location(),
                                                          StorageSlot(controller.GetBus(), attachment.GetPort(), attachment.GetDevice()));
                        success = false;
                        break;
                    }
                }
            }
            break;
        }
        case VBoxDefs::MediumType_DVD:
        {
            CMediumAttachmentVector attachments = machine.GetMediumAttachments();
            foreach (const CMediumAttachment &attachment, attachments)
            {
                if (attachment.GetType() != KDeviceType_DVD) continue;

                VBoxMedium medium = vboxGlobal().findMedium (attachment.GetMedium().isNull() ? QString() : attachment.GetMedium().GetId());
                if (medium.id() == aMedium.id())
                {
                    machine.MountMedium (attachment.GetController(), attachment.GetPort(), attachment.GetDevice(), CMedium(), false /* force */);
                    if (!machine.isOk())
                    {
                        vboxProblem().cannotRemountMedium (this, machine, aMedium, false /* mount? */, false /* retry? */);
                        success = false;
                        break;
                    }
                }
            }
            break;
        }
        case VBoxDefs::MediumType_Floppy:
        {
            CMediumAttachmentVector attachments = machine.GetMediumAttachments();
            foreach (const CMediumAttachment &attachment, attachments)
            {
                if (attachment.GetType() != KDeviceType_Floppy) continue;

                VBoxMedium medium = vboxGlobal().findMedium (attachment.GetMedium().isNull() ? QString() : attachment.GetMedium().GetId());
                if (medium.id() == aMedium.id())
                {
                    machine.MountMedium (attachment.GetController(), attachment.GetPort(), attachment.GetDevice(), CMedium(), false /* force */);
                    if (!machine.isOk())
                    {
                        vboxProblem().cannotRemountMedium (this, machine, aMedium, false /* mount? */, false /* retry? */);
                        success = false;
                        break;
                    }
                }
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
        session.UnlockMachine();

    return success;
}

QTreeWidget* VBoxMediaManagerDlg::treeWidget (VBoxDefs::MediumType aType) const
{
    QTreeWidget* tree = 0;
    switch (aType)
    {
        case VBoxDefs::MediumType_HardDisk:
            tree = mTwHD;
            break;
        case VBoxDefs::MediumType_DVD:
            tree = mTwCD;
            break;
        case VBoxDefs::MediumType_Floppy:
            tree = mTwFD;
            break;
        default:
            AssertMsgFailed (("Disk type %d unknown!\n", aType));
            break;
    }
    return tree;
}

VBoxDefs::MediumType VBoxMediaManagerDlg::currentTreeWidgetType() const
{
    VBoxDefs::MediumType type = VBoxDefs::MediumType_Invalid;
    switch (mTabWidget->currentIndex())
    {
        case HDTab:
            type = VBoxDefs::MediumType_HardDisk;
            break;
        case CDTab:
            type = VBoxDefs::MediumType_DVD;
            break;
        case FDTab:
            type = VBoxDefs::MediumType_Floppy;
            break;
        default:
            AssertMsgFailed (("Page type %d unknown!\n", mTabWidget->currentIndex()));
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
    if (aItem && aItem->type() == MediaItem::MediaItemType)
        item = static_cast <MediaItem*> (aItem);
    return item;
}

void VBoxMediaManagerDlg::setCurrentItem (QTreeWidget *aTree, QTreeWidgetItem *aItem)
{
    if (aTree && aItem)
    {
        aItem->setSelected (true);
        aTree->setCurrentItem (aItem);
        aTree->scrollToItem (aItem, QAbstractItemView::EnsureVisible);
        processCurrentChanged (aItem);
    }
    else processCurrentChanged();
}

void VBoxMediaManagerDlg::processCurrentChanged (int /* aIndex = -1 */)
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
    bool newEnabled     = currentTreeWidgetType() == VBoxDefs::MediumType_HardDisk;
    bool addEnabled     = true;
    bool copyEnabled    = notInEnum && item && checkMediumFor (item, Action_Copy);
    bool removeEnabled  = notInEnum && item && checkMediumFor (item, Action_Remove);
    bool releaseEnabled = item && checkMediumFor (item, Action_Release);

    mNewAction->setEnabled (newEnabled);
    mAddAction->setEnabled (addEnabled);
    mCopyAction->setEnabled (copyEnabled);
    mRemoveAction->setEnabled (removeEnabled);
    mReleaseAction->setEnabled (releaseEnabled);

    if (mDoSelect)
    {
        bool selectEnabled = item && checkMediumFor (item, Action_Select);
        mButtonBox->button (QDialogButtonBox::Ok)->setEnabled (selectEnabled);
    }

    if (item)
    {
        QString details = item->details();
        QString usage = item->usage().isNull() ?
                        formatPaneText (tr ("<i>Not&nbsp;Attached</i>"), false) :
                        formatPaneText (item->usage());

        if (item->treeWidget() == mTwHD)
        {
            /* Check if thats parent medium: */
            bool fIsThatParentMedium = !item->medium().parent();
            m_pChangeMediumTypeButton->setEnabled(fIsThatParentMedium);

            /* Other panes: */
            m_pTypePane->setText(item->hardDiskType());
            m_pLocationPane->setText(formatPaneText(item->location(), true, "end"));
            m_pFormatPane->setText(item->hardDiskFormat());
            m_pDetailsPane->setText(details);
            m_pUsagePane->setText(usage);
        }
        else if (item->treeWidget() == mTwCD)
        {
            mIpCD1->setText (formatPaneText (item->location(), true, "end"));
            mIpCD2->setText (usage);
        }
        else if (item->treeWidget() == mTwFD)
        {
            mIpFD1->setText (formatPaneText (item->location(), true, "end"));
            mIpFD2->setText (usage);
        }
    }
    else
        clearInfoPanes();

    mHDContainer->setEnabled (item);
    mCDContainer->setEnabled (item);
    mFDContainer->setEnabled (item);
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

void VBoxMediaManagerDlg::machineStateChanged(QString /* strId */, KMachineState state)
{
    switch (state)
    {
        case KMachineState_PoweredOff:
        case KMachineState_Aborted:
        case KMachineState_Saved:
        case KMachineState_Teleported:
        case KMachineState_Starting:
        case KMachineState_Restoring:
        case KMachineState_TeleportingIn:
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
    widgetList << mTwHD;
    widgetList << mTwCD;
    widgetList << mTwFD;

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

void VBoxMediaManagerDlg::sltOpenMediumTypeChangeDialog()
{
    MediaItem *pMediumItem = toMediaItem(currentTreeWidget()->currentItem());
    UIMediumTypeChangeDialog dlg(this, pMediumItem->id());
    if (dlg.exec() == QDialog::Accepted)
    {
        pMediumItem->refreshAll();
        m_pTypePane->setText(pMediumItem->hardDiskType());
    }
}

void VBoxMediaManagerDlg::addMediumToList(const QString &aLocation, VBoxDefs::MediumType aType)
{
    AssertReturnVoid (!aLocation.isEmpty());

    VBoxMedium medium;
    KDeviceType devType;

    switch (aType)
    {
        case VBoxDefs::MediumType_HardDisk:
            devType = KDeviceType_HardDisk;
        break;
        case VBoxDefs::MediumType_DVD:
            devType = KDeviceType_DVD;
        break;
        case VBoxDefs::MediumType_Floppy:
            devType = KDeviceType_Floppy;
        break;
        default:
            AssertMsgFailedReturnVoid (("Invalid aType %d\n", aType));
    }

    CMedium med = mVBox.OpenMedium(aLocation, devType, KAccessMode_ReadWrite, false /* fForceNewUuid */);
    if (mVBox.isOk())
        medium = VBoxMedium(CMedium(med), aType, KMediumState_Created);

    if (!mVBox.isOk())
        vboxProblem().cannotOpenMedium(this, mVBox, aType, aLocation);
    else
        vboxGlobal().addMedium(medium);
}

MediaItem* VBoxMediaManagerDlg::createHardDiskItem (QTreeWidget *aTree, const VBoxMedium &aMedium) const
{
    AssertReturn (!aMedium.medium().isNull(), 0);

    MediaItem *item = 0;

    CMedium parent = aMedium.medium().GetParent();
    if (parent.isNull())
    {
        item = new MediaItem(aTree, aMedium, this);
    }
    else
    {
        MediaItem *root = searchItem (aTree, parent.GetId());
        if (root)
            item = new MediaItem(root, aMedium, this);
        else
            item = new MediaItem(aTree, aMedium, this);
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
        case VBoxDefs::MediumType_HardDisk:
            tab = HDTab;
            icon = &mHardDiskIcon;
            inaccessible = &mHardDisksInaccessible;
            break;
        case VBoxDefs::MediumType_DVD:
            tab = CDTab;
            icon = &mDVDImageIcon;
            inaccessible = &mDVDImagesInaccessible;
            break;
        case VBoxDefs::MediumType_Floppy:
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
            if (*inaccessible || aItem->state() != KMediumState_Inaccessible)
                break; /* no */

            *inaccessible = true;

            mTabWidget->setTabIcon (tab, vboxGlobal().warningIcon());

            break;
        }
        case ItemAction_Updated:
        case ItemAction_Removed:
        {
            bool checkRest = false;

            if (aAction == ItemAction_Updated)
            {
                /* Does it change the overall state? */
                if ((*inaccessible && aItem->state() == KMediumState_Inaccessible) ||
                    (!*inaccessible && aItem->state() != KMediumState_Inaccessible))
                    break; /* no */

                /* Is the given item in charge? */
                if (!*inaccessible && aItem->state() == KMediumState_Inaccessible)
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
                    if (*it != aItem && (*it)->state() == KMediumState_Inaccessible)
                    {
                        *inaccessible = true;
                        break;
                    }
                }
            }

            if (*inaccessible)
                mTabWidget->setTabIcon (tab, vboxGlobal().warningIcon());
            else
                mTabWidget->setTabIcon (tab, *icon);

            break;
        }
    }
}

MediaItem* VBoxMediaManagerDlg::searchItem (QTreeWidget *aTree, const QString &aId) const
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
            /* Restrict selecting mediums
             * already used by other attachments */
            return !mUsedMediaIds.contains (aItem->id());
        }
        case Action_Edit:
        {
            /* Edit means changing the description and alike; any media that is
             * not being read to or written from can be altered in these
             * terms */
            switch (aItem->state())
            {
                case KMediumState_NotCreated:
                case KMediumState_Inaccessible:
                case KMediumState_LockedRead:
                case KMediumState_LockedWrite:
                    return false;
                default:
                    break;
            }
            return true;
        }
        case Action_Copy:
        {
            /* False for children: */
            return !aItem->medium().parent();
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
            case VBoxDefs::MediumType_HardDisk:
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
            case VBoxDefs::MediumType_DVD:
                err |= (suffix != "iso");
                break;
            case VBoxDefs::MediumType_Floppy:
                err |= (suffix != "img");
                break;
            default:
                AssertMsgFailed (("Selected tree should be equal to one item in VBoxDefs::MediumType.\n"));
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
        VBoxDefs::MediumType type = currentTreeWidgetType();
        addMediumToList (file, type);
    }
}

void VBoxMediaManagerDlg::clearInfoPanes()
{
    m_pTypePane->clear(); m_pLocationPane->clear(); m_pFormatPane->clear(); m_pDetailsPane->clear(); m_pUsagePane->clear();
    mIpCD1->clear(); mIpCD2->clear();
    mIpFD1->clear(); mIpFD2->clear();
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

    mi = toMediaItem (mTwHD->currentItem());
    if (mHDSelectedId.isNull())
        mHDSelectedId = mi ? mi->id() : QString::null;

    mi = toMediaItem (mTwCD->currentItem());
    if (mCDSelectedId.isNull())
        mCDSelectedId = mi ? mi->id() : QString::null;

    mi = toMediaItem (mTwFD->currentItem());
    if (mFDSelectedId.isNull())
        mFDSelectedId = mi ? mi->id() : QString::null;

    /* Finally, clear all the lists...
     * Qt4 has interesting bug here. It sends the currentChanged (cur, prev)
     * signal during list clearing with 'cur' set to null and 'prev' pointing
     * to already excluded element if this element is not topLevelItem
     * (has parent). Cause the Hard Disk list has such elements there is
     * seg-fault when trying to make 'prev' element the current due to 'cur'
     * is null and at least one element have to be selected (by policy).
     * So just blocking any signals outgoing from the list during clearing. */
    mTwHD->blockSignals (true);
    mTwHD->clear();
    mTwHD->blockSignals (false);
    mTwCD->clear();
    mTwFD->clear();
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

