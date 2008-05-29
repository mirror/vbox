/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxDiskImageManagerDlg class implementation
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

#include "VBoxDiskImageManagerDlg.h"
#include "VBoxToolBar.h"
#include "QIRichLabel.h"
#include "VBoxNewHDWzd.h"
#include "VBoxProblemReporter.h"

/* Qt includes */
#include <QLabel>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QMenuBar>
#include <QPushButton>
#include <QUrl>

class AddVDMUrlsEvent: public QEvent
{
public:

    AddVDMUrlsEvent (const QList<QUrl> &aUrls)
        : QEvent (static_cast<QEvent::Type> (VBoxDefs::AddVDMUrlsEventType))
        , mUrls (aUrls) 
    {}

    const QList<QUrl> &urls() const { return mUrls; }

private:

    const QList<QUrl> mUrls;
};

class DiskImageItem : public QTreeWidgetItem
{
public:

    enum { TypeId = QTreeWidgetItem::UserType + 1 };

    DiskImageItem (DiskImageItem *aParent) :
        QTreeWidgetItem (aParent, TypeId), mStatus (VBoxMedia::Unknown) {}

    DiskImageItem (QTreeWidget *aParent) :
        QTreeWidgetItem (aParent, TypeId), mStatus (VBoxMedia::Unknown) {}

    void setMedia (const VBoxMedia &aMedia) { mMedia = aMedia; }
    const VBoxMedia &media() const { return mMedia; }

    void setPath (const QString &aPath) { mPath = aPath; }
    const QString &path() const { return mPath; }

    void setUsage (const QString &aUsage) { mUsage = aUsage; }
    const QString &usage() const { return mUsage; }

    void setSnapshotUsage (const QString &aSnapshotUsage) { mSnapshotUsage = aSnapshotUsage; }
    const QString &snapshotUsage() const { return mSnapshotUsage; }

    QString totalUsage() const
    {
        /* Should correlate with VBoxDiskImageManagerDlg::compose[Cd/Fd]Tooltip */
        return mSnapshotUsage.isNull() ? mUsage :
            QString ("%1 (%2)").arg (mUsage, mSnapshotUsage);
    }

    void setSnapshotName (const QString &aSnapshotName) { mSnapshotName = aSnapshotName; }
    const QString &snapshotName() const { return mSnapshotName; }

    void setDiskType (const QString &aDiskType) { mDiskType = aDiskType; }
    const QString &diskType() const { return mDiskType; }

    void setStorageType (const QString &aStorageType) { mStorageType = aStorageType; }
    const QString &storageType() const { return mStorageType; }

    void setVirtualSize (const QString &aVirtualSize) { mVirtualSize = aVirtualSize; }
    const QString &virtualSize() const { return mVirtualSize; }

    void setActualSize (const QString &aActualSize) { mActualSize = aActualSize; }
    const QString &ActualSize() const { return mActualSize; }

    void setUuid (const QUuid &aUuid) { mUuid = aUuid; }
    const QUuid &uuid() const { return mUuid; }

    void setMachineId (const QUuid &aMachineId) { mMachineId = aMachineId; }
    const QUuid &machineId() const { return mMachineId; }

    void setStatus (VBoxMedia::Status aStatus) { mStatus = aStatus; }
    VBoxMedia::Status status() const { return mStatus; }

    void setToolTip (const QString& aToolTip) 
    { 
        mToolTip = aToolTip; 
        for (int i=0; i < treeWidget()->columnCount(); ++i)
            QTreeWidgetItem::setToolTip (i, mToolTip);
    }
    const QString &toolTip() const { return mToolTip; }

    QString information (const QString &aInfo, bool aCompact = true,
                         const QString &aElipsis = "middle")
    {
        QString compactString = QString ("<compact elipsis=\"%1\">").arg (aElipsis);
        QString info = QString ("<nobr>%1%2%3</nobr>")
                       .arg (aCompact ? compactString : "")
                       .arg (aInfo.isEmpty() ?
                             QApplication::translate ("VBoxDiskImageManagerDlg", "--", "no info") :
                             aInfo)
                       .arg (aCompact ? "</compact>" : "");
        return info;
    }

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
//        if (mStatus == VBoxMedia::Unknown)
//            cGroup.setColor (QColorGroup::Text, cGroup.mid());
//        Q3ListViewItem::paintCell (aPainter, cGroup, aColumn, aWidth, aSlign);
//    }

protected:

    /* Protected member vars */
    VBoxMedia mMedia;

    QString mName;
    QString mPath;
    QString mUsage;
    QString mSnapshotUsage;
    QString mSnapshotName;
    QString mDiskType;
    QString mStorageType;
    QString mVirtualSize;
    QString mActualSize;

    QUuid mUuid;
    QUuid mMachineId;

    QString mToolTip;

    VBoxMedia::Status mStatus;
};

class DiskImageItemIterator : public QTreeWidgetItemIterator
{
public:

    DiskImageItemIterator (QTreeWidget* aTree)
        : QTreeWidgetItemIterator (aTree) {}

    DiskImageItem* operator*()
    {
        QTreeWidgetItem *item = QTreeWidgetItemIterator::operator*();
        return item && item->type() == DiskImageItem::TypeId ?
            static_cast<DiskImageItem*> (item) : NULL;
    }

    DiskImageItemIterator& operator++()
    {
        return static_cast<DiskImageItemIterator&> (QTreeWidgetItemIterator::operator++());
    }
};

class InfoPaneLabel : public QIRichLabel
{
public:

    InfoPaneLabel (QWidget *aParent, QLabel *aLabel = 0)
        : QIRichLabel (aParent), mLabel (aLabel) {}

    QLabel* label() const { return mLabel; }

private:

    /* Private member vars */
    QLabel *mLabel;
};

VBoxDiskImageManagerDlg *VBoxDiskImageManagerDlg::mModelessDialog = NULL;

VBoxDiskImageManagerDlg::VBoxDiskImageManagerDlg (QWidget *aParent /* = NULL */,  Qt::WindowFlags aFlags /* = 0 */)
    : QIWithRetranslateUI2<QMainWindow> (aParent, aFlags)
    , mRescode (Rejected)
    , mType (VBoxDefs::InvalidType)
{
    /* Apply UI decorations */
    Ui::VBoxDiskImageManagerDlg::setupUi (this);

//    defaultButton = searchDefaultButton();

    mType = VBoxDefs::InvalidType;

    mIconInaccessible = style()->standardIcon (QStyle::SP_MessageBoxWarning);
    mIconErroneous = style()->standardIcon (QStyle::SP_MessageBoxCritical);
    mIconHD = VBoxGlobal::iconSet (":/hd_16px.png", ":/hd_disabled_16px.png");
    mIconCD = VBoxGlobal::iconSet (":/cd_16px.png", ":/cd_disabled_16px.png");
    mIconFD = VBoxGlobal::iconSet (":/fd_16px.png", ":/fd_disabled_16px.png");

    /* Setup tab widget icons */
    twImages->setTabIcon (HDTab, mIconHD);
    twImages->setTabIcon (CDTab, mIconCD);
    twImages->setTabIcon (FDTab, mIconFD);

    connect (twImages, SIGNAL (currentChanged (int)),
             this, SLOT (processCurrentChanged (int)));

    /* Setup the tree view widgets */
    mHdsTree->header()->setResizeMode (0, QHeaderView::Stretch);
    mHdsTree->header()->setResizeMode (1, QHeaderView::ResizeToContents);
    mHdsTree->header()->setResizeMode (2, QHeaderView::ResizeToContents);
    mHdsTree->setSortingEnabled(true);
    mHdsTree->setSupportedDropActions (Qt::LinkAction);
    mHdsTree->installEventFilter (this);
    connect (mHdsTree, SIGNAL (currentItemChanged (QTreeWidgetItem *, QTreeWidgetItem *)),
             this, SLOT (processCurrentChanged (QTreeWidgetItem *, QTreeWidgetItem *)));
    connect (mHdsTree, SIGNAL (itemDoubleClicked (QTreeWidgetItem *, int)),
             this, SLOT (processDoubleClick (QTreeWidgetItem *, int)));
    connect (mHdsTree, SIGNAL (itemRightClicked (const QPoint &, QTreeWidgetItem *, int)),
             this, SLOT (processRightClick (const QPoint &, QTreeWidgetItem *, int)));

    mCdsTree->header()->setResizeMode (0, QHeaderView::Stretch);
    mCdsTree->header()->setResizeMode (1, QHeaderView::ResizeToContents);
    mCdsTree->setSortingEnabled(true);
    mCdsTree->setSupportedDropActions (Qt::LinkAction);
    mCdsTree->installEventFilter (this);
    connect (mCdsTree, SIGNAL (currentItemChanged (QTreeWidgetItem *, QTreeWidgetItem *)),
             this, SLOT (processCurrentChanged (QTreeWidgetItem *, QTreeWidgetItem *)));
    connect (mCdsTree, SIGNAL (itemDoubleClicked (QTreeWidgetItem *, int)),
             this, SLOT (processDoubleClick (QTreeWidgetItem *, int)));
    connect (mCdsTree, SIGNAL (itemRightClicked (const QPoint &, QTreeWidgetItem *, int)),
             this, SLOT (processRightClick (const QPoint &, QTreeWidgetItem *, int)));

    mFdsTree->header()->setResizeMode (0, QHeaderView::Stretch);
    mFdsTree->header()->setResizeMode (1, QHeaderView::ResizeToContents);
    mFdsTree->setSortingEnabled(true);
    mFdsTree->setSupportedDropActions (Qt::LinkAction);
    mFdsTree->installEventFilter (this);
    connect (mFdsTree, SIGNAL (currentItemChanged (QTreeWidgetItem *, QTreeWidgetItem *)),
             this, SLOT (processCurrentChanged (QTreeWidgetItem *, QTreeWidgetItem *)));
    connect (mFdsTree, SIGNAL (itemDoubleClicked (QTreeWidgetItem *, int)),
             this, SLOT (processDoubleClick (QTreeWidgetItem *, int)));
    connect (mFdsTree, SIGNAL (itemRightClicked (const QPoint &, QTreeWidgetItem *, int)),
             this, SLOT (processRightClick (const QPoint &, QTreeWidgetItem *, int)));

    /* Context menu composing */
    mActionsContextMenu = new QMenu (this);

    mNewAction = new QAction (this);
    mAddAction = new QAction (this);
    // mEditAction = new QAction (this);
    mRemoveAction = new QAction (this);
    mReleaseAction = new QAction (this);
    mRefreshAction = new QAction (this);

    connect (mNewAction, SIGNAL (activated()),
             this, SLOT (newImage()));
    connect (mAddAction, SIGNAL (activated()),
             this, SLOT (addImage()));
    // connect (mEditAction, SIGNAL (activated()),
    //          this, SLOT (editImage()));
    connect (mRemoveAction, SIGNAL (activated()),
             this, SLOT (removeImage()));
    connect (mReleaseAction, SIGNAL (activated()),
             this, SLOT (releaseImage()));
    connect (mRefreshAction, SIGNAL (activated()),
             this, SLOT (refreshAll()));

    mNewAction->setIcon (VBoxGlobal::iconSetEx (
        ":/vdm_new_22px.png", ":/vdm_new_16px.png",
        ":/vdm_new_disabled_22px.png", ":/vdm_new_disabled_16px.png"));
    mAddAction->setIcon (VBoxGlobal::iconSetEx (
        ":/vdm_add_22px.png", ":/vdm_add_16px.png",
        ":/vdm_add_disabled_22px.png", ":/vdm_add_disabled_16px.png"));
    // mEditAction->setIcon (VBoxGlobal::iconSet (":/guesttools_16px.png", ":/guesttools_disabled_16px.png"));
    mRemoveAction->setIcon (VBoxGlobal::iconSetEx (
        ":/vdm_remove_22px.png", ":/vdm_remove_16px.png",
        ":/vdm_remove_disabled_22px.png", ":/vdm_remove_disabled_16px.png"));
    mReleaseAction->setIcon (VBoxGlobal::iconSetEx (
        ":/vdm_release_22px.png", ":/vdm_release_16px.png",
        ":/vdm_release_disabled_22px.png", ":/vdm_release_disabled_16px.png"));
    mRefreshAction->setIcon (VBoxGlobal::iconSetEx (
        ":/refresh_22px.png", ":/refresh_16px.png",
        ":/refresh_disabled_22px.png", ":/refresh_disabled_16px.png"));

//    mActionsMenu->addAction (mEditAction);
    mActionsContextMenu->addAction (mRemoveAction);
    mActionsContextMenu->addAction (mReleaseAction);

    /* Toolbar composing */
    mActionsToolBar = new VBoxToolBar (this);
    mActionsToolBar->setIconSize (QSize (32, 32));
    mActionsToolBar->setToolButtonStyle (Qt::ToolButtonTextUnderIcon);
    mActionsToolBar->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Preferred);

    /* Really messy what the uic produce here: a vbox layout in an hbox layout */
    QHBoxLayout *centralLayout = qobject_cast<QHBoxLayout*> (centralWidget()->layout());
    Assert (VALID_PTR (centralLayout));
    QVBoxLayout *mainLayout = static_cast<QVBoxLayout*> (centralLayout->itemAt(0));
    Assert (VALID_PTR (mainLayout));
#if MAC_LEOPARD_STYLE
    /* Enable unified toolbars on Mac OS X. Available on Qt >= 4.3 */
    setUnifiedTitleAndToolBarOnMac (true);
    addToolBar (mActionsToolBar);
    /* No spacing/margin on the mac */
    VBoxGlobal::setLayoutMargin (centralLayout, 0);
    VBoxGlobal::setLayoutMargin (mainLayout, 0);
#else /* MAC_LEOPARD_STYLE */
    /* Add the toolbar */
    mainLayout->insertWidget (0, mActionsToolBar);
    /* Set spacing/margin like in the selector window */
    centralLayout->setSpacing (0);
    VBoxGlobal::setLayoutMargin (centralLayout, 0);
    mainLayout->setSpacing (9);
    VBoxGlobal::setLayoutMargin (mainLayout, 5);
#endif /* MAC_LEOPARD_STYLE */

    mActionsToolBar->addAction (mNewAction);
    mActionsToolBar->addAction (mAddAction);
    mActionsToolBar->addSeparator();
//    mActionsToolBar->addAction (mEditAction);
    mActionsToolBar->addAction (mRemoveAction);
    mActionsToolBar->addAction (mReleaseAction);
    mActionsToolBar->addSeparator();
    mActionsToolBar->addAction (mRefreshAction);

    /* Menu bar */
    mActionsMenu = menuBar()->addMenu (QString::null);
    mActionsMenu->addAction (mNewAction);
    mActionsMenu->addAction (mAddAction);
    mActionsMenu->addSeparator();
//    mActionsMenu->addAction (mEditAction);
    mActionsMenu->addAction (mRemoveAction);
    mActionsMenu->addAction (mReleaseAction);
    mActionsMenu->addSeparator();
    mActionsMenu->addAction (mRefreshAction);

//    qApp->installEventFilter (this);

    /* Setup information pane layouts */
    QGridLayout *hdsContainerLayout = new QGridLayout (mHdsContainer);
    VBoxGlobal::setLayoutMargin (hdsContainerLayout, 5);
    hdsContainerLayout->setSpacing (0);
    QGridLayout *cdsContainerLayout = new QGridLayout (mCdsContainer);
    VBoxGlobal::setLayoutMargin (cdsContainerLayout, 5);
    cdsContainerLayout->setSpacing (0);
    QGridLayout *fdsContainerLayout = new QGridLayout (mFdsContainer);
    VBoxGlobal::setLayoutMargin (fdsContainerLayout, 5);
    fdsContainerLayout->setSpacing (0);
    /* Create info-pane for hd list-view */
    createInfoString (mHdsPane1, mHdsContainer, 0, 0, 1, 3);
    createInfoString (mHdsPane2, mHdsContainer, 1, 0);
    createInfoString (mHdsPane3, mHdsContainer, 1, 2);
    createInfoString (mHdsPane4, mHdsContainer, 2, 0);
    createInfoString (mHdsPane5, mHdsContainer, 2, 2);
    /* Create info-pane for cd list-view */
    createInfoString (mCdsPane1, mCdsContainer, 0, 0);
    createInfoString (mCdsPane2, mCdsContainer, 1, 0);
    /* Create info-pane for fd list-view */
    createInfoString (mFdsPane1, mFdsContainer, 0, 0);
    createInfoString (mFdsPane2, mFdsContainer, 1, 0);

    /* enumeration progressbar creation */
//    mProgressText = new QLabel (centralWidget());
//    mProgressText->setHidden (true);
#warning port me
//    buttonLayout->insertWidget (2, mProgressText);
//    mProgressBar = new Q3ProgressBar (centralWidget());
//    mProgressBar->setHidden (true);
//    mProgressBar->setFrameShadow (Q3Frame::Sunken);
//    mProgressBar->setFrameShape  (Q3Frame::Panel);
//    mProgressBar->setPercentageVisible (false);
//    mProgressBar->setMaximumWidth (100);
#warning port me
//    buttonLayout->insertWidget (3, mProgressBar);

    /* Connects for the button box */
    connect (mButtonBox, SIGNAL (accepted()),
             this, SLOT (accept()));
    connect (mButtonBox, SIGNAL (rejected()),
             this, SLOT (reject()));
    connect (mButtonBox, SIGNAL (helpRequested()),
             &vboxProblem(), SLOT (showHelpHelpDialog()));

    /* Applying language settings */
    retranslateUi();
}

void VBoxDiskImageManagerDlg::setup (int aType, bool aDoSelect,
                                     const QUuid *aTargetVMId /* = NULL */,
                                     bool aRefresh /* = true */,
                                     CMachine aMachine /* = NULL */,
                                     const QUuid &aHdId,
                                     const QUuid &aCdId,
                                     const QUuid &aFdId)
{
    mMachine = aMachine;
    mHdSelectedId = aHdId;
    mCdSelectedId = aCdId;
    mFdSelectedId = aFdId;

    mType = aType;
    twImages->setTabEnabled (HDTab, mType & VBoxDefs::HD);
    twImages->setTabEnabled (CDTab, mType & VBoxDefs::CD);
    twImages->setTabEnabled (FDTab, mType & VBoxDefs::FD);

    mDoSelect = aDoSelect;
    if (aTargetVMId)
        mTargetVMId = *aTargetVMId;

    if (mDoSelect)
        mButtonBox->button (QDialogButtonBox::Ok)->setText (tr ("&Select"));
    else
        mButtonBox->button (QDialogButtonBox::Cancel)->setVisible (false);

    /* Listen to "media enumeration started" signals */
    connect (&vboxGlobal(), SIGNAL (mediaEnumStarted()),
             this, SLOT (mediaEnumStarted()));
    /* Listen to "media enumeration" signals */
    connect (&vboxGlobal(), SIGNAL (mediaEnumerated (const VBoxMedia &, int)),
             this, SLOT (mediaEnumerated (const VBoxMedia &, int)));
    /* Listen to "media enumeration finished" signals */
    connect (&vboxGlobal(), SIGNAL (mediaEnumFinished (const VBoxMediaList &)),
             this, SLOT (mediaEnumFinished (const VBoxMediaList &)));

    /* Listen to "media add" signals */
    connect (&vboxGlobal(), SIGNAL (mediaAdded (const VBoxMedia &)),
             this, SLOT (mediaAdded (const VBoxMedia &)));
    /* Listen to "media update" signals */
    connect (&vboxGlobal(), SIGNAL (mediaUpdated (const VBoxMedia &)),
             this, SLOT (mediaUpdated (const VBoxMedia &)));
    /* Listen to "media remove" signals */
    connect (&vboxGlobal(), SIGNAL (mediaRemoved (VBoxDefs::DiskType, const QUuid &)),
             this, SLOT (mediaRemoved (VBoxDefs::DiskType, const QUuid &)));

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
            mediaAdded (*it);
//            if ((*it).status != VBoxMedia::Unknown)
//                mProgressBar->setProgress (++ index);
        }

        /* Emulate the finished signal to reuse the code */
        if (!vboxGlobal().isMediaEnumerationStarted())
            mediaEnumFinished (list);
    }

    /* For a newly opened dialog, select the first item */
    if (!mHdsTree->selectedItems().isEmpty())
        if (QTreeWidgetItem *item = mHdsTree->topLevelItem (0))
            setCurrentItem (mHdsTree, item);
    if (!mCdsTree->selectedItems().isEmpty())
        if (QTreeWidgetItem *item = mCdsTree->topLevelItem (0))
            setCurrentItem (mCdsTree, item);
    if (!mFdsTree->selectedItems().isEmpty())
        if (QTreeWidgetItem *item = mFdsTree->topLevelItem (0))
            setCurrentItem (mFdsTree, item);
}

void VBoxDiskImageManagerDlg::showModeless (bool aRefresh /* = true */)
{
    if (!mModelessDialog)
    {
        mModelessDialog =
            new VBoxDiskImageManagerDlg (NULL, Qt::Window);
        mModelessDialog->setAttribute (Qt::WA_DeleteOnClose);
        mModelessDialog->setup (VBoxDefs::HD | VBoxDefs::CD | VBoxDefs::FD,
                                false, NULL, aRefresh);

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

int VBoxDiskImageManagerDlg::exec()
{
    AssertMsg (!mEventLoop, ("exec is called recursively!\n"));

    /* Reset the result code */
    setResult (Rejected);
    bool deleteOnClose = testAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_DeleteOnClose, false);
    /* Create a local event loop */
    mEventLoop = new QEventLoop();
    /* Show the window */
    show();
    /* A guard to ourself for the case we destroy ourself. */
    QPointer<VBoxDiskImageManagerDlg> guard = this;
    /* Start the event loop. This blocks. */
    mEventLoop->exec();
    /* Delete the event loop */
    delete mEventLoop;
    /* Are we valid anymore? */
    if (guard.isNull())
        return Rejected;
     int res = result();
     if (deleteOnClose)
         delete this;
    /* Return the final result */
    return res;
}

QUuid VBoxDiskImageManagerDlg::selectedUuid() const
{
    QTreeWidget *tree = currentTreeWidget();
    QUuid uuid;

    DiskImageItem *item = toDiskImageItem (selectedItem (tree));
    if (item)
        uuid = item->uuid();

    return uuid;
}

QString VBoxDiskImageManagerDlg::selectedPath() const
{
    QTreeWidget *tree = currentTreeWidget();
    QString path;

    DiskImageItem *item = toDiskImageItem (selectedItem (tree));
    if (item)
        path = item->path().trimmed();

    return path;
}

QString VBoxDiskImageManagerDlg::composeHdToolTip (CHardDisk &aHd,
                                                   VBoxMedia::Status aStatus,
                                                   DiskImageItem *aItem)
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    QUuid machineId = aItem ? aItem->machineId() : aHd.GetMachineId();

    QString src = aItem ? aItem->path() : aHd.GetLocation();
    QString location = aItem || aHd.GetStorageType() == KHardDiskStorageType_ISCSIHardDisk ? src :
        QDir::convertSeparators (QFileInfo (src).absoluteFilePath());

    QString storageType = aItem ? aItem->storageType() :
        vboxGlobal().toString (aHd.GetStorageType());
    QString hardDiskType = aItem ? aItem->diskType() :
        vboxGlobal().hardDiskTypeString (aHd);

    QString usage;
    if (aItem)
        usage = aItem->usage();
    else if (!machineId.isNull())
        usage = vbox.GetMachine (machineId).GetName();

    QUuid snapshotId = aItem ? aItem->uuid() : aHd.GetSnapshotId();
    QString snapshotName;
    if (aItem)
        snapshotName = aItem->snapshotName();
    else if (!machineId.isNull() && !snapshotId.isNull())
    {
        CSnapshot snapshot = vbox.GetMachine (machineId).
                                  GetSnapshot (aHd.GetSnapshotId());
        if (!snapshot.isNull())
            snapshotName = snapshot.GetName();
    }

    /* Compose tool-tip information */
    QString tip;
    switch (aStatus)
    {
        case VBoxMedia::Unknown:
        {
            tip = tr ("<nobr><b>%1</b></nobr><br>"
                      "Checking accessibility...", "HDD")
                      .arg (location);
            break;
        }
        case VBoxMedia::Ok:
        {
            tip = tr ("<nobr><b>%1</b></nobr><br>"
                      "<nobr>Disk type:&nbsp;&nbsp;%2</nobr><br>"
                      "<nobr>Storage type:&nbsp;&nbsp;%3</nobr>")
                      .arg (location)
                      .arg (hardDiskType)
                      .arg (storageType);

            if (!usage.isNull())
                tip += tr ("<br><nobr>Attached to:&nbsp;&nbsp;%1</nobr>", "HDD")
                           .arg (usage);
            if (!snapshotName.isNull())
                tip += tr ("<br><nobr>Snapshot:&nbsp;&nbsp;%5</nobr>", "HDD")
                           .arg (snapshotName);
            break;
        }
        case VBoxMedia::Error:
        {
            /// @todo (r=dmik) paass a complete VBoxMedia instance here
            //  to get the result of blabla.GetAccessible() call form CUnknown
            tip = tr ("<nobr><b>%1</b></nobr><br>"
                      "Error checking media accessibility", "HDD")
                      .arg (location);
            break;
        }
        case VBoxMedia::Inaccessible:
        {
            tip = tr ("<nobr><b>%1</b></nobr><br>%2", "HDD")
                      .arg (location)
                      .arg (VBoxGlobal::highlight (aHd.GetLastAccessError(),
                                                   true /* aToolTip */));
            break;
        }
        default:
            AssertFailed();
    }
    return tip;
}

QString VBoxDiskImageManagerDlg::composeCdToolTip (CDVDImage &aCd,
                                                   VBoxMedia::Status aStatus,
                                                   DiskImageItem *aItem)
{
    QString location = aItem ? aItem->path() :
        QDir::convertSeparators (QFileInfo (aCd.GetFilePath()).absoluteFilePath());
    QUuid uuid = aItem ? aItem->uuid() : aCd.GetId();
    QString usage;
    if (aItem)
        usage = aItem->totalUsage();
    else
    {
        QString snapshotUsage;
        usage = DVDImageUsage (uuid, snapshotUsage);
        /* Should correlate with DiskImageItem::getTotalUsage() */
        if (!snapshotUsage.isNull())
            usage = QString ("%1 (%2)").arg (usage, snapshotUsage);
    }

    /* Compose tool-tip information */
    QString tip;
    switch (aStatus)
    {
        case VBoxMedia::Unknown:
        {
            tip = tr ("<nobr><b>%1</b></nobr><br>"
                      "Checking accessibility...", "CD/DVD/Floppy")
                      .arg (location);
            break;
        }
        case VBoxMedia::Ok:
        {
            tip = tr ("<nobr><b>%1</b></nobr>", "CD/DVD/Floppy")
                      .arg (location);

            if (!usage.isNull())
                tip += tr ("<br><nobr>Attached to:&nbsp;&nbsp;%1</nobr>",
                           "CD/DVD/Floppy")
                           .arg (usage);
            break;
        }
        case VBoxMedia::Error:
        {
            /// @todo (r=dmik) paass a complete VBoxMedia instance here
            //  to get the result of blabla.GetAccessible() call form CUnknown
            tip = tr ("<nobr><b>%1</b></nobr><br>"
                      "Error checking media accessibility", "CD/DVD/Floppy")
                      .arg (location);
            break;
        }
        case VBoxMedia::Inaccessible:
        {
            /// @todo (r=dmik) correct this when GetLastAccessError() is
            //  implemented for IDVDImage
            tip = tr ("<nobr><b>%1</b></nobr><br>%2")
                      .arg (location)
                      .arg (tr ("The image file is not accessible",
                                "CD/DVD/Floppy"));
            break;
        }
        default:
            AssertFailed();
    }
    return tip;
}

QString VBoxDiskImageManagerDlg::composeFdToolTip (CFloppyImage &aFd,
                                                   VBoxMedia::Status aStatus,
                                                   DiskImageItem *aItem)
{
    QString location = aItem ? aItem->path() :
        QDir::convertSeparators (QFileInfo (aFd.GetFilePath()).absoluteFilePath());
    QUuid uuid = aItem ? aItem->uuid() : aFd.GetId();
    QString usage;
    if (aItem)
        usage = aItem->totalUsage();
    else
    {
        QString snapshotUsage;
        usage = FloppyImageUsage (uuid, snapshotUsage);
        /* Should correlate with DiskImageItem::getTotalUsage() */
        if (!snapshotUsage.isNull())
            usage = QString ("%1 (%2)").arg (usage, snapshotUsage);
    }

    /* Compose tool-tip information */
    QString tip;
    switch (aStatus)
    {
        case VBoxMedia::Unknown:
        {
            tip = tr ("<nobr><b>%1</b></nobr><br>"
                      "Checking accessibility...", "CD/DVD/Floppy")
                      .arg (location);
            break;
        }
        case VBoxMedia::Ok:
        {
            tip = tr ("<nobr><b>%1</b></nobr>", "CD/DVD/Floppy")
                      .arg (location);

            if (!usage.isNull())
                tip += tr ("<br><nobr>Attached to:&nbsp;&nbsp;%1</nobr>",
                           "CD/DVD/Floppy")
                           .arg (usage);
            break;
        }
        case VBoxMedia::Error:
        {
            /// @todo (r=dmik) paass a complete VBoxMedia instance here
            //  to get the result of blabla.GetAccessible() call form CUnknown
            tip = tr ("<nobr><b>%1</b></nobr><br>"
                      "Error checking media accessibility", "CD/DVD/Floppy")
                      .arg (location);
            break;
        }
        case VBoxMedia::Inaccessible:
        {
            /// @todo (r=dmik) correct this when GetLastAccessError() is
            //  implemented for IDVDImage
            tip = tr ("<nobr><b>%1</b></nobr><br>%2")
                      .arg (location)
                      .arg (tr ("The image file is not accessible",
                                "CD/DVD/Floppy"));
            break;
        }
        default:
            AssertFailed();
    }
    return tip;
}

void VBoxDiskImageManagerDlg::refreshAll()
{
    /* Start enumerating media */
    vboxGlobal().startEnumeratingMedia();
}

void VBoxDiskImageManagerDlg::setVisible (bool aVisible)
{
    QMainWindow::setVisible (aVisible);
    /* Exit from the event loop if there is any and we are changing our state
     * from visible to invisible. */
    if(mEventLoop && !aVisible)
        mEventLoop->exit();
}

void VBoxDiskImageManagerDlg::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxDiskImageManagerDlg::retranslateUi (this);

    mActionsMenu->setTitle (tr ("&Actions"));

    mNewAction->setText (tr ("&New"));
    mAddAction->setText (tr ("&Add"));
    // mEditAction->setText (tr ("&Edit"));
    mRemoveAction->setText (tr ("R&emove"));
    mReleaseAction->setText (tr ("Re&lease"));
    mRefreshAction->setText (tr ("Re&fresh"));

    mNewAction->setShortcut (tr ("Ctrl+N"));
    mAddAction->setShortcut (tr ("Ctrl+A"));
    // mEditAction->setShortcut (tr ("Ctrl+E"));
    mRemoveAction->setShortcut (tr ("Ctrl+D"));
    mReleaseAction->setShortcut (tr ("Ctrl+L"));
    mRefreshAction->setShortcut (tr ("Ctrl+R"));

    mNewAction->setStatusTip (tr ("Create a new virtual hard disk"));
    mAddAction->setStatusTip (tr ("Add (register) an existing image file"));
    // mEditAction->setStatusTip (tr ("Edit the properties of the selected item"));
    mRemoveAction->setStatusTip (tr ("Remove (unregister) the selected media"));
    mReleaseAction->setStatusTip (tr ("Release the selected media by detaching it from the machine"));
    mRefreshAction->setStatusTip (tr ("Refresh the media list"));

    mHdsPane1->label()->setText (QString ("<nobr>%1:</nobr>").arg (tr ("Location")));
    mHdsPane2->label()->setText (QString ("<nobr>%1:</nobr>").arg (tr ("Disk Type")));
    mHdsPane3->label()->setText (QString ("<nobr>&nbsp;&nbsp;%1:</nobr>").arg (tr ("Storage Type")));
    mHdsPane4->label()->setText (QString ("<nobr>%1:</nobr>").arg (tr ("Attached to")));
    mHdsPane5->label()->setText (QString ("<nobr>&nbsp;&nbsp;%1:</nobr>").arg (tr ("Snapshot")));
    mCdsPane1->label()->setText (QString ("<nobr>%1:</nobr>").arg (tr ("Location")));
    mCdsPane2->label()->setText (QString ("<nobr>%1:</nobr>").arg (tr ("Attached to")));
    mFdsPane1->label()->setText (QString ("<nobr>%1:</nobr>").arg (tr ("Location")));
    mFdsPane2->label()->setText (QString ("<nobr>%1:</nobr>").arg (tr ("Attached to")));

//    mProgressText->setText (tr ("Checking accessibility"));
//
    if (mHdsTree->model()->rowCount() || mCdsTree->model()->rowCount() || mFdsTree->model()->rowCount())
        refreshAll();
}

void VBoxDiskImageManagerDlg::closeEvent (QCloseEvent *aEvent)
{
    mModelessDialog = NULL;
    aEvent->accept();
}

void VBoxDiskImageManagerDlg::keyPressEvent (QKeyEvent *aEvent)
{
    if (aEvent->modifiers() == Qt::NoModifier ||
        (aEvent->modifiers() != Qt::NoModifier && aEvent->key() == Qt::Key_Enter))
    {
        switch (aEvent->key())
        {
            case Qt::Key_Enter:
            case Qt::Key_Return:
                {
                    accept();
                    break;
                }
            case Qt::Key_Escape:
                {
                    reject();
                    break;
                }
        }
    }
    else
        aEvent->ignore();
}

bool VBoxDiskImageManagerDlg::eventFilter (QObject *aObject, QEvent *aEvent)
{
    switch (aEvent->type())
    {
        case QEvent::DragEnter:
            {
                QDragEnterEvent *deEvent = static_cast<QDragEnterEvent*> (aEvent);
                if (deEvent->mimeData()->hasUrls())
                {
                    if (checkDndUrls (deEvent->mimeData()->urls()))
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
                QDropEvent *dEvent = static_cast<QDropEvent*> (aEvent);
                if (dEvent->mimeData()->hasUrls())
                {
                    AddVDMUrlsEvent *event = new AddVDMUrlsEvent (dEvent->mimeData()->urls());
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
                AddVDMUrlsEvent *addEvent = static_cast<AddVDMUrlsEvent*> (aEvent);
                addDndUrls (addEvent->urls());
                return true;
            }
            break;
        }
        default:
            break;
    }
    return QMainWindow::eventFilter (aObject, aEvent);
}

/* @todo: Currently not used (Ported from Qt3): */
void VBoxDiskImageManagerDlg::machineStateChanged (const VBoxMachineStateChangeEvent &aEvent)
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

void VBoxDiskImageManagerDlg::mediaAdded (const VBoxMedia &aMedia)
{
    /* Ignore non-interesting aMedia */
    if (!(mType & aMedia.type))
        return;

    DiskImageItem *item = NULL;
    switch (aMedia.type)
    {
        case VBoxDefs::HD:
            item = createHdItem (mHdsTree, aMedia);
            if (item->uuid() == mHdSelectedId)
            {
                setCurrentItem (mHdsTree, item);
                mHdSelectedId = QUuid();
            }
            break;
        case VBoxDefs::CD:
            item = createCdItem (mCdsTree, aMedia);
            if (item->uuid() == mCdSelectedId)
            {
                setCurrentItem (mCdsTree, item);
                mCdSelectedId = QUuid();
            }
            break;
        case VBoxDefs::FD:
            item = createFdItem (mFdsTree, aMedia);
            if (item->uuid() == mFdSelectedId)
            {
                setCurrentItem (mFdsTree, item);
                mFdSelectedId = QUuid();
            }
            break;
        default:
            AssertMsgFailed (("Invalid aMedia type\n"));
    }

    if (!item)
        return;

    if (!vboxGlobal().isMediaEnumerationStarted())
        setCurrentItem (treeWidget (aMedia.type), item);
    if (item == currentTreeWidget()->currentItem())
        processCurrentChanged (item);
}

void VBoxDiskImageManagerDlg::mediaUpdated (const VBoxMedia &aMedia)
{
    /* Ignore non-interesting aMedia */
    if (!(mType & aMedia.type))
        return;

    DiskImageItem *item = NULL;
    switch (aMedia.type)
    {
        case VBoxDefs::HD:
        {
            CHardDisk hd = aMedia.disk;
            item = searchItem (mHdsTree, hd.GetId());
            updateHdItem (item, aMedia);
            break;
        }
        case VBoxDefs::CD:
        {
            CDVDImage cd = aMedia.disk;
            item = searchItem (mCdsTree, cd.GetId());
            updateCdItem (item, aMedia);
            break;
        }
        case VBoxDefs::FD:
        {
            CFloppyImage fd = aMedia.disk;
            item = searchItem (mFdsTree, fd.GetId());
            updateFdItem (item, aMedia);
            break;
        }
        default:
            AssertMsgFailed (("Invalid aMedia type\n"));
    }

    if (!item)
        return;

    /* Note: current items on invisible tabs are not updated because
     * it is always done in processCurrentChanged() when the user switches
     * to an invisible tab */
    if (item == currentTreeWidget()->currentItem())
        processCurrentChanged (item);
}

void VBoxDiskImageManagerDlg::mediaRemoved (VBoxDefs::DiskType aType,
                                            const QUuid &aId)
{
    QTreeWidget *tree = treeWidget (aType);
    DiskImageItem *item = searchItem (tree, aId);
    delete item;
    setCurrentItem (tree, tree->currentItem());
    /* Search the list for inaccessible media */
    if (!searchItem (tree, VBoxMedia::Inaccessible) &&
        !searchItem (tree, VBoxMedia::Error))
    {
        int index = aType == VBoxDefs::HD ? HDTab :
                    aType == VBoxDefs::CD ? CDTab :
                    aType == VBoxDefs::FD ? FDTab : -1;
        const QIcon &set = aType == VBoxDefs::HD ? mIconHD :
                           aType == VBoxDefs::CD ? mIconCD :
                           aType == VBoxDefs::FD ? mIconFD : QIcon();
        Assert (index != -1 && !set.isNull()); /* atype should be the correct one */
        twImages->setTabIcon (index, set);
    }
}

void VBoxDiskImageManagerDlg::mediaEnumStarted()
{
    /* Load default tab icons */
    twImages->setTabIcon (HDTab, mIconHD);
    twImages->setTabIcon (CDTab, mIconCD);
    twImages->setTabIcon (FDTab, mIconFD);

    /* Load current media list */
    const VBoxMediaList &list = vboxGlobal().currentMediaList();
    prepareToRefresh (list.size());
    VBoxMediaList::const_iterator it;
    for (it = list.begin(); it != list.end(); ++ it)
        mediaAdded (*it);

    /* Select the first item if the previous saved item is not found or no
     * current item at all */
    if (!mHdsTree->currentItem() || !mHdSelectedId.isNull())
        if (QTreeWidgetItem *item = mHdsTree->topLevelItem (0))
            setCurrentItem (mHdsTree, item);
    if (!mCdsTree->currentItem() || !mCdSelectedId.isNull())
        if (QTreeWidgetItem *item = mCdsTree->topLevelItem (0))
            setCurrentItem (mCdsTree, item);
    if (!mFdsTree->currentItem() || !mFdSelectedId.isNull())
        if (QTreeWidgetItem *item = mFdsTree->topLevelItem (0))
            setCurrentItem (mFdsTree, item);

    processCurrentChanged();
}

void VBoxDiskImageManagerDlg::mediaEnumerated (const VBoxMedia &aMedia,
                                               int aIndex)
{
    mediaUpdated (aMedia);
    Assert (aMedia.status != VBoxMedia::Unknown);
//    if (aMedia.status != VBoxMedia::Unknown)
//        mProgressBar->setProgress (aIndex + 1);
}

void VBoxDiskImageManagerDlg::mediaEnumFinished (const VBoxMediaList &/* aList */)
{
//    mProgressBar->setHidden (true);
//    mProgressText->setHidden (true);

    mRefreshAction->setEnabled (true);
    unsetCursor();

    processCurrentChanged();
}

void VBoxDiskImageManagerDlg::newImage()
{
    AssertReturnVoid (currentTreeWidgetType() == VBoxDefs::HD);

    VBoxNewHDWzd dlg (this);

    if (dlg.exec() == QDialog::Accepted)
    {
        CHardDisk hd = dlg.hardDisk();
        VBoxMedia::Status status =
            hd.GetAccessible() ? VBoxMedia::Ok :
            hd.isOk() ? VBoxMedia::Inaccessible :
            VBoxMedia::Error;
        vboxGlobal().addMedia (VBoxMedia (CUnknown (hd), VBoxDefs::HD, status));
    }
}

void VBoxDiskImageManagerDlg::addImage()
{
    QTreeWidget *tree = currentTreeWidget();
    DiskImageItem *item = toDiskImageItem (tree->currentItem());

    CVirtualBox vbox = vboxGlobal().virtualBox();

    QString title;
    QString filter;
    VBoxDefs::DiskType type = currentTreeWidgetType();

    QString dir;
    if (item && item->status() == VBoxMedia::Ok)
        dir = QFileInfo (item->path().trimmed()).absolutePath ();

    if (dir.isEmpty())
        if (type == VBoxDefs::HD)
            dir = vbox.GetSystemProperties().GetDefaultVDIFolder();

    if (dir.isEmpty() || !QFileInfo (dir).exists())
        dir = vbox.GetHomeFolder();

    switch (type)
    {
        case VBoxDefs::HD:
            {
                filter = tr ("All hard disk images (*.vdi *.vmdk);;"
                             "Virtual Disk images (*.vdi);;"
                             "VMDK images (*.vmdk);;"
                             "All files (*)");
                title = tr ("Select a hard disk image file");
                break;
            }
        case VBoxDefs::CD:
            {
                filter = tr ("CD/DVD-ROM images (*.iso);;"
                             "All files (*)");
                title = tr ("Select a CD/DVD-ROM disk image file");
                break;
            }
        case VBoxDefs::FD:
            {
                filter = tr ("Floppy images (*.img);;"
                             "All files (*)");
                title = tr ("Select a floppy disk image file");
                break;
            }
        default:
            AssertMsgFailed (("Selected tree should be equal to one item in VBoxDefs::DiskType.\n"));
            break;
    }

    QString src = VBoxGlobal::getOpenFileName (dir, filter, this, title);
    src =  QDir::convertSeparators (src);

    addImageToList (src, type);
    if (!vbox.isOk())
        vboxProblem().cannotRegisterMedia (this, vbox, type, src);
}

void VBoxDiskImageManagerDlg::removeImage()
{
    QTreeWidget *tree = currentTreeWidget();
    DiskImageItem *item = toDiskImageItem (tree->currentItem());
    AssertMsg (item, ("Current item must not be null\n"));

    CVirtualBox vbox = vboxGlobal().virtualBox();

    QUuid uuid = item->uuid();
    AssertMsg (!uuid.isNull(), ("Current item must have uuid\n"));

    QString src = item->path().trimmed();
    VBoxDefs::DiskType type = currentTreeWidgetType();

    switch (type)
    {
        case VBoxDefs::HD:
            {
                bool deleteImage = false;

                /// @todo When creation of VMDK is implemented, we should
                /// enable image deletion for  them as well (use
                /// GetStorageType() to define the correct cast).
                CHardDisk disk = item->media().disk;
                if (disk.GetStorageType() == KHardDiskStorageType_VirtualDiskImage &&
                    disk.GetParent().isNull() && /* must not be differencing (see below) */
                    item->status() == VBoxMedia::Ok)
                {
                    int rc = vboxProblem().confirmHardDiskImageDeletion (this, src);
                    if (rc == QIMessageBox::Cancel)
                        return;
                    deleteImage = rc == QIMessageBox::Yes;
                }
                else
                {
                    /// @todo note that differencing images are always automatically
                    /// deleted when unregistered, but the following message box
                    /// doesn't mention it. I keep it as is for now because
                    /// implementing the portability feature will most likely change
                    /// this behavior (we'll update the message afterwards).
                    if (!vboxProblem().confirmHardDiskUnregister (this, src))
                        return;
                }

                CHardDisk hd = vbox.UnregisterHardDisk (uuid);
                if (!vbox.isOk())
                    vboxProblem().cannotUnregisterMedia (this, vbox, type, src);
                else if (deleteImage)
                {
                    /// @todo When creation of VMDK is implemented, we should
                    /// enable image deletion for  them as well (use
                    /// GetStorageType() to define the correct cast).
                    CVirtualDiskImage vdi = CUnknown (hd);
                    if (vdi.isOk())
                        vdi.DeleteImage();
                    if (!vdi.isOk())
                        vboxProblem().cannotDeleteHardDiskImage (this, vdi);
                }
                break;
            }
        case VBoxDefs::CD:
            vbox.UnregisterDVDImage (uuid);
            break;
        case VBoxDefs::FD:
            vbox.UnregisterFloppyImage (uuid);
            break;
        default:
            AssertMsgFailed (("Selected tree should be equal to one item in VBoxDefs::DiskType.\n"));
            break;
    }

    if (vbox.isOk())
        vboxGlobal().removeMedia (type, uuid);
    else
        vboxProblem().cannotUnregisterMedia (this, vbox, type, src);
}

void VBoxDiskImageManagerDlg::releaseImage()
{
    QTreeWidget *tree = currentTreeWidget();
    DiskImageItem *item = toDiskImageItem (tree->currentItem());
    AssertMsg (item, ("Current item must not be null\n"));

    CVirtualBox vbox = vboxGlobal().virtualBox();

    QUuid itemId = item->uuid();
    AssertMsg (!itemId.isNull(), ("Current item must have uuid\n"));

    switch (currentTreeWidgetType())
    {
        /* If it is a hard disk sub-item: */
        case VBoxDefs::HD:
            {
                CHardDisk hd = item->media().disk;
                QUuid machineId = hd.GetMachineId();
                if (vboxProblem().confirmReleaseImage (this,
                                                       vbox.GetMachine (machineId).GetName()))
                {
                    releaseDisk (machineId, itemId, VBoxDefs::HD);
                    VBoxMedia media (item->media());
                    media.status = hd.GetAccessible() ? VBoxMedia::Ok :
                        hd.isOk() ? VBoxMedia::Inaccessible :
                        VBoxMedia::Error;
                    vboxGlobal().updateMedia (media);
                }
                break;
            }
        /* If it is a cd/dvd sub-item: */
        case VBoxDefs::CD:
            {
                QString usage = item->totalUsage();
                if (vboxProblem().confirmReleaseImage (this, usage))
                {
                    QStringList permMachines =
                        vbox.GetDVDImageUsage (itemId,
                                               KResourceUsage_Permanent).split (' ', QString::SkipEmptyParts);
                    for (QStringList::Iterator it = permMachines.begin();
                         it != permMachines.end(); ++it)
                        releaseDisk (QUuid (*it), itemId, VBoxDefs::CD);

                    CDVDImage cd = vbox.GetDVDImage (itemId);
                    VBoxMedia media (item->media());
                    media.status = cd.GetAccessible() ? VBoxMedia::Ok :
                        cd.isOk() ? VBoxMedia::Inaccessible :
                        VBoxMedia::Error;
                    vboxGlobal().updateMedia (media);
                }
            }
        /* If it is a floppy sub-item: */
        case VBoxDefs::FD:
            {
                QString usage = item->totalUsage();
                if (vboxProblem().confirmReleaseImage (this, usage))
                {
                    QStringList permMachines =
                        vbox.GetFloppyImageUsage (itemId,
                                                  KResourceUsage_Permanent).split (' ', QString::SkipEmptyParts);
                    for (QStringList::Iterator it = permMachines.begin();
                         it != permMachines.end(); ++it)
                        releaseDisk (QUuid (*it), itemId, VBoxDefs::FD);

                    CFloppyImage fd = vbox.GetFloppyImage (itemId);
                    VBoxMedia media (item->media());
                    media.status = fd.GetAccessible() ? VBoxMedia::Ok :
                        fd.isOk() ? VBoxMedia::Inaccessible :
                        VBoxMedia::Error;
                    vboxGlobal().updateMedia (media);
                }
            }
        default:
            AssertMsgFailed (("Selected tree should be equal to one item in VBoxDefs::DiskType.\n"));
            break;
    }
}

void VBoxDiskImageManagerDlg::releaseDisk (const QUuid &aMachineId,
                                           const QUuid &aItemId,
                                           VBoxDefs::DiskType aDiskType)
{
    CSession session;
    CMachine machine;
    /* Is this media image mapped to this VM: */
    if (!mMachine.isNull() && mMachine.GetId() == aMachineId)
        machine = mMachine;
    /* Or some other: */
    else
    {
        session = vboxGlobal().openSession (aMachineId);
        if (session.isNull()) return;
        machine = session.GetMachine();
    }
    /* Perform disk releasing: */
    switch (aDiskType)
    {
        case VBoxDefs::HD:
        {
            /* Releasing hd: */
            CHardDiskAttachmentEnumerator en =
                machine.GetHardDiskAttachments().Enumerate();
            while (en.HasMore())
            {
                CHardDiskAttachment hda = en.GetNext();
                if (hda.GetHardDisk().GetId() == aItemId)
                {
                    machine.DetachHardDisk (hda.GetBus(),
                                            hda.GetChannel(),
                                            hda.GetDevice());
                    if (!machine.isOk())
                        vboxProblem().cannotDetachHardDisk (this,
                            machine, hda.GetBus(), hda.GetChannel(), hda.GetDevice());
                    break;
                }
            }
            break;
        }
        case VBoxDefs::CD:
        {
            /* Releasing cd: */
            machine.GetDVDDrive().Unmount();
            break;
        }
        case VBoxDefs::FD:
        {
            /* Releasing fd: */
            machine.GetFloppyDrive().Unmount();
            break;
        }
        default:
            AssertMsgFailed (("Incorrect disk type."));
    }
    /* Save all setting changes: */
    machine.SaveSettings();
    if (!machine.isOk())
        vboxProblem().cannotSaveMachineSettings (machine);
    /* If local session was opened - close this session: */
    if (!session.isNull())
        session.Close();
}

void VBoxDiskImageManagerDlg::accept() 
{ 
    done (Accepted); 
}

void VBoxDiskImageManagerDlg::reject() 
{ 
    done (Rejected); 
}

void VBoxDiskImageManagerDlg::done (int aResult)
{
    /* Set the final result */
    setResult (aResult);
    /* Hide this window */
    hide();
    /* And close the window */
    close();
}

int VBoxDiskImageManagerDlg::result() const 
{ 
    return mRescode; 
}

void VBoxDiskImageManagerDlg::setResult (int aRescode) 
{ 
    mRescode = aRescode; 
}

QTreeWidget* VBoxDiskImageManagerDlg::treeWidget (VBoxDefs::DiskType aType) const
{
    QTreeWidget* tree = NULL;
    switch (aType)
    {
        case VBoxDefs::HD:
            tree = mHdsTree;
            break;
        case VBoxDefs::CD:
            tree = mCdsTree;
            break;
        case VBoxDefs::FD:
            tree = mFdsTree;
            break;
        default:
            AssertMsgFailed (("Disk type %d unknown!\n", aType));
            break;
    }
    return tree;
}

VBoxDefs::DiskType VBoxDiskImageManagerDlg::currentTreeWidgetType() const
{
    VBoxDefs::DiskType type = VBoxDefs::InvalidType;
    switch (twImages->currentIndex ())
    {
        case HDTab: 
            type = VBoxDefs::HD;
            break;
        case CDTab: 
            type = VBoxDefs::CD;
            break;
        case FDTab: 
            type = VBoxDefs::FD;
            break;
        default:
            AssertMsgFailed (("Page type %d unknown!\n", twImages->currentIndex ()));
            break;
    }
    return type;
}

QTreeWidget* VBoxDiskImageManagerDlg::currentTreeWidget() const
{
    return treeWidget (currentTreeWidgetType());
}


QTreeWidgetItem *VBoxDiskImageManagerDlg::selectedItem (const QTreeWidget *aTree) const
{
    /* Return the current selected item. The user can select one item at the
     * time only, so return the first element always. */
    QList<QTreeWidgetItem *> selItems = aTree->selectedItems();
    if (!selItems.isEmpty())
        return selItems.first();
    else
        return NULL;
}


DiskImageItem *VBoxDiskImageManagerDlg::toDiskImageItem (QTreeWidgetItem *aItem) const
{
    /* Convert the QTreeWidgetItem to a DiskImageItem if it is valid. */
    DiskImageItem *item = NULL;
    if (aItem &&
        aItem->type() == DiskImageItem::TypeId)
        item = static_cast <DiskImageItem *> (aItem);

    return item;
}

void VBoxDiskImageManagerDlg::setCurrentItem (QTreeWidget *aTree,
                                              QTreeWidgetItem *aItem)
{
    if (aTree && aItem)
    {
        aItem->setSelected (true);
        aTree->setCurrentItem (aItem);
        aTree->scrollToItem (aItem, QAbstractItemView::EnsureVisible);
    }
}

void VBoxDiskImageManagerDlg::processCurrentChanged (int /* index = -1 */)
{
    QTreeWidget *tree = currentTreeWidget();
    tree->setFocus();

    /* Tab stop setup */
    setTabOrder (mHdsTree, mHdsPane1);
    setTabOrder (mHdsPane1, mHdsPane2);
    setTabOrder (mHdsPane2, mHdsPane3);
    setTabOrder (mHdsPane3, mHdsPane4);
    setTabOrder (mHdsPane4, mHdsPane5);

    setTabOrder (mCdsTree, mCdsPane1);
    setTabOrder (mCdsPane1, mCdsPane2);

    setTabOrder (mFdsTree, mFdsPane1);
    setTabOrder (mFdsPane1, mFdsPane2);

    processCurrentChanged (tree->currentItem());
}

void VBoxDiskImageManagerDlg::processCurrentChanged (QTreeWidgetItem *aItem, QTreeWidgetItem *aPrevItem /* = NULL */)
{
    DiskImageItem *item = toDiskImageItem (aItem);

    if (!item)
    {
        DiskImageItem *itemOld = toDiskImageItem (aPrevItem);
        /* We have to make sure that one item is selected always. If the new
         * item is 0, set the old item again. */
        setCurrentItem (currentTreeWidget(), itemOld);
        return;
    }

    /* Ensures current item visible every time we are switching page */
    item->treeWidget()->scrollToItem (item, QAbstractItemView::EnsureVisible);

    bool notInEnum      = !vboxGlobal().isMediaEnumerationStarted();
    bool modifyEnabled  = notInEnum &&
                          item &&  item->usage().isNull() &&
                          (item->childCount() == 0) && !item->path().isNull();
    bool releaseEnabled = item && !item->usage().isNull() &&
                          item->snapshotUsage().isNull() &&
                          checkImage (item) &&
                          !item->parent() && (item->childCount() == 0) &&
                          item->snapshotName().isNull();
    bool newEnabled     = notInEnum &&
                          currentTreeWidget() == mHdsTree ? true : false;
    bool addEnabled     = notInEnum;

    // mEditAction->setEnabled (modifyEnabled);
    mRemoveAction->setEnabled (modifyEnabled);
    mReleaseAction->setEnabled (releaseEnabled);
    mNewAction->setEnabled (newEnabled);
    mAddAction->setEnabled (addEnabled);

    if (mDoSelect)
    {
        bool selectEnabled = item && !item->parent() &&
                             (!newEnabled ||
                                (item->usage().isNull() ||
                                 item->machineId() == mTargetVMId));

        mButtonBox->button (QDialogButtonBox::Ok)->setEnabled (selectEnabled);
    }

    if (item)
    {
        if (item->treeWidget() == mHdsTree)
        {
            mHdsPane1->setText (item->information (item->path(), true, "end"));
            mHdsPane2->setText (item->information (item->diskType(), false));
            mHdsPane3->setText (item->information (item->storageType(), false));
            mHdsPane4->setText (item->information (item->usage()));
            mHdsPane5->setText (item->information (item->snapshotName()));
        }
        else if (item->treeWidget() == mCdsTree)
        {
            mCdsPane1->setText (item->information (item->path(), true, "end"));
            mCdsPane2->setText (item->information (item->totalUsage()));
        }
        else if (item->treeWidget() == mFdsTree)
        {
            mFdsPane1->setText (item->information (item->path(), true, "end"));
            mFdsPane2->setText (item->information (item->totalUsage()));
        }
    }
    else
        clearInfoPanes();
}

void VBoxDiskImageManagerDlg::processDoubleClick (QTreeWidgetItem * /* aItem */, int /* aColumn */)
{
    QTreeWidget *tree = currentTreeWidget();

    if (mDoSelect && selectedItem (tree) && mButtonBox->button (QDialogButtonBox::Ok)->isEnabled())
        accept();
}

void VBoxDiskImageManagerDlg::processRightClick (const QPoint &aPos, QTreeWidgetItem *aItem, int /* aColumn */)
{
    if (aItem)
    {
        /* Make sure the item is selected and current */
        setCurrentItem (currentTreeWidget(), aItem);
        mActionsContextMenu->exec (aPos);
    }
}

void VBoxDiskImageManagerDlg::addImageToList (const QString &aSource,
                                              VBoxDefs::DiskType aDiskType)
{
    if (aSource.isEmpty())
        return;

    CVirtualBox vbox = vboxGlobal().virtualBox();

    QUuid uuid;
    VBoxMedia media;
    switch (aDiskType)
    {
        case VBoxDefs::HD:
        {
            CHardDisk hd = vbox.OpenHardDisk (aSource);
            if (vbox.isOk())
            {
                vbox.RegisterHardDisk (hd);
                if (vbox.isOk())
                {
                    VBoxMedia::Status status =
                        hd.GetAccessible() ? VBoxMedia::Ok :
                        hd.isOk() ? VBoxMedia::Inaccessible :
                        VBoxMedia::Error;
                    media = VBoxMedia (CUnknown (hd), VBoxDefs::HD, status);
                }
            }
            break;
        }
        case VBoxDefs::CD:
        {
            CDVDImage cd = vbox.OpenDVDImage (aSource, uuid);
            if (vbox.isOk())
            {
                vbox.RegisterDVDImage (cd);
                if (vbox.isOk())
                {
                    VBoxMedia::Status status =
                        cd.GetAccessible() ? VBoxMedia::Ok :
                        cd.isOk() ? VBoxMedia::Inaccessible :
                        VBoxMedia::Error;
                    media = VBoxMedia (CUnknown (cd), VBoxDefs::CD, status);
                }
            }
            break;
        }
        case VBoxDefs::FD:
        {
            CFloppyImage fd = vbox.OpenFloppyImage (aSource, uuid);
            if (vbox.isOk())
            {
                vbox.RegisterFloppyImage (fd);
                if (vbox.isOk())
                {
                    VBoxMedia::Status status =
                        fd.GetAccessible() ? VBoxMedia::Ok :
                        fd.isOk() ? VBoxMedia::Inaccessible :
                        VBoxMedia::Error;
                    media = VBoxMedia (CUnknown (fd), VBoxDefs::FD, status);
                }
            }
            break;
        }
        default:
            AssertMsgFailed (("Invalid aDiskType type\n"));
    }
    if (media.type != VBoxDefs::InvalidType)
        vboxGlobal().addMedia (media);
}

DiskImageItem* VBoxDiskImageManagerDlg::createImageNode (QTreeWidget *aTree,
                                                         DiskImageItem *aRoot,
                                                         const VBoxMedia &aMedia) const
{
    Assert (!(aTree == NULL && aRoot == NULL));

    DiskImageItem *item = NULL;

    if (aRoot)
        item = new DiskImageItem (aRoot);
    else if (aTree)
        item = new DiskImageItem (aTree);

    item->setMedia (aMedia);

    return item;
}

DiskImageItem* VBoxDiskImageManagerDlg::createHdItem (QTreeWidget *aTree,
                                                      const VBoxMedia &aMedia) const
{
    CHardDisk hd = aMedia.disk;
    QUuid rootId = hd.GetParent().isNull() ? QUuid() : hd.GetParent().GetId();
    DiskImageItem *root = searchItem (aTree, rootId);
    DiskImageItem *item = createImageNode (aTree, root, aMedia);
    updateHdItem (item, aMedia);
    return item;
}

DiskImageItem* VBoxDiskImageManagerDlg::createCdItem (QTreeWidget *aTree,
                                                      const VBoxMedia &aMedia) const
{
    DiskImageItem *item = createImageNode (aTree, 0, aMedia);
    updateCdItem (item, aMedia);
    return item;
}

DiskImageItem* VBoxDiskImageManagerDlg::createFdItem (QTreeWidget *aTree,
                                                      const VBoxMedia &aMedia) const
{
    DiskImageItem *item = createImageNode (aTree, 0, aMedia);
    updateFdItem (item, aMedia);
    return item;
}

void VBoxDiskImageManagerDlg::updateHdItem (DiskImageItem *aItem,
                                            const VBoxMedia &aMedia) const
{
    if (!aItem)
        return;

    CHardDisk hd = aMedia.disk;
    VBoxMedia::Status status = aMedia.status;

    QUuid uuid = hd.GetId();
    QString src = hd.GetLocation();
    QUuid machineId = hd.GetMachineId();
    QString usage;
    if (!machineId.isNull())
        usage = vboxGlobal().virtualBox().GetMachine (machineId).GetName();
    QString storageType = vboxGlobal().toString (hd.GetStorageType());
    QString hardDiskType = vboxGlobal().hardDiskTypeString (hd);
    QString virtualSize = status == VBoxMedia::Ok ?
        vboxGlobal().formatSize ((ULONG64)hd.GetSize() * _1M) : QString ("--");
    QString actualSize = status == VBoxMedia::Ok ?
        vboxGlobal().formatSize (hd.GetActualSize()) : QString ("--");
    QString snapshotName;
    if (!machineId.isNull() && !hd.GetSnapshotId().isNull())
    {
        CSnapshot snapshot = vboxGlobal().virtualBox().GetMachine (machineId).
                                  GetSnapshot (hd.GetSnapshotId());
        if (!snapshot.isNull())
            snapshotName = QString ("%1").arg (snapshot.GetName());
    }
    QFileInfo fi (src);

    aItem->setText (0, fi.fileName());
    aItem->setText (1, virtualSize);
    aItem->setTextAlignment (1, Qt::AlignRight);
    aItem->setText (2, actualSize);
    aItem->setTextAlignment (2, Qt::AlignRight);
    aItem->setPath (hd.GetStorageType() == KHardDiskStorageType_ISCSIHardDisk ? src :
                    QDir::convertSeparators (fi.absoluteFilePath()));
    aItem->setUsage (usage);
    aItem->setSnapshotName (snapshotName);
    aItem->setDiskType (hardDiskType);
    aItem->setStorageType (storageType);
    aItem->setVirtualSize (virtualSize);
    aItem->setActualSize (actualSize);
    aItem->setUuid (uuid);
    aItem->setMachineId (machineId);
    aItem->setToolTip (composeHdToolTip (hd, status, aItem));
    aItem->setStatus (status);

    makeWarningMark (aItem, aMedia.status, VBoxDefs::HD);
}

void VBoxDiskImageManagerDlg::updateCdItem (DiskImageItem *aItem,
                                            const VBoxMedia &aMedia) const
{
    if (!aItem)
        return;

    CDVDImage cd = aMedia.disk;
    VBoxMedia::Status status = aMedia.status;

    QUuid uuid = cd.GetId();
    QString src = cd.GetFilePath();
    QString snapshotUsage;
    QString usage = DVDImageUsage (uuid, snapshotUsage);
    QString size = status == VBoxMedia::Ok ?
        vboxGlobal().formatSize (cd.GetSize()) : QString ("--");
    QFileInfo fi (src);

    aItem->setText (0, fi.fileName());
    aItem->setText (1, size);
    aItem->setTextAlignment (1, Qt::AlignRight);
    aItem->setPath (QDir::convertSeparators (fi.absoluteFilePath ()));
    aItem->setUsage (usage);
    aItem->setSnapshotUsage (snapshotUsage);
    aItem->setActualSize (size);
    aItem->setUuid (uuid);
    aItem->setToolTip (composeCdToolTip (cd, status, aItem));
    aItem->setStatus (status);

    makeWarningMark (aItem, aMedia.status, VBoxDefs::CD);
}

void VBoxDiskImageManagerDlg::updateFdItem (DiskImageItem *aItem,
                                            const VBoxMedia &aMedia) const
{
    if (!aItem)
        return;

    CFloppyImage fd = aMedia.disk;
    VBoxMedia::Status status = aMedia.status;

    QUuid uuid = fd.GetId();
    QString src = fd.GetFilePath();
    QString snapshotUsage;
    QString usage = FloppyImageUsage (uuid, snapshotUsage);
    QString size = status == VBoxMedia::Ok ?
        vboxGlobal().formatSize (fd.GetSize()) : QString ("--");
    QFileInfo fi (src);

    aItem->setText (0, fi.fileName());
    aItem->setText (1, size);
    aItem->setTextAlignment (1, Qt::AlignRight);
    aItem->setPath (QDir::convertSeparators (fi.absoluteFilePath ()));
    aItem->setUsage (usage);
    aItem->setSnapshotUsage (snapshotUsage);
    aItem->setActualSize (size);
    aItem->setUuid (uuid);
    aItem->setToolTip (composeFdToolTip (fd, status, aItem));
    aItem->setStatus (status);

    makeWarningMark (aItem, aMedia.status, VBoxDefs::FD);
}

DiskImageItem* VBoxDiskImageManagerDlg::searchItem (QTreeWidget *aTree,
                                                    const QUuid &aId) const
{
    if (aId.isNull()) 
        return NULL;

    DiskImageItemIterator iterator (aTree);
    while (*iterator)
    {
        if ((*iterator)->uuid() == aId)
            return *iterator;
        ++iterator;
    }
    return NULL;
}

DiskImageItem* VBoxDiskImageManagerDlg::searchItem (QTreeWidget *aTree,
                                                    VBoxMedia::Status aStatus) const
{
    DiskImageItemIterator iterator (aTree);
    while (*iterator)
    {
        if ((*iterator)->status() == aStatus)
            return *iterator;
        ++iterator;
    }
    return NULL;
}

bool VBoxDiskImageManagerDlg::checkImage (DiskImageItem *aItem)
{
    QUuid itemId = aItem ? aItem->uuid() : QUuid();
    if (itemId.isNull()) 
        return false;

    CVirtualBox vbox = vboxGlobal().virtualBox();

    switch (currentTreeWidgetType())
    {
        case VBoxDefs::HD:
            {
                CHardDisk hd = aItem->media().disk;
                QUuid machineId = hd.GetMachineId();
                if (machineId.isNull() ||
                    (vbox.GetMachine (machineId).GetState() != KMachineState_PoweredOff &&
                     vbox.GetMachine (machineId).GetState() != KMachineState_Aborted))
                    return false;
                break;
            }
        case VBoxDefs::CD:
            {
                /* Check if there is temporary usage: */
                QStringList tempMachines =
                    vbox.GetDVDImageUsage (itemId,
                                           KResourceUsage_Temporary).split (' ', QString::SkipEmptyParts);
                if (!tempMachines.isEmpty())
                    return false;
                /* Only permanently mounted .iso could be released */
                QStringList permMachines =
                    vbox.GetDVDImageUsage (itemId,
                                           KResourceUsage_Permanent).split (' ', QString::SkipEmptyParts);
                for (QStringList::Iterator it = permMachines.begin();
                     it != permMachines.end(); ++it)
                    if (vbox.GetMachine(QUuid (*it)).GetState() != KMachineState_PoweredOff &&
                        vbox.GetMachine(QUuid (*it)).GetState() != KMachineState_Aborted)
                        return false;
                break;
            }
        case VBoxDefs::FD:
            {
                /* Check if there is temporary usage: */
                QStringList tempMachines =
                    vbox.GetFloppyImageUsage (itemId,
                                              KResourceUsage_Temporary).split (' ', QString::SkipEmptyParts);
                if (!tempMachines.isEmpty())
                    return false;
                /* Only permanently mounted floppies could be released */
                QStringList permMachines =
                    vbox.GetFloppyImageUsage (itemId,
                                              KResourceUsage_Permanent).split (' ', QString::SkipEmptyParts);
                for (QStringList::Iterator it = permMachines.begin();
                     it != permMachines.end(); ++it)
                    if (vbox.GetMachine(QUuid (*it)).GetState() != KMachineState_PoweredOff &&
                        vbox.GetMachine(QUuid (*it)).GetState() != KMachineState_Aborted)
                        return false;
                break;
            }
        default:
            {
                return false;
                break;
            }
    }
    return true;
}

bool VBoxDiskImageManagerDlg::checkDndUrls (const QList<QUrl> &aUrls) const
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
            case VBoxDefs::HD: 
                err |= (!(suffix == "vdi" || suffix == "vmdk")); 
                break;
            case VBoxDefs::CD: 
                err |= (suffix != "iso"); 
                break;
            case VBoxDefs::FD: 
                err |= (suffix != "img"); 
                break;
            default:
                AssertMsgFailed (("Selected tree should be equal to one item in VBoxDefs::DiskType.\n"));
                break;
        }
    }
    return !err;
}

void VBoxDiskImageManagerDlg::addDndUrls (const QList<QUrl> &aUrls)
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    foreach (QUrl u, aUrls)
    {
        QString file = u.toLocalFile();
        VBoxDefs::DiskType type = currentTreeWidgetType();
        addImageToList (file, type);
        if (!vbox.isOk())
            vboxProblem().cannotRegisterMedia (this, vbox, type, file);
    }
}

void VBoxDiskImageManagerDlg::clearInfoPanes()
{
    mHdsPane1->clear();
    mHdsPane2->clear(); mHdsPane3->clear();
    mHdsPane4->clear(); mHdsPane5->clear();
    mCdsPane1->clear(); mCdsPane2->clear();
    mFdsPane1->clear(); mFdsPane2->clear();
}

void VBoxDiskImageManagerDlg::prepareToRefresh (int aTotal)
{
    /* Info panel clearing */
    clearInfoPanes();

    /* Prepare progressbar */
//    if (mProgressBar)
//    {
//        mProgressBar->setProgress (0, aTotal);
//        mProgressBar->setHidden (false);
//        mProgressText->setHidden (false);
//    }

    mRefreshAction->setEnabled (false);
    setCursor (QCursor (Qt::BusyCursor));

    /* Store the current list selections */
    DiskImageItem *di;

    di = toDiskImageItem (mHdsTree->currentItem());
    if (mHdSelectedId.isNull())
        mHdSelectedId = di ? di->uuid() : QUuid();

    di = toDiskImageItem (mCdsTree->currentItem());
    if (mCdSelectedId.isNull())
        mCdSelectedId = di ? di->uuid() : QUuid();

    di = toDiskImageItem (mFdsTree->currentItem());
    if (mFdSelectedId.isNull())
        mFdSelectedId = di ? di->uuid() : QUuid();

    /* Finally, clear all lists */
    mHdsTree->clear();
    mCdsTree->clear();
    mFdsTree->clear();
}

void VBoxDiskImageManagerDlg::createInfoString (InfoPaneLabel *&aInfo,
                                                QWidget *aRoot,
                                                int aRow, int aCol,
                                                int aRowSpan /* = 1 */, int aColSpan /* = 1 */) const
{
    /* Create the label & the info pane itself */
    QLabel *nameLabel = new QLabel (aRoot);
    aInfo = new InfoPaneLabel (aRoot, nameLabel);

    /* Setup focus policy <strong> default for info pane */
    aInfo->setFocusPolicy (Qt::StrongFocus);

    /* Prevent the name columns from being expanded */
    nameLabel->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);

    QGridLayout *rootLayout = qobject_cast<QGridLayout*> (aRoot->layout());
    Assert (VALID_PTR (rootLayout));

    /* Add the two widgets */
    rootLayout->addWidget (nameLabel, aRow, aCol, 1, 1);
    rootLayout->addWidget (aInfo, aRow, aCol + 1, aRowSpan, aColSpan);
}

void VBoxDiskImageManagerDlg::makeWarningMark (DiskImageItem *aItem,
                                               VBoxMedia::Status aStatus,
                                               VBoxDefs::DiskType aType) const
{
    const QIcon &icon = aStatus == VBoxMedia::Inaccessible ? mIconInaccessible :
                        aStatus == VBoxMedia::Error ? mIconErroneous : QIcon();

    if (!icon.isNull())
    {
        aItem->setIcon (0, icon);
        treeWidget (aType);
        int index = aType == VBoxDefs::HD ? HDTab :
                    aType == VBoxDefs::CD ? CDTab :
                    aType == VBoxDefs::FD ? FDTab : -1;
        Assert (index != -1); /* aType should be correct */
        twImages->setTabIcon (index, icon);
        aItem->treeWidget()->scrollToItem (aItem, QAbstractItemView::EnsureVisible);
    }
}

QString VBoxDiskImageManagerDlg::DVDImageUsage (const QUuid &aId, QString &aSnapshotUsage)
{
    CVirtualBox vbox = vboxGlobal().virtualBox();

    QStringList permMachines =
        vbox.GetDVDImageUsage (aId, KResourceUsage_Permanent).split (' ', QString::SkipEmptyParts);
    QStringList tempMachines =
        vbox.GetDVDImageUsage (aId, KResourceUsage_Temporary).split (' ', QString::SkipEmptyParts);

    QString usage;

    for (QStringList::Iterator it = permMachines.begin();
         it != permMachines.end();
         ++it)
    {
        if (!usage.isEmpty())
            usage += ", ";
        CMachine machine = vbox.GetMachine (QUuid (*it));
        usage += machine.GetName();

        DVDImageSnapshotUsage (aId, machine.GetSnapshot (QUuid()),
                               aSnapshotUsage);
    }

    for (QStringList::Iterator it = tempMachines.begin();
         it != tempMachines.end();
         ++it)
    {
        /* Skip IDs that are in the permanent list */
        if (!permMachines.contains (*it))
        {
            if (!usage.isEmpty())
                usage += ", [";
            else
                usage += "[";
            CMachine machine = vbox.GetMachine (QUuid (*it));
            usage += machine.GetName() + "]";

            DVDImageSnapshotUsage (aId, machine.GetSnapshot (QUuid()),
                                   aSnapshotUsage);
        }
    }

    return usage;
}

QString VBoxDiskImageManagerDlg::FloppyImageUsage (const QUuid &aId, QString &aSnapshotUsage)
{
    CVirtualBox vbox = vboxGlobal().virtualBox();

    QStringList permMachines =
        vbox.GetFloppyImageUsage (aId, KResourceUsage_Permanent).split (' ', QString::SkipEmptyParts);
    QStringList tempMachines =
        vbox.GetFloppyImageUsage (aId, KResourceUsage_Temporary).split (' ', QString::SkipEmptyParts);

    QString usage;

    for (QStringList::Iterator it = permMachines.begin();
         it != permMachines.end();
         ++it)
    {
        if (!usage.isEmpty())
            usage += ", ";
        CMachine machine = vbox.GetMachine (QUuid (*it));
        usage += machine.GetName();

        FloppyImageSnapshotUsage (aId, machine.GetSnapshot (QUuid()),
                                  aSnapshotUsage);
    }

    for (QStringList::Iterator it = tempMachines.begin();
         it != tempMachines.end();
         ++it)
    {
        /* Skip IDs that are in the permanent list */
        if (!permMachines.contains (*it))
        {
            if (!usage.isEmpty())
                usage += ", [";
            else
                usage += "[";
            CMachine machine = vbox.GetMachine (QUuid (*it));
            usage += machine.GetName() + "]";

            FloppyImageSnapshotUsage (aId, machine.GetSnapshot (QUuid()),
                                      aSnapshotUsage);
        }
    }

    return usage;
}

void VBoxDiskImageManagerDlg::DVDImageSnapshotUsage (const QUuid &aId, const CSnapshot &aSnapshot, QString &aUsage)
{
    if (aSnapshot.isNull())
        return;

    if (!aSnapshot.GetMachine().GetDVDDrive().GetImage().isNull() &&
        aSnapshot.GetMachine().GetDVDDrive().GetImage().GetId() == aId)
    {
        if (!aUsage.isEmpty())
            aUsage += ", ";
        aUsage += aSnapshot.GetName();
    }

    CSnapshotEnumerator en = aSnapshot.GetChildren().Enumerate();
    while (en.HasMore())
        DVDImageSnapshotUsage (aId, en.GetNext(), aUsage);
}

void VBoxDiskImageManagerDlg::FloppyImageSnapshotUsage (const QUuid &aId, const CSnapshot &aSnapshot, QString &aUsage)
{
    if (aSnapshot.isNull())
        return;

    if (!aSnapshot.GetMachine().GetFloppyDrive().GetImage().isNull() &&
        aSnapshot.GetMachine().GetFloppyDrive().GetImage().GetId() == aId)
    {
        if (!aUsage.isEmpty())
            aUsage += ", ";
        aUsage += aSnapshot.GetName();
    }

    CSnapshotEnumerator en = aSnapshot.GetChildren().Enumerate();
    while (en.HasMore())
        FloppyImageSnapshotUsage (aId, en.GetNext(), aUsage);
}

