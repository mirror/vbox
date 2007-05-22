/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxConsoleWnd class implementation
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#include "VBoxGlobal.h"

#include "VBoxConsoleWnd.h"

#include "VBoxDefs.h"
#include "VBoxConsoleView.h"
#include "VBoxProblemReporter.h"
#include "VBoxCloseVMDlg.h"
#include "VBoxTakeSnapshotDlg.h"
#include "VBoxDiskImageManagerDlg.h"
#include "VBoxSharedFoldersSettings.h"
#include "VBoxUtils.h"
#include "QIStateIndicator.h"
#include "QIStatusBar.h"
#include "QIHotKeyEdit.h"

#include <qlabel.h>
#include <qaction.h>
#include <qmenubar.h>
#include <qpopupmenu.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include <qtooltip.h>
#include <qlineedit.h>
#include <qtextedit.h>
#include <qtooltip.h>
#include <qdir.h>
#include <qpushbutton.h>

#include <qeventloop.h>

#include <qlayout.h>
#include <qhbox.h>

#include <qtimer.h>

#include <VBox/VBoxGuest.h>

#if defined(Q_WS_X11)
#include <X11/Xlib.h>
#endif

#ifdef VBOX_WITH_DEBUGGER_GUI
#include <VBox/err.h>
#endif

/** class VBoxUSBLedTip
 *
 *  The VBoxUSBLedTip class is an auxiliary ToolTip class
 *  for the USB LED indicator.
 */
class VBoxUSBLedTip : public QToolTip
{
public:

    VBoxUSBLedTip (QWidget *aWidget, const CConsole &aConsole, bool aUSBEnabled) :
        QToolTip (aWidget), mConsole (aConsole), mUSBEnabled (aUSBEnabled) {}

    ~VBoxUSBLedTip() { remove (parentWidget()); }

    bool isUSBEnabled() const { return mUSBEnabled; }

protected:

    void maybeTip (const QPoint &/* aPoint */)
    {
        QString toolTip = VBoxConsoleWnd::tr (
            "<qt>Indicates&nbsp;the&nbsp;activity&nbsp;of&nbsp;"
            "attached&nbsp;USB&nbsp;devices<br>"
            "%1</qt>",
            "USB device indicator");

        QString devices;

        if (mUSBEnabled)
        {
            CUSBDeviceEnumerator en = mConsole.GetUSBDevices().Enumerate();
            while (en.HasMore())
            {
                CUSBDevice usb = en.GetNext();
                devices += QString ("[<b><nobr>%1</nobr></b>]<br>")
                                    .arg (vboxGlobal().details (usb));
            }
            if (devices.isNull())
                devices = VBoxConsoleWnd::tr ("<nobr>[<b>not attached</b>]</nobr>",
                                              "USB device indicator");
        }
        else
            devices = VBoxConsoleWnd::tr ("<nobr>[<b>USB Controller is disabled</b>]</nobr>",
                                          "USB device indicator");

        tip (parentWidget()->rect(), toolTip.arg (devices));
    }

private:

    CConsole mConsole;
    bool mUSBEnabled;
};

/** class VBoxNetworkLedTip
 *
 *  The VBoxNetworkLedTip class is an auxiliary ToolTip class
 *  for the Network LED indicator.
 */
class VBoxNetworkLedTip : public QToolTip
{
public:

    VBoxNetworkLedTip (QWidget *aWidget, const CMachine &aMachine) :
        QToolTip (aWidget), mMachine (aMachine) {}

    ~VBoxNetworkLedTip() { remove (parentWidget()); }

protected:

    void maybeTip (const QPoint &/* aPoint */)
    {
        QString toolTip = VBoxConsoleWnd::tr (
            "<qt>Indicates&nbsp;the&nbsp;activity&nbsp;of&nbsp;the&nbsp;network&nbsp;interfaces",
            "Network adapters indicator"
        );

        QString devices;

        ulong count = vboxGlobal().virtualBox().GetSystemProperties().GetNetworkAdapterCount();
        for (ulong slot = 0; slot < count; ++ slot)
        {
            CNetworkAdapter adapter = mMachine.GetNetworkAdapter (slot);
            if (adapter.GetEnabled())
                devices += VBoxConsoleWnd::tr ("<br><nobr><b>Adapter %1 (%2)</b>: cable %3</nobr>",
                                               "Network adapters indicator")
                    .arg (slot)
                    .arg (vboxGlobal().toString (adapter.GetAttachmentType()))
                    .arg (adapter.GetCableConnected() ?
                          VBoxConsoleWnd::tr ("connected", "Network adapters indicator") :
                          VBoxConsoleWnd::tr ("disconnected", "Network adapters indicator"));
        }

        if (devices.isNull())
            devices = VBoxConsoleWnd::tr ("<br><nobr><b>All network adapters are disabled</b></nobr>",
                                          "Network adapters indicator");

        tip (parentWidget()->rect(), toolTip + devices);
    }

private:

    CMachine mMachine;
};

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
VBoxConsoleWnd (VBoxConsoleWnd **aSelf, QWidget* aParent, const char* aName,
                WFlags aFlags)
    : QMainWindow (aParent, aName, aFlags)
#ifdef VBOX_WITH_DEBUGGER_GUI
    , dbgStatisticsAction (NULL)
    , dbgCommandLineAction (NULL)
    , dbgMenu (NULL)
#endif
    , console (0)
    , mUsbLedTip (0)
    , mNetworkLedTip (0)
    , machine_state (CEnums::InvalidMachineState)
    , no_auto_close (false)
    , full_screen (false)
    , normal_wflags (getWFlags())
    , was_max (false)
    , console_style (0)
#ifdef VBOX_WITH_DEBUGGER_GUI
    , dbg_gui (NULL)
#endif
#ifdef Q_WS_MAC
    , dockImgStateRunning (NULL)
    , dockImgStatePaused (NULL)
    , dockImgStateSaving (NULL)
    , dockImgStateRestoring (NULL)
    , dockImgBack75x75 (NULL)
    , dockImgBack100x75 (NULL)
    , dockImgOS (NULL)
#endif
{
    if (aSelf)
        *aSelf = this;

    idle_timer = new QTimer (this);

    /* application icon */
    setIcon( QPixmap::fromMimeSource( "ico40x01.png" ) );

    /* ensure status bar is created */
    new QIStatusBar (this, "statusBar");

    ///// Actions ///////////////////////////////////////////////////////////

    /*
     *  a group for all actions that are enabled only when the VM is running.
     *  note that only actions whose enabled state depends exclusively on the
     *  running state of the VM are added to this group.
     */
    runningActions = new QActionGroup (this);
    runningActions->setExclusive (false);

    /* VM menu actions */

    vmFullscreenAction = new QAction (this, "vmFullscreenAction");
    vmFullscreenAction->setIconSet (
        VBoxGlobal::iconSet ("fullscreen_16px.png", "fullscreen_disabled_16px.png"));
    vmFullscreenAction->setToggleAction (true);

    vmAutoresizeGuestAction = new QAction (runningActions, "vmAutoresizeGuestAction");
    vmAutoresizeGuestAction->setIconSet (
        VBoxGlobal::iconSet ("auto_resize_on_16px.png", "auto_resize_on_disabled_16px.png"));
    vmAutoresizeGuestAction->setToggleAction (true);
    vmAutoresizeGuestAction->setEnabled (false);

    vmAdjustWindowAction = new QAction (this, "vmAdjustWindowAction");
    vmAdjustWindowAction->setIconSet (
        VBoxGlobal::iconSet ("adjust_win_size_16px.png",
                             "adjust_win_size_disabled_16px.png"));

    vmTypeCADAction = new QAction (runningActions, "vmTypeCADAction");
    vmTypeCADAction->setIconSet (VBoxGlobal::iconSet ("hostkey_16px.png",
                                                      "hostkey_disabled_16px.png"));

#if defined(Q_WS_X11)
    vmTypeCABSAction = new QAction (runningActions, "vmTypeCABSAction");
    vmTypeCABSAction->setIconSet (VBoxGlobal::iconSet ("hostkey_16px.png",
                                                       "hostkey_disabled_16px.png"));
#endif

    vmResetAction = new QAction (runningActions, "vmResetAction");
    vmResetAction->setIconSet (VBoxGlobal::iconSet ("reset_16px.png",
                                           "reset_disabled_16px.png"));

    vmPauseAction = new QAction( this, "vmPauseAction" );
    vmPauseAction->setIconSet (VBoxGlobal::iconSet ("pause_16px.png"));
    vmPauseAction->setToggleAction( true );

    vmACPIShutdownAction = new QAction (this, "vmACPIShutdownAction");
    vmACPIShutdownAction->setIconSet (
        VBoxGlobal::iconSet ("acpi_16px.png", "acpi_disabled_16px.png"));

    vmCloseAction = new QAction (this, "vmCloseAction");
    vmCloseAction->setIconSet (VBoxGlobal::iconSet ("exit_16px.png"));

    vmTakeSnapshotAction = new QAction (this, "vmTakeSnapshotAction");
    vmTakeSnapshotAction->setIconSet (VBoxGlobal::iconSet (
        "take_snapshot_16px.png", "take_snapshot_dis_16px.png"));

    vmDisableMouseIntegrAction = new QAction (this, "vmDisableMouseIntegrAction");
    vmDisableMouseIntegrAction->setIconSet (VBoxGlobal::iconSet (
        "mouse_can_seamless_16px.png", "mouse_can_seamless_disabled_16px.png"));
    vmDisableMouseIntegrAction->setToggleAction (true);

    /* Devices menu actions */

    devicesMountFloppyImageAction = new QAction (runningActions, "devicesMountFloppyImageAction");

    devicesUnmountFloppyAction = new QAction (this, "devicesUnmountFloppyAction");
    devicesUnmountFloppyAction->setIconSet (VBoxGlobal::iconSet ("fd_unmount_16px.png",
                                                        "fd_unmount_dis_16px.png"));

    devicesMountDVDImageAction = new QAction (runningActions, "devicesMountISOImageAction");

    devicesUnmountDVDAction = new QAction (this, "devicesUnmountDVDAction");
    devicesUnmountDVDAction->setIconSet (VBoxGlobal::iconSet ("cd_unmount_16px.png",
                                                     "cd_unmount_dis_16px.png"));

    devicesSFDialogAction = new QAction (runningActions, "devicesSFDialogAction");
    devicesSFDialogAction->setToggleAction (true);
    devicesSFDialogAction->setIconSet (VBoxGlobal::iconSet ("shared_folder_16px.png",
                                                            "shared_folder_disabled_16px.png"));

    devicesSwitchVrdpAction = new QAction (runningActions, "devicesSwitchVrdpAction");
    devicesSwitchVrdpAction->setIconSet (VBoxGlobal::iconSet ("vrdp_16px.png",
                                                              "vrdp_disabled_16px.png"));
    devicesSwitchVrdpAction->setToggleAction (true);

    devicesInstallGuestToolsAction = new QAction (runningActions, "devicesInstallGuestToolsAction");
    devicesInstallGuestToolsAction->setIconSet (VBoxGlobal::iconSet ("guesttools_16px.png",
                                                                     "guesttools_disabled_16px.png"));

#ifdef VBOX_WITH_DEBUGGER_GUI
    if (vboxGlobal().isDebuggerEnabled())
    {
        dbgStatisticsAction = new QAction (this, "dbgStatisticsAction");
        dbgCommandLineAction = new QAction (this, "dbgCommandLineAction");
    }
    else
    {
        dbgStatisticsAction = NULL;
        dbgCommandLineAction = NULL;
    }
#endif

    /* Help menu actions */

    helpWebAction = new QAction( this, "helpWebAction" );
    helpWebAction->setIconSet (VBoxGlobal::iconSet ("site_16px.png"));
    helpAboutAction = new QAction( this, "helpAboutAction" );
    helpAboutAction->setIconSet (VBoxGlobal::iconSet ("about_16px.png"));
    helpResetMessagesAction = new QAction( this, "helpResetMessagesAction" );
    helpResetMessagesAction->setIconSet (VBoxGlobal::iconSet ("reset_16px.png"));

    ///// Menubar ///////////////////////////////////////////////////////////

    /* VM popup menu */

    QPopupMenu *vmMenu = new QPopupMenu (this, "vmMenu");
    vmFullscreenAction->addTo (vmMenu);
    vmAdjustWindowAction->addTo (vmMenu);
    vmAutoresizeGuestAction->addTo (vmMenu);
    vmMenu->insertSeparator();
    vmDisableMouseIntegrAction->addTo (vmMenu);
    vmMenu->insertSeparator();
    vmTypeCADAction->addTo (vmMenu);
#if defined(Q_WS_X11)
    vmTypeCABSAction->addTo (vmMenu);
#endif
    vmMenu->insertSeparator();
    vmTakeSnapshotAction->addTo (vmMenu);
    vmMenu->insertSeparator();
    vmResetAction->addTo (vmMenu);
    vmPauseAction->addTo (vmMenu);
    vmACPIShutdownAction->addTo (vmMenu);
    vmMenu->insertSeparator();
    vmCloseAction->addTo (vmMenu);
    menuBar()->insertItem (QString::null, vmMenu, vmMenuId);
    vmAutoresizeMenu = new VBoxSwitchMenu (vmMenu, vmAutoresizeGuestAction);
    vmDisMouseIntegrMenu = new VBoxSwitchMenu (vmMenu, vmDisableMouseIntegrAction,
                                               true /* inverted toggle state */);

    /* Devices popup menu */

    devicesMenu = new QPopupMenu (this, "devicesMenu");

    /* two dynamic submenus */
    devicesMountFloppyMenu = new QPopupMenu (devicesMenu, "devicesMountFloppyMenu");
    devicesMountDVDMenu = new QPopupMenu (devicesMenu, "devicesMountDVDMenu");

    devicesSharedFolders = new QPopupMenu (devicesMenu, "devicesSharedFolders");
    devicesNetworkMenu = new QPopupMenu (devicesMenu, "devicesNetworkMenu");
    devicesUSBMenu = new VBoxUSBMenu (devicesMenu);
    devicesVRDPMenu = new VBoxSwitchMenu (devicesMenu, devicesSwitchVrdpAction);

    devicesMenu->insertItem (VBoxGlobal::iconSet ("fd_16px.png", "fd_disabled_16px.png"),
        QString::null, devicesMountFloppyMenu, devicesMountFloppyMenuId);
    devicesUnmountFloppyAction->addTo (devicesMenu);
    devicesMenu->insertSeparator();
    devicesMenu->insertItem (VBoxGlobal::iconSet ("cd_16px.png", "cd_disabled_16px.png"),
        QString::null, devicesMountDVDMenu, devicesMountDVDMenuId);
    devicesUnmountDVDAction->addTo (devicesMenu);
    devicesMenu->insertSeparator();
    devicesMenu->insertItem (VBoxGlobal::iconSet ("usb_16px.png", "usb_disabled_16px.png"),
        QString::null, devicesUSBMenu, devicesUSBMenuId);
    devicesUSBMenuSeparatorId = devicesMenu->insertSeparator();
    devicesSFDialogAction->addTo (devicesMenu);
    devicesSFMenuSeparatorId = devicesMenu->insertSeparator();
    devicesMenu->insertItem (VBoxGlobal::iconSet ("nw_16px.png", "nw_disabled_16px.png"),
        QString::null, devicesNetworkMenu, devicesNetworkMenuId);
    devicesMenu->insertSeparator();
    devicesSwitchVrdpAction->addTo (devicesMenu);
    devicesVRDPMenuSeparatorId = devicesMenu->insertSeparator();
    devicesInstallGuestToolsAction->addTo (devicesMenu);
    menuBar()->insertItem (QString::null, devicesMenu, devicesMenuId);

    /* reset the "context menu" flag */
    devicesMenu->setItemParameter (devicesMountFloppyMenuId, 0);
    devicesMenu->setItemParameter (devicesMountDVDMenuId, 0);
    devicesMenu->setItemParameter (devicesUSBMenuId, 0);

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* Debug popup menu */
    if (vboxGlobal().isDebuggerEnabled())
    {
        dbgMenu = new QPopupMenu (this, "dbgMenu");
        dbgStatisticsAction->addTo (dbgMenu);
        dbgCommandLineAction->addTo (dbgMenu);
        menuBar()->insertItem (QString::null, dbgMenu, dbgMenuId);
    }
    else
        dbgMenu = NULL;
#endif

    /* Help popup menu */

    QPopupMenu *helpMenu = new QPopupMenu( this, "helpMenu" );
    helpWebAction->addTo( helpMenu );
    helpMenu->insertSeparator();
    helpAboutAction->addTo( helpMenu );
    helpMenu->insertSeparator();
    helpResetMessagesAction->addTo (helpMenu);
    menuBar()->insertItem( QString::null, helpMenu, helpMenuId );

    ///// Status bar ////////////////////////////////////////////////////////

    QHBox *indicatorBox = new QHBox (0, "indicatorBox");
    indicatorBox->setSpacing (5);
    /* i/o devices */
    hd_light = new QIStateIndicator (CEnums::DeviceIdle, indicatorBox, "hd_light", WNoAutoErase);
    hd_light->setStateIcon (CEnums::DeviceIdle, QPixmap::fromMimeSource ("hd_16px.png"));
    hd_light->setStateIcon (CEnums::DeviceReading, QPixmap::fromMimeSource ("hd_read_16px.png"));
    hd_light->setStateIcon (CEnums::DeviceWriting, QPixmap::fromMimeSource ("hd_write_16px.png"));
    hd_light->setStateIcon (CEnums::InvalidActivity, QPixmap::fromMimeSource ("hd_disabled_16px.png"));
    cd_light = new QIStateIndicator (CEnums::DeviceIdle, indicatorBox, "cd_light", WNoAutoErase);
    cd_light->setStateIcon (CEnums::DeviceIdle, QPixmap::fromMimeSource ("cd_16px.png"));
    cd_light->setStateIcon (CEnums::DeviceReading, QPixmap::fromMimeSource ("cd_read_16px.png"));
    cd_light->setStateIcon (CEnums::DeviceWriting, QPixmap::fromMimeSource ("cd_write_16px.png"));
    cd_light->setStateIcon (CEnums::InvalidActivity, QPixmap::fromMimeSource ("cd_disabled_16px.png"));
    fd_light = new QIStateIndicator (CEnums::DeviceIdle, indicatorBox, "fd_light", WNoAutoErase);
    fd_light->setStateIcon (CEnums::DeviceIdle, QPixmap::fromMimeSource ("fd_16px.png"));
    fd_light->setStateIcon (CEnums::DeviceReading, QPixmap::fromMimeSource ("fd_read_16px.png"));
    fd_light->setStateIcon (CEnums::DeviceWriting, QPixmap::fromMimeSource ("fd_write_16px.png"));
    fd_light->setStateIcon (CEnums::InvalidActivity, QPixmap::fromMimeSource ("fd_disabled_16px.png"));
    net_light = new QIStateIndicator (CEnums::DeviceIdle, indicatorBox, "net_light", WNoAutoErase);
    net_light->setStateIcon (CEnums::DeviceIdle, QPixmap::fromMimeSource ("nw_16px.png"));
    net_light->setStateIcon (CEnums::DeviceReading, QPixmap::fromMimeSource ("nw_read_16px.png"));
    net_light->setStateIcon (CEnums::DeviceWriting, QPixmap::fromMimeSource ("nw_write_16px.png"));
    net_light->setStateIcon (CEnums::InvalidActivity, QPixmap::fromMimeSource ("nw_disabled_16px.png"));
    usb_light = new QIStateIndicator (CEnums::DeviceIdle, indicatorBox, "usb_light", WNoAutoErase);
    usb_light->setStateIcon (CEnums::DeviceIdle, QPixmap::fromMimeSource ("usb_16px.png"));
    usb_light->setStateIcon (CEnums::DeviceReading, QPixmap::fromMimeSource ("usb_read_16px.png"));
    usb_light->setStateIcon (CEnums::DeviceWriting, QPixmap::fromMimeSource ("usb_write_16px.png"));
    usb_light->setStateIcon (CEnums::InvalidActivity, QPixmap::fromMimeSource ("usb_disabled_16px.png"));
    /// @todo add proper read/write states for shared folders when it is implemented
    sf_state = new QIStateIndicator (0, indicatorBox, "sf_state", WNoAutoErase);
    sf_state->setStateIcon (0, QPixmap::fromMimeSource ("shared_folder_16px.png"));
    sf_state->setStateIcon (1, QPixmap::fromMimeSource ("shared_folder_disabled_16px.png"));

    (new QFrame (indicatorBox))->setFrameStyle (QFrame::VLine | QFrame::Sunken);

#if 0 // do not show these indicators, information overload
    /* vrdp state */
    vrdp_state = new QIStateIndicator (0, indicatorBox, "vrdp_state", WNoAutoErase);
    vrdp_state->setStateIcon (0, QPixmap::fromMimeSource ("vrdp_disabled_16px.png"));
    vrdp_state->setStateIcon (1, QPixmap::fromMimeSource ("vrdp_16px.png"));
    /* auto resize state */
    autoresize_state = new QIStateIndicator (1, indicatorBox, "autoresize_state", WNoAutoErase);
    autoresize_state->setStateIcon (0, QPixmap::fromMimeSource ("auto_resize_off_disabled_16px.png"));
    autoresize_state->setStateIcon (1, QPixmap::fromMimeSource ("auto_resize_off_16px.png"));
    autoresize_state->setStateIcon (2, QPixmap::fromMimeSource ("auto_resize_on_disabled_16px.png"));
    autoresize_state->setStateIcon (3, QPixmap::fromMimeSource ("auto_resize_on_16px.png"));
#endif

    /* mouse */
    mouse_state = new QIStateIndicator (0, indicatorBox, "mouse_state", WNoAutoErase);
    mouse_state->setStateIcon (0, QPixmap::fromMimeSource ("mouse_disabled_16px.png"));
    mouse_state->setStateIcon (1, QPixmap::fromMimeSource ("mouse_16px.png"));
    mouse_state->setStateIcon (2, QPixmap::fromMimeSource ("mouse_seamless_16px.png"));
    mouse_state->setStateIcon (3, QPixmap::fromMimeSource ("mouse_can_seamless_16px.png"));
    mouse_state->setStateIcon (4, QPixmap::fromMimeSource ("mouse_can_seamless_uncaptured_16px.png"));
    /* host key */
    hostkey_hbox = new QHBox (indicatorBox, "hostkey_hbox");
    hostkey_hbox->setSpacing (3);
    hostkey_state = new QIStateIndicator (0, hostkey_hbox, "hostkey_state");
    hostkey_state->setStateIcon (0, QPixmap::fromMimeSource ("hostkey_16px.png"));
    hostkey_state->setStateIcon (1, QPixmap::fromMimeSource ("hostkey_captured_16px.png"));
    hostkey_state->setStateIcon (2, QPixmap::fromMimeSource ("hostkey_pressed_16px.png"));
    hostkey_state->setStateIcon (3, QPixmap::fromMimeSource ("hostkey_captured_pressed_16px.png"));
    hostkey_name = new QLabel (QIHotKeyEdit::keyName (vboxGlobal().settings().hostKey()),
                               hostkey_hbox, "hostkey_name");
    /* add to statusbar */
    statusBar()->addWidget (indicatorBox, 0, true);

    /////////////////////////////////////////////////////////////////////////

    languageChange();

    setCaption (caption_prefix);

    ///// Connections ///////////////////////////////////////////////////////

    connect (vmFullscreenAction, SIGNAL (toggled (bool)),
             this, SLOT (vmFullscreen (bool)));
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

    connect (vmDisableMouseIntegrAction, SIGNAL(toggled (bool)), this, SLOT(vmDisableMouseIntegr (bool)));

    connect (devicesMountFloppyImageAction, SIGNAL(activated()), this, SLOT(devicesMountFloppyImage()));
    connect (devicesUnmountFloppyAction, SIGNAL(activated()), this, SLOT(devicesUnmountFloppy()));
    connect (devicesMountDVDImageAction, SIGNAL(activated()), this, SLOT(devicesMountDVDImage()));
    connect (devicesUnmountDVDAction, SIGNAL(activated()), this, SLOT(devicesUnmountDVD()));
    connect (devicesSwitchVrdpAction, SIGNAL(toggled (bool)), this, SLOT(devicesSwitchVrdp (bool)));
    connect (devicesSFDialogAction, SIGNAL(toggled (bool)), this, SLOT(devicesToggleSFDialog (bool)));
    connect (devicesInstallGuestToolsAction, SIGNAL(activated()), this, SLOT(devicesInstallGuestAdditions()));


    connect (devicesMountFloppyMenu, SIGNAL(aboutToShow()), this, SLOT(prepareFloppyMenu()));
    connect (devicesMountDVDMenu, SIGNAL(aboutToShow()), this, SLOT(prepareDVDMenu()));
    connect (devicesNetworkMenu, SIGNAL(aboutToShow()), this, SLOT(prepareNetworkMenu()));

    connect (statusBar(), SIGNAL(messageChanged (const QString &)), this, SLOT(statusTipChanged (const QString &)));

    connect (devicesMountFloppyMenu, SIGNAL(activated(int)), this, SLOT(captureFloppy(int)));
    connect (devicesMountDVDMenu, SIGNAL(activated(int)), this, SLOT(captureDVD(int)));
    connect (devicesUSBMenu, SIGNAL(activated(int)), this, SLOT(switchUSB(int)));
    connect (devicesSharedFolders, SIGNAL(activated(int)), this, SLOT(activateSFMenu()));
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

    connect (helpWebAction, SIGNAL (activated()),
             &vboxProblem(), SLOT (showHelpWebDialog()));
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
    connect (sf_state, SIGNAL (contextMenuRequested (QIStateIndicator *, QContextMenuEvent *)),
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
    /* prepare the dock images */
    dockImgStateRunning   = ::DarwinCreateDockBadge ("state_running_16px.png");
    dockImgStatePaused    = ::DarwinCreateDockBadge ("state_paused_16px.png");
    dockImgStateSaving    = ::DarwinCreateDockBadge ("state_saving_16px.png");
    dockImgStateRestoring = ::DarwinCreateDockBadge ("state_restoring_16px.png");
    dockImgBack75x75      = ::DarwinCreateDockBadge ("dock_0.png");
    dockImgBack100x75     = ::DarwinCreateDockBadge ("dock_1.png");
    SetApplicationDockTileImage (dockImgOS);
    OverlayApplicationDockTileImage (dockImgStateRunning);
#endif
}

VBoxConsoleWnd::~VBoxConsoleWnd()
{
    if (mUsbLedTip)
        delete mUsbLedTip;
    if (mNetworkLedTip)
        delete mNetworkLedTip;
#ifdef Q_WS_MAC
    /* release the dock images */
    if (dockImgStateRunning)
        CGImageRelease (dockImgStateRunning);
    if (dockImgStatePaused)
        CGImageRelease (dockImgStatePaused);
    if (dockImgStateSaving)
        CGImageRelease (dockImgStateSaving);
    if (dockImgStateRestoring)
        CGImageRelease (dockImgStateRestoring);
    if (dockImgBack75x75)
        CGImageRelease (dockImgBack75x75);
    if (dockImgBack100x75)
        CGImageRelease (dockImgBack100x75);
    if (dockImgOS)
        CGImageRelease (dockImgOS);
#endif
}

//
// Public members
/////////////////////////////////////////////////////////////////////////////

static const char *GUI_LastWindowPosition = "GUI/LastWindowPostion";
static const char *GUI_LastWindowPosition_Max = "max";

static const char *GUI_Fullscreen = "GUI/Fullscreen";
static const char *GUI_AutoresizeGuest = "GUI/AutoresizeGuest";

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
        setCentralWidget (new QWidget (this, "centralWidget"));
        new QBoxLayout (centralWidget(), QBoxLayout::LeftToRight);
    }

    vmPauseAction->setOn (false);

    VBoxDefs::RenderMode mode = vboxGlobal().vmRenderMode();

    CConsole cconsole = csession.GetConsole();
    AssertWrapperOk (csession);

    console = new VBoxConsoleView (this, cconsole, mode,
                                   centralWidget(), "console");

    loadGuiCustomizations();

    ((QBoxLayout*) centralWidget()->layout())->addWidget (
        console, 0, AlignVCenter | AlignHCenter);

    CMachine cmachine = csession.GetMachine();

    /* restore the position of the window and some options */
    {
        QString str = cmachine.GetExtraData (GUI_Fullscreen);
        if (str == "on")
            vmFullscreenAction->setOn (true);

        str = cmachine.GetExtraData (GUI_AutoresizeGuest);
        if (str != "off")
            vmAutoresizeGuestAction->setOn (true);

        str = cmachine.GetExtraData (GUI_LastWindowPosition);

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
            max = str.section (',', 4, 4) == GUI_LastWindowPosition_Max;
        if (ok)
        {
            normal_pos = QPoint (x, y);
            normal_size = QSize (w, h);
            if (!full_screen)
            {
                move (normal_pos);
                resize (normal_size);
                /* normalize to the optimal size */
                console->normalizeGeometry (true /* adjustPosition */);
                /* maximize if needed */
                if (max)
                    setWindowState (windowState() | WindowMaximized);
            }
            else
                was_max = max;
        }
        else
        {
            normal_pos = QPoint();
            normal_size = QSize();
            if (!full_screen)
            {
                console->normalizeGeometry (true /* adjustPosition */);
                /* maximize if needed */
                if (max)
                    setWindowState (windowState() | WindowMaximized);
            }
            else
                was_max = max;
        }
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
        usb_light->setState (isUSBEnabled ? CEnums::DeviceIdle
                                          : CEnums::InvalidActivity);
        mUsbLedTip = new VBoxUSBLedTip (usb_light, cconsole, isUSBEnabled);
    }

    /* initialize network stuff */
    mNetworkLedTip = new VBoxNetworkLedTip (net_light, cmachine);

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
        sf_state->setHidden (true);
    }

    /* start an idle timer that will update device lighths */
    connect (idle_timer, SIGNAL (timeout()), SLOT (updateDeviceLights()));
    idle_timer->start (50, false);

    connect (console, SIGNAL (mouseStateChanged (int)),
             this, SLOT (updateMouseState (int)));
    connect (console, SIGNAL (keyboardStateChanged (int)),
             hostkey_state, SLOT (setState (int)));
    connect (console, SIGNAL (machineStateChanged (CEnums::MachineState)),
             this, SLOT (updateMachineState (CEnums::MachineState)));
    connect (console, SIGNAL (additionsStateChanged (const QString&, bool)),
             this, SLOT (updateAdditionsState (const QString &, bool)));

#ifdef Q_WS_MAC
    QString osTypeId = cmachine.GetOSTypeId();
# if 0
    QImage osImg75x75 = vboxGlobal().vmGuestOSTypeIcon (osTypeId).convertToImage().smoothScale (75, 75);
    QImage osImg = QImage::fromMimeSource ("dock_0.png");
    bitBlt (&osImg, 25, 22,
            &osImg75x75, 0, 0,
            75, 75, /* conversion_flags */ 0);
# else
    QImage osImg100x75 = vboxGlobal().vmGuestOSTypeIcon (osTypeId).convertToImage().smoothScale (100, 75);
    QImage osImg = QImage::fromMimeSource ("dock_1.png");
    bitBlt (&osImg, 14, 22,
            &osImg100x75, 0, 0,
            100, 75, /* conversion_flags */ 0);
# endif
    if (dockImgOS)
        CGImageRelease (dockImgOS);
    dockImgOS = ::DarwinQImageToCGImage (&osImg);
#endif

    /* set the correct initial machine_state value */
    machine_state = cconsole.GetState();

    updateAppearanceOf (AllStuff);

    show();

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

void VBoxConsoleWnd::loadGuiCustomizations()
{
    VBoxGlobalSettings settings = vboxGlobal().settings();
    /* Process known keys */
    menuBar()->setHidden (settings.isFeatureActive ("noMenuBar"));
    statusBar()->setHidden (settings.isFeatureActive ("noStatusBar"));
}

void VBoxConsoleWnd::finalizeOpenView()
{
    LogFlowFuncEnter();

    bool saved = machine_state == CEnums::Saved;

    CMachine cmachine = csession.GetMachine();
    CConsole cconsole = console->console();

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
    if (machine_state < CEnums::Running)
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

    LogFlowFuncLeave();
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
        if (isMaximized() || (full_screen && was_max))
            winPos += QString (",%1").arg (GUI_LastWindowPosition_Max);

        machine.SetExtraData (GUI_LastWindowPosition, winPos);

        machine.SetExtraData (GUI_Fullscreen,
                              vmFullscreenAction->isOn() ? "on" : "off");
        machine.SetExtraData (GUI_AutoresizeGuest,
                              vmAutoresizeGuestAction->isOn() ? "on" : "off");
    }

    console->detach();

    centralWidget()->layout()->remove( console );
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
            if (!full_screen &&
                (windowState() & (WindowMaximized | WindowMinimized |
                                  WindowFullScreen)) == 0)
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
            if (!full_screen &&
                (windowState() & (WindowMaximized | WindowMinimized |
                                  WindowFullScreen)) == 0)
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
            statusBar()->message (ev->mTip);
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

    static const char *GUI_LastCloseAction = "GUI/LastCloseAction";
    static const char *Save = "save";
    static const char *PowerOff = "powerOff";
    static const char *DiscardCurState = "discardCurState";

    if (!console)
    {
        e->accept();
        LogFlowFunc (("Console already destroyed!"));
        LogFlowFuncLeave();
        return;
    }

    if (machine_state > CEnums::Paused)
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
    if (machine_state < CEnums::Running)
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

        bool wasPaused = machine_state == CEnums::Paused;
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
            VBoxCloseVMDlg dlg (this, "VBoxCloseVMDlg");
            QString typeId = cmachine.GetOSTypeId();
            dlg.pmIcon->setPixmap (vboxGlobal().vmGuestOSTypeIcon (typeId));

            /* read the last user's choice for the given VM */
            QStringList lastAction = QStringList::split (',',
                cmachine.GetExtraData (GUI_LastCloseAction));
            AssertWrapperOk (cmachine);
            if (lastAction [0] == PowerOff)
                dlg.buttonGroup->setButton (dlg.buttonGroup->id (dlg.rbPowerOff));
            else if (lastAction [0] == Save)
                dlg.buttonGroup->setButton (dlg.buttonGroup->id (dlg.rbSave));
            else
                dlg.buttonGroup->setButton (dlg.buttonGroup->id (dlg.rbPowerOff));
            dlg.cbDiscardCurState->setChecked (
                lastAction.count() > 1 && lastAction [1] == DiscardCurState);

            /* make the Discard checkbox invisible if there are no snapshots */
            dlg.cbDiscardCurState->setShown ((cmachine.GetSnapshotCount() > 0));

            if (dlg.exec() == QDialog::Accepted)
            {
                CConsole cconsole = console->console();

                if (dlg.rbSave->isChecked())
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
                if (dlg.rbPowerOff->isChecked())
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
                        if (dlg.cbDiscardCurState->isShown() &&
                            dlg.cbDiscardCurState->isChecked())
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
                    e->accept();

                    /* memorize the last user's choice for the given VM */
                    QString lastAction;
                    if (dlg.rbSave->isChecked())
                        lastAction = Save;
                    else if (dlg.rbPowerOff->isChecked())
                        lastAction = PowerOff;
                    else
                        AssertFailed();
                    if (dlg.cbDiscardCurState->isChecked())
                        (lastAction += ",") += DiscardCurState;
                    cmachine.SetExtraData (GUI_LastCloseAction, lastAction);
                    AssertWrapperOk (cmachine);
                }
            }
        }

        no_auto_close = false;

        if (machine_state < CEnums::Running)
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
                if (!wasPaused && machine_state == CEnums::Paused)
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
    caption_prefix = tr ("InnoTek VirtualBox");
#endif

    /*
     *  Note: All action shortcuts should be added to the menu text in the
     *  form of "\tHost+<Key>" where <Key> is the shortcut key according
     *  to regular QKeySequence rules. No translation of the "Host" word is
     *  allowed (VBoxConsoleView relies on its spelling). setAccel() must not
     *  be used.
     */

    /* VM actions */

    vmFullscreenAction->setMenuText (tr ("&Fullscreen Mode") + "\tHost+F");
    vmFullscreenAction->setStatusTip (tr ("Switch to fullscreen mode" ));

    vmDisMouseIntegrMenu->setToolTip (tr ("Mouse Integration",
                                          "enable/disable..."));
    vmAutoresizeMenu->setToolTip (tr ("Auto-resize Guest Display",
                                      "enable/disable..."));
    vmAutoresizeGuestAction->setMenuText (tr ("Auto-resize &Guest Display") +
                                          "\tHost+G");
    vmAutoresizeGuestAction->setStatusTip (
        tr ("Automatically resize the guest display when the window is resized "
            "(requires Guest Additions)"));

    vmAdjustWindowAction->setMenuText (tr ("&Adjust Window Size") + "\tHost+A");
    vmAdjustWindowAction->setStatusTip (
        tr ("Adjust window size and position to best fit the guest display"));

    vmTypeCADAction->setMenuText (tr ("&Insert Ctrl-Alt-Del") + "\tHost+Del");
    vmTypeCADAction->setStatusTip (
        tr ("Send the Ctrl-Alt-Del sequence to the virtual machine"));

#if defined(Q_WS_X11)
    vmTypeCABSAction->setMenuText (tr ("&Insert Ctrl-Alt-Backspace") +
                                   "\tHost+Backspace");
    vmTypeCABSAction->setStatusTip (
        tr ("Send the Ctrl-Alt-Backspace sequence to the virtual machine"));
#endif

    vmResetAction->setMenuText (tr ("&Reset") + "\tHost+R");
    vmResetAction->setStatusTip (tr ("Reset the virtual machine"));

    /* vmPauseAction is set up in updateAppearanceOf() */

    vmACPIShutdownAction->setMenuText (tr ("ACPI S&hutdown") + "\tHost+H");
    vmACPIShutdownAction->setStatusTip (
        tr ("Send the ACPI Power Button press event to the virtual machine"));

    vmCloseAction->setMenuText (tr ("&Close..." ) + "\tHost+Q");
    vmCloseAction->setStatusTip (tr ("Close the virtual machine"));

    vmTakeSnapshotAction->setMenuText (tr ("Take &Snapshot..." ) + "\tHost+S");
    vmTakeSnapshotAction->setStatusTip (tr ("Take a snapshot of the virtual machine"));

    /* vmDisableMouseIntegrAction is set up in updateAppearanceOf() */

    /* Devices actions */

    devicesMountFloppyImageAction->setMenuText (tr ("&Floppy Image..."));
    devicesMountFloppyImageAction->setStatusTip (tr ("Mount a floppy image file"));

    devicesUnmountFloppyAction->setMenuText (tr ("Unmount F&loppy"));
    devicesUnmountFloppyAction->setStatusTip (
        tr ("Unmount the currently mounted floppy media"));

    devicesMountDVDImageAction->setMenuText (tr ("&CD/DVD-ROM Image..."));
    devicesMountDVDImageAction->setStatusTip (
        tr ("Mount a CD/DVD-ROM image file"));

    devicesUnmountDVDAction->setMenuText (tr ("Unmount C&D/DVD-ROM"));
    devicesUnmountDVDAction->setStatusTip (
        tr ("Unmount the currently mounted CD/DVD-ROM media"));

    devicesVRDPMenu->setToolTip (tr ("Remote Desktop (RDP) Server",
                                     "enable/disable..."));
    devicesSwitchVrdpAction->setMenuText (tr ("Remote Dis&play"));
    devicesSwitchVrdpAction->setStatusTip (
        tr ("Enable or disable remote desktop (RDP) connections to this machine"));

    devicesSharedFolders->insertItem (tr ("Open"));
    devicesSFDialogAction->setMenuText (tr ("&Shared Folders"));
    devicesSFDialogAction->setStatusTip (
        tr ("Open the dialog to operate on shared folders"));

    devicesInstallGuestToolsAction->setMenuText (tr ("&Install Guest Additions..."));
    devicesInstallGuestToolsAction->setStatusTip (
        tr ("Mount the Guest Additions installation image"));

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* Debug actions */

    if (dbgStatisticsAction)
        dbgStatisticsAction->setMenuText (tr ("&Statistics..."));
    if (dbgCommandLineAction)
        dbgCommandLineAction->setMenuText (tr ("&Command line..."));
#endif

    /* Help actions */

    helpWebAction->setMenuText (tr ("&VirtualBox Web Site..."));
    helpWebAction->setStatusTip (
        tr ("Open the browser and go to the VirtualBox product web site"));

    helpAboutAction->setMenuText (tr ("&About VirtualBox..."));
    helpAboutAction->setStatusTip (tr ("Show a dialog with product information"));

    helpResetMessagesAction->setMenuText (tr ("&Reset All Warnings"));
    helpResetMessagesAction->setStatusTip (
        tr ("Cause all suppressed warnings and messages to be shown again"));

    /* Devices menu submenus */

    devicesMenu->changeItem (devicesMountFloppyMenuId, tr ("Mount &Floppy"));
    devicesMenu->changeItem (devicesMountDVDMenuId, tr ("Mount &CD/DVD-ROM"));
    devicesMenu->changeItem (devicesNetworkMenuId, tr ("&Network Adapters"));
    devicesMenu->changeItem (devicesUSBMenuId, tr ("&USB Devices"));

    menuBar()->changeItem (vmMenuId, tr ("&Machine"));
    menuBar()->changeItem (devicesMenuId, tr ("&Devices"));
#ifdef VBOX_WITH_DEBUGGER_GUI
    if (vboxGlobal().isDebuggerEnabled())
        menuBar()->changeItem (dbgMenuId, tr ("De&bug"));
#endif
    menuBar()->changeItem (helpMenuId, tr ("&Help"));

    /* status bar widgets */

#if 0
    QToolTip::add (autoresize_state,
        tr ("Indicates whether the guest display auto-resize function is On "
            "(<img src=auto_resize_on_16px.png/>) or Off (<img src=auto_resize_off_16px.png/>). "
            "Note that this function requires Guest Additions to be installed in the guest OS."));
#endif
    QToolTip::add (mouse_state,
        tr ("Indicates whether the host mouse pointer is captured by the guest OS:<br>"
            "<nobr><img src=mouse_disabled_16px.png/>&nbsp;&nbsp;pointer is not captured</nobr><br>"
            "<nobr><img src=mouse_16px.png/>&nbsp;&nbsp;pointer is captured</nobr><br>"
            "<nobr><img src=mouse_seamless_16px.png/>&nbsp;&nbsp;mouse integration (MI) is On</nobr><br>"
            "<nobr><img src=mouse_can_seamless_16px.png/>&nbsp;&nbsp;MI is Off, pointer is captured</nobr><br>"
            "<nobr><img src=mouse_can_seamless_uncaptured_16px.png/>&nbsp;&nbsp;MI is Off, pointer is not captured</nobr><br>"
            "Note that the mouse integration feature requires Guest Additions to be installed in the guest OS."));
    QToolTip::add (hostkey_state,
        tr ("Indicates whether the keyboard is captured by the guest OS "
            "(<img src=hostkey_captured_16px.png/>) or not (<img src=hostkey_16px.png/>)"));
    QToolTip::add (hostkey_name,
        tr ("Shows the currently assigned Host key.<br>"
            "This key, when pressed alone, toggles the the keyboard and mouse "
            "capture state. It can also be used in combination with other keys "
            "to quickly perform actions from the main menu." ));
    QToolTip::add (sf_state,
/// @todo add later, when activity is actually reported
//        tr ("Indicates the activity of shared folders."));
        tr ("Provides quick access to shared folders (by a right mouse button click).<br>"
            "Note that the shared folders feature requires Guest Additions to be installed in the guest OS."));

    updateAppearanceOf (AllStuff);
}


void VBoxConsoleWnd::updateAppearanceOf (int element)
{
    if (!console) return;

    CMachine cmachine = csession.GetMachine();
    CConsole cconsole = console->console();

    if (element & Caption)
    {
        QString snapshotName;
        if (cmachine.GetSnapshotCount() > 0)
        {
            CSnapshot snapshot = cmachine.GetCurrentSnapshot();
            snapshotName = " (" + snapshot.GetName() + ")";
        }
        setCaption (cmachine.GetName() + snapshotName +
                    " [" + vboxGlobal().toString (machine_state) + "] - " +
                    caption_prefix);
    }
    if (element & FloppyStuff)
    {
        devicesMountFloppyMenu->setEnabled (machine_state == CEnums::Running);
        CFloppyDrive floppy = cmachine.GetFloppyDrive();
        CEnums::DriveState state = floppy.GetState();
        bool mounted = state != CEnums::NotMounted;
        devicesUnmountFloppyAction->setEnabled (machine_state == CEnums::Running && mounted);
        fd_light->setState (mounted ? CEnums::DeviceIdle : CEnums::InvalidActivity);
        QString tip = tr (
            "<qt>Indicates&nbsp;the&nbsp;activity&nbsp;of&nbsp;the&nbsp;floppy&nbsp;media"
            "<br>[<b>%1</b>]</qt>"
        );
        QString name;
        switch (state)
        {
            case CEnums::HostDriveCaptured:
                name = tr ("Host&nbsp;Drive&nbsp;", "Floppy tooltip") +
                    floppy.GetHostDrive().GetName();
                break;
            case CEnums::ImageMounted:
                name = floppy.GetImage().GetFilePath();
                break;
            case CEnums::NotMounted:
                name = tr ("not&nbsp;mounted", "Floppy tooltip");
                break;
            default:
                AssertMsgFailed (("Invalid floppy drive state: %d\n", state));
        }
        QToolTip::add (fd_light, tip.arg (name));
    }
    if (element & DVDStuff)
    {
        devicesMountDVDMenu->setEnabled (machine_state == CEnums::Running);
        CDVDDrive dvd = cmachine.GetDVDDrive();
        CEnums::DriveState state = dvd.GetState();
        bool mounted = state != CEnums::NotMounted;
        devicesUnmountDVDAction->setEnabled (machine_state == CEnums::Running && mounted);
        cd_light->setState (mounted ? CEnums::DeviceIdle : CEnums::InvalidActivity);
        QString tip = tr (
            "<qt>Indicates&nbsp;the&nbsp;activity&nbsp;of&nbsp;the&nbsp;CD/DVD-ROM&nbsp;media"
            "<br>[<b>%1</b>]</qt>"
        );
        QString name;
        switch (state)
        {
            case CEnums::HostDriveCaptured:
                name = tr ("Host&nbsp;Drive&nbsp;", "DVD-ROM tooltip") +
                    dvd.GetHostDrive().GetName();
                break;
            case CEnums::ImageMounted:
                name = dvd.GetImage().GetFilePath();
                break;
            case CEnums::NotMounted:
                name = tr ("not&nbsp;mounted", "DVD-ROM tooltip");
                break;
            default:
                AssertMsgFailed (("Invalid dvd drive state: %d\n", state));
        }
        QToolTip::add (cd_light, tip.arg (name));
    }
    if (element & HardDiskStuff)
    {
        QString info =
            tr ("<qt>Indicates&nbsp;the&nbsp;activity&nbsp;of&nbsp;virtual&nbsp;hard&nbsp;disks");
        const char *ctlNames[] = { "IDE0", "IDE1", "Controller%1" };
        const char *devNames[] = { "Master", "Slave", "Device%1" };
        bool hasDisks = false;
        CHardDiskAttachmentEnumerator aen = cmachine.GetHardDiskAttachments().Enumerate();
        while (aen.HasMore()) {
            CHardDiskAttachment hda = aen.GetNext();
            QString ctl, dev;
            switch (hda.GetController()) {
                case CEnums::IDE0Controller:
                    ctl = ctlNames[0];
                    break;
                case CEnums::IDE1Controller:
                    ctl = ctlNames[1];
                    break;
                default:
                    break;
            }
            if (!ctl.isNull()) {
                dev = devNames[hda.GetDeviceNumber()];
            } else {
                ctl = QString (ctlNames[2]).arg (hda.GetController());
                dev = QString (devNames[2]).arg (hda.GetDeviceNumber());
            }
            CHardDisk hd = hda.GetHardDisk();
            info += QString ("<br>[%1&nbsp;%2:&nbsp;<b>%3</b>]")
                .arg (ctl).arg (dev)
                .arg (hd.GetLocation().replace (' ', "&nbsp;"));
            hasDisks = true;
        }
        if (!hasDisks)
            info += tr ("<br>[<b>not attached</b>]", "HDD tooltip");
        info += "</qt>";
        QToolTip::add (hd_light, info);
        hd_light->setState (hasDisks ? CEnums::DeviceIdle : CEnums::InvalidActivity);
    }
    if (element & NetworkStuff)
    {
        ulong maxCount = vboxGlobal().virtualBox()
                        .GetSystemProperties().GetNetworkAdapterCount();
        ulong count = 0;
        for (ulong slot = 0; slot < maxCount; slot++)
            if (cmachine.GetNetworkAdapter (slot).GetEnabled())
                count++;
        net_light->setState (count > 0 ? CEnums::DeviceIdle
                                         : CEnums::InvalidActivity);
    }
    if (element & USBStuff)
    {
        /// @todo (r=dmik) do we really need to disable the control while
        //  in Pause? Check the same for CD/DVD above.
        if (mUsbLedTip && mUsbLedTip->isUSBEnabled())
            devicesUSBMenu->setEnabled (machine_state == CEnums::Running);
    }
    if (element & VRDPStuff)
    {
        CVRDPServer vrdpsrv = csession.GetMachine().GetVRDPServer();
        if (vrdpsrv.isNull())
            return;

        /* update menu&status icon state */
        bool isVRDPEnabled = vrdpsrv.GetEnabled();
        devicesSwitchVrdpAction->setOn (isVRDPEnabled);
#if 0
        vrdp_state->setState (isVRDPEnabled ? 1 : 0);

        /* compose status icon tooltip */
        QString tip = tr ("Indicates whether the Remote Display (VRDP Server) "
                          "is enabled (<img src=vrdp_16px.png/>) or not "
                          "(<img src=vrdp_disabled_16px.png/>)"
        );
        if (vrdpsrv.GetEnabled())
            tip += tr ("<hr>VRDP Server is listening on port %1").arg (vrdpsrv.GetPort());
        QToolTip::add (vrdp_state, tip);
#endif
    }
    if (element & PauseAction)
    {
        if (!vmPauseAction->isOn())
        {
            vmPauseAction->setMenuText (tr ("&Pause") + "\tHost+P");
            vmPauseAction->setStatusTip (
                tr ("Suspend the execution of the virtual machine"));
        }
        else
        {
            vmPauseAction->setMenuText (tr ("R&esume") + "\tHost+P");
            vmPauseAction->setStatusTip (
                tr ("Resume the execution of the virtual machine" ) );
        }
        vmPauseAction->setEnabled (machine_state == CEnums::Running ||
                                   machine_state == CEnums::Paused);
    }
    if (element & DisableMouseIntegrAction)
    {
        if (!vmDisableMouseIntegrAction->isOn())
        {
            vmDisableMouseIntegrAction->setMenuText (tr ("Disable &Mouse Integration") +
                                                     "\tHost+I");
            vmDisableMouseIntegrAction->setStatusTip (
                tr ("Temporarily disable host mouse pointer integration"));
        }
        else
        {
            vmDisableMouseIntegrAction->setMenuText (tr ("Enable &Mouse Integration") +
                                                     "\tHost+I");
            vmDisableMouseIntegrAction->setStatusTip (
                tr ("Enable temporarily disabled host mouse pointer integration"));
        }
        if (machine_state == CEnums::Running)
            vmDisableMouseIntegrAction->setEnabled (console->isMouseAbsolute());
        else
            vmDisableMouseIntegrAction->setEnabled (false);
    }
#ifdef Q_WS_MAC
    if (1)//(element & DockTile)
    {
        //CGContextDrawImage
        //HIViewDrawCGImage();
        //CGImageAlphaInfo
        CGImageRef img;
        if (machine_state == CEnums::Running)
            img = dockImgStateRunning;
        else if (machine_state == CEnums::Paused)
            img = dockImgStatePaused;
        else if (machine_state == CEnums::Restoring)
            img = dockImgStateRestoring;
        else if (machine_state == CEnums::Saving)
            img = dockImgStateSaving;
        else
            img = NULL;
        SetApplicationDockTileImage (dockImgOS);
        if (img)
            OverlayApplicationDockTileImage (img);
    }
#endif
}

//
// Private slots
/////////////////////////////////////////////////////////////////////////////

void VBoxConsoleWnd::vmFullscreen (bool on)
{
    AssertReturnVoid (console);
    AssertReturnVoid (full_screen != on);
    AssertReturnVoid ((hidden_children.isEmpty() == on));

    if (on)
    {
        /* take the Fullscreen hot key from the menu item */
        QString hotKey = vmFullscreenAction->menuText();
        hotKey = QStringList::split ('\t', hotKey) [1];
        Assert (!hotKey.isEmpty());
        /* get the host key name */
        QString hostKey = QIHotKeyEdit::keyName (vboxGlobal().settings().hostKey());
        /* show the info message */
        vboxProblem().remindAboutGoingFullscreen (hotKey, hostKey);
    }

    full_screen = on;

    vmAdjustWindowAction->setEnabled (!on);

    bool wasHidden = isHidden();

    if (on)
    {
        /* Save the previous scroll-view minimum size before entering
         * fullscreen state to restore this minimum size before the exiting
         * fullscreen. Required for correct scroll-view and guest display
         * update in SDL mode. */
        prev_min_size = console->minimumSize();
        console->setMinimumSize (0, 0);

        /* memorize the maximized state */
        was_max = isMaximized();
        /* set the correct flags to disable unnecessary frame controls */
        int flags = WType_TopLevel | WStyle_Customize | WStyle_NoBorder |
                    WStyle_StaysOnTop;
        QRect scrGeo = QApplication::desktop()->screenGeometry (this);
        /* hide early to avoid extra flicker */
        hide();
        /* hide all but the central widget containing the console view */
        QObjectList *list = queryList (NULL, NULL, false, false);
        for (QObject *obj = list->first(); obj != NULL; obj = list->next())
        {
            if (obj->isWidgetType() && obj != centralWidget())
            {
                QWidget *w = (QWidget *) obj;
                if (!w->isHidden())
                {
                    w->hide();
                    hidden_children.append (w);
                }
            }
        }
        delete list;
        /* reparet to apply new flags and place to the top left corner of the
         * current desktop */
        reparent (parentWidget(), flags, QPoint (scrGeo.x(), scrGeo.y()), false);
        /* adjust colors and appearance */
        erase_color = centralWidget()->eraseColor();
        centralWidget()->setEraseColor (black);
        console_style = console->frameStyle();
        console->setFrameStyle (QFrame::NoFrame);
        console->setMaximumSize (console->sizeHint());
        console->setVScrollBarMode (QScrollView::AlwaysOff);
        console->setHScrollBarMode (QScrollView::AlwaysOff);
        /* go fullscreen */
        resize (scrGeo.size());
        setMinimumSize (size());
        setMaximumSize (size());
#ifdef Q_WS_MAC
        /* make the apple menu bar go away. */
        OSStatus orc = SetSystemUIMode (kUIModeAllHidden, kUIOptionDisableAppleMenu);
        if (orc)
            LogRel (("Error: Failed to change UI mode (rc=%#x) when changing to fullscreen mode. (=> menu bar trouble)\n", orc));
#endif
    }
    else
    {
        /* Restore the previous scroll-view minimum size before the exiting
         * fullscreen. Required for correct scroll-view and guest display
         * update in SDL mode. */
        console->setMinimumSize (prev_min_size);

        /* hide early to avoid extra flicker */
#ifdef Q_WS_MAC
        SetSystemUIMode (kUIModeNormal, 0);
#endif
        hide();
        /* reparet to restore normal flags */
        reparent (parentWidget(), normal_wflags, QPoint (0, 0), false);
        /* adjust colors and appearance */
        centralWidget()->setEraseColor (erase_color);
        console->setFrameStyle (console_style);
        console->setMaximumSize (console->sizeHint());
        console->setVScrollBarMode (QScrollView::Auto);
        console->setHScrollBarMode (QScrollView::Auto);
        /* show everything hidden when going fullscreen */
        for (QObject *obj = hidden_children.first(); obj != NULL;
             obj = hidden_children.next())
            ((QWidget *) obj)->show();
        hidden_children.clear();
        /* restore normal values */
        setMinimumSize (QSize(0, 0));
        setMaximumSize (QSize (QWIDGETSIZE_MAX, QWIDGETSIZE_MAX));
        move (normal_pos);
        resize (normal_size);
        /* let our toplevel widget calculate its sizeHint properly */
        QApplication::sendPostedEvents (0, QEvent::LayoutHint);
        /* restore the maximized state */
        if (was_max)
            setWindowState (windowState() | WindowMaximized);
        QTimer::singleShot (0, console, SLOT (exitFullScreen()));
    }

    /* VBoxConsoleView loses focus for some reason after reparenting,
     * restore it */
    console->setFocus();

    if (!wasHidden)
        show();
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
        static LONG cadSequence[] = {
            0x1d, // Ctrl down
            0x38, // Alt down
            0x0E, // Backspace down
            0x8E, // Backspace up
            0xb8, // Alt up
            0x9d  // Ctrl up
        };
        keyboard.PutScancodes (cadSequence, ELEMENTS (cadSequence));
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
        console->console().PowerButton();
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
    bool wasPaused = machine_state == CEnums::Paused;
    if (!wasPaused)
    {
        /* Suspend the VM and ignore the close event if failed to do so.
         * pause() will show the error message to the user. */
        if (!console->pause (true))
            return;
    }

    CMachine cmachine = csession.GetMachine();

    VBoxTakeSnapshotDlg dlg (this, "VBoxTakeSnapshotDlg");

    QString typeId = cmachine.GetOSTypeId();
    dlg.pmIcon->setPixmap (vboxGlobal().vmGuestOSTypeIcon (typeId));

    dlg.leName->setText (tr ("Snapshot %1").arg (cmachine.GetSnapshotCount() + 1));

    if (dlg.exec() == QDialog::Accepted)
    {
        CConsole cconsole = csession.GetConsole();

        CProgress progress =
            cconsole.TakeSnapshot (dlg.leName->text().stripWhiteSpace(),
                                   dlg.txeDescription->text());

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

    VBoxDiskImageManagerDlg dlg (this, "VBoxDiskImageManagerDlg", WType_Dialog | WShowModal);
    QUuid id = csession.GetMachine().GetId();
    dlg.setup (VBoxDefs::FD, true, &id);

    if (dlg.exec() == VBoxDiskImageManagerDlg::Accepted)
    {
        CFloppyDrive drv = csession.GetMachine().GetFloppyDrive();
        drv.MountImage (dlg.getSelectedUuid());
        AssertWrapperOk (drv);
        if (drv.isOk())
        {
// @todo: save the settings only on power off if the appropriate flag is set
//            console->console().SaveSettings();
            updateAppearanceOf (FloppyStuff);
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
// @todo: save the settings only on power off if the appropriate flag is set
//        console->machine().SaveSettings();
        updateAppearanceOf (FloppyStuff);
    }
}

void VBoxConsoleWnd::devicesMountDVDImage()
{
    if (!console) return;

    VBoxDiskImageManagerDlg dlg (this, "VBoxDiskImageManagerDlg", WType_Dialog | WShowModal);
    QUuid id = csession.GetMachine().GetId();
    dlg.setup (VBoxDefs::CD, true, &id);

    if (dlg.exec() == VBoxDiskImageManagerDlg::Accepted)
    {
        CDVDDrive drv = csession.GetMachine().GetDVDDrive();
        drv.MountImage (dlg.getSelectedUuid());
        AssertWrapperOk (drv);
        if (drv.isOk())
        {
// @todo: save the settings only on power off if the appropriate flag is set
//            console->console().SaveSettings();
            updateAppearanceOf (DVDStuff);
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

void VBoxConsoleWnd::devicesToggleSFDialog (bool aOn)
{
    if (!console) return;

    if (aOn)
        (new VBoxSFDialog (console, csession, devicesSFDialogAction))->show();
}

void VBoxConsoleWnd::devicesInstallGuestAdditions()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();

    QString src1 = qApp->applicationDirPath() + "/VBoxGuestAdditions.iso";
    QString src2 = qApp->applicationDirPath() + "/additions/VBoxGuestAdditions.iso";
    QString src;

    if (QFile::exists (src1))
        src = src1;
    else if (QFile::exists (src2))
        src = src2;
    else
    {
        vboxProblem().message (this, VBoxProblemReporter::Error,
            tr ("<p>Failed to find the VirtualBox Guest Additions CD image "
                "<nobr><b>%1</b></nobr> or "
                "<nobr><b>%2</b></nobr></p>")
            .arg (src1).arg (src2));
        return;
    }

    QUuid uuid;
    CDVDImage newImage = vbox.OpenDVDImage (src, uuid);
    if (vbox.isOk())
    {
        src = newImage.GetFilePath();
        CDVDImage oldImage = vbox.FindDVDImage(src);
        if (oldImage.isNull())
            vbox.RegisterDVDImage (newImage);
        else
            newImage = oldImage;
        if (vbox.isOk())
            uuid = newImage.GetId();
    }
    if (!vbox.isOk())
    {
        vboxProblem().cannotRegisterMedia (this, vbox, VBoxDefs::CD, src);
        return;
    }
    CDVDDrive drv = csession.GetMachine().GetDVDDrive();
    drv.MountImage (uuid);
    /// @todo (r=dmik) use VBoxProblemReporter::cannotMountMedia...
    AssertWrapperOk (drv);
    if (drv.isOk())
        updateAppearanceOf (DVDStuff);
}

void VBoxConsoleWnd::devicesUnmountDVD()
{
    if (!console) return;

    CDVDDrive drv = csession.GetMachine().GetDVDDrive();
    drv.Unmount();
    AssertWrapperOk (drv);
    if (drv.isOk())
    {
// @todo: save the settings only on power off if the appropriate flag is set
//        console->machine().SaveSettings();
        updateAppearanceOf (DVDStuff);
    }
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
        int id = devicesMountFloppyMenu->insertItem (
            tr ("Host Drive ") + hostFloppy.GetName()
        );
        hostFloppyMap [id] = hostFloppy;
        if (machine_state != CEnums::Running)
            devicesMountFloppyMenu->setItemEnabled (id, false);
        else if (!selected.isNull())
            if (!selected.GetName().compare (hostFloppy.GetName()))
                devicesMountFloppyMenu->setItemEnabled (id, false);
    }

    if (devicesMountFloppyMenu->count() > 0)
        devicesMountFloppyMenu->insertSeparator();
    devicesMountFloppyImageAction->addTo (devicesMountFloppyMenu);

    /* if shown as a context menu */
    if (devicesMenu->itemParameter (devicesMountFloppyMenuId))
    {
        devicesMountFloppyMenu->insertSeparator();
        devicesUnmountFloppyAction->addTo (devicesMountFloppyMenu);
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
        int id = devicesMountDVDMenu->insertItem (
            tr ("Host Drive ") + hostDVD.GetName()
        );
        hostDVDMap [id] = hostDVD;
        if (machine_state != CEnums::Running)
            devicesMountDVDMenu->setItemEnabled (id, false);
        else if (!selected.isNull())
            if (!selected.GetName().compare (hostDVD.GetName()))
                devicesMountDVDMenu->setItemEnabled (id, false);
    }

    if (devicesMountDVDMenu->count() > 0)
        devicesMountDVDMenu->insertSeparator();
    devicesMountDVDImageAction->addTo (devicesMountDVDMenu);

    /* if shown as a context menu */
    if (devicesMenu->itemParameter (devicesMountDVDMenuId))
    {
        devicesMountDVDMenu->insertSeparator();
        devicesUnmountDVDAction->addTo (devicesMountDVDMenu);
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
        statusBar()->clear();
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
// @todo: save the settings only on power off if the appropriate flag is set
//        console->machine().SaveSettings();
        updateAppearanceOf (FloppyStuff);
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
// @todo: save the settings only on power off if the appropriate flag is set
//        console->machine().SaveSettings();
        updateAppearanceOf (DVDStuff);
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

/**
 *  Show Shared Folders list.
 */
void VBoxConsoleWnd::activateSFMenu()
{
    if (!devicesSFDialogAction->isOn())
        devicesSFDialogAction->setOn (true);
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
    if (ind == sf_state)
    {
        devicesSharedFolders->exec (e->globalPos());
    }
    else
    if (ind == net_light)
    {
        /* set "this is a context menu" flag */
        devicesMenu->setItemParameter (devicesNetworkMenuId, 1);
        devicesNetworkMenu->exec (e->globalPos());
        devicesMenu->setItemParameter (devicesNetworkMenuId, 0);
    }
}

void VBoxConsoleWnd::updateDeviceLights()
{
    if (console) {
        CConsole &cconsole = console->console();
        int st;
        if (hd_light->state() != CEnums::InvalidActivity) {
            st = cconsole.GetDeviceActivity (CEnums::HardDiskDevice);
            if (hd_light->state() != st)
                hd_light->setState (st);
        }
        if (cd_light->state() != CEnums::InvalidActivity) {
            st = cconsole.GetDeviceActivity (CEnums::DVDDevice);
            if (cd_light->state() != st)
                cd_light->setState (st);
        }
        if (fd_light->state() != CEnums::InvalidActivity) {
            st = cconsole.GetDeviceActivity (CEnums::FloppyDevice);
            if (fd_light->state() != st)
                fd_light->setState (st);
        }
        if (net_light->state() != CEnums::InvalidActivity) {
            st = cconsole.GetDeviceActivity (CEnums::NetworkDevice);
            if (net_light->state() != st)
                net_light->setState (st);
        }
        if (usb_light->state() != CEnums::InvalidActivity) {
            st = cconsole.GetDeviceActivity (CEnums::USBDevice);
            if (usb_light->state() != st)
                usb_light->setState (st);
        }
    }
}

void VBoxConsoleWnd::updateMachineState (CEnums::MachineState state)
{
    if (console && machine_state != state)
    {
        if (state >= CEnums::Running)
        {
            switch (state)
            {
                case CEnums::Paused:
                {
                    if (!vmPauseAction->isOn())
                        vmPauseAction->setOn (true);
                    break;
                }
                case CEnums::Running:
                {
                    if (machine_state == CEnums::Paused && vmPauseAction->isOn())
                        vmPauseAction->setOn (false);
                    break;
                }
                default:
                    break;
            }
        }

        runningActions->setEnabled (state == CEnums::Running);

        machine_state = state;

        updateAppearanceOf (Caption | FloppyStuff | DVDStuff |
                            USBStuff | VRDPStuff | PauseAction |
                            DisableMouseIntegrAction );

        if (state < CEnums::Running)
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


void VBoxConsoleWnd::updateAdditionsState (const QString &aVersion, bool aActive)
{
    vmAutoresizeGuestAction->setEnabled (aActive);

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

/**
 *  Helper to safely close the main console window.
 *
 *  This method ensures that close() will not be called if there is some
 *  modal widget currently being executed, as it can cause uninitialization
 *  at the point of code where it is not expected at all (example:
 *  VBoxConsoleView::mouseEvent() calling
 *  VBoxProblemReporter::remindAboutInputCapture()). Instead, an attempt to
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
    LogFlowFunc (("eventLoopLevel=%d\n", qApp->eventLoop()->loopLevel()));

    if (qApp->eventLoop()->loopLevel() > 1)
    {
        if (QApplication::activeModalWidget())
            QApplication::activeModalWidget()->close();
        else if (QApplication::activePopupWidget())
            QApplication::activePopupWidget()->close();
        else
        {
            /// @todo (r=dmik) in general, the following is not that correct
            //  because some custom modal event loop may not expect to be
            //  exited externally (e.g., it might want to set some internal
            //  flags before calling exitLoop()). The alternative is to do
            //  nothing but wait keeping to post singleShot timers.
            qApp->eventLoop()->exitLoop();
        }

        QTimer::singleShot (0, this, SLOT (tryClose()));
    }
    else
        close();
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

VBoxSFDialog::VBoxSFDialog (QWidget  *aParent, CSession &aSession, QAction *aAction)
    : QDialog (aParent, "VBoxSFDialog", false /* modal */,
               WStyle_Customize | WStyle_Title | WStyle_SysMenu |
               WStyle_MinMax | WDestructiveClose)
    , mSettings (0), mSession (aSession), mAction (aAction)
{
    /* Setup Dialog's title */
    setCaption (tr ("Shared Folders"));
    setIcon (QPixmap::fromMimeSource ("select_file_16px.png"));

    /* Setup main dialog's layout */
    QVBoxLayout *mainLayout = new QVBoxLayout (this, 10, 10, "mainLayout");

    /* Setup settings layout */
    mSettings = new VBoxSharedFoldersSettings (this, "mSettings");
    mSettings->setDialogType (VBoxSharedFoldersSettings::ConsoleType);
    mSettings->getFromMachine (aSession.GetMachine());
    mSettings->getFromConsole (aSession.GetConsole());
    mainLayout->addWidget (mSettings);

    /* Setup button's layout */
    QHBoxLayout *buttonLayout = new QHBoxLayout (mainLayout, 10, "buttonLayout");
    QPushButton *pbOk = new QPushButton (tr ("OK"), this, "pbOk");
    QSpacerItem *spacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    QPushButton *pbCancel = new QPushButton (tr ("Cancel"), this, "pbCancel");
    connect (pbOk, SIGNAL (clicked()), this, SLOT (accept()));
    connect (pbCancel, SIGNAL (clicked()), this, SLOT (reject()));
    buttonLayout->addWidget (pbOk);
    buttonLayout->addItem (spacer);
    buttonLayout->addWidget (pbCancel);

    /* Setup destruction handler */
    connect (mAction, SIGNAL (toggled (bool)), this, SLOT (suicide (bool)));
}

VBoxSFDialog::~VBoxSFDialog()
{
    mAction->setOn (false);
}

void VBoxSFDialog::accept()
{
    mSettings->putBackToConsole();
    QDialog::accept();
}

void VBoxSFDialog::suicide (bool aNo)
{
    if (!aNo) close();
}

void VBoxSFDialog::showEvent (QShowEvent *aEvent)
{
    QDialog::showEvent (aEvent);
    setMinimumWidth (400);
    VBoxGlobal::centerWidget (this, parentWidget());
}

