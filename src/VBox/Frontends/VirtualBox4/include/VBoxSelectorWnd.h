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

class QTabWidget;
class QListView;
class QEvent;

class VBoxSelectorWnd: public QIWithRetranslateUI2 <QMainWindow>
{
    Q_OBJECT;

public:

    VBoxSelectorWnd (VBoxSelectorWnd **aSelf,
                     QWidget* aParent = 0,
                     Qt::WindowFlags aFlags = Qt::Window);
    virtual ~VBoxSelectorWnd();

    bool startMachine (const QUuid &aId);

signals:

    void closing();

public slots:

    void fileMediaMgr();
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

#ifdef VBOX_GUI_WITH_SYSTRAY
    void refreshSysTray (bool aRetranslate = false);
#endif

    void refreshVMList();
    void refreshVMItem (const QUuid &aID, bool aDetails,
                                          bool aSnapshots,
                                          bool aDescription);

    void showContextMenu (const QPoint &aPoint);

#ifdef VBOX_GUI_WITH_SYSTRAY
    void trayIconActivated (QSystemTrayIcon::ActivationReason aReason);
    void showWindow();
#endif

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

private:

    /* Main menus */
    QMenu *mFileMenu;
    QMenu *mVMMenu;
    QMenu *mHelpMenu;

    /* VM list context menu */
    QMenu *mVMCtxtMenu;

    /* Actions */
    QAction *fileMediaMgrAction;
    QAction *fileSettingsAction;
    QAction *fileExitAction;
    QAction *vmNewAction;
    QAction *vmConfigAction;
    QAction *vmDeleteAction;
    QAction *vmStartAction;
    QAction *vmDiscardAction;
    QAction *vmPauseAction;
    QAction *vmRefreshAction;
    QAction *vmShowLogsAction;

#ifdef VBOX_GUI_WITH_SYSTRAY
    QAction *mTrayShowWindowAction;
    QAction *mTrayExitAction;
#endif

    VBoxHelpActions mHelpActions;

#ifdef VBOX_GUI_WITH_SYSTRAY
    /* The systray icon */
    QSystemTrayIcon *mTrayIcon;
    QMenu *mTrayIconMenu;
#endif

    /* The vm list view/model */
    VBoxVMListView *mVMListView;
    VBoxVMModel *mVMModel;

    /* The right information widgets */
    QTabWidget *vmTabWidget;
    VBoxVMDetailsView *vmDetailsView;
    VBoxSnapshotsWgt *vmSnapshotsWgt;
    VBoxVMDescriptionPage *vmDescriptionPage;

    QPoint normal_pos;
    QSize normal_size;

    bool doneInaccessibleWarningOnce : 1;
};

#endif // __VBoxSelectorWnd_h__
