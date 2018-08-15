/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualBoxManager class declaration.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIVirtualBoxManager_h___
#define ___UIVirtualBoxManager_h___

/* Qt includes: */
#include <QUrl>

/* GUI includes: */
#include "QIMainWindow.h"
#include "QIWithRetranslateUI.h"
#include "VBoxGlobal.h"

/* Forward declarations: */
class QMenu;
class QIManagerDialog;
class UIAction;
class UIActionPool;
class UIVirtualBoxManagerWidget;
class UIVirtualMachineItem;

/* Type definitions: */
typedef QMap<QString, QIManagerDialog*> VMLogViewerMap;

/** Singleton QIMainWindow extension used as VirtualBox Manager instance. */
class UIVirtualBoxManager : public QIWithRetranslateUI<QIMainWindow>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about this window remapped to another screen. */
    void sigWindowRemapped();

public:

    /** Singleton constructor. */
    static void create();
    /** Singleton destructor. */
    static void destroy();
    /** Singleton instance provider. */
    static UIVirtualBoxManager *instance() { return s_pInstance; }

    /** Returns the action-pool instance. */
    UIActionPool *actionPool() const { return m_pActionPool; }

protected:

    /** Constructs VirtualBox Manager. */
    UIVirtualBoxManager();
    /** Destructs VirtualBox Manager. */
    virtual ~UIVirtualBoxManager() /* override */;

    /** Returns whether the window should be maximized when geometry being restored. */
    virtual bool shouldBeMaximized() const /* override */;

    /** @name Event handling stuff.
      * @{ */
#ifdef VBOX_WS_MAC
        /** Mac OS X: Preprocesses any @a pEvent for passed @a pObject. */
        virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;
#endif

        /** Handles translation event. */
        virtual void retranslateUi() /* override */;

        /** Handles any Qt @a pEvent. */
        virtual bool event(QEvent *pEvent) /* override */;
        /** Handles show @a pEvent. */
        virtual void showEvent(QShowEvent *pEvent) /* override */;
        /** Handles first show @a pEvent. */
        virtual void polishEvent(QShowEvent *pEvent) /* override */;
        /** Handles close @a pEvent. */
        virtual void closeEvent(QCloseEvent *pEvent) /* override */;
    /** @} */

private slots:

    /** @name Common stuff.
      * @{ */
#if QT_VERSION == 0
        /** Stupid moc does not warn if it cannot find headers! */
        void QT_VERSION_NOT_DEFINED
#elif defined(VBOX_WS_X11)
        /** Handles host-screen available-area change. */
        void sltHandleHostScreenAvailableAreaChange();
#endif /* VBOX_WS_X11 */

        /** Handles signal about medium-enumeration finished. */
        void sltHandleMediumEnumerationFinish();

        /** Handles call to open a @a list of URLs. */
        void sltHandleOpenUrlCall(QList<QUrl> list = QList<QUrl>());

        /** Hnadles singal about Chooser-pane index change.  */
        void sltHandleChooserPaneIndexChange();
        /** Handles signal about group saving progress change. */
        void sltHandleGroupSavingProgressChange();

        /** Handles singal about Tool type change.  */
        void sltHandleToolTypeChange();
    /** @} */

    /** @name CVirtualBox event handling stuff.
      * @{ */
        /** Handles CVirtualBox event about state change for machine with @a strID. */
        void sltHandleStateChange(const QString &strID);
    /** @} */

    /** @name File menu stuff.
      * @{ */
        /** Handles call to open Virtual Medium Manager window. */
        void sltOpenVirtualMediumManagerWindow();
        /** Handles call to close Virtual Medium Manager window. */
        void sltCloseVirtualMediumManagerWindow();

        /** Handles call to open Host Network Manager window. */
        void sltOpenHostNetworkManagerWindow();
        /** Handles call to close Host Network Manager window. */
        void sltCloseHostNetworkManagerWindow();

        /** Handles call to close a Machine LogViewer window. */
        void sltCloseLogViewerWindow();

        /** Handles call to open Import Appliance wizard.
          * @param strFileName can bring the name of file to import appliance from. */
        void sltOpenImportApplianceWizard(const QString &strFileName = QString());
        /** Handles call to open Import Appliance wizard the default way. */
        void sltOpenImportApplianceWizardDefault() { sltOpenImportApplianceWizard(); }
        /** Handles call to open Export Appliance wizard. */
        void sltOpenExportApplianceWizard();

#ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
        /** Handles call to open Extra-data Manager window. */
        void sltOpenExtraDataManagerWindow();
#endif

        /** Handles call to open Preferences dialog. */
        void sltOpenPreferencesDialog();

        /** Handles call to exit application. */
        void sltPerformExit();
    /** @} */

    /** @name Machine menu stuff.
      * @{ */
        /** Handles call to open Add Machine dialog.
          * @param strFileName can bring the name of file to add machine from. */
        void sltOpenAddMachineDialog(const QString &strFileName = QString());
        /** Handles call to open Add Machine dialog the default way. */
        void sltOpenAddMachineDialogDefault() { sltOpenAddMachineDialog(); }

        /** Handles call to open Machine Settings dialog.
          * @param strCategory can bring the settings category to start from.
          * @param strControl  can bring the widget of the page to focus.
          * @param strID       can bring the ID of machine to manage. */
        void sltOpenMachineSettingsDialog(QString strCategory = QString(),
                                          QString strControl = QString(),
                                          const QString &strID = QString());
        /** Handles call to open Machine Settings dialog the default way. */
        void sltOpenMachineSettingsDialogDefault() { sltOpenMachineSettingsDialog(); }

        /** Handles call to open Clone Machine wizard. */
        void sltOpenCloneMachineWizard();

        /** Handles the Move Machine action. */
        void sltPerformMachineMove();

        /** Handles call to start or show machine. */
        void sltPerformStartOrShowMachine();
        /** Handles call to start machine in normal mode. */
        void sltPerformStartMachineNormal();
        /** Handles call to start machine in headless mode. */
        void sltPerformStartMachineHeadless();
        /** Handles call to start machine in detachable mode. */
        void sltPerformStartMachineDetachable();

        /** Handles call to discard machine state. */
        void sltPerformDiscardMachineState();

        /** Handles call to @a fPause or resume machine otherwise. */
        void sltPerformPauseOrResumeMachine(bool fPause);

        /** Handles call to reset machine. */
        void sltPerformResetMachine();

        /** Handles call to detach machine UI. */
        void sltPerformDetachMachineUI();
        /** Handles call to save machine state. */
        void sltPerformSaveMachineState();
        /** Handles call to ask machine for shutdown. */
        void sltPerformShutdownMachine();
        /** Handles call to power machine off. */
        void sltPerformPowerOffMachine();

        /** Handles call to open machine Log dialog. */
        void sltOpenMachineLogDialog();

        /** Handles call to show machine in File Manager. */
        void sltShowMachineInFileManager();

        /** Handles call to create machine shortcut. */
        void sltPerformCreateMachineShortcut();

        /** Handles call to show group Close menu. */
        void sltGroupCloseMenuAboutToShow();
        /** Handles call to show machine Close menu. */
        void sltMachineCloseMenuAboutToShow();
    /** @} */

private:

    /** @name Prepare/Cleanup cascade.
      * @{ */
        /** Prepares window. */
        void prepare();
        /** Prepares icon. */
        void prepareIcon();
        /** Prepares menu-bar. */
        void prepareMenuBar();
        /** Prepares @a pMenu File. */
        void prepareMenuFile(QMenu *pMenu);
        /** Prepares @a pMenu Group. */
        void prepareMenuGroup(QMenu *pMenu);
        /** Prepares @a pMenu Machine. */
        void prepareMenuMachine(QMenu *pMenu);
        /** Prepares @a pMenu Group => Start or Show. */
        void prepareMenuGroupStartOrShow(QMenu *pMenu);
        /** Prepares @a pMenu Machine => Start or Show. */
        void prepareMenuMachineStartOrShow(QMenu *pMenu);
        /** Prepares @a pMenu Group => Close. */
        void prepareMenuGroupClose(QMenu *pMenu);
        /** Prepares @a pMenu Machine => Close. */
        void prepareMenuMachineClose(QMenu *pMenu);
        /** Prepares @a pMenu Snapshot. */
        void prepareMenuSnapshot(QMenu *pMenu);
        /** Prepares @a pMenu Log Viewer. */
        void prepareMenuLogViewer(QMenu *pMenu);
        /** Prepares status-bar. */
        void prepareStatusBar();
        /** Prepares toolbar. */
        void prepareToolbar();
        /** Prepares widgets. */
        void prepareWidgets();
        /** Prepares connections. */
        void prepareConnections();
        /** Loads settings. */
        void loadSettings();

        /** Saves settings. */
        void saveSettings();
        /** Cleanups widgets. */
        void cleanupWidgets();
        /** Cleanups menu-bar. */
        void cleanupMenuBar();
        /** Cleanups window. */
        void cleanup();
    /** @} */

    /** @name Common stuff.
      * @{ */
        /** Returns current-item. */
        UIVirtualMachineItem *currentItem() const;
        /** Returns a list of current-items. */
        QList<UIVirtualMachineItem*> currentItems() const;

        /** Returns whether group saving is in progress. */
        bool isGroupSavingInProgress() const;
        /** Returns whether all items of one group is selected. */
        bool isAllItemsOfOneGroupSelected() const;
        /** Returns whether single group is selected. */
        bool isSingleGroupSelected() const;
    /** @} */

    /** @name VM launching stuff.
      * @{ */
        /** Launches or shows virtual machines represented by passed @a items in corresponding @a enmLaunchMode (for launch). */
        void performStartOrShowVirtualMachines(const QList<UIVirtualMachineItem*> &items, VBoxGlobal::LaunchMode enmLaunchMode);
    /** @} */

    /** @name Action update stuff.
      * @{ */
        /** Performs update of actions visibility. */
        void updateActionsVisibility();
        /** Performs update of actions appearance. */
        void updateActionsAppearance();

        /** Returns whether the action with @a iActionIndex is enabled.
          * @param items used to calculate verdict about should the action be enabled. */
        bool isActionEnabled(int iActionIndex, const QList<UIVirtualMachineItem*> &items);

        /** Returns whether all passed @a items are powered off. */
        static bool isItemsPoweredOff(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items is able to shutdown. */
        static bool isAtLeastOneItemAbleToShutdown(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items supports shortcut creation. */
        static bool isAtLeastOneItemSupportsShortcuts(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items is accessible. */
        static bool isAtLeastOneItemAccessible(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items is inaccessible. */
        static bool isAtLeastOneItemInaccessible(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items is removable. */
        static bool isAtLeastOneItemRemovable(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items can be started. */
        static bool isAtLeastOneItemCanBeStarted(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items can be shown. */
        static bool isAtLeastOneItemCanBeShown(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items can be started or shown. */
        static bool isAtLeastOneItemCanBeStartedOrShown(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items can be discarded. */
        static bool isAtLeastOneItemDiscardable(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items is started. */
        static bool isAtLeastOneItemStarted(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items is running. */
        static bool isAtLeastOneItemRunning(const QList<UIVirtualMachineItem*> &items);
    /** @} */

    /** Holds the static instance. */
    static UIVirtualBoxManager *s_pInstance;

    /** Holds whether the dialog is polished. */
    bool  m_fPolished                      : 1;
    /** Holds whether first medium enumeration handled. */
    bool  m_fFirstMediumEnumerationHandled : 1;

    /** Holds the action-pool instance. */
    UIActionPool *m_pActionPool;

    /** Holds the list of Group menu actions. */
    QList<UIAction*>  m_groupActions;
    /** Holds the Group menu parent action. */
    QAction          *m_pGroupMenuAction;

    /** Holds the list of Machine menu actions. */
    QList<UIAction*>  m_machineActions;
    /** Holds the Machine menu parent action. */
    QAction          *m_pMachineMenuAction;

    /** Holds the list of Snapshot menu actions. */
    QList<UIAction*>  m_snapshotActions;
    /** Holds the Snapshot menu parent action. */
    QAction          *m_pSnapshotMenuAction;

    /** Holds the list of Log Viewer menu actions. */
    QList<UIAction*>  m_logViewerActions;
    /** Holds the Log menu parent action. */
    QAction          *m_pLogViewerMenuAction;

    /** Holds the Virtual Media Manager window instance. */
    QIManagerDialog *m_pManagerVirtualMedia;
    /** Holds the Host Network Manager window instance. */
    QIManagerDialog *m_pManagerHostNetwork;
    /** Holds a map of (machineUUID, UIVMLogViewerDialog). */
    VMLogViewerMap   m_logViewers;

    /** Holds the central-widget instance. */
    UIVirtualBoxManagerWidget *m_pWidget;
};

#define gpManager UIVirtualBoxManager::instance()

#endif /* !___UIVirtualBoxManager_h___ */
