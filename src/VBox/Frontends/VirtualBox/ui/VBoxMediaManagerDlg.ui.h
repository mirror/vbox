/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * "Virtual Media Manager" dialog UI include (Qt Designer)
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

/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you wish to add, delete or rename functions or slots use
** Qt Designer which will update this file, preserving your code. Create an
** init() function in place of a constructor, and a destroy() function in
** place of a destructor.
*****************************************************************************/


/**
 * List view items for VBoxMedium objects.
 */
class MediaItem : public QListViewItem
{
public:

    enum { TypeId = 1001 };

    MediaItem (MediaItem *aParent, const VBoxMedium &aMedium,
               VBoxMediaManagerDlg *aManager)
        : QListViewItem (aParent), mMedium (aMedium)
        , mManager (aManager)
        { refresh(); }

    MediaItem (QListView *aParent, const VBoxMedium &aMedium,
               VBoxMediaManagerDlg *aManager)
        : QListViewItem (aParent), mMedium (aMedium)
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

    QUuid id() const { return mMedium.id(); }
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

    // QListViewItem interface

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

    void paintCell (QPainter *aPainter, const QColorGroup &aColorGroup,
                    int aColumn, int aWidth, int aSlign)
    {
        QColorGroup cGroup (aColorGroup);
        /* KMediaState_NotCreated is abused to mean "not checked" */
        if (mMedium.state() == KMediaState_NotCreated)
            cGroup.setColor (QColorGroup::Text, cGroup.mid());
        QListViewItem::paintCell (aPainter, cGroup, aColumn, aWidth, aSlign);
    }

protected:

    void refresh();

private:

    VBoxMedium mMedium;

    VBoxMediaManagerDlg *mManager;
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
    /* fill in columns */

    setPixmap (0, mMedium.icon (!mManager->showDiffs(),
                                mManager->inAttachMode()));

    setText (0, mMedium.name (!mManager->showDiffs()));
    setText (1, mMedium.logicalSize (!mManager->showDiffs()));
    setText (2, mMedium.size (!mManager->showDiffs()));
}


/**
 * Iterator for MediaItem.
 */
class MediaItemIterator : public QListViewItemIterator
{
public:

    MediaItemIterator (QListView* aList)
        : QListViewItemIterator (aList) {}

    MediaItem *operator*()
    {
        QListViewItem *item = QListViewItemIterator::operator*();
        return item && item->rtti() == MediaItem::TypeId ?
            static_cast <MediaItem *> (item) : 0;
    }

    MediaItemIterator& operator++()
    {
        return (MediaItemIterator&) QListViewItemIterator::operator++();
    }
};


/**
 * COmbines QLabel with QIRichLabel.
 */
class InfoPaneLabel : public QIRichLabel
{
public:

    InfoPaneLabel (QWidget *aParent, QLabel *aLabel = 0)
        : QIRichLabel (aParent, "infoLabel"), mLabel (aLabel) {}

    QLabel* label() { return mLabel; }

private:

    QLabel *mLabel;
};


/**
 * Modifies the given text string for QIRichLabel so that it optionally uses
 * the <compact> syntax controlling visual text "compression" with elipsis.
 *
 * @param aText     Original text string.
 * @param aCompact  @c true if "compression" should be activated.
 * @param aElipsis  Where to put the elipsis (see <compact> in QIRichLabel).
 *
 * @return Modified text string.
 */
static QString richLabel (const QString &aText, bool aCompact = true,
                          const QString &aElipsis = "middle")
{
    Assert (!aText.isEmpty());
    QString text = aText.isEmpty() ? "???" : aText;

    if (aCompact)
        text = QString ("<nobr><compact elipsis=\"%1\">%2</compact></nobr>")
            .arg (aElipsis, text);
    else
        text = QString ("<nobr>%1</nobr>").arg (text);

    return text;
}


////////////////////////////////////////////////////////////////////////////////


VBoxMediaManagerDlg *VBoxMediaManagerDlg::mModelessDialog = 0;


void VBoxMediaManagerDlg::showModeless (bool aRefresh /* = true */)
{
    if (!mModelessDialog)
    {
        mModelessDialog =
            new VBoxMediaManagerDlg (NULL, "VBoxMediaManagerDlg",
                                     WType_TopLevel | WDestructiveClose);
        mModelessDialog->setup (VBoxDefs::MediaType_All,
                                false /* aDoSelect */, aRefresh);

        /* listen to events that may change the media status and refresh
         * the contents of the modeless dialog */
        /// @todo refreshAll() may be slow, so it may be better to analyze
        //  event details and update only what is changed */
        connect (&vboxGlobal(),
                 SIGNAL (machineDataChanged (const VBoxMachineDataChangeEvent &)),
                 mModelessDialog, SLOT (refreshAll()));
        connect (&vboxGlobal(), SIGNAL
                 (machineRegistered (const VBoxMachineRegisteredEvent &)),
                 mModelessDialog, SLOT (refreshAll()));
        connect (&vboxGlobal(),
                 SIGNAL (snapshotChanged (const VBoxSnapshotEvent &)),
                 mModelessDialog, SLOT (refreshAll()));
    }

    mModelessDialog->show();
    mModelessDialog->setWindowState (mModelessDialog->windowState() &
                                     ~WindowMinimized);
    mModelessDialog->setActiveWindow();
}


void VBoxMediaManagerDlg::init()
{
    mIsPolished = false;

    mInLoop = false;

    defaultButton = searchDefaultButton();

    mVBox = vboxGlobal().virtualBox();
    Assert (!mVBox.isNull());

    setIcon (QPixmap::fromMimeSource ("diskim_16px.png"));

    mType = VBoxDefs::MediaType_Invalid;

    mShowDiffs = true;

    mHardDiskIconSet = VBoxGlobal::iconSet ("hd_16px.png", "hd_disabled_16px.png");
    mDVDImageIconSet = VBoxGlobal::iconSet ("cd_16px.png", "cd_disabled_16px.png");
    mFloppyImageIconSet = VBoxGlobal::iconSet ("fd_16px.png", "fd_disabled_16px.png");

    mHardDisksInaccessible =
        mDVDImagesInaccessible =
            mFloppyImagesInaccessible = false;

    /* setup tab widget icons */
    mMediaTabs->setTabIconSet (mMediaTabs->page (0), mHardDiskIconSet);
    mMediaTabs->setTabIconSet (mMediaTabs->page (1), mDVDImageIconSet);
    mMediaTabs->setTabIconSet (mMediaTabs->page (2), mFloppyImageIconSet);

    /* setup media list views */

    mHardDiskView->setColumnAlignment (1, Qt::AlignRight);
    mHardDiskView->setColumnAlignment (2, Qt::AlignRight);
    mHardDiskView->header()->setStretchEnabled (false);
    mHardDiskView->header()->setStretchEnabled (true, 0);

    mDVDView->setColumnAlignment (1, Qt::AlignRight);
    mDVDView->header()->setStretchEnabled (false);
    mDVDView->header()->setStretchEnabled (true, 0);

    mFloppyView->setColumnAlignment (1, Qt::AlignRight);
    mFloppyView->header()->setStretchEnabled (false);
    mFloppyView->header()->setStretchEnabled (true, 0);

    /* setup media tooltip signals */

    mHardDiskView->setShowToolTips (false);
    mDVDView->setShowToolTips (false);
    mFloppyView->setShowToolTips (false);

    connect (mHardDiskView, SIGNAL (onItem (QListViewItem *)),
             this, SLOT (mouseOnItem (QListViewItem *)));
    connect (mDVDView, SIGNAL (onItem (QListViewItem *)),
             this, SLOT (mouseOnItem (QListViewItem *)));
    connect (mFloppyView, SIGNAL (onItem (QListViewItem *)),
             this, SLOT (mouseOnItem (QListViewItem *)));

    /* status-bar currently disabled */
    /// @todo we must enable it and disable our size grip hack!
    /// (at least, to have action help text showh)
    statusBar()->setHidden (true);


    /* context menu composing */
    mItemMenu = new QPopupMenu (this, "mItemMenu");

    mNewAction = new QAction (this, "mNewAction");
    mAddAction = new QAction (this, "mAddAction");
    // mEditAction = new QAction (this, "mEditAction");
    mRemoveAction = new QAction (this, "mRemoveAction");
    mReleaseAction = new QAction (this, "mReleaseAction");
    mRefreshAction = new QAction (this, "mRefreshAction");

    connect (mNewAction, SIGNAL (activated()),
             this, SLOT (doNewMedium()));
    connect (mAddAction, SIGNAL (activated()),
             this, SLOT (doAddMedium()));
    // connect (mEditAction, SIGNAL (activated()),
    //          this, SLOT (doEditMedia()));
    connect (mRemoveAction, SIGNAL (activated()),
             this, SLOT (doRemoveMedium()));
    connect (mReleaseAction, SIGNAL (activated()),
             this, SLOT (doReleaseMedium()));
    connect (mRefreshAction, SIGNAL (activated()),
             this, SLOT (refreshAll()));

    mNewAction->setIconSet (VBoxGlobal::iconSetEx (
        "vdm_new_22px.png", "vdm_new_16px.png",
        "vdm_new_disabled_22px.png", "vdm_new_disabled_16px.png"));
    mAddAction->setIconSet (VBoxGlobal::iconSetEx (
        "vdm_add_22px.png", "vdm_add_16px.png",
        "vdm_add_disabled_22px.png", "vdm_add_disabled_16px.png"));
    // mEditAction->setIconSet (VBoxGlobal::iconSet (
    //    "guesttools_16px.png", "guesttools_disabled_16px.png"));
    mRemoveAction->setIconSet (VBoxGlobal::iconSetEx (
        "vdm_remove_22px.png", "vdm_remove_16px.png",
        "vdm_remove_disabled_22px.png", "vdm_remove_disabled_16px.png"));
    mReleaseAction->setIconSet (VBoxGlobal::iconSetEx (
        "vdm_release_22px.png", "vdm_release_16px.png",
        "vdm_release_disabled_22px.png", "vdm_release_disabled_16px.png"));
    mRefreshAction->setIconSet (VBoxGlobal::iconSetEx (
        "refresh_22px.png", "refresh_16px.png",
        "refresh_disabled_22px.png", "refresh_disabled_16px.png"));

    // mEditAction->addTo (mItemMenu);
    mRemoveAction->addTo (mItemMenu);
    mReleaseAction->addTo (mItemMenu);


    /* toolbar composing */
    mToolBar = new VBoxToolBar (this, centralWidget(), "mToolBar");
    mToolBar->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Minimum);
    ((QVBoxLayout*)centralWidget()->layout())->insertWidget(0, mToolBar);

    mToolBar->setUsesTextLabel (true);
    mToolBar->setUsesBigPixmaps (true);

    mNewAction->addTo (mToolBar);
    mAddAction->addTo (mToolBar);
    mToolBar->addSeparator();
    // mEditAction->addTo (mToolBar);
    mRemoveAction->addTo (mToolBar);
    mReleaseAction->addTo (mToolBar);
    mToolBar->addSeparator();
    mRefreshAction->addTo (mToolBar);
#ifdef Q_WS_MAC
    mToolBar->setMacStyle();
#endif

    /* menu bar */
    QPopupMenu *actionMenu = new QPopupMenu (this, "actionMenu");
    mNewAction->addTo (actionMenu);
    mAddAction->addTo (actionMenu);
    actionMenu->insertSeparator();
    // mEditAction->addTo (mToolBar);
    mRemoveAction->addTo (actionMenu);
    mReleaseAction->addTo (actionMenu);
    actionMenu->insertSeparator();
    mRefreshAction->addTo (actionMenu);
    menuBar()->insertItem (QString::null, actionMenu, 1);


    /* setup size grip */
    sizeGrip = new QSizeGrip (centralWidget(), "sizeGrip");
    sizeGrip->resize (sizeGrip->sizeHint());
    sizeGrip->stackUnder(buttonOk);

    /* setup information pane */
    QApplication::setGlobalMouseTracking (true);
    qApp->installEventFilter (this);
    /* setup information pane layouts */
    QGridLayout *mHDInfoPaneLayout = new QGridLayout (mHDInfoPane, 4, 4);
    mHDInfoPaneLayout->setMargin (10);
    QGridLayout *mDVDInfoPaneLayout = new QGridLayout (mDVDInfoPane, 2, 4);
    mDVDInfoPaneLayout->setMargin (10);
    QGridLayout *mFloppyInfoPaneLayout = new QGridLayout (mFloppyInfoPane, 2, 4);
    mFloppyInfoPaneLayout->setMargin (10);
    /* create info-pane for hd list-view */
    createInfoString (mHDLocationLabel, mHDInfoPane, 0, -1);
    createInfoString (mHDTypeLabel, mHDInfoPane, 1, -1);
    createInfoString (mHDUsageLabel, mHDInfoPane, 2, -1);
    /* create info-pane for cd list-view */
    createInfoString (mDVDLocationLabel, mDVDInfoPane, 0, -1);
    createInfoString (mDVDUsageLabel, mDVDInfoPane, 1, -1);
    /* create info-pane for fd list-view */
    createInfoString (mFloppyLocationLabel, mFloppyInfoPane, 0, -1);
    createInfoString (mFloppyUsageLabel, mFloppyInfoPane, 1, -1);


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


void VBoxMediaManagerDlg::languageChangeImp()
{
    mNewAction->setMenuText (tr ("&New..."));
    mAddAction->setMenuText (tr ("&Add..."));
    // mEditAction->setMenuText (tr ("&Edit..."));
    mRemoveAction->setMenuText (tr ("R&emove"));
    mReleaseAction->setMenuText (tr ("Re&lease"));
    mRefreshAction->setMenuText (tr ("Re&fresh"));

    mNewAction->setText (tr ("New"));
    mAddAction->setText (tr ("Add"));
    // mEditAction->setText (tr ("Edit"));
    mRemoveAction->setText (tr ("Remove"));
    mReleaseAction->setText (tr ("Release"));
    mRefreshAction->setText (tr ("Refresh"));

    mNewAction->setAccel (tr ("Ctrl+N"));
    mAddAction->setAccel (tr ("Insert"));
    // mEditAction->setAccel (tr ("Ctrl+E"));
    mRemoveAction->setAccel (tr ("Del"));
    mReleaseAction->setAccel (tr ("Ctrl+L"));
    mRefreshAction->setAccel (tr ("Ctrl+R"));

    mNewAction->setStatusTip (tr ("Create a new virtual hard disk"));
    mAddAction->setStatusTip (tr ("Add an existing medium"));
    // mEditAction->setStatusTip (tr ("Edit the properties of the selected medium"));
    mRemoveAction->setStatusTip (tr ("Remove the selected medium"));
    mReleaseAction->setStatusTip (tr ("Release the selected medium by detaching it from the machines"));
    mRefreshAction->setStatusTip (tr ("Refresh the media list"));

    if (menuBar()->findItem(1))
        menuBar()->findItem(1)->setText (tr ("&Actions"));

    /* set labels for the info pane (makes sense to keep the format in sync with
     * VBoxMedium::refresh()) */

    mHDLocationLabel->label()->
        setText (QString ("<nobr>%1:</nobr>").arg (tr ("Location")));
    mHDTypeLabel->label()->
        setText (QString ("<nobr>%1:</nobr>").arg (tr ("Type (Format)")));
    mHDUsageLabel->label()->
        setText (QString ("<nobr>%1:</nobr>").arg (tr ("Attached to")));

    mDVDLocationLabel->label()->
        setText (QString ("<nobr>%1:</nobr>").arg (tr ("Location")));
    mDVDUsageLabel->label()->
        setText (QString ("<nobr>%1:</nobr>").arg (tr ("Attached to")));

    mFloppyLocationLabel->label()->
        setText (QString ("<nobr>%1:</nobr>").arg (tr ("Location")));
    mFloppyUsageLabel->label()->
        setText (QString ("<nobr>%1:</nobr>").arg (tr ("Attached to")));

    mProgressText->setText (tr ("Checking accessibility"));

    if (mHardDiskView->childCount() ||
        mDVDView->childCount() ||
        mFloppyView->childCount())
        refreshAll();
}


void VBoxMediaManagerDlg::createInfoString (InfoPaneLabel *&aInfo,
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


void VBoxMediaManagerDlg::showEvent (QShowEvent *e)
{
    QMainWindow::showEvent (e);

    /* one may think that QWidget::polish() is the right place to do things
     * below, but apparently, by the time when QWidget::polish() is called,
     * the widget style & layout are not fully done, at least the minimum
     * size hint is not properly calculated. Since this is sometimes necessary,
     * we provide our own "polish" implementation. */

    if (mIsPolished)
        return;

    mIsPolished = true;

    VBoxGlobal::centerWidget (this, parentWidget());
}


void VBoxMediaManagerDlg::mouseOnItem (QListViewItem *aItem)
{
    QString tip;
    switch (aItem->rtti())
    {
        case MediaItem::TypeId:
            tip = static_cast <MediaItem *> (aItem)->toolTip();
            break;
        default:
            AssertFailedReturnVoid();
    }

    QListView *currentList = currentListView();
    QToolTip::add (currentList->viewport(), currentList->itemRect (aItem), tip);
}


void VBoxMediaManagerDlg::resizeEvent (QResizeEvent*)
{
    sizeGrip->move (centralWidget()->rect().bottomRight() -
                    QPoint(sizeGrip->rect().width() - 1, sizeGrip->rect().height() - 1));
}


void VBoxMediaManagerDlg::closeEvent (QCloseEvent *aEvent)
{
    mModelessDialog = 0;
    aEvent->accept();
}


void VBoxMediaManagerDlg::keyPressEvent (QKeyEvent *aEvent)
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


QPushButton* VBoxMediaManagerDlg::searchDefaultButton()
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


int  VBoxMediaManagerDlg::result() { return mRescode; }
void VBoxMediaManagerDlg::setResult (int aRescode) { mRescode = aRescode; }
void VBoxMediaManagerDlg::accept() { done( Accepted ); }
void VBoxMediaManagerDlg::reject() { done( Rejected ); }

int  VBoxMediaManagerDlg::exec()
{
    setResult (0);

    if (mInLoop) return result();
    show();
    mInLoop = true;
    qApp->eventLoop()->enterLoop();
    mInLoop = false;

    return result();
}

void VBoxMediaManagerDlg::done (int aResult)
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


QListView* VBoxMediaManagerDlg::currentListView()
{
    QListView *clv = static_cast <QListView *> (mMediaTabs->currentPage()->
        queryList ("QListView")->getFirst());
    Assert (clv);
    return clv;
}

QListView* VBoxMediaManagerDlg::listView (VBoxDefs::MediaType aType)
{
    switch (aType)
    {
        case VBoxDefs::MediaType_HardDisk:
            return mHardDiskView;
        case VBoxDefs::MediaType_DVD:
            return mDVDView;
        case VBoxDefs::MediaType_Floppy:
            return mFloppyView;
        default:
            break;
    }

    AssertFailed();
    return NULL;
}

bool VBoxMediaManagerDlg::eventFilter (QObject *aObject, QEvent *aEvent)
{
    QListView *currentList = currentListView();

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


bool VBoxMediaManagerDlg::event (QEvent *aEvent)
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


void VBoxMediaManagerDlg::addDroppedImages (QStringList *aDroppedList)
{
    QListView *currentList = currentListView();

    for (QStringList::Iterator it = (*aDroppedList).begin();
         it != (*aDroppedList).end(); ++it)
    {
        QString loc = *it;
        /* Check dropped media type */
        /// @todo On OS/2 and windows (and mac?) extension checks should be case
        /// insensitive, as OPPOSED to linux and the rest where case matters.
        VBoxDefs::MediaType type = VBoxDefs::MediaType_Invalid;
        if      (loc.endsWith (".iso", false))
        {
            if (currentList == mDVDView)
                type = VBoxDefs::MediaType_DVD;
        }
        else if (loc.endsWith (".img", false))
        {
            if (currentList == mFloppyView)
                type = VBoxDefs::MediaType_Floppy;
        }
        /// @todo NEWMEDIA use CSystemProperties::GetHardDIskFormats to detect
        /// possible hard disk extensions
        else if (loc.endsWith (".vdi", false) ||
                 loc.endsWith (".vmdk", false))
        {
            if (currentList == mHardDiskView)
                type = VBoxDefs::MediaType_HardDisk;
        }

        /* If the medium is supported, add it */
        if (type != VBoxDefs::MediaType_Invalid && !loc.isEmpty())
            addMediumToList (loc, type);
    }

    delete aDroppedList;
}


void VBoxMediaManagerDlg::addMediumToList (const QString &aLocation,
                                           VBoxDefs::MediaType aType)
{
    AssertReturnVoid (!aLocation.isEmpty());

    QUuid uuid;
    VBoxMedium medium;

    switch (aType)
    {
        case VBoxDefs::MediaType_HardDisk:
        {
            CHardDisk2 hd = mVBox.OpenHardDisk2 (aLocation);
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
            CDVDImage2 image = mVBox.OpenDVDImage (aLocation, uuid);
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
            CFloppyImage2 image = mVBox.OpenFloppyImage (aLocation, uuid);
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

void VBoxMediaManagerDlg::invokePopup (QListViewItem *aItem, const QPoint & aPos, int)
{
    if (aItem)
        mItemMenu->popup (aPos);
}

MediaItem *VBoxMediaManagerDlg::createHardDiskItem (QListView *aList,
                                                    const VBoxMedium &aMedium)
{
    AssertReturn (!aMedium.hardDisk().isNull(), NULL);

    MediaItem *item = NULL;

    CHardDisk2 parent = aMedium.hardDisk().GetParent();
    if (parent.isNull())
    {
        item = new MediaItem (aList, aMedium, this);
    }
    else
    {
        MediaItem *root = findMediaItem (aList, parent.GetId());
        AssertReturn (root != NULL, NULL);
        item = new MediaItem (root, aMedium, this);
    }

    return item;
}

void VBoxMediaManagerDlg::updateTabIcons (MediaItem *aItem, ItemAction aAction)
{
    AssertReturnVoid (aItem != NULL);

    /* Since this method is called from all addded/updated/removed signal
     * handlers, it's a good place to reset the possibly outdated item's
     * tooltip */
    {
        QListView *view = aItem->listView();
        QToolTip::remove (view->viewport(), view->itemRect (aItem));
    }

    QWidget *tab = NULL;
    QIconSet *iconSet = NULL;
    bool *inaccessible = NULL;

    switch (aItem->type())
    {
        case VBoxDefs::MediaType_HardDisk:
            tab = mMediaTabs->page (0);
            iconSet = &mHardDiskIconSet;
            inaccessible = &mHardDisksInaccessible;
            break;
        case VBoxDefs::MediaType_DVD:
            tab = mMediaTabs->page (1);
            iconSet = &mDVDImageIconSet;
            inaccessible = &mDVDImagesInaccessible;
            break;
        case VBoxDefs::MediaType_Floppy:
            tab = mMediaTabs->page (2);
            iconSet = &mFloppyImageIconSet;
            inaccessible = &mFloppyImagesInaccessible;
            break;
        default:
            AssertFailed();
    }

    AssertReturnVoid (tab != NULL && iconSet != NULL && inaccessible != NULL);

    switch (aAction)
    {
        case ItemAction_Added:
        {
            /* does it change the overall state? */
            if (*inaccessible ||
                aItem->state() != KMediaState_Inaccessible)
                break; /* no */

            *inaccessible = true;

            mMediaTabs->setTabIconSet (tab, vboxGlobal().warningIcon());

            break;
        }
        case ItemAction_Updated:
        case ItemAction_Removed:
        {
            QListView *view = aItem->listView();
            bool checkRest = false;

            if (aAction == ItemAction_Updated)
            {
                /* does it change the overall state? */
                if ((*inaccessible && aItem->state() == KMediaState_Inaccessible) ||
                    (!*inaccessible && aItem->state() != KMediaState_Inaccessible))
                    break; /* no */

                /* is the given item in charge? */
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

                /* find the first inaccessible item to be in charge */
                MediaItemIterator it (view);
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
                mMediaTabs->setTabIconSet (tab, vboxGlobal().warningIcon());
            else
                mMediaTabs->setTabIconSet (tab, *iconSet);

            break;
        }
    }
}

MediaItem *VBoxMediaManagerDlg::findMediaItem (QListView *aList,
                                               const QUuid &aId)
{
    AssertReturn (!aId.isNull(), NULL);

    MediaItemIterator it (aList);
    while (*it)
    {
        if ((*it)->id() == aId)
            return *it;
        ++ it;
    }

    return NULL;
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
                                 const QUuid &aSelectId /*= QUuid() */,
                                 bool aShowDiffs /*= true*/)
{
    mType = aType;

    mDoSelect = aDoSelect;

    mSessionMachine = aSessionMachine;
    if (aSessionMachine.isNull())
    {
        mSessionMachineId = QUuid();
        mShowDiffs = true;
    }
    else
    {
        mSessionMachineId = aSessionMachine.GetId();
        mShowDiffs = aShowDiffs;
        /* suppress refresh when called from the settings UI which has just
         * initiated a refresh on its own when opening the dialog */
        aRefresh = false;
    }

    switch (aType)
    {
        case VBoxDefs::MediaType_HardDisk:  mHDSelectedId = aSelectId; break;
        case VBoxDefs::MediaType_DVD:       mDVDSelectedId = aSelectId; break;
        case VBoxDefs::MediaType_Floppy:    mFloppySelectedId = aSelectId; break;
        case VBoxDefs::MediaType_All: break;
        default:
            AssertFailedReturnVoid();
    }

    mMediaTabs->setTabEnabled (mMediaTabs->page(0),
                               aType == VBoxDefs::MediaType_All ||
                               aType == VBoxDefs::MediaType_HardDisk);
    mMediaTabs->setTabEnabled (mMediaTabs->page(1),
                               aType == VBoxDefs::MediaType_All ||
                               aType == VBoxDefs::MediaType_DVD);
    mMediaTabs->setTabEnabled (mMediaTabs->page(2),
                               aType == VBoxDefs::MediaType_All ||
                               aType == VBoxDefs::MediaType_Floppy);

    if (mDoSelect)
        buttonOk->setText (tr ("&Select"));
    else
        buttonCancel->setShown (false);

    /* listen to "media enumeration started" signals */
    connect (&vboxGlobal(), SIGNAL (mediaEnumStarted()),
             this, SLOT (mediaEnumStarted()));
    /* listen to "media enumeration" signals */
    connect (&vboxGlobal(), SIGNAL (mediumEnumerated (const VBoxMedium &, int)),
             this, SLOT (mediumEnumerated (const VBoxMedium &, int)));
    /* listen to "media enumeration finished" signals */
    connect (&vboxGlobal(), SIGNAL (mediaEnumFinished (const VBoxMediaList &)),
             this, SLOT (mediaEnumFinished (const VBoxMediaList &)));

    /* listen to "media add" signals */
    connect (&vboxGlobal(), SIGNAL (mediumAdded (const VBoxMedium &)),
             this, SLOT (mediumAdded (const VBoxMedium &)));
    /* listen to "media update" signals */
    connect (&vboxGlobal(), SIGNAL (mediumUpdated (const VBoxMedium &)),
             this, SLOT (mediumUpdated (const VBoxMedium &)));
    /* listen to "media remove" signals */
    connect (&vboxGlobal(), SIGNAL (mediumRemoved (VBoxDefs::MediaType, const QUuid &)),
             this, SLOT (mediumRemoved (VBoxDefs::MediaType, const QUuid &)));

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
            mediumAdded (*it);
            if ((*it).state() != KMediaState_NotCreated)
                mProgressBar->setProgress (++ index);
        }

        /* emulate the finished signal to reuse the code */
        if (!vboxGlobal().isMediaEnumerationStarted())
            mediaEnumFinished (list);
    }

    /* for a newly opened dialog, select the first item */
    if (!mHardDiskView->selectedItem())
        setCurrentItem (mHardDiskView, mHardDiskView->firstChild());
    if (!mDVDView->selectedItem())
        setCurrentItem (mDVDView, mDVDView->firstChild());
    if (!mFloppyView->selectedItem())
        setCurrentItem (mFloppyView, mFloppyView->firstChild());
}


void VBoxMediaManagerDlg::mediaEnumStarted()
{
    /* reset inaccessible flags */
    mHardDisksInaccessible =
        mDVDImagesInaccessible =
            mFloppyImagesInaccessible = false;

    /* load default tab icons */
    mMediaTabs->setTabIconSet (mMediaTabs->page (0), mHardDiskIconSet);
    mMediaTabs->setTabIconSet (mMediaTabs->page (1), mDVDImageIconSet);
    mMediaTabs->setTabIconSet (mMediaTabs->page (2), mFloppyImageIconSet);

    /* insert already enumerated media */
    const VBoxMediaList &list = vboxGlobal().currentMediaList();
    prepareToRefresh (list.size());
    VBoxMediaList::const_iterator it;
    for (it = list.begin(); it != list.end(); ++ it)
        mediumAdded (*it);

    /* select the first item if the previous saved item is not found
     * or no current item at all */
    if (!mHardDiskView->currentItem() || !mHDSelectedId.isNull())
        setCurrentItem (mHardDiskView, mHardDiskView->firstChild());
    if (!mDVDView->currentItem() || !mDVDSelectedId.isNull())
        setCurrentItem (mDVDView, mDVDView->firstChild());
    if (!mFloppyView->currentItem() || !mFloppySelectedId.isNull())
        setCurrentItem (mFloppyView, mFloppyView->firstChild());

    processCurrentChanged();
}

void VBoxMediaManagerDlg::mediumEnumerated (const VBoxMedium &aMedium,
                                            int aIndex)
{
    AssertReturnVoid (aMedium.state() != KMediaState_NotCreated);

    mediumUpdated (aMedium);

    mProgressBar->setProgress (aIndex + 1);
}

void VBoxMediaManagerDlg::mediaEnumFinished (const VBoxMediaList &/*aList*/)
{
    mProgressBar->setHidden (true);
    mProgressText->setHidden (true);

    mRefreshAction->setEnabled (true);
    unsetCursor();

    /* adjust columns (it is strange to repeat but it works) */

    mHardDiskView->adjustColumn (1);
    mHardDiskView->adjustColumn (2);
    mHardDiskView->adjustColumn (1);

    mDVDView->adjustColumn (1);
    mDVDView->adjustColumn (2);
    mDVDView->adjustColumn (1);

    mFloppyView->adjustColumn (1);
    mFloppyView->adjustColumn (2);
    mFloppyView->adjustColumn (1);

    processCurrentChanged();
}

void VBoxMediaManagerDlg::mediumAdded (const VBoxMedium &aMedium)
{
    /* ignore non-interesting aMedium */
    if (mType != VBoxDefs::MediaType_All && mType != aMedium.type())
        return;

    if (!mShowDiffs && aMedium.type() == VBoxDefs::MediaType_HardDisk)
    {
        if (aMedium.parent() != NULL && !mSessionMachineId.isNull())
        {
            /* in !mShowDiffs mode, we ignore all diffs except ones that are
             * directly attached to the related VM in the current state */
            if (!aMedium.isAttachedInCurStateTo (mSessionMachineId))
                return;

            /* Since the base hard disk of this diff has been already appended,
             * we want to replace it with this diff to avoid duplicates in
             * !mShowDiffs mode. */
            MediaItem *item = findMediaItem (mHardDiskView, aMedium.root().id());
            AssertReturnVoid (item != NULL);

            item->setMedium (aMedium);
            updateTabIcons (item, ItemAction_Updated);
            return;
        }
    }

    MediaItem *item = NULL;

    switch (aMedium.type())
    {
        case VBoxDefs::MediaType_HardDisk:
        {
            item = createHardDiskItem (mHardDiskView, aMedium);
            AssertReturnVoid (item != NULL);

            if (item->id() == mHDSelectedId)
            {
                setCurrentItem (mHardDiskView, item);
                mHDSelectedId = QUuid();
            }
            break;
        }
        case VBoxDefs::MediaType_DVD:
        {
            item = new MediaItem (mDVDView, aMedium, this);
            AssertReturnVoid (item != NULL);

            if (item->id() == mDVDSelectedId)
            {
                setCurrentItem (mDVDView, item);
                mDVDSelectedId = QUuid();
            }
            break;
        }
        case VBoxDefs::MediaType_Floppy:
        {
            item = new MediaItem (mFloppyView, aMedium, this);
            AssertReturnVoid (item != NULL);

            if (item->id() == mFloppySelectedId)
            {
                setCurrentItem (mFloppyView, item);
                mFloppySelectedId = QUuid();
            }
            break;
        }
        default:
            AssertFailed();
    }

    AssertReturnVoid (item != NULL);

    updateTabIcons (item, ItemAction_Added);

    /* cause to update the details box when appropriate. Note: current items on
     * invisible tabs are not updated because it is always done in
     * processCurrentChanged() when the user switches to an invisible tab */
    if (item == currentListView()->currentItem())
        processCurrentChanged (item);
}

void VBoxMediaManagerDlg::mediumUpdated (const VBoxMedium &aMedium)
{
    /* ignore non-interesting aMedium */
    if (mType != VBoxDefs::MediaType_All && mType != aMedium.type())
        return;

    MediaItem *item = NULL;

    switch (aMedium.type())
    {
        case VBoxDefs::MediaType_HardDisk:
        {
            item = findMediaItem (mHardDiskView, aMedium.id());
            break;
        }
        case VBoxDefs::MediaType_DVD:
        {
            item = findMediaItem (mDVDView, aMedium.id());
            break;
        }
        case VBoxDefs::MediaType_Floppy:
        {
            item = findMediaItem (mFloppyView, aMedium.id());
            break;
        }
        default:
            AssertFailed();
    }

    /* there may be unwanted items due to !mShowDiffs */
    if (item == NULL)
        return;

    item->setMedium (aMedium);

    updateTabIcons (item, ItemAction_Updated);

    /* cause to update the details box when appropriate. Note: current items on
     * invisible tabs are not updated because it is always done in
     * processCurrentChanged() when the user switches to an invisible tab */
    if (item == currentListView()->currentItem())
        processCurrentChanged (item);
}

void VBoxMediaManagerDlg::mediumRemoved (VBoxDefs::MediaType aType,
                                         const QUuid &aId)
{
    /* ignore non-interesting aMedium */
    if (mType != VBoxDefs::MediaType_All && mType != aType)
        return;

    QListView *view = listView (aType);
    MediaItem *item = findMediaItem (view, aId);

    /* there may be unwanted items due to !mShowDiffs */
    if (item == NULL)
        return;

    updateTabIcons (item, ItemAction_Removed);

    delete item;

    setCurrentItem (view, view->currentItem());
}

void VBoxMediaManagerDlg::machineStateChanged (const VBoxMachineStateChangeEvent &e)
{
    /// @todo (r=dmik) IVirtualBoxCallback::OnMachineStateChange
    //  must also expose the old state! In this case we won't need to cache
    //  the state value in every class in GUI that uses this signal.

    switch (e.state)
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

void VBoxMediaManagerDlg::clearInfoPanes()
{
    mHDLocationLabel->clear();
    mHDTypeLabel->clear();
    mHDUsageLabel->clear();

    mDVDLocationLabel->clear();
    mDVDUsageLabel->clear();

    mFloppyLocationLabel->clear();
    mFloppyUsageLabel->clear();
}

void VBoxMediaManagerDlg::prepareToRefresh (int aTotal)
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

    mRefreshAction->setEnabled (false);
    setCursor (QCursor (BusyCursor));

    /* store the current list selections */

    QListViewItem *item;
    MediaItem *mi;

    item = mHardDiskView->currentItem();
    mi = (item && item->rtti() == MediaItem::TypeId) ?
        static_cast <MediaItem *> (item) : 0;
    if (mHDSelectedId.isNull())
        mHDSelectedId = mi ? mi->id() : QUuid();

    item = mDVDView->currentItem();
    mi = (item && item->rtti() == MediaItem::TypeId) ?
        static_cast <MediaItem *> (item) : 0;
    if (mDVDSelectedId.isNull())
        mDVDSelectedId = mi ? mi->id() : QUuid();

    item = mFloppyView->currentItem();
    mi = (item && item->rtti() == MediaItem::TypeId) ?
        static_cast <MediaItem *> (item) : 0;
    if (mFloppySelectedId.isNull())
        mFloppySelectedId = mi ? mi->id() : QUuid();

    /* finally, clear all lists */
    mHardDiskView->clear();
    mDVDView->clear();
    mFloppyView->clear();
}

void VBoxMediaManagerDlg::refreshAll()
{
    /* start enumerating media */
    vboxGlobal().startEnumeratingMedia();
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
    AssertReturn (aItem != NULL, false);

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
            /* edit means chagning the description and alike; any media that is
             * not being read to or written from can be altered in these terms*/
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
            /* removable if not attacded to anything */
            return !aItem->isUsed();
        }
        case Action_Release:
        {
            /* releasable if attacded but not in snapshots */
            return aItem->isUsed() && !aItem->isUsedInSnapshots();
        }
    }

    AssertFailedReturn (false);
}

void VBoxMediaManagerDlg::setCurrentItem (QListView *aListView,
                                          QListViewItem *aItem)
{
    if (!aItem)
        return;

    aListView->setCurrentItem (aItem);
    aListView->setSelected (aListView->currentItem(), true);
    aListView->ensureItemVisible (aListView->currentItem());
}

void VBoxMediaManagerDlg::processCurrentChanged()
{
    QListView *currentList = currentListView();
    currentList->setFocus();

    /* tab stop setup */
    setTabOrder (mHardDiskView, mHDLocationLabel);
    setTabOrder (mHDLocationLabel, mHDTypeLabel);
    setTabOrder (mHDTypeLabel, mHDUsageLabel);
    setTabOrder (mHDUsageLabel, buttonHelp);

    setTabOrder (mDVDView, mDVDLocationLabel);
    setTabOrder (mDVDLocationLabel, mDVDUsageLabel);
    setTabOrder (mDVDUsageLabel, buttonHelp);

    setTabOrder (mFloppyView, mFloppyLocationLabel);
    setTabOrder (mFloppyLocationLabel, mFloppyUsageLabel);
    setTabOrder (mFloppyUsageLabel, buttonHelp);

    setTabOrder (buttonHelp, buttonOk);
    setTabOrder (buttonOk, mMediaTabs);

    processCurrentChanged (currentList->currentItem());
}

void VBoxMediaManagerDlg::processCurrentChanged (QListViewItem *aItem)
{
    MediaItem *item = aItem && aItem->rtti() == MediaItem::TypeId ?
        static_cast <MediaItem *> (aItem) : 0;

    /* Ensures current item visible every time we are switching page */
    item->listView()->ensureItemVisible (item);

    bool notInEnum      = !vboxGlobal().isMediaEnumerationStarted();
    // bool editEnabled    = notInEnum && item &&
    //                       checkMediumFor (item, Action_Edit);
    bool removeEnabled  = notInEnum && item &&
                          checkMediumFor (item, Action_Remove);
    bool releaseEnabled = item && checkMediumFor (item, Action_Release);

    /* New and Add are now enabled even when enumerating since it should be
     * safe */
    bool newEnabled     = currentListView() == mHardDiskView;
    bool addEnabled     = true;

    // mEditAction->setEnabled (editEnabled);
    mRemoveAction->setEnabled (removeEnabled);
    mReleaseAction->setEnabled (releaseEnabled);
    mNewAction->setEnabled (newEnabled);
    mAddAction->setEnabled (addEnabled);

    if (mDoSelect)
    {
        bool selectEnabled = item && checkMediumFor (item, Action_Select);
        buttonOk->setEnabled (selectEnabled);
    }

    if (item)
    {
        /* fill in the info pane (makes sense to keep the format in sync
         * with VBoxMedium::refresh()) */

        QString usage = item->usage().isNull() ?
            richLabel (tr ("<i>Not&nbsp;Attached</i>"), false) :
            richLabel (item->usage());

        if (item->listView() == mHardDiskView)
        {
            mHDLocationLabel->setText (richLabel (item->location(), true, "end"));
            mHDTypeLabel->setText (richLabel (QString ("%1 (%2)")
                .arg (item->hardDiskType(), item->hardDiskFormat()), false));
            mHDUsageLabel->setText (usage);
        }
        else if (item->listView() == mDVDView)
        {
            mDVDLocationLabel->setText (richLabel (item->location(), true, "end"));
            mDVDUsageLabel->setText (usage);
        }
        else if (item->listView() == mFloppyView)
        {
            mFloppyLocationLabel->setText (richLabel (item->location(), true, "end"));
            mFloppyUsageLabel->setText (usage);
        }
    }
    else
        clearInfoPanes();
}

void VBoxMediaManagerDlg::processPressed (QListViewItem * aItem)
{
    if (!aItem)
    {
        QListView *currentList = currentListView();
        currentList->setSelected (currentList->currentItem(), true);
    }
}

void VBoxMediaManagerDlg::doNewMedium()
{
    AssertReturnVoid (currentListView() == mHardDiskView);

    VBoxNewHDWzd dlg (this, "VBoxNewHDWzd");

    if (dlg.exec() == QDialog::Accepted)
    {
        CHardDisk2 hd = dlg.hardDisk();
        /* select the newly created hard disk */
        MediaItem *item = findMediaItem (mHardDiskView, hd.GetId());
        AssertReturnVoid (item != NULL);
        mHardDiskView->setCurrentItem (item);
    }
}

void VBoxMediaManagerDlg::doAddMedium()
{
    QListView *currentList = currentListView();
    MediaItem *item =
        currentList->currentItem() &&
        currentList->currentItem()->rtti() == MediaItem::TypeId ?
        static_cast <MediaItem *> (currentList->currentItem()) : NULL;

    QString dir;
    if (item && item->state() != KMediaState_Inaccessible
             && item->state() != KMediaState_NotCreated)
        dir = QFileInfo (item->location().stripWhiteSpace()).dirPath (true);

    if (!dir)
        if (currentList == mHardDiskView)
            dir = mVBox.GetSystemProperties().GetDefaultHardDiskFolder();

    if (!dir || !QFileInfo (dir).exists())
        dir = mVBox.GetHomeFolder();

    QString title;
    QString filter;
    VBoxDefs::MediaType type = VBoxDefs::MediaType_Invalid;

    if (currentList == mHardDiskView)
    {
        /// @todo NEWMEDIA use CSystemProperties::GetHardDIskFormats to detect
        /// possible hard disk extensions
        filter = tr ("All hard disk images (*.vdi; *.vmdk);;"
                     "Virtual Disk images (*.vdi);;"
                     "VMDK images (*.vmdk);;"
                     "All files (*)");
        title = tr ("Select a hard disk image file");
        type = VBoxDefs::MediaType_HardDisk;
    }
    else if (currentList == mDVDView)
    {
        filter = tr ("CD/DVD-ROM images (*.iso);;"
                     "All files (*)");
        title = tr ("Select a CD/DVD-ROM disk image file");
        type = VBoxDefs::MediaType_DVD;
    }
    else if (currentList == mFloppyView)
    {
        filter = tr ("Floppy images (*.img);;"
                     "All files (*)");
        title = tr ("Select a floppy disk image file");
        type = VBoxDefs::MediaType_Floppy;
    }
    else
    {
        AssertMsgFailed (("Root list should be either mHardDiskView, "
                          "mDVDView or mFloppyView"));
    }

    QString loc = VBoxGlobal::getOpenFileName (dir, filter, this,
                                               "AddMediaDialog", title);
    loc =  QDir::convertSeparators (loc);
    if (!loc.isEmpty())
        addMediumToList (loc, type);
}

void VBoxMediaManagerDlg::doRemoveMedium()
{
    QListView *currentList = currentListView();

    MediaItem *item =
        currentList->currentItem() &&
        currentList->currentItem()->rtti() == MediaItem::TypeId ?
        static_cast <MediaItem *> (currentList->currentItem()) : NULL;
    AssertMsgReturnVoid (item != NULL, ("Current item must not be null"));

    /* remember ID/type as they may get lost after the closure/deletion */
    QUuid id = item->id();
    VBoxDefs::MediaType type = item->type();

    AssertReturnVoid (!id.isNull());

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

            CHardDisk2 hardDisk = item->medium().hardDisk();

            if (deleteStorage)
            {
                bool success = false;

                CProgress progress = hardDisk.DeleteStorage();
                if (hardDisk.isOk())
                {
                    vboxProblem().showModalProgressDialog (progress, caption(),
                                                           parentWidget());
                    if (progress.isOk() && progress.GetResultCode() == S_OK)
                        success = true;
                }

                if (success)
                    vboxGlobal().removeMedium (VBoxDefs::MediaType_HardDisk, id);
                else
                    vboxProblem().cannotDeleteHardDiskStorage (this, hardDisk,
                                                               progress);

                /* we don't want to close the hard disk because it was
                 * implicitly closed and removed from the list of known media on
                 * storage deletion */
                return;
            }

            hardDisk.Close();
            result = hardDisk;
            break;
        }
        case VBoxDefs::MediaType_DVD:
        {
            CDVDImage2 image = item->medium().dvdImage();
            image.Close();
            result = image;
            break;
        }
        case VBoxDefs::MediaType_Floppy:
        {
            CFloppyImage2 image = item->medium().floppyImage();
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
    QListView *currentList = currentListView();

    MediaItem *item =
        currentList->currentItem() &&
        currentList->currentItem()->rtti() == MediaItem::TypeId ?
        static_cast <MediaItem *> (currentList->currentItem()) : NULL;
    AssertMsgReturnVoid (item != NULL, ("Current item must not be null"));

    AssertReturnVoid (!item->id().isNull());

    /* get the fresh attached VM list */
    item->refreshAll();

    QString usage;
    CMachineVector machines;

    const QValueList <QUuid> &machineIds = item->medium().curStateMachineIds();
    for (QValueList <QUuid>::const_iterator it = machineIds.begin();
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
        /* it may happen that the new machine list is empty (medium was already
         * released by a third party); update the details and silently return.*/
        processCurrentChanged (item);
        return;
    }

    AssertReturnVoid (machines.size() > 0);

    if (!vboxProblem().confirmReleaseMedium (this, item->medium(), usage))
        return;

    for (QValueList <QUuid>::const_iterator it = machineIds.begin();
         it != machineIds.end(); ++ it)
    {
        if (!releaseMediumFrom (item->medium(), *it))
            break;
    }

    /* inform others about medium changes (use a copy since data owning is not
     * clean there (to be fixed one day using shared_ptr)) */
    VBoxMedium newMedium = item->medium();
    newMedium.refresh();
    vboxGlobal().updateMedium (newMedium);
}

bool VBoxMediaManagerDlg::releaseMediumFrom (const VBoxMedium &aMedium,
                                             const QUuid &aMachineId)
{
    CSession session;
    CMachine machine;

    /* is this medium is attached to the VM we are setting up */
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
            CHardDisk2AttachmentVector vec =machine.GetHardDisk2Attachments();
            for (size_t i = 0; i < vec.size(); ++ i)
            {
                CHardDisk2Attachment hda = vec [i];
                if (hda.GetHardDisk().GetId() == aMedium.id())
                {
                    machine.DetachHardDisk2 (hda.GetBus(),
                                             hda.GetChannel(),
                                             hda.GetDevice());
                    if (!machine.isOk())
                    {
                        vboxProblem().cannotDetachHardDisk (
                            this, machine, aMedium.location(), hda.GetBus(),
                            hda.GetChannel(), hda.GetDevice());
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

    /* if a new session was opened, we must close it */
    if (!session.isNull())
        session.Close();

    return success;
}

QUuid VBoxMediaManagerDlg::selectedId()
{
    QListView *currentList = currentListView();
    QUuid id;

    if (currentList->selectedItem() &&
        currentList->selectedItem()->rtti() == MediaItem::TypeId)
        id = static_cast <MediaItem *> (
            currentList->selectedItem())->id();

    return id;
}

QString VBoxMediaManagerDlg::selectedLocation()
{
    QListView *currentList = currentListView();
    QString loc;

    if (currentList->selectedItem() &&
        currentList->selectedItem()->rtti() == MediaItem::TypeId )
        loc = static_cast <MediaItem *> (currentList->selectedItem())->
            location().stripWhiteSpace();

    return loc;
}

void VBoxMediaManagerDlg::processDoubleClick (QListViewItem*)
{
    QListView *currentList = currentListView();

    if (mDoSelect && currentList->selectedItem() && buttonOk->isEnabled())
        accept();
}
