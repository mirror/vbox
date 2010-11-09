/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsStorage class implementation
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Local Includes */
#include "QIWidgetValidator.h"
#include "UIIconPool.h"
#include "UINewHDWzd.h"
#include "VBoxGlobal.h"
#include "QIFileDialog.h"
#include "VBoxProblemReporter.h"
#include "UIMachineSettingsStorage.h"

/* Global Includes */
#include <QHeaderView>
#include <QItemEditorFactory>
#include <QMetaProperty>
#include <QMouseEvent>
#include <QScrollBar>
#include <QStylePainter>
#include <QTimer>
#include <QCommonStyle>

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
    , mPool(MaxIndex)
{
    mPool [ControllerAddEn]          = QPixmap (":/controller_add_16px.png");
    mPool [ControllerAddDis]         = QPixmap (":/controller_add_disabled_16px.png");
    mPool [ControllerDelEn]          = QPixmap (":/controller_remove_16px.png");
    mPool [ControllerDelDis]         = QPixmap (":/controller_remove_disabled_16px.png");

    mPool [AttachmentAddEn]          = QPixmap (":/attachment_add_16px.png");
    mPool [AttachmentAddDis]         = QPixmap (":/attachment_add_disabled_16px.png");
    mPool [AttachmentDelEn]          = QPixmap (":/attachment_remove_16px.png");
    mPool [AttachmentDelDis]         = QPixmap (":/attachment_remove_disabled_16px.png");

    mPool [IDEControllerNormal]      = QPixmap (":/ide_16px.png");
    mPool [IDEControllerExpand]      = QPixmap (":/ide_expand_16px.png");
    mPool [IDEControllerCollapse]    = QPixmap (":/ide_collapse_16px.png");
    mPool [SATAControllerNormal]     = QPixmap (":/sata_16px.png");
    mPool [SATAControllerExpand]     = QPixmap (":/sata_expand_16px.png");
    mPool [SATAControllerCollapse]   = QPixmap (":/sata_collapse_16px.png");
    mPool [SCSIControllerNormal]     = QPixmap (":/scsi_16px.png");
    mPool [SCSIControllerExpand]     = QPixmap (":/scsi_expand_16px.png");
    mPool [SCSIControllerCollapse]   = QPixmap (":/scsi_collapse_16px.png");
    mPool [FloppyControllerNormal]   = QPixmap (":/floppy_16px.png");
    mPool [FloppyControllerExpand]   = QPixmap (":/floppy_expand_16px.png");
    mPool [FloppyControllerCollapse] = QPixmap (":/floppy_collapse_16px.png");

    mPool [IDEControllerAddEn]       = QPixmap (":/ide_add_16px.png");
    mPool [IDEControllerAddDis]      = QPixmap (":/ide_add_disabled_16px.png");
    mPool [SATAControllerAddEn]      = QPixmap (":/sata_add_16px.png");
    mPool [SATAControllerAddDis]     = QPixmap (":/sata_add_disabled_16px.png");
    mPool [SCSIControllerAddEn]      = QPixmap (":/scsi_add_16px.png");
    mPool [SCSIControllerAddDis]     = QPixmap (":/scsi_add_disabled_16px.png");
    mPool [FloppyControllerAddEn]    = QPixmap (":/floppy_add_16px.png");
    mPool [FloppyControllerAddDis]   = QPixmap (":/floppy_add_disabled_16px.png");

    mPool [HDAttachmentNormal]       = QPixmap (":/hd_16px.png");
    mPool [CDAttachmentNormal]       = QPixmap (":/cd_16px.png");
    mPool [FDAttachmentNormal]       = QPixmap (":/fd_16px.png");

    mPool [HDAttachmentAddEn]        = QPixmap (":/hd_add_16px.png");
    mPool [HDAttachmentAddDis]       = QPixmap (":/hd_add_disabled_16px.png");
    mPool [CDAttachmentAddEn]        = QPixmap (":/cd_add_16px.png");
    mPool [CDAttachmentAddDis]       = QPixmap (":/cd_add_disabled_16px.png");
    mPool [FDAttachmentAddEn]        = QPixmap (":/fd_add_16px.png");
    mPool [FDAttachmentAddDis]       = QPixmap (":/fd_add_disabled_16px.png");

    mPool [VMMEn]                    = QPixmap (":/select_file_16px.png");
    mPool [VMMDis]                   = QPixmap (":/select_file_dis_16px.png");
}

QPixmap PixmapPool::pixmap (PixmapType aType) const
{
    return aType > InvalidPixmap && aType < MaxIndex ? mPool [aType] : 0;
}

/* Abstract Controller Type */
AbstractControllerType::AbstractControllerType (KStorageBus aBusType, KStorageControllerType aCtrType)
    : mBusType (aBusType)
    , mCtrType (aCtrType)
{
    AssertMsg (mBusType != KStorageBus_Null, ("Wrong Bus Type {%d}!\n", mBusType));
    AssertMsg (mCtrType != KStorageControllerType_Null, ("Wrong Controller Type {%d}!\n", mCtrType));

    for (int i = 0; i < State_MAX; ++ i)
    {
        mPixmaps << PixmapPool::InvalidPixmap;
        switch (mBusType)
        {
            case KStorageBus_IDE:
                mPixmaps [i] = (PixmapPool::PixmapType) (PixmapPool::IDEControllerNormal + i);
                break;
            case KStorageBus_SATA:
                mPixmaps [i] = (PixmapPool::PixmapType) (PixmapPool::SATAControllerNormal + i);
                break;
            case KStorageBus_SCSI:
                mPixmaps [i] = (PixmapPool::PixmapType) (PixmapPool::SCSIControllerNormal + i);
                break;
            case KStorageBus_Floppy:
                mPixmaps [i] = (PixmapPool::PixmapType) (PixmapPool::FloppyControllerNormal + i);
                break;
            case KStorageBus_SAS:
                mPixmaps [i] = (PixmapPool::PixmapType) (PixmapPool::SATAControllerNormal + i);
                break;
            default:
                break;
        }
        AssertMsg (mPixmaps [i] != PixmapPool::InvalidPixmap, ("Item state pixmap was not set!\n"));
    }
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

PixmapPool::PixmapType AbstractControllerType::pixmap (ItemState aState) const
{
    return mPixmaps [aState];
}

void AbstractControllerType::setCtrType (KStorageControllerType aCtrType)
{
    mCtrType = aCtrType;
}

DeviceTypeList AbstractControllerType::deviceTypeList() const
{
    return vboxGlobal().virtualBox().GetSystemProperties().GetDeviceTypesForStorageBus (mBusType).toList();
}

/* IDE Controller Type */
IDEControllerType::IDEControllerType (KStorageControllerType aSubType)
    : AbstractControllerType (KStorageBus_IDE, aSubType)
{
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

KStorageControllerType FloppyControllerType::first() const
{
    return KStorageControllerType_I82078;
}

uint FloppyControllerType::size() const
{
    return 1;
}

/* SAS Controller Type */
SASControllerType::SASControllerType (KStorageControllerType aSubType)
    : AbstractControllerType (KStorageBus_SAS, aSubType)
{
}

KStorageControllerType SASControllerType::first() const
{
    return KStorageControllerType_LsiLogicSas;
}

uint SASControllerType::size() const
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

void AbstractItem::setMachineId (const QString &aMachineId)
{
    mMachineId = aMachineId;
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

ULONG RootItem::childCount (KStorageBus aBus) const
{
    ULONG result = 0;
    foreach (AbstractItem *item, mControllers)
    {
        ControllerItem *ctrItem = static_cast <ControllerItem*> (item);
        if (ctrItem->ctrBusType() == aBus)
            ++ result;
    }
    return result;
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

QPixmap RootItem::pixmap (ItemState /* aState */)
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
        case KStorageBus_SAS:
            mCtrType = new SASControllerType (aControllerType);
            break;
        default:
            AssertMsgFailed (("Wrong Controller Type {%d}!\n", aBusType));
            break;
    }

    mUseIoCache = vboxGlobal().virtualBox().GetSystemProperties().GetDefaultIoCacheSettingForStorageController (aControllerType);
}

ControllerItem::~ControllerItem()
{
    delete mCtrType;
    while (!mAttachments.isEmpty())
        delete mAttachments.first();
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

bool ControllerItem::ctrUseIoCache() const
{
    return mUseIoCache;
}

void ControllerItem::setCtrName (const QString &aCtrName)
{
    mCtrName = aCtrName;
}

void ControllerItem::setCtrType (KStorageControllerType aCtrType)
{
    mCtrType->setCtrType (aCtrType);
}

void ControllerItem::setCtrUseIoCache (bool aUseIoCache)
{
    mUseIoCache = aUseIoCache;
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
    QStringList allMediums;
    foreach (const VBoxMedium &medium, vboxGlobal().currentMediaList())
    {
         foreach (const KDeviceType &deviceType, mCtrType->deviceTypeList())
         {
             if (medium.isNull() || medium.medium().GetDeviceType() == deviceType)
             {
                 /* In 'don't show diffs' mode we should only show hard-disks which are:
                  * 1. Attached to 'current state' of this VM even if these are differencing disks.
                  * 2. Not attached to this VM at all only if they are not differencing disks. */
                 if (!aShowDiffs && medium.type() == VBoxDefs::MediumType_HardDisk)
                 {
                     if (medium.isAttachedInCurStateTo (parent()->machineId()) ||
                         (!medium.medium().GetMachineIds().contains (parent()->machineId()) && !medium.parent()))
                         allMediums << medium.id();
                 }
                 else allMediums << medium.id();
                 break;
             }
         }
    }
    return allMediums;
}

QStringList ControllerItem::ctrUsedMediumIds() const
{
    QStringList usedImages;
    for (int i = 0; i < mAttachments.size(); ++ i)
    {
        QString usedMediumId = static_cast <AttachmentItem*> (mAttachments [i])->attMediumId();
        if (!vboxGlobal().findMedium (usedMediumId).isNull())
            usedImages << usedMediumId;
    }
    return usedImages;
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
    return UIMachineSettingsStorage::tr ("<nobr><b>%1</b></nobr><br>"
                                 "<nobr>Bus:&nbsp;&nbsp;%2</nobr><br>"
                                 "<nobr>Type:&nbsp;&nbsp;%3</nobr>")
                                 .arg (mCtrName)
                                 .arg (vboxGlobal().toString (mCtrType->busType()))
                                 .arg (vboxGlobal().toString (mCtrType->ctrType()));
}

QPixmap ControllerItem::pixmap (ItemState aState)
{
    return PixmapPool::pool()->pixmap (mCtrType->pixmap (aState));
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

    /* Try to select unique medium for HD and empty medium for CD/FD device */
    QStringList freeMediumIds (attMediumIds());
    if (freeMediumIds.size() > 0)
        setAttMediumId (freeMediumIds [0]);
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
            (!medium.isNull() && medium.medium().GetDeviceType() == mAttDeviceType))
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
    mAttTip = medium.toolTipCheckRO (!mAttIsShowDiffs, mAttDeviceType != KDeviceType_HardDisk);
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

QPixmap AttachmentItem::pixmap (ItemState /* aState */)
{
    if (mAttPixmap.isNull())
    {
        switch (mAttDeviceType)
        {
            case KDeviceType_HardDisk:
                mAttPixmap = PixmapPool::pool()->pixmap (PixmapPool::HDAttachmentNormal);
                break;
            case KDeviceType_DVD:
                mAttPixmap = PixmapPool::pool()->pixmap (PixmapPool::CDAttachmentNormal);
                break;
            case KDeviceType_Floppy:
                mAttPixmap = PixmapPool::pool()->pixmap (PixmapPool::FDAttachmentNormal);
                break;
            default:
                break;
        }
    }
    return mAttPixmap;
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
    , mToolTipType (DefaultToolTip)
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

int StorageModel::columnCount (const QModelIndex & /* aParent */) const
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
            {
                if (item->rtti() == AbstractItem::Type_ControllerItem)
                {
                    QString tip (item->tip());
                    switch (mToolTipType)
                    {
                        case ExpanderToolTip:
                            if (aIndex.child (0, 0).isValid())
                                tip = UIMachineSettingsStorage::tr ("<nobr>Expand/Collapse&nbsp;Item</nobr>");
                            break;
                        case HDAdderToolTip:
                            tip = UIMachineSettingsStorage::tr ("<nobr>Add&nbsp;Hard&nbsp;Disk</nobr>");
                            break;
                        case CDAdderToolTip:
                            tip = UIMachineSettingsStorage::tr ("<nobr>Add&nbsp;CD/DVD&nbsp;Device</nobr>");
                            break;
                        case FDAdderToolTip:
                            tip = UIMachineSettingsStorage::tr ("<nobr>Add&nbsp;Floppy&nbsp;Device</nobr>");
                            break;
                        default:
                            break;
                    }
                    return tip;
                }
                return item->tip();
            }
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
            {
                ItemState state = State_DefaultItem;
                if (hasChildren (aIndex))
                    if (QTreeView *view = qobject_cast <QTreeView*> (QObject::parent()))
                        state = view->isExpanded (aIndex) ? State_ExpandedItem : State_CollapsedItem;
                return item->pixmap (state);
            }
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

        case R_ToolTipType:
        {
            return QVariant::fromValue (mToolTipType);
        }
        case R_IsMoreIDEControllersPossible:
        {
            return static_cast <RootItem*> (mRootItem)->childCount (KStorageBus_IDE) <
                    vboxGlobal().virtualBox().GetSystemProperties().GetMaxInstancesOfStorageBus (chipsetType(), KStorageBus_IDE);
        }
        case R_IsMoreSATAControllersPossible:
        {
            return static_cast <RootItem*> (mRootItem)->childCount (KStorageBus_SATA) <
                   vboxGlobal().virtualBox().GetSystemProperties().GetMaxInstancesOfStorageBus (chipsetType(), KStorageBus_SATA);
        }
        case R_IsMoreSCSIControllersPossible:
        {
            return static_cast <RootItem*> (mRootItem)->childCount (KStorageBus_SCSI) <
                   vboxGlobal().virtualBox().GetSystemProperties().GetMaxInstancesOfStorageBus (chipsetType(), KStorageBus_SCSI);
        }
        case R_IsMoreFloppyControllersPossible:
        {
            return static_cast <RootItem*> (mRootItem)->childCount (KStorageBus_Floppy) <
                   vboxGlobal().virtualBox().GetSystemProperties().GetMaxInstancesOfStorageBus (chipsetType(), KStorageBus_Floppy);
        }
        case R_IsMoreSASControllersPossible:
        {
            return static_cast <RootItem*> (mRootItem)->childCount (KStorageBus_SAS) <
                   vboxGlobal().virtualBox().GetSystemProperties().GetMaxInstancesOfStorageBus (chipsetType(), KStorageBus_SAS);
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
        case R_CtrIoCache:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_ControllerItem)
                    return static_cast <ControllerItem*> (item)->ctrUseIoCache();
            return false;
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
            return PixmapPool::pool()->pixmap (PixmapPool::HDAttachmentNormal);
        }
        case R_CDPixmapEn:
        {
            return PixmapPool::pool()->pixmap (PixmapPool::CDAttachmentNormal);
        }
        case R_FDPixmapEn:
        {
            return PixmapPool::pool()->pixmap (PixmapPool::FDAttachmentNormal);
        }

        case R_HDPixmapAddEn:
        {
            return PixmapPool::pool()->pixmap (PixmapPool::HDAttachmentAddEn);
        }
        case R_HDPixmapAddDis:
        {
            return PixmapPool::pool()->pixmap (PixmapPool::HDAttachmentAddDis);
        }
        case R_CDPixmapAddEn:
        {
            return PixmapPool::pool()->pixmap (PixmapPool::CDAttachmentAddEn);
        }
        case R_CDPixmapAddDis:
        {
            return PixmapPool::pool()->pixmap (PixmapPool::CDAttachmentAddDis);
        }
        case R_FDPixmapAddEn:
        {
            return PixmapPool::pool()->pixmap (PixmapPool::FDAttachmentAddEn);
        }
        case R_FDPixmapAddDis:
        {
            return PixmapPool::pool()->pixmap (PixmapPool::FDAttachmentAddDis);
        }
        case R_HDPixmapRect:
        {
            int margin = data (aIndex, R_Margin).toInt();
            int width = data (aIndex, R_IconSize).toInt();
            return QRect (0 - width - margin, margin, width, width);
        }
        case R_CDPixmapRect:
        {
            int margin = data (aIndex, R_Margin).toInt();
            int spacing = data (aIndex, R_Spacing).toInt();
            int width = data (aIndex, R_IconSize).toInt();
            return QRect (0 - width - spacing - width - margin, margin, width, width);
        }
        case R_FDPixmapRect:
        {
            int margin = data (aIndex, R_Margin).toInt();
            int width = data (aIndex, R_IconSize).toInt();
            return QRect (0 - width - margin, margin, width, width);
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
        case R_ToolTipType:
        {
            mToolTipType = aValue.value <ToolTipType>();
            emit dataChanged (aIndex, aIndex);
            return true;
        }
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
        case R_CtrIoCache:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_ControllerItem)
                {
                    static_cast <ControllerItem*> (item)->setCtrUseIoCache (aValue.toBool());
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
                    sort();
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

void StorageModel::sort(int /* iColumn */, Qt::SortOrder order)
{
    /* Count of controller items: */
    int iItemLevel1Count = mRootItem->childCount();
    /* For each of controller items: */
    for (int iItemLevel1Pos = 0; iItemLevel1Pos < iItemLevel1Count; ++iItemLevel1Pos)
    {
        /* Get iterated controller item: */
        AbstractItem *pItemLevel1 = mRootItem->childByPos(iItemLevel1Pos);
        ControllerItem *pControllerItem = static_cast<ControllerItem*>(pItemLevel1);
        /* Count of attachment items: */
        int iItemLevel2Count = pItemLevel1->childCount();
        /* Prepare empty list for sorted attachments: */
        QList<AbstractItem*> newAttachments;
        /* For each of attachment items: */
        for (int iItemLevel2Pos = 0; iItemLevel2Pos < iItemLevel2Count; ++iItemLevel2Pos)
        {
            /* Get iterated attachment item: */
            AbstractItem *pItemLevel2 = pItemLevel1->childByPos(iItemLevel2Pos);
            AttachmentItem *pAttachmentItem = static_cast<AttachmentItem*>(pItemLevel2);
            /* Get iterated attachment storage slot: */
            StorageSlot attachmentSlot = pAttachmentItem->attSlot();
            int iInsertPosition = 0;
            for (; iInsertPosition < newAttachments.size(); ++iInsertPosition)
            {
                /* Get sorted attachment item: */
                AbstractItem *pNewItemLevel2 = newAttachments[iInsertPosition];
                AttachmentItem *pNewAttachmentItem = static_cast<AttachmentItem*>(pNewItemLevel2);
                /* Get sorted attachment storage slot: */
                StorageSlot newAttachmentSlot = pNewAttachmentItem->attSlot();
                /* Apply sorting rule: */
                if (((order == Qt::AscendingOrder) && (attachmentSlot < newAttachmentSlot)) ||
                    ((order == Qt::DescendingOrder) && (attachmentSlot > newAttachmentSlot)))
                    break;
            }
            /* Insert iterated attachment into sorted position: */
            newAttachments.insert(iInsertPosition, pItemLevel2);
        }

        /* If that controller has attachments: */
        if (iItemLevel2Count)
        {
            /* We should update corresponding model-indexes: */
            QModelIndex controllerIndex = index(iItemLevel1Pos, 0, root());
            pControllerItem->setAttachments(newAttachments);
            /* That is actually beginMoveRows() + endMoveRows() which
             * unfortunately become available only in Qt 4.6 version. */
            beginRemoveRows(controllerIndex, 0, iItemLevel2Count - 1);
            endRemoveRows();
            beginInsertRows(controllerIndex, 0, iItemLevel2Count - 1);
            endInsertRows();
        }
    }
}

QModelIndex StorageModel::attachmentBySlot(QModelIndex controllerIndex, StorageSlot attachmentStorageSlot)
{
    /* Check what parent model index is valid, set and of 'controller' type: */
    AssertMsg(controllerIndex.isValid(), ("Controller index should be valid!\n"));
    AbstractItem *pParentItem = static_cast<AbstractItem*>(controllerIndex.internalPointer());
    AssertMsg(pParentItem, ("Parent item should be set!\n"));
    AssertMsg(pParentItem->rtti() == AbstractItem::Type_ControllerItem, ("Parent item should be of 'controller' type!\n"));
    NOREF(pParentItem);

    /* Search for suitable attachment one by one: */
    for (int i = 0; i < rowCount(controllerIndex); ++i)
    {
        QModelIndex curAttachmentIndex = index(i, 0, controllerIndex);
        StorageSlot curAttachmentStorageSlot = data(curAttachmentIndex, R_AttSlot).value<StorageSlot>();
        if (curAttachmentStorageSlot ==  attachmentStorageSlot)
            return curAttachmentIndex;
    }
    return QModelIndex();
}

Qt::ItemFlags StorageModel::flags (const QModelIndex &aIndex) const
{
    return !aIndex.isValid() ? QAbstractItemModel::flags (aIndex) :
           Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

KChipsetType StorageModel::chipsetType() const
{
    CMachine machine = vboxGlobal().virtualBox().FindMachine(mRootItem->machineId());
    Assert(!machine.isNull());
    return machine.GetChipsetType();
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

    /* Draw item background */
    QItemDelegate::drawBackground (aPainter, aOption, aIndex);

    /* Setup foreground settings */
    QPalette::ColorGroup cg = state & QStyle::State_Active ? QPalette::Active : QPalette::Inactive;
    bool isSelected = state & QStyle::State_Selected;
    bool isFocused = state & QStyle::State_HasFocus;
    bool isGrayOnLoosingFocus = QApplication::style()->styleHint (QStyle::SH_ItemView_ChangeHighlightOnFocus, &aOption) != 0;
    aPainter->setPen (aOption.palette.color (cg, isSelected && (isFocused || !isGrayOnLoosingFocus) ?
                                             QPalette::HighlightedText : QPalette::Text));

    aPainter->translate (rect.x(), rect.y());

    /* Draw Item Pixmap */
    aPainter->drawPixmap (model->data (aIndex, StorageModel::R_ItemPixmapRect).toRect().topLeft(),
                          model->data (aIndex, StorageModel::R_ItemPixmap).value <QPixmap>());

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
                                   model->data (aIndex, StorageModel::R_HDPixmapAddEn).value <QPixmap>() :
                                   model->data (aIndex, StorageModel::R_HDPixmapAddDis).value <QPixmap>();
                    break;
                }
                case KDeviceType_DVD:
                {
                    deviceRect = model->data (aIndex, StorageModel::R_CDPixmapRect).value <QRect>();
                    devicePixmap = model->data (aIndex, StorageModel::R_IsMoreAttachmentsPossible).toBool() ?
                                   model->data (aIndex, StorageModel::R_CDPixmapAddEn).value <QPixmap>() :
                                   model->data (aIndex, StorageModel::R_CDPixmapAddDis).value <QPixmap>();
                    break;
                }
                case KDeviceType_Floppy:
                {
                    deviceRect = model->data (aIndex, StorageModel::R_FDPixmapRect).value <QRect>();
                    devicePixmap = model->data (aIndex, StorageModel::R_IsMoreAttachmentsPossible).toBool() ?
                                   model->data (aIndex, StorageModel::R_FDPixmapAddEn).value <QPixmap>() :
                                   model->data (aIndex, StorageModel::R_FDPixmapAddDis).value <QPixmap>();
                    break;
                }
                default:
                    break;
            }

            aPainter->drawPixmap (QPoint (rect.width() + deviceRect.x(), deviceRect.y()), devicePixmap);
        }
    }

    aPainter->restore();

    drawFocus (aPainter, aOption, rect);
}

/**
 * QWidget class reimplementation.
 * Used as HD Settings widget.
 */
UIMachineSettingsStorage::UIMachineSettingsStorage()
    : mValidator(0)
    , mStorageModel(0)
    , mAddCtrAction(0), mDelCtrAction(0)
    , mAddIDECtrAction(0), mAddSATACtrAction(0), mAddSCSICtrAction(0), mAddSASCtrAction(0), mAddFloppyCtrAction(0)
    , mAddAttAction(0), mDelAttAction(0)
    , mAddHDAttAction(0), mAddCDAttAction(0), mAddFDAttAction(0)
    , mIsLoadingInProgress(0)
    , mIsPolished(false)
    , mDisableStaticControls(0)
{
    /* Apply UI decorations */
    Ui::UIMachineSettingsStorage::setupUi (this);

    /* Initialize pixmap pool */
    PixmapPool::pool (this);

    /* Controller Actions */
    mAddCtrAction = new QAction (this);
    mAddCtrAction->setIcon(UIIconPool::iconSet(PixmapPool::pool()->pixmap (PixmapPool::ControllerAddEn),
                                               PixmapPool::pool()->pixmap (PixmapPool::ControllerAddDis)));

    mAddIDECtrAction = new QAction (this);
    mAddIDECtrAction->setIcon(UIIconPool::iconSet(PixmapPool::pool()->pixmap (PixmapPool::IDEControllerAddEn),
                                                  PixmapPool::pool()->pixmap (PixmapPool::IDEControllerAddDis)));

    mAddSATACtrAction = new QAction (this);
    mAddSATACtrAction->setIcon(UIIconPool::iconSet(PixmapPool::pool()->pixmap (PixmapPool::SATAControllerAddEn),
                                                   PixmapPool::pool()->pixmap (PixmapPool::SATAControllerAddDis)));

    mAddSCSICtrAction = new QAction (this);
    mAddSCSICtrAction->setIcon(UIIconPool::iconSet(PixmapPool::pool()->pixmap (PixmapPool::SCSIControllerAddEn),
                                                   PixmapPool::pool()->pixmap (PixmapPool::SCSIControllerAddDis)));

    mAddFloppyCtrAction = new QAction (this);
    mAddFloppyCtrAction->setIcon(UIIconPool::iconSet(PixmapPool::pool()->pixmap (PixmapPool::FloppyControllerAddEn),
                                                     PixmapPool::pool()->pixmap (PixmapPool::FloppyControllerAddDis)));

    mAddSASCtrAction = new QAction (this);
    mAddSASCtrAction->setIcon(UIIconPool::iconSet(PixmapPool::pool()->pixmap (PixmapPool::SATAControllerAddEn),
                                                  PixmapPool::pool()->pixmap (PixmapPool::SATAControllerAddDis)));

    mDelCtrAction = new QAction (this);
    mDelCtrAction->setIcon(UIIconPool::iconSet(PixmapPool::pool()->pixmap (PixmapPool::ControllerDelEn),
                                               PixmapPool::pool()->pixmap (PixmapPool::ControllerDelDis)));

    /* Attachment Actions */
    mAddAttAction = new QAction (this);
    mAddAttAction->setIcon(UIIconPool::iconSet(PixmapPool::pool()->pixmap (PixmapPool::AttachmentAddEn),
                                               PixmapPool::pool()->pixmap (PixmapPool::AttachmentAddDis)));

    mAddHDAttAction = new QAction (this);
    mAddHDAttAction->setIcon(UIIconPool::iconSet(PixmapPool::pool()->pixmap (PixmapPool::HDAttachmentAddEn),
                                                 PixmapPool::pool()->pixmap (PixmapPool::HDAttachmentAddDis)));

    mAddCDAttAction = new QAction (this);
    mAddCDAttAction->setIcon(UIIconPool::iconSet(PixmapPool::pool()->pixmap (PixmapPool::CDAttachmentAddEn),
                                                 PixmapPool::pool()->pixmap (PixmapPool::CDAttachmentAddDis)));

    mAddFDAttAction = new QAction (this);
    mAddFDAttAction->setIcon(UIIconPool::iconSet(PixmapPool::pool()->pixmap (PixmapPool::FDAttachmentAddEn),
                                                 PixmapPool::pool()->pixmap (PixmapPool::FDAttachmentAddDis)));

    mDelAttAction = new QAction (this);
    mDelAttAction->setIcon(UIIconPool::iconSet(PixmapPool::pool()->pixmap (PixmapPool::AttachmentDelEn),
                                               PixmapPool::pool()->pixmap (PixmapPool::AttachmentDelDis)));

    /* Storage Model/View */
    mStorageModel = new StorageModel (mTwStorageTree);
    StorageDelegate *storageDelegate = new StorageDelegate (mTwStorageTree);
    mTwStorageTree->setMouseTracking (true);
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
    mTbOpen->setIcon(UIIconPool::iconSet(PixmapPool::pool()->pixmap (PixmapPool::VMMEn),
                                        PixmapPool::pool()->pixmap (PixmapPool::VMMDis)));
    mTbNew->setIcon(UIIconPool::iconSet(PixmapPool::pool()->pixmap (PixmapPool::HDAttachmentAddEn),
                                        PixmapPool::pool()->pixmap (PixmapPool::HDAttachmentAddDis)));

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
    connect (mAddSASCtrAction, SIGNAL (triggered (bool)), this, SLOT (addSASController()));
    connect (mAddFloppyCtrAction, SIGNAL (triggered (bool)), this, SLOT (addFloppyController()));
    connect (mDelCtrAction, SIGNAL (triggered (bool)), this, SLOT (delController()));
    connect (mAddAttAction, SIGNAL (triggered (bool)), this, SLOT (addAttachment()));
    connect (mAddHDAttAction, SIGNAL (triggered (bool)), this, SLOT (addHDAttachment()));
    connect (mAddCDAttAction, SIGNAL (triggered (bool)), this, SLOT (addCDAttachment()));
    connect (mAddFDAttAction, SIGNAL (triggered (bool)), this, SLOT (addFDAttachment()));
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
    connect (mTwStorageTree, SIGNAL (mouseMoved (QMouseEvent*)),
             this, SLOT (onMouseMoved (QMouseEvent*)));
    connect (mTwStorageTree, SIGNAL (mousePressed (QMouseEvent*)),
             this, SLOT (onMouseClicked (QMouseEvent*)));
    connect (mTwStorageTree, SIGNAL (mouseDoubleClicked (QMouseEvent*)),
             this, SLOT (onMouseClicked (QMouseEvent*)));
    connect (mLeName, SIGNAL (textEdited (const QString&)), this, SLOT (setInformation()));
    connect (mCbType, SIGNAL (activated (int)), this, SLOT (setInformation()));
    connect (mCbSlot, SIGNAL (activated (int)), this, SLOT (setInformation()));
    connect (mCbIoCache, SIGNAL (stateChanged (int)), this, SLOT (setInformation()));
    connect (mCbVdi, SIGNAL (activated (int)), this, SLOT (setInformation()));
    connect (mTbOpen, SIGNAL (clicked (bool)), this, SLOT (sltOpenMedium()));
    connect (mTbNew, SIGNAL (clicked (bool)), this, SLOT (sltNewMedium()));
    connect (mCbShowDiffs, SIGNAL (stateChanged (int)), this, SLOT (setInformation()));
    connect (mCbPassthrough, SIGNAL (stateChanged (int)), this, SLOT (setInformation()));

    /* Applying language settings */
    retranslateUi();

    /* Initial setup */
    setMinimumWidth (500);
    mSplitter->setSizes (QList<int>() << (int) (0.45 * minimumWidth()) << (int) (0.55 * minimumWidth()));
}

/* Load data to cashe from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsStorage::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Enumerate Mediums. We need at least the MediaList filled, so this is the
     * lasted point, where we can start. The rest of the media checking is done
     * in a background thread. */
    vboxGlobal().startEnumeratingMedia();

    /* Fill internal variables with corresponding values: */
    m_cache.m_strMachineId = m_machine.GetId();
    /* Load controllers list: */
    const CStorageControllerVector &controllers = m_machine.GetStorageControllers();
    for (int iControllerIndex = 0; iControllerIndex < controllers.size(); ++iControllerIndex)
    {
        /* Prepare controller item: */
        const CStorageController &controller = controllers[iControllerIndex];
        UIStorageControllerData controllerData;
        controllerData.m_strControllerName = controller.GetName();
        controllerData.m_controllerBus = controller.GetBus();
        controllerData.m_controllerType = controller.GetControllerType();
        controllerData.m_fUseHostIOCache = controller.GetUseHostIOCache();
        /* Load attachments list: */
        const CMediumAttachmentVector &attachments = m_machine.GetMediumAttachmentsOfController(controllerData.m_strControllerName);
        for (int iAttachmentIndex = 0; iAttachmentIndex < attachments.size(); ++iAttachmentIndex)
        {
            /* Prepare attachment item: */
            const CMediumAttachment &attachment = attachments[iAttachmentIndex];
            UIStorageAttachmentData attachmentData;
            attachmentData.m_attachmentType = attachment.GetType();
            attachmentData.m_iAttachmentPort = attachment.GetPort();
            attachmentData.m_iAttachmentDevice = attachment.GetDevice();
            attachmentData.m_fAttachmentPassthrough = attachment.GetPassthrough();
            CMedium comMedium(attachment.GetMedium());
            VBoxMedium vboxMedium;
            vboxGlobal().findMedium(comMedium, vboxMedium);
            attachmentData.m_strAttachmentMediumId = vboxMedium.id();
            controllerData.m_items << attachmentData;
        }
        m_cache.m_items << controllerData;
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

/* Load data to corresponding widgets from cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsStorage::getFromCache()
{
    /* Apply internal variables data to QWidget(s): */
    mCbVdi->setMachineId(m_cache.m_strMachineId);
    mStorageModel->setMachineId(m_cache.m_strMachineId);
    for (int iControllerIndex = 0; iControllerIndex < m_cache.m_items.size(); ++iControllerIndex)
    {
        /* Get iterated controller: */
        const UIStorageControllerData &controllerData = m_cache.m_items[iControllerIndex];
        QModelIndex controllerIndex = mStorageModel->addController(controllerData.m_strControllerName,
                                                                   controllerData.m_controllerBus,
                                                                   controllerData.m_controllerType);
        QUuid controllerId = QUuid(mStorageModel->data(controllerIndex, StorageModel::R_ItemId).toString());
        mStorageModel->setData(controllerIndex, controllerData.m_fUseHostIOCache, StorageModel::R_CtrIoCache);
        for (int iAttachmentIndex = 0; iAttachmentIndex < controllerData.m_items.size(); ++iAttachmentIndex)
        {
            /* Get iterated attachment: */
            const UIStorageAttachmentData &attachmentData = controllerData.m_items[iAttachmentIndex];
            QModelIndex attachmentIndex = mStorageModel->addAttachment(controllerId, attachmentData.m_attachmentType);
            StorageSlot attachmentStorageSlot(controllerData.m_controllerBus,
                                              attachmentData.m_iAttachmentPort,
                                              attachmentData.m_iAttachmentDevice);
            mStorageModel->setData(attachmentIndex, QVariant::fromValue(attachmentStorageSlot), StorageModel::R_AttSlot);
            mStorageModel->setData(attachmentIndex, attachmentData.m_fAttachmentPassthrough, StorageModel::R_AttIsPassthrough);
            mStorageModel->setData(attachmentIndex, attachmentData.m_strAttachmentMediumId, StorageModel::R_AttMediumId);
        }
    }
    /* Set the first controller as current if present */
    if (mStorageModel->rowCount(mStorageModel->root()) > 0)
        mTwStorageTree->setCurrentIndex(mStorageModel->index(0, 0, mStorageModel->root()));

    /* Update actions: */
    updateActionsState();

    /* Revalidate if possible: */
    if (mValidator) mValidator->revalidate();
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsStorage::putToCache()
{
    /* Gather internal variables data from QWidget(s): */
    m_cache.m_items.clear();
    QModelIndex rootIndex = mStorageModel->root();
    for (int iControllerIndex = 0; iControllerIndex < mStorageModel->rowCount(rootIndex); ++iControllerIndex)
    {
        QModelIndex controllerIndex = mStorageModel->index(iControllerIndex, 0, rootIndex);
        UIStorageControllerData controllerData;
        controllerData.m_strControllerName = mStorageModel->data(controllerIndex, StorageModel::R_CtrName).toString();
        controllerData.m_controllerBus = mStorageModel->data(controllerIndex, StorageModel::R_CtrBusType).value<KStorageBus>();
        controllerData.m_controllerType = mStorageModel->data(controllerIndex, StorageModel::R_CtrType).value<KStorageControllerType>();
        controllerData.m_fUseHostIOCache = mStorageModel->data(controllerIndex, StorageModel::R_CtrIoCache).toBool();
        for (int iAttachmentIndex = 0; iAttachmentIndex < mStorageModel->rowCount(controllerIndex); ++iAttachmentIndex)
        {
            QModelIndex attachmentIndex = mStorageModel->index(iAttachmentIndex, 0, controllerIndex);
            UIStorageAttachmentData attachmentData;
            attachmentData.m_attachmentType = mStorageModel->data(attachmentIndex, StorageModel::R_AttDevice).value<KDeviceType>();
            StorageSlot attachmentSlot = mStorageModel->data(attachmentIndex, StorageModel::R_AttSlot).value<StorageSlot>();
            attachmentData.m_iAttachmentPort = attachmentSlot.port;
            attachmentData.m_iAttachmentDevice = attachmentSlot.device;
            attachmentData.m_fAttachmentPassthrough = mStorageModel->data(attachmentIndex, StorageModel::R_AttIsPassthrough).toBool();
            attachmentData.m_strAttachmentMediumId = mStorageModel->data(attachmentIndex, StorageModel::R_AttMediumId).toString();
            controllerData.m_items << attachmentData;
        }
        m_cache.m_items << controllerData;
    }
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsStorage::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Remove currently present controllers & attachments */
    const CStorageControllerVector &controllers = m_machine.GetStorageControllers();
    for (int iControllerIndex = 0; iControllerIndex < controllers.size(); ++iControllerIndex)
    {
        const CStorageController &controller = controllers[iControllerIndex];
        QString strControllerName(controller.GetName());
        const CMediumAttachmentVector &attachments = m_machine.GetMediumAttachmentsOfController(strControllerName);
        for (int iAttachmentIndex = 0; iAttachmentIndex < attachments.size(); ++iAttachmentIndex)
        {
            const CMediumAttachment &attachment = attachments[iAttachmentIndex];
            m_machine.DetachDevice(strControllerName, attachment.GetPort(), attachment.GetDevice());
        }
        m_machine.RemoveStorageController(strControllerName);
    }
    /* Save created controllers: */
    for (int iControllerIndex = 0; iControllerIndex < m_cache.m_items.size(); ++iControllerIndex)
    {
        const UIStorageControllerData &controllerData = m_cache.m_items[iControllerIndex];
        CStorageController controller = m_machine.AddStorageController(controllerData.m_strControllerName, controllerData.m_controllerBus);
        controller.SetControllerType(controllerData.m_controllerType);
        controller.SetUseHostIOCache(controllerData.m_fUseHostIOCache);
        int cMaxUsedPort = -1;
        /* Save created attachments: */
        for (int iAttachmentIndex = 0; iAttachmentIndex < controllerData.m_items.size(); ++iAttachmentIndex)
        {
            const UIStorageAttachmentData &attachmentData = controllerData.m_items[iAttachmentIndex];
            VBoxMedium vboxMedium = vboxGlobal().findMedium(attachmentData.m_strAttachmentMediumId);
            CMedium comMedium = vboxMedium.medium();
            m_machine.AttachDevice(controllerData.m_strControllerName,
                                   attachmentData.m_iAttachmentPort, attachmentData.m_iAttachmentDevice,
                                   attachmentData.m_attachmentType, comMedium);
            if (m_machine.isOk())
            {
                if (attachmentData.m_attachmentType == KDeviceType_DVD)
                    m_machine.PassthroughDevice(controllerData.m_strControllerName,
                                                attachmentData.m_iAttachmentPort, attachmentData.m_iAttachmentDevice,
                                                attachmentData.m_fAttachmentPassthrough);
                cMaxUsedPort = attachmentData.m_iAttachmentPort > cMaxUsedPort ? attachmentData.m_iAttachmentPort : cMaxUsedPort;
            }
            else
            {
                // TODO: Fix problem reporter!
                //vboxProblem().cannotAttachDevice(this, m_machine, VBoxDefs::MediumType_HardDisk, vboxMedium.location(),
                //                                 controllerData.m_controllerBus, attachmentData.m_iAttachmentPort, attachmentData.m_iAttachmentDevice);
            }
        }
        if (controllerData.m_controllerBus == KStorageBus_SATA)
        {
            ULONG uSataPortsCount = cMaxUsedPort + 1;
            uSataPortsCount = qMax(uSataPortsCount, controller.GetMinPortCount());
            uSataPortsCount = qMin(uSataPortsCount, controller.GetMaxPortCount());
            controller.SetPortCount(uSataPortsCount);
        }
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsStorage::setValidator (QIWidgetValidator *aVal)
{
    mValidator = aVal;
}

bool UIMachineSettingsStorage::revalidate (QString &aWarning, QString &)
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
            QString value (QString ("%1 (%2)").arg (ctrName, vboxGlobal().toString (attSlot)));
            /* Check for emptiness */
            if (vboxGlobal().findMedium (key).isNull() && attDevice == KDeviceType_HardDisk)
            {
                aWarning = tr ("No hard disk is selected for <i>%1</i>.").arg (value);
                return aWarning.isNull();
            }
            /* Check for coincidence */
            if (!vboxGlobal().findMedium (key).isNull() && config.contains (key))
            {
                aWarning = tr ("<i>%1</i> uses a medium that is already attached to <i>%2</i>.")
                              .arg (value).arg (config [key]);
                return aWarning.isNull();
            }
            else config.insert (key, value);
        }
    }
    return aWarning.isNull();
}

void UIMachineSettingsStorage::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIMachineSettingsStorage::retranslateUi (this);

    mAddCtrAction->setShortcut (QKeySequence ("Ins"));
    mDelCtrAction->setShortcut (QKeySequence ("Del"));
    mAddAttAction->setShortcut (QKeySequence ("+"));
    mDelAttAction->setShortcut (QKeySequence ("-"));

    mAddCtrAction->setText (tr ("Add Controller"));
    mAddIDECtrAction->setText (tr ("Add IDE Controller"));
    mAddSATACtrAction->setText (tr ("Add SATA Controller"));
    mAddSCSICtrAction->setText (tr ("Add SCSI Controller"));
    mAddSASCtrAction->setText (tr ("Add SAS Controller"));
    mAddFloppyCtrAction->setText (tr ("Add Floppy Controller"));
    mDelCtrAction->setText (tr ("Remove Controller"));
    mAddAttAction->setText (tr ("Add Attachment"));
    mAddHDAttAction->setText (tr ("Add Hard Disk"));
    mAddCDAttAction->setText (tr ("Add CD/DVD Device"));
    mAddFDAttAction->setText (tr ("Add Floppy Device"));
    mDelAttAction->setText (tr ("Remove Attachment"));

    mAddCtrAction->setWhatsThis (tr ("Adds a new controller to the end of the Storage Tree."));
    mDelCtrAction->setWhatsThis (tr ("Removes the controller highlighted in the Storage Tree."));
    mAddAttAction->setWhatsThis (tr ("Adds a new attachment to the Storage Tree using "
                                     "currently selected controller as parent."));
    mDelAttAction->setWhatsThis (tr ("Removes the attachment highlighted in the Storage Tree."));

    mAddCtrAction->setToolTip (QString ("<nobr>%1&nbsp;(%2)")
                               .arg (mAddCtrAction->text().remove ('&'))
                               .arg (mAddCtrAction->shortcut().toString()));
    mDelCtrAction->setToolTip (QString ("<nobr>%1&nbsp;(%2)")
                               .arg (mDelCtrAction->text().remove ('&'))
                               .arg (mDelCtrAction->shortcut().toString()));
    mAddAttAction->setToolTip (QString ("<nobr>%1&nbsp;(%2)")
                               .arg (mAddAttAction->text().remove ('&'))
                               .arg (mAddAttAction->shortcut().toString()));
    mDelAttAction->setToolTip (QString ("<nobr>%1&nbsp;(%2)")
                               .arg (mDelAttAction->text().remove ('&'))
                               .arg (mDelAttAction->shortcut().toString()));
}

void UIMachineSettingsStorage::showEvent (QShowEvent *aEvent)
{
    if (!mIsPolished)
    {
        mIsPolished = true;

        /* First column indent */
        mLtEmpty->setColumnMinimumWidth (0, 10);
        mLtController->setColumnMinimumWidth (0, 10);
        mLtAttachment->setColumnMinimumWidth (0, 10);
#if 0
        /* Second column indent minimum width */
        QList <QLabel*> labelsList;
        labelsList << mLbSlot << mLbVdi
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
#endif
    }
    UISettingsPage::showEvent (aEvent);
}

void UIMachineSettingsStorage::mediumUpdated (const VBoxMedium &aMedium)
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
                if (mValidator) mValidator->revalidate();
            }
        }
    }
}

void UIMachineSettingsStorage::mediumRemoved (VBoxDefs::MediumType /* aType */, const QString &aMediumId)
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
                if (mValidator) mValidator->revalidate();
            }
        }
    }
}

void UIMachineSettingsStorage::addController()
{
    QMenu menu;
    menu.addAction (mAddIDECtrAction);
    menu.addAction (mAddSATACtrAction);
    menu.addAction (mAddSCSICtrAction);
    menu.addAction (mAddSASCtrAction);
    menu.addAction (mAddFloppyCtrAction);
    menu.exec (QCursor::pos());
}

void UIMachineSettingsStorage::addIDEController()
{
    addControllerWrapper (generateUniqueName (tr ("IDE Controller")), KStorageBus_IDE, KStorageControllerType_PIIX4);
}

void UIMachineSettingsStorage::addSATAController()
{
    addControllerWrapper (generateUniqueName (tr ("SATA Controller")), KStorageBus_SATA, KStorageControllerType_IntelAhci);
}

void UIMachineSettingsStorage::addSCSIController()
{
    addControllerWrapper (generateUniqueName (tr ("SCSI Controller")), KStorageBus_SCSI, KStorageControllerType_LsiLogic);
}

void UIMachineSettingsStorage::addFloppyController()
{
    addControllerWrapper (generateUniqueName (tr ("Floppy Controller")), KStorageBus_Floppy, KStorageControllerType_I82078);
}

void UIMachineSettingsStorage::addSASController()
{
    addControllerWrapper (generateUniqueName (tr ("SAS Controller")), KStorageBus_SAS, KStorageControllerType_LsiLogicSas);
}

void UIMachineSettingsStorage::delController()
{
    QModelIndex index = mTwStorageTree->currentIndex();
    if (!mStorageModel->data (index, StorageModel::R_IsController).toBool()) return;

    mStorageModel->delController (QUuid (mStorageModel->data (index, StorageModel::R_ItemId).toString()));
    emit storageChanged();
    if (mValidator) mValidator->revalidate();
}

void UIMachineSettingsStorage::addAttachment()
{
    QModelIndex index = mTwStorageTree->currentIndex();
    Assert (mStorageModel->data (index, StorageModel::R_IsController).toBool());

    DeviceTypeList deviceTypeList (mStorageModel->data (index, StorageModel::R_CtrDevices).value <DeviceTypeList>());
    bool justTrigger = deviceTypeList.size() == 1;
    bool showMenu = deviceTypeList.size() > 1;
    QMenu menu;
    foreach (const KDeviceType &deviceType, deviceTypeList)
    {
        switch (deviceType)
        {
            case KDeviceType_HardDisk:
                if (justTrigger)
                    mAddHDAttAction->trigger();
                if (showMenu)
                    menu.addAction (mAddHDAttAction);
                break;
            case KDeviceType_DVD:
                if (justTrigger)
                    mAddCDAttAction->trigger();
                if (showMenu)
                    menu.addAction (mAddCDAttAction);
                break;
            case KDeviceType_Floppy:
                if (justTrigger)
                    mAddFDAttAction->trigger();
                if (showMenu)
                    menu.addAction (mAddFDAttAction);
                break;
            default:
                break;
        }
    }
    if (showMenu)
        menu.exec (QCursor::pos());
}

void UIMachineSettingsStorage::addHDAttachment()
{
    addAttachmentWrapper (KDeviceType_HardDisk);
}

void UIMachineSettingsStorage::addCDAttachment()
{
    addAttachmentWrapper (KDeviceType_DVD);
}

void UIMachineSettingsStorage::addFDAttachment()
{
    addAttachmentWrapper (KDeviceType_Floppy);
}

void UIMachineSettingsStorage::delAttachment()
{
    QModelIndex index = mTwStorageTree->currentIndex();

    KDeviceType device = mStorageModel->data (index, StorageModel::R_AttDevice).value <KDeviceType>();
    /* Check if this would be the last DVD. If so let the user confirm this again. */
    if (   device == KDeviceType_DVD
        && deviceCount (KDeviceType_DVD) == 1)
    {
        if (vboxProblem().confirmRemovingOfLastDVDDevice() != QIMessageBox::Ok)
            return;
    }

    QModelIndex parent = index.parent();
    if (!index.isValid() || !parent.isValid() ||
        !mStorageModel->data (index, StorageModel::R_IsAttachment).toBool() ||
        !mStorageModel->data (parent, StorageModel::R_IsController).toBool())
        return;

    mStorageModel->delAttachment (QUuid (mStorageModel->data (parent, StorageModel::R_ItemId).toString()),
                                  QUuid (mStorageModel->data (index, StorageModel::R_ItemId).toString()));
    emit storageChanged();
    if (mValidator) mValidator->revalidate();
}

void UIMachineSettingsStorage::getInformation()
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

                bool isUseIoCache = mStorageModel->data (index, StorageModel::R_CtrIoCache).toBool();
                mCbIoCache->setChecked(isUseIoCache);

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
                    mCbSlot->insertItem (mCbSlot->count(), vboxGlobal().toString (slotsList [i]));
                StorageSlot slt = mStorageModel->data (index, StorageModel::R_AttSlot).value <StorageSlot>();
                int attSlotPos = mCbSlot->findText (vboxGlobal().toString (slt));
                mCbSlot->setCurrentIndex (attSlotPos == -1 ? 0 : attSlotPos);
                mCbSlot->setToolTip (mCbSlot->itemText (mCbSlot->currentIndex()));

                /* Getting Show Diffs state */
                bool isShowDiffs = mStorageModel->data (index, StorageModel::R_AttIsShowDiffs).toBool();
                mCbShowDiffs->setChecked (isShowDiffs);

                /* Getting Attachment Medium */
                KDeviceType device = mStorageModel->data (index, StorageModel::R_AttDevice).value <KDeviceType>();
                switch (device)
                {
                    case KDeviceType_HardDisk:
                        mLbVdi->setText(tr("Hard &Disk:"));
                        mTbOpen->setWhatsThis(tr("Open hard disk image file using file-open dialog."));
                        mTbOpen->setToolTip(tr("Open hard disk image file"));
                        mTbNew->setVisible(true);
                        break;
                    case KDeviceType_DVD:
                        mLbVdi->setText(tr("&CD/DVD Device:"));
                        mTbOpen->setWhatsThis(tr("Open CD/DVD image file using file-open dialog."));
                        mTbOpen->setToolTip(tr("Open CD/DVD image file"));
                        mTbNew->setVisible(false);
                        break;
                    case KDeviceType_Floppy:
                        mLbVdi->setText(tr("&Floppy Device:"));
                        mTbOpen->setWhatsThis(tr("Open floppy image file using file-open dialog."));
                        mTbOpen->setToolTip(tr("Open floppy image file"));
                        mTbNew->setVisible(false);
                        break;
                    default:
                        break;
                }
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

    if (mValidator) mValidator->revalidate();

    mIsLoadingInProgress = false;
}

void UIMachineSettingsStorage::setInformation()
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
            else if (sdr == mCbIoCache)
                mStorageModel->setData (index, mCbIoCache->isChecked(), StorageModel::R_CtrIoCache);
            break;
        }
        case AbstractItem::Type_AttachmentItem:
        {
            /* Setting Attachment Slot */
            if (sdr == mCbSlot)
            {
                QModelIndex controllerIndex = mStorageModel->parent(index);
                StorageSlot attachmentStorageSlot = vboxGlobal().toStorageSlot(mCbSlot->currentText());
                mStorageModel->setData(index, QVariant::fromValue(attachmentStorageSlot), StorageModel::R_AttSlot);
                QModelIndex theSameIndexAtNewPosition = mStorageModel->attachmentBySlot(controllerIndex, attachmentStorageSlot);
                AssertMsg(theSameIndexAtNewPosition.isValid(), ("Current attachment disappears!\n"));
                mTwStorageTree->setCurrentIndex(theSameIndexAtNewPosition);
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

void UIMachineSettingsStorage::sltOpenMedium()
{
    QString id = vboxGlobal().openMediumWithFileOpenDialog(mCbVdi->type(), this);
    if (!id.isNull())
        mCbVdi->setCurrentItem(id);
}

void UIMachineSettingsStorage::sltNewMedium()
{
    QString id = getWithNewHDWizard();
    if (!id.isNull())
        mCbVdi->setCurrentItem (id);
}

void UIMachineSettingsStorage::updateActionsState()
{
    QModelIndex index = mTwStorageTree->currentIndex();

    bool isIDEPossible = mStorageModel->data (index, StorageModel::R_IsMoreIDEControllersPossible).toBool();
    bool isSATAPossible = mStorageModel->data (index, StorageModel::R_IsMoreSATAControllersPossible).toBool();
    bool isSCSIPossible = mStorageModel->data (index, StorageModel::R_IsMoreSCSIControllersPossible).toBool();
    bool isFloppyPossible = mStorageModel->data (index, StorageModel::R_IsMoreFloppyControllersPossible).toBool();
    bool isSASPossible = mStorageModel->data (index, StorageModel::R_IsMoreSASControllersPossible).toBool();

    bool isController = mStorageModel->data (index, StorageModel::R_IsController).toBool();
    bool isAttachment = mStorageModel->data (index, StorageModel::R_IsAttachment).toBool();
    bool isAttachmentsPossible = mStorageModel->data (index, StorageModel::R_IsMoreAttachmentsPossible).toBool();

    mAddCtrAction->setEnabled (isIDEPossible || isSATAPossible || isSCSIPossible || isFloppyPossible || isSASPossible);
    mAddIDECtrAction->setEnabled (isIDEPossible);
    mAddSATACtrAction->setEnabled (isSATAPossible);
    mAddSCSICtrAction->setEnabled (isSCSIPossible);
    mAddFloppyCtrAction->setEnabled (isFloppyPossible);
    mAddSASCtrAction->setEnabled (isSASPossible);

    mAddAttAction->setEnabled (isController && isAttachmentsPossible);
    mAddHDAttAction->setEnabled (isController && isAttachmentsPossible);
    mAddCDAttAction->setEnabled (isController && isAttachmentsPossible);
    mAddFDAttAction->setEnabled (isController && isAttachmentsPossible);

    mDelCtrAction->setEnabled (isController);
    mDelAttAction->setEnabled (isAttachment);
}

void UIMachineSettingsStorage::onRowInserted (const QModelIndex &aParent, int aPosition)
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
                int askResult = vboxProblem().confirmRunNewHDWzdOrOFD (deviceType);
                QString mediumId = askResult == QIMessageBox::Yes ? getWithNewHDWizard() :
                                   askResult == QIMessageBox::No ? vboxGlobal().openMediumWithFileOpenDialog(typeToLocal (deviceType), this) : QString();
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

void UIMachineSettingsStorage::onRowRemoved()
{
    if (mStorageModel->rowCount (mStorageModel->root()) == 0)
        mTwStorageTree->setCurrentIndex (mStorageModel->root());

    updateActionsState();
    getInformation();
}

void UIMachineSettingsStorage::onCurrentItemChanged()
{
    updateActionsState();
    getInformation();
}

void UIMachineSettingsStorage::onContextMenuRequested (const QPoint &aPosition)
{
    QModelIndex index = mTwStorageTree->indexAt (aPosition);
    if (!index.isValid()) return addController();

    QMenu menu;
    switch (mStorageModel->data (index, StorageModel::R_ItemType).value <AbstractItem::ItemType>())
    {
        case AbstractItem::Type_ControllerItem:
        {
            DeviceTypeList deviceTypeList (mStorageModel->data (index, StorageModel::R_CtrDevices).value <DeviceTypeList>());
            foreach (KDeviceType deviceType, deviceTypeList)
            {
                switch (deviceType)
                {
                    case KDeviceType_HardDisk:
                        menu.addAction (mAddHDAttAction);
                        break;
                    case KDeviceType_DVD:
                        menu.addAction (mAddCDAttAction);
                        break;
                    case KDeviceType_Floppy:
                        menu.addAction (mAddFDAttAction);
                        break;
                    default:
                        break;
                }
            }
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

void UIMachineSettingsStorage::onDrawItemBranches (QPainter *aPainter, const QRect &aRect, const QModelIndex &aIndex)
{
    if (!aIndex.parent().isValid() || !aIndex.parent().parent().isValid()) return;

    aPainter->save();
    QStyleOption options;
    options.initFrom (mTwStorageTree);
    options.rect = aRect;
    options.state |= QStyle::State_Item;
    if (aIndex.row() < mStorageModel->rowCount (aIndex.parent()) - 1)
        options.state |= QStyle::State_Sibling;
    /* This pen is commonly used by different
     * look and feel styles to paint tree-view branches. */
    QPen pen (QBrush (options.palette.dark().color(), Qt::Dense4Pattern), 0);
    aPainter->setPen (pen);
    /* If we want tree-view branches to be always painted we have to use QCommonStyle::drawPrimitive()
     * because QCommonStyle performs branch painting as opposed to particular inherited sub-classing styles. */
    qobject_cast <QCommonStyle*> (style())->QCommonStyle::drawPrimitive (QStyle::PE_IndicatorBranch, &options, aPainter);
    aPainter->restore();
}

void UIMachineSettingsStorage::onMouseMoved (QMouseEvent *aEvent)
{
    QModelIndex index = mTwStorageTree->indexAt (aEvent->pos());
    QRect indexRect = mTwStorageTree->visualRect (index);

    /* Expander tool-tip */
    if (mStorageModel->data (index, StorageModel::R_IsController).toBool())
    {
        QRect expanderRect = mStorageModel->data (index, StorageModel::R_ItemPixmapRect).toRect();
        expanderRect.translate (indexRect.x(), indexRect.y());
        if (expanderRect.contains (aEvent->pos()))
        {
            aEvent->setAccepted (true);
            if (mStorageModel->data (index, StorageModel::R_ToolTipType).value <StorageModel::ToolTipType>() != StorageModel::ExpanderToolTip)
                mStorageModel->setData (index, QVariant::fromValue (StorageModel::ExpanderToolTip), StorageModel::R_ToolTipType);
            return;
        }
    }

    /* Adder tool-tip */
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
                switch (deviceType)
                {
                    case KDeviceType_HardDisk:
                    {
                        if (mStorageModel->data (index, StorageModel::R_ToolTipType).value <StorageModel::ToolTipType>() != StorageModel::HDAdderToolTip)
                            mStorageModel->setData (index, QVariant::fromValue (StorageModel::HDAdderToolTip), StorageModel::R_ToolTipType);
                        break;
                    }
                    case KDeviceType_DVD:
                    {
                        if (mStorageModel->data (index, StorageModel::R_ToolTipType).value <StorageModel::ToolTipType>() != StorageModel::CDAdderToolTip)
                            mStorageModel->setData (index, QVariant::fromValue (StorageModel::CDAdderToolTip), StorageModel::R_ToolTipType);
                        break;
                    }
                    case KDeviceType_Floppy:
                    {
                        if (mStorageModel->data (index, StorageModel::R_ToolTipType).value <StorageModel::ToolTipType>() != StorageModel::FDAdderToolTip)
                            mStorageModel->setData (index, QVariant::fromValue (StorageModel::FDAdderToolTip), StorageModel::R_ToolTipType);
                        break;
                    }
                    default:
                        break;
                }
                return;
            }
        }
    }

    /* Default tool-tip */
    if (mStorageModel->data (index, StorageModel::R_ToolTipType).value <StorageModel::ToolTipType>() != StorageModel::DefaultToolTip)
        mStorageModel->setData (index, StorageModel::DefaultToolTip, StorageModel::R_ToolTipType);
}

void UIMachineSettingsStorage::onMouseClicked (QMouseEvent *aEvent)
{
    QModelIndex index = mTwStorageTree->indexAt (aEvent->pos());
    QRect indexRect = mTwStorageTree->visualRect (index);

    /* Expander icon */
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

    /* Adder icons */
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
                    addAttachmentWrapper (deviceType);
                return;
            }
        }
    }
}

void UIMachineSettingsStorage::addControllerWrapper (const QString &aName, KStorageBus aBus, KStorageControllerType aType)
{
    QModelIndex index = mTwStorageTree->currentIndex();
    switch (aBus)
    {
        case KStorageBus_IDE:
            Assert (mStorageModel->data (index, StorageModel::R_IsMoreIDEControllersPossible).toBool());
            break;
        case KStorageBus_SATA:
            Assert (mStorageModel->data (index, StorageModel::R_IsMoreSATAControllersPossible).toBool());
            break;
        case KStorageBus_SCSI:
            Assert (mStorageModel->data (index, StorageModel::R_IsMoreSCSIControllersPossible).toBool());
            break;
        case KStorageBus_SAS:
            Assert (mStorageModel->data (index, StorageModel::R_IsMoreSASControllersPossible).toBool());
            break;
        case KStorageBus_Floppy:
            Assert (mStorageModel->data (index, StorageModel::R_IsMoreFloppyControllersPossible).toBool());
            break;
        default:
            break;
    }

    mStorageModel->addController (aName, aBus, aType);
    emit storageChanged();
}

void UIMachineSettingsStorage::addAttachmentWrapper (KDeviceType aDevice)
{
    QModelIndex index = mTwStorageTree->currentIndex();
    Assert (mStorageModel->data (index, StorageModel::R_IsController).toBool());
    Assert (mStorageModel->data (index, StorageModel::R_IsMoreAttachmentsPossible).toBool());

    mStorageModel->addAttachment (QUuid (mStorageModel->data (index, StorageModel::R_ItemId).toString()), aDevice);
    mStorageModel->sort();
    emit storageChanged();
    if (mValidator) mValidator->revalidate();
}

QString UIMachineSettingsStorage::getWithNewHDWizard()
{
    /* Run New HD Wizard */
    UINewHDWzd dlg(this);
    /* Initialize variables: */
    CGuestOSType guestOSType = vboxGlobal().virtualBox().GetGuestOSType(m_machine.GetOSTypeId());
    dlg.setRecommendedSize(guestOSType.GetRecommendedHDD());
    QString strMachineSettingsFilepath = m_machine.GetSettingsFilePath();
    QFileInfo fileInfo(strMachineSettingsFilepath);
    dlg.setDefaultPath(fileInfo.absolutePath());

    return dlg.exec() == QDialog::Accepted ? dlg.hardDisk().GetId() : QString();
}

void UIMachineSettingsStorage::updateAdditionalObjects (KDeviceType aType)
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

QString UIMachineSettingsStorage::generateUniqueName (const QString &aTemplate) const
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
            maxNumber = isConverted && (number > maxNumber) ? number : 1;
        }
    }
    return maxNumber ? QString ("%1 %2").arg (aTemplate).arg (++ maxNumber) : aTemplate;
}

uint32_t UIMachineSettingsStorage::deviceCount (KDeviceType aType) const
{
    uint32_t cDevices = 0;
    QModelIndex rootIndex = mStorageModel->root();
    for (int i = 0; i < mStorageModel->rowCount (rootIndex); ++ i)
    {
        QModelIndex ctrIndex = rootIndex.child (i, 0);
        for (int j = 0; j < mStorageModel->rowCount (ctrIndex); ++ j)
        {
            QModelIndex attIndex = ctrIndex.child (j, 0);
            KDeviceType attDevice = mStorageModel->data (attIndex, StorageModel::R_AttDevice).value <KDeviceType>();
            if (attDevice == aType)
                ++cDevices;
        }
    }

    return cDevices;
}

