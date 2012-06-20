/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UISelectorWindow class declaration
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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
#ifdef VBOX_GUI_WITH_SYSTRAY
# include <QSystemTrayIcon>
#endif /* VBOX_GUI_WITH_SYSTRAY */

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UIMedium.h"
#include "UINetworkDefs.h"

/* Forward declarations: */
class QISplitter;
class QMenu;
class UIActionInterface;
class UIMainBar;
class UIToolBar;
class UIVMDesktop;
class UIVMItem;
class UIVMItemModel;
class UIVMListView;
#ifdef VBOX_GUI_WITH_SYSTRAY
class VBoxTrayIcon;
#endif /* VBOX_GUI_WITH_SYSTRAY */

/* VM selector window class: */
class UISelectorWindow : public QIWithRetranslateUI2<QMainWindow>
{
    Q_OBJECT;

public:

    /* Constructor/destructor: */
    UISelectorWindow(UISelectorWindow **ppSelf,
                     QWidget* pParent = 0,
                     Qt::WindowFlags flags = Qt::Window);
    virtual ~UISelectorWindow();

signals:

    /* Signal to notify listeners about this dialog closed: */
    void closing();

private slots:

    /* Menubar/status bar slots: */
    void sltShowSelectorContextMenu(const QPoint &pos);

    /* 'File' menu slots: */
    void sltShowMediumManager();
    void sltShowImportApplianceWizard(const QString &strFileName = QString());
    void sltShowExportApplianceWizard();
    void sltShowPreferencesDialog();
    void sltPerformExit();

    /* 'Machine' menu slots: */
    void sltShowNewMachineWizard();
    void sltShowAddMachineDialog(const QString &strFileName = QString());
    void sltShowMachineSettingsDialog(const QString &strCategory = QString(),
                                      const QString &strControl = QString(),
                                      const QString &strMachineId = QString());
    void sltShowCloneMachineWizard();
    void sltShowRemoveMachineDialog();
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
    void sltPerformSortAction();
    void sltMachineMenuAboutToShow();
    void sltMachineCloseMenuAboutToShow();
    void sltMachineContextMenuHovered(QAction *pAction);

    /* VM list slots: */
    void sltRefreshVMList();
    void sltRefreshVMItem(const QString &strMachineId, bool fDetails, bool fSnapshots, bool fDescription);
    void sltShowContextMenu(const QPoint &point);
    void sltCurrentVMItemChanged(bool fRefreshDetails = true, bool fRefreshSnapshots = true, bool fRefreshDescription = true);
    void sltOpenUrls(QList<QUrl> list = QList<QUrl>());

    /* VirtualBox callback events we're interested in: */
    void sltMachineRegistered(QString strID, bool fRegistered);
    void sltMachineStateChanged(QString strId, KMachineState state);
    void sltMachineDataChanged(QString strId);
    void sltSessionStateChanged(QString strId, KSessionState state);
    void sltSnapshotChanged(QString strId, QString strSnapshotId);
#ifdef VBOX_GUI_WITH_SYSTRAY
    /* Sys tray related event handlers: */
    void sltMainWindowCountChanged(int count);
    void sltTrayIconCanShow(bool fEnabled);
    void sltTrayIconShow(bool fEnabled);
    void sltTrayIconChanged(bool fEnabled);
    /* Sys tray related slots: */
    void sltTrayIconActivated(QSystemTrayIcon::ActivationReason aReason);
    void sltShowWindow();
#endif /* VBOX_GUI_WITH_SYSTRAY */

    /* Medium enumeration related slots: */
    void sltMediumEnumerationStarted();
    void sltMediumEnumFinished(const VBoxMediaList &mediumList);

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Event handlers: */
    bool event(QEvent *pEvent);
    void closeEvent(QCloseEvent *pEvent);
#ifdef Q_WS_MAC
    bool eventFilter(QObject *pObject, QEvent *pEvent);
#endif /* Q_WS_MAC */

    /* Helping stuff: */
    void prepareIcon();
    void prepareMenuBar();
    void prepareMenuFile(QMenu *pMenu);
    void prepareMenuMachine(QMenu *pMenu);
    void prepareMenuMachineClose(QMenu *pMenu);
    void prepareMenuHelp(QMenu *pMenu);
    void prepareContextMenu();
    void prepareStatusBar();
    void prepareWidgets();
    void prepareConnections();
    void loadSettings();
    void saveSettings();

    /* Static helping stuff: */
    static bool isActionEnabled(int iActionIndex, UIVMItem *pItem, const QList<UIVMItem*> &items);

    /* Central splitter window: */
    QISplitter *m_pSplitter;

    /* Main toolbar: */
#ifndef Q_WS_MAC
    UIMainBar *m_pBar;
#endif /* !Q_WS_MAC */
    UIToolBar *mVMToolBar;

    /* VM list view: */
    UIVMListView *m_pVMListView;
    /* VM list model: */
    UIVMItemModel *m_pVMModel;
    /* VM list context menu: */
    QMenu *m_pMachineContextMenu;

    /* VM details widget: */
    UIVMDesktop *m_pVMDesktop;

    /* 'File' menu action pointers: */
    QMenu *m_pFileMenu;
    UIActionInterface *m_pMediumManagerDialogAction;
    UIActionInterface *m_pImportApplianceWizardAction;
    UIActionInterface *m_pExportApplianceWizardAction;
    UIActionInterface *m_pPreferencesDialogAction;
    UIActionInterface *m_pExitAction;

    /* 'Machine' menu action pointers: */
    QMenu *m_pMachineMenu;
    UIActionInterface *m_pNewWizardAction;
    UIActionInterface *m_pAddDialogAction;
    UIActionInterface *m_pSettingsDialogAction;
    UIActionInterface *m_pCloneWizardAction;
    UIActionInterface *m_pRemoveDialogAction;
    UIActionInterface *m_pStartOrShowAction;
    UIActionInterface *m_pDiscardAction;
    UIActionInterface *m_pPauseAndResumeAction;
    UIActionInterface *m_pResetAction;
    UIActionInterface *m_pRefreshAction;
    UIActionInterface *m_pLogDialogAction;
    UIActionInterface *m_pShowInFileManagerAction;
    UIActionInterface *m_pCreateShortcutAction;
    UIActionInterface *m_pSortAction;

    /* 'Machine / Close' menu action pointers: */
    UIActionInterface *m_pMachineCloseMenuAction;
    QMenu *m_pMachineCloseMenu;
    UIActionInterface *m_pACPIShutdownAction;
    UIActionInterface *m_pPowerOffAction;

    /* 'Help' menu action pointers: */
    QMenu *m_pHelpMenu;
    UIActionInterface *m_pHelpAction;
    UIActionInterface *m_pWebAction;
    UIActionInterface *m_pResetWarningsAction;
    UIActionInterface *m_pNetworkAccessManager;
#ifdef VBOX_WITH_REGISTRATION
    UIActionInterface *m_pRegisterAction;
#endif /* VBOX_WITH_REGISTRATION */
    UIActionInterface *m_pUpdateAction;
    UIActionInterface *m_pAboutAction;

    /* Other variables: */
    QRect m_normalGeo;
    bool m_fDoneInaccessibleWarningOnce : 1;
#ifdef VBOX_GUI_WITH_SYSTRAY
    /* The systray icon: */
    VBoxTrayIcon *m_pTrayIcon;
#endif /* VBOX_GUI_WITH_SYSTRAY */
};

#endif // __UISelectorWindow_h__

