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

    /** Notifies about HID LEDs synchronization state change. */
    void sigHidLedsSyncStateChange(bool fEnabled);

#ifdef RT_OS_DARWIN
    /** Mac OS X: Notifies about 'presentation mode' status change. */
    void sigPresentationModeChange(bool fEnabled);
    /** Mac OS X: Notifies about 'dock icon' appearance change. */
    void sigDockIconAppearanceChange(bool fEnabled);
#endif /* RT_OS_DARWIN */

public:

    /** Global extra-data ID. */
    static const QString GlobalID;

    /** Static Extra-data Manager instance/constructor. */
    static UIExtraDataManager* instance();
    /** Static Extra-data Manager destructor. */
    static void destroy();

    /** @name Messaging
      * @{ */
        /** Returns the list of supressed messages for the Message/Popup center frameworks. */
        QStringList suppressedMessages() const;
        /** Defines the @a list of supressed messages for the Message/Popup center frameworks. */
        void setSuppressedMessages(const QStringList &list);

        /** Returns the list of messages for the Message/Popup center frameworks with inverted check-box state. */
        QStringList messagesWithInvertedOption() const;

#if !defined(VBOX_BLEEDING_EDGE) && !defined(DEBUG)
        /** Returns version for which user wants to prevent BETA build warning. */
        QString preventBetaBuildWarningForVersion() const;
#endif /* !defined(VBOX_BLEEDING_EDGE) && !defined(DEBUG) */
    /** @} */

#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    /** @name Application Update
      * @{ */
        /** Returns whether Application Update functionality enabled. */
        bool applicationUpdateEnabled() const;

        /** Returns Application Update data. */
        QString applicationUpdateData() const;
        /** Defines Application Update data as @a strValue. */
        void setApplicationUpdateData(const QString &strValue);

        /** Returns Application Update check counter. */
        qulonglong applicationUpdateCheckCounter() const;
        /** Increments Application Update check counter. */
        void incrementApplicationUpdateCheckCounter();
    /** @} */
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */

    /** @name Settings
      * @{ */
        /** Returns restricted global settings pages. */
        QList<GlobalSettingsPageType> restrictedGlobalSettingsPages() const;
        /** Returns restricted machine settings pages. */
        QList<MachineSettingsPageType> restrictedMachineSettingsPages(const QString &strID) const;
    /** @} */

    /** @name Settings: Keyboard
      * @{ */
        /** Returns shortcut overrides for shortcut-pool with @a strPoolExtraDataID. */
        QStringList shortcutOverrides(const QString &strPoolExtraDataID) const;
    /** @} */

    /** @name Settings: Storage
      * @{ */
        /** Returns recent folder for hard-drives. */
        QString recentFolderForHardDrives() const;
        /** Returns recent folder for optical-disks. */
        QString recentFolderForOpticalDisks() const;
        /** Returns recent folder for floppy-disks. */
        QString recentFolderForFloppyDisks() const;
        /** Defines recent folder for hard-drives as @a strValue. */
        void setRecentFolderForHardDrives(const QString &strValue);
        /** Defines recent folder for optical-disk as @a strValue. */
        void setRecentFolderForOpticalDisks(const QString &strValue);
        /** Defines recent folder for floppy-disk as @a strValue. */
        void setRecentFolderForFloppyDisks(const QString &strValue);

        /** Returns the list of recently used hard-drives. */
        QStringList recentListOfHardDrives() const;
        /** Returns the list of recently used optical-disk. */
        QStringList recentListOfOpticalDisks() const;
        /** Returns the list of recently used floppy-disk. */
        QStringList recentListOfFloppyDisks() const;
        /** Defines the list of recently used hard-drives as @a value. */
        void setRecentListOfHardDrives(const QStringList &value);
        /** Defines the list of recently used optical-disks as @a value. */
        void setRecentListOfOpticalDisks(const QStringList &value);
        /** Defines the list of recently used floppy-disks as @a value. */
        void setRecentListOfFloppyDisks(const QStringList &value);
    /** @} */

    /** @name VirtualBox Manager
      * @{ */
        /** Returns selector-window geometry using @a pWidget as the hint. */
        QRect selectorWindowGeometry(QWidget *pWidget) const;
        /** Returns whether selector-window should be maximized. */
        bool selectorWindowShouldBeMaximized() const;
        /** Defines selector-window @a geometry and @a fMaximized state. */
        void setSelectorWindowGeometry(const QRect &geometry, bool fMaximized);

        /** Returns selector-window splitter hints. */
        QList<int> selectorWindowSplitterHints() const;
        /** Defines selector-window splitter @a hints. */
        void setSelectorWindowSplitterHints(const QList<int> &hints);

        /** Returns whether selector-window tool-bar visible. */
        bool selectorWindowToolBarVisible() const;
        /** Defines whether selector-window tool-bar @a fVisible. */
        void setSelectorWindowToolBarVisible(bool fVisible);

        /** Returns whether selector-window status-bar visible. */
        bool selectorWindowStatusBarVisible() const;
        /** Defines whether selector-window status-bar @a fVisible. */
        void setSelectorWindowStatusBarVisible(bool fVisible);

        /** Clears all the existing selector-window chooser-pane' group definitions. */
        void clearSelectorWindowGroupsDefinitions();
        /** Returns selector-window chooser-pane' groups definitions for passed @a strGroupID. */
        QStringList selectorWindowGroupsDefinitions(const QString &strGroupID) const;
        /** Defines selector-window chooser-pane' groups @a definitions for passed @a strGroupID. */
        void setSelectorWindowGroupsDefinitions(const QString &strGroupID, const QStringList &definitions);

        /** Returns last-item ID of the item chosen in selector-window chooser-pane. */
        QString selectorWindowLastItemChosen() const;
        /** Defines @a lastItemID of the item chosen in selector-window chooser-pane. */
        void setSelectorWindowLastItemChosen(const QString &strItemID);

        /** Returns selector-window details-pane' elements. */
        QMap<DetailsElementType, bool> selectorWindowDetailsElements();
        /** Defines selector-window details-pane' @a elements. */
        void setSelectorWindowDetailsElements(const QMap<DetailsElementType, bool> &elements);

        /** Returns selector-window details-pane' preview update interval. */
        PreviewUpdateIntervalType selectorWindowPreviewUpdateInterval() const;
        /** Defines selector-window details-pane' preview update @a interval. */
        void setSelectorWindowPreviewUpdateInterval(PreviewUpdateIntervalType interval);
    /** @} */

    /** @name Wizards
      * @{ */
        /** Returns mode for wizard of passed @a type. */
        WizardMode modeForWizardType(WizardType type);
        /** Defines @a mode for wizard of passed @a type. */
        void setModeForWizardType(WizardType type, WizardMode mode);
    /** @} */

    /** @name Virtual Machine
      * @{ */
        /** Returns whether machine should be shown in selector-window chooser-pane. */
        bool showMachineInSelectorChooser(const QString &strID) const;
        /** Returns whether machine should be shown in selector-window details-pane. */
        bool showMachineInSelectorDetails(const QString &strID) const;

        /** Returns whether machine reconfiguration enabled. */
        bool machineReconfigurationEnabled(const QString &strID) const;
        /** Returns whether machine snapshot operations enabled. */
        bool machineSnapshotOperationsEnabled(const QString &strID) const;

        /** Returns whether this machine is first time started. */
        bool machineFirstTimeStarted(const QString &strID) const;
        /** Returns whether this machine is fFirstTimeStarted. */
        void setMachineFirstTimeStarted(bool fFirstTimeStarted, const QString &strID);

#ifndef Q_WS_MAC
        /** Except Mac OS X: Returns redefined machine-window icon names. */
        QStringList machineWindowIconNames(const QString &strID) const;
        /** Except Mac OS X: Returns redefined machine-window name postfix. */
        QString machineWindowNamePostfix(const QString &strID) const;
#endif /* !Q_WS_MAC */

        /** Returns geometry for machine-window with @a uScreenIndex in @a visualStateType. */
        QRect machineWindowGeometry(UIVisualStateType visualStateType, ulong uScreenIndex, const QString &strID) const;
        /** Returns whether machine-window with @a uScreenIndex in @a visualStateType should be maximized. */
        bool machineWindowShouldBeMaximized(UIVisualStateType visualStateType, ulong uScreenIndex, const QString &strID) const;
        /** Defines @a geometry and @a fMaximized state for machine-window with @a uScreenIndex in @a visualStateType. */
        void setMachineWindowGeometry(UIVisualStateType visualStateType, ulong uScreenIndex, const QRect &geometry, bool fMaximized, const QString &strID);

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

        /** Returns restricted Runtime UI visual-states. */
        UIVisualStateType restrictedVisualStates(const QString &strID) const;

        /** Returns requested Runtime UI visual-state. */
        UIVisualStateType requestedVisualState(const QString &strID) const;
        /** Defines requested Runtime UI visual-state as @a visualState. */
        void setRequestedVisualState(UIVisualStateType visualState, const QString &strID);

        /** Returns whether guest-screen auto-resize according machine-window size is enabled. */
        bool guestScreenAutoResizeEnabled(const QString &strID) const;
        /** Defines whether guest-screen auto-resize according machine-window size is @a fEnabled. */
        void setGuestScreenAutoResizeEnabled(bool fEnabled, const QString &strID);

        /** Returns last guest-screen size-hint for screen with @a uScreenIndex. */
        QSize lastGuestSizeHint(ulong uScreenIndex, const QString &strID) const;
        /** Defines last guest-screen @a sizeHint for screen with @a uScreenIndex. */
        void setLastGuestSizeHint(ulong uScreenIndex, const QSize &sizeHint, const QString &strID);
        /** Returns whether guest size-hint was for full or seamless screen with @a uScreenIndex. */
        bool wasLastGuestSizeHintForFullScreen(ulong uScreenIndex, const QString &strID) const;
        /** Defines whether guest size-hint @a fWas for full or seamless screen with @a uScreenIndex. */
        void markLastGuestSizeHintAsFullScreen(ulong uScreenIndex, bool fWas, const QString &strID);

        /** Returns host-screen index corresponding to passed guest-screen @a iGuestScreenIndex. */
        int hostScreenForPassedGuestScreen(int iGuestScreenIndex, const QString &strID);
        /** Defines @a iHostScreenIndex corresponding to passed guest-screen @a iGuestScreenIndex. */
        void setHostScreenForPassedGuestScreen(int iGuestScreenIndex, int iHostScreenIndex, const QString &strID);

        /** Returns whether automatic mounting/unmounting of guest-screens enabled. */
        bool autoMountGuestScreensEnabled(const QString &strID) const;

#ifdef VBOX_WITH_VIDEOHWACCEL
        /** Returns whether 2D acceleration should use linear sretch. */
        bool useLinearStretch(const QString &strID) const;
        /** Returns whether 2D acceleration should use YV12 pixel format. */
        bool usePixelFormatYV12(const QString &strID) const;
        /** Returns whether 2D acceleration should use UYVY pixel format. */
        bool usePixelFormatUYVY(const QString &strID) const;
        /** Returns whether 2D acceleration should use YUY2 pixel format. */
        bool usePixelFormatYUY2(const QString &strID) const;
        /** Returns whether 2D acceleration should use AYUV pixel format. */
        bool usePixelFormatAYUV(const QString &strID) const;
#endif /* VBOX_WITH_VIDEOHWACCEL */

        /** Returns Runtime UI HiDPI optimization type. */
        HiDPIOptimizationType hiDPIOptimizationType(const QString &strID) const;

        /** Returns whether mini-toolbar is enabled for full and seamless screens. */
        bool miniToolbarEnabled(const QString &strID) const;
        /** Defines whether mini-toolbar is @a fEnabled for full and seamless screens. */
        void setMiniToolbarEnabled(bool fEnabled, const QString &strID);

        /** Returns whether mini-toolbar should auto-hide itself. */
        bool autoHideMiniToolbar(const QString &strID) const;
        /** Defines whether mini-toolbar should @a fAutoHide itself. */
        void setAutoHideMiniToolbar(bool fAutoHide, const QString &strID);

        /** Returns mini-toolbar alignment. */
        Qt::AlignmentFlag miniToolbarAlignment(const QString &strID) const;
        /** Returns mini-toolbar @a alignment. */
        void setMiniToolbarAlignment(Qt::AlignmentFlag alignment, const QString &strID);

        /** Returns restricted Runtime UI status-bar indicators. */
        QList<IndicatorType> restrictedStatusBarIndicators(const QString &strID) const;

#ifdef Q_WS_MAC
        /** Mac OS X: Returns whether 'presentation mode' enabled. */
        bool presentationModeEnabled(const QString &strID) const;

        /** Mac OS X: Returns whether Dock icon should be updated at runtime. */
        bool realtimeDockIconUpdateEnabled(const QString &strID) const;
        /** Mac OS X: Defines whether Dock icon update should be fEnabled at runtime. */
        void setRealtimeDockIconUpdateEnabled(bool fEnabled, const QString &strID);

        /** Mac OS X: Returns guest-screen which Dock icon should reflect at runtime. */
        int realtimeDockIconUpdateMonitor(const QString &strID) const;
        /** Mac OS X: Defines guest-screen @a iIndex which Dock icon should reflect at runtime. */
        void setRealtimeDockIconUpdateMonitor(int iIndex, const QString &strID);
#endif /* Q_WS_MAC */

        /** Returns whether machine should pass CAD to guest. */
        bool passCADtoGuest(const QString &strID) const;

        /** Returns redefined guru-meditation handler type. */
        GuruMeditationHandlerType guruMeditationHandlerType(const QString &strID) const;

        /** Returns whether machine should perform HID LEDs synchronization. */
        bool hidLedsSyncState(const QString &strID) const;
    /** @} */

    /** @name Virtual Machine: Information dialog
      * @{ */
        /** Returns information-window geometry using @a pWidget and @a pParentWidget as hints. */
        QRect informationWindowGeometry(QWidget *pWidget, QWidget *pParentWidget, const QString &strID) const;
        /** Returns whether information-window should be maximized or not. */
        bool informationWindowShouldBeMaximized(const QString &strID) const;
        /** Defines information-window @a geometry and @a fMaximized state. */
        void setInformationWindowGeometry(const QRect &geometry, bool fMaximized, const QString &strID);
    /** @} */

    /** @name Virtual Machine: Close dialog
      * @{ */
        /** Returns default machine close action. */
        MachineCloseAction defaultMachineCloseAction(const QString &strID) const;
        /** Returns restricted machine close actions. */
        MachineCloseAction restrictedMachineCloseActions(const QString &strID) const;

        /** Returns last machine close action. */
        MachineCloseAction lastMachineCloseAction(const QString &strID) const;
        /** Defines last @a machineCloseAction. */
        void setLastMachineCloseAction(MachineCloseAction machineCloseAction, const QString &strID);

        /** Returns machine close hook script name as simple string. */
        QString machineCloseHookScript(const QString &strID) const;
    /** @} */

#ifdef VBOX_WITH_DEBUGGER_GUI
    /** @name Virtual Machine: Debug UI
      * @{ */
        /** Returns debug flag value for passed @a strDebugFlagKey. */
        QString debugFlagValue(const QString &strDebugFlagKey) const;
    /** @} */
#endif /* VBOX_WITH_DEBUGGER_GUI */

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
    bool isFeatureAllowed(const QString &strKey, const QString &strID = GlobalID) const;
    /** Determines whether feature corresponding to passed @a strKey is restricted.
      * If valid @a strID is set => applies to machine extra-data, otherwise => to global one. */
    bool isFeatureRestricted(const QString &strKey, const QString &strID = GlobalID) const;

    /** Translates bool flag into 'allowed' value. */
    QString toFeatureAllowed(bool fAllowed);
    /** Translates bool flag into 'restricted' value. */
    QString toFeatureRestricted(bool fRestricted);

    /** Returns extra-data value corresponding to passed @a strKey as QString.
      * If valid @a strID is set => applies to machine extra-data, otherwise => to global one. */
    QString extraDataString(const QString &strKey, const QString &strID = GlobalID) const;
    /** Defines extra-data value corresponding to passed @a strKey as strValue.
      * If valid @a strID is set => applies to machine extra-data, otherwise => to global one. */
    void setExtraDataString(const QString &strKey, const QString &strValue, const QString &strID = GlobalID);

    /** Returns extra-data value corresponding to passed @a strKey as QStringList.
      * If valid @a strID is set => applies to machine extra-data, otherwise => to global one. */
    QStringList extraDataStringList(const QString &strKey, const QString &strID = GlobalID) const;
    /** Defines extra-data value corresponding to passed @a strKey as strValue.
      * If valid @a strID is set => applies to machine extra-data, otherwise => to global one. */
    void setExtraDataStringList(const QString &strKey, const QStringList &strValue, const QString &strID = GlobalID);

    /** Returns string consisting of @a strBase appended with @a uScreenIndex for the *non-primary* screen-index.
      * If @a fSameRuleForPrimary is 'true' same rule will be used for *primary* screen-index. Used for storing per-screen extra-data. */
    static QString extraDataKeyPerScreen(const QString &strBase, ulong uScreenIndex, bool fSameRuleForPrimary = false);

    /** Singleton Extra-data Manager instance. */
    static UIExtraDataManager *m_spInstance;

    /** Holds main event-listener instance. */
    CEventListener m_listener;
    /** Holds extra-data event-handler instance. */
    UIExtraDataEventHandler *m_pHandler;

    /** Holds extra-data map instance. */
    mutable QMap<QString, ExtraDataMap> m_data;
};

/** Singleton Extra-data Manager 'official' name. */
#define gEDataManager UIExtraDataManager::instance()

#endif /* !___UIExtraDataManager_h___ */
