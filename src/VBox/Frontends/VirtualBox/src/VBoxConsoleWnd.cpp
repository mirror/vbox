/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxConsoleWnd class implementation
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

#include "VBoxConsoleWnd.h"
#include "VBoxConsoleView.h"
#include "VBoxCloseVMDlg.h"
#include "VBoxTakeSnapshotDlg.h"
#include "VBoxMediaManagerDlg.h"
#include "VBoxVMFirstRunWzd.h"
#include "VBoxVMSettingsSF.h"
#include "VBoxVMInformationDlg.h"
#include "VBoxDownloaderWgt.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"

#include "QIStateIndicator.h"
#include "QIStatusBar.h"
#include "QIHotKeyEdit.h"
#include "QIHttp.h"

/* Qt includes */
#include <QActionGroup>
#include <QDesktopWidget>
#include <QMenuBar>
#include <QFileInfo>
#include <QDir>
#include <QTimer>
#include <QProgressBar>
#ifdef Q_WS_X11
# include <QX11Info>
#endif

#include <VBox/VBoxGuest.h>

#if defined(Q_WS_X11)
#include <X11/Xlib.h>
#include <XKeyboard.h>
#endif

#ifdef Q_WS_MAC
# include "VBoxUtils.h"
# include "VBoxIChatTheaterWrapper.h"
# include <ApplicationServices/ApplicationServices.h>
  /* Qt includes */
# include <QPainter>
#endif

#ifdef VBOX_WITH_DEBUGGER_GUI
# include <VBox/err.h>
# include <iprt/ldr.h>
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

class VBoxAdditionsDownloader : public VBoxDownloaderWgt
{
    Q_OBJECT;

public:

    VBoxAdditionsDownloader (const QString &aSource, const QString &aTarget, QAction *aAction)
        : VBoxDownloaderWgt (aSource, aTarget)
        , mAction (aAction)
    {
        mAction->setEnabled (false);
        retranslateUi();
    }

    void start()
    {
        acknowledgeStart();
    }

protected:

    void retranslateUi()
    {
        mCancelButton->setText (tr ("Cancel"));
        mProgressBar->setToolTip (tr ("Downloading the VirtualBox Guest Additions "
                                      "CD image from <nobr><b>%1</b>...</nobr>")
                                      .arg (mSource.toString()));
        mCancelButton->setToolTip (tr ("Cancel the VirtualBox Guest "
                                       "Additions CD image download"));
    }

private slots:

    void downloadFinished (bool aError)
    {
        if (aError)
            VBoxDownloaderWgt::downloadFinished (aError);
        else
        {
            QByteArray receivedData (mHttp->readAll());
            /* Serialize the incoming buffer into the .iso image. */
            while (true)
            {
                QFile file (mTarget);
                if (file.open (QIODevice::WriteOnly))
                {
                    file.write (receivedData);
                    file.close();
                    if (vboxProblem().confirmMountAdditions (mSource.toString(),
                        QDir::toNativeSeparators (mTarget)))
                        vboxGlobal().consoleWnd().installGuestAdditionsFrom (mTarget);
                    QTimer::singleShot (0, this, SLOT (suicide()));
                    break;
                }
                else
                {
                    vboxProblem().message (window(), VBoxProblemReporter::Error,
                        tr ("<p>Failed to save the downloaded file as "
                            "<nobr><b>%1</b>.</nobr></p>")
                        .arg (QDir::toNativeSeparators (mTarget)));
                }

                QString target = vboxGlobal().getExistingDirectory (
                    QFileInfo (mTarget).absolutePath(), this,
                    tr ("Select folder to save Guest Additions image to"), true);
                if (target.isNull())
                    QTimer::singleShot (0, this, SLOT (suicide()));
                else
                    mTarget = QDir (target).absoluteFilePath (QFileInfo (mTarget).fileName());
            }
        }
    }

    void suicide()
    {
        QStatusBar *sb = qobject_cast <QStatusBar*> (parent());
        Assert (sb);
        sb->removeWidget (this);
        mAction->setEnabled (true);
        VBoxDownloaderWgt::suicide();
    }

private:

    bool confirmDownload()
    {
        return vboxProblem().confirmDownloadAdditions (mSource.toString(),
            mHttp->lastResponse().contentLength());
    }

    void warnAboutError (const QString &aError)
    {
        return vboxProblem().cannotDownloadGuestAdditions (mSource.toString(), aError);
    }

    QAction *mAction;
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
VBoxConsoleWnd (VBoxConsoleWnd **aSelf, QWidget* aParent,
                Qt::WindowFlags aFlags /* = Qt::Window */)
    : QIWithRetranslateUI2<QMainWindow> (aParent, aFlags)
    , mMainMenu (0)
#ifdef VBOX_WITH_DEBUGGER_GUI
    , mDbgStatisticsAction (NULL)
    , mDbgCommandLineAction (NULL)
    , mDbgLoggingAction (NULL)
    , mDbgMenu (NULL)
#endif
    , console (0)
    , machine_state (KMachineState_Null)
    , no_auto_close (false)
    , mIsFullscreen (false)
    , mIsSeamless (false)
    , mIsSeamlessSupported (false)
    , mIsGraphicsSupported (false)
    , mIsWaitingModeResize (false)
    , was_max (false)
    , console_style (0)
    , mIsOpenViewFinished (false)
    , mIsFirstTimeStarted (false)
    , mIsAutoSaveMedia (true)
#ifdef VBOX_WITH_DEBUGGER_GUI
    , mDbgGui (NULL)
    , mDbgGuiVT (NULL)
#endif
{
    if (aSelf)
        *aSelf = this;

    idle_timer = new QTimer (this);

#if !(defined (Q_WS_WIN) || defined (Q_WS_MAC))
    /* The default application icon (will change to the VM-specific icon in
     * openView()). On Win32, it's built-in to the executable. On Mac OS X the
     * icon referenced in info.plist is used. */
    setWindowIcon (QIcon (":/VirtualBox_48px.png"));
#endif

    /* ensure status bar is created */
    setStatusBar (new QIStatusBar (this));

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

    mVmFullscreenAction = new QAction (this);
    mVmFullscreenAction->setIcon (
        VBoxGlobal::iconSetOnOff (
            ":/fullscreen_on_16px.png", ":/fullscreen_16px.png",
            ":/fullscreen_on_disabled_16px.png", ":/fullscreen_disabled_16px.png"));
    mVmFullscreenAction->setCheckable (true);

    mVmSeamlessAction = new QAction (this);
    mVmSeamlessAction->setIcon (
        VBoxGlobal::iconSetOnOff (
            ":/seamless_on_16px.png", ":/seamless_16px.png",
            ":/seamless_on_disabled_16px.png", ":/seamless_disabled_16px.png"));
    mVmSeamlessAction->setCheckable (true);

    mVmAutoresizeGuestAction = new QAction (mRunningActions);
    mVmAutoresizeGuestAction->setIcon (
        VBoxGlobal::iconSetOnOff (
            ":/auto_resize_on_on_16px.png", ":/auto_resize_on_16px.png",
            ":/auto_resize_on_on_disabled_16px.png", ":/auto_resize_on_disabled_16px.png"));
    mVmAutoresizeGuestAction->setCheckable (true);
    mVmAutoresizeGuestAction->setEnabled (false);

    mVmAdjustWindowAction = new QAction (this);
    mVmAdjustWindowAction->setIcon (
        VBoxGlobal::iconSet (":/adjust_win_size_16px.png",
                             ":/adjust_win_size_disabled_16px.png"));

    mVmTypeCADAction = new QAction (mRunningActions);
    mVmTypeCADAction->setIcon (VBoxGlobal::iconSet (":/hostkey_16px.png",
                                                      ":/hostkey_disabled_16px.png"));

#if defined(Q_WS_X11)
    mVmTypeCABSAction = new QAction (mRunningActions);
    mVmTypeCABSAction->setIcon (VBoxGlobal::iconSet (":/hostkey_16px.png",
                                                       ":/hostkey_disabled_16px.png"));
#endif

    mVmResetAction = new QAction (mRunningActions);
    mVmResetAction->setIcon (VBoxGlobal::iconSet (":/reset_16px.png",
                                                    ":/reset_disabled_16px.png"));

    mVmPauseAction = new QAction (this);
    mVmPauseAction->setIcon (
        /* checkbox over the pause icon doesn't look nice */
#if 0
        VBoxGlobal::iconSetOnOff (
            ":/pause_on_16px.png", ":/pause_16px.png",
            ":/pause_on_disabled_16px.png", ":/pause_disabled_16px.png"));
#else
        VBoxGlobal::iconSet (":/pause_16px.png", ":/pause_disabled_16px.png"));
#endif
    mVmPauseAction->setCheckable (true);

    mVmACPIShutdownAction = new QAction (mRunningActions);
    mVmACPIShutdownAction->setIcon (
        VBoxGlobal::iconSet (":/acpi_16px.png", ":/acpi_disabled_16px.png"));

    mVmCloseAction = new QAction (this);
    mVmCloseAction->setMenuRole (QAction::QuitRole);
    mVmCloseAction->setIcon (VBoxGlobal::iconSet (":/exit_16px.png"));

    mVmTakeSnapshotAction = new QAction (mRunningOrPausedActions);
    mVmTakeSnapshotAction->setIcon (VBoxGlobal::iconSet (
        ":/take_snapshot_16px.png", ":/take_snapshot_dis_16px.png"));

    mVmShowInformationDlgAction = new QAction (this);
    mVmShowInformationDlgAction->setIcon (VBoxGlobal::iconSet (
        ":/session_info_16px.png", ":/session_info_disabled_16px.png"));

    mVmDisableMouseIntegrAction = new QAction (this);
    mVmDisableMouseIntegrAction->setIcon (
        VBoxGlobal::iconSetOnOff (
            ":/mouse_can_seamless_on_16px.png", ":/mouse_can_seamless_16px.png",
            ":/mouse_can_seamless_on_disabled_16px.png", ":/mouse_can_seamless_disabled_16px.png"));
    mVmDisableMouseIntegrAction->setCheckable (true);

    /* Devices menu actions */

    mDevicesMountFloppyImageAction = new QAction (mRunningOrPausedActions);

    mDevicesUnmountFloppyAction = new QAction (this);
    mDevicesUnmountFloppyAction->setIcon (
        VBoxGlobal::iconSet (":/fd_unmount_16px.png",
                             ":/fd_unmount_dis_16px.png"));

    mDevicesMountDVDImageAction = new QAction (mRunningOrPausedActions);

    mDevicesUnmountDVDAction = new QAction (this);
    mDevicesUnmountDVDAction->setIcon (
        VBoxGlobal::iconSet (":/cd_unmount_16px.png",
                             ":/cd_unmount_dis_16px.png"));

    mDevicesSFDialogAction = new QAction (mRunningOrPausedActions);
    mDevicesSFDialogAction->setIcon (
        VBoxGlobal::iconSet (":/shared_folder_16px.png",
                             ":/shared_folder_disabled_16px.png"));

    mDevicesSwitchVrdpAction = new QAction (mRunningOrPausedActions);
    mDevicesSwitchVrdpAction->setIcon (
        VBoxGlobal::iconSetOnOff (
            ":/vrdp_on_16px.png", ":/vrdp_16px.png",
            ":/vrdp_on_disabled_16px.png", ":/vrdp_disabled_16px.png"));
    mDevicesSwitchVrdpAction->setCheckable (true);

    mDevicesInstallGuestToolsAction = new QAction (mRunningActions);
    mDevicesInstallGuestToolsAction->setIcon (
        VBoxGlobal::iconSet (":/guesttools_16px.png",
                             ":/guesttools_disabled_16px.png"));

#ifdef VBOX_WITH_DEBUGGER_GUI
    if (vboxGlobal().isDebuggerEnabled())
    {
        mDbgStatisticsAction = new QAction (this);
        mDbgCommandLineAction = new QAction (this);
        if (vboxGlobal().getDebuggerModule()== NIL_RTLDRMOD)
        {
            mDbgStatisticsAction->setEnabled (false);
            mDbgCommandLineAction->setEnabled (false);
        }
        mDbgLoggingAction = new QAction (this);
        mDbgLoggingAction->setCheckable (true);
    }
    else
    {
        mDbgStatisticsAction = NULL;
        mDbgCommandLineAction = NULL;
        mDbgLoggingAction = NULL;
    }
#endif

    /* Help menu actions */

    mHelpActions.setup (this);

    ///// Menubar ///////////////////////////////////////////////////////////

    mMainMenu = new QIMenu (this);

    /* Machine submenu */

    mVMMenu = menuBar()->addMenu (QString::null);
    mMainMenu->addMenu (mVMMenu);

    /* dynamic & status line popup menus */
    mVmAutoresizeMenu = new VBoxSwitchMenu (mVMMenu, mVmAutoresizeGuestAction);
    mVmDisMouseIntegrMenu = new VBoxSwitchMenu (mVMMenu, mVmDisableMouseIntegrAction,
                                               true /* inverted toggle state */);

    mVMMenu->addAction (mVmFullscreenAction);
    mVMMenu->addAction (mVmSeamlessAction);
    mVMMenu->addAction (mVmAutoresizeGuestAction);
    mVMMenu->addAction (mVmAdjustWindowAction);
    mVMMenu->addSeparator();
    mVMMenu->addAction (mVmDisableMouseIntegrAction);
    mVMMenu->addSeparator();
    mVMMenu->addAction (mVmTypeCADAction);
#if defined(Q_WS_X11)
    mVMMenu->addAction (mVmTypeCABSAction);
#endif
    mVMMenu->addSeparator();
    mVMMenu->addAction (mVmTakeSnapshotAction);
    mVMMenu->addSeparator();
    mVMMenu->addAction (mVmShowInformationDlgAction);
    mVMMenu->addSeparator();
    mVMMenu->addAction (mVmResetAction);
    mVMMenu->addAction (mVmPauseAction);
    mVMMenu->addAction (mVmACPIShutdownAction);
#ifndef Q_WS_MAC
    mVMMenu->addSeparator();
#endif /* Q_WS_MAC */
    mVMMenu->addAction (mVmCloseAction);

    /* Devices submenu */

    mDevicesMenu = menuBar()->addMenu (QString::null);
    mMainMenu->addMenu (mDevicesMenu);

    /* dynamic & statusline popup menus */

    mDevicesMountDVDMenu = mDevicesMenu->addMenu (VBoxGlobal::iconSet (":/cd_16px.png", ":/cd_disabled_16px.png"), QString::null);
    mDevicesMenu->addAction (mDevicesUnmountDVDAction);
    mDevicesMenu->addSeparator();

    mDevicesMountFloppyMenu = mDevicesMenu->addMenu (VBoxGlobal::iconSet (":/fd_16px.png", ":/fd_disabled_16px.png"), QString::null);
    mDevicesMenu->addAction (mDevicesUnmountFloppyAction);
    mDevicesMenu->addSeparator();

    mDevicesNetworkMenu = mDevicesMenu->addMenu (VBoxGlobal::iconSet (":/nw_16px.png", ":/nw_disabled_16px.png"), QString::null);
    mDevicesMenu->addSeparator();

    mDevicesUSBMenu = new VBoxUSBMenu (mDevicesMenu);
    mDevicesMenu->addMenu (mDevicesUSBMenu);
    mDevicesUSBMenu->setIcon (VBoxGlobal::iconSet (":/usb_16px.png", ":/usb_disabled_16px.png"));
    mDevicesUSBMenuSeparator = mDevicesMenu->addSeparator();

    /* see showIndicatorContextMenu for a description of mDevicesSFMenu */
    /* mDevicesSFMenu = mDevicesMenu->addMenu (QString::null); */
    mDevicesMenu->addAction (mDevicesSFDialogAction);
    mDevicesSFMenuSeparator = mDevicesMenu->addSeparator();

    /* Currently not needed cause there is no state icon in the statusbar */
    /* mDevicesVRDPMenu = new VBoxSwitchMenu (mDevicesMenu, mDevicesSwitchVrdpAction); */
    mDevicesMenu->addAction (mDevicesSwitchVrdpAction);
    mDevicesVRDPMenuSeparator = mDevicesMenu->addSeparator();

    mDevicesMenu->addAction (mDevicesInstallGuestToolsAction);

    /* reset the "context menu" flag */
    mDevicesMountFloppyMenu->menuAction()->setData (false);
    mDevicesMountDVDMenu->menuAction()->setData (false);
    mDevicesUSBMenu->menuAction()->setData (false);

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* Debug popup menu */
    if (vboxGlobal().isDebuggerEnabled())
    {
        mDbgMenu = menuBar()->addMenu (QString::null);
        mMainMenu->addMenu (mDbgMenu);
        mDbgMenu->addAction (mDbgStatisticsAction);
        mDbgMenu->addAction (mDbgCommandLineAction);
        mDbgMenu->addAction (mDbgLoggingAction);
    }
    else
        mDbgMenu = NULL;
#endif

    /* Help submenu */

    mHelpMenu = menuBar()->addMenu (QString::null);
    mMainMenu->addMenu (mHelpMenu);

    mHelpActions.addTo (mHelpMenu);

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
    fd_light->hide(); /* This LED hidden due to little importance (defect #3630) */
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

    /* virtualization */
    mVirtLed = new QIStateIndicator (0);
    mVirtLed->setStateIcon (0, QPixmap (":/vtx_amdv_disabled_16px.png"));
    mVirtLed->setStateIcon (1, QPixmap (":/vtx_amdv_16px.png"));
    indicatorBoxHLayout->addWidget (mVirtLed);

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

    retranslateUi();

    setWindowTitle (caption_prefix);

    ///// Connections ///////////////////////////////////////////////////////

    connect (mVmFullscreenAction, SIGNAL (toggled (bool)),
             this, SLOT (vmFullscreen (bool)));
    connect (mVmSeamlessAction, SIGNAL (toggled (bool)),
             this, SLOT (vmSeamless (bool)));
    connect (mVmAutoresizeGuestAction, SIGNAL (toggled (bool)),
             this, SLOT (vmAutoresizeGuest (bool)));
    connect (mVmAdjustWindowAction, SIGNAL (triggered()),
             this, SLOT (vmAdjustWindow()));

    connect (mVmTypeCADAction, SIGNAL(triggered()), this, SLOT(vmTypeCAD()));
#if defined(Q_WS_X11)
    connect (mVmTypeCABSAction, SIGNAL(triggered()), this, SLOT(vmTypeCABS()));
#endif
    connect (mVmResetAction, SIGNAL(triggered()), this, SLOT (vmReset()));
    connect (mVmPauseAction, SIGNAL(toggled (bool)), this, SLOT (vmPause (bool)));
    connect (mVmACPIShutdownAction, SIGNAL (triggered()), this, SLOT (vmACPIShutdown()));
    connect (mVmCloseAction, SIGNAL(triggered()), this, SLOT (vmClose()));

    connect (mVmTakeSnapshotAction, SIGNAL(triggered()), this, SLOT(vmTakeSnapshot()));
    connect (mVmShowInformationDlgAction, SIGNAL(triggered()), this, SLOT (vmShowInfoDialog()));

    connect (mVmDisableMouseIntegrAction, SIGNAL(toggled (bool)), this, SLOT(vmDisableMouseIntegr (bool)));

    connect (mDevicesMountFloppyImageAction, SIGNAL(triggered()), this, SLOT(devicesMountFloppyImage()));
    connect (mDevicesUnmountFloppyAction, SIGNAL(triggered()), this, SLOT(devicesUnmountFloppy()));
    connect (mDevicesMountDVDImageAction, SIGNAL(triggered()), this, SLOT(devicesMountDVDImage()));
    connect (mDevicesUnmountDVDAction, SIGNAL(triggered()), this, SLOT(devicesUnmountDVD()));
    connect (mDevicesSwitchVrdpAction, SIGNAL(toggled (bool)), this, SLOT(devicesSwitchVrdp (bool)));
    connect (mDevicesSFDialogAction, SIGNAL(triggered()), this, SLOT(devicesOpenSFDialog()));
    connect (mDevicesInstallGuestToolsAction, SIGNAL(triggered()), this, SLOT(devicesInstallGuestAdditions()));


    connect (mDevicesMountFloppyMenu, SIGNAL(aboutToShow()), this, SLOT(prepareFloppyMenu()));
    connect (mDevicesMountDVDMenu, SIGNAL(aboutToShow()), this, SLOT(prepareDVDMenu()));
    connect (mDevicesNetworkMenu, SIGNAL(aboutToShow()), this, SLOT(prepareNetworkMenu()));

    connect (statusBar(), SIGNAL(messageChanged (const QString &)), this, SLOT(statusTipChanged (const QString &)));

    connect (mDevicesMountFloppyMenu, SIGNAL(triggered(QAction *)), this, SLOT(captureFloppy(QAction *)));
    connect (mDevicesMountDVDMenu, SIGNAL(triggered(QAction *)), this, SLOT(captureDVD(QAction *)));
    connect (mDevicesUSBMenu, SIGNAL(triggered(QAction *)), this, SLOT(switchUSB(QAction *)));
    connect (mDevicesNetworkMenu, SIGNAL(triggered(QAction *)), this, SLOT(activateNetworkMenu(QAction *)));

    connect (mDevicesMountFloppyMenu, SIGNAL (hovered (QAction *)),
             this, SLOT (setDynamicMenuItemStatusTip (QAction *)));
    connect (mDevicesMountDVDMenu, SIGNAL (hovered (QAction *)),
             this, SLOT (setDynamicMenuItemStatusTip (QAction *)));
    connect (mDevicesNetworkMenu, SIGNAL (hovered (QAction *)),
             this, SLOT (setDynamicMenuItemStatusTip (QAction *)));

    /* Cleanup the status bar tip when a menu with dynamic items is
     * hidden. This is necessary for context menus in the first place but also
     * for normal menus (because Qt will not do it on pressing ESC if the menu
     * is constructed of dynamic items only) */
    connect (mDevicesMountFloppyMenu, SIGNAL (aboutToHide()),
             statusBar(), SLOT (clearMessage()));
    connect (mDevicesMountDVDMenu, SIGNAL (aboutToHide()),
             statusBar(), SLOT (clearMessage()));
    connect (mDevicesNetworkMenu, SIGNAL (aboutToHide()),
             statusBar(), SLOT (clearMessage()));

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
#ifdef Q_WS_MAC
    connect (&vboxGlobal(), SIGNAL (dockIconUpdateChanged (const VBoxChangeDockIconUpdateEvent &)),
             this, SLOT (changeDockIconUpdate (const VBoxChangeDockIconUpdateEvent &)));
#endif

#ifdef VBOX_WITH_DEBUGGER_GUI
    if (mDbgMenu)
        connect (mDbgMenu, SIGNAL (aboutToShow()), this, SLOT (dbgPrepareDebugMenu()));
    if (mDbgStatisticsAction)
        connect (mDbgStatisticsAction, SIGNAL (triggered()), this, SLOT (dbgShowStatistics()));
    if (mDbgCommandLineAction)
        connect (mDbgCommandLineAction, SIGNAL (triggered()), this, SLOT (dbgShowCommandLine()));
    if (mDbgLoggingAction)
        connect (mDbgLoggingAction, SIGNAL (toggled (bool)), this, SLOT (dbgLoggingToggled (bool)));
#endif

#ifdef Q_WS_MAC
    /* For the status bar on Cocoa */
    setUnifiedTitleAndToolBarOnMac (true);

# ifdef VBOX_WITH_ICHAT_THEATER
//    int setAttr[] = { kHIWindowBitDoesNotShowBadgeInDock, 0 };
//    HIWindowChangeAttributes (window, setAttr, NULL);
    initSharedAVManager();
# endif
#endif
    mMaskShift.scale (0, 0, Qt::IgnoreAspectRatio);
}

VBoxConsoleWnd::~VBoxConsoleWnd()
{
    closeView();

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* destroy the debugger gui */
    dbgDestroy();
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

#ifdef Q_WS_MAC
    /* We have to make sure that we are getting the front most process. This is
     * necessary for Qt versions > 4.3.3 */
    ProcessSerialNumber psn = { 0, kCurrentProcess };
    ::SetFrontProcess (&psn);
#endif /* Q_WS_MAC */

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

    mVmPauseAction->setChecked (false);

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

    /* Restore the position of the window and some options */
    {
        QString str = cmachine.GetExtraData (VBoxDefs::GUI_LastWindowPosition);

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

        QRect ar = ok ? QApplication::desktop()->availableGeometry (QPoint (x, y)) :
                        QApplication::desktop()->availableGeometry (this);

        if (ok /* previous parameters were read correctly */)
        {
            mNormalGeo = QRect (x, y, w, h);
            setGeometry (mNormalGeo);

            /* Normalize to the optimal size */
            console->normalizeGeometry (true /* adjustPosition */);

            if (max)
            {
                /* Maximize if needed */
                setWindowState (windowState() | Qt::WindowMaximized);
                was_max = max;
            }
        }
        else
        {
            /* Normalize to the optimal size */
            console->normalizeGeometry (true /* adjustPosition */);

            /* Move newly created window to the screen center. */
            mNormalGeo = geometry();
            mNormalGeo.moveCenter (ar.center());
            setGeometry (mNormalGeo);
        }

        show();

        /* Process show & possible maximize events */
        qApp->processEvents();

        mVmSeamlessAction->setEnabled (false);
        str = cmachine.GetExtraData (VBoxDefs::GUI_Seamless);
        if (str == "on")
            mVmSeamlessAction->setChecked (true);

        str = cmachine.GetExtraData (VBoxDefs::GUI_AutoresizeGuest);
        if (str != "off")
            mVmAutoresizeGuestAction->setChecked (true);

        str = cmachine.GetExtraData (VBoxDefs::GUI_FirstRun);
        if (str == "yes")
            mIsFirstTimeStarted = true;
        else if (!str.isEmpty())
            cmachine.SetExtraData (VBoxDefs::GUI_FirstRun, QString::null);

        str = cmachine.GetExtraData (VBoxDefs::GUI_SaveMountedAtRuntime);
        if (str == "no")
            mIsAutoSaveMedia = false;

        /* Check if one of extended modes to be activated on loading */
        QString fsMode = cmachine.GetExtraData (VBoxDefs::GUI_Fullscreen);
        QString slMode = cmachine.GetExtraData (VBoxDefs::GUI_Seamless);
        bool extendedMode = fsMode == "on" || slMode == "on";

        /* If one of extended modes to be loaded we have to ignore default
         * console resize event which will come from VGA Device on loading. */
        if (extendedMode)
            console->requestToResize (QSize (w, h - menuBar()->height() - statusBar()->height()));
    }

    /* initialize usb stuff */
    CUSBController usbctl = cmachine.GetUSBController();
    if (usbctl.isNull())
    {
        /* hide usb_menu & usb_separator & usb_status_led */
        mDevicesUSBMenu->setVisible (false);
        mDevicesUSBMenuSeparator->setVisible (false);
        usb_light->setHidden (true);
    }
    else
    {
        bool isUSBEnabled = usbctl.GetEnabled();
        mDevicesUSBMenu->setEnabled (isUSBEnabled);
        mDevicesUSBMenu->setConsole (cconsole);
        usb_light->setState (isUSBEnabled ? KDeviceActivity_Idle
                                          : KDeviceActivity_Null);
    }

    /* initialize vrdp stuff */
    CVRDPServer vrdpsrv = cmachine.GetVRDPServer();
    if (vrdpsrv.isNull())
    {
        /* hide vrdp_menu_action & vrdp_separator & vrdp_status_icon */
        mDevicesSwitchVrdpAction->setVisible (false);
        mDevicesVRDPMenuSeparator->setVisible (false);
#if 0
        vrdp_state->setHidden (true);
#endif
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
    connect (console, SIGNAL (additionsStateChanged (const QString&, bool, bool, bool)),
             this, SLOT (updateAdditionsState (const QString &, bool, bool, bool)));
    connect (console, SIGNAL (mediaDriveChanged (VBoxDefs::MediaType)),
             this, SLOT (updateMediaDriveState (VBoxDefs::MediaType)));
    connect (console, SIGNAL (usbStateChange()),
             this, SLOT (updateUsbState()));
    connect (console, SIGNAL (networkStateChange()),
             this, SLOT (updateNetworkAdarptersState()));
    connect (console, SIGNAL (sharedFoldersChanged()),
             this, SLOT (updateSharedFoldersState()));

#ifdef Q_WS_MAC
    QString testStr = vboxGlobal().virtualBox().GetExtraData (VBoxDefs::GUI_RealtimeDockIconUpdateEnabled).toLower();
    /* Default to true if it is an empty value */
    bool f = (testStr.isEmpty() || testStr == "true");
    console->setDockIconEnabled (f);
    console->updateDockOverlay();
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
     *  showing various modal dialogs that will process events from within
     *  this method that in turn can lead to various side effects like this
     *  window is closed before this method returns, etc.
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
    CProgress progress = vboxGlobal().isDebuggerAutoShowEnabled()
                       ? cconsole.PowerUpPaused()
                       : cconsole.PowerUp();

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

    /* Currently the machine is started and the guest API could be used...
     * Checking if the fullscreen mode should be activated */
    QString str = cmachine.GetExtraData (VBoxDefs::GUI_Fullscreen);
    if (str == "on")
        mVmFullscreenAction->setChecked (true);

    /* If seamless mode should be enabled then check if it is enabled
     * currently and re-enable it if seamless is supported */
    if (   mVmSeamlessAction->isChecked()
        && mIsSeamlessSupported
        && mIsGraphicsSupported)
        toggleFullscreenMode (true, true);
#ifdef VBOX_WITH_DEBUGGER_GUI
    /* Open the debugger in "full screen" mode requested by the user. */
    else if (vboxGlobal().isDebuggerAutoShowEnabled())
    {
        /* console in upper left corner of the desktop. */
        QRect rct(0, 0, 0, 0);
        QDesktopWidget *desktop = QApplication::desktop();
        if (desktop)
            rct = desktop->availableGeometry(pos());
        move (QPoint (rct.x(), rct.y()));

        dbgShowStatistics();
        dbgShowCommandLine();
    }
#endif

    mIsOpenViewFinished = true;
    LogFlowFuncLeave();

#ifdef VBOX_WITH_REGISTRATION_REQUEST
    vboxGlobal().showRegistrationDialog (false /* aForce */);
#endif
#ifdef VBOX_WITH_UPDATE_REQUEST
    vboxGlobal().showUpdateDialog (false /* aForce */);
#endif

    /* Finally check the status of required features. */
    checkRequiredFeatures();

    /* Re-request all the static values finally after
     * view is really opened and attached. */
    updateAppearanceOf (VirtualizationStuff);
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
 *  This slot is called just after leaving the fullscreen/seamless mode,
 *  when the console was resized to previous size.
 */
void VBoxConsoleWnd::onExitFullscreen()
{
    console->setIgnoreMainwndResize (false);
}

void VBoxConsoleWnd::unlockActionsSwitch()
{
    if (mIsSeamless)
        mVmSeamlessAction->setEnabled (true);
    else if (mIsFullscreen)
        mVmFullscreenAction->setEnabled (true);
    else
    {
        mVmSeamlessAction->setEnabled (mIsSeamlessSupported && mIsGraphicsSupported);
        mVmFullscreenAction->setEnabled (true);
    }

#ifdef Q_WS_MAC
    if (!mIsSeamless)
    {
        /* Fade back to the normal gamma */
        CGDisplayFade (mFadeToken, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0.0, 0.0, 0.0, false);
        CGReleaseDisplayFadeReservation (mFadeToken);
    }
    console->setMouseCoalescingEnabled (true);
#endif
}

void VBoxConsoleWnd::setMouseIntegrationLocked (bool aDisabled)
{
    mVmDisableMouseIntegrAction->setChecked (aDisabled);
    mVmDisableMouseIntegrAction->setEnabled (false);
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
    mMainMenu->selectFirstAction();
#ifdef Q_WS_WIN
    mMainMenu->activateWindow();
#endif
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

            if (!mIsWaitingModeResize && !isWindowMaximized() &&
                !isTrueFullscreen() && !isTrueSeamless())
            {
                mNormalGeo.setSize (re->size());
#ifdef VBOX_WITH_DEBUGGER_GUI
                dbgAdjustRelativePos();
#endif
            }

            if (mIsWaitingModeResize)
            {
                if (!mIsFullscreen && !mIsSeamless)
                {
                    mIsWaitingModeResize = false;
                    QTimer::singleShot (0, this, SLOT (onExitFullscreen()));
                }
            }
            break;
        }
        case QEvent::Move:
        {
            if (!isWindowMaximized() && !isTrueFullscreen() && !isTrueSeamless())
            {
                mNormalGeo.moveTo (geometry().x(), geometry().y());
#ifdef VBOX_WITH_DEBUGGER_GUI
                dbgAdjustRelativePos();
#endif
            }
            break;
        }
#ifdef Q_WS_MAC
        case QEvent::Paint:
        {
            if (mIsSeamless)
            {
                /* Clear the background */
                CGContextClearRect (::darwinToCGContextRef (this), ::darwinToCGRect (frameGeometry()));
            }
            break;
        }
#endif
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

        bool isACPIEnabled = csession.GetConsole().GetGuestEnteredACPIMode();

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
            dlg.mCbDiscardCurState->setVisible (cmachine.GetSnapshotCount() > 0);

            if (machine_state != KMachineState_Stuck)
            {
                /* read the last user's choice for the given VM */
                QStringList lastAction =
                    cmachine.GetExtraData (VBoxDefs::GUI_LastCloseAction).split (',');
                AssertWrapperOk (cmachine);
                if (lastAction [0] == kSave)
                {
                    dlg.mRbSave->setChecked (true);
                    dlg.mRbSave->setFocus();
                }
                else if (lastAction [0] == kPowerOff || !isACPIEnabled)
                {
                    dlg.mRbShutdown->setEnabled (isACPIEnabled);
                    dlg.mRbPowerOff->setChecked (true);
                    dlg.mRbPowerOff->setFocus();
                }
                else /* the default is ACPI Shutdown */
                {
                    dlg.mRbShutdown->setChecked (true);
                    dlg.mRbShutdown->setFocus();
                }
                dlg.mCbDiscardCurState->setChecked (lastAction.count() > 1 &&
                                                    lastAction [1] == kDiscardCurState);
            }
            else
            {
                /* The stuck VM can only be powered off; disable anything
                 * else and choose PowerOff */
                dlg.mRbSave->setEnabled (false);
                dlg.mRbShutdown->setEnabled (false);
                dlg.mRbPowerOff->setChecked (true);
            }

            bool wasShutdown = false;

            if (dlg.exec() == QDialog::Accepted)
            {
                /* Disable auto closure because we want to have a chance to show the
                 * error dialog on save state / power off failure. */
                no_auto_close = true;

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
                    CProgress progress = cconsole.PowerDownAsync();

                    if (cconsole.isOk())
                    {
                        /* show the power down progress dialog */
                        vboxProblem()
                            .showModalProgressDialog (progress, cmachine.GetName(),
                                                      this, 0);
                        if (progress.GetResultCode() != 0)
                            vboxProblem().cannotStopMachine (progress);
                        else
                            success = true;
                    }
                    else
                        vboxProblem().cannotStopMachine (cconsole);

                    if (success)
                    {
                        /* Note: leave success = true even if we fail to
                         * discard the current state later -- the console window
                         * will closed anyway */

                        /* discard the current state if requested */
                        if (dlg.mCbDiscardCurState->isChecked() &&
                            dlg.mCbDiscardCurState->isVisibleTo (&dlg))
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
                    /* read the last user's choice for the given VM */
                    QStringList prevAction =
                        cmachine.GetExtraData (VBoxDefs::GUI_LastCloseAction).split (',');
                    /* memorize the last user's choice for the given VM */
                    QString lastAction = kPowerOff;
                    if (dlg.mRbSave->isChecked())
                        lastAction = kSave;
                    else if (dlg.mRbShutdown->isChecked() ||
                             (dlg.mRbPowerOff->isChecked() &&
                              prevAction [0] == kShutdown &&
                              !isACPIEnabled))
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

        /* Stop LED update timer */
        idle_timer->stop();
        idle_timer->disconnect (SIGNAL (timeout()), this, SLOT (updateDeviceLights()));

        /* Hide console window */
        hide();

        /* Save the position of the window and some options */
        CMachine machine = csession.GetMachine();
        QString winPos = QString ("%1,%2,%3,%4")
            .arg (mNormalGeo.x()).arg (mNormalGeo.y())
            .arg (mNormalGeo.width()).arg (mNormalGeo.height());
        if (isWindowMaximized() || (mIsFullscreen && was_max)
                          || (mIsSeamless && was_max))
            winPos += QString (",%1").arg (VBoxDefs::GUI_LastWindowPosition_Max);

        machine.SetExtraData (VBoxDefs::GUI_LastWindowPosition, winPos);

        machine.SetExtraData (VBoxDefs::GUI_Fullscreen,
                              mVmFullscreenAction->isChecked() ? "on" : "off");
        machine.SetExtraData (VBoxDefs::GUI_Seamless,
                              mVmSeamlessAction->isChecked() ? "on" : "off");
        machine.SetExtraData (VBoxDefs::GUI_AutoresizeGuest,
                              mVmAutoresizeGuestAction->isChecked() ? "on" : "off");

#ifdef VBOX_WITH_DEBUGGER_GUI
        /* Close & destroy the debugger GUI */
        dbgDestroy();
#endif

        /* Make sure all events are delievered */
        qApp->processEvents();

        /* Notify all the top-level dialogs about closing */
        emit closing();
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

#ifdef Q_WS_MAC
extern void qt_set_sequence_auto_mnemonic (bool on);
#endif

/**
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void VBoxConsoleWnd::retranslateUi()
{
#ifdef VBOX_OSE
    caption_prefix = tr ("VirtualBox OSE");
#else
    caption_prefix = tr ("Sun xVM VirtualBox");
#endif

    /*
     *  Note: All action shortcuts should be added to the menu text in the
     *  form of "\tHost+<Key>" where <Key> is the shortcut key according
     *  to regular QKeySequence rules. No translation of the "Host" word is
     *  allowed (VBoxConsoleView relies on its spelling). setAccel() must not
     *  be used. Unfortunately on the Mac the "host" string is silently removed
     *  & the key is created as an global shortcut. So every e.g. F key stroke
     *  in the vm leads to a menu call of the F entry. Mysteriously the
     *  associated action isn't started. As a workaround the Host+<key> is
     *  written in braces after the menu text.
     */

    /* VM actions */

#ifdef Q_WS_MAC
    qt_set_sequence_auto_mnemonic (false);
#endif

    mVmFullscreenAction->setText (VBoxGlobal::insertKeyToActionText (tr ("&Fullscreen Mode"), "F"));
    mVmFullscreenAction->setStatusTip (tr ("Switch to fullscreen mode" ));

    mVmSeamlessAction->setText (VBoxGlobal::insertKeyToActionText (tr ("Seam&less Mode"), "L"));
    mVmSeamlessAction->setStatusTip (tr ("Switch to seamless desktop integration mode"));

    mVmDisMouseIntegrMenu->setToolTip (tr ("Mouse Integration",
                                           "enable/disable..."));
    mVmAutoresizeMenu->setToolTip (tr ("Auto-resize Guest Display",
                                       "enable/disable..."));
    mVmAutoresizeGuestAction->setText (VBoxGlobal::insertKeyToActionText (tr ("Auto-resize &Guest Display"),
                                                                          "G"));
    mVmAutoresizeGuestAction->setStatusTip (
        tr ("Automatically resize the guest display when the window is resized "
            "(requires Guest Additions)"));

    mVmAdjustWindowAction->setText (VBoxGlobal::insertKeyToActionText (tr ("&Adjust Window Size"), "A"));
    mVmAdjustWindowAction->setStatusTip (
        tr ("Adjust window size and position to best fit the guest display"));

    mVmTypeCADAction->setText (VBoxGlobal::insertKeyToActionText (tr ("&Insert Ctrl-Alt-Del"), "Del"));
    mVmTypeCADAction->setStatusTip (
        tr ("Send the Ctrl-Alt-Del sequence to the virtual machine"));

#if defined(Q_WS_X11)
    mVmTypeCABSAction->setText (VBoxGlobal::insertKeyToActionText (tr ("&Insert Ctrl-Alt-Backspace"),
                                                                   "Backspace"));
    mVmTypeCABSAction->setStatusTip (
        tr ("Send the Ctrl-Alt-Backspace sequence to the virtual machine"));
#endif

    mVmResetAction->setText (VBoxGlobal::insertKeyToActionText (tr ("&Reset"), "R"));
    mVmResetAction->setStatusTip (tr ("Reset the virtual machine"));

    /* mVmPauseAction is set up in updateAppearanceOf() */

#ifdef Q_WS_MAC
    /* Host+H is Hide on the mac */
    mVmACPIShutdownAction->setText (VBoxGlobal::insertKeyToActionText (tr ("ACPI S&hutdown"), "U"));
#else /* Q_WS_MAC */
    mVmACPIShutdownAction->setText (VBoxGlobal::insertKeyToActionText (tr ("ACPI S&hutdown"), "H"));
#endif /* !Q_WS_MAC */
    mVmACPIShutdownAction->setStatusTip (
        tr ("Send the ACPI Power Button press event to the virtual machine"));

    mVmCloseAction->setText (VBoxGlobal::insertKeyToActionText (tr ("&Close..." ), "Q"));
    mVmCloseAction->setStatusTip (tr ("Close the virtual machine"));
    mVmCloseAction->setMenuRole (QAction::QuitRole);

    mVmTakeSnapshotAction->setText (VBoxGlobal::insertKeyToActionText (tr ("Take &Snapshot..."), "S"));
    mVmTakeSnapshotAction->setStatusTip (tr ("Take a snapshot of the virtual machine"));

    mVmShowInformationDlgAction->setText (VBoxGlobal::insertKeyToActionText (tr ("Session I&nformation Dialog"),
                                                                             "N"));
    mVmShowInformationDlgAction->setStatusTip (tr ("Show Session Information Dialog"));

    /* mVmDisableMouseIntegrAction is set up in updateAppearanceOf() */

    /* Devices actions */

    mDevicesMountFloppyImageAction->setText (tr ("&Floppy Image..."));
    mDevicesMountFloppyImageAction->setStatusTip (tr ("Mount a floppy image file"));

    mDevicesUnmountFloppyAction->setText (tr ("Unmount F&loppy"));
    mDevicesUnmountFloppyAction->setStatusTip (
        tr ("Unmount the currently mounted floppy media"));

    mDevicesMountDVDImageAction->setText (tr ("&CD/DVD-ROM Image..."));
    mDevicesMountDVDImageAction->setStatusTip (
        tr ("Mount a CD/DVD-ROM image file"));

    mDevicesUnmountDVDAction->setText (tr ("Unmount C&D/DVD-ROM"));
    mDevicesUnmountDVDAction->setStatusTip (
        tr ("Unmount the currently mounted CD/DVD-ROM media"));

    /* mDevicesVRDPMenu->setToolTip (tr ("Remote Desktop (RDP) Server",
                                     "enable/disable...")); */
    mDevicesSwitchVrdpAction->setText (tr ("Remote Dis&play"));
    mDevicesSwitchVrdpAction->setStatusTip (
        tr ("Enable or disable remote desktop (RDP) connections to this machine"));

    mDevicesSFDialogAction->setText (tr ("&Shared Folders..."));
    mDevicesSFDialogAction->setStatusTip (
        tr ("Open the dialog to operate on shared folders"));

    mDevicesInstallGuestToolsAction->setText (tr ("&Install Guest Additions..."));
    mDevicesInstallGuestToolsAction->setStatusTip (
        tr ("Mount the Guest Additions installation image"));

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* Debug actions */

    if (mDbgStatisticsAction)
        mDbgStatisticsAction->setText (tr ("&Statistics...", "debug action"));
    if (mDbgCommandLineAction)
        mDbgCommandLineAction->setText (tr ("&Command Line...", "debug action"));
    if (mDbgLoggingAction)
        mDbgLoggingAction->setText (tr ("&Logging...", "debug action"));
#endif

    /* Help actions */

    mHelpActions.retranslateUi();

    /* Devices menu submenus */

    mDevicesMountFloppyMenu->setTitle (tr ("Mount &Floppy"));
    mDevicesMountDVDMenu->setTitle (tr ("Mount &CD/DVD-ROM"));
    mDevicesNetworkMenu->setTitle (tr ("&Network Adapters"));
    mDevicesUSBMenu->setTitle (tr ("&USB Devices"));

    /* main menu & seamless popup menu */

    mVMMenu->setTitle (tr ("&Machine"));
//    mVMMenu->setIcon (VBoxGlobal::iconSet (":/machine_16px.png"));

    mDevicesMenu->setTitle (tr ("&Devices"));
//    mDevicesMenu->setIcon (VBoxGlobal::iconSet (":/settings_16px.png"));

#ifdef VBOX_WITH_DEBUGGER_GUI
    if (vboxGlobal().isDebuggerEnabled())
        mDbgMenu->setTitle (tr ("De&bug"));
#endif
    mHelpMenu->setTitle (tr ("&Help"));
//    mHelpMenu->setIcon (VBoxGlobal::iconSet (":/help_16px.png"));

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
            "This key, when pressed alone, toggles the keyboard and mouse "
            "capture state. It can also be used in combination with other keys "
            "to quickly perform actions from the main menu."));
    hostkey_name->setText (QIHotKeyEdit::keyName (vboxGlobal().settings().hostKey()));

    updateAppearanceOf (AllStuff);
}

//
// Private members
/////////////////////////////////////////////////////////////////////////////

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
        mDevicesMountFloppyMenu->setEnabled (isRunningOrPaused);
        CFloppyDrive floppy = cmachine.GetFloppyDrive();
        KDriveState state = floppy.GetState();
        bool mounted = state != KDriveState_NotMounted;
        mDevicesUnmountFloppyAction->setEnabled (isRunningOrPaused && mounted);
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
                    .arg (QDir::toNativeSeparators (floppy.GetImage().GetLocation()));
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
        mDevicesMountDVDMenu->setEnabled (isRunningOrPaused);
        CDVDDrive dvd = cmachine.GetDVDDrive();
        KDriveState state = dvd.GetState();
        bool mounted = state != KDriveState_NotMounted;
        mDevicesUnmountDVDAction->setEnabled (isRunningOrPaused && mounted);
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
                    .arg (QDir::toNativeSeparators (dvd.GetImage().GetLocation()));
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
        CHardDiskAttachmentVector vec = cmachine.GetHardDiskAttachments();
        for (CHardDiskAttachmentVector::ConstIterator hda = vec.begin();
             hda != vec.end(); ++ hda)
        {
            CHardDisk hd = hda->GetHardDisk();
            const QString ctlName = hda->GetController();
            CStorageController ctl = cmachine.GetStorageControllerByName(ctlName);

            data += QString ("<br><nobr><b>%1 %2</b>: %3</nobr>")
                .arg (vboxGlobal().toString (ctl.GetBus(), hda->GetPort()))
                .arg (vboxGlobal().toString (ctl.GetBus(), hda->GetPort(),
                                             hda->GetDevice()))
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

        mDevicesNetworkMenu->setEnabled (isRunningOrPaused && count > 0);

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
                    .arg (slot+1)
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

            CUSBController usbctl = cmachine.GetUSBController();
            if (    !usbctl.isNull()
                &&  usbctl.GetEnabled())
            {
                mDevicesUSBMenu->setEnabled (isRunningOrPaused);

                CUSBDeviceVector devsvec = cconsole.GetUSBDevices();
                for (int i = 0; i < devsvec.size(); ++i)
                {
                    CUSBDevice usb = devsvec[i];
                    info += QString ("<br><b><nobr>%1</nobr></b>")
                                     .arg (vboxGlobal().details (usb));
                }
                if (info.isNull())
                    info = tr ("<br><nobr><b>No USB devices attached</b></nobr>",
                               "USB device tooltip");
            }
            else
            {
                mDevicesUSBMenu->setEnabled (false);

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
            mDevicesSwitchVrdpAction->setChecked (isVRDPEnabled);
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
        CSharedFolderVector psfvec = cmachine.GetSharedFolders();

        for (int i = 0; i < psfvec.size(); ++i)
        {
            CSharedFolder sf = psfvec[i];
            sfs.insert (sf.GetName(), sf.GetHostPath());
        }

        /* transient folders */
        CSharedFolderVector tsfvec = cconsole.GetSharedFolders();

        for (int i = 0; i < tsfvec.size(); ++i)
        {
            CSharedFolder sf = tsfvec[i];
            sfs.insert (sf.GetName(), sf.GetHostPath());
        }

        for (QMap <QString, QString>::const_iterator it = sfs.constBegin();
             it != sfs.constEnd(); ++ it)
        {
            /* Select slashes depending on the OS type */
            if (VBoxGlobal::isDOSType (cconsole.GetGuest().GetOSTypeId()))
                data += QString ("<br><nobr><b>\\\\vboxsvr\\%1&nbsp;</b></nobr>"
                                 "<nobr>%2</nobr>")
                    .arg (it.key(), it.value());
            else
                data += QString ("<br><nobr><b>%1&nbsp;</b></nobr>"
                                 "<nobr>%2</nobr>")
                    .arg (it.key(), it.value());
        }

        if (sfs.count() == 0)
            data = tr ("<br><nobr><b>No shared folders</b></nobr>",
                       "Shared folders tooltip");

        sf_light->setToolTip (tip.arg (data));
    }
    if (element & VirtualizationStuff)
    {
        bool virtEnabled = cconsole.GetDebugger().GetHWVirtExEnabled();
        bool nestEnabled = cconsole.GetDebugger().GetHWVirtExNestedPagingEnabled();

        QString tip ("<qt>Indicates the status of the hardware virtualization "
                     "features used by this virtual machine:<br>"
                     "<nobr><b>%1:</b>&nbsp;%2</nobr><br>"
                     "<nobr><b>%3:</b>&nbsp;%4</nobr></qt>");
        QString virtualization = virtEnabled ?
            VBoxGlobal::tr ("Enabled", "details report (VT-x/AMD-V)") :
            VBoxGlobal::tr ("Disabled", "details report (VT-x/AMD-V)");
        QString nestedPaging = nestEnabled ?
            VBoxVMInformationDlg::tr ("Enabled", "nested paging") :
            VBoxVMInformationDlg::tr ("Disabled", "nested paging");

        mVirtLed->setToolTip (tip
            .arg (VBoxGlobal::tr ("VT-x/AMD-V", "details report"), virtualization)
            .arg (VBoxVMInformationDlg::tr ("Nested Paging"), nestedPaging));

        mVirtLed->setState (virtEnabled);
    }
    if (element & PauseAction)
    {
        if (!mVmPauseAction->isChecked())
        {
            mVmPauseAction->setText (VBoxGlobal::insertKeyToActionText (tr ("&Pause"), "P"));
            mVmPauseAction->setStatusTip (
                tr ("Suspend the execution of the virtual machine"));
        }
        else
        {
            mVmPauseAction->setText (VBoxGlobal::insertKeyToActionText (tr ("R&esume"), "P"));
            mVmPauseAction->setStatusTip (
                tr ("Resume the execution of the virtual machine" ) );
        }
        mVmPauseAction->setEnabled (isRunningOrPaused);
    }
    if (element & DisableMouseIntegrAction)
    {
        if (!mVmDisableMouseIntegrAction->isChecked())
        {
            mVmDisableMouseIntegrAction->setText (VBoxGlobal::insertKeyToActionText (tr ("Disable &Mouse Integration"),
                                                                                     "I"));
            mVmDisableMouseIntegrAction->setStatusTip (
                tr ("Temporarily disable host mouse pointer integration"));
        }
        else
        {
            mVmDisableMouseIntegrAction->setText (VBoxGlobal::insertKeyToActionText (tr ("Enable &Mouse Integration"),
                                                                                     "I"));
            mVmDisableMouseIntegrAction->setStatusTip (
                tr ("Enable temporarily disabled host mouse pointer integration"));
        }
        if (machine_state == KMachineState_Running)
            mVmDisableMouseIntegrAction->setEnabled (console->isMouseAbsolute());
        else
            mVmDisableMouseIntegrAction->setEnabled (false);
    }
}

/**
 *  This function checks the status of required features and
 *  makes a warning and/or some action if something necessary
 *  is not in good condition.
 *  Does nothing if no console view was opened.
 */
void VBoxConsoleWnd::checkRequiredFeatures()
{
    if (!console) return;

    CConsole cconsole = console->console();

    /* Check if virtualization feature enabled for 64 bits guest */
    bool is64BitsGuest = vboxGlobal().virtualBox().GetGuestOSType (
                         cconsole.GetGuest().GetOSTypeId()).GetIs64Bit();
    bool isVirtEnabled = cconsole.GetDebugger().GetHWVirtExEnabled();
    if (is64BitsGuest && !isVirtEnabled)
    {
        vmPause (true);
        if (vboxProblem().warnAboutVirtNotEnabled())
            close();
        else
            vmPause (false);
    }
}

/**
 * @return @c true if successfully performed the requested operation and false
 * otherwise.
 */
bool VBoxConsoleWnd::toggleFullscreenMode (bool aOn, bool aSeamless)
{
    /* Please note: For some platforms like the Mac, the calling order of the
     * functions in this methods is vital. So please be careful on changing
     * this. */

    QSize initialSize = size();
    if (aSeamless || console->isAutoresizeGuestActive())
    {
        QRect screen = aSeamless ?
            QApplication::desktop()->availableGeometry (this) :
            QApplication::desktop()->screenGeometry (this);
        ULONG64 availBits = csession.GetMachine().GetVRAMSize() /* vram */
                          * _1M /* mb to bytes */
                          * 8; /* to bits */
        ULONG guestBpp = console->console().GetDisplay().GetBitsPerPixel();
        ULONG64 usedBits = (screen.width() /* display width */
                         * screen.height() /* display height */
                         * guestBpp
                         + _1M * 8) /* current cache per screen - may be changed in future */
                         * csession.GetMachine().GetMonitorCount() /**< @todo fix assumption that all screens have same resolution */
                         + 4096 * 8; /* adapter info */
        if (aOn && (availBits < usedBits))
        {
            if (aSeamless)
            {
                vboxProblem().cannotEnterSeamlessMode (
                    screen.width(), screen.height(), guestBpp,
                    (((usedBits + 7) / 8 + _1M - 1) / _1M) * _1M);
                return false;
            }
            else
            {
                int result = vboxProblem().cannotEnterFullscreenMode (
                    screen.width(), screen.height(), guestBpp,
                    (((usedBits + 7) / 8 + _1M - 1) / _1M) * _1M);
                if (result == QIMessageBox::Cancel)
                    return false;
            }
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
        /* Take the toggle hot key from the menu item. Since
         * VBoxGlobal::extractKeyFromActionText gets exactly the
         * linked key without the 'Host+' part we are adding it here. */
        QString hotKey = QString ("Host+%1")
            .arg (VBoxGlobal::extractKeyFromActionText (aSeamless ?
                  mVmSeamlessAction->text() : mVmFullscreenAction->text()));

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
        if (!mVmAutoresizeGuestAction->isChecked())
            mVmAutoresizeGuestAction->setChecked (true);

        /* Activate the mouse integration feature for the seamless mode. */
        if (mVmDisableMouseIntegrAction->isChecked())
            mVmDisableMouseIntegrAction->setChecked (false);

        mVmAdjustWindowAction->setEnabled (!aOn);
        mVmFullscreenAction->setEnabled (!aOn);
        mVmAutoresizeGuestAction->setEnabled (!aOn);
        mVmDisableMouseIntegrAction->setEnabled (!aOn);

        console->console().GetDisplay().SetSeamlessMode (aOn);
        mIsSeamless = aOn;
    }
    else
    {
        mIsFullscreen = aOn;
        mVmAdjustWindowAction->setEnabled (!aOn);
        mVmSeamlessAction->setEnabled (!aOn && mIsSeamlessSupported && mIsGraphicsSupported);
    }

    bool wasHidden = isHidden();

    /* Temporarily disable the mode-related action to make sure
     * user can not leave the mode before he enter it and inside out. */
    aSeamless ? mVmSeamlessAction->setEnabled (false) :
                mVmFullscreenAction->setEnabled (false);

    /* Calculate initial console size */
    QSize consoleSize;

    if (aOn)
    {
        consoleSize = console->frameSize();
        consoleSize -= QSize (console->frameWidth() * 2, console->frameWidth() * 2);

        /* Toggle console to manual resize mode. */
        console->setIgnoreMainwndResize (true);

        /* Memorize the maximized state. */
        QDesktopWidget *dtw = QApplication::desktop();
        was_max = isWindowMaximized() &&
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
        if (aSeamless)
            setViewInSeamlessMode (scrGeo);

#ifdef Q_WS_WIN
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

        /* Adjust colors and appearance. */
        mErasePalette = centralWidget()->palette();
        QPalette palette(mErasePalette);
        palette.setColor (centralWidget()->backgroundRole(), Qt::black);
        centralWidget()->setPalette (palette);
        centralWidget()->setAutoFillBackground (!aSeamless);
        console_style = console->frameStyle();
        console->setFrameStyle (QFrame::NoFrame);
        console->setMaximumSize (scrGeo.size());
        console->setHorizontalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
        console->setVerticalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
    }
    else
    {
        /* Reset the shifting spacers. */
        mShiftingSpacerLeft->changeSize (0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        mShiftingSpacerTop->changeSize (0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        mShiftingSpacerRight->changeSize (0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        mShiftingSpacerBottom->changeSize (0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);

        /* Restore the previous scroll-view minimum size before the exiting
         * fullscreen. Required for correct scroll-view and guest display
         * update in SDL mode. */
        console->setMinimumSize (prev_min_size);

#ifdef Q_WS_MAC
        if (aSeamless)
        {
            /* Please note: All the stuff below has to be done before the
             * window switch back to normal size. Qt changes the winId on the
             * fullscreen switch and make this stuff useless with the old
             * winId. So please be careful on rearrangement of the method
             * calls. */
            /* Undo all mac specific installations */
            ::darwinSetShowsWindowTransparent (this, false);
        }
#endif

        /* Adjust colors and appearance. */
        clearMask();
        centralWidget()->setPalette (mErasePalette);
        centralWidget()->setAutoFillBackground (false);
        console->setFrameStyle (console_style);
        console->setMaximumSize (console->sizeHint());
        console->setHorizontalScrollBarPolicy (Qt::ScrollBarAsNeeded);
        console->setVerticalScrollBarPolicy (Qt::ScrollBarAsNeeded);

        /* Show everything hidden when going fullscreen. */
        foreach (QObject *obj, hidden_children)
            ((QWidget *) obj)->show();
        hidden_children.clear();
    }

    /* Set flag for waiting host resize if it awaited during mode entering */
    if ((mIsFullscreen || mIsSeamless) && (consoleSize != initialSize))
        mIsWaitingModeResize = true;

    /* Toggle qt full-screen mode */
    switchToFullscreen (aOn, aSeamless);

#ifdef Q_WS_MAC
    if (aOn && aSeamless)
    {
        /* Please note: All the stuff below has to be done after the window has
         * switched to fullscreen. Qt changes the winId on the fullscreen
         * switch and make this stuff useless with the old winId. So please be
         * careful on rearrangement of the method calls. */
        ::darwinSetShowsWindowTransparent (this, true);
    }
#endif

    /* Send guest size hint */
    console->toggleFSMode (consoleSize);

    /* Process all console attributes changes and sub-widget hidings */
    qApp->processEvents();

    if (!mIsWaitingModeResize)
        onExitFullscreen();

    /* Unlock FS actions locked during modes toggling */
    QTimer::singleShot (300, this, SLOT (unlockActionsSwitch()));

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

void VBoxConsoleWnd::changeDockIconUpdate (const VBoxChangeDockIconUpdateEvent &e)
{
#ifdef Q_WS_MAC
    if (console)
    {
        console->setDockIconEnabled (e.mChanged);
        console->updateDockOverlay();
    }
#else
    Q_UNUSED (e);
#endif
}

//
// Private slots
/////////////////////////////////////////////////////////////////////////////
void VBoxConsoleWnd::switchToFullscreen (bool aOn, bool aSeamless)
{
#ifdef Q_WS_MAC
# ifndef QT_MAC_USE_COCOA
    /* setWindowState removes the window group connection somehow. So save it
     * temporary. */
    WindowGroupRef g = GetWindowGroup (::darwinToNativeWindow (this));
# endif  /* !QT_MAC_USE_COCOA */
    if (aSeamless)
        if (aOn)
        {
            /* Save for later restoring */
            mNormalGeometry = geometry();
            mSavedFlags = windowFlags();
            /* Remove the frame from the window */
            const QRect fullscreen (qApp->desktop()->screenGeometry (qApp->desktop()->screenNumber (this)));
            setParent (0, Qt::Window | Qt::FramelessWindowHint | (windowFlags() & 0xffff0000));
            setGeometry (fullscreen);
            /* Set it maximized */
            setWindowState (windowState() ^ Qt::WindowMaximized);
        }
        else
        {
            /* Restore old values */
            setParent (0, mSavedFlags);
            setGeometry (mNormalGeometry);
        }
    else
        /* Here we are going really fullscreen */
        setWindowState (windowState() ^ Qt::WindowFullScreen);
# ifndef QT_MAC_USE_COCOA
    /* Reassign the correct window group. */
    SetWindowGroup (::darwinToNativeWindow (this), g);
# endif /* !QT_MAC_USE_COCOA */
#else
    NOREF (aOn);
    NOREF (aSeamless);
    setWindowState (windowState() ^ Qt::WindowFullScreen);
#endif
}

void VBoxConsoleWnd::setViewInSeamlessMode (const QRect &aTargetRect)
{
#ifndef Q_WS_MAC
    /* It isn't guaranteed that the guest os set the video mode that
     * we requested. So after all the resizing stuff set the clipping
     * mask and the spacing shifter to the corresponding values. */
    QDesktopWidget *dtw = QApplication::desktop();
    QRect sRect = dtw->screenGeometry (this);
    QRect aRect (aTargetRect);
    mMaskShift.scale (aTargetRect.left(), aTargetRect.top(), Qt::IgnoreAspectRatio);
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
#else // !Q_WS_MAC
    NOREF (aTargetRect);
#endif // !Q_WS_MAC
}

void VBoxConsoleWnd::vmFullscreen (bool aOn)
{
    bool ok = toggleFullscreenMode (aOn, false /* aSeamless */);
    if (!ok)
    {
        /* on failure, restore the previous button state */
        mVmFullscreenAction->blockSignals (true);
        mVmFullscreenAction->setChecked (!aOn);
        mVmFullscreenAction->blockSignals (false);
    }
}

void VBoxConsoleWnd::vmSeamless (bool aOn)
{
    /* check if it is possible to enter/leave seamless mode */
    if ((mIsSeamlessSupported && mIsGraphicsSupported) || !aOn)
    {
        bool ok = toggleFullscreenMode (aOn, true /* aSeamless */);
        if (!ok)
        {
            /* on failure, restore the previous button state */
            mVmSeamlessAction->blockSignals (true);
            mVmSeamlessAction->setChecked (!aOn);
            mVmSeamlessAction->blockSignals (false);
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
        if (isWindowMaximized())
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
        static QVector <LONG> sSequence (6);
        sSequence[0] = 0x1d; // Ctrl down
        sSequence[1] = 0x38; // Alt down
        sSequence[2] = 0x0E; // Backspace down
        sSequence[3] = 0x8E; // Backspace up
        sSequence[4] = 0xb8; // Alt up
        sSequence[5] = 0x9d; // Ctrl up
        keyboard.PutScancodes (sSequence);
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
    if (!csession.GetConsole().GetGuestEnteredACPIMode())
        return vboxProblem().cannotSendACPIToMachine();

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
        CSnapshotVector c = index.GetChildren();
        if (c.size() > 0)
            index = c[0];
        else
            break;
    }
    dlg.mLeName->setText (snapShotName.arg (maxSnapShotIndex + 1));

    if (dlg.exec() == QDialog::Accepted)
    {
        CConsole cconsole = csession.GetConsole();

        CProgress progress =
            cconsole.TakeSnapshot (dlg.mLeName->text().trimmed(),
                                   dlg.mTeDescription->toPlainText());

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

    VBoxMediaManagerDlg dlg (this);

    dlg.setup (VBoxDefs::MediaType_Floppy, true /* aDoSelect */,
               true /* aRefresh */, csession.GetMachine());

    if (dlg.exec() == QDialog::Accepted)
    {
        CFloppyDrive drv = csession.GetMachine().GetFloppyDrive();

        drv.MountImage (dlg.selectedId());

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

    VBoxMediaManagerDlg dlg (this);

    dlg.setup (VBoxDefs::MediaType_DVD, true /* aDoSelect */,
               true /* aRefresh */, csession.GetMachine());

    if (dlg.exec() == QDialog::Accepted)
    {
        CDVDDrive drv = csession.GetMachine().GetDVDDrive();

        drv.MountImage (dlg.selectedId());

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
    int rc = RTPathAppPrivateNoArch (szAppPrivPath, sizeof (szAppPrivPath));
    AssertRC (rc);

    QString src1 = QString (szAppPrivPath) + "/VBoxGuestAdditions.iso";
    QString src2 = qApp->applicationDirPath() + "/additions/VBoxGuestAdditions.iso";
#endif

    /* Check the standard image locations */
    if (QFile::exists (src1))
        return installGuestAdditionsFrom (src1);
    else if (QFile::exists (src2))
        return installGuestAdditionsFrom (src2);

    /* Check for the already registered image */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    QString name = QString ("VBoxGuestAdditions_%1.iso")
                            .arg (vbox.GetVersion().remove ("_OSE"));

    CDVDImageVector vec = vbox.GetDVDImages();
    for (CDVDImageVector::ConstIterator it = vec.begin();
         it != vec.end(); ++ it)
    {
        QString path = it->GetLocation();
        /* Compare the name part ignoring the file case */
        QString fn = QFileInfo (path).fileName();
        if (RTPathCompare (name.toUtf8().constData(), fn.toUtf8().constData()) == 0)
            return installGuestAdditionsFrom (path);
    }

    /* Download the required image */
    int result = vboxProblem().cannotFindGuestAdditions (
        QDir::toNativeSeparators (src1), QDir::toNativeSeparators (src2));
    if (result == QIMessageBox::Yes)
    {
        QString source = QString ("http://download.virtualbox.org/virtualbox/%1/")
                                  .arg (vbox.GetVersion().remove ("_OSE")) + name;
        QString target = QDir (vboxGlobal().virtualBox().GetHomeFolder())
                               .absoluteFilePath (name);

        VBoxAdditionsDownloader *dl =
            new VBoxAdditionsDownloader (source, target, mDevicesInstallGuestToolsAction);
        statusBar()->addWidget (dl, 0);
        dl->start();
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
            uuid = image.GetId();
    }
    else
        uuid = image.GetId();

    if (!vbox.isOk())
        return vboxProblem().cannotOpenMedium (this, vbox,
                                               VBoxDefs::MediaType_DVD, aSource);

    Assert (!uuid.isNull());
    CDVDDrive drv = csession.GetMachine().GetDVDDrive();
    drv.MountImage (uuid);
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
        ::darwinWindowInvalidateShape (console->viewport());
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
//        /* Save the current region for later processing in the darwin event handler. */
//        mCurrRegion = region;
//        /* We repaint the screen before the ReshapeCustomWindow command. Unfortunately
//         * this command flushes a copy of the backbuffer to the screen after the new
//         * mask is set. This leads into a missplaced drawing of the content. Currently
//         * no alternative to this and also this is not 100% perfect. */
//        repaint();
//        qApp->processEvents();
//        /* Now force the reshaping of the window. This is definitly necessary. */
//        ReshapeCustomWindow (reinterpret_cast <WindowPtr> (winId()));
        QMainWindow::setMask (region);
//        HIWindowInvalidateShadow (::darwinToWindowRef (console->viewport()));
    }
#else
    QMainWindow::setMask (region);
#endif
}

void VBoxConsoleWnd::clearMask()
{
#ifdef Q_WS_WIN
    SetWindowRgn (winId(), 0, TRUE);
#else
    QMainWindow::clearMask();
#endif
}

/**
 *  Prepares the "Mount Floppy..." menu by populating the existent host
 *  floppies.
 */
void VBoxConsoleWnd::prepareFloppyMenu()
{
    if (!console) return;

    mDevicesMountFloppyMenu->clear();

    CHostFloppyDrive selected = csession.GetMachine().GetFloppyDrive().GetHostDrive();

    hostFloppyMap.clear();
    CHostFloppyDriveVector drvvec =
        vboxGlobal().virtualBox().GetHost().GetFloppyDrives();
    for (int i = 0; i < drvvec.size(); ++i)
    {
        CHostFloppyDrive hostFloppy = drvvec[i];
        /** @todo set icon */
        QString drvName = hostFloppy.GetName();
        QString description = hostFloppy.GetDescription();
        QString fullName = description.isEmpty() ?
            drvName :
            QString ("%1 (%2)").arg (description, drvName);
        QAction *action = mDevicesMountFloppyMenu->addAction (tr ("Host Drive ") + fullName);
        action->setStatusTip (tr ("Mount the selected physical drive of the host PC",
                                  "Floppy tip"));
        hostFloppyMap [action] = hostFloppy;
        if (machine_state != KMachineState_Running && machine_state != KMachineState_Paused)
            action->setEnabled (false);
        else if (!selected.isNull())
            if (!selected.GetName().compare (hostFloppy.GetName()))
                action->setEnabled (false);
    }

    if (mDevicesMountFloppyMenu->actions().count() > 0)
        mDevicesMountFloppyMenu->addSeparator();
    mDevicesMountFloppyMenu->addAction (mDevicesMountFloppyImageAction);

    /* if shown as a context menu */
    if(mDevicesMountFloppyMenu->menuAction()->data().toBool())
    {
        mDevicesMountFloppyMenu->addSeparator();
        mDevicesMountFloppyMenu->addAction (mDevicesUnmountFloppyAction);
    }
}

/**
 *  Prepares the "Capture DVD..." menu by populating the existent host
 *  CD/DVD-ROM drives.
 */
void VBoxConsoleWnd::prepareDVDMenu()
{
    if (!console) return;

    mDevicesMountDVDMenu->clear();

    CHostDVDDrive selected = csession.GetMachine().GetDVDDrive().GetHostDrive();

    hostDVDMap.clear();
    CHostDVDDriveVector drvvec =
        vboxGlobal().virtualBox().GetHost().GetDVDDrives();
    for (int i = 0; i < drvvec.size(); ++i)
    {
        CHostDVDDrive hostDVD = drvvec[i];
        /** @todo set icon */
        QString drvName = hostDVD.GetName();
        QString description = hostDVD.GetDescription();
        QString fullName = description.isEmpty() ?
            drvName :
            QString ("%1 (%2)").arg (description, drvName);
        QAction *action = mDevicesMountDVDMenu->addAction (
            tr ("Host Drive ") + fullName);
        action->setStatusTip (tr ("Mount the selected physical drive of the host PC",
                                  "CD/DVD tip"));
        hostDVDMap [action] = hostDVD;
        if (machine_state != KMachineState_Running && machine_state != KMachineState_Paused)
            action->setEnabled (false);
        else if (!selected.isNull())
            if (!selected.GetName().compare (hostDVD.GetName()))
                action->setEnabled (false);
    }

    if (mDevicesMountDVDMenu->actions().count() > 0)
        mDevicesMountDVDMenu->addSeparator();
    mDevicesMountDVDMenu->addAction (mDevicesMountDVDImageAction);

    /* if shown as a context menu */
    if(mDevicesMountDVDMenu->menuAction()->data().toBool())
    {
        mDevicesMountDVDMenu->addSeparator();
        mDevicesMountDVDMenu->addAction (mDevicesUnmountDVDAction);
    }
}

/**
 *  Prepares the "Network adapter" menu by populating the existent adapters.
 */
void VBoxConsoleWnd::prepareNetworkMenu()
{
    mDevicesNetworkMenu->clear();
    ulong count = qMin ((ULONG) 4,
        vboxGlobal().virtualBox().GetSystemProperties().GetNetworkAdapterCount());
    for (ulong slot = 0; slot < count; ++ slot)
    {
        CNetworkAdapter adapter = csession.GetMachine().GetNetworkAdapter (slot);
        QAction *action = mDevicesNetworkMenu->addAction (tr ("Adapter %1", "network").arg (slot+1));
        action->setEnabled (adapter.GetEnabled());
        action->setCheckable (true);
        action->setChecked (adapter.GetEnabled() && adapter.GetCableConnected());
        action->setData (static_cast<qulonglong> (slot));
    }
}

void VBoxConsoleWnd::setDynamicMenuItemStatusTip (QAction *aAction)
{
    QString tip;

    if (sender() == mDevicesNetworkMenu)
    {
        tip = aAction->isChecked() ?
            tr ("Disconnect the cable from the selected virtual network adapter") :
            tr ("Connect the cable to the selected virtual network adapter");
    }

    if (!tip.isNull())
    {
        StatusTipEvent *ev = new StatusTipEvent (tip);
        QApplication::postEvent (this, ev);
        mWaitForStatusBarChange = true;
    }
}

void VBoxConsoleWnd::statusTipChanged (const QString & /*aMes*/)
{
    mStatusBarChangedInside = mWaitForStatusBarChange;
    mWaitForStatusBarChange = false;
}

void VBoxConsoleWnd::clearStatusBar()
{
    if (mStatusBarChangedInside)
        statusBar()->clearMessage();
}

/**
 *  Captures a floppy device corresponding to a given menu action.
 */
void VBoxConsoleWnd::captureFloppy (QAction *aAction)
{
    if (!console) return;

    CHostFloppyDrive d = hostFloppyMap [aAction];
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
 *  Captures a CD/DVD-ROM device corresponding to a given menu action.
 */
void VBoxConsoleWnd::captureDVD (QAction *aAction)
{
    if (!console) return;

    CHostDVDDrive d = hostDVDMap [aAction];
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
void VBoxConsoleWnd::activateNetworkMenu (QAction *aAction)
{
    ulong slot = aAction->data().toULongLong();
    CNetworkAdapter adapter = csession.GetMachine().GetNetworkAdapter (slot);
    bool connected = adapter.GetCableConnected();
    if (adapter.GetEnabled())
        adapter.SetCableConnected (!connected);
}

/**
 *  Attach/Detach selected USB Device.
 */
void VBoxConsoleWnd::switchUSB (QAction *aAction)
{
    if (!console) return;

    CConsole cconsole = csession.GetConsole();
    AssertWrapperOk (csession);

    CUSBDevice usb = mDevicesUSBMenu->getUSB (aAction);
    /* if null then some other item but a USB device is selected */
    if (usb.isNull())
        return;

    if (!aAction->isChecked())
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
        mDevicesMountDVDMenu->menuAction()->setData (true);
        mDevicesMountDVDMenu->exec (e->globalPos());
        mDevicesMountDVDMenu->menuAction()->setData (false);
    }
    else
    if (ind == fd_light)
    {
        /* set "this is a context menu" flag */
        mDevicesMountFloppyMenu->menuAction()->setData (true);
        mDevicesMountFloppyMenu->exec (e->globalPos());
        mDevicesMountFloppyMenu->menuAction()->setData (false);
    }
    else
    if (ind == usb_light)
    {
        if (mDevicesUSBMenu->isEnabled())
        {
            /* set "this is a context menu" flag */
            mDevicesUSBMenu->menuAction()->setData (true);
            mDevicesUSBMenu->exec (e->globalPos());
            mDevicesUSBMenu->menuAction()->setData (false);
        }
    }
    else
    /* if (ind == vrdp_state)
    {
        mDevicesVRDPMenu->exec (e->globalPos());
    }
    else */
    if (ind == autoresize_state)
    {
        mVmAutoresizeMenu->exec (e->globalPos());
    }
    else
    if (ind == mouse_state)
    {
        mVmDisMouseIntegrMenu->exec (e->globalPos());
    }
    else
    if (ind == sf_light)
    {
        /* Showing the context menu that always contains a single item is a
         * bit stupid; let's better execute this item's action directly. The
         * menu itself is kept just in case if we need more than one item in
         * the future. */
        /* mDevicesSFMenu->exec (e->globalPos()); */
        if (mDevicesSFDialogAction->isEnabled())
            mDevicesSFDialogAction->trigger();
    }
    else
    if (ind == net_light)
    {
        if (mDevicesNetworkMenu->isEnabled())
        {
            /* set "this is a context menu" flag */
            mDevicesNetworkMenu->menuAction()->setData (true);
            mDevicesNetworkMenu->exec (e->globalPos());
            mDevicesNetworkMenu->menuAction()->setData (false);
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
                    if (!mVmPauseAction->isChecked())
                        mVmPauseAction->setChecked (true);
                    break;
                }
                case KMachineState_Running:
                {
                    if (machine_state == KMachineState_Paused && mVmPauseAction->isChecked())
                        mVmPauseAction->setChecked (false);
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
                cconsole, QDir::toNativeSeparators (logFolder)))
        {
            qApp->processEvents();
            cconsole.PowerDown();
            if (!cconsole.isOk())
                vboxProblem().cannotStopMachine (cconsole);
        }
    }

#ifdef Q_WS_MAC
    if (console)
        console->updateDockOverlay();
#endif
}

void VBoxConsoleWnd::updateMouseState (int state)
{
    mVmDisableMouseIntegrAction->setEnabled (state & VBoxConsoleView::MouseAbsolute);

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
                                           bool aSeamlessSupported,
                                           bool aGraphicsSupported)
{
    mVmAutoresizeGuestAction->setEnabled (aActive && aGraphicsSupported);
    if (   (mIsSeamlessSupported != aSeamlessSupported)
        || (mIsGraphicsSupported != aGraphicsSupported))
    {
        mVmSeamlessAction->setEnabled (aSeamlessSupported && aGraphicsSupported);
        mIsSeamlessSupported = aSeamlessSupported;
        mIsGraphicsSupported = aGraphicsSupported;
        /* If seamless mode should be enabled then check if it is enabled
         * currently and re-enable it if open-view procedure is finished */
        if (   mVmSeamlessAction->isChecked()
            && mIsOpenViewFinished
            && aSeamlessSupported
            && aGraphicsSupported)
            toggleFullscreenMode (true, true);
        /* Disable auto-resizing if advanced graphics are not available */
        console->setAutoresizeGuest (   mIsGraphicsSupported
                                     && mVmAutoresizeGuestAction->isChecked());
        mVmAutoresizeGuestAction->setEnabled (mIsGraphicsSupported);
    }

    /* Check the GA version only in case of additions are active */
    if (!aActive)
        return;

    /* Check the Guest Additions version and warn the user about possible
     * compatibility issues in case if the installed version is outdated. */
    uint version = aVersion.toUInt();
    QString versionStr = QString ("%1.%2")
        .arg (RT_HIWORD (version)).arg (RT_LOWORD (version));
    QString expectedStr = QString ("%1.%2")
        .arg (VMMDEV_VERSION_MAJOR).arg (VMMDEV_VERSION_MINOR);

    if (RT_HIWORD (version) < VMMDEV_VERSION_MAJOR)
    {
        vboxProblem().warnAboutTooOldAdditions (this, versionStr, expectedStr);
    }
    else if (RT_HIWORD (version) == VMMDEV_VERSION_MAJOR &&
             RT_LOWORD (version) <  VMMDEV_VERSION_MINOR)
    {
        vboxProblem().warnAboutOldAdditions (this, versionStr, expectedStr);
    }
    else if (version > VMMDEV_VERSION)
    {
        vboxProblem().warnAboutNewAdditions (this, versionStr, expectedStr);
    }
}

void VBoxConsoleWnd::updateMediaDriveState (VBoxDefs::MediaType aType)
{
    Assert (aType == VBoxDefs::MediaType_DVD || aType == VBoxDefs::MediaType_Floppy);
    updateAppearanceOf (aType == VBoxDefs::MediaType_DVD ? DVDStuff :
                        aType == VBoxDefs::MediaType_Floppy ? FloppyStuff :
                        AllStuff);
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
    /* First close any open modal & popup widgets. Use a single shot with
     * timeout 0 to allow the widgets to cleany close and test then again. If
     * all open widgets are closed destroy ourself. */

    /// #warning "port me":
    /// @todo (r=dsen) we have to test this on windows

    QWidget *widget = QApplication::activeModalWidget() ?
                      QApplication::activeModalWidget() :
                      QApplication::activePopupWidget() ?
                      QApplication::activePopupWidget() : 0;
    if (widget)
    {
        widget->close();
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
 * Prepare the Debug menu.
 */
void VBoxConsoleWnd::dbgPrepareDebugMenu()
{
#ifdef VBOX_WITH_DEBUGGER_GUI
    /* The "Logging" item. */
    bool fEnabled = false;
    bool fChecked = false;
    CConsole cconsole = csession.GetConsole();
    if (cconsole.isOk())
    {
        CMachineDebugger cdebugger = cconsole.GetDebugger();
        if (cconsole.isOk())
        {
            fEnabled = true;
            fChecked = cdebugger.GetLogEnabled() != FALSE;
        }
    }
    if (fEnabled != mDbgLoggingAction->isEnabled())
        mDbgLoggingAction->setEnabled (fEnabled);
    if (fChecked != mDbgLoggingAction->isChecked())
        mDbgLoggingAction->setChecked (fChecked);
#endif /* VBOX_WITH_DEBUGGER_GUI */
}

/**
 * Called when the Debug->Statistics... menu item is selected.
 */
void VBoxConsoleWnd::dbgShowStatistics()
{
#ifdef VBOX_WITH_DEBUGGER_GUI
    if (dbgCreated())
        mDbgGuiVT->pfnShowStatistics (mDbgGui);
#endif
}

/**
 * Called when the Debug->Command Line... menu item is selected.
 */
void VBoxConsoleWnd::dbgShowCommandLine()
{
#ifdef VBOX_WITH_DEBUGGER_GUI
    if (dbgCreated())
        mDbgGuiVT->pfnShowCommandLine (mDbgGui);
#endif
}

/**
 * Called when the Debug->Logging menu item is selected.
 */
void VBoxConsoleWnd::dbgLoggingToggled(bool aState)
{
    NOREF(aState);
#ifdef VBOX_WITH_DEBUGGER_GUI
    CConsole cconsole = csession.GetConsole();
    if (cconsole.isOk())
    {
        CMachineDebugger cdebugger = cconsole.GetDebugger();
        if (cconsole.isOk())
            cdebugger.SetLogEnabled(aState);
    }
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
    if (mDbgGui)
        return true;

    RTLDRMOD hLdrMod = vboxGlobal().getDebuggerModule();
    if (hLdrMod == NIL_RTLDRMOD)
        return false;

    PFNDBGGUICREATE pfnGuiCreate;
    int rc = RTLdrGetSymbol (hLdrMod, "DBGGuiCreate", (void **)&pfnGuiCreate);
    if (RT_SUCCESS (rc))
    {
        ISession *pISession = csession.raw();
        rc = pfnGuiCreate (pISession, &mDbgGui, &mDbgGuiVT);
        if (RT_SUCCESS (rc))
        {
            if (    DBGGUIVT_ARE_VERSIONS_COMPATIBLE (mDbgGuiVT->u32Version, DBGGUIVT_VERSION)
                ||  mDbgGuiVT->u32EndVersion == mDbgGuiVT->u32Version)
            {
                mDbgGuiVT->pfnSetParent (mDbgGui, (QWidget *)this);
                mDbgGuiVT->pfnSetMenu (mDbgGui, (QMenu *)mDbgMenu);
                dbgAdjustRelativePos();
                return true;
            }

            LogRel (("DBGGuiCreate failed, incompatible versions (loaded %#x/%#x, expected %#x)\n",
                     mDbgGuiVT->u32Version, mDbgGuiVT->u32EndVersion, DBGGUIVT_VERSION));
        }
        else
            LogRel (("DBGGuiCreate failed, rc=%Rrc\n", rc));
    }
    else
        LogRel (("RTLdrGetSymbol(,\"DBGGuiCreate\",) -> %Rrc\n", rc));

    mDbgGui = NULL;
    mDbgGuiVT = NULL;
    return false;
}

/**
 * Destroys the debugger GUI instacne if it has been created.
 */
void VBoxConsoleWnd::dbgDestroy()
{
    if (mDbgGui)
    {
        mDbgGuiVT->pfnDestroy (mDbgGui);
        mDbgGui = NULL;
        mDbgGuiVT = NULL;
    }
}

/**
 * Tells the debugger GUI that the console window has moved or been resized.
 */
void VBoxConsoleWnd::dbgAdjustRelativePos()
{
    if (mDbgGui)
    {
        QRect rct = frameGeometry();
        mDbgGuiVT->pfnAdjustRelativePos (mDbgGui, rct.x(), rct.y(), rct.width(), rct.height());
    }
}

#endif /* VBOX_WITH_DEBUGGER_GUI */

VBoxSFDialog::VBoxSFDialog (QWidget *aParent, CSession &aSession)
    : QIWithRetranslateUI<QDialog> (aParent)
    , mSettings (0)
    , mSession (aSession)
{
    setModal (true);
    /* Setup Dialog's options */
    setWindowIcon (QIcon (":/select_file_16px.png"));
    setSizeGripEnabled (true);

    /* Setup main dialog's layout */
    QVBoxLayout *mainLayout = new QVBoxLayout (this);
    VBoxGlobal::setLayoutMargin (mainLayout, 10);
    mainLayout->setSpacing (10);

    /* Setup settings layout */
    mSettings = new VBoxVMSettingsSF (MachineType | ConsoleType, this);
    VBoxGlobal::setLayoutMargin (mSettings->layout(), 0);
    mSettings->getFromConsole (aSession.GetConsole());
    mSettings->getFromMachine (aSession.GetMachine());
    mainLayout->addWidget (mSettings);

    /* Setup button's layout */
    QIDialogButtonBox *buttonBox = new QIDialogButtonBox (QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help);

    connect (buttonBox, SIGNAL (helpRequested()), &vboxProblem(), SLOT (showHelpHelpDialog()));
    connect (buttonBox, SIGNAL (accepted()), this, SLOT (accept()));
    connect (buttonBox, SIGNAL (rejected()), this, SLOT (reject()));
    mainLayout->addWidget (buttonBox);

    retranslateUi();
}

void VBoxSFDialog::retranslateUi()
{
    setWindowTitle (tr ("Shared Folders"));
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

#include "VBoxConsoleWnd.moc"
