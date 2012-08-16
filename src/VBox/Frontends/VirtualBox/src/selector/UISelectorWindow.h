/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UISelectorWindow class declaration
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UISelectorWindow_h__
#define __UISelectorWindow_h__

/* Qt includes: */
#include <QMainWindow>
#include <QUrl>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UIMedium.h"
#include "UINetworkDefs.h"

/* Forward declarations: */
class QISplitter;
class QMenu;
class UIAction;
class UIMainBar;
class UIToolBar;
class UIVMDesktop;
class UIVMItem;
class UIVMItemModel;
class UIVMListView;
class UIGChooser;
class UIGDetails;
class QStackedWidget;

/* VM selector window class: */
class UISelectorWindow : public QIWithRetranslateUI2<QMainWindow>
{
    Q_OBJECT;

signals:

    /* Obsolete: Signal to notify listeners about this dialog closed: */
    void closing();

public:

    /* Constructor/destructor: */
    UISelectorWindow(UISelectorWindow **ppSelf,
                     QWidget* pParent = 0,
                     Qt::WindowFlags flags = Qt::Window);
    ~UISelectorWindow();

private slots:

    /* Handlers: Global-event stuff: */
    void sltStateChanged(QString strId);
    void sltSnapshotChanged(QString strId);

    /* Handler: Details-view stuff: */
    void sltDetailsViewIndexChanged(int iWidgetIndex);

    /* Handler: Medium enumeration stuff: */
    void sltMediumEnumFinished(const VBoxMediaList &mediumList);

    /* Handler: Menubar/status stuff: */
    void sltShowSelectorContextMenu(const QPoint &pos);

    /* Handlers: File-menu stuff: */
    void sltShowMediumManager();
    void sltShowImportApplianceWizard(const QString &strFileName = QString());
    void sltShowExportApplianceWizard();
    void sltShowPreferencesDialog();
    void sltPerformExit();

    /* Handlers: Machine-menu slots: */
    void sltShowAddMachineDialog(const QString &strFileName = QString());
    void sltShowMachineSettingsDialog(const QString &strCategory = QString(),
                                      const QString &strControl = QString(),
                                      const QString &strId = QString());
    void sltShowCloneMachineWizard();
    void sltPerformStartOrShowAction();
    void sltPerformDiscardAction();
    void sltPerformPauseResumeAction(bool fPause);
    void sltPerformResetAction();
    void sltPerformSaveAction();
    void sltPerformACPIShutdownAction();
    void sltPerformPowerOffAction();
    void sltPerformRefreshAction();
    void sltShowLogDialog();
    void sltShowMachineInFileManager();
    void sltPerformCreateShortcutAction();
    void sltGroupCloseMenuAboutToShow();
    void sltMachineCloseMenuAboutToShow();

    /* VM list slots: */
    void sltCurrentVMItemChanged(bool fRefreshDetails = true, bool fRefreshSnapshots = true, bool fRefreshDescription = true);
    void sltOpenUrls(QList<QUrl> list = QList<QUrl>());

    /* Handlers: Group saving stuff: */
    void sltGroupSavingUpdate();

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Event handlers: */
    bool event(QEvent *pEvent);
    void closeEvent(QCloseEvent *pEvent);
#ifdef Q_WS_MAC
    bool eventFilter(QObject *pObject, QEvent *pEvent);
#endif /* Q_WS_MAC */

    /* Helpers: Prepare stuff: */
    void prepareIcon();
    void prepareMenuBar();
    void prepareMenuFile(QMenu *pMenu);
    void prepareCommonActions();
    void prepareGroupActions();
    void prepareMachineActions();
    void prepareMenuGroup(QMenu *pMenu);
    void prepareMenuMachine(QMenu *pMenu);
    void prepareMenuGroupClose(QMenu *pMenu);
    void prepareMenuMachineClose(QMenu *pMenu);
    void prepareMenuHelp(QMenu *pMenu);
    void prepareStatusBar();
    void prepareWidgets();
    void prepareConnections();
    void loadSettings();
    void saveSettings();

    /* Helpers: Current item stuff: */
    UIVMItem* currentItem() const;
    QList<UIVMItem*> currentItems() const;

    /* Helper: Action update stuff: */
    void updateActionsAppearance();

    /* Helpers: Action stuff: */
    bool isActionEnabled(int iActionIndex, const QList<UIVMItem*> &items);
    static bool isItemsPoweredOff(const QList<UIVMItem*> &items);
    static bool isAtLeastOneItemAbleToShutdown(const QList<UIVMItem*> &items);
    static bool isAtLeastOneItemSupportsShortcuts(const QList<UIVMItem*> &items);
    static bool isAtLeastOneItemAccessible(const QList<UIVMItem*> &items);
    static bool isAtLeastOneItemInaccessible(const QList<UIVMItem*> &items);
    static bool isAtLeastOneItemRemovable(const QList<UIVMItem*> &items);
    static bool isAtLeastOneItemCanBeStartedOrShowed(const QList<UIVMItem*> &items);
    static bool isAtLeastOneItemDiscardable(const QList<UIVMItem*> &items);
    static bool isAtLeastOneItemStarted(const QList<UIVMItem*> &items);
    static bool isAtLeastOneItemRunning(const QList<UIVMItem*> &items);
    static bool isItemEditable(UIVMItem *pItem);
    static bool isItemSaved(UIVMItem *pItem);
    static bool isItemPoweredOff(UIVMItem *pItem);
    static bool isItemStarted(UIVMItem *pItem);
    static bool isItemRunning(UIVMItem *pItem);
    static bool isItemPaused(UIVMItem *pItem);
    static bool isItemStuck(UIVMItem *pItem);

    /* Central splitter window: */
    QISplitter *m_pSplitter;

    /* Main toolbar: */
#ifndef Q_WS_MAC
    UIMainBar *m_pBar;
#endif /* !Q_WS_MAC */
    UIToolBar *mVMToolBar;

    /* Details widgets container: */
    QStackedWidget *m_pContainer;

    /* Graphics chooser/details: */
    UIGChooser *m_pChooser;
    UIGDetails *m_pDetails;

    /* VM details widget: */
    UIVMDesktop *m_pVMDesktop;

    /* 'File' menu action pointers: */
    QMenu *m_pFileMenu;
    UIAction *m_pMediumManagerDialogAction;
    UIAction *m_pImportApplianceWizardAction;
    UIAction *m_pExportApplianceWizardAction;
    UIAction *m_pPreferencesDialogAction;
    UIAction *m_pExitAction;

    /* Common Group/Machine actions: */
    UIAction *m_pAction_Common_New;
    UIAction *m_pAction_Common_Add;
    UIAction *m_pAction_Common_StartOrShow;
    UIAction *m_pAction_Common_PauseAndResume;
    UIAction *m_pAction_Common_Reset;
    UIAction *m_pAction_Common_Discard;
    UIAction *m_pAction_Common_LogDialog;
    UIAction *m_pAction_Common_Refresh;
    UIAction *m_pAction_Common_ShowInFileManager;
    UIAction *m_pAction_Common_CreateShortcut;

    /* 'Group' menu action pointers: */
    QList<UIAction*> m_groupActions;
    QAction *m_pGroupMenuAction;
    QMenu *m_pGroupMenu;
    UIAction *m_pAction_Group_Rename;
    UIAction *m_pAction_Group_Remove;
    UIAction *m_pAction_Group_Sort;
    /* 'Group / Close' menu action pointers: */
    UIAction *m_pGroupCloseMenuAction;
    QMenu *m_pGroupCloseMenu;
    UIAction *m_pGroupSaveAction;
    UIAction *m_pGroupACPIShutdownAction;
    UIAction *m_pGroupPowerOffAction;

    /* 'Machine' menu action pointers: */
    QList<UIAction*> m_machineActions;
    QAction *m_pMachineMenuAction;
    QMenu *m_pMachineMenu;
    UIAction *m_pAction_Machine_Settings;
    UIAction *m_pAction_Machine_Clone;
    UIAction *m_pAction_Machine_Remove;
    UIAction *m_pAction_Machine_AddGroup;
    UIAction *m_pAction_Machine_SortParent;
    /* 'Machine / Close' menu action pointers: */
    UIAction *m_pMachineCloseMenuAction;
    QMenu *m_pMachineCloseMenu;
    UIAction *m_pMachineSaveAction;
    UIAction *m_pMachineACPIShutdownAction;
    UIAction *m_pMachinePowerOffAction;

    /* 'Help' menu action pointers: */
    QMenu *m_pHelpMenu;
    UIAction *m_pHelpAction;
    UIAction *m_pWebAction;
    UIAction *m_pResetWarningsAction;
    UIAction *m_pNetworkAccessManager;
    UIAction *m_pUpdateAction;
    UIAction *m_pAboutAction;

    /* Other variables: */
    QRect m_normalGeo;
    bool m_fDoneInaccessibleWarningOnce : 1;
};

#endif // __UISelectorWindow_h__

