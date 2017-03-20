/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsStorage class declaration.
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

#ifndef __UIMachineSettingsStorage_h__
#define __UIMachineSettingsStorage_h__

/* Qt includes: */
#ifdef VBOX_WS_MAC
/* Somewhere Carbon.h includes AssertMacros.h which defines the macro "check".
 * In QItemDelegate a class method is called "check" also. As we not used the
 * macro undefine it here. */
# undef check
#endif /* VBOX_WS_MAC */
#include <QItemDelegate>
#include <QPointer>

/* GUI includes: */
#include "QITreeView.h"
#include "UISettingsPage.h"
#include "UIMachineSettingsStorage.gen.h"

/* Forward declarations: */
class AttachmentItem;
class ControllerItem;
class UIMediumIDHolder;

/* Internal Types */
typedef QList <StorageSlot> SlotsList;
typedef QList <KDeviceType> DeviceTypeList;
typedef QList <KStorageControllerType> ControllerTypeList;
Q_DECLARE_METATYPE (SlotsList);
Q_DECLARE_METATYPE (DeviceTypeList);
Q_DECLARE_METATYPE (ControllerTypeList);

/** Known item states. */
enum ItemState
{
    State_DefaultItem,
    State_CollapsedItem,
    State_ExpandedItem,
    State_MAX
};

/** Known pixmap types. */
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
    USBControllerNormal,
    USBControllerExpand,
    USBControllerCollapse,
    NVMeControllerNormal,
    NVMeControllerExpand,
    NVMeControllerCollapse,
    FloppyControllerNormal,
    FloppyControllerExpand,
    FloppyControllerCollapse,

    IDEControllerAddEn,
    IDEControllerAddDis,
    SATAControllerAddEn,
    SATAControllerAddDis,
    SCSIControllerAddEn,
    SCSIControllerAddDis,
    USBControllerAddEn,
    USBControllerAddDis,
    NVMeControllerAddEn,
    NVMeControllerAddDis,
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
    HDNewEn,
    HDNewDis,
    CDUnmountEnabled,
    CDUnmountDisabled,
    FDUnmountEnabled,
    FDUnmountDisabled,

    MaxIndex
};

/* Abstract Controller Type */
class AbstractControllerType
{
public:

    AbstractControllerType (KStorageBus aBusType, KStorageControllerType aCtrType);
    virtual ~AbstractControllerType() {}

    KStorageBus busType() const;
    KStorageControllerType ctrType() const;
    ControllerTypeList ctrTypes() const;
    PixmapType pixmap(ItemState aState) const;

    void setCtrType (KStorageControllerType aCtrType);

    DeviceTypeList deviceTypeList() const;

protected:

    virtual KStorageControllerType first() const = 0;
    virtual uint size() const = 0;

    KStorageBus mBusType;
    KStorageControllerType mCtrType;
    QList<PixmapType> mPixmaps;
};

/* IDE Controller Type */
class IDEControllerType : public AbstractControllerType
{
public:

    IDEControllerType (KStorageControllerType aSubType);

private:

    KStorageControllerType first() const;
    uint size() const;
};

/* SATA Controller Type */
class SATAControllerType : public AbstractControllerType
{
public:

    SATAControllerType (KStorageControllerType aSubType);

private:

    KStorageControllerType first() const;
    uint size() const;
};

/* SCSI Controller Type */
class SCSIControllerType : public AbstractControllerType
{
public:

    SCSIControllerType (KStorageControllerType aSubType);

private:

    KStorageControllerType first() const;
    uint size() const;
};

/* Floppy Controller Type */
class FloppyControllerType : public AbstractControllerType
{
public:

    FloppyControllerType (KStorageControllerType aSubType);

private:

    KStorageControllerType first() const;
    uint size() const;
};

/* SAS Controller Type */
class SASControllerType : public AbstractControllerType
{
public:

    SASControllerType (KStorageControllerType aSubType);

private:

    KStorageControllerType first() const;
    uint size() const;
};

/* USB Controller Type */
class USBStorageControllerType : public AbstractControllerType
{
public:

    USBStorageControllerType (KStorageControllerType aSubType);

private:

    KStorageControllerType first() const;
    uint size() const;
};

/* NVMe Controller Type */
class NVMeStorageControllerType : public AbstractControllerType
{
public:

    NVMeStorageControllerType (KStorageControllerType aSubType);

private:

    KStorageControllerType first() const;
    uint size() const;
};

/* Abstract Item */
class AbstractItem : public QITreeViewItem
{
    Q_OBJECT;

public:

    enum ItemType
    {
        Type_InvalidItem    = 0,
        Type_RootItem       = 1,
        Type_ControllerItem = 2,
        Type_AttachmentItem = 3
    };

    AbstractItem(QITreeView *pParent);
    AbstractItem(AbstractItem *pParentItem);
    virtual ~AbstractItem();

    AbstractItem* parent() const;
    QUuid id() const;
    QString machineId() const;

    void setMachineId (const QString &aMchineId);

    virtual ItemType rtti() const = 0;
    virtual AbstractItem* childItem (int aIndex) const = 0;
    virtual AbstractItem* childItemById (const QUuid &aId) const = 0;
    virtual int posOfChild (AbstractItem *aItem) const = 0;
    virtual QString tip() const = 0;
    virtual QPixmap pixmap (ItemState aState = State_DefaultItem) = 0;

protected:

    virtual void addChild (AbstractItem *aItem) = 0;
    virtual void delChild (AbstractItem *aItem) = 0;

    AbstractItem *m_pParentItem;
    QUuid         mId;
    QString       mMachineId;
};
Q_DECLARE_METATYPE (AbstractItem::ItemType);

/* Root Item */
class RootItem : public AbstractItem
{
public:

    RootItem(QITreeView *pParent);
   ~RootItem();

    ULONG childCount (KStorageBus aBus) const;

private:

    ItemType rtti() const;
    AbstractItem* childItem (int aIndex) const;
    AbstractItem* childItemById (const QUuid &aId) const;
    int posOfChild (AbstractItem *aItem) const;
    int childCount() const;
    QString text() const;
    QString tip() const;
    QPixmap pixmap (ItemState aState);
    void addChild (AbstractItem *aItem);
    void delChild (AbstractItem *aItem);

    QList <AbstractItem*> mControllers;
};

/* Controller Item */
class ControllerItem : public AbstractItem
{
public:

    ControllerItem (AbstractItem *aParent, const QString &aName, KStorageBus aBusType,
                    KStorageControllerType aControllerType);
   ~ControllerItem();

    KStorageBus ctrBusType() const;
    QString ctrName() const;
    KStorageControllerType ctrType() const;
    ControllerTypeList ctrTypes() const;
    uint portCount();
    uint maxPortCount();
    bool ctrUseIoCache() const;

    void setCtrName (const QString &aCtrName);
    void setCtrType (KStorageControllerType aCtrType);
    void setPortCount (uint aPortCount);
    void setCtrUseIoCache (bool aUseIoCache);

    SlotsList ctrAllSlots() const;
    SlotsList ctrUsedSlots() const;
    DeviceTypeList ctrDeviceTypeList() const;

    void setAttachments(const QList<AbstractItem*> &attachments) { mAttachments = attachments; }

private:

    ItemType rtti() const;
    AbstractItem* childItem (int aIndex) const;
    AbstractItem* childItemById (const QUuid &aId) const;
    int posOfChild (AbstractItem *aItem) const;
    int childCount() const;
    QString text() const;
    QString tip() const;
    QPixmap pixmap (ItemState aState);
    void addChild (AbstractItem *aItem);
    void delChild (AbstractItem *aItem);

    QString mCtrName;
    AbstractControllerType *mCtrType;
    uint mPortCount;
    bool mUseIoCache;
    QList <AbstractItem*> mAttachments;
};

/* Attachment Item */
class AttachmentItem : public AbstractItem
{
public:

    AttachmentItem (AbstractItem *aParent, KDeviceType aDeviceType);

    StorageSlot attSlot() const;
    SlotsList attSlots() const;
    KDeviceType attDeviceType() const;
    DeviceTypeList attDeviceTypes() const;
    QString attMediumId() const;
    bool attIsHostDrive() const;
    bool attIsPassthrough() const;
    bool attIsTempEject() const;
    bool attIsNonRotational() const;
    bool attIsHotPluggable() const;

    void setAttSlot (const StorageSlot &aAttSlot);
    void setAttDevice (KDeviceType aAttDeviceType);
    void setAttMediumId (const QString &aAttMediumId);
    void setAttIsPassthrough (bool aPassthrough);
    void setAttIsTempEject (bool aTempEject);
    void setAttIsNonRotational (bool aNonRotational);
    void setAttIsHotPluggable(bool fIsHotPluggable);

    QString attSize() const;
    QString attLogicalSize() const;
    QString attLocation() const;
    QString attFormat() const;
    QString attDetails() const;
    QString attUsage() const;
    QString attEncryptionPasswordID() const;

private:

    void cache();

    ItemType rtti() const;
    AbstractItem* childItem (int aIndex) const;
    AbstractItem* childItemById (const QUuid &aId) const;
    int posOfChild (AbstractItem *aItem) const;
    int childCount() const;
    QString text() const;
    QString tip() const;
    QPixmap pixmap (ItemState aState);
    void addChild (AbstractItem *aItem);
    void delChild (AbstractItem *aItem);

    KDeviceType mAttDeviceType;

    StorageSlot mAttSlot;
    QString mAttMediumId;
    bool mAttIsHostDrive;
    bool mAttIsPassthrough;
    bool mAttIsTempEject;
    bool mAttIsNonRotational;
    bool m_fIsHotPluggable;

    QString mAttName;
    QString mAttTip;
    QPixmap mAttPixmap;

    QString mAttSize;
    QString mAttLogicalSize;
    QString mAttLocation;
    QString mAttFormat;
    QString mAttDetails;
    QString mAttUsage;
    QString m_strAttEncryptionPasswordID;
};

/* Storage Model */
class StorageModel : public QAbstractItemModel
{
    Q_OBJECT;

public:

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
        R_IsMoreAttachmentsPossible,

        R_CtrName,
        R_CtrType,
        R_CtrTypes,
        R_CtrDevices,
        R_CtrBusType,
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

    enum ToolTipType
    {
        DefaultToolTip  = 0,
        ExpanderToolTip = 1,
        HDAdderToolTip  = 2,
        CDAdderToolTip  = 3,
        FDAdderToolTip  = 4
    };

    StorageModel(QITreeView *pParent);
   ~StorageModel();

    int rowCount (const QModelIndex &aParent = QModelIndex()) const;
    int columnCount (const QModelIndex &aParent = QModelIndex()) const;

    QModelIndex root() const;
    QModelIndex index (int aRow, int aColumn, const QModelIndex &aParent = QModelIndex()) const;
    QModelIndex parent (const QModelIndex &aIndex) const;

    QVariant data (const QModelIndex &aIndex, int aRole) const;
    bool setData (const QModelIndex &aIndex, const QVariant &aValue, int aRole);

    QModelIndex addController (const QString &aCtrName, KStorageBus aBusType, KStorageControllerType aCtrType);
    void delController (const QUuid &aCtrId);

    QModelIndex addAttachment (const QUuid &aCtrId, KDeviceType aDeviceType, const QString &strMediumId);
    void delAttachment (const QUuid &aCtrId, const QUuid &aAttId);

    void setMachineId (const QString &aMachineId);

    void sort(int iColumn = 0, Qt::SortOrder order = Qt::AscendingOrder);
    QModelIndex attachmentBySlot(QModelIndex controllerIndex, StorageSlot attachmentStorageSlot);

    KChipsetType chipsetType() const;
    void setChipsetType(KChipsetType type);

    /** Defines configuration access level. */
    void setConfigurationAccessLevel(ConfigurationAccessLevel newConfigurationAccessLevel);

    void clear();

    QMap<KStorageBus, int> currentControllerTypes() const;
    QMap<KStorageBus, int> maximumControllerTypes() const;

private:

    Qt::ItemFlags flags (const QModelIndex &aIndex) const;

    AbstractItem *mRootItem;

    QPixmap mPlusPixmapEn;
    QPixmap mPlusPixmapDis;

    QPixmap mMinusPixmapEn;
    QPixmap mMinusPixmapDis;

    ToolTipType mToolTipType;

    KChipsetType m_chipsetType;

    /** Holds configuration access level. */
    ConfigurationAccessLevel m_configurationAccessLevel;
};
Q_DECLARE_METATYPE (StorageModel::ToolTipType);

/* Storage Delegate */
class StorageDelegate : public QItemDelegate
{
    Q_OBJECT;

public:

    StorageDelegate (QObject *aParent);

private:

    void paint (QPainter *aPainter, const QStyleOptionViewItem &aOption, const QModelIndex &aIndex) const;

    bool mDisableStaticControls;
};

/* Machine settings / Storage page / Storage attachment data: */
struct UIDataSettingsMachineStorageAttachment
{
    /* Default constructor: */
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
    /* Functions: */
    bool equal(const UIDataSettingsMachineStorageAttachment &other) const
    {
        return (m_attachmentType == other.m_attachmentType) &&
               (m_iAttachmentPort == other.m_iAttachmentPort) &&
               (m_iAttachmentDevice == other.m_iAttachmentDevice) &&
               (m_strAttachmentMediumId == other.m_strAttachmentMediumId) &&
               (m_fAttachmentPassthrough == other.m_fAttachmentPassthrough) &&
               (m_fAttachmentTempEject == other.m_fAttachmentTempEject) &&
               (m_fAttachmentNonRotational == other.m_fAttachmentNonRotational) &&
               (m_fAttachmentHotPluggable == other.m_fAttachmentHotPluggable);
    }
    /* Operators: */
    bool operator==(const UIDataSettingsMachineStorageAttachment &other) const { return equal(other); }
    bool operator!=(const UIDataSettingsMachineStorageAttachment &other) const { return !equal(other); }
    /* Variables: */
    KDeviceType m_attachmentType;
    LONG m_iAttachmentPort;
    LONG m_iAttachmentDevice;
    QString m_strAttachmentMediumId;
    bool m_fAttachmentPassthrough;
    bool m_fAttachmentTempEject;
    bool m_fAttachmentNonRotational;
    bool m_fAttachmentHotPluggable;
};
typedef UISettingsCache<UIDataSettingsMachineStorageAttachment> UISettingsCacheMachineStorageAttachment;

/* Machine settings / Storage page / Storage controller data: */
struct UIDataSettingsMachineStorageController
{
    /* Default constructor: */
    UIDataSettingsMachineStorageController()
        : m_strControllerName(QString())
        , m_controllerBus(KStorageBus_Null)
        , m_controllerType(KStorageControllerType_Null)
        , m_uPortCount(0)
        , m_fUseHostIOCache(false) {}
    /* Functions: */
    bool equal(const UIDataSettingsMachineStorageController &other) const
    {
        return (m_strControllerName == other.m_strControllerName) &&
               (m_controllerBus == other.m_controllerBus) &&
               (m_controllerType == other.m_controllerType) &&
               (m_uPortCount == other.m_uPortCount) &&
               (m_fUseHostIOCache == other.m_fUseHostIOCache);
    }
    /* Operators: */
    bool operator==(const UIDataSettingsMachineStorageController &other) const { return equal(other); }
    bool operator!=(const UIDataSettingsMachineStorageController &other) const { return !equal(other); }
    /* Variables: */
    QString m_strControllerName;
    KStorageBus m_controllerBus;
    KStorageControllerType m_controllerType;
    uint m_uPortCount;
    bool m_fUseHostIOCache;
};
typedef UISettingsCachePool<UIDataSettingsMachineStorageController, UISettingsCacheMachineStorageAttachment> UISettingsCacheMachineStorageController;

/* Machine settings / Storage page / Storage data: */
struct UIDataSettingsMachineStorage
{
    /* Default constructor: */
    UIDataSettingsMachineStorage() {}
    /* Operators: */
    bool operator==(const UIDataSettingsMachineStorage& /* other */) const { return true; }
    bool operator!=(const UIDataSettingsMachineStorage& /* other */) const { return false; }
};
typedef UISettingsCachePool<UIDataSettingsMachineStorage, UISettingsCacheMachineStorageController> UISettingsCacheMachineStorage;

/* Machine settings / Storage page: */
class UIMachineSettingsStorage : public UISettingsPageMachine,
                         public Ui::UIMachineSettingsStorage
{
    Q_OBJECT;

public:

    UIMachineSettingsStorage();
    ~UIMachineSettingsStorage();

    void setChipsetType(KChipsetType type);

signals:

    void storageChanged();

protected:

    /** Loads data into the cache from corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    void loadToCacheFrom(QVariant &data);
    /** Loads data into corresponding widgets from the cache,
      * this task SHOULD be performed in the GUI thread only. */
    void getFromCache();

    /** Saves data from corresponding widgets to the cache,
      * this task SHOULD be performed in the GUI thread only. */
    void putToCache();
    /** Saves data from the cache to corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    void saveFromCacheTo(QVariant &data);

    /** Returns whether the page content was changed. */
    bool changed() const { return m_cache.wasChanged(); }

    /** Performs validation, updates @a messages list if something is wrong. */
    bool validate(QList<UIValidationMessage> &messages);

    /** Handles translation event. */
    void retranslateUi();

    /** Handles show @a pEvent. */
    void showEvent(QShowEvent *pEvent);

private slots:

    /* Handlers: Medium-processing stuff: */
    void sltHandleMediumEnumerated(const QString &strMediumID);
    void sltHandleMediumDeleted(const QString &strMediumID);

    void addController();
    void addIDEController();
    void addSATAController();
    void addSCSIController();
    void addFloppyController();
    void addSASController();
    void addUSBController();
    void addNVMeController();
    void delController();

    void addAttachment();
    void addHDAttachment();
    void addCDAttachment();
    void addFDAttachment();
    void delAttachment();

    void getInformation();
    void setInformation();

    void sltPrepareOpenMediumMenu();
    void sltCreateNewHardDisk();
    void sltUnmountDevice();
    void sltChooseExistingMedium();
    void sltChooseHostDrive();
    void sltChooseRecentMedium();

    void updateActionsState();

    void onRowInserted (const QModelIndex &aParent, int aIndex);
    void onRowRemoved();

    void onCurrentItemChanged();

    void onContextMenuRequested (const QPoint &aPosition);

    void onDrawItemBranches (QPainter *aPainter, const QRect &aRect, const QModelIndex &aIndex);

    void onMouseMoved (QMouseEvent *aEvent);
    void onMouseClicked (QMouseEvent *aEvent);

private:

    void addControllerWrapper (const QString &aName, KStorageBus aBus, KStorageControllerType aType);
    void addAttachmentWrapper (KDeviceType aDevice);

    QString getWithNewHDWizard();

    void updateAdditionalObjects (KDeviceType aType);

    QString generateUniqueName (const QString &aTemplate) const;

    uint32_t deviceCount (KDeviceType aType) const;

    void addChooseExistingMediumAction(QMenu *pOpenMediumMenu, const QString &strActionName);
    void addChooseHostDriveActions(QMenu *pOpenMediumMenu);
    void addRecentMediumActions(QMenu *pOpenMediumMenu, UIMediumType recentMediumType);

    bool updateStorageData();
    bool removeStorageController(const UISettingsCacheMachineStorageController &controllerCache);
    bool createStorageController(const UISettingsCacheMachineStorageController &controllerCache);
    bool updateStorageController(const UISettingsCacheMachineStorageController &controllerCache);
    bool removeStorageAttachment(const UISettingsCacheMachineStorageController &controllerCache,
                                 const UISettingsCacheMachineStorageAttachment &attachmentCache);
    bool createStorageAttachment(const UISettingsCacheMachineStorageController &controllerCache,
                                 const UISettingsCacheMachineStorageAttachment &attachmentCache);
    bool updateStorageAttachment(const UISettingsCacheMachineStorageController &controllerCache,
                                 const UISettingsCacheMachineStorageAttachment &attachmentCache);
    bool isControllerCouldBeUpdated(const UISettingsCacheMachineStorageController &controllerCache) const;
    bool isAttachmentCouldBeUpdated(const UISettingsCacheMachineStorageAttachment &attachmentCache) const;

    /** Defines configuration access level. */
    void setConfigurationAccessLevel(ConfigurationAccessLevel configurationAccessLevel);

    void polishPage();

    QString m_strMachineId;
    QString m_strMachineSettingsFilePath;
    QString m_strMachineGuestOSTypeId;

    /** Holds the storage-tree instance. */
    QITreeView *mTwStorageTree;
    /** Holds the storage-model instance. */
    StorageModel *mStorageModel;

    QAction *mAddCtrAction;
    QAction *mDelCtrAction;
    QAction *mAddIDECtrAction;
    QAction *mAddSATACtrAction;
    QAction *mAddSCSICtrAction;
    QAction *mAddSASCtrAction;
    QAction *mAddFloppyCtrAction;
    QAction *mAddUSBCtrAction;
    QAction *mAddNVMeCtrAction;
    QAction *mAddAttAction;
    QAction *mDelAttAction;
    QAction *mAddHDAttAction;
    QAction *mAddCDAttAction;
    QAction *mAddFDAttAction;

    UIMediumIDHolder *m_pMediumIdHolder;

    bool mIsLoadingInProgress;
    bool mIsPolished;
    bool mDisableStaticControls;

    /* Cache: */
    UISettingsCacheMachineStorage m_cache;
};

#endif // __UIMachineSettingsStorage_h__

