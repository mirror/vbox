/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxSelectorWnd class declaration
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __VBoxSelectorWnd_h__
#define __VBoxSelectorWnd_h__

#include "COMDefs.h"

#include "QIWithRetranslateUI.h"

#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "VBoxHelpActions.h"

/* Qt includes */
#include <QMainWindow>
#ifdef VBOX_GUI_WITH_SYSTRAY
    #include <QSystemTrayIcon>
#endif

class VBoxSnapshotsWgt;
class VBoxVMDetailsView;
class VBoxVMDescriptionPage;
class VBoxVMLogViewer;
class VBoxVMListView;
class VBoxVMModel;
class VBoxVMItem;
class VBoxTrayIcon;

class QTabWidget;
class QListView;
class QEvent;

class VBoxSelectorWnd : public QIWithRetranslateUI2 <QMainWindow>
{
    Q_OBJECT;

public:

    VBoxSelectorWnd (VBoxSelectorWnd **aSelf,
                     QWidget* aParent = 0,
                     Qt::WindowFlags aFlags = Qt::Window);
    virtual ~VBoxSelectorWnd();

signals:

    void closing();

public slots:

    void fileMediaMgr();
    void fileImportAppliance();
    void fileExportAppliance();
    void fileSettings();
    void fileExit();

    void vmNew();
    void vmSettings (const QString &aCategory = QString::null,
                     const QString &aControl = QString::null,
                     const QUuid & = QUuid_null);
    void vmDelete (const QUuid & = QUuid_null);
    void vmStart (const QUuid & = QUuid_null);
    void vmDiscard (const QUuid & = QUuid_null);
    void vmPause (bool, const QUuid & = QUuid_null);
    void vmRefresh (const QUuid & = QUuid_null);
    void vmShowLogs (const QUuid & = QUuid_null);

    void refreshVMList();
    void refreshVMItem (const QUuid &aID, bool aDetails,
                                          bool aSnapshots,
                                          bool aDescription);

    void showContextMenu (const QPoint &aPoint);

#ifdef VBOX_GUI_WITH_SYSTRAY
    void trayIconActivated (QSystemTrayIcon::ActivationReason aReason);
    void showWindow();
#endif

    const QAction *vmNewAction() const { return mVmNewAction; }
    const QAction *vmConfigAction() const { return mVmConfigAction; }
    const QAction *vmDeleteAction() const { return mVmDeleteAction; }
    const QAction *vmStartAction() const { return mVmStartAction; }
    const QAction *vmDiscardAction() const { return mVmDiscardAction; }
    const QAction *vmPauseAction() const { return mVmPauseAction; }
    const QAction *vmRefreshAction() const { return mVmRefreshAction; }
    const QAction *vmShowLogsAction() const { return mVmShowLogsAction; }

protected:

    /* Events */
    bool event (QEvent *aEvent);
    void closeEvent (QCloseEvent *aEvent);
#if defined (Q_WS_MAC) && (QT_VERSION < 0x040402)
    bool eventFilter (QObject *aObject, QEvent *aEvent);
#endif /* defined (Q_WS_MAC) && (QT_VERSION < 0x040402) */

    void retranslateUi();

private slots:

    void vmListViewCurrentChanged (bool aRefreshDetails = true,
                                   bool aRefreshSnapshots = true,
                                   bool aRefreshDescription = true);

    void mediumEnumStarted();
    void mediumEnumFinished (const VBoxMediaList &);

    /* VirtualBox callback events we're interested in */

    void machineStateChanged (const VBoxMachineStateChangeEvent &e);
    void machineDataChanged (const VBoxMachineDataChangeEvent &e);
    void machineRegistered (const VBoxMachineRegisteredEvent &e);
    void sessionStateChanged (const VBoxSessionStateChangeEvent &e);
    void snapshotChanged (const VBoxSnapshotEvent &e);
#ifdef VBOX_GUI_WITH_SYSTRAY
    void mainWindowCountChanged (const VBoxMainWindowCountChangeEvent &aEvent);
    void trayIconCanShow (const VBoxCanShowTrayIconEvent &e);
    void trayIconShow (const VBoxShowTrayIconEvent &e);
    void trayIconChanged (const VBoxChangeTrayIconEvent &e);
#endif

private:

    /* Main menus */
    QMenu *mFileMenu;
    QMenu *mVMMenu;
    QMenu *mHelpMenu;

    /* VM list context menu */
    QMenu *mVMCtxtMenu;

    /* Actions */
    QAction *mFileMediaMgrAction;
    QAction *mFileApplianceImportAction;
    QAction *mFileApplianceExportAction;
    QAction *mFileSettingsAction;
    QAction *mFileExitAction;
    QAction *mVmNewAction;
    QAction *mVmConfigAction;
    QAction *mVmDeleteAction;
    QAction *mVmStartAction;
    QAction *mVmDiscardAction;
    QAction *mVmPauseAction;
    QAction *mVmRefreshAction;
    QAction *mVmShowLogsAction;

    VBoxHelpActions mHelpActions;

#ifdef VBOX_GUI_WITH_SYSTRAY
    /* The systray icon */
    VBoxTrayIcon *mTrayIcon;
#endif

    /* The vm list view/model */
    VBoxVMListView *mVMListView;
    VBoxVMModel *mVMModel;

    /* The right information widgets */
    QTabWidget *mVmTabWidget;
    VBoxVMDetailsView *mVmDetailsView;
    VBoxSnapshotsWgt *mVmSnapshotsWgt;
    VBoxVMDescriptionPage *mVmDescriptionPage;

    QRect mNormalGeo;

    bool mDoneInaccessibleWarningOnce : 1;
};

#ifdef VBOX_GUI_WITH_SYSTRAY

Q_DECLARE_METATYPE(QUuid);

class VBoxTrayIcon : public QSystemTrayIcon
{
    Q_OBJECT;

public:

    VBoxTrayIcon (VBoxSelectorWnd* aParent, VBoxVMModel* aVMModel);
    virtual ~VBoxTrayIcon ();

    void refresh ();
    void retranslateUi ();

protected:

    VBoxVMItem* GetItem (QObject* aObject);

signals:

public slots:

    void trayIconShow (bool aShow = false);

private slots:

    void showSubMenu();
    void hideSubMenu ();

    void vmSettings();
    void vmDelete();
    void vmStart();
    void vmDiscard();
    void vmPause(bool aPause);
    void vmRefresh();
    void vmShowLogs();

private:

    bool mActive;           /* Is systray menu active/available? */

    /* The vm list model */
    VBoxVMModel *mVMModel;

    VBoxSelectorWnd* mParent;
    QMenu *mTrayIconMenu;

    QAction *mShowSelectorAction;
    QAction *mHideSystrayMenuAction;
    QAction *mVmConfigAction;
    QAction *mVmDeleteAction;
    QAction *mVmStartAction;
    QAction *mVmDiscardAction;
    QAction *mVmPauseAction;
    QAction *mVmRefreshAction;
    QAction *mVmShowLogsAction;
};

#endif // VBOX_GUI_WITH_SYSTRAY

#endif // __VBoxSelectorWnd_h__
