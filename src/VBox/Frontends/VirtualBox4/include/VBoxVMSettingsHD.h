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

#include "VBoxVMSettingsHD.gen.h"
#include "COMDefs.h"
#include "VBoxMediaComboBox.h"

/* Qt includes */
#include <QComboBox>

class VBoxVMSettingsDlg;
class QIWidgetValidator;

/** Register type to store slot data */
class HDSltValue
{
public:
    HDSltValue()
        : name (QString::null), bus (KStorageBus_Null)
        , channel (0), device (0) {}
    HDSltValue (const QString &aName, KStorageBus aBus,
                LONG aChannel, LONG aDevice)
        : name (aName), bus (aBus)
        , channel (aChannel), device (aDevice) {}
    HDSltValue (const HDSltValue &aOther)
        : name (aOther.name), bus (aOther.bus)
        , channel (aOther.channel), device (aOther.device) {}

    HDSltValue& operator= (const HDSltValue &aOther)
    {
        name    = aOther.name;
        bus     = aOther.bus;
        channel = aOther.channel;
        device  = aOther.device;
        return *this;
    }

    bool operator== (const HDSltValue &aOther)
    {
        return name    == aOther.name &&
               bus     == aOther.bus &&
               channel == aOther.channel &&
               device  == aOther.device;
    }

    bool operator!= (const HDSltValue &aOther)
    {
        return ! (*this == aOther);
    }

    QString     name;
    KStorageBus bus;
    LONG        channel;
    LONG        device;
};
Q_DECLARE_METATYPE (HDSltValue);

/** Register type to store vdi data */
class HDVdiValue
{
public:
    HDVdiValue()
        : name (QString::null), id (QUuid()) {}
    HDVdiValue (const QString &aName, const QUuid &aId)
        : name (aName), id (aId) {}
    HDVdiValue (const HDVdiValue &aOther)
        : name (aOther.name), id (aOther.id) {}

    HDVdiValue& operator= (const HDVdiValue &aOther)
    {
        name = aOther.name;
        id   = aOther.id;
        return *this;
    }

    bool operator== (const HDVdiValue &aOther)
    {
        return name == aOther.name &&
               id   == aOther.id;
    }

    bool operator!= (const HDVdiValue &aOther)
    {
        return ! (*this == aOther);
    }

    QString name;
    QUuid   id;
};
Q_DECLARE_METATYPE (HDVdiValue);

/** Declare type to store both slt&vdi data */
class HDValue
{
public:
    HDValue (HDSltValue aSlt, HDVdiValue aVdi)
        : slt (aSlt), vdi (aVdi) {}

    /* Define sorting rules */
    bool operator< (const HDValue &aOther) const
    {
        return slt.bus <  aOther.slt.bus ||
               slt.bus == aOther.slt.bus && slt.channel <  aOther.slt.channel ||
               slt.bus == aOther.slt.bus && slt.channel == aOther.slt.channel && slt.device <  aOther.slt.device;
    }

    HDSltValue slt;
    HDVdiValue vdi;
};

/** QAbstractTableModel class reimplementation to feat slot/vdi
  * selection mechanism */
class HDItemsModel : public QAbstractTableModel
{
    Q_OBJECT

public:

    HDItemsModel (QObject *aParent, int aSltId, int aVdiId)
        : QAbstractTableModel (aParent)
        , mSltId (aSltId), mVdiId (aVdiId) {}

    int columnCount (const QModelIndex &aParent = QModelIndex()) const
        { NOREF (aParent); return 2; }
    int rowCount (const QModelIndex &aParent = QModelIndex()) const
        { NOREF (aParent); return mSltList.count() + 1; }
    Qt::ItemFlags flags (const QModelIndex &aIndex) const;

    QVariant data (const QModelIndex &aIndex,
                   int aRole = Qt::DisplayRole) const;
    bool setData (const QModelIndex &aIndex,
                  const QVariant &aValue,
                  int aRole = Qt::EditRole);
    QVariant headerData (int aSection,
                         Qt::Orientation aOrientation,
                         int aRole = Qt::DisplayRole) const;

    void addItem (const HDSltValue &aSlt = HDSltValue(),
                  const HDVdiValue &aVdi = HDVdiValue());
    void delItem (int aIndex);

    const QList<HDSltValue>& slotsList() { return mSltList; }
    const QList<HDVdiValue>& vdiList() { return mVdiList; }
    QList<HDValue> fullList (bool aSorted = true);

    void removeSata();

private:

    QList<HDSltValue> mSltList;
    QList<HDVdiValue> mVdiList;
    int mSltId;
    int mVdiId;
};

/** QComboBox class reimplementation used as editor for hd slot */
class HDSltEditor : public QComboBox
{
    Q_OBJECT
    Q_PROPERTY (QVariant slot READ slot WRITE setSlot USER true);

public:

    HDSltEditor (QWidget *aParent);

    QVariant slot() const;
    void setSlot (QVariant aSlot);

signals:

    void readyToCommit (QWidget *aThis);

private slots:

    void onActivate();

private:

    void populate (const HDSltValue &aIncluding);

    QList<HDSltValue> mList;
};

/** VBoxMediaComboBox class reimplementation used as editor for hd vdi */
class HDVdiEditor : public VBoxMediaComboBox
{
    Q_OBJECT
    Q_PROPERTY (QVariant vdi READ vdi WRITE setVdi USER true);

public:

    HDVdiEditor (QWidget *aParent);
   ~HDVdiEditor();

    QVariant vdi() const;
    void setVdi (QVariant aVdi);

    void tryToChooseUniqueVdi (QList<HDVdiValue> &aList);

    static HDVdiEditor* activeEditor();

signals:

    void readyToCommit (QWidget *aThis);

private slots:

    void onActivate();

private:

    static HDVdiEditor *mInstance;
};

/** Singleton QObject class reimplementation to use for making
  * selected IDE & SATA slots unique */
class HDSlotUniquizer : public QObject
{
    Q_OBJECT

public:

    static HDSlotUniquizer* instance (QWidget *aParent = 0,
                                      HDItemsModel *aWatched = 0);

    QList<HDSltValue> list (const HDSltValue &aIncluding, bool aFilter = true);

    int sataCount() { return mSataCount; }
    void setSataCount (int aSataCount)
    {
        mSataCount = aSataCount;
        makeSATAList();
    }

protected:

    HDSlotUniquizer (QWidget *aParent, HDItemsModel *aWatched);
    virtual ~HDSlotUniquizer();

private:

    void makeIDEList();
    void makeSATAList();

    static HDSlotUniquizer *mInstance;

    int mSataCount;
    HDItemsModel *mModel;
    QList<HDSltValue> mIDEList;
    QList<HDSltValue> mSATAList;
};

/** QWidget class reimplementation used as hard disks settings */
class VBoxVMSettingsHD : public QWidget,
                         public Ui::VBoxVMSettingsHD
{
    Q_OBJECT

public:

    VBoxVMSettingsHD (QWidget *aParent, VBoxVMSettingsDlg *aDlg,
                      const QString &aPath);
   ~VBoxVMSettingsHD();

    static void getFromMachine (const CMachine &aMachine,
                                QWidget *aPage,
                                VBoxVMSettingsDlg *aDlg,
                                const QString &aPath);
    static void putBackToMachine();
    static bool revalidate (QString &aWarning);

    bool eventFilter (QObject *aObj, QEvent *aEvent);

    static CMachine mMachine;

signals:

    void hdChanged();

protected:

    void getFrom();
    void putBackTo();
    bool validate (QString &aWarning);

private slots:

    void newClicked();
    void delClicked();
    void vdmClicked();

    void onCurrentChanged (const QModelIndex &aIndex);
    void cbSATAToggled (int);
    void onMediaRemoved (VBoxDefs::DiskType, const QUuid &);

private:

    int maxNameLength() const;
    void showEvent (QShowEvent *aEvent);

    static VBoxVMSettingsHD *mSettings;

    QIWidgetValidator *mValidator;
    HDItemsModel *mModel;
    QAction *mNewAction;
    QAction *mDelAction;
    QAction *mVdmAction;
};

#endif // __VBoxVMSettingsHD_h__

