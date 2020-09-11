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
#include "UIMediumDefs.h"
#include "UISettingsPage.h"

/* Forward declarations: */
class QCheckBox;
class QComboBox;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QSpinBox;
class QStackedWidget;
class QILabel;
class QLineEdit;
class QVBoxLayout;
class QILabelSeparator;
class QISplitter;
class QIToolButton;
class QITreeView;
class StorageModel;
class UIMediumIDHolder;
class UIToolBar;
struct UIDataSettingsMachineStorage;
struct UIDataSettingsMachineStorageController;
struct UIDataSettingsMachineStorageAttachment;
typedef UISettingsCache<UIDataSettingsMachineStorageAttachment> UISettingsCacheMachineStorageAttachment;
typedef UISettingsCachePool<UIDataSettingsMachineStorageController, UISettingsCacheMachineStorageAttachment> UISettingsCacheMachineStorageController;
typedef UISettingsCachePool<UIDataSettingsMachineStorage, UISettingsCacheMachineStorageController> UISettingsCacheMachineStorage;

/** Machine settings: Storage page. */
class SHARED_LIBRARY_STUFF UIMachineSettingsStorage : public UISettingsPageMachine
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

    /** Loads settings from external object(s) packed inside @a data to cache.
      * @note  This task WILL be performed in other than the GUI thread, no widget interactions! */
    virtual void loadToCacheFrom(QVariant &data) /* override */;
    /** Loads data from cache to corresponding widgets.
      * @note  This task WILL be performed in the GUI thread only, all widget interactions here! */
    virtual void getFromCache() /* override */;

    /** Saves data from corresponding widgets to cache.
      * @note  This task WILL be performed in the GUI thread only, all widget interactions here! */
    virtual void putToCache() /* override */;
    /** Saves settings from cache to external object(s) packed inside @a data.
      * @note  This task WILL be performed in other than the GUI thread, no widget interactions! */
    virtual void saveFromCacheTo(QVariant &data) /* overrride */;

    /** Performs validation, updates @a messages list if something is wrong. */
    virtual bool validate(QList<UIValidationMessage> &messages) /* override */;

    /** Defines the configuration access @a enmLevel. */
    virtual void setConfigurationAccessLevel(ConfigurationAccessLevel enmLevel) /* override */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Performs final page polishing. */
    virtual void polishPage() /* override */;

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
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares left pane. */
    void prepareLeftPane();
    /** Prepares tree view. */
    void prepareTreeView();
    /** Prepares toolbar. */
    void prepareToolBar();
    /** Prepares right pane. */
    void prepareRightPane();
    /** Prepares empty widget. */
    void prepareEmptyWidget();
    /** Prepares controller widget. */
    void prepareControllerWidget();
    /** Prepares attachment widget. */
    void prepareAttachmentWidget();
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

    /** Holds the storage-model instance. */
    StorageModel *m_pModelStorage;

    /** Holds the medium ID wrapper instance. */
    UIMediumIDHolder *m_pMediumIdHolder;

    /** Holds whether the loading is in progress. */
    bool  m_fLoadingInProgress;

    /** Holds the last mouse-press position. */
    QPoint  m_mousePressPosition;

    /** Holds the page data cache instance. */
    UISettingsCacheMachineStorage *m_pCache;

    /** @name Widgets
     * @{ */
        /** Holds the splitter instance. */
        QISplitter *m_pSplitter;

        /** Holds the left pane instance. */
        QWidget                                *m_pWidgetLeftPane;
        /** Holds the left pane separator instance. */
        QILabelSeparator                       *m_pLabelSeparatorLeftPane;
        /** Holds the tree-view layout instance. */
        QVBoxLayout                            *m_pLayoutTree;
        /** Holds the tree-view instance. */
        QITreeView                             *m_pTreeViewStorage;
        /** Holds the toolbar layout instance. */
        QHBoxLayout                            *m_pLayoutToolbar;
        /** Holds the toolbar instance. */
        UIToolBar                              *m_pToolbar;
        /** Holds the 'Add Controller' action instance. */
        QAction                                *m_pActionAddController;
        /** Holds the 'Remove Controller' action instance. */
        QAction                                *m_pActionRemoveController;
        /** Holds the map of add controller action instances. */
        QMap<KStorageControllerType, QAction*>  m_addControllerActions;
        /** Holds the 'Add Attachment' action instance. */
        QAction                                *m_pActionAddAttachment;
        /** Holds the 'Remove Attachment' action instance. */
        QAction                                *m_pActionRemoveAttachment;
        /** Holds the 'Add HD Attachment' action instance. */
        QAction                                *m_pActionAddAttachmentHD;
        /** Holds the 'Add CD Attachment' action instance. */
        QAction                                *m_pActionAddAttachmentCD;
        /** Holds the 'Add FD Attachment' action instance. */
        QAction                                *m_pActionAddAttachmentFD;

        /** Holds the right pane instance. */
        QStackedWidget   *m_pStackRightPane;
        /** Holds the right pane empty widget separator instance. */
        QILabelSeparator *m_pLabelSeparatorEmpty;
        /** Holds the info label instance. */
        QLabel           *m_pLabelInfo;
        /** Holds the right pane controller widget separator instance. */
        QILabelSeparator *m_pLabelSeparatorParameters;
        /** Holds the name label instance. */
        QLabel           *m_pLabelName;
        /** Holds the name editor instance. */
        QLineEdit        *m_pEditorName;
        /** Holds the type label instance. */
        QLabel           *m_pLabelType;
        /** Holds the type combo instance. */
        QComboBox        *m_pComboType;
        /** Holds the port count label instance. */
        QLabel           *m_pLabelPortCount;
        /** Holds the port count spinbox instance. */
        QSpinBox         *m_pSpinboxPortCount;
        /** Holds the IO cache check-box instance. */
        QCheckBox        *m_pCheckBoxIoCache;
        /** Holds the right pane attachment widget separator instance. */
        QILabelSeparator *m_pLabelSeparatorAttributes;
        /** Holds the medium label instance. */
        QLabel           *m_pLabelMedium;
        /** Holds the slot combo instance. */
        QComboBox        *m_pComboSlot;
        /** Holds the open tool-button instance. */
        QIToolButton     *m_pToolButtonOpen;
        /** Holds the passthrough check-box instance. */
        QCheckBox        *m_pCheckBoxPassthrough;
        /** Holds the temporary eject check-box instance. */
        QCheckBox        *m_pCheckBoxTempEject;
        /** Holds the non-rotational check-box instance. */
        QCheckBox        *m_pCheckBoxNonRotational;
        /** Holds the hot-pluggable check-box instance. */
        QCheckBox        *m_pCheckBoxHotPluggable;
        /** Holds the right pane attachment widget separator instance. */
        QILabelSeparator *m_pLabelSeparatorInformation;
        /** Holds the HD format label instance. */
        QLabel           *m_pLabelHDFormat;
        /** Holds the HD format field instance. */
        QILabel          *m_pFieldHDFormat;
        /** Holds the CD/FD type label instance. */
        QLabel           *m_pLabelCDFDType;
        /** Holds the CD/FD type field instance. */
        QILabel          *m_pFieldCDFDType;
        /** Holds the HD virtual size label instance. */
        QLabel           *m_pLabelHDVirtualSize;
        /** Holds the HD virtual size field instance. */
        QILabel          *m_pFieldHDVirtualSize;
        /** Holds the HD actual size label instance. */
        QLabel           *m_pLabelHDActualSize;
        /** Holds the HD actual size field instance. */
        QILabel          *m_pFieldHDActualSize;
        /** Holds the CD/FD size label instance. */
        QLabel           *m_pLabelCDFDSize;
        /** Holds the CD/FD size field instance. */
        QILabel          *m_pFieldCDFDSize;
        /** Holds the HD details label instance. */
        QLabel           *m_pLabelHDDetails;
        /** Holds the HD details field instance. */
        QILabel          *m_pFieldHDDetails;
        /** Holds the location label instance. */
        QLabel           *m_pLabelLocation;
        /** Holds the location field instance. */
        QILabel          *m_pFieldLocation;
        /** Holds the usage label instance. */
        QLabel           *m_pLabelUsage;
        /** Holds the usage field instance. */
        QILabel          *m_pFieldUsage;
        /** Holds the encryption label instance. */
        QLabel           *m_pLabelEncryption;
        /** Holds the encryption field instance. */
        QILabel          *m_pFieldEncryption;
   /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsStorage_h */
