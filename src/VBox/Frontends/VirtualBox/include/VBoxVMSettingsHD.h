/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsHD class declaration
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

#ifndef __VBoxVMSettingsHD_h__
#define __VBoxVMSettingsHD_h__

#include "VBoxSettingsPage.h"
#include "VBoxVMSettingsHD.gen.h"
#include "COMDefs.h"
#include "VBoxMediaComboBox.h"

/* Qt includes */
#include <QComboBox>

/** Register type to store slot data */
class SlotValue
{
public:
    SlotValue()
        : bus (KStorageBus_Null), channel (0), device (0)
        , name (QString::null) {}
    SlotValue (KStorageBus aBus, LONG aChannel, LONG aDevice)
        : bus (aBus), channel (aChannel), device (aDevice)
        , name (vboxGlobal().toFullString (aBus, aChannel, aDevice)) {}
    SlotValue (const SlotValue &aOther)
        : bus (aOther.bus), channel (aOther.channel), device (aOther.device)
        , name (aOther.name) {}

    SlotValue& operator= (const SlotValue &aOther)
    {
        bus     = aOther.bus;
        channel = aOther.channel;
        device  = aOther.device;
        name    = aOther.name;
        return *this;
    }

    bool operator== (const SlotValue &aOther)
    {
        return bus     == aOther.bus &&
               channel == aOther.channel &&
               device  == aOther.device;
    }

    bool operator!= (const SlotValue &aOther)
    {
        return ! (*this == aOther);
    }

    KStorageBus bus;
    LONG        channel;
    LONG        device;
    QString     name;
};
Q_DECLARE_METATYPE (SlotValue);

/** Register type to store disk data */
class DiskValue
{
public:
    DiskValue()
        : id (QUuid())
        , name (QString::null), tip (QString::null), pix (QPixmap()) {}
    DiskValue (const QUuid &aId);
    DiskValue (const DiskValue &aOther)
        : id (aOther.id)
        , name (aOther.name), tip (aOther.tip), pix (aOther.pix) {}

    DiskValue& operator= (const DiskValue &aOther)
    {
        id   = aOther.id;
        name = aOther.name;
        tip  = aOther.tip;
        pix  = aOther.pix;
        return *this;
    }

    bool operator== (const DiskValue &aOther)
    {
        return id == aOther.id;
    }

    bool operator!= (const DiskValue &aOther)
    {
        return ! (*this == aOther);
    }

    QUuid   id;
    QString name;
    QString tip;
    QPixmap pix;
};
Q_DECLARE_METATYPE (DiskValue);

/** Declare type to store both slot&disk data */
class Attachment
{
public:
    Attachment (SlotValue aSlot, DiskValue aDisk)
        : slot (aSlot), disk (aDisk) {}

    /* Define sorting rules */
    bool operator< (const Attachment &aOther) const
    {
        return (slot.bus <  aOther.slot.bus) ||
               (slot.bus == aOther.slot.bus && slot.channel <  aOther.slot.channel) ||
               (slot.bus == aOther.slot.bus && slot.channel == aOther.slot.channel && slot.device <  aOther.slot.device);
    }

    SlotValue slot;
    DiskValue disk;
};

/**
 * QAbstractTableModel class reimplementation.
 * Used to feat slot/disk selection mechanism.
 */
class AttachmentsModel : public QAbstractTableModel
{
    Q_OBJECT;

public:

    AttachmentsModel (QITableView *aParent, int aSlotId, int aDiskId)
        : QAbstractTableModel (aParent), mParent (aParent)
        , mSlotId (aSlotId), mDiskId (aDiskId) {}

    Qt::ItemFlags flags (const QModelIndex &aIndex) const;

    int columnCount (const QModelIndex &aParent = QModelIndex()) const
        { NOREF (aParent); return 2; }
    int rowCount (const QModelIndex &aParent = QModelIndex()) const
        { NOREF (aParent); return mUsedSlotsList.count() + 1; }

    QVariant data (const QModelIndex &aIndex,
                   int aRole = Qt::DisplayRole) const;
    bool setData (const QModelIndex &aIndex,
                  const QVariant &aValue,
                  int aRole = Qt::EditRole);
    QVariant headerData (int aSection,
                         Qt::Orientation aOrientation,
                         int aRole = Qt::DisplayRole) const;

    void addItem (const SlotValue &aSlot = SlotValue(),
                  const DiskValue &aDisk = DiskValue());
    void delItem (int aIndex);

    const QList<SlotValue>& usedSlotsList() { return mUsedSlotsList; }
    const QList<DiskValue>& usedDisksList() { return mUsedDisksList; }
    QList<Attachment> fullUsedList();

    void removeSata();
    void updateDisks();

private:

    QITableView *mParent;
    QList<SlotValue> mUsedSlotsList;
    QList<DiskValue> mUsedDisksList;
    int mSlotId;
    int mDiskId;
};

/**
 * QComboBox class reimplementation.
 * Used as editor for HD Attachment SLOT field.
 */
class SlotEditor : public QComboBox
{
    Q_OBJECT;
    Q_PROPERTY (QVariant slot READ slot WRITE setSlot USER true);

public:

    SlotEditor (QWidget *aParent);

    QVariant slot() const;
    void setSlot (QVariant aSlot);

signals:

    void readyToCommit (QWidget *aThis);

private slots:

    void onActivate();

private:

#if 0 /* F2 key binding left for future releases... */
    void keyPressEvent (QKeyEvent *aEvent);
#endif

    void populate (const SlotValue &aIncluding);

    QList<SlotValue> mList;
};

/**
 * VBoxMediaComboBox class reimplementation.
 * Used as editor for HD Attachment DISK field.
 */
class DiskEditor : public VBoxMediaComboBox
{
    Q_OBJECT;
    Q_PROPERTY (QVariant disk READ disk WRITE setDisk USER true);

public:

    static DiskEditor* activeEditor();

    DiskEditor (QWidget *aParent);
   ~DiskEditor();

    QVariant disk() const;
    void setDisk (QVariant aDisk);

signals:

    void readyToCommit (QWidget *aThis);

protected:

    void paintEvent (QPaintEvent *aEvent);
    void initStyleOption (QStyleOptionComboBox *aOption) const;

private slots:

    void onActivate();

private:

#if 0 /* F2 key binding left for future releases... */
    void keyPressEvent (QKeyEvent *aEvent);
#endif

    static DiskEditor *mInstance;
};

/**
 * Singleton QObject class reimplementation.
 * Used to make selected HD Attachments slots unique &
 * stores some specific data used for HD Settings.
 */
class HDSettings : public QObject
{
    Q_OBJECT;

public:

    static HDSettings* instance (QWidget *aParent = 0,
                                 AttachmentsModel *aWatched = 0);

    QList<SlotValue> slotsList (const SlotValue &aIncluding = SlotValue(),
                                bool aFilter = false) const;
    QList<DiskValue> disksList() const;

    bool tryToChooseUniqueDisk (DiskValue &aResult) const;

    const CMachine& machine() const { return mMachine; }
    void setMachine (const CMachine &aMachine) { mMachine = aMachine; }

    int sataCount() const { return mSataCount; }
    void setSataCount (int aSataCount)
    {
        if (mSataCount != aSataCount)
        {
            mSataCount = aSataCount;
            makeSATAList();
        }
    }

    bool showDiffs() const { return mShowDiffs; }
    void setShowDiffs (bool aShowDiffs)
    {
        mShowDiffs = aShowDiffs;
        update();
    }

protected:

    HDSettings (QWidget *aParent, AttachmentsModel *aWatched);
    virtual ~HDSettings();

private slots:

    void update()
    {
        makeMediumList();
        mModel->updateDisks();
    }

private:

    void makeIDEList();
    void makeSATAList();
    void makeMediumList();

    static HDSettings *mInstance;

    AttachmentsModel *mModel;
    CMachine mMachine;

    QList<SlotValue> mIDEList;
    QList<SlotValue> mSATAList;
    QList<DiskValue> mDisksList;

    int mSataCount;
    bool mShowDiffs;
};

/**
 * QWidget class reimplementation.
 * Used as HD Settings widget.
 */
class VBoxVMSettingsHD : public VBoxSettingsPage,
                         public Ui::VBoxVMSettingsHD
{
    Q_OBJECT;

public:

    VBoxVMSettingsHD();

signals:

    void hdChanged();

protected:

    void getFrom (const CMachine &aMachine);
    void putBackTo();

    void setValidator (QIWidgetValidator *aVal);
    bool revalidate (QString &aWarning, QString &aTitle);

    void setOrderAfter (QWidget *aWidget);

    void retranslateUi();

private slots:

    void addAttachment();
    void delAttachment();
    void showMediaManager();

    void onSATACheckToggled (int);
    void onShowDiffsCheckToggled (int);

    void updateActions (const QModelIndex &aIndex);

private:

    /* events */
    bool eventFilter (QObject *aObj, QEvent *aEvent);
    void showEvent (QShowEvent *aEvent);

    /* private functions */
    QUuid getWithMediaManager (const QUuid &aInitialId = QUuid());
    QUuid getWithNewHDWizard();
    int maxNameLength() const;
    void removeFocus();

    /* variables */
    CMachine mMachine;
    AttachmentsModel *mModel;
    QIWidgetValidator *mValidator;

    QAction *mNewAction;
    QAction *mDelAction;
    QAction *mVdmAction;

    bool mWasTableSelected;
    bool mPolished;
};

#endif // __VBoxVMSettingsHD_h__

