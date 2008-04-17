/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxHardDiskSettings widget UI include (Qt Designer)
 */

/*
 * Copyright (C) 2008 innotek GmbH
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
** If you wish to add, delete or rename functions or slots use
** Qt Designer which will update this file, preserving your code. Create an
** init() function in place of a constructor, and a destroy() function in
** place of a destructor.
*****************************************************************************/

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

    QValueList <HDSlot> list (HDSlotItem *aForSubscriber);

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
    QValueList <HDSlot> mIDEList;
    QValueList <HDSlot> mSATAList;
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

        refresh();
        mUniq->subscribe (this);
        qApp->installEventFilter (this);
        connect (mUniq, SIGNAL (listChanged()), this, SLOT (refresh()));
        connect (this, SIGNAL (activated (int)), mUniq, SIGNAL (listChanged()));
    }

   ~HDSlotItem()
    {
        mUniq->unsubscribe (this);
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

        for (QValueList <HDSlot>::const_iterator it = mHDSlots.begin();
             it != mHDSlots.end(); ++ it)
        {
            insertItem ((*it).str);
            if (!setCurrent)
                setCurrent = (*it).str == current;
        }

        if (setCurrent)
            setCurrentText (current);
    }

private:

    bool eventFilter (QObject *aObject, QEvent *aEvent)
    {
        if (aObject != listBox())
            return false;

        if (aEvent->type() == QEvent::Hide)
            hide();

        return QComboBox::eventFilter (aObject, aEvent);
    }

    HDSlotUniquizer *mUniq;

    QValueList <HDSlot> mHDSlots;
};

/**
 *  VBoxMediaComboBox class reimplementation to use as selector for VDI
 *  image.
 */
class HDVdiItem : public VBoxMediaComboBox
{
public:

    HDVdiItem (QWidget *aParent, int aType)
        : VBoxMediaComboBox (aParent, "VBoxMediaComboBox", aType)
    {
        qApp->installEventFilter (this);
    }

private:

    bool eventFilter (QObject *aObject, QEvent *aEvent)
    {
        if (aObject != listBox())
            return false;

        if (aEvent->type() == QEvent::Hide)
            hide();

        return VBoxMediaComboBox::eventFilter (aObject, aEvent);
    }
};

QValueList <HDSlot> HDSlotUniquizer::list (HDSlotItem *aSubscriber)
{
    QValueList <HDSlot> list = mIDEList + mSATAList;

    /* Compose exclude list */
    QStringList excludeList;
    for (uint i = 0; i < mSubscribersList.size(); ++ i)
        if (mSubscribersList [i] != aSubscriber)
            excludeList << mSubscribersList [i]->currentText();

    /* Filter the list */
    QValueList <HDSlot>::Iterator it = list.begin();
    while (it != list.end())
    {
        if (excludeList.contains ((*it).str))
            it = list.remove (it);
        else
            ++ it;
    }

    return list;
}

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
        return static_cast <HDSlotItem *> (mVector [0])->currentBus();
    }

    LONG channel() const
    {
        return static_cast <HDSlotItem *> (mVector [0])->currentChannel();
    }

    LONG device() const
    {
        return static_cast <HDSlotItem *> (mVector [0])->currentDevice();
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
        {
            mVector [mFocusColumn]->show();
            mVector [mFocusColumn]->popup();
        }
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
            mVector [0]->setCurrentText (device);

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

        VBoxMediaComboBox *cbvdi = new HDVdiItem (listView()->viewport(), VBoxDefs::HD);
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

        QColorGroup cg (aColorGroup);
        if (aColumn == mFocusColumn)
        {
            cg.setColor (QColorGroup::Base, aColorGroup.highlight());
            cg.setColor (QColorGroup::Text, aColorGroup.highlightedText());
        }
        QListViewItem::paintCell (aPainter, cg, aColumn, aWidth, aAlign);
    }

    void paintFocus (QPainter *aPainter, const QColorGroup &aColorGroup,
                     const QRect &aRect)
    {
        if (mFocusColumn != -1)
        {
            int indent = 0;
            for (int i = 0; i < mFocusColumn; ++ i)
                indent = listView()->columnWidth (i);

            QRect newFocus (QPoint (aRect.x() + indent, aRect.y()),
                            QSize (listView()->columnWidth (mFocusColumn),
                                   aRect.height()));

            QListViewItem::paintFocus (aPainter, aColorGroup, newFocus);
        }
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

    mBtnAdd->setIconSet (VBoxGlobal::iconSet ("vdm_add_16px.png",
                                              "vdm_add_disabled_16px.png"));
    mBtnDel->setIconSet (VBoxGlobal::iconSet ("vdm_remove_16px.png",
                                              "vdm_remove_disabled_16px.png"));
    mBtnOpn->setIconSet (VBoxGlobal::iconSet ("select_file_16px.png",
                                              "select_file_dis_16px.png"));

    mSlotUniquizer = new HDSlotUniquizer (this);

    connect (mCbSATA, SIGNAL (toggled (bool)),
             this, SLOT (onToggleSATAController (bool)));
    connect (mBtnAdd, SIGNAL (clicked()), this, SLOT (addHDItem()));
    connect (mBtnDel, SIGNAL (clicked()), this, SLOT (delHDItem()));
    connect (mBtnOpn, SIGNAL (clicked()), this, SLOT (showVDM()));
    connect (mLvHD, SIGNAL (pressed (QListViewItem*, const QPoint&, int)),
             this, SLOT (moveFocus (QListViewItem*, const QPoint&, int)));
    connect (mLvHD, SIGNAL (currentChanged (QListViewItem*)),
             this, SLOT (onCurrentChanged (QListViewItem*)));
    connect (mLvHD, SIGNAL (contextMenuRequested (QListViewItem*, const QPoint&, int)),
             this, SLOT (onContextMenuRequested (QListViewItem*, const QPoint&, int)));

    qApp->installEventFilter (this);
}

void VBoxHardDiskSettings::getFromMachine (const CMachine &aMachine)
{
    mMachine = aMachine;

    mCbSATA->setChecked (mMachine.GetSATAController().GetEnabled());

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
    mMachine.GetSATAController().SetEnabled (mCbSATA->isChecked());

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
    HDListItem *item = mLvHD->firstChild() &&
        mLvHD->firstChild()->rtti() == HDListItem::HDListItemType ?
        static_cast<HDListItem*> (mLvHD->firstChild()) : 0;
    while (item)
    {
        mMachine.AttachHardDisk (item->getId(),
            item->bus(), item->channel(), item->device());
        if (!mMachine.isOk())
            vboxProblem().cannotAttachHardDisk (this, mMachine, item->getId(),
                item->bus(), item->channel(), item->device());
        item = item->nextSibling();
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
            result = tr ("%1 uses disk image already assigned to %2")
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
        static_cast<HDListItem*> (aItem)->moveFocusToColumn (aCol);
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
            int rc = vboxProblem().confirmSATASlotsRemoving (this);
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

    int newSATAPorts = aOn && !mMachine.isNull() ?
                       mMachine.GetSATAController().GetPortCount() : 0;
    if (mSlotUniquizer->getSATAPortsCount() != newSATAPorts)
    {
        mSlotUniquizer->setSATAPortsCount (newSATAPorts);
        onAfterCurrentChanged (mLvHD->currentItem());
    }
}

void VBoxHardDiskSettings::onAfterCurrentChanged (QListViewItem *aItem)
{
    /* Process postponed onCurrentChanged event */
    mBtnAdd->setEnabled (mLvHD->childCount() < mSlotUniquizer->totalCount());
    mBtnDel->setEnabled (aItem);
    mBtnOpn->setEnabled (aItem);

    if (aItem == mPrevItem)
        return;

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

void VBoxHardDiskSettings::onContextMenuRequested (QListViewItem*,
                                                   const QPoint &aPoint, int)
{
    QAction opnAction (tr ("Open Virtual Disk Manager"), QKeySequence (tr ("Ctrl+Space")), this);
    QAction delAction (tr ("Delete Attachment"), QKeySequence (tr ("Delete")), this);
    delAction.setIconSet (VBoxGlobal::iconSet ("vdm_remove_16px.png",
                                               "vdm_remove_disabled_16px.png"));
    opnAction.setIconSet (VBoxGlobal::iconSet ("select_file_16px.png",
                                               "select_file_dis_16px.png"));
    QPopupMenu menu (this);
    opnAction.addTo (&menu);
    delAction.addTo (&menu);
    int id = menu.exec (aPoint);
    int index = id == -1 ? id : menu.indexOf (id);
    if (index == 0)
        showVDM();
    else if (index == 1)
        delHDItem();
}

HDListItem* VBoxHardDiskSettings::createItem (HDSlotUniquizer *aUniq,
                                              const CMachine &aMachine)
{
    return mLvHD->lastItem() ?
        new HDListItem (this, mLvHD, mLvHD->lastItem(), aUniq, aMachine) :
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

            HDListItem *item = mLvHD->currentItem() &&
                mLvHD->currentItem()->rtti() == HDListItem::HDListItemType ?
                static_cast<HDListItem*> (mLvHD->currentItem()) : 0;
            if (item)
                item->showEditor();
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
                    item->moveFocusToColumn (item->focusColumn() - 1);
            } else
            /* Process cursor-right as "move focus right" action */
            if (e->key() == Qt::Key_Right && !e->state())
            {
                if (item && item->focusColumn() != -1 &&
                    item->focusColumn() < mLvHD->columns() - 1)
                    item->moveFocusToColumn (item->focusColumn() + 1);
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

#include "VBoxHardDiskSettings.ui.moc"

