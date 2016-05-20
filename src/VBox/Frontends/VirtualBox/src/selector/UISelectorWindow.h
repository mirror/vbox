/* $Id$ */
/** @file
 * VBox Qt GUI - UISelectorWindow class declaration.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UISelectorWindow_h___
#define ___UISelectorWindow_h___

/* Qt includes: */
#include <QMainWindow>
#include <QUrl>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class UIAction;
class UIActionPool;
class UIActionPolymorphic;
class UIGChooser;
class UIGDetails;
class UIMainBar;
class UIToolBar;
class UIVMDesktop;
class UIVMItem;
class QISplitter;
class QMenu;
class QStackedWidget;

/** Singleton QMainWindow extension
  * used as VirtualBox Manager (selector-window) instance. */
class UISelectorWindow : public QIWithRetranslateUI<QMainWindow>
{
    Q_OBJECT;

public:

    /** Static constructor. */
    static void create();
    /** Static destructor. */
    static void destroy();
    /** Static instance provider. */
    static UISelectorWindow* instance() { return m_spInstance; }

    /** Returns the action-pool instance. */
    UIActionPool* actionPool() const { return m_pActionPool; }

protected:

    /** Constructs selector-window. */
    UISelectorWindow();
    /** Destructs selector-window. */
    ~UISelectorWindow();

private slots:

    /** Handles selector-window context-menu call for passed @a position. */
    void sltShowSelectorWindowContextMenu(const QPoint &position);

    /** Handles signal about Details-container @a iIndex change. */
    void sltHandleDetailsContainerIndexChange(int iIndex);

    /** Handles signal about Chooser-pane index change.
      * @param fRefreshDetails     brings whether details should be updated.
      * @param fRefreshSnapshots   brings whether snapshots should be updated.
      * @param fRefreshDescription brings whether description should be updated. */
    void sltHandleChooserPaneIndexChange(bool fRefreshDetails = true, bool fRefreshSnapshots = true, bool fRefreshDescription = true);

    /** Handles signal about medium-enumeration finished. */
    void sltHandleMediumEnumerationFinish();

    /** Handles call to open a @a list of URLs. */
    void sltOpenUrls(QList<QUrl> list = QList<QUrl>());

    /** Handles signal about group saving progress change. */
    void sltHandleGroupSavingProgressChange();

    /** @name CVirtualBox event handling stuff.
      * @{ */
        /** Handles CVirtualBox event about state change for machine with @a strID. */
        void sltHandleStateChange(QString strID);
        /** Handles CVirtualBox event about snapshot change for machine with @a strID. */
        void sltHandleSnapshotChange(QString strID);
    /** @} */

    /** @name File menu stuff.
      * @{ */
        /** Handles call to open Media Manager window. */
        void sltOpenMediaManagerWindow();
        /** Handles call to open Import Appliance wizard.
          * @param strFileName can bring the name of file to import appliance from. */
        void sltOpenImportApplianceWizard(const QString &strFileName = QString());
        /** Handles call to open Export Appliance wizard. */
        void sltOpenExportApplianceWizard();
#ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
        /** Handles call to open Extra-data Manager window. */
        void sltOpenExtraDataManagerWindow();
#endif /* VBOX_GUI_WITH_EXTRADATA_MANAGER_UI */
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
        /** Handles call to open Machine Settings dialog.
          * @param strCategory can bring the settings category to start from.
          * @param strControl  can bring the widget of the page to focus.
          * @param strID       can bring the ID of machine to manage. */
        void sltOpenMachineSettingsDialog(const QString &strCategory = QString(),
                                          const QString &strControl = QString(),
                                          const QString &strID = QString());
        /** Handles call to open Clone Machine wizard. */
        void sltOpenCloneMachineWizard();
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

    /** Returns current-item. */
    UIVMItem* currentItem() const;
    /** Returns a list of current-items. */
    QList<UIVMItem*> currentItems() const;

    /** @name Event handling stuff.
      * @{ */
        /** Handles translation event. */
        virtual void retranslateUi();

        /** Handles any Qt @a pEvent. */
        virtual bool event(QEvent *pEvent);
        /** Handles Qt show @a pEvent. */
        virtual void showEvent(QShowEvent *pEvent);
        /** Handles first Qt show @a pEvent. */
        virtual void polishEvent(QShowEvent *pEvent);
#ifdef VBOX_WS_MAC
        /** Mac OS X: Preprocesses any Qt @a pEvent for passed @a pObject. */
        virtual bool eventFilter(QObject *pObject, QEvent *pEvent);
#endif /* VBOX_WS_MAC */
    /** @} */

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
        /** Prepares status-bar. */
        void prepareStatusBar();
        /** Prepares widgets. */
        void prepareWidgets();
        /** Prepares connections. */
        void prepareConnections();
        /** Loads settings. */
        void loadSettings();

        /** Saves settings. */
        void saveSettings();
        /** Cleanups connections. */
        void cleanupConnections();
        /** Cleanups menu-bar. */
        void cleanupMenuBar();
        /** Cleanups window. */
        void cleanup();
    /** @} */

    /** @name Action update stuff.
      * @{ */
        /** Performs update of actions appearance. */
        void updateActionsAppearance();

        /** Returns whether the action with @a iActionIndex is enabled.
          * @param items used to calculate verdict about should the action be enabled. */
        bool isActionEnabled(int iActionIndex, const QList<UIVMItem*> &items);

        /** Returns whether all passed @a items are powered off. */
        static bool isItemsPoweredOff(const QList<UIVMItem*> &items);
        /** Returns whether at least one of passed @a items is able to shutdown. */
        static bool isAtLeastOneItemAbleToShutdown(const QList<UIVMItem*> &items);
        /** Returns whether at least one of passed @a items supports shortcut creation. */
        static bool isAtLeastOneItemSupportsShortcuts(const QList<UIVMItem*> &items);
        /** Returns whether at least one of passed @a items is accessible. */
        static bool isAtLeastOneItemAccessible(const QList<UIVMItem*> &items);
        /** Returns whether at least one of passed @a items is inaccessible. */
        static bool isAtLeastOneItemInaccessible(const QList<UIVMItem*> &items);
        /** Returns whether at least one of passed @a items is removable. */
        static bool isAtLeastOneItemRemovable(const QList<UIVMItem*> &items);
        /** Returns whether at least one of passed @a items can be started or shown. */
        static bool isAtLeastOneItemCanBeStartedOrShowed(const QList<UIVMItem*> &items);
        /** Returns whether at least one of passed @a items can be discarded. */
        static bool isAtLeastOneItemDiscardable(const QList<UIVMItem*> &items);
        /** Returns whether at least one of passed @a items is started. */
        static bool isAtLeastOneItemStarted(const QList<UIVMItem*> &items);
        /** Returns whether at least one of passed @a items is running. */
        static bool isAtLeastOneItemRunning(const QList<UIVMItem*> &items);
    /** @} */

    /** Holds the static instance. */
    static UISelectorWindow *m_spInstance;

    /** Holds whether the dialog is polished. */
    bool m_fPolished : 1;
    /** Holds whether the warning about inaccessible media shown. */
    bool m_fWarningAboutInaccessibleMediaShown : 1;

    /** Holds the action-pool instance. */
    UIActionPool *m_pActionPool;

    /** Holds the central splitter instance. */
    QISplitter *m_pSplitter;

#ifndef VBOX_WS_MAC
    /** Holds the main bar instance. */
    UIMainBar *m_pBar;
#endif /* !VBOX_WS_MAC */
    /** Holds the main toolbar instance. */
    UIToolBar *m_pToolBar;

    /** Holds the Details-container instance. */
    QStackedWidget *m_pContainerDetails;

    /** Holds the Chooser-pane instance. */
    UIGChooser *m_pPaneChooser;
    /** Holds the Details-pane instance. */
    UIGDetails *m_pPaneDetails;
    /** Holds the Desktop-pane instance. */
    UIVMDesktop *m_pPaneDesktop;

    /** Holds the list of Group menu actions. */
    QList<UIAction*> m_groupActions;
    /** Holds the Group menu parent action. */
    QAction *m_pGroupMenuAction;

    /** Holds the list of Machine menu actions. */
    QList<UIAction*> m_machineActions;
    /** Holds the Machine menu parent action. */
    QAction *m_pMachineMenuAction;

    /** Holds the dialog geometry. */
    QRect m_geometry;
};

#define gpSelectorWindow UISelectorWindow::instance()

#endif /* !___UISelectorWindow_h___ */

