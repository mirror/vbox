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
            mSATAList << HDSlot (vboxGlobal().toFullString (KStorageBus_SATA, i, 0),
                                 KStorageBus_SATA, i, 0);

        emit listChanged();
    }

    int mSataPortsCount;
    QValueList<HDSlot> mIDEList;
    QValueList<HDSlot> mSATAList;
    QPtrVector<HDSlotItem> mSubscribersList;
};

/**
 * QComboBox class reimplementation to use as an editor for the device slot
 * column.
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

    static int scrollBarWidth()
    {
        QListBox lb;
        lb.setVScrollBarMode (QScrollView::AlwaysOn);
        return lb.verticalScrollBar()->width();
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

QValueList <HDSlot> HDSlotUniquizer::list (HDSlotItem *aSubscriber, bool aFilter)
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

/**
 * VBoxMediaComboBox class reimplementation to use as an editor for the hard
 * disk column.
 */
class HDItem : public VBoxMediaComboBox
{
    Q_OBJECT

public:

    HDItem (QWidget *aParent, const char *aName, QUuid aMachineID,
            QListViewItem *aItem)
        : VBoxMediaComboBox (aParent, aName, VBoxDefs::MediaType_HardDisk,
                             aMachineID)
        , mItem (aItem)
    {
        setFocusPolicy (QWidget::NoFocus);

        connect (this, SIGNAL (activated (int)),
                 this, SLOT (onThisActivated (int)));
    }

private slots:

    void onThisActivated (int)
    {
        mItem->repaint();
    }

private:

    QListViewItem *mItem;
};

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

    HDListItem (QListView *aParent, QListViewItem *aAfter,
                VBoxHardDiskSettings *aSettings, HDSlotUniquizer *aUniq,
                const QUuid &aMachineId)
        : QListViewItem (aParent, aAfter)
        , mFocusColumn (-1)
        , mAutoFocus (false)
    {
        init (aSettings, aUniq, aMachineId);
    }

    HDListItem (QListView *aParent,
                VBoxHardDiskSettings *aSettings, HDSlotUniquizer *aUniq,
                const QUuid &aMachineId)
        : QListViewItem (aParent)
        , mFocusColumn (-1)
        , mAutoFocus (false)
    {
        init (aSettings, aUniq, aMachineId);
    }

    virtual ~HDListItem()
    {
        mHDCombo->deleteLater();
        mSlotCombo->deleteLater();
    }

    int rtti() const { return HDListItemType; }

    QString toolTip() { return QToolTip::textFor (mHDCombo); }

    void setId (const QUuid &aId) { mHDCombo->setCurrentItem (aId); }

    QUuid id() const { return mHDCombo->id(); }
    QString location() const { return mHDCombo->location(); }

    KStorageBus bus() { return mSlotCombo->currentBus(); }

    LONG channel() const { return mSlotCombo->currentChannel(); }

    LONG device() const { return mSlotCombo->currentDevice(); }

    QString text (int aColumn) const
    {
        AssertReturn (aColumn >= 0 && (size_t) aColumn < ELEMENTS (mCombos),
                      QString::null);

        return mCombos [aColumn]->currentText();
    }

    const QPixmap *pixmap (int aColumn) const
    {
        AssertReturn (aColumn >= 0 && (size_t) aColumn < ELEMENTS (mCombos),
                      NULL);

        return mCombos [aColumn]->pixmap (mCombos [aColumn]->currentItem());
    }

    void moveFocusToColumn (int aCol)
    {
        mFocusColumn = aCol;
        mAutoFocus = mFocusColumn != -1;
        repaint();
    }

    void setAutoFocus (bool aOn)
    {
        mAutoFocus = aOn;
    }

    void showEditor()
    {
        if (mFocusColumn >= 0)
        {
            AssertReturnVoid ((size_t) mFocusColumn < ELEMENTS (mCombos));

            if (mCombos [mFocusColumn]->count())
                mCombos [mFocusColumn]->popup();
        }
    }

    int focusColumn() const
    {
        return mFocusColumn;
    }

    void setAttachment (const CHardDisk2Attachment &aHda)
    {
        QString device = vboxGlobal()
            .toFullString (aHda.GetBus(), aHda.GetChannel(), aHda.GetDevice());

        if (mSlotCombo->listBox()->findItem (device, Qt::ExactMatch))
            mSlotCombo->setText (device);

        mHDCombo->setCurrentItem (aHda.GetHardDisk().GetId());
    }

    int hardDiskCount()
    {
        int count = mHDCombo->count();
        if (count == 1 && mHDCombo-> id (0).isNull())
            return 0; /* ignore the "no media" item */
        return count;
    }

    void tryToSelectNotOneOf (const QValueList <QUuid> &aUsedList)
    {
        for (int i = 0; i < mHDCombo->count(); ++ i)
        {
            if (!aUsedList.contains (mHDCombo->id (i)))
            {
                mHDCombo->setCurrentItem (mHDCombo->id (i));
                break;
            }
        }
    }

    void setShowDiffs (bool aOn)
    {
        if (mHDCombo->showDiffs() != aOn)
        {
            mHDCombo->setShowDiffs (aOn);
            mHDCombo->refresh();
        }
    }

private:

    void init (VBoxHardDiskSettings *aSettings, HDSlotUniquizer *aUniq,
               const QUuid &aMachineId)
    {
        AssertReturnVoid (listView()->columns() == ELEMENTS (mCombos));

        setSelectable (false);

        mSlotCombo = new HDSlotItem (listView()->viewport(), aUniq);
        QObject::connect (mSlotCombo, SIGNAL (activated (int)),
                          aSettings, SIGNAL (hardDiskListChanged()));
        mCombos [0] = mSlotCombo;

        mHDCombo = new HDItem (listView()->viewport(), "mHDCombo",
                               aMachineId, this);
        QObject::connect (mHDCombo, SIGNAL (activated (int)),
                          aSettings, SIGNAL (hardDiskListChanged()));
        mCombos [1] = mHDCombo;

        /* populate the media combobox */
        mHDCombo->refresh();

#ifdef Q_WS_MAC
        /* White background on Mac OS X */
        mSlotCombo->setPaletteBackgroundColor (mSlotCombo->parentWidget()->
                                               paletteBackgroundColor());
        mHDCombo->setPaletteBackgroundColor (mHDCombo->parentWidget()->
                                             paletteBackgroundColor());
#endif /* Q_WS_MAC */

        mSlotCombo->setHidden (true);
        mHDCombo->setHidden (true);
    }

    void paintCell (QPainter *aPainter, const QColorGroup &aColorGroup,
                    int aColumn, int aWidth, int aAlign)
    {
        AssertReturnVoid (aColumn >= 0 && (size_t) aColumn < ELEMENTS (mCombos));

        QComboBox *cb = mCombos [aColumn];

        /// @todo (r=dmik) show/hide functionality should be removed from here
        /// to the appropriate places like focus handling routines

        if (aColumn == mFocusColumn)
        {
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

            if (cb->isHidden())
                cb->show();
            if (mAutoFocus && !cb->hasFocus())
                QTimer::singleShot (0, cb, SLOT (setFocus()));

            return;

        }

        if (aColumn != mFocusColumn && !cb->isHidden())
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

    HDSlotItem            *mSlotCombo;
    VBoxMediaComboBox     *mHDCombo;
    QComboBox             *mCombos [2];
    int                    mFocusColumn;
    bool                   mAutoFocus;
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

    /* rest */

    new HDSpaceItem (mLvHD);

    mSlotUniquizer = new HDSlotUniquizer (this);

    qApp->installEventFilter (this);
}

void VBoxHardDiskSettings::getFromMachine (const CMachine &aMachine)
{
    AssertReturnVoid (mMachine.isNull());
    AssertReturnVoid (!aMachine.isNull());

    mMachine = aMachine;

    {
        CSATAController ctl = mMachine.GetSATAController();
        if (ctl.isNull())
        {
            /* hide the SATA check box if the SATA controller is not available
             * (i.e. in VirtualBox OSE) */
            mSATACheck->setHidden (true);
        }
        else
        {
            mSATACheck->setChecked (ctl.GetEnabled());
        }
    }

    CHardDisk2AttachmentVector vec = mMachine.GetHardDisk2Attachments();
    for (size_t i = 0; i < vec.size(); ++ i)
    {
        CHardDisk2Attachment hda = vec [i];
        HDListItem *item = createItem();
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
        ctl.SetEnabled (mSATACheck->isChecked());
    }

    /* Detach all attached Hard Disks */
    CHardDisk2AttachmentVector vec = mMachine.GetHardDisk2Attachments();
    for (size_t i = 0; i < vec.size(); ++ i)
    {
        CHardDisk2Attachment hda = vec [i];
        mMachine.DetachHardDisk2 (hda.GetBus(), hda.GetChannel(), hda.GetDevice());
        if (!mMachine.isOk())
            vboxProblem().cannotDetachHardDisk (this, mMachine,
                vboxGlobal().getMedium (CMedium (hda.GetHardDisk())).location(),
                hda.GetBus(), hda.GetChannel(), hda.GetDevice());
    }

    /* Sort & attach all listed Hard Disks */

    mLvHD->setSortColumn (0);
    mLvHD->sort();
    LONG maxSATAPort = 1;

    for (QListViewItem *item = mLvHD->firstChild(); item;
         item = item->nextSibling())
    {
        if (item->rtti() != HDListItem::HDListItemType)
            continue;

        HDListItem *hdi = static_cast <HDListItem *> (item);

        if (hdi->bus() == KStorageBus_SATA)
            maxSATAPort = QMAX (hdi->channel() + 1, maxSATAPort);
        mMachine.AttachHardDisk2 (hdi->id(),
            hdi->bus(), hdi->channel(), hdi->device());
        if (!mMachine.isOk())
            vboxProblem().cannotAttachHardDisk (this, mMachine,
                hdi->location(), hdi->bus(), hdi->channel(), hdi->device());
    }

    if (!ctl.isNull())
    {
        ctl.SetPortCount (maxSATAPort);
    }
}

QString VBoxHardDiskSettings::checkValidity()
{
    QString result;
    QStringList slList;
    QValueList <QUuid> idList;

    /* Search for coincidences through all the media-id */
    for (QListViewItem *item = mLvHD->firstChild(); item;
         item = item->nextSibling())
    {
        if (item->rtti() != HDListItem::HDListItemType)
            continue;

        HDListItem *hdi = static_cast <HDListItem *> (item);

        QUuid id = hdi->id();
        if (id.isNull())
        {
            result = tr ("No hard disk is selected for <i>%1</i>")
                .arg (hdi->text (0));
            break;
        }
        else if (idList.contains (id))
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
    }

    return result;
}

void VBoxHardDiskSettings::addHDItem()
{
    /* Create a new item */
    HDListItem *item = createItem();
    item->moveFocusToColumn (1);
    mLvHD->setCurrentItem (item);
    if (!mLvHD->hasFocus())
        mLvHD->setFocus();

    /* Qt3 doesn't emit currentChanged() signal when the first item is added */
    if (mLvHD->childCount() == 1)
        onCurrentChanged (item);

    /* Search through the attachments for the used hard disks */
    QValueList <QUuid> usedList;
    for (QListViewItem *it = mLvHD->firstChild(); it;
         it = it->nextSibling())
    {
        if (it->rtti() != HDListItem::HDListItemType)
            continue;

        HDListItem *hdi = static_cast <HDListItem *> (it);

        usedList << hdi->id();
    }

    item->tryToSelectNotOneOf (usedList);

    /* Ask the user for a method to add a new hard disk */
    int result = mLvHD->childCount() - 1 > item->hardDiskCount() ?
        vboxProblem().confirmRunNewHDWzdOrVDM (this) :
        QIMessageBox::Cancel;
    if (result == QIMessageBox::Yes)
    {
        VBoxNewHDWzd dlg (this, "VBoxNewHDWzd");
        if (dlg.exec() == QDialog::Accepted)
        {
            CHardDisk2 hd = dlg.hardDisk();
            item->setId (dlg.hardDisk().GetId());
        }
    }
    else if (result == QIMessageBox::No)
        showMediaManager();

    emit hardDiskListChanged();
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

    emit hardDiskListChanged();
}

void VBoxHardDiskSettings::showMediaManager()
{
    HDListItem *item = mLvHD->currentItem() &&
        mLvHD->currentItem()->rtti() == HDListItem::HDListItemType ?
        static_cast <HDListItem *> (mLvHD->currentItem()) : 0;

    VBoxMediaManagerDlg dlg (this, "VBoxMediaManagerDlg",
                             WType_Dialog | WShowModal);

    dlg.setup (VBoxDefs::MediaType_HardDisk, true /* aDoSelect */,
               true /* aRefresh */, mMachine, item->id(),
               mShowDiffsCheck->isOn());

    if (dlg.exec() == VBoxMediaManagerDlg::Accepted)
        item->setId (dlg.selectedId());
}

void VBoxHardDiskSettings::moveFocus (QListViewItem *aItem, const QPoint&, int aCol)
{
    if (aItem && aItem->rtti() == HDListItem::HDListItemType)
    {
        static_cast <HDListItem *> (aItem)->moveFocusToColumn (aCol);
        onAfterCurrentChanged (aItem);
    }
}

void VBoxHardDiskSettings::onCurrentChanged (QListViewItem *aItem)
{
    /* Postpone onCurrentChanged signal to be post-processed after all others */
    QApplication::postEvent (this, new OnItemChangedEvent (aItem));
}

void VBoxHardDiskSettings::onSATACheckToggled (bool aOn)
{
    if (!aOn)
    {
        /* Search the list for the SATA ports */
        HDListItem *sataItem = NULL;
        for (QListViewItem *item = mLvHD->firstChild(); item;
             item = item->nextSibling())
        {
            if (item->rtti() != HDListItem::HDListItemType)
                continue;

            HDListItem *hdi = static_cast <HDListItem *> (item);

            if (hdi->bus() == KStorageBus_SATA)
            {
                sataItem = hdi;
                break;
            }
        }

        /* If list contains at least one SATA port */
        if (sataItem)
        {
            int rc = vboxProblem().confirmDetachSATASlots (this);
            if (rc != QIMessageBox::Ok)
            {
                /* Switch check-box back to "on" */
                mSATACheck->blockSignals (true);
                mSATACheck->setChecked (true);
                mSATACheck->blockSignals (false);
                return;
            }
            else
            {
                /* Delete SATA items */
                mLvHD->blockSignals (true);
                for (QListViewItem *item = mLvHD->firstChild(); item;)
                {
                    if (item->rtti() == HDListItem::HDListItemType)
                    {
                        HDListItem *hdi = static_cast <HDListItem *> (item);
                        if (hdi->bus() == KStorageBus_SATA)
                        {
                            if (hdi == mLvHD->currentItem())
                                mPrevItem = NULL;

                            item = hdi->nextSibling();
                            delete hdi;
                            continue;
                        }
                    }

                    item = item->nextSibling();
                }

                mLvHD->blockSignals (false);
                emit hardDiskListChanged();
            }
        }
    }

    int newSATAPortsCount = aOn && !mMachine.isNull() ? SATAPortsCount : 0;
    if (mSlotUniquizer->getSATAPortsCount() != newSATAPortsCount)
    {
        mSlotUniquizer->setSATAPortsCount (newSATAPortsCount);
        onAfterCurrentChanged (mPrevItem);
    }
}

void VBoxHardDiskSettings::onShowDiffsCheckToggled (bool aOn)
{
    for (QListViewItem *item = mLvHD->firstChild(); item;
         item = item->nextSibling())
    {
        if (item->rtti() != HDListItem::HDListItemType)
            continue;

        HDListItem *hdi = static_cast <HDListItem *> (item);

        hdi->setShowDiffs (aOn);
    }
}

void VBoxHardDiskSettings::onAfterCurrentChanged (QListViewItem *aItem)
{
    /* Process postponed onCurrentChanged event */
    if (aItem != mPrevItem)
    {
        int prevFocusColumn =
            mPrevItem && mPrevItem->rtti() == HDListItem::HDListItemType ?
            static_cast<HDListItem*> (mPrevItem)->focusColumn() : 1;

        if (mPrevItem && mPrevItem->rtti() == HDListItem::HDListItemType)
            static_cast<HDListItem*> (mPrevItem)->moveFocusToColumn (-1);

        if (aItem && aItem->rtti() == HDListItem::HDListItemType &&
            static_cast<HDListItem*> (aItem)->focusColumn() == -1)
            static_cast<HDListItem*> (aItem)->moveFocusToColumn (prevFocusColumn);

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

HDListItem *VBoxHardDiskSettings::createItem()
{
    QListViewItem *item = mLvHD->lastItem();
    Assert (item->rtti() == HDSpaceItem::HDSpaceItemType);
    HDListItem *last = item->itemAbove() &&
        item->itemAbove()->rtti() == HDListItem::HDListItemType ?
        static_cast <HDListItem *> (item->itemAbove()) : 0;

    QUuid machineId = mMachine.GetId();

    HDListItem *newItem = last ?
        new HDListItem (mLvHD, last, this, mSlotUniquizer, machineId) :
        new HDListItem (mLvHD, this, mSlotUniquizer, machineId);

    AssertReturn (newItem != NULL, NULL);

    newItem->setShowDiffs (mShowDiffsCheck->isOn());

    return newItem;
}

bool VBoxHardDiskSettings::event (QEvent *aEvent)
{
    switch (aEvent->type())
    {
        /* Redirect postponed onCurrentChanged event */
        case OnItemChangedEvent::Type:
        {
            OnItemChangedEvent *e = static_cast <OnItemChangedEvent *> (aEvent);
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
            QListViewItem *clickedItem = mLvHD->itemAt (e->pos());
            HDListItem *item = clickedItem &&
                clickedItem->rtti() == HDListItem::HDListItemType ?
                static_cast <HDListItem *> (clickedItem) : 0;

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

            QMouseEvent *e = static_cast <QMouseEvent *> (aEvent);
            QListViewItem *hoveredItem = mLvHD->itemAt (e->pos());
            HDListItem *item = hoveredItem &&
                hoveredItem->rtti() == HDListItem::HDListItemType ?
                static_cast <HDListItem *> (hoveredItem) : 0;

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
            if (!mLvHD->queryList (0, 0, false, true)->contains (aObject))
                break;

            HDListItem *item = mLvHD->currentItem() &&
                mLvHD->currentItem()->rtti() == HDListItem::HDListItemType ?
                static_cast <HDListItem *> (mLvHD->currentItem()) : 0;

            QKeyEvent *e = static_cast<QKeyEvent*> (aEvent);
            /* Process cursor-left as "move focus left" action */
            if (e->key() == Qt::Key_Left && !e->state())
            {
                if (item && item->focusColumn() != -1 &&
                    item->focusColumn() > 0)
                {
                    item->setAutoFocus (false);
                    mLvHD->setFocus();
                    item->moveFocusToColumn (item->focusColumn() - 1);
                    onAfterCurrentChanged (item);
                }
                return true;
            } else
            /* Process cursor-right as "move focus right" action */
            if (e->key() == Qt::Key_Right && !e->state())
            {
                if (item && item->focusColumn() != -1 &&
                    item->focusColumn() < mLvHD->columns() - 1)
                {
                    item->setAutoFocus (false);
                    mLvHD->setFocus();
                    item->moveFocusToColumn (item->focusColumn() + 1);
                    onAfterCurrentChanged (item);
                }
                return true;
            } else
            /* Process cursor-up as "move focus up" action */
            if (e->key() == Qt::Key_Up && !e->state())
            {
                if (item && item->focusColumn() != -1 &&
                    item->itemAbove())
                {
                    item->setAutoFocus (false);
                    mLvHD->setFocus();
                    mLvHD->setCurrentItem (item->itemAbove());
                }
                return true;
            } else
            /* Process cursor-down as "move focus down" action */
            if (e->key() == Qt::Key_Down && !e->state())
            {
                if (item && item->focusColumn() != -1 &&
                    item->itemBelow())
                {
                    item->setAutoFocus (false);
                    mLvHD->setFocus();
                    mLvHD->setCurrentItem (item->itemBelow());
                }
                return true;
            } else
            /* Process F2/Space as "open combo-box" actions */
            if (!e->state() &&
                (e->key() == Qt::Key_F2 || e->key() == Qt::Key_Space))
            {
                if (item)
                    item->showEditor();
                return true;
            }
            /* Process Ctrl/Alt+Up/Down as "open combo-box" actions */
            if ((e->state() == Qt::AltButton || e->state() == Qt::ControlButton) &&
                (e->key() == Qt::Key_Up || e->key() == Qt::Key_Down))
            {
                if (item)
                    item->showEditor();
                return true;
            } else
            if ((e->key() == Qt::Key_Tab && !e->state()) ||
                e->key() == Qt::Key_Backtab)
            {
                item->setAutoFocus (false);
                mLvHD->setFocus();
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

    mLvHD->setColumnWidth (0,
                           minLength /* maximum string width */ +
                           6 * 2 /* 2 combo-box margin */ +
                           HDSlotItem::scrollBarWidth() /* scrollbar */);
    mLvHD->setColumnWidth (1, mLvHD->viewport()->width() - mLvHD->columnWidth (0));
}

#include "VBoxHardDiskSettings.ui.moc"

