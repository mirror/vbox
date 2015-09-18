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

/** QMainWindow extension
  * used as VirtualBox Manager (selector-window) instance. */
class UISelectorWindow : public QIWithRetranslateUI2<QMainWindow>
{
    Q_OBJECT;

public:

    /** Constructs selector-window passing @a pParent to the QMainWindow base-class.
      * @param ppSelf brings the pointer to pointer to this window instance used by the external caller.
      * @param flags  brings the selector-window flags dialogs should have. */
    UISelectorWindow(UISelectorWindow **ppSelf,
                     QWidget *pParent = 0,
                     Qt::WindowFlags flags = Qt::Window);
    /** Destructs selector-window. */
    ~UISelectorWindow();

    /** Returns the action-pool instance. */
    UIActionPool* actionPool() const { return m_pActionPool; }

private slots:

    /** Handles CVirtualBox event about state change for machine with @a strId. */
    void sltStateChanged(QString strId);
    /** Handles CVirtualBox event about snapshot change for machine with @a strId. */
    void sltSnapshotChanged(QString strId);

    /** Handles signal about Details-view @a iWidgetIndex change. */
    void sltDetailsViewIndexChanged(int iWidgetIndex);

    /** Handles signal about medium-enumeration finished. */
    void sltHandleMediumEnumerationFinish();

    /** Handles selector-window context-menu call for passed @a pos. */
    void sltShowSelectorContextMenu(const QPoint &pos);

    /** Handles call to open Media Manager dialog. */
    void sltShowMediumManager();
    /** Handles call to open Import Appliance wizard.
      * @param strFileName can bring the name of file to import appliance from. */
    void sltShowImportApplianceWizard(const QString &strFileName = QString());
    /** Handles call to open Export Appliance wizard. */
    void sltShowExportApplianceWizard();
#ifdef DEBUG
    /** Handles call to open Extra-data Manager dialog. */
    void sltOpenExtraDataManagerWindow();
#endif /* DEBUG */
    /** Handles call to open Preferences dialog. */
    void sltShowPreferencesDialog();
    /** Handles call to exit application. */
    void sltPerformExit();

    /** Handles call to open Add Machine dialog.
      * @param strFileName can bring the name of file to add machine from. */
    void sltShowAddMachineDialog(const QString &strFileName = QString());
    /** Handles call to open Machine Settings dialog.
      * @param strCategory can bring the settings category to start from.
      * @param strControl  can bring the widget of the page to focus.
      * @param strId       can bring the ID of machine to manage. */
    void sltShowMachineSettingsDialog(const QString &strCategory = QString(),
                                      const QString &strControl = QString(),
                                      const QString &strId = QString());
    /** Handles call to open Clone Machine wizard. */
    void sltShowCloneMachineWizard();
    /** Handles call to start or show machine. */
    void sltPerformStartOrShowAction();
    /** Handles call to start machine in normal mode. */
    void sltPerformStartNormal();
    /** Handles call to start machine in headless mode. */
    void sltPerformStartHeadless();
    /** Handles call to start machine in detachable mode. */
    void sltPerformStartDetachable();
    /** Handles call to discard machine state. */
    void sltPerformDiscardAction();
    /** Handles call to @a fPause or resume machine otherwise. */
    void sltPerformPauseResumeAction(bool fPause);
    /** Handles call to reset machine. */
    void sltPerformResetAction();
    /** Handles call to save machine state. */
    void sltPerformSaveAction();
    /** Handles call to ask machine for shutdown. */
    void sltPerformACPIShutdownAction();
    /** Handles call to power machine off. */
    void sltPerformPowerOffAction();
    /** Handles call to open machine Log dialog. */
    void sltShowLogDialog();
    /** Handles call to show machine in File Manager. */
    void sltShowMachineInFileManager();
    /** Handles call to create machine shortcut. */
    void sltPerformCreateShortcutAction();
    /** Handles call to show group Close menu. */
    void sltGroupCloseMenuAboutToShow();
    /** Handles call to show machine Close menu. */
    void sltMachineCloseMenuAboutToShow();

    /** Handles signal about Chooser-pane index change.
      * @param fRefreshDetails     brings whether details should be updated.
      * @param fRefreshSnapshots   brings whether snapshots should be updated.
      * @param fRefreshDescription brings whether description should be updated. */
    void sltCurrentVMItemChanged(bool fRefreshDetails = true, bool fRefreshSnapshots = true, bool fRefreshDescription = true);

    /** Handles call to open a @a list of URLs. */
    void sltOpenUrls(QList<QUrl> list = QList<QUrl>());

    /** Handles signal about group saving process status change. */
    void sltGroupSavingUpdate();

private:

    /** Handles translation event. */
    void retranslateUi();

    /** Handles any Qt @a pEvent. */
    bool event(QEvent *pEvent);
    /** Handles Qt show @a pEvent. */
    void showEvent(QShowEvent *pEvent);
    /** Handles Qt polish @a pEvent. */
    void polishEvent(QShowEvent *pEvent);
#ifdef Q_WS_MAC
    /** Preprocesses any Qt @a pEvent for passed @a pObject. */
    bool eventFilter(QObject *pObject, QEvent *pEvent);
#endif /* Q_WS_MAC */

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

    /** Returns current-item. */
    UIVMItem* currentItem() const;
    /** Returns a list of current-items. */
    QList<UIVMItem*> currentItems() const;

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

    /** Holds whether the dialog is polished. */
    bool m_fPolished : 1;
    /** Holds whether the warning about inaccessible mediums shown. */
    bool m_fWarningAboutInaccessibleMediumShown : 1;

    /** Holds the action-pool instance. */
    UIActionPool *m_pActionPool;

    /** Holds the central splitter instance. */
    QISplitter *m_pSplitter;

#ifndef Q_WS_MAC
    /** Holds the main bar instance. */
    UIMainBar *m_pBar;
#endif /* !Q_WS_MAC */
    /** Holds the main toolbar instance. */
    UIToolBar *mVMToolBar;

    /** Holds the Details-view container instance. */
    QStackedWidget *m_pContainer;

    /** Holds the Chooser-pane instance. */
    UIGChooser *m_pChooser;
    /** Holds the Details-pane instance. */
    UIGDetails *m_pDetails;

    /** Holds the Desktop-widget instance. */
    UIVMDesktop *m_pVMDesktop;

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

#endif /* !___UISelectorWindow_h___ */

