/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsStorage class declaration.
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

#ifndef FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsStorage_h
#define FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsStorage_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIMachineSettingsStorage.gen.h"
#include "UIMediumDefs.h"
#include "UISettingsPage.h"

/* Forward declarations: */
class QITreeView;
class StorageModel;
class UIMediumIDHolder;
struct UIDataSettingsMachineStorage;
struct UIDataSettingsMachineStorageController;
struct UIDataSettingsMachineStorageAttachment;
typedef UISettingsCache<UIDataSettingsMachineStorageAttachment> UISettingsCacheMachineStorageAttachment;
typedef UISettingsCachePool<UIDataSettingsMachineStorageController, UISettingsCacheMachineStorageAttachment> UISettingsCacheMachineStorageController;
typedef UISettingsCachePool<UIDataSettingsMachineStorage, UISettingsCacheMachineStorageController> UISettingsCacheMachineStorage;

/** Machine settings: Storage page. */
class SHARED_LIBRARY_STUFF UIMachineSettingsStorage : public UISettingsPageMachine,
                                                      public Ui::UIMachineSettingsStorage
{
    Q_OBJECT;

signals:

    /** Notifies listeners about storage changed. */
    void sigStorageChanged();

public:

    /** Holds the controller mime-type for the D&D system. */
    static const QString  s_strControllerMimeType;
    /** Holds the attachment mime-type for the D&D system. */
    static const QString  s_strAttachmentMimeType;

    /** Constructs Storage settings page. */
    UIMachineSettingsStorage();
    /** Destructs Storage settings page. */
    virtual ~UIMachineSettingsStorage() /* override */;

    /** Defines chipset @a enmType. */
    void setChipsetType(KChipsetType enmType);

protected:

    /** Returns whether the page content was changed. */
    virtual bool changed() const /* override */;

    /** Loads data into the cache from corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    virtual void loadToCacheFrom(QVariant &data) /* override */;
    /** Loads data into corresponding widgets from the cache,
      * this task SHOULD be performed in the GUI thread only. */
    virtual void getFromCache() /* override */;

    /** Saves data from corresponding widgets to the cache,
      * this task SHOULD be performed in the GUI thread only. */
    virtual void putToCache() /* override */;
    /** Saves data from the cache to corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    virtual void saveFromCacheTo(QVariant &data) /* overrride */;

    /** Performs validation, updates @a messages list if something is wrong. */
    virtual bool validate(QList<UIValidationMessage> &messages) /* override */;

    /** Defines the configuration access @a enmLevel. */
    virtual void setConfigurationAccessLevel(ConfigurationAccessLevel enmLevel) /* override */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Performs final page polishing. */
    virtual void polishPage() /* override */;

    /** Handles show @a pEvent. */
    virtual void showEvent(QShowEvent *pEvent) /* override */;

private slots:

    /** Handles enumeration of medium with @a uMediumId. */
    void sltHandleMediumEnumerated(const QUuid &uMediumId);
    /** Handles removing of medium with @a uMediumId. */
    void sltHandleMediumDeleted(const QUuid &uMediumId);

    /** Handles command to add controller. */
    void sltAddController();
    /** Handles command to add PIIX3 controller. */
    void sltAddControllerPIIX3();
    /** Handles command to add PIIX4 controller. */
    void sltAddControllerPIIX4();
    /** Handles command to add ICH6 controller. */
    void sltAddControllerICH6();
    /** Handles command to add AHCI controller. */
    void sltAddControllerAHCI();
    /** Handles command to add LsiLogic controller. */
    void sltAddControllerLsiLogic();
    /** Handles command to add BusLogic controller. */
    void sltAddControllerBusLogic();
    /** Handles command to add Floppy controller. */
    void sltAddControllerFloppy();
    /** Handles command to add SAS controller. */
    void sltAddControllerLsiLogicSAS();
    /** Handles command to add USB controller. */
    void sltAddControllerUSB();
    /** Handles command to add NVMe controller. */
    void sltAddControllerNVMe();
    /** Handles command to add virtio-scsi controller. */
    void sltAddControllerVirtioSCSI();
    /** Handles command to remove controller. */
    void sltRemoveController();

    /** Handles command to add attachment. */
    void sltAddAttachment();
    /** Handles command to add HD attachment. */
    void sltAddAttachmentHD();
    /** Handles command to add CD attachment. */
    void sltAddAttachmentCD();
    /** Handles command to add FD attachment. */
    void sltAddAttachmentFD();
    /** Handles command to remove attachment. */
    void sltRemoveAttachment();

    /** Loads information from model to widgets. */
    void sltGetInformation();
    /** Saves information from widgets to model. */
    void sltSetInformation();

    /** Prepares 'Open Medium' menu. */
    void sltPrepareOpenMediumMenu();
    /** Unmounts current device. */
    void sltUnmountDevice();
    /** Mounts existing medium. */
    void sltChooseExistingMedium();
    /** Mounts a medium from a disk file. */
    void sltChooseDiskFile();
    /** Mounts existing host-drive. */
    void sltChooseHostDrive();
    /** Mounts one of recent media. */
    void sltChooseRecentMedium();

    /** Updates action states. */
    void sltUpdateActionStates();

    /** Handles row insertion into @a parentIndex on @a iPosition. */
    void sltHandleRowInsertion(const QModelIndex &parentIndex, int iPosition);
    /** Handles row removal. */
    void sltHandleRowRemoval();

    /** Handles current item change. */
    void sltHandleCurrentItemChange();

    /** Handles context menu request for @a position. */
    void sltHandleContextMenuRequest(const QPoint &position);

    /** Handles item branch drawing with @a pPainter, within @a rect for item with @a index. */
    void sltHandleDrawItemBranches(QPainter *pPainter, const QRect &rect, const QModelIndex &index);

    /** Handles mouse-move @a pEvent. */
    void sltHandleMouseMove(QMouseEvent *pEvent);
    /** Handles mouse-click @a pEvent. */
    void sltHandleMouseClick(QMouseEvent *pEvent);
    /** Handles mouse-release @a pEvent. */
    void sltHandleMouseRelease(QMouseEvent *pEvent);

    /** Handles drag-enter @a pEvent. */
    void sltHandleDragEnter(QDragEnterEvent *pEvent);
    /** Handles drag-move @a pEvent. */
    void sltHandleDragMove(QDragMoveEvent *pEvent);
    /** Handles drag-drop @a pEvent. */
    void sltHandleDragDrop(QDropEvent *pEvent);

private:

    /** Prepares all. */
    void prepare();
    /** Prepares storage tree. */
    void prepareStorageTree();
    /** Prepares storage toolbar. */
    void prepareStorageToolbar();
    /** Prepares storage widgets. */
    void prepareStorageWidgets();
    /** Prepares connections. */
    void prepareConnections();

    /** Cleanups all. */
    void cleanup();

    /** Adds controller with @a strName, @a enmBus and @a enmType. */
    void addControllerWrapper(const QString &strName, KStorageBus enmBus, KStorageControllerType enmType);
    /** Adds attachment with @a enmDevice. */
    void addAttachmentWrapper(KDeviceType enmDevice);

    /** Updates additions details according to passed @a enmType. */
    void updateAdditionalDetails(KDeviceType enmType);

    /** Generates unique controller name based on passed @a strTemplate. */
    QString generateUniqueControllerName(const QString &strTemplate) const;

    /** Returns current devices count for passed @a enmType. */
    uint32_t deviceCount(KDeviceType enmType) const;

    /** Adds 'Choose/Create Medium' action into passed @a pOpenMediumMenu under passed @a strActionName. */
    void addChooseExistingMediumAction(QMenu *pOpenMediumMenu, const QString &strActionName);
    /** Adds 'Choose Disk File' action into passed @a pOpenMediumMenu under passed @a strActionName. */
    void addChooseDiskFileAction(QMenu *pOpenMediumMenu, const QString &strActionName);
    /** Adds 'Choose Host Drive' actions into passed @a pOpenMediumMenu. */
    void addChooseHostDriveActions(QMenu *pOpenMediumMenu);
    /** Adds 'Choose Recent Medium' actions of passed @a enmRecentMediumType into passed @a pOpenMediumMenu. */
    void addRecentMediumActions(QMenu *pOpenMediumMenu, UIMediumDeviceType enmRecentMediumType);

    /** Saves existing storage data from the cache. */
    bool saveStorageData();
    /** Removes existing storage controller described by the @a controllerCache. */
    bool removeStorageController(const UISettingsCacheMachineStorageController &controllerCache);
    /** Creates existing storage controller described by the @a controllerCache. */
    bool createStorageController(const UISettingsCacheMachineStorageController &controllerCache);
    /** Updates existing storage controller described by the @a controllerCache. */
    bool updateStorageController(const UISettingsCacheMachineStorageController &controllerCache,
                                 bool fRemovingStep);
    /** Removes existing storage attachment described by the @a controllerCache and @a attachmentCache. */
    bool removeStorageAttachment(const UISettingsCacheMachineStorageController &controllerCache,
                                 const UISettingsCacheMachineStorageAttachment &attachmentCache);
    /** Creates existing storage attachment described by the @a controllerCache and @a attachmentCache. */
    bool createStorageAttachment(const UISettingsCacheMachineStorageController &controllerCache,
                                 const UISettingsCacheMachineStorageAttachment &attachmentCache);
    /** Updates existing storage attachment described by the @a controllerCache and @a attachmentCache. */
    bool updateStorageAttachment(const UISettingsCacheMachineStorageController &controllerCache,
                                 const UISettingsCacheMachineStorageAttachment &attachmentCache);
    /** Returns whether the controller described by the @a controllerCache could be updated or recreated otherwise. */
    bool isControllerCouldBeUpdated(const UISettingsCacheMachineStorageController &controllerCache) const;
    /** Returns whether the attachment described by the @a attachmentCache could be updated or recreated otherwise. */
    bool isAttachmentCouldBeUpdated(const UISettingsCacheMachineStorageAttachment &attachmentCache) const;

    /** Holds the machine ID. */
    QUuid    m_uMachineId;
    /** Holds the machine settings file-path. */
    QString  m_strMachineSettingsFilePath;
    /** Holds the machine settings file-path. */
    QString  m_strMachineName;
    /** Holds the machine guest OS type ID. */
    QString  m_strMachineGuestOSTypeId;

    /** Holds the storage-tree instance. */
    QITreeView   *m_pTreeStorage;
    /** Holds the storage-model instance. */
    StorageModel *m_pModelStorage;

    /** Holds the 'Add Controller' action instance. */
    QAction *m_pActionAddController;
    /** Holds the 'Remove Controller' action instance. */
    QAction *m_pActionRemoveController;
    /** Holds the map of add controller action instances. */
    QMap<KStorageControllerType, QAction*> m_addControllerActions;

    /** Holds the 'Add Attachment' action instance. */
    QAction *m_pActionAddAttachment;
    /** Holds the 'Remove Attachment' action instance. */
    QAction *m_pActionRemoveAttachment;
    /** Holds the 'Add HD Attachment' action instance. */
    QAction *m_pActionAddAttachmentHD;
    /** Holds the 'Add CD Attachment' action instance. */
    QAction *m_pActionAddAttachmentCD;
    /** Holds the 'Add FD Attachment' action instance. */
    QAction *m_pActionAddAttachmentFD;

    /** Holds the medium ID wrapper instance. */
    UIMediumIDHolder *m_pMediumIdHolder;

    /** Holds whether the page is polished. */
    bool  m_fPolished;
    /** Holds whether the loading is in progress. */
    bool  m_fLoadingInProgress;

    /** Holds the last mouse-press position. */
    QPoint  m_mousePressPosition;

    /** Holds the page data cache instance. */
    UISettingsCacheMachineStorage *m_pCache;
};

#endif /* !FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsStorage_h */
