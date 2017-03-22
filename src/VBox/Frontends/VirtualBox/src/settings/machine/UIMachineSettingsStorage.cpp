/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsStorage class implementation.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QHeaderView>
# include <QItemEditorFactory>
# include <QMouseEvent>
# include <QScrollBar>
# include <QStylePainter>
# include <QTimer>

/* GUI includes: */
# include "QIWidgetValidator.h"
# include "UIIconPool.h"
# include "UIWizardNewVD.h"
# include "VBoxGlobal.h"
# include "QIFileDialog.h"
# include "UIMessageCenter.h"
# include "UIMachineSettingsStorage.h"
# include "UIConverter.h"
# include "UIMedium.h"
# include "UIExtraDataManager.h"

/* COM includes: */
# include "CStorageController.h"
# include "CMediumAttachment.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

#include <QCommonStyle>
#include <QMetaProperty>


QString compressText (const QString &aText)
{
    return QString ("<nobr><compact elipsis=\"end\">%1</compact></nobr>").arg (aText);
}


/** Machine settings: Storage Attachment data structure. */
struct UIDataSettingsMachineStorageAttachment
{
    /** Constructs data. */
    UIDataSettingsMachineStorageAttachment()
        : m_attachmentType(KDeviceType_Null)
        , m_iAttachmentPort(-1)
        , m_iAttachmentDevice(-1)
        , m_strAttachmentMediumId(QString())
        , m_fAttachmentPassthrough(false)
        , m_fAttachmentTempEject(false)
        , m_fAttachmentNonRotational(false)
        , m_fAttachmentHotPluggable(false)
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsMachineStorageAttachment &other) const
    {
        return true
               && (m_attachmentType == other.m_attachmentType)
               && (m_iAttachmentPort == other.m_iAttachmentPort)
               && (m_iAttachmentDevice == other.m_iAttachmentDevice)
               && (m_strAttachmentMediumId == other.m_strAttachmentMediumId)
               && (m_fAttachmentPassthrough == other.m_fAttachmentPassthrough)
               && (m_fAttachmentTempEject == other.m_fAttachmentTempEject)
               && (m_fAttachmentNonRotational == other.m_fAttachmentNonRotational)
               && (m_fAttachmentHotPluggable == other.m_fAttachmentHotPluggable)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsMachineStorageAttachment &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsMachineStorageAttachment &other) const { return !equal(other); }

    /** Holds the attachment type. */
    KDeviceType  m_attachmentType;
    /** Holds the attachment port. */
    LONG         m_iAttachmentPort;
    /** Holds the attachment device. */
    LONG         m_iAttachmentDevice;
    /** Holds the attachment medium ID. */
    QString      m_strAttachmentMediumId;
    /** Holds whether the attachment being passed through. */
    bool         m_fAttachmentPassthrough;
    /** Holds whether the attachment being temporarily eject. */
    bool         m_fAttachmentTempEject;
    /** Holds whether the attachment is solid-state. */
    bool         m_fAttachmentNonRotational;
    /** Holds whether the attachment is hot-pluggable. */
    bool         m_fAttachmentHotPluggable;
};


/** Machine settings: Storage Controller data structure. */
struct UIDataSettingsMachineStorageController
{
    /** Constructs data. */
    UIDataSettingsMachineStorageController()
        : m_strControllerName(QString())
        , m_controllerBus(KStorageBus_Null)
        , m_controllerType(KStorageControllerType_Null)
        , m_uPortCount(0)
        , m_fUseHostIOCache(false)
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsMachineStorageController &other) const
    {
        return true
               && (m_strControllerName == other.m_strControllerName)
               && (m_controllerBus == other.m_controllerBus)
               && (m_controllerType == other.m_controllerType)
               && (m_uPortCount == other.m_uPortCount)
               && (m_fUseHostIOCache == other.m_fUseHostIOCache)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsMachineStorageController &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsMachineStorageController &other) const { return !equal(other); }

    /** Holds the controller name. */
    QString                 m_strControllerName;
    /** Holds the controller bus. */
    KStorageBus             m_controllerBus;
    /** Holds the controller type. */
    KStorageControllerType  m_controllerType;
    /** Holds the controller port count. */
    uint                    m_uPortCount;
    /** Holds whether the controller uses host IO cache. */
    bool                    m_fUseHostIOCache;
};


/** Machine settings: Storage page data structure. */
struct UIDataSettingsMachineStorage
{
    /** Constructs data. */
    UIDataSettingsMachineStorage() {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsMachineStorage& /* other */) const { return true; }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsMachineStorage& /* other */) const { return false; }
};


/** UIIconPool interface extension used as Storage Settings page icon-pool. */
class UIIconPoolStorageSettings : public UIIconPool
{
public:

    /** Icon-pool instance access method. */
    static UIIconPoolStorageSettings* instance();
    /** Create icon-pool instance. */
    static void create();
    /** Destroy icon-pool instance. */
    static void destroy();

    /** Returns pixmap corresponding to passed @a pixmapType. */
    QPixmap pixmap(PixmapType pixmapType) const;
    /** Returns icon (probably merged) corresponding to passed @a pixmapType and @a pixmapDisabledType. */
    QIcon icon(PixmapType pixmapType, PixmapType pixmapDisabledType = InvalidPixmap) const;

private:

    /** Icon-pool constructor. */
    UIIconPoolStorageSettings();
    /** Icon-pool destructor. */
    ~UIIconPoolStorageSettings();

    /** Icon-pool instance. */
    static UIIconPoolStorageSettings *m_spInstance;
    /** Icon-pool names cache. */
    QMap<PixmapType, QString> m_names;
    /** Icon-pool icons cache. */
    mutable QMap<PixmapType, QIcon> m_icons;
};
UIIconPoolStorageSettings* iconPool() { return UIIconPoolStorageSettings::instance(); }

/* static */
UIIconPoolStorageSettings* UIIconPoolStorageSettings::m_spInstance = 0;
UIIconPoolStorageSettings* UIIconPoolStorageSettings::instance() { return m_spInstance; }
void UIIconPoolStorageSettings::create() { new UIIconPoolStorageSettings; }
void UIIconPoolStorageSettings::destroy() { delete m_spInstance; }

QPixmap UIIconPoolStorageSettings::pixmap(PixmapType pixmapType) const
{
    /* Prepare fallback pixmap: */
    static QPixmap nullPixmap;

    /* If we do NOT have that 'pixmap type' icon cached already: */
    if (!m_icons.contains(pixmapType))
    {
        /* Compose proper icon if we have that 'pixmap type' known: */
        if (m_names.contains(pixmapType))
            m_icons[pixmapType] = iconSet(m_names[pixmapType]);
        /* Assign fallback icon if we do NOT have that 'pixmap type' known: */
        else
            m_icons[pixmapType] = iconSet(nullPixmap);
    }

    /* Retrieve corresponding icon: */
    const QIcon &icon = m_icons[pixmapType];
    AssertMsgReturn(!icon.isNull(),
                    ("Undefined icon for type '%d'.", (int)pixmapType),
                    nullPixmap);

    /* Retrieve available sizes for that icon: */
    const QList<QSize> availableSizes = icon.availableSizes();
    AssertMsgReturn(!availableSizes.isEmpty(),
                    ("Undefined icon for type '%s'.", (int)pixmapType),
                    nullPixmap);

    /* Determine icon metric: */
    const QStyle *pStyle = QApplication::style();
    const int iIconMetric = pStyle->pixelMetric(QStyle::PM_SmallIconSize);

    /* Return pixmap of first available size: */
    return icon.pixmap(QSize(iIconMetric, iIconMetric));
}

QIcon UIIconPoolStorageSettings::icon(PixmapType pixmapType,
                                      PixmapType pixmapDisabledType /* = InvalidPixmap */) const
{
    /* Prepare fallback pixmap: */
    static QPixmap nullPixmap;
    /* Prepare fallback icon: */
    static QIcon nullIcon;

    /* If we do NOT have that 'pixmap type' icon cached already: */
    if (!m_icons.contains(pixmapType))
    {
        /* Compose proper icon if we have that 'pixmap type' known: */
        if (m_names.contains(pixmapType))
            m_icons[pixmapType] = iconSet(m_names[pixmapType]);
        /* Assign fallback icon if we do NOT have that 'pixmap type' known: */
        else
            m_icons[pixmapType] = iconSet(nullPixmap);
    }

    /* Retrieve normal icon: */
    const QIcon &icon = m_icons[pixmapType];
    AssertMsgReturn(!icon.isNull(),
                    ("Undefined icon for type '%d'.", (int)pixmapType),
                    nullIcon);

    /* If 'disabled' icon is invalid => just return 'normal' icon: */
    if (pixmapDisabledType == InvalidPixmap)
        return icon;

    /* If we do NOT have that 'pixmap disabled type' icon cached already: */
    if (!m_icons.contains(pixmapDisabledType))
    {
        /* Compose proper icon if we have that 'pixmap disabled type' known: */
        if (m_names.contains(pixmapDisabledType))
            m_icons[pixmapDisabledType] = iconSet(m_names[pixmapDisabledType]);
        /* Assign fallback icon if we do NOT have that 'pixmap disabled type' known: */
        else
            m_icons[pixmapDisabledType] = iconSet(nullPixmap);
    }

    /* Retrieve disabled icon: */
    const QIcon &iconDisabled = m_icons[pixmapDisabledType];
    AssertMsgReturn(!iconDisabled.isNull(),
                    ("Undefined icon for type '%d'.", (int)pixmapDisabledType),
                    nullIcon);

    /* Return icon composed on the basis of two above: */
    QIcon resultIcon = icon;
    foreach (const QSize &size, iconDisabled.availableSizes())
        resultIcon.addPixmap(iconDisabled.pixmap(size), QIcon::Disabled);
    return resultIcon;
}

UIIconPoolStorageSettings::UIIconPoolStorageSettings()
{
    /* Connect instance: */
    m_spInstance = this;

    /* Controller file-names: */
    m_names.insert(ControllerAddEn,          ":/controller_add_16px.png");
    m_names.insert(ControllerAddDis,         ":/controller_add_disabled_16px.png");
    m_names.insert(ControllerDelEn,          ":/controller_remove_16px.png");
    m_names.insert(ControllerDelDis,         ":/controller_remove_disabled_16px.png");
    /* Attachment file-names: */
    m_names.insert(AttachmentAddEn,          ":/attachment_add_16px.png");
    m_names.insert(AttachmentAddDis,         ":/attachment_add_disabled_16px.png");
    m_names.insert(AttachmentDelEn,          ":/attachment_remove_16px.png");
    m_names.insert(AttachmentDelDis,         ":/attachment_remove_disabled_16px.png");
    /* Specific controller default/expand/collapse file-names: */
    m_names.insert(IDEControllerNormal,      ":/ide_16px.png");
    m_names.insert(IDEControllerExpand,      ":/ide_expand_16px.png");
    m_names.insert(IDEControllerCollapse,    ":/ide_collapse_16px.png");
    m_names.insert(SATAControllerNormal,     ":/sata_16px.png");
    m_names.insert(SATAControllerExpand,     ":/sata_expand_16px.png");
    m_names.insert(SATAControllerCollapse,   ":/sata_collapse_16px.png");
    m_names.insert(SCSIControllerNormal,     ":/scsi_16px.png");
    m_names.insert(SCSIControllerExpand,     ":/scsi_expand_16px.png");
    m_names.insert(SCSIControllerCollapse,   ":/scsi_collapse_16px.png");
    m_names.insert(USBControllerNormal,      ":/usb_16px.png");
    m_names.insert(USBControllerExpand,      ":/usb_expand_16px.png");
    m_names.insert(USBControllerCollapse,    ":/usb_collapse_16px.png");
    m_names.insert(NVMeControllerNormal,     ":/ide_16px.png");
    m_names.insert(NVMeControllerExpand,     ":/ide_expand_16px.png");
    m_names.insert(NVMeControllerCollapse,   ":/ide_collapse_16px.png");
    m_names.insert(FloppyControllerNormal,   ":/floppy_16px.png");
    m_names.insert(FloppyControllerExpand,   ":/floppy_expand_16px.png");
    m_names.insert(FloppyControllerCollapse, ":/floppy_collapse_16px.png");
    /* Specific controller add file-names: */
    m_names.insert(IDEControllerAddEn,       ":/ide_add_16px.png");
    m_names.insert(IDEControllerAddDis,      ":/ide_add_disabled_16px.png");
    m_names.insert(SATAControllerAddEn,      ":/sata_add_16px.png");
    m_names.insert(SATAControllerAddDis,     ":/sata_add_disabled_16px.png");
    m_names.insert(SCSIControllerAddEn,      ":/scsi_add_16px.png");
    m_names.insert(SCSIControllerAddDis,     ":/scsi_add_disabled_16px.png");
    m_names.insert(USBControllerAddEn,       ":/usb_add_16px.png");
    m_names.insert(USBControllerAddDis,      ":/usb_add_disabled_16px.png");
    m_names.insert(NVMeControllerAddEn,      ":/ide_add_16px.png");
    m_names.insert(NVMeControllerAddDis,     ":/ide_add_disabled_16px.png");
    m_names.insert(FloppyControllerAddEn,    ":/floppy_add_16px.png");
    m_names.insert(FloppyControllerAddDis,   ":/floppy_add_disabled_16px.png");
    /* Specific attachment file-names: */
    m_names.insert(HDAttachmentNormal,       ":/hd_16px.png");
    m_names.insert(CDAttachmentNormal,       ":/cd_16px.png");
    m_names.insert(FDAttachmentNormal,       ":/fd_16px.png");
    /* Specific attachment add file-names: */
    m_names.insert(HDAttachmentAddEn,        ":/hd_add_16px.png");
    m_names.insert(HDAttachmentAddDis,       ":/hd_add_disabled_16px.png");
    m_names.insert(CDAttachmentAddEn,        ":/cd_add_16px.png");
    m_names.insert(CDAttachmentAddDis,       ":/cd_add_disabled_16px.png");
    m_names.insert(FDAttachmentAddEn,        ":/fd_add_16px.png");
    m_names.insert(FDAttachmentAddDis,       ":/fd_add_disabled_16px.png");
    /* Specific attachment custom file-names: */
    m_names.insert(ChooseExistingEn,         ":/select_file_16px.png");
    m_names.insert(ChooseExistingDis,        ":/select_file_disabled_16px.png");
    m_names.insert(HDNewEn,                  ":/hd_new_16px.png");
    m_names.insert(HDNewDis,                 ":/hd_new_disabled_16px.png");
    m_names.insert(CDUnmountEnabled,         ":/cd_unmount_16px.png");
    m_names.insert(CDUnmountDisabled,        ":/cd_unmount_disabled_16px.png");
    m_names.insert(FDUnmountEnabled,         ":/fd_unmount_16px.png");
    m_names.insert(FDUnmountDisabled,        ":/fd_unmount_disabled_16px.png");
}

UIIconPoolStorageSettings::~UIIconPoolStorageSettings()
{
    /* Disconnect instance: */
    m_spInstance = 0;
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
        mPixmaps << InvalidPixmap;
        switch (mBusType)
        {
            case KStorageBus_IDE:
                mPixmaps [i] = (PixmapType)(IDEControllerNormal + i);
                break;
            case KStorageBus_SATA:
                mPixmaps [i] = (PixmapType)(SATAControllerNormal + i);
                break;
            case KStorageBus_SCSI:
                mPixmaps [i] = (PixmapType)(SCSIControllerNormal + i);
                break;
            case KStorageBus_Floppy:
                mPixmaps [i] = (PixmapType)(FloppyControllerNormal + i);
                break;
            case KStorageBus_SAS:
                mPixmaps [i] = (PixmapType)(SATAControllerNormal + i);
                break;
            case KStorageBus_USB:
                mPixmaps [i] = (PixmapType)(USBControllerNormal + i);
                break;
            case KStorageBus_PCIe:
                mPixmaps [i] = (PixmapType)(NVMeControllerNormal + i);
                break;
            default:
                break;
        }
        AssertMsg (mPixmaps [i] != InvalidPixmap, ("Item state pixmap was not set!\n"));
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

PixmapType AbstractControllerType::pixmap(ItemState aState) const
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

/* USB Controller Type */
USBStorageControllerType::USBStorageControllerType (KStorageControllerType aSubType)
    : AbstractControllerType (KStorageBus_USB, aSubType)
{
}

KStorageControllerType USBStorageControllerType::first() const
{
    return KStorageControllerType_USB;
}

uint USBStorageControllerType::size() const
{
    return 1;
}

/* NVMe Controller Type */
NVMeStorageControllerType::NVMeStorageControllerType (KStorageControllerType aSubType)
    : AbstractControllerType (KStorageBus_PCIe, aSubType)
{
}

KStorageControllerType NVMeStorageControllerType::first() const
{
    return KStorageControllerType_NVMe;
}

uint NVMeStorageControllerType::size() const
{
    return 1;
}

/* Abstract Item */
AbstractItem::AbstractItem(QITreeView *pParent)
    : QITreeViewItem(pParent)
    , m_pParentItem(0)
    , mId(QUuid::createUuid())
{
    if (m_pParentItem)
        m_pParentItem->addChild(this);
}

AbstractItem::AbstractItem(AbstractItem *pParentItem)
    : QITreeViewItem(pParentItem)
    , m_pParentItem(pParentItem)
    , mId(QUuid::createUuid())
{
    if (m_pParentItem)
        m_pParentItem->addChild(this);
}

AbstractItem::~AbstractItem()
{
    if (m_pParentItem)
        m_pParentItem->delChild(this);
}

AbstractItem* AbstractItem::parent() const
{
    return m_pParentItem;
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
RootItem::RootItem(QITreeView *pParent)
    : AbstractItem(pParent)
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

AbstractItem* RootItem::childItem (int aIndex) const
{
    return mControllers [aIndex];
}

AbstractItem* RootItem::childItemById (const QUuid &aId) const
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
    , mPortCount (0)
    , mUseIoCache (false)
{
    /* Check for proper parent type */
    AssertMsg(m_pParentItem->rtti() == AbstractItem::Type_RootItem, ("Incorrect parent type!\n"));

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
        case KStorageBus_USB:
            mCtrType = new USBStorageControllerType (aControllerType);
            break;
        case KStorageBus_PCIe:
            mCtrType = new NVMeStorageControllerType (aControllerType);
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

uint ControllerItem::portCount()
{
    /* Recalculate actual port count: */
    for (int i = 0; i < mAttachments.size(); ++i)
    {
        AttachmentItem *pItem = static_cast<AttachmentItem*>(mAttachments[i]);
        if (mPortCount < (uint)pItem->attSlot().port + 1)
            mPortCount = (uint)pItem->attSlot().port + 1;
    }
    return mPortCount;
}

uint ControllerItem::maxPortCount()
{
    return (uint)vboxGlobal().virtualBox().GetSystemProperties().GetMaxPortCountForStorageBus(ctrBusType());
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

void ControllerItem::setPortCount (uint aPortCount)
{
    /* Limit maximum port count: */
    mPortCount = qMin(aPortCount, (uint)vboxGlobal().virtualBox().GetSystemProperties().GetMaxPortCountForStorageBus(ctrBusType()));
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

AbstractItem::ItemType ControllerItem::rtti() const
{
    return Type_ControllerItem;
}

AbstractItem* ControllerItem::childItem (int aIndex) const
{
    return mAttachments [aIndex];
}

AbstractItem* ControllerItem::childItemById (const QUuid &aId) const
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
    return UIMachineSettingsStorage::tr("Controller: %1").arg(ctrName());
}

QString ControllerItem::tip() const
{
    return UIMachineSettingsStorage::tr ("<nobr><b>%1</b></nobr><br>"
                                 "<nobr>Bus:&nbsp;&nbsp;%2</nobr><br>"
                                 "<nobr>Type:&nbsp;&nbsp;%3</nobr>")
                                 .arg (mCtrName)
                                 .arg (gpConverter->toString (mCtrType->busType()))
                                 .arg (gpConverter->toString (mCtrType->ctrType()));
}

QPixmap ControllerItem::pixmap (ItemState aState)
{
    return iconPool()->pixmap(mCtrType->pixmap(aState));
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
    , mAttIsHostDrive (false)
    , mAttIsPassthrough (false)
    , mAttIsTempEject (false)
    , mAttIsNonRotational (false)
    , m_fIsHotPluggable(false)
{
    /* Check for proper parent type */
    AssertMsg(m_pParentItem->rtti() == AbstractItem::Type_ControllerItem, ("Incorrect parent type!\n"));

    /* Select default slot */
    AssertMsg (!attSlots().isEmpty(), ("There should be at least one available slot!\n"));
    mAttSlot = attSlots() [0];
}

StorageSlot AttachmentItem::attSlot() const
{
    return mAttSlot;
}

SlotsList AttachmentItem::attSlots() const
{
    ControllerItem *ctr = static_cast<ControllerItem*>(m_pParentItem);

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
    return static_cast<ControllerItem*>(m_pParentItem)->ctrDeviceTypeList();
}

QString AttachmentItem::attMediumId() const
{
    return mAttMediumId;
}

bool AttachmentItem::attIsHostDrive() const
{
    return mAttIsHostDrive;
}

bool AttachmentItem::attIsPassthrough() const
{
    return mAttIsPassthrough;
}

bool AttachmentItem::attIsTempEject() const
{
    return mAttIsTempEject;
}

bool AttachmentItem::attIsNonRotational() const
{
    return mAttIsNonRotational;
}

bool AttachmentItem::attIsHotPluggable() const
{
    return m_fIsHotPluggable;
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
    AssertMsg(!aAttMediumId.isEmpty(), ("Medium ID value can't be null/empty!\n"));
    mAttMediumId = vboxGlobal().medium(aAttMediumId).id();
    cache();
}

void AttachmentItem::setAttIsPassthrough (bool aIsAttPassthrough)
{
    mAttIsPassthrough = aIsAttPassthrough;
}

void AttachmentItem::setAttIsTempEject (bool aIsAttTempEject)
{
    mAttIsTempEject = aIsAttTempEject;
}

void AttachmentItem::setAttIsNonRotational (bool aIsAttNonRotational)
{
    mAttIsNonRotational = aIsAttNonRotational;
}

void AttachmentItem::setAttIsHotPluggable(bool fIsHotPluggable)
{
    m_fIsHotPluggable = fIsHotPluggable;
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

QString AttachmentItem::attDetails() const
{
    return mAttDetails;
}

QString AttachmentItem::attUsage() const
{
    return mAttUsage;
}

QString AttachmentItem::attEncryptionPasswordID() const
{
    return m_strAttEncryptionPasswordID;
}

void AttachmentItem::cache()
{
    UIMedium medium = vboxGlobal().medium(mAttMediumId);

    /* Cache medium information */
    mAttName = medium.name (true);
    mAttTip = medium.toolTipCheckRO (true, mAttDeviceType != KDeviceType_HardDisk);
    mAttPixmap = medium.iconCheckRO (true);
    mAttIsHostDrive = medium.isHostDrive();

    /* Cache additional information */
    mAttSize = medium.size (true);
    mAttLogicalSize = medium.logicalSize (true);
    mAttLocation = medium.location (true);
    m_strAttEncryptionPasswordID = QString("--");
    if (medium.isNull())
    {
        mAttFormat = QString("--");
    }
    else
    {
        switch (mAttDeviceType)
        {
            case KDeviceType_HardDisk:
            {
                mAttFormat = QString("%1 (%2)").arg(medium.hardDiskType(true)).arg(medium.hardDiskFormat(true));
                mAttDetails = medium.storageDetails();
                const QString strAttEncryptionPasswordID = medium.encryptionPasswordID();
                if (!strAttEncryptionPasswordID.isNull())
                    m_strAttEncryptionPasswordID = strAttEncryptionPasswordID;
                break;
            }
            case KDeviceType_DVD:
            case KDeviceType_Floppy:
            {
                mAttFormat = mAttIsHostDrive ? UIMachineSettingsStorage::tr("Host Drive") : UIMachineSettingsStorage::tr("Image", "storage image");
                break;
            }
            default:
                break;
        }
    }
    mAttUsage = medium.usage (true);

    /* Fill empty attributes */
    if (mAttUsage.isEmpty())
        mAttUsage = QString ("--");
}

AbstractItem::ItemType AttachmentItem::rtti() const
{
    return Type_AttachmentItem;
}

AbstractItem* AttachmentItem::childItem (int /* aIndex */) const
{
    return 0;
}

AbstractItem* AttachmentItem::childItemById (const QUuid& /* aId */) const
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
                mAttPixmap = iconPool()->pixmap(HDAttachmentNormal);
                break;
            case KDeviceType_DVD:
                mAttPixmap = iconPool()->pixmap(CDAttachmentNormal);
                break;
            case KDeviceType_Floppy:
                mAttPixmap = iconPool()->pixmap(FDAttachmentNormal);
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
StorageModel::StorageModel(QITreeView *pParent)
    : QAbstractItemModel(pParent)
    , mRootItem(new RootItem(pParent))
    , mToolTipType(DefaultToolTip)
    , m_chipsetType(KChipsetType_PIIX3)
    , m_configurationAccessLevel(ConfigurationAccessLevel_Null)
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
                         static_cast <AbstractItem*> (aParent.internalPointer())->childItem (aRow);

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
                                tip = UIMachineSettingsStorage::tr("<nobr>Expands/Collapses&nbsp;item.</nobr>");
                            break;
                        case HDAdderToolTip:
                            tip = UIMachineSettingsStorage::tr("<nobr>Adds&nbsp;hard&nbsp;disk.</nobr>");
                            break;
                        case CDAdderToolTip:
                            tip = UIMachineSettingsStorage::tr("<nobr>Adds&nbsp;optical&nbsp;drive.</nobr>");
                            break;
                        case FDAdderToolTip:
                            tip = UIMachineSettingsStorage::tr("<nobr>Adds&nbsp;floppy&nbsp;drive.</nobr>");
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
            return (m_configurationAccessLevel == ConfigurationAccessLevel_Full) &&
                   (static_cast<RootItem*>(mRootItem)->childCount(KStorageBus_IDE) <
                    vboxGlobal().virtualBox().GetSystemProperties().GetMaxInstancesOfStorageBus(chipsetType(), KStorageBus_IDE));
        }
        case R_IsMoreSATAControllersPossible:
        {
            return (m_configurationAccessLevel == ConfigurationAccessLevel_Full) &&
                   (static_cast<RootItem*>(mRootItem)->childCount(KStorageBus_SATA) <
                    vboxGlobal().virtualBox().GetSystemProperties().GetMaxInstancesOfStorageBus(chipsetType(), KStorageBus_SATA));
        }
        case R_IsMoreSCSIControllersPossible:
        {
            return (m_configurationAccessLevel == ConfigurationAccessLevel_Full) &&
                   (static_cast<RootItem*>(mRootItem)->childCount(KStorageBus_SCSI) <
                    vboxGlobal().virtualBox().GetSystemProperties().GetMaxInstancesOfStorageBus(chipsetType(), KStorageBus_SCSI));
        }
        case R_IsMoreFloppyControllersPossible:
        {
            return (m_configurationAccessLevel == ConfigurationAccessLevel_Full) &&
                   (static_cast<RootItem*>(mRootItem)->childCount(KStorageBus_Floppy) <
                    vboxGlobal().virtualBox().GetSystemProperties().GetMaxInstancesOfStorageBus(chipsetType(), KStorageBus_Floppy));
        }
        case R_IsMoreSASControllersPossible:
        {
            return (m_configurationAccessLevel == ConfigurationAccessLevel_Full) &&
                   (static_cast<RootItem*>(mRootItem)->childCount(KStorageBus_SAS) <
                    vboxGlobal().virtualBox().GetSystemProperties().GetMaxInstancesOfStorageBus(chipsetType(), KStorageBus_SAS));
        }
        case R_IsMoreUSBControllersPossible:
        {
            return (m_configurationAccessLevel == ConfigurationAccessLevel_Full) &&
                   (static_cast<RootItem*>(mRootItem)->childCount(KStorageBus_USB) <
                    vboxGlobal().virtualBox().GetSystemProperties().GetMaxInstancesOfStorageBus(chipsetType(), KStorageBus_USB));
        }
        case R_IsMoreNVMeControllersPossible:
        {
            return (m_configurationAccessLevel == ConfigurationAccessLevel_Full) &&
                   (static_cast<RootItem*>(mRootItem)->childCount(KStorageBus_PCIe) <
                    vboxGlobal().virtualBox().GetSystemProperties().GetMaxInstancesOfStorageBus(chipsetType(), KStorageBus_PCIe));
        }
        case R_IsMoreAttachmentsPossible:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
            {
                if (item->rtti() == AbstractItem::Type_ControllerItem)
                {
                    ControllerItem *ctr = static_cast <ControllerItem*> (item);
                    CSystemProperties sp = vboxGlobal().virtualBox().GetSystemProperties();
                    const bool fIsMoreAttachmentsPossible = (ULONG)rowCount(aIndex) <
                                                            (sp.GetMaxPortCountForStorageBus(ctr->ctrBusType()) *
                                                             sp.GetMaxDevicesPerPortForStorageBus(ctr->ctrBusType()));
                    if (fIsMoreAttachmentsPossible)
                    {
                        switch (m_configurationAccessLevel)
                        {
                            case ConfigurationAccessLevel_Full:
                                return true;
                            case ConfigurationAccessLevel_Partial_Running:
                            {
                                switch (ctr->ctrBusType())
                                {
                                    case KStorageBus_USB:
                                        return true;
                                    case KStorageBus_SATA:
                                        return (uint)rowCount(aIndex) < ctr->portCount();
                                    default:
                                        break;
                                }
                            }
                            default:
                                break;
                        }
                    }
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
        case R_CtrPortCount:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_ControllerItem)
                    return static_cast <ControllerItem*> (item)->portCount();
            return 0;
        }
        case R_CtrMaxPortCount:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_ControllerItem)
                    return static_cast <ControllerItem*> (item)->maxPortCount();
            return 0;
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
        case R_AttIsTempEject:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                    return static_cast <AttachmentItem*> (item)->attIsTempEject();
            return false;
        }
        case R_AttIsNonRotational:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                    return static_cast <AttachmentItem*> (item)->attIsNonRotational();
            return false;
        }
        case R_AttIsHotPluggable:
        {
            if (AbstractItem *item = static_cast<AbstractItem*>(aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                    return static_cast<AttachmentItem*>(item)->attIsHotPluggable();
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
        case R_AttDetails:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                    return static_cast <AttachmentItem*> (item)->attDetails();
            return QString();
        }
        case R_AttUsage:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                    return static_cast <AttachmentItem*> (item)->attUsage();
            return QString();
        }
        case R_AttEncryptionPasswordID:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(aIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return static_cast<AttachmentItem*>(pItem)->attEncryptionPasswordID();
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
            return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
        }

        case R_HDPixmapEn:
        {
            return iconPool()->pixmap(HDAttachmentNormal);
        }
        case R_CDPixmapEn:
        {
            return iconPool()->pixmap(CDAttachmentNormal);
        }
        case R_FDPixmapEn:
        {
            return iconPool()->pixmap(FDAttachmentNormal);
        }

        case R_HDPixmapAddEn:
        {
            return iconPool()->pixmap(HDAttachmentAddEn);
        }
        case R_HDPixmapAddDis:
        {
            return iconPool()->pixmap(HDAttachmentAddDis);
        }
        case R_CDPixmapAddEn:
        {
            return iconPool()->pixmap(CDAttachmentAddEn);
        }
        case R_CDPixmapAddDis:
        {
            return iconPool()->pixmap(CDAttachmentAddDis);
        }
        case R_FDPixmapAddEn:
        {
            return iconPool()->pixmap(FDAttachmentAddEn);
        }
        case R_FDPixmapAddDis:
        {
            return iconPool()->pixmap(FDAttachmentAddDis);
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
        case R_CtrPortCount:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_ControllerItem)
                {
                    static_cast <ControllerItem*> (item)->setPortCount (aValue.toUInt());
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
        case R_AttIsTempEject:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                {
                    static_cast <AttachmentItem*> (item)->setAttIsTempEject (aValue.toBool());
                    emit dataChanged (aIndex, aIndex);
                    return true;
                }
            return false;
        }
        case R_AttIsNonRotational:
        {
            if (AbstractItem *item = static_cast <AbstractItem*> (aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                {
                    static_cast <AttachmentItem*> (item)->setAttIsNonRotational (aValue.toBool());
                    emit dataChanged (aIndex, aIndex);
                    return true;
                }
            return false;
        }
        case R_AttIsHotPluggable:
        {
            if (AbstractItem *item = static_cast<AbstractItem*>(aIndex.internalPointer()))
                if (item->rtti() == AbstractItem::Type_AttachmentItem)
                {
                    static_cast<AttachmentItem*>(item)->setAttIsHotPluggable(aValue.toBool());
                    emit dataChanged(aIndex, aIndex);
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
    if (AbstractItem *item = mRootItem->childItemById (aCtrId))
    {
        int itemPosition = mRootItem->posOfChild (item);
        beginRemoveRows (root(), itemPosition, itemPosition);
        delete item;
        endRemoveRows();
    }
}

QModelIndex StorageModel::addAttachment (const QUuid &aCtrId, KDeviceType aDeviceType, const QString &strMediumId)
{
    if (AbstractItem *parent = mRootItem->childItemById (aCtrId))
    {
        int parentPosition = mRootItem->posOfChild (parent);
        QModelIndex parentIndex = index (parentPosition, 0, root());
        beginInsertRows (parentIndex, parent->childCount(), parent->childCount());
        AttachmentItem *pItem = new AttachmentItem (parent, aDeviceType);
        pItem->setAttIsHotPluggable(m_configurationAccessLevel != ConfigurationAccessLevel_Full);
        pItem->setAttMediumId(strMediumId);
        endInsertRows();
        return index (parent->childCount() - 1, 0, parentIndex);
    }
    return QModelIndex();
}

void StorageModel::delAttachment (const QUuid &aCtrId, const QUuid &aAttId)
{
    if (AbstractItem *parent = mRootItem->childItemById (aCtrId))
    {
        int parentPosition = mRootItem->posOfChild (parent);
        if (AbstractItem *item = parent->childItemById (aAttId))
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
        AbstractItem *pItemLevel1 = mRootItem->childItem(iItemLevel1Pos);
        ControllerItem *pControllerItem = static_cast<ControllerItem*>(pItemLevel1);
        /* Count of attachment items: */
        int iItemLevel2Count = pItemLevel1->childCount();
        /* Prepare empty list for sorted attachments: */
        QList<AbstractItem*> newAttachments;
        /* For each of attachment items: */
        for (int iItemLevel2Pos = 0; iItemLevel2Pos < iItemLevel2Count; ++iItemLevel2Pos)
        {
            /* Get iterated attachment item: */
            AbstractItem *pItemLevel2 = pItemLevel1->childItem(iItemLevel2Pos);
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

KChipsetType StorageModel::chipsetType() const
{
    return m_chipsetType;
}

void StorageModel::setChipsetType(KChipsetType type)
{
    m_chipsetType = type;
}

void StorageModel::setConfigurationAccessLevel(ConfigurationAccessLevel newConfigurationAccessLevel)
{
    m_configurationAccessLevel = newConfigurationAccessLevel;
}

void StorageModel::clear()
{
    while (mRootItem->childCount())
    {
        beginRemoveRows(root(), 0, 0);
        delete mRootItem->childItem(0);
        endRemoveRows();
    }
}

QMap<KStorageBus, int> StorageModel::currentControllerTypes() const
{
    QMap<KStorageBus, int> currentMap;
    for (int iStorageBusType = KStorageBus_IDE; iStorageBusType <= KStorageBus_USB; ++iStorageBusType)
    {
        currentMap.insert((KStorageBus)iStorageBusType,
                          static_cast<RootItem*>(mRootItem)->childCount((KStorageBus)iStorageBusType));
    }
    return currentMap;
}

QMap<KStorageBus, int> StorageModel::maximumControllerTypes() const
{
    QMap<KStorageBus, int> maximumMap;
    for (int iStorageBusType = KStorageBus_IDE; iStorageBusType <= KStorageBus_USB; ++iStorageBusType)
    {
        maximumMap.insert((KStorageBus)iStorageBusType,
                          vboxGlobal().virtualBox().GetSystemProperties().GetMaxInstancesOfStorageBus(chipsetType(), (KStorageBus)iStorageBusType));
    }
    return maximumMap;
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
 * UI Medium ID Holder.
 * Used for compliance with other storage page widgets
 * which caching and holding corresponding information.
 */
class UIMediumIDHolder : public QObject
{
    Q_OBJECT;

public:

    UIMediumIDHolder(QWidget *pParent) : QObject(pParent) {}

    QString id() const { return m_strId; }
    void setId(const QString &strId) { m_strId = strId; emit sigChanged(); }

    UIMediumType type() const { return m_type; }
    void setType(UIMediumType type) { m_type = type; }

    bool isNull() const { return m_strId == UIMedium().id(); }

signals:

    void sigChanged();

private:

    QString m_strId;
    UIMediumType m_type;
};

/**
 * QWidget class reimplementation.
 * Used as HD Settings widget.
 */
UIMachineSettingsStorage::UIMachineSettingsStorage()
    : mStorageModel(0)
    , mAddCtrAction(0), mDelCtrAction(0)
    , mAddIDECtrAction(0), mAddSATACtrAction(0), mAddSCSICtrAction(0), mAddSASCtrAction(0), mAddFloppyCtrAction(0), mAddUSBCtrAction(0), mAddNVMeCtrAction(0)
    , mAddAttAction(0), mDelAttAction(0)
    , mAddHDAttAction(0), mAddCDAttAction(0), mAddFDAttAction(0)
    , m_pMediumIdHolder(new UIMediumIDHolder(this))
    , mIsLoadingInProgress(0)
    , mIsPolished(false)
    , mDisableStaticControls(0)
    , m_pCache(new UISettingsCacheMachineStorage)
{
    /* Apply UI decorations */
    Ui::UIMachineSettingsStorage::setupUi (this);

    /* Enumerate Mediums. We need at least the MediaList filled, so this is the
     * lasted point, where we can start. The rest of the media checking is done
     * in a background thread. */
    vboxGlobal().startMediumEnumeration();

    /* Create icon-pool: */
    UIIconPoolStorageSettings::create();

    /* Controller Actions */
    mAddCtrAction = new QAction (this);
    mAddCtrAction->setIcon(iconPool()->icon(ControllerAddEn, ControllerAddDis));

    mAddIDECtrAction = new QAction (this);
    mAddIDECtrAction->setIcon(iconPool()->icon(IDEControllerAddEn, IDEControllerAddDis));

    mAddSATACtrAction = new QAction (this);
    mAddSATACtrAction->setIcon(iconPool()->icon(SATAControllerAddEn, SATAControllerAddDis));

    mAddSCSICtrAction = new QAction (this);
    mAddSCSICtrAction->setIcon(iconPool()->icon(SCSIControllerAddEn, SCSIControllerAddDis));

    mAddFloppyCtrAction = new QAction (this);
    mAddFloppyCtrAction->setIcon(iconPool()->icon(FloppyControllerAddEn, FloppyControllerAddDis));

    mAddSASCtrAction = new QAction (this);
    mAddSASCtrAction->setIcon(iconPool()->icon(SATAControllerAddEn, SATAControllerAddDis));

    mAddUSBCtrAction = new QAction (this);
    mAddUSBCtrAction->setIcon(iconPool()->icon(USBControllerAddEn, USBControllerAddDis));

    mAddNVMeCtrAction = new QAction (this);
    mAddNVMeCtrAction->setIcon(iconPool()->icon(NVMeControllerAddEn, NVMeControllerAddDis));

    mDelCtrAction = new QAction (this);
    mDelCtrAction->setIcon(iconPool()->icon(ControllerDelEn, ControllerDelDis));

    /* Attachment Actions */
    mAddAttAction = new QAction (this);
    mAddAttAction->setIcon(iconPool()->icon(AttachmentAddEn, AttachmentAddDis));

    mAddHDAttAction = new QAction (this);
    mAddHDAttAction->setIcon(iconPool()->icon(HDAttachmentAddEn, HDAttachmentAddDis));

    mAddCDAttAction = new QAction (this);
    mAddCDAttAction->setIcon(iconPool()->icon(CDAttachmentAddEn, CDAttachmentAddDis));

    mAddFDAttAction = new QAction (this);
    mAddFDAttAction->setIcon(iconPool()->icon(FDAttachmentAddEn, FDAttachmentAddDis));

    mDelAttAction = new QAction (this);
    mDelAttAction->setIcon(iconPool()->icon(AttachmentDelEn, AttachmentDelDis));

    /* Create storage-view: */
    mTwStorageTree = new QITreeView(this);
    {
        /* Configure storage-view: */
        mLsLeftPane->setBuddy(mTwStorageTree);
        /* Add storage-view into layout: */
        mLtStorage->insertWidget(0, mTwStorageTree);
    }

    /* Create storage-model: */
    mStorageModel = new StorageModel (mTwStorageTree);
    StorageDelegate *storageDelegate = new StorageDelegate (mTwStorageTree);
    mTwStorageTree->setMouseTracking (true);
    mTwStorageTree->setContextMenuPolicy (Qt::CustomContextMenu);
    mTwStorageTree->setModel (mStorageModel);
    mTwStorageTree->setItemDelegate (storageDelegate);
    mTwStorageTree->setRootIndex (mStorageModel->root());
    mTwStorageTree->setCurrentIndex (mStorageModel->root());

    /* Determine icon metric: */
    const QStyle *pStyle = QApplication::style();
    const int iIconMetric = pStyle->pixelMetric(QStyle::PM_SmallIconSize);

    /* Storage ToolBar */
    mTbStorageBar->setIconSize (QSize (iIconMetric, iIconMetric));
    mTbStorageBar->addAction (mAddAttAction);
    mTbStorageBar->addAction (mDelAttAction);
    mTbStorageBar->addAction (mAddCtrAction);
    mTbStorageBar->addAction (mDelCtrAction);

#ifdef VBOX_WS_MAC
    /* We need a little more space for the focus rect. */
    mLtStorage->setContentsMargins (3, 0, 3, 0);
    mLtStorage->setSpacing (3);
#endif /* VBOX_WS_MAC */

    /* Setup choose-medium button: */
    QMenu *pOpenMediumMenu = new QMenu(mTbOpen);
    mTbOpen->setMenu(pOpenMediumMenu);

    /* Controller pane initialization: */
    mSbPortCount->setValue(0);

    /* Info Pane initialization */
    mLbHDFormatValue->setFullSizeSelection (true);
    mLbCDFDTypeValue->setFullSizeSelection (true);
    mLbHDVirtualSizeValue->setFullSizeSelection (true);
    mLbHDActualSizeValue->setFullSizeSelection (true);
    mLbSizeValue->setFullSizeSelection (true);
    mLbHDDetailsValue->setFullSizeSelection (true);
    mLbLocationValue->setFullSizeSelection (true);
    mLbUsageValue->setFullSizeSelection (true);
    m_pLabelEncryptionValue->setFullSizeSelection(true);

    /* Setup connections: */
    connect(&vboxGlobal(), SIGNAL(sigMediumEnumerated(const QString&)),
            this, SLOT(sltHandleMediumEnumerated(const QString&)));
    connect(&vboxGlobal(), SIGNAL(sigMediumDeleted(const QString&)),
            this, SLOT(sltHandleMediumDeleted(const QString&)));
    connect (mAddCtrAction, SIGNAL (triggered (bool)), this, SLOT (addController()));
    connect (mAddIDECtrAction, SIGNAL (triggered (bool)), this, SLOT (addIDEController()));
    connect (mAddSATACtrAction, SIGNAL (triggered (bool)), this, SLOT (addSATAController()));
    connect (mAddSCSICtrAction, SIGNAL (triggered (bool)), this, SLOT (addSCSIController()));
    connect (mAddSASCtrAction, SIGNAL (triggered (bool)), this, SLOT (addSASController()));
    connect (mAddFloppyCtrAction, SIGNAL (triggered (bool)), this, SLOT (addFloppyController()));
    connect (mAddUSBCtrAction, SIGNAL (triggered (bool)), this, SLOT (addUSBController()));
    connect (mAddNVMeCtrAction, SIGNAL (triggered (bool)), this, SLOT (addNVMeController()));
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
    connect (mSbPortCount, SIGNAL (valueChanged (int)), this, SLOT (setInformation()));
    connect (mCbIoCache, SIGNAL (stateChanged (int)), this, SLOT (setInformation()));
    connect (m_pMediumIdHolder, SIGNAL (sigChanged()), this, SLOT (setInformation()));
    connect (mTbOpen, SIGNAL (clicked (bool)), mTbOpen, SLOT (showMenu()));
    connect (pOpenMediumMenu, SIGNAL (aboutToShow()), this, SLOT (sltPrepareOpenMediumMenu()));
    connect (mCbPassthrough, SIGNAL (stateChanged (int)), this, SLOT (setInformation()));
    connect (mCbTempEject, SIGNAL (stateChanged (int)), this, SLOT (setInformation()));
    connect (mCbNonRotational, SIGNAL (stateChanged (int)), this, SLOT (setInformation()));
    connect(m_pCheckBoxHotPluggable, SIGNAL(stateChanged(int)), this, SLOT(setInformation()));

    /* Applying language settings */
    retranslateUi();

    /* Initial setup */
    setMinimumWidth (500);
    mSplitter->setSizes (QList<int>() << (int) (0.45 * minimumWidth()) << (int) (0.55 * minimumWidth()));
}

UIMachineSettingsStorage::~UIMachineSettingsStorage()
{
    /* Destroy icon-pool: */
    UIIconPoolStorageSettings::destroy();

    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

void UIMachineSettingsStorage::setChipsetType(KChipsetType type)
{
    /* Make sure chipset type has changed: */
    if (mStorageModel->chipsetType() == type)
        return;

    /* Update chipset type value: */
    mStorageModel->setChipsetType(type);
    updateActionsState();

    /* Revalidate: */
    revalidate();
}

bool UIMachineSettingsStorage::changed() const
{
    return m_pCache->wasChanged();
}

void UIMachineSettingsStorage::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Gather storage data: */
    m_strMachineId = m_machine.GetId();
    m_strMachineSettingsFilePath = m_machine.GetSettingsFilePath();
    m_strMachineGuestOSTypeId = m_machine.GetOSTypeId();

    /* For each controller: */
    const CStorageControllerVector &controllers = m_machine.GetStorageControllers();
    for (int iControllerIndex = 0; iControllerIndex < controllers.size(); ++iControllerIndex)
    {
        /* Prepare storage controller data: */
        UIDataSettingsMachineStorageController storageControllerData;

        /* Check if controller is valid: */
        const CStorageController &controller = controllers[iControllerIndex];
        if (!controller.isNull())
        {
            /* Gather storage controller data: */
            storageControllerData.m_strControllerName = controller.GetName();
            storageControllerData.m_controllerBus = controller.GetBus();
            storageControllerData.m_controllerType = controller.GetControllerType();
            storageControllerData.m_uPortCount = controller.GetPortCount();
            storageControllerData.m_fUseHostIOCache = controller.GetUseHostIOCache();

            /* Sort attachments before caching/fetching: */
            const CMediumAttachmentVector &attachmentVector =
                    m_machine.GetMediumAttachmentsOfController(storageControllerData.m_strControllerName);
            QMap<StorageSlot, CMediumAttachment> attachmentMap;
            foreach (const CMediumAttachment &attachment, attachmentVector)
            {
                StorageSlot storageSlot(storageControllerData.m_controllerBus,
                                        attachment.GetPort(), attachment.GetDevice());
                attachmentMap.insert(storageSlot, attachment);
            }
            const QList<CMediumAttachment> &attachments = attachmentMap.values();

            /* For each attachment: */
            for (int iAttachmentIndex = 0; iAttachmentIndex < attachments.size(); ++iAttachmentIndex)
            {
                /* Prepare storage attachment data: */
                UIDataSettingsMachineStorageAttachment storageAttachmentData;

                /* Check if attachment is valid: */
                const CMediumAttachment &attachment = attachments[iAttachmentIndex];
                if (!attachment.isNull())
                {
                    /* Gather storage attachment data: */
                    storageAttachmentData.m_attachmentType = attachment.GetType();
                    storageAttachmentData.m_iAttachmentPort = attachment.GetPort();
                    storageAttachmentData.m_iAttachmentDevice = attachment.GetDevice();
                    storageAttachmentData.m_fAttachmentPassthrough = attachment.GetPassthrough();
                    storageAttachmentData.m_fAttachmentTempEject = attachment.GetTemporaryEject();
                    storageAttachmentData.m_fAttachmentNonRotational = attachment.GetNonRotational();
                    storageAttachmentData.m_fAttachmentHotPluggable = attachment.GetHotPluggable();
                    const CMedium cmedium = attachment.GetMedium();
                    storageAttachmentData.m_strAttachmentMediumId = cmedium.isNull() ? UIMedium::nullID() : cmedium.GetId();
                }

                /* Cache storage attachment data: */
                m_pCache->child(iControllerIndex).child(iAttachmentIndex).cacheInitialData(storageAttachmentData);
            }
        }

        /* Cache storage controller data: */
        m_pCache->child(iControllerIndex).cacheInitialData(storageControllerData);
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsStorage::getFromCache()
{
    /* Clear model initially: */
    mStorageModel->clear();

    /* Load storage data to page: */
    mStorageModel->setMachineId(m_strMachineId);

    /* For each storage controller: */
    for (int iControllerIndex = 0; iControllerIndex < m_pCache->childCount(); ++iControllerIndex)
    {
        /* Get storage controller cache: */
        const UISettingsCacheMachineStorageController &controllerCache = m_pCache->child(iControllerIndex);
        /* Get storage controller data from cache: */
        const UIDataSettingsMachineStorageController &controllerData = controllerCache.base();

        /* Load storage controller data to page: */
        QModelIndex controllerIndex = mStorageModel->addController(controllerData.m_strControllerName,
                                                                   controllerData.m_controllerBus,
                                                                   controllerData.m_controllerType);
        QUuid controllerId = QUuid(mStorageModel->data(controllerIndex, StorageModel::R_ItemId).toString());
        mStorageModel->setData(controllerIndex, controllerData.m_uPortCount, StorageModel::R_CtrPortCount);
        mStorageModel->setData(controllerIndex, controllerData.m_fUseHostIOCache, StorageModel::R_CtrIoCache);

        /* For each storage attachment: */
        for (int iAttachmentIndex = 0; iAttachmentIndex < controllerCache.childCount(); ++iAttachmentIndex)
        {
            /* Get storage attachment cache: */
            const UISettingsCacheMachineStorageAttachment &attachmentCache = controllerCache.child(iAttachmentIndex);
            /* Get storage controller data from cache: */
            const UIDataSettingsMachineStorageAttachment &attachmentData = attachmentCache.base();

            /* Load storage attachment data to page: */
            QModelIndex attachmentIndex = mStorageModel->addAttachment(controllerId, attachmentData.m_attachmentType, attachmentData.m_strAttachmentMediumId);
            StorageSlot attachmentStorageSlot(controllerData.m_controllerBus,
                                              attachmentData.m_iAttachmentPort,
                                              attachmentData.m_iAttachmentDevice);
            mStorageModel->setData(attachmentIndex, QVariant::fromValue(attachmentStorageSlot), StorageModel::R_AttSlot);
            mStorageModel->setData(attachmentIndex, attachmentData.m_fAttachmentPassthrough, StorageModel::R_AttIsPassthrough);
            mStorageModel->setData(attachmentIndex, attachmentData.m_fAttachmentTempEject, StorageModel::R_AttIsTempEject);
            mStorageModel->setData(attachmentIndex, attachmentData.m_fAttachmentNonRotational, StorageModel::R_AttIsNonRotational);
            mStorageModel->setData(attachmentIndex, attachmentData.m_fAttachmentHotPluggable, StorageModel::R_AttIsHotPluggable);
        }
    }
    /* Set the first controller as current if present */
    if (mStorageModel->rowCount(mStorageModel->root()) > 0)
        mTwStorageTree->setCurrentIndex(mStorageModel->index(0, 0, mStorageModel->root()));

    /* Update actions: */
    updateActionsState();

    /* Polish page finally: */
    polishPage();

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsStorage::putToCache()
{
    /* Prepare storage data: */
    UIDataSettingsMachineStorage storageData = m_pCache->base();

    /* For each storage controller: */
    QModelIndex rootIndex = mStorageModel->root();
    for (int iControllerIndex = 0; iControllerIndex < mStorageModel->rowCount(rootIndex); ++iControllerIndex)
    {
        /* Prepare storage controller data & key: */
        UIDataSettingsMachineStorageController controllerData;

        /* Gather storage controller data: */
        QModelIndex controllerIndex = mStorageModel->index(iControllerIndex, 0, rootIndex);
        controllerData.m_strControllerName = mStorageModel->data(controllerIndex, StorageModel::R_CtrName).toString();
        controllerData.m_controllerBus = mStorageModel->data(controllerIndex, StorageModel::R_CtrBusType).value<KStorageBus>();
        controllerData.m_controllerType = mStorageModel->data(controllerIndex, StorageModel::R_CtrType).value<KStorageControllerType>();
        controllerData.m_uPortCount = mStorageModel->data(controllerIndex, StorageModel::R_CtrPortCount).toUInt();
        controllerData.m_fUseHostIOCache = mStorageModel->data(controllerIndex, StorageModel::R_CtrIoCache).toBool();

        /* For each storage attachment: */
        for (int iAttachmentIndex = 0; iAttachmentIndex < mStorageModel->rowCount(controllerIndex); ++iAttachmentIndex)
        {
            /* Prepare storage attachment data & key: */
            UIDataSettingsMachineStorageAttachment attachmentData;

            /* Gather storage controller data: */
            QModelIndex attachmentIndex = mStorageModel->index(iAttachmentIndex, 0, controllerIndex);
            attachmentData.m_attachmentType = mStorageModel->data(attachmentIndex, StorageModel::R_AttDevice).value<KDeviceType>();
            StorageSlot attachmentSlot = mStorageModel->data(attachmentIndex, StorageModel::R_AttSlot).value<StorageSlot>();
            attachmentData.m_iAttachmentPort = attachmentSlot.port;
            attachmentData.m_iAttachmentDevice = attachmentSlot.device;
            attachmentData.m_fAttachmentPassthrough = mStorageModel->data(attachmentIndex, StorageModel::R_AttIsPassthrough).toBool();
            attachmentData.m_fAttachmentTempEject = mStorageModel->data(attachmentIndex, StorageModel::R_AttIsTempEject).toBool();
            attachmentData.m_fAttachmentNonRotational = mStorageModel->data(attachmentIndex, StorageModel::R_AttIsNonRotational).toBool();
            attachmentData.m_fAttachmentHotPluggable = mStorageModel->data(attachmentIndex, StorageModel::R_AttIsHotPluggable).toBool();
            attachmentData.m_strAttachmentMediumId = mStorageModel->data(attachmentIndex, StorageModel::R_AttMediumId).toString();

            /* Recache storage attachment data: */
            m_pCache->child(iControllerIndex).child(iAttachmentIndex).cacheCurrentData(attachmentData);
        }

        /* Recache storage controller data: */
        m_pCache->child(iControllerIndex).cacheCurrentData(controllerData);
    }

    /* Recache storage data: */
    m_pCache->cacheCurrentData(storageData);
}

void UIMachineSettingsStorage::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Update storage data, update failing state: */
    setFailed(!updateStorageData());

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

bool UIMachineSettingsStorage::validate(QList<UIValidationMessage> &messages)
{
    /* Pass by default: */
    bool fPass = true;

    /* Prepare message: */
    UIValidationMessage message;

    /* Check controllers for name emptiness & coincidence.
     * Check attachments for the hd presence / uniqueness. */
    QModelIndex rootIndex = mStorageModel->root();
    QMap <QString, QString> config;
    QMap<int, QString> names;
    /* For each controller: */
    for (int i = 0; i < mStorageModel->rowCount (rootIndex); ++ i)
    {
        QModelIndex ctrIndex = rootIndex.child (i, 0);
        QString ctrName = mStorageModel->data (ctrIndex, StorageModel::R_CtrName).toString();

        /* Check for name emptiness: */
        if (ctrName.isEmpty())
        {
            message.second << tr("No name is currently specified for the controller at position <b>%1</b>.").arg(i + 1);
            fPass = false;
        }
        /* Check for name coincidence: */
        if (names.values().contains(ctrName))
        {
            message.second << tr("The controller at position <b>%1</b> has the same name as the controller at position <b>%2</b>.")
                                 .arg(i + 1).arg(names.key(ctrName) + 1);
            fPass = false;
        }
        else
            names.insert(i, ctrName);

        /* For each attachment: */
        for (int j = 0; j < mStorageModel->rowCount (ctrIndex); ++ j)
        {
            QModelIndex attIndex = ctrIndex.child (j, 0);
            StorageSlot attSlot = mStorageModel->data (attIndex, StorageModel::R_AttSlot).value <StorageSlot>();
            KDeviceType attDevice = mStorageModel->data (attIndex, StorageModel::R_AttDevice).value <KDeviceType>();
            QString key (mStorageModel->data (attIndex, StorageModel::R_AttMediumId).toString());
            QString value (QString ("%1 (%2)").arg (ctrName, gpConverter->toString (attSlot)));
            /* Check for emptiness: */
            if (vboxGlobal().medium(key).isNull() && attDevice == KDeviceType_HardDisk)
            {
                message.second << tr("No hard disk is selected for <i>%1</i>.").arg (value);
                fPass = false;
            }
            /* Check for coincidence: */
            if (!vboxGlobal().medium(key).isNull() && config.contains (key))
            {
                message.second << tr("<i>%1</i> is using a disk that is already attached to <i>%2</i>.")
                                     .arg (value).arg (config [key]);
                fPass = false;
            }
            else
                config.insert (key, value);
        }
    }

    /* Check for excessive controllers on Storage page controllers list: */
    QStringList excessiveList;
    QMap<KStorageBus, int> currentType = mStorageModel->currentControllerTypes();
    QMap<KStorageBus, int> maximumType = mStorageModel->maximumControllerTypes();
    for (int iStorageBusType = KStorageBus_IDE; iStorageBusType <= KStorageBus_USB; ++iStorageBusType)
    {
        if (currentType[(KStorageBus)iStorageBusType] > maximumType[(KStorageBus)iStorageBusType])
        {
            QString strExcessiveRecord = QString("%1 (%2)");
            strExcessiveRecord = strExcessiveRecord.arg(QString("<b>%1</b>").arg(gpConverter->toString((KStorageBus)iStorageBusType)));
            strExcessiveRecord = strExcessiveRecord.arg(maximumType[(KStorageBus)iStorageBusType] == 1 ?
                                                        tr("at most one supported", "controller") :
                                                        tr("up to %1 supported", "controllers").arg(maximumType[(KStorageBus)iStorageBusType]));
            excessiveList << strExcessiveRecord;
        }
    }
    if (!excessiveList.isEmpty())
    {
        message.second << tr("The machine currently has more storage controllers assigned than a %1 chipset supports. "
                             "Please change the chipset type on the System settings page or reduce the number "
                             "of the following storage controllers on the Storage settings page: %2")
                             .arg(gpConverter->toString(mStorageModel->chipsetType()))
                             .arg(excessiveList.join(", "));
        fPass = false;
    }

    /* Serialize message: */
    if (!message.second.isEmpty())
        messages << message;

    /* Return result: */
    return fPass;
}

void UIMachineSettingsStorage::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIMachineSettingsStorage::retranslateUi(this);

    /* Translate storage-view: */
    mTwStorageTree->setWhatsThis(tr("Lists all storage controllers for this machine and "
                                    "the virtual images and host drives attached to them."));

    /* Translate tool-bar: */
    mAddCtrAction->setShortcut(QKeySequence("Ins"));
    mDelCtrAction->setShortcut(QKeySequence("Del"));
    mAddAttAction->setShortcut(QKeySequence("+"));
    mDelAttAction->setShortcut(QKeySequence("-"));

    mAddCtrAction->setText(tr("Add Controller"));
    mAddIDECtrAction->setText(tr("Add IDE Controller"));
    mAddSATACtrAction->setText(tr("Add SATA Controller"));
    mAddSCSICtrAction->setText(tr("Add SCSI Controller"));
    mAddSASCtrAction->setText(tr("Add SAS Controller"));
    mAddFloppyCtrAction->setText(tr("Add Floppy Controller"));
    mAddUSBCtrAction->setText(tr("Add USB Controller"));
    mAddNVMeCtrAction->setText(tr("Add NVMe Controller"));
    mDelCtrAction->setText(tr("Remove Controller"));
    mAddAttAction->setText(tr("Add Attachment"));
    mAddHDAttAction->setText(tr("Add Hard Disk"));
    mAddCDAttAction->setText(tr("Add Optical Drive"));
    mAddFDAttAction->setText(tr("Add Floppy Drive"));
    mDelAttAction->setText(tr("Remove Attachment"));

    mAddCtrAction->setWhatsThis(tr("Adds new storage controller."));
    mDelCtrAction->setWhatsThis(tr("Removes selected storage controller."));
    mAddAttAction->setWhatsThis(tr("Adds new storage attachment."));
    mDelAttAction->setWhatsThis(tr("Removes selected storage attachment."));

    mAddCtrAction->setToolTip(mAddCtrAction->whatsThis());
    mDelCtrAction->setToolTip(mDelCtrAction->whatsThis());
    mAddAttAction->setToolTip(mAddAttAction->whatsThis());
    mDelAttAction->setToolTip(mDelAttAction->whatsThis());
}

void UIMachineSettingsStorage::polishPage()
{
    /* Declare required variables: */
    QModelIndex index = mTwStorageTree->currentIndex();
    KDeviceType device = mStorageModel->data(index, StorageModel::R_AttDevice).value<KDeviceType>();

    /* Left pane: */
    mLsLeftPane->setEnabled(isMachineInValidMode());
    mTwStorageTree->setEnabled(isMachineInValidMode());
    /* Empty information pane: */
    mLsEmpty->setEnabled(isMachineInValidMode());
    mLbInfo->setEnabled(isMachineInValidMode());
    /* Controllers pane: */
    mLsParameters->setEnabled(isMachineInValidMode());
    mLbName->setEnabled(isMachineOffline());
    mLeName->setEnabled(isMachineOffline());
    mLbType->setEnabled(isMachineOffline());
    mCbType->setEnabled(isMachineOffline());
    mLbPortCount->setEnabled(isMachineOffline());
    mSbPortCount->setEnabled(isMachineOffline());
    mCbIoCache->setEnabled(isMachineOffline());
    /* Attachments pane: */
    mLsAttributes->setEnabled(isMachineInValidMode());
    mLbMedium->setEnabled(isMachineOffline() || (isMachineOnline() && device != KDeviceType_HardDisk));
    mCbSlot->setEnabled(isMachineOffline());
    mTbOpen->setEnabled(isMachineOffline() || (isMachineOnline() && device != KDeviceType_HardDisk));
    mCbPassthrough->setEnabled(isMachineOffline());
    mCbTempEject->setEnabled(isMachineInValidMode());
    mCbNonRotational->setEnabled(isMachineOffline());
    m_pCheckBoxHotPluggable->setEnabled(isMachineOffline());
    mLsInformation->setEnabled(isMachineInValidMode());
    mLbHDFormat->setEnabled(isMachineInValidMode());
    mLbHDFormatValue->setEnabled(isMachineInValidMode());
    mLbCDFDType->setEnabled(isMachineInValidMode());
    mLbCDFDTypeValue->setEnabled(isMachineInValidMode());
    mLbHDVirtualSize->setEnabled(isMachineInValidMode());
    mLbHDVirtualSizeValue->setEnabled(isMachineInValidMode());
    mLbHDActualSize->setEnabled(isMachineInValidMode());
    mLbHDActualSizeValue->setEnabled(isMachineInValidMode());
    mLbSize->setEnabled(isMachineInValidMode());
    mLbSizeValue->setEnabled(isMachineInValidMode());
    mLbHDDetails->setEnabled(isMachineInValidMode());
    mLbHDDetailsValue->setEnabled(isMachineInValidMode());
    mLbLocation->setEnabled(isMachineInValidMode());
    mLbLocationValue->setEnabled(isMachineInValidMode());
    mLbUsage->setEnabled(isMachineInValidMode());
    mLbUsageValue->setEnabled(isMachineInValidMode());
    m_pLabelEncryption->setEnabled(isMachineInValidMode());
    m_pLabelEncryptionValue->setEnabled(isMachineInValidMode());

    /* Update action states: */
    updateActionsState();
}

void UIMachineSettingsStorage::showEvent(QShowEvent *pEvent)
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
        labelsList << mLbMedium << mLbHDFormat << mLbCDFDType
                   << mLbHDVirtualSize << mLbHDActualSize << mLbSize
                   << mLbLocation << mLbUsage;
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
    UISettingsPageMachine::showEvent(pEvent);
}

void UIMachineSettingsStorage::sltHandleMediumEnumerated(const QString &strMediumID)
{
    /* Search for corresponding medium: */
    UIMedium medium = vboxGlobal().medium(strMediumID);

    QModelIndex rootIndex = mStorageModel->root();
    for (int i = 0; i < mStorageModel->rowCount(rootIndex); ++i)
    {
        QModelIndex ctrIndex = rootIndex.child(i, 0);
        for (int j = 0; j < mStorageModel->rowCount(ctrIndex); ++j)
        {
            QModelIndex attIndex = ctrIndex.child(j, 0);
            QString attMediumId = mStorageModel->data(attIndex, StorageModel::R_AttMediumId).toString();
            if (attMediumId == medium.id())
            {
                mStorageModel->setData(attIndex, attMediumId, StorageModel::R_AttMediumId);

                /* Revalidate: */
                revalidate();
            }
        }
    }
}

void UIMachineSettingsStorage::sltHandleMediumDeleted(const QString &strMediumID)
{
    QModelIndex rootIndex = mStorageModel->root();
    for (int i = 0; i < mStorageModel->rowCount(rootIndex); ++i)
    {
        QModelIndex ctrIndex = rootIndex.child(i, 0);
        for (int j = 0; j < mStorageModel->rowCount(ctrIndex); ++j)
        {
            QModelIndex attIndex = ctrIndex.child(j, 0);
            QString attMediumId = mStorageModel->data(attIndex, StorageModel::R_AttMediumId).toString();
            if (attMediumId == strMediumID)
            {
                mStorageModel->setData(attIndex, UIMedium().id(), StorageModel::R_AttMediumId);

                /* Revalidate: */
                revalidate();
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
    menu.addAction (mAddUSBCtrAction);
    menu.addAction (mAddNVMeCtrAction);
    menu.exec (QCursor::pos());
}

void UIMachineSettingsStorage::addIDEController()
{
    addControllerWrapper (generateUniqueName ("IDE"), KStorageBus_IDE, KStorageControllerType_PIIX4);
}

void UIMachineSettingsStorage::addSATAController()
{
    addControllerWrapper (generateUniqueName ("SATA"), KStorageBus_SATA, KStorageControllerType_IntelAhci);
}

void UIMachineSettingsStorage::addSCSIController()
{
    addControllerWrapper (generateUniqueName ("SCSI"), KStorageBus_SCSI, KStorageControllerType_LsiLogic);
}

void UIMachineSettingsStorage::addFloppyController()
{
    addControllerWrapper (generateUniqueName ("Floppy"), KStorageBus_Floppy, KStorageControllerType_I82078);
}

void UIMachineSettingsStorage::addSASController()
{
    addControllerWrapper (generateUniqueName ("SAS"), KStorageBus_SAS, KStorageControllerType_LsiLogicSas);
}

void UIMachineSettingsStorage::addUSBController()
{
    addControllerWrapper (generateUniqueName ("USB"), KStorageBus_USB, KStorageControllerType_USB);
}

void UIMachineSettingsStorage::addNVMeController()
{
    addControllerWrapper (generateUniqueName ("NVMe"), KStorageBus_PCIe, KStorageControllerType_NVMe);
}

void UIMachineSettingsStorage::delController()
{
    QModelIndex index = mTwStorageTree->currentIndex();
    if (!mStorageModel->data (index, StorageModel::R_IsController).toBool()) return;

    mStorageModel->delController (QUuid (mStorageModel->data (index, StorageModel::R_ItemId).toString()));
    emit storageChanged();

    /* Revalidate: */
    revalidate();
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
        if (!msgCenter().confirmRemovingOfLastDVDDevice(this))
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

    /* Revalidate: */
    revalidate();
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
                    mCbType->insertItem (mCbType->count(), gpConverter->toString (controllerTypeList [i]));
                KStorageControllerType type = mStorageModel->data (index, StorageModel::R_CtrType).value <KStorageControllerType>();
                int ctrPos = mCbType->findText (gpConverter->toString (type));
                mCbType->setCurrentIndex (ctrPos == -1 ? 0 : ctrPos);

                KStorageBus bus = mStorageModel->data (index, StorageModel::R_CtrBusType).value <KStorageBus>();
                mLbPortCount->setVisible (bus == KStorageBus_SATA || bus == KStorageBus_SAS);
                mSbPortCount->setVisible (bus == KStorageBus_SATA || bus == KStorageBus_SAS);
                uint uPortCount = mStorageModel->data (index, StorageModel::R_CtrPortCount).toUInt();
                uint uMaxPortCount = mStorageModel->data (index, StorageModel::R_CtrMaxPortCount).toUInt();
                mSbPortCount->setMaximum(uMaxPortCount);
                mSbPortCount->setValue (uPortCount);

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
                    mCbSlot->insertItem (mCbSlot->count(), gpConverter->toString (slotsList [i]));
                StorageSlot slt = mStorageModel->data (index, StorageModel::R_AttSlot).value <StorageSlot>();
                int attSlotPos = mCbSlot->findText (gpConverter->toString (slt));
                mCbSlot->setCurrentIndex (attSlotPos == -1 ? 0 : attSlotPos);
                mCbSlot->setToolTip (mCbSlot->itemText (mCbSlot->currentIndex()));

                /* Getting Attachment Medium */
                KDeviceType device = mStorageModel->data (index, StorageModel::R_AttDevice).value <KDeviceType>();
                switch (device)
                {
                    case KDeviceType_HardDisk:
                        mLbMedium->setText(tr("Hard &Disk:"));
                        mTbOpen->setIcon(iconPool()->icon(HDAttachmentNormal));
                        mTbOpen->setWhatsThis(tr("Choose or create a virtual hard disk file. The virtual machine will see "
                                                 "the data in the file as the contents of the virtual hard disk."));
                        break;
                    case KDeviceType_DVD:
                        mLbMedium->setText(tr("Optical &Drive:"));
                        mTbOpen->setIcon(iconPool()->icon(CDAttachmentNormal));
                        mTbOpen->setWhatsThis(tr("Choose a virtual optical disk or a physical drive to use with the virtual drive. "
                                                 "The virtual machine will see a disk inserted into the drive with the data "
                                                 "in the file or on the disk in the physical drive as its contents."));
                        break;
                    case KDeviceType_Floppy:
                        mLbMedium->setText(tr("Floppy &Drive:"));
                        mTbOpen->setIcon(iconPool()->icon(FDAttachmentNormal));
                        mTbOpen->setWhatsThis(tr("Choose a virtual floppy disk or a physical drive to use with the virtual drive. "
                                                 "The virtual machine will see a disk inserted into the drive with the data "
                                                 "in the file or on the disk in the physical drive as its contents."));
                        break;
                    default:
                        break;
                }

                /* Get hot-pluggable state: */
                bool fIsHotPluggable = mStorageModel->data(index, StorageModel::R_AttIsHotPluggable).toBool();

                /* Fetch device-type, medium-id: */
                m_pMediumIdHolder->setType(mediumTypeToLocal(device));
                m_pMediumIdHolder->setId(mStorageModel->data(index, StorageModel::R_AttMediumId).toString());

                /* Get/fetch editable state: */
                bool fIsEditable =    (isMachineOffline())
                                   || (isMachineOnline() && device != KDeviceType_HardDisk)
                                   || (isMachineOnline() && device == KDeviceType_HardDisk && fIsHotPluggable);
                mLbMedium->setEnabled(fIsEditable);
                mTbOpen->setEnabled(fIsEditable);

                /* Getting Passthrough state */
                bool isHostDrive = mStorageModel->data (index, StorageModel::R_AttIsHostDrive).toBool();
                mCbPassthrough->setVisible (device == KDeviceType_DVD && isHostDrive);
                mCbPassthrough->setChecked (isHostDrive && mStorageModel->data (index, StorageModel::R_AttIsPassthrough).toBool());

                /* Getting TempEject state */
                mCbTempEject->setVisible (device == KDeviceType_DVD && !isHostDrive);
                mCbTempEject->setChecked (!isHostDrive && mStorageModel->data (index, StorageModel::R_AttIsTempEject).toBool());

                /* Getting NonRotational state */
                mCbNonRotational->setVisible (device == KDeviceType_HardDisk);
                mCbNonRotational->setChecked (mStorageModel->data (index, StorageModel::R_AttIsNonRotational).toBool());

                /* Fetch hot-pluggable state: */
                m_pCheckBoxHotPluggable->setVisible((slt.bus == KStorageBus_SATA) || (slt.bus == KStorageBus_USB));
                m_pCheckBoxHotPluggable->setChecked(fIsHotPluggable);

                /* Update optional widgets visibility */
                updateAdditionalObjects (device);

                /* Getting Other Information */
                mLbHDFormatValue->setText (compressText (mStorageModel->data (index, StorageModel::R_AttFormat).toString()));
                mLbCDFDTypeValue->setText (compressText (mStorageModel->data (index, StorageModel::R_AttFormat).toString()));
                mLbHDVirtualSizeValue->setText (compressText (mStorageModel->data (index, StorageModel::R_AttLogicalSize).toString()));
                mLbHDActualSizeValue->setText (compressText (mStorageModel->data (index, StorageModel::R_AttSize).toString()));
                mLbSizeValue->setText (compressText (mStorageModel->data (index, StorageModel::R_AttSize).toString()));
                mLbHDDetailsValue->setText (compressText (mStorageModel->data (index, StorageModel::R_AttDetails).toString()));
                mLbLocationValue->setText (compressText (mStorageModel->data (index, StorageModel::R_AttLocation).toString()));
                mLbUsageValue->setText (compressText (mStorageModel->data (index, StorageModel::R_AttUsage).toString()));
                m_pLabelEncryptionValue->setText(compressText(mStorageModel->data(index, StorageModel::R_AttEncryptionPasswordID).toString()));

                /* Showing Attachment Page */
                mSwRightPane->setCurrentIndex (2);
                break;
            }
            default:
                break;
        }
    }

    /* Revalidate: */
    revalidate();

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
                mStorageModel->setData (index, QVariant::fromValue (gpConverter->fromString<KStorageControllerType> (mCbType->currentText())),
                                        StorageModel::R_CtrType);
            else if (sdr == mSbPortCount)
                mStorageModel->setData (index, mSbPortCount->value(), StorageModel::R_CtrPortCount);
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
                StorageSlot attachmentStorageSlot = gpConverter->fromString<StorageSlot>(mCbSlot->currentText());
                mStorageModel->setData(index, QVariant::fromValue(attachmentStorageSlot), StorageModel::R_AttSlot);
                QModelIndex theSameIndexAtNewPosition = mStorageModel->attachmentBySlot(controllerIndex, attachmentStorageSlot);
                AssertMsg(theSameIndexAtNewPosition.isValid(), ("Current attachment disappears!\n"));
                mTwStorageTree->setCurrentIndex(theSameIndexAtNewPosition);
            }
            /* Setting Attachment Medium */
            else if (sdr == m_pMediumIdHolder)
                mStorageModel->setData (index, m_pMediumIdHolder->id(), StorageModel::R_AttMediumId);
            else if (sdr == mCbPassthrough)
            {
                if (mStorageModel->data (index, StorageModel::R_AttIsHostDrive).toBool())
                    mStorageModel->setData (index, mCbPassthrough->isChecked(), StorageModel::R_AttIsPassthrough);
            }
            else if (sdr == mCbTempEject)
            {
                if (!mStorageModel->data (index, StorageModel::R_AttIsHostDrive).toBool())
                    mStorageModel->setData (index, mCbTempEject->isChecked(), StorageModel::R_AttIsTempEject);
            }
            else if (sdr == mCbNonRotational)
            {
                mStorageModel->setData (index, mCbNonRotational->isChecked(), StorageModel::R_AttIsNonRotational);
            }
            else if (sdr == m_pCheckBoxHotPluggable)
            {
                mStorageModel->setData(index, m_pCheckBoxHotPluggable->isChecked(), StorageModel::R_AttIsHotPluggable);
            }
            break;
        }
        default:
            break;
    }

    emit storageChanged();
    getInformation();
}

void UIMachineSettingsStorage::sltPrepareOpenMediumMenu()
{
    /* This slot should be called only by open-medium menu: */
    QMenu *pOpenMediumMenu = qobject_cast<QMenu*>(sender());
    AssertMsg(pOpenMediumMenu, ("Can't access open-medium menu!\n"));
    if (pOpenMediumMenu)
    {
        /* Eraze menu initially: */
        pOpenMediumMenu->clear();
        /* Depending on current medium type: */
        switch (m_pMediumIdHolder->type())
        {
            case UIMediumType_HardDisk:
            {
                /* Add "Create a new virtual hard disk" action: */
                QAction *pCreateNewHardDisk = pOpenMediumMenu->addAction(tr("Create New Hard Disk..."));
                pCreateNewHardDisk->setIcon(iconPool()->icon(HDNewEn, HDNewDis));
                connect(pCreateNewHardDisk, SIGNAL(triggered(bool)), this, SLOT(sltCreateNewHardDisk()));
                /* Add "Choose a virtual hard disk file" action: */
                addChooseExistingMediumAction(pOpenMediumMenu, tr("Choose Virtual Hard Disk File..."));
                /* Add recent mediums list: */
                addRecentMediumActions(pOpenMediumMenu, m_pMediumIdHolder->type());
                break;
            }
            case UIMediumType_DVD:
            {
                /* Add "Choose a virtual optical disk file" action: */
                addChooseExistingMediumAction(pOpenMediumMenu, tr("Choose Virtual Optical Disk File..."));
                /* Add "Choose a physical drive" actions: */
                addChooseHostDriveActions(pOpenMediumMenu);
                /* Add recent mediums list: */
                addRecentMediumActions(pOpenMediumMenu, m_pMediumIdHolder->type());
                /* Add "Eject current medium" action: */
                pOpenMediumMenu->addSeparator();
                QAction *pEjectCurrentMedium = pOpenMediumMenu->addAction(tr("Remove Disk from Virtual Drive"));
                pEjectCurrentMedium->setEnabled(!m_pMediumIdHolder->isNull());
                pEjectCurrentMedium->setIcon(iconPool()->icon(CDUnmountEnabled, CDUnmountDisabled));
                connect(pEjectCurrentMedium, SIGNAL(triggered(bool)), this, SLOT(sltUnmountDevice()));
                break;
            }
            case UIMediumType_Floppy:
            {
                /* Add "Choose a virtual floppy disk file" action: */
                addChooseExistingMediumAction(pOpenMediumMenu, tr("Choose Virtual Floppy Disk File..."));
                /* Add "Choose a physical drive" actions: */
                addChooseHostDriveActions(pOpenMediumMenu);
                /* Add recent mediums list: */
                addRecentMediumActions(pOpenMediumMenu, m_pMediumIdHolder->type());
                /* Add "Eject current medium" action: */
                pOpenMediumMenu->addSeparator();
                QAction *pEjectCurrentMedium = pOpenMediumMenu->addAction(tr("Remove Disk from Virtual Drive"));
                pEjectCurrentMedium->setEnabled(!m_pMediumIdHolder->isNull());
                pEjectCurrentMedium->setIcon(iconPool()->icon(FDUnmountEnabled, FDUnmountDisabled));
                connect(pEjectCurrentMedium, SIGNAL(triggered(bool)), this, SLOT(sltUnmountDevice()));
                break;
            }
            default:
                break;
        }
    }
}

void UIMachineSettingsStorage::sltCreateNewHardDisk()
{
    QString strMediumId = getWithNewHDWizard();
    if (!strMediumId.isNull())
        m_pMediumIdHolder->setId(strMediumId);
}

void UIMachineSettingsStorage::sltUnmountDevice()
{
    m_pMediumIdHolder->setId(UIMedium().id());
}

void UIMachineSettingsStorage::sltChooseExistingMedium()
{
    QString strMachineFolder(QFileInfo(m_strMachineSettingsFilePath).absolutePath());
    QString strMediumId = vboxGlobal().openMediumWithFileOpenDialog(m_pMediumIdHolder->type(), this, strMachineFolder);
    if (!strMediumId.isNull())
        m_pMediumIdHolder->setId(strMediumId);
}

void UIMachineSettingsStorage::sltChooseHostDrive()
{
    /* This slot should be called ONLY by choose-host-drive action: */
    QAction *pChooseHostDriveAction = qobject_cast<QAction*>(sender());
    AssertMsg(pChooseHostDriveAction, ("Can't access choose-host-drive action!\n"));
    if (pChooseHostDriveAction)
        m_pMediumIdHolder->setId(pChooseHostDriveAction->data().toString());
}

void UIMachineSettingsStorage::sltChooseRecentMedium()
{
    /* This slot should be called ONLY by choose-recent-medium action: */
    QAction *pChooseRecentMediumAction = qobject_cast<QAction*>(sender());
    AssertMsg(pChooseRecentMediumAction, ("Can't access choose-recent-medium action!\n"));
    if (pChooseRecentMediumAction)
    {
        /* Get recent medium type & name: */
        QStringList mediumInfoList = pChooseRecentMediumAction->data().toString().split(',');
        UIMediumType mediumType = (UIMediumType)mediumInfoList[0].toUInt();
        QString strMediumLocation = mediumInfoList[1];
        QString strMediumId = vboxGlobal().openMedium(mediumType, strMediumLocation, this);
        if (!strMediumId.isNull())
            m_pMediumIdHolder->setId(strMediumId);
    }
}

void UIMachineSettingsStorage::updateActionsState()
{
    QModelIndex index = mTwStorageTree->currentIndex();

    bool isIDEPossible = mStorageModel->data (index, StorageModel::R_IsMoreIDEControllersPossible).toBool();
    bool isSATAPossible = mStorageModel->data (index, StorageModel::R_IsMoreSATAControllersPossible).toBool();
    bool isSCSIPossible = mStorageModel->data (index, StorageModel::R_IsMoreSCSIControllersPossible).toBool();
    bool isFloppyPossible = mStorageModel->data (index, StorageModel::R_IsMoreFloppyControllersPossible).toBool();
    bool isSASPossible = mStorageModel->data (index, StorageModel::R_IsMoreSASControllersPossible).toBool();
    bool isUSBPossible = mStorageModel->data (index, StorageModel::R_IsMoreUSBControllersPossible).toBool();
    bool isNVMePossible = mStorageModel->data (index, StorageModel::R_IsMoreNVMeControllersPossible).toBool();

    bool isController = mStorageModel->data (index, StorageModel::R_IsController).toBool();
    bool isAttachment = mStorageModel->data (index, StorageModel::R_IsAttachment).toBool();
    bool isAttachmentsPossible = mStorageModel->data (index, StorageModel::R_IsMoreAttachmentsPossible).toBool();
    bool fIsAttachmentHotPluggable = mStorageModel->data(index, StorageModel::R_AttIsHotPluggable).toBool();

    /* Configure "add controller" actions: */
    mAddCtrAction->setEnabled (isIDEPossible || isSATAPossible || isSCSIPossible || isFloppyPossible || isSASPossible || isUSBPossible || isNVMePossible);
    mAddIDECtrAction->setEnabled (isIDEPossible);
    mAddSATACtrAction->setEnabled (isSATAPossible);
    mAddSCSICtrAction->setEnabled (isSCSIPossible);
    mAddFloppyCtrAction->setEnabled (isFloppyPossible);
    mAddSASCtrAction->setEnabled (isSASPossible);
    mAddUSBCtrAction->setEnabled (isUSBPossible);
    mAddNVMeCtrAction->setEnabled (isNVMePossible);

    /* Configure "add attachment" actions: */
    mAddAttAction->setEnabled (isController && isAttachmentsPossible);
    mAddHDAttAction->setEnabled (isController && isAttachmentsPossible);
    mAddCDAttAction->setEnabled (isController && isAttachmentsPossible);
    mAddFDAttAction->setEnabled (isController && isAttachmentsPossible);

    /* Configure "delete controller" action: */
    bool fControllerInSuitableState = isMachineOffline();
    mDelCtrAction->setEnabled(isController && fControllerInSuitableState);

    /* Configure "delete attachment" action: */
    bool fAttachmentInSuitableState = isMachineOffline() ||
                                      (isMachineOnline() && fIsAttachmentHotPluggable);
    mDelAttAction->setEnabled(isAttachment && fAttachmentInSuitableState);
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
#ifdef RT_STRICT
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
        case KStorageBus_USB:
            Assert (mStorageModel->data (index, StorageModel::R_IsMoreUSBControllersPossible).toBool());
            break;
        case KStorageBus_PCIe:
            Assert (mStorageModel->data (index, StorageModel::R_IsMoreNVMeControllersPossible).toBool());
            break;
        default:
            break;
    }
#endif

    mStorageModel->addController (aName, aBus, aType);
    emit storageChanged();
}

void UIMachineSettingsStorage::addAttachmentWrapper(KDeviceType deviceType)
{
    QModelIndex index = mTwStorageTree->currentIndex();
    Assert(mStorageModel->data(index, StorageModel::R_IsController).toBool());
    Assert(mStorageModel->data(index, StorageModel::R_IsMoreAttachmentsPossible).toBool());
    QString strControllerName(mStorageModel->data(index, StorageModel::R_CtrName).toString());
    QString strMachineFolder(QFileInfo(m_strMachineSettingsFilePath).absolutePath());

    QString strMediumId;
    switch (deviceType)
    {
        case KDeviceType_HardDisk:
        {
            int iAnswer = msgCenter().confirmHardDiskAttachmentCreation(strControllerName, this);
            if (iAnswer == AlertButton_Choice1)
                strMediumId = getWithNewHDWizard();
            else if (iAnswer == AlertButton_Choice2)
                strMediumId = vboxGlobal().openMediumWithFileOpenDialog(UIMediumType_HardDisk, this, strMachineFolder);
            break;
        }
        case KDeviceType_DVD:
        {
            int iAnswer = msgCenter().confirmOpticalAttachmentCreation(strControllerName, this);
            if (iAnswer == AlertButton_Choice1)
                strMediumId = vboxGlobal().medium(strMediumId).id();
            else if (iAnswer == AlertButton_Choice2)
                strMediumId = vboxGlobal().openMediumWithFileOpenDialog(UIMediumType_DVD, this, strMachineFolder);
            break;
        }
        case KDeviceType_Floppy:
        {
            int iAnswer = msgCenter().confirmFloppyAttachmentCreation(strControllerName, this);
            if (iAnswer == AlertButton_Choice1)
                strMediumId = vboxGlobal().medium(strMediumId).id();
            else if (iAnswer == AlertButton_Choice2)
                strMediumId = vboxGlobal().openMediumWithFileOpenDialog(UIMediumType_Floppy, this, strMachineFolder);
            break;
        }
        default: break; /* Shut up, MSC! */
    }

    if (!strMediumId.isEmpty())
    {
        mStorageModel->addAttachment(QUuid(mStorageModel->data(index, StorageModel::R_ItemId).toString()), deviceType, strMediumId);
        mStorageModel->sort();
        emit storageChanged();

        /* Revalidate: */
        revalidate();
    }
}

QString UIMachineSettingsStorage::getWithNewHDWizard()
{
    /* Initialize variables: */
    CGuestOSType guestOSType = vboxGlobal().virtualBox().GetGuestOSType(m_strMachineGuestOSTypeId);
    QFileInfo fileInfo(m_strMachineSettingsFilePath);
    /* Show New VD wizard: */
    UISafePointerWizardNewVD pWizard = new UIWizardNewVD(this, QString(), fileInfo.absolutePath(), guestOSType.GetRecommendedHDD());
    pWizard->prepare();
    QString strResult = pWizard->exec() == QDialog::Accepted ? pWizard->virtualDisk().GetId() : QString();
    if (pWizard)
        delete pWizard;
    return strResult;
}

void UIMachineSettingsStorage::updateAdditionalObjects (KDeviceType aType)
{
    mLbHDFormat->setVisible (aType == KDeviceType_HardDisk);
    mLbHDFormatValue->setVisible (aType == KDeviceType_HardDisk);

    mLbCDFDType->setVisible (aType != KDeviceType_HardDisk);
    mLbCDFDTypeValue->setVisible (aType != KDeviceType_HardDisk);

    mLbHDVirtualSize->setVisible (aType == KDeviceType_HardDisk);
    mLbHDVirtualSizeValue->setVisible (aType == KDeviceType_HardDisk);

    mLbHDActualSize->setVisible (aType == KDeviceType_HardDisk);
    mLbHDActualSizeValue->setVisible (aType == KDeviceType_HardDisk);

    mLbSize->setVisible (aType != KDeviceType_HardDisk);
    mLbSizeValue->setVisible (aType != KDeviceType_HardDisk);

    mLbHDDetails->setVisible (aType == KDeviceType_HardDisk);
    mLbHDDetailsValue->setVisible (aType == KDeviceType_HardDisk);

    m_pLabelEncryption->setVisible(aType == KDeviceType_HardDisk);
    m_pLabelEncryptionValue->setVisible(aType == KDeviceType_HardDisk);
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

void UIMachineSettingsStorage::addChooseExistingMediumAction(QMenu *pOpenMediumMenu, const QString &strActionName)
{
    QAction *pChooseExistingMedium = pOpenMediumMenu->addAction(strActionName);
    pChooseExistingMedium->setIcon(iconPool()->icon(ChooseExistingEn, ChooseExistingDis));
    connect(pChooseExistingMedium, SIGNAL(triggered(bool)), this, SLOT(sltChooseExistingMedium()));
}

void UIMachineSettingsStorage::addChooseHostDriveActions(QMenu *pOpenMediumMenu)
{
    foreach (const QString &strMediumID, vboxGlobal().mediumIDs())
    {
        const UIMedium medium = vboxGlobal().medium(strMediumID);
        if (medium.isHostDrive() && m_pMediumIdHolder->type() == medium.type())
        {
            QAction *pHostDriveAction = pOpenMediumMenu->addAction(medium.name());
            pHostDriveAction->setData(medium.id());
            connect(pHostDriveAction, SIGNAL(triggered(bool)), this, SLOT(sltChooseHostDrive()));
        }
    }
}

void UIMachineSettingsStorage::addRecentMediumActions(QMenu *pOpenMediumMenu, UIMediumType recentMediumType)
{
    /* Get recent-medium list: */
    QStringList recentMediumList;
    switch (recentMediumType)
    {
        case UIMediumType_HardDisk: recentMediumList = gEDataManager->recentListOfHardDrives(); break;
        case UIMediumType_DVD:      recentMediumList = gEDataManager->recentListOfOpticalDisks(); break;
        case UIMediumType_Floppy:   recentMediumList = gEDataManager->recentListOfFloppyDisks(); break;
        default: break;
    }
    /* For every list-item: */
    for (int index = 0; index < recentMediumList.size(); ++index)
    {
        /* Prepare corresponding action: */
        QString strRecentMediumLocation = recentMediumList[index];
        if (QFile::exists(strRecentMediumLocation))
        {
            QAction *pChooseRecentMediumAction = pOpenMediumMenu->addAction(QFileInfo(strRecentMediumLocation).fileName(),
                                                                            this, SLOT(sltChooseRecentMedium()));
            pChooseRecentMediumAction->setData(QString("%1,%2").arg(recentMediumType).arg(strRecentMediumLocation));
        }
    }
}

bool UIMachineSettingsStorage::updateStorageData()
{
    /* Prepare result: */
    bool fSuccess = m_machine.isOk();
    if (fSuccess)
    {
        /* Check if storage data was changed: */
        if (m_pCache->wasChanged())
        {
            /* For each controller (removing step): */
            for (int iControllerIndex = 0; fSuccess && iControllerIndex < m_pCache->childCount(); ++iControllerIndex)
            {
                /* Get controller cache: */
                const UISettingsCacheMachineStorageController &controllerCache = m_pCache->child(iControllerIndex);

                /* Remove controllers marked for 'remove' and 'update' (if they can't be updated): */
                if (controllerCache.wasRemoved() || (controllerCache.wasUpdated() && !isControllerCouldBeUpdated(controllerCache)))
                    fSuccess = removeStorageController(controllerCache);

                else

                /* Update controllers marked for 'update' (if they can be updated): */
                if (controllerCache.wasUpdated() && isControllerCouldBeUpdated(controllerCache))
                {
                    /* For each attachment (removing step): */
                    for (int iAttachmentIndex = 0; fSuccess && iAttachmentIndex < controllerCache.childCount(); ++iAttachmentIndex)
                    {
                        /* Get attachment cache: */
                        const UISettingsCacheMachineStorageAttachment &attachmentCache = controllerCache.child(iAttachmentIndex);

                        /* Remove attachments marked for 'remove' and 'update' (if they can't be updated): */
                        if (attachmentCache.wasRemoved() || (attachmentCache.wasUpdated() && !isAttachmentCouldBeUpdated(attachmentCache)))
                            fSuccess = removeStorageAttachment(controllerCache, attachmentCache);
                    }
                }
            }

            /* For each controller (creating step): */
            for (int iControllerIndex = 0; fSuccess && iControllerIndex < m_pCache->childCount(); ++iControllerIndex)
            {
                /* Get controller cache: */
                const UISettingsCacheMachineStorageController &controllerCache = m_pCache->child(iControllerIndex);

                /* Create controllers marked for 'create' or 'update' (if they can't be updated): */
                if (controllerCache.wasCreated() || (controllerCache.wasUpdated() && !isControllerCouldBeUpdated(controllerCache)))
                    fSuccess = createStorageController(controllerCache);

                else

                /* Update controllers marked for 'update' (if they can be updated): */
                if (controllerCache.wasUpdated() && isControllerCouldBeUpdated(controllerCache))
                    fSuccess = updateStorageController(controllerCache);
            }
        }
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsStorage::removeStorageController(const UISettingsCacheMachineStorageController &controllerCache)
{
    /* Prepare result: */
    bool fSuccess = m_machine.isOk();
    if (fSuccess)
    {
        /* Get storage controller data form cache: */
        const UIDataSettingsMachineStorageController &controllerData = controllerCache.base();

        /* Get storage controller name: */
        QString strControllerName = controllerData.m_strControllerName;

        /* Check that storage controller exists: */
        const CStorageController &controller = m_machine.GetStorageControllerByName(strControllerName);
        /* Check that machine is OK: */
        fSuccess = m_machine.isOk();
        /* If controller exists: */
        if (fSuccess && !controller.isNull())
        {
            /*remove controller with all the attachments at one shot*/
            m_machine.RemoveStorageController(strControllerName);
            /* Check that machine is OK: */
            fSuccess = m_machine.isOk();
        }
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsStorage::createStorageController(const UISettingsCacheMachineStorageController &controllerCache)
{
    /* Prepare result: */
    bool fSuccess = m_machine.isOk();
    if (fSuccess)
    {
        /* Get controller data form cache: */
        const UIDataSettingsMachineStorageController &controllerData = controllerCache.data();

        /* Get storage controller attributes: */
        QString strControllerName = controllerData.m_strControllerName;
        KStorageBus controllerBus = controllerData.m_controllerBus;
        KStorageControllerType controllerType = controllerData.m_controllerType;
        ULONG uPortCount = controllerData.m_uPortCount;
        bool fUseHostIOCache = controllerData.m_fUseHostIOCache;

        /* Check that storage controller doesn't exists: */
        CStorageController controller = m_machine.GetStorageControllerByName(strControllerName);
        /* Check that machine is not OK: */
        fSuccess = !m_machine.isOk();
        /* If controller doesn't exists: */
        if (fSuccess && controller.isNull())
        {
            /* Create new storage controller: */
            controller = m_machine.AddStorageController(strControllerName, controllerBus);
            /* Check that machine is OK: */
            fSuccess = m_machine.isOk();
            /* If controller exists: */
            if (fSuccess && !controller.isNull())
            {
                /* Set storage controller attributes: */
                controller.SetControllerType(controllerType);
                controller.SetUseHostIOCache(fUseHostIOCache);
                if (controllerBus == KStorageBus_SATA || controllerBus == KStorageBus_SAS || controllerBus == KStorageBus_PCIe)
                {
                    uPortCount = qMax(uPortCount, controller.GetMinPortCount());
                    uPortCount = qMin(uPortCount, controller.GetMaxPortCount());
                    controller.SetPortCount(uPortCount);
                }
                /* For each storage attachment: */
                for (int iAttachmentIndex = 0; fSuccess && iAttachmentIndex < controllerCache.childCount(); ++iAttachmentIndex)
                {
                    /* Get storage attachment cache: */
                    const UISettingsCacheMachineStorageAttachment &attachmentCache = controllerCache.child(iAttachmentIndex);

                    /* Create attachment if it was not just 'removed': */
                    if (!attachmentCache.wasRemoved())
                        fSuccess = createStorageAttachment(controllerCache, attachmentCache);
                }
            }
        }
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsStorage::updateStorageController(const UISettingsCacheMachineStorageController &controllerCache)
{
    /* Prepare result: */
    bool fSuccess = m_machine.isOk();
    if (fSuccess)
    {
        /* Get controller data form cache: */
        const UIDataSettingsMachineStorageController &controllerData = controllerCache.data();

        /* Get storage controller attributes: */
        QString strControllerName = controllerData.m_strControllerName;
        KStorageBus controllerBus = controllerData.m_controllerBus;
        KStorageControllerType controllerType = controllerData.m_controllerType;
        ULONG uPortCount = controllerData.m_uPortCount;
        bool fUseHostIOCache = controllerData.m_fUseHostIOCache;

        /* Check that controller exists: */
        CStorageController controller = m_machine.GetStorageControllerByName(strControllerName);
        /* Check that machine is OK: */
        fSuccess = m_machine.isOk();
        /* If controller exists: */
        if (fSuccess && !controller.isNull())
        {
            /* Set storage controller attributes: */
            controller.SetControllerType(controllerType);
            controller.SetUseHostIOCache(fUseHostIOCache);
            if (controllerBus == KStorageBus_SATA || controllerBus == KStorageBus_SAS)
            {
                uPortCount = qMax(uPortCount, controller.GetMinPortCount());
                uPortCount = qMin(uPortCount, controller.GetMaxPortCount());
                controller.SetPortCount(uPortCount);
            }
            /* For each storage attachment: */
            for (int iAttachmentIndex = 0; fSuccess && iAttachmentIndex < controllerCache.childCount(); ++iAttachmentIndex)
            {
                /* Get storage attachment cache: */
                const UISettingsCacheMachineStorageAttachment &attachmentCache = controllerCache.child(iAttachmentIndex);

                /* Create attachments marked for 'create' and 'update' (if they can't be updated): */
                if (attachmentCache.wasCreated() || (attachmentCache.wasUpdated() && !isAttachmentCouldBeUpdated(attachmentCache)))
                    fSuccess = createStorageAttachment(controllerCache, attachmentCache);

                else

                /* Update attachments marked for 'update' (if they can be updated): */
                if (attachmentCache.wasUpdated() && isAttachmentCouldBeUpdated(attachmentCache))
                    fSuccess = updateStorageAttachment(controllerCache, attachmentCache);
            }
        }
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsStorage::removeStorageAttachment(const UISettingsCacheMachineStorageController &controllerCache,
                                                       const UISettingsCacheMachineStorageAttachment &attachmentCache)
{
    /* Prepare result: */
    bool fSuccess = m_machine.isOk();
    if (fSuccess)
    {
        /* Get storage controller data from cache: */
        const UIDataSettingsMachineStorageController &controllerData = controllerCache.base();
        /* Get storage attachment data from cache: */
        const UIDataSettingsMachineStorageAttachment &attachmentData = attachmentCache.base();

        /* Get storage controller attributes: */
        QString strControllerName = controllerData.m_strControllerName;
        /* Get storage attachment attributes: */
        int iAttachmentPort = attachmentData.m_iAttachmentPort;
        int iAttachmentDevice = attachmentData.m_iAttachmentDevice;

        /* Check that storage attachment exists: */
        const CMediumAttachment &attachment = m_machine.GetMediumAttachment(strControllerName, iAttachmentPort, iAttachmentDevice);
        /* Check that machine is OK: */
        fSuccess = m_machine.isOk();
        /* If attachment exists: */
        if (fSuccess && !attachment.isNull())
        {
            /* Remove storage attachment: */
            m_machine.DetachDevice(strControllerName, iAttachmentPort, iAttachmentDevice);
            /* Check that machine is OK: */
            fSuccess = m_machine.isOk();
        }
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsStorage::createStorageAttachment(const UISettingsCacheMachineStorageController &controllerCache,
                                                       const UISettingsCacheMachineStorageAttachment &attachmentCache)
{
    /* Prepare result: */
    bool fSuccess = m_machine.isOk();
    if (fSuccess)
    {
        /* Get storage controller data from cache: */
        const UIDataSettingsMachineStorageController &controllerData = controllerCache.data();
        /* Get storage attachment data from cache: */
        const UIDataSettingsMachineStorageAttachment &attachmentData = attachmentCache.data();

        /* Get storage controller attributes: */
        QString strControllerName = controllerData.m_strControllerName;
        KStorageBus controllerBus = controllerData.m_controllerBus;
        /* Get storage attachment attributes: */
        int iAttachmentPort = attachmentData.m_iAttachmentPort;
        int iAttachmentDevice = attachmentData.m_iAttachmentDevice;
        KDeviceType attachmentDeviceType = attachmentData.m_attachmentType;
        QString strAttachmentMediumId = attachmentData.m_strAttachmentMediumId;
        bool fAttachmentPassthrough = attachmentData.m_fAttachmentPassthrough;
        bool fAttachmentTempEject = attachmentData.m_fAttachmentTempEject;
        bool fAttachmentNonRotational = attachmentData.m_fAttachmentNonRotational;
        bool fAttachmentHotPluggable = attachmentData.m_fAttachmentHotPluggable;
        /* Get GUI medium object: */
        UIMedium vboxMedium = vboxGlobal().medium(strAttachmentMediumId);
        /* Get COM medium object: */
        CMedium comMedium = vboxMedium.medium();

        /* Check that storage attachment doesn't exists: */
        const CMediumAttachment &attachment = m_machine.GetMediumAttachment(strControllerName, iAttachmentPort, iAttachmentDevice);
        /* Check that machine is not OK: */
        fSuccess = !m_machine.isOk();
        /* If attachment doesn't exists: */
        if (fSuccess && attachment.isNull())
        {
            /* Create storage attachment: */
            m_machine.AttachDevice(strControllerName, iAttachmentPort, iAttachmentDevice, attachmentDeviceType, comMedium);
            /* Check that machine is OK: */
            fSuccess = m_machine.isOk();
            if (fSuccess)
            {
                if (attachmentDeviceType == KDeviceType_DVD)
                {
                    if (fSuccess && isMachineOffline())
                    {
                        m_machine.PassthroughDevice(strControllerName, iAttachmentPort, iAttachmentDevice, fAttachmentPassthrough);
                        /* Check that machine is OK: */
                        fSuccess = m_machine.isOk();
                    }
                    if (fSuccess && isMachineInValidMode())
                    {
                        m_machine.TemporaryEjectDevice(strControllerName, iAttachmentPort, iAttachmentDevice, fAttachmentTempEject);
                        /* Check that machine is OK: */
                        fSuccess = m_machine.isOk();
                    }
                }
                else if (attachmentDeviceType == KDeviceType_HardDisk)
                {
                    if (fSuccess && isMachineOffline())
                    {
                        m_machine.NonRotationalDevice(strControllerName, iAttachmentPort, iAttachmentDevice, fAttachmentNonRotational);
                        /* Check that machine is OK: */
                        fSuccess = m_machine.isOk();
                    }
                }
                if ((controllerBus == KStorageBus_SATA) || (controllerBus == KStorageBus_USB))
                {
                    if (fSuccess && isMachineOffline())
                    {
                        m_machine.SetHotPluggableForDevice(strControllerName, iAttachmentPort, iAttachmentDevice, fAttachmentHotPluggable);
                        /* Check that machine is OK: */
                        fSuccess = m_machine.isOk();
                    }
                }
            }
            else
            {
                /* Show error message: */
                msgCenter().cannotAttachDevice(m_machine, mediumTypeToLocal(attachmentDeviceType),
                                               vboxMedium.location(),
                                               StorageSlot(controllerBus, iAttachmentPort, iAttachmentDevice), this);
            }
        }
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsStorage::updateStorageAttachment(const UISettingsCacheMachineStorageController &controllerCache,
                                                       const UISettingsCacheMachineStorageAttachment &attachmentCache)
{
    /* Prepare result: */
    bool fSuccess = m_machine.isOk();
    if (fSuccess)
    {
        /* Get storage controller data from cache: */
        const UIDataSettingsMachineStorageController &controllerData = controllerCache.data();
        /* Get current storage attachment data from cache: */
        const UIDataSettingsMachineStorageAttachment &attachmentData = attachmentCache.data();

        /* Get storage controller attributes: */
        QString strControllerName = controllerData.m_strControllerName;
        KStorageBus controllerBus = controllerData.m_controllerBus;
        /* Get storage attachment attributes: */
        int iAttachmentPort = attachmentData.m_iAttachmentPort;
        int iAttachmentDevice = attachmentData.m_iAttachmentDevice;
        QString strAttachmentMediumId = attachmentData.m_strAttachmentMediumId;
        bool fAttachmentPassthrough = attachmentData.m_fAttachmentPassthrough;
        bool fAttachmentTempEject = attachmentData.m_fAttachmentTempEject;
        bool fAttachmentNonRotational = attachmentData.m_fAttachmentNonRotational;
        bool fAttachmentHotPluggable = attachmentData.m_fAttachmentHotPluggable;
        KDeviceType attachmentDeviceType = attachmentData.m_attachmentType;

        /* Check that storage attachment exists: */
        CMediumAttachment attachment = m_machine.GetMediumAttachment(strControllerName, iAttachmentPort, iAttachmentDevice);
        /* Check that machine is OK: */
        fSuccess = m_machine.isOk();
        /* If attachment exists: */
        if (fSuccess && !attachment.isNull())
        {
            /* Get GUI medium object: */
            UIMedium vboxMedium = vboxGlobal().medium(strAttachmentMediumId);
            /* Get COM medium object: */
            CMedium comMedium = vboxMedium.medium();
            /* Remount storage attachment: */
            m_machine.MountMedium(strControllerName, iAttachmentPort, iAttachmentDevice, comMedium, true /* force? */);
            /* Check that machine is OK: */
            fSuccess = m_machine.isOk();
            if (fSuccess)
            {
                if (attachmentDeviceType == KDeviceType_DVD)
                {
                    if (fSuccess && isMachineOffline())
                    {
                        m_machine.PassthroughDevice(strControllerName, iAttachmentPort, iAttachmentDevice, fAttachmentPassthrough);
                        /* Check that machine is OK: */
                        fSuccess = m_machine.isOk();
                    }
                    if (fSuccess && isMachineInValidMode())
                    {
                        m_machine.TemporaryEjectDevice(strControllerName, iAttachmentPort, iAttachmentDevice, fAttachmentTempEject);
                        /* Check that machine is OK: */
                        fSuccess = m_machine.isOk();
                    }
                }
                else if (attachmentDeviceType == KDeviceType_HardDisk)
                {
                    if (fSuccess && isMachineOffline())
                    {
                        m_machine.NonRotationalDevice(strControllerName, iAttachmentPort, iAttachmentDevice, fAttachmentNonRotational);
                        /* Check that machine is OK: */
                        fSuccess = m_machine.isOk();
                    }
                }
                if ((controllerBus == KStorageBus_SATA) || (controllerBus == KStorageBus_USB))
                {
                    if (fSuccess && isMachineOffline())
                    {
                        m_machine.SetHotPluggableForDevice(strControllerName, iAttachmentPort, iAttachmentDevice, fAttachmentHotPluggable);
                        /* Check that machine is OK: */
                        fSuccess = m_machine.isOk();
                    }
                }
            }
            else
            {
                /* Show error message: */
                msgCenter().cannotAttachDevice(m_machine, mediumTypeToLocal(attachmentDeviceType),
                                               vboxMedium.location(),
                                               StorageSlot(controllerBus, iAttachmentPort, iAttachmentDevice), this);
            }
        }
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsStorage::isControllerCouldBeUpdated(const UISettingsCacheMachineStorageController &controllerCache) const
{
    /* IController interface doesn't allows to change 'name' and 'bus' attributes.
     * But those attributes could be changed in GUI directly or indirectly.
     * For such cases we have to recreate IController instance,
     * for other cases we will update controller attributes only. */
    const UIDataSettingsMachineStorageController &oldControllerData = controllerCache.base();
    const UIDataSettingsMachineStorageController &newControllerData = controllerCache.data();
    return newControllerData.m_controllerBus == oldControllerData.m_controllerBus &&
           newControllerData.m_strControllerName == oldControllerData.m_strControllerName;
}

bool UIMachineSettingsStorage::isAttachmentCouldBeUpdated(const UISettingsCacheMachineStorageAttachment &attachmentCache) const
{
    /* IMediumAttachment could be indirectly updated through IMachine
     * only if attachment type, device and port were NOT changed and is one of the next types:
     * KDeviceType_Floppy or KDeviceType_DVD.
     * For other cases we will recreate attachment fully: */
    const UIDataSettingsMachineStorageAttachment &initialAttachmentData = attachmentCache.base();
    const UIDataSettingsMachineStorageAttachment &currentAttachmentData = attachmentCache.data();
    KDeviceType initialAttachmentDeviceType = initialAttachmentData.m_attachmentType;
    KDeviceType currentAttachmentDeviceType = currentAttachmentData.m_attachmentType;
    LONG iInitialAttachmentDevice = initialAttachmentData.m_iAttachmentDevice;
    LONG iCurrentAttachmentDevice = currentAttachmentData.m_iAttachmentDevice;
    LONG iInitialAttachmentPort = initialAttachmentData.m_iAttachmentPort;
    LONG iCurrentAttachmentPort = currentAttachmentData.m_iAttachmentPort;
    return (currentAttachmentDeviceType == initialAttachmentDeviceType) &&
           (iCurrentAttachmentDevice == iInitialAttachmentDevice) &&
           (iCurrentAttachmentPort == iInitialAttachmentPort) &&
           (currentAttachmentDeviceType == KDeviceType_Floppy || currentAttachmentDeviceType == KDeviceType_DVD);
}

void UIMachineSettingsStorage::setConfigurationAccessLevel(ConfigurationAccessLevel newConfigurationAccessLevel)
{
    /* Update model 'configuration access level': */
    mStorageModel->setConfigurationAccessLevel(newConfigurationAccessLevel);
    /* Update 'configuration access level' of base class: */
    UISettingsPageMachine::setConfigurationAccessLevel(newConfigurationAccessLevel);
}

# include "UIMachineSettingsStorage.moc"

