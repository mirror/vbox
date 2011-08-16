/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxSelectorWnd class implementation
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include "precomp.h"
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */
#include "QISplitter.h"
#include "UIBar.h"
#include "UIUpdateManager.h"
#include "UIDownloaderUserManual.h"
#include "UIDownloaderExtensionPack.h"
#include "UIExportApplianceWzd.h"
#include "UIIconPool.h"
#include "UIImportApplianceWzd.h"
#include "UICloneVMWizard.h"
#include "UINewVMWzd.h"
#include "UIVMDesktop.h"
#include "UIVMListView.h"
#include "UIVirtualBoxEventHandler.h"
#include "VBoxGlobal.h"
#include "VBoxMediaManagerDlg.h"
#include "UIMessageCenter.h"
#include "VBoxSelectorWnd.h"
#include "UISettingsDialogSpecific.h"
#include "UIToolBar.h"
#include "VBoxVMLogViewer.h"
#include "QIFileDialog.h"
#include "UISelectorShortcuts.h"
#include "UIDesktopServices.h"
#include "UIGlobalSettingsExtension.h" /* extension pack installation */
#include "UIActionPool.h"

#ifdef VBOX_GUI_WITH_SYSTRAY
# include "VBoxTrayIcon.h"
# include "UIExtraDataEventHandler.h"
#endif /* VBOX_GUI_WITH_SYSTRAY */

#ifdef Q_WS_MAC
# include "VBoxUtils.h"
# include "UIWindowMenuManager.h"
# include "UIImageTools.h"
#endif

/* Global includes */
#include <QDesktopWidget>
#include <QMenuBar>
#include <QResizeEvent>
#include <QDesktopServices>

#include <iprt/buildconfig.h>
#include <VBox/version.h>
#ifdef Q_WS_X11
# include <iprt/env.h>
#endif
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

// VBoxSelectorWnd class
////////////////////////////////////////////////////////////////////////////////

/** \class VBoxSelectorWnd
 *
 *  The VBoxSelectorWnd class is a VM selector window, one of two main VBox
 *  GUI windows.
 *
 *  This window appears when the user starts the VirtualBox executable.
 *  It allows to view the list of configured VMs, their settings
 *  and the current state, create, reconfigure, delete and start VMs.
 */

/**
 *  Constructs the VM selector window.
 *
 *  @param aSelf pointer to a variable where to store |this| right after
 *               this object's constructor is called (necessary to avoid
 *               recursion in VBoxGlobal::selectorWnd())
 */
VBoxSelectorWnd::
VBoxSelectorWnd(VBoxSelectorWnd **aSelf, QWidget* aParent,
                Qt::WindowFlags aFlags /* = Qt::Window */)
    : QIWithRetranslateUI2<QMainWindow>(aParent, aFlags)
    , mDoneInaccessibleWarningOnce(false)
{
    VBoxGlobalSettings settings = vboxGlobal().settings();

    if (aSelf)
        *aSelf = this;

    statusBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(statusBar(), SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(showViewContextMenu(const QPoint&)));

#ifdef Q_WS_MAC
    qApp->installEventFilter(this);
#endif /* Q_WS_MAC */

#if !(defined (Q_WS_WIN) || defined (Q_WS_MAC))
    /* The application icon. On Win32, it's built-in to the executable. On Mac
     * OS X the icon referenced in info.plist is used. */
    setWindowIcon(QIcon(":/VirtualBox_48px.png"));
#endif

    /* actions */

    mFileMediaMgrAction = new QAction(this);
    mFileMediaMgrAction->setIcon(UIIconPool::iconSet(":/diskimage_16px.png"));

    mFileApplianceImportAction = new QAction(this);
    mFileApplianceImportAction->setIcon(UIIconPool::iconSet(":/import_16px.png"));

    mFileApplianceExportAction = new QAction(this);
    mFileApplianceExportAction->setIcon(UIIconPool::iconSet(":/export_16px.png"));

    mFileSettingsAction = new QAction(this);
    mFileSettingsAction->setMenuRole(QAction::PreferencesRole);
    mFileSettingsAction->setIcon(UIIconPool::iconSet(":/global_settings_16px.png"));
    mFileExitAction = new QAction(this);
    mFileExitAction->setMenuRole(QAction::QuitRole);
    mFileExitAction->setIcon(UIIconPool::iconSet(":/exit_16px.png"));

    mVmNewAction = new QAction(this);
    mVmNewAction->setIcon(UIIconPool::iconSetFull(
        QSize(32, 32), QSize(16, 16),
        ":/vm_new_32px.png", ":/new_16px.png"));
    mVmAddAction = new QAction(this);
    mVmAddAction->setIcon(UIIconPool::iconSet(
        ":/vm_add_16px.png"));
    mVmConfigAction = new QAction(this);
    mVmConfigAction->setIcon(UIIconPool::iconSetFull(
        QSize(32, 32), QSize(16, 16),
        ":/vm_settings_32px.png", ":/settings_16px.png",
        ":/vm_settings_disabled_32px.png", ":/settings_dis_16px.png"));
    mVmCloneAction = new QAction(this);
    mVmCloneAction->setIcon(UIIconPool::iconSet(
        ":/vm_clone_16px.png", ":/vm_clone_disabled_16px.png"));
    mVmDeleteAction = new QAction(this);
    mVmDeleteAction->setIcon(UIIconPool::iconSetFull(
        QSize(32, 32), QSize(16, 16),
        ":/vm_delete_32px.png", ":/delete_16px.png",
        ":/vm_delete_disabled_32px.png", ":/delete_dis_16px.png"));
    mVmStartAction = new QAction(this);
    mVmStartAction->setIcon(UIIconPool::iconSetFull(
        QSize(32, 32), QSize(16, 16),
        ":/vm_start_32px.png", ":/start_16px.png",
        ":/vm_start_disabled_32px.png", ":/start_dis_16px.png"));
    mVmDiscardAction = new QAction(this);
    mVmDiscardAction->setIcon(UIIconPool::iconSetFull(
        QSize(32, 32), QSize(16, 16),
        ":/vm_discard_32px.png", ":/discard_16px.png",
        ":/vm_discard_disabled_32px.png", ":/discard_dis_16px.png"));
    mVmPauseAction = new QAction(this);
    mVmPauseAction->setCheckable(true);
    mVmPauseAction->setIcon(UIIconPool::iconSetFull(
        QSize(32, 32), QSize(16, 16),
        ":/vm_pause_32px.png", ":/pause_16px.png",
        ":/vm_pause_disabled_32px.png", ":/pause_disabled_16px.png"));
    mVmRefreshAction = new QAction(this);
    mVmRefreshAction->setIcon(UIIconPool::iconSetFull(
        QSize(32, 32), QSize(16, 16),
        ":/refresh_32px.png", ":/refresh_16px.png",
        ":/refresh_disabled_32px.png", ":/refresh_disabled_16px.png"));
    mVmShowLogsAction = new QAction(this);
    mVmShowLogsAction->setIcon(UIIconPool::iconSetFull(
        QSize(32, 32), QSize(16, 16),
        ":/vm_show_logs_32px.png", ":/show_logs_16px.png",
        ":/vm_show_logs_disabled_32px.png", ":/show_logs_disabled_16px.png"));
    mVmOpenInFileManagerAction = new QAction(this);
    mVmOpenInFileManagerAction->setIcon(UIIconPool::iconSet(
        ":/vm_open_filemanager_16px.png", ":/vm_open_filemanager_disabled_16px.png"));
    mVmCreateShortcut = new QAction(this);
    mVmCreateShortcut->setIcon(UIIconPool::iconSet(
        ":/vm_create_shortcut_16px.png", ":/vm_create_shortcut_disabled_16px.png"));

    /* VM list toolbar */
    mVMToolBar = new UIToolBar(this);
    mVMToolBar->setContextMenuPolicy(Qt::CustomContextMenu);
#ifndef Q_WS_MAC
    connect(mVMToolBar, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(showViewContextMenu(const QPoint&)));
#else /* !Q_WS_MAC */
    /* A simple connect doesn't work on the Mac, also we want receive right
     * click notifications on the title bar. So register our own handler. */
    ::darwinRegisterForUnifiedToolbarContextMenuEvents(this);
#endif /* Q_WS_MAC */

    /* VM list view */
    mVMModel = new UIVMItemModel(this);
    mVMListView = new UIVMListView(mVMModel);
    mVMListView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(mVMListView, SIGNAL(sigUrlsDropped(QList<QUrl>)),
            this, SLOT(sltOpenUrls(QList<QUrl>)), Qt::QueuedConnection);

    /* Make non-possible to activate list elements by single click,
     * this hack should disable the current possibility to do it if present */
    if (mVMListView->style()->styleHint(QStyle::SH_ItemView_ActivateItemOnSingleClick, 0, mVMListView))
        mVMListView->setStyleSheet("activate-on-singleclick : 0");

    m_pSplitter = new QISplitter(this);
    m_pSplitter->setHandleType(QISplitter::Native);

#define BIG_TOOLBAR
#if MAC_LEOPARD_STYLE
    addToolBar(mVMToolBar);
    /* Central widget @ horizontal layout */
    setCentralWidget(m_pSplitter);
    m_pSplitter->addWidget(mVMListView);
#else /* MAC_LEOPARD_STYLE */
//    mVMToolBar->setContentsMargins(5, 5, 0, 0);
//    addToolBar(mVMToolBar);
//    m_pSplitter->addWidget(mVMListView);
    QWidget *pLeftWidget = new QWidget(this);
    QVBoxLayout *pLeftVLayout = new QVBoxLayout(pLeftWidget);
    pLeftVLayout->setContentsMargins(0, 0, 0, 0);
    pLeftVLayout->setSpacing(0);
# ifdef BIG_TOOLBAR
    m_pBar = new UIMainBar(this);
    m_pBar->setContentWidget(mVMToolBar);
    pLeftVLayout->addWidget(m_pBar);
    pLeftVLayout->addWidget(m_pSplitter);
    setCentralWidget(pLeftWidget);
    m_pSplitter->addWidget(mVMListView);
# else /* BIG_TOOLBAR */
    pLeftVLayout->addWidget(mVMToolBar);
    pLeftVLayout->addWidget(mVMListView);
    setCentralWidget(m_pSplitter);
    m_pSplitter->addWidget(pLeftWidget);
# endif /* BIG_TOOLBAR */
#endif /* MAC_LEOPARD_STYLE */

    /* add actions to the toolbar */

    mVMToolBar->setIconSize(QSize(32, 32));
    mVMToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
//    mVMToolBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    mVMToolBar->addAction(mVmNewAction);
    mVMToolBar->addAction(mVmConfigAction);
#if 0 /* delete action is really rare */
    mVMToolBar->addAction(mVmDeleteAction);
#endif
    mVMToolBar->addAction(mVmStartAction);
    mVMToolBar->addAction(mVmDiscardAction);

    /* VM tab widget containing details and snapshots tabs */
    QWidget *w = new QWidget();
    QVBoxLayout *pVBox = new QVBoxLayout(w);
    pVBox->setContentsMargins(0, 0, 0, 0);

    m_pVMDesktop = new UIVMDesktop(mVMToolBar, mVmRefreshAction, this);
    pVBox->addWidget(m_pVMDesktop);

    m_pSplitter->addWidget(w);

    /* Set the initial distribution. The right site is bigger. */
    m_pSplitter->setStretchFactor(0, 2);
    m_pSplitter->setStretchFactor(1, 3);

    /* Configure menubar */
    menuBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(menuBar(), SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(showViewContextMenu(const QPoint&)));

    /* add actions to menubar */
    mFileMenu = menuBar()->addMenu(QString::null);
    mFileMenu->addAction(mFileMediaMgrAction);
    mFileMenu->addAction(mFileApplianceImportAction);
    mFileMenu->addAction(mFileApplianceExportAction);
#ifndef Q_WS_MAC
    mFileMenu->addSeparator();
#endif /* Q_WS_MAC */
    mFileMenu->addAction(mFileSettingsAction);
#ifndef Q_WS_MAC
    mFileMenu->addSeparator();
#endif /* Q_WS_MAC */
    mFileMenu->addAction(mFileExitAction);

    mVMMenu = menuBar()->addMenu(QString::null);
    mVMMenu->addAction(mVmNewAction);
    mVMMenu->addAction(mVmAddAction);
    mVMMenu->addAction(mVmConfigAction);
    mVMMenu->addAction(mVmCloneAction);
    mVMMenu->addAction(mVmDeleteAction);
    mVMMenu->addSeparator();
    mVMMenu->addAction(mVmStartAction);
    mVMMenu->addAction(mVmDiscardAction);
    mVMMenu->addAction(mVmPauseAction);
    mVMMenu->addSeparator();
    mVMMenu->addAction(mVmRefreshAction);
    mVMMenu->addAction(mVmShowLogsAction);
    mVMMenu->addSeparator();
    mVMMenu->addAction(mVmOpenInFileManagerAction);
    mVMMenu->addAction(mVmCreateShortcut);

#ifdef Q_WS_MAC
    menuBar()->addMenu(UIWindowMenuManager::instance(this)->createMenu(this));
#endif /* Q_WS_MAC */

    mVMCtxtMenu = new QMenu(this);
    mVMCtxtMenu->addAction(mVmConfigAction);
    mVMCtxtMenu->addAction(mVmCloneAction);
    mVMCtxtMenu->addAction(mVmDeleteAction);
    mVMCtxtMenu->addSeparator();
    mVMCtxtMenu->addAction(mVmStartAction);
    mVMCtxtMenu->addAction(mVmDiscardAction);
    mVMCtxtMenu->addAction(mVmPauseAction);
    mVMCtxtMenu->addSeparator();
    mVMCtxtMenu->addAction(mVmRefreshAction);
    mVMCtxtMenu->addAction(mVmShowLogsAction);
    mVMCtxtMenu->addSeparator();
    mVMCtxtMenu->addAction(mVmOpenInFileManagerAction);
    mVMCtxtMenu->addAction(mVmCreateShortcut);

    /* Make sure every status bar hint from the context menu is cleared when
     * the menu is closed. */
    connect(mVMCtxtMenu, SIGNAL(aboutToHide()),
            statusBar(), SLOT(clearMessage()));

    /* Prepare help menu: */
    QMenu *pHelpMenu = gActionPool->action(UIActionIndex_Menu_Help)->menu();
    prepareMenuHelp(pHelpMenu);
    menuBar()->addMenu(pHelpMenu);

#ifdef VBOX_GUI_WITH_SYSTRAY
    mTrayIcon = new VBoxTrayIcon(this, mVMModel);
    Assert(mTrayIcon);
    connect(mTrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
#endif

    retranslateUi();

    CVirtualBox vbox = vboxGlobal().virtualBox();
    /* Restore the position of the window */
    {
        QString winPos = vbox.GetExtraData(VBoxDefs::GUI_LastWindowPosition);

        bool ok = false, max = false;
        int x = 0, y = 0, w = 0, h = 0;
        x = winPos.section(',', 0, 0).toInt(&ok);
        if (ok)
            y = winPos.section(',', 1, 1).toInt(&ok);
        if (ok)
            w = winPos.section(',', 2, 2).toInt(&ok);
        if (ok)
            h = winPos.section(',', 3, 3).toInt(&ok);
        if (ok)
            max = winPos.section(',', 4, 4) == VBoxDefs::GUI_LastWindowState_Max;

        QRect ar = ok ? QApplication::desktop()->availableGeometry(QPoint(x, y)) :
                        QApplication::desktop()->availableGeometry(this);

        if (ok /* previous parameters were read correctly */
            && (y > 0) && (y < ar.bottom()) /* check vertical bounds */
            && (x + w > ar.left()) && (x < ar.right()) /* & horizontal bounds */)
        {
            mNormalGeo.moveTo(x, y);
            mNormalGeo.setSize(QSize(w, h)
                               .expandedTo(minimumSizeHint())
                               .boundedTo(ar.size()));
#if defined(Q_WS_MAC) && (QT_VERSION >= 0x040700)
            move(mNormalGeo.topLeft());
            resize(mNormalGeo.size());
            mNormalGeo = normalGeometry();
#else /* defined(Q_WS_MAC) && (QT_VERSION >= 0x040700) */
            setGeometry(mNormalGeo);
#endif /* !(defined(Q_WS_MAC) && (QT_VERSION >= 0x040700)) */
            if (max) /* maximize if needed */
                showMaximized();
        }
        else
        {
            mNormalGeo.setSize(QSize(770, 550).expandedTo(minimumSizeHint())
                                              .boundedTo(ar.size()));
            mNormalGeo.moveCenter(ar.center());
            setGeometry(mNormalGeo);
        }
    }
#if MAC_LEOPARD_STYLE
    /* Enable unified toolbars on Mac OS X. Available on Qt >= 4.3. We do this
     * after setting the window pos/size, cause Qt sometime includes the
     * toolbar height in the content height. */
    mVMToolBar->setMacToolbar();
#endif /* MAC_LEOPARD_STYLE */

    /* Update the list */
    refreshVMList();

    /* Reset to the first item */
    mVMListView->selectItemByRow(0);

    /* restore the position of vm selector */
    {
        QString prevVMId = vbox.GetExtraData(VBoxDefs::GUI_LastVMSelected);

        mVMListView->selectItemById(prevVMId);
    }

    /* Read the splitter handle position */
    {
        QList<int> sizes = vbox.GetExtraDataIntList(VBoxDefs::GUI_SplitterSizes);
        if (sizes.size() == 2)
            m_pSplitter->setSizes(sizes);
    }

    /* Restore toolbar and statusbar visibility */
    {
        QString strToolbar = vbox.GetExtraData(VBoxDefs::GUI_Toolbar);
        QString strStatusbar = vbox.GetExtraData(VBoxDefs::GUI_Statusbar);

#ifdef Q_WS_MAC
        mVMToolBar->setVisible(strToolbar.isEmpty() || strToolbar == "true");
#else /* Q_WS_MAC */
        m_pBar->setVisible(strToolbar.isEmpty() || strToolbar == "true");
#endif /* !Q_WS_MAC */
        statusBar()->setVisible(strStatusbar.isEmpty() || strStatusbar == "true");
    }

    /* refresh the details et all (necessary for the case when the stored
     * selection is still the first list item) */
    vmListViewCurrentChanged();

    /* signals and slots connections */
    connect(mFileMediaMgrAction, SIGNAL(triggered()), this, SLOT(fileMediaMgr()));
    connect(mFileApplianceImportAction, SIGNAL(triggered()), this, SLOT(fileImportAppliance()));
    connect(mFileApplianceExportAction, SIGNAL(triggered()), this, SLOT(fileExportAppliance()));
    connect(mFileSettingsAction, SIGNAL(triggered()), this, SLOT(fileSettings()));
    connect(mFileExitAction, SIGNAL(triggered()), this, SLOT(fileExit()));
    connect(mVmNewAction, SIGNAL(triggered()), this, SLOT(vmNew()));
    connect(mVmAddAction, SIGNAL(triggered()), this, SLOT(vmAdd()));

    connect(mVmConfigAction, SIGNAL(triggered()), this, SLOT(vmSettings()));
    connect(mVmCloneAction, SIGNAL(triggered()), this, SLOT(vmClone()));
    connect(mVmDeleteAction, SIGNAL(triggered()), this, SLOT(vmDelete()));
    connect(mVmStartAction, SIGNAL(triggered()), this, SLOT(vmStart()));
    connect(mVmDiscardAction, SIGNAL(triggered()), this, SLOT(vmDiscard()));
    connect(mVmPauseAction, SIGNAL(toggled(bool)), this, SLOT(vmPause(bool)));
    connect(mVmRefreshAction, SIGNAL(triggered()), this, SLOT(vmRefresh()));
    connect(mVmShowLogsAction, SIGNAL(triggered()), this, SLOT(vmShowLogs()));
    connect(mVmOpenInFileManagerAction, SIGNAL(triggered()), this, SLOT(vmOpenInFileManager()));
    connect(mVmCreateShortcut, SIGNAL(triggered()), this, SLOT(vmCreateShortcut()));

    connect(mVMListView, SIGNAL(currentChanged()),
            this, SLOT(vmListViewCurrentChanged()));
    connect(mVMListView, SIGNAL(activated()),
            this, SLOT(vmStart()));
    connect(mVMListView, SIGNAL(customContextMenuRequested(const QPoint &)),
            this, SLOT(showContextMenu(const QPoint &)));

    connect(m_pVMDesktop, SIGNAL(linkClicked(const QString &)),
            this, SLOT(vmSettings(const QString &)));

    /* listen to media enumeration signals */
    connect(&vboxGlobal(), SIGNAL(mediumEnumStarted()),
            this, SLOT(mediumEnumStarted()));
    connect(&vboxGlobal(), SIGNAL(mediumEnumFinished(const VBoxMediaList &)),
            this, SLOT(mediumEnumFinished(const VBoxMediaList &)));

    /* connect VirtualBox events */
    connect(gVBoxEvents, SIGNAL(sigMachineStateChange(QString, KMachineState)),
            this, SLOT(machineStateChanged(QString, KMachineState)));
    connect(gVBoxEvents, SIGNAL(sigMachineDataChange(QString)),
            this, SLOT(machineDataChanged(QString)));
    connect(gVBoxEvents, SIGNAL(sigMachineRegistered(QString, bool)),
            this, SLOT(machineRegistered(QString, bool)));
    connect(gVBoxEvents, SIGNAL(sigSessionStateChange(QString, KSessionState)),
            this, SLOT(sessionStateChanged(QString, KSessionState)));
    connect(gVBoxEvents, SIGNAL(sigSnapshotChange(QString, QString)),
            this, SLOT(snapshotChanged(QString, QString)));
#ifdef VBOX_GUI_WITH_SYSTRAY
    connect(gEDataEvents, SIGNAL(sigMainWindowCountChange(int)),
            this, SLOT(mainWindowCountChanged(int)));
    connect(gEDataEvents, SIGNAL(sigCanShowTrayIcon(bool)),
            this, SLOT(trayIconCanShow(bool)));
    connect(gEDataEvents, SIGNAL(sigTrayIconChange(bool)),
            this, SLOT(trayIconChanged(bool)));
    connect(&vboxGlobal(), SIGNAL(sigTrayIconShow(bool)),
            this, SLOT(trayIconShow(bool)));
#endif

    /* Listen to potential downloaders signals: */
    connect(&msgCenter(), SIGNAL(sigDownloaderUserManualCreated()), this, SLOT(sltEmbedDownloaderForUserManual()));
    connect(gUpdateManager, SIGNAL(sigDownloaderCreatedForExtensionPack()), this, SLOT(sltEmbedDownloaderForExtensionPack()));

    /* bring the VM list to the focus */
    mVMListView->setFocus();

#ifdef Q_WS_MAC
    UIWindowMenuManager::instance()->addWindow(this);
    /* Beta label? */
    if (vboxGlobal().isBeta())
    {
        QPixmap betaLabel = ::betaLabelSleeve(QSize(107, 16));
        ::darwinLabelWindow(this, &betaLabel, false);
    }
#endif /* Q_WS_MAC */
}

VBoxSelectorWnd::~VBoxSelectorWnd()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();

    /* Destroy our event handlers */
    UIVirtualBoxEventHandler::destroy();

    /* Save the position of the window */
    {
#if defined(Q_WS_MAC) && (QT_VERSION >= 0x040700)
        QRect frameGeo = frameGeometry();
        QRect save(frameGeo.x(), frameGeo.y(), mNormalGeo.width(), mNormalGeo.height());
#else /* defined(Q_WS_MAC) && (QT_VERSION >= 0x040700) */
        QRect save(mNormalGeo);
#endif /* !(defined(Q_WS_MAC) && (QT_VERSION >= 0x040700)) */
        QString winPos = QString("%1,%2,%3,%4")
            .arg(save.x()).arg(save.y())
            .arg(save.width()).arg(save.height());
#ifdef Q_WS_MAC
        UIWindowMenuManager::destroy();
        ::darwinUnregisterForUnifiedToolbarContextMenuEvents(this);
        if (::darwinIsWindowMaximized(this))
#else /* Q_WS_MAC */
        if (isMaximized())
#endif /* !Q_WS_MAC */
            winPos += QString(",%1").arg(VBoxDefs::GUI_LastWindowState_Max);

        vbox.SetExtraData(VBoxDefs::GUI_LastWindowPosition, winPos);
    }

    /* Save VM selector position */
    {
        UIVMItem *item = mVMListView->selectedItem();
        QString curVMId = item ?
            QString(item->id()) :
            QString::null;
        vbox.SetExtraData(VBoxDefs::GUI_LastVMSelected, curVMId);
        vbox.SetExtraDataStringList(VBoxDefs::GUI_SelectorVMPositions, mVMModel->idList());
    }

    /* Save the splitter handle position */
    {
        vbox.SetExtraDataIntList(VBoxDefs::GUI_SplitterSizes, m_pSplitter->sizes());
    }

#ifdef VBOX_GUI_WITH_SYSTRAY
    /* Delete systray menu object */
    delete mTrayIcon;
    mTrayIcon = NULL;
#endif

    /* Delete the items from our model */
    mVMModel->clear();
}

//
// Public slots
/////////////////////////////////////////////////////////////////////////////

void VBoxSelectorWnd::fileMediaMgr()
{
    VBoxMediaManagerDlg::showModeless(this);
}

void VBoxSelectorWnd::fileImportAppliance(const QString &strFile /* = "" */)
{
#ifdef Q_WS_MAC
    QString strTmpFile = ::darwinResolveAlias(strFile);
#else /* Q_WS_MAC */
    QString strTmpFile = strFile;
#endif /* !Q_WS_MAC */
    UIImportApplianceWzd wzd(strTmpFile, this);

    if (   strFile.isEmpty()
        || wzd.isValid())
        wzd.exec();
}

void VBoxSelectorWnd::fileExportAppliance()
{
    QString name;

    UIVMItem *item = mVMListView->selectedItem();
    if (item)
        name = item->name();

    UIExportApplianceWzd wzd(this, name);

    wzd.exec();
}

void VBoxSelectorWnd::fileSettings()
{
    /* Check that we do NOT handling that already: */
    if (mFileSettingsAction->data().toBool())
        return;
    /* Remember that we handling that already: */
    mFileSettingsAction->setData(true);

    /* Create and execute global settings dialog: */
    UISettingsDialogGlobal dlg(this);
    dlg.execute();

    /* Remember that we do NOT handling that already: */
    mFileSettingsAction->setData(false);
}

void VBoxSelectorWnd::fileExit()
{
    /* We have to check if there are any open windows beside this mainwindow
     * (e.g. VDM) and if so close them. Note that the default behavior is
     * different to Qt3 where a *mainWidget* exists & if this going to close
     * all other windows are closed automatically. We do the same below. */
    foreach(QWidget *widget, QApplication::topLevelWidgets())
    {
        if (widget->isVisible() &&
            widget != this)
            widget->close();
    }
    /* We close this widget last. */
    close();
}

void VBoxSelectorWnd::vmNew()
{
    UINewVMWzd wzd(this);
    if (wzd.exec() == QDialog::Accepted)
    {
        CMachine m = wzd.machine();

        /* wait until the list is updated by OnMachineRegistered() */
        QModelIndex index;
        while(!index.isValid())
        {
            qApp->processEvents();
            index = mVMModel->indexById(m.GetId());
        }
        mVMListView->setCurrentIndex(index);
    }
}

void VBoxSelectorWnd::vmAdd(const QString &strFile /* = "" */)
{
#ifdef Q_WS_MAC
    QString strTmpFile = ::darwinResolveAlias(strFile);
#else /* Q_WS_MAC */
    QString strTmpFile = strFile;
#endif /* !Q_WS_MAC */
    /* Initialize variables: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    if (strTmpFile.isEmpty())
    {
        QString strBaseFolder = vbox.GetSystemProperties().GetDefaultMachineFolder();
        QString strTitle = tr("Select a virtual machine file");
        QStringList extensions;
        for (int i = 0; i < VBoxDefs::VBoxFileExts.size(); ++i)
            extensions << QString("*.%1").arg(VBoxDefs::VBoxFileExts[i]);
        QString strFilter = tr("Virtual machine files (%1)").arg(extensions.join(" "));
        /* Create open file dialog: */
        QStringList fileNames = QIFileDialog::getOpenFileNames(strBaseFolder, strFilter, this, strTitle, 0, true, true);
        if (!fileNames.isEmpty())
            strTmpFile = fileNames.at(0);
    }
    if (!strTmpFile.isEmpty())
    {
        /* Open corresponding machine: */
        CMachine newMachine = vbox.OpenMachine(strTmpFile);
        /* First we should test what machine was opened: */
        if (vbox.isOk() && !newMachine.isNull())
        {
            /* Second we should check what such machine was NOT already registered.
             * Actually current Main implementation will even prevent such machine opening
             * but we will perform such a check anyway: */
            CMachine oldMachine = vbox.FindMachine(newMachine.GetId());
            if (oldMachine.isNull())
            {
                /* Register that machine: */
                vbox.RegisterMachine(newMachine);

                /* Wait until the list is updated by OnMachineRegistered(): */
                QModelIndex index;
                while(!index.isValid())
                {
                    qApp->processEvents();
                    index = mVMModel->indexById(newMachine.GetId());
                }
                /* And choose the new machine item: */
                mVMListView->setCurrentIndex(index);
            }
            else
                msgCenter().cannotReregisterMachine(this, strTmpFile, oldMachine.GetName());
        }
        else
            msgCenter().cannotOpenMachine(this, strTmpFile, vbox);
    }
}

/**
 *  Opens the VM settings dialog.
 */
void VBoxSelectorWnd::vmSettings(const QString &strCategoryRef /* = QString::null */,
                                 const QString &strControlRef /* = QString::null */,
                                 const QString &strMachineId /* = QString::null */)
{
    /* Check that we do NOT handling that already: */
    if (mVmConfigAction->data().toBool())
        return;
    /* Remember that we handling that already: */
    mVmConfigAction->setData(true);

    /* Process href from VM details / description: */
    if (!strCategoryRef.isEmpty() && strCategoryRef[0] != '#')
    {
        vboxGlobal().openURL(strCategoryRef);
        return;
    }

    /* Get category and control: */
    QString strCategory = strCategoryRef;
    QString strControl = strControlRef;
    /* Check if control is coded into the URL by %%: */
    if (strControl.isEmpty())
    {
        QStringList parts = strCategory.split("%%");
        if (parts.size() == 2)
        {
            strCategory = parts.at(0);
            strControl = parts.at(1);
        }
    }

    /* Don't show the inaccessible warning if the user tries to open VM settings: */
    mDoneInaccessibleWarningOnce = true;

    /* Get corresponding VM item: */
    UIVMItem *pItem = strMachineId.isNull() ? mVMListView->selectedItem() : mVMModel->itemById(strMachineId);
    AssertMsgReturnVoid(pItem, ("Item must be always selected here!\n"));

    /* Create and execute corresponding VM settings dialog: */
    UISettingsDialogMachine dlg(this, pItem->id(), strCategory, strControl);
    dlg.execute();

    /* Remember that we do NOT handling that already: */
    mVmConfigAction->setData(false);
}

void VBoxSelectorWnd::vmClone(const QString &aUuid /* = QString::null */)
{
    UIVMItem *item = aUuid.isNull() ? mVMListView->selectedItem() : mVMModel->itemById(aUuid);

    AssertMsgReturnVoid(item, ("Item must be always selected here"));

    CMachine machine = item->machine();

    UICloneVMWizard wzd(this, machine);
    wzd.exec();
}

void VBoxSelectorWnd::vmDelete(const QString &aUuid /* = QString::null */)
{
    UIVMItem *item = aUuid.isNull() ? mVMListView->selectedItem() : mVMModel->itemById(aUuid);

    AssertMsgReturnVoid(item, ("Item must be always selected here"));

    CMachine machine = item->machine();
    int rc = msgCenter().confirmMachineDeletion(machine);
    if (rc != QIMessageBox::Cancel)
    {
        if (rc == QIMessageBox::Yes)
        {
            /* Unregister and cleanup machine's data & hard-disks: */
            CMediumVector mediums = machine.Unregister(KCleanupMode_DetachAllReturnHardDisksOnly);
            if (machine.isOk())
            {
                /* Delete machine hard-disks: */
                CProgress progress = machine.Delete(mediums);
                if (machine.isOk())
                {
                    msgCenter().showModalProgressDialog(progress, item->name(), ":/progress_delete_90px.png", 0, true);
                    if (progress.GetResultCode() != 0)
                        msgCenter().cannotDeleteMachine(machine, progress);
                }
            }
            if (!machine.isOk())
                msgCenter().cannotDeleteMachine(machine);
        }
        else
        {
            /* Just unregister machine: */
            machine.Unregister(KCleanupMode_DetachAllReturnNone);
            if (!machine.isOk())
                msgCenter().cannotDeleteMachine(machine);
        }
    }
}

void VBoxSelectorWnd::vmStart(const QString &aUuid /* = QString::null */)
{
    QUuid uuid(aUuid);
    UIVMItem *item = uuid.isNull() ? mVMListView->selectedItem() :
                       mVMModel->itemById(aUuid);

    AssertMsgReturnVoid(item, ("Item must be always selected here"));

    /* Are we called from the mVMListView's activated() signal? */
    if (uuid.isNull())
    {
        /* We always get here when mVMListView emits the activated() signal,
         * so we must explicitly check if the action is enabled or not. */
        if (!mVmStartAction->isEnabled())
            return;
    }

    AssertMsg(!vboxGlobal().isVMConsoleProcess(),
              ("Must NOT be a VM console process"));

    CMachine machine = item->machine();
    vboxGlobal().launchMachine(machine, qApp->keyboardModifiers() == Qt::ShiftModifier);
}

void VBoxSelectorWnd::vmDiscard(const QString &aUuid /* = QString::null */)
{
    UIVMItem *item = aUuid.isNull() ? mVMListView->selectedItem() :
                       mVMModel->itemById(aUuid);

    AssertMsgReturnVoid(item, ("Item must be always selected here"));

    if (!msgCenter().confirmDiscardSavedState(item->machine()))
        return;

    /* open a session to modify VM settings */
    QString id = item->id();
    CSession session;
    CVirtualBox vbox = vboxGlobal().virtualBox();
    session.createInstance(CLSID_Session);
    if (session.isNull())
    {
        msgCenter().cannotOpenSession(session);
        return;
    }

    CMachine foundMachine = vbox.FindMachine(id);
    if (!foundMachine.isNull())
        foundMachine.LockMachine(session, KLockType_Write);
    if (!vbox.isOk())
    {
        msgCenter().cannotOpenSession(vbox, item->machine());
        return;
    }

    CConsole console = session.GetConsole();
    console.DiscardSavedState(true /* fDeleteFile */);
    if (!console.isOk())
        msgCenter().cannotDiscardSavedState(console);

    session.UnlockMachine();
}

void VBoxSelectorWnd::vmPause(bool aPause, const QString &aUuid /* = QString::null */)
{
    UIVMItem *item = aUuid.isNull() ? mVMListView->selectedItem() :
                       mVMModel->itemById(aUuid);

    AssertMsgReturnVoid(item, ("Item must be always selected here"));

    CSession session = vboxGlobal().openExistingSession(item->id());
    if (session.isNull())
        return;

    CConsole console = session.GetConsole();
    if (console.isNull())
        return;

    if (aPause)
        console.Pause();
    else
        console.Resume();

    bool ok = console.isOk();
    if (!ok)
    {
        if (aPause)
            msgCenter().cannotPauseMachine(console);
        else
            msgCenter().cannotResumeMachine(console);
    }

    session.UnlockMachine();
}

void VBoxSelectorWnd::vmRefresh(const QString &aUuid /* = QString::null */)
{
    UIVMItem *item = aUuid.isNull() ? mVMListView->selectedItem() :
                       mVMModel->itemById(aUuid);

    AssertMsgReturnVoid(item, ("Item must be always selected here"));

    refreshVMItem(item->id(),
                   true /* aDetails */,
                   true /* aSnapshot */,
                   true /* aDescription */);
}

void VBoxSelectorWnd::vmShowLogs(const QString &aUuid /* = QString::null */)
{
    UIVMItem *item = aUuid.isNull() ? mVMListView->selectedItem() :
                       mVMModel->itemById(aUuid);

    AssertMsgReturnVoid(item, ("Item must be always selected here"));

    CMachine machine = item->machine();
    VBoxVMLogViewer::createLogViewer(this, machine);
}

void VBoxSelectorWnd::vmOpenInFileManager(const QString &aUuid /* = QString::null */)
{
    UIVMItem *item = aUuid.isNull() ? mVMListView->selectedItem() :
                       mVMModel->itemById(aUuid);

    AssertMsgReturnVoid(item, ("Item must be always selected here"));

    CMachine machine = item->machine();
    UIDesktopServices::openInFileManager(machine.GetSettingsFilePath());
}

void VBoxSelectorWnd::vmCreateShortcut(const QString &aUuid /* = QString::null */)
{
    UIVMItem *item = aUuid.isNull() ? mVMListView->selectedItem() :
                       mVMModel->itemById(aUuid);

    AssertMsgReturnVoid(item, ("Item must be always selected here"));

    CMachine machine = item->machine();
    UIDesktopServices::createMachineShortcut(machine.GetSettingsFilePath(), QDesktopServices::storageLocation(QDesktopServices::DesktopLocation), machine.GetName(), machine.GetId());
}

void VBoxSelectorWnd::refreshVMList()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    CMachineVector vec = vbox.GetMachines();
    for (CMachineVector::ConstIterator m = vec.begin();
         m != vec.end(); ++m)
        mVMModel->addItem(*m);
    /* Apply the saved sort order. */
    mVMModel->sortByIdList(vbox.GetExtraDataStringList(VBoxDefs::GUI_SelectorVMPositions));
    /* Update details page. */
    vmListViewCurrentChanged();

#ifdef VBOX_GUI_WITH_SYSTRAY
    if (vboxGlobal().isTrayMenu())
        mTrayIcon->refresh();
#endif
}

void VBoxSelectorWnd::refreshVMItem(const QString &aID,
                                    bool aDetails,
                                    bool aSnapshots,
                                    bool aDescription)
{
    UIVMItem *item = mVMModel->itemById(aID);
    if (item)
    {
        mVMModel->refreshItem(item);
        if (item && item->id() == aID)
            vmListViewCurrentChanged(aDetails, aSnapshots, aDescription);
    }
}

void VBoxSelectorWnd::showContextMenu(const QPoint &aPoint)
{
    /* Send a context menu request */
    const QModelIndex &index = mVMListView->indexAt(aPoint);
    if (index.isValid())
        if (mVMListView->model()->data(index,
            UIVMItemModel::UIVMItemPtrRole).value <UIVMItem*>())
                mVMCtxtMenu->exec(mVMListView->mapToGlobal(aPoint));
}


void VBoxSelectorWnd::sltOpenUrls(QList<QUrl> list /* = QList<QUrl>() */)
{
    /* Make sure any pending D&D events are consumed. */
    qApp->processEvents();
    if (list.isEmpty())
    {
        list = vboxGlobal().argUrlList();
        vboxGlobal().argUrlList().clear();
    }
    /* Check if we are can handle the dropped urls. */
    for (int i = 0; i < list.size(); ++i)
    {
#ifdef Q_WS_MAC
        QString strFile = ::darwinResolveAlias(list.at(i).toLocalFile());
#else /* Q_WS_MAC */
        QString strFile = list.at(i).toLocalFile();
#endif /* !Q_WS_MAC */
        if (   !strFile.isEmpty()
            && QFile::exists(strFile))
        {
            if (VBoxGlobal::hasAllowedExtension(strFile, VBoxDefs::VBoxFileExts))
            {
                /* VBox config files. */
                CVirtualBox vbox = vboxGlobal().virtualBox();
                CMachine machine = vbox.FindMachine(strFile);
                if (!machine.isNull())
                {
                    CVirtualBox vbox = vboxGlobal().virtualBox();
                    CMachine machine = vbox.FindMachine(strFile);
                    if (!machine.isNull())
                        vboxGlobal().launchMachine(machine);
                }else
                    vmAdd(strFile);
            }
            else if (VBoxGlobal::hasAllowedExtension(strFile, VBoxDefs::OVFFileExts))
            {
                /* OVF/OVA. Only one file at the time. */
                fileImportAppliance(strFile);
                break;
            }
            else if (VBoxGlobal::hasAllowedExtension(strFile, VBoxDefs::VBoxExtPackFileExts))
            {
                UIGlobalSettingsExtension::doInstallation(strFile, this, NULL);
            }
        }
    }
}

#ifdef VBOX_GUI_WITH_SYSTRAY

void VBoxSelectorWnd::trayIconActivated(QSystemTrayIcon::ActivationReason aReason)
{
    switch (aReason)
    {
        case QSystemTrayIcon::Context:

            mTrayIcon->refresh();
            break;

        case QSystemTrayIcon::Trigger:
            break;

        case QSystemTrayIcon::DoubleClick:

            vboxGlobal().trayIconShowSelector();
            break;

        case QSystemTrayIcon::MiddleClick:
            break;

        default:
            break;
    }
}

void VBoxSelectorWnd::showWindow()
{
    showNormal();
    raise();
    activateWindow();
}

#endif // VBOX_GUI_WITH_SYSTRAY

// Protected members
/////////////////////////////////////////////////////////////////////////////

bool VBoxSelectorWnd::event(QEvent *e)
{
    switch (e->type())
    {
        /* By handling every Resize and Move we keep track of the normal
         * (non-minimized and non-maximized) window geometry. Shame on Qt
         * that it doesn't provide this geometry in its public APIs. */

        case QEvent::Resize:
        {
            QResizeEvent *re = (QResizeEvent *) e;
            if ((windowState() & (Qt::WindowMaximized | Qt::WindowMinimized |
                                  Qt::WindowFullScreen)) == 0)
                mNormalGeo.setSize(re->size());
            break;
        }
        case QEvent::Move:
        {
            if ((windowState() & (Qt::WindowMaximized | Qt::WindowMinimized |
                                  Qt::WindowFullScreen)) == 0)
                mNormalGeo.moveTo(geometry().x(), geometry().y());
            break;
        }
        case QEvent::WindowDeactivate:
        {
            /* Make sure every status bar hint is cleared when the window lost
             * focus. */
            statusBar()->clearMessage();
            break;
        }
#ifdef Q_WS_MAC
        case QEvent::ContextMenu:
        {
            /* This is the unified context menu event. Lets show the context
             * menu. */
            QContextMenuEvent *pCE = static_cast<QContextMenuEvent*>(e);
            showViewContextMenu(pCE->globalPos());
            /* Accept it to interrupt the chain. */
            pCE->accept();
            return false;
            break;
        }
        case QEvent::ToolBarChange:
        {
            CVirtualBox vbox = vboxGlobal().virtualBox();
            /* We have to invert the isVisible check one time, cause this event
             * is sent *before* the real toggle is done. Really intuitive
             * Trolls. */
            vbox.SetExtraData(VBoxDefs::GUI_Toolbar, !::darwinIsToolbarVisible(mVMToolBar) ? "true" : "false");
            break;
        }
#endif /* Q_WS_MAC */
        default:
            break;
    }

    return QMainWindow::event(e);
}

void VBoxSelectorWnd::closeEvent(QCloseEvent *aEvent)
{
#ifdef VBOX_GUI_WITH_SYSTRAY
    /* Needed for breaking out of the while() loop in main(). */
    if (vboxGlobal().isTrayMenu())
        vboxGlobal().setTrayMenu(false);
#endif

    emit closing();
    QMainWindow::closeEvent(aEvent);
}

#ifdef Q_WS_MAC
bool VBoxSelectorWnd::eventFilter(QObject *pObject, QEvent *pEvent)
{
    if (!isActiveWindow())
        return QIWithRetranslateUI2<QMainWindow>::eventFilter(pObject, pEvent);

    if (qobject_cast<QWidget*>(pObject) &&
        qobject_cast<QWidget*>(pObject)->window() != this)
        return QIWithRetranslateUI2<QMainWindow>::eventFilter(pObject, pEvent);

    switch (pEvent->type())
    {
        case QEvent::FileOpen:
        {
            sltOpenUrls(QList<QUrl>() << static_cast<QFileOpenEvent*>(pEvent)->file());
            pEvent->accept();
            return true;
            break;
        }
# if (QT_VERSION < 0x040402)
        case QEvent::KeyPress:
        {
            /* Bug in Qt below 4.4.2. The key events are send to the current
             * window even if a menu is shown & has the focus. See
             * http://trolltech.com/developer/task-tracker/index_html?method=entry&id=214681. */
            if (::darwinIsMenuOpen())
                return true;
            break;
        }
# endif
        default:
            break;
    }
    return QIWithRetranslateUI2<QMainWindow>::eventFilter(pObject, pEvent);
}
#endif /* Q_WS_MAC */

/**
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void VBoxSelectorWnd::retranslateUi()
{
    QString title(VBOX_PRODUCT);
    title += " " + tr("Manager", "Note: main window title which is pretended by the product name.");

#ifdef VBOX_BLEEDING_EDGE
    title += QString(" EXPERIMENTAL build ")
          +  QString(RTBldCfgVersion())
          +  QString(" r")
          +  QString(RTBldCfgRevisionStr())
          +  QString(" - "VBOX_BLEEDING_EDGE);
#endif

    setWindowTitle(title);

    /* ensure the details and screenshot view are updated */
    vmListViewCurrentChanged();

    mFileMediaMgrAction->setText(tr("&Virtual Media Manager..."));
    mFileMediaMgrAction->setShortcut(gSS->keySequence(UISelectorShortcuts::VirtualMediaManagerShortcut));
    mFileMediaMgrAction->setStatusTip(tr("Display the Virtual Media Manager dialog"));

    mFileApplianceImportAction->setText(tr("&Import Appliance..."));
    mFileApplianceImportAction->setShortcut(gSS->keySequence(UISelectorShortcuts::ImportApplianceShortcut));
    mFileApplianceImportAction->setStatusTip(tr("Import an appliance into VirtualBox"));

    mFileApplianceExportAction->setText(tr("&Export Appliance..."));
    mFileApplianceExportAction->setShortcut(gSS->keySequence(UISelectorShortcuts::ExportApplianceShortcut));
    mFileApplianceExportAction->setStatusTip(tr("Export one or more VirtualBox virtual machines as an appliance"));

    mFileSettingsAction->setText(tr("&Preferences...", "global settings"));
    mFileSettingsAction->setShortcut(gSS->keySequence(UISelectorShortcuts::PreferencesShortcut));
    mFileSettingsAction->setStatusTip(tr("Display the global settings dialog"));

    mFileExitAction->setText(tr("E&xit"));
    mFileExitAction->setShortcut(gSS->keySequence(UISelectorShortcuts::ExitShortcut));
    mFileExitAction->setStatusTip(tr("Close application"));

    mVmNewAction->setText(tr("&New..."));
    mVmNewAction->setShortcut(gSS->keySequence(UISelectorShortcuts::NewVMShortcut));
    mVmNewAction->setStatusTip(tr("Create a new virtual machine"));
    mVmNewAction->setToolTip(mVmNewAction->text().remove('&').remove('.') +
        (mVmNewAction->shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(mVmNewAction->shortcut().toString())));

    mVmAddAction->setText(tr("&Add..."));
    mVmAddAction->setShortcut(gSS->keySequence(UISelectorShortcuts::AddVMShortcut));
    mVmAddAction->setStatusTip(tr("Add an existing virtual machine"));

    mVmConfigAction->setText(tr("&Settings..."));
    mVmConfigAction->setShortcut(gSS->keySequence(UISelectorShortcuts::SettingsVMShortcut));
    mVmConfigAction->setStatusTip(tr("Configure the selected virtual machine"));
    mVmConfigAction->setToolTip(mVmConfigAction->text().remove('&').remove('.') +
        (mVmConfigAction->shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(mVmConfigAction->shortcut().toString())));

    mVmCloneAction->setText(tr("Cl&one..."));
    mVmCloneAction->setShortcut(gSS->keySequence(UISelectorShortcuts::CloneVMShortcut));
    mVmCloneAction->setStatusTip(tr("Clone the selected virtual machine"));

    mVmDeleteAction->setText(tr("&Remove"));
    mVmDeleteAction->setShortcut(gSS->keySequence(UISelectorShortcuts::RemoveVMShortcut));
    mVmDeleteAction->setStatusTip(tr("Remove the selected virtual machine"));

    /* Note: mVmStartAction text is set up in vmListViewCurrentChanged() */

    mVmDiscardAction->setIconText(tr("Discard"));
    mVmDiscardAction->setText(tr("D&iscard Saved State"));
    mVmDiscardAction->setShortcut(gSS->keySequence(UISelectorShortcuts::DiscardVMShortcut));
    mVmDiscardAction->setStatusTip(
        tr("Discard the saved state of the selected virtual machine"));
    mVmDiscardAction->setToolTip(mVmDiscardAction->text().remove('&').remove('.') +
        (mVmDiscardAction->shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(mVmDiscardAction->shortcut().toString())));

    mVmPauseAction->setText(tr("&Pause"));
    mVmPauseAction->setStatusTip(
        tr("Suspend the execution of the virtual machine"));

    mVmRefreshAction->setText(tr("Re&fresh"));
    mVmRefreshAction->setShortcut(gSS->keySequence(UISelectorShortcuts::RefreshVMShortcut));
    mVmRefreshAction->setStatusTip(
        tr("Refresh the accessibility state of the selected virtual machine"));

    mVmShowLogsAction->setText(tr("Show &Log..."));
    mVmShowLogsAction->setIconText(tr("Log", "icon text"));
    mVmShowLogsAction->setShortcut(gSS->keySequence(UISelectorShortcuts::ShowVMLogShortcut));
    mVmShowLogsAction->setStatusTip(
        tr("Show the log files of the selected virtual machine"));

#if defined(Q_WS_MAC)
    mVmOpenInFileManagerAction->setText(tr("Show in Finder"));
    mVmOpenInFileManagerAction->setStatusTip(tr("Show the VirtualBox Machine Definition file in Finder."));
    mVmCreateShortcut->setText(tr("Create Alias on Desktop"));
    mVmCreateShortcut->setStatusTip(tr("Creates an Alias file to the VirtualBox Machine Definition file on your Desktop."));
#elif defined(Q_WS_WIN)
    mVmOpenInFileManagerAction->setText(tr("Show in Explorer"));
    mVmOpenInFileManagerAction->setStatusTip(tr("Show the VirtualBox Machine Definition file in Explorer."));
    mVmCreateShortcut->setText(tr("Create Shortcut on Desktop"));
    mVmCreateShortcut->setStatusTip(tr("Creates an Shortcut file to the VirtualBox Machine Definition file on your Desktop."));
#else
    mVmOpenInFileManagerAction->setText(tr("Show in File Manager"));
    mVmOpenInFileManagerAction->setStatusTip(tr("Show the VirtualBox Machine Definition file in the File Manager"));
    mVmCreateShortcut->setText(tr("Create Shortcut on Desktop"));
    mVmCreateShortcut->setStatusTip(tr("Creates an Shortcut file to the VirtualBox Machine Definition file on your Desktop."));
#endif
    mVmOpenInFileManagerAction->setShortcut(gSS->keySequence(UISelectorShortcuts::ShowVMInFileManagerShortcut));
    mVmCreateShortcut->setShortcut(gSS->keySequence(UISelectorShortcuts::CreateVMAliasShortcut));

#ifdef Q_WS_MAC
    mFileMenu->setTitle(tr("&File", "Mac OS X version"));
#else /* Q_WS_MAC */
    mFileMenu->setTitle(tr("&File", "Non Mac OS X version"));
#endif /* !Q_WS_MAC */
    mVMMenu->setTitle(tr("&Machine"));

#ifdef VBOX_GUI_WITH_SYSTRAY
    if (vboxGlobal().isTrayMenu())
    {
        mTrayIcon->retranslateUi();
        mTrayIcon->refresh();
    }
#endif

#ifdef QT_MAC_USE_COCOA
    /* There is a bug in Qt Cocoa which result in showing a "more arrow" when
       the necessary size of the toolbar is increased. Also for some languages
       the with doesn't match if the text increase. So manually adjust the size
       after changing the text. */
    mVMToolBar->updateLayout();
#endif /* QT_MAC_USE_COCOA */
}


// Private members
/////////////////////////////////////////////////////////////////////////////

//
// Private slots
/////////////////////////////////////////////////////////////////////////////

void VBoxSelectorWnd::vmListViewCurrentChanged(bool aRefreshDetails,
                                               bool aRefreshSnapshots,
                                               bool aRefreshDescription)
{
    UIVMItem *item = mVMListView->selectedItem();

    if (item && item->accessible())
    {
        CMachine m = item->machine();

        KMachineState state = item->machineState();
        bool running = item->sessionState() != KSessionState_Unlocked;
        bool modifyEnabled = state != KMachineState_Stuck && state != KMachineState_Saved /* for now! */;

        if (   aRefreshDetails
            || aRefreshDescription)
            m_pVMDesktop->updateDetails(item, m);
        if (aRefreshSnapshots)
            m_pVMDesktop->updateSnapshots(item, m);
//        if (aRefreshDescription)
//            m_pVMDesktop->updateDescription(item, m);

        /* enable/disable modify actions */
        mVmConfigAction->setEnabled(modifyEnabled);
        mVmCloneAction->setEnabled(!running);
        mVmDeleteAction->setEnabled(!running);
        mVmDiscardAction->setEnabled(state == KMachineState_Saved && !running);
        mVmPauseAction->setEnabled(   state == KMachineState_Running
                                   || state == KMachineState_Teleporting
                                   || state == KMachineState_LiveSnapshotting
                                   || state == KMachineState_Paused
                                   || state == KMachineState_TeleportingPausedVM /** @todo Live Migration: does this make sense? */
                                   );

        /* change the Start button text accordingly */
        if (   state == KMachineState_PoweredOff
            || state == KMachineState_Saved
            || state == KMachineState_Teleported
            || state == KMachineState_Aborted
           )
        {
            mVmStartAction->setText(tr("S&tart"));
            mVmStartAction->setShortcut(gSS->keySequence(UISelectorShortcuts::StartVMShortcut));
            mVmStartAction->setToolTip(mVmStartAction->text().remove('&').remove('.') +
                  (mVmStartAction->shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(mVmStartAction->shortcut().toString())));
#ifdef QT_MAC_USE_COCOA
            /* There is a bug in Qt Cocoa which result in showing a "more arrow" when
               the necessary size of the toolbar is increased. Also for some languages
               the with doesn't match if the text increase. So manually adjust the size
               after changing the text. */
            mVMToolBar->updateLayout();
#endif /* QT_MAC_USE_COCOA */
            mVmStartAction->setStatusTip(
                tr("Start the selected virtual machine"));

            mVmStartAction->setEnabled(!running);
        }
        else
        {
            mVmStartAction->setText(tr("S&how"));
            mVmStartAction->setShortcut(gSS->keySequence(UISelectorShortcuts::StartVMShortcut));
            mVmStartAction->setToolTip(mVmStartAction->text().remove('&').remove('.') +
                  (mVmStartAction->shortcut().toString().isEmpty() ? "" : QString(" (%1)").arg(mVmStartAction->shortcut().toString())));
#ifdef QT_MAC_USE_COCOA
            /* There is a bug in Qt Cocoa which result in showing a "more arrow" when
               the necessary size of the toolbar is increased. Also for some languages
               the with doesn't match if the text increase. So manually adjust the size
               after changing the text. */
            mVMToolBar->updateLayout();
#endif /* QT_MAC_USE_COCOA */
            mVmStartAction->setStatusTip(
                tr("Switch to the window of the selected virtual machine"));

            mVmStartAction->setEnabled(item->canSwitchTo());
        }

        /* change the Pause/Resume button text accordingly */
        if (   state == KMachineState_Paused
            || state == KMachineState_TeleportingPausedVM /*?*/
           )
        {
            mVmPauseAction->setShortcut(gSS->keySequence(UISelectorShortcuts::PauseVMShortcut));
            mVmPauseAction->setStatusTip(
                tr("Resume the execution of the virtual machine"));
            mVmPauseAction->blockSignals(true);
            mVmPauseAction->setChecked(true);
            mVmPauseAction->blockSignals(false);
        }
        else
        {
            mVmPauseAction->setShortcut(gSS->keySequence(UISelectorShortcuts::PauseVMShortcut));
            mVmPauseAction->setStatusTip(
                tr("Suspend the execution of the virtual machine"));
            mVmPauseAction->blockSignals(true);
            mVmPauseAction->setChecked(false);
            mVmPauseAction->blockSignals(false);
        }

        /* disable Refresh for accessible machines */
        mVmRefreshAction->setEnabled(false);

        /* enable the show log item for the selected vm */
        mVmShowLogsAction->setEnabled(true);
        /* Enable the shell interaction features. */
        mVmOpenInFileManagerAction->setEnabled(true);
#ifdef Q_WS_MAC
        /* On Mac OS X this are real alias files, which don't work with the old
         * legacy xml files. On the other OS's some kind of start up script is
         * used. */
        mVmCreateShortcut->setEnabled(item->settingsFile().endsWith(".vbox", Qt::CaseInsensitive));
#else /* Q_WS_MAC */
        mVmCreateShortcut->setEnabled(true);
#endif /* Q_WS_MAC */
    }
    else
    {
        /* Note that the machine becomes inaccessible (or if the last VM gets
         * deleted), we have to update all fields, ignoring input
         * arguments. */

        if (item)
        {
            /* the VM is inaccessible */
            m_pVMDesktop->updateDetailsErrorText(
                UIMessageCenter::formatErrorInfo(item->accessError()));
            mVmRefreshAction->setEnabled(true);
        }
        else
        {
            /* default HTML support in Qt is terrible so just try to get
             * something really simple */
            m_pVMDesktop->updateDetailsText(
                tr("<h3>"
                   "Welcome to VirtualBox!</h3>"
                   "<p>The left part of this window is  "
                   "a list of all virtual machines on your computer. "
                   "The list is empty now because you haven't created any virtual "
                   "machines yet."
                   "<img src=:/welcome.png align=right/></p>"
                   "<p>In order to create a new virtual machine, press the "
                   "<b>New</b> button in the main tool bar located "
                   "at the top of the window.</p>"
                   "<p>You can press the <b>%1</b> key to get instant help, "
                   "or visit "
                   "<a href=http://www.virtualbox.org>www.virtualbox.org</a> "
                   "for the latest information and news.</p>").arg(QKeySequence(QKeySequence::HelpContents).toString(QKeySequence::NativeText)));
            mVmRefreshAction->setEnabled(false);
        }

        /* empty and disable other tabs */
        m_pVMDesktop->updateSnapshots(0, CMachine());
//        m_pVMDesktop->updateDescription(0, CMachine());

        /* disable modify actions */
        mVmConfigAction->setEnabled(false);
        mVmCloneAction->setEnabled(false);
        mVmDeleteAction->setEnabled(item != NULL);
        mVmDiscardAction->setEnabled(false);
        mVmPauseAction->setEnabled(false);

        /* change the Start button text accordingly */
        mVmStartAction->setText(tr("S&tart"));
        mVmStartAction->setStatusTip(
            tr("Start the selected virtual machine"));
        mVmStartAction->setEnabled(false);

        /* disable the show log item for the selected vm */
        mVmShowLogsAction->setEnabled(false);
        /* Disable the shell interaction features. */
        mVmOpenInFileManagerAction->setEnabled(false);
        mVmCreateShortcut->setEnabled(false);
    }
}

void VBoxSelectorWnd::mediumEnumStarted()
{
    /* refresh the current details to pick up hard disk sizes */
    vmListViewCurrentChanged(true /* aRefreshDetails */);
}

void VBoxSelectorWnd::mediumEnumFinished(const VBoxMediaList &list)
{
    /* refresh the current details to pick up hard disk sizes */
    vmListViewCurrentChanged(true /* aRefreshDetails */);

    /* we warn about inaccessible media only once (after media emumeration
     * started from main() at startup), to avoid annoying the user */
    if (   mDoneInaccessibleWarningOnce
#ifdef VBOX_GUI_WITH_SYSTRAY
        || vboxGlobal().isTrayMenu()
#endif
       )
        return;

    mDoneInaccessibleWarningOnce = true;

    do
    {
        /* ignore the signal if a modal widget is currently active (we won't be
         * able to properly show the modeless VDI manager window in this case) */
        if (QApplication::activeModalWidget())
            break;

        /* ignore the signal if a VBoxMediaManagerDlg window is active */
        if (qApp->activeWindow() &&
            !strcmp(qApp->activeWindow()->metaObject()->className(), "VBoxMediaManagerDlg"))
            break;

        /* look for at least one inaccessible media */
        VBoxMediaList::const_iterator it;
        for (it = list.begin(); it != list.end(); ++ it)
            if ((*it).state() == KMediumState_Inaccessible)
                break;

        if (it != list.end() && msgCenter().remindAboutInaccessibleMedia())
        {
            /* Show the VDM dialog but don't refresh once more after a
             * just-finished refresh */
            VBoxMediaManagerDlg::showModeless(this, false /* aRefresh */);
        }
    }
    while (0);
}

void VBoxSelectorWnd::machineStateChanged(QString strId, KMachineState /* state */)
{
#ifdef VBOX_GUI_WITH_SYSTRAY
    if (vboxGlobal().isTrayMenu())
    {
        /* Check if there are some machines alive - else quit, since
         * we're not needed as a systray menu anymore. */
        if (vboxGlobal().mainWindowCount() == 0)
        {
            fileExit();
            return;
        }
    }
#endif

    refreshVMItem(strId,
                  false /* aDetails */,
                  false /* aSnapshots */,
                  false /* aDescription */);

    /* simulate a state change signal */
//    m_pVMDesktop->updateDescriptionState();
}

void VBoxSelectorWnd::machineDataChanged(QString strId)
{
    refreshVMItem(strId,
                  true  /* aDetails */,
                  false /* aSnapshots */,
                  true  /* aDescription */);
}

void VBoxSelectorWnd::machineRegistered(QString strId, bool fRegistered)
{
    if (fRegistered)
    {
        CVirtualBox vbox = vboxGlobal().virtualBox();
        CMachine m = vbox.FindMachine(strId);
        if (!m.isNull())
        {
            mVMModel->addItem(m);
            /* Make sure the description, ... pages are properly updated.
             * Actually we haven't call the next method, but unfortunately Qt
             * seems buggy if the new item is on the same position as the
             * previous one. So go on the safe side and call this by our self. */
            vmListViewCurrentChanged();
        }
        /* m.isNull() is ok (theoretically, the machine could have been
         * already deregistered by some other client at this point) */
    }
    else
    {
        UIVMItem *item = mVMModel->itemById(strId);
        if (item)
        {
            int row = mVMModel->rowById(item->id());
            mVMModel->removeItem(item);
            mVMListView->ensureSomeRowSelected(row);
        }

        /* item = 0 is ok (if we originated this event then the item
         * has been already removed) */
    }
}

void VBoxSelectorWnd::sessionStateChanged(QString strId, KSessionState /* state */)
{
    refreshVMItem(strId,
                  true  /* aDetails */,
                  false /* aSnapshots */,
                  false /* aDescription */);

    /* simulate a state change signal */
//    m_pVMDesktop->updateDescriptionState();
}

void VBoxSelectorWnd::snapshotChanged(QString strId, QString /* strSnapshotId */)
{
    refreshVMItem(strId,
                  false /* aDetails */,
                  true  /* aSnapshot */,
                  false /* aDescription */);
}

#ifdef VBOX_GUI_WITH_SYSTRAY

void VBoxSelectorWnd::mainWindowCountChanged(int count)
{
    if (vboxGlobal().isTrayMenu() && count <= 1)
        fileExit();
}

void VBoxSelectorWnd::trayIconCanShow(bool fEnabled)
{
    emit trayIconChanged(VBoxChangeTrayIconEvent(vboxGlobal().settings().trayIconEnabled()));
}

void VBoxSelectorWnd::trayIconShow(bool fEnabled)
{
    if (vboxGlobal().isTrayMenu() && mTrayIcon)
        mTrayIcon->trayIconShow(fEnabled);
}

void VBoxSelectorWnd::trayIconChanged(bool fEnabled)
{
    /* Not used yet. */
}

#endif /* VBOX_GUI_WITH_SYSTRAY */

void VBoxSelectorWnd::sltEmbedDownloaderForUserManual()
{
    /* If there is User Manual downloader created => show the process bar: */
    if (UIDownloaderUserManual *pDl = UIDownloaderUserManual::current())
        statusBar()->addWidget(pDl->progressWidget(this), 0);
}

void VBoxSelectorWnd::sltEmbedDownloaderForExtensionPack()
{
    /* If there is Extension Pack downloader created => show the process bar: */
    if (UIDownloaderExtensionPack *pDl = UIDownloaderExtensionPack::current())
        statusBar()->addWidget(pDl->progressWidget(this), 0);
}

void VBoxSelectorWnd::showViewContextMenu(const QPoint &pos)
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    QString strToolbar = vbox.GetExtraData(VBoxDefs::GUI_Toolbar);
    QString strStatusbar = vbox.GetExtraData(VBoxDefs::GUI_Statusbar);
    bool fToolbar = strToolbar.isEmpty() || strToolbar == "true";
    bool fStatusbar = strStatusbar.isEmpty() || strStatusbar == "true";

    QList<QAction*> actions;
    QAction *pShowToolBar = new QAction(tr("Show Toolbar"), 0);
    pShowToolBar->setCheckable(true);
    pShowToolBar->setChecked(fToolbar);
    actions << pShowToolBar;
    QAction *pShowStatusBar = new QAction(tr("Show Statusbar"), 0);
    pShowStatusBar->setCheckable(true);
    pShowStatusBar->setChecked(fStatusbar);
    actions << pShowStatusBar;

    QPoint gpos = pos;
    QWidget *pSender = static_cast<QWidget*>(sender());
    if (pSender)
        gpos = pSender->mapToGlobal(pos);
    QAction *pResult = QMenu::exec(actions, gpos);
    if (pResult == pShowToolBar)
    {
        if (pResult->isChecked())
        {
#ifdef Q_WS_MAC
            mVMToolBar->show();
#else /* Q_WS_MAC */
            m_pBar->show();
#endif /* !Q_WS_MAC */
            vbox.SetExtraData(VBoxDefs::GUI_Toolbar, "true");
        }
        else
        {
#ifdef Q_WS_MAC
            mVMToolBar->hide();
#else /* Q_WS_MAC */
            m_pBar->hide();
#endif /* !Q_WS_MAC */
            vbox.SetExtraData(VBoxDefs::GUI_Toolbar, "false");
        }
    }else if (pResult == pShowStatusBar)
    {
        if (pResult->isChecked())
        {
            statusBar()->show();
            vbox.SetExtraData(VBoxDefs::GUI_Statusbar, "true");
        }
        else
        {
            statusBar()->hide();
            vbox.SetExtraData(VBoxDefs::GUI_Statusbar, "false");
        }
    }
}

void VBoxSelectorWnd::prepareMenuHelp(QMenu *pMenu)
{
    /* Do not touch if filled already: */
    if (!pMenu->isEmpty())
        return;

    /* Help submenu: */
    pMenu->addAction(gActionPool->action(UIActionIndex_Simple_Help));
    pMenu->addAction(gActionPool->action(UIActionIndex_Simple_Web));

    pMenu->addSeparator();

    pMenu->addAction(gActionPool->action(UIActionIndex_Simple_ResetWarnings));

    pMenu->addSeparator();

#ifdef VBOX_WITH_REGISTRATION
    pMenu->addAction(gActionPool->action(UIActionIndex_Simple_Register));
#endif /* VBOX_WITH_REGISTRATION */

    pMenu->addAction(gActionPool->action(UIActionIndex_Simple_Update));

#ifndef Q_WS_MAC
    pMenu->addSeparator();
#endif /* !Q_WS_MAC */

    pMenu->addAction(gActionPool->action(UIActionIndex_Simple_About));

    /* Configure connections: */
    VBoxGlobal::connect(gActionPool->action(UIActionIndex_Simple_Help), SIGNAL(triggered()),
                        &msgCenter(), SLOT(sltShowHelpHelpDialog()));
    VBoxGlobal::connect(gActionPool->action(UIActionIndex_Simple_Web), SIGNAL(triggered()),
                        &msgCenter(), SLOT(sltShowHelpWebDialog()));
    VBoxGlobal::connect(gActionPool->action(UIActionIndex_Simple_ResetWarnings), SIGNAL(triggered()),
                        &msgCenter(), SLOT(sltResetSuppressedMessages()));
#ifdef VBOX_WITH_REGISTRATION
    VBoxGlobal::connect(gActionPool->action(UIActionIndex_Simple_Register), SIGNAL(triggered()),
                        &vboxGlobal(), SLOT(showRegistrationDialog()));
    VBoxGlobal::connect(gEDataEvents, SIGNAL(sigCanShowRegistrationDlg(bool)),
                        gActionPool->action(UIActionIndex_Simple_Register), SLOT(setEnabled(bool)));
#endif /* VBOX_WITH_REGISTRATION */
    VBoxGlobal::connect(gActionPool->action(UIActionIndex_Simple_Update), SIGNAL(triggered()),
                        gUpdateManager, SLOT(sltForceCheck()));
    VBoxGlobal::connect(gActionPool->action(UIActionIndex_Simple_About), SIGNAL(triggered()),
                        &msgCenter(), SLOT(sltShowHelpAboutDialog()));
}

