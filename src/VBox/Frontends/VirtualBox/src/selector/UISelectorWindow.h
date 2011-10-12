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

/* Global includes: */
#include <QMainWindow>
#include <QUrl>
#ifdef VBOX_GUI_WITH_SYSTRAY
# include <QSystemTrayIcon>
#endif /* VBOX_GUI_WITH_SYSTRAY */

/* Local includes: */
#include "QIWithRetranslateUI.h"
#include "VBoxMedium.h"
#include "COMDefs.h"

/* Forward declarations: */
class QISplitter;
class UIMainBar;
class UIToolBar;
class UIVMDesktop;
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
    void sltShowCloneMachineWizard(const QString &strMachineId = QString());
    void sltShowRemoveMachineDialog(const QString &strMachineId = QString());
    void sltPerformStartOrShowAction(const QString &strMachineId = QString());
    void sltPerformDiscardAction(const QString &strMachineId = QString());
    void sltPerformPauseResumeAction(bool fPause, const QString &strMachineId = QString());
    void sltPerformResetAction(const QString &strMachineId = QString());
    void sltPerformACPIShutdownAction(const QString &strMachineId = QString());
    void sltPerformPowerOffAction(const QString &strMachineId = QString());
    void sltPerformRefreshAction(const QString &strMachineId = QString());
    void sltShowLogDialog(const QString &strMachineId = QString());
    void sltShowMachineInFileManager(const QString &strMachineId = QString());
    void sltPerformCreateShortcutAction(const QString &strMachineId = QString());
    void sltPerformSortAction(const QString &strMachineId = QString());
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

    /* Downloader related slots: */
    void sltEmbedDownloaderForUserManual();
    void sltEmbedDownloaderForExtensionPack();

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

    /* Other variables: */
    QRect m_normalGeo;
    bool m_fDoneInaccessibleWarningOnce : 1;
#ifdef VBOX_GUI_WITH_SYSTRAY
    /* The systray icon: */
    VBoxTrayIcon *m_pTrayIcon;
#endif /* VBOX_GUI_WITH_SYSTRAY */
};

#endif // __UISelectorWindow_h__

