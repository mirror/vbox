/** @file
 * VBox Qt GUI - UIExtraDataManager class declaration.
 */

/*
 * Copyright (C) 2010-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIExtraDataManager_h___
#define ___UIExtraDataManager_h___

/* Qt includes: */
#include <QObject>
#include <QMap>

/* GUI includes: */
#include "UIExtraDataDefs.h"

/* COM includes: */
#include "CEventListener.h"

/* Forward declarations: */
class UIExtraDataEventHandler;

/* Type definitions: */
typedef QMap<QString, QString> ExtraDataMap;

/** Singleton QObject extension
  * providing GUI with corresponding extra-data values,
  * and notifying it whenever any of those values changed. */
class UIExtraDataManager : public QObject
{
    Q_OBJECT;

    /** Extra-data Manager constructor. */
    UIExtraDataManager();
    /** Extra-data Manager destructor. */
    ~UIExtraDataManager();

signals:

    /** Notifies about GUI language change. */
    void sigLanguageChange(QString strLanguage);

    /** Notifies about Selector UI keyboard shortcut change. */
    void sigSelectorUIShortcutChange();
    /** Notifies about Runtime UI keyboard shortcut change. */
    void sigRuntimeUIShortcutChange();

    /** Notifies about HID LED sync state change. */
    void sigHIDLedsSyncStateChange(bool fEnabled);

#ifdef RT_OS_DARWIN
    /** Mac OS X: Notifies about 'presentation mode' status change. */
    void sigPresentationModeChange(bool fEnabled);
    /** Mac OS X: Notifies about 'dock icon' appearance change. */
    void sigDockIconAppearanceChange(bool fEnabled);
#endif /* RT_OS_DARWIN */

public:

    /** Static Extra-data Manager instance/constructor. */
    static UIExtraDataManager* instance();
    /** Static Extra-data Manager destructor. */
    static void destroy();

    /** Returns version for which user wants to prevent BETA warning. */
    QString preventBETAwarningForVersion() const;

    /** Returns recent hard-drive folder. */
    QString recentFolderForHardDrives() const;
    /** Returns recent optical-disk folder. */
    QString recentFolderForOpticalDisks() const;
    /** Returns recent floppy-disk folder. */
    QString recentFolderForFloppyDisks() const;
    /** Defines recent hard-drive folder as @a strValue. */
    void setRecentFolderForHardDrives(const QString &strValue);
    /** Defines recent optical-disk folder as @a strValue. */
    void setRecentFolderForOpticalDisks(const QString &strValue);
    /** Defines recent floppy-disk folder as @a strValue. */
    void setRecentFolderForFloppyDisks(const QString &strValue);

    /** Returns recent hard-drive list. */
    QStringList recentListOfHardDrives() const;
    /** Returns recent optical-disk list. */
    QStringList recentListOfOpticalDisks() const;
    /** Returns recent floppy-disk list. */
    QStringList recentListOfFloppyDisks() const;
    /** Defines recent hard-drive list as @a value. */
    void setRecentListOfHardDrives(const QStringList &value);
    /** Defines recent optical-disk list as @a value. */
    void setRecentListOfOpticalDisks(const QStringList &value);
    /** Defines recent floppy-disk list as @a value. */
    void setRecentListOfFloppyDisks(const QStringList &value);

    /** Returns list of the supressed messages for the Message/Popup center frameworks. */
    QStringList suppressedMessages() const;
    /** Defines list of the supressed messages for the Message/Popup center frameworks as @a value. */
    void setSuppressedMessages(const QStringList &value);

    /** Returns list of the messages for the Message/Popup center frameworks with inverted check-box state. */
    QStringList messagesWithInvertedOption() const;

#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    /** Returns whether we should allow Application Update. */
    bool shouldWeAllowApplicationUpdate() const;
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */

    /** Returns whether description should be hidden for wizard @a strWizardName. */
    bool isDescriptionHiddenForWizard(const QString &strWizardName);
    /** Defines whether description should be @a fHidden for wizard @a strWizardName. */
    void setDescriptionHiddenForWizard(const QString &strWizardName, bool fHidden);

    /** Returns whether this machine started for the first time. */
    bool isFirstRun(const QString &strId) const;
    /** Defines whether this machine started for the first time. */
    void setFirstRun(bool fIsFirstRun, const QString &strId);

    /** Returns whether we should show machine. */
    bool shouldWeShowMachine(const QString &strID) const;
    /** Returns whether we should show machine details. */
    bool shouldWeShowMachineDetails(const QString &strID) const;

    /** Returns whether we should allow machine reconfiguration. */
    bool shouldWeAllowMachineReconfiguration(const QString &strID) const;
    /** Returns whether we should allow machine snapshot operations. */
    bool shouldWeAllowMachineSnapshotOperations(const QString &strID) const;

    /** Returns whether we should automatically mount/unmount guest-screens. */
    bool shouldWeAutoMountGuestScreens(const QString &strID) const;

    /** Returns restricted Runtime UI menu types. */
    RuntimeMenuType restrictedRuntimeMenuTypes(const QString &strID) const;
#ifdef Q_WS_MAC
    /** Mac OS X: Returns restricted Runtime UI action types for Application menu. */
    RuntimeMenuApplicationActionType restrictedRuntimeMenuApplicationActionTypes(const QString &strID) const;
#endif /* Q_WS_MAC */
    /** Returns restricted Runtime UI action types for Machine menu. */
    RuntimeMenuMachineActionType restrictedRuntimeMenuMachineActionTypes(const QString &strID) const;
    /** Returns restricted Runtime UI action types for View menu. */
    RuntimeMenuViewActionType restrictedRuntimeMenuViewActionTypes(const QString &strID) const;
    /** Returns restricted Runtime UI action types for Devices menu. */
    RuntimeMenuDevicesActionType restrictedRuntimeMenuDevicesActionTypes(const QString &strID) const;
#ifdef VBOX_WITH_DEBUGGER_GUI
    /** Returns restricted Runtime UI action types for Debugger menu. */
    RuntimeMenuDebuggerActionType restrictedRuntimeMenuDebuggerActionTypes(const QString &strID) const;
#endif /* VBOX_WITH_DEBUGGER_GUI */
    /** Returns restricted Runtime UI action types for Help menu. */
    RuntimeMenuHelpActionType restrictedRuntimeMenuHelpActionTypes(const QString &strID) const;

    /** Returns restricted Runtime UI visual-state types. */
    UIVisualStateType restrictedVisualStateTypes(const QString &strID) const;

    /** Returns default machine close action. */
    MachineCloseAction defaultMachineCloseAction(const QString &strID) const;
    /** Returns restricted machine close actions. */
    MachineCloseAction restrictedMachineCloseActions(const QString &strID) const;

    /** Returns restricted Runtime UI status-bar indicators. */
    QList<IndicatorType> restrictedStatusBarIndicators(const QString &strID) const;

    /** Returns global settings pages. */
    QList<GlobalSettingsPageType> restrictedGlobalSettingsPages() const;
    /** Returns machine settings pages. */
    QList<MachineSettingsPageType> restrictedMachineSettingsPages(const QString &strID) const;

#ifndef Q_WS_MAC
    /** Except Mac OS X: Returns redefined machine-window icon names. */
    QStringList machineWindowIconNames(const QString &strID) const;
    /** Except Mac OS X: Returns redefined machine-window name postfix. */
    QString machineWindowNamePostfix(const QString &strID) const;
#endif /* !Q_WS_MAC */

    /** Returns redefined guru-meditation handler type. */
    GuruMeditationHandlerType guruMeditationHandlerType(const QString &strID) const;

    /** Returns Runtime UI HiDPI optimization type. */
    HiDPIOptimizationType hiDPIOptimizationType(const QString &strID) const;

private slots:

    /** Handles 'extra-data change' event: */
    void sltExtraDataChange(QString strMachineID, QString strKey, QString strValue);

private:

    /** Prepare Extra-data Manager. */
    void prepare();
    /** Prepare global extra-data map. */
    void prepareGlobalExtraDataMap();
    /** Prepare extra-data event-handler. */
    void prepareExtraDataEventHandler();
    /** Prepare Main event-listener. */
    void prepareMainEventListener();

    /** Cleanup Extra-data Manager. */
    void cleanup();
    /** Cleanup Main event-listener. */
    void cleanupMainEventListener();
    // /** Cleanup extra-data event-handler. */
    // void cleanupExtraDataEventHandler();
    // /** Cleanup extra-data map. */
    // void cleanupExtraDataMap();

    /** Hot-load machine extra-data map. */
    void hotloadMachineExtraDataMap(const QString &strID) const;

    /** Determines whether feature corresponding to passed @a strKey is allowed.
      * If valid @a strID is set => applies to machine extra-data, otherwise => to global one. */
    bool isFeatureAllowed(const QString &strKey, const QString &strID = QString()) const;
    /** Determines whether feature corresponding to passed @a strKey is restricted.
      * If valid @a strID is set => applies to machine extra-data, otherwise => to global one. */
    bool isFeatureRestricted(const QString &strKey, const QString &strID = QString()) const;

    /** Translates bool flag into 'allowed' value. */
    QString toFeatureAllowed(bool fAllowed);
    /** Translates bool flag into 'restricted' value. */
    QString toFeatureRestricted(bool fRestricted);

    /** Returns extra-data value corresponding to passed @a strKey as QString.
      * If valid @a strID is set => applies to machine extra-data, otherwise => to global one. */
    QString extraDataString(const QString &strKey, const QString &strID = QString()) const;
    /** Defines extra-data value corresponding to passed @a strKey as strValue.
      * If valid @a strID is set => applies to machine extra-data, otherwise => to global one. */
    void setExtraDataString(const QString &strKey, const QString &strValue, const QString &strID = QString());

    /** Returns extra-data value corresponding to passed @a strKey as QStringList.
      * If valid @a strID is set => applies to machine extra-data, otherwise => to global one. */
    QStringList extraDataStringList(const QString &strKey, const QString &strID = QString()) const;
    /** Defines extra-data value corresponding to passed @a strKey as strValue.
      * If valid @a strID is set => applies to machine extra-data, otherwise => to global one. */
    void setExtraDataStringList(const QString &strKey, const QStringList &strValue, const QString &strID = QString());

    /** Singleton Extra-data Manager instance. */
    static UIExtraDataManager *m_pInstance;

    /** Main event-listener instance. */
    CEventListener m_listener;
    /** Extra-data event-handler instance. */
    UIExtraDataEventHandler *m_pHandler;

    /** Extra-data map. */
    mutable QMap<QString, ExtraDataMap> m_data;
};

/** Singleton Extra-data Manager 'official' name. */
#define gEDataManager UIExtraDataManager::instance()

#endif /* !___UIExtraDataManager_h___ */
