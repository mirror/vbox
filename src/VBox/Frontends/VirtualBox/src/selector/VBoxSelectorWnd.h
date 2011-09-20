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

    void fileMediaMgr();
    void fileImportAppliance(const QString &strFile = "");
    void fileExportAppliance();
    void fileSettings();
    void fileExit();

    void vmNew();
    void vmAdd(const QString &strFile = "");
    void vmSettings(const QString &aCategory = QString::null, const QString &aControl = QString::null, const QString & = QString::null);
    void vmClone(const QString & = QString::null);
    void vmDelete(const QString & = QString::null);
    void vmStart(const QString & = QString::null);
    void vmDiscard(const QString & = QString::null);
    void vmPause(bool, const QString & = QString::null);
    void vmReset(const QString & = QString::null);
    void vmACPIShutdown(const QString &aUuid = QString::null);
    void vmPowerOff(const QString &aUuid = QString::null);
    void vmRefresh(const QString & = QString::null);
    void vmShowLogs(const QString & = QString::null);
    void vmOpenInFileManager(const QString &aUuid = QString::null);
    void vmCreateShortcut(const QString &aUuid = QString::null);
    void vmResort(const QString &aUuid = QString::null);
    void sltCloseMenuAboutToShow();

    void refreshVMList();
    void refreshVMItem(const QString &aID, bool aDetails, bool aSnapshots, bool aDescription);

    void showContextMenu(const QPoint &aPoint);

    void sltOpenUrls(QList<QUrl> list = QList<QUrl>());

#ifdef VBOX_GUI_WITH_SYSTRAY
    void trayIconActivated(QSystemTrayIcon::ActivationReason aReason);
    void showWindow();
#endif

    const QAction *vmNewAction() const { return mVmNewAction; }
    const QAction *vmAddAction() const { return mVmAddAction; }
    const QAction *vmConfigAction() const { return mVmConfigAction; }
    const QAction *vmCloneAction() const { return mVmCloneAction; }
    const QAction *vmDeleteAction() const { return mVmDeleteAction; }
    const QAction *vmStartAction() const { return mVmStartAction; }
    const QAction *vmDiscardAction() const { return mVmDiscardAction; }
    const QAction *vmPauseAction() const { return mVmPauseAction; }
    const QAction *vmResetAction() const { return mVmResetAction; }
    const QAction *vmRefreshAction() const { return mVmRefreshAction; }
    const QAction *vmShowLogsAction() const { return mVmShowLogsAction; }

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
    void prepareMenuHelp(QMenu *pMenu);

    /* Main menus */
    QMenu *mFileMenu;
    QMenu *mVMMenu;

    /* Central splitter window */
    QISplitter *m_pSplitter;

    /* Main toolbar */
#ifndef Q_WS_MAC
    UIMainBar *m_pBar;
#endif /* !Q_WS_MAC */
    UIToolBar *mVMToolBar;

    /* VM list context menu */
    QMenu *mVMCtxtMenu;
    QMenu *mVMCloseMenu;

    /* Actions */
    QAction *mFileMediaMgrAction;
    QAction *mFileApplianceImportAction;
    QAction *mFileApplianceExportAction;
    QAction *mFileSettingsAction;
    QAction *mFileExitAction;
    QAction *mVmNewAction;
    QAction *mVmAddAction;
    QAction *mVmConfigAction;
    QAction *mVmCloneAction;
    QAction *mVmDeleteAction;
    QAction *mVmStartAction;
    QAction *mVmDiscardAction;
    QAction *mVmPauseAction;
    QAction *mVmResetAction;
    QAction *mVmACPIShutdownAction;
    QAction *mVmPowerOffAction;
    QAction *mVmRefreshAction;
    QAction *mVmShowLogsAction;
    QAction *mVmOpenInFileManagerAction;
    QAction *mVmCreateShortcutAction;
    QAction *mVmResortAction;

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

