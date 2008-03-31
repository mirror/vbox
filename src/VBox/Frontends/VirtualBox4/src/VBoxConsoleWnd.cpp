/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxConsoleWnd class implementation
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxConsoleWnd.h"
#include "VBoxConsoleView.h"
#include "VBoxCloseVMDlg.h"
#include "VBoxTakeSnapshotDlg.h"
#include "VBoxDiskImageManagerDlg.h"
#include "VBoxVMFirstRunWzd.h"
#include "VBoxSharedFoldersSettings.h"
#include "VBoxVMInformationDlg.h"
#include "VBoxDownloaderWgt.h"
#include "VBoxGlobal.h"

#include "QIStateIndicator.h"
#include "QIStatusBar.h"
#include "QIHotKeyEdit.h"

/* Qt includes */
#include <QActionGroup>
#include <QDesktopWidget>
#include <QMenuBar>
#include <QFileInfo>
#include <QDir>
#include <QTimer>
#ifdef Q_WS_X11
# include <QX11Info>
#endif

#include <VBox/VBoxGuest.h>

#if defined(Q_WS_X11)
#include <X11/Xlib.h>
#include <XKeyboard.h>
#endif

#ifdef Q_WS_MAC
#include "VBoxUtils.h"
#include "VBoxIChatTheaterWrapper.h"
#endif

#ifdef VBOX_WITH_DEBUGGER_GUI
#include <VBox/err.h>
#endif

#include <iprt/param.h>
#include <iprt/path.h>

/** class StatusTipEvent
 *
 *  The StatusTipEvent class is an auxiliary QEvent class
 *  for carrying statusTip text of non-QAction menu item's.
 *  This event is posted then the menu item is highlighted but
 *  processed later in VBoxConsoleWnd::event() handler to
 *  avoid statusBar messaging collisions.
 */
class StatusTipEvent : public QEvent
{
public:
    enum { Type = QEvent::User + 10 };
    StatusTipEvent (const QString &aTip)
        : QEvent ((QEvent::Type) Type), mTip (aTip) {}

    QString mTip;
};

#ifdef RT_OS_DARWIN
class Q3Http;
class Q3HttpResponseHeader;
#endif

/** \class VBoxConsoleWnd
 *
 *  The VBoxConsoleWnd class is a VM console window, one of two main VBox
 *  GUI windows.
 *
 *  This window appears when the user starts the virtual machine. It
 *  contains the VBoxConsoleView widget that acts as a console of the
 *  running virtual machine.
 */

/**
 *  Constructs the VM console window.
 *
 *  @param aSelf pointer to a variable where to store |this| right after
 *               this object's constructor is called (necessary to avoid
 *               recursion in VBoxGlobal::consoleWnd())
 */
VBoxConsoleWnd::
VBoxConsoleWnd (VBoxConsoleWnd **aSelf, QWidget* aParent,
                Qt::WFlags aFlags)
    : QMainWindow (aParent, aFlags)
    , mMainMenu (0)
#ifdef VBOX_WITH_DEBUGGER_GUI
    , dbgStatisticsAction (NULL)
    , dbgCommandLineAction (NULL)
    , dbgMenu (NULL)
#endif
    , console (0)
    , machine_state (KMachineState_Null)
    , no_auto_close (false)
    , mIsFullscreen (false)
    , mIsSeamless (false)
    , mIsSeamlessSupported (false)
#warning port me
//    , normal_wflags (getWFlags())
    , was_max (false)
    , console_style (0)
    , mIsOpenViewFinished (false)
    , mIsFirstTimeStarted (false)
    , mIsAutoSaveMedia (true)
#ifdef VBOX_WITH_DEBUGGER_GUI
    , dbg_gui (NULL)
#endif
#ifdef Q_WS_MAC
    , dockImgStatePaused (NULL)
    , dockImgStateSaving (NULL)
    , dockImgStateRestoring (NULL)
    , dockImgBack100x75 (NULL)
    , dockImgOS (NULL)
#endif
{
    if (aSelf)
        *aSelf = this;

    idle_timer = new QTimer (this);

    /* default application icon (will change to the VM-specific icon in
     * openView()) */
    setWindowIcon (QIcon (":/ico40x01.png"));

    /* ensure status bar is created */
    new QIStatusBar (this);

    ///// Actions ///////////////////////////////////////////////////////////

    /* a group for all actions that are enabled only when the VM is running.
     * Note that only actions whose enabled state depends exclusively on the
     * execution state of the VM are added to this group. */
    mRunningActions = new QActionGroup (this);
    mRunningActions->setExclusive (false);

    /* a group for all actions that are enabled when the VM is running or
     * paused. Note that only actions whose enabled state depends exclusively
     * on the execution state of the VM are added to this group. */
    mRunningOrPausedActions = new QActionGroup (this);
    mRunningOrPausedActions->setExclusive (false);

    /* VM menu actions */

    vmFullscreenAction = new QAction (this);
    vmFullscreenAction->setIcon (
        VBoxGlobal::iconSet (":/fullscreen_16px.png", ":/fullscreen_disabled_16px.png"));
    vmFullscreenAction->setCheckable (true);

    vmSeamlessAction = new QAction (this);
    vmSeamlessAction->setIcon (
        VBoxGlobal::iconSet (":/nw_16px.png", ":/nw_disabled_16px.png"));
    vmSeamlessAction->setCheckable (true);

    vmAutoresizeGuestAction = new QAction (mRunningActions);
    vmAutoresizeGuestAction->setIcon (
        VBoxGlobal::iconSet (":/auto_resize_on_16px.png", ":/auto_resize_on_disabled_16px.png"));
    vmAutoresizeGuestAction->setCheckable (true);
    vmAutoresizeGuestAction->setEnabled (false);

    vmAdjustWindowAction = new QAction (this);
    vmAdjustWindowAction->setIcon (
        VBoxGlobal::iconSet (":/adjust_win_size_16px.png",
                             ":/adjust_win_size_disabled_16px.png"));

    vmTypeCADAction = new QAction (mRunningActions);
    vmTypeCADAction->setIcon (VBoxGlobal::iconSet (":/hostkey_16px.png",
                                                      ":/hostkey_disabled_16px.png"));

#if defined(Q_WS_X11)
    vmTypeCABSAction = new QAction (mRunningActions);
    vmTypeCABSAction->setIcon (VBoxGlobal::iconSet (":/hostkey_16px.png",
                                                       ":/hostkey_disabled_16px.png"));
#endif

    vmResetAction = new QAction (mRunningActions);
    vmResetAction->setIcon (VBoxGlobal::iconSet (":/reset_16px.png",
                                                    ":/reset_disabled_16px.png"));

    vmPauseAction = new QAction (this);
    vmPauseAction->setIcon (VBoxGlobal::iconSet (":/pause_16px.png"));
    vmPauseAction->setCheckable (true);

    vmACPIShutdownAction = new QAction (mRunningActions);
    vmACPIShutdownAction->setIcon (
        VBoxGlobal::iconSet (":/acpi_16px.png", ":/acpi_disabled_16px.png"));

    vmCloseAction = new QAction (this);
    vmCloseAction->setIcon (VBoxGlobal::iconSet (":/exit_16px.png"));

    vmTakeSnapshotAction = new QAction (mRunningOrPausedActions);
    vmTakeSnapshotAction->setIcon (VBoxGlobal::iconSet (
        ":/take_snapshot_16px.png", ":/take_snapshot_dis_16px.png"));

    vmShowInformationDlgAction = new QAction (this);
    vmShowInformationDlgAction->setIcon (VBoxGlobal::iconSet (
        ":/description_16px.png", ":/description_disabled_16px.png"));

    vmDisableMouseIntegrAction = new QAction (this);
    vmDisableMouseIntegrAction->setIcon (VBoxGlobal::iconSet (
        ":/mouse_can_seamless_16px.png", ":/mouse_can_seamless_disabled_16px.png"));
    vmDisableMouseIntegrAction->setCheckable (true);

    /* Devices menu actions */

    devicesMountFloppyImageAction = new QAction (mRunningOrPausedActions);

    devicesUnmountFloppyAction = new QAction (this);
    devicesUnmountFloppyAction->setIcon (VBoxGlobal::iconSet (":/fd_unmount_16px.png",
                                                                 ":/fd_unmount_dis_16px.png"));

    devicesMountDVDImageAction = new QAction (mRunningOrPausedActions);

    devicesUnmountDVDAction = new QAction (this);
    devicesUnmountDVDAction->setIcon (VBoxGlobal::iconSet (":/cd_unmount_16px.png",
                                                              ":/cd_unmount_dis_16px.png"));

    devicesSFDialogAction = new QAction (mRunningOrPausedActions);
    devicesSFDialogAction->setIcon (VBoxGlobal::iconSet (":/shared_folder_16px.png",
                                                            ":/shared_folder_disabled_16px.png"));

    devicesSwitchVrdpAction = new QAction (mRunningOrPausedActions);
    devicesSwitchVrdpAction->setIcon (VBoxGlobal::iconSet (":/vrdp_16px.png",
                                                              ":/vrdp_disabled_16px.png"));
    devicesSwitchVrdpAction->setCheckable (true);

    devicesInstallGuestToolsAction = new QAction (mRunningActions);
    devicesInstallGuestToolsAction->setIcon (VBoxGlobal::iconSet (":/guesttools_16px.png",
                                                                     ":/guesttools_disabled_16px.png"));

#ifdef VBOX_WITH_DEBUGGER_GUI
    if (vboxGlobal().isDebuggerEnabled())
    {
        dbgStatisticsAction = new QAction (this);
        dbgCommandLineAction = new QAction (this);
    }
    else
    {
        dbgStatisticsAction = NULL;
        dbgCommandLineAction = NULL;
    }
#endif

    /* Help menu actions */

    helpContentsAction = new QAction (this);
    helpContentsAction->setIcon (VBoxGlobal::iconSet (":/help_16px.png"));
    helpWebAction = new QAction (this);
    helpWebAction->setIcon (VBoxGlobal::iconSet (":/site_16px.png"));
    helpRegisterAction = new QAction (this);
    helpRegisterAction->setIcon (VBoxGlobal::iconSet (":/register_16px.png",
                                                         ":/register_disabled_16px.png"));
    helpAboutAction = new QAction (this);
    helpAboutAction->setIcon (VBoxGlobal::iconSet (":/about_16px.png"));
    helpResetMessagesAction = new QAction (this);
    helpResetMessagesAction->setIcon (VBoxGlobal::iconSet (":/reset_16px.png"));

    ///// Menubar ///////////////////////////////////////////////////////////

    mMainMenu = new QMenu (this);

    /* Machine submenu */

    QMenu *vmMenu = new QMenu (this);

    /* dynamic & status line popup menus */
    vmAutoresizeMenu = new VBoxSwitchMenu (vmMenu, vmAutoresizeGuestAction);
    vmDisMouseIntegrMenu = new VBoxSwitchMenu (vmMenu, vmDisableMouseIntegrAction,
                                               true /* inverted toggle state */);

    vmMenu->addAction (vmFullscreenAction);
    vmMenu->addAction (vmSeamlessAction);
    vmMenu->addAction (vmAdjustWindowAction);
    vmMenu->addAction (vmAutoresizeGuestAction);
    vmMenu->addSeparator();
    vmMenu->addAction (vmDisableMouseIntegrAction);
    vmMenu->addSeparator();
    vmMenu->addAction (vmTypeCADAction);
#if defined(Q_WS_X11)
    vmMenu->addAction (vmTypeCABSAction);
#endif
    vmMenu->addSeparator();
    vmMenu->addAction (vmTakeSnapshotAction);
    vmMenu->addSeparator();
    vmMenu->addAction (vmShowInformationDlgAction);
    vmMenu->addSeparator();
    vmMenu->addAction (vmResetAction);
    vmMenu->addAction (vmPauseAction);
    vmMenu->addAction (vmACPIShutdownAction);
    vmMenu->addSeparator();
    vmMenu->addAction (vmCloseAction);

    menuBar()->insertItem (QString::null, vmMenu, vmMenuId);
    mMainMenu->insertItem (QString::null, vmMenu, vmMenuId);

    /* Devices submenu */

    devicesMenu = new QMenu (this);

    /* dynamic & statusline popup menus */
    devicesMountFloppyMenu = new QMenu (devicesMenu);
    devicesMountDVDMenu = new QMenu (devicesMenu);
    devicesSFMenu = new QMenu (devicesMenu);
    devicesNetworkMenu = new QMenu (devicesMenu);
    devicesUSBMenu = new VBoxUSBMenu (devicesMenu);
    devicesVRDPMenu = new VBoxSwitchMenu (devicesMenu, devicesSwitchVrdpAction);

    devicesMenu->insertItem (VBoxGlobal::iconSet (":/cd_16px.png", ":/cd_disabled_16px.png"),
        QString::null, devicesMountDVDMenu, devicesMountDVDMenuId);
    devicesMenu->addAction (devicesUnmountDVDAction);
    devicesMenu->addSeparator();

    devicesMenu->insertItem (VBoxGlobal::iconSet (":/fd_16px.png", ":/fd_disabled_16px.png"),
        QString::null, devicesMountFloppyMenu, devicesMountFloppyMenuId);
    devicesMenu->addAction (devicesUnmountFloppyAction);
    devicesMenu->addSeparator();

    devicesMenu->insertItem (VBoxGlobal::iconSet (":/nw_16px.png", ":/nw_disabled_16px.png"),
        QString::null, devicesNetworkMenu, devicesNetworkMenuId);
    devicesMenu->addSeparator();

    devicesMenu->insertItem (VBoxGlobal::iconSet (":/usb_16px.png", ":/usb_disabled_16px.png"),
        QString::null, devicesUSBMenu, devicesUSBMenuId);
    devicesUSBMenuSeparatorId = devicesMenu->insertSeparator();

    devicesMenu->addAction (devicesSFDialogAction);
    devicesSFMenuSeparatorId = devicesMenu->insertSeparator();
    devicesMenu->addAction (devicesSFDialogAction);

    devicesMenu->addAction (devicesSwitchVrdpAction);
    devicesVRDPMenuSeparatorId = devicesMenu->insertSeparator();

    devicesMenu->addAction (devicesInstallGuestToolsAction);

    menuBar()->insertItem (QString::null, devicesMenu, devicesMenuId);
    mMainMenu->insertItem (QString::null, devicesMenu, devicesMenuId);

    /* reset the "context menu" flag */
    devicesMenu->setItemParameter (devicesMountFloppyMenuId, 0);
    devicesMenu->setItemParameter (devicesMountDVDMenuId, 0);
    devicesMenu->setItemParameter (devicesUSBMenuId, 0);

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* Debug popup menu */
    if (vboxGlobal().isDebuggerEnabled())
    {
        dbgMenu = new QMenu (this);
        dbgMenu->addAction (dbgStatisticsAction);
        dbgMenu->addAction (dbgCommandLineAction);
        menuBar()->insertItem (QString::null, dbgMenu, dbgMenuId);
        mMainMenu->insertItem (QString::null, dbgMenu, dbgMenuId);
    }
    else
        dbgMenu = NULL;
#endif

    /* Help submenu */

    QMenu *helpMenu = new QMenu (this);

    helpMenu->addAction (helpContentsAction);
    helpMenu->addAction (helpWebAction);
    helpMenu->addSeparator();
#ifdef VBOX_WITH_REGISTRATION
    helpMenu->addAction (helpRegisterAction);
    helpRegisterAction->setEnabled (vboxGlobal().virtualBox().
        GetExtraData (VBoxDefs::GUI_RegistrationDlgWinID).isEmpty());
#endif
    helpMenu->addAction (helpAboutAction);
    helpMenu->addSeparator();
    helpMenu->addAction (helpResetMessagesAction);

    menuBar()->insertItem( QString::null, helpMenu, helpMenuId );
    mMainMenu->insertItem (QString::null, helpMenu, helpMenuId);

    ///// Status bar ////////////////////////////////////////////////////////

    QWidget *indicatorBox = new QWidget ();
    QHBoxLayout *indicatorBoxHLayout = new QHBoxLayout (indicatorBox);
    VBoxGlobal::setLayoutMargin (indicatorBoxHLayout, 0);
    indicatorBoxHLayout->setSpacing (5);
    /* i/o devices */
    hd_light = new QIStateIndicator (KDeviceActivity_Idle);
    hd_light->setStateIcon (KDeviceActivity_Idle, QPixmap (":/hd_16px.png"));
    hd_light->setStateIcon (KDeviceActivity_Reading, QPixmap (":/hd_read_16px.png"));
    hd_light->setStateIcon (KDeviceActivity_Writing, QPixmap (":/hd_write_16px.png"));
    hd_light->setStateIcon (KDeviceActivity_Null, QPixmap (":/hd_disabled_16px.png"));
    indicatorBoxHLayout->addWidget (hd_light);
    cd_light = new QIStateIndicator (KDeviceActivity_Idle);
    cd_light->setStateIcon (KDeviceActivity_Idle, QPixmap (":/cd_16px.png"));
    cd_light->setStateIcon (KDeviceActivity_Reading, QPixmap (":/cd_read_16px.png"));
    cd_light->setStateIcon (KDeviceActivity_Writing, QPixmap (":/cd_write_16px.png"));
    cd_light->setStateIcon (KDeviceActivity_Null, QPixmap (":/cd_disabled_16px.png"));
    indicatorBoxHLayout->addWidget (cd_light);
    fd_light = new QIStateIndicator (KDeviceActivity_Idle);
    fd_light->setStateIcon (KDeviceActivity_Idle, QPixmap (":/fd_16px.png"));
    fd_light->setStateIcon (KDeviceActivity_Reading, QPixmap (":/fd_read_16px.png"));
    fd_light->setStateIcon (KDeviceActivity_Writing, QPixmap (":/fd_write_16px.png"));
    fd_light->setStateIcon (KDeviceActivity_Null, QPixmap (":/fd_disabled_16px.png"));
    indicatorBoxHLayout->addWidget (fd_light);
    net_light = new QIStateIndicator (KDeviceActivity_Idle);
    net_light->setStateIcon (KDeviceActivity_Idle, QPixmap (":/nw_16px.png"));
    net_light->setStateIcon (KDeviceActivity_Reading, QPixmap (":/nw_read_16px.png"));
    net_light->setStateIcon (KDeviceActivity_Writing, QPixmap (":/nw_write_16px.png"));
    net_light->setStateIcon (KDeviceActivity_Null, QPixmap (":/nw_disabled_16px.png"));
    indicatorBoxHLayout->addWidget (net_light);
    usb_light = new QIStateIndicator (KDeviceActivity_Idle);
    usb_light->setStateIcon (KDeviceActivity_Idle, QPixmap (":/usb_16px.png"));
    usb_light->setStateIcon (KDeviceActivity_Reading, QPixmap (":/usb_read_16px.png"));
    usb_light->setStateIcon (KDeviceActivity_Writing, QPixmap (":/usb_write_16px.png"));
    usb_light->setStateIcon (KDeviceActivity_Null, QPixmap (":/usb_disabled_16px.png"));
    indicatorBoxHLayout->addWidget (usb_light);
    sf_light = new QIStateIndicator (KDeviceActivity_Idle);
    sf_light->setStateIcon (KDeviceActivity_Idle, QPixmap (":/shared_folder_16px.png"));
    sf_light->setStateIcon (KDeviceActivity_Reading, QPixmap (":/shared_folder_read_16px.png"));
    sf_light->setStateIcon (KDeviceActivity_Writing, QPixmap (":/shared_folder_write_16px.png"));
    sf_light->setStateIcon (KDeviceActivity_Null, QPixmap (":/shared_folder_disabled_16px.png"));
    indicatorBoxHLayout->addWidget (sf_light);

    QFrame *separator = new QFrame();
    separator->setFrameStyle (QFrame::VLine | QFrame::Sunken);
    indicatorBoxHLayout->addWidget (separator);

#if 0 // do not show these indicators, information overload
    /* vrdp state */
    vrdp_state = new QIStateIndicator (0, indicatorBox, "vrdp_state", Qt::WNoAutoErase);
    vrdp_state->setStateIcon (0, QPixmap (":/vrdp_disabled_16px.png"));
    vrdp_state->setStateIcon (1, QPixmap (":/vrdp_16px.png"));
    /* auto resize state */
    autoresize_state = new QIStateIndicator (1, indicatorBox, "autoresize_state", Qt::WNoAutoErase);
    autoresize_state->setStateIcon (0, QPixmap (":/auto_resize_off_disabled_16px.png"));
    autoresize_state->setStateIcon (1, QPixmap (":/auto_resize_off_16px.png"));
    autoresize_state->setStateIcon (2, QPixmap (":/auto_resize_on_disabled_16px.png"));
    autoresize_state->setStateIcon (3, QPixmap (":/auto_resize_on_16px.png"));
#endif

    /* mouse */
    mouse_state = new QIStateIndicator (0);
    mouse_state->setStateIcon (0, QPixmap (":/mouse_disabled_16px.png"));
    mouse_state->setStateIcon (1, QPixmap (":/mouse_16px.png"));
    mouse_state->setStateIcon (2, QPixmap (":/mouse_seamless_16px.png"));
    mouse_state->setStateIcon (3, QPixmap (":/mouse_can_seamless_16px.png"));
    mouse_state->setStateIcon (4, QPixmap (":/mouse_can_seamless_uncaptured_16px.png"));
    indicatorBoxHLayout->addWidget (mouse_state);
    /* host key */
    hostkey_hbox = new QWidget();
    QHBoxLayout *hostkeyHBoxLayout = new QHBoxLayout (hostkey_hbox);
    VBoxGlobal::setLayoutMargin (hostkeyHBoxLayout, 0);
    hostkeyHBoxLayout->setSpacing (3);
    indicatorBoxHLayout->addWidget (hostkey_hbox);

    hostkey_state = new QIStateIndicator (0);
    hostkey_state->setStateIcon (0, QPixmap (":/hostkey_16px.png"));
    hostkey_state->setStateIcon (1, QPixmap (":/hostkey_captured_16px.png"));
    hostkey_state->setStateIcon (2, QPixmap (":/hostkey_pressed_16px.png"));
    hostkey_state->setStateIcon (3, QPixmap (":/hostkey_captured_pressed_16px.png"));
    hostkeyHBoxLayout->addWidget (hostkey_state);
    hostkey_name = new QLabel (QIHotKeyEdit::keyName (vboxGlobal().settings().hostKey()));
    hostkeyHBoxLayout->addWidget (hostkey_name);
    /* add to statusbar */
    statusBar()->addPermanentWidget (indicatorBox, 0);

    /////////////////////////////////////////////////////////////////////////

    languageChange();

    setWindowTitle (caption_prefix);

    ///// Connections ///////////////////////////////////////////////////////

    connect (vmFullscreenAction, SIGNAL (toggled (bool)),
             this, SLOT (vmFullscreen (bool)));
    connect (vmSeamlessAction, SIGNAL (toggled (bool)),
             this, SLOT (vmSeamless (bool)));
    connect (vmAutoresizeGuestAction, SIGNAL (toggled (bool)),
             this, SLOT (vmAutoresizeGuest (bool)));
    connect (vmAdjustWindowAction, SIGNAL (activated()),
             this, SLOT (vmAdjustWindow()));

    connect (vmTypeCADAction, SIGNAL(activated()), this, SLOT(vmTypeCAD()));
#if defined(Q_WS_X11)
    connect (vmTypeCABSAction, SIGNAL(activated()), this, SLOT(vmTypeCABS()));
#endif
    connect (vmResetAction, SIGNAL(activated()), this, SLOT (vmReset()));
    connect (vmPauseAction, SIGNAL(toggled (bool)), this, SLOT (vmPause (bool)));
    connect (vmACPIShutdownAction, SIGNAL (activated()), this, SLOT (vmACPIShutdown()));
    connect (vmCloseAction, SIGNAL(activated()), this, SLOT (vmClose()));

    connect (vmTakeSnapshotAction, SIGNAL(activated()), this, SLOT(vmTakeSnapshot()));
    connect (vmShowInformationDlgAction, SIGNAL(activated()), this, SLOT (vmShowInfoDialog()));

    connect (vmDisableMouseIntegrAction, SIGNAL(toggled (bool)), this, SLOT(vmDisableMouseIntegr (bool)));

    connect (devicesMountFloppyImageAction, SIGNAL(activated()), this, SLOT(devicesMountFloppyImage()));
    connect (devicesUnmountFloppyAction, SIGNAL(activated()), this, SLOT(devicesUnmountFloppy()));
    connect (devicesMountDVDImageAction, SIGNAL(activated()), this, SLOT(devicesMountDVDImage()));
    connect (devicesUnmountDVDAction, SIGNAL(activated()), this, SLOT(devicesUnmountDVD()));
    connect (devicesSwitchVrdpAction, SIGNAL(toggled (bool)), this, SLOT(devicesSwitchVrdp (bool)));
    connect (devicesSFDialogAction, SIGNAL(activated()), this, SLOT(devicesOpenSFDialog()));
    connect (devicesInstallGuestToolsAction, SIGNAL(activated()), this, SLOT(devicesInstallGuestAdditions()));


    connect (devicesMountFloppyMenu, SIGNAL(aboutToShow()), this, SLOT(prepareFloppyMenu()));
    connect (devicesMountDVDMenu, SIGNAL(aboutToShow()), this, SLOT(prepareDVDMenu()));
    connect (devicesNetworkMenu, SIGNAL(aboutToShow()), this, SLOT(prepareNetworkMenu()));

    connect (statusBar(), SIGNAL(messageChanged (const QString &)), this, SLOT(statusTipChanged (const QString &)));

    connect (devicesMountFloppyMenu, SIGNAL(activated(int)), this, SLOT(captureFloppy(int)));
    connect (devicesMountDVDMenu, SIGNAL(activated(int)), this, SLOT(captureDVD(int)));
    connect (devicesUSBMenu, SIGNAL(activated(int)), this, SLOT(switchUSB(int)));
    connect (devicesNetworkMenu, SIGNAL(activated(int)), this, SLOT(activateNetworkMenu(int)));

    connect (devicesMountFloppyMenu, SIGNAL (highlighted (int)),
             this, SLOT (setDynamicMenuItemStatusTip (int)));
    connect (devicesMountDVDMenu, SIGNAL (highlighted (int)),
             this, SLOT (setDynamicMenuItemStatusTip (int)));
    connect (devicesNetworkMenu, SIGNAL (highlighted (int)),
             this, SLOT (setDynamicMenuItemStatusTip (int)));

    /* Cleanup the status bar tip when a menu with dynamic items is
     * hidden. This is necessary for context menus in the first place but also
     * for normal menus (because Qt will not do it on pressing ESC if the menu
     * is constructed of dynamic items only) */
    connect (devicesMountFloppyMenu, SIGNAL (aboutToHide()),
             this, SLOT (clearStatusBar()));
    connect (devicesMountDVDMenu, SIGNAL (aboutToHide()),
             this, SLOT (clearStatusBar()));
    connect (devicesNetworkMenu, SIGNAL (aboutToHide()),
             this, SLOT (clearStatusBar()));

    connect (helpContentsAction, SIGNAL (activated()),
             &vboxProblem(), SLOT (showHelpHelpDialog()));
    connect (helpWebAction, SIGNAL (activated()),
             &vboxProblem(), SLOT (showHelpWebDialog()));
    connect (helpRegisterAction, SIGNAL (activated()),
             &vboxGlobal(), SLOT (showRegistrationDialog()));
    connect (&vboxGlobal(), SIGNAL (canShowRegDlg (bool)),
             helpRegisterAction, SLOT (setEnabled (bool)));
    connect (helpAboutAction, SIGNAL (activated()),
             &vboxProblem(), SLOT (showHelpAboutDialog()));
    connect (helpResetMessagesAction, SIGNAL (activated()),
             &vboxProblem(), SLOT (resetSuppressedMessages()));

    connect (fd_light, SIGNAL (contextMenuRequested (QIStateIndicator *, QContextMenuEvent *)),
             this, SLOT (showIndicatorContextMenu (QIStateIndicator *, QContextMenuEvent *)));
    connect (cd_light, SIGNAL (contextMenuRequested (QIStateIndicator *, QContextMenuEvent *)),
             this, SLOT (showIndicatorContextMenu (QIStateIndicator *, QContextMenuEvent *)));
    connect (usb_light, SIGNAL (contextMenuRequested (QIStateIndicator *, QContextMenuEvent *)),
             this, SLOT (showIndicatorContextMenu (QIStateIndicator *, QContextMenuEvent *)));
    connect (sf_light, SIGNAL (contextMenuRequested (QIStateIndicator *, QContextMenuEvent *)),
             this, SLOT (showIndicatorContextMenu (QIStateIndicator *, QContextMenuEvent *)));
    connect (net_light, SIGNAL (contextMenuRequested (QIStateIndicator *, QContextMenuEvent *)),
             this, SLOT (showIndicatorContextMenu (QIStateIndicator *, QContextMenuEvent *)));

#if 0
    connect (vrdp_state, SIGNAL (contextMenuRequested (QIStateIndicator *, QContextMenuEvent *)),
             this, SLOT (showIndicatorContextMenu (QIStateIndicator *, QContextMenuEvent *)));
    connect (autoresize_state, SIGNAL (contextMenuRequested (QIStateIndicator *, QContextMenuEvent *)),
             this, SLOT (showIndicatorContextMenu (QIStateIndicator *, QContextMenuEvent *)));
#endif
    connect (mouse_state, SIGNAL (contextMenuRequested (QIStateIndicator *, QContextMenuEvent *)),
             this, SLOT (showIndicatorContextMenu (QIStateIndicator *, QContextMenuEvent *)));

    /* watch global settings changes */
    connect (&vboxGlobal().settings(), SIGNAL (propertyChanged (const char *, const char *)),
             this, SLOT (processGlobalSettingChange (const char *, const char *)));

#ifdef VBOX_WITH_DEBUGGER_GUI
    if (dbgStatisticsAction)
        connect (dbgStatisticsAction, SIGNAL (activated()),
                 this, SLOT (dbgShowStatistics()));
    if (dbgCommandLineAction)
        connect (dbgCommandLineAction, SIGNAL (activated()),
                 this, SLOT (dbgShowCommandLine()));
#endif

#ifdef Q_WS_MAC
# ifdef VBOX_WITH_ICHAT_THEATER
    initSharedAVManager();
# endif
    /* prepare the dock images */
    dockImgStatePaused    = ::DarwinCreateDockBadge (":/state_paused_16px.png");
    dockImgStateSaving    = ::DarwinCreateDockBadge (":/state_saving_16px.png");
    dockImgStateRestoring = ::DarwinCreateDockBadge (":/state_restoring_16px.png");
    dockImgBack100x75     = ::DarwinCreateDockBadge (":/dock_1.png");
    SetApplicationDockTileImage (dockImgOS);
#endif
    mMaskShift.scale (0, 0, Qt::IgnoreAspectRatio);
}

VBoxConsoleWnd::~VBoxConsoleWnd()
{
#ifdef Q_WS_MAC
    /* release the dock images */
    if (dockImgStatePaused)
        CGImageRelease (dockImgStatePaused);
    if (dockImgStateSaving)
        CGImageRelease (dockImgStateSaving);
    if (dockImgStateRestoring)
        CGImageRelease (dockImgStateRestoring);
    if (dockImgBack100x75)
        CGImageRelease (dockImgBack100x75);
    if (dockImgOS)
        CGImageRelease (dockImgOS);
#endif
}

//
// Public members
/////////////////////////////////////////////////////////////////////////////

/**
 *  Opens a new console view to interact with a given VM.
 *  Does nothing if the console view is already opened.
 *  Used by VBoxGlobal::startMachine(), should not be called directly.
 */
bool VBoxConsoleWnd::openView (const CSession &session)
{
    LogFlowFuncEnter();

    if (console)
    {
        LogFlowFunc (("Already opened\n"));
        LogFlowFuncLeave();
        return false;
    }

    csession = session;

    if (!centralWidget())
    {
        setCentralWidget (new QWidget (this));
        QGridLayout *pMainLayout = new QGridLayout(centralWidget());
        VBoxGlobal::setLayoutMargin (pMainLayout, 0);
        pMainLayout->setSpacing (0);

        mShiftingSpacerLeft = new QSpacerItem (0, 0,
                                               QSizePolicy::Fixed,
                                               QSizePolicy::Fixed);
        mShiftingSpacerTop = new QSpacerItem (0, 0,
                                              QSizePolicy::Fixed,
                                              QSizePolicy::Fixed);
        mShiftingSpacerRight = new QSpacerItem (0, 0,
                                                QSizePolicy::Fixed,
                                                QSizePolicy::Fixed);
        mShiftingSpacerBottom = new QSpacerItem (0, 0,
                                                 QSizePolicy::Fixed,
                                                 QSizePolicy::Fixed);
        pMainLayout->addItem (mShiftingSpacerTop, 0, 0, 1, -1);
        pMainLayout->addItem (mShiftingSpacerLeft, 1, 0);
        pMainLayout->addItem (mShiftingSpacerRight, 1, 2);
        pMainLayout->addItem (mShiftingSpacerBottom, 2, 0, 1, -1);
    }

    vmPauseAction->setChecked (false);

    VBoxDefs::RenderMode mode = vboxGlobal().vmRenderMode();

    CConsole cconsole = csession.GetConsole();
    AssertWrapperOk (csession);

    console = new VBoxConsoleView (this, cconsole, mode,
                                   centralWidget());

    activateUICustomizations();

    static_cast<QGridLayout*>(centralWidget()->layout())->addWidget(console, 1, 1, Qt::AlignVCenter | Qt::AlignHCenter);

    CMachine cmachine = csession.GetMachine();

    /* Set the VM-specific application icon */
    /* Not on Mac OS X. The dock icon is handled below. */
#ifndef Q_WS_MAC
    setWindowIcon (vboxGlobal().vmGuestOSTypeIcon (cmachine.GetOSTypeId()));
#endif

    /* restore the position of the window and some options */
    {
        QString str = cmachine.GetExtraData (VBoxDefs::GUI_LastWindowPosition);

        QRect ar = QApplication::desktop()->availableGeometry (this);
        bool ok = false, max = false;
        int x = 0, y = 0, w = 0, h = 0;
        x = str.section (',', 0, 0).toInt (&ok);
        if (ok)
            y = str.section (',', 1, 1).toInt (&ok);
        if (ok)
            w = str.section (',', 2, 2).toInt (&ok);
        if (ok)
            h = str.section (',', 3, 3).toInt (&ok);
        if (ok)
            max = str.section (',', 4, 4) == VBoxDefs::GUI_LastWindowPosition_Max;
        if (ok)
        {
            normal_pos = QPoint (x, y);
            normal_size = QSize (w, h);

            move (normal_pos);
            resize (normal_size);
        }
        else
        {
            normal_pos = QPoint();
            normal_size = QSize();
        }
        /* normalize to the optimal size */
        console->normalizeGeometry (true /* adjustPosition */);
        /* maximize if needed */
        if (max)
            setWindowState (windowState() | Qt::WindowMaximized);
        was_max = max;

        show();

        str = cmachine.GetExtraData (VBoxDefs::GUI_Fullscreen);
        if (str == "on")
            vmFullscreenAction->setChecked (true);

        vmSeamlessAction->setEnabled (false);
        str = cmachine.GetExtraData (VBoxDefs::GUI_Seamless);
        if (str == "on")
            vmSeamlessAction->setChecked (true);

        str = cmachine.GetExtraData (VBoxDefs::GUI_AutoresizeGuest);
        if (str != "off")
            vmAutoresizeGuestAction->setChecked (true);

        str = cmachine.GetExtraData (VBoxDefs::GUI_FirstRun);
        if (str == "yes")
            mIsFirstTimeStarted = true;
        else if (!str.isEmpty())
            cmachine.SetExtraData (VBoxDefs::GUI_FirstRun, QString::null);

        str = cmachine.GetExtraData (VBoxDefs::GUI_SaveMountedAtRuntime);
        if (str == "no")
            mIsAutoSaveMedia = false;
    }

    /* initialize usb stuff */
    CUSBController usbctl = cmachine.GetUSBController();
    if (usbctl.isNull())
    {
        /* hide usb_menu & usb_separator & usb_status_led */
        devicesMenu->setItemVisible (devicesUSBMenuId, false);
        devicesMenu->setItemVisible (devicesUSBMenuSeparatorId, false);
        usb_light->setHidden (true);
    }
    else
    {
        bool isUSBEnabled = usbctl.GetEnabled();
        devicesUSBMenu->setEnabled (isUSBEnabled);
        devicesUSBMenu->setConsole (cconsole);
        usb_light->setState (isUSBEnabled ? KDeviceActivity_Idle
                                          : KDeviceActivity_Null);
    }

    /* initialize vrdp stuff */
    CVRDPServer vrdpsrv = cmachine.GetVRDPServer();
    if (vrdpsrv.isNull())
    {
        /* hide vrdp_menu_action & vrdp_separator & vrdp_status_icon */
        devicesSwitchVrdpAction->setVisible (false);
        devicesMenu->setItemVisible (devicesVRDPMenuSeparatorId, false);
#if 0
        vrdp_state->setHidden (true);
#endif
    }

    /* initialize shared folders stuff */
    CSharedFolderCollection sfcoll = cconsole.GetSharedFolders();
    if (sfcoll.isNull())
    {
        /* hide shared folders menu action & sf_separator & sf_status_icon */
        devicesSFDialogAction->setVisible (false);
        devicesMenu->setItemVisible (devicesSFMenuSeparatorId, false);
        sf_light->setHidden (true);
    }

    /* start an idle timer that will update device lighths */
    connect (idle_timer, SIGNAL (timeout()), SLOT (updateDeviceLights()));
    idle_timer->start (50);

    connect (console, SIGNAL (mouseStateChanged (int)),
             this, SLOT (updateMouseState (int)));
    connect (console, SIGNAL (keyboardStateChanged (int)),
             hostkey_state, SLOT (setState (int)));
    connect (console, SIGNAL (machineStateChanged (KMachineState)),
             this, SLOT (updateMachineState (KMachineState)));
    connect (console, SIGNAL (additionsStateChanged (const QString&, bool, bool)),
             this, SLOT (updateAdditionsState (const QString &, bool, bool)));
    connect (console, SIGNAL (mediaChanged (VBoxDefs::DiskType)),
             this, SLOT (updateMediaState (VBoxDefs::DiskType)));
    connect (console, SIGNAL (usbStateChange()),
             this, SLOT (updateUsbState()));
    connect (console, SIGNAL (networkStateChange()),
             this, SLOT (updateNetworkAdarptersState()));
    connect (console, SIGNAL (sharedFoldersChanged()),
             this, SLOT (updateSharedFoldersState()));

#ifdef Q_WS_MAC
    QString osTypeId = cmachine.GetOSTypeId();
    QImage osImg100x75 = vboxGlobal().vmGuestOSTypeIcon (osTypeId).convertToImage().smoothScale (100, 75);
    QImage osImg = QImage (":/dock_1.png");
#warning port me
//    bitBlt (&osImg, 14, 22,
//            &osImg100x75, 0, 0,
//            100, 75, /* conversion_flags */ 0);
//    QImage VBoxOverlay = QImage (":/VirtualBox_48px.png");
//    bitBlt (&osImg, osImg.width() - VBoxOverlay.width(), osImg.height() - VBoxOverlay.height(),
//            &VBoxOverlay, 0, 0,
//            VBoxOverlay.width(), VBoxOverlay.height(), /* conversion_flags */ 0);
//    if (dockImgOS)
//        CGImageRelease (dockImgOS);
    dockImgOS = ::DarwinQImageToCGImage (&osImg);
    SetApplicationDockTileImage (dockImgOS);
#endif

    /* set the correct initial machine_state value */
    machine_state = cconsole.GetState();

    console->normalizeGeometry (false /* adjustPosition */);

    updateAppearanceOf (AllStuff);

    if (vboxGlobal().settings().autoCapture())
        vboxProblem().remindAboutAutoCapture();

    /*
     *  The further startup procedure should be done after we leave this method
     *  and enter the main event loop in main(), because it may result into
     *  showing various modal dialigs that will process events from within
     *  this method that in turn can lead to various side effects like this
     *  window is closed before this mehod returns, etc.
     */

    QTimer::singleShot (0, this, SLOT (finalizeOpenView()));

    LogFlowFuncLeave();
    return true;
}

void VBoxConsoleWnd::activateUICustomizations()
{
    VBoxGlobalSettings settings = vboxGlobal().settings();
    /* Process known keys */
    menuBar()->setHidden (settings.isFeatureActive ("noMenuBar"));
    statusBar()->setHidden (settings.isFeatureActive ("noStatusBar"));
}

void VBoxConsoleWnd::finalizeOpenView()
{
    LogFlowFuncEnter();

    /* Notify the console scroll-view about the console-window is opened. */
    console->onViewOpened();

    bool saved = machine_state == KMachineState_Saved;

    CMachine cmachine = csession.GetMachine();
    CConsole cconsole = console->console();

    if (mIsFirstTimeStarted)
    {
        VBoxVMFirstRunWzd wzd (cmachine, this);
        wzd.exec();

        /* Remove GUI_FirstRun extra data key from the machine settings
         * file after showing the wizard once. */
        cmachine.SetExtraData (VBoxDefs::GUI_FirstRun, QString::null);
    }

    /* start the VM */
    CProgress progress = cconsole.PowerUp();

    /* check for an immediate failure */
    if (!cconsole.isOk())
    {
        vboxProblem().cannotStartMachine (cconsole);
        /* close this window (this will call closeView()) */
        close();

        LogFlowFunc (("Error starting VM\n"));
        LogFlowFuncLeave();
        return;
    }

    console->attach();

    /* Disable auto closure because we want to have a chance to show the
     * error dialog on startup failure */
    no_auto_close = true;

    /* show the "VM starting / restoring" progress dialog */

    if (saved)
        vboxProblem().showModalProgressDialog (progress, cmachine.GetName(),
                                               this, 0);
    else
        vboxProblem().showModalProgressDialog (progress, cmachine.GetName(),
                                               this);

    if (progress.GetResultCode() != 0)
    {
        vboxProblem().cannotStartMachine (progress);
        /* close this window (this will call closeView()) */
        close();

        LogFlowFunc (("Error starting VM\n"));
        LogFlowFuncLeave();
        return;
    }

    no_auto_close = false;

    /* Check if we missed a really quick termination after successful
     * startup, and process it if we did. */
    if (machine_state < KMachineState_Running)
    {
        close();
        LogFlowFuncLeave();
        return;
    }

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* open debugger windows if requested */
    if (vboxGlobal().isDebuggerVisibleAtStartup())
    {
        move (QPoint (0, 0));
        dbgShowStatistics();
        dbgShowCommandLine();
    }
#endif

    /* If seamless mode should be enabled then check if it is enabled
     * currently and re-enable it if seamless is supported */
    if (vmSeamlessAction->isChecked() && mIsSeamlessSupported)
        toggleFullscreenMode (true, true);
    mIsOpenViewFinished = true;
    LogFlowFuncLeave();

#ifdef VBOX_WITH_REGISTRATION_REQUEST
    vboxGlobal().showRegistrationDialog (false /* aForce */);
#endif
}

/**
 *  Closes the console view opened by openView().
 *  Does nothing if no console view was opened.
 */
void VBoxConsoleWnd::closeView()
{
    LogFlowFuncEnter();

    if (!console)
    {
        LogFlow (("Already closed!\n"));
        LogFlowFuncLeave();
        return;
    }

    idle_timer->stop();
    idle_timer->disconnect (SIGNAL (timeout()), this, SLOT (updateDeviceLights()));

    hide();

    /* save the position of the window and some options */
    {
        CMachine machine = csession.GetMachine();
        QString winPos = QString ("%1,%2,%3,%4")
                                 .arg (normal_pos.x()).arg (normal_pos.y())
                                 .arg (normal_size.width())
                                 .arg (normal_size.height());
        if (isMaximized() || (mIsFullscreen && was_max)
                          || (mIsSeamless && was_max))
            winPos += QString (",%1").arg (VBoxDefs::GUI_LastWindowPosition_Max);

        machine.SetExtraData (VBoxDefs::GUI_LastWindowPosition, winPos);

        machine.SetExtraData (VBoxDefs::GUI_Fullscreen,
                              vmFullscreenAction->isChecked() ? "on" : "off");
        machine.SetExtraData (VBoxDefs::GUI_Seamless,
                              vmSeamlessAction->isChecked() ? "on" : "off");
        machine.SetExtraData (VBoxDefs::GUI_AutoresizeGuest,
                              vmAutoresizeGuestAction->isChecked() ? "on" : "off");
    }

    console->detach();

    centralWidget()->layout()->removeWidget (console);
    delete console;
    console = 0;
    csession.Close();
    csession.detach();

    LogFlowFuncLeave();
}

/**
 *  Refreshes the console view by readressing this call to
 *  VBoxConsoleView::refresh(). Does nothing if the console view doesn't
 *  exist (i.e., as before openView() or after stopView()).
 */
void VBoxConsoleWnd::refreshView()
{
    if ( console ) {
        console->refresh();
    }
}

/**
 *  This slot is called just after entering the fullscreen/seamless mode,
 *  when the console was resized to required size.
 */
void VBoxConsoleWnd::onEnterFullscreen()
{
    disconnect (console, SIGNAL (resizeHintDone()), 0, 0);
    /* It isn't guaranteed that the guest os set the video mode that
     * we requested. So after all the resizing stuff set the clipping
     * mask and the spacing shifter to the corresponding values. */
    setViewInSeamlessMode (QRect(console->mapToGlobal (QPoint(0, 0)), console->size()));
#ifdef Q_WS_MAC
    if (!mIsSeamless)
    {
        /* Fade back to the normal gamma */
        CGDisplayFade (mFadeToken, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0.0, 0.0, 0.0, false);
        CGReleaseDisplayFadeReservation (mFadeToken);
    }
#endif

    vmSeamlessAction->setEnabled (mIsSeamless);
    vmFullscreenAction->setEnabled (mIsFullscreen);
    if (mIsSeamless)
        connect (console, SIGNAL (resizeHintDone()),
                 this, SLOT(exitSeamless()));
    else if (mIsFullscreen)
        connect (console, SIGNAL (resizeHintDone()),
                 this, SLOT(exitFullscreen()));
}

/**
 *  This slot is called just after leaving the fullscreen/seamless mode,
 *  when the console was resized to previous size.
 */
void VBoxConsoleWnd::onExitFullscreen()
{
    disconnect (console, SIGNAL (resizeHintDone()), 0, 0);
#ifdef Q_WS_MAC
    if (!mIsSeamless)
    {
        /* Fade back to the normal gamma */
        CGDisplayFade (mFadeToken, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0.0, 0.0, 0.0, false);
        CGReleaseDisplayFadeReservation (mFadeToken);
    }
#endif

    vmSeamlessAction->setEnabled (mIsSeamlessSupported);
    vmFullscreenAction->setEnabled (true);

    console->setIgnoreMainwndResize (false);
    console->normalizeGeometry (true /* adjustPosition */);
}

/**
 *  This slot is called if the guest changes resolution while in fullscreen
 *  mode.
 */
void VBoxConsoleWnd::exitFullscreen()
{
    if (mIsFullscreen && vmFullscreenAction->isEnabled())
        vmFullscreenAction->toggle();
}

/**
 *  This slot is called if the guest changes resolution while in seamless
 *  mode.
 */
void VBoxConsoleWnd::exitSeamless()
{
    if (mIsSeamless && vmSeamlessAction->isEnabled())
        vmSeamlessAction->toggle();
}

void VBoxConsoleWnd::setMouseIntegrationLocked (bool aDisabled)
{
    vmDisableMouseIntegrAction->setChecked (aDisabled);
    vmDisableMouseIntegrAction->setEnabled (false);
}

/**
 *  Shows up and activates the popup version of the main menu.
 *
 *  @param aCenter If @a true, center the popup menu on the screen, otherwise
 *                 show it at the current mouse pointer location.
 */
void VBoxConsoleWnd::popupMainMenu (bool aCenter)
{
    QPoint pos = QCursor::pos();
    if (aCenter)
    {
        QRect deskGeo = QApplication::desktop()->screenGeometry (this);
        QRect popGeo = mMainMenu->frameGeometry();
        popGeo.moveCenter (QPoint (deskGeo.width() / 2, deskGeo.height() / 2));
        pos = popGeo.topLeft();
    }
    else
    {
        /* put the menu's bottom right corner to the pointer's hotspot point */
        pos.setX (pos.x() - mMainMenu->frameGeometry().width());
        pos.setY (pos.y() - mMainMenu->frameGeometry().height());
    }

    mMainMenu->popup (pos);
    mMainMenu->activateWindow();
    mMainMenu->setActiveItem (0);
}

//
// Protected Members
/////////////////////////////////////////////////////////////////////////////

bool VBoxConsoleWnd::event (QEvent *e)
{
    switch (e->type())
    {
        /* By handling every Resize and Move we keep track of the normal
         * (non-minimized and non-maximized) window geometry. Shame on Qt
         * that it doesn't provide this geometry in its public APIs. */

        case QEvent::Resize:
        {
            QResizeEvent *re = (QResizeEvent *) e;

            if (!isMaximized() && !isTrueFullscreen() && !isTrueSeamless())
            {
                normal_size = re->size();
#ifdef VBOX_WITH_DEBUGGER_GUI
                dbgAdjustRelativePos();
#endif
            }
            break;
        }
        case QEvent::Move:
        {
            if (!isMaximized() && !isTrueFullscreen() && !isTrueSeamless())
            {
                normal_pos = pos();
#ifdef VBOX_WITH_DEBUGGER_GUI
                dbgAdjustRelativePos();
#endif
            }
            break;
        }
        case StatusTipEvent::Type:
        {
            StatusTipEvent *ev = (StatusTipEvent*) e;
            statusBar()->showMessage (ev->mTip);
            break;
        }
        default:
            break;
    }

    return QMainWindow::event (e);
}

void VBoxConsoleWnd::closeEvent (QCloseEvent *e)
{
    LogFlowFuncEnter();

    static const char *kSave = "save";
    static const char *kShutdown = "shutdown";
    static const char *kPowerOff = "powerOff";
    static const char *kDiscardCurState = "discardCurState";

    if (!console)
    {
        e->accept();
        LogFlowFunc (("Console already destroyed!"));
        LogFlowFuncLeave();
        return;
    }

    if (machine_state > KMachineState_Paused &&
        machine_state != KMachineState_Stuck)
    {
        /*
         *  The machine is in some temporary state like Saving or Stopping.
         *  Ignore the close event. When it is Stopping, it will be soon closed
         *  anyway from updateMachineState(). In all other cases, an appropriate
         *  progress dialog will be shown within a few seconds.
         */
        e->ignore();
    }
    else
    if (machine_state < KMachineState_Running)
    {
        /*
         *  the machine has been already powered off or saved or aborted --
         *  close the window immediately
         */
        e->accept();
    }
    else
    {
        /* start with ignore the close event */
        e->ignore();

        /* Disable auto closure because we want to have a chance to show the
         * error dialog on save state / power off failure. */
        no_auto_close = true;

        bool success = true;

        bool wasPaused = machine_state == KMachineState_Paused ||
                         machine_state == KMachineState_Stuck;
        if (!wasPaused)
        {
            /* Suspend the VM and ignore the close event if failed to do so.
             * pause() will show the error message to the user. */
            success = console->pause (true);
        }

        if (success)
        {
            success = false;

            CMachine cmachine = csession.GetMachine();
            VBoxCloseVMDlg dlg (this);
            QString typeId = cmachine.GetOSTypeId();
            dlg.pmIcon->setPixmap (vboxGlobal().vmGuestOSTypeIcon (typeId));

            /* make the Discard checkbox invisible if there are no snapshots */
            dlg.mCbDiscardCurState->setVisible ((cmachine.GetSnapshotCount() > 0));

            if (machine_state != KMachineState_Stuck)
            {
                /* read the last user's choice for the given VM */
                QStringList lastAction =
                    cmachine.GetExtraData (VBoxDefs::GUI_LastCloseAction).split (',');
                AssertWrapperOk (cmachine);
                if (lastAction [0] == kPowerOff)
                    dlg.mRbPowerOff->setChecked (true);
                else if (lastAction [0] == kShutdown)
                    dlg.mRbShutdown->setChecked (true);
                else if (lastAction [0] == kSave)
                    dlg.mRbSave->setChecked (true);
                else /* the default is ACPI Shutdown */
                    dlg.mRbShutdown->setChecked (true);
                dlg.mCbDiscardCurState->setChecked (
                    lastAction.count() > 1 && lastAction [1] == kDiscardCurState);
            }
            else
            {
                /* The stuck VM can only be powered off; disable anything
                 * else and choose PowerOff */
                dlg.mRbSave->setEnabled (false);
                dlg.mRbPowerOff->setChecked (true);
            }

            bool wasShutdown = false;

            if (dlg.exec() == QDialog::Accepted)
            {
                CConsole cconsole = console->console();

                if (dlg.mRbSave->isChecked())
                {
                    CProgress progress = cconsole.SaveState();

                    if (cconsole.isOk())
                    {
                        /* show the "VM saving" progress dialog */
                        vboxProblem()
                            .showModalProgressDialog (progress, cmachine.GetName(),
                                                      this, 0);
                        if (progress.GetResultCode() != 0)
                            vboxProblem().cannotSaveMachineState (progress);
                        else
                            success = true;
                    }
                    else
                        vboxProblem().cannotSaveMachineState (cconsole);
                }
                else
                if (dlg.mRbShutdown->isChecked())
                {
                    /* unpause the VM to let it grab the ACPI shutdown event */
                    console->pause (false);
                    /* prevent the subsequent unpause request */
                    wasPaused = true;
                    /* signal ACPI shutdown (if there is no ACPI device, the
                     * operation will fail) */
                    cconsole.PowerButton();
                    wasShutdown = cconsole.isOk();
                    if (!wasShutdown)
                        vboxProblem().cannotACPIShutdownMachine (cconsole);
                    /* success is always false because we never accept the close
                     * window action when doing ACPI shutdown */
                    success = false;
                }
                else
                if (dlg.mRbPowerOff->isChecked())
                {
                    cconsole.PowerDown();
                    if (!cconsole.isOk())
                    {
                        /// @todo (dmik) add an option to close the GUI anyway
                        //  and handle it
                        vboxProblem().cannotStopMachine (cconsole);
                    }
                    else
                    {
                        /*
                         *  set success to true even if we fail to discard the
                         *  current state later -- the console window will be
                         *  closed anyway
                         */
                        success = true;

                        /* discard the current state if requested */
                        if (dlg.mCbDiscardCurState->isVisible() &&
                            dlg.mCbDiscardCurState->isChecked())
                        {
                            CProgress progress = cconsole.DiscardCurrentState();
                            if (cconsole.isOk())
                            {
                                /* show the progress dialog */
                                vboxProblem()
                                    .showModalProgressDialog (progress,
                                                              cmachine.GetName(),
                                                              this);
                                if (progress.GetResultCode() != 0)
                                    vboxProblem()
                                        .cannotDiscardCurrentState (progress);
                            }
                            else
                                vboxProblem().cannotDiscardCurrentState (cconsole);
                        }
                    }
                }

                if (success)
                {
                    /* accept the close action on success */
                    e->accept();
                }

                if (success || wasShutdown)
                {
                    /* memorize the last user's choice for the given VM */
                    QString lastAction = kPowerOff;
                    if (dlg.mRbSave->isChecked())
                        lastAction = kSave;
                    else if (dlg.mRbShutdown->isChecked())
                        lastAction = kShutdown;
                    else if (dlg.mRbPowerOff->isChecked())
                        lastAction = kPowerOff;
                    else
                        AssertFailed();
                    if (dlg.mCbDiscardCurState->isChecked())
                        (lastAction += ",") += kDiscardCurState;
                    cmachine.SetExtraData (VBoxDefs::GUI_LastCloseAction, lastAction);
                    AssertWrapperOk (cmachine);
                }
            }
        }

        no_auto_close = false;

        if (machine_state < KMachineState_Running)
        {
            /*
             *  the machine has been stopped while showing the Close or the Pause
             *  failure dialog -- accept the close event immediately
             */
            e->accept();
        }
        else
        {
            if (!success)
            {
                /* restore the running state if needed */
                if (!wasPaused && machine_state == KMachineState_Paused)
                    console->pause (false);
            }
        }
    }

    if (e->isAccepted())
    {
#ifndef VBOX_GUI_SEPARATE_VM_PROCESS
        vboxGlobal().selectorWnd().show();
#endif
        closeView();
    }

    LogFlowFunc (("accepted=%d\n", e->isAccepted()));
    LogFlowFuncLeave();
}

#if defined(Q_WS_X11)
bool VBoxConsoleWnd::x11Event (XEvent *event)
{
    // Qt bug: when the console view grabs the keyboard, FocusIn, FocusOut,
    // WindowActivate and WindowDeactivate Qt events are not properly sent
    // on top level window (i.e. this) deactivation. The fix is to substiute
    // the mode in FocusOut X11 event structure to NotifyNormal to cause
    // Qt to process it as desired.
    if (console && event->type == FocusOut)
    {
        if (event->xfocus.mode == NotifyWhileGrabbed  &&
            (event->xfocus.detail == NotifyAncestor ||
             event->xfocus.detail == NotifyInferior ||
             event->xfocus.detail == NotifyNonlinear))
        {
             event->xfocus.mode = NotifyNormal;
        }
    }
    return false;
}
#endif

//
// Private members
/////////////////////////////////////////////////////////////////////////////

/**
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void VBoxConsoleWnd::languageChange()
{
#ifdef VBOX_OSE
    caption_prefix = tr ("VirtualBox OSE");
#else
    caption_prefix = tr ("innotek VirtualBox");
#endif

    /*
     *  Note: All action shortcuts should be added to the menu text in the
     *  form of "\tHost+<Key>" where <Key> is the shortcut key according
     *  to regular QKeySequence rules. No translation of the "Host" word is
     *  allowed (VBoxConsoleView relies on its spelling). setAccel() must not
     *  be used.
     */

    /* VM actions */

    vmFullscreenAction->setText (tr ("&Fullscreen Mode") + "\tHost+F");
    vmFullscreenAction->setStatusTip (tr ("Switch to fullscreen mode" ));

    vmSeamlessAction->setText (tr ("Seam&less Mode") + "\tHost+L");
    vmSeamlessAction->setStatusTip (tr ("Switch to seamless desktop integration mode"));

    vmDisMouseIntegrMenu->setToolTip (tr ("Mouse Integration",
                                          "enable/disable..."));
    vmAutoresizeMenu->setToolTip (tr ("Auto-resize Guest Display",
                                      "enable/disable..."));
    vmAutoresizeGuestAction->setText (tr ("Auto-resize &Guest Display") +
                                          "\tHost+G");
    vmAutoresizeGuestAction->setStatusTip (
        tr ("Automatically resize the guest display when the window is resized "
            "(requires Guest Additions)"));

    vmAdjustWindowAction->setText (tr ("&Adjust Window Size") + "\tHost+A");
    vmAdjustWindowAction->setStatusTip (
        tr ("Adjust window size and position to best fit the guest display"));

    vmTypeCADAction->setText (tr ("&Insert Ctrl-Alt-Del") + "\tHost+Del");
    vmTypeCADAction->setStatusTip (
        tr ("Send the Ctrl-Alt-Del sequence to the virtual machine"));

#if defined(Q_WS_X11)
    vmTypeCABSAction->setText (tr ("&Insert Ctrl-Alt-Backspace") +
                                   "\tHost+Backspace");
    vmTypeCABSAction->setStatusTip (
        tr ("Send the Ctrl-Alt-Backspace sequence to the virtual machine"));
#endif

    vmResetAction->setText (tr ("&Reset") + "\tHost+R");
    vmResetAction->setStatusTip (tr ("Reset the virtual machine"));

    /* vmPauseAction is set up in updateAppearanceOf() */

    vmACPIShutdownAction->setText (tr ("ACPI S&hutdown") + "\tHost+H");
    vmACPIShutdownAction->setStatusTip (
        tr ("Send the ACPI Power Button press event to the virtual machine"));

    vmCloseAction->setText (tr ("&Close..." ) + "\tHost+Q");
    vmCloseAction->setStatusTip (tr ("Close the virtual machine"));

    vmTakeSnapshotAction->setText (tr ("Take &Snapshot..." ) + "\tHost+S");
    vmTakeSnapshotAction->setStatusTip (tr ("Take a snapshot of the virtual machine"));

    vmShowInformationDlgAction->setText (tr ("Session I&nformation Dialog") +
                                             "\tHost+N");
    vmShowInformationDlgAction->setStatusTip (tr ("Show Session Information Dialog"));

    /* vmDisableMouseIntegrAction is set up in updateAppearanceOf() */

    /* Devices actions */

    devicesMountFloppyImageAction->setText (tr ("&Floppy Image..."));
    devicesMountFloppyImageAction->setStatusTip (tr ("Mount a floppy image file"));

    devicesUnmountFloppyAction->setText (tr ("Unmount F&loppy"));
    devicesUnmountFloppyAction->setStatusTip (
        tr ("Unmount the currently mounted floppy media"));

    devicesMountDVDImageAction->setText (tr ("&CD/DVD-ROM Image..."));
    devicesMountDVDImageAction->setStatusTip (
        tr ("Mount a CD/DVD-ROM image file"));

    devicesUnmountDVDAction->setText (tr ("Unmount C&D/DVD-ROM"));
    devicesUnmountDVDAction->setStatusTip (
        tr ("Unmount the currently mounted CD/DVD-ROM media"));

    devicesVRDPMenu->setToolTip (tr ("Remote Desktop (RDP) Server",
                                     "enable/disable..."));
    devicesSwitchVrdpAction->setText (tr ("Remote Dis&play"));
    devicesSwitchVrdpAction->setStatusTip (
        tr ("Enable or disable remote desktop (RDP) connections to this machine"));

    devicesSFDialogAction->setText (tr ("&Shared Folders..."));
    devicesSFDialogAction->setStatusTip (
        tr ("Open the dialog to operate on shared folders"));

    devicesInstallGuestToolsAction->setText (tr ("&Install Guest Additions..."));
    devicesInstallGuestToolsAction->setStatusTip (
        tr ("Mount the Guest Additions installation image"));

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* Debug actions */

    if (dbgStatisticsAction)
        dbgStatisticsAction->setText (tr ("&Statistics..."));
    if (dbgCommandLineAction)
        dbgCommandLineAction->setText (tr ("&Command line..."));
#endif

    /* Help actions */

    helpContentsAction->setText (tr ("&Contents..."));
    helpContentsAction->setStatusTip (tr ("Show the online help contents"));

    helpWebAction->setText (tr ("&VirtualBox Web Site..."));
    helpWebAction->setStatusTip (
        tr ("Open the browser and go to the VirtualBox product web site"));

    helpRegisterAction->setText (tr ("R&egister VirtualBox..."));
    helpRegisterAction->setStatusTip (
        tr ("Open VirtualBox registration form"));

    helpAboutAction->setText (tr ("&About VirtualBox..."));
    helpAboutAction->setStatusTip (tr ("Show a dialog with product information"));

    helpResetMessagesAction->setText (tr ("&Reset All Warnings"));
    helpResetMessagesAction->setStatusTip (
        tr ("Cause all suppressed warnings and messages to be shown again"));

    /* Devices menu submenus */

    devicesMenu->changeItem (devicesMountFloppyMenuId, tr ("Mount &Floppy"));
    devicesMenu->changeItem (devicesMountDVDMenuId, tr ("Mount &CD/DVD-ROM"));
    devicesMenu->changeItem (devicesNetworkMenuId, tr ("&Network Adapters"));
    devicesMenu->changeItem (devicesUSBMenuId, tr ("&USB Devices"));

    /* main menu & seamless popup menu */

    menuBar()->changeItem (vmMenuId, tr ("&Machine"));
#warning port me: check this if the menu popups in the vm. (H+HOME?)
//    mMainMenu->changeItem (vmMenuId,
//        VBoxGlobal::iconSet (":/machine_16px.png"), tr ("&Machine"));
    menuBar()->changeItem (devicesMenuId, tr ("&Devices"));
//    mMainMenu->changeItem (devicesMenuId,
//        VBoxGlobal::iconSet (":/settings_16px.png"), tr ("&Devices"));
#ifdef VBOX_WITH_DEBUGGER_GUI
    if (vboxGlobal().isDebuggerEnabled())
    {
        menuBar()->changeItem (dbgMenuId, tr ("De&bug"));
        mMainMenu->changeItem (dbgMenuId, tr ("De&bug"));
    }
#endif
    menuBar()->changeItem (helpMenuId, tr ("&Help"));
//    mMainMenu->changeItem (helpMenuId,
//        VBoxGlobal::iconSet (":/help_16px.png"), tr ("&Help"));

    /* status bar widgets */

#if 0
    autoresize_state->setToolTip (
        tr ("Indicates whether the guest display auto-resize function is On "
            "(<img src=:/auto_resize_on_16px.png/>) or Off (<img src=:/auto_resize_off_16px.png/>). "
            "Note that this function requires Guest Additions to be installed in the guest OS."));
#endif
    mouse_state->setToolTip (
        tr ("Indicates whether the host mouse pointer is captured by the guest OS:<br>"
            "<nobr><img src=:/mouse_disabled_16px.png/>&nbsp;&nbsp;pointer is not captured</nobr><br>"
            "<nobr><img src=:/mouse_16px.png/>&nbsp;&nbsp;pointer is captured</nobr><br>"
            "<nobr><img src=:/mouse_seamless_16px.png/>&nbsp;&nbsp;mouse integration (MI) is On</nobr><br>"
            "<nobr><img src=:/mouse_can_seamless_16px.png/>&nbsp;&nbsp;MI is Off, pointer is captured</nobr><br>"
            "<nobr><img src=:/mouse_can_seamless_uncaptured_16px.png/>&nbsp;&nbsp;MI is Off, pointer is not captured</nobr><br>"
            "Note that the mouse integration feature requires Guest Additions to be installed in the guest OS."));
    hostkey_state->setToolTip (
        tr ("Indicates whether the keyboard is captured by the guest OS "
            "(<img src=:/hostkey_captured_16px.png/>) or not (<img src=:/hostkey_16px.png/>)."));
    hostkey_name->setToolTip (
        tr ("Shows the currently assigned Host key.<br>"
            "This key, when pressed alone, toggles the the keyboard and mouse "
            "capture state. It can also be used in combination with other keys "
            "to quickly perform actions from the main menu."));

    updateAppearanceOf (AllStuff);
}


void VBoxConsoleWnd::updateAppearanceOf (int element)
{
    if (!console) return;

    CMachine cmachine = csession.GetMachine();
    CConsole cconsole = console->console();

    bool isRunningOrPaused = machine_state == KMachineState_Running ||
                             machine_state == KMachineState_Paused;

    if (element & Caption)
    {
        QString snapshotName;
        if (cmachine.GetSnapshotCount() > 0)
        {
            CSnapshot snapshot = cmachine.GetCurrentSnapshot();
            snapshotName = " (" + snapshot.GetName() + ")";
        }
        setWindowTitle (cmachine.GetName() + snapshotName +
                        " [" + vboxGlobal().toString (machine_state) + "] - " +
                        caption_prefix);
//#ifdef Q_WS_MAC
//        SetWindowTitleWithCFString (reinterpret_cast <WindowPtr> (this->winId()), CFSTR("sfds"));
//SetWindowAlternateTitle
//#endif
    }
    if (element & FloppyStuff)
    {
        devicesMountFloppyMenu->setEnabled (isRunningOrPaused);
        CFloppyDrive floppy = cmachine.GetFloppyDrive();
        KDriveState state = floppy.GetState();
        bool mounted = state != KDriveState_NotMounted;
        devicesUnmountFloppyAction->setEnabled (isRunningOrPaused && mounted);
        fd_light->setState (mounted ? KDeviceActivity_Idle : KDeviceActivity_Null);
        QString tip = tr ("<qt><nobr>Indicates the activity of the floppy media:</nobr>"
                          "%1</qt>",
                          "Floppy tooltip");
        QString name;
        switch (state)
        {
            case KDriveState_HostDriveCaptured:
            {
                CHostFloppyDrive drv = floppy.GetHostDrive();
                QString drvName = drv.GetName();
                QString description = drv.GetDescription();
                QString fullName = description.isEmpty() ?
                    drvName :
                    QString ("%1 (%2)").arg (description, drvName);
                name = tr ("<br><nobr><b>Host Drive</b>: %1</nobr>",
                           "Floppy tooltip").arg (fullName);
                break;
            }
            case KDriveState_ImageMounted:
            {
                name = tr ("<br><nobr><b>Image</b>: %1</nobr>",
                           "Floppy tooltip")
                    .arg (QDir::convertSeparators (floppy.GetImage().GetFilePath()));
                break;
            }
            case KDriveState_NotMounted:
            {
                name = tr ("<br><nobr><b>No media mounted</b></nobr>",
                           "Floppy tooltip");
                break;
            }
            default:
                AssertMsgFailed (("Invalid floppy drive state: %d\n", state));
        }
        fd_light->setToolTip (tip.arg (name));
    }
    if (element & DVDStuff)
    {
        devicesMountDVDMenu->setEnabled (isRunningOrPaused);
        CDVDDrive dvd = cmachine.GetDVDDrive();
        KDriveState state = dvd.GetState();
        bool mounted = state != KDriveState_NotMounted;
        devicesUnmountDVDAction->setEnabled (isRunningOrPaused && mounted);
        cd_light->setState (mounted ? KDeviceActivity_Idle : KDeviceActivity_Null);
        QString tip = tr ("<qt><nobr>Indicates the activity of the CD/DVD-ROM media:</nobr>"
                          "%1</qt>",
                          "DVD-ROM tooltip");
        QString name;
        switch (state)
        {
            case KDriveState_HostDriveCaptured:
            {
                CHostDVDDrive drv = dvd.GetHostDrive();
                QString drvName = drv.GetName();
                QString description = drv.GetDescription();
                QString fullName = description.isEmpty() ?
                    drvName :
                    QString ("%1 (%2)").arg (description, drvName);
                name = tr ("<br><nobr><b>Host Drive</b>: %1</nobr>",
                           "DVD-ROM tooltip").arg (fullName);
                break;
            }
            case KDriveState_ImageMounted:
            {
                name = tr ("<br><nobr><b>Image</b>: %1</nobr>",
                           "DVD-ROM tooltip")
                    .arg (QDir::convertSeparators (dvd.GetImage().GetFilePath()));
                break;
            }
            case KDriveState_NotMounted:
            {
                name = tr ("<br><nobr><b>No media mounted</b></nobr>",
                           "DVD-ROM tooltip");
                break;
            }
            default:
                AssertMsgFailed (("Invalid DVD drive state: %d\n", state));
        }
        cd_light->setToolTip (tip.arg (name));
    }
    if (element & HardDiskStuff)
    {
        QString tip = tr ("<qt><nobr>Indicates the activity of virtual hard disks:</nobr>"
                          "%1</qt>",
                          "HDD tooltip");
        QString data;
        bool hasDisks = false;
        CHardDiskAttachmentEnumerator aen = cmachine.GetHardDiskAttachments().Enumerate();
        while (aen.HasMore())
        {
            CHardDiskAttachment hda = aen.GetNext();
            CHardDisk hd = hda.GetHardDisk();
            data += QString ("<br><nobr><b>%1 %2</b>: %3</nobr>")
                .arg (vboxGlobal().toString (hda.GetBus(), hda.GetChannel()))
                .arg (vboxGlobal().toString (hda.GetBus(), hda.GetChannel(),
                                             hda.GetDevice()))
                .arg (QDir::convertSeparators (hd.GetLocation()));
            hasDisks = true;
        }
        if (!hasDisks)
            data += tr ("<br><nobr><b>No hard disks attached</b></nobr>",
                        "HDD tooltip");
        hd_light->setToolTip (tip.arg (data));
        hd_light->setState (hasDisks ? KDeviceActivity_Idle : KDeviceActivity_Null);
    }
    if (element & NetworkStuff)
    {
        ulong maxCount = vboxGlobal().virtualBox()
                        .GetSystemProperties().GetNetworkAdapterCount();
        ulong count = 0;
        for (ulong slot = 0; slot < maxCount; slot++)
            if (cmachine.GetNetworkAdapter (slot).GetEnabled())
                count++;
        net_light->setState (count > 0 ? KDeviceActivity_Idle
                                       : KDeviceActivity_Null);

        devicesNetworkMenu->setEnabled (isRunningOrPaused && count > 0);

        /* update tooltip */
        QString ttip = tr ("<qt><nobr>Indicates the activity of the network "
                           "interfaces:</nobr>"
                           "%1</qt>",
                           "Network adapters tooltip");
        QString info;

        for (ulong slot = 0; slot < maxCount; ++ slot)
        {
            CNetworkAdapter adapter = cmachine.GetNetworkAdapter (slot);
            if (adapter.GetEnabled())
                info += tr ("<br><nobr><b>Adapter %1 (%2)</b>: cable %3</nobr>",
                            "Network adapters tooltip")
                    .arg (slot)
                    .arg (vboxGlobal().toString (adapter.GetAttachmentType()))
                    .arg (adapter.GetCableConnected() ?
                          tr ("connected", "Network adapters tooltip") :
                          tr ("disconnected", "Network adapters tooltip"));
        }

        if (info.isNull())
            info = tr ("<br><nobr><b>All network adapters are disabled</b></nobr>",
                       "Network adapters tooltip");

        net_light->setToolTip (ttip.arg (info));
    }
    if (element & USBStuff)
    {
        if (!usb_light->isHidden())
        {
            /* update tooltip */
            QString ttip = tr ("<qt><nobr>Indicates the activity of the "
                               "attached USB devices:</nobr>"
                               "%1</qt>",
                               "USB device tooltip");
            QString info;

            bool isUSBEnabled = cmachine.GetUSBController().GetEnabled();
            if (isUSBEnabled)
            {
                devicesUSBMenu->setEnabled (isRunningOrPaused);

                CUSBDeviceEnumerator en = cconsole.GetUSBDevices().Enumerate();
                while (en.HasMore())
                {
                    CUSBDevice usb = en.GetNext();
                    info += QString ("<br><b><nobr>%1</nobr></b>")
                                     .arg (vboxGlobal().details (usb));
                }
                if (info.isNull())
                    info = tr ("<br><nobr><b>No USB devices attached</b></nobr>",
                               "USB device tooltip");
            }
            else
            {
                devicesUSBMenu->setEnabled (false);

                info = tr ("<br><nobr><b>USB Controller is disabled</b></nobr>",
                           "USB device tooltip");
            }

            usb_light->setToolTip (ttip.arg (info));
        }
    }
    if (element & VRDPStuff)
    {
        CVRDPServer vrdpsrv = csession.GetMachine().GetVRDPServer();
        if (!vrdpsrv.isNull())
        {
            /* update menu&status icon state */
            bool isVRDPEnabled = vrdpsrv.GetEnabled();
            devicesSwitchVrdpAction->setChecked (isVRDPEnabled);
#if 0
            vrdp_state->setState (isVRDPEnabled ? 1 : 0);

            /* compose status icon tooltip */
            QString tip = tr ("Indicates whether the Remote Display (VRDP Server) "
                              "is enabled (<img src=:/vrdp_16px.png/>) or not "
                              "(<img src=:/vrdp_disabled_16px.png/>)."
                              );
            if (vrdpsrv.GetEnabled())
                tip += tr ("<hr>VRDP Server is listening on port %1").arg (vrdpsrv.GetPort());
            vrdp_state->setToolTip (tip);
#endif
        }
    }
    if (element & SharedFolderStuff)
    {
        QString tip = tr ("<qt><nobr>Indicates the activity of shared folders:</nobr>"
                          "%1</qt>",
                          "Shared folders tooltip");

        QString data;
        QMap <QString, QString> sfs;

        /// @todo later: add global folders

        /* permanent folders */
        CSharedFolderEnumerator en = cmachine.GetSharedFolders().Enumerate();
        while (en.HasMore())
        {
            CSharedFolder sf = en.GetNext();
            sfs.insert (sf.GetName(), sf.GetHostPath());
        }
        /* transient folders */
        en = cconsole.GetSharedFolders().Enumerate();
        while (en.HasMore())
        {
            CSharedFolder sf = en.GetNext();
            sfs.insert (sf.GetName(), sf.GetHostPath());
        }

        for (QMap<QString, QString>::const_iterator it = sfs.constBegin();
             it != sfs.constEnd(); ++it)
        {
            /* select slashes depending on the OS type */
            if (VBoxGlobal::isDOSType (cconsole.GetGuest().GetOSTypeId()))
                data += QString ("<tr><td><nobr><b>\\\\vboxsvr\\%1</b></nobr></td>"
                                 "<td><nobr>%2</nobr></td>")
                    .arg (it.key(), it.value());
            else
                data += QString ("<tr><td><nobr><b>%1</b></nobr></td>"
                                 "<td><nobr>%2</nobr></td></tr>")
                    .arg (it.key(), it.value());
        }

        if (sfs.count() == 0)
            data = tr ("<br><nobr><b>No shared folders</b></nobr>",
                       "Shared folders tooltip");
        else
            data = QString ("<br><table border=0 cellspacing=0 cellpadding=0 "
                            "width=100%>%1</table>").arg (data);

        sf_light->setToolTip (tip.arg (data));
    }
    if (element & PauseAction)
    {
        if (!vmPauseAction->isChecked())
        {
            vmPauseAction->setText (tr ("&Pause") + "\tHost+P");
            vmPauseAction->setStatusTip (
                tr ("Suspend the execution of the virtual machine"));
        }
        else
        {
            vmPauseAction->setText (tr ("R&esume") + "\tHost+P");
            vmPauseAction->setStatusTip (
                tr ("Resume the execution of the virtual machine" ) );
        }
        vmPauseAction->setEnabled (isRunningOrPaused);
    }
    if (element & DisableMouseIntegrAction)
    {
        if (!vmDisableMouseIntegrAction->isChecked())
        {
            vmDisableMouseIntegrAction->setText (tr ("Disable &Mouse Integration") +
                                                     "\tHost+I");
            vmDisableMouseIntegrAction->setStatusTip (
                tr ("Temporarily disable host mouse pointer integration"));
        }
        else
        {
            vmDisableMouseIntegrAction->setText (tr ("Enable &Mouse Integration") +
                                                     "\tHost+I");
            vmDisableMouseIntegrAction->setStatusTip (
                tr ("Enable temporarily disabled host mouse pointer integration"));
        }
        if (machine_state == KMachineState_Running)
            vmDisableMouseIntegrAction->setEnabled (console->isMouseAbsolute());
        else
            vmDisableMouseIntegrAction->setEnabled (false);
    }
}

/**
 * @return @c true if successfully performed the requested operation and false
 * otherwise.
 */
bool VBoxConsoleWnd::toggleFullscreenMode (bool aOn, bool aSeamless)
{
    disconnect (console, SIGNAL (resizeHintDone()), 0, 0);
    if (aSeamless)
    {
        /* Check if the Guest Video RAM enough for the seamless mode. */
        QRect screen = QApplication::desktop()->screenGeometry (this);
        ULONG64 availBits = (csession.GetMachine().GetVRAMSize() /* vram */
                          * _1M /* mb to bytes */
                          - 4096 /* adapter info */
                          - _1M /* current cache - may be changed in future */)
                          / csession.GetMachine().GetMonitorCount()
                          * 8; /* to bits */
        ULONG64 usedBits = screen.width() /* display width */
                         * screen.height() /* display height */
#warning port me: check this
                         * depth(); /* bit per pixel */
//                         * QColor::numBitPlanes(); /* bit per pixel */
        if (aOn && (availBits < usedBits))
        {
            vboxProblem().cannotEnterSeamlessMode (screen.width(),
#warning port me: check this
                screen.height(), depth());
//                screen.height(), QColor::numBitPlanes());
            return false;
        }
    }

    AssertReturn (console, false);
    AssertReturn ((hidden_children.isEmpty() == aOn), false);
    AssertReturn ((aSeamless && mIsSeamless != aOn) ||
                  (!aSeamless && mIsFullscreen != aOn), false);
    if (aOn)
        AssertReturn ((aSeamless && !mIsFullscreen) ||
                      (!aSeamless && !mIsSeamless), false);

    if (aOn)
    {
        /* Take the toggle hot key from the menu item. */
        QString hotKey = aSeamless ? vmSeamlessAction->text() :
                                     vmFullscreenAction->text();
        hotKey = hotKey.split ('\t')[1];
        Assert (!hotKey.isEmpty());

        /* Show the info message. */
        bool ok = aSeamless ?
            vboxProblem().confirmGoingSeamless (hotKey) :
            vboxProblem().confirmGoingFullscreen (hotKey);
        if (!ok)
            return false;
    }

#ifdef Q_WS_MAC
    if (!aSeamless)
    {
        /* Fade to black */
        CGAcquireDisplayFadeReservation (kCGMaxDisplayReservationInterval, &mFadeToken);
        CGDisplayFade (mFadeToken, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0.0, 0.0, 0.0, true);
    }
#endif

    if (aSeamless)
    {
        /* Activate the auto-resize feature required for the seamless mode. */
        if (!vmAutoresizeGuestAction->isChecked())
            vmAutoresizeGuestAction->setChecked (true);

        /* Activate the mouse integration feature for the seamless mode. */
        if (vmDisableMouseIntegrAction->isChecked())
            vmDisableMouseIntegrAction->setChecked (false);

        vmAdjustWindowAction->setEnabled (!aOn);
        vmFullscreenAction->setEnabled (!aOn);
        vmAutoresizeGuestAction->setEnabled (!aOn);
        vmDisableMouseIntegrAction->setEnabled (!aOn);

        console->console().GetDisplay().SetSeamlessMode (aOn);
        mIsSeamless = aOn;
    }
    else
    {
        mIsFullscreen = aOn;
        vmAdjustWindowAction->setEnabled (!aOn);
        vmSeamlessAction->setEnabled (!aOn && mIsSeamlessSupported);
    }

    bool wasHidden = isHidden();

    if (aOn)
    {
        /* Temporarily disable the mode-related action to make sure
         * user can not leave the mode before he enter it. */
        aSeamless ? vmSeamlessAction->setEnabled (false) :
                    vmFullscreenAction->setEnabled (false);
        /* Toggle console to manual resize mode. */
        console->setIgnoreMainwndResize (true);
        connect (console, SIGNAL (resizeHintDone()),
                 this, SLOT (onEnterFullscreen()));

        /* Memorize the maximized state. */
        QDesktopWidget *dtw = QApplication::desktop();
        was_max = isMaximized() &&
                  dtw->availableGeometry().width()  == frameSize().width() &&
                  dtw->availableGeometry().height() == frameSize().height();

        /* Save the previous scroll-view minimum size before entering
         * fullscreen/seamless state to restore this minimum size before
         * the exiting fullscreen. Required for correct scroll-view and
         * guest display update in SDL mode. */
        prev_min_size = console->minimumSize();
        console->setMinimumSize (0, 0);

        /* let the widget take the whole available desktop space */
        QRect scrGeo = aSeamless ?
            dtw->availableGeometry (this) : dtw->screenGeometry (this);

        /* It isn't guaranteed that the guest os set the video mode that
         * we requested. So after all the resizing stuff set the clipping
         * mask and the spacing shifter to the corresponding values. */
        setViewInSeamlessMode (dtw->availableGeometry (this));

#ifdef Q_WS_WIN32
        mPrevRegion = dtw->screenGeometry (this);
#endif

        /* Hide all but the central widget containing the console view. */
        QList<QWidget *> list = findChildren<QWidget *>();
        foreach (QWidget *w, list)
        {
            /* todo: The list is now recursive. So think about a better way to
             * prevent the childrens of the centralWidget to be hidden */
            if (w != centralWidget() &&
                w != console &&
                w != console->viewport())
            {
                if (!w->isHidden())
                {
                    w->hide();
                    hidden_children.append (w);
                }
            }
        }

#ifdef Q_WS_MAC
        if (!aSeamless)
        {
            /* Make the apple menu bar go away before setMaximumSize! */
            OSStatus orc = SetSystemUIMode (kUIModeAllHidden, kUIOptionDisableAppleMenu);
            if (orc)
                LogRel (("Error: Failed to change UI mode (rc=%#x) when changing to fullscreen mode. (=> menu bar trouble)\n", orc));
        }
#endif

        /* Adjust colors and appearance. */
        mErasePalette = centralWidget()->palette();
        QPalette palette(mErasePalette);
        palette.setColor (centralWidget()->backgroundRole(), Qt::black);
        centralWidget()->setPalette (palette);
        centralWidget()->setAutoFillBackground (true);
        console_style = console->frameStyle();
        console->setFrameStyle (QFrame::NoFrame);
        console->setMaximumSize (scrGeo.size());
        console->setHorizontalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
        console->setVerticalScrollBarPolicy (Qt::ScrollBarAlwaysOff);

        /* Going fullscreen */
        setWindowState (windowState() ^ Qt::WindowFullScreen);
#ifdef Q_WS_MAC /* setMask seems to not include the far border pixels. */
//        QRect maskRect = dtw->screenGeometry (this);
//        maskRect.setRight (maskRect.right() + 1);
//        maskRect.setBottom (maskRect.bottom() + 1);
//        setMask (maskRect);
        if (aSeamless)
        {
            OSStatus status;
            WindowPtr WindowRef = reinterpret_cast<WindowPtr>(winId());
            EventTypeSpec wNonCompositingEvent = { kEventClassWindow, kEventWindowGetRegion };
            status = InstallWindowEventHandler (WindowRef, DarwinRegionHandler, GetEventTypeCount (wNonCompositingEvent), &wNonCompositingEvent, &mCurrRegion, &mDarwinRegionEventHandlerRef);
            Assert (status == noErr);
            status = ReshapeCustomWindow (WindowRef);
            Assert (status == noErr);
            UInt32 features;
            status = GetWindowFeatures (WindowRef, &features);
            Assert (status == noErr);
            if (( features & kWindowIsOpaque ) != 0)
            {
                status = HIWindowChangeFeatures (WindowRef, 0, kWindowIsOpaque);
                Assert(status == noErr);
            }
            status = SetWindowAlpha(WindowRef, 0.999);
            Assert (status == noErr);
            /* For now disable the shadow of the window. This feature cause errors
             * if a window in vbox looses focus, is reselected and than moved. */
            /** @todo Search for an option to enable this again. A shadow on every
             * window has a big coolness factor. */
            ChangeWindowAttributes (WindowRef, kWindowNoShadowAttribute, 0);
        }
#else
//        setMask (dtw->screenGeometry (this));
#endif

        qApp->processEvents();
        console->toggleFSMode();
    }
    else
    {
        /* Temporarily disable the mode-related action to make sure
         * user can not enter the mode before he leave it. */
        aSeamless ? vmSeamlessAction->setEnabled (false) :
                    vmFullscreenAction->setEnabled (false);
        /* Toggle console to manual resize mode. */
        connect (console, SIGNAL (resizeHintDone()), this, SLOT (onExitFullscreen()));

        /* Reset the shifting spacer. */
        mShiftingSpacerLeft->changeSize (0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        mShiftingSpacerTop->changeSize (0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        mShiftingSpacerRight->changeSize (0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        mShiftingSpacerBottom->changeSize (0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);

        /* Restore the previous scroll-view minimum size before the exiting
         * fullscreen. Required for correct scroll-view and guest display
         * update in SDL mode. */
        console->setMinimumSize (prev_min_size);

#ifdef Q_WS_MAC
        if (!aSeamless)
            SetSystemUIMode (kUIModeNormal, 0);

        if (aSeamless)
        {
            /* Undo all mac specific installations */
            OSStatus status;
            WindowPtr WindowRef = reinterpret_cast<WindowPtr>(winId());
            status = RemoveEventHandler (mDarwinRegionEventHandlerRef);
            Assert (status == noErr);
            status = ReshapeCustomWindow (WindowRef);
            Assert (status == noErr);
            status = SetWindowAlpha (WindowRef, 1);
            Assert (status == noErr);
        }
#endif

        /* Adjust colors and appearance. */
        clearMask();
        centralWidget()->setPalette (mErasePalette);
        centralWidget()->setBackgroundMode (Qt::PaletteBackground);
        centralWidget()->setAutoFillBackground (false);
        console->setFrameStyle (console_style);
        console->setMaximumSize (console->sizeHint());
        console->setHorizontalScrollBarPolicy (Qt::ScrollBarAsNeeded);
        console->setVerticalScrollBarPolicy (Qt::ScrollBarAsNeeded);

        /* Show everything hidden when going fullscreen. */
        foreach (QObject *obj, hidden_children)
            ((QWidget *) obj)->show();
        hidden_children.clear();

        /* Going normal || maximized */
        setWindowState (windowState() ^ Qt::WindowFullScreen);

        qApp->processEvents();
        console->toggleFSMode();
    }

#ifdef Q_WS_MAC /* wasHidden is wrong on the mac it seems. */
    /** @todo figure out what is really wrong here... */
    if (!wasHidden)
        show();
#else
    if (wasHidden)
        hide();
#endif
    return true;
}

#ifdef Q_WS_MAC
CGImageRef VBoxConsoleWnd::dockImageState() const
{
    CGImageRef img;
    if (machine_state == KMachineState_Paused)
        img = dockImgStatePaused;
    else if (machine_state == KMachineState_Restoring)
        img = dockImgStateRestoring;
    else if (machine_state == KMachineState_Saving)
        img = dockImgStateSaving;
    else
        img = NULL;
    return img;
}
#endif

//
// Private slots
/////////////////////////////////////////////////////////////////////////////
void VBoxConsoleWnd::setViewInSeamlessMode (const QRect &aTargetRect)
{
    if (mIsSeamless)
    {
        /* It isn't guaranteed that the guest os set the video mode that
         * we requested. So after all the resizing stuff set the clipping
         * mask and the spacing shifter to the corresponding values. */
        QDesktopWidget *dtw = QApplication::desktop();
        QRect sRect = dtw->screenGeometry (this);
        QRect aRect (aTargetRect);
        mMaskShift.scale (aTargetRect.left(), aTargetRect.top(), Qt::IgnoreAspectRatio);
#ifdef Q_WS_MAC
        /* On mac os x this isn't necessary cause the screen starts
         * by y=0 always regardless if there is the global menubar or not. */
        aRect.setRect (aRect.left(), 0, aRect.width(), aRect.height() + aRect.top());
#endif // Q_WS_MAC
        /* Set the clipping mask */
        mStrictedRegion = aRect;
        /* Set the shifting spacer */
        mShiftingSpacerLeft->changeSize (RT_ABS (sRect.left() - aRect.left()), 0,
                                         QSizePolicy::Fixed, QSizePolicy::Preferred);
        mShiftingSpacerTop->changeSize (0, RT_ABS (sRect.top() - aRect.top()),
                                        QSizePolicy::Preferred, QSizePolicy::Fixed);
        mShiftingSpacerRight->changeSize (RT_ABS (sRect.right() - aRect.right()), 0,
                                          QSizePolicy::Fixed, QSizePolicy::Preferred);
        mShiftingSpacerBottom->changeSize (0, RT_ABS (sRect.bottom() - aRect.bottom()),
                                           QSizePolicy::Preferred, QSizePolicy::Fixed);
    }
}

void VBoxConsoleWnd::vmFullscreen (bool aOn)
{
    bool ok = toggleFullscreenMode (aOn, false /* aSeamless */);
    if (!ok)
    {
        /* on failure, restore the previous button state */
        vmFullscreenAction->blockSignals (true);
        vmFullscreenAction->setChecked (!aOn);
        vmFullscreenAction->blockSignals (false);
    }
}

void VBoxConsoleWnd::vmSeamless (bool aOn)
{
    /* check if it is possible to enter/leave seamless mode */
    if (mIsSeamlessSupported || !aOn)
    {
        bool ok = toggleFullscreenMode (aOn, true /* aSeamless */);
        if (!ok)
        {
            /* on failure, restore the previous button state */
            vmSeamlessAction->blockSignals (true);
            vmSeamlessAction->setChecked (!aOn);
            vmSeamlessAction->blockSignals (false);
        }
    }
}

void VBoxConsoleWnd::vmAutoresizeGuest (bool on)
{
    if (!console)
        return;

    /* Currently, we use only "off" and "on" icons. Later,
     * we may want to use disabled versions of icons when no guest additions
     * are available (to indicate that this function is ineffective). */
#if 0
    autoresize_state->setState (on ? 3 : 1);
#endif

    console->setAutoresizeGuest (on);
}

void VBoxConsoleWnd::vmAdjustWindow()
{
    if (console)
    {
        if (isMaximized())
            showNormal();
        console->normalizeGeometry (true /* adjustPosition */);
    }
}

void VBoxConsoleWnd::vmTypeCAD()
{
    if (console)
    {
        CKeyboard keyboard  = console->console().GetKeyboard();
        Assert (!keyboard.isNull());
        keyboard.PutCAD();
        AssertWrapperOk (keyboard);
    }
}

void VBoxConsoleWnd::vmTypeCABS()
{
#if defined(Q_WS_X11)
    if (console)
    {
        CKeyboard keyboard  = console->console().GetKeyboard();
        Assert (!keyboard.isNull());
        static LONG sSequence[] =
        {
            0x1d, // Ctrl down
            0x38, // Alt down
            0x0E, // Backspace down
            0x8E, // Backspace up
            0xb8, // Alt up
            0x9d  // Ctrl up
        };
        keyboard.PutScancodes (sSequence, ELEMENTS (sSequence));
        AssertWrapperOk (keyboard);
    }
#else
    /* we have to define this slot anyway because MOC doesn't understand ifdefs */
#endif
}

void VBoxConsoleWnd::vmReset()
{
    if (console)
    {
        if (vboxProblem().confirmVMReset (this))
            console->console().Reset();
    }
}

void VBoxConsoleWnd::vmPause (bool on)
{
    if (console)
    {
        console->pause (on);
        updateAppearanceOf (PauseAction);
    }
}

void VBoxConsoleWnd::vmACPIShutdown()
{
    if (console)
    {
        CConsole cconsole = console->console();
        cconsole.PowerButton();
        if (!cconsole.isOk())
            vboxProblem().cannotACPIShutdownMachine (cconsole);
    }
}

void VBoxConsoleWnd::vmClose()
{
    if (console)
    {
        close();
    }
}

void VBoxConsoleWnd::vmTakeSnapshot()
{
    AssertReturn (console, (void) 0);

    /* remember the paused state */
    bool wasPaused = machine_state == KMachineState_Paused;
    if (!wasPaused)
    {
        /* Suspend the VM and ignore the close event if failed to do so.
         * pause() will show the error message to the user. */
        if (!console->pause (true))
            return;
    }

    CMachine cmachine = csession.GetMachine();

    VBoxTakeSnapshotDlg dlg (this);

    QString typeId = cmachine.GetOSTypeId();
    dlg.mLbIcon->setPixmap (vboxGlobal().vmGuestOSTypeIcon (typeId));

    /* search for the max available filter index */
    int maxSnapShotIndex = 0;
    QString snapShotName = tr ("Snapshot %1");
    QRegExp regExp (QString ("^") + snapShotName.arg ("([0-9]+)") + QString ("$"));
    CSnapshot index = cmachine.GetSnapshot (QUuid());
    while (!index.isNull())
    {
        /* Check the current snapshot name */
        QString name = index.GetName();
        int pos = regExp.indexIn (name);
        if (pos != -1)
            maxSnapShotIndex = regExp.cap (1).toInt() > maxSnapShotIndex ?
                               regExp.cap (1).toInt() : maxSnapShotIndex;
        /* Traversing to the next child */
        index = index.GetChildren().GetItemAt (0);
    }
    dlg.mLeName->setText (snapShotName.arg (maxSnapShotIndex + 1));

    if (dlg.exec() == QDialog::Accepted)
    {
        CConsole cconsole = csession.GetConsole();

        CProgress progress =
            cconsole.TakeSnapshot (dlg.mLeName->text().trimmed(),
                                   dlg.mTeDescription->text());

        if (cconsole.isOk())
        {
            /* show the "Taking Snapshot" progress dialog */
            vboxProblem().
                showModalProgressDialog (progress, cmachine.GetName(), this, 0);

            if (progress.GetResultCode() != 0)
                vboxProblem().cannotTakeSnapshot (progress);
        }
        else
            vboxProblem().cannotTakeSnapshot (cconsole);
    }

    /* restore the running state if needed */
    if (!wasPaused)
        console->pause (false);
}

void VBoxConsoleWnd::vmShowInfoDialog()
{
    VBoxVMInformationDlg::createInformationDlg (csession, console);
}

void VBoxConsoleWnd::vmDisableMouseIntegr (bool aOff)
{
    if (console)
    {
        console->setMouseIntegrationEnabled (!aOff);
        updateAppearanceOf (DisableMouseIntegrAction);
    }
}

void VBoxConsoleWnd::devicesMountFloppyImage()
{
    if (!console) return;

    VBoxDiskImageManagerDlg dlg (this, "VBoxDiskImageManagerDlg", Qt::WType_Dialog | Qt::WShowModal);
    QUuid id = csession.GetMachine().GetId();
    dlg.setup (VBoxDefs::FD, true, &id);

    if (dlg.exec() == VBoxDiskImageManagerDlg::Accepted)
    {
        CFloppyDrive drv = csession.GetMachine().GetFloppyDrive();
        drv.MountImage (dlg.getSelectedUuid());
        AssertWrapperOk (drv);
        if (drv.isOk())
        {
            if (mIsAutoSaveMedia)
            {
                CMachine m = csession.GetMachine();
                m.SaveSettings();
                if (!m.isOk())
                    vboxProblem().cannotSaveMachineSettings (m);
            }
        }
    }
}

void VBoxConsoleWnd::devicesUnmountFloppy()
{
    if (!console) return;

    CFloppyDrive drv = csession.GetMachine().GetFloppyDrive();
    drv.Unmount();
    if (drv.isOk())
    {
        if (mIsAutoSaveMedia)
        {
            CMachine m = csession.GetMachine();
            m.SaveSettings();
            if (!m.isOk())
                vboxProblem().cannotSaveMachineSettings (m);
        }
    }
}

void VBoxConsoleWnd::devicesMountDVDImage()
{
    if (!console) return;

    VBoxDiskImageManagerDlg dlg (this, "VBoxDiskImageManagerDlg", Qt::WType_Dialog | Qt::WShowModal);
    QUuid id = csession.GetMachine().GetId();
    dlg.setup (VBoxDefs::CD, true, &id);

    if (dlg.exec() == VBoxDiskImageManagerDlg::Accepted)
    {
        CDVDDrive drv = csession.GetMachine().GetDVDDrive();
        drv.MountImage (dlg.getSelectedUuid());
        AssertWrapperOk (drv);
        if (drv.isOk())
        {
            if (mIsAutoSaveMedia)
            {
                CMachine m = csession.GetMachine();
                m.SaveSettings();
                if (!m.isOk())
                    vboxProblem().cannotSaveMachineSettings (m);
            }
        }
    }
}


void VBoxConsoleWnd::devicesUnmountDVD()
{
    if (!console) return;

    CDVDDrive drv = csession.GetMachine().GetDVDDrive();
    drv.Unmount();
    AssertWrapperOk (drv);
    if (drv.isOk())
    {
        if (mIsAutoSaveMedia)
        {
            CMachine m = csession.GetMachine();
            m.SaveSettings();
            if (!m.isOk())
                vboxProblem().cannotSaveMachineSettings (m);
        }
    }
}

void VBoxConsoleWnd::devicesSwitchVrdp (bool aOn)
{
    if (!console) return;

    CVRDPServer vrdpServer = csession.GetMachine().GetVRDPServer();
    /* this method should not be executed if vrdpServer is null */
    Assert (!vrdpServer.isNull());

    vrdpServer.SetEnabled (aOn);
    updateAppearanceOf (VRDPStuff);
}

void VBoxConsoleWnd::devicesOpenSFDialog()
{
    if (!console) return;

    VBoxSFDialog dlg (console, csession);
    dlg.exec();
}

void VBoxConsoleWnd::devicesInstallGuestAdditions()
{
#if defined (DEBUG_dmik) /* subscribe yourself here if you care for this behavior. */
    QString src1 = qApp->applicationDirPath() + "/../../release/bin/VBoxGuestAdditions.iso";
    QString src2 = qApp->applicationDirPath() + "/../../release/bin/additions/VBoxGuestAdditions.iso";
#else
    char szAppPrivPath [RTPATH_MAX];
    int rc;

    rc = RTPathAppPrivateNoArch (szAppPrivPath, sizeof (szAppPrivPath));
    Assert (RT_SUCCESS (rc));

    QString src1 = QString (szAppPrivPath) + "/VBoxGuestAdditions.iso";
    QString src2 = qApp->applicationDirPath() + "/additions/VBoxGuestAdditions.iso";
#endif

    if (QFile::exists (src1))
        installGuestAdditionsFrom (src1);
    else if (QFile::exists (src2))
        installGuestAdditionsFrom (src2);
    else
    {
        /* Check for the already registered required image: */
        CVirtualBox vbox = vboxGlobal().virtualBox();
        QString name = QString ("VBoxGuestAdditions_%1.iso")
                                 .arg (vbox.GetVersion().remove ("_OSE"));
        CDVDImageEnumerator en = vbox.GetDVDImages().Enumerate();
        while (en.HasMore())
        {
            QString path = en.GetNext().GetFilePath();
            /* compare the name part ignoring the file case*/
            QString fn = QFileInfo (path).fileName();
            if (RTPathCompare (name.toUtf8().constData(), fn.toUtf8().constData()) == 0)
                return installGuestAdditionsFrom (path);
        }
        /* Download required image: */
        int rc = vboxProblem().cannotFindGuestAdditions (
            QDir::convertSeparators (src1), QDir::convertSeparators (src2));
        if (rc == QIMessageBox::Yes)
        {
            QString url = QString ("http://www.virtualbox.org/download/%1/")
                                   .arg (vbox.GetVersion().remove ("_OSE")) + name;
            QString target = QDir (vboxGlobal().virtualBox().GetHomeFolder())
                                   .absoluteFilePath (name);

            new VBoxDownloaderWgt (statusBar(), devicesInstallGuestToolsAction,
                                   url, target);
        }
    }
}

void VBoxConsoleWnd::installGuestAdditionsFrom (const QString &aSource)
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    QUuid uuid;

    CDVDImage image = vbox.FindDVDImage (aSource);
    if (image.isNull())
    {
        image = vbox.OpenDVDImage (aSource, uuid);
        if (vbox.isOk())
            vbox.RegisterDVDImage (image);
        if (vbox.isOk())
            uuid = image.GetId();
    }
    else
        uuid = image.GetId();

    if (!vbox.isOk())
    {
        vboxProblem().cannotRegisterMedia (this, vbox, VBoxDefs::CD, aSource);
        return;
    }

    Assert (!uuid.isNull());
    CDVDDrive drv = csession.GetMachine().GetDVDDrive();
    drv.MountImage (uuid);
    /// @todo (r=dmik) use VBoxProblemReporter::cannotMountMedia...
    AssertWrapperOk (drv);
}

void VBoxConsoleWnd::setMask (const QRegion &aRegion)
{
    QRegion region = aRegion;
    /* The global mask shift cause of toolbars and such things. */
    region.translate (mMaskShift.width(), mMaskShift.height());
    /* Restrict the drawing to the available space on the screen.
     * (The &operator is better than the previous used -operator,
     * because this excludes space around the real screen also.
     * This is necessary for the mac.) */
    region &= mStrictedRegion;

#ifdef Q_WS_WIN
    QRegion difference = mPrevRegion.subtract (region);

    /* Region offset calculation */
    int fleft = 0, ftop = 0;
    /* I think this isn't necessary anymore because the 4 shifting spacer.
     * Has to be verified. */
//    if (isTopLevel())
//    {
//        ftop = topData()->ftop;
//        fleft = topData()->fleft;
//    }

    /* Visible region calculation */
    HRGN newReg = CreateRectRgn (0, 0, 0, 0);
    CombineRgn (newReg, region.handle(), 0, RGN_COPY);
    OffsetRgn (newReg, fleft, ftop);

    /* Invisible region calculation */
    HRGN diffReg = CreateRectRgn (0, 0, 0, 0);
    CombineRgn (diffReg, difference.handle(), 0, RGN_COPY);
    OffsetRgn (diffReg, fleft, ftop);

    /* Set the current visible region and clean the previous */
    SetWindowRgn (winId(), newReg, FALSE);
    RedrawWindow (NULL, NULL, diffReg, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    RedrawWindow (console->viewport()->winId(), NULL, NULL, RDW_INVALIDATE);

    mPrevRegion = region;
#elif defined(Q_WS_MAC)
# if defined(VBOX_GUI_USE_QUARTZ2D)
    if (vboxGlobal().vmRenderMode() == VBoxDefs::Quartz2DMode)
    {
        /* If we are using the Quartz2D backend we have to trigger
         * an repaint only. All the magic clipping stuff is done
         * in the paint engine. */
        repaint();
//        qApp->processEvents();
    }
    else
# endif
    {
        /* This is necessary to avoid the flicker by an mask update.
         * See http://lists.apple.com/archives/Carbon-development/2001/Apr/msg01651.html
         * for the hint.
         * There *must* be a better solution. */
        if (!region.isEmpty())
            region |= QRect (0, 0, 1, 1);
        /* Save the current region for later processing in the darwin event handler. */
        mCurrRegion = region;
        /* We repaint the screen before the ReshapeCustomWindow command. Unfortunately
         * this command flushes a copy of the backbuffer to the screen after the new
         * mask is set. This leads into a missplaced drawing of the content. Currently
         * no alternative to this and also this is not 100% perfect. */
        repaint();
        qApp->processEvents();
        /* Now force the reshaping of the window. This is definitly necessary. */
        ReshapeCustomWindow (reinterpret_cast <WindowPtr> (winId()));
    }
#else
    QMainWindow::setMask (region);
#endif
}

/**
 *  Prepares the "Mount Floppy..." menu by populating the existent host
 *  floppies.
 */
void VBoxConsoleWnd::prepareFloppyMenu()
{
    if (!console) return;

    devicesMountFloppyMenu->clear();

    CHostFloppyDrive selected = csession.GetMachine().GetFloppyDrive().GetHostDrive();

    hostFloppyMap.clear();
    CHostFloppyDriveEnumerator en =
        vboxGlobal().virtualBox().GetHost().GetFloppyDrives().Enumerate();
    while (en.HasMore())
    {
        CHostFloppyDrive hostFloppy = en.GetNext();
        /** @todo set icon */
        QString drvName = hostFloppy.GetName();
        QString description = hostFloppy.GetDescription();
        QString fullName = description.isEmpty() ?
            drvName :
            QString ("%1 (%2)").arg (description, drvName);
        int id = devicesMountFloppyMenu->insertItem (
            tr ("Host Drive ") + fullName);
        hostFloppyMap [id] = hostFloppy;
        if (machine_state != KMachineState_Running && machine_state != KMachineState_Paused)
            devicesMountFloppyMenu->setItemEnabled (id, false);
        else if (!selected.isNull())
            if (!selected.GetName().compare (hostFloppy.GetName()))
                devicesMountFloppyMenu->setItemEnabled (id, false);
    }

    if (devicesMountFloppyMenu->count() > 0)
        devicesMountFloppyMenu->addSeparator();
    devicesMountFloppyMenu->addAction (devicesMountFloppyImageAction);

    /* if shown as a context menu */
    if (devicesMenu->itemParameter (devicesMountFloppyMenuId))
    {
        devicesMountFloppyMenu->addSeparator();
        devicesMountFloppyMenu->addAction (devicesUnmountFloppyAction);
    }
}

/**
 *  Prepares the "Capture DVD..." menu by populating the existent host
 *  CD/DVD-ROM drives.
 */
void VBoxConsoleWnd::prepareDVDMenu()
{
    if (!console) return;

    devicesMountDVDMenu->clear();

    CHostDVDDrive selected = csession.GetMachine().GetDVDDrive().GetHostDrive();

    hostDVDMap.clear();
    CHostDVDDriveEnumerator en =
        vboxGlobal().virtualBox().GetHost().GetDVDDrives().Enumerate();
    while (en.HasMore())
    {
        CHostDVDDrive hostDVD = en.GetNext();
        /** @todo set icon */
        QString drvName = hostDVD.GetName();
        QString description = hostDVD.GetDescription();
        QString fullName = description.isEmpty() ?
            drvName :
            QString ("%1 (%2)").arg (description, drvName);
        int id = devicesMountDVDMenu->insertItem (
            tr ("Host Drive ") + fullName);
        hostDVDMap [id] = hostDVD;
        if (machine_state != KMachineState_Running && machine_state != KMachineState_Paused)
            devicesMountDVDMenu->setItemEnabled (id, false);
        else if (!selected.isNull())
            if (!selected.GetName().compare (hostDVD.GetName()))
                devicesMountDVDMenu->setItemEnabled (id, false);
    }

    if (devicesMountDVDMenu->count() > 0)
        devicesMountDVDMenu->addSeparator();
    devicesMountDVDMenu->addAction (devicesMountDVDImageAction);

    /* if shown as a context menu */

    if (devicesMenu->itemParameter (devicesMountDVDMenuId))
    {
        devicesMountDVDMenu->addSeparator();
        devicesMountDVDMenu->addAction (devicesUnmountDVDAction);
    }
}

/**
 *  Prepares the "Network adapter" menu by populating the existent adapters.
 */
void VBoxConsoleWnd::prepareNetworkMenu()
{
    devicesNetworkMenu->clear();
    ulong count = vboxGlobal().virtualBox().GetSystemProperties().GetNetworkAdapterCount();
    for (ulong slot = 0; slot < count; ++ slot)
    {
        CNetworkAdapter adapter = csession.GetMachine().GetNetworkAdapter (slot);
        int id = devicesNetworkMenu->insertItem (tr ("Adapter %1", "network").arg (slot));
        devicesNetworkMenu->setItemEnabled (id, adapter.GetEnabled());
        devicesNetworkMenu->setItemChecked (id, adapter.GetEnabled() && adapter.GetCableConnected());
    }
}

void VBoxConsoleWnd::setDynamicMenuItemStatusTip (int aId)
{
    QString tip;

    if (sender() == devicesMountFloppyMenu)
    {
        if (hostFloppyMap.find (aId) != hostFloppyMap.end())
            tip = tr ("Mount the selected physical drive of the host PC",
                      "Floppy tip");
    }
    else if (sender() == devicesMountDVDMenu)
    {
        if (hostDVDMap.find (aId) != hostDVDMap.end())
            tip = tr ("Mount the selected physical drive of the host PC",
                      "CD/DVD tip");
    }
    else if (sender() == devicesNetworkMenu)
    {
        tip = devicesNetworkMenu->isItemChecked (aId) ?
            tr ("Disconnect the cable from the selected virtual network adapter") :
            tr ("Connect the cable to the selected virtual network adapter");
    }

    if (!tip.isNull())
    {
        StatusTipEvent *ev = new StatusTipEvent (tip);
        QApplication::postEvent (this, ev);
        waitForStatusBarChange = true;
    }
}

void VBoxConsoleWnd::statusTipChanged (const QString & /*aMes*/)
{
    statusBarChangedInside = waitForStatusBarChange;
    waitForStatusBarChange = false;
}

void VBoxConsoleWnd::clearStatusBar()
{
    if (statusBarChangedInside)
        statusBar()->clearMessage();
}

/**
 *  Captures a floppy device corresponding to a given menu id.
 */
void VBoxConsoleWnd::captureFloppy (int aId)
{
    if (!console) return;

    CHostFloppyDrive d = hostFloppyMap [aId];
    /* if null then some other item but host drive is selected */
    if (d.isNull()) return;

    CFloppyDrive drv = csession.GetMachine().GetFloppyDrive();
    drv.CaptureHostDrive (d);
    AssertWrapperOk (drv);

    if (drv.isOk())
    {
        if (mIsAutoSaveMedia)
        {
            CMachine m = csession.GetMachine();
            m.SaveSettings();
            if (!m.isOk())
                vboxProblem().cannotSaveMachineSettings (m);
        }
    }
}

/**
 *  Captures a CD/DVD-ROM device corresponding to a given menu id.
 */
void VBoxConsoleWnd::captureDVD (int aId)
{
    if (!console) return;

    CHostDVDDrive d = hostDVDMap [aId];
    /* if null then some other item but host drive is selected */
    if (d.isNull()) return;

    CDVDDrive drv = csession.GetMachine().GetDVDDrive();
    drv.CaptureHostDrive (d);
    AssertWrapperOk (drv);

    if (drv.isOk())
    {
        if (mIsAutoSaveMedia)
        {
            CMachine m = csession.GetMachine();
            m.SaveSettings();
            if (!m.isOk())
                vboxProblem().cannotSaveMachineSettings (m);
        }
    }
}

/**
 *  Switch the cable connected/disconnected for the selected network adapter
 */
void VBoxConsoleWnd::activateNetworkMenu (int aId)
{
    ulong count = vboxGlobal().virtualBox().GetSystemProperties().GetNetworkAdapterCount();
    for (ulong slot = 0; slot < count; ++ slot)
    {
        if (aId == devicesNetworkMenu->idAt (slot))
        {
            CNetworkAdapter adapter = csession.GetMachine().GetNetworkAdapter (slot);
            bool connected = adapter.GetCableConnected();
            if (adapter.GetEnabled())
                adapter.SetCableConnected (!connected);
            break;
        }
    }
}

/**
 *  Attach/Detach selected USB Device.
 */
void VBoxConsoleWnd::switchUSB (int aId)
{
    if (!console) return;

    CConsole cconsole = csession.GetConsole();
    AssertWrapperOk (csession);

    CUSBDevice usb = devicesUSBMenu->getUSB (aId);
    /* if null then some other item but a USB device is selected */
    if (usb.isNull())
        return;

    if (devicesUSBMenu->isItemChecked (aId))
    {
        cconsole.DetachUSBDevice (usb.GetId());
        if (!cconsole.isOk())
        {
            /// @todo (r=dmik) the dialog should be either modeless
            //  or we have to pause the VM
            vboxProblem().cannotDetachUSBDevice (cconsole,
                                                 vboxGlobal().details (usb));
        }
    }
    else
    {
        cconsole.AttachUSBDevice (usb.GetId());
        if (!cconsole.isOk())
        {
            /// @todo (r=dmik) the dialog should be either modeless
            //  or we have to pause the VM
            vboxProblem().cannotAttachUSBDevice (cconsole,
                                                 vboxGlobal().details (usb));
        }
    }
}

void VBoxConsoleWnd::showIndicatorContextMenu (QIStateIndicator *ind, QContextMenuEvent *e)
{
    if (ind == cd_light)
    {
        /* set "this is a context menu" flag */
        devicesMenu->setItemParameter (devicesMountDVDMenuId, 1);
        devicesMountDVDMenu->exec (e->globalPos());
        devicesMenu->setItemParameter (devicesMountDVDMenuId, 0);
    }
    else
    if (ind == fd_light)
    {
        /* set "this is a context menu" flag */
        devicesMenu->setItemParameter (devicesMountFloppyMenuId, 1);
        devicesMountFloppyMenu->exec (e->globalPos());
        devicesMenu->setItemParameter (devicesMountFloppyMenuId, 0);
    }
    else
    if (ind == usb_light)
    {
        if (devicesUSBMenu->isEnabled())
        {
            /* set "this is a context menu" flag */
            devicesMenu->setItemParameter (devicesUSBMenuId, 1);
            devicesUSBMenu->exec (e->globalPos());
            devicesMenu->setItemParameter (devicesUSBMenuId, 0);
        }
    }
    else
    if (ind == vrdp_state)
    {
        devicesVRDPMenu->exec (e->globalPos());
    }
    else
    if (ind == autoresize_state)
    {
        vmAutoresizeMenu->exec (e->globalPos());
    }
    else
    if (ind == mouse_state)
    {
        vmDisMouseIntegrMenu->exec (e->globalPos());
    }
    else
    if (ind == sf_light)
    {
        /* Showing the context menu that always contains a single item is a
         * bit stupid; let's better execute this item's action directly. The
         * menu itself is kept just in case if we need more than one item in
         * the future. */
#if 0
        devicesSFMenu->exec (e->globalPos());
#else
        if (devicesSFDialogAction->isEnabled())
            devicesSFDialogAction->trigger();
#endif
    }
    else
    if (ind == net_light)
    {
        if (devicesNetworkMenu->isEnabled())
        {
            /* set "this is a context menu" flag */
            devicesMenu->setItemParameter (devicesNetworkMenuId, 1);
            devicesNetworkMenu->exec (e->globalPos());
            devicesMenu->setItemParameter (devicesNetworkMenuId, 0);
        }
    }
}

void VBoxConsoleWnd::updateDeviceLights()
{
    if (console) {
        CConsole &cconsole = console->console();
        int st;
        if (hd_light->state() != KDeviceActivity_Null) {
            st = cconsole.GetDeviceActivity (KDeviceType_HardDisk);
            if (hd_light->state() != st)
                hd_light->setState (st);
        }
        if (cd_light->state() != KDeviceActivity_Null) {
            st = cconsole.GetDeviceActivity (KDeviceType_DVD);
            if (cd_light->state() != st)
                cd_light->setState (st);
        }
        if (fd_light->state() != KDeviceActivity_Null) {
            st = cconsole.GetDeviceActivity (KDeviceType_Floppy);
            if (fd_light->state() != st)
                fd_light->setState (st);
        }
        if (net_light->state() != KDeviceActivity_Null) {
            st = cconsole.GetDeviceActivity (KDeviceType_Network);
            if (net_light->state() != st)
                net_light->setState (st);
        }
        if (usb_light->state() != KDeviceActivity_Null) {
            st = cconsole.GetDeviceActivity (KDeviceType_USB);
            if (usb_light->state() != st)
                usb_light->setState (st);
        }
        if (sf_light->state() != KDeviceActivity_Null) {
            st = cconsole.GetDeviceActivity (KDeviceType_SharedFolder);
            if (sf_light->state() != st)
                sf_light->setState (st);
        }
    }
}

void VBoxConsoleWnd::updateMachineState (KMachineState state)
{
    bool guruMeditation = false;

    if (console && machine_state != state)
    {
        if (state >= KMachineState_Running)
        {
            switch (state)
            {
                case KMachineState_Stuck:
                {
                    guruMeditation = true;
                    break;
                }
                case KMachineState_Paused:
                {
                    if (!vmPauseAction->isChecked())
                        vmPauseAction->setChecked (true);
                    break;
                }
                case KMachineState_Running:
                {
                    if (machine_state == KMachineState_Paused && vmPauseAction->isChecked())
                        vmPauseAction->setChecked (false);
                    break;
                }
#ifdef Q_WS_X11
                case KMachineState_Starting:
                {
                    /* The keyboard handler may wish to do some release logging
                       on startup.  Tell it that the logger is now active. */
                    doXKeyboardLogging(QX11Info::display());
                    break;
                }
#endif
                default:
                    break;
            }
        }

        bool isRunningOrPaused = state == KMachineState_Running ||
                                 state == KMachineState_Paused;

        /* enable/disable actions that are not managed by
         * updateAppearanceOf() */

        mRunningActions->setEnabled (state == KMachineState_Running);
        mRunningOrPausedActions->setEnabled (isRunningOrPaused);

        machine_state = state;

        updateAppearanceOf (Caption | FloppyStuff | DVDStuff | NetworkStuff |
                            USBStuff | VRDPStuff | PauseAction |
                            DisableMouseIntegrAction);

        if (state < KMachineState_Running)
        {
            /*
             *  VM has been powered off or saved or aborted, no matter
             *  internally or externally -- we must *safely* close the console
             *  window unless auto closure is disabled.
             */
            if (!no_auto_close)
                tryClose();
        }
    }

    if (guruMeditation)
    {
        CConsole cconsole = console->console();
        QString logFolder = cconsole.GetMachine().GetLogFolder();

        /* Take the screenshot for debugging purposes and save it */
        QString fname = logFolder + "/VBox.png";

        /// @todo for some reason, IDisplay::takeScreenShot() may not work
        /// properly on a VM which is Stuck -- investigate it.
        CDisplay dsp = cconsole.GetDisplay();
        QImage shot = QImage (dsp.GetWidth(), dsp.GetHeight(), QImage::Format_RGB32);
        dsp.TakeScreenShot (shot.bits(), shot.width(), shot.height());
        shot.save (QFile::encodeName (fname), "PNG");

        if (vboxProblem().remindAboutGuruMeditation (
                cconsole, QDir::convertSeparators (logFolder)))
        {
            cconsole.PowerDown();
            if (!cconsole.isOk())
                vboxProblem().cannotStopMachine (cconsole);
        }
    }

#ifdef Q_WS_MAC
    CGImageRef img = dockImageState();
    if (img)
        OverlayApplicationDockTileImage (img);
#endif
}

void VBoxConsoleWnd::updateMouseState (int state)
{
    vmDisableMouseIntegrAction->setEnabled (state & VBoxConsoleView::MouseAbsolute);

    if ((state & VBoxConsoleView::MouseAbsoluteDisabled) &&
        (state & VBoxConsoleView::MouseAbsolute) &&
        !(state & VBoxConsoleView::MouseCaptured))
    {
        mouse_state->setState (4);
    }
    else
    {
        mouse_state->setState (state & (VBoxConsoleView::MouseAbsolute |
                                        VBoxConsoleView::MouseCaptured));
    }
}


void VBoxConsoleWnd::updateAdditionsState (const QString &aVersion,
                                           bool aActive,
                                           bool aSeamlessSupported)
{
    vmAutoresizeGuestAction->setEnabled (aActive);
    if (mIsSeamlessSupported != aSeamlessSupported)
    {
        vmSeamlessAction->setEnabled (aSeamlessSupported);
        mIsSeamlessSupported = aSeamlessSupported;
        /* If seamless mode should be enabled then check if it is enabled
         * currently and re-enable it if open-view procedure is finished */
        if (vmSeamlessAction->isOn() && mIsOpenViewFinished && aSeamlessSupported)
            toggleFullscreenMode (true, true);
    }

    /* Check the GA version only in case of additions are active */
    if (!aActive)
        return;

    /* Check the Guest Additions version and warn the user about possible
     * compatibility issues in case if the installed version is outdated. */
    uint version = aVersion.toUInt();
    QString verisonStr = QString ("%1.%2")
        .arg (RT_HIWORD (version)).arg (RT_LOWORD (version));
    QString expectedStr = QString ("%1.%2")
        .arg (VMMDEV_VERSION_MAJOR).arg (VMMDEV_VERSION_MINOR);

    if (RT_HIWORD (version) < VMMDEV_VERSION_MAJOR)
    {
        vboxProblem().warnAboutTooOldAdditions (this, verisonStr, expectedStr);
    }
    else if (RT_HIWORD (version) == VMMDEV_VERSION_MAJOR &&
             RT_LOWORD (version) <  VMMDEV_VERSION_MINOR)
    {
        vboxProblem().warnAboutOldAdditions (this, verisonStr, expectedStr);
    }
    else if (version > VMMDEV_VERSION)
    {
        vboxProblem().warnAboutNewAdditions (this, verisonStr, expectedStr);
    }
}

void VBoxConsoleWnd::updateMediaState (VBoxDefs::DiskType aType)
{
    Assert (aType == VBoxDefs::CD || aType == VBoxDefs::FD);
    updateAppearanceOf (aType == VBoxDefs::CD ? DVDStuff :
                        aType == VBoxDefs::FD ? FloppyStuff : AllStuff);
}

void VBoxConsoleWnd::updateSharedFoldersState()
{
    updateAppearanceOf (SharedFolderStuff);
}

void VBoxConsoleWnd::updateUsbState()
{
    updateAppearanceOf (USBStuff);
}

void VBoxConsoleWnd::updateNetworkAdarptersState()
{
    updateAppearanceOf (NetworkStuff);
}

/**
 *  Helper to safely close the main console window.
 *
 *  This method ensures that close() will not be called if there is some
 *  modal widget currently being executed, as it can cause uninitialization
 *  at the point of code where it is not expected at all (example:
 *  VBoxConsoleView::mouseEvent() calling
 *  VBoxProblemReporter::confirmInputCapture()). Instead, an attempt to
 *  close the current modal widget is done and tryClose() is rescheduled for
 *  later execution using a single-shot zero timer.
 *
 *  In particular, this method is used by updateMachineState() when the VM
 *  goes offline, which can even happen if we are inside the modal event loop,
 *  (for example, the VM has been externally powered off or the guest OS
 *  has initiated a shutdown procedure).
 */
void VBoxConsoleWnd::tryClose()
{
#warning port me
//    LogFlowFunc (("eventLoopLevel=%d\n", qApp->eventLoop()->loopLevel()));
//
//    if (qApp->eventLoop()->loopLevel() > 1)
//    {
//        if (QApplication::activeModalWidget())
//            QApplication::activeModalWidget()->close();
//        else if (QApplication::activePopupWidget())
//            QApplication::activePopupWidget()->close();
//        else
//        {
            /// @todo (r=dmik) in general, the following is not that correct
            //  because some custom modal event loop may not expect to be
            //  exited externally (e.g., it might want to set some internal
            //  flags before calling exitLoop()). The alternative is to do
            //  nothing but wait keeping to post singleShot timers.
//            qApp->eventLoop()->exitLoop();
//        }
//
//        QTimer::singleShot (0, this, SLOT (tryClose()));
//    }
//    else
//        close();
}

/**
 *  Called (on non-UI thread!) when a global GUI setting changes.
 */
void VBoxConsoleWnd::processGlobalSettingChange (const char * /*publicName*/,
                                                 const char * /*name*/)
{
    hostkey_name->setText (QIHotKeyEdit::keyName (vboxGlobal().settings().hostKey()));
}

/**
 * Called when the Debug->Statistics... menu item is selected.
 */
void VBoxConsoleWnd::dbgShowStatistics()
{
#ifdef VBOX_WITH_DEBUGGER_GUI
    if (dbgCreated())
        DBGGuiShowStatistics (dbg_gui);
#endif
}

/**
 * Called when the Debug->Command Line... menu item is selected.
 */
void VBoxConsoleWnd::dbgShowCommandLine()
{
#ifdef VBOX_WITH_DEBUGGER_GUI
    if (dbgCreated())
        DBGGuiShowCommandLine (dbg_gui);
#endif
}

#ifdef VBOX_WITH_DEBUGGER_GUI

/**
 * Ensures that the debugger GUI instance is ready.
 *
 * @returns true if instance is fine and dandy.
 * @returns flase if it's not.
 */
bool VBoxConsoleWnd::dbgCreated()
{
    if (dbg_gui)
        return true;
    int rc = DBGGuiCreate (csession.iface(), &dbg_gui);
    if (VBOX_SUCCESS (rc))
    {
        dbgAdjustRelativePos();
        return true;
    }
    return false;
}

/**
 * Destroys the debugger GUI instacne if it has been created.
 */
void VBoxConsoleWnd::dbgDestroy()
{
    if (dbg_gui)
    {
        DBGGuiDestroy (dbg_gui);
        dbg_gui = NULL;
    }
}

/**
 * Tells the debugger GUI that the console window has moved or been resized.
 */
void VBoxConsoleWnd::dbgAdjustRelativePos()
{
    if (dbg_gui)
    {
        QRect rct = frameGeometry();
        DBGGuiAdjustRelativePos (dbg_gui, rct.x(), rct.y(), rct.width(), rct.height());
    }
}

#endif

VBoxSFDialog::VBoxSFDialog (QWidget *aParent, CSession &aSession)
    : QDialog (aParent)
    , mSettings (0)
    , mSession (aSession)
{
    setModal (true);
    /* Setup Dialog's options */
    setWindowTitle (tr ("Shared Folders"));
    setWindowIcon (QIcon (":/select_file_16px.png"));
    setSizeGripEnabled (true);

    /* Setup main dialog's layout */
    QVBoxLayout *mainLayout = new QVBoxLayout (this);
    VBoxGlobal::setLayoutMargin (mainLayout, 10);
    mainLayout->setSpacing (10);

    /* Setup settings layout */
    mSettings = new VBoxSharedFoldersSettings (this, MachineType | ConsoleType);
    mSettings->getFromConsole (aSession.GetConsole());
    mSettings->getFromMachine (aSession.GetMachine());
    mainLayout->addWidget (mSettings);

    /* Setup button's layout */
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing (10);
    mainLayout->addLayout (buttonLayout);
    QPushButton *pbHelp = new QPushButton (tr ("Help"));
    QSpacerItem *spacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    QPushButton *pbOk = new QPushButton (tr ("&OK"));
    QPushButton *pbCancel = new QPushButton (tr ("Cancel"));
    connect (pbHelp, SIGNAL (clicked()), &vboxProblem(), SLOT (showHelpHelpDialog()));
    connect (pbOk, SIGNAL (clicked()), this, SLOT (accept()));
    connect (pbCancel, SIGNAL (clicked()), this, SLOT (reject()));
    pbHelp->setShortcut (Qt::Key_F1);
    buttonLayout->addWidget (pbHelp);
    buttonLayout->addItem (spacer);
    buttonLayout->addWidget (pbOk);
    buttonLayout->addWidget (pbCancel);

    /* Setup the default push button */
    pbOk->setAutoDefault (true);
    pbOk->setDefault (true);
}

void VBoxSFDialog::accept()
{
    mSettings->putBackToConsole();
    mSettings->putBackToMachine();
    CMachine machine = mSession.GetMachine();
    machine.SaveSettings();
    if (!machine.isOk())
        vboxProblem().cannotSaveMachineSettings (machine);
    QDialog::accept();
}

void VBoxSFDialog::showEvent (QShowEvent *aEvent)
{
    resize (450, 300);
    VBoxGlobal::centerWidget (this, parentWidget());
    setMinimumWidth (400);
    QDialog::showEvent (aEvent);
}
