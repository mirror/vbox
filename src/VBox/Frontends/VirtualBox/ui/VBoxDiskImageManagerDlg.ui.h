/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * "Virtual Disk Manager" dialog UI include (Qt Designer)
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
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

    DiskImageItem (DiskImageItem *parent, QString aLabel1,
                   QString aLabel2 = QString::null,
                   QString aLabel3 = QString::null,
                   QString aLabel4 = QString::null,
                   QString aLabel5 = QString::null,
                   QString aLabel6 = QString::null,
                   QString aLabel7 = QString::null,
                   QString aLabel8 = QString::null) :
        QListViewItem (parent, aLabel1, aLabel2, aLabel3, aLabel4, aLabel5,
                       aLabel6, aLabel7, aLabel8), mName (aLabel1) {}

    DiskImageItem (QListView *parent, QString aLabel1,
                   QString aLabel2 = QString::null,
                   QString aLabel3 = QString::null,
                   QString aLabel4 = QString::null,
                   QString aLabel5 = QString::null,
                   QString aLabel6 = QString::null,
                   QString aLabel7 = QString::null,
                   QString aLabel8 = QString::null) :
        QListViewItem (parent, aLabel1, aLabel2, aLabel3, aLabel4, aLabel5,
                       aLabel6, aLabel7, aLabel8), mName (aLabel1) {}

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


    void setToolTip (QString aToolTip) { mToolTip = aToolTip; }
    const QString &getToolTip() { return mToolTip; }

    QString getInformation (const QString &aInfo, bool aCompact = true,
                            const QString &aElipsis = "middle")
    {
        QString compactString = QString ("<compact elipsis=\"%1\">").arg (aElipsis);
        QString info = QString ("<nobr>%1%2%3</nobr>")
                       .arg (aCompact ? compactString : "")
                       .arg (aInfo.isEmpty() ? QObject::tr ("--") : aInfo)
                       .arg (aCompact ? "</compact>" : "");
        return info;
    }

    int rtti() const { return 1001; }

    int compare (QListViewItem *aItem, int aColumn, bool aAscending) const
    {
        ULONG64 thisValue = vboxGlobal().parseSize (       text (aColumn));
        ULONG64 thatValue = vboxGlobal().parseSize (aItem->text (aColumn));
        if (thisValue && thatValue)
            return thisValue > thatValue ? 1 : -1;
        else
            return QListViewItem::compare (aItem, aColumn, aAscending);
    }

    DiskImageItem* nextSibling() const
    {
        return (QListViewItem::nextSibling() &&
                QListViewItem::nextSibling()->rtti() == 1001) ?
                static_cast<DiskImageItem*> (QListViewItem::nextSibling()) : 0;
    }

protected:

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
};


VBoxDiskImageManagerDlg *VBoxDiskImageManagerDlg::mModelessDialog = 0;


void VBoxDiskImageManagerDlg::showModeless (const VBoxMediaList *list /* = NULL */)
{
    if (!mModelessDialog)
    {
        mModelessDialog =
            new VBoxDiskImageManagerDlg (NULL,
                                         "VBoxDiskImageManagerDlg",
                                         WType_TopLevel | WDestructiveClose);
        mModelessDialog->setup (VBoxDefs::HD | VBoxDefs::CD | VBoxDefs::FD,
                                false, NULL, list);

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
        
        /* listen also to the machine state change because hard disks of running
         * VMs are inaccessible by the current design */
        connect (&vboxGlobal(), SIGNAL (machineStateChanged (const VBoxMachineStateChangeEvent &)),
                 mModelessDialog, SLOT (machineStateChanged (const VBoxMachineStateChangeEvent &)));
    }

    mModelessDialog->show();
    mModelessDialog->setWindowState (mModelessDialog->windowState() &
                                     ~WindowMinimized);
    mModelessDialog->setActiveWindow();

}


void VBoxDiskImageManagerDlg::init()
{
    polished = false;

    mToBeRefreshed = false;
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


    /* setup tab widget icons */
    twImages->setTabIconSet (twImages->page (0),
                             VBoxGlobal::iconSet ("hd_16px.png",
                                                  "hd_disabled_16px.png"));
    twImages->setTabIconSet (twImages->page (1),
                             VBoxGlobal::iconSet ("cd_16px.png",
                                                  "cd_disabled_16px.png"));
    twImages->setTabIconSet (twImages->page (2),
                             VBoxGlobal::iconSet ("fd_16px.png",
                                                  "fd_disabled_16px.png"));


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

    connect (hdsView, SIGNAL (onItem (QListViewItem*)),
             this, SLOT (mouseOnItem(QListViewItem*)));
    connect (cdsView, SIGNAL (onItem (QListViewItem*)),
             this, SLOT (mouseOnItem(QListViewItem*)));
    connect (fdsView, SIGNAL (onItem (QListViewItem*)),
             this, SLOT (mouseOnItem(QListViewItem*)));
    hdsView->setShowToolTips (false);
    cdsView->setShowToolTips (false);
    fdsView->setShowToolTips (false);

    /* status-bar currently disabled */
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

    imNewAction->setStatusTip (tr ("Create new VDI file and attach it to media list"));
    imAddAction->setStatusTip (tr ("Add existing media image file to media list"));
    // imEditAction->setStatusTip (tr ("Edit properties of selected media image file"));
    imRemoveAction->setStatusTip (tr ("Remove selected media image file from media list"));
    imReleaseAction->setStatusTip (tr ("Release selected media image file from being using in some VM"));
    imRefreshAction->setStatusTip (tr ("Refresh media image list"));

    imNewAction->setIconSet (VBoxGlobal::iconSet ("hd_16px.png", "hd_disabled_16px.png"));
    imAddAction->setIconSet (VBoxGlobal::iconSet ("cd_16px.png", "cd_disabled_16px.png"));
    // imEditAction->setIconSet (VBoxGlobal::iconSet ("guesttools_16px.png", "guesttools_disabled_16px.png"));
    imRemoveAction->setIconSet (VBoxGlobal::iconSet ("delete_16px.png", "delete_dis_16px.png"));
    imReleaseAction->setIconSet (VBoxGlobal::iconSet ("start_16px.png", "start_dis_16px.png"));
    imRefreshAction->setIconSet (VBoxGlobal::iconSet ("settings_16px.png", "settings_dis_16px.png"));

    // imEditAction->addTo (itemMenu);
    imRemoveAction->addTo (itemMenu);
    imReleaseAction->addTo (itemMenu);


    /* toolbar composing */
    toolBar = new VBoxToolBar (this, centralWidget(), "toolBar");
    toolBar->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Minimum);
    ((QVBoxLayout*)centralWidget()->layout())->insertWidget(0, toolBar);
    setUsesTextLabel (true);

    imNewAction->addTo (toolBar);
    imAddAction->addTo (toolBar);
    toolBar->addSeparator();
    // imEditAction->addTo (toolBar);
    imRemoveAction->addTo (toolBar);
    imReleaseAction->addTo (toolBar);
    toolBar->addSeparator();
    imRefreshAction->addTo (toolBar);


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
    menuBar()->insertItem (QString (tr ("&Actions")), actionMenu, 1);


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
    hdsPane1 = createInfoString (tr ("Location"), hdsContainer, 0, -1);
    hdsPane2 = createInfoString (tr ("Disk Type"), hdsContainer, 1, 0);
    hdsPane3 = createInfoString (tr ("Storage Type"), hdsContainer, 1, 1);
    hdsPane4 = createInfoString (tr ("Attached to"), hdsContainer, 2, 0);
    hdsPane5 = createInfoString (tr ("Snapshot"), hdsContainer, 2, 1);
    /* create info-pane for cd list-view */
    cdsPane1 = createInfoString (tr ("Location"), cdsContainer, 0, -1);
    cdsPane2 = createInfoString (tr ("Attached to"), cdsContainer, 1, -1);
    /* create info-pane for fd list-view */
    fdsPane1 = createInfoString (tr ("Location"), fdsContainer, 0, -1);
    fdsPane2 = createInfoString (tr ("Attached to"), fdsContainer, 1, -1);
}


QIRichLabel *VBoxDiskImageManagerDlg::createInfoString (QString name, QWidget *root, int row, int column)
{
    QLabel *nameLabel = new QLabel (name, root, "nameLabel");
    QIRichLabel *infoLabel = new QIRichLabel (root, "infoPane");

    /* prevent the name columns from being expanded */
    nameLabel->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);

    if (column == -1)
    {
        /* add qt-html tags to prevent wrapping and to have the same initial
         * height of nameLabel and infoLabel (plain text gets a height smaller
         * than rich text */
        nameLabel->setText (QString ("<nobr>%1:</nobr>").arg (name));

        ((QGridLayout *) root->layout())->addWidget (nameLabel, row, 0);
        ((QGridLayout *) root->layout())->
            addMultiCellWidget (infoLabel, row, row,
                                1, ((QGridLayout *) root->layout())->numCols() - 1);
    }
    else
    {
        /* add some spacing to the left of the name field for all columns but
         * the first one, to separate it from the value field (note that adding
         * spacing to the right is not necessary since Qt does it anyway for
         * rich text for whatever stupid reason). */
        if (column == 0)
            nameLabel->setText (QString ("<nobr>%1:</nobr>").arg (name));
        else
            nameLabel->setText (QString ("<nobr>&nbsp;&nbsp;%1:</nobr>").arg (name));

        ((QGridLayout *) root->layout())->addWidget (nameLabel, row, column * 2);
        ((QGridLayout *) root->layout())->addWidget (infoLabel, row, column * 2 + 1);
    }

    return infoLabel;
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

    updateNotice (getCurrentListView());
}


void VBoxDiskImageManagerDlg::mouseOnItem (QListViewItem *aItem)
{
    QListView *currentList = getCurrentListView();
    DiskImageItem *item = 0;
    if (aItem->rtti() == 1001)
        item = static_cast<DiskImageItem*> (aItem);
    Assert (item);

    QToolTip::add (currentList->viewport(), currentList->itemRect(item), item->getToolTip());
}


void VBoxDiskImageManagerDlg::resizeEvent (QResizeEvent*)
{
    sizeGrip->move (centralWidget()->rect().bottomRight() -
                    QPoint(sizeGrip->rect().width() - 1, sizeGrip->rect().height() - 1));

    updateNotice (getCurrentListView());
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


bool VBoxDiskImageManagerDlg::eventFilter (QObject *aObject, QEvent *aEvent)
{
    QListView *currentList = getCurrentListView();

    switch (aEvent->type())
    {
        /* Dragged object(s) has entered list-view area */
        case QEvent::DragEnter:
        {
            if (aObject == currentList)
            {
                QDragEnterEvent *dragEnterEvent =
                    static_cast<QDragEnterEvent*>(aEvent);
                dragEnterEvent->accept();
            }
            break;
        }
        /* Dragged object(s) was dropped on list-view area */
        case QEvent::Drop:
        {
            if (aObject == currentList)
            {
                QDropEvent *dropEvent = static_cast<QDropEvent*>(aEvent);

                QStringList droppedList;
                QUriDrag::decodeLocalFiles (dropEvent, droppedList);

                for (QStringList::Iterator it = droppedList.begin();
                     it != droppedList.end(); ++it)
                {
                    /* Checking dropped media type */
                    VBoxDefs::DiskType type = VBoxDefs::InvalidType;
                    if      ((*it).endsWith ("iso", false))
                    {
                        if (currentList == cdsView) type = VBoxDefs::CD;
                    }
                    else if ((*it).endsWith ("img", false))
                    {
                        if (currentList == fdsView) type = VBoxDefs::FD;
                    }
                    else if ((*it).endsWith ("vdi", false))
                    {
                        if (currentList == hdsView) type = VBoxDefs::HD;
                    }
                    /* If media type has been determined - attach this device */
                    if (type)
                        addDroppedImage(*it, type);
                }
                dropEvent->accept();
                refreshAll();
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


void VBoxDiskImageManagerDlg::addDroppedImage (QString aSource, VBoxDefs::DiskType aDiskType)
{
    if (aSource.isEmpty())
        return;

    QUuid uuid;
    switch (aDiskType)
    {
        case VBoxDefs::HD:
        {
            CVirtualDiskImage vdi = vbox.OpenVirtualDiskImage (aSource);
            if (vbox.isOk())
            {
                CHardDisk hardDisk = CUnknown (vdi);
                vbox.RegisterHardDisk (hardDisk);
            }
            break;
        }
        case VBoxDefs::CD:
        {
            CDVDImage dvdImage = vbox.OpenDVDImage (aSource, uuid);
            if (vbox.isOk())
                vbox.RegisterDVDImage (dvdImage);
            break;
        }
        case VBoxDefs::FD:
        {
            CFloppyImage floppyImage = vbox.OpenFloppyImage (aSource, uuid);
            if (vbox.isOk())
                vbox.RegisterFloppyImage (floppyImage);
            break;
        }
        default:
        {
            Assert(0);
        }
    }
}


DiskImageItem* VBoxDiskImageManagerDlg::createImageNode (QListView *aList,
                                                         DiskImageItem *aRoot,
                                                         QString aName,
                                                         QString aLabel2,
                                                         QString aLabel3,
                                                         QString aLabel4,
                                                         QString aLabel5,
                                                         QString aLabel6,
                                                         QString aLabel7,
                                                         QString aLabel8)
{
    DiskImageItem *item = 0;

    if (aList)
        item = new DiskImageItem (aList, aName,
            aLabel2, aLabel3, aLabel4, aLabel5, aLabel6, aLabel7, aLabel8);
    else if (aRoot)
        item = new DiskImageItem (aRoot, aName,
            aLabel2, aLabel3, aLabel4, aLabel5, aLabel6, aLabel7, aLabel8);
    else
        Assert(0);

    return item;
}


void VBoxDiskImageManagerDlg::invokePopup (QListViewItem *aItem, const QPoint & aPos, int)
{
    if (aItem)
        itemMenu->popup(aPos);
}


QString VBoxDiskImageManagerDlg::getDVDImageUsage (const QUuid &aId)
{
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
        usage += vbox.GetMachine (QUuid (*it)).GetName();
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
            usage += vbox.GetMachine (QUuid (*it)).GetName() + "]";
        }
    }

    return usage;
}

QString VBoxDiskImageManagerDlg::getFloppyImageUsage (const QUuid &aId)
{
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
        usage += vbox.GetMachine (QUuid (*it)).GetName();
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
            usage += vbox.GetMachine (QUuid (*it)).GetName() + "]";
        }
    }

    return usage;
}


DiskImageItem* VBoxDiskImageManagerDlg::createHdItem (QListView *aList,
                                                      DiskImageItem *aRoot,
                                                      CHardDisk &aHd)
{
    DiskImageItem *item = 0;

    QUuid uuid = aHd.GetId();
    QString src = aHd.GetLocation();
    QUuid machineId = aHd.GetMachineId();
    bool accessible = aHd.GetAccessible();

    QString usage;
    if (!machineId.isNull())
        usage = vbox.GetMachine (machineId).GetName();
    QString storageType = vboxGlobal().toString (aHd.GetStorageType());
    QString hardDiskType = vboxGlobal().hardDiskTypeString (aHd);
    QString virtualSize = accessible ?
        vboxGlobal().formatSize ((ULONG64)aHd.GetSize() * _1M) : QString ("--");
    QString actualSize = accessible ?
        vboxGlobal().formatSize (aHd.GetActualSize()) : QString ("--");
    QString snapshotName;
    if (!machineId.isNull() && !aHd.GetSnapshotId().isNull())
    {
        CSnapshot snapshot = vbox.GetMachine (machineId).
                                  GetSnapshot (aHd.GetSnapshotId());
        if (!snapshot.isNull())
            snapshotName = QString ("%1").arg (snapshot.GetName());
    }
    QFileInfo fi (src);

    item = createImageNode (
        aList, aRoot,
        fi.fileName(),
        virtualSize,
        actualSize
    );
    item->setPath (aHd.GetStorageType() == CEnums::ISCSIHardDisk ? src :
                   QDir::convertSeparators (fi.absFilePath()));
    item->setUsage (usage);
    item->setSnapshotName (snapshotName);
    item->setDiskType (hardDiskType);
    item->setStorageType (storageType);
    item->setVirtualSize (virtualSize);
    item->setActualSize (actualSize);
    item->setUuid (uuid);
    item->setMachineId (machineId);

    /* compose tool-tip information */
    if (!accessible)
    {
        item->setToolTip (tr ("<nobr><b>%1</b></nobr><br>%2")
                          .arg (item->getPath())
                          .arg (aHd.GetLastAccessError()));
    }
    else
    {
        QString tip = tr ("<nobr><b>%1</b></nobr><br>"
                          "<nobr>Disk type:&nbsp;&nbsp;%2</nobr><br>"
                          "<nobr>Storage type:&nbsp;&nbsp;%3</nobr>")
                      .arg (item->getPath())
                      .arg (item->getDiskType())
                      .arg (item->getStorageType());

        QString str = item->getUsage();
        if (!str.isNull())
            tip += tr ("<br><nobr>Attached to:&nbsp;&nbsp;%1</nobr>").arg (str);

        str = item->getSnapshotName();
        if (!str.isNull())
            tip += tr ("<br><nobr>Snapshot:&nbsp;&nbsp;%5</nobr>").arg (str);

        item->setToolTip (tip);
    }

    return item;
}


DiskImageItem* VBoxDiskImageManagerDlg::createCdItem (QListView *aList,
                                                      DiskImageItem *aRoot,
                                                      CDVDImage &aCd)
{
    DiskImageItem *item = 0;

    QUuid uuid = aCd.GetId();
    QString src = aCd.GetFilePath();
    QString usage = getDVDImageUsage (uuid);
    bool accessible = aCd.GetAccessible();

    QString size = accessible ?
        vboxGlobal().formatSize (aCd.GetSize()) : QString ("--");
    QFileInfo fi (src);

    item = createImageNode (aList, aRoot,
                            fi.fileName(),
                            size);
    item->setPath (QDir::convertSeparators (fi.absFilePath ()));
    item->setUsage (usage);
    item->setActualSize (size);
    item->setUuid (uuid);

    /* compose tool-tip information */
    if (!accessible)
    {
        /// @todo (r=dmik) correct this when GetLastAccessError() is
        //  implemented for IFloppyImage/IDVDImage
        item->setToolTip (tr ("<nobr><b>%1</b></nobr><br>%2")
                          .arg (item->getPath())
                          .arg (tr ("The image file is not accessible",
                                    "CD/DVD/Floppy")));
    }
    else
    {
        QString tip = tr ("<nobr><b>%1</b></nobr>")
                      .arg (item->getPath());

        QString str = item->getUsage();
        if (!str.isNull())
            tip += tr ("<br><nobr>Attached to:&nbsp;&nbsp;%1</nobr>").arg (str);

        item->setToolTip (tip);
    }

    return item;
}


DiskImageItem* VBoxDiskImageManagerDlg::createFdItem (QListView *aList,
                                                      DiskImageItem *aRoot,
                                                      CFloppyImage &aFd)
{
    DiskImageItem *item = 0;

    QUuid uuid = aFd.GetId();
    QString src = aFd.GetFilePath();
    QString usage = getFloppyImageUsage (uuid);
    bool accessible = aFd.GetAccessible();

    QString size = accessible ?
        vboxGlobal().formatSize (aFd.GetSize()) : QString ("--");
    QFileInfo fi (src);

    item = createImageNode (aList, aRoot,
                            fi.fileName(),
                            size);
    item->setPath (QDir::convertSeparators (fi.absFilePath ()));
    item->setUsage (usage);
    item->setActualSize (size);
    item->setUuid (uuid);

    /* compose tool-tip information */
    if (!accessible)
    {
        /// @todo (r=dmik) correct this when GetLastAccessError() is
        //  implemented for IFloppyImage/IDVDImage
        item->setToolTip (tr ("<nobr><b>%1</b></nobr><br>%2")
                          .arg (item->getPath())
                          .arg (tr ("The image file is not accessible",
                                    "CD/DVD/Floppy")));
    }
    else
    {
        QString tip = tr ("<nobr><b>%1</b></nobr>")
                      .arg (item->getPath());

        QString str = item->getUsage();
        if (!str.isNull())
            tip += tr ("<br><nobr>Attached to:&nbsp;&nbsp;%1</nobr>").arg (str);

        item->setToolTip (tip);
    }

    return item;
}


void VBoxDiskImageManagerDlg::createHdChildren (DiskImageItem *aRoot,
                                                CHardDisk &aHd)
{
    CHardDiskEnumerator enumerator = aHd.GetChildren().Enumerate();
    while (enumerator.HasMore())
    {
        CHardDisk subHd = enumerator.GetNext();
        DiskImageItem *subItem = createHdItem (0, aRoot, subHd);
        createHdChildren (subItem, subHd);
    }
}


void VBoxDiskImageManagerDlg::insertMedia (const VBoxMedia &aMedia)
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
            item = createHdItem (hdsView, 0, hd);
            createHdChildren (item, hd);
            hdsView->adjustColumn (1);
            hdsView->adjustColumn (2);
            break;
        }
        case VBoxDefs::CD:
        {
            CDVDImage cd = aMedia.disk;
            item = createCdItem (cdsView, 0, cd);
            cdsView->adjustColumn (1);
            break;
        }
        case VBoxDefs::FD:
        {
            CFloppyImage fd = aMedia.disk;
            item = createFdItem (fdsView, 0, fd);
            fdsView->adjustColumn (1);
            break;
        }
        default:
        {
            AssertMsgFailed (("Invalid aMedia type\n"));
            break;
        }
    }

    if (aMedia.status == VBoxMedia::Inaccessible)
        item->setPixmap (0, pxInaccessible);
    else if (aMedia.status == VBoxMedia::Error)
        item->setPixmap (0, pxErroneous);
}


void VBoxDiskImageManagerDlg::setup (int aType, bool aDoSelect,
                                     const QUuid *aTargetVMId,
                                     const VBoxMediaList *mediaList,
                                     CMachine machine)
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

    /* listen to "media enumeration" signals */
    connect (&vboxGlobal(), SIGNAL (mediaEnumerated (const VBoxMedia &)),
             this, SLOT (mediaEnumerated (const VBoxMedia &)));
    /* listen to "media enumeration finished" signals */
    connect (&vboxGlobal(), SIGNAL (mediaEnumerated (const VBoxMediaList &)),
             this, SLOT (mediaEnumerated (const VBoxMediaList &)));

    /* insert already enumerated media */
    VBoxMediaList list;
    if (mediaList)
        list = *mediaList;
    else
        list = vboxGlobal().currentMediaList();

    VBoxMediaList::const_iterator it;
    for (it = list.begin(); it != list.end(); ++ it)
        insertMedia (*it);

    if (!mediaList)
    {
        /* only start enumerating media if we haven't been supplied with a list
         * (it's ok if the enumeration has been already started, nothing will
         * happen) */
        refreshAll();
    }
}


void VBoxDiskImageManagerDlg::mediaEnumerated (const VBoxMedia &)
{
    /* This function is unnecessary since media devices enumeration
       performs on full VBoxMediaList completion */
}


void VBoxDiskImageManagerDlg::mediaEnumerated (const VBoxMediaList &aList)
{
    if (!mToBeRefreshed) return;

    hdsView->clear();
    cdsView->clear();
    fdsView->clear();

    VBoxMediaList::const_iterator it;
    for (it = aList.begin(); it != aList.end(); ++ it)
        insertMedia (*it);

    removeNotice (hdsView), removeNotice (cdsView), removeNotice (fdsView);

    cdsView->setCurrentItem (cdsView->firstChild());
    fdsView->setCurrentItem (fdsView->firstChild());
    hdsView->setCurrentItem (hdsView->firstChild());
    cdsView->setSelected (cdsView->firstChild(), true);
    fdsView->setSelected (fdsView->firstChild(), true);
    hdsView->setSelected (hdsView->firstChild(), true);
    processCurrentChanged();

    imRefreshAction->setEnabled (true);
    unsetCursor();

    mToBeRefreshed = false;
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


void VBoxDiskImageManagerDlg::createNotice (QListView *aListView)
{
    QString warning = tr ("...enumerating media...");
    QLabel *notice = new QLabel (warning, aListView->viewport(), "notice");
    notice->setFrameShape (QFrame::Box);
    notice->setMargin (10);
    notice->adjustSize();
    aListView->setEnabled (false);
}


void VBoxDiskImageManagerDlg::updateNotice (QListView *aListView)
{
    QLabel *notice = (QLabel*)aListView->child ("notice");
    if (notice)
        notice->move ((((QListView*)notice->parent())->width() - notice->width())/2,
                      (((QListView*)notice->parent())->height() - notice->height())/2);
}


void VBoxDiskImageManagerDlg::removeNotice (QListView *aListView)
{
    QLabel *notice = (QLabel*)aListView->child ("notice");
    if (notice) delete notice;
    aListView->setEnabled (true);
}


void VBoxDiskImageManagerDlg::refreshAll()
{
    if (mToBeRefreshed) return;
    mToBeRefreshed = true;

    hdsView->clear(), cdsView->clear(), fdsView->clear();
    createNotice (hdsView), createNotice (cdsView), createNotice (fdsView);
    imRefreshAction->setEnabled (false);
    setCursor (QCursor (BusyCursor));

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
        QUuid machineId = vbox.GetHardDisk (itemId).GetMachineId();
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


void VBoxDiskImageManagerDlg::setCurrentItem (QListView *aListView, DiskImageItem *aItem)
{
    if (aItem)
    {
        aListView->setCurrentItem (aItem);
        aListView->setSelected (aListView->currentItem(), true);
        aListView->adjustColumn (1);
        aListView->adjustColumn (2);
    }
    aListView->setFocus();
    processCurrentChanged (aListView->currentItem());
}


void VBoxDiskImageManagerDlg::processCurrentChanged()
{
    QListView *currentList = getCurrentListView();
    currentList->setFocus();
    updateNotice (currentList);

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

    processCurrentChanged (currentList->selectedItem());
}

void VBoxDiskImageManagerDlg::processCurrentChanged (QListViewItem *aItem)
{
    DiskImageItem *item = aItem && aItem->rtti() == 1001 ?
        static_cast<DiskImageItem*> (aItem) : 0;

    bool modifyEnabled  = item &&  item->getUsage().isNull();
    bool releaseEnabled = item && !item->getUsage().isNull() &&
                          checkImage (item) &&
                          !item->parent() && !item->firstChild() &&
                          item->getSnapshotName().isNull();
    bool newEnabled = getCurrentListView() == hdsView ? true : false;

    // imEditAction->setEnabled (modifyEnabled);
    imRemoveAction->setEnabled (modifyEnabled);
    imReleaseAction->setEnabled (releaseEnabled);
    imNewAction->setEnabled (newEnabled);

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
        if (getCurrentListView() == hdsView)
        {
            hdsPane1->setText (item->getInformation (item->getPath(), true, "end"));
            hdsPane2->setText (item->getInformation (item->getDiskType(), false));
            hdsPane3->setText (item->getInformation (item->getStorageType(), false));
            hdsPane4->setText (item->getInformation (item->getUsage()));
            hdsPane5->setText (item->getInformation (item->getSnapshotName()));
        }
        else if (getCurrentListView() == cdsView)
        {
            cdsPane1->setText (item->getInformation (item->getPath(), true, "end"));
            cdsPane2->setText (item->getInformation (item->getUsage()));
        }
        else if (getCurrentListView() == fdsView)
        {
            fdsPane1->setText (item->getInformation (item->getPath(), true, "end"));
            fdsPane2->setText (item->getInformation (item->getUsage()));
        }
    }
}


void VBoxDiskImageManagerDlg::processPressed (QListViewItem * aItem)
{
    if (!aItem)
    {
        QListView *currentList = getCurrentListView();
        currentList->setSelected (currentList->currentItem(), true);
    }
    processCurrentChanged();
}


void VBoxDiskImageManagerDlg::newImage()
{
    AssertReturnVoid (getCurrentListView() == hdsView);

    VBoxNewHDWzd dlg (this, "VBoxNewHDWzd");

    if (dlg.exec() == QDialog::Accepted)
    {
        CHardDisk hd = dlg.hardDisk();
        DiskImageItem *createdItem = createHdItem (hdsView, 0, hd);
        setCurrentItem (hdsView, createdItem);
        /* synchronize modeless dialog if present */
        if (mModelessDialog && mModelessDialog != this)
            mModelessDialog->createHdItem (mModelessDialog->hdsView, 0, hd);
    }
}


void VBoxDiskImageManagerDlg::addImage()
{
    QListView *currentList = getCurrentListView();
    DiskImageItem *item =
        currentList->currentItem() && currentList->currentItem()->rtti() == 1001 ?
        static_cast<DiskImageItem*> (currentList->currentItem()) : 0;

    QString dir;
    if (item)
        dir = item->getPath().stripWhiteSpace();

    if (!dir)
        if (currentList == hdsView)
            dir = vbox.GetSystemProperties().GetDefaultVDIFolder();
    if (!dir || !QFileInfo (dir).exists())
        dir = vbox.GetHomeFolder();

    QString title;
    QString filter;
    VBoxDefs::DiskType type = VBoxDefs::InvalidType;

    if (currentList == hdsView) {
        filter = tr( "Hard disk images (*.vdi)" );
        title = tr( "Select a hard disk image file" );
        type = VBoxDefs::HD;
    } else if (currentList == cdsView) {
        filter = tr( "CDROM images (*.iso)" );
        title = tr( "Select a DVD/CD-ROM disk image file" );
        type = VBoxDefs::CD;
    } else if (currentList == fdsView) {
        filter = tr( "Floppy images (*.img)" );
        title = tr( "Select a floppy disk image file" );
        type = VBoxDefs::FD;
    } else {
        AssertMsgFailed (("Root list should be equal to hdsView, cdsView or fdsView"));
    }

    QString src = QFileDialog::getOpenFileName (dir, filter,
                                                this, "AddDiskImageDialog",
                                                title);

    DiskImageItem *createdItem = 0;
    if (src)
    {
        QUuid uuid;
        if (currentList == hdsView)
        {
            CVirtualDiskImage vdi = vbox.OpenVirtualDiskImage (src);
            if (vbox.isOk())
            {
                /// @todo (dmik) later, change wrappers so that converting
                //  to CUnknown is not necessary for cross-assignments
                CHardDisk hardDisk = CUnknown (vdi);
                vbox.RegisterHardDisk (hardDisk);
                if (vbox.isOk())
                {
                    createdItem = createHdItem (hdsView, 0, hardDisk);
                    /* synchronize modeless dialog if present */
                    if (mModelessDialog && mModelessDialog != this)
                        mModelessDialog->createHdItem (mModelessDialog->hdsView, 0, hardDisk);
                }
            }
        }
        else
        if (currentList == cdsView)
        {
            CDVDImage dvdImage = vbox.OpenDVDImage (src, uuid);
            if (vbox.isOk())
            {
                vbox.RegisterDVDImage (dvdImage);
                if (vbox.isOk())
                {
                    createdItem = createCdItem (cdsView, 0, dvdImage);
                    /* synchronize modeless dialog if present */
                    if (mModelessDialog && mModelessDialog != this)
                        mModelessDialog->createCdItem (mModelessDialog->cdsView, 0, dvdImage);
                }
            }
        }
        else
        if (currentList == fdsView)
        {
            CFloppyImage floppyImage = vbox.OpenFloppyImage (src, uuid);
            if (vbox.isOk())
            {
                vbox.RegisterFloppyImage (floppyImage);
                if (vbox.isOk())
                {
                    createdItem = createFdItem (fdsView, 0, floppyImage);
                    /* synchronize modeless dialog if present */
                    if (mModelessDialog && mModelessDialog != this)
                        mModelessDialog->createFdItem (mModelessDialog->fdsView, 0, floppyImage);
                }
            }
        }

        if (!vbox.isOk())
        {
            vboxProblem().cannotRegisterMedia (this, vbox, type, src);
        }
    }
    /* set current item */
    setCurrentItem (currentList, createdItem);
}


void VBoxDiskImageManagerDlg::removeImage()
{
    QListView *currentList = getCurrentListView();
    DiskImageItem *item =
        currentList->currentItem() && currentList->currentItem()->rtti() == 1001 ?
        static_cast<DiskImageItem*> (currentList->currentItem()) : 0;
    AssertMsg (item, ("Current item must not be null"));
    QString removedItemName = item->getName();

    QString src = item->getPath().stripWhiteSpace();
    QUuid uuid = QUuid (item->getUuid());
    VBoxDefs::DiskType type = VBoxDefs::InvalidType;

    if (currentList == hdsView)
    {
        type = VBoxDefs::HD;
        int deleteImage;
        if (vbox.GetHardDisk (uuid).GetStorageType() == CEnums::VirtualDiskImage &&
            vbox.GetHardDisk (uuid).GetAccessible())
        {
            deleteImage = vboxProblem().confirmHardDiskImageDeletion (this, src);
        }
        else
        {
            deleteImage = vboxProblem().confirmHardDiskUnregister (this, src);
        }
        if (deleteImage == QIMessageBox::Cancel)
            return;
        CHardDisk hd = vbox.UnregisterHardDisk (uuid);
        if (vbox.isOk() && deleteImage == QIMessageBox::Yes)
        {
            /// @todo (dmik) later, change wrappers so that converting
            //  to CUnknown is not necessary for cross-assignments
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

    if (!vbox.isOk())
    {
        vboxProblem().cannotUnregisterMedia (this, vbox, type, src);
    }
    else
    {
        delete item;
        setCurrentItem (currentList, (DiskImageItem*)currentList->currentItem());
        /* synchronize modeless dialog if present */
        if (mModelessDialog && mModelessDialog != this)
        {
            DiskImageItem *itemToRemove = 0;
            QListView *modelessView = 0;
            if (currentList == hdsView)
                modelessView = mModelessDialog->hdsView;
            else if (currentList == cdsView)
                modelessView = mModelessDialog->cdsView;
            else if (currentList == fdsView)
                modelessView = mModelessDialog->fdsView;
            itemToRemove = static_cast<DiskImageItem*>
                (modelessView->findItem (removedItemName, 0));
            delete itemToRemove;
            if (modelessView->currentItem())
                modelessView->setSelected (modelessView->currentItem(), true);
            mModelessDialog->processCurrentChanged();
        }
    }
}


void VBoxDiskImageManagerDlg::releaseImage()
{
    QListView *currentList = getCurrentListView();
    DiskImageItem *item =
        currentList->currentItem() && currentList->currentItem()->rtti() == 1001 ?
        static_cast<DiskImageItem*> (currentList->currentItem()) : 0;
    AssertMsg (item, ("Current item must not be null"));
    QString releasedItemName = item->getName();
    QString usageAfterRelease;

    QUuid itemId = QUuid (item->getUuid());
    AssertMsg (!itemId.isNull(), ("Current item must have uuid"));

    QUuid machineId;
    /* if it is a hard disk sub-item: */
    if (currentList == hdsView)
    {
        machineId = vbox.GetHardDisk (itemId).GetMachineId();
        if (vboxProblem().confirmReleaseImage (this,
                           vbox.GetMachine(machineId).GetName()))
        {
            releaseDisk (machineId, itemId, VBoxDefs::HD);

            /* re-request current usage */
            machineId = vbox.GetHardDisk (itemId).GetMachineId();
            usageAfterRelease = machineId.isNull() ? QString::null :
                                vbox.GetMachine (machineId).GetName();
            item->setUsage (usageAfterRelease);
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

            /* re-request current usage */
            usageAfterRelease = getDVDImageUsage (itemId);
            item->setUsage (usageAfterRelease);
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

            /* re-request current usage */
            usageAfterRelease = getFloppyImageUsage (itemId);
            item->setUsage (usageAfterRelease);
        }
    }
    processCurrentChanged (item);

    /* processing modeless dialog */
    if (mModelessDialog && mModelessDialog != this)
    {
        DiskImageItem *itemToRelease = 0;
        QListView *modelessView = 0;
        if (currentList == hdsView)
            modelessView = mModelessDialog->hdsView;
        else if (currentList == cdsView)
            modelessView = mModelessDialog->cdsView;
        else if (currentList == fdsView)
            modelessView = mModelessDialog->fdsView;
        itemToRelease = static_cast<DiskImageItem*>
            (modelessView->findItem (releasedItemName, 0));
        itemToRelease->setUsage (usageAfterRelease);
        mModelessDialog->processCurrentChanged();
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
            AssertFailed();
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

    if ( currentList->selectedItem() &&
         currentList->selectedItem()->rtti() == 1001 )
        uuid = QUuid (static_cast<DiskImageItem *>(currentList->selectedItem())
                      ->getUuid());

    return uuid;
}


QString VBoxDiskImageManagerDlg::getSelectedPath()
{
    QListView *currentList = getCurrentListView();
    QString path;

    if ( currentList->selectedItem() && currentList->selectedItem()->rtti() == 1001 )
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


void VBoxDiskImageManagerDlg::uploadCurrentList (QStringList &aNames,
                                                 QStringList &aKeys,
                                                 const QUuid &aMachineId)
{
    QListView *currentList = getCurrentListView();
    DiskImageItem *item = 0;
    if (currentList->firstChild() &&
        currentList->firstChild()->rtti() == 1001)
        item = static_cast<DiskImageItem*> (currentList->firstChild());
    Assert (item);

    do {
        if (QUuid (item->getMachineId()).isNull() ||
            QUuid (item->getMachineId()) == aMachineId)
        {
            aNames << QString ("%1 (%2)")
                      .arg (item->getName().stripWhiteSpace())
                      .arg (item->getPath().stripWhiteSpace());
            aKeys  << item->getUuid();
        }
        item = item->nextSibling();
    } while (item);
}
