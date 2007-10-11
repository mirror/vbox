/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * "Virtual Disk Manager" dialog UI include (Qt Designer)
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you wish to add, delete or rename functions or slots use
** Qt Designer which will update this file, preserving your code. Create an
** init() function in place of a constructor, and a destroy() function in
** place of a destructor.
*****************************************************************************/


class DiskImageItem : public QListViewItem
{
public:

    enum { TypeId = 1001 };

    DiskImageItem (DiskImageItem *parent) :
        QListViewItem (parent), mStatus (VBoxMedia::Unknown) {}

    DiskImageItem (QListView *parent) :
        QListViewItem (parent), mStatus (VBoxMedia::Unknown) {}

    void setMedia (const VBoxMedia &aMedia) { mMedia = aMedia; }
    VBoxMedia &getMedia() { return mMedia; }

    QString getName() { return mName; }

    void setPath (QString aPath) { mPath = aPath; }
    const QString &getPath() { return mPath; }

    void setUsage (QString aUsage) { mUsage = aUsage; }
    const QString &getUsage() { return mUsage; }

    void setSnapshotName (QString aSnapshotName) { mSnapshotName = aSnapshotName; }
    const QString &getSnapshotName() { return mSnapshotName; }

    void setDiskType (QString aDiskType) { mDiskType = aDiskType; }
    const QString &getDiskType() { return mDiskType; }

    void setStorageType (QString aStorageType) { mStorageType = aStorageType; }
    const QString &getStorageType() { return mStorageType; }

    void setVirtualSize (QString aVirtualSize) { mVirtualSize = aVirtualSize; }
    const QString &getVirtualSize() { return mVirtualSize; }

    void setActualSize (QString aActualSize) { mActualSize = aActualSize; }
    const QString &getActualSize() { return mActualSize; }


    void setUuid (QUuid aUuid) { mUuid = aUuid; }
    const QString &getUuid() { return mUuid; }

    void setMachineId (QString aMachineId) { mMachineId = aMachineId; }
    const QString &getMachineId() { return mMachineId; }


    void setStatus (VBoxMedia::Status aStatus) { mStatus = aStatus; }
    VBoxMedia::Status getStatus() { return mStatus; }


    void setToolTip (QString aToolTip) { mToolTip = aToolTip; }
    const QString &getToolTip() { return mToolTip; }

    QString getInformation (const QString &aInfo, bool aCompact = true,
                            const QString &aElipsis = "middle")
    {
        QString compactString = QString ("<compact elipsis=\"%1\">").arg (aElipsis);
        QString info = QString ("<nobr>%1%2%3</nobr>")
                       .arg (aCompact ? compactString : "")
                       .arg (aInfo.isEmpty() ?
                             VBoxDiskImageManagerDlg::tr ("--", "no info") :
                             aInfo)
                       .arg (aCompact ? "</compact>" : "");
        return info;
    }

    int rtti() const { return TypeId; }

    int compare (QListViewItem *aItem, int aColumn, bool aAscending) const
    {
        ULONG64 thisValue = vboxGlobal().parseSize (       text (aColumn));
        ULONG64 thatValue = vboxGlobal().parseSize (aItem->text (aColumn));
        if (thisValue && thatValue)
        {
            if (thisValue == thatValue)
                return 0;
            else
                return thisValue > thatValue ? 1 : -1;
        }
        else
            return QListViewItem::compare (aItem, aColumn, aAscending);
    }

    DiskImageItem* nextSibling() const
    {
        return (QListViewItem::nextSibling() &&
                QListViewItem::nextSibling()->rtti() == DiskImageItem::TypeId) ?
                static_cast<DiskImageItem*> (QListViewItem::nextSibling()) : 0;
    }

    void paintCell (QPainter *aPainter, const QColorGroup &aColorGroup,
                    int aColumn, int aWidth, int aSlign)
    {
        QColorGroup cGroup (aColorGroup);
        if (mStatus == VBoxMedia::Unknown)
            cGroup.setColor (QColorGroup::Text, cGroup.mid());
        QListViewItem::paintCell (aPainter, cGroup, aColumn, aWidth, aSlign);
    }

protected:

    VBoxMedia mMedia;

    QString mName;
    QString mPath;
    QString mUsage;
    QString mSnapshotName;
    QString mDiskType;
    QString mStorageType;
    QString mVirtualSize;
    QString mActualSize;

    QString mUuid;
    QString mMachineId;

    QString mToolTip;

    VBoxMedia::Status mStatus;
};


class DiskImageItemIterator : public QListViewItemIterator
{
public:

    DiskImageItemIterator (QListView* aList)
        : QListViewItemIterator (aList) {}

    DiskImageItem* operator*()
    {
        QListViewItem *item = QListViewItemIterator::operator*();
        return item && item->rtti() == DiskImageItem::TypeId ?
            static_cast<DiskImageItem*> (item) : 0;
    }

    DiskImageItemIterator& operator++()
    {
        return (DiskImageItemIterator&) QListViewItemIterator::operator++();
    }
};


class InfoPaneLabel : public QIRichLabel
{
public:

    InfoPaneLabel (QWidget *aParent, QLabel *aLabel = 0)
        : QIRichLabel (aParent, "infoLabel"), mLabel (aLabel) {}

    QLabel* label() { return mLabel; }

private:

    QLabel *mLabel;
};


VBoxDiskImageManagerDlg *VBoxDiskImageManagerDlg::mModelessDialog = 0;


void VBoxDiskImageManagerDlg::showModeless (bool aRefresh /* = true */)
{
    if (!mModelessDialog)
    {
        mModelessDialog =
            new VBoxDiskImageManagerDlg (NULL,
                                         "VBoxDiskImageManagerDlg",
                                         WType_TopLevel | WDestructiveClose);
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
                                     ~WindowMinimized);
    mModelessDialog->setActiveWindow();
}


void VBoxDiskImageManagerDlg::init()
{
    polished = false;

    mInLoop = false;

    defaultButton = searchDefaultButton();

    vbox = vboxGlobal().virtualBox();
    Assert (!vbox.isNull());

    setIcon (QPixmap::fromMimeSource ("diskim_16px.png"));

    type = VBoxDefs::InvalidType;

    QImage img =
        QMessageBox::standardIcon (QMessageBox::Warning).convertToImage();
    img = img.smoothScale (16, 16);
    pxInaccessible.convertFromImage (img);
    Assert (!pxInaccessible.isNull());

    img =
        QMessageBox::standardIcon (QMessageBox::Critical).convertToImage();
    img = img.smoothScale (16, 16);
    pxErroneous.convertFromImage (img);
    Assert (!pxErroneous.isNull());

    pxHD = VBoxGlobal::iconSet ("hd_16px.png", "hd_disabled_16px.png");
    pxCD = VBoxGlobal::iconSet ("cd_16px.png", "cd_disabled_16px.png");
    pxFD = VBoxGlobal::iconSet ("fd_16px.png", "fd_disabled_16px.png");

    /* setup tab widget icons */
    twImages->setTabIconSet (twImages->page (0), pxHD);
    twImages->setTabIconSet (twImages->page (1), pxCD);
    twImages->setTabIconSet (twImages->page (2), pxFD);

    /* setup image list views */
    hdsView->setColumnAlignment (1, Qt::AlignRight);
    hdsView->setColumnAlignment (2, Qt::AlignRight);
    hdsView->header()->setStretchEnabled (false);
    hdsView->header()->setStretchEnabled (true, 0);

    fdsView->setColumnAlignment (1, Qt::AlignRight);
    fdsView->header()->setStretchEnabled (false);
    fdsView->header()->setStretchEnabled (true, 0);

    cdsView->setColumnAlignment (1, Qt::AlignRight);
    cdsView->header()->setStretchEnabled (false);
    cdsView->header()->setStretchEnabled (true, 0);


    /* setup list-view's item tooltip */
    hdsView->setShowToolTips (false);
    cdsView->setShowToolTips (false);
    fdsView->setShowToolTips (false);
    connect (hdsView, SIGNAL (onItem (QListViewItem*)),
             this, SLOT (mouseOnItem(QListViewItem*)));
    connect (cdsView, SIGNAL (onItem (QListViewItem*)),
             this, SLOT (mouseOnItem(QListViewItem*)));
    connect (fdsView, SIGNAL (onItem (QListViewItem*)),
             this, SLOT (mouseOnItem(QListViewItem*)));


    /* status-bar currently disabled */
    /// @todo we must enable it and disable our size grip hack!
    /// (at least, to have action help text showh)
    statusBar()->setHidden (true);


    /* context menu composing */
    itemMenu = new QPopupMenu (this, "itemMenu");

    imNewAction = new QAction (this, "imNewAction");
    imAddAction = new QAction (this, "imAddAction");
    // imEditAction = new QAction (this, "imEditAction");
    imRemoveAction = new QAction (this, "imRemoveAction");
    imReleaseAction = new QAction (this, "imReleaseAction");
    imRefreshAction = new QAction (this, "imRefreshAction");

    connect (imNewAction, SIGNAL (activated()),
             this, SLOT (newImage()));
    connect (imAddAction, SIGNAL (activated()),
             this, SLOT (addImage()));
    // connect (imEditAction, SIGNAL (activated()),
    //          this, SLOT (editImage()));
    connect (imRemoveAction, SIGNAL (activated()),
             this, SLOT (removeImage()));
    connect (imReleaseAction, SIGNAL (activated()),
             this, SLOT (releaseImage()));
    connect (imRefreshAction, SIGNAL (activated()),
             this, SLOT (refreshAll()));

    imNewAction->setIconSet (VBoxGlobal::iconSetEx (
        "vdm_new_22px.png", "vdm_new_16px.png",
        "vdm_new_disabled_22px.png", "vdm_new_disabled_16px.png"));
    imAddAction->setIconSet (VBoxGlobal::iconSetEx (
        "vdm_add_22px.png", "vdm_add_16px.png",
        "vdm_add_disabled_22px.png", "vdm_add_disabled_16px.png"));
    // imEditAction->setIconSet (VBoxGlobal::iconSet ("guesttools_16px.png", "guesttools_disabled_16px.png"));
    imRemoveAction->setIconSet (VBoxGlobal::iconSetEx (
        "vdm_remove_22px.png", "vdm_remove_16px.png",
        "vdm_remove_disabled_22px.png", "vdm_remove_disabled_16px.png"));
    imReleaseAction->setIconSet (VBoxGlobal::iconSetEx (
        "vdm_release_22px.png", "vdm_release_16px.png",
        "vdm_release_disabled_22px.png", "vdm_release_disabled_16px.png"));
    imRefreshAction->setIconSet (VBoxGlobal::iconSetEx (
        "refresh_22px.png", "refresh_16px.png",
        "refresh_disabled_22px.png", "refresh_disabled_16px.png"));

    // imEditAction->addTo (itemMenu);
    imRemoveAction->addTo (itemMenu);
    imReleaseAction->addTo (itemMenu);


    /* toolbar composing */
    toolBar = new VBoxToolBar (this, centralWidget(), "toolBar");
    toolBar->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Minimum);
    ((QVBoxLayout*)centralWidget()->layout())->insertWidget(0, toolBar);

    toolBar->setUsesTextLabel (true);
    toolBar->setUsesBigPixmaps (true);

    imNewAction->addTo (toolBar);
    imAddAction->addTo (toolBar);
    toolBar->addSeparator();
    // imEditAction->addTo (toolBar);
    imRemoveAction->addTo (toolBar);
    imReleaseAction->addTo (toolBar);
    toolBar->addSeparator();
    imRefreshAction->addTo (toolBar);
#ifdef Q_WS_MAC
    toolBar->setMacStyle();
#endif 


    /* menu bar */
    QPopupMenu *actionMenu = new QPopupMenu (this, "actionMenu");
    imNewAction->addTo    (actionMenu);
    imAddAction->addTo    (actionMenu);
    actionMenu->insertSeparator();
    // imEditAction->addTo (toolBar);
    imRemoveAction->addTo (actionMenu);
    imReleaseAction->addTo (actionMenu);
    actionMenu->insertSeparator();
    imRefreshAction->addTo (actionMenu);
    menuBar()->insertItem (QString::null, actionMenu, 1);


    /* setup size grip */
    sizeGrip = new QSizeGrip (centralWidget(), "sizeGrip");
    sizeGrip->resize (sizeGrip->sizeHint());
    sizeGrip->stackUnder(buttonOk);

    /* setup information pane */
    QApplication::setGlobalMouseTracking (true);
    qApp->installEventFilter (this);
    /* setup information pane layouts */
    QGridLayout *hdsContainerLayout = new QGridLayout (hdsContainer, 4, 4);
    hdsContainerLayout->setMargin (10);
    QGridLayout *cdsContainerLayout = new QGridLayout (cdsContainer, 2, 4);
    cdsContainerLayout->setMargin (10);
    QGridLayout *fdsContainerLayout = new QGridLayout (fdsContainer, 2, 4);
    fdsContainerLayout->setMargin (10);
    /* create info-pane for hd list-view */
    createInfoString (hdsPane1, hdsContainer, 0, -1);
    createInfoString (hdsPane2, hdsContainer, 1, 0);
    createInfoString (hdsPane3, hdsContainer, 1, 1);
    createInfoString (hdsPane4, hdsContainer, 2, 0);
    createInfoString (hdsPane5, hdsContainer, 2, 1);
    /* create info-pane for cd list-view */
    createInfoString (cdsPane1, cdsContainer, 0, -1);
    createInfoString (cdsPane2, cdsContainer, 1, -1);
    /* create info-pane for fd list-view */
    createInfoString (fdsPane1, fdsContainer, 0, -1);
    createInfoString (fdsPane2, fdsContainer, 1, -1);


    /* enumeration progressbar creation */
    mProgressText = new QLabel (centralWidget());
    mProgressText->setHidden (true);
    buttonLayout->insertWidget (2, mProgressText);
    mProgressBar = new QProgressBar (centralWidget());
    mProgressBar->setHidden (true);
    mProgressBar->setFrameShadow (QFrame::Sunken);
    mProgressBar->setFrameShape  (QFrame::Panel);
    mProgressBar->setPercentageVisible (false);
    mProgressBar->setMaximumWidth (100);
    buttonLayout->insertWidget (3, mProgressBar);


    /* applying language settings */
    languageChangeImp();
}


void VBoxDiskImageManagerDlg::languageChangeImp()
{
    imNewAction->setMenuText (tr ("&New..."));
    imAddAction->setMenuText (tr ("&Add..."));
    // imEditAction->setMenuText (tr ("&Edit..."));
    imRemoveAction->setMenuText (tr ("R&emove"));
    imReleaseAction->setMenuText (tr ("Re&lease"));
    imRefreshAction->setMenuText (tr ("Re&fresh"));

    imNewAction->setText (tr ("New"));
    imAddAction->setText (tr ("Add"));
    // imEditAction->setText (tr ("Edit"));
    imRemoveAction->setText (tr ("Remove"));
    imReleaseAction->setText (tr ("Release"));
    imRefreshAction->setText (tr ("Refresh"));

    imNewAction->setAccel (tr ("Ctrl+N"));
    imAddAction->setAccel (tr ("Ctrl+A"));
    // imEditAction->setAccel (tr ("Ctrl+E"));
    imRemoveAction->setAccel (tr ("Ctrl+D"));
    imReleaseAction->setAccel (tr ("Ctrl+L"));
    imRefreshAction->setAccel (tr ("Ctrl+R"));

    imNewAction->setStatusTip (tr ("Create a new virtual hard disk"));
    imAddAction->setStatusTip (tr ("Add (register) an existing image file"));
    // imEditAction->setStatusTip (tr ("Edit the properties of the selected item"));
    imRemoveAction->setStatusTip (tr ("Remove (unregister) the selected media"));
    imReleaseAction->setStatusTip (tr ("Release the selected media by detaching it from the machine"));
    imRefreshAction->setStatusTip (tr ("Refresh the media list"));

    if (menuBar()->findItem(1))
        menuBar()->findItem(1)->setText (tr ("&Actions"));

    hdsPane1->label()->setText (QString ("<nobr>%1:</nobr>").arg (tr ("Location")));
    hdsPane2->label()->setText (QString ("<nobr>%1:</nobr>").arg (tr ("Disk Type")));
    hdsPane3->label()->setText (QString ("<nobr>&nbsp;&nbsp;%1:</nobr>").arg (tr ("Storage Type")));
    hdsPane4->label()->setText (QString ("<nobr>%1:</nobr>").arg (tr ("Attached to")));
    hdsPane5->label()->setText (QString ("<nobr>&nbsp;&nbsp;%1:</nobr>").arg (tr ("Snapshot")));
    cdsPane1->label()->setText (QString ("<nobr>%1:</nobr>").arg (tr ("Location")));
    cdsPane2->label()->setText (QString ("<nobr>%1:</nobr>").arg (tr ("Attached to")));
    fdsPane1->label()->setText (QString ("<nobr>%1:</nobr>").arg (tr ("Location")));
    fdsPane2->label()->setText (QString ("<nobr>%1:</nobr>").arg (tr ("Attached to")));

    mProgressText->setText (tr ("Checking accessibility"));

    if (hdsView->childCount() || cdsView->childCount() || fdsView->childCount())
        refreshAll();
}


void VBoxDiskImageManagerDlg::createInfoString (InfoPaneLabel *&aInfo,
                                                QWidget *aRoot,
                                                int aRow, int aColumn)
{
    QLabel *nameLabel = new QLabel (aRoot);
    aInfo = new InfoPaneLabel (aRoot, nameLabel);

    /* Setup focus policy <strong> default for info pane */
    aInfo->setFocusPolicy (QWidget::StrongFocus);

    /* prevent the name columns from being expanded */
    nameLabel->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);

    if (aColumn == -1)
    {
        ((QGridLayout *) aRoot->layout())->addWidget (nameLabel, aRow, 0);
        ((QGridLayout *) aRoot->layout())->
            addMultiCellWidget (aInfo, aRow, aRow,
                                1, ((QGridLayout *) aRoot->layout())->numCols() - 1);
    }
    else
    {
        ((QGridLayout *) aRoot->layout())->addWidget (nameLabel, aRow, aColumn * 2);
        ((QGridLayout *) aRoot->layout())->addWidget (aInfo, aRow, aColumn * 2 + 1);
    }
}


void VBoxDiskImageManagerDlg::showEvent (QShowEvent *e)
{
    QMainWindow::showEvent (e);

    /* one may think that QWidget::polish() is the right place to do things
     * below, but apparently, by the time when QWidget::polish() is called,
     * the widget style & layout are not fully done, at least the minimum
     * size hint is not properly calculated. Since this is sometimes necessary,
     * we provide our own "polish" implementation. */

    if (polished)
        return;

    polished = true;

    VBoxGlobal::centerWidget (this, parentWidget());
}


void VBoxDiskImageManagerDlg::mouseOnItem (QListViewItem *aItem)
{
    QListView *currentList = getCurrentListView();
    QString tip;
    switch (aItem->rtti())
    {
        case DiskImageItem::TypeId:
            tip = static_cast<DiskImageItem*> (aItem)->getToolTip();
            break;
        default:
            Assert (0);
    }
    QToolTip::add (currentList->viewport(), currentList->itemRect (aItem), tip);
}


void VBoxDiskImageManagerDlg::resizeEvent (QResizeEvent*)
{
    sizeGrip->move (centralWidget()->rect().bottomRight() -
                    QPoint(sizeGrip->rect().width() - 1, sizeGrip->rect().height() - 1));
}


void VBoxDiskImageManagerDlg::closeEvent (QCloseEvent *aEvent)
{
    mModelessDialog = 0;
    aEvent->accept();
}


void VBoxDiskImageManagerDlg::keyPressEvent (QKeyEvent *aEvent)
{
    if ( aEvent->state() == 0 ||
         (aEvent->state() & Keypad && aEvent->key() == Key_Enter) )
    {
        switch ( aEvent->key() )
        {
            case Key_Enter:
            case Key_Return:
            {
                QPushButton *currentDefault = searchDefaultButton();
                if (currentDefault)
                    currentDefault->animateClick();
                break;
            }
            case Key_Escape:
            {
                reject();
                break;
            }
        }
    }
    else
        aEvent->ignore();
}


QPushButton* VBoxDiskImageManagerDlg::searchDefaultButton()
{
    QPushButton *defButton = 0;
    QObjectList *list = queryList ("QPushButton");
    QObjectListIt it (*list);
    while ( (defButton = (QPushButton*)it.current()) && !defButton->isDefault() )
    {
        ++it;
    }
    return defButton;
}


int  VBoxDiskImageManagerDlg::result() { return mRescode; }
void VBoxDiskImageManagerDlg::setResult (int aRescode) { mRescode = aRescode; }
void VBoxDiskImageManagerDlg::accept() { done( Accepted ); }
void VBoxDiskImageManagerDlg::reject() { done( Rejected ); }

int  VBoxDiskImageManagerDlg::exec()
{
    setResult (0);

    if (mInLoop) return result();
    show();
    mInLoop = true;
    qApp->eventLoop()->enterLoop();
    mInLoop = false;

    return result();
}

void VBoxDiskImageManagerDlg::done (int aResult)
{
    setResult (aResult);

    if (mInLoop)
    {
        hide();
        qApp->eventLoop()->exitLoop();
    }
    else
    {
        close();
    }
}


QListView* VBoxDiskImageManagerDlg::getCurrentListView()
{
    QListView *clv = static_cast<QListView*>(twImages->currentPage()->
        queryList("QListView")->getFirst());
    Assert(clv);
    return clv;
}

QListView* VBoxDiskImageManagerDlg::getListView (VBoxDefs::DiskType aType)
{
    switch (aType)
    {
        case VBoxDefs::HD:
            return hdsView;
        case VBoxDefs::CD:
            return cdsView;
        case VBoxDefs::FD:
            return fdsView;
        default:
            return 0;
    }
}


bool VBoxDiskImageManagerDlg::eventFilter (QObject *aObject, QEvent *aEvent)
{
    QListView *currentList = getCurrentListView();

    switch (aEvent->type())
    {
        case QEvent::DragEnter:
        {
            if (aObject == currentList)
            {
                QDragEnterEvent *dragEnterEvent =
                    static_cast<QDragEnterEvent*>(aEvent);
                dragEnterEvent->acceptAction();
                return true;
            }
            break;
        }
        case QEvent::Drop:
        {
            if (aObject == currentList)
            {
                QDropEvent *dropEvent =
                    static_cast<QDropEvent*>(aEvent);
                QStringList *droppedList = new QStringList();
                QUriDrag::decodeLocalFiles (dropEvent, *droppedList);
                QCustomEvent *updateEvent = new QCustomEvent (1001);
                updateEvent->setData (droppedList);
                QApplication::postEvent (currentList, updateEvent);
                dropEvent->acceptAction();
                return true;
            }
            break;
        }
        case 1001: /* QCustomEvent 1001 - DnD Update Event */
        {
            if (aObject == currentList)
            {
                QCustomEvent *updateEvent =
                    static_cast<QCustomEvent*>(aEvent);
                addDroppedImages ((QStringList*) updateEvent->data());
                return true;
            }
            break;
        }
        case QEvent::FocusIn:
        {
            if (aObject->inherits ("QPushButton") && aObject->parent() == centralWidget())
            {
                ((QPushButton*)aObject)->setDefault (aObject != defaultButton);
                if (defaultButton)
                    defaultButton->setDefault (aObject == defaultButton);
            }
            break;
        }
        case QEvent::FocusOut:
        {
            if (aObject->inherits ("QPushButton") && aObject->parent() == centralWidget())
            {
                if (defaultButton)
                    defaultButton->setDefault (aObject != defaultButton);
                ((QPushButton*)aObject)->setDefault (aObject == defaultButton);
            }
            break;
        }
        default:
            break;
    }
    return QMainWindow::eventFilter (aObject, aEvent);
}


bool VBoxDiskImageManagerDlg::event (QEvent *aEvent)
{
    bool result = QMainWindow::event (aEvent);
    switch (aEvent->type())
    {
        case QEvent::LanguageChange:
        {
            languageChangeImp();
            break;
        }
        default:
            break;
    }
    return result;
}


void VBoxDiskImageManagerDlg::addDroppedImages (QStringList *aDroppedList)
{
    QListView *currentList = getCurrentListView();

    for (QStringList::Iterator it = (*aDroppedList).begin();
         it != (*aDroppedList).end(); ++it)
    {
        QString src = *it;
        /* Check dropped media type */
        /// @todo On OS/2 and windows (and mac?) extension checks should be case
        /// insensitive, as OPPOSED to linux and the rest where case matters.
        VBoxDefs::DiskType type = VBoxDefs::InvalidType;
        if      (src.endsWith (".iso", false))
        {
            if (currentList == cdsView) type = VBoxDefs::CD;
        }
        else if (src.endsWith (".img", false))
        {
            if (currentList == fdsView) type = VBoxDefs::FD;
        }
        else if (src.endsWith (".vdi", false) ||
                 src.endsWith (".vmdk", false))
        {
            if (currentList == hdsView) type = VBoxDefs::HD;
        }
        /* If media type has been determined - attach this device */
        if (type)
        {
            addImageToList (*it, type);
            if (!vbox.isOk())
                vboxProblem().cannotRegisterMedia (this, vbox, type, src);
        }
    }
    delete aDroppedList;
}


void VBoxDiskImageManagerDlg::addImageToList (const QString &aSource,
                                              VBoxDefs::DiskType aDiskType)
{
    if (aSource.isEmpty())
        return;

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


DiskImageItem* VBoxDiskImageManagerDlg::createImageNode (QListView *aList,
                                                         DiskImageItem *aRoot,
                                                         const VBoxMedia &aMedia)
{
    DiskImageItem *item = 0;

    if (aRoot)
        item = new DiskImageItem (aRoot);
    else if (aList)
        item = new DiskImageItem (aList);
    else
        Assert (0);

    item->setMedia (aMedia);

    return item;
}


void VBoxDiskImageManagerDlg::invokePopup (QListViewItem *aItem, const QPoint & aPos, int)
{
    if (aItem)
        itemMenu->popup(aPos);
}


QString VBoxDiskImageManagerDlg::getDVDImageUsage (const QUuid &aId)
{
    CVirtualBox vbox = vboxGlobal().virtualBox();

    QStringList permMachines =
        QStringList::split (' ', vbox.GetDVDImageUsage (aId, CEnums::PermanentUsage));
    QStringList tempMachines =
        QStringList::split (' ', vbox.GetDVDImageUsage (aId, CEnums::TemporaryUsage));

    QString usage;

    for (QStringList::Iterator it = permMachines.begin();
         it != permMachines.end();
         ++it)
    {
        if (usage)
            usage += ", ";
        CMachine machine = vbox.GetMachine (QUuid (*it));
        usage += machine.GetName();

        QString snapshots;
        sourceSnapshotsCD (aId, machine.GetSnapshot (QUuid()), snapshots);
        if (!snapshots.isEmpty())
            usage += QString (" (%1)").arg (snapshots);
    }

    for (QStringList::Iterator it = tempMachines.begin();
         it != tempMachines.end();
         ++it)
    {
        /* skip IDs that are in the permanent list */
        if (!permMachines.contains (*it))
        {
            if (usage)
                usage += ", [";
            else
                usage += "[";
            CMachine machine = vbox.GetMachine (QUuid (*it));
            usage += machine.GetName() + "]";

            QString snapshots;
            sourceSnapshotsCD (aId, machine.GetSnapshot (QUuid()), snapshots);
            if (!snapshots.isEmpty())
                usage += QString (" (%1)").arg (snapshots);
        }
    }

    return usage;
}

QString VBoxDiskImageManagerDlg::getFloppyImageUsage (const QUuid &aId)
{
    CVirtualBox vbox = vboxGlobal().virtualBox();

    QStringList permMachines =
        QStringList::split (' ', vbox.GetFloppyImageUsage (aId, CEnums::PermanentUsage));
    QStringList tempMachines =
        QStringList::split (' ', vbox.GetFloppyImageUsage (aId, CEnums::TemporaryUsage));

    QString usage;

    for (QStringList::Iterator it = permMachines.begin();
         it != permMachines.end();
         ++it)
    {
        if (usage)
            usage += ", ";
        CMachine machine = vbox.GetMachine (QUuid (*it));
        usage += machine.GetName();

        QString snapshots;
        sourceSnapshotsFD (aId, machine.GetSnapshot (QUuid()), snapshots);
        if (!snapshots.isEmpty())
            usage += QString (" (%1)").arg (snapshots);
    }

    for (QStringList::Iterator it = tempMachines.begin();
         it != tempMachines.end();
         ++it)
    {
        /* skip IDs that are in the permanent list */
        if (!permMachines.contains (*it))
        {
            if (usage)
                usage += ", [";
            else
                usage += "[";
            CMachine machine = vbox.GetMachine (QUuid (*it));
            usage += machine.GetName() + "]";

            QString snapshots;
            sourceSnapshotsFD (aId, machine.GetSnapshot (QUuid()), snapshots);
            if (!snapshots.isEmpty())
                usage += QString (" (%1)").arg (snapshots);
        }
    }

    return usage;
}


void VBoxDiskImageManagerDlg::sourceSnapshotsCD (const QUuid &aImageId,
                                                 const CSnapshot &aSnapshot,
                                                 QString &aUsage)
{
    if (aSnapshot.isNull())
        return;

    if (!aSnapshot.GetMachine().GetDVDDrive().GetImage().isNull() &&
        aSnapshot.GetMachine().GetDVDDrive().GetImage().GetId() == aImageId)
    {
        if (aUsage)
            aUsage += ", ";
        aUsage += aSnapshot.GetName();
    }

    CSnapshotEnumerator en = aSnapshot.GetChildren().Enumerate();
    while (en.HasMore())
        sourceSnapshotsCD (aImageId, en.GetNext(), aUsage);
}

void VBoxDiskImageManagerDlg::sourceSnapshotsFD (const QUuid &aImageId,
                                                 const CSnapshot &aSnapshot,
                                                 QString &aUsage)
{
    if (aSnapshot.isNull())
        return;

    if (!aSnapshot.GetMachine().GetFloppyDrive().GetImage().isNull() &&
        aSnapshot.GetMachine().GetFloppyDrive().GetImage().GetId() == aImageId)
    {
        if (aUsage)
            aUsage += ", ";
        aUsage += aSnapshot.GetName();
    }

    CSnapshotEnumerator en = aSnapshot.GetChildren().Enumerate();
    while (en.HasMore())
        sourceSnapshotsFD (aImageId, en.GetNext(), aUsage);
}


QString VBoxDiskImageManagerDlg::composeHdToolTip (CHardDisk &aHd,
                                                   VBoxMedia::Status aStatus)
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    QUuid machineId = aHd.GetMachineId();

    QString src = aHd.GetLocation();
    QFileInfo fi (src);
    QString location = aHd.GetStorageType() == CEnums::ISCSIHardDisk ? src :
                       QDir::convertSeparators (fi.absFilePath());

    QString storageType = vboxGlobal().toString (aHd.GetStorageType());
    QString hardDiskType = vboxGlobal().hardDiskTypeString (aHd);

    QString usage;
    if (!machineId.isNull())
        usage = vbox.GetMachine (machineId).GetName();

    QString snapshotName;
    if (!machineId.isNull() && !aHd.GetSnapshotId().isNull())
    {
        CSnapshot snapshot = vbox.GetMachine (machineId).
                                  GetSnapshot (aHd.GetSnapshotId());
        if (!snapshot.isNull())
            snapshotName = snapshot.GetName();
    }

    /* compose tool-tip information */
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
                                                   VBoxMedia::Status aStatus)
{
    QString src = aCd.GetFilePath();
    QFileInfo fi (src);
    QString location = QDir::convertSeparators (fi.absFilePath ());
    QUuid uuid = aCd.GetId();
    QString usage = getDVDImageUsage (uuid);

    /* compose tool-tip information */
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
                                                   VBoxMedia::Status aStatus)
{
    QString src = aFd.GetFilePath();
    QFileInfo fi (src);
    QString location = QDir::convertSeparators (fi.absFilePath ());
    QUuid uuid = aFd.GetId();
    QString usage = getFloppyImageUsage (uuid);

    /* compose tool-tip information */
    /* compose tool-tip information */
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


void VBoxDiskImageManagerDlg::updateHdItem (DiskImageItem   *aItem,
                                            const VBoxMedia &aMedia)
{
    if (!aItem) return;
    CHardDisk hd = aMedia.disk;
    VBoxMedia::Status status = aMedia.status;

    QUuid uuid = hd.GetId();
    QString src = hd.GetLocation();
    QUuid machineId = hd.GetMachineId();
    QString usage;
    if (!machineId.isNull())
        usage = vbox.GetMachine (machineId).GetName();
    QString storageType = vboxGlobal().toString (hd.GetStorageType());
    QString hardDiskType = vboxGlobal().hardDiskTypeString (hd);
    QString virtualSize = status == VBoxMedia::Ok ?
        vboxGlobal().formatSize ((ULONG64)hd.GetSize() * _1M) : QString ("--");
    QString actualSize = status == VBoxMedia::Ok ?
        vboxGlobal().formatSize (hd.GetActualSize()) : QString ("--");
    QString snapshotName;
    if (!machineId.isNull() && !hd.GetSnapshotId().isNull())
    {
        CSnapshot snapshot = vbox.GetMachine (machineId).
                                  GetSnapshot (hd.GetSnapshotId());
        if (!snapshot.isNull())
            snapshotName = QString ("%1").arg (snapshot.GetName());
    }
    QFileInfo fi (src);

    aItem->setText (0, fi.fileName());
    aItem->setText (1, virtualSize);
    aItem->setText (2, actualSize);
    aItem->setPath (hd.GetStorageType() == CEnums::ISCSIHardDisk ? src :
                   QDir::convertSeparators (fi.absFilePath()));
    aItem->setUsage (usage);
    aItem->setSnapshotName (snapshotName);
    aItem->setDiskType (hardDiskType);
    aItem->setStorageType (storageType);
    aItem->setVirtualSize (virtualSize);
    aItem->setActualSize (actualSize);
    aItem->setUuid (uuid);
    aItem->setMachineId (machineId);
    aItem->setToolTip (composeHdToolTip (hd, status));
    aItem->setStatus (status);

    makeWarningMark (aItem, aMedia.status, VBoxDefs::HD);
}

void VBoxDiskImageManagerDlg::updateCdItem (DiskImageItem   *aItem,
                                            const VBoxMedia &aMedia)
{
    if (!aItem) return;
    CDVDImage cd = aMedia.disk;
    VBoxMedia::Status status = aMedia.status;

    QUuid uuid = cd.GetId();
    QString src = cd.GetFilePath();
    QString usage = getDVDImageUsage (uuid);
    QString size = status == VBoxMedia::Ok ?
        vboxGlobal().formatSize (cd.GetSize()) : QString ("--");
    QFileInfo fi (src);

    aItem->setText (0, fi.fileName());
    aItem->setText (1, size);
    aItem->setPath (QDir::convertSeparators (fi.absFilePath ()));
    aItem->setUsage (usage);
    aItem->setActualSize (size);
    aItem->setUuid (uuid);
    aItem->setToolTip (composeCdToolTip (cd, status));
    aItem->setStatus (status);

    makeWarningMark (aItem, aMedia.status, VBoxDefs::CD);
}

void VBoxDiskImageManagerDlg::updateFdItem (DiskImageItem   *aItem,
                                            const VBoxMedia &aMedia)
{
    if (!aItem) return;
    CFloppyImage fd = aMedia.disk;
    VBoxMedia::Status status = aMedia.status;

    QUuid uuid = fd.GetId();
    QString src = fd.GetFilePath();
    QString usage = getFloppyImageUsage (uuid);
    QString size = status == VBoxMedia::Ok ?
        vboxGlobal().formatSize (fd.GetSize()) : QString ("--");
    QFileInfo fi (src);

    aItem->setText (0, fi.fileName());
    aItem->setText (1, size);
    aItem->setPath (QDir::convertSeparators (fi.absFilePath ()));
    aItem->setUsage (usage);
    aItem->setActualSize (size);
    aItem->setUuid (uuid);
    aItem->setToolTip (composeFdToolTip (fd, status));
    aItem->setStatus (status);

    makeWarningMark (aItem, aMedia.status, VBoxDefs::FD);
}


DiskImageItem* VBoxDiskImageManagerDlg::createHdItem (QListView *aList,
                                                      const VBoxMedia &aMedia)
{
    CHardDisk hd = aMedia.disk;
    QUuid rootId = hd.GetParent().isNull() ? QUuid() : hd.GetParent().GetId();
    DiskImageItem *root = searchItem (aList, rootId);
    DiskImageItem *item = createImageNode (aList, root, aMedia);
    updateHdItem (item, aMedia);
    return item;
}

DiskImageItem* VBoxDiskImageManagerDlg::createCdItem (QListView *aList,
                                                      const VBoxMedia &aMedia)
{
    DiskImageItem *item = createImageNode (aList, 0, aMedia);
    updateCdItem (item, aMedia);
    return item;
}

DiskImageItem* VBoxDiskImageManagerDlg::createFdItem (QListView *aList,
                                                      const VBoxMedia &aMedia)
{
    DiskImageItem *item = createImageNode (aList, 0, aMedia);
    updateFdItem (item, aMedia);
    return item;
}


void VBoxDiskImageManagerDlg::makeWarningMark (DiskImageItem *aItem,
                                               VBoxMedia::Status aStatus,
                                               VBoxDefs::DiskType aType)
{
    const QPixmap &pm = aStatus == VBoxMedia::Inaccessible ? pxInaccessible :
                        aStatus == VBoxMedia::Error ? pxErroneous : QPixmap();

    if (!pm.isNull())
    {
        aItem->setPixmap (0, pm);
        QIconSet iconSet (pm);
        QWidget *wt = aType == VBoxDefs::HD ? twImages->page (0) :
                      aType == VBoxDefs::CD ? twImages->page (1) :
                      aType == VBoxDefs::FD ? twImages->page (2) : 0;
        Assert (wt); /* aType should be correct */
        twImages->changeTab (wt, iconSet, twImages->tabLabel (wt));
        aItem->listView()->ensureItemVisible (aItem);
    }
}


DiskImageItem* VBoxDiskImageManagerDlg::searchItem (QListView *aList,
                                                    const QUuid &aId)
{
    if (aId.isNull()) return 0;
    DiskImageItemIterator iterator (aList);
    while (*iterator)
    {
        if ((*iterator)->getUuid() == aId)
            return *iterator;
        ++iterator;
    }
    return 0;
}


DiskImageItem* VBoxDiskImageManagerDlg::searchItem (QListView *aList,
                                                    VBoxMedia::Status aStatus)
{
    DiskImageItemIterator iterator (aList);
    while (*iterator)
    {
        if ((*iterator)->getStatus() == aStatus)
            return *iterator;
        ++iterator;
    }
    return 0;
}


void VBoxDiskImageManagerDlg::setup (int aType, bool aDoSelect,
                                     const QUuid *aTargetVMId /* = NULL */,
                                     bool aRefresh /* = true */,
                                     CMachine machine /* = NULL */)
{
    cmachine = machine;

    type = aType;
    twImages->setTabEnabled (twImages->page(0), type & VBoxDefs::HD);
    twImages->setTabEnabled (twImages->page(1), type & VBoxDefs::CD);
    twImages->setTabEnabled (twImages->page(2), type & VBoxDefs::FD);

    doSelect = aDoSelect;
    if (aTargetVMId)
        targetVMId = aTargetVMId->toString();

    if (doSelect)
        buttonOk->setText (tr ("&Select"));
    else
        buttonCancel->setShown (false);

    /* listen to "media enumeration started" signals */
    connect (&vboxGlobal(), SIGNAL (mediaEnumStarted()),
             this, SLOT (mediaEnumStarted()));
    /* listen to "media enumeration" signals */
    connect (&vboxGlobal(), SIGNAL (mediaEnumerated (const VBoxMedia &, int)),
             this, SLOT (mediaEnumerated (const VBoxMedia &, int)));
    /* listen to "media enumeration finished" signals */
    connect (&vboxGlobal(), SIGNAL (mediaEnumFinished (const VBoxMediaList &)),
             this, SLOT (mediaEnumFinished (const VBoxMediaList &)));

    /* listen to "media add" signals */
    connect (&vboxGlobal(), SIGNAL (mediaAdded (const VBoxMedia &)),
             this, SLOT (mediaAdded (const VBoxMedia &)));
    /* listen to "media update" signals */
    connect (&vboxGlobal(), SIGNAL (mediaUpdated (const VBoxMedia &)),
             this, SLOT (mediaUpdated (const VBoxMedia &)));
    /* listen to "media remove" signals */
    connect (&vboxGlobal(), SIGNAL (mediaRemoved (VBoxDefs::DiskType, const QUuid &)),
             this, SLOT (mediaRemoved (VBoxDefs::DiskType, const QUuid &)));

    if (aRefresh && !vboxGlobal().isMediaEnumerationStarted())
    {
        vboxGlobal().startEnumeratingMedia();
    }
    else
    {
        /* insert already enumerated media */
        const VBoxMediaList &list = vboxGlobal().currentMediaList();
        prepareToRefresh (list.size());
        VBoxMediaList::const_iterator it;
        int index = 0;
        for (it = list.begin(); it != list.end(); ++ it)
        {
            mediaAdded (*it);
            if ((*it).status != VBoxMedia::Unknown)
                mProgressBar->setProgress (++ index);
        }

        /* emulate the finished signal to reuse the code */
        if (!vboxGlobal().isMediaEnumerationStarted())
            mediaEnumFinished (list);
    }

    /* for a newly opened dialog, select the first item */
    setCurrentItem (hdsView, hdsView->firstChild());
    setCurrentItem (cdsView, cdsView->firstChild());
    setCurrentItem (fdsView, fdsView->firstChild());
}


void VBoxDiskImageManagerDlg::mediaEnumStarted()
{
    /* load default tab icons */
    twImages->changeTab (twImages->page (0), pxHD,
                         twImages->tabLabel (twImages->page (0)));
    twImages->changeTab (twImages->page (1), pxCD,
                         twImages->tabLabel (twImages->page (1)));
    twImages->changeTab (twImages->page (2), pxFD,
                         twImages->tabLabel (twImages->page (2)));

    /* load current media list */
    const VBoxMediaList &list = vboxGlobal().currentMediaList();
    prepareToRefresh (list.size());
    VBoxMediaList::const_iterator it;
    for (it = list.begin(); it != list.end(); ++ it)
        mediaAdded (*it);

    /* select the first item if the previous saved item is not found
     * or no current item at all */
    if (!hdsView->currentItem() || !hdSelectedId.isNull())
        setCurrentItem (hdsView, hdsView->firstChild());
    if (!cdsView->currentItem() || !cdSelectedId.isNull())
        setCurrentItem (cdsView, cdsView->firstChild());
    if (!fdsView->currentItem() || !fdSelectedId.isNull())
        setCurrentItem (fdsView, fdsView->firstChild());

    processCurrentChanged();
}

void VBoxDiskImageManagerDlg::mediaEnumerated (const VBoxMedia &aMedia,
                                               int aIndex)
{
    mediaUpdated (aMedia);
    Assert (aMedia.status != VBoxMedia::Unknown);
    if (aMedia.status != VBoxMedia::Unknown)
        mProgressBar->setProgress (aIndex + 1);
}

void VBoxDiskImageManagerDlg::mediaEnumFinished (const VBoxMediaList &/* aList */)
{
    mProgressBar->setHidden (true);
    mProgressText->setHidden (true);

    imRefreshAction->setEnabled (true);
    unsetCursor();

    /* adjust columns (it is strange to repeat but it works) */

    hdsView->adjustColumn (1);
    hdsView->adjustColumn (2);
    hdsView->adjustColumn (1);

    cdsView->adjustColumn (1);
    cdsView->adjustColumn (2);
    cdsView->adjustColumn (1);

    fdsView->adjustColumn (1);
    fdsView->adjustColumn (2);
    fdsView->adjustColumn (1);

    processCurrentChanged();
}


void VBoxDiskImageManagerDlg::mediaAdded (const VBoxMedia &aMedia)
{
    /* ignore non-interesting aMedia */
    if (!(type & aMedia.type))
        return;

    DiskImageItem *item = 0;
    switch (aMedia.type)
    {
        case VBoxDefs::HD:
            item = createHdItem (hdsView, aMedia);
            if (item->getUuid() == hdSelectedId)
            {
                setCurrentItem (hdsView, item);
                hdSelectedId = QUuid();
            }
            break;
        case VBoxDefs::CD:
            item = createCdItem (cdsView, aMedia);
            if (item->getUuid() == cdSelectedId)
            {
                setCurrentItem (cdsView, item);
                cdSelectedId = QUuid();
            }
            break;
        case VBoxDefs::FD:
            item = createFdItem (fdsView, aMedia);
            if (item->getUuid() == fdSelectedId)
            {
                setCurrentItem (fdsView, item);
                fdSelectedId = QUuid();
            }
            break;
        default:
            AssertMsgFailed (("Invalid aMedia type\n"));
    }

    if (!item)
        return;

    if (!vboxGlobal().isMediaEnumerationStarted())
        setCurrentItem (getListView (aMedia.type), item);
    if (item == getCurrentListView()->currentItem())
        processCurrentChanged (item);
}

void VBoxDiskImageManagerDlg::mediaUpdated (const VBoxMedia &aMedia)
{
    /* ignore non-interesting aMedia */
    if (!(type & aMedia.type))
        return;

    DiskImageItem *item = 0;
    switch (aMedia.type)
    {
        case VBoxDefs::HD:
        {
            CHardDisk hd = aMedia.disk;
            item = searchItem (hdsView, hd.GetId());
            updateHdItem (item, aMedia);
            break;
        }
        case VBoxDefs::CD:
        {
            CDVDImage cd = aMedia.disk;
            item = searchItem (cdsView, cd.GetId());
            updateCdItem (item, aMedia);
            break;
        }
        case VBoxDefs::FD:
        {
            CFloppyImage fd = aMedia.disk;
            item = searchItem (fdsView, fd.GetId());
            updateFdItem (item, aMedia);
            break;
        }
        default:
            AssertMsgFailed (("Invalid aMedia type\n"));
    }

    if (!item)
        return;

    /* note: current items on invisible tabs are not updated because
     * it is always done in processCurrentChanged() when the user switches
     * to an invisible tab */
    if (item == getCurrentListView()->currentItem())
        processCurrentChanged (item);
}

void VBoxDiskImageManagerDlg::mediaRemoved (VBoxDefs::DiskType aType,
                                            const QUuid &aId)
{
    QListView *listView = getListView (aType);
    DiskImageItem *item = searchItem (listView, aId);
    delete item;
    setCurrentItem (listView, listView->currentItem());
    /* search the list for inaccessible media */
    if (!searchItem (listView, VBoxMedia::Inaccessible) &&
        !searchItem (listView, VBoxMedia::Error))
    {
        QWidget *wt = aType == VBoxDefs::HD ? twImages->page (0) :
                      aType == VBoxDefs::CD ? twImages->page (1) :
                      aType == VBoxDefs::FD ? twImages->page (2) : 0;
        const QIconSet &set = aType == VBoxDefs::HD ? pxHD :
                              aType == VBoxDefs::CD ? pxCD :
                              aType == VBoxDefs::FD ? pxFD : QIconSet();
        Assert (wt && !set.isNull()); /* atype should be the correct one */
        twImages->changeTab (wt, set, twImages->tabLabel (wt));
    }
}


void VBoxDiskImageManagerDlg::machineStateChanged (const VBoxMachineStateChangeEvent &e)
{
    /// @todo (r=dmik) IVirtualBoxCallback::OnMachineStateChange
    //  must also expose the old state! In this case we won't need to cache
    //  the state value in every class in GUI that uses this signal.

    switch (e.state)
    {
        case CEnums::PoweredOff:
        case CEnums::Aborted:
        case CEnums::Saved:
        case CEnums::Starting:
        case CEnums::Restoring:
        {
            refreshAll();
            break;
        }
        default:
            break;
    }
}


void VBoxDiskImageManagerDlg::clearInfoPanes()
{
    hdsPane1->clear();
    hdsPane2->clear(), hdsPane3->clear();
    hdsPane4->clear(), hdsPane5->clear();
    cdsPane1->clear(), cdsPane2->clear();
    fdsPane1->clear(), fdsPane2->clear();
}


void VBoxDiskImageManagerDlg::prepareToRefresh (int aTotal)
{
    /* info panel clearing */
    clearInfoPanes();

    /* prepare progressbar */
    if (mProgressBar)
    {
        mProgressBar->setProgress (0, aTotal);
        mProgressBar->setHidden (false);
        mProgressText->setHidden (false);
    }

    imRefreshAction->setEnabled (false);
    setCursor (QCursor (BusyCursor));

    /* store the current list selections */

    QListViewItem *item;
    DiskImageItem *di;

    item = hdsView->currentItem();
    di = (item && item->rtti() == DiskImageItem::TypeId) ?
        static_cast <DiskImageItem *> (item) : 0;
    hdSelectedId = di ? di->getUuid() : QString::null;

    item = cdsView->currentItem();
    di = (item && item->rtti() == DiskImageItem::TypeId) ?
        static_cast <DiskImageItem *> (item) : 0;
    cdSelectedId = di ? di->getUuid() : QString::null;

    item = fdsView->currentItem();
    di = (item && item->rtti() == DiskImageItem::TypeId) ?
        static_cast <DiskImageItem *> (item) : 0;
    fdSelectedId = di ? di->getUuid() : QString::null;

    /* finally, clear all lists */
    hdsView->clear();
    cdsView->clear();
    fdsView->clear();
}


void VBoxDiskImageManagerDlg::refreshAll()
{
    /* start enumerating media */
    vboxGlobal().startEnumeratingMedia();
}


bool VBoxDiskImageManagerDlg::checkImage (DiskImageItem* aItem)
{
    QUuid itemId = aItem ? QUuid (aItem->getUuid()) : QUuid();
    if (itemId.isNull()) return false;

    QListView* parentList = aItem->listView();
    if (parentList == hdsView)
    {
        CHardDisk hd = aItem->getMedia().disk;
        QUuid machineId = hd.GetMachineId();
        if (machineId.isNull() ||
            vbox.GetMachine (machineId).GetState() != CEnums::PoweredOff &&
            vbox.GetMachine (machineId).GetState() != CEnums::Aborted)
            return false;
    }
    else if (parentList == cdsView)
    {
        QString usage = getDVDImageUsage (itemId);
        /* check if there is temporary usage: */
        QStringList tempMachines =
            QStringList::split (' ', vbox.GetDVDImageUsage (itemId,
                                          CEnums::TemporaryUsage));
        if (!tempMachines.isEmpty())
            return false;
        /* only permamently mounted .iso could be released */
        QStringList permMachines =
            QStringList::split (' ', vbox.GetDVDImageUsage (itemId,
                                          CEnums::PermanentUsage));
        for (QStringList::Iterator it = permMachines.begin();
             it != permMachines.end(); ++it)
            if (vbox.GetMachine(QUuid (*it)).GetState() != CEnums::PoweredOff &&
                vbox.GetMachine(QUuid (*it)).GetState() != CEnums::Aborted)
                return false;
    }
    else if (parentList == fdsView)
    {
        QString usage = getFloppyImageUsage(itemId);
        /* check if there is temporary usage: */
        QStringList tempMachines =
            QStringList::split (' ', vbox.GetFloppyImageUsage (itemId,
                                             CEnums::TemporaryUsage));
        if (!tempMachines.isEmpty())
            return false;
        /* only permamently mounted .iso could be released */
        QStringList permMachines =
            QStringList::split (' ', vbox.GetFloppyImageUsage (itemId,
                                             CEnums::PermanentUsage));
        for (QStringList::Iterator it = permMachines.begin();
             it != permMachines.end(); ++it)
            if (vbox.GetMachine(QUuid (*it)).GetState() != CEnums::PoweredOff &&
                vbox.GetMachine(QUuid (*it)).GetState() != CEnums::Aborted)
                return false;
    }
    else
    {
        return false;
    }
    return true;
}


void VBoxDiskImageManagerDlg::setCurrentItem (QListView *aListView,
                                              QListViewItem *aItem)
{
    if (!aItem)
        return;

    aListView->setCurrentItem (aItem);
    aListView->setSelected (aListView->currentItem(), true);
}


void VBoxDiskImageManagerDlg::processCurrentChanged()
{
    QListView *currentList = getCurrentListView();
    currentList->setFocus();

    /* tab stop setup */
    setTabOrder (hdsView, hdsPane1);
    setTabOrder (hdsPane1, hdsPane2);
    setTabOrder (hdsPane2, hdsPane3);
    setTabOrder (hdsPane3, hdsPane4);
    setTabOrder (hdsPane4, hdsPane5);
    setTabOrder (hdsPane5, buttonHelp);

    setTabOrder (cdsView, cdsPane1);
    setTabOrder (cdsPane1, cdsPane2);
    setTabOrder (cdsPane2, buttonHelp);

    setTabOrder (fdsView, fdsPane1);
    setTabOrder (fdsPane1, fdsPane2);
    setTabOrder (fdsPane2, buttonHelp);

    setTabOrder (buttonHelp, buttonOk);
    setTabOrder (buttonOk, twImages);

    processCurrentChanged (currentList->currentItem());
}

void VBoxDiskImageManagerDlg::processCurrentChanged (QListViewItem *aItem)
{
    DiskImageItem *item = aItem && aItem->rtti() == DiskImageItem::TypeId ?
        static_cast<DiskImageItem*> (aItem) : 0;

    bool notInEnum      = !vboxGlobal().isMediaEnumerationStarted();
    bool modifyEnabled  = notInEnum &&
                          item &&  item->getUsage().isNull() &&
                          !item->firstChild() && !item->getPath().isNull();
    bool releaseEnabled = item && !item->getUsage().isNull() &&
        item->getUsage().find (QRegExp ("\\([\\s\\S]+\\)")) == -1 &&
        checkImage (item) &&
        !item->parent() && !item->firstChild() &&
        item->getSnapshotName().isNull();
    bool newEnabled     = notInEnum &&
                          getCurrentListView() == hdsView ? true : false;
    bool addEnabled     = notInEnum;

    // imEditAction->setEnabled (modifyEnabled);
    imRemoveAction->setEnabled (modifyEnabled);
    imReleaseAction->setEnabled (releaseEnabled);
    imNewAction->setEnabled (newEnabled);
    imAddAction->setEnabled (addEnabled);

    // itemMenu->setItemVisible (itemMenu->idAt(0), modifyEnabled);
    itemMenu->setItemEnabled (itemMenu->idAt(0), modifyEnabled);
    itemMenu->setItemEnabled (itemMenu->idAt(1), releaseEnabled);

    if (doSelect)
    {
        bool selectEnabled = item && !item->parent() &&
                             (!newEnabled ||
                                (item->getUsage().isNull() ||
                                 item->getMachineId() == targetVMId));

        buttonOk->setEnabled (selectEnabled);
    }

    if (item)
    {
        if (item->listView() == hdsView)
        {
            hdsPane1->setText (item->getInformation (item->getPath(), true, "end"));
            hdsPane2->setText (item->getInformation (item->getDiskType(), false));
            hdsPane3->setText (item->getInformation (item->getStorageType(), false));
            hdsPane4->setText (item->getInformation (item->getUsage()));
            hdsPane5->setText (item->getInformation (item->getSnapshotName()));
        }
        else if (item->listView() == cdsView)
        {
            cdsPane1->setText (item->getInformation (item->getPath(), true, "end"));
            cdsPane2->setText (item->getInformation (item->getUsage()));
        }
        else if (item->listView() == fdsView)
        {
            fdsPane1->setText (item->getInformation (item->getPath(), true, "end"));
            fdsPane2->setText (item->getInformation (item->getUsage()));
        }
    }
    else
        clearInfoPanes();
}


void VBoxDiskImageManagerDlg::processPressed (QListViewItem * aItem)
{
    if (!aItem)
    {
        QListView *currentList = getCurrentListView();
        currentList->setSelected (currentList->currentItem(), true);
    }
}


void VBoxDiskImageManagerDlg::newImage()
{
    AssertReturnVoid (getCurrentListView() == hdsView);

    VBoxNewHDWzd dlg (this, "VBoxNewHDWzd");

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
    QListView *currentList = getCurrentListView();
    DiskImageItem *item =
        currentList->currentItem() &&
        currentList->currentItem()->rtti() == DiskImageItem::TypeId ?
        static_cast <DiskImageItem*> (currentList->currentItem()) : 0;

    QString dir;
    if (item && item->getStatus() == VBoxMedia::Ok)
        dir = QFileInfo (item->getPath().stripWhiteSpace()).dirPath (true);

    if (!dir)
        if (currentList == hdsView)
            dir = vbox.GetSystemProperties().GetDefaultVDIFolder();

    if (!dir || !QFileInfo (dir).exists())
        dir = vbox.GetHomeFolder();

    QString title;
    QString filter;
    VBoxDefs::DiskType type = VBoxDefs::InvalidType;

    if (currentList == hdsView)
    {
        filter = tr ("All hard disk images (*.vdi; *.vmdk);;"
                     "Virtual Disk images (*.vdi);;"
                     "VMDK images (*.vmdk);;"
                     "All files (*)");
        title = tr ("Select a hard disk image file");
        type = VBoxDefs::HD;
    }
    else if (currentList == cdsView)
    {
        filter = tr ("CD/DVD-ROM images (*.iso);;"
                     "All files (*)");
        title = tr ("Select a CD/DVD-ROM disk image file");
        type = VBoxDefs::CD;
    }
    else if (currentList == fdsView)
    {
        filter = tr ("Floppy images (*.img);;"
                     "All files (*)");
        title = tr ("Select a floppy disk image file");
        type = VBoxDefs::FD;
    }
    else
    {
        AssertMsgFailed (("Root list should be equal to hdsView, cdsView or fdsView"));
    }

    QString src = VBoxGlobal::getOpenFileName (dir, filter, this,
                                               "AddDiskImageDialog", title);
    src =  QDir::convertSeparators (src);

    addImageToList (src, type);
    if (!vbox.isOk())
        vboxProblem().cannotRegisterMedia (this, vbox, type, src);
}


void VBoxDiskImageManagerDlg::removeImage()
{
    QListView *currentList = getCurrentListView();
    DiskImageItem *item =
        currentList->currentItem() &&
        currentList->currentItem()->rtti() == DiskImageItem::TypeId ?
        static_cast<DiskImageItem*> (currentList->currentItem()) : 0;
    AssertMsg (item, ("Current item must not be null"));

    QUuid uuid = QUuid (item->getUuid());
    AssertMsg (!uuid.isNull(), ("Current item must have uuid"));

    QString src = item->getPath().stripWhiteSpace();
    VBoxDefs::DiskType type = VBoxDefs::InvalidType;

    if (currentList == hdsView)
    {
        type = VBoxDefs::HD;
        int deleteImage;
        /// @todo When creation of VMDK is implemented, we should
        /// enable image deletion for  them as well (use
        /// GetStorageType() to define the correct cast).
        CHardDisk disk = item->getMedia().disk;
        if (disk.GetStorageType() == CEnums::VirtualDiskImage &&
            item->getStatus() == VBoxMedia::Ok)
            deleteImage = vboxProblem().confirmHardDiskImageDeletion (this, src);
        else
            deleteImage = vboxProblem().confirmHardDiskUnregister (this, src);
        if (deleteImage == QIMessageBox::Cancel)
            return;
        CHardDisk hd = vbox.UnregisterHardDisk (uuid);
        if (vbox.isOk() && deleteImage == QIMessageBox::Yes)
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
    }
    else if (currentList == cdsView)
    {
        type = VBoxDefs::CD;
        vbox.UnregisterDVDImage (uuid);
    }
    else if (currentList == fdsView)
    {
        type = VBoxDefs::FD;
        vbox.UnregisterFloppyImage (uuid);
    }

    if (vbox.isOk())
        vboxGlobal().removeMedia (type, uuid);
    else
        vboxProblem().cannotUnregisterMedia (this, vbox, type, src);
}


void VBoxDiskImageManagerDlg::releaseImage()
{
    QListView *currentList = getCurrentListView();
    DiskImageItem *item =
        currentList->currentItem() &&
        currentList->currentItem()->rtti() == DiskImageItem::TypeId ?
        static_cast<DiskImageItem*> (currentList->currentItem()) : 0;
    AssertMsg (item, ("Current item must not be null"));

    QUuid itemId = QUuid (item->getUuid());
    AssertMsg (!itemId.isNull(), ("Current item must have uuid"));

    /* if it is a hard disk sub-item: */
    if (currentList == hdsView)
    {
        CHardDisk hd = item->getMedia().disk;
        QUuid machineId = hd.GetMachineId();
        if (vboxProblem().confirmReleaseImage (this,
            vbox.GetMachine (machineId).GetName()))
        {
            releaseDisk (machineId, itemId, VBoxDefs::HD);
            vboxGlobal().updateMedia (item->getMedia());
        }
    }
    /* if it is a cd/dvd sub-item: */
    else if (currentList == cdsView)
    {
        QString usage = getDVDImageUsage (itemId);
        /* only permamently mounted .iso could be released */
        if (vboxProblem().confirmReleaseImage (this, usage))
        {
            QStringList permMachines =
                QStringList::split (' ', vbox.GetDVDImageUsage (itemId,
                                              CEnums::PermanentUsage));
            for (QStringList::Iterator it = permMachines.begin();
                 it != permMachines.end(); ++it)
                releaseDisk (QUuid (*it), itemId, VBoxDefs::CD);

            CDVDImage cd = vbox.GetDVDImage (itemId);
            vboxGlobal().updateMedia (item->getMedia());
        }
    }
    /* if it is a floppy sub-item: */
    else if (currentList == fdsView)
    {
        QString usage = getFloppyImageUsage (itemId);
        /* only permamently mounted .img could be released */
        if (vboxProblem().confirmReleaseImage (this, usage))
        {
            QStringList permMachines =
                QStringList::split (' ', vbox.GetFloppyImageUsage (itemId,
                                                 CEnums::PermanentUsage));
            for (QStringList::Iterator it = permMachines.begin();
                 it != permMachines.end(); ++it)
                releaseDisk (QUuid (*it), itemId, VBoxDefs::FD);

            CFloppyImage fd = vbox.GetFloppyImage (itemId);
            vboxGlobal().updateMedia (item->getMedia());
        }
    }
}


void VBoxDiskImageManagerDlg::releaseDisk (QUuid aMachineId,
                                           QUuid aItemId,
                                           VBoxDefs::DiskType aDiskType)
{
    CSession session;
    CMachine machine;
    /* is this media image mapped to this VM: */
    if (!cmachine.isNull() && cmachine.GetId() == aMachineId)
    {
        machine = cmachine;
    }
    /* or some other: */
    else
    {
        session = vboxGlobal().openSession (aMachineId);
        if (session.isNull()) return;
        machine = session.GetMachine();
    }
    /* perform disk releasing: */
    switch (aDiskType)
    {
        case VBoxDefs::HD:
        {
            /* releasing hd: */
            CHardDiskAttachmentEnumerator en =
                machine.GetHardDiskAttachments().Enumerate();
            while (en.HasMore())
            {
                CHardDiskAttachment hda = en.GetNext();
                if (hda.GetHardDisk().GetId() == aItemId)
                {
                    machine.DetachHardDisk (hda.GetController(),
                                            hda.GetDeviceNumber());
                    if (!machine.isOk())
                        vboxProblem().cannotDetachHardDisk (this,
                            machine, hda.GetController(), hda.GetDeviceNumber());
                    break;
                }
            }
            break;
        }
        case VBoxDefs::CD:
        {
            /* releasing cd: */
            machine.GetDVDDrive().Unmount();
            break;
        }
        case VBoxDefs::FD:
        {
            /* releasing fd: */
            machine.GetFloppyDrive().Unmount();
            break;
        }
        default:
            AssertMsgFailed (("Incorrect disk type."));
    }
    /* save all setting changes: */
    machine.SaveSettings();
    if (!machine.isOk())
        vboxProblem().cannotSaveMachineSettings (machine);
    /* if local session was opened - close this session: */
    if (!session.isNull())
        session.Close();
}


QUuid VBoxDiskImageManagerDlg::getSelectedUuid()
{
    QListView *currentList = getCurrentListView();
    QUuid uuid;

    if (currentList->selectedItem() &&
        currentList->selectedItem()->rtti() == DiskImageItem::TypeId)
        uuid = QUuid (static_cast<DiskImageItem *>(currentList->selectedItem())
                      ->getUuid());

    return uuid;
}


QString VBoxDiskImageManagerDlg::getSelectedPath()
{
    QListView *currentList = getCurrentListView();
    QString path;

    if (currentList->selectedItem() &&
        currentList->selectedItem()->rtti() == DiskImageItem::TypeId )
        path = static_cast<DiskImageItem*> (currentList->selectedItem())
               ->getPath().stripWhiteSpace();

    return path;
}


void VBoxDiskImageManagerDlg::processDoubleClick (QListViewItem*)
{
    QListView *currentList = getCurrentListView();

    if (doSelect && currentList->selectedItem() && buttonOk->isEnabled())
        accept();
}
