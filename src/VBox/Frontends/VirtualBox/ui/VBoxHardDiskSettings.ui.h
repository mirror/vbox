/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxHardDiskSettings widget UI include (Qt Designer)
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

/** SATA Ports count */
static const ULONG SATAPortsCount = 30;

class HDSlotItem;

/** Combines the string and the numeric representation of the hard disk slot. */
struct HDSlot
{
    HDSlot() : bus (KStorageBus_Null), channel (0), device (0) {}
    HDSlot (const QString &aStr, KStorageBus aBus, LONG aChannel, LONG aDevice)
        : str (aStr), bus (aBus), channel (aChannel), device (aDevice) {}

    QString str;
    KStorageBus bus;
    LONG channel;
    LONG device;
};

/**
 *  QObject class reimplementation to use for making selected IDE & SATA
 *  slots unique.
 */
class HDSlotUniquizer : public QObject
{
    Q_OBJECT

public:

    HDSlotUniquizer (QWidget *aParent, int aSataPortsCount = 0)
        : QObject (aParent)
        , mSataPortsCount (aSataPortsCount)
    {
        /* Compose Lists */
        makeIDEList();
        makeSATAList();
    }

    QValueList<HDSlot> list (HDSlotItem *aForSubscriber, bool aFilter = true);

    int totalCount() { return mIDEList.size() + mSATAList.size(); }

    int getSATAPortsCount()
    {
        return mSataPortsCount;
    }

    void setSATAPortsCount (int aSataPortsCount)
    {
        mSataPortsCount = aSataPortsCount;
        makeSATAList();
    }

    void subscribe (HDSlotItem *aSubscriber)
    {
        bool result = mSubscribersList.resize (mSubscribersList.size() + 1);
        if (!result)
            return;

        mSubscribersList.insert (mSubscribersList.size() - 1, aSubscriber);
        mSubscribersList.sort();

        emit listChanged();
    }

    void unsubscribe (HDSlotItem *aSubscriber)
    {
        int index = mSubscribersList.findRef (aSubscriber);
        if (index == -1)
            return;

        mSubscribersList.remove (index);
        mSubscribersList.sort();
        mSubscribersList.resize (mSubscribersList.size() - 1);

        emit listChanged();
    }

signals:

    void listChanged();

private:

    void makeIDEList()
    {
        mIDEList.clear();

        /* IDE Primary Master */
        mIDEList << HDSlot (vboxGlobal().toFullString (KStorageBus_IDE, 0, 0),
                            KStorageBus_IDE, 0, 0);
        /* IDE Primary Slave */
        mIDEList << HDSlot (vboxGlobal().toFullString (KStorageBus_IDE, 0, 1),
                            KStorageBus_IDE, 0, 1);
        /* IDE Secondary Slave */
        mIDEList << HDSlot (vboxGlobal().toFullString (KStorageBus_IDE, 1, 1),
                            KStorageBus_IDE, 1, 1);

        emit listChanged();
    }

    void makeSATAList()
    {
        mSATAList.clear();

        for (int i = 0; i < mSataPortsCount; ++ i)
            mSATAList << HDSlot (vboxGlobal().toFullString (KStorageBus_SATA, 0, i),
                                 KStorageBus_SATA, 0, i);

        emit listChanged();
    }

    int mSataPortsCount;
    QValueList<HDSlot> mIDEList;
    QValueList<HDSlot> mSATAList;
    QPtrVector<HDSlotItem> mSubscribersList;
};

/**
 *  QComboBox class reimplementation to use as selector for IDE & SATA
 *  slots.
 */
class HDSlotItem : public QComboBox
{
    Q_OBJECT

public:

    HDSlotItem (QWidget *aParent, HDSlotUniquizer *aUniq)
        : QComboBox (aParent)
        , mUniq (aUniq)
    {
        /* In some qt themes embedded list-box is not used by default */
        if (!listBox())
            setListBox (new QListBox (this));

        setFocusPolicy (QWidget::NoFocus);
        refresh();
        mUniq->subscribe (this);
        connect (mUniq, SIGNAL (listChanged()), this, SLOT (refresh()));
        connect (mUniq, SIGNAL (listChanged()), this, SLOT (updateToolTip()));
        connect (this, SIGNAL (activated (int)), mUniq, SIGNAL (listChanged()));
        connect (this, SIGNAL (textChanged()), mUniq, SIGNAL (listChanged()));
    }

   ~HDSlotItem()
    {
        mUniq->unsubscribe (this);
    }

    void setText (const QString &aText)
    {
        QComboBox::setCurrentText (aText);
        emit textChanged();
    }

    KStorageBus currentBus() const
    {
        AssertReturn (currentItem() >= 0 && (size_t) currentItem() < mHDSlots.size(),
                      KStorageBus_Null);
        return mHDSlots [currentItem()].bus;
    }

    LONG currentChannel() const
    {
        AssertReturn (currentItem() >= 0 && (size_t) currentItem() < mHDSlots.size(),
                      0);
        return mHDSlots [currentItem()].channel;
    }

    LONG currentDevice() const
    {
        AssertReturn (currentItem() >= 0 && (size_t) currentItem() < mHDSlots.size(),
                      0);
        return mHDSlots [currentItem()].device;
    }

private slots:

    void refresh()
    {
        QString current = currentText();
        mHDSlots = mUniq->list (this);
        clear();

        bool setCurrent = false;

        for (QValueList<HDSlot>::const_iterator it = mHDSlots.begin();
             it != mHDSlots.end(); ++ it)
        {
            insertItem ((*it).str);
            if (!setCurrent)
                setCurrent = (*it).str == current;
        }

        if (setCurrent)
            setCurrentText (current);
    }

    void updateToolTip()
    {
        QString oldTip = QToolTip::textFor (this);
        QString newTip = currentText();

        if (newTip != oldTip)
        {
            QToolTip::remove (this);
            QToolTip::add (this, newTip);
        }
    }

signals:

    void textChanged();

private:

    HDSlotUniquizer *mUniq;

    QValueList<HDSlot> mHDSlots;
};

/**
 *  VBoxMediaComboBox class reimplementation to use as selector for VDI
 *  image.
 */
class HDVdiItem : public VBoxMediaComboBox
{
    Q_OBJECT

public:

    HDVdiItem (QWidget *aParent, int aType, QListViewItem *aItem)
        : VBoxMediaComboBox (aParent, "HDVdiItem", aType)
        , mItem (aItem)
    {
        setFocusPolicy (QWidget::NoFocus);
        connect (&vboxGlobal(),
                 SIGNAL (mediaRemoved (VBoxDefs::DiskType, const QUuid &)),
                 this, SLOT (repaintHandler()));
    }

private slots:

    void repaintHandler()
    {
        mItem->repaint();
    }

private:

    QListViewItem *mItem;
};

QValueList<HDSlot> HDSlotUniquizer::list (HDSlotItem *aSubscriber, bool aFilter)
{
    QValueList<HDSlot> list = mIDEList + mSATAList;

    if (!aFilter)
        return list;

    /* Compose exclude list */
    QStringList excludeList;
    for (uint i = 0; i < mSubscribersList.size(); ++ i)
        if (mSubscribersList [i] != aSubscriber)
            excludeList << mSubscribersList [i]->currentText();

    /* Filter the list */
    QValueList<HDSlot>::Iterator it = list.begin();
    while (it != list.end())
    {
        if (excludeList.contains ((*it).str))
            it = list.remove (it);
        else
            ++ it;
    }

    return list;
}

class HDSpaceItem : public QListViewItem
{
public:

    enum { HDSpaceItemType = 1011 };

    HDSpaceItem (QListView *aParent)
        : QListViewItem (aParent)
    {
        setSelectable (false);
    }

    int rtti() const { return HDSpaceItemType; }
};

class HDListItem : public QListViewItem
{
public:

    enum { HDListItemType = 1010 };

    HDListItem (VBoxHardDiskSettings *aWidget, QListView *aParent,
                QListViewItem *aAfter,
                HDSlotUniquizer *aUniq, const CMachine &aMachine)
        : QListViewItem (aParent, aAfter)
        , mWidget (aWidget)
        , mUniq (aUniq)
        , mMachine (aMachine)
        , mFocusColumn (-1)
    {
        init();
    }

    HDListItem (VBoxHardDiskSettings *aWidget, QListView *aParent,
                HDSlotUniquizer *aUniq, const CMachine &aMachine)
        : QListViewItem (aParent)
        , mWidget (aWidget)
        , mUniq (aUniq)
        , mMachine (aMachine)
        , mFocusColumn (-1)
    {
        init();
    }

    int rtti() const { return HDListItemType; }

    QString toolTip()
    {
        return QToolTip::textFor (mVector [1]);
    }

    HDListItem* nextSibling() const
    {
        QListViewItem *item = QListViewItem::nextSibling();
        return item && item->rtti() == HDListItemType ?
            static_cast<HDListItem*> (item) : 0;
    }

    void setId (const QUuid &aId) const
    {
        static_cast<VBoxMediaComboBox*> (mVector [1])->setCurrentItem (aId);
    }

    QUuid getId() const
    {
        return static_cast<VBoxMediaComboBox*> (mVector [1])->getId();
    }

    KStorageBus bus() const
    {
        return static_cast<HDSlotItem*> (mVector [0])->currentBus();
    }

    LONG channel() const
    {
        return static_cast<HDSlotItem*> (mVector [0])->currentChannel();
    }

    LONG device() const
    {
        return static_cast<HDSlotItem*> (mVector [0])->currentDevice();
    }

    QString text (int aColumn) const
    {
        return mVector [aColumn]->currentText();
    }

    void moveFocusToColumn (int aCol)
    {
        mFocusColumn = aCol;
        repaint();
    }

    void showEditor()
    {
        if (mVector [mFocusColumn]->count())
            mVector [mFocusColumn]->popup();
    }

    int focusColumn() const
    {
        return mFocusColumn;
    }

    void setAttachment (const CHardDiskAttachment &aHda)
    {
        QString device = vboxGlobal()
            .toFullString (aHda.GetBus(), aHda.GetChannel(), aHda.GetDevice());

        if (mVector [0]->listBox()->findItem (device, Qt::ExactMatch))
            static_cast<HDSlotItem*> (mVector [0])->setText (device);

        static_cast<VBoxMediaComboBox*> (mVector [1])->
            setCurrentItem (aHda.GetHardDisk().GetId());

        mVector [0]->setHidden (true);
        mVector [1]->setHidden (true);
    }

private:

    void init()
    {
        setSelectable (false);
        mVector.setAutoDelete (true);
        mVector.resize (listView()->columns());

        QComboBox *cbslot = new HDSlotItem (listView()->viewport(), mUniq);
        QObject::connect (cbslot, SIGNAL (activated (int)),
                          mWidget, SIGNAL (hddListChanged()));
        mVector.insert (0, cbslot);

        VBoxMediaComboBox *cbvdi = new HDVdiItem (listView()->viewport(),
                                                  VBoxDefs::HD, this);
        QObject::connect (cbvdi, SIGNAL (activated (int)),
                          mWidget, SIGNAL (hddListChanged()));
        mVector.insert (1, cbvdi);
        cbvdi->setBelongsTo (mMachine.GetId());
        cbvdi->refresh();
    }

    void paintCell (QPainter *aPainter, const QColorGroup &aColorGroup,
                    int aColumn, int aWidth, int aAlign)
    {
        QComboBox *cb = mVector [aColumn];

        int indent = 0;
        for (int i = 0; i < aColumn; ++ i)
            indent = listView()->columnWidth (i);

        QRect rect = listView()->itemRect (this);

        int xc = rect.x() + indent;
        int yc = rect.y();
        int wc = listView()->columnWidth (aColumn);
        int hc = rect.height();

        cb->move (xc, yc);
        cb->resize (wc, hc);

        if (aColumn == mFocusColumn && cb->isHidden())
            cb->show();
        else if (aColumn != mFocusColumn && !cb->isHidden())
            cb->hide();

        QListViewItem::paintCell (aPainter, aColorGroup, aColumn, aWidth, aAlign);
    }

    void paintFocus (QPainter *, const QColorGroup &, const QRect &)
    {
        /* Do not paint focus, because it presented by combo-box */
    }

    void setup()
    {
        QListViewItem::setup();
        /* Increasing item's height by 30% */
        setHeight ((int) (height() * 1.3));
    }

    VBoxHardDiskSettings  *mWidget;
    HDSlotUniquizer       *mUniq;
    CMachine               mMachine;
    QPtrVector<QComboBox>  mVector;
    int                    mFocusColumn;
};

class OnItemChangedEvent : public QEvent
{
public:
    enum { Type = QEvent::User + 10 };
    OnItemChangedEvent (QListViewItem *aItem)
        : QEvent ((QEvent::Type) Type), mItem (aItem) {}

    QListViewItem *mItem;
};

void VBoxHardDiskSettings::init()
{
    mPrevItem = 0;

    /* toolbar */

    VBoxToolBar *toolBar = new VBoxToolBar (0, mGbHDList, "snapshotToolBar");

    mAddAttachmentAct->addTo (toolBar);
    mRemoveAttachmentAct->addTo (toolBar);
    mSelectHardDiskAct->addTo (toolBar);

    toolBar->setUsesTextLabel (false);
    toolBar->setUsesBigPixmaps (false);
    toolBar->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);
    toolBar->setOrientation (Qt::Vertical);
#ifdef Q_WS_MAC
    toolBar->setMacStyle();
#endif
    mToolBarLayout->insertWidget (0, toolBar);

    /* context menu */
    mContextMenu = new QPopupMenu (this);
    mAddAttachmentAct->addTo (mContextMenu);
    mRemoveAttachmentAct->addTo (mContextMenu);
    mSelectHardDiskAct->addTo (mContextMenu);

    /* icons */
    mAddAttachmentAct->setIconSet
        (VBoxGlobal::iconSet ("vdm_add_16px.png", "vdm_add_disabled_16px.png"));
    mRemoveAttachmentAct->setIconSet
        (VBoxGlobal::iconSet ("vdm_remove_16px.png", "vdm_remove_disabled_16px.png"));
    mSelectHardDiskAct->setIconSet
        (VBoxGlobal::iconSet ("select_file_16px.png", "select_file_dis_16px.png"));

    /* connections */
    connect (mCbSATA, SIGNAL (toggled (bool)),
             this, SLOT (onToggleSATAController (bool)));
    connect (mLvHD, SIGNAL (pressed (QListViewItem*, const QPoint&, int)),
             this, SLOT (moveFocus (QListViewItem*, const QPoint&, int)));
    connect (mLvHD, SIGNAL (currentChanged (QListViewItem*)),
             this, SLOT (onCurrentChanged (QListViewItem*)));
    connect (mLvHD, SIGNAL (contextMenuRequested (QListViewItem*, const QPoint&, int)),
             this, SLOT (onContextMenuRequested (QListViewItem*, const QPoint&, int)));

    /* rest */

    new HDSpaceItem (mLvHD);

    mSlotUniquizer = new HDSlotUniquizer (this);

    qApp->installEventFilter (this);
}

void VBoxHardDiskSettings::getFromMachine (const CMachine &aMachine)
{
    mMachine = aMachine;

    {
        CSATAController ctl = mMachine.GetSATAController();
        if (ctl.isNull())
        {
            /* hide the SATA check box if the SATA controller is not available
             * (i.e. in VirtualBox OSE) */
            mCbSATA->setHidden (true);
        }
        else
        {
            mCbSATA->setChecked (ctl.GetEnabled());
        }
    }

    CHardDiskAttachmentEnumerator en =
        mMachine.GetHardDiskAttachments().Enumerate();
    while (en.HasMore())
    {
        CHardDiskAttachment hda = en.GetNext();
        HDListItem *item = createItem (mSlotUniquizer, mMachine);
        item->setAttachment (hda);
    }
    mLvHD->setSortColumn (0);
    mLvHD->sort();
    mLvHD->setSorting (-1);
    mLvHD->setCurrentItem (mLvHD->firstChild());
    onAfterCurrentChanged (0);
}

void VBoxHardDiskSettings::putBackToMachine()
{
    CSATAController ctl = mMachine.GetSATAController();
    if (!ctl.isNull())
    {
        ctl.SetEnabled (mCbSATA->isChecked());
    }

    /* Detach all attached Hard Disks */
    CHardDiskAttachmentEnumerator en =
        mMachine.GetHardDiskAttachments().Enumerate();
    while (en.HasMore())
    {
        CHardDiskAttachment hda = en.GetNext();
        mMachine.DetachHardDisk (hda.GetBus(), hda.GetChannel(), hda.GetDevice());
        if (!mMachine.isOk())
            vboxProblem().cannotDetachHardDisk (this, mMachine,
                hda.GetBus(), hda.GetChannel(), hda.GetDevice());
    }

    /* Sort&Attach all listed Hard Disks */
    mLvHD->setSortColumn (0);
    mLvHD->sort();
    LONG maxSATAPort = 1;
    HDListItem *item = mLvHD->firstChild() &&
        mLvHD->firstChild()->rtti() == HDListItem::HDListItemType ?
        static_cast<HDListItem*> (mLvHD->firstChild()) : 0;
    while (item)
    {
        if (item->bus() == KStorageBus_SATA)
            maxSATAPort = maxSATAPort < item->device() ?
                          item->device() : maxSATAPort;
        mMachine.AttachHardDisk (item->getId(),
            item->bus(), item->channel(), item->device());
        if (!mMachine.isOk())
            vboxProblem().cannotAttachHardDisk (this, mMachine, item->getId(),
                item->bus(), item->channel(), item->device());
        item = item->nextSibling();
    }

    if (!ctl.isNull())
    {
        mMachine.GetSATAController().SetPortCount (maxSATAPort);
    }
}

QString VBoxHardDiskSettings::checkValidity()
{
    QString result;
    QStringList slList;
    QStringList idList;

    /* Search for coincidences through all the media-id */
    HDListItem *item = mLvHD->firstChild() &&
        mLvHD->firstChild()->rtti() == HDListItem::HDListItemType ?
        static_cast<HDListItem*> (mLvHD->firstChild()) : 0;
    while (item)
    {
        QString id = item->getId().toString();
        if (idList.contains (id))
        {
            result = tr ("<i>%1</i> uses the hard disk that is already "
                         "attached to <i>%2</i>")
                .arg (item->text (0)).arg (slList [idList.findIndex (id)]);
            break;
        }
        else
        {
            slList << item->text (0);
            idList << id;
        }
        item = item->nextSibling();
    }

    return result;
}

void VBoxHardDiskSettings::addHDItem()
{
    HDListItem *item = createItem (mSlotUniquizer, mMachine);
    item->moveFocusToColumn (0);
    mLvHD->setCurrentItem (item);
    if (!mLvHD->hasFocus())
        mLvHD->setFocus();
    /* Qt3 isn't emits currentChanged() signal if first list-view item added */
    if (mLvHD->childCount() == 1)
        onCurrentChanged (item);

    emit hddListChanged();
}

void VBoxHardDiskSettings::delHDItem()
{
    if (mLvHD->currentItem())
    {
        QListViewItem *item = mLvHD->currentItem();
        Assert (item == mPrevItem);
        if (item == mPrevItem)
        {
            delete item;
            mPrevItem = 0;

            if (mLvHD->currentItem() &&
                mLvHD->currentItem()->rtti() == HDSpaceItem::HDSpaceItemType &&
                mLvHD->currentItem()->itemAbove() &&
                mLvHD->currentItem()->itemAbove()->rtti() == HDListItem::HDListItemType)
                mLvHD->setCurrentItem (mLvHD->currentItem()->itemAbove());
        }
    }

    emit hddListChanged();
}

void VBoxHardDiskSettings::showVDM()
{
    HDListItem *item = mLvHD->currentItem() &&
        mLvHD->currentItem()->rtti() == HDListItem::HDListItemType ?
        static_cast<HDListItem*> (mLvHD->currentItem()) : 0;

    VBoxDiskImageManagerDlg dlg (this, "VBoxDiskImageManagerDlg",
                                 WType_Dialog | WShowModal);

    QUuid machineId = mMachine.GetId();
    QUuid hdId = item->getId();

    dlg.setup (VBoxDefs::HD, true, &machineId, true /* aRefresh */,
               mMachine, hdId, QUuid(), QUuid());

    if (dlg.exec() == VBoxDiskImageManagerDlg::Accepted)
        item->setId (dlg.getSelectedUuid());
}

void VBoxHardDiskSettings::moveFocus (QListViewItem *aItem, const QPoint&, int aCol)
{
    if (aItem && aItem->rtti() == HDListItem::HDListItemType)
    {
        static_cast<HDListItem*> (aItem)->moveFocusToColumn (aCol);
        onAfterCurrentChanged (aItem);
    }
}

void VBoxHardDiskSettings::onCurrentChanged (QListViewItem *aItem)
{
    /* Postpone onCurrentChanged signal to be post-processed after all others */
    QApplication::postEvent (this, new OnItemChangedEvent (aItem));
}

void VBoxHardDiskSettings::onToggleSATAController (bool aOn)
{
    if (!aOn)
    {
        HDListItem *firstItem = mLvHD->firstChild() &&
            mLvHD->firstChild()->rtti() == HDListItem::HDListItemType ?
            static_cast<HDListItem*> (mLvHD->firstChild()) : 0;

        /* Search the list for the SATA ports in */
        HDListItem *sataItem = firstItem;
        while (sataItem)
        {
            if (sataItem->bus() == KStorageBus_SATA)
                break;
            sataItem = sataItem->nextSibling();
        }

        /* If list contains at least one SATA port */
        if (sataItem)
        {
            int rc = vboxProblem().confirmDetachSATASlots (this);
            if (rc != QIMessageBox::Ok)
            {
                /* Switch check-box back to "on" */
                mCbSATA->blockSignals (true);
                mCbSATA->setChecked (true);
                mCbSATA->blockSignals (false);
                return;
            }
            else
            {
                /* Delete SATA items */
                HDListItem *it = firstItem;
                mLvHD->blockSignals (true);
                while (it)
                {
                    HDListItem *curIt = it;
                    it = it->nextSibling();
                    if (curIt->bus() == KStorageBus_SATA)
                    {
                        if (curIt == mLvHD->currentItem())
                            mPrevItem = 0;
                        delete curIt;
                    }
                }
                mLvHD->blockSignals (false);
                emit hddListChanged();
            }
        }
    }

    int newSATAPortsCount = aOn && !mMachine.isNull() ? SATAPortsCount : 0;
    if (mSlotUniquizer->getSATAPortsCount() != newSATAPortsCount)
    {
        mSlotUniquizer->setSATAPortsCount (newSATAPortsCount);
        onAfterCurrentChanged (mLvHD->currentItem());
    }
}

void VBoxHardDiskSettings::onAfterCurrentChanged (QListViewItem *aItem)
{
    /* Process postponed onCurrentChanged event */
    if (aItem != mPrevItem)
    {
        if (aItem && aItem->rtti() == HDListItem::HDListItemType &&
            static_cast<HDListItem*> (aItem)->focusColumn() == -1)
        {
            int prevFocusColumn = 0;
            if (mPrevItem && mPrevItem->rtti() == HDListItem::HDListItemType)
                prevFocusColumn = static_cast<HDListItem*> (mPrevItem)->focusColumn();
            static_cast<HDListItem*> (aItem)->moveFocusToColumn (prevFocusColumn);
        }

        if (mPrevItem && mPrevItem->rtti() == HDListItem::HDListItemType)
            static_cast<HDListItem*> (mPrevItem)->moveFocusToColumn (-1);

        mPrevItem = aItem;
    }

    mAddAttachmentAct->setEnabled (mLvHD->childCount() <=
        mSlotUniquizer->totalCount());
    mRemoveAttachmentAct->setEnabled (aItem &&
        aItem->rtti() == HDListItem::HDListItemType);
    mSelectHardDiskAct->setEnabled (aItem &&
        aItem->rtti() == HDListItem::HDListItemType &&
        static_cast<HDListItem*> (aItem)->focusColumn() == 1);
}

void VBoxHardDiskSettings::onContextMenuRequested (QListViewItem * /*aItem*/,
                                                   const QPoint &aPoint, int)
{
    mContextMenu->exec (aPoint);
}

HDListItem* VBoxHardDiskSettings::createItem (HDSlotUniquizer *aUniq,
                                              const CMachine &aMachine)
{
    QListViewItem *item = mLvHD->lastItem();
    Assert (item->rtti() == HDSpaceItem::HDSpaceItemType);
    HDListItem *last = item->itemAbove() &&
        item->itemAbove()->rtti() == HDListItem::HDListItemType ?
        static_cast<HDListItem*> (item->itemAbove()) : 0;

    return last ?
        new HDListItem (this, mLvHD, last, aUniq, aMachine) :
        new HDListItem (this, mLvHD, aUniq, aMachine);
}

bool VBoxHardDiskSettings::event (QEvent *aEvent)
{
    switch (aEvent->type())
    {
        /* Redirect postponed onCurrentChanged event */
        case OnItemChangedEvent::Type:
        {
            OnItemChangedEvent *e = static_cast<OnItemChangedEvent*> (aEvent);
            onAfterCurrentChanged (e->mItem);
            break;
        }
        default:
            break;
    }

    return QWidget::event (aEvent);
}

void VBoxHardDiskSettings::showEvent (QShowEvent *aEvent)
{
    QWidget::showEvent (aEvent);
    QTimer::singleShot (0, this, SLOT (adjustList()));
}

bool VBoxHardDiskSettings::eventFilter (QObject *aObject, QEvent *aEvent)
{
    if (!aObject->isWidgetType())
        return QWidget::eventFilter (aObject, aEvent);

    if (static_cast<QWidget*> (aObject)->topLevelWidget() != topLevelWidget())
        return QWidget::eventFilter (aObject, aEvent);

    switch (aEvent->type())
    {
        /* Process double-click as "open combo-box" action */
        case QEvent::MouseButtonDblClick:
        {
            if (aObject != mLvHD->viewport())
                break;

            QMouseEvent *e = static_cast<QMouseEvent*> (aEvent);
            QListViewItem *clickedItem = mLvHD->itemAt (QPoint (e->x(), e->y()));
            HDListItem *item = clickedItem &&
                clickedItem->rtti() == HDListItem::HDListItemType ?
                static_cast<HDListItem*> (clickedItem) : 0;

            if (!item && mAddAttachmentAct->isEnabled())
                addHDItem();
            break;
        }
        /* Process mouse-move as "make tool-tip" action */
        case QEvent::MouseMove:
        {
            if (aObject != mLvHD->viewport())
            {
                if (!QToolTip::textFor (mLvHD->viewport()).isNull())
                    QToolTip::remove (mLvHD->viewport());
                break;
            }

            QMouseEvent *e = static_cast<QMouseEvent*> (aEvent);
            QListViewItem *hoveredItem = mLvHD->itemAt (QPoint (e->x(), e->y()));
            HDListItem *item = hoveredItem &&
                hoveredItem->rtti() == HDListItem::HDListItemType ?
                static_cast<HDListItem*> (hoveredItem) : 0;

            QString oldTip = QToolTip::textFor (mLvHD->viewport());
            QString newTip = item ? item->toolTip() :
                             tr ("Double-click to add a new attachment");

            if (newTip != oldTip)
            {
                QToolTip::remove (mLvHD->viewport());
                QToolTip::add (mLvHD->viewport(), newTip);
            }
            break;
        }
        case QEvent::KeyPress:
        {
            if (aObject != mLvHD)
                break;

            HDListItem *item = mLvHD->currentItem() &&
                mLvHD->currentItem()->rtti() == HDListItem::HDListItemType ?
                static_cast<HDListItem*> (mLvHD->currentItem()) : 0;

            QKeyEvent *e = static_cast<QKeyEvent*> (aEvent);
            /* Process cursor-left as "move focus left" action */
            if (e->key() == Qt::Key_Left && !e->state())
            {
                if (item && item->focusColumn() != -1 &&
                    item->focusColumn() > 0)
                {
                    item->moveFocusToColumn (item->focusColumn() - 1);
                    onAfterCurrentChanged (item);
                }
            } else
            /* Process cursor-right as "move focus right" action */
            if (e->key() == Qt::Key_Right && !e->state())
            {
                if (item && item->focusColumn() != -1 &&
                    item->focusColumn() < mLvHD->columns() - 1)
                {
                    item->moveFocusToColumn (item->focusColumn() + 1);
                    onAfterCurrentChanged (item);
                }
            } else
            /* Process F2/Space as "open combo-box" actions */
            if (!e->state() &&
                (e->key() == Qt::Key_F2 || e->key() == Qt::Key_Space))
            {
                if (item)
                    item->showEditor();
            }
            /* Process Ctrl/Alt+Up/Down as "open combo-box" actions */
            if ((e->state() == Qt::AltButton || e->state() == Qt::ControlButton) &&
                (e->key() == Qt::Key_Up || e->key() == Qt::Key_Down))
            {
                if (item)
                {
                    item->showEditor();
                    return true;
                }
            }
            break;
        }
        /* Process focus event to toggle the current selection state */
        case QEvent::FocusIn:
        {
            if (aObject == mLvHD)
                onAfterCurrentChanged (mLvHD->currentItem());
            else if (!mGbHDList->queryList (0, 0, false, true)->contains (aObject))
                onAfterCurrentChanged (0);
            break;
        }
        default:
            break;
    }

    return QWidget::eventFilter (aObject, aEvent);
}

void VBoxHardDiskSettings::adjustList()
{
    /* Search through the slots list for maximum element width */
    int minLength = 0;
    QFontMetrics fm = mLvHD->fontMetrics();
    QValueList<HDSlot> list = mSlotUniquizer->list (0, false);
    for (uint i = 0; i < list.size(); ++ i)
    {
        int length = fm.width (list [i].str);
        minLength = minLength < length ? length : minLength;
    }
    minLength = minLength > mLvHD->viewport()->width() * 0.4 ?
                (int) (mLvHD->viewport()->width() * 0.4) : minLength;

    mLvHD->setColumnWidth (0, minLength + 10 /* little spacing */);
    mLvHD->setColumnWidth (1, mLvHD->viewport()->width() - mLvHD->columnWidth (0));
}

#include "VBoxHardDiskSettings.ui.moc"

