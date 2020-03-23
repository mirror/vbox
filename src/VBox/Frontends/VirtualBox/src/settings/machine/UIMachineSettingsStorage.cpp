/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsStorage class implementation.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QCommonStyle>
#include <QDrag>
#include <QDragMoveEvent>
#include <QItemDelegate>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>

/* GUI includes: */
#include "QITreeView.h"
#include "UICommon.h"
#include "UIConverter.h"
#include "UIErrorString.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIMachineSettingsStorage.h"
#include "UIMedium.h"
#include "UIMediumSelector.h"
#include "UIMessageCenter.h"

/* COM includes: */
#include "CMediumAttachment.h"
#include "CStorageController.h"

/* Defines: */
typedef QList<StorageSlot> SlotsList;
typedef QList<KDeviceType> DeviceTypeList;
typedef QList<KStorageBus> ControllerBusList;
typedef QList<KStorageControllerType> ControllerTypeList;
Q_DECLARE_METATYPE(SlotsList);
Q_DECLARE_METATYPE(DeviceTypeList);
Q_DECLARE_METATYPE(ControllerBusList);
Q_DECLARE_METATYPE(ControllerTypeList);


/** Item states. */
enum ItemState
{
    State_DefaultItem,
    State_CollapsedItem,
    State_ExpandedItem,
    State_MAX
};


/** Pixmap types. */
enum PixmapType
{
    InvalidPixmap,

    ControllerAddEn,
    ControllerAddDis,
    ControllerDelEn,
    ControllerDelDis,

    AttachmentAddEn,
    AttachmentAddDis,
    AttachmentDelEn,
    AttachmentDelDis,

    IDEControllerNormal,
    IDEControllerExpand,
    IDEControllerCollapse,
    SATAControllerNormal,
    SATAControllerExpand,
    SATAControllerCollapse,
    SCSIControllerNormal,
    SCSIControllerExpand,
    SCSIControllerCollapse,
    SASControllerNormal,
    SASControllerExpand,
    SASControllerCollapse,
    USBControllerNormal,
    USBControllerExpand,
    USBControllerCollapse,
    NVMeControllerNormal,
    NVMeControllerExpand,
    NVMeControllerCollapse,
    VirtioSCSIControllerNormal,
    VirtioSCSIControllerExpand,
    VirtioSCSIControllerCollapse,
    FloppyControllerNormal,
    FloppyControllerExpand,
    FloppyControllerCollapse,

    IDEControllerAddEn,
    IDEControllerAddDis,
    SATAControllerAddEn,
    SATAControllerAddDis,
    SCSIControllerAddEn,
    SCSIControllerAddDis,
    SASControllerAddEn,
    SASControllerAddDis,
    USBControllerAddEn,
    USBControllerAddDis,
    NVMeControllerAddEn,
    NVMeControllerAddDis,
    VirtioSCSIControllerAddEn,
    VirtioSCSIControllerAddDis,
    FloppyControllerAddEn,
    FloppyControllerAddDis,

    HDAttachmentNormal,
    CDAttachmentNormal,
    FDAttachmentNormal,

    HDAttachmentAddEn,
    HDAttachmentAddDis,
    CDAttachmentAddEn,
    CDAttachmentAddDis,
    FDAttachmentAddEn,
    FDAttachmentAddDis,

    ChooseExistingEn,
    ChooseExistingDis,
    CDUnmountEnabled,
    CDUnmountDisabled,
    FDUnmountEnabled,
    FDUnmountDisabled,

    MaxIndex
};


/** Machine settings: Storage Attachment data structure. */
struct UIDataSettingsMachineStorageAttachment
{
    /** Constructs data. */
    UIDataSettingsMachineStorageAttachment()
        : m_enmDeviceType(KDeviceType_Null)
        , m_iPort(-1)
        , m_iDevice(-1)
        , m_uMediumId(QUuid())
        , m_fPassthrough(false)
        , m_fTempEject(false)
        , m_fNonRotational(false)
        , m_fHotPluggable(false)
    {}

    /** Returns whether @a another passed data is equal to this one. */
    bool equal(const UIDataSettingsMachineStorageAttachment &another) const
    {
        return true
               && (m_enmDeviceType == another.m_enmDeviceType)
               && (m_iPort == another.m_iPort)
               && (m_iDevice == another.m_iDevice)
               && (m_uMediumId == another.m_uMediumId)
               && (m_fPassthrough == another.m_fPassthrough)
               && (m_fTempEject == another.m_fTempEject)
               && (m_fNonRotational == another.m_fNonRotational)
               && (m_fHotPluggable == another.m_fHotPluggable)
               ;
    }

    /** Returns whether @a another passed data is equal to this one. */
    bool operator==(const UIDataSettingsMachineStorageAttachment &another) const { return equal(another); }
    /** Returns whether @a another passed data is different from this one. */
    bool operator!=(const UIDataSettingsMachineStorageAttachment &another) const { return !equal(another); }

    /** Holds the device type. */
    KDeviceType  m_enmDeviceType;
    /** Holds the port. */
    LONG         m_iPort;
    /** Holds the device. */
    LONG         m_iDevice;
    /** Holds the medium ID. */
    QUuid        m_uMediumId;
    /** Holds whether the attachment being passed through. */
    bool         m_fPassthrough;
    /** Holds whether the attachment being temporarily eject. */
    bool         m_fTempEject;
    /** Holds whether the attachment is solid-state. */
    bool         m_fNonRotational;
    /** Holds whether the attachment is hot-pluggable. */
    bool         m_fHotPluggable;
};


/** Machine settings: Storage Controller data structure. */
struct UIDataSettingsMachineStorageController
{
    /** Constructs data. */
    UIDataSettingsMachineStorageController()
        : m_strName(QString())
        , m_enmBus(KStorageBus_Null)
        , m_enmType(KStorageControllerType_Null)
        , m_uPortCount(0)
        , m_fUseHostIOCache(false)
    {}

    /** Returns whether @a another passed data is equal to this one. */
    bool equal(const UIDataSettingsMachineStorageController &another) const
    {
        return true
               && (m_strName == another.m_strName)
               && (m_enmBus == another.m_enmBus)
               && (m_enmType == another.m_enmType)
               && (m_uPortCount == another.m_uPortCount)
               && (m_fUseHostIOCache == another.m_fUseHostIOCache)
               ;
    }

    /** Returns whether @a another passed data is equal to this one. */
    bool operator==(const UIDataSettingsMachineStorageController &another) const { return equal(another); }
    /** Returns whether @a another passed data is different from this one. */
    bool operator!=(const UIDataSettingsMachineStorageController &another) const { return !equal(another); }

    /** Holds the name. */
    QString                 m_strName;
    /** Holds the bus. */
    KStorageBus             m_enmBus;
    /** Holds the type. */
    KStorageControllerType  m_enmType;
    /** Holds the port count. */
    uint                    m_uPortCount;
    /** Holds whether the controller uses host IO cache. */
    bool                    m_fUseHostIOCache;
};


/** Machine settings: Storage page data structure. */
struct UIDataSettingsMachineStorage
{
    /** Constructs data. */
    UIDataSettingsMachineStorage() {}

    /** Returns whether @a another passed data is equal to this one. */
    bool operator==(const UIDataSettingsMachineStorage & /* another */) const { return true; }
    /** Returns whether @a another passed data is different from this one. */
    bool operator!=(const UIDataSettingsMachineStorage & /* another */) const { return false; }
};


/** UIIconPool interface extension used as Storage Settings page icon-pool. */
class UIIconPoolStorageSettings : public UIIconPool
{
public:

    /** Create icon-pool instance. */
    static void create();
    /** Destroy icon-pool instance. */
    static void destroy();

    /** Returns pixmap corresponding to passed @a enmPixmapType. */
    QPixmap pixmap(PixmapType enmPixmapType) const;
    /** Returns icon (probably merged) corresponding to passed @a enmPixmapType and @a enmPixmapDisabledType. */
    QIcon icon(PixmapType enmPixmapType, PixmapType enmPixmapDisabledType = InvalidPixmap) const;

private:

    /** Icon-pool constructor. */
    UIIconPoolStorageSettings();
    /** Icon-pool destructor. */
    virtual ~UIIconPoolStorageSettings() /* override */;

    /** Icon-pool instance access method. */
    static UIIconPoolStorageSettings *instance();

    /** Icon-pool instance. */
    static UIIconPoolStorageSettings *s_pInstance;
    /** Icon-pool names cache. */
    QMap<PixmapType, QString>         m_names;
    /** Icon-pool icons cache. */
    mutable QMap<PixmapType, QIcon>   m_icons;

    /** Allows for shortcut access. */
    friend UIIconPoolStorageSettings *iconPool();
};
UIIconPoolStorageSettings *iconPool() { return UIIconPoolStorageSettings::instance(); }


/** QITreeViewItem subclass used as abstract storage tree-view item. */
class AbstractItem : public QITreeViewItem
{
    Q_OBJECT;

public:

    /** Item types. */
    enum ItemType
    {
        Type_InvalidItem    = 0,
        Type_RootItem       = 1,
        Type_ControllerItem = 2,
        Type_AttachmentItem = 3
    };

    /** Constructs top-level item passing @a pParentTree to the base-class. */
    AbstractItem(QITreeView *pParentTree);
    /** Constructs sub-level item passing @a pParentItem to the base-class. */
    AbstractItem(AbstractItem *pParentItem);
    /** Destructs item. */
    virtual ~AbstractItem() /* override */;

    /** Returns parent-item. */
    AbstractItem *parent() const;
    /** Returns ID. */
    QUuid id() const;

    /** Returns machine ID. */
    QUuid machineId() const;
    /** Defines @a uMachineId. */
    void setMachineId(const QUuid &uMachineId);

    /** Returns runtime type information. */
    virtual ItemType rtti() const = 0;

    /** Returns child item with specified @a iIndex. */
    virtual AbstractItem *childItem(int iIndex) const = 0;
    /** Returns child item with specified @a uId. */
    virtual AbstractItem *childItemById(const QUuid &uId) const = 0;
    /** Returns position of specified child @a pItem. */
    virtual int posOfChild(AbstractItem *pItem) const = 0;

    /** Returns tool-tip information. */
    virtual QString toolTip() const = 0;
    /** Returns pixmap information for specified @a enmState. */
    virtual QPixmap pixmap(ItemState enmState = State_DefaultItem) = 0;

protected:

    /** Adds a child @a pItem. */
    virtual void addChild(AbstractItem *pItem) = 0;
    /** Removes the child @a pItem. */
    virtual void delChild(AbstractItem *pItem) = 0;

private:

    /** Holds the parent item reference. */
    AbstractItem *m_pParentItem;
    /** Holds the item ID. */
    QUuid         m_uId;
    /** Holds the item machine ID. */
    QUuid         m_uMachineId;
};
Q_DECLARE_METATYPE(AbstractItem::ItemType);


/** AbstractItem subclass used as root storage tree-view item. */
class RootItem : public AbstractItem
{
    Q_OBJECT;

public:

    /** Constructs top-level item passing @a pParentTree to the base-class. */
    RootItem(QITreeView *pParentTree);
    /** Destructs item. */
    virtual ~RootItem() /* override */;

    /** Returns a number of children of certain @a enmBus type. */
    ULONG childCount(KStorageBus enmBus) const;

protected:

    /** Returns runtime type information. */
    virtual ItemType rtti() const /* override */;

    /** Returns child item with specified @a iIndex. */
    virtual AbstractItem *childItem(int iIndex) const /* override */;
    /** Returns child item with specified @a uId. */
    virtual AbstractItem *childItemById(const QUuid &uId) const /* override */;
    /** Returns position of specified child @a pItem. */
    virtual int posOfChild(AbstractItem *pItem) const /* override */;
    /** Returns the number of children. */
    virtual int childCount() const /* override */;

    /** Returns the item text. */
    virtual QString text() const /* override */;
    /** Returns tool-tip information. */
    virtual QString toolTip() const /* override */;
    /** Returns pixmap information for specified @a enmState. */
    virtual QPixmap pixmap(ItemState enmState) /* override */;

    /** Adds a child @a pItem. */
    virtual void addChild(AbstractItem *pItem) /* override */;
    /** Removes the child @a pItem. */
    virtual void delChild(AbstractItem *pItem) /* override */;

private:

    /** Holds the list of controller items. */
    QList<AbstractItem*> m_controllers;
};


/** AbstractItem subclass used as controller storage tree-view item. */
class ControllerItem : public AbstractItem
{
    Q_OBJECT;

public:

    /** Constructs sub-level item passing @a pParentItem to the base-class.
      * @param  strName  Brings the name.
      * @param  enmBus   Brings the bus.
      * @param  enmType  Brings the type. */
    ControllerItem(AbstractItem *pParentItem, const QString &strName,
                   KStorageBus enmBus, KStorageControllerType enmType);
    /** Destructs item. */
    virtual ~ControllerItem() /* override */;

    /** Defines current @a strName. */
    void setName(const QString &strName);
    /** Returns current name. */
    QString name() const;
    /** Returns old name. */
    QString oldName() const;

    /** Defines @a enmBus. */
    void setBus(KStorageBus enmBus);
    /** Returns bus. */
    KStorageBus bus() const;
    /** Returns possible buses to switch from current one. */
    ControllerBusList buses() const;

    /** Defines @a enmType. */
    void setType(KStorageControllerType enmType);
    /** Returns type. */
    KStorageControllerType type() const;
    /** Returns possible types for specified @a enmBus to switch from current one. */
    ControllerTypeList types(KStorageBus enmBus) const;

    /** Defines current @a uPortCount. */
    void setPortCount(uint uPortCount);
    /** Returns current port count. */
    uint portCount();
    /** Returns maximum port count. */
    uint maxPortCount();

    /** Defines whether controller @a fUseIoCache. */
    void setUseIoCache(bool fUseIoCache);
    /** Returns whether controller uses IO cache. */
    bool useIoCache() const;

    /** Returns possible controller slots. */
    SlotsList allSlots() const;
    /** Returns used controller slots. */
    SlotsList usedSlots() const;
    /** Returns supported device type list. */
    DeviceTypeList deviceTypeList() const;

    /** Defines a list of @a attachments. */
    void setAttachments(const QList<AbstractItem*> &attachments) { m_attachments = attachments; }
    /** Returns a list of attachments. */
    QList<AbstractItem*> attachments() const { return m_attachments; }
    /** Returns an ID list of attached media of specified @a enmType. */
    QList<QUuid> attachmentIDs(KDeviceType enmType = KDeviceType_Null) const;

private:

    /** Returns runtime type information. */
    virtual ItemType rtti() const /* override */;

    /** Returns child item with specified @a iIndex. */
    virtual AbstractItem *childItem(int iIndex) const /* override */;
    /** Returns child item with specified @a uId. */
    virtual AbstractItem *childItemById(const QUuid &uId) const /* override */;
    /** Returns position of specified child @a pItem. */
    virtual int posOfChild(AbstractItem *pItem) const /* override */;
    /** Returns the number of children. */
    virtual int childCount() const /* override */;

    /** Returns the item text. */
    virtual QString text() const /* override */;
    /** Returns tool-tip information. */
    virtual QString toolTip() const /* override */;
    /** Returns pixmap information for specified @a enmState. */
    virtual QPixmap pixmap(ItemState enmState) /* override */;

    /** Adds a child @a pItem. */
    virtual void addChild(AbstractItem *pItem) /* override */;
    /** Removes the child @a pItem. */
    virtual void delChild(AbstractItem *pItem) /* override */;

    /** Updates possible buses. */
    void updateBusInfo();
    /** Updates possible types. */
    void updateTypeInfo();
    /** Updates pixmaps of possible buses. */
    void updatePixmaps();

    /** Holds the current name. */
    QString  m_strName;
    /** Holds the old name. */
    QString  m_strOldName;

    /** Holds the bus. */
    KStorageBus             m_enmBus;
    /** Holds the type. */
    KStorageControllerType  m_enmType;

    /** Holds the possible buses. */
    ControllerBusList                      m_buses;
    /** Holds the possible types on per bus basis. */
    QMap<KStorageBus, ControllerTypeList>  m_types;
    /** Holds the pixmaps of possible buses. */
    QList<PixmapType>                      m_pixmaps;

    /** Holds the current port count. */
    uint  m_uPortCount;
    /** Holds whether controller uses IO cache. */
    bool  m_fUseIoCache;

    /** Holds the list of attachments. */
    QList<AbstractItem*>  m_attachments;
};


/** AbstractItem subclass used as attachment storage tree-view item. */
class AttachmentItem : public AbstractItem
{
    Q_OBJECT;

public:

    /** Constructs sub-level item passing @a pParentItem to the base-class.
      * @param  enmDeviceType  Brings the attachment device type. */
    AttachmentItem(AbstractItem *pParentItem, KDeviceType enmDeviceType);

    /** Defines @a enmDeviceType. */
    void setDeviceType(KDeviceType enmDeviceType);
    /** Returns device type. */
    KDeviceType deviceType() const;
    /** Returns possible device types. */
    DeviceTypeList deviceTypes() const;

    /** Defines storage @a slot. */
    void setStorageSlot(const StorageSlot &slot);
    /** Returns storage slot. */
    StorageSlot storageSlot() const;
    /** Returns possible storage slots. */
    SlotsList storageSlots() const;

    /** Defines @a uMediumId. */
    void setMediumId(const QUuid &uMediumId);
    /** Returns the medium id. */
    QUuid mediumId() const;

    /** Returns whether attachment is a host drive. */
    bool isHostDrive() const;

    /** Defines whether attachment is @a fPassthrough. */
    void setPassthrough(bool fPassthrough);
    /** Returns whether attachment is passthrough. */
    bool isPassthrough() const;

    /** Defines whether attachment is @a fTemporaryEjectable. */
    void setTempEject(bool fTemporaryEjectable);
    /** Returns whether attachment is temporary ejectable. */
    bool isTempEject() const;

    /** Defines whether attachment is @a fNonRotational. */
    void setNonRotational(bool fNonRotational);
    /** Returns whether attachment is non-rotational. */
    bool isNonRotational() const;

    /** Returns whether attachment is @a fIsHotPluggable. */
    void setHotPluggable(bool fIsHotPluggable);
    /** Returns whether attachment is hot-pluggable. */
    bool isHotPluggable() const;

    /** Returns medium size. */
    QString size() const;
    /** Returns logical medium size. */
    QString logicalSize() const;
    /** Returns medium location. */
    QString location() const;
    /** Returns medium format. */
    QString format() const;
    /** Returns medium details. */
    QString details() const;
    /** Returns medium usage. */
    QString usage() const;
    /** Returns medium encryption password ID. */
    QString encryptionPasswordId() const;

private:

    /** Caches medium information. */
    void cache();

    /** Returns runtime type information. */
    virtual ItemType rtti() const /* override */;

    /** Returns child item with specified @a iIndex. */
    virtual AbstractItem *childItem(int iIndex) const /* override */;
    /** Returns child item with specified @a uId. */
    virtual AbstractItem *childItemById(const QUuid &uId) const /* override */;
    /** Returns position of specified child @a pItem. */
    virtual int posOfChild(AbstractItem *pItem) const /* override */;
    /** Returns the number of children. */
    virtual int childCount() const /* override */;

    /** Returns the item text. */
    virtual QString text() const /* override */;
    /** Returns tool-tip information. */
    virtual QString toolTip() const /* override */;
    /** Returns pixmap information for specified @a enmState. */
    virtual QPixmap pixmap(ItemState enmState) /* override */;

    /** Adds a child @a pItem. */
    virtual void addChild(AbstractItem *pItem) /* override */;
    /** Removes the child @a pItem. */
    virtual void delChild(AbstractItem *pItem) /* override */;

    /** Holds the device type. */
    KDeviceType  m_enmDeviceType;
    /** Holds the storage slot. */
    StorageSlot  m_storageSlot;
    /** Holds the medium ID. */
    QUuid        m_uMediumId;
    /** Holds whether attachment is a host drive. */
    bool         m_fHostDrive;
    /** Holds whether attachment is passthrough. */
    bool         m_fPassthrough;
    /** Holds whether attachment is temporary ejectable. */
    bool         m_fTempEject;
    /** Holds whether attachment is non-rotational. */
    bool         m_fNonRotational;
    /** Holds whether attachment is hot-pluggable. */
    bool         m_fHotPluggable;

    /** Holds the name. */
    QString  m_strName;
    /** Holds the tool-tip. */
    QString  m_strTip;
    /** Holds the pixmap. */
    QPixmap  m_strPixmap;

    /** Holds the medium size. */
    QString  m_strSize;
    /** Holds the logical medium size. */
    QString  m_strLogicalSize;
    /** Holds the medium location. */
    QString  m_strLocation;
    /** Holds the medium format. */
    QString  m_strFormat;
    /** Holds the medium details. */
    QString  m_strDetails;
    /** Holds the medium usage. */
    QString  m_strUsage;
    /** Holds the medium encryption password ID. */
    QString  m_strAttEncryptionPasswordID;
};


/** QAbstractItemModel subclass used as complex storage model. */
class StorageModel : public QAbstractItemModel
{
    Q_OBJECT;

public:

    /** Data roles. */
    enum DataRole
    {
        R_ItemId = Qt::UserRole + 1,
        R_ItemPixmap,
        R_ItemPixmapRect,
        R_ItemName,
        R_ItemNamePoint,
        R_ItemType,
        R_IsController,
        R_IsAttachment,

        R_ToolTipType,
        R_IsMoreIDEControllersPossible,
        R_IsMoreSATAControllersPossible,
        R_IsMoreSCSIControllersPossible,
        R_IsMoreFloppyControllersPossible,
        R_IsMoreSASControllersPossible,
        R_IsMoreUSBControllersPossible,
        R_IsMoreNVMeControllersPossible,
        R_IsMoreVirtioSCSIControllersPossible,
        R_IsMoreAttachmentsPossible,

        R_CtrOldName,
        R_CtrName,
        R_CtrType,
        R_CtrTypesForIDE,
        R_CtrTypesForSATA,
        R_CtrTypesForSCSI,
        R_CtrTypesForFloppy,
        R_CtrTypesForSAS,
        R_CtrTypesForUSB,
        R_CtrTypesForPCIe,
        R_CtrTypesForVirtioSCSI,
        R_CtrDevices,
        R_CtrBusType,
        R_CtrBusTypes,
        R_CtrPortCount,
        R_CtrMaxPortCount,
        R_CtrIoCache,

        R_AttSlot,
        R_AttSlots,
        R_AttDevice,
        R_AttMediumId,
        R_AttIsShowDiffs,
        R_AttIsHostDrive,
        R_AttIsPassthrough,
        R_AttIsTempEject,
        R_AttIsNonRotational,
        R_AttIsHotPluggable,
        R_AttSize,
        R_AttLogicalSize,
        R_AttLocation,
        R_AttFormat,
        R_AttDetails,
        R_AttUsage,
        R_AttEncryptionPasswordID,

        R_Margin,
        R_Spacing,
        R_IconSize,

        R_HDPixmapEn,
        R_CDPixmapEn,
        R_FDPixmapEn,

        R_HDPixmapAddEn,
        R_HDPixmapAddDis,
        R_CDPixmapAddEn,
        R_CDPixmapAddDis,
        R_FDPixmapAddEn,
        R_FDPixmapAddDis,
        R_HDPixmapRect,
        R_CDPixmapRect,
        R_FDPixmapRect
    };

    /** Tool-tip types. */
    enum ToolTipType
    {
        DefaultToolTip  = 0,
        ExpanderToolTip = 1,
        HDAdderToolTip  = 2,
        CDAdderToolTip  = 3,
        FDAdderToolTip  = 4
    };

    /** Constructs storage model passing @a pParentTree to the base-class. */
    StorageModel(QITreeView *pParentTree);
    /** Destructs storage model. */
    virtual ~StorageModel() /* override */;

    /** Returns row count for the passed @a parentIndex. */
    int rowCount(const QModelIndex &parentIndex = QModelIndex()) const;
    /** Returns column count for the passed @a parentIndex. */
    int columnCount(const QModelIndex &parentIndex = QModelIndex()) const;

    /** Returns root item. */
    QModelIndex root() const;
    /** Returns item specified by @a iRow, @a iColum and @a parentIndex. */
    QModelIndex index(int iRow, int iColumn, const QModelIndex &parentIndex = QModelIndex()) const;
    /** Returns parent item of specified @a index item. */
    QModelIndex parent(const QModelIndex &index) const;

    /** Returns model data for specified @a index and @a iRole. */
    QVariant data(const QModelIndex &index, int iRole) const;
    /** Defines model data for specified @a index and @a iRole as @a value. */
    bool setData(const QModelIndex &index, const QVariant &value, int iRole);

    /** Adds controller with certain @a strCtrName, @a enmBus and @a enmType. */
    QModelIndex addController(const QString &strCtrName, KStorageBus enmBus, KStorageControllerType enmType);
    /** Deletes controller with certain @a uCtrId. */
    void delController(const QUuid &uCtrId);

    /** Adds attachment with certain @a enmDeviceType and @a uMediumId to controller with certain @a uCtrId. */
    QModelIndex addAttachment(const QUuid &uCtrId, KDeviceType enmDeviceType, const QUuid &uMediumId);
    /** Deletes attachment with certain @a uAttId from controller with certain @a uCtrId. */
    void delAttachment(const QUuid &uCtrId, const QUuid &uAttId);
    /** Moves attachment with certain @a uAttId from controller with certain @a uCtrOldId to one with another @a uCtrNewId. */
    void moveAttachment(const QUuid &uAttId, const QUuid &uCtrOldId, const QUuid &uCtrNewId);

    /** Returns device type of attachment with certain @a uAttId from controller with certain @a uCtrId. */
    KDeviceType attachmentDeviceType(const QUuid &uCtrId, const QUuid &uAttId) const;

    /** Defines @a uMachineId for reference. */
    void setMachineId(const QUuid &uMachineId);

    /** Sorts the contents of model by @a iColumn and @a enmOrder. */
    void sort(int iColumn = 0, Qt::SortOrder enmOrder = Qt::AscendingOrder);
    /** Returns attachment index by specified @a controllerIndex and @a attachmentStorageSlot. */
    QModelIndex attachmentBySlot(QModelIndex controllerIndex, StorageSlot attachmentStorageSlot);

    /** Returns chipset type. */
    KChipsetType chipsetType() const;
    /** Defines @a enmChipsetType. */
    void setChipsetType(KChipsetType enmChipsetType);

    /** Defines @a enmConfigurationAccessLevel. */
    void setConfigurationAccessLevel(ConfigurationAccessLevel enmConfigurationAccessLevel);

    /** Clears model of all contents. */
    void clear();

    /** Returns current controller types. */
    QMap<KStorageBus, int> currentControllerTypes() const;
    /** Returns maximum controller types. */
    QMap<KStorageBus, int> maximumControllerTypes() const;

    /** Returns bus corresponding to passed enmRole. */
    static KStorageBus roleToBus(StorageModel::DataRole enmRole);
    /** Returns role corresponding to passed enmBus. */
    static StorageModel::DataRole busToRole(KStorageBus enmBus);

private:

    /** Returns model flags for specified @a index. */
    Qt::ItemFlags flags(const QModelIndex &index) const;

    /** Holds the root item instance. */
    AbstractItem *m_pRootItem;

    /** Holds the enabled plus pixmap instance. */
    QPixmap  m_pixmapPlusEn;
    /** Holds the disabled plus pixmap instance. */
    QPixmap  m_pixmapPlusDis;
    /** Holds the enabled minus pixmap instance. */
    QPixmap  m_pixmapMinusEn;
    /** Holds the disabled minus pixmap instance. */
    QPixmap  m_pixmapMinusDis;

    /** Holds the tool-tip type. */
    ToolTipType  m_enmToolTipType;

    /** Holds the chipset type. */
    KChipsetType  m_enmChipsetType;

    /** Holds configuration access level. */
    ConfigurationAccessLevel  m_enmConfigurationAccessLevel;
};
Q_DECLARE_METATYPE(StorageModel::ToolTipType);


/** QItemDelegate subclass used as storage table item delegate. */
class StorageDelegate : public QItemDelegate
{
    Q_OBJECT;

public:

    /** Constructs storage delegate passing @a pParent to the base-class. */
    StorageDelegate(QObject *pParent);

private:

    /** Paints @a index item with specified @a option using specified @a pPainter. */
    void paint(QPainter *pPainter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
};


/** QObject subclass used as UI medium ID holder.
  * Used for compliance with other storage page widgets
  * which caching and holding corresponding information. */
class UIMediumIDHolder : public QObject
{
    Q_OBJECT;

public:

    /** Constructs medium ID holder passing @a pParent to the base-class. */
    UIMediumIDHolder(QWidget *pParent) : QObject(pParent) {}

    /** Defines medium @a uId. */
    void setId(const QUuid &uId) { m_uId = uId; emit sigChanged(); }
    /** Returns medium ID. */
    QUuid id() const { return m_uId; }

    /** Defines medium device @a enmType. */
    void setType(UIMediumDeviceType enmType) { m_enmType = enmType; }
    /** Returns medium device type. */
    UIMediumDeviceType type() const { return m_enmType; }

    /** Returns whether medium ID is null. */
    bool isNull() const { return m_uId == UIMedium().id(); }

signals:

    /** Notify about medium ID changed. */
    void sigChanged();

private:

    /** Holds the medium ID. */
    QUuid               m_uId;
    /** Holds the medium device type. */
    UIMediumDeviceType  m_enmType;
};


QString compressText(const QString &strText)
{
    return QString("<nobr><compact elipsis=\"end\">%1</compact></nobr>").arg(strText);
}


/*********************************************************************************************************************************
*   Class UIIconPoolStorageSettings implementation.                                                                              *
*********************************************************************************************************************************/

/* static */
UIIconPoolStorageSettings *UIIconPoolStorageSettings::s_pInstance = 0;
UIIconPoolStorageSettings *UIIconPoolStorageSettings::instance() { return s_pInstance; }
void UIIconPoolStorageSettings::create() { new UIIconPoolStorageSettings; }
void UIIconPoolStorageSettings::destroy() { delete s_pInstance; }

QPixmap UIIconPoolStorageSettings::pixmap(PixmapType enmPixmapType) const
{
    /* Prepare fallback pixmap: */
    static QPixmap nullPixmap;

    /* If we do NOT have that 'pixmap type' icon cached already: */
    if (!m_icons.contains(enmPixmapType))
    {
        /* Compose proper icon if we have that 'pixmap type' known: */
        if (m_names.contains(enmPixmapType))
            m_icons[enmPixmapType] = iconSet(m_names[enmPixmapType]);
        /* Assign fallback icon if we do NOT have that 'pixmap type' known: */
        else
            m_icons[enmPixmapType] = iconSet(nullPixmap);
    }

    /* Retrieve corresponding icon: */
    const QIcon &icon = m_icons[enmPixmapType];
    AssertMsgReturn(!icon.isNull(),
                    ("Undefined icon for type '%d'.", (int)enmPixmapType),
                    nullPixmap);

    /* Retrieve available sizes for that icon: */
    const QList<QSize> availableSizes = icon.availableSizes();
    AssertMsgReturn(!availableSizes.isEmpty(),
                    ("Undefined icon for type '%s'.", (int)enmPixmapType),
                    nullPixmap);

    /* Determine icon metric: */
    const QStyle *pStyle = QApplication::style();
    const int iIconMetric = pStyle->pixelMetric(QStyle::PM_SmallIconSize);

    /* Return pixmap of first available size: */
    return icon.pixmap(QSize(iIconMetric, iIconMetric));
}

QIcon UIIconPoolStorageSettings::icon(PixmapType enmPixmapType,
                                      PixmapType enmPixmapDisabledType /* = InvalidPixmap */) const
{
    /* Prepare fallback pixmap: */
    static QPixmap nullPixmap;
    /* Prepare fallback icon: */
    static QIcon nullIcon;

    /* If we do NOT have that 'pixmap type' icon cached already: */
    if (!m_icons.contains(enmPixmapType))
    {
        /* Compose proper icon if we have that 'pixmap type' known: */
        if (m_names.contains(enmPixmapType))
            m_icons[enmPixmapType] = iconSet(m_names[enmPixmapType]);
        /* Assign fallback icon if we do NOT have that 'pixmap type' known: */
        else
            m_icons[enmPixmapType] = iconSet(nullPixmap);
    }

    /* Retrieve normal icon: */
    const QIcon &icon = m_icons[enmPixmapType];
    AssertMsgReturn(!icon.isNull(),
                    ("Undefined icon for type '%d'.", (int)enmPixmapType),
                    nullIcon);

    /* If 'disabled' icon is invalid => just return 'normal' icon: */
    if (enmPixmapDisabledType == InvalidPixmap)
        return icon;

    /* If we do NOT have that 'pixmap disabled type' icon cached already: */
    if (!m_icons.contains(enmPixmapDisabledType))
    {
        /* Compose proper icon if we have that 'pixmap disabled type' known: */
        if (m_names.contains(enmPixmapDisabledType))
            m_icons[enmPixmapDisabledType] = iconSet(m_names[enmPixmapDisabledType]);
        /* Assign fallback icon if we do NOT have that 'pixmap disabled type' known: */
        else
            m_icons[enmPixmapDisabledType] = iconSet(nullPixmap);
    }

    /* Retrieve disabled icon: */
    const QIcon &iconDisabled = m_icons[enmPixmapDisabledType];
    AssertMsgReturn(!iconDisabled.isNull(),
                    ("Undefined icon for type '%d'.", (int)enmPixmapDisabledType),
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
    s_pInstance = this;

    /* Controller file-names: */
    m_names.insert(ControllerAddEn,              ":/controller_add_16px.png");
    m_names.insert(ControllerAddDis,             ":/controller_add_disabled_16px.png");
    m_names.insert(ControllerDelEn,              ":/controller_remove_16px.png");
    m_names.insert(ControllerDelDis,             ":/controller_remove_disabled_16px.png");
    /* Attachment file-names: */
    m_names.insert(AttachmentAddEn,              ":/attachment_add_16px.png");
    m_names.insert(AttachmentAddDis,             ":/attachment_add_disabled_16px.png");
    m_names.insert(AttachmentDelEn,              ":/attachment_remove_16px.png");
    m_names.insert(AttachmentDelDis,             ":/attachment_remove_disabled_16px.png");
    /* Specific controller default/expand/collapse file-names: */
    m_names.insert(IDEControllerNormal,          ":/ide_16px.png");
    m_names.insert(IDEControllerExpand,          ":/ide_expand_16px.png");
    m_names.insert(IDEControllerCollapse,        ":/ide_collapse_16px.png");
    m_names.insert(SATAControllerNormal,         ":/sata_16px.png");
    m_names.insert(SATAControllerExpand,         ":/sata_expand_16px.png");
    m_names.insert(SATAControllerCollapse,       ":/sata_collapse_16px.png");
    m_names.insert(SCSIControllerNormal,         ":/scsi_16px.png");
    m_names.insert(SCSIControllerExpand,         ":/scsi_expand_16px.png");
    m_names.insert(SCSIControllerCollapse,       ":/scsi_collapse_16px.png");
    m_names.insert(SASControllerNormal,          ":/sas_16px.png");
    m_names.insert(SASControllerExpand,          ":/sas_expand_16px.png");
    m_names.insert(SASControllerCollapse,        ":/sas_collapse_16px.png");
    m_names.insert(USBControllerNormal,          ":/usb_16px.png");
    m_names.insert(USBControllerExpand,          ":/usb_expand_16px.png");
    m_names.insert(USBControllerCollapse,        ":/usb_collapse_16px.png");
    m_names.insert(NVMeControllerNormal,         ":/pcie_16px.png");
    m_names.insert(NVMeControllerExpand,         ":/pcie_expand_16px.png");
    m_names.insert(NVMeControllerCollapse,       ":/pcie_collapse_16px.png");
    m_names.insert(VirtioSCSIControllerNormal,   ":/virtio_scsi_16px.png");
    m_names.insert(VirtioSCSIControllerExpand,   ":/virtio_scsi_expand_16px.png");
    m_names.insert(VirtioSCSIControllerCollapse, ":/virtio_scsi_collapse_16px.png");
    m_names.insert(FloppyControllerNormal,       ":/floppy_16px.png");
    m_names.insert(FloppyControllerExpand,       ":/floppy_expand_16px.png");
    m_names.insert(FloppyControllerCollapse,     ":/floppy_collapse_16px.png");
    /* Specific controller add file-names: */
    m_names.insert(IDEControllerAddEn,           ":/ide_add_16px.png");
    m_names.insert(IDEControllerAddDis,          ":/ide_add_disabled_16px.png");
    m_names.insert(SATAControllerAddEn,          ":/sata_add_16px.png");
    m_names.insert(SATAControllerAddDis,         ":/sata_add_disabled_16px.png");
    m_names.insert(SCSIControllerAddEn,          ":/scsi_add_16px.png");
    m_names.insert(SCSIControllerAddDis,         ":/scsi_add_disabled_16px.png");
    m_names.insert(SASControllerAddEn,           ":/sas_add_16px.png");
    m_names.insert(SASControllerAddDis,          ":/sas_add_disabled_16px.png");
    m_names.insert(USBControllerAddEn,           ":/usb_add_16px.png");
    m_names.insert(USBControllerAddDis,          ":/usb_add_disabled_16px.png");
    m_names.insert(NVMeControllerAddEn,          ":/pcie_add_16px.png");
    m_names.insert(NVMeControllerAddDis,         ":/pcie_add_disabled_16px.png");
    m_names.insert(VirtioSCSIControllerAddEn,    ":/virtio_scsi_add_16px.png");
    m_names.insert(VirtioSCSIControllerAddDis,   ":/virtio_scsi_add_disabled_16px.png");
    m_names.insert(FloppyControllerAddEn,        ":/floppy_add_16px.png");
    m_names.insert(FloppyControllerAddDis,       ":/floppy_add_disabled_16px.png");
    /* Specific attachment file-names: */
    m_names.insert(HDAttachmentNormal,           ":/hd_16px.png");
    m_names.insert(CDAttachmentNormal,           ":/cd_16px.png");
    m_names.insert(FDAttachmentNormal,           ":/fd_16px.png");
    /* Specific attachment add file-names: */
    m_names.insert(HDAttachmentAddEn,            ":/hd_add_16px.png");
    m_names.insert(HDAttachmentAddDis,           ":/hd_add_disabled_16px.png");
    m_names.insert(CDAttachmentAddEn,            ":/cd_add_16px.png");
    m_names.insert(CDAttachmentAddDis,           ":/cd_add_disabled_16px.png");
    m_names.insert(FDAttachmentAddEn,            ":/fd_add_16px.png");
    m_names.insert(FDAttachmentAddDis,           ":/fd_add_disabled_16px.png");
    /* Specific attachment custom file-names: */
    m_names.insert(ChooseExistingEn,             ":/select_file_16px.png");
    m_names.insert(ChooseExistingDis,            ":/select_file_disabled_16px.png");
    m_names.insert(CDUnmountEnabled,             ":/cd_unmount_16px.png");
    m_names.insert(CDUnmountDisabled,            ":/cd_unmount_disabled_16px.png");
    m_names.insert(FDUnmountEnabled,             ":/fd_unmount_16px.png");
    m_names.insert(FDUnmountDisabled,            ":/fd_unmount_disabled_16px.png");
}

UIIconPoolStorageSettings::~UIIconPoolStorageSettings()
{
    /* Disconnect instance: */
    s_pInstance = 0;
}


/*********************************************************************************************************************************
*   Class AbstractItem implementation.                                                                                           *
*********************************************************************************************************************************/

AbstractItem::AbstractItem(QITreeView *pParentTree)
    : QITreeViewItem(pParentTree)
    , m_pParentItem(0)
    , m_uId(QUuid::createUuid())
{
    if (m_pParentItem)
        m_pParentItem->addChild(this);
}

AbstractItem::AbstractItem(AbstractItem *pParentItem)
    : QITreeViewItem(pParentItem)
    , m_pParentItem(pParentItem)
    , m_uId(QUuid::createUuid())
{
    if (m_pParentItem)
        m_pParentItem->addChild(this);
}

AbstractItem::~AbstractItem()
{
    if (m_pParentItem)
        m_pParentItem->delChild(this);
}

AbstractItem *AbstractItem::parent() const
{
    return m_pParentItem;
}

QUuid AbstractItem::id() const
{
    return m_uId;
}

QUuid AbstractItem::machineId() const
{
    return m_uMachineId;
}

void AbstractItem::setMachineId(const QUuid &uMachineId)
{
    m_uMachineId = uMachineId;
}


/*********************************************************************************************************************************
*   Class RootItem implementation.                                                                                               *
*********************************************************************************************************************************/

RootItem::RootItem(QITreeView *pParentTree)
    : AbstractItem(pParentTree)
{
}

RootItem::~RootItem()
{
    while (!m_controllers.isEmpty())
        delete m_controllers.first();
}

ULONG RootItem::childCount(KStorageBus enmBus) const
{
    ULONG uResult = 0;
    foreach (AbstractItem *pItem, m_controllers)
    {
        ControllerItem *pItemController = qobject_cast<ControllerItem*>(pItem);
        if (pItemController->bus() == enmBus)
            ++ uResult;
    }
    return uResult;
}

AbstractItem::ItemType RootItem::rtti() const
{
    return Type_RootItem;
}

AbstractItem *RootItem::childItem(int iIndex) const
{
    return m_controllers[iIndex];
}

AbstractItem *RootItem::childItemById(const QUuid &uId) const
{
    for (int i = 0; i < childCount(); ++ i)
        if (m_controllers[i]->id() == uId)
            return m_controllers[i];
    return 0;
}

int RootItem::posOfChild(AbstractItem *pItem) const
{
    return m_controllers.indexOf(pItem);
}

int RootItem::childCount() const
{
    return m_controllers.size();
}

QString RootItem::text() const
{
    return QString();
}

QString RootItem::toolTip() const
{
    return QString();
}

QPixmap RootItem::pixmap(ItemState /* enmState */)
{
    return QPixmap();
}

void RootItem::addChild(AbstractItem *pItem)
{
    m_controllers << pItem;
}

void RootItem::delChild(AbstractItem *pItem)
{
    m_controllers.removeAll(pItem);
}


/*********************************************************************************************************************************
*   Class ControllerItem implementation.                                                                                         *
*********************************************************************************************************************************/

ControllerItem::ControllerItem(AbstractItem *pParentItem, const QString &strName,
                               KStorageBus enmBus, KStorageControllerType enmType)
    : AbstractItem(pParentItem)
    , m_strName(strName)
    , m_strOldName(strName)
    , m_enmBus(enmBus)
    , m_enmType(enmType)
    , m_uPortCount(0)
    , m_fUseIoCache(false)
{
    /* Check for proper parent type: */
    AssertMsg(parent()->rtti() == AbstractItem::Type_RootItem, ("Incorrect parent type!\n"));

    AssertMsg(m_enmBus != KStorageBus_Null, ("Wrong Bus Type {%d}!\n", m_enmBus));
    AssertMsg(m_enmType != KStorageControllerType_Null, ("Wrong Controller Type {%d}!\n", m_enmType));

    updateBusInfo();
    updateTypeInfo();
    updatePixmaps();

    m_fUseIoCache = uiCommon().virtualBox().GetSystemProperties().GetDefaultIoCacheSettingForStorageController(enmType);
}

ControllerItem::~ControllerItem()
{
    while (!m_attachments.isEmpty())
        delete m_attachments.first();
}

void ControllerItem::setName(const QString &strName)
{
    m_strName = strName;
}

QString ControllerItem::name() const
{
    return m_strName;
}

QString ControllerItem::oldName() const
{
    return m_strOldName;
}

void ControllerItem::setBus(KStorageBus enmBus)
{
    m_enmBus = enmBus;

    updateBusInfo();
    updateTypeInfo();
    updatePixmaps();
}

KStorageBus ControllerItem::bus() const
{
    return m_enmBus;
}

ControllerBusList ControllerItem::buses() const
{
    return m_buses;
}

void ControllerItem::setType(KStorageControllerType enmType)
{
    m_enmType = enmType;

    updateTypeInfo();
}

KStorageControllerType ControllerItem::type() const
{
    return m_enmType;
}

ControllerTypeList ControllerItem::types(KStorageBus enmBus) const
{
    return m_types.value(enmBus);
}

void ControllerItem::setPortCount(uint uPortCount)
{
    /* Limit maximum port count: */
    m_uPortCount = qMin(uPortCount, (uint)uiCommon().virtualBox().GetSystemProperties().GetMaxPortCountForStorageBus(bus()));
}

uint ControllerItem::portCount()
{
    /* Recalculate actual port count: */
    for (int i = 0; i < m_attachments.size(); ++i)
    {
        AttachmentItem *pItem = qobject_cast<AttachmentItem*>(m_attachments.at(i));
        if (m_uPortCount < (uint)pItem->storageSlot().port + 1)
            m_uPortCount = (uint)pItem->storageSlot().port + 1;
    }
    return m_uPortCount;
}

uint ControllerItem::maxPortCount()
{
    return (uint)uiCommon().virtualBox().GetSystemProperties().GetMaxPortCountForStorageBus(bus());
}

void ControllerItem::setUseIoCache(bool fUseIoCache)
{
    m_fUseIoCache = fUseIoCache;
}

bool ControllerItem::useIoCache() const
{
    return m_fUseIoCache;
}

SlotsList ControllerItem::allSlots() const
{
    SlotsList allSlots;
    CSystemProperties comProps = uiCommon().virtualBox().GetSystemProperties();
    for (ULONG i = 0; i < comProps.GetMaxPortCountForStorageBus(bus()); ++ i)
        for (ULONG j = 0; j < comProps.GetMaxDevicesPerPortForStorageBus(bus()); ++ j)
            allSlots << StorageSlot(bus(), i, j);
    return allSlots;
}

SlotsList ControllerItem::usedSlots() const
{
    SlotsList usedSlots;
    for (int i = 0; i < m_attachments.size(); ++ i)
        usedSlots << qobject_cast<AttachmentItem*>(m_attachments.at(i))->storageSlot();
    return usedSlots;
}

DeviceTypeList ControllerItem::deviceTypeList() const
{
     return uiCommon().virtualBox().GetSystemProperties().GetDeviceTypesForStorageBus(m_enmBus).toList();
}

QList<QUuid> ControllerItem::attachmentIDs(KDeviceType enmType /* = KDeviceType_Null */) const
{
    QList<QUuid> ids;
    foreach (AbstractItem *pItem, m_attachments)
    {
        AttachmentItem *pItemAttachment = qobject_cast<AttachmentItem*>(pItem);
        if (   enmType == KDeviceType_Null
            || pItemAttachment->deviceType() == enmType)
            ids << pItem->id();
    }
    return ids;
}

AbstractItem::ItemType ControllerItem::rtti() const
{
    return Type_ControllerItem;
}

AbstractItem *ControllerItem::childItem(int iIndex) const
{
    return m_attachments[iIndex];
}

AbstractItem *ControllerItem::childItemById(const QUuid &uId) const
{
    for (int i = 0; i < childCount(); ++ i)
        if (m_attachments[i]->id() == uId)
            return m_attachments[i];
    return 0;
}

int ControllerItem::posOfChild(AbstractItem *pItem) const
{
    return m_attachments.indexOf(pItem);
}

int ControllerItem::childCount() const
{
    return m_attachments.size();
}

QString ControllerItem::text() const
{
    return UIMachineSettingsStorage::tr("Controller: %1").arg(name());
}

QString ControllerItem::toolTip() const
{
    return UIMachineSettingsStorage::tr("<nobr><b>%1</b></nobr><br>"
                                        "<nobr>Bus:&nbsp;&nbsp;%2</nobr><br>"
                                        "<nobr>Type:&nbsp;&nbsp;%3</nobr>")
                                        .arg(m_strName)
                                        .arg(gpConverter->toString(bus()))
                                        .arg(gpConverter->toString(type()));
}

QPixmap ControllerItem::pixmap(ItemState enmState)
{
    return iconPool()->pixmap(m_pixmaps.at(enmState));
}

void ControllerItem::addChild(AbstractItem *pItem)
{
    m_attachments << pItem;
}

void ControllerItem::delChild(AbstractItem *pItem)
{
    m_attachments.removeAll(pItem);
}

void ControllerItem::updateBusInfo()
{
    /* Clear the buses initially: */
    m_buses.clear();

    /* Load currently supported storage buses: */
    CSystemProperties comProperties = uiCommon().virtualBox().GetSystemProperties();
    const QVector<KStorageBus> supportedBuses = comProperties.GetSupportedStorageBuses();

    /* If current bus is NOT KStorageBus_Floppy: */
    if (m_enmBus != KStorageBus_Floppy)
    {
        /* We update the list with all supported buses
         * and remove the current one from that list. */
        m_buses << supportedBuses.toList();
        m_buses.removeAll(m_enmBus);
    }

    /* And prepend current bus finally: */
    m_buses.prepend(m_enmBus);
}

void ControllerItem::updateTypeInfo()
{
    /* Clear the types initially: */
    m_types.clear();

    /* Load currently supported storage buses & types: */
    CSystemProperties comProperties = uiCommon().virtualBox().GetSystemProperties();
    const QVector<KStorageBus> supportedBuses = comProperties.GetSupportedStorageBuses();
    const QVector<KStorageControllerType> supportedTypes = comProperties.GetSupportedStorageControllerTypes();

    /* We update the list with all supported buses
     * and remove the current one from that list. */
    ControllerBusList possibleBuses = supportedBuses.toList();
    possibleBuses.removeAll(m_enmBus);

    /* And prepend current bus finally: */
    possibleBuses.prepend(m_enmBus);

    /* Enumerate possible buses: */
    foreach (const KStorageBus &enmBus, possibleBuses)
    {
        /* Enumerate possible types and check whether type is supported or already selected before adding it: */
        foreach (const KStorageControllerType &enmType, comProperties.GetStorageControllerTypesForStorageBus(enmBus))
            if (supportedTypes.contains(enmType) || enmType == m_enmType)
                m_types[enmBus] << enmType;
    }
}

void ControllerItem::updatePixmaps()
{
    m_pixmaps.clear();

    for (int i = 0; i < State_MAX; ++i)
    {
        m_pixmaps << InvalidPixmap;
        switch (m_enmBus)
        {
            case KStorageBus_IDE:        m_pixmaps[i] = static_cast<PixmapType>(IDEControllerNormal + i); break;
            case KStorageBus_SATA:       m_pixmaps[i] = static_cast<PixmapType>(SATAControllerNormal + i); break;
            case KStorageBus_SCSI:       m_pixmaps[i] = static_cast<PixmapType>(SCSIControllerNormal + i); break;
            case KStorageBus_Floppy:     m_pixmaps[i] = static_cast<PixmapType>(FloppyControllerNormal + i); break;
            case KStorageBus_SAS:        m_pixmaps[i] = static_cast<PixmapType>(SASControllerNormal + i); break;
            case KStorageBus_USB:        m_pixmaps[i] = static_cast<PixmapType>(USBControllerNormal + i); break;
            case KStorageBus_PCIe:       m_pixmaps[i] = static_cast<PixmapType>(NVMeControllerNormal + i); break;
            case KStorageBus_VirtioSCSI: m_pixmaps[i] = static_cast<PixmapType>(VirtioSCSIControllerNormal + i); break;
            default: break;
        }
        AssertMsg(m_pixmaps[i] != InvalidPixmap, ("Invalid item state pixmap!\n"));
    }
}


/*********************************************************************************************************************************
*   Class AttachmentItem implementation.                                                                                         *
*********************************************************************************************************************************/

AttachmentItem::AttachmentItem(AbstractItem *pParentItem, KDeviceType enmDeviceType)
    : AbstractItem(pParentItem)
    , m_enmDeviceType(enmDeviceType)
    , m_fHostDrive(false)
    , m_fPassthrough(false)
    , m_fTempEject(false)
    , m_fNonRotational(false)
    , m_fHotPluggable(false)
{
    /* Check for proper parent type: */
    AssertMsg(parent()->rtti() == AbstractItem::Type_ControllerItem, ("Incorrect parent type!\n"));

    /* Select default slot: */
    AssertMsg(!storageSlots().isEmpty(), ("There should be at least one available slot!\n"));
    m_storageSlot = storageSlots()[0];
}

void AttachmentItem::setDeviceType(KDeviceType enmDeviceType)
{
    m_enmDeviceType = enmDeviceType;
}

KDeviceType AttachmentItem::deviceType() const
{
    return m_enmDeviceType;
}

DeviceTypeList AttachmentItem::deviceTypes() const
{
    return qobject_cast<ControllerItem*>(parent())->deviceTypeList();
}

void AttachmentItem::setStorageSlot(const StorageSlot &storageSlot)
{
    m_storageSlot = storageSlot;
}

StorageSlot AttachmentItem::storageSlot() const
{
    return m_storageSlot;
}

SlotsList AttachmentItem::storageSlots() const
{
    ControllerItem *pItemController = qobject_cast<ControllerItem*>(parent());

    /* Filter list from used slots: */
    SlotsList allSlots(pItemController->allSlots());
    SlotsList usedSlots(pItemController->usedSlots());
    foreach(StorageSlot usedSlot, usedSlots)
        if (usedSlot != m_storageSlot)
            allSlots.removeAll(usedSlot);

    return allSlots;
}

void AttachmentItem::setMediumId(const QUuid &uMediumId)
{
    /// @todo is this required?
    //AssertMsg(!aAttMediumId.isNull(), ("Medium ID value can't be null!\n"));
    m_uMediumId = uiCommon().medium(uMediumId).id();
    cache();
}

QUuid AttachmentItem::mediumId() const
{
    return m_uMediumId;
}

bool AttachmentItem::isHostDrive() const
{
    return m_fHostDrive;
}

void AttachmentItem::setPassthrough(bool fPassthrough)
{
    m_fPassthrough = fPassthrough;
}

bool AttachmentItem::isPassthrough() const
{
    return m_fPassthrough;
}

void AttachmentItem::setTempEject(bool fTempEject)
{
    m_fTempEject = fTempEject;
}

bool AttachmentItem::isTempEject() const
{
    return m_fTempEject;
}

void AttachmentItem::setNonRotational(bool fNonRotational)
{
    m_fNonRotational = fNonRotational;
}

bool AttachmentItem::isNonRotational() const
{
    return m_fNonRotational;
}

void AttachmentItem::setHotPluggable(bool fHotPluggable)
{
    m_fHotPluggable = fHotPluggable;
}

bool AttachmentItem::isHotPluggable() const
{
    return m_fHotPluggable;
}

QString AttachmentItem::size() const
{
    return m_strSize;
}

QString AttachmentItem::logicalSize() const
{
    return m_strLogicalSize;
}

QString AttachmentItem::location() const
{
    return m_strLocation;
}

QString AttachmentItem::format() const
{
    return m_strFormat;
}

QString AttachmentItem::details() const
{
    return m_strDetails;
}

QString AttachmentItem::usage() const
{
    return m_strUsage;
}

QString AttachmentItem::encryptionPasswordId() const
{
    return m_strAttEncryptionPasswordID;
}

void AttachmentItem::cache()
{
    UIMedium guiMedium = uiCommon().medium(m_uMediumId);

    /* Cache medium information: */
    m_strName = guiMedium.name(true);
    m_strTip = guiMedium.toolTipCheckRO(true, m_enmDeviceType != KDeviceType_HardDisk);
    m_strPixmap = guiMedium.iconCheckRO(true);
    m_fHostDrive = guiMedium.isHostDrive();

    /* Cache additional information: */
    m_strSize = guiMedium.size(true);
    m_strLogicalSize = guiMedium.logicalSize(true);
    m_strLocation = guiMedium.location(true);
    m_strAttEncryptionPasswordID = QString("--");
    if (guiMedium.isNull())
    {
        m_strFormat = QString("--");
    }
    else
    {
        switch (m_enmDeviceType)
        {
            case KDeviceType_HardDisk:
            {
                m_strFormat = QString("%1 (%2)").arg(guiMedium.hardDiskType(true)).arg(guiMedium.hardDiskFormat(true));
                m_strDetails = guiMedium.storageDetails();
                const QString strAttEncryptionPasswordID = guiMedium.encryptionPasswordID();
                if (!strAttEncryptionPasswordID.isNull())
                    m_strAttEncryptionPasswordID = strAttEncryptionPasswordID;
                break;
            }
            case KDeviceType_DVD:
            case KDeviceType_Floppy:
            {
                m_strFormat = m_fHostDrive ? UIMachineSettingsStorage::tr("Host Drive") : UIMachineSettingsStorage::tr("Image", "storage image");
                break;
            }
            default:
                break;
        }
    }
    m_strUsage = guiMedium.usage(true);

    /* Fill empty attributes: */
    if (m_strUsage.isEmpty())
        m_strUsage = QString("--");
}

AbstractItem::ItemType AttachmentItem::rtti() const
{
    return Type_AttachmentItem;
}

AbstractItem *AttachmentItem::childItem(int /* iIndex */) const
{
    return 0;
}

AbstractItem *AttachmentItem::childItemById(const QUuid& /* uId */) const
{
    return 0;
}

int AttachmentItem::posOfChild(AbstractItem * /* pItem */) const
{
    return 0;
}

int AttachmentItem::childCount() const
{
    return 0;
}

QString AttachmentItem::text() const
{
    return m_strName;
}

QString AttachmentItem::toolTip() const
{
    return m_strTip;
}

QPixmap AttachmentItem::pixmap(ItemState /* enmState */)
{
    if (m_strPixmap.isNull())
    {
        switch (m_enmDeviceType)
        {
            case KDeviceType_HardDisk:
                m_strPixmap = iconPool()->pixmap(HDAttachmentNormal);
                break;
            case KDeviceType_DVD:
                m_strPixmap = iconPool()->pixmap(CDAttachmentNormal);
                break;
            case KDeviceType_Floppy:
                m_strPixmap = iconPool()->pixmap(FDAttachmentNormal);
                break;
            default:
                break;
        }
    }
    return m_strPixmap;
}

void AttachmentItem::addChild(AbstractItem * /* pItem */)
{
}

void AttachmentItem::delChild(AbstractItem * /* pItem */)
{
}


/*********************************************************************************************************************************
*   Class StorageModel implementation.                                                                                           *
*********************************************************************************************************************************/

StorageModel::StorageModel(QITreeView *pParentTree)
    : QAbstractItemModel(pParentTree)
    , m_pRootItem(new RootItem(pParentTree))
    , m_enmToolTipType(DefaultToolTip)
    , m_enmChipsetType(KChipsetType_PIIX3)
    , m_enmConfigurationAccessLevel(ConfigurationAccessLevel_Null)
{
}

StorageModel::~StorageModel()
{
    delete m_pRootItem;
}

int StorageModel::rowCount(const QModelIndex &parentIndex) const
{
    return !parentIndex.isValid() ? 1 /* only root item has invalid parent */ :
           static_cast<AbstractItem*>(parentIndex.internalPointer())->childCount();
}

int StorageModel::columnCount(const QModelIndex & /* parentIndex */) const
{
    return 1;
}

QModelIndex StorageModel::root() const
{
    return index(0, 0);
}

QModelIndex StorageModel::index(int iRow, int iColumn, const QModelIndex &parentIndex) const
{
    if (!hasIndex(iRow, iColumn, parentIndex))
        return QModelIndex();

    AbstractItem *pItem = !parentIndex.isValid() ? m_pRootItem :
                          static_cast<AbstractItem*>(parentIndex.internalPointer())->childItem(iRow);

    return pItem ? createIndex(iRow, iColumn, pItem) : QModelIndex();
}

QModelIndex StorageModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer());
    AbstractItem *pParentOfItem = pItem->parent();
    AbstractItem *pParentOfParent = pParentOfItem ? pParentOfItem->parent() : 0;
    int iPosition = pParentOfParent ? pParentOfParent->posOfChild(pParentOfItem) : 0;

    if (pParentOfItem)
        return createIndex(iPosition, 0, pParentOfItem);
    else
        return QModelIndex();
}

QVariant StorageModel::data(const QModelIndex &index, int iRole) const
{
    if (!index.isValid())
        return QVariant();

    switch (iRole)
    {
        /* Basic Attributes: */
        case Qt::FontRole:
        {
            return QVariant(qApp->font());
        }
        case Qt::SizeHintRole:
        {
            QFontMetrics fm(data(index, Qt::FontRole).value<QFont>());
            int iMinimumHeight = qMax(fm.height(), data(index, R_IconSize).toInt());
            int iMargin = data(index, R_Margin).toInt();
            return QSize(1 /* ignoring width */, 2 * iMargin + iMinimumHeight);
        }
        case Qt::ToolTipRole:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
            {
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                {
                    QString strTip(pItem->toolTip());
                    switch (m_enmToolTipType)
                    {
                        case ExpanderToolTip:
                            if (index.child(0, 0).isValid())
                                strTip = UIMachineSettingsStorage::tr("<nobr>Expands/Collapses&nbsp;item.</nobr>");
                            break;
                        case HDAdderToolTip:
                            strTip = UIMachineSettingsStorage::tr("<nobr>Adds&nbsp;hard&nbsp;disk.</nobr>");
                            break;
                        case CDAdderToolTip:
                            strTip = UIMachineSettingsStorage::tr("<nobr>Adds&nbsp;optical&nbsp;drive.</nobr>");
                            break;
                        case FDAdderToolTip:
                            strTip = UIMachineSettingsStorage::tr("<nobr>Adds&nbsp;floppy&nbsp;drive.</nobr>");
                            break;
                        default:
                            break;
                    }
                    return strTip;
                }
                return pItem->toolTip();
            }
            return QString();
        }

        /* Advanced Attributes: */
        case R_ItemId:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                return pItem->id();
            return QUuid();
        }
        case R_ItemPixmap:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
            {
                ItemState enmState = State_DefaultItem;
                if (hasChildren(index))
                    if (QTreeView *view = qobject_cast<QTreeView*>(QObject::parent()))
                        enmState = view->isExpanded(index) ? State_ExpandedItem : State_CollapsedItem;
                return pItem->pixmap(enmState);
            }
            return QPixmap();
        }
        case R_ItemPixmapRect:
        {
            int iMargin = data(index, R_Margin).toInt();
            int iWidth = data(index, R_IconSize).toInt();
            return QRect(iMargin, iMargin, iWidth, iWidth);
        }
        case R_ItemName:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                return pItem->text();
            return QString();
        }
        case R_ItemNamePoint:
        {
            int iMargin = data(index, R_Margin).toInt();
            int iSpacing = data(index, R_Spacing).toInt();
            int iWidth = data(index, R_IconSize).toInt();
            QFontMetrics fm(data(index, Qt::FontRole).value<QFont>());
            QSize sizeHint = data(index, Qt::SizeHintRole).toSize();
            return QPoint(iMargin + iWidth + 2 * iSpacing,
                          sizeHint.height() / 2 + fm.ascent() / 2 - 1 /* base line */);
        }
        case R_ItemType:
        {
            QVariant result(QVariant::fromValue(AbstractItem::Type_InvalidItem));
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                result.setValue(pItem->rtti());
            return result;
        }
        case R_IsController:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                return pItem->rtti() == AbstractItem::Type_ControllerItem;
            return false;
        }
        case R_IsAttachment:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                return pItem->rtti() == AbstractItem::Type_AttachmentItem;
            return false;
        }

        case R_ToolTipType:
        {
            return QVariant::fromValue(m_enmToolTipType);
        }
        case R_IsMoreIDEControllersPossible:
        {
            return (m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Full) &&
                   (qobject_cast<RootItem*>(m_pRootItem)->childCount(KStorageBus_IDE) <
                    uiCommon().virtualBox().GetSystemProperties().GetMaxInstancesOfStorageBus(chipsetType(), KStorageBus_IDE));
        }
        case R_IsMoreSATAControllersPossible:
        {
            return (m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Full) &&
                   (qobject_cast<RootItem*>(m_pRootItem)->childCount(KStorageBus_SATA) <
                    uiCommon().virtualBox().GetSystemProperties().GetMaxInstancesOfStorageBus(chipsetType(), KStorageBus_SATA));
        }
        case R_IsMoreSCSIControllersPossible:
        {
            return (m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Full) &&
                   (qobject_cast<RootItem*>(m_pRootItem)->childCount(KStorageBus_SCSI) <
                    uiCommon().virtualBox().GetSystemProperties().GetMaxInstancesOfStorageBus(chipsetType(), KStorageBus_SCSI));
        }
        case R_IsMoreFloppyControllersPossible:
        {
            return (m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Full) &&
                   (qobject_cast<RootItem*>(m_pRootItem)->childCount(KStorageBus_Floppy) <
                    uiCommon().virtualBox().GetSystemProperties().GetMaxInstancesOfStorageBus(chipsetType(), KStorageBus_Floppy));
        }
        case R_IsMoreSASControllersPossible:
        {
            return (m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Full) &&
                   (qobject_cast<RootItem*>(m_pRootItem)->childCount(KStorageBus_SAS) <
                    uiCommon().virtualBox().GetSystemProperties().GetMaxInstancesOfStorageBus(chipsetType(), KStorageBus_SAS));
        }
        case R_IsMoreUSBControllersPossible:
        {
            return (m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Full) &&
                   (qobject_cast<RootItem*>(m_pRootItem)->childCount(KStorageBus_USB) <
                    uiCommon().virtualBox().GetSystemProperties().GetMaxInstancesOfStorageBus(chipsetType(), KStorageBus_USB));
        }
        case R_IsMoreNVMeControllersPossible:
        {
            return (m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Full) &&
                   (qobject_cast<RootItem*>(m_pRootItem)->childCount(KStorageBus_PCIe) <
                    uiCommon().virtualBox().GetSystemProperties().GetMaxInstancesOfStorageBus(chipsetType(), KStorageBus_PCIe));
        }
        case R_IsMoreVirtioSCSIControllersPossible:
        {
            return (m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Full) &&
                   (qobject_cast<RootItem*>(m_pRootItem)->childCount(KStorageBus_VirtioSCSI) <
                    uiCommon().virtualBox().GetSystemProperties().GetMaxInstancesOfStorageBus(chipsetType(), KStorageBus_VirtioSCSI));
        }
        case R_IsMoreAttachmentsPossible:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
            {
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                {
                    ControllerItem *pItemController = qobject_cast<ControllerItem*>(pItem);
                    CSystemProperties comProps = uiCommon().virtualBox().GetSystemProperties();
                    const bool fIsMoreAttachmentsPossible = (ULONG)rowCount(index) <
                                                            (comProps.GetMaxPortCountForStorageBus(pItemController->bus()) *
                                                             comProps.GetMaxDevicesPerPortForStorageBus(pItemController->bus()));
                    if (fIsMoreAttachmentsPossible)
                    {
                        switch (m_enmConfigurationAccessLevel)
                        {
                            case ConfigurationAccessLevel_Full:
                                return true;
                            case ConfigurationAccessLevel_Partial_Running:
                            {
                                switch (pItemController->bus())
                                {
                                    case KStorageBus_USB:
                                        return true;
                                    case KStorageBus_SATA:
                                        return (uint)rowCount(index) < pItemController->portCount();
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

        case R_CtrOldName:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                    return qobject_cast<ControllerItem*>(pItem)->oldName();
            return QString();
        }
        case R_CtrName:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                    return qobject_cast<ControllerItem*>(pItem)->name();
            return QString();
        }
        case R_CtrType:
        {
            QVariant result(QVariant::fromValue(KStorageControllerType_Null));
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                    result.setValue(qobject_cast<ControllerItem*>(pItem)->type());
            return result;
        }
        case R_CtrTypesForIDE:
        case R_CtrTypesForSATA:
        case R_CtrTypesForSCSI:
        case R_CtrTypesForFloppy:
        case R_CtrTypesForSAS:
        case R_CtrTypesForUSB:
        case R_CtrTypesForPCIe:
        case R_CtrTypesForVirtioSCSI:
        {
            QVariant result(QVariant::fromValue(ControllerTypeList()));
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                    result.setValue(qobject_cast<ControllerItem*>(pItem)->types(roleToBus((StorageModel::DataRole)iRole)));
            return result;
        }
        case R_CtrDevices:
        {
            QVariant result(QVariant::fromValue(DeviceTypeList()));
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                    result.setValue(qobject_cast<ControllerItem*>(pItem)->deviceTypeList());
            return result;
        }
        case R_CtrBusType:
        {
            QVariant result(QVariant::fromValue(KStorageBus_Null));
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                    result.setValue(qobject_cast<ControllerItem*>(pItem)->bus());
            return result;
        }
        case R_CtrBusTypes:
        {
            QVariant result(QVariant::fromValue(ControllerBusList()));
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                    result.setValue(qobject_cast<ControllerItem*>(pItem)->buses());
            return result;
        }
        case R_CtrPortCount:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                    return qobject_cast<ControllerItem*>(pItem)->portCount();
            return 0;
        }
        case R_CtrMaxPortCount:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                    return qobject_cast<ControllerItem*>(pItem)->maxPortCount();
            return 0;
        }
        case R_CtrIoCache:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                    return qobject_cast<ControllerItem*>(pItem)->useIoCache();
            return false;
        }

        case R_AttSlot:
        {
            QVariant result(QVariant::fromValue(StorageSlot()));
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    result.setValue(qobject_cast<AttachmentItem*>(pItem)->storageSlot());
            return result;
        }
        case R_AttSlots:
        {
            QVariant result(QVariant::fromValue(SlotsList()));
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    result.setValue(qobject_cast<AttachmentItem*>(pItem)->storageSlots());
            return result;
        }
        case R_AttDevice:
        {
            QVariant result(QVariant::fromValue(KDeviceType_Null));
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    result.setValue(qobject_cast<AttachmentItem*>(pItem)->deviceType());
            return result;
        }
        case R_AttMediumId:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->mediumId();
            return QUuid();
        }
        case R_AttIsHostDrive:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->isHostDrive();
            return false;
        }
        case R_AttIsPassthrough:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->isPassthrough();
            return false;
        }
        case R_AttIsTempEject:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->isTempEject();
            return false;
        }
        case R_AttIsNonRotational:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->isNonRotational();
            return false;
        }
        case R_AttIsHotPluggable:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->isHotPluggable();
            return false;
        }
        case R_AttSize:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->size();
            return QString();
        }
        case R_AttLogicalSize:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->logicalSize();
            return QString();
        }
        case R_AttLocation:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->location();
            return QString();
        }
        case R_AttFormat:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->format();
            return QString();
        }
        case R_AttDetails:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->details();
            return QString();
        }
        case R_AttUsage:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->usage();
            return QString();
        }
        case R_AttEncryptionPasswordID:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->encryptionPasswordId();
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
            int iMargin = data(index, R_Margin).toInt();
            int iWidth = data(index, R_IconSize).toInt();
            return QRect(0 - iWidth - iMargin, iMargin, iWidth, iWidth);
        }
        case R_CDPixmapRect:
        {
            int iMargin = data(index, R_Margin).toInt();
            int iSpacing = data(index, R_Spacing).toInt();
            int iWidth = data(index, R_IconSize).toInt();
            return QRect(0 - iWidth - iSpacing - iWidth - iMargin, iMargin, iWidth, iWidth);
        }
        case R_FDPixmapRect:
        {
            int iMargin = data(index, R_Margin).toInt();
            int iWidth = data(index, R_IconSize).toInt();
            return QRect(0 - iWidth - iMargin, iMargin, iWidth, iWidth);
        }

        default:
            break;
    }
    return QVariant();
}

bool StorageModel::setData(const QModelIndex &index, const QVariant &aValue, int iRole)
{
    if (!index.isValid())
        return QAbstractItemModel::setData(index, aValue, iRole);

    switch (iRole)
    {
        case R_ToolTipType:
        {
            m_enmToolTipType = aValue.value<ToolTipType>();
            emit dataChanged(index, index);
            return true;
        }
        case R_CtrName:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                {
                    qobject_cast<ControllerItem*>(pItem)->setName(aValue.toString());
                    emit dataChanged(index, index);
                    return true;
                }
            return false;
        }
        case R_CtrBusType:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                {
                    /* Acquire controller item and requested storage bus type: */
                    ControllerItem *pItemController = qobject_cast<ControllerItem*>(pItem);
                    const KStorageBus enmNewCtrBusType = aValue.value<KStorageBus>();

                    /* PCIe devices allows for hard-drives attachments only,
                     * no optical devices. So, lets make sure that rule is fulfilled. */
                    if (enmNewCtrBusType == KStorageBus_PCIe)
                    {
                        const QList<QUuid> opticalIds = pItemController->attachmentIDs(KDeviceType_DVD);
                        if (!opticalIds.isEmpty())
                        {
                            if (!msgCenter().confirmStorageBusChangeWithOpticalRemoval(qobject_cast<QWidget*>(QObject::parent())))
                                return false;
                            foreach (const QUuid &uId, opticalIds)
                                delAttachment(pItemController->id(), uId);
                        }
                    }

                    /* Lets make sure there is enough of place for all the remaining attachments: */
                    const uint uMaxPortCount =
                        (uint)uiCommon().virtualBox().GetSystemProperties().GetMaxPortCountForStorageBus(enmNewCtrBusType);
                    const uint uMaxDevicePerPortCount =
                        (uint)uiCommon().virtualBox().GetSystemProperties().GetMaxDevicesPerPortForStorageBus(enmNewCtrBusType);
                    const QList<QUuid> ids = pItemController->attachmentIDs();
                    if (uMaxPortCount * uMaxDevicePerPortCount < (uint)ids.size())
                    {
                        if (!msgCenter().confirmStorageBusChangeWithExcessiveRemoval(qobject_cast<QWidget*>(QObject::parent())))
                            return false;
                        for (int i = uMaxPortCount * uMaxDevicePerPortCount; i < ids.size(); ++i)
                            delAttachment(pItemController->id(), ids.at(i));
                    }

                    /* Push new bus/controller type: */
                    pItemController->setBus(enmNewCtrBusType);
                    pItemController->setType(pItemController->types(enmNewCtrBusType).first());
                    emit dataChanged(index, index);

                    /* Make sure each of remaining attachments has valid slot: */
                    foreach (AbstractItem *pChildItem, pItemController->attachments())
                    {
                        AttachmentItem *pChildItemAttachment = qobject_cast<AttachmentItem*>(pChildItem);
                        const SlotsList availableSlots = pChildItemAttachment->storageSlots();
                        const StorageSlot currentSlot = pChildItemAttachment->storageSlot();
                        if (!availableSlots.isEmpty() && !availableSlots.contains(currentSlot))
                            pChildItemAttachment->setStorageSlot(availableSlots.first());
                    }

                    /* This means success: */
                    return true;
                }
            return false;
        }
        case R_CtrType:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                {
                    qobject_cast<ControllerItem*>(pItem)->setType(aValue.value<KStorageControllerType>());
                    emit dataChanged(index, index);
                    return true;
                }
            return false;
        }
        case R_CtrPortCount:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                {
                    qobject_cast<ControllerItem*>(pItem)->setPortCount(aValue.toUInt());
                    emit dataChanged(index, index);
                    return true;
                }
            return false;
        }
        case R_CtrIoCache:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                {
                    qobject_cast<ControllerItem*>(pItem)->setUseIoCache(aValue.toBool());
                    emit dataChanged(index, index);
                    return true;
                }
            return false;
        }
        case R_AttSlot:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                {
                    qobject_cast<AttachmentItem*>(pItem)->setStorageSlot(aValue.value<StorageSlot>());
                    emit dataChanged(index, index);
                    sort();
                    return true;
                }
            return false;
        }
        case R_AttDevice:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                {
                    qobject_cast<AttachmentItem*>(pItem)->setDeviceType(aValue.value<KDeviceType>());
                    emit dataChanged(index, index);
                    return true;
                }
            return false;
        }
        case R_AttMediumId:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                {
                    qobject_cast<AttachmentItem*>(pItem)->setMediumId(aValue.toUuid());
                    emit dataChanged(index, index);
                    return true;
                }
            return false;
        }
        case R_AttIsPassthrough:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                {
                    qobject_cast<AttachmentItem*>(pItem)->setPassthrough(aValue.toBool());
                    emit dataChanged(index, index);
                    return true;
                }
            return false;
        }
        case R_AttIsTempEject:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                {
                    qobject_cast<AttachmentItem*>(pItem)->setTempEject(aValue.toBool());
                    emit dataChanged(index, index);
                    return true;
                }
            return false;
        }
        case R_AttIsNonRotational:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                {
                    qobject_cast<AttachmentItem*>(pItem)->setNonRotational(aValue.toBool());
                    emit dataChanged(index, index);
                    return true;
                }
            return false;
        }
        case R_AttIsHotPluggable:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                {
                    qobject_cast<AttachmentItem*>(pItem)->setHotPluggable(aValue.toBool());
                    emit dataChanged(index, index);
                    return true;
                }
            return false;
        }
        default:
            break;
    }

    return false;
}

QModelIndex StorageModel::addController(const QString &aCtrName, KStorageBus enmBus, KStorageControllerType enmType)
{
    beginInsertRows(root(), m_pRootItem->childCount(), m_pRootItem->childCount());
    new ControllerItem(m_pRootItem, aCtrName, enmBus, enmType);
    endInsertRows();
    return index(m_pRootItem->childCount() - 1, 0, root());
}

void StorageModel::delController(const QUuid &uCtrId)
{
    if (AbstractItem *pItem = m_pRootItem->childItemById(uCtrId))
    {
        int iItemPosition = m_pRootItem->posOfChild(pItem);
        beginRemoveRows(root(), iItemPosition, iItemPosition);
        delete pItem;
        endRemoveRows();
    }
}

QModelIndex StorageModel::addAttachment(const QUuid &uCtrId, KDeviceType enmDeviceType, const QUuid &uMediumId)
{
    if (AbstractItem *pParentItem = m_pRootItem->childItemById(uCtrId))
    {
        int iParentPosition = m_pRootItem->posOfChild(pParentItem);
        QModelIndex parentIndex = index(iParentPosition, 0, root());
        beginInsertRows(parentIndex, pParentItem->childCount(), pParentItem->childCount());
        AttachmentItem *pItem = new AttachmentItem(pParentItem, enmDeviceType);
        pItem->setHotPluggable(m_enmConfigurationAccessLevel != ConfigurationAccessLevel_Full);
        pItem->setMediumId(uMediumId);
        endInsertRows();
        return index(pParentItem->childCount() - 1, 0, parentIndex);
    }
    return QModelIndex();
}

void StorageModel::delAttachment(const QUuid &uCtrId, const QUuid &uAttId)
{
    if (AbstractItem *pParentItem = m_pRootItem->childItemById(uCtrId))
    {
        int iParentPosition = m_pRootItem->posOfChild(pParentItem);
        if (AbstractItem *pItem = pParentItem->childItemById(uAttId))
        {
            int iItemPosition = pParentItem->posOfChild(pItem);
            beginRemoveRows(index(iParentPosition, 0, root()), iItemPosition, iItemPosition);
            delete pItem;
            endRemoveRows();
        }
    }
}

void StorageModel::moveAttachment(const QUuid &uAttId, const QUuid &uCtrOldId, const QUuid &uCtrNewId)
{
    /* No known info about attachment device type and medium ID: */
    KDeviceType enmDeviceType = KDeviceType_Null;
    QUuid uMediumId;

    /* First of all we are looking for old controller item: */
    AbstractItem *pOldItem = m_pRootItem->childItemById(uCtrOldId);
    if (pOldItem)
    {
        /* And acquire controller position: */
        const int iOldCtrPosition = m_pRootItem->posOfChild(pOldItem);

        /* Then we are looking for an attachment item: */
        if (AbstractItem *pSubItem = pOldItem->childItemById(uAttId))
        {
            /* And make sure this is really an attachment: */
            AttachmentItem *pSubItemAttachment = qobject_cast<AttachmentItem*>(pSubItem);
            if (pSubItemAttachment)
            {
                /* This way we can acquire actual attachment device type and medium ID: */
                enmDeviceType = pSubItemAttachment->deviceType();
                uMediumId = pSubItemAttachment->mediumId();

                /* And delete atachment item finally: */
                const int iAttPosition = pOldItem->posOfChild(pSubItem);
                beginRemoveRows(index(iOldCtrPosition, 0, root()), iAttPosition, iAttPosition);
                delete pSubItem;
                endRemoveRows();
            }
        }
    }

    /* As the last step we are looking for new controller item: */
    AbstractItem *pNewItem = m_pRootItem->childItemById(uCtrNewId);
    if (pNewItem)
    {
        /* And acquire controller position: */
        const int iNewCtrPosition = m_pRootItem->posOfChild(pNewItem);

        /* Then we have to make sure moved attachment is valid: */
        if (enmDeviceType != KDeviceType_Null)
        {
            /* And create new attachment item finally: */
            QModelIndex newCtrIndex = index(iNewCtrPosition, 0, root());
            beginInsertRows(newCtrIndex, pNewItem->childCount(), pNewItem->childCount());
            AttachmentItem *pItem = new AttachmentItem(pNewItem, enmDeviceType);
            pItem->setHotPluggable(m_enmConfigurationAccessLevel != ConfigurationAccessLevel_Full);
            pItem->setMediumId(uMediumId);
            endInsertRows();
        }
    }
}

KDeviceType StorageModel::attachmentDeviceType(const QUuid &uCtrId, const QUuid &uAttId) const
{
    /* First of all we are looking for top-level (controller) item: */
    AbstractItem *pTopLevelItem = m_pRootItem->childItemById(uCtrId);
    if (pTopLevelItem)
    {
        /* Then we are looking for sub-level (attachment) item: */
        AbstractItem *pSubLevelItem = pTopLevelItem->childItemById(uAttId);
        if (pSubLevelItem)
        {
            /* And make sure this is really an attachment: */
            AttachmentItem *pAttachmentItem = qobject_cast<AttachmentItem*>(pSubLevelItem);
            if (pAttachmentItem)
            {
                /* This way we can acquire actual attachment device type: */
                return pAttachmentItem->deviceType();
            }
        }
    }

    /* Null by default: */
    return KDeviceType_Null;
}

void StorageModel::setMachineId(const QUuid &uMachineId)
{
    m_pRootItem->setMachineId(uMachineId);
}

void StorageModel::sort(int /* iColumn */, Qt::SortOrder enmOrder)
{
    /* Count of controller items: */
    int iItemLevel1Count = m_pRootItem->childCount();
    /* For each of controller items: */
    for (int iItemLevel1Pos = 0; iItemLevel1Pos < iItemLevel1Count; ++iItemLevel1Pos)
    {
        /* Get iterated controller item: */
        AbstractItem *pItemLevel1 = m_pRootItem->childItem(iItemLevel1Pos);
        ControllerItem *pControllerItem = qobject_cast<ControllerItem*>(pItemLevel1);
        /* Count of attachment items: */
        int iItemLevel2Count = pItemLevel1->childCount();
        /* Prepare empty list for sorted attachments: */
        QList<AbstractItem*> newAttachments;
        /* For each of attachment items: */
        for (int iItemLevel2Pos = 0; iItemLevel2Pos < iItemLevel2Count; ++iItemLevel2Pos)
        {
            /* Get iterated attachment item: */
            AbstractItem *pItemLevel2 = pItemLevel1->childItem(iItemLevel2Pos);
            AttachmentItem *pAttachmentItem = qobject_cast<AttachmentItem*>(pItemLevel2);
            /* Get iterated attachment storage slot: */
            StorageSlot attachmentSlot = pAttachmentItem->storageSlot();
            int iInsertPosition = 0;
            for (; iInsertPosition < newAttachments.size(); ++iInsertPosition)
            {
                /* Get sorted attachment item: */
                AbstractItem *pNewItemLevel2 = newAttachments[iInsertPosition];
                AttachmentItem *pNewAttachmentItem = qobject_cast<AttachmentItem*>(pNewItemLevel2);
                /* Get sorted attachment storage slot: */
                StorageSlot newAttachmentSlot = pNewAttachmentItem->storageSlot();
                /* Apply sorting rule: */
                if (((enmOrder == Qt::AscendingOrder) && (attachmentSlot < newAttachmentSlot)) ||
                    ((enmOrder == Qt::DescendingOrder) && (attachmentSlot > newAttachmentSlot)))
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
    return m_enmChipsetType;
}

void StorageModel::setChipsetType(KChipsetType enmChipsetType)
{
    m_enmChipsetType = enmChipsetType;
}

void StorageModel::setConfigurationAccessLevel(ConfigurationAccessLevel enmConfigurationAccessLevel)
{
    m_enmConfigurationAccessLevel = enmConfigurationAccessLevel;
}

void StorageModel::clear()
{
    while (m_pRootItem->childCount())
    {
        beginRemoveRows(root(), 0, 0);
        delete m_pRootItem->childItem(0);
        endRemoveRows();
    }
}

QMap<KStorageBus, int> StorageModel::currentControllerTypes() const
{
    QMap<KStorageBus, int> currentMap;
    for (int iStorageBusType = KStorageBus_IDE; iStorageBusType < KStorageBus_Max; ++iStorageBusType)
    {
        currentMap.insert((KStorageBus)iStorageBusType,
                          qobject_cast<RootItem*>(m_pRootItem)->childCount((KStorageBus)iStorageBusType));
    }
    return currentMap;
}

QMap<KStorageBus, int> StorageModel::maximumControllerTypes() const
{
    QMap<KStorageBus, int> maximumMap;
    for (int iStorageBusType = KStorageBus_IDE; iStorageBusType < KStorageBus_Max; ++iStorageBusType)
    {
        maximumMap.insert((KStorageBus)iStorageBusType,
                          uiCommon().virtualBox().GetSystemProperties().GetMaxInstancesOfStorageBus(chipsetType(), (KStorageBus)iStorageBusType));
    }
    return maximumMap;
}

/* static */
KStorageBus StorageModel::roleToBus(StorageModel::DataRole enmRole)
{
    QMap<StorageModel::DataRole, KStorageBus> typeRoles;
    typeRoles[StorageModel::R_CtrTypesForIDE]        = KStorageBus_IDE;
    typeRoles[StorageModel::R_CtrTypesForSATA]       = KStorageBus_SATA;
    typeRoles[StorageModel::R_CtrTypesForSCSI]       = KStorageBus_SCSI;
    typeRoles[StorageModel::R_CtrTypesForFloppy]     = KStorageBus_Floppy;
    typeRoles[StorageModel::R_CtrTypesForSAS]        = KStorageBus_SAS;
    typeRoles[StorageModel::R_CtrTypesForUSB]        = KStorageBus_USB;
    typeRoles[StorageModel::R_CtrTypesForPCIe]       = KStorageBus_PCIe;
    typeRoles[StorageModel::R_CtrTypesForVirtioSCSI] = KStorageBus_VirtioSCSI;
    return typeRoles.value(enmRole);
}

/* static */
StorageModel::DataRole StorageModel::busToRole(KStorageBus enmBus)
{
    QMap<KStorageBus, StorageModel::DataRole> typeRoles;
    typeRoles[KStorageBus_IDE]        = StorageModel::R_CtrTypesForIDE;
    typeRoles[KStorageBus_SATA]       = StorageModel::R_CtrTypesForSATA;
    typeRoles[KStorageBus_SCSI]       = StorageModel::R_CtrTypesForSCSI;
    typeRoles[KStorageBus_Floppy]     = StorageModel::R_CtrTypesForFloppy;
    typeRoles[KStorageBus_SAS]        = StorageModel::R_CtrTypesForSAS;
    typeRoles[KStorageBus_USB]        = StorageModel::R_CtrTypesForUSB;
    typeRoles[KStorageBus_PCIe]       = StorageModel::R_CtrTypesForPCIe;
    typeRoles[KStorageBus_VirtioSCSI] = StorageModel::R_CtrTypesForVirtioSCSI;
    return typeRoles.value(enmBus);
}

Qt::ItemFlags StorageModel::flags(const QModelIndex &index) const
{
    return !index.isValid() ? QAbstractItemModel::flags(index) :
           Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}


/*********************************************************************************************************************************
*   Class StorageDelegate implementation.                                                                                        *
*********************************************************************************************************************************/

StorageDelegate::StorageDelegate(QObject *pParent)
    : QItemDelegate(pParent)
{
}

void StorageDelegate::paint(QPainter *pPainter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (!index.isValid()) return;

    /* Initialize variables: */
    QStyle::State enmState = option.state;
    QRect rect = option.rect;
    const StorageModel *pModel = qobject_cast<const StorageModel*>(index.model());
    Assert(pModel);

    pPainter->save();

    /* Draw item background: */
    QItemDelegate::drawBackground(pPainter, option, index);

    /* Setup foreground settings: */
    QPalette::ColorGroup cg = enmState & QStyle::State_Active ? QPalette::Active : QPalette::Inactive;
    bool fSelected = enmState & QStyle::State_Selected;
    bool fFocused = enmState & QStyle::State_HasFocus;
    bool fGrayOnLoosingFocus = QApplication::style()->styleHint(QStyle::SH_ItemView_ChangeHighlightOnFocus, &option) != 0;
    pPainter->setPen(option.palette.color(cg, fSelected &&(fFocused || !fGrayOnLoosingFocus) ?
                                          QPalette::HighlightedText : QPalette::Text));

    pPainter->translate(rect.x(), rect.y());

    /* Draw Item Pixmap: */
    pPainter->drawPixmap(pModel->data(index, StorageModel::R_ItemPixmapRect).toRect().topLeft(),
                         pModel->data(index, StorageModel::R_ItemPixmap).value<QPixmap>());

    /* Draw compressed item name: */
    int iMargin = pModel->data(index, StorageModel::R_Margin).toInt();
    int iIconWidth = pModel->data(index, StorageModel::R_IconSize).toInt();
    int iSpacing = pModel->data(index, StorageModel::R_Spacing).toInt();
    QPoint textPosition = pModel->data(index, StorageModel::R_ItemNamePoint).toPoint();
    int iTextWidth = rect.width() - textPosition.x();
    if (pModel->data(index, StorageModel::R_IsController).toBool() && enmState & QStyle::State_Selected)
    {
        iTextWidth -= (2 * iSpacing + iIconWidth + iMargin);
        if (pModel->data(index, StorageModel::R_CtrBusType).value<KStorageBus>() != KStorageBus_Floppy)
            iTextWidth -= (iSpacing + iIconWidth);
    }
    QString strText(pModel->data(index, StorageModel::R_ItemName).toString());
    QString strShortText(strText);
    QFont font = pModel->data(index, Qt::FontRole).value<QFont>();
    QFontMetrics fm(font);
    while ((strShortText.size() > 1) && (fm.width(strShortText) + fm.width("...") > iTextWidth))
        strShortText.truncate(strShortText.size() - 1);
    if (strShortText != strText)
        strShortText += "...";
    pPainter->setFont(font);
    pPainter->drawText(textPosition, strShortText);

    /* Draw Controller Additions: */
    if (pModel->data(index, StorageModel::R_IsController).toBool() && enmState & QStyle::State_Selected)
    {
        DeviceTypeList devicesList(pModel->data(index, StorageModel::R_CtrDevices).value<DeviceTypeList>());
        for (int i = 0; i < devicesList.size(); ++ i)
        {
            KDeviceType enmDeviceType = devicesList[i];

            QRect deviceRect;
            QPixmap devicePixmap;
            switch (enmDeviceType)
            {
                case KDeviceType_HardDisk:
                {
                    deviceRect = pModel->data(index, StorageModel::R_HDPixmapRect).value<QRect>();
                    devicePixmap = pModel->data(index, StorageModel::R_IsMoreAttachmentsPossible).toBool() ?
                                   pModel->data(index, StorageModel::R_HDPixmapAddEn).value<QPixmap>() :
                                   pModel->data(index, StorageModel::R_HDPixmapAddDis).value<QPixmap>();
                    break;
                }
                case KDeviceType_DVD:
                {
                    deviceRect = pModel->data(index, StorageModel::R_CDPixmapRect).value<QRect>();
                    devicePixmap = pModel->data(index, StorageModel::R_IsMoreAttachmentsPossible).toBool() ?
                                   pModel->data(index, StorageModel::R_CDPixmapAddEn).value<QPixmap>() :
                                   pModel->data(index, StorageModel::R_CDPixmapAddDis).value<QPixmap>();
                    break;
                }
                case KDeviceType_Floppy:
                {
                    deviceRect = pModel->data(index, StorageModel::R_FDPixmapRect).value<QRect>();
                    devicePixmap = pModel->data(index, StorageModel::R_IsMoreAttachmentsPossible).toBool() ?
                                   pModel->data(index, StorageModel::R_FDPixmapAddEn).value<QPixmap>() :
                                   pModel->data(index, StorageModel::R_FDPixmapAddDis).value<QPixmap>();
                    break;
                }
                default:
                    break;
            }

            pPainter->drawPixmap(QPoint(rect.width() + deviceRect.x(), deviceRect.y()), devicePixmap);
        }
    }

    pPainter->restore();

    drawFocus(pPainter, option, rect);
}


/*********************************************************************************************************************************
*   Class UIMachineSettingsStorage implementation.                                                                               *
*********************************************************************************************************************************/

/* static */
const QString UIMachineSettingsStorage::s_strControllerMimeType = QString("application/virtualbox;value=StorageControllerID");
const QString UIMachineSettingsStorage::s_strAttachmentMimeType = QString("application/virtualbox;value=StorageAttachmentID");

UIMachineSettingsStorage::UIMachineSettingsStorage()
    : m_pModelStorage(0)
    , m_pActionAddController(0), m_pActionRemoveController(0)
    , m_pActionAddAttachment(0), m_pActionRemoveAttachment(0)
    , m_pActionAddAttachmentHD(0), m_pActionAddAttachmentCD(0), m_pActionAddAttachmentFD(0)
    , m_pMediumIdHolder(new UIMediumIDHolder(this))
    , m_fPolished(false)
    , m_fLoadingInProgress(0)
    , m_pCache(0)
{
    /* Prepare: */
    prepare();
}

UIMachineSettingsStorage::~UIMachineSettingsStorage()
{
    /* Cleanup: */
    cleanup();
}

void UIMachineSettingsStorage::setChipsetType(KChipsetType enmType)
{
    /* Make sure chipset type has changed: */
    if (m_pModelStorage->chipsetType() == enmType)
        return;

    /* Update chipset type value: */
    m_pModelStorage->setChipsetType(enmType);
    sltUpdateActionStates();

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

    /* Prepare old storage data: */
    UIDataSettingsMachineStorage oldStorageData;

    /* Gather old common data: */
    m_uMachineId = m_machine.GetId();
    m_strMachineSettingsFilePath = m_machine.GetSettingsFilePath();
    m_strMachineName = m_machine.GetName();
    m_strMachineGuestOSTypeId = m_machine.GetOSTypeId();

    /* For each controller: */
    const CStorageControllerVector &controllers = m_machine.GetStorageControllers();
    for (int iControllerIndex = 0; iControllerIndex < controllers.size(); ++iControllerIndex)
    {
        /* Prepare old controller data & cache key: */
        UIDataSettingsMachineStorageController oldControllerData;
        QString strControllerKey = QString::number(iControllerIndex);

        /* Check whether controller is valid: */
        const CStorageController &comController = controllers.at(iControllerIndex);
        if (!comController.isNull())
        {
            /* Gather old controller data: */
            oldControllerData.m_strName = comController.GetName();
            oldControllerData.m_enmBus = comController.GetBus();
            oldControllerData.m_enmType = comController.GetControllerType();
            oldControllerData.m_uPortCount = comController.GetPortCount();
            oldControllerData.m_fUseHostIOCache = comController.GetUseHostIOCache();
            /* Override controller cache key: */
            strControllerKey = oldControllerData.m_strName;

            /* Sort attachments before caching/fetching: */
            const CMediumAttachmentVector &attachmentVector =
                m_machine.GetMediumAttachmentsOfController(oldControllerData.m_strName);
            QMap<StorageSlot, CMediumAttachment> attachmentMap;
            foreach (const CMediumAttachment &comAttachment, attachmentVector)
            {
                const StorageSlot storageSlot(oldControllerData.m_enmBus,
                                              comAttachment.GetPort(), comAttachment.GetDevice());
                attachmentMap.insert(storageSlot, comAttachment);
            }
            const QList<CMediumAttachment> &attachments = attachmentMap.values();

            /* For each attachment: */
            for (int iAttachmentIndex = 0; iAttachmentIndex < attachments.size(); ++iAttachmentIndex)
            {
                /* Prepare old attachment data & cache key: */
                UIDataSettingsMachineStorageAttachment oldAttachmentData;
                QString strAttachmentKey = QString::number(iAttachmentIndex);

                /* Check whether attachment is valid: */
                const CMediumAttachment &comAttachment = attachments.at(iAttachmentIndex);
                if (!comAttachment.isNull())
                {
                    /* Gather old attachment data: */
                    oldAttachmentData.m_enmDeviceType = comAttachment.GetType();
                    oldAttachmentData.m_iPort = comAttachment.GetPort();
                    oldAttachmentData.m_iDevice = comAttachment.GetDevice();
                    oldAttachmentData.m_fPassthrough = comAttachment.GetPassthrough();
                    oldAttachmentData.m_fTempEject = comAttachment.GetTemporaryEject();
                    oldAttachmentData.m_fNonRotational = comAttachment.GetNonRotational();
                    oldAttachmentData.m_fHotPluggable = comAttachment.GetHotPluggable();
                    const CMedium comMedium = comAttachment.GetMedium();
                    oldAttachmentData.m_uMediumId = comMedium.isNull() ? UIMedium::nullID() : comMedium.GetId();
                    /* Override controller cache key: */
                    strAttachmentKey = QString("%1:%2").arg(oldAttachmentData.m_iPort).arg(oldAttachmentData.m_iDevice);
                }

                /* Cache old attachment data: */
                m_pCache->child(strControllerKey).child(strAttachmentKey).cacheInitialData(oldAttachmentData);
            }
        }

        /* Cache old controller data: */
        m_pCache->child(strControllerKey).cacheInitialData(oldControllerData);
    }

    /* Cache old storage data: */
    m_pCache->cacheInitialData(oldStorageData);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsStorage::getFromCache()
{
    /* Clear model initially: */
    m_pModelStorage->clear();

    /* Load old common data from the cache: */
    m_pModelStorage->setMachineId(m_uMachineId);

    /* For each controller: */
    for (int iControllerIndex = 0; iControllerIndex < m_pCache->childCount(); ++iControllerIndex)
    {
        /* Get controller cache: */
        const UISettingsCacheMachineStorageController &controllerCache = m_pCache->child(iControllerIndex);
        /* Get old controller data from the cache: */
        const UIDataSettingsMachineStorageController &oldControllerData = controllerCache.base();

        /* Load old controller data from the cache: */
        const QModelIndex controllerIndex = m_pModelStorage->addController(oldControllerData.m_strName,
                                                                           oldControllerData.m_enmBus,
                                                                           oldControllerData.m_enmType);
        const QUuid controllerId = QUuid(m_pModelStorage->data(controllerIndex, StorageModel::R_ItemId).toString());
        m_pModelStorage->setData(controllerIndex, oldControllerData.m_uPortCount, StorageModel::R_CtrPortCount);
        m_pModelStorage->setData(controllerIndex, oldControllerData.m_fUseHostIOCache, StorageModel::R_CtrIoCache);

        /* For each attachment: */
        for (int iAttachmentIndex = 0; iAttachmentIndex < controllerCache.childCount(); ++iAttachmentIndex)
        {
            /* Get attachment cache: */
            const UISettingsCacheMachineStorageAttachment &attachmentCache = controllerCache.child(iAttachmentIndex);
            /* Get old attachment data from the cache: */
            const UIDataSettingsMachineStorageAttachment &oldAttachmentData = attachmentCache.base();

            /* Load old attachment data from the cache: */
            const QModelIndex attachmentIndex = m_pModelStorage->addAttachment(controllerId,
                                                                               oldAttachmentData.m_enmDeviceType,
                                                                               oldAttachmentData.m_uMediumId);
            const StorageSlot attachmentStorageSlot(oldControllerData.m_enmBus,
                                                    oldAttachmentData.m_iPort,
                                                    oldAttachmentData.m_iDevice);
            m_pModelStorage->setData(attachmentIndex, QVariant::fromValue(attachmentStorageSlot), StorageModel::R_AttSlot);
            m_pModelStorage->setData(attachmentIndex, oldAttachmentData.m_fPassthrough, StorageModel::R_AttIsPassthrough);
            m_pModelStorage->setData(attachmentIndex, oldAttachmentData.m_fTempEject, StorageModel::R_AttIsTempEject);
            m_pModelStorage->setData(attachmentIndex, oldAttachmentData.m_fNonRotational, StorageModel::R_AttIsNonRotational);
            m_pModelStorage->setData(attachmentIndex, oldAttachmentData.m_fHotPluggable, StorageModel::R_AttIsHotPluggable);
        }
    }

    /* Choose first controller as current: */
    if (m_pModelStorage->rowCount(m_pModelStorage->root()) > 0)
        m_pTreeStorage->setCurrentIndex(m_pModelStorage->index(0, 0, m_pModelStorage->root()));

    /* Fetch recent information: */
    sltHandleCurrentItemChange();

    /* Polish page finally: */
    polishPage();

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsStorage::putToCache()
{
    /* Prepare new storage data: */
    UIDataSettingsMachineStorage newStorageData;

    /* For each controller: */
    const QModelIndex rootIndex = m_pModelStorage->root();
    for (int iControllerIndex = 0; iControllerIndex < m_pModelStorage->rowCount(rootIndex); ++iControllerIndex)
    {
        /* Prepare new controller data & key: */
        UIDataSettingsMachineStorageController newControllerData;

        /* Gather new controller data & cache key from model: */
        const QModelIndex controllerIndex = m_pModelStorage->index(iControllerIndex, 0, rootIndex);
        newControllerData.m_strName = m_pModelStorage->data(controllerIndex, StorageModel::R_CtrName).toString();
        newControllerData.m_enmBus = m_pModelStorage->data(controllerIndex, StorageModel::R_CtrBusType).value<KStorageBus>();
        newControllerData.m_enmType = m_pModelStorage->data(controllerIndex, StorageModel::R_CtrType).value<KStorageControllerType>();
        newControllerData.m_uPortCount = m_pModelStorage->data(controllerIndex, StorageModel::R_CtrPortCount).toUInt();
        newControllerData.m_fUseHostIOCache = m_pModelStorage->data(controllerIndex, StorageModel::R_CtrIoCache).toBool();
        const QString strControllerKey = m_pModelStorage->data(controllerIndex, StorageModel::R_CtrOldName).toString();

        /* For each attachment: */
        for (int iAttachmentIndex = 0; iAttachmentIndex < m_pModelStorage->rowCount(controllerIndex); ++iAttachmentIndex)
        {
            /* Prepare new attachment data & key: */
            UIDataSettingsMachineStorageAttachment newAttachmentData;

            /* Gather new attachment data & cache key from model: */
            const QModelIndex attachmentIndex = m_pModelStorage->index(iAttachmentIndex, 0, controllerIndex);
            newAttachmentData.m_enmDeviceType = m_pModelStorage->data(attachmentIndex, StorageModel::R_AttDevice).value<KDeviceType>();
            const StorageSlot attachmentSlot = m_pModelStorage->data(attachmentIndex, StorageModel::R_AttSlot).value<StorageSlot>();
            newAttachmentData.m_iPort = attachmentSlot.port;
            newAttachmentData.m_iDevice = attachmentSlot.device;
            newAttachmentData.m_fPassthrough = m_pModelStorage->data(attachmentIndex, StorageModel::R_AttIsPassthrough).toBool();
            newAttachmentData.m_fTempEject = m_pModelStorage->data(attachmentIndex, StorageModel::R_AttIsTempEject).toBool();
            newAttachmentData.m_fNonRotational = m_pModelStorage->data(attachmentIndex, StorageModel::R_AttIsNonRotational).toBool();
            newAttachmentData.m_fHotPluggable = m_pModelStorage->data(attachmentIndex, StorageModel::R_AttIsHotPluggable).toBool();
            newAttachmentData.m_uMediumId = m_pModelStorage->data(attachmentIndex, StorageModel::R_AttMediumId).toString();
            const QString strAttachmentKey = QString("%1:%2").arg(newAttachmentData.m_iPort).arg(newAttachmentData.m_iDevice);

            /* Cache new attachment data: */
            m_pCache->child(strControllerKey).child(strAttachmentKey).cacheCurrentData(newAttachmentData);
        }

        /* Cache new controller data: */
        m_pCache->child(strControllerKey).cacheCurrentData(newControllerData);
    }

    /* Cache new storage data: */
    m_pCache->cacheCurrentData(newStorageData);
}

void UIMachineSettingsStorage::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Update storage data and failing state: */
    setFailed(!saveStorageData());

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
    const QModelIndex rootIndex = m_pModelStorage->root();
    QMap<QString, QString> config;
    QMap<int, QString> names;
    /* For each controller: */
    for (int i = 0; i < m_pModelStorage->rowCount(rootIndex); ++i)
    {
        const QModelIndex controllerIndex = rootIndex.child(i, 0);
        const QString ctrName = m_pModelStorage->data(controllerIndex, StorageModel::R_CtrName).toString();

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
        for (int j = 0; j < m_pModelStorage->rowCount(controllerIndex); ++j)
        {
            const QModelIndex attachmentIndex = controllerIndex.child(j, 0);
            const StorageSlot attSlot = m_pModelStorage->data(attachmentIndex, StorageModel::R_AttSlot).value<StorageSlot>();
            const KDeviceType enmDeviceType = m_pModelStorage->data(attachmentIndex, StorageModel::R_AttDevice).value<KDeviceType>();
            const QString key(m_pModelStorage->data(attachmentIndex, StorageModel::R_AttMediumId).toString());
            const QString value(QString("%1 (%2)").arg(ctrName, gpConverter->toString(attSlot)));
            /* Check for emptiness: */
            if (uiCommon().medium(key).isNull() && enmDeviceType == KDeviceType_HardDisk)
            {
                message.second << tr("No hard disk is selected for <i>%1</i>.").arg(value);
                fPass = false;
            }
            /* Check for coincidence: */
            if (!uiCommon().medium(key).isNull() && config.contains(key))
            {
                message.second << tr("<i>%1</i> is using a disk that is already attached to <i>%2</i>.")
                                     .arg(value).arg(config[key]);
                fPass = false;
            }
            else
                config.insert(key, value);
        }
    }

    /* Check for excessive controllers on Storage page controllers list: */
    QStringList excessiveList;
    const QMap<KStorageBus, int> currentType = m_pModelStorage->currentControllerTypes();
    const QMap<KStorageBus, int> maximumType = m_pModelStorage->maximumControllerTypes();
    for (int iStorageBusType = KStorageBus_IDE; iStorageBusType < KStorageBus_Max; ++iStorageBusType)
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
                             .arg(gpConverter->toString(m_pModelStorage->chipsetType()))
                             .arg(excessiveList.join(", "));
        fPass = false;
    }

    /* Serialize message: */
    if (!message.second.isEmpty())
        messages << message;

    /* Return result: */
    return fPass;
}

void UIMachineSettingsStorage::setConfigurationAccessLevel(ConfigurationAccessLevel enmLevel)
{
    /* Update model 'configuration access level': */
    m_pModelStorage->setConfigurationAccessLevel(enmLevel);
    /* Update 'configuration access level' of base class: */
    UISettingsPageMachine::setConfigurationAccessLevel(enmLevel);
}

void UIMachineSettingsStorage::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIMachineSettingsStorage::retranslateUi(this);

    /* Translate storage-view: */
    m_pTreeStorage->setWhatsThis(tr("Lists all storage controllers for this machine and "
                                    "the virtual images and host drives attached to them."));

    /* Translate tool-bar: */
    m_pActionAddController->setShortcut(QKeySequence("Ins"));
    m_pActionRemoveController->setShortcut(QKeySequence("Del"));
    m_pActionAddAttachment->setShortcut(QKeySequence("+"));
    m_pActionRemoveAttachment->setShortcut(QKeySequence("-"));

    m_pActionAddController->setText(tr("Add Controller"));
    m_addControllerActions.value(KStorageControllerType_PIIX3)->setText(tr("PIIX3 (IDE)"));
    m_addControllerActions.value(KStorageControllerType_PIIX4)->setText(tr("PIIX4 (Default IDE)"));
    m_addControllerActions.value(KStorageControllerType_ICH6)->setText(tr("ICH6 (IDE)"));
    m_addControllerActions.value(KStorageControllerType_IntelAhci)->setText(tr("AHCI (SATA)"));
    m_addControllerActions.value(KStorageControllerType_LsiLogic)->setText(tr("LsiLogic (Default SCSI)"));
    m_addControllerActions.value(KStorageControllerType_BusLogic)->setText(tr("BusLogic (SCSI)"));
    m_addControllerActions.value(KStorageControllerType_LsiLogicSas)->setText(tr("LsiLogic SAS (SAS)"));
    m_addControllerActions.value(KStorageControllerType_I82078)->setText(tr("I82078 (Floppy)"));
    m_addControllerActions.value(KStorageControllerType_USB)->setText(tr("USB"));
    m_addControllerActions.value(KStorageControllerType_NVMe)->setText(tr("NVMe (PCIe)"));
    m_addControllerActions.value(KStorageControllerType_VirtioSCSI)->setText(tr("virtio-scsi"));
    m_pActionRemoveController->setText(tr("Remove Controller"));
    m_pActionAddAttachment->setText(tr("Add Attachment"));
    m_pActionAddAttachmentHD->setText(tr("Hard Disk"));
    m_pActionAddAttachmentCD->setText(tr("Optical Drive"));
    m_pActionAddAttachmentFD->setText(tr("Floppy Drive"));
    m_pActionRemoveAttachment->setText(tr("Remove Attachment"));

    m_pActionAddController->setWhatsThis(tr("Adds new storage controller."));
    m_pActionRemoveController->setWhatsThis(tr("Removes selected storage controller."));
    m_pActionAddAttachment->setWhatsThis(tr("Adds new storage attachment."));
    m_pActionRemoveAttachment->setWhatsThis(tr("Removes selected storage attachment."));

    m_pActionAddController->setToolTip(m_pActionAddController->whatsThis());
    m_pActionRemoveController->setToolTip(m_pActionRemoveController->whatsThis());
    m_pActionAddAttachment->setToolTip(m_pActionAddAttachment->whatsThis());
    m_pActionRemoveAttachment->setToolTip(m_pActionRemoveAttachment->whatsThis());
}

void UIMachineSettingsStorage::polishPage()
{
    /* Declare required variables: */
    const QModelIndex index = m_pTreeStorage->currentIndex();
    const KDeviceType enmDeviceType = m_pModelStorage->data(index, StorageModel::R_AttDevice).value<KDeviceType>();

    /* Polish left pane availability: */
    mLsLeftPane->setEnabled(isMachineInValidMode());
    m_pTreeStorage->setEnabled(isMachineInValidMode());

    /* Polish empty information pane availability: */
    mLsEmpty->setEnabled(isMachineInValidMode());
    mLbInfo->setEnabled(isMachineInValidMode());

    /* Polish controllers pane availability: */
    mLsParameters->setEnabled(isMachineInValidMode());
    mLbName->setEnabled(isMachineOffline());
    mLeName->setEnabled(isMachineOffline());
    mLbType->setEnabled(isMachineOffline());
    mCbType->setEnabled(isMachineOffline());
    mLbPortCount->setEnabled(isMachineOffline());
    mSbPortCount->setEnabled(isMachineOffline());
    mCbIoCache->setEnabled(isMachineOffline());

    /* Polish attachments pane availability: */
    mLsAttributes->setEnabled(isMachineInValidMode());
    mLbMedium->setEnabled(isMachineOffline() || (isMachineOnline() && enmDeviceType != KDeviceType_HardDisk));
    mCbSlot->setEnabled(isMachineOffline());
    mTbOpen->setEnabled(isMachineOffline() || (isMachineOnline() && enmDeviceType != KDeviceType_HardDisk));
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
    sltUpdateActionStates();
}

void UIMachineSettingsStorage::showEvent(QShowEvent *pEvent)
{
    if (!m_fPolished)
    {
        m_fPolished = true;

        /* First column indent: */
        mLtEmpty->setColumnMinimumWidth(0, 10);
        mLtController->setColumnMinimumWidth(0, 10);
        mLtAttachment->setColumnMinimumWidth(0, 10);
#if 0
        /* Second column indent minimum width: */
        QList <QLabel*> labelsList;
        labelsList << mLbMedium << mLbHDFormat << mLbCDFDType
                   << mLbHDVirtualSize << mLbHDActualSize << mLbSize
                   << mLbLocation << mLbUsage;
        int maxWidth = 0;
        QFontMetrics metrics(font());
        foreach (QLabel *label, labelsList)
        {
            int iWidth = metrics.width(label->text());
            maxWidth = iWidth > maxWidth ? iWidth : maxWidth;
        }
        mLtAttachment->setColumnMinimumWidth(1, maxWidth);
#endif
    }

    /* Call to base-class: */
    UISettingsPageMachine::showEvent(pEvent);
}

void UIMachineSettingsStorage::sltHandleMediumEnumerated(const QUuid &uMediumId)
{
    /* Search for corresponding medium: */
    const UIMedium medium = uiCommon().medium(uMediumId);

    const QModelIndex rootIndex = m_pModelStorage->root();
    for (int i = 0; i < m_pModelStorage->rowCount(rootIndex); ++i)
    {
        const QModelIndex controllerIndex = rootIndex.child(i, 0);
        for (int j = 0; j < m_pModelStorage->rowCount(controllerIndex); ++j)
        {
            const QModelIndex attachmentIndex = controllerIndex.child(j, 0);
            const QUuid attMediumId = m_pModelStorage->data(attachmentIndex, StorageModel::R_AttMediumId).toString();
            if (attMediumId == medium.id())
            {
                m_pModelStorage->setData(attachmentIndex, attMediumId, StorageModel::R_AttMediumId);

                /* Revalidate: */
                revalidate();
            }
        }
    }
}

void UIMachineSettingsStorage::sltHandleMediumDeleted(const QUuid &uMediumId)
{
    QModelIndex rootIndex = m_pModelStorage->root();
    for (int i = 0; i < m_pModelStorage->rowCount(rootIndex); ++i)
    {
        QModelIndex controllerIndex = rootIndex.child(i, 0);
        for (int j = 0; j < m_pModelStorage->rowCount(controllerIndex); ++j)
        {
            QModelIndex attachmentIndex = controllerIndex.child(j, 0);
            QUuid attMediumId = m_pModelStorage->data(attachmentIndex, StorageModel::R_AttMediumId).toString();
            if (attMediumId == uMediumId)
            {
                m_pModelStorage->setData(attachmentIndex, UIMedium().id(), StorageModel::R_AttMediumId);

                /* Revalidate: */
                revalidate();
            }
        }
    }
}

void UIMachineSettingsStorage::sltAddController()
{
    /* Load currently supported storage buses and types: */
    CSystemProperties comProperties = uiCommon().virtualBox().GetSystemProperties();
    const QVector<KStorageBus> supportedBuses = comProperties.GetSupportedStorageBuses();
    const QVector<KStorageControllerType> supportedTypes = comProperties.GetSupportedStorageControllerTypes();

    /* Prepare menu: */
    QMenu menu;
    foreach (const KStorageControllerType &enmType, supportedTypes)
    {
        QAction *pAction = m_addControllerActions.value(enmType);
        if (supportedBuses.contains(comProperties.GetStorageBusForStorageControllerType(enmType)))
            menu.addAction(pAction);
    }

    /* Popup it finally: */
    menu.exec(QCursor::pos());
}

void UIMachineSettingsStorage::sltAddControllerPIIX3()
{
    addControllerWrapper(generateUniqueControllerName("PIIX3"), KStorageBus_IDE, KStorageControllerType_PIIX3);
}

void UIMachineSettingsStorage::sltAddControllerPIIX4()
{
    addControllerWrapper(generateUniqueControllerName("PIIX4"), KStorageBus_IDE, KStorageControllerType_PIIX4);
}

void UIMachineSettingsStorage::sltAddControllerICH6()
{
    addControllerWrapper(generateUniqueControllerName("ICH6"), KStorageBus_IDE, KStorageControllerType_ICH6);
}

void UIMachineSettingsStorage::sltAddControllerAHCI()
{
    addControllerWrapper(generateUniqueControllerName("AHCI"), KStorageBus_SATA, KStorageControllerType_IntelAhci);
}

void UIMachineSettingsStorage::sltAddControllerLsiLogic()
{
    addControllerWrapper(generateUniqueControllerName("LsiLogic"), KStorageBus_SCSI, KStorageControllerType_LsiLogic);
}

void UIMachineSettingsStorage::sltAddControllerBusLogic()
{
    addControllerWrapper(generateUniqueControllerName("BusLogic"), KStorageBus_SCSI, KStorageControllerType_BusLogic);
}

void UIMachineSettingsStorage::sltAddControllerFloppy()
{
    addControllerWrapper(generateUniqueControllerName("Floppy"), KStorageBus_Floppy, KStorageControllerType_I82078);
}

void UIMachineSettingsStorage::sltAddControllerLsiLogicSAS()
{
    addControllerWrapper(generateUniqueControllerName("LsiLogic SAS"), KStorageBus_SAS, KStorageControllerType_LsiLogicSas);
}

void UIMachineSettingsStorage::sltAddControllerUSB()
{
    addControllerWrapper(generateUniqueControllerName("USB"), KStorageBus_USB, KStorageControllerType_USB);
}

void UIMachineSettingsStorage::sltAddControllerNVMe()
{
    addControllerWrapper(generateUniqueControllerName("NVMe"), KStorageBus_PCIe, KStorageControllerType_NVMe);
}

void UIMachineSettingsStorage::sltAddControllerVirtioSCSI()
{
    addControllerWrapper(generateUniqueControllerName("VirtIO"), KStorageBus_VirtioSCSI, KStorageControllerType_VirtioSCSI);
}

void UIMachineSettingsStorage::sltRemoveController()
{
    const QModelIndex index = m_pTreeStorage->currentIndex();
    if (!m_pModelStorage->data(index, StorageModel::R_IsController).toBool())
        return;

    m_pModelStorage->delController(QUuid(m_pModelStorage->data(index, StorageModel::R_ItemId).toString()));
    emit sigStorageChanged();

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsStorage::sltAddAttachment()
{
    const QModelIndex index = m_pTreeStorage->currentIndex();
    Assert(m_pModelStorage->data(index, StorageModel::R_IsController).toBool());

    const DeviceTypeList deviceTypeList(m_pModelStorage->data(index, StorageModel::R_CtrDevices).value<DeviceTypeList>());
    const bool fJustTrigger = deviceTypeList.size() == 1;
    const bool fShowMenu = deviceTypeList.size() > 1;
    QMenu menu;
    foreach (const KDeviceType &enmDeviceType, deviceTypeList)
    {
        switch (enmDeviceType)
        {
            case KDeviceType_HardDisk:
                if (fJustTrigger)
                    m_pActionAddAttachmentHD->trigger();
                if (fShowMenu)
                    menu.addAction(m_pActionAddAttachmentHD);
                break;
            case KDeviceType_DVD:
                if (fJustTrigger)
                    m_pActionAddAttachmentCD->trigger();
                if (fShowMenu)
                    menu.addAction(m_pActionAddAttachmentCD);
                break;
            case KDeviceType_Floppy:
                if (fJustTrigger)
                    m_pActionAddAttachmentFD->trigger();
                if (fShowMenu)
                    menu.addAction(m_pActionAddAttachmentFD);
                break;
            default:
                break;
        }
    }
    if (fShowMenu)
        menu.exec(QCursor::pos());
}

void UIMachineSettingsStorage::sltAddAttachmentHD()
{
    addAttachmentWrapper(KDeviceType_HardDisk);
}

void UIMachineSettingsStorage::sltAddAttachmentCD()
{
    addAttachmentWrapper(KDeviceType_DVD);
}

void UIMachineSettingsStorage::sltAddAttachmentFD()
{
    addAttachmentWrapper(KDeviceType_Floppy);
}

void UIMachineSettingsStorage::sltRemoveAttachment()
{
    const QModelIndex index = m_pTreeStorage->currentIndex();

    const KDeviceType enmDeviceType = m_pModelStorage->data(index, StorageModel::R_AttDevice).value<KDeviceType>();
    /* Check if this would be the last DVD. If so let the user confirm this again. */
    if (   enmDeviceType == KDeviceType_DVD
        && deviceCount(KDeviceType_DVD) == 1)
    {
        if (!msgCenter().confirmRemovingOfLastDVDDevice(this))
            return;
    }

    const QModelIndex parentIndex = index.parent();
    if (!index.isValid() || !parentIndex.isValid() ||
        !m_pModelStorage->data(index, StorageModel::R_IsAttachment).toBool() ||
        !m_pModelStorage->data(parentIndex, StorageModel::R_IsController).toBool())
        return;

    m_pModelStorage->delAttachment(QUuid(m_pModelStorage->data(parentIndex, StorageModel::R_ItemId).toString()),
                                   QUuid(m_pModelStorage->data(index, StorageModel::R_ItemId).toString()));
    emit sigStorageChanged();

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsStorage::sltGetInformation()
{
    m_fLoadingInProgress = true;

    const QModelIndex index = m_pTreeStorage->currentIndex();
    if (!index.isValid() || index == m_pModelStorage->root())
    {
        /* Showing Initial Page: */
        mSwRightPane->setCurrentIndex(0);
    }
    else
    {
        switch (m_pModelStorage->data(index, StorageModel::R_ItemType).value<AbstractItem::ItemType>())
        {
            case AbstractItem::Type_ControllerItem:
            {
                /* Getting Controller Name: */
                const QString strCtrName = m_pModelStorage->data(index, StorageModel::R_CtrName).toString();
                if (mLeName->text() != strCtrName)
                    mLeName->setText(strCtrName);

                /* Rebuild type combo: */
                mCbType->clear();
                /* Getting controller buses: */
                const ControllerBusList controllerBusList(m_pModelStorage->data(index, StorageModel::R_CtrBusTypes).value<ControllerBusList>());
                foreach (const KStorageBus &enmCurrentBus, controllerBusList)
                {
                    /* Getting controller types: */
                    const ControllerTypeList controllerTypeList(m_pModelStorage->data(index, m_pModelStorage->busToRole(enmCurrentBus)).value<ControllerTypeList>());
                    foreach (const KStorageControllerType &enmCurrentType, controllerTypeList)
                    {
                        mCbType->addItem(gpConverter->toString(enmCurrentType));
                        mCbType->setItemData(mCbType->count() - 1, QVariant::fromValue(enmCurrentBus), StorageModel::R_CtrBusType);
                        mCbType->setItemData(mCbType->count() - 1, QVariant::fromValue(enmCurrentType), StorageModel::R_CtrType);
                    }
                }
                const KStorageControllerType enmType = m_pModelStorage->data(index, StorageModel::R_CtrType).value<KStorageControllerType>();
                const int iCtrPos = mCbType->findData(enmType, StorageModel::R_CtrType);
                mCbType->setCurrentIndex(iCtrPos == -1 ? 0 : iCtrPos);

                const KStorageBus enmBus = m_pModelStorage->data(index, StorageModel::R_CtrBusType).value<KStorageBus>();
                mLbPortCount->setVisible(enmBus == KStorageBus_SATA || enmBus == KStorageBus_SAS);
                mSbPortCount->setVisible(enmBus == KStorageBus_SATA || enmBus == KStorageBus_SAS);
                const uint uPortCount = m_pModelStorage->data(index, StorageModel::R_CtrPortCount).toUInt();
                const uint uMaxPortCount = m_pModelStorage->data(index, StorageModel::R_CtrMaxPortCount).toUInt();
                mSbPortCount->setMaximum(uMaxPortCount);
                mSbPortCount->setValue(uPortCount);

                const bool fUseIoCache = m_pModelStorage->data(index, StorageModel::R_CtrIoCache).toBool();
                mCbIoCache->setChecked(fUseIoCache);

                /* Showing Controller Page: */
                mSwRightPane->setCurrentIndex(1);
                break;
            }
            case AbstractItem::Type_AttachmentItem:
            {
                /* Getting Attachment Slot: */
                mCbSlot->clear();
                const SlotsList slotsList(m_pModelStorage->data(index, StorageModel::R_AttSlots).value<SlotsList>());
                for (int i = 0; i < slotsList.size(); ++i)
                    mCbSlot->insertItem(mCbSlot->count(), gpConverter->toString(slotsList[i]));
                const StorageSlot slt = m_pModelStorage->data(index, StorageModel::R_AttSlot).value<StorageSlot>();
                const int iAttSlotPos = mCbSlot->findText(gpConverter->toString(slt));
                mCbSlot->setCurrentIndex(iAttSlotPos == -1 ? 0 : iAttSlotPos);
                mCbSlot->setToolTip(mCbSlot->itemText(mCbSlot->currentIndex()));

                /* Getting Attachment Medium: */
                const KDeviceType enmDeviceType = m_pModelStorage->data(index, StorageModel::R_AttDevice).value<KDeviceType>();
                switch (enmDeviceType)
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
                const bool fIsHotPluggable = m_pModelStorage->data(index, StorageModel::R_AttIsHotPluggable).toBool();

                /* Fetch device-type, medium-id: */
                m_pMediumIdHolder->setType(mediumTypeToLocal(enmDeviceType));
                m_pMediumIdHolder->setId(m_pModelStorage->data(index, StorageModel::R_AttMediumId).toString());

                /* Get/fetch editable state: */
                const bool fIsEditable =    (isMachineOffline())
                                         || (isMachineOnline() && enmDeviceType != KDeviceType_HardDisk)
                                         || (isMachineOnline() && enmDeviceType == KDeviceType_HardDisk && fIsHotPluggable);
                mLbMedium->setEnabled(fIsEditable);
                mTbOpen->setEnabled(fIsEditable);

                /* Getting Passthrough state: */
                const bool fHostDrive = m_pModelStorage->data(index, StorageModel::R_AttIsHostDrive).toBool();
                mCbPassthrough->setVisible(enmDeviceType == KDeviceType_DVD && fHostDrive);
                mCbPassthrough->setChecked(fHostDrive && m_pModelStorage->data(index, StorageModel::R_AttIsPassthrough).toBool());

                /* Getting TempEject state: */
                mCbTempEject->setVisible(enmDeviceType == KDeviceType_DVD && !fHostDrive);
                mCbTempEject->setChecked(!fHostDrive && m_pModelStorage->data(index, StorageModel::R_AttIsTempEject).toBool());

                /* Getting NonRotational state: */
                mCbNonRotational->setVisible(enmDeviceType == KDeviceType_HardDisk);
                mCbNonRotational->setChecked(m_pModelStorage->data(index, StorageModel::R_AttIsNonRotational).toBool());

                /* Fetch hot-pluggable state: */
                m_pCheckBoxHotPluggable->setVisible((slt.bus == KStorageBus_SATA) || (slt.bus == KStorageBus_USB));
                m_pCheckBoxHotPluggable->setChecked(fIsHotPluggable);

                /* Update optional widgets visibility: */
                updateAdditionalDetails(enmDeviceType);

                /* Getting Other Information: */
                mLbHDFormatValue->setText(compressText(m_pModelStorage->data(index, StorageModel::R_AttFormat).toString()));
                mLbCDFDTypeValue->setText(compressText(m_pModelStorage->data(index, StorageModel::R_AttFormat).toString()));
                mLbHDVirtualSizeValue->setText(compressText(m_pModelStorage->data(index, StorageModel::R_AttLogicalSize).toString()));
                mLbHDActualSizeValue->setText(compressText(m_pModelStorage->data(index, StorageModel::R_AttSize).toString()));
                mLbSizeValue->setText(compressText(m_pModelStorage->data(index, StorageModel::R_AttSize).toString()));
                mLbHDDetailsValue->setText(compressText(m_pModelStorage->data(index, StorageModel::R_AttDetails).toString()));
                mLbLocationValue->setText(compressText(m_pModelStorage->data(index, StorageModel::R_AttLocation).toString()));
                mLbUsageValue->setText(compressText(m_pModelStorage->data(index, StorageModel::R_AttUsage).toString()));
                m_pLabelEncryptionValue->setText(compressText(m_pModelStorage->data(index, StorageModel::R_AttEncryptionPasswordID).toString()));

                /* Showing Attachment Page: */
                mSwRightPane->setCurrentIndex(2);
                break;
            }
            default:
                break;
        }
    }

    /* Revalidate: */
    revalidate();

    m_fLoadingInProgress = false;
}

void UIMachineSettingsStorage::sltSetInformation()
{
    const QModelIndex index = m_pTreeStorage->currentIndex();
    if (m_fLoadingInProgress || !index.isValid() || index == m_pModelStorage->root())
        return;

    QObject *pSender = sender();
    switch (m_pModelStorage->data(index, StorageModel::R_ItemType).value<AbstractItem::ItemType>())
    {
        case AbstractItem::Type_ControllerItem:
        {
            /* Setting Controller Name: */
            if (pSender == mLeName)
                m_pModelStorage->setData(index, mLeName->text(), StorageModel::R_CtrName);
            /* Setting Controller Sub-Type: */
            else if (pSender == mCbType)
            {
                const bool fResult =
                    m_pModelStorage->setData(index,
                                             QVariant::fromValue(mCbType->currentData(StorageModel::R_CtrBusType).value<KStorageBus>()),
                                             StorageModel::R_CtrBusType);
                if (fResult)
                    m_pModelStorage->setData(index,
                                             QVariant::fromValue(mCbType->currentData(StorageModel::R_CtrType).value<KStorageControllerType>()),
                                             StorageModel::R_CtrType);
            }
            else if (pSender == mSbPortCount)
                m_pModelStorage->setData(index, mSbPortCount->value(), StorageModel::R_CtrPortCount);
            else if (pSender == mCbIoCache)
                m_pModelStorage->setData(index, mCbIoCache->isChecked(), StorageModel::R_CtrIoCache);
            break;
        }
        case AbstractItem::Type_AttachmentItem:
        {
            /* Setting Attachment Slot: */
            if (pSender == mCbSlot)
            {
                QModelIndex controllerIndex = m_pModelStorage->parent(index);
                StorageSlot attachmentStorageSlot = gpConverter->fromString<StorageSlot>(mCbSlot->currentText());
                m_pModelStorage->setData(index, QVariant::fromValue(attachmentStorageSlot), StorageModel::R_AttSlot);
                QModelIndex theSameIndexAtNewPosition = m_pModelStorage->attachmentBySlot(controllerIndex, attachmentStorageSlot);
                AssertMsg(theSameIndexAtNewPosition.isValid(), ("Current attachment disappears!\n"));
                m_pTreeStorage->setCurrentIndex(theSameIndexAtNewPosition);
            }
            /* Setting Attachment Medium: */
            else if (pSender == m_pMediumIdHolder)
                m_pModelStorage->setData(index, m_pMediumIdHolder->id(), StorageModel::R_AttMediumId);
            else if (pSender == mCbPassthrough)
            {
                if (m_pModelStorage->data(index, StorageModel::R_AttIsHostDrive).toBool())
                    m_pModelStorage->setData(index, mCbPassthrough->isChecked(), StorageModel::R_AttIsPassthrough);
            }
            else if (pSender == mCbTempEject)
            {
                if (!m_pModelStorage->data(index, StorageModel::R_AttIsHostDrive).toBool())
                    m_pModelStorage->setData(index, mCbTempEject->isChecked(), StorageModel::R_AttIsTempEject);
            }
            else if (pSender == mCbNonRotational)
            {
                m_pModelStorage->setData(index, mCbNonRotational->isChecked(), StorageModel::R_AttIsNonRotational);
            }
            else if (pSender == m_pCheckBoxHotPluggable)
            {
                m_pModelStorage->setData(index, m_pCheckBoxHotPluggable->isChecked(), StorageModel::R_AttIsHotPluggable);
            }
            break;
        }
        default:
            break;
    }

    emit sigStorageChanged();
    sltUpdateActionStates();
    sltGetInformation();
}

void UIMachineSettingsStorage::sltPrepareOpenMediumMenu()
{
    /* This slot should be called only by open-medium menu: */
    QMenu *pOpenMediumMenu = qobject_cast<QMenu*>(sender());
    AssertMsg(pOpenMediumMenu, ("Can't access open-medium menu!\n"));
    if (pOpenMediumMenu)
    {
        /* Erase menu initially: */
        pOpenMediumMenu->clear();
        /* Depending on current medium type: */
        switch (m_pMediumIdHolder->type())
        {
            case UIMediumDeviceType_HardDisk:
            {
                /* Add "Choose a virtual hard disk" action: */
                addChooseExistingMediumAction(pOpenMediumMenu, tr("Choose/Create a Virtual Hard Disk..."));
                addChooseDiskFileAction(pOpenMediumMenu, tr("Choose a disk file..."));
                pOpenMediumMenu->addSeparator();
                /* Add recent media list: */
                addRecentMediumActions(pOpenMediumMenu, m_pMediumIdHolder->type());
                break;
            }
            case UIMediumDeviceType_DVD:
            {
                /* Add "Choose a virtual optical disk" action: */
                addChooseExistingMediumAction(pOpenMediumMenu, tr("Choose/Create a Virtual Optical Disk..."));
                addChooseDiskFileAction(pOpenMediumMenu, tr("Choose a disk file..."));
                /* Add "Choose a physical drive" actions: */
                addChooseHostDriveActions(pOpenMediumMenu);
                pOpenMediumMenu->addSeparator();
                /* Add recent media list: */
                addRecentMediumActions(pOpenMediumMenu, m_pMediumIdHolder->type());
                /* Add "Eject current medium" action: */
                pOpenMediumMenu->addSeparator();
                QAction *pEjectCurrentMedium = pOpenMediumMenu->addAction(tr("Remove Disk from Virtual Drive"));
                pEjectCurrentMedium->setEnabled(!m_pMediumIdHolder->isNull());
                pEjectCurrentMedium->setIcon(iconPool()->icon(CDUnmountEnabled, CDUnmountDisabled));
                connect(pEjectCurrentMedium, &QAction::triggered, this, &UIMachineSettingsStorage::sltUnmountDevice);
                break;
            }
            case UIMediumDeviceType_Floppy:
            {
                /* Add "Choose a virtual floppy disk" action: */
                addChooseExistingMediumAction(pOpenMediumMenu, tr("Choose/Create a Virtual Floppy Disk..."));
                addChooseDiskFileAction(pOpenMediumMenu, tr("Choose a disk file..."));
                /* Add "Choose a physical drive" actions: */
                addChooseHostDriveActions(pOpenMediumMenu);
                pOpenMediumMenu->addSeparator();
                /* Add recent media list: */
                addRecentMediumActions(pOpenMediumMenu, m_pMediumIdHolder->type());
                /* Add "Eject current medium" action: */
                pOpenMediumMenu->addSeparator();
                QAction *pEjectCurrentMedium = pOpenMediumMenu->addAction(tr("Remove Disk from Virtual Drive"));
                pEjectCurrentMedium->setEnabled(!m_pMediumIdHolder->isNull());
                pEjectCurrentMedium->setIcon(iconPool()->icon(FDUnmountEnabled, FDUnmountDisabled));
                connect(pEjectCurrentMedium, &QAction::triggered, this, &UIMachineSettingsStorage::sltUnmountDevice);
                break;
            }
            default:
                break;
        }
    }
}

void UIMachineSettingsStorage::sltUnmountDevice()
{
    m_pMediumIdHolder->setId(UIMedium().id());
}

void UIMachineSettingsStorage::sltChooseExistingMedium()
{
    const QString strMachineFolder(QFileInfo(m_strMachineSettingsFilePath).absolutePath());

    QUuid uMediumId;
    int iResult = uiCommon().openMediumSelectorDialog(this, m_pMediumIdHolder->type(), uMediumId,
                                                      strMachineFolder, m_strMachineName,
                                                      m_strMachineGuestOSTypeId,
                                                      true /* enable create action: */, m_uMachineId);

    if (iResult == UIMediumSelector::ReturnCode_Rejected ||
        (iResult == UIMediumSelector::ReturnCode_Accepted && uMediumId.isNull()))
        return;
    if (iResult == static_cast<int>(UIMediumSelector::ReturnCode_LeftEmpty) &&
        (m_pMediumIdHolder->type() != UIMediumDeviceType_DVD && m_pMediumIdHolder->type() != UIMediumDeviceType_Floppy))
        return;

    m_pMediumIdHolder->setId(uMediumId);
}

void UIMachineSettingsStorage::sltChooseDiskFile()
{
    const QString strMachineFolder(QFileInfo(m_strMachineSettingsFilePath).absolutePath());

    QUuid uMediumId = uiCommon().openMediumWithFileOpenDialog(m_pMediumIdHolder->type(), this, strMachineFolder);
    if (uMediumId.isNull())
        return;
    m_pMediumIdHolder->setId(uMediumId);
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
        const QStringList mediumInfoList = pChooseRecentMediumAction->data().toString().split(',');
        const UIMediumDeviceType enmMediumType = (UIMediumDeviceType)mediumInfoList[0].toUInt();
        const QString strMediumLocation = mediumInfoList[1];
        const QUuid uMediumId = uiCommon().openMedium(enmMediumType, strMediumLocation, this);
        if (!uMediumId.isNull())
            m_pMediumIdHolder->setId(uMediumId);
    }
}

void UIMachineSettingsStorage::sltUpdateActionStates()
{
    const QModelIndex index = m_pTreeStorage->currentIndex();

    const bool fIDEPossible = m_pModelStorage->data(index, StorageModel::R_IsMoreIDEControllersPossible).toBool();
    const bool fSATAPossible = m_pModelStorage->data(index, StorageModel::R_IsMoreSATAControllersPossible).toBool();
    const bool fSCSIPossible = m_pModelStorage->data(index, StorageModel::R_IsMoreSCSIControllersPossible).toBool();
    const bool fFloppyPossible = m_pModelStorage->data(index, StorageModel::R_IsMoreFloppyControllersPossible).toBool();
    const bool fSASPossible = m_pModelStorage->data(index, StorageModel::R_IsMoreSASControllersPossible).toBool();
    const bool fUSBPossible = m_pModelStorage->data(index, StorageModel::R_IsMoreUSBControllersPossible).toBool();
    const bool fNVMePossible = m_pModelStorage->data(index, StorageModel::R_IsMoreNVMeControllersPossible).toBool();
    const bool fVirtioSCSIPossible = m_pModelStorage->data(index, StorageModel::R_IsMoreVirtioSCSIControllersPossible).toBool();

    const bool fController = m_pModelStorage->data(index, StorageModel::R_IsController).toBool();
    const bool fAttachment = m_pModelStorage->data(index, StorageModel::R_IsAttachment).toBool();
    const bool fAttachmentsPossible = m_pModelStorage->data(index, StorageModel::R_IsMoreAttachmentsPossible).toBool();
    const bool fIsAttachmentHotPluggable = m_pModelStorage->data(index, StorageModel::R_AttIsHotPluggable).toBool();

    /* Configure "add controller" actions: */
    m_pActionAddController->setEnabled(fIDEPossible || fSATAPossible || fSCSIPossible || fFloppyPossible || fSASPossible || fUSBPossible || fNVMePossible || fVirtioSCSIPossible);
    m_addControllerActions.value(KStorageControllerType_PIIX3)->setEnabled(fIDEPossible);
    m_addControllerActions.value(KStorageControllerType_PIIX4)->setEnabled(fIDEPossible);
    m_addControllerActions.value(KStorageControllerType_ICH6)->setEnabled(fIDEPossible);
    m_addControllerActions.value(KStorageControllerType_IntelAhci)->setEnabled(fSATAPossible);
    m_addControllerActions.value(KStorageControllerType_LsiLogic)->setEnabled(fSCSIPossible);
    m_addControllerActions.value(KStorageControllerType_BusLogic)->setEnabled(fSCSIPossible);
    m_addControllerActions.value(KStorageControllerType_I82078)->setEnabled(fFloppyPossible);
    m_addControllerActions.value(KStorageControllerType_LsiLogicSas)->setEnabled(fSASPossible);
    m_addControllerActions.value(KStorageControllerType_USB)->setEnabled(fUSBPossible);
    m_addControllerActions.value(KStorageControllerType_NVMe)->setEnabled(fNVMePossible);
    m_addControllerActions.value(KStorageControllerType_VirtioSCSI)->setEnabled(fVirtioSCSIPossible);

    /* Configure "add attachment" actions: */
    m_pActionAddAttachment->setEnabled(fController && fAttachmentsPossible);
    m_pActionAddAttachmentHD->setEnabled(fController && fAttachmentsPossible);
    m_pActionAddAttachmentCD->setEnabled(fController && fAttachmentsPossible);
    m_pActionAddAttachmentFD->setEnabled(fController && fAttachmentsPossible);

    /* Configure "delete controller" action: */
    const bool fControllerInSuitableState = isMachineOffline();
    m_pActionRemoveController->setEnabled(fController && fControllerInSuitableState);

    /* Configure "delete attachment" action: */
    const bool fAttachmentInSuitableState = isMachineOffline() ||
                                            (isMachineOnline() && fIsAttachmentHotPluggable);
    m_pActionRemoveAttachment->setEnabled(fAttachment && fAttachmentInSuitableState);
}

void UIMachineSettingsStorage::sltHandleRowInsertion(const QModelIndex &parentIndex, int iPosition)
{
    const QModelIndex index = m_pModelStorage->index(iPosition, 0, parentIndex);

    switch (m_pModelStorage->data(index, StorageModel::R_ItemType).value<AbstractItem::ItemType>())
    {
        case AbstractItem::Type_ControllerItem:
        {
            /* Select the newly created Controller Item: */
            m_pTreeStorage->setCurrentIndex(index);
            break;
        }
        case AbstractItem::Type_AttachmentItem:
        {
            /* Expand parent if it is not expanded yet: */
            if (!m_pTreeStorage->isExpanded(parentIndex))
                m_pTreeStorage->setExpanded(parentIndex, true);
            break;
        }
        default:
            break;
    }

    sltUpdateActionStates();
    sltGetInformation();
}

void UIMachineSettingsStorage::sltHandleRowRemoval()
{
    if (m_pModelStorage->rowCount (m_pModelStorage->root()) == 0)
        m_pTreeStorage->setCurrentIndex (m_pModelStorage->root());

    sltUpdateActionStates();
    sltGetInformation();
}

void UIMachineSettingsStorage::sltHandleCurrentItemChange()
{
    sltUpdateActionStates();
    sltGetInformation();
}

void UIMachineSettingsStorage::sltHandleContextMenuRequest(const QPoint &position)
{
    /* Forget last mouse press position: */
    m_mousePressPosition = QPoint();

    const QModelIndex index = m_pTreeStorage->indexAt(position);
    if (!index.isValid())
        return sltAddController();

    QMenu menu;
    switch (m_pModelStorage->data(index, StorageModel::R_ItemType).value<AbstractItem::ItemType>())
    {
        case AbstractItem::Type_ControllerItem:
        {
            const DeviceTypeList deviceTypeList(m_pModelStorage->data(index, StorageModel::R_CtrDevices).value<DeviceTypeList>());
            foreach (KDeviceType enmDeviceType, deviceTypeList)
            {
                switch (enmDeviceType)
                {
                    case KDeviceType_HardDisk:
                        menu.addAction(m_pActionAddAttachmentHD);
                        break;
                    case KDeviceType_DVD:
                        menu.addAction(m_pActionAddAttachmentCD);
                        break;
                    case KDeviceType_Floppy:
                        menu.addAction(m_pActionAddAttachmentFD);
                        break;
                    default:
                        break;
                }
            }
            menu.addAction(m_pActionRemoveController);
            break;
        }
        case AbstractItem::Type_AttachmentItem:
        {
            menu.addAction(m_pActionRemoveAttachment);
            break;
        }
        default:
            break;
    }
    if (!menu.isEmpty())
        menu.exec(m_pTreeStorage->viewport()->mapToGlobal(position));
}

void UIMachineSettingsStorage::sltHandleDrawItemBranches(QPainter *pPainter, const QRect &rect, const QModelIndex &index)
{
    if (!index.parent().isValid() || !index.parent().parent().isValid())
        return;

    pPainter->save();
    QStyleOption options;
    options.initFrom(m_pTreeStorage);
    options.rect = rect;
    options.state |= QStyle::State_Item;
    if (index.row() < m_pModelStorage->rowCount(index.parent()) - 1)
        options.state |= QStyle::State_Sibling;
    /* This pen is commonly used by different
     * look and feel styles to paint tree-view branches. */
    const QPen pen(QBrush(options.palette.dark().color(), Qt::Dense4Pattern), 0);
    pPainter->setPen(pen);
    /* If we want tree-view branches to be always painted we have to use QCommonStyle::drawPrimitive()
     * because QCommonStyle performs branch painting as opposed to particular inherited sub-classing styles. */
    qobject_cast<QCommonStyle*>(style())->QCommonStyle::drawPrimitive(QStyle::PE_IndicatorBranch, &options, pPainter);
    pPainter->restore();
}

void UIMachineSettingsStorage::sltHandleMouseMove(QMouseEvent *pEvent)
{
    /* Make sure event is valid: */
    AssertPtrReturnVoid(pEvent);

    const QModelIndex index = m_pTreeStorage->indexAt(pEvent->pos());
    const QRect indexRect = m_pTreeStorage->visualRect(index);

    /* Expander tool-tip: */
    if (m_pModelStorage->data(index, StorageModel::R_IsController).toBool())
    {
        QRect expanderRect = m_pModelStorage->data(index, StorageModel::R_ItemPixmapRect).toRect();
        expanderRect.translate(indexRect.x(), indexRect.y());
        if (expanderRect.contains(pEvent->pos()))
        {
            pEvent->setAccepted(true);
            if (m_pModelStorage->data(index, StorageModel::R_ToolTipType).value<StorageModel::ToolTipType>() != StorageModel::ExpanderToolTip)
                m_pModelStorage->setData(index, QVariant::fromValue(StorageModel::ExpanderToolTip), StorageModel::R_ToolTipType);
            return;
        }
    }

    /* Adder tool-tip: */
    if (m_pModelStorage->data(index, StorageModel::R_IsController).toBool() &&
        m_pTreeStorage->currentIndex() == index)
    {
        const DeviceTypeList devicesList(m_pModelStorage->data(index, StorageModel::R_CtrDevices).value<DeviceTypeList>());
        for (int i = 0; i < devicesList.size(); ++ i)
        {
            const KDeviceType enmDeviceType = devicesList[i];

            QRect deviceRect;
            switch (enmDeviceType)
            {
                case KDeviceType_HardDisk:
                {
                    deviceRect = m_pModelStorage->data(index, StorageModel::R_HDPixmapRect).toRect();
                    break;
                }
                case KDeviceType_DVD:
                {
                    deviceRect = m_pModelStorage->data(index, StorageModel::R_CDPixmapRect).toRect();
                    break;
                }
                case KDeviceType_Floppy:
                {
                    deviceRect = m_pModelStorage->data(index, StorageModel::R_FDPixmapRect).toRect();
                    break;
                }
                default:
                    break;
            }
            deviceRect.translate(indexRect.x() + indexRect.width(), indexRect.y());

            if (deviceRect.contains(pEvent->pos()))
            {
                pEvent->setAccepted(true);
                switch (enmDeviceType)
                {
                    case KDeviceType_HardDisk:
                    {
                        if (m_pModelStorage->data(index, StorageModel::R_ToolTipType).value<StorageModel::ToolTipType>() != StorageModel::HDAdderToolTip)
                            m_pModelStorage->setData(index, QVariant::fromValue(StorageModel::HDAdderToolTip), StorageModel::R_ToolTipType);
                        break;
                    }
                    case KDeviceType_DVD:
                    {
                        if (m_pModelStorage->data(index, StorageModel::R_ToolTipType).value<StorageModel::ToolTipType>() != StorageModel::CDAdderToolTip)
                            m_pModelStorage->setData(index, QVariant::fromValue(StorageModel::CDAdderToolTip), StorageModel::R_ToolTipType);
                        break;
                    }
                    case KDeviceType_Floppy:
                    {
                        if (m_pModelStorage->data(index, StorageModel::R_ToolTipType).value<StorageModel::ToolTipType>() != StorageModel::FDAdderToolTip)
                            m_pModelStorage->setData(index, QVariant::fromValue(StorageModel::FDAdderToolTip), StorageModel::R_ToolTipType);
                        break;
                    }
                    default:
                        break;
                }
                return;
            }
        }
    }

    /* Default tool-tip: */
    if (m_pModelStorage->data(index, StorageModel::R_ToolTipType).value<StorageModel::ToolTipType>() != StorageModel::DefaultToolTip)
        m_pModelStorage->setData(index, StorageModel::DefaultToolTip, StorageModel::R_ToolTipType);

    /* Check whether we should initiate dragging: */
    if (   !m_mousePressPosition.isNull()
        && QLineF(pEvent->screenPos(), m_mousePressPosition).length() >= QApplication::startDragDistance())
    {
        /* Forget last mouse press position: */
        m_mousePressPosition = QPoint();

        /* Check what item we are hovering currently: */
        QModelIndex index = m_pTreeStorage->indexAt(pEvent->pos());
        AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer());
        /* And make sure this is attachment item, we are supporting dragging for this kind only: */
        AttachmentItem *pItemAttachment = qobject_cast<AttachmentItem*>(pItem);
        if (pItemAttachment)
        {
            /* Initialize dragging: */
            pEvent->setAccepted(true);
            QDrag *pDrag = new QDrag(this);
            if (pDrag)
            {
                /* Assign pixmap: */
                pDrag->setPixmap(pItem->pixmap());
                /* Prepare mime: */
                QMimeData *pMimeData = new QMimeData;
                if (pMimeData)
                {
                    pMimeData->setData(s_strControllerMimeType, pItemAttachment->parent()->id().toString().toLatin1());
                    pMimeData->setData(s_strAttachmentMimeType, pItemAttachment->id().toString().toLatin1());
                    pDrag->setMimeData(pMimeData);
                }
                /* Start dragging: */
                pDrag->exec();
            }
        }
    }
}

void UIMachineSettingsStorage::sltHandleMouseClick(QMouseEvent *pEvent)
{
    /* Make sure event is valid: */
    AssertPtrReturnVoid(pEvent);

    /* Remember last mouse press position: */
    m_mousePressPosition = pEvent->globalPos();

    const QModelIndex index = m_pTreeStorage->indexAt(pEvent->pos());
    const QRect indexRect = m_pTreeStorage->visualRect(index);

    /* Expander icon: */
    if (m_pModelStorage->data(index, StorageModel::R_IsController).toBool())
    {
        QRect expanderRect = m_pModelStorage->data(index, StorageModel::R_ItemPixmapRect).toRect();
        expanderRect.translate(indexRect.x(), indexRect.y());
        if (expanderRect.contains(pEvent->pos()))
        {
            pEvent->setAccepted(true);
            m_pTreeStorage->setExpanded(index, !m_pTreeStorage->isExpanded(index));
            return;
        }
    }

    /* Adder icons: */
    if (m_pModelStorage->data(index, StorageModel::R_IsController).toBool() &&
        m_pTreeStorage->currentIndex() == index)
    {
        const DeviceTypeList devicesList(m_pModelStorage->data(index, StorageModel::R_CtrDevices).value<DeviceTypeList>());
        for (int i = 0; i < devicesList.size(); ++ i)
        {
            const KDeviceType enmDeviceType = devicesList[i];

            QRect deviceRect;
            switch (enmDeviceType)
            {
                case KDeviceType_HardDisk:
                {
                    deviceRect = m_pModelStorage->data(index, StorageModel::R_HDPixmapRect).toRect();
                    break;
                }
                case KDeviceType_DVD:
                {
                    deviceRect = m_pModelStorage->data(index, StorageModel::R_CDPixmapRect).toRect();
                    break;
                }
                case KDeviceType_Floppy:
                {
                    deviceRect = m_pModelStorage->data(index, StorageModel::R_FDPixmapRect).toRect();
                    break;
                }
                default:
                    break;
            }
            deviceRect.translate(indexRect.x() + indexRect.width(), indexRect.y());

            if (deviceRect.contains(pEvent->pos()))
            {
                pEvent->setAccepted(true);
                if (m_pActionAddAttachment->isEnabled())
                    addAttachmentWrapper(enmDeviceType);
                return;
            }
        }
    }
}

void UIMachineSettingsStorage::sltHandleMouseRelease(QMouseEvent *)
{
    /* Forget last mouse press position: */
    m_mousePressPosition = QPoint();
}

void UIMachineSettingsStorage::sltHandleDragEnter(QDragEnterEvent *pEvent)
{
    /* Make sure event is valid: */
    AssertPtrReturnVoid(pEvent);

    /* Accept event but not the proposed action: */
    pEvent->accept();
}

void UIMachineSettingsStorage::sltHandleDragMove(QDragMoveEvent *pEvent)
{
    /* Make sure event is valid: */
    AssertPtrReturnVoid(pEvent);
    /* And mime-data is set: */
    const QMimeData *pMimeData = pEvent->mimeData();
    AssertPtrReturnVoid(pMimeData);

    /* Make sure mime-data format is valid: */
    if (   !pMimeData->hasFormat(UIMachineSettingsStorage::s_strControllerMimeType)
        || !pMimeData->hasFormat(UIMachineSettingsStorage::s_strAttachmentMimeType))
        return;

    /* Get controller/attachment ids: */
    const QString strControllerId = pMimeData->data(UIMachineSettingsStorage::s_strControllerMimeType);
    const QString strAttachmentId = pMimeData->data(UIMachineSettingsStorage::s_strAttachmentMimeType);

    /* Check what item we are hovering currently: */
    QModelIndex index = m_pTreeStorage->indexAt(pEvent->pos());
    AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer());
    /* And make sure this is controller item, we are supporting dropping for this kind only: */
    ControllerItem *pItemController = qobject_cast<ControllerItem*>(pItem);
    if (!pItemController || pItemController->id().toString() == strControllerId)
        return;
    /* Then make sure we support such attachment device type: */
    const DeviceTypeList devicesList(m_pModelStorage->data(index, StorageModel::R_CtrDevices).value<DeviceTypeList>());
    if (!devicesList.contains(m_pModelStorage->attachmentDeviceType(strControllerId, strAttachmentId)))
        return;
    /* Also make sure there is enough place for new attachment: */
    const bool fIsMoreAttachmentsPossible = m_pModelStorage->data(index, StorageModel::R_IsMoreAttachmentsPossible).toBool();
    if (!fIsMoreAttachmentsPossible)
        return;

    /* Accept drag-enter event: */
    pEvent->acceptProposedAction();
}

void UIMachineSettingsStorage::sltHandleDragDrop(QDropEvent *pEvent)
{
    /* Make sure event is valid: */
    AssertPtrReturnVoid(pEvent);
    /* And mime-data is set: */
    const QMimeData *pMimeData = pEvent->mimeData();
    AssertPtrReturnVoid(pMimeData);

    /* Check what item we are hovering currently: */
    QModelIndex index = m_pTreeStorage->indexAt(pEvent->pos());
    AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer());
    /* And make sure this is controller item, we are supporting dropping for this kind only: */
    ControllerItem *pItemController = qobject_cast<ControllerItem*>(pItem);
    if (pItemController)
    {
        /* Get controller/attachment ids: */
        const QString strControllerId = pMimeData->data(UIMachineSettingsStorage::s_strControllerMimeType);
        const QString strAttachmentId = pMimeData->data(UIMachineSettingsStorage::s_strAttachmentMimeType);
        m_pModelStorage->moveAttachment(strAttachmentId, strControllerId, pItemController->id());
    }
}

void UIMachineSettingsStorage::prepare()
{
    /* Apply UI decorations: */
    Ui::UIMachineSettingsStorage::setupUi(this);

    /* Prepare cache: */
    m_pCache = new UISettingsCacheMachineStorage;
    AssertPtrReturnVoid(m_pCache);

    /* Create icon-pool: */
    UIIconPoolStorageSettings::create();

    /* Start full medium-enumeration (if necessary): */
    if (!uiCommon().isFullMediumEnumerationRequested())
        uiCommon().enumerateMedia();

    /* Layout created in the .ui file. */
    AssertPtrReturnVoid(mLtStorage);
    {
#ifdef VBOX_WS_MAC
        /* We need a little more space for the focus rect: */
        mLtStorage->setContentsMargins(3, 0, 3, 0);
        mLtStorage->setSpacing(3);
#else
        mLtStorage->setSpacing(qApp->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing) / 3);
#endif

        /* Prepare storage tree: */
        prepareStorageTree();
        /* Prepare storage toolbar: */
        prepareStorageToolbar();
        /* Prepare storage widgets: */
        prepareStorageWidgets();
        /* Prepare connections: */
        prepareConnections();
    }

    /* Apply language settings: */
    retranslateUi();

    /* Initial setup (after first retranslateUi() call): */
    setMinimumWidth(500);
    mSplitter->setSizes(QList<int>() << (int)(0.45 * minimumWidth()) << (int)(0.55 * minimumWidth()));
}

void UIMachineSettingsStorage::prepareStorageTree()
{
    /* Create storage tree-view: */
    m_pTreeStorage = new QITreeView;
    AssertPtrReturnVoid(m_pTreeStorage);
    AssertPtrReturnVoid(mLsLeftPane);
    {
        /* Configure tree-view: */
        mLsLeftPane->setBuddy(m_pTreeStorage);
        m_pTreeStorage->setMouseTracking(true);
        m_pTreeStorage->setAcceptDrops(true);
        m_pTreeStorage->setContextMenuPolicy(Qt::CustomContextMenu);

        /* Create storage model: */
        m_pModelStorage = new StorageModel(m_pTreeStorage);
        AssertPtrReturnVoid(m_pModelStorage);
        {
            /* Configure model: */
            m_pTreeStorage->setModel(m_pModelStorage);
            m_pTreeStorage->setRootIndex(m_pModelStorage->root());
            m_pTreeStorage->setCurrentIndex(m_pModelStorage->root());
        }

        /* Create storage delegate: */
        StorageDelegate *pStorageDelegate = new StorageDelegate(m_pTreeStorage);
        AssertPtrReturnVoid(pStorageDelegate);
        {
            /* Configure delegate: */
            m_pTreeStorage->setItemDelegate(pStorageDelegate);
        }

        /* Insert tree-view into layout: */
        mLtStorage->insertWidget(0, m_pTreeStorage);
    }
}

void UIMachineSettingsStorage::prepareStorageToolbar()
{
    /* Storage toolbar created in the .ui file. */
    AssertPtrReturnVoid(mTbStorageBar);
    {
        /* Configure toolbar: */
        const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
        mTbStorageBar->setIconSize(QSize(iIconMetric, iIconMetric));

        /* Create 'Add Controller' action: */
        m_pActionAddController = new QAction(this);
        AssertPtrReturnVoid(m_pActionAddController);
        {
            /* Configure action: */
            m_pActionAddController->setIcon(iconPool()->icon(ControllerAddEn, ControllerAddDis));

            /* Add action into toolbar: */
            mTbStorageBar->addAction(m_pActionAddController);
        }

        /* Create 'Add PIIX3 Controller' action: */
        m_addControllerActions[KStorageControllerType_PIIX3] = new QAction(this);
        AssertPtrReturnVoid(m_addControllerActions.value(KStorageControllerType_PIIX3));
        {
            /* Configure action: */
            m_addControllerActions.value(KStorageControllerType_PIIX3)->setIcon(iconPool()->icon(IDEControllerAddEn, IDEControllerAddDis));
        }

        /* Create 'Add PIIX4 Controller' action: */
        m_addControllerActions[KStorageControllerType_PIIX4] = new QAction(this);
        AssertPtrReturnVoid(m_addControllerActions.value(KStorageControllerType_PIIX4));
        {
            /* Configure action: */
            m_addControllerActions.value(KStorageControllerType_PIIX4)->setIcon(iconPool()->icon(IDEControllerAddEn, IDEControllerAddDis));
        }

        /* Create 'Add ICH6 Controller' action: */
        m_addControllerActions[KStorageControllerType_ICH6] = new QAction(this);
        AssertPtrReturnVoid(m_addControllerActions.value(KStorageControllerType_ICH6));
        {
            /* Configure action: */
            m_addControllerActions.value(KStorageControllerType_ICH6)->setIcon(iconPool()->icon(IDEControllerAddEn, IDEControllerAddDis));
        }

        /* Create 'Add AHCI Controller' action: */
        m_addControllerActions[KStorageControllerType_IntelAhci] = new QAction(this);
        AssertPtrReturnVoid(m_addControllerActions.value(KStorageControllerType_IntelAhci));
        {
            /* Configure action: */
            m_addControllerActions.value(KStorageControllerType_IntelAhci)->setIcon(iconPool()->icon(SATAControllerAddEn, SATAControllerAddDis));
        }

        /* Create 'Add LsiLogic Controller' action: */
        m_addControllerActions[KStorageControllerType_LsiLogic] = new QAction(this);
        AssertPtrReturnVoid(m_addControllerActions.value(KStorageControllerType_LsiLogic));
        {
            /* Configure action: */
            m_addControllerActions.value(KStorageControllerType_LsiLogic)->setIcon(iconPool()->icon(SCSIControllerAddEn, SCSIControllerAddDis));
        }

        /* Create 'Add BusLogic Controller' action: */
        m_addControllerActions[KStorageControllerType_BusLogic] = new QAction(this);
        AssertPtrReturnVoid(m_addControllerActions.value(KStorageControllerType_BusLogic));
        {
            /* Configure action: */
            m_addControllerActions.value(KStorageControllerType_BusLogic)->setIcon(iconPool()->icon(SCSIControllerAddEn, SCSIControllerAddDis));
        }

        /* Create 'Add Floppy Controller' action: */
        m_addControllerActions[KStorageControllerType_I82078] = new QAction(this);
        AssertPtrReturnVoid(m_addControllerActions.value(KStorageControllerType_I82078));
        {
            /* Configure action: */
            m_addControllerActions.value(KStorageControllerType_I82078)->setIcon(iconPool()->icon(FloppyControllerAddEn, FloppyControllerAddDis));
        }

        /* Create 'Add LsiLogic SAS Controller' action: */
        m_addControllerActions[KStorageControllerType_LsiLogicSas] = new QAction(this);
        AssertPtrReturnVoid(m_addControllerActions.value(KStorageControllerType_LsiLogicSas));
        {
            /* Configure action: */
            m_addControllerActions.value(KStorageControllerType_LsiLogicSas)->setIcon(iconPool()->icon(SASControllerAddEn, SASControllerAddDis));
        }

        /* Create 'Add USB Controller' action: */
        m_addControllerActions[KStorageControllerType_USB] = new QAction(this);
        AssertPtrReturnVoid(m_addControllerActions.value(KStorageControllerType_USB));
        {
            /* Configure action: */
            m_addControllerActions.value(KStorageControllerType_USB)->setIcon(iconPool()->icon(USBControllerAddEn, USBControllerAddDis));
        }

        /* Create 'Add NVMe Controller' action: */
        m_addControllerActions[KStorageControllerType_NVMe] = new QAction(this);
        AssertPtrReturnVoid(m_addControllerActions.value(KStorageControllerType_NVMe));
        {
            /* Configure action: */
            m_addControllerActions.value(KStorageControllerType_NVMe)->setIcon(iconPool()->icon(NVMeControllerAddEn, NVMeControllerAddDis));
        }

        /* Create 'Add virtio-scsi Controller' action: */
        m_addControllerActions[KStorageControllerType_VirtioSCSI] = new QAction(this);
        AssertPtrReturnVoid(m_addControllerActions.value(KStorageControllerType_VirtioSCSI));
        {
            /* Configure action: */
            m_addControllerActions.value(KStorageControllerType_VirtioSCSI)->setIcon(iconPool()->icon(VirtioSCSIControllerAddEn, VirtioSCSIControllerAddDis));
        }

        /* Create 'Remove Controller' action: */
        m_pActionRemoveController = new QAction(this);
        AssertPtrReturnVoid(m_pActionRemoveController);
        {
            /* Configure action: */
            m_pActionRemoveController->setIcon(iconPool()->icon(ControllerDelEn, ControllerDelDis));

            /* Add action into toolbar: */
            mTbStorageBar->addAction(m_pActionRemoveController);
        }

        /* Create 'Add Attachment' action: */
        m_pActionAddAttachment = new QAction(this);
        AssertPtrReturnVoid(m_pActionAddAttachment);
        {
            /* Configure action: */
            m_pActionAddAttachment->setIcon(iconPool()->icon(AttachmentAddEn, AttachmentAddDis));

            /* Add action into toolbar: */
            mTbStorageBar->addAction(m_pActionAddAttachment);
        }

        /* Create 'Add HD Attachment' action: */
        m_pActionAddAttachmentHD = new QAction(this);
        AssertPtrReturnVoid(m_pActionAddAttachmentHD);
        {
            /* Configure action: */
            m_pActionAddAttachmentHD->setIcon(iconPool()->icon(HDAttachmentAddEn, HDAttachmentAddDis));
        }

        /* Create 'Add CD Attachment' action: */
        m_pActionAddAttachmentCD = new QAction(this);
        AssertPtrReturnVoid(m_pActionAddAttachmentCD);
        {
            /* Configure action: */
            m_pActionAddAttachmentCD->setIcon(iconPool()->icon(CDAttachmentAddEn, CDAttachmentAddDis));
        }

        /* Create 'Add FD Attachment' action: */
        m_pActionAddAttachmentFD = new QAction(this);
        AssertPtrReturnVoid(m_pActionAddAttachmentFD);
        {
            /* Configure action: */
            m_pActionAddAttachmentFD->setIcon(iconPool()->icon(FDAttachmentAddEn, FDAttachmentAddDis));
        }

        /* Create 'Remove Attachment' action: */
        m_pActionRemoveAttachment = new QAction(this);
        AssertPtrReturnVoid(m_pActionRemoveAttachment);
        {
            /* Configure action: */
            m_pActionRemoveAttachment->setIcon(iconPool()->icon(AttachmentDelEn, AttachmentDelDis));

            /* Add action into toolbar: */
            mTbStorageBar->addAction(m_pActionRemoveAttachment);
        }
    }
}

void UIMachineSettingsStorage::prepareStorageWidgets()
{
    /* Open Medium tool-button created in the .ui file. */
    AssertPtrReturnVoid(mTbOpen);
    {
        /* Create Open Medium menu: */
        QMenu *pOpenMediumMenu = new QMenu(mTbOpen);
        AssertPtrReturnVoid(pOpenMediumMenu);
        {
            /* Add menu into tool-button: */
            mTbOpen->setMenu(pOpenMediumMenu);
        }
    }

    /* Other widgets created in the .ui file. */
    AssertPtrReturnVoid(mSbPortCount);
    AssertPtrReturnVoid(mLbHDFormatValue);
    AssertPtrReturnVoid(mLbCDFDTypeValue);
    AssertPtrReturnVoid(mLbHDVirtualSizeValue);
    AssertPtrReturnVoid(mLbHDActualSizeValue);
    AssertPtrReturnVoid(mLbSizeValue);
    AssertPtrReturnVoid(mLbHDDetailsValue);
    AssertPtrReturnVoid(mLbLocationValue);
    AssertPtrReturnVoid(mLbUsageValue);
    AssertPtrReturnVoid(m_pLabelEncryptionValue);
    {
        /* Configure widgets: */
        mSbPortCount->setValue(0);
        mLbHDFormatValue->setFullSizeSelection(true);
        mLbCDFDTypeValue->setFullSizeSelection(true);
        mLbHDVirtualSizeValue->setFullSizeSelection(true);
        mLbHDActualSizeValue->setFullSizeSelection(true);
        mLbSizeValue->setFullSizeSelection(true);
        mLbHDDetailsValue->setFullSizeSelection(true);
        mLbLocationValue->setFullSizeSelection(true);
        mLbUsageValue->setFullSizeSelection(true);
        m_pLabelEncryptionValue->setFullSizeSelection(true);
    }
}

void UIMachineSettingsStorage::prepareConnections()
{
    /* Configure this: */
    connect(&uiCommon(), &UICommon::sigMediumEnumerated,
            this, &UIMachineSettingsStorage::sltHandleMediumEnumerated);
    connect(&uiCommon(), &UICommon::sigMediumDeleted,
            this, &UIMachineSettingsStorage::sltHandleMediumDeleted);

    /* Configure tree-view: */
    connect(m_pTreeStorage, &QITreeView::currentItemChanged,
             this, &UIMachineSettingsStorage::sltHandleCurrentItemChange);
    connect(m_pTreeStorage, &QITreeView::customContextMenuRequested,
            this, &UIMachineSettingsStorage::sltHandleContextMenuRequest);
    connect(m_pTreeStorage, &QITreeView::drawItemBranches,
            this, &UIMachineSettingsStorage::sltHandleDrawItemBranches);
    connect(m_pTreeStorage, &QITreeView::mouseMoved,
            this, &UIMachineSettingsStorage::sltHandleMouseMove);
    connect(m_pTreeStorage, &QITreeView::mousePressed,
            this, &UIMachineSettingsStorage::sltHandleMouseClick);
    connect(m_pTreeStorage, &QITreeView::mouseReleased,
            this, &UIMachineSettingsStorage::sltHandleMouseRelease);
    connect(m_pTreeStorage, &QITreeView::mouseDoubleClicked,
            this, &UIMachineSettingsStorage::sltHandleMouseClick);
    connect(m_pTreeStorage, &QITreeView::dragEntered,
            this, &UIMachineSettingsStorage::sltHandleDragEnter);
    connect(m_pTreeStorage, &QITreeView::dragMoved,
            this, &UIMachineSettingsStorage::sltHandleDragMove);
    connect(m_pTreeStorage, &QITreeView::dragDropped,
            this, &UIMachineSettingsStorage::sltHandleDragDrop);

    /* Create model: */
    connect(m_pModelStorage, &StorageModel::rowsInserted,
            this, &UIMachineSettingsStorage::sltHandleRowInsertion);
    connect(m_pModelStorage, &StorageModel::rowsRemoved,
            this, &UIMachineSettingsStorage::sltHandleRowRemoval);

    /* Configure actions: */
    connect(m_pActionAddController, &QAction::triggered, this, &UIMachineSettingsStorage::sltAddController);
    connect(m_addControllerActions.value(KStorageControllerType_PIIX3), &QAction::triggered, this, &UIMachineSettingsStorage::sltAddControllerPIIX3);
    connect(m_addControllerActions.value(KStorageControllerType_PIIX4), &QAction::triggered, this, &UIMachineSettingsStorage::sltAddControllerPIIX4);
    connect(m_addControllerActions.value(KStorageControllerType_ICH6), &QAction::triggered, this, &UIMachineSettingsStorage::sltAddControllerICH6);
    connect(m_addControllerActions.value(KStorageControllerType_IntelAhci), &QAction::triggered, this, &UIMachineSettingsStorage::sltAddControllerAHCI);
    connect(m_addControllerActions.value(KStorageControllerType_LsiLogic), &QAction::triggered, this, &UIMachineSettingsStorage::sltAddControllerLsiLogic);
    connect(m_addControllerActions.value(KStorageControllerType_BusLogic), &QAction::triggered, this, &UIMachineSettingsStorage::sltAddControllerBusLogic);
    connect(m_addControllerActions.value(KStorageControllerType_I82078), &QAction::triggered, this, &UIMachineSettingsStorage::sltAddControllerFloppy);
    connect(m_addControllerActions.value(KStorageControllerType_LsiLogicSas), &QAction::triggered, this, &UIMachineSettingsStorage::sltAddControllerLsiLogicSAS);
    connect(m_addControllerActions.value(KStorageControllerType_USB), &QAction::triggered, this, &UIMachineSettingsStorage::sltAddControllerUSB);
    connect(m_addControllerActions.value(KStorageControllerType_NVMe), &QAction::triggered, this, &UIMachineSettingsStorage::sltAddControllerNVMe);
    connect(m_addControllerActions.value(KStorageControllerType_VirtioSCSI), &QAction::triggered, this, &UIMachineSettingsStorage::sltAddControllerVirtioSCSI);
    connect(m_pActionRemoveController, &QAction::triggered, this, &UIMachineSettingsStorage::sltRemoveController);
    connect(m_pActionAddAttachment, &QAction::triggered, this, &UIMachineSettingsStorage::sltAddAttachment);
    connect(m_pActionAddAttachmentHD, &QAction::triggered, this, &UIMachineSettingsStorage::sltAddAttachmentHD);
    connect(m_pActionAddAttachmentCD, &QAction::triggered, this, &UIMachineSettingsStorage::sltAddAttachmentCD);
    connect(m_pActionAddAttachmentFD, &QAction::triggered, this, &UIMachineSettingsStorage::sltAddAttachmentFD);
    connect(m_pActionRemoveAttachment, &QAction::triggered, this, &UIMachineSettingsStorage::sltRemoveAttachment);

    /* Configure tool-button: */
    connect(mTbOpen, &QIToolButton::clicked, mTbOpen, &QIToolButton::showMenu);
    /* Configure menu: */
    connect(mTbOpen->menu(), &QMenu::aboutToShow, this, &UIMachineSettingsStorage::sltPrepareOpenMediumMenu);

    /* Configure widgets: */
    connect(m_pMediumIdHolder, &UIMediumIDHolder::sigChanged,
            this, &UIMachineSettingsStorage::sltSetInformation);
    connect(mSbPortCount, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &UIMachineSettingsStorage::sltSetInformation);
    connect(mLeName, &QLineEdit::textEdited,
            this, &UIMachineSettingsStorage::sltSetInformation);
    connect(mCbType, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated),
            this, &UIMachineSettingsStorage::sltSetInformation);
    connect(mCbSlot, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated),
            this, &UIMachineSettingsStorage::sltSetInformation);
    connect(mCbIoCache, &QCheckBox::stateChanged,
            this, &UIMachineSettingsStorage::sltSetInformation);
    connect(mCbPassthrough, &QCheckBox::stateChanged,
            this, &UIMachineSettingsStorage::sltSetInformation);
    connect(mCbTempEject, &QCheckBox::stateChanged,
            this, &UIMachineSettingsStorage::sltSetInformation);
    connect(mCbNonRotational, &QCheckBox::stateChanged,
            this, &UIMachineSettingsStorage::sltSetInformation);
    connect(m_pCheckBoxHotPluggable, &QCheckBox::stateChanged,
            this, &UIMachineSettingsStorage::sltSetInformation);
}

void UIMachineSettingsStorage::cleanup()
{
    /* Destroy icon-pool: */
    UIIconPoolStorageSettings::destroy();

    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

void UIMachineSettingsStorage::addControllerWrapper(const QString &strName, KStorageBus enmBus, KStorageControllerType enmType)
{
#ifdef RT_STRICT
    const QModelIndex index = m_pTreeStorage->currentIndex();
    switch (enmBus)
    {
        case KStorageBus_IDE:
            Assert(m_pModelStorage->data(index, StorageModel::R_IsMoreIDEControllersPossible).toBool());
            break;
        case KStorageBus_SATA:
            Assert(m_pModelStorage->data(index, StorageModel::R_IsMoreSATAControllersPossible).toBool());
            break;
        case KStorageBus_SCSI:
            Assert(m_pModelStorage->data(index, StorageModel::R_IsMoreSCSIControllersPossible).toBool());
            break;
        case KStorageBus_SAS:
            Assert(m_pModelStorage->data(index, StorageModel::R_IsMoreSASControllersPossible).toBool());
            break;
        case KStorageBus_Floppy:
            Assert(m_pModelStorage->data(index, StorageModel::R_IsMoreFloppyControllersPossible).toBool());
            break;
        case KStorageBus_USB:
            Assert(m_pModelStorage->data(index, StorageModel::R_IsMoreUSBControllersPossible).toBool());
            break;
        case KStorageBus_PCIe:
            Assert(m_pModelStorage->data(index, StorageModel::R_IsMoreNVMeControllersPossible).toBool());
            break;
        case KStorageBus_VirtioSCSI:
            Assert(m_pModelStorage->data(index, StorageModel::R_IsMoreVirtioSCSIControllersPossible).toBool());
            break;
        default:
            break;
    }
#endif

    m_pModelStorage->addController(strName, enmBus, enmType);
    emit sigStorageChanged();
}

void UIMachineSettingsStorage::addAttachmentWrapper(KDeviceType enmDeviceType)
{
    const QModelIndex index = m_pTreeStorage->currentIndex();
    Assert(m_pModelStorage->data(index, StorageModel::R_IsController).toBool());
    Assert(m_pModelStorage->data(index, StorageModel::R_IsMoreAttachmentsPossible).toBool());
    // const QString strControllerName(m_pModelStorage->data(index, StorageModel::R_CtrName).toString());
    const QString strMachineFolder(QFileInfo(m_strMachineSettingsFilePath).absolutePath());

    QUuid uMediumId;
    int iResult = uiCommon().openMediumSelectorDialog(this, UIMediumDefs::mediumTypeToLocal(enmDeviceType), uMediumId,
                                                      strMachineFolder, m_strMachineName,
                                                      m_strMachineGuestOSTypeId,
                                                      true /* enable cr1eate action: */, m_uMachineId);

    /* Continue only if iResult is either UIMediumSelector::ReturnCode_Accepted or UIMediumSelector::ReturnCode_LeftEmpty: */
    /* If iResult is UIMediumSelector::ReturnCode_Accepted then we have to have a valid uMediumId: */
    if (iResult == UIMediumSelector::ReturnCode_Rejected ||
        (iResult == UIMediumSelector::ReturnCode_Accepted && uMediumId.isNull()))
        return;

    /* Only DVDs and floppy can be created empty: */
    if (iResult == static_cast<int>(UIMediumSelector::ReturnCode_LeftEmpty) &&
        (enmDeviceType != KDeviceType_DVD && enmDeviceType != KDeviceType_Floppy))
        return;

    m_pModelStorage->addAttachment(QUuid(m_pModelStorage->data(index, StorageModel::R_ItemId).toString()), enmDeviceType, uMediumId);
    m_pModelStorage->sort();
    emit sigStorageChanged();

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsStorage::updateAdditionalDetails(KDeviceType enmType)
{
    mLbHDFormat->setVisible(enmType == KDeviceType_HardDisk);
    mLbHDFormatValue->setVisible(enmType == KDeviceType_HardDisk);

    mLbCDFDType->setVisible(enmType != KDeviceType_HardDisk);
    mLbCDFDTypeValue->setVisible(enmType != KDeviceType_HardDisk);

    mLbHDVirtualSize->setVisible(enmType == KDeviceType_HardDisk);
    mLbHDVirtualSizeValue->setVisible(enmType == KDeviceType_HardDisk);

    mLbHDActualSize->setVisible(enmType == KDeviceType_HardDisk);
    mLbHDActualSizeValue->setVisible(enmType == KDeviceType_HardDisk);

    mLbSize->setVisible(enmType != KDeviceType_HardDisk);
    mLbSizeValue->setVisible(enmType != KDeviceType_HardDisk);

    mLbHDDetails->setVisible(enmType == KDeviceType_HardDisk);
    mLbHDDetailsValue->setVisible(enmType == KDeviceType_HardDisk);

    m_pLabelEncryption->setVisible(enmType == KDeviceType_HardDisk);
    m_pLabelEncryptionValue->setVisible(enmType == KDeviceType_HardDisk);
}

QString UIMachineSettingsStorage::generateUniqueControllerName(const QString &strTemplate) const
{
    int iMaxNumber = 0;
    const QModelIndex rootIndex = m_pModelStorage->root();
    for (int i = 0; i < m_pModelStorage->rowCount(rootIndex); ++i)
    {
        const QModelIndex controllerIndex = rootIndex.child(i, 0);
        const QString strName = m_pModelStorage->data(controllerIndex, StorageModel::R_CtrName).toString();
        if (strName.startsWith(strTemplate))
        {
            const QString strNumber(strName.right(strName.size() - strTemplate.size()));
            bool fConverted = false;
            const int iNumber = strNumber.toInt(&fConverted);
            iMaxNumber = fConverted && (iNumber > iMaxNumber) ? iNumber : 1;
        }
    }
    return iMaxNumber ? QString("%1 %2").arg(strTemplate).arg(++iMaxNumber) : strTemplate;
}

uint32_t UIMachineSettingsStorage::deviceCount(KDeviceType enmType) const
{
    uint32_t cDevices = 0;
    const QModelIndex rootIndex = m_pModelStorage->root();
    for (int i = 0; i < m_pModelStorage->rowCount(rootIndex); ++i)
    {
        const QModelIndex controllerIndex = rootIndex.child(i, 0);
        for (int j = 0; j < m_pModelStorage->rowCount(controllerIndex); ++j)
        {
            const QModelIndex attachmentIndex = controllerIndex.child(j, 0);
            const KDeviceType enmDeviceType = m_pModelStorage->data(attachmentIndex, StorageModel::R_AttDevice).value<KDeviceType>();
            if (enmDeviceType == enmType)
                ++cDevices;
        }
    }

    return cDevices;
}

void UIMachineSettingsStorage::addChooseExistingMediumAction(QMenu *pOpenMediumMenu, const QString &strActionName)
{
    QAction *pChooseExistingMedium = pOpenMediumMenu->addAction(strActionName);
    pChooseExistingMedium->setIcon(iconPool()->icon(ChooseExistingEn, ChooseExistingDis));
    connect(pChooseExistingMedium, &QAction::triggered, this, &UIMachineSettingsStorage::sltChooseExistingMedium);
}

void UIMachineSettingsStorage::addChooseDiskFileAction(QMenu *pOpenMediumMenu, const QString &strActionName)
{
    QAction *pChooseDiskFile = pOpenMediumMenu->addAction(strActionName);
    pChooseDiskFile->setIcon(iconPool()->icon(ChooseExistingEn, ChooseExistingDis));
    connect(pChooseDiskFile, &QAction::triggered, this, &UIMachineSettingsStorage::sltChooseDiskFile);
}

void UIMachineSettingsStorage::addChooseHostDriveActions(QMenu *pOpenMediumMenu)
{
    foreach (const QUuid &uMediumId, uiCommon().mediumIDs())
    {
        const UIMedium guiMedium = uiCommon().medium(uMediumId);
        if (guiMedium.isHostDrive() && m_pMediumIdHolder->type() == guiMedium.type())
        {
            QAction *pHostDriveAction = pOpenMediumMenu->addAction(guiMedium.name());
            pHostDriveAction->setData(guiMedium.id());
            connect(pHostDriveAction, &QAction::triggered, this, &UIMachineSettingsStorage::sltChooseHostDrive);
        }
    }
}

void UIMachineSettingsStorage::addRecentMediumActions(QMenu *pOpenMediumMenu, UIMediumDeviceType enmRecentMediumType)
{
    /* Get recent-medium list: */
    QStringList recentMediumList;
    switch (enmRecentMediumType)
    {
        case UIMediumDeviceType_HardDisk: recentMediumList = gEDataManager->recentListOfHardDrives(); break;
        case UIMediumDeviceType_DVD:      recentMediumList = gEDataManager->recentListOfOpticalDisks(); break;
        case UIMediumDeviceType_Floppy:   recentMediumList = gEDataManager->recentListOfFloppyDisks(); break;
        default: break;
    }
    /* For every list-item: */
    for (int iIndex = 0; iIndex < recentMediumList.size(); ++iIndex)
    {
        /* Prepare corresponding action: */
        const QString &strRecentMediumLocation = recentMediumList.at(iIndex);
        if (QFile::exists(strRecentMediumLocation))
        {
            QAction *pChooseRecentMediumAction = pOpenMediumMenu->addAction(QFileInfo(strRecentMediumLocation).fileName(),
                                                                            this, SLOT(sltChooseRecentMedium()));
            pChooseRecentMediumAction->setData(QString("%1,%2").arg(enmRecentMediumType).arg(strRecentMediumLocation));
        }
    }
}

bool UIMachineSettingsStorage::saveStorageData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save storage settings from the cache: */
    if (fSuccess && isMachineInValidMode() && m_pCache->wasChanged())
    {
        /* For each controller ('removing' step): */
        // We need to separately remove controllers first because
        // there could be limited amount of controllers available.
        for (int iControllerIndex = 0; fSuccess && iControllerIndex < m_pCache->childCount(); ++iControllerIndex)
        {
            /* Get controller cache: */
            const UISettingsCacheMachineStorageController &controllerCache = m_pCache->child(iControllerIndex);

            /* Remove controller marked for 'remove' or 'update' (if it can't be updated): */
            if (controllerCache.wasRemoved() || (controllerCache.wasUpdated() && !isControllerCouldBeUpdated(controllerCache)))
                fSuccess = removeStorageController(controllerCache);
        }

        /* For each controller ('updating' step): */
        // We need to separately update controllers first because
        // attachments should be removed/updated/created same separate way.
        for (int iControllerIndex = 0; fSuccess && iControllerIndex < m_pCache->childCount(); ++iControllerIndex)
        {
            /* Get controller cache: */
            const UISettingsCacheMachineStorageController &controllerCache = m_pCache->child(iControllerIndex);

            /* Update controller marked for 'update' (if it can be updated): */
            if (controllerCache.wasUpdated() && isControllerCouldBeUpdated(controllerCache))
                fSuccess = updateStorageController(controllerCache, true);
        }
        for (int iControllerIndex = 0; fSuccess && iControllerIndex < m_pCache->childCount(); ++iControllerIndex)
        {
            /* Get controller cache: */
            const UISettingsCacheMachineStorageController &controllerCache = m_pCache->child(iControllerIndex);

            /* Update controller marked for 'update' (if it can be updated): */
            if (controllerCache.wasUpdated() && isControllerCouldBeUpdated(controllerCache))
                fSuccess = updateStorageController(controllerCache, false);
        }

        /* For each controller ('creating' step): */
        // Finally we are creating new controllers,
        // with attachments which were released for sure.
        for (int iControllerIndex = 0; fSuccess && iControllerIndex < m_pCache->childCount(); ++iControllerIndex)
        {
            /* Get controller cache: */
            const UISettingsCacheMachineStorageController &controllerCache = m_pCache->child(iControllerIndex);

            /* Create controller marked for 'create' or 'update' (if it can't be updated): */
            if (controllerCache.wasCreated() || (controllerCache.wasUpdated() && !isControllerCouldBeUpdated(controllerCache)))
                fSuccess = createStorageController(controllerCache);
        }
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsStorage::removeStorageController(const UISettingsCacheMachineStorageController &controllerCache)
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Remove controller: */
    if (fSuccess && isMachineOffline())
    {
        /* Get old controller data from the cache: */
        const UIDataSettingsMachineStorageController &oldControllerData = controllerCache.base();

        /* Search for a controller with the same name: */
        const CStorageController &comController = m_machine.GetStorageControllerByName(oldControllerData.m_strName);
        fSuccess = m_machine.isOk() && comController.isNotNull();

        /* Make sure controller really exists: */
        if (fSuccess)
        {
            /* Remove controller with all the attachments at one shot: */
            m_machine.RemoveStorageController(oldControllerData.m_strName);
            fSuccess = m_machine.isOk();
        }

        /* Show error message if necessary: */
        if (!fSuccess)
            notifyOperationProgressError(UIErrorString::formatErrorInfo(m_machine));
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsStorage::createStorageController(const UISettingsCacheMachineStorageController &controllerCache)
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Create controller: */
    if (fSuccess && isMachineOffline())
    {
        /* Get new controller data from the cache: */
        const UIDataSettingsMachineStorageController &newControllerData = controllerCache.data();

        /* Search for a controller with the same name: */
        const CMachine comMachine(m_machine);
        CStorageController comController = comMachine.GetStorageControllerByName(newControllerData.m_strName);
        fSuccess = !comMachine.isOk() && comController.isNull();
        AssertReturn(fSuccess, false);

        /* Make sure controller doesn't exist: */
        if (fSuccess)
        {
            /* Create controller: */
            comController = m_machine.AddStorageController(newControllerData.m_strName, newControllerData.m_enmBus);
            fSuccess = m_machine.isOk() && comController.isNotNull();
        }

        /* Show error message if necessary: */
        if (!fSuccess)
            notifyOperationProgressError(UIErrorString::formatErrorInfo(m_machine));
        else
        {
            /* Save controller type: */
            if (fSuccess)
            {
                comController.SetControllerType(newControllerData.m_enmType);
                fSuccess = comController.isOk();
            }
            /* Save whether controller uses host IO cache: */
            if (fSuccess)
            {
                comController.SetUseHostIOCache(newControllerData.m_fUseHostIOCache);
                fSuccess = comController.isOk();
            }
            /* Save controller port number: */
            if (   fSuccess
                && (   newControllerData.m_enmBus == KStorageBus_SATA
                    || newControllerData.m_enmBus == KStorageBus_SAS
                    || newControllerData.m_enmBus == KStorageBus_PCIe
                    || newControllerData.m_enmBus == KStorageBus_VirtioSCSI))
            {
                ULONG uNewPortCount = newControllerData.m_uPortCount;
                if (fSuccess)
                {
                    uNewPortCount = qMax(uNewPortCount, comController.GetMinPortCount());
                    fSuccess = comController.isOk();
                }
                if (fSuccess)
                {
                    uNewPortCount = qMin(uNewPortCount, comController.GetMaxPortCount());
                    fSuccess = comController.isOk();
                }
                if (fSuccess)
                {
                    comController.SetPortCount(uNewPortCount);
                    fSuccess = comController.isOk();
                }
            }

            /* Show error message if necessary: */
            if (!fSuccess)
                notifyOperationProgressError(UIErrorString::formatErrorInfo(comController));

            /* For each attachment: */
            for (int iAttachmentIndex = 0; fSuccess && iAttachmentIndex < controllerCache.childCount(); ++iAttachmentIndex)
            {
                /* Get attachment cache: */
                const UISettingsCacheMachineStorageAttachment &attachmentCache = controllerCache.child(iAttachmentIndex);

                /* Create attachment if it was not 'removed': */
                if (!attachmentCache.wasRemoved())
                    fSuccess = createStorageAttachment(controllerCache, attachmentCache);
            }
        }
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsStorage::updateStorageController(const UISettingsCacheMachineStorageController &controllerCache,
                                                       bool fRemovingStep)
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Update controller: */
    if (fSuccess)
    {
        /* Get old controller data from the cache: */
        const UIDataSettingsMachineStorageController &oldControllerData = controllerCache.base();
        /* Get new controller data from the cache: */
        const UIDataSettingsMachineStorageController &newControllerData = controllerCache.data();

        /* Search for a controller with the same name: */
        CStorageController comController = m_machine.GetStorageControllerByName(oldControllerData.m_strName);
        fSuccess = m_machine.isOk() && comController.isNotNull();

        /* Show error message if necessary: */
        if (!fSuccess)
            notifyOperationProgressError(UIErrorString::formatErrorInfo(m_machine));
        else
        {
            /* Save controller type: */
            if (fSuccess && newControllerData.m_enmType != oldControllerData.m_enmType)
            {
                comController.SetControllerType(newControllerData.m_enmType);
                fSuccess = comController.isOk();
            }
            /* Save whether controller uses IO cache: */
            if (fSuccess && newControllerData.m_fUseHostIOCache != oldControllerData.m_fUseHostIOCache)
            {
                comController.SetUseHostIOCache(newControllerData.m_fUseHostIOCache);
                fSuccess = comController.isOk();
            }
            /* Save controller port number: */
            if (   fSuccess
                && newControllerData.m_uPortCount != oldControllerData.m_uPortCount
                && (   newControllerData.m_enmBus == KStorageBus_SATA
                    || newControllerData.m_enmBus == KStorageBus_SAS
                    || newControllerData.m_enmBus == KStorageBus_PCIe
                    || newControllerData.m_enmBus == KStorageBus_VirtioSCSI))
            {
                ULONG uNewPortCount = newControllerData.m_uPortCount;
                if (fSuccess)
                {
                    uNewPortCount = qMax(uNewPortCount, comController.GetMinPortCount());
                    fSuccess = comController.isOk();
                }
                if (fSuccess)
                {
                    uNewPortCount = qMin(uNewPortCount, comController.GetMaxPortCount());
                    fSuccess = comController.isOk();
                }
                if (fSuccess)
                {
                    comController.SetPortCount(uNewPortCount);
                    fSuccess = comController.isOk();
                }
            }

            /* Show error message if necessary: */
            if (!fSuccess)
                notifyOperationProgressError(UIErrorString::formatErrorInfo(comController));

            // We need to separately remove attachments first because
            // there could be limited amount of attachments or media available.
            if (fRemovingStep)
            {
                /* For each attachment ('removing' step): */
                for (int iAttachmentIndex = 0; fSuccess && iAttachmentIndex < controllerCache.childCount(); ++iAttachmentIndex)
                {
                    /* Get attachment cache: */
                    const UISettingsCacheMachineStorageAttachment &attachmentCache = controllerCache.child(iAttachmentIndex);

                    /* Remove attachment marked for 'remove' or 'update' (if it can't be updated): */
                    if (attachmentCache.wasRemoved() || (attachmentCache.wasUpdated() && !isAttachmentCouldBeUpdated(attachmentCache)))
                        fSuccess = removeStorageAttachment(controllerCache, attachmentCache);
                }
            }
            else
            {
                /* For each attachment ('creating' step): */
                for (int iAttachmentIndex = 0; fSuccess && iAttachmentIndex < controllerCache.childCount(); ++iAttachmentIndex)
                {
                    /* Get attachment cache: */
                    const UISettingsCacheMachineStorageAttachment &attachmentCache = controllerCache.child(iAttachmentIndex);

                    /* Create attachment marked for 'create' or 'update' (if it can't be updated): */
                    if (attachmentCache.wasCreated() || (attachmentCache.wasUpdated() && !isAttachmentCouldBeUpdated(attachmentCache)))
                        fSuccess = createStorageAttachment(controllerCache, attachmentCache);

                    else

                    /* Update attachment marked for 'update' (if it can be updated): */
                    if (attachmentCache.wasUpdated() && isAttachmentCouldBeUpdated(attachmentCache))
                        fSuccess = updateStorageAttachment(controllerCache, attachmentCache);
                }
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
    bool fSuccess = true;
    /* Remove attachment: */
    if (fSuccess)
    {
        /* Get old controller data from the cache: */
        const UIDataSettingsMachineStorageController &oldControllerData = controllerCache.base();
        /* Get old attachment data from the cache: */
        const UIDataSettingsMachineStorageAttachment &oldAttachmentData = attachmentCache.base();

        /* Search for an attachment with the same parameters: */
        const CMediumAttachment &comAttachment = m_machine.GetMediumAttachment(oldControllerData.m_strName,
                                                                               oldAttachmentData.m_iPort,
                                                                               oldAttachmentData.m_iDevice);
        fSuccess = m_machine.isOk() && comAttachment.isNotNull();

        /* Make sure attachment really exists: */
        if (fSuccess)
        {
            /* Remove attachment: */
            m_machine.DetachDevice(oldControllerData.m_strName,
                                   oldAttachmentData.m_iPort,
                                   oldAttachmentData.m_iDevice);
            fSuccess = m_machine.isOk();
        }

        /* Show error message if necessary: */
        if (!fSuccess)
            notifyOperationProgressError(UIErrorString::formatErrorInfo(m_machine));
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsStorage::createStorageAttachment(const UISettingsCacheMachineStorageController &controllerCache,
                                                       const UISettingsCacheMachineStorageAttachment &attachmentCache)
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Create attachment: */
    if (fSuccess)
    {
        /* Get new controller data from the cache: */
        const UIDataSettingsMachineStorageController &newControllerData = controllerCache.data();
        /* Get new attachment data from the cache: */
        const UIDataSettingsMachineStorageAttachment &newAttachmentData = attachmentCache.data();

        /* Search for an attachment with the same parameters: */
        const CMachine comMachine(m_machine);
        const CMediumAttachment &comAttachment = comMachine.GetMediumAttachment(newControllerData.m_strName,
                                                                                newAttachmentData.m_iPort,
                                                                                newAttachmentData.m_iDevice);
        fSuccess = !comMachine.isOk() && comAttachment.isNull();
        AssertReturn(fSuccess, false);

        /* Make sure attachment doesn't exist: */
        if (fSuccess)
        {
            /* Create attachment: */
            const UIMedium vboxMedium = uiCommon().medium(newAttachmentData.m_uMediumId);
            const CMedium comMedium = vboxMedium.medium();
            m_machine.AttachDevice(newControllerData.m_strName,
                                   newAttachmentData.m_iPort,
                                   newAttachmentData.m_iDevice,
                                   newAttachmentData.m_enmDeviceType,
                                   comMedium);
            fSuccess = m_machine.isOk();
        }

        if (newAttachmentData.m_enmDeviceType == KDeviceType_DVD)
        {
            /* Save whether this is a passthrough device: */
            if (fSuccess && isMachineOffline())
            {
                m_machine.PassthroughDevice(newControllerData.m_strName,
                                            newAttachmentData.m_iPort,
                                            newAttachmentData.m_iDevice,
                                            newAttachmentData.m_fPassthrough);
                fSuccess = m_machine.isOk();
            }
            /* Save whether this is a live cd device: */
            if (fSuccess)
            {
                m_machine.TemporaryEjectDevice(newControllerData.m_strName,
                                               newAttachmentData.m_iPort,
                                               newAttachmentData.m_iDevice,
                                               newAttachmentData.m_fTempEject);
                fSuccess = m_machine.isOk();
            }
        }
        else if (newAttachmentData.m_enmDeviceType == KDeviceType_HardDisk)
        {
            /* Save whether this is a ssd device: */
            if (fSuccess && isMachineOffline())
            {
                m_machine.NonRotationalDevice(newControllerData.m_strName,
                                              newAttachmentData.m_iPort,
                                              newAttachmentData.m_iDevice,
                                              newAttachmentData.m_fNonRotational);
                fSuccess = m_machine.isOk();
            }
        }

        if (newControllerData.m_enmBus == KStorageBus_SATA || newControllerData.m_enmBus == KStorageBus_USB)
        {
            /* Save whether this device is hot-pluggable: */
            if (fSuccess && isMachineOffline())
            {
                m_machine.SetHotPluggableForDevice(newControllerData.m_strName,
                                                   newAttachmentData.m_iPort,
                                                   newAttachmentData.m_iDevice,
                                                   newAttachmentData.m_fHotPluggable);
                fSuccess = m_machine.isOk();
            }
        }

        /* Show error message if necessary: */
        if (!fSuccess)
            notifyOperationProgressError(UIErrorString::formatErrorInfo(m_machine));
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsStorage::updateStorageAttachment(const UISettingsCacheMachineStorageController &controllerCache,
                                                       const UISettingsCacheMachineStorageAttachment &attachmentCache)
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Update attachment: */
    if (fSuccess)
    {
        /* Get new controller data from the cache: */
        const UIDataSettingsMachineStorageController &newControllerData = controllerCache.data();
        /* Get new attachment data from the cache: */
        const UIDataSettingsMachineStorageAttachment &newAttachmentData = attachmentCache.data();

        /* Search for an attachment with the same parameters: */
        const CMediumAttachment &comAttachment = m_machine.GetMediumAttachment(newControllerData.m_strName,
                                                                               newAttachmentData.m_iPort,
                                                                               newAttachmentData.m_iDevice);
        fSuccess = m_machine.isOk() && comAttachment.isNotNull();

        /* Make sure attachment doesn't exist: */
        if (fSuccess)
        {
            /* Remount attachment: */
            const UIMedium vboxMedium = uiCommon().medium(newAttachmentData.m_uMediumId);
            const CMedium comMedium = vboxMedium.medium();
            m_machine.MountMedium(newControllerData.m_strName,
                                  newAttachmentData.m_iPort,
                                  newAttachmentData.m_iDevice,
                                  comMedium,
                                  true /* force? */);
            fSuccess = m_machine.isOk();
        }

        if (newAttachmentData.m_enmDeviceType == KDeviceType_DVD)
        {
            /* Save whether this is a passthrough device: */
            if (fSuccess && isMachineOffline())
            {
                m_machine.PassthroughDevice(newControllerData.m_strName,
                                            newAttachmentData.m_iPort,
                                            newAttachmentData.m_iDevice,
                                            newAttachmentData.m_fPassthrough);
                fSuccess = m_machine.isOk();
            }
            /* Save whether this is a live cd device: */
            if (fSuccess)
            {
                m_machine.TemporaryEjectDevice(newControllerData.m_strName,
                                               newAttachmentData.m_iPort,
                                               newAttachmentData.m_iDevice,
                                               newAttachmentData.m_fTempEject);
                fSuccess = m_machine.isOk();
            }
        }
        else if (newAttachmentData.m_enmDeviceType == KDeviceType_HardDisk)
        {
            /* Save whether this is a ssd device: */
            if (fSuccess && isMachineOffline())
            {
                m_machine.NonRotationalDevice(newControllerData.m_strName,
                                              newAttachmentData.m_iPort,
                                              newAttachmentData.m_iDevice,
                                              newAttachmentData.m_fNonRotational);
                fSuccess = m_machine.isOk();
            }
        }

        if (newControllerData.m_enmBus == KStorageBus_SATA || newControllerData.m_enmBus == KStorageBus_USB)
        {
            /* Save whether this device is hot-pluggable: */
            if (fSuccess && isMachineOffline())
            {
                m_machine.SetHotPluggableForDevice(newControllerData.m_strName,
                                                   newAttachmentData.m_iPort,
                                                   newAttachmentData.m_iDevice,
                                                   newAttachmentData.m_fHotPluggable);
                fSuccess = m_machine.isOk();
            }
        }

        /* Show error message if necessary: */
        if (!fSuccess)
            notifyOperationProgressError(UIErrorString::formatErrorInfo(m_machine));
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsStorage::isControllerCouldBeUpdated(const UISettingsCacheMachineStorageController &controllerCache) const
{
    /* IController interface doesn't allow to change 'bus' attribute but allows
     * to change 'name' attribute which can conflict with another one controller.
     * Both those attributes could be changed in GUI directly or indirectly.
     * For such cases we have to recreate IController instance,
     * for other cases we will update controller attributes only. */
    const UIDataSettingsMachineStorageController &oldControllerData = controllerCache.base();
    const UIDataSettingsMachineStorageController &newControllerData = controllerCache.data();
    return true
           && (newControllerData.m_strName == oldControllerData.m_strName)
           && (newControllerData.m_enmBus == oldControllerData.m_enmBus)
           ;
}

bool UIMachineSettingsStorage::isAttachmentCouldBeUpdated(const UISettingsCacheMachineStorageAttachment &attachmentCache) const
{
    /* IMediumAttachment could be indirectly updated through IMachine
     * only if attachment type, device and port were NOT changed and is one of the next types:
     * KDeviceType_Floppy or KDeviceType_DVD.
     * For other cases we will recreate attachment fully: */
    const UIDataSettingsMachineStorageAttachment &oldAttachmentData = attachmentCache.base();
    const UIDataSettingsMachineStorageAttachment &newAttachmentData = attachmentCache.data();
    return true
           && (newAttachmentData.m_enmDeviceType == oldAttachmentData.m_enmDeviceType)
           && (newAttachmentData.m_iPort == oldAttachmentData.m_iPort)
           && (newAttachmentData.m_iDevice == oldAttachmentData.m_iDevice)
           && (newAttachmentData.m_enmDeviceType == KDeviceType_Floppy || newAttachmentData.m_enmDeviceType == KDeviceType_DVD)
           ;
}


# include "UIMachineSettingsStorage.moc"
