/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxSelectorWnd class declaration
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxSelectorWnd_h__
#define __VBoxSelectorWnd_h__

/* Local includes */
#include "COMDefs.h"
#include "QIWithRetranslateUI.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"

/* Global includes */
#include <QMainWindow>
#include <QUrl>
#ifdef VBOX_GUI_WITH_SYSTRAY
# include <QSystemTrayIcon>
#endif /* VBOX_GUI_WITH_SYSTRAY */

/* Local forward declarations */
class QISplitter;
class UIMainBar;
class UIVMDesktop;
class UIVMItem;
class UIVMItemModel;
class UIVMListView;
class UIToolBar;
class VBoxTrayIcon;
class VBoxVMLogViewer;

class VBoxSelectorWnd : public QIWithRetranslateUI2<QMainWindow>
{
    Q_OBJECT;

public:

    VBoxSelectorWnd(VBoxSelectorWnd **aSelf,
                    QWidget* aParent = 0,
                    Qt::WindowFlags aFlags = Qt::Window);
    virtual ~VBoxSelectorWnd();

signals:

    void closing();

public slots:

    void sltShowMediumManager();
    void sltShowImportApplianceWizard(const QString &strFile = "");
    void sltShowExportApplianceWizard();
    void sltShowPreferencesDialog();
    void sltPerformExit();

    void sltShowNewMachineWizard();
    void sltShowAddMachineDialog(const QString &strFile = "");
    void sltShowMachineSettingsDialog(const QString &aCategory = QString::null, const QString &aControl = QString::null, const QString & = QString::null);
    void sltShowCloneMachineDialog(const QString & = QString::null);
    void sltShowRemoveMachineDialog(const QString & = QString::null);
    void sltPerformStartOrShowAction(const QString & = QString::null);
    void sltPerformDiscardAction(const QString & = QString::null);
    void sltPerformPauseResumeAction(bool, const QString & = QString::null);
    void sltPerformResetAction(const QString & = QString::null);
    void sltPerformACPIShutdownAction(const QString &aUuid = QString::null);
    void sltPerformPowerOffAction(const QString &aUuid = QString::null);
    void sltPerformRefreshAction(const QString & = QString::null);
    void sltShowLogDialog(const QString & = QString::null);
    void sltShowMachineInFileManager(const QString &aUuid = QString::null);
    void sltPerformCreateShortcutAction(const QString &aUuid = QString::null);
    void sltPerformSortAction(const QString &aUuid = QString::null);
    void sltMachineMenuAboutToShow();
    void sltMachineCloseMenuAboutToShow();
    void sltMachineContextMenuHovered(QAction *pAction);

    void refreshVMList();
    void refreshVMItem(const QString &aID, bool aDetails, bool aSnapshots, bool aDescription);

    void showContextMenu(const QPoint &aPoint);

    void sltOpenUrls(QList<QUrl> list = QList<QUrl>());

#ifdef VBOX_GUI_WITH_SYSTRAY
    void trayIconActivated(QSystemTrayIcon::ActivationReason aReason);
    void showWindow();
#endif

protected:

    /* Events */
    bool event(QEvent *aEvent);
    void closeEvent(QCloseEvent *aEvent);
#ifdef Q_WS_MAC
    bool eventFilter(QObject *pObject, QEvent *pEvent);
#endif /* Q_WS_MAC */

    void retranslateUi();

private slots:

    void vmListViewCurrentChanged(bool aRefreshDetails = true, bool aRefreshSnapshots = true, bool aRefreshDescription = true);
    void mediumEnumStarted();
    void mediumEnumFinished(const VBoxMediaList &);

    /* VirtualBox callback events we're interested in */

    void machineStateChanged(QString strId, KMachineState state);
    void machineDataChanged(QString strId);
    void machineRegistered(QString strID, bool fRegistered);
    void sessionStateChanged(QString strId, KSessionState state);
    void snapshotChanged(QString strId, QString strSnapshotId);
#ifdef VBOX_GUI_WITH_SYSTRAY
    void mainWindowCountChanged(int count);
    void trayIconCanShow(bool fEnabled);
    void trayIconShow(bool fEnabled);
    void trayIconChanged(bool fEnabled);
#endif

    void sltEmbedDownloaderForUserManual();
    void sltEmbedDownloaderForExtensionPack();

    void showViewContextMenu(const QPoint &pos);

private:

    /* Helping stuff: */
    void prepareMenuBar();
    void prepareMenuFile(QMenu *pMenu);
    void prepareMenuMachine(QMenu *pMenu);
    void prepareMenuMachineClose(QMenu *pMenu);
    void prepareMenuHelp(QMenu *pMenu);
    void prepareContextMenu();
    void prepareStatusBar();
    void prepareWidgets();
    void prepareConnections();

    /* Central splitter window */
    QISplitter *m_pSplitter;

    /* Main toolbar */
#ifndef Q_WS_MAC
    UIMainBar *m_pBar;
#endif /* !Q_WS_MAC */
    UIToolBar *mVMToolBar;

    /* VM list context menu */
    QMenu *m_pMachineContextMenu;

#ifdef VBOX_GUI_WITH_SYSTRAY
    /* The systray icon */
    VBoxTrayIcon *mTrayIcon;
#endif

    /* The vm list view/model */
    UIVMListView *mVMListView;
    UIVMItemModel *mVMModel;

    /* The right information widgets */
    UIVMDesktop *m_pVMDesktop;

    QRect mNormalGeo;

    bool mDoneInaccessibleWarningOnce : 1;
};

#endif // __VBoxSelectorWnd_h__

