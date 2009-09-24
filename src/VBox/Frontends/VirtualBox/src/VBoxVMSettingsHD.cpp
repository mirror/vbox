/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsHD class implementation
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

/* Global Includes */
#include <QHeaderView>
#include <QItemEditorFactory>
#include <QMetaProperty>
#include <QMouseEvent>
#include <QScrollBar>
#include <QStylePainter>
#include <QTimer>

/* Local Includes */
#include "VBoxVMSettingsHD.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "QIWidgetValidator.h"
#include "VBoxToolBar.h"
#include "VBoxMediaManagerDlg.h"
#include "VBoxNewHDWzd.h"

/* String Tags */
const char *firstAvailableId = "first available";

/* Type converters */
VBoxDefs::MediumType typeToLocal (KDeviceType aType)
{
    VBoxDefs::MediumType result = VBoxDefs::MediumType_Invalid;
    switch (aType)
    {
        case KDeviceType_HardDisk:
            result = VBoxDefs::MediumType_HardDisk;
            break;
        case KDeviceType_DVD:
            result = VBoxDefs::MediumType_DVD;
            break;
        case KDeviceType_Floppy:
            result = VBoxDefs::MediumType_Floppy;
            break;
        default:
            AssertMsgFailed (("Incorrect medium type!\n"));
            break;
    }
    return result;
}


KDeviceType typeToGlobal (VBoxDefs::MediumType aType)
{
    KDeviceType result = KDeviceType_Null;
    switch (aType)
    {
        case VBoxDefs::MediumType_HardDisk:
            result = KDeviceType_HardDisk;
            break;
        case VBoxDefs::MediumType_DVD:
            result = KDeviceType_DVD;
            break;
        case VBoxDefs::MediumType_Floppy:
            result = KDeviceType_Floppy;
            break;
        default:
            AssertMsgFailed (("Incorrect device type!\n"));
            break;
    }
    return result;
}

QString compressText (const QString &aText)
{
    return QString ("<nobr><compact elipsis=\"end\">%1</compact></nobr>").arg (aText);
}


/* Pixmap Storage */
QPointer <PixmapPool> PixmapPool::mThis = 0;

PixmapPool* PixmapPool::pool (QObject *aParent)
{
    if (!mThis)
    {
        AssertMsg (aParent, ("This object must have parent!\n"));
        mThis = new PixmapPool (aParent);
    }
    else
    {
        AssertMsg (!aParent, ("Parent already set!\n"));
    }
    return mThis;
}

PixmapPool::PixmapPool (QObject *aParent)
    : QObject (aParent)
{
    mPool.resize (MaxIndex);

    mPool [AddControllerEn]  = QPixmap (":/add_host_iface_16px.png"); // TODO (":/controller_add_16px.png");
    mPool [AddControllerDis] = QPixmap (":/add_host_iface_disabled_16px.png"); // TODO (":/controller_add_disabled_16px.png");
    mPool [DelControllerEn]  = QPixmap (":/remove_host_iface_16px.png"); // TODO (":/controller_remove_16px.png");
    mPool [DelControllerDis] = QPixmap (":/remove_host_iface_disabled_16px.png"); // TODO (":/controller_remove_disabled_16px.png");

    mPool [AddAttachmentEn]  = QPixmap (":/attachment_add_16px.png");
    mPool [AddAttachmentDis] = QPixmap (":/attachment_add_disabled_16px.png");
    mPool [DelAttachmentEn]  = QPixmap (":/attachment_remove_16px.png");
    mPool [DelAttachmentDis] = QPixmap (":/attachment_remove_disabled_16px.png");

    mPool [IDEController]    = QPixmap (":/ide_16px.png");
    mPool [SATAController]   = QPixmap (":/sata_16px.png");
    mPool [SCSIController]   = QPixmap (":/scsi_16px.png");
    mPool [FloppyController] = QPixmap (":/floppy_16px.png");

    mPool [HDAttachmentEn]   = QPixmap (":/hd_16px.png");
    mPool [HDAttachmentDis]  = QPixmap (":/hd_disabled_16px.png");
    mPool [CDAttachmentEn]   = QPixmap (":/cd_16px.png");
    mPool [CDAttachmentDis]  = QPixmap (":/cd_disabled_16px.png");
    mPool [FDAttachmentEn]   = QPixmap (":/fd_16px.png");
    mPool [FDAttachmentDis]  = QPixmap (":/fd_disabled_16px.png");

    mPool [PlusEn]           = QPixmap (":/plus_10px.png");
    mPool [PlusDis]          = QPixmap (":/plus_disabled_10px.png");
    mPool [MinusEn]          = QPixmap (":/minus_10px.png");
    mPool [MinusDis]         = QPixmap (":/minus_disabled_10px.png");

    mPool [UnknownEn]        = QPixmap (":/help_16px.png");

    mPool [VMMEn]            = QPixmap (":/select_file_16px.png");
    mPool [VMMDis]           = QPixmap (":/select_file_dis_16px.png");
}

QPixmap PixmapPool::pixmap (PixmapType aType) const
{
    return aType > InvalidPixmap && aType < MaxIndex ? mPool [aType] : 0;
}

/* Abstract Controller Type */
AbstractControllerType::AbstractControllerType (KStorageBus aBusType, KStorageControllerType aCtrType)
    : mBusType (aBusType)
    , mCtrType (aCtrType)
    , mPixmap (PixmapPool::InvalidPixmap)
{
    AssertMsg (mBusType != KStorageBus_Null, ("Wrong Bus Type {%d}!\n", mBusType));
    AssertMsg (mCtrType != KStorageControllerType_Null, ("Wrong Controller Type {%d}!\n", mCtrType));

    switch (mBusType)
    {
        case KStorageBus_IDE:
            mPixmap = PixmapPool::IDEController;
            break;
        case KStorageBus_SATA:
            mPixmap = PixmapPool::SATAController;
            break;
        case KStorageBus_SCSI:
            mPixmap = PixmapPool::SCSIController;
            break;
        case KStorageBus_Floppy:
            mPixmap = PixmapPool::FloppyController;
            break;
        default:
            break;
    }

    AssertMsg (mPixmap != PixmapPool::InvalidPixmap, ("Bus pixmap was not set!\n"));
}

KStorageBus AbstractControllerType::busType() const
{
    return mBusType;
}

KStorageControllerType AbstractControllerType::ctrType() const
{
    return mCtrType;
}

ControllerTypeList AbstractControllerType::ctrTypes() const
{
    ControllerTypeList result;
    for (uint i = first(); i < first() + size(); ++ i)
        result << (KStorageControllerType) i;
    return result;
}

PixmapPool::PixmapType AbstractControllerType::pixmap() const
{
    return mPixmap;
}

void AbstractControllerType::setCtrType (KStorageControllerType aCtrType)
{
    mCtrType = aCtrType;
}

/* IDE Controller Type */
IDEControllerType::IDEControllerType (KStorageControllerType aSubType)
    : AbstractControllerType (KStorageBus_IDE, aSubType)
{
}

DeviceTypeList IDEControllerType::deviceTypeList() const
{
    return DeviceTypeList() << KDeviceType_HardDisk << KDeviceType_DVD;
}

KStorageControllerType IDEControllerType::first() const
{
    return KStorageControllerType_PIIX3;
}

uint IDEControllerType::size() const
{
    return 3;
}

/* SATA Controller Type */
SATAControllerType::SATAControllerType (KStorageControllerType aSubType)
    : AbstractControllerType (KStorageBus_SATA, aSubType)
{
}

DeviceTypeList SATAControllerType::deviceTypeList() const
{
    return DeviceTypeList() << KDeviceType_HardDisk << KDeviceType_DVD;
}

KStorageControllerType SATAControllerType::first() const
{
    return KStorageControllerType_IntelAhci;
}

uint SATAControllerType::size() const
{
    return 1;
}

/* SCSI Controller Type */
SCSIControllerType::SCSIControllerType (KStorageControllerType aSubType)
    : AbstractControllerType (KStorageBus_SCSI, aSubType)
{
}

DeviceTypeList SCSIControllerType::deviceTypeList() const
{
    return DeviceTypeList() << KDeviceType_HardDisk << KDeviceType_DVD;
}

KStorageControllerType SCSIControllerType::first() const
{
    return KStorageControllerType_LsiLogic;
}

uint SCSIControllerType::size() const
{
    return 2;
}

/* Floppy Controller Type */
FloppyControllerType::FloppyControllerType (KStorageControllerType aSubType)
    : AbstractControllerType (KStorageBus_Floppy, aSubType)
{
}

DeviceTypeList FloppyControllerType::deviceTypeList() const
{
    return DeviceTypeList() << KDeviceType_Floppy;
}

KStorageControllerType FloppyControllerType::first() const
{
    return KStorageControllerType_I82078;
}

uint FloppyControllerType::size() const
{
    return 1;
}

/* Abstract Item */
AbstractItem::AbstractItem (AbstractItem *aParent)
    : mParent (aParent)
    , mId (QUuid::createUuid())
{
    if (mParent) mParent->addChild (this);
}

AbstractItem::~AbstractItem()
{
    if (mParent) mParent->delChild (this);
}

AbstractItem* AbstractItem::parent() const
{
    return mParent;
}

QUuid AbstractItem::id() const
{
    return mId;
}

QString AbstractItem::machineId() const
{
    return mMachineId;
}

void AbstractItem::setMachineId (const QString &aMchineId)
{
    mMachineId = aMchineId;
}

/* Root Item */
RootItem::RootItem()
    : AbstractItem (0)
{
}

RootItem::~RootItem()
{
    while (!mControllers.isEmpty())
        delete mControllers.first();
}

AbstractItem::ItemType RootItem::rtti() const
{
    return Type_RootItem;
}

AbstractItem* RootItem::childByPos (int aIndex)
{
    return mControllers [aIndex];
}

AbstractItem* RootItem::childById (const QUuid &aId)
{
    for (int i = 0; i < childCount(); ++ i)
        if (mControllers [i]->id() == aId)
            return mControllers [i];
    return 0;
}

int RootItem::posOfChild (AbstractItem *aItem) const
{
    return mControllers.indexOf (aItem);
}

int RootItem::childCount() const
{
    return mControllers.size();
}

QString RootItem::text() const
{
    return QString();
}

QString RootItem::tip() const
{
    return QString();
}

QPixmap RootItem::pixmap()
{
    return QPixmap();
}

void RootItem::addChild (AbstractItem *aItem)
{
    mControllers << aItem;
}

void RootItem::delChild (AbstractItem *aItem)
{
    mControllers.removeAll (aItem);
}

/* Controller Item */
ControllerItem::ControllerItem (AbstractItem *aParent, const QString &aName,
                                KStorageBus aBusType, KStorageControllerType aControllerType)
    : AbstractItem (aParent)
    , mCtrName (aName)
    , mCtrType (0)
{
    /* Check for proper parent type */
    AssertMsg (mParent->rtti() == AbstractItem::Type_RootItem, ("Incorrect parent type!\n"));

    /* Select default type */
    switch (aBusType)
    {
        case KStorageBus_IDE:
            mCtrType = new IDEControllerType (aControllerType);
            break;
        case KStorageBus_SATA:
            mCtrType = new SATAControllerType (aControllerType);
            break;
        case KStorageBus_SCSI:
            mCtrType = new SCSIControllerType (aControllerType);
            break;
        case KStorageBus_Floppy:
            mCtrType = new FloppyControllerType (aControllerType);
            break;
        default:
            AssertMsgFailed (("Wrong Controller Type {%d}!\n", aBusType));
            break;
    }
}

ControllerItem::~ControllerItem()
{
    delete mCtrType;
    while (!mAttachments.isEmpty())
        delete mAttachments.first();
}

AbstractItem::ItemType ControllerItem::rtti() const
{
    return Type_ControllerItem;
}

AbstractItem* ControllerItem::childByPos (int aIndex)
{
    return mAttachments [aIndex];
}

AbstractItem* ControllerItem::childById (const QUuid &aId)
{
    for (int i = 0; i < childCount(); ++ i)
        if (mAttachments [i]->id() == aId)
            return mAttachments [i];
    return 0;
}

int ControllerItem::posOfChild (AbstractItem *aItem) const
{
    return mAttachments.indexOf (aItem);
}

int ControllerItem::childCount() const
{
    return mAttachments.size();
}

QString ControllerItem::text() const
{
    return ctrName();
}

QString ControllerItem::tip() const
{
    return QString();
}

QPixmap ControllerItem::pixmap()
{
    return PixmapPool::pool()->pixmap (mCtrType->pixmap());
}

KStorageBus ControllerItem::ctrBusType() const
{
    return mCtrType->busType();
}

QString ControllerItem::ctrName() const
{
    return mCtrName;
}

KStorageControllerType ControllerItem::ctrType() const
{
    return mCtrType->ctrType();
}

ControllerTypeList ControllerItem::ctrTypes() const
{
    return mCtrType->ctrTypes();
}

void ControllerItem::setCtrName (const QString &aCtrName)
{
    mCtrName = aCtrName;
}

void ControllerItem::setCtrType (KStorageControllerType aCtrType)
{
    mCtrType->setCtrType (aCtrType);
}

SlotsList ControllerItem::ctrAllSlots() const
{
    SlotsList allSlots;
    CSystemProperties sp = vboxGlobal().virtualBox().GetSystemProperties();
    for (ULONG i = 0; i < sp.GetMaxPortCountForStorageBus (mCtrType->busType()); ++ i)
        for (ULONG j = 0; j < sp.GetMaxDevicesPerPortForStorageBus (mCtrType->busType()); ++ j)
            allSlots << StorageSlot (mCtrType->busType(), i, j);
    return allSlots;
}

SlotsList ControllerItem::ctrUsedSlots() const
{
    SlotsList usedSlots;
    for (int i = 0; i < mAttachments.size(); ++ i)
        usedSlots << static_cast <AttachmentItem*> (mAttachments [i])->attSlot();
    return usedSlots;
}

DeviceTypeList ControllerItem::ctrDeviceTypeList() const
{
     return mCtrType->deviceTypeList();
}

QStringList ControllerItem::ctrAllMediumIds (bool aShowDiffs) const
{
    QStringList allImages;
    for (VBoxMediaList::const_iterator it = vboxGlobal().currentMediaList().begin();
         it != vboxGlobal().currentMediaList().end(); ++ it)
    {
         foreach (KDeviceType deviceType, mCtrType->deviceTypeList())
         {
             if ((*it).isNull() || typeToGlobal ((*it).type()) == deviceType)
             {
                 /* We should filter out the base hard-disk of diff-disks,
                  * as we want to insert here a diff-disks and want
                  * to avoid duplicates in !aShowDiffs mode. */
                 if (!aShowDiffs && (*it).parent() && !parent()->machineId().isNull() &&
                     (*it).isAttachedInCurStateTo (parent()->machineId()))
                     allImages.removeAll ((*it).root().id());
                 allImages << (*it).id();
                 break;
             }
         }
    }
    return allImages;
}

QStringList ControllerItem::ctrUsedMediumIds() const
{
    QStringList usedImages;
    for (int i = 0; i < mAttachments.size(); ++ i)
        usedImages << static_cast <AttachmentItem*> (mAttachments [i])->attMediumId();
    return usedImages;
}

void ControllerItem::addChild (AbstractItem *aItem)
{
    mAttachments << aItem;
}

void ControllerItem::delChild (AbstractItem *aItem)
{
    mAttachments.removeAll (aItem);
}

/* Attachment Item */
AttachmentItem::AttachmentItem (AbstractItem *aParent, KDeviceType aDeviceType)
    : AbstractItem (aParent)
    , mAttDeviceType (aDeviceType)
    , mAttIsShowDiffs (false)
    , mAttIsHostDrive (false)
    , mAttIsPassthrough (false)
{
    /* Check for proper parent type */
    AssertMsg (mParent->rtti() == AbstractItem::Type_ControllerItem, ("Incorrect parent type!\n"));

    /* Select default slot */
    AssertMsg (!attSlots().isEmpty(), ("There should be at least one available slot!\n"));
    mAttSlot = attSlots() [0];

    /* Try to select unique medium */
    QStringList freeMediumIds (attMediumIds());
    switch (mAttDeviceType)
    {
        case KDeviceType_HardDisk:
            if (freeMediumIds.size() > 0)
                setAttMediumId (freeMediumIds [0]);
            break;
        case KDeviceType_DVD:
        case KDeviceType_Floppy:
            if (freeMediumIds.size() > 1)
                setAttMediumId (freeMediumIds [1]);
            else if (freeMediumIds.size() > 0)
                setAttMediumId (freeMediumIds [0]);
            break;
        default:
            break;
    }
}

AbstractItem::ItemType AttachmentItem::rtti() const
{
    return Type_AttachmentItem;
}

AbstractItem* AttachmentItem::childByPos (int /* aIndex */)
{
    return 0;
}

AbstractItem* AttachmentItem::childById (const QUuid& /* aId */)
{
    return 0;
}

int AttachmentItem::posOfChild (AbstractItem* /* aItem */) const
{
    return 0;
}

int AttachmentItem::childCount() const
{
    return 0;
}

QString AttachmentItem::text() const
{
    return mAttName;
}

QString AttachmentItem::tip() const
{
    return mAttTip;
}

QPixmap AttachmentItem::pixmap()
{
    if (mAttPixmap.isNull())
    {
        switch (mAttDeviceType)
        {
            case KDeviceType_HardDisk:
                mAttPixmap = PixmapPool::pool()->pixmap (PixmapPool::HDAttachmentEn);
                break;
            case KDeviceType_DVD:
                mAttPixmap = PixmapPool::pool()->pixmap (PixmapPool::CDAttachmentEn);
                break;
            case KDeviceType_Floppy:
                mAttPixmap = PixmapPool::pool()->pixmap (PixmapPool::FDAttachmentEn);
                break;
            default:
                break;
        }
    }
    return mAttPixmap;
}

StorageSlot AttachmentItem::attSlot() const
{
    return mAttSlot;
}

SlotsList AttachmentItem::attSlots() const
{
    ControllerItem *ctr = static_cast <ControllerItem*> (mParent);

    /* Filter list from used slots */
    SlotsList allSlots (ctr->ctrAllSlots());
    SlotsList usedSlots (ctr->ctrUsedSlots());
    foreach (StorageSlot usedSlot, usedSlots)
        if (usedSlot != mAttSlot)
            allSlots.removeAll (usedSlot);

    return allSlots;
}

KDeviceType AttachmentItem::attDeviceType() const
{
    return mAttDeviceType;
}

DeviceTypeList AttachmentItem::attDeviceTypes() const
{
    return static_cast <ControllerItem*> (mParent)->ctrDeviceTypeList();
}

QString AttachmentItem::attMediumId() const
{
    return mAttMediumId;
}

QStringList AttachmentItem::attMediumIds (bool aFilter) const
{
    ControllerItem *ctr = static_cast <ControllerItem*> (mParent);
    QStringList allMediumIds;

    /* Populate list of suitable medium ids */
    foreach (QString mediumId, ctr->ctrAllMediumIds (mAttIsShowDiffs))
    {
        VBoxMedium medium = vboxGlobal().findMedium (mediumId);
        if ((medium.isNull() && mAttDeviceType != KDeviceType_HardDisk) ||
            (!medium.isNull() && typeToGlobal (medium.type()) == mAttDeviceType))
            allMediumIds << mediumId;
    }

    if (aFilter)
    {
        /* Filter list from used medium ids */
        QStringList usedMediumIds (ctr->ctrUsedMediumIds());
        foreach (QString usedMediumId, usedMediumIds)
            if (usedMediumId != mAttMediumId)
                allMediumIds.removeAll (usedMediumId);
    }

    return allMediumIds;
}

bool AttachmentItem::attIsShowDiffs() const
{
    return mAttIsShowDiffs;
}

bool AttachmentItem::attIsHostDrive() const
{
    return mAttIsHostDrive;
}

bool AttachmentItem::attIsPassthrough() const
{
    return mAttIsPassthrough;
}

void AttachmentItem::setAttSlot (const StorageSlot &aAttSlot)
{
    mAttSlot = aAttSlot;
}

void AttachmentItem::setAttDevice (KDeviceType aAttDeviceType)
{
    mAttDeviceType = aAttDeviceType;
}

void AttachmentItem::setAttMediumId (const QString &aAttMediumId)
{
    VBoxMedium medium;

    /* Caching first available medium */
    if (aAttMediumId == firstAvailableId && !attMediumIds (false).isEmpty())
        medium = vboxGlobal().findMedium (attMediumIds (false) [0]);
    /* Caching passed medium */
    else if (!aAttMediumId.isEmpty())
        medium = vboxGlobal().findMedium (aAttMediumId);

    mAttMediumId = medium.id();
    cache();
}

void AttachmentItem::setAttIsShowDiffs (bool aAttIsShowDiffs)
{
    mAttIsShowDiffs = aAttIsShowDiffs;
    cache();
}

void AttachmentItem::setAttIsPassthrough (bool aIsAttPassthrough)
{
    mAttIsPassthrough = aIsAttPassthrough;
}

QString AttachmentItem::attSize() const
{
    return mAttSize;
}

QString AttachmentItem::attLogicalSize() const
{
    return mAttLogicalSize;
}

QString AttachmentItem::attLocation() const
{
    return mAttLocation;
}

QString AttachmentItem::attFormat() const
{
    return mAttFormat;
}

QString AttachmentItem::attUsage() const
{
    return mAttUsage;
}

void AttachmentItem::cache()
{
    VBoxMedium medium = vboxGlobal().findMedium (mAttMediumId);

    /* Cache medium information */
    mAttName = medium.name (!mAttIsShowDiffs);
    mAttTip = medium.toolTipCheckRO (!mAttIsShowDiffs);
    mAttPixmap = medium.iconCheckRO (!mAttIsShowDiffs);
    mAttIsHostDrive = medium.isHostDrive();

    /* Cache additional information */
    mAttSize = medium.size (!mAttIsShowDiffs);
    mAttLogicalSize = medium.logicalSize (!mAttIsShowDiffs);
    mAttLocation = medium.location (!mAttIsShowDiffs);
    mAttFormat = medium.isNull() ? QString ("--") :
        QString ("%1 (%2)").arg (medium.hardDiskType (!mAttIsShowDiffs))
                           .arg (medium.hardDiskFormat (!mAttIsShowDiffs));
    mAttUsage = medium.usage (!mAttIsShowDiffs);

    /* Fill empty attributes */
    if (mAttUsage.isEmpty())
        mAttUsage = QString ("--");
}

void AttachmentItem::addChild (AbstractItem* /* aItem */)
{
}

void AttachmentItem::delChild (AbstractItem* /* aItem */)
{
}

/* Storage model */
StorageModel::StorageModel (QObject *aParent)
    : QAbstractItemModel (aParent)
    , mRootItem (new RootItem)
{
}

StorageModel::~StorageModel()
{
    delete mRootItem;
}

int StorageModel::rowCount (const QModelIndex &aParent) const
{
    return !aParent.isValid() ? 1 /* only root item has invalid parent */ :
           static_cast <AbstractItem*> (aParent.internalPointer())->childCount();
}

int StorageModel::columnCount (const QModelIndex &aParent) const
{
    return 1;
}

QModelIndex StorageModel::root() const
{
    return index (0, 0);
}

QModelIndex StorageModel::index (int aRow, int aColumn, const QModelIndex &aParent) const
{
    if (!hasIndex (aRow, aColumn, aParent))
        return QModelIndex();

    AbstractItem *item = !aParent.isValid() ? mRootItem :
                         static_cast <AbstractItem*> (aParent.internalPointer())->childByPos (aRow);

    return item ? createIndex (aRow, aColumn, item) : QModelIndex();
}

QModelIndex StorageModel::parent (const QModelIndex &aIndex) const
{
    if (!aIndex.isValid())
        return QModelIndex();

    AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer());
    AbstractItem *parentOfItem = item->parent();
    AbstractItem *parentOfParent = parentOfItem ? parentOfItem->parent() : 0;
    int position = parentOfParent ? parentOfParent->posOfChild (parentOfItem) : 0;

    if (parentOfItem)
        return createIndex (position, 0, parentOfItem);
    else
        return QModelIndex();
}

QVariant StorageModel::data (const QModelIndex &aIndex, int aRole) const
{
    if (!aIndex.isValid())
        return QVariant();

    switch (aRole)
    {
        /* Basic Attributes: */
        case Qt::FontRole:
        {
            return QVariant (qApp->font());
        }
        case Qt::SizeHintRole:
        {
            QFontMetrics fm (data (aIndex, Qt::FontRole).value <QFont>());
            int minimumHeight = qMax (fm.height(), data (aIndex, R_IconSize).toInt());
            int margin = data (aIndex, R_Margin).toInt();
            return QSize (1 /* ignoring width */, 2 * margin + minimumHeight);
        }
        case Qt::ToolTipRole:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                return item->tip();
            return QString();
        }

        /* Advanced Attributes: */
        case R_ItemId:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                return item->id().toString();
            return QUuid().toString();
        }
        case R_ItemPixmap:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                return item->pixmap();
            return QPixmap();
        }
        case R_ItemPixmapRect:
        {
            int margin = data (aIndex, R_Margin).toInt();
            int width = data (aIndex, R_IconSize).toInt();
            return QRect (margin, margin, width, width);
        }
        case R_ItemName:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                return item->text();
            return QString();
        }
        case R_ItemNamePoint:
        {
            int margin = data (aIndex, R_Margin).toInt();
            int spacing = data (aIndex, R_Spacing).toInt();
            int width = data (aIndex, R_IconSize).toInt();
            QFontMetrics fm (data (aIndex, Qt::FontRole).value <QFont>());
            QSize sizeHint = data (aIndex, Qt::SizeHintRole).toSize();
            return QPoint (margin + width + 2 * spacing,
                           sizeHint.height() / 2 + fm.ascent() / 2 - 1 /* base line */);
        }
        case R_ItemType:
        {
            QVariant result (QVariant::fromValue (AbstractItem::Type_InvalidItem));
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                result.setValue (item->rtti());
            return result;
        }
        case R_IsController:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                return item->rtti() == AbstractItem::Type_ControllerItem;
            return false;
        }
        case R_IsAttachment:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                return item->rtti() == AbstractItem::Type_AttachmentItem;
            return false;
        }

        case R_IsMoreControllersPossible:
        {
            return rowCount (root()) < 16;
        }
        case R_IsMoreAttachmentsPossible:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
            {
                if (item->rtti() == AbstractItem::Type_ControllerItem)
                {
                    ControllerItem *ctr = static_cast <ControllerItem*> (item);
                    CSystemProperties sp = vboxGlobal().virtualBox().GetSystemProperties();
                    return (uint) rowCount (aIndex) < sp.GetMaxPortCountForStorageBus (ctr->ctrBusType()) *
                                                      sp.GetMaxDevicesPerPortForStorageBus (ctr->ctrBusType());
                }
            }
            return false;
        }

        case R_CtrName:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_ControllerItem)
                    return static_cast <ControllerItem*> (item)->ctrName();
            return QString();
        }
        case R_CtrType:
        {
            QVariant result (QVariant::fromValue (KStorageControllerType_Null));
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_ControllerItem)
                    result.setValue (static_cast <ControllerItem*> (item)->ctrType());
            return result;
        }
        case R_CtrTypes:
        {
            QVariant result (QVariant::fromValue (ControllerTypeList()));
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_ControllerItem)
                    result.setValue (static_cast <ControllerItem*> (item)->ctrTypes());
            return result;
        }
        case R_CtrDevices:
        {
            QVariant result (QVariant::fromValue (DeviceTypeList()));
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_ControllerItem)
                    result.setValue (static_cast <ControllerItem*> (item)->ctrDeviceTypeList());
            return result;
        }
        case R_CtrBusType:
        {
            QVariant result (QVariant::fromValue (KStorageBus_Null));
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_ControllerItem)
                    result.setValue (static_cast <ControllerItem*> (item)->ctrBusType());
            return result;
        }

        case R_AttSlot:
        {
            QVariant result (QVariant::fromValue (StorageSlot()));
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                    result.setValue (static_cast <AttachmentItem*> (item)->attSlot());
            return result;
        }
        case R_AttSlots:
        {
            QVariant result (QVariant::fromValue (SlotsList()));
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                    result.setValue (static_cast <AttachmentItem*> (item)->attSlots());
            return result;
        }
        case R_AttDevice:
        {
            QVariant result (QVariant::fromValue (KDeviceType_Null));
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                    result.setValue (static_cast <AttachmentItem*> (item)->attDeviceType());
            return result;
        }
        case R_AttDevices:
        {
            QVariant result (QVariant::fromValue (DeviceTypeList()));
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                    result.setValue (static_cast <AttachmentItem*> (item)->attDeviceTypes());
            return result;
        }
        case R_AttMediumId:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                    return static_cast <AttachmentItem*> (item)->attMediumId();
            return QString();
        }
        case R_AttIsShowDiffs:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                    return static_cast <AttachmentItem*> (item)->attIsShowDiffs();
            return false;
        }
        case R_AttIsHostDrive:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                    return static_cast <AttachmentItem*> (item)->attIsHostDrive();
            return false;
        }
        case R_AttIsPassthrough:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                    return static_cast <AttachmentItem*> (item)->attIsPassthrough();
            return false;
        }
        case R_AttSize:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                    return static_cast <AttachmentItem*> (item)->attSize();
            return QString();
        }
        case R_AttLogicalSize:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                    return static_cast <AttachmentItem*> (item)->attLogicalSize();
            return QString();
        }
        case R_AttLocation:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                    return static_cast <AttachmentItem*> (item)->attLocation();
            return QString();
        }
        case R_AttFormat:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                    return static_cast <AttachmentItem*> (item)->attFormat();
            return QString();
        }
        case R_AttUsage:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                    return static_cast <AttachmentItem*> (item)->attUsage();
            return QString();
        }
        case R_Margin:
        {
            return 4;
        }
        case R_Spacing:
        {
            return 4;
        }
        case R_IconSize:
        {
            return 16;
        }

        case R_HDPixmapEn:
        {
            return PixmapPool::pool()->pixmap (PixmapPool::HDAttachmentEn);
        }
        case R_HDPixmapDis:
        {
            return PixmapPool::pool()->pixmap (PixmapPool::HDAttachmentDis);
        }
        case R_CDPixmapEn:
        {
            return PixmapPool::pool()->pixmap (PixmapPool::CDAttachmentEn);
        }
        case R_CDPixmapDis:
        {
            return PixmapPool::pool()->pixmap (PixmapPool::CDAttachmentDis);
        }
        case R_FDPixmapEn:
        {
            return PixmapPool::pool()->pixmap (PixmapPool::FDAttachmentEn);
        }
        case R_FDPixmapDis:
        {
            return PixmapPool::pool()->pixmap (PixmapPool::FDAttachmentDis);
        }
        case R_HDPixmapRect:
        {
            int margin = data (aIndex, R_Margin).toInt();
            int spacing = data (aIndex, R_Spacing).toInt();
            int width = data (aIndex, R_IconSize).toInt();
            return QRect (0 - width - spacing - width - margin, margin, width, width);
        }
        case R_CDPixmapRect:
        {
            int margin = data (aIndex, R_Margin).toInt();
            int width = data (aIndex, R_IconSize).toInt();
            return QRect (0 - width - margin, margin, width, width);
        }
        case R_FDPixmapRect:
        {
            int margin = data (aIndex, R_Margin).toInt();
            int width = data (aIndex, R_IconSize).toInt();
            return QRect (0 - width - margin, margin, width, width);
        }

        case R_PlusPixmapEn:
        {
            return PixmapPool::pool()->pixmap (PixmapPool::PlusEn);
        }
        case R_PlusPixmapDis:
        {
            return PixmapPool::pool()->pixmap (PixmapPool::PlusDis);
        }
        case R_MinusPixmapEn:
        {
            return PixmapPool::pool()->pixmap (PixmapPool::MinusEn);
        }
        case R_MinusPixmapDis:
        {
            return PixmapPool::pool()->pixmap (PixmapPool::MinusDis);
        }
        case R_AdderPoint:
        {
            int margin = data (aIndex, R_Margin).toInt();
            return QPoint (margin + 6, margin + 6);
        }
        default:
            break;
    }
    return QVariant();
}

bool StorageModel::setData (const QModelIndex &aIndex, const QVariant &aValue, int aRole)
{
    if (!aIndex.isValid())
        return QAbstractItemModel::setData (aIndex, aValue, aRole);

    switch (aRole)
    {
        case R_CtrName:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_ControllerItem)
                {
                    static_cast <ControllerItem*> (item)->setCtrName (aValue.toString());
                    emit dataChanged (aIndex, aIndex);
                    return true;
                }
            return false;
        }
        case R_CtrType:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_ControllerItem)
                {
                    static_cast <ControllerItem*> (item)->setCtrType (aValue.value <KStorageControllerType>());
                    emit dataChanged (aIndex, aIndex);
                    return true;
                }
            return false;
        }
        case R_AttSlot:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                {
                    static_cast <AttachmentItem*> (item)->setAttSlot (aValue.value <StorageSlot>());
                    emit dataChanged (aIndex, aIndex);
                    return true;
                }
            return false;
        }
        case R_AttDevice:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                {
                    static_cast <AttachmentItem*> (item)->setAttDevice (aValue.value <KDeviceType>());
                    emit dataChanged (aIndex, aIndex);
                    return true;
                }
            return false;
        }
        case R_AttMediumId:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                {
                    static_cast <AttachmentItem*> (item)->setAttMediumId (aValue.toString());
                    emit dataChanged (aIndex, aIndex);
                    return true;
                }
            return false;
        }
        case R_AttIsShowDiffs:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                {
                    static_cast <AttachmentItem*> (item)->setAttIsShowDiffs (aValue.toBool());
                    emit dataChanged (aIndex, aIndex);
                    return true;
                }
            return false;
        }
        case R_AttIsPassthrough:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                {
                    static_cast <AttachmentItem*> (item)->setAttIsPassthrough (aValue.toBool());
                    emit dataChanged (aIndex, aIndex);
                    return true;
                }
            return false;
        }
        default:
            break;
    }

    return false;
}

QModelIndex StorageModel::addController (const QString &aCtrName, KStorageBus aBusType, KStorageControllerType aCtrType)
{
    beginInsertRows (root(), mRootItem->childCount(), mRootItem->childCount());
    new ControllerItem (mRootItem, aCtrName, aBusType, aCtrType);
    endInsertRows();
    return index (mRootItem->childCount() - 1, 0, root());
}

void StorageModel::delController (const QUuid &aCtrId)
{
    if (AbstractItem *item = mRootItem->childById (aCtrId))
    {
        int itemPosition = mRootItem->posOfChild (item);
        beginRemoveRows (root(), itemPosition, itemPosition);
        delete item;
        endRemoveRows();
    }
}

QModelIndex StorageModel::addAttachment (const QUuid &aCtrId, KDeviceType aDeviceType)
{
    if (AbstractItem *parent = mRootItem->childById (aCtrId))
    {
        int parentPosition = mRootItem->posOfChild (parent);
        QModelIndex parentIndex = index (parentPosition, 0, root());
        beginInsertRows (parentIndex, parent->childCount(), parent->childCount());
        new AttachmentItem (parent, aDeviceType);
        endInsertRows();
        return index (parent->childCount() - 1, 0, parentIndex);
    }
    return QModelIndex();
}

void StorageModel::delAttachment (const QUuid &aCtrId, const QUuid &aAttId)
{
    if (AbstractItem *parent = mRootItem->childById (aCtrId))
    {
        int parentPosition = mRootItem->posOfChild (parent);
        if (AbstractItem *item = parent->childById (aAttId))
        {
            int itemPosition = parent->posOfChild (item);
            beginRemoveRows (index (parentPosition, 0, root()), itemPosition, itemPosition);
            delete item;
            endRemoveRows();
        }
    }
}

void StorageModel::setMachineId (const QString &aMachineId)
{
    mRootItem->setMachineId (aMachineId);
}

Qt::ItemFlags StorageModel::flags (const QModelIndex &aIndex) const
{
    return !aIndex.isValid() ? QAbstractItemModel::flags (aIndex) :
           Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

/* Storage Delegate */
StorageDelegate::StorageDelegate (QObject *aParent)
    : QItemDelegate (aParent)
{
}

void StorageDelegate::paint (QPainter *aPainter, const QStyleOptionViewItem &aOption, const QModelIndex &aIndex) const
{
    if (!aIndex.isValid()) return;

    /* Initialize variables */
    QStyle::State state = aOption.state;
    QRect rect = aOption.rect;
    const StorageModel *model = qobject_cast <const StorageModel*> (aIndex.model());
    Assert (model);

    aPainter->save();

    /* Draw selection backgroung */
    if (state & QStyle::State_Selected)
    {
        QPalette::ColorGroup cg = (state & QStyle::State_Enabled && state & QStyle::State_Active) ? QPalette::Normal :
                                  (state & QStyle::State_Enabled) ? QPalette::Inactive : QPalette::Disabled;
        aPainter->fillRect (rect, aOption.palette.brush (cg, QPalette::Highlight));
    }

    aPainter->translate (rect.x(), rect.y());

    /* Draw Item Pixmap */
    aPainter->drawPixmap (model->data (aIndex, StorageModel::R_ItemPixmapRect).toRect().topLeft(),
                          model->data (aIndex, StorageModel::R_ItemPixmap).value <QPixmap>());

    /* Draw expand/collapse Pixmap */
    if (model->hasChildren (aIndex))
    {
        QPixmap expander = state & QStyle::State_Open ?
                           model->data (aIndex, StorageModel::R_MinusPixmapEn).value <QPixmap>() :
                           model->data (aIndex, StorageModel::R_PlusPixmapEn).value <QPixmap>();
        aPainter->drawPixmap (model->data (aIndex, StorageModel::R_AdderPoint).toPoint(), expander);
    }

    /* Draw compressed item name */
    int margin = model->data (aIndex, StorageModel::R_Margin).toInt();
    int iconWidth = model->data (aIndex, StorageModel::R_IconSize).toInt();
    int spacing = model->data (aIndex, StorageModel::R_Spacing).toInt();
    QPoint textPosition = model->data (aIndex, StorageModel::R_ItemNamePoint).toPoint();
    int textWidth = rect.width() - textPosition.x();
    if (model->data (aIndex, StorageModel::R_IsController).toBool() && state & QStyle::State_Selected)
    {
        textWidth -= (2 * spacing + iconWidth + margin);
        if (model->data (aIndex, StorageModel::R_CtrBusType).value <KStorageBus>() != KStorageBus_Floppy)
            textWidth -= (spacing + iconWidth);
    }
    QString text (model->data (aIndex, StorageModel::R_ItemName).toString());
    QString shortText (text);
    QFont font = model->data (aIndex, Qt::FontRole).value <QFont>();
    QFontMetrics fm (font);
    while ((shortText.size() > 1) && (fm.width (shortText) + fm.width ("...") > textWidth))
        shortText.truncate (shortText.size() - 1);
    if (shortText != text)
        shortText += "...";
    aPainter->setFont (font);
    aPainter->drawText (textPosition, shortText);

    /* Draw Controller Additions */
    if (model->data (aIndex, StorageModel::R_IsController).toBool() && state & QStyle::State_Selected)
    {
        DeviceTypeList devicesList (model->data (aIndex, StorageModel::R_CtrDevices).value <DeviceTypeList>());
        for (int i = 0; i < devicesList.size(); ++ i)
        {
            KDeviceType deviceType = devicesList [i];

            QRect deviceRect;
            QPixmap devicePixmap;
            switch (deviceType)
            {
                case KDeviceType_HardDisk:
                {
                    deviceRect = model->data (aIndex, StorageModel::R_HDPixmapRect).value <QRect>();
                    devicePixmap = model->data (aIndex, StorageModel::R_IsMoreAttachmentsPossible).toBool() ?
                                   model->data (aIndex, StorageModel::R_HDPixmapEn).value <QPixmap>() :
                                   model->data (aIndex, StorageModel::R_HDPixmapDis).value <QPixmap>();
                    break;
                }
                case KDeviceType_DVD:
                {
                    deviceRect = model->data (aIndex, StorageModel::R_CDPixmapRect).value <QRect>();
                    devicePixmap = model->data (aIndex, StorageModel::R_IsMoreAttachmentsPossible).toBool() ?
                                   model->data (aIndex, StorageModel::R_CDPixmapEn).value <QPixmap>() :
                                   model->data (aIndex, StorageModel::R_CDPixmapDis).value <QPixmap>();
                    break;
                }
                case KDeviceType_Floppy:
                {
                    deviceRect = model->data (aIndex, StorageModel::R_FDPixmapRect).value <QRect>();
                    devicePixmap = model->data (aIndex, StorageModel::R_IsMoreAttachmentsPossible).toBool() ?
                                   model->data (aIndex, StorageModel::R_FDPixmapEn).value <QPixmap>() :
                                   model->data (aIndex, StorageModel::R_FDPixmapDis).value <QPixmap>();
                    break;
                }
                default:
                    break;
            }
            QPixmap adderPixmap = model->data (aIndex, StorageModel::R_IsMoreAttachmentsPossible).toBool() ?
                                  model->data (aIndex, StorageModel::R_PlusPixmapEn).value <QPixmap>() :
                                  model->data (aIndex, StorageModel::R_PlusPixmapDis).value <QPixmap>();

            aPainter->drawPixmap (QPoint (rect.width() + deviceRect.x(), deviceRect.y()), devicePixmap);
            aPainter->drawPixmap (QPoint (rect.width() + deviceRect.x() + 6, deviceRect.y() + 6), adderPixmap);
        }
    }

    aPainter->restore();

    drawFocus (aPainter, aOption, rect);
}

/**
 * QWidget class reimplementation.
 * Used as HD Settings widget.
 */
VBoxVMSettingsHD::VBoxVMSettingsHD()
    : mValidator (0)
    , mIsPolished (false)
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsHD::setupUi (this);

    /* Enumerate Mediums */
    vboxGlobal().startEnumeratingMedia();

    /* Initialize pixmap pool */
    PixmapPool::pool (this);

    /* Controller Actions */
    mAddCtrAction = new QAction (this);
    mAddCtrAction->setIcon (VBoxGlobal::iconSet (PixmapPool::pool()->pixmap (PixmapPool::AddControllerEn),
                                                 PixmapPool::pool()->pixmap (PixmapPool::AddControllerDis)));

    mAddIDECtrAction = new QAction (this);
    mAddIDECtrAction->setIcon (VBoxGlobal::iconSet (PixmapPool::pool()->pixmap (PixmapPool::IDEController)));

    mAddSATACtrAction = new QAction (this);
    mAddSATACtrAction->setIcon (VBoxGlobal::iconSet (PixmapPool::pool()->pixmap (PixmapPool::SATAController)));

    mAddSCSICtrAction = new QAction (this);
    mAddSCSICtrAction->setIcon (VBoxGlobal::iconSet (PixmapPool::pool()->pixmap (PixmapPool::SCSIController)));

    mAddFloppyCtrAction = new QAction (this);
    mAddFloppyCtrAction->setIcon (VBoxGlobal::iconSet (PixmapPool::pool()->pixmap (PixmapPool::FloppyController)));

    mDelCtrAction = new QAction (this);
    mDelCtrAction->setIcon (VBoxGlobal::iconSet (PixmapPool::pool()->pixmap (PixmapPool::DelControllerEn),
                                                 PixmapPool::pool()->pixmap (PixmapPool::DelControllerDis)));

    /* Attachment Actions */
    mAddAttAction = new QAction (this);
    mAddAttAction->setIcon (VBoxGlobal::iconSet (PixmapPool::pool()->pixmap (PixmapPool::AddAttachmentEn),
                                                 PixmapPool::pool()->pixmap (PixmapPool::AddAttachmentDis)));

    mDelAttAction = new QAction (this);
    mDelAttAction->setIcon (VBoxGlobal::iconSet (PixmapPool::pool()->pixmap (PixmapPool::DelAttachmentEn),
                                                 PixmapPool::pool()->pixmap (PixmapPool::DelAttachmentDis)));

    /* Storage Model/View */
    mStorageModel = new StorageModel (mTwStorageTree);
    StorageDelegate *storageDelegate = new StorageDelegate (mTwStorageTree);
    mTwStorageTree->setContextMenuPolicy (Qt::CustomContextMenu);
    mTwStorageTree->setModel (mStorageModel);
    mTwStorageTree->setItemDelegate (storageDelegate);
    mTwStorageTree->setRootIndex (mStorageModel->root());
    mTwStorageTree->setCurrentIndex (mStorageModel->root());

    /* Storage ToolBar */
    mTbStorageBar->setIconSize (QSize (16, 16));
    mTbStorageBar->addAction (mAddAttAction);
    mTbStorageBar->addAction (mDelAttAction);
    mTbStorageBar->addAction (mAddCtrAction);
    mTbStorageBar->addAction (mDelCtrAction);

#ifdef Q_WS_MAC
    /* We need a little more space for the focus rect. */
    mLtStorage->setContentsMargins (3, 0, 3, 0);
    mLtStorage->setSpacing (3);
#endif /* Q_WS_MAC */

    /* Vdi Combo */
    mCbVdi->setNullItemPresent (true);
    mCbVdi->refresh();

    /* Vmm Button */
    mTbVmm->setIcon (VBoxGlobal::iconSet (PixmapPool::pool()->pixmap (PixmapPool::VMMEn),
                                          PixmapPool::pool()->pixmap (PixmapPool::VMMDis)));

    /* Info Pane initialization */
    mLbHDVirtualSizeValue->setFullSizeSelection (true);
    mLbHDActualSizeValue->setFullSizeSelection (true);
    mLbSizeValue->setFullSizeSelection (true);
    mLbLocationValue->setFullSizeSelection (true);
    mLbHDFormatValue->setFullSizeSelection (true);
    mLbUsageValue->setFullSizeSelection (true);

    /* Setup connections */
    connect (&vboxGlobal(), SIGNAL (mediumEnumerated (const VBoxMedium &)),
             this, SLOT (mediumUpdated (const VBoxMedium &)));
    connect (&vboxGlobal(), SIGNAL (mediumUpdated (const VBoxMedium &)),
             this, SLOT (mediumUpdated (const VBoxMedium &)));
    connect (&vboxGlobal(), SIGNAL (mediumRemoved (VBoxDefs::MediumType, const QString &)),
             this, SLOT (mediumRemoved (VBoxDefs::MediumType, const QString &)));
    connect (mAddCtrAction, SIGNAL (triggered (bool)), this, SLOT (addController()));
    connect (mAddIDECtrAction, SIGNAL (triggered (bool)), this, SLOT (addIDEController()));
    connect (mAddSATACtrAction, SIGNAL (triggered (bool)), this, SLOT (addSATAController()));
    connect (mAddSCSICtrAction, SIGNAL (triggered (bool)), this, SLOT (addSCSIController()));
    connect (mAddFloppyCtrAction, SIGNAL (triggered (bool)), this, SLOT (addFloppyController()));
    connect (mDelCtrAction, SIGNAL (triggered (bool)), this, SLOT (delController()));
    connect (mAddAttAction, SIGNAL (triggered (bool)), this, SLOT (addAttachment()));
    connect (mDelAttAction, SIGNAL (triggered (bool)), this, SLOT (delAttachment()));
    connect (mStorageModel, SIGNAL (rowsInserted (const QModelIndex&, int, int)),
             this, SLOT (onRowInserted (const QModelIndex&, int)));
    connect (mStorageModel, SIGNAL (rowsRemoved (const QModelIndex&, int, int)),
             this, SLOT (onRowRemoved()));
    connect (mTwStorageTree, SIGNAL (currentItemChanged (const QModelIndex&, const QModelIndex&)),
             this, SLOT (onCurrentItemChanged()));
    connect (mTwStorageTree, SIGNAL (customContextMenuRequested (const QPoint&)),
             this, SLOT (onContextMenuRequested (const QPoint&)));
    connect (mTwStorageTree, SIGNAL (drawItemBranches (QPainter*, const QRect&, const QModelIndex&)),
             this, SLOT (onDrawItemBranches (QPainter *, const QRect &, const QModelIndex &)));
    connect (mTwStorageTree, SIGNAL (mousePressed (QMouseEvent*)),
             this, SLOT (onMouseClicked (QMouseEvent*)));
    connect (mTwStorageTree, SIGNAL (mouseDoubleClicked (QMouseEvent*)),
             this, SLOT (onMouseClicked (QMouseEvent*)));
    connect (mLeName, SIGNAL (textEdited (const QString&)), this, SLOT (setInformation()));
    connect (mCbType, SIGNAL (activated (int)), this, SLOT (setInformation()));
    connect (mCbSlot, SIGNAL (activated (int)), this, SLOT (setInformation()));
    connect (mCbDevice, SIGNAL (activated (int)), this, SLOT (setInformation()));
    connect (mCbVdi, SIGNAL (activated (int)), this, SLOT (setInformation()));
    connect (mTbVmm, SIGNAL (clicked (bool)), this, SLOT (onVmmInvoked()));
    connect (mCbShowDiffs, SIGNAL (stateChanged (int)), this, SLOT (setInformation()));
    connect (mCbPassthrough, SIGNAL (stateChanged (int)), this, SLOT (setInformation()));

    /* Update actions */
    updateActionsState();

    /* Applying language settings */
    retranslateUi();

    /* Initial setup */
    setMinimumWidth (500);
    mSplitter->setSizes (QList<int>() << 0.45 * minimumWidth() << 0.55 * minimumWidth());
}

void VBoxVMSettingsHD::getFrom (const CMachine &aMachine)
{
    mMachine = aMachine;

    /* Set the machine id for the media-combo */
    mCbVdi->setMachineId (mMachine.GetId());
    mStorageModel->setMachineId (mMachine.GetId());

    /* Load currently present controllers & attachments */
    CStorageControllerVector controllers = mMachine.GetStorageControllers();
    foreach (const CStorageController &controller, controllers)
    {
        QString controllerName = controller.GetName();
        QModelIndex ctrIndex = mStorageModel->addController (controllerName, controller.GetBus(), controller.GetControllerType());
        QUuid ctrId = QUuid (mStorageModel->data (ctrIndex, StorageModel::R_ItemId).toString());

        CMediumAttachmentVector attachments = mMachine.GetMediumAttachmentsOfController (controllerName);
        foreach (const CMediumAttachment &attachment, attachments)
        {
            QModelIndex attIndex = mStorageModel->addAttachment (ctrId, attachment.GetType());
            mStorageModel->setData (attIndex, QVariant::fromValue (StorageSlot (controller.GetBus(), attachment.GetPort(), attachment.GetDevice())), StorageModel::R_AttSlot);
            CMedium medium (attachment.GetMedium());
            VBoxMedium vboxMedium;
            vboxGlobal().findMedium (medium, vboxMedium);
            mStorageModel->setData (attIndex, vboxMedium.id(), StorageModel::R_AttMediumId);
            mStorageModel->setData (attIndex, attachment.GetPassthrough(), StorageModel::R_AttIsPassthrough);
        }
    }
}

void VBoxVMSettingsHD::putBackTo()
{
    /* Remove currently present controllers & attachments */
    CStorageControllerVector controllers = mMachine.GetStorageControllers();
    foreach (const CStorageController &controller, controllers)
    {
        QString controllerName (controller.GetName());
        CMediumAttachmentVector attachments = mMachine.GetMediumAttachmentsOfController (controllerName);
        foreach (const CMediumAttachment &attachment, attachments)
            mMachine.DetachDevice (controllerName, attachment.GetPort(), attachment.GetDevice());
        mMachine.RemoveStorageController (controllerName);
    }

    /* Save created controllers & attachments */
    QModelIndex rootIndex = mStorageModel->root();
    for (int i = 0; i < mStorageModel->rowCount (rootIndex); ++ i)
    {
        QModelIndex ctrIndex = rootIndex.child (i, 0);
        QString ctrName = mStorageModel->data (ctrIndex, StorageModel::R_CtrName).toString();
        KStorageBus ctrBusType = mStorageModel->data (ctrIndex, StorageModel::R_CtrBusType).value <KStorageBus>();
        KStorageControllerType ctrType = mStorageModel->data (ctrIndex, StorageModel::R_CtrType).value <KStorageControllerType>();
        CStorageController ctr = mMachine.AddStorageController (ctrName, ctrBusType);
        ctr.SetControllerType (ctrType);
        for (int j = 0; j < mStorageModel->rowCount (ctrIndex); ++ j)
        {
            QModelIndex attIndex = ctrIndex.child (j, 0);
            StorageSlot attStorageSlot = mStorageModel->data (attIndex, StorageModel::R_AttSlot).value <StorageSlot>();
            KDeviceType attDeviceType = mStorageModel->data (attIndex, StorageModel::R_AttDevice).value <KDeviceType>();
            QString attMediumId = mStorageModel->data (attIndex, StorageModel::R_AttMediumId).toString();
            mMachine.AttachDevice (ctrName, attStorageSlot.port, attStorageSlot.device, attDeviceType, attMediumId);
            CMediumAttachment attachment = mMachine.GetMediumAttachment (ctrName, attStorageSlot.port, attStorageSlot.device);
            attachment.SetPassthrough (mStorageModel->data (attIndex, StorageModel::R_AttIsHostDrive).toBool() &&
                                       mStorageModel->data (attIndex, StorageModel::R_AttIsPassthrough).toBool());
        }
    }
}

void VBoxVMSettingsHD::setValidator (QIWidgetValidator *aVal)
{
    mValidator = aVal;
}

bool VBoxVMSettingsHD::revalidate (QString &aWarning, QString &)
{
    QModelIndex rootIndex = mStorageModel->root();
    QMap <QString, QString> config;
    for (int i = 0; i < mStorageModel->rowCount (rootIndex); ++ i)
    {
        QModelIndex ctrIndex = rootIndex.child (i, 0);
        QString ctrName = mStorageModel->data (ctrIndex, StorageModel::R_CtrName).toString();
        for (int j = 0; j < mStorageModel->rowCount (ctrIndex); ++ j)
        {
            QModelIndex attIndex = ctrIndex.child (j, 0);
            StorageSlot attSlot = mStorageModel->data (attIndex, StorageModel::R_AttSlot).value <StorageSlot>();
            KDeviceType attDevice = mStorageModel->data (attIndex, StorageModel::R_AttDevice).value <KDeviceType>();
            QString key (mStorageModel->data (attIndex, StorageModel::R_AttMediumId).toString());
            QString value (QString ("%1 (%2)").arg (ctrName, vboxGlobal().toFullString (attSlot)));
            /* Check for emptiness */
            if (vboxGlobal().findMedium (key).isNull() && attDevice == KDeviceType_HardDisk)
            {
                aWarning = tr ("No hard disk is selected for <i>%1</i>.").arg (value);
                return aWarning.isNull();
            }
            /* Check for coincidence */
            if (!vboxGlobal().findMedium (key).isNull() && config.contains (key))
            {
                aWarning = tr ("<i>%1</i> uses the medium that is already attached to <i>%2</i>.")
                              .arg (value).arg (config [key]);
                return aWarning.isNull();
            }
            else config.insert (key, value);
        }
    }
    return aWarning.isNull();
}

void VBoxVMSettingsHD::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsHD::retranslateUi (this);

    mAddCtrAction->setShortcut (QKeySequence ("Ins"));
    mDelCtrAction->setShortcut (QKeySequence ("Del"));
    mAddAttAction->setShortcut (QKeySequence ("+"));
    mDelAttAction->setShortcut (QKeySequence ("-"));

    mAddCtrAction->setText (tr ("Add Controller"));
    mAddIDECtrAction->setText (tr ("Add IDE Controller"));
    mAddSATACtrAction->setText (tr ("Add SATA Controller"));
    mAddSCSICtrAction->setText (tr ("Add SCSI Controller"));
    mAddFloppyCtrAction->setText (tr ("Add Floppy Controller"));
    mDelCtrAction->setText (tr ("Remove Controller"));
    mAddAttAction->setText (tr ("Add Attachment"));
    mDelAttAction->setText (tr ("Remove Attachment"));

    mAddCtrAction->setWhatsThis (tr ("Adds a new controller to the end of Storage Tree."));
    mDelCtrAction->setWhatsThis (tr ("Removes controller highlighted in Storage Tree."));
    mAddAttAction->setWhatsThis (tr ("Adds a new attachment to the Storage Tree using "
                                     "currently selected controller as parent."));
    mDelAttAction->setWhatsThis (tr ("Removes attachment highlighted in Storage Tree."));

    mAddCtrAction->setToolTip (mAddCtrAction->text().remove ('&') +
        QString (" (%1)").arg (mAddCtrAction->shortcut().toString()));
    mDelCtrAction->setToolTip (mDelCtrAction->text().remove ('&') +
        QString (" (%1)").arg (mDelCtrAction->shortcut().toString()));
    mAddAttAction->setToolTip (mAddAttAction->text().remove ('&') +
        QString (" (%1)").arg (mAddAttAction->shortcut().toString()));
    mDelAttAction->setToolTip (mDelAttAction->text().remove ('&') +
        QString (" (%1)").arg (mDelAttAction->shortcut().toString()));
}

void VBoxVMSettingsHD::showEvent (QShowEvent *aEvent)
{
    if (!mIsPolished)
    {
        mIsPolished = true;

        /* First column indent */
        mLtEmpty->setColumnMinimumWidth (0, 10);
        mLtController->setColumnMinimumWidth (0, 10);
        mLtAttachment->setColumnMinimumWidth (0, 10);

        /* Second column indent minimum width */
        QList <QLabel*> labelsList;
        labelsList << mLbSlot << mLbDevice << mLbVdi
                   << mLbHDVirtualSize << mLbHDActualSize << mLbSize
                   << mLbLocation << mLbHDFormat << mLbUsage;
        int maxWidth = 0;
        QFontMetrics metrics (font());
        foreach (QLabel *label, labelsList)
        {
            int width = metrics.width (label->text());
            maxWidth = width > maxWidth ? width : maxWidth;
        }
        mLtAttachment->setColumnMinimumWidth (1, maxWidth);
    }
    VBoxSettingsPage::showEvent (aEvent);
}

void VBoxVMSettingsHD::mediumUpdated (const VBoxMedium &aMedium)
{
    QModelIndex rootIndex = mStorageModel->root();
    for (int i = 0; i < mStorageModel->rowCount (rootIndex); ++ i)
    {
        QModelIndex ctrIndex = rootIndex.child (i, 0);
        for (int j = 0; j < mStorageModel->rowCount (ctrIndex); ++ j)
        {
            QModelIndex attIndex = ctrIndex.child (j, 0);
            QString attMediumId = mStorageModel->data (attIndex, StorageModel::R_AttMediumId).toString();
            if (attMediumId == aMedium.id())
            {
                mStorageModel->setData (attIndex, attMediumId, StorageModel::R_AttMediumId);
                mValidator->revalidate();
            }
        }
    }
}

void VBoxVMSettingsHD::mediumRemoved (VBoxDefs::MediumType /* aType */, const QString &aMediumId)
{
    QModelIndex rootIndex = mStorageModel->root();
    for (int i = 0; i < mStorageModel->rowCount (rootIndex); ++ i)
    {
        QModelIndex ctrIndex = rootIndex.child (i, 0);
        for (int j = 0; j < mStorageModel->rowCount (ctrIndex); ++ j)
        {
            QModelIndex attIndex = ctrIndex.child (j, 0);
            QString attMediumId = mStorageModel->data (attIndex, StorageModel::R_AttMediumId).toString();
            if (attMediumId == aMediumId)
            {
                mStorageModel->setData (attIndex, firstAvailableId, StorageModel::R_AttMediumId);
                mValidator->revalidate();
            }
        }
    }
}

void VBoxVMSettingsHD::addController()
{
    QMenu menu;
    menu.addAction (mAddIDECtrAction);
    menu.addAction (mAddSATACtrAction);
    menu.addAction (mAddSCSICtrAction);
    menu.addAction (mAddFloppyCtrAction);
    menu.exec (QCursor::pos());
}

void VBoxVMSettingsHD::addIDEController()
{
    mStorageModel->addController (generateUniqueName (tr ("IDE Controller")),
                                  KStorageBus_IDE, KStorageControllerType_PIIX4);
    emit storageChanged();
}

void VBoxVMSettingsHD::addSATAController()
{
    mStorageModel->addController (generateUniqueName (tr ("SATA Controller")),
                                  KStorageBus_SATA, KStorageControllerType_IntelAhci);
    emit storageChanged();
}

void VBoxVMSettingsHD::addSCSIController()
{
    mStorageModel->addController (generateUniqueName (tr ("SCSI Controller")),
                                  KStorageBus_SCSI, KStorageControllerType_LsiLogic);
    emit storageChanged();
}

void VBoxVMSettingsHD::addFloppyController()
{
    mStorageModel->addController (generateUniqueName (tr ("Floppy Controller")),
                                  KStorageBus_Floppy, KStorageControllerType_I82078);
    emit storageChanged();
}

void VBoxVMSettingsHD::delController()
{
    QModelIndex index = mTwStorageTree->currentIndex();
    if (!mStorageModel->data (index, StorageModel::R_IsController).toBool()) return;

    mStorageModel->delController (QUuid (mStorageModel->data (index, StorageModel::R_ItemId).toString()));
    emit storageChanged();
    mValidator->revalidate();
}

void VBoxVMSettingsHD::addAttachment (KDeviceType aDeviceType)
{
    QModelIndex index = mTwStorageTree->currentIndex();
    if (!mStorageModel->data (index, StorageModel::R_IsController).toBool()) return;

    if (aDeviceType == KDeviceType_Null)
        aDeviceType = mStorageModel->data (index, StorageModel::R_CtrDevices).value <DeviceTypeList>() [0];

    mStorageModel->addAttachment (QUuid (mStorageModel->data (index, StorageModel::R_ItemId).toString()), aDeviceType);
    emit storageChanged();
    mValidator->revalidate();
}

void VBoxVMSettingsHD::delAttachment()
{
    QModelIndex index = mTwStorageTree->currentIndex();
    QModelIndex parent = index.parent();
    if (!index.isValid() || !parent.isValid() ||
        !mStorageModel->data (index, StorageModel::R_IsAttachment).toBool() ||
        !mStorageModel->data (parent, StorageModel::R_IsController).toBool())
        return;

    mStorageModel->delAttachment (QUuid (mStorageModel->data (parent, StorageModel::R_ItemId).toString()),
                                  QUuid (mStorageModel->data (index, StorageModel::R_ItemId).toString()));
    emit storageChanged();
    mValidator->revalidate();
}

void VBoxVMSettingsHD::getInformation()
{
    mIsLoadingInProgress = true;

    QModelIndex index = mTwStorageTree->currentIndex();
    if (!index.isValid() || index == mStorageModel->root())
    {
        /* Showing Initial Page */
        mSwRightPane->setCurrentIndex (0);
    }
    else
    {
        switch (mStorageModel->data (index, StorageModel::R_ItemType).value <AbstractItem::ItemType>())
        {
            case AbstractItem::Type_ControllerItem:
            {
                /* Getting Controller Name */
                mLeName->setText (mStorageModel->data (index, StorageModel::R_CtrName).toString());

                /* Getting Controller Sub type */
                mCbType->clear();
                ControllerTypeList controllerTypeList (mStorageModel->data (index, StorageModel::R_CtrTypes).value <ControllerTypeList>());
                for (int i = 0; i < controllerTypeList.size(); ++ i)
                    mCbType->insertItem (mCbType->count(), vboxGlobal().toString (controllerTypeList [i]));
                KStorageControllerType type = mStorageModel->data (index, StorageModel::R_CtrType).value <KStorageControllerType>();
                int ctrPos = mCbType->findText (vboxGlobal().toString (type));
                mCbType->setCurrentIndex (ctrPos == -1 ? 0 : ctrPos);

                /* Showing Controller Page */
                mSwRightPane->setCurrentIndex (1);
                break;
            }
            case AbstractItem::Type_AttachmentItem:
            {
                /* Getting Attachment Slot */
                mCbSlot->clear();
                SlotsList slotsList (mStorageModel->data (index, StorageModel::R_AttSlots).value <SlotsList>());
                for (int i = 0; i < slotsList.size(); ++ i)
                    mCbSlot->insertItem (mCbSlot->count(), vboxGlobal().toFullString (slotsList [i]));
                StorageSlot slt = mStorageModel->data (index, StorageModel::R_AttSlot).value <StorageSlot>();
                int attSlotPos = mCbSlot->findText (vboxGlobal().toFullString (slt));
                mCbSlot->setCurrentIndex (attSlotPos == -1 ? 0 : attSlotPos);

                /* Getting Attachment Device */
                mCbDevice->clear();
                DeviceTypeList deviceTypeList (mStorageModel->data (index, StorageModel::R_AttDevices).value <DeviceTypeList>());
                for (int i = 0; i < deviceTypeList.size(); ++ i)
                    mCbDevice->insertItem (mCbDevice->count(), vboxGlobal().toString (deviceTypeList [i]));
                KDeviceType device = mStorageModel->data (index, StorageModel::R_AttDevice).value <KDeviceType>();
                int attDevicePos = mCbDevice->findText (vboxGlobal().toString (device));
                mCbDevice->setCurrentIndex (attDevicePos == -1 ? 0 : attDevicePos);

                /* Getting Show Diffs state */
                bool isShowDiffs = mStorageModel->data (index, StorageModel::R_AttIsShowDiffs).toBool();
                mCbShowDiffs->setChecked (isShowDiffs);

                /* Getting Attachment Medium */
                mCbVdi->setType (typeToLocal (device));
                mCbVdi->setShowDiffs (isShowDiffs);
                mCbVdi->setCurrentItem (mStorageModel->data (index, StorageModel::R_AttMediumId).toString());
                mCbVdi->refresh();

                /* Getting Passthrough state */
                bool isHostDrive = mStorageModel->data (index, StorageModel::R_AttIsHostDrive).toBool();
                mCbPassthrough->setEnabled (isHostDrive);
                mCbPassthrough->setChecked (isHostDrive && mStorageModel->data (index, StorageModel::R_AttIsPassthrough).toBool());

                /* Update optional widgets visibility */
                updateAdditionalObjects (device);

                /* Getting Other Information */
                mLbHDVirtualSizeValue->setText (compressText (mStorageModel->data (index, StorageModel::R_AttLogicalSize).toString()));
                mLbHDActualSizeValue->setText (compressText (mStorageModel->data (index, StorageModel::R_AttSize).toString()));
                mLbSizeValue->setText (compressText (mStorageModel->data (index, StorageModel::R_AttSize).toString()));
                mLbLocationValue->setText (compressText (mStorageModel->data (index, StorageModel::R_AttLocation).toString()));
                mLbHDFormatValue->setText (compressText (mStorageModel->data (index, StorageModel::R_AttFormat).toString()));
                mLbUsageValue->setText (compressText (mStorageModel->data (index, StorageModel::R_AttUsage).toString()));

                /* Showing Attachment Page */
                mSwRightPane->setCurrentIndex (2);
                break;
            }
            default:
                break;
        }
    }

    mValidator->revalidate();

    mIsLoadingInProgress = false;
}

void VBoxVMSettingsHD::setInformation()
{
    QModelIndex index = mTwStorageTree->currentIndex();
    if (mIsLoadingInProgress || !index.isValid() || index == mStorageModel->root()) return;

    QObject *sdr = sender();
    switch (mStorageModel->data (index, StorageModel::R_ItemType).value <AbstractItem::ItemType>())
    {
        case AbstractItem::Type_ControllerItem:
        {
            /* Setting Controller Name */
            if (sdr == mLeName)
                mStorageModel->setData (index, mLeName->text(), StorageModel::R_CtrName);
            /* Setting Controller Sub-Type */
            else if (sdr == mCbType)
                mStorageModel->setData (index, QVariant::fromValue (vboxGlobal().toControllerType (mCbType->currentText())),
                                        StorageModel::R_CtrType);
            break;
        }
        case AbstractItem::Type_AttachmentItem:
        {
            /* Setting Attachment Slot */
            if (sdr == mCbSlot)
                mStorageModel->setData (index, QVariant::fromValue (vboxGlobal().toStorageSlot (mCbSlot->currentText())),
                                        StorageModel::R_AttSlot);
            /* Setting Attachment Device-Type */
            else if (sdr == mCbDevice)
            {
                KDeviceType device = vboxGlobal().toDeviceType (mCbDevice->currentText());
                mStorageModel->setData (index, QVariant::fromValue (device), StorageModel::R_AttDevice);
                mCbVdi->setType (typeToLocal (device));
                mCbVdi->setShowDiffs (mCbShowDiffs->isChecked());
                mCbVdi->refresh();
            }
            /* Setting Attachment Medium */
            else if (sdr == mCbVdi)
                mStorageModel->setData (index, mCbVdi->id(), StorageModel::R_AttMediumId);
            else if (sdr == mCbShowDiffs)
                mStorageModel->setData (index, mCbShowDiffs->isChecked(), StorageModel::R_AttIsShowDiffs);
            else if (sdr == mCbPassthrough)
            {
                if (mStorageModel->data (index, StorageModel::R_AttIsHostDrive).toBool())
                    mStorageModel->setData (index, mCbPassthrough->isChecked(), StorageModel::R_AttIsPassthrough);
            }
            break;
        }
        default:
            break;
    }

    emit storageChanged();
    getInformation();
}

void VBoxVMSettingsHD::onVmmInvoked()
{
    QString id = getWithMediaManager (typeToLocal (vboxGlobal().toDeviceType (mCbDevice->currentText())));
    if (!id.isNull())
        mCbVdi->setCurrentItem (id);
}

void VBoxVMSettingsHD::updateActionsState()
{
    QModelIndex index = mTwStorageTree->currentIndex();

    mAddCtrAction->setEnabled (mStorageModel->data (index, StorageModel::R_IsMoreControllersPossible).toBool());
    mAddIDECtrAction->setEnabled (mStorageModel->data (index, StorageModel::R_IsMoreControllersPossible).toBool());
    mAddSATACtrAction->setEnabled (mStorageModel->data (index, StorageModel::R_IsMoreControllersPossible).toBool());
    mAddSCSICtrAction->setEnabled (mStorageModel->data (index, StorageModel::R_IsMoreControllersPossible).toBool());
    mAddFloppyCtrAction->setEnabled (mStorageModel->data (index, StorageModel::R_IsMoreControllersPossible).toBool());
    mAddAttAction->setEnabled (mStorageModel->data (index, StorageModel::R_IsController).toBool() &&
                               mStorageModel->data (index, StorageModel::R_IsMoreAttachmentsPossible).toBool());

    mDelCtrAction->setEnabled (mStorageModel->data (index, StorageModel::R_IsController).toBool());
    mDelAttAction->setEnabled (mStorageModel->data (index, StorageModel::R_IsAttachment).toBool());
}

void VBoxVMSettingsHD::onRowInserted (const QModelIndex &aParent, int aPosition)
{
    QModelIndex index = mStorageModel->index (aPosition, 0, aParent);

    switch (mStorageModel->data (index, StorageModel::R_ItemType).value <AbstractItem::ItemType>())
    {
        case AbstractItem::Type_ControllerItem:
        {
            /* Select the newly created Controller Item */
            mTwStorageTree->setCurrentIndex (index);
            break;
        }
        case AbstractItem::Type_AttachmentItem:
        {
            /* Expand parent if it is not expanded yet */
            if (!mTwStorageTree->isExpanded (aParent))
                mTwStorageTree->setExpanded (aParent, true);

            /* Check if no medium was selected for this attachment */
            if (mStorageModel->data (index, StorageModel::R_AttMediumId).toString().isEmpty())
            {
                /* Ask the user for the method to select medium */
                KDeviceType deviceType = mStorageModel->data (index, StorageModel::R_AttDevice).value <KDeviceType>();
                int askResult = vboxProblem().confirmRunNewHDWzdOrVDM (deviceType);
                QString mediumId = askResult == QIMessageBox::Yes ? getWithNewHDWizard() :
                                   askResult == QIMessageBox::No ? getWithMediaManager (typeToLocal (deviceType)) : QString();
                if (mediumId.isNull())
                    mediumId = firstAvailableId;
                mStorageModel->setData (index, mediumId, StorageModel::R_AttMediumId);
            }
            break;
        }
        default:
            break;
    }

    updateActionsState();
    getInformation();
}

void VBoxVMSettingsHD::onRowRemoved()
{
    updateActionsState();
    getInformation();
}

void VBoxVMSettingsHD::onCurrentItemChanged()
{
    updateActionsState();
    getInformation();
}

void VBoxVMSettingsHD::onContextMenuRequested (const QPoint &aPosition)
{
    QModelIndex index = mTwStorageTree->indexAt (aPosition);
    if (!index.isValid()) return addController();

    QMenu menu;
    switch (mStorageModel->data (index, StorageModel::R_ItemType).value <AbstractItem::ItemType>())
    {
        case AbstractItem::Type_ControllerItem:
        {
            menu.addAction (mAddAttAction);
            menu.addAction (mDelCtrAction);
            break;
        }
        case AbstractItem::Type_AttachmentItem:
        {
            menu.addAction (mDelAttAction);
            break;
        }
        default:
            break;
    }
    if (!menu.isEmpty())
        menu.exec (mTwStorageTree->viewport()->mapToGlobal (aPosition));
}

void VBoxVMSettingsHD::onDrawItemBranches (QPainter *aPainter, const QRect &aRect, const QModelIndex &aIndex)
{
    if (!aIndex.parent().isValid() || !aIndex.parent().parent().isValid()) return;

    aPainter->save();
    aPainter->translate (aRect.x(), aRect.y());

    int rows = mStorageModel->rowCount (aIndex.parent());

    // TODO Draw Correct Braches!

    if (aIndex.row() == 0)
        aPainter->drawLine (QPoint (aRect.width() / 2, 2), QPoint (aRect.width() / 2, aRect.height() / 2));
    else
        aPainter->drawLine (QPoint (aRect.width() / 2, 0), QPoint (aRect.width() / 2, aRect.height() / 2));

    if (aIndex.row() < rows - 1)
        aPainter->drawLine (QPoint (aRect.width() / 2, aRect.height() / 2), QPoint (aRect.width() / 2, aRect.height()));

    aPainter->drawLine (QPoint (aRect.width() / 2, aRect.height() / 2), QPoint (aRect.width() - 2, aRect.height() / 2));

    aPainter->restore();
}

void VBoxVMSettingsHD::onMouseClicked (QMouseEvent *aEvent)
{
    QModelIndex index = mTwStorageTree->indexAt (aEvent->pos());
    QRect indexRect = mTwStorageTree->visualRect (index);

    /* Process expander icon */
    if (mStorageModel->data (index, StorageModel::R_IsController).toBool())
    {
        QRect expanderRect = mStorageModel->data (index, StorageModel::R_ItemPixmapRect).toRect();
        expanderRect.translate (indexRect.x(), indexRect.y());
        if (expanderRect.contains (aEvent->pos()))
        {
            aEvent->setAccepted (true);
            mTwStorageTree->setExpanded (index, !mTwStorageTree->isExpanded (index));
            return;
        }
    }

    /* Process add-attachment icons */
    if (mStorageModel->data (index, StorageModel::R_IsController).toBool() &&
        mTwStorageTree->currentIndex() == index)
    {
        DeviceTypeList devicesList (mStorageModel->data (index, StorageModel::R_CtrDevices).value <DeviceTypeList>());
        for (int i = 0; i < devicesList.size(); ++ i)
        {
            KDeviceType deviceType = devicesList [i];

            QRect deviceRect;
            switch (deviceType)
            {
                case KDeviceType_HardDisk:
                {
                    deviceRect = mStorageModel->data (index, StorageModel::R_HDPixmapRect).toRect();
                    break;
                }
                case KDeviceType_DVD:
                {
                    deviceRect = mStorageModel->data (index, StorageModel::R_CDPixmapRect).toRect();
                    break;
                }
                case KDeviceType_Floppy:
                {
                    deviceRect = mStorageModel->data (index, StorageModel::R_FDPixmapRect).toRect();
                    break;
                }
                default:
                    break;
            }
            deviceRect.translate (indexRect.x() + indexRect.width(), indexRect.y());

            if (deviceRect.contains (aEvent->pos()))
            {
                aEvent->setAccepted (true);
                if (mAddAttAction->isEnabled())
                    addAttachment (deviceType);
                return;
            }
        }
    }
}

QString VBoxVMSettingsHD::getWithNewHDWizard()
{
    /* Run New HD Wizard */
    VBoxNewHDWzd dlg (this);

    return dlg.exec() == QDialog::Accepted ? dlg.hardDisk().GetId() : QString();
}

QString VBoxVMSettingsHD::getWithMediaManager (VBoxDefs::MediumType aMediumType)
{
    /* Run Media Manager */
    VBoxMediaManagerDlg dlg (this);
    dlg.setup (aMediumType,
               true /* do select? */,
               false /* do refresh? */,
               mMachine,
               mCbVdi->id(),
               mCbShowDiffs->isChecked());

    return dlg.exec() == QDialog::Accepted ? dlg.selectedId() : QString();
}

void VBoxVMSettingsHD::updateAdditionalObjects (KDeviceType aType)
{
    mCbShowDiffs->setVisible (aType == KDeviceType_HardDisk);
    mCbPassthrough->setVisible (aType == KDeviceType_DVD);

    mLbHDVirtualSize->setVisible (aType == KDeviceType_HardDisk);
    mLbHDVirtualSizeValue->setVisible (aType == KDeviceType_HardDisk);

    mLbHDActualSize->setVisible (aType == KDeviceType_HardDisk);
    mLbHDActualSizeValue->setVisible (aType == KDeviceType_HardDisk);

    mLbSize->setVisible (aType != KDeviceType_HardDisk);
    mLbSizeValue->setVisible (aType != KDeviceType_HardDisk);

    mLbHDFormat->setVisible (aType == KDeviceType_HardDisk);
    mLbHDFormatValue->setVisible (aType == KDeviceType_HardDisk);
}

QString VBoxVMSettingsHD::generateUniqueName (const QString &aTemplate) const
{
    int maxNumber = 0;
    QModelIndex rootIndex = mStorageModel->root();
    for (int i = 0; i < mStorageModel->rowCount (rootIndex); ++ i)
    {
        QModelIndex ctrIndex = rootIndex.child (i, 0);
        QString ctrName = mStorageModel->data (ctrIndex, StorageModel::R_CtrName).toString();
        if (ctrName.startsWith (aTemplate))
        {
            QString stringNumber (ctrName.right (ctrName.size() - aTemplate.size()));
            bool isConverted = false;
            int number = stringNumber.toInt (&isConverted);
            if (isConverted && number > maxNumber)
                maxNumber = number;
        }
    }
    return QString ("%1 %2").arg (aTemplate).arg (++ maxNumber);
}

