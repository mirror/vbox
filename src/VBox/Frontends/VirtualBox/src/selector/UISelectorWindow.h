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
    void sltPerformACPIShutdownAction();
    void sltPerformPowerOffAction();
    void sltPerformRefreshAction();
    void sltShowLogDialog();
    void sltShowMachineInFileManager();
    void sltPerformCreateShortcutAction();
    void sltMachineMenuAboutToShow();
    void sltMachineCloseMenuAboutToShow();

    /* VM list slots: */
    void sltCurrentVMItemChanged(bool fRefreshDetails = true, bool fRefreshSnapshots = true, bool fRefreshDescription = true);
    void sltOpenUrls(QList<QUrl> list = QList<QUrl>());

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
    void prepareMenuGroup(QMenu *pMenu);
    void prepareMenuMachine(QMenu *pMenu);
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
    static bool isActionEnabled(int iActionIndex, const QList<UIVMItem*> &items);
    static bool isItemsAccessible(const QList<UIVMItem*> &items);
    static bool isItemsInaccessible(const QList<UIVMItem*> &items);
    static bool isItemsHasUnlockedSession(const QList<UIVMItem*> &items);
    static bool isItemsSupportsShortcuts(const QList<UIVMItem*> &items);
    static bool isItemsPoweredOff(const QList<UIVMItem*> &items);

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

    /* 'Group' menu action pointers: */
    QList<UIAction*> m_groupActions;
    QAction *m_pGroupMenuAction;
    QMenu *m_pGroupMenu;
    UIAction *m_pActionGroupNewWizard;
    UIAction *m_pActionGroupAddDialog;
    UIAction *m_pActionGroupRenameDialog;
    UIAction *m_pActionGroupRemoveDialog;
    UIAction *m_pActionGroupStartOrShow;
    UIAction *m_pActionGroupPauseAndResume;
    UIAction *m_pActionGroupReset;
    UIAction *m_pActionGroupRefresh;
    UIAction *m_pActionGroupLogDialog;
    UIAction *m_pActionGroupShowInFileManager;
    UIAction *m_pActionGroupCreateShortcut;
    UIAction *m_pActionGroupSortParent;
    UIAction *m_pActionGroupSort;

    /* 'Machine' menu action pointers: */
    QList<UIAction*> m_machineActions;
    QAction *m_pMachineMenuAction;
    QMenu *m_pMachineMenu;
    UIAction *m_pActionMachineNewWizard;
    UIAction *m_pActionMachineAddDialog;
    UIAction *m_pActionMachineSettingsDialog;
    UIAction *m_pActionMachineCloneWizard;
    UIAction *m_pActionMachineAddGroupDialog;
    UIAction *m_pActionMachineRemoveDialog;
    UIAction *m_pActionMachineStartOrShow;
    UIAction *m_pActionMachineDiscard;
    UIAction *m_pActionMachinePauseAndResume;
    UIAction *m_pActionMachineReset;
    UIAction *m_pActionMachineRefresh;
    UIAction *m_pActionMachineLogDialog;
    UIAction *m_pActionMachineShowInFileManager;
    UIAction *m_pActionMachineCreateShortcut;
    UIAction *m_pActionMachineSortParent;

    /* 'Machine / Close' menu action pointers: */
    UIAction *m_pMachineCloseMenuAction;
    QMenu *m_pMachineCloseMenu;
    UIAction *m_pACPIShutdownAction;
    UIAction *m_pPowerOffAction;

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

