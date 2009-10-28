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

/* Global includes */
#include <QActionGroup>
#include <QDesktopWidget>
#include <QDir>
#include <QFileInfo>
#include <QMenuBar>
#include <QProgressBar>
#include <QTimer>

#ifdef Q_WS_X11
# include <QX11Info>
#endif
#ifdef Q_WS_MAC
# include <QPainter>
#endif

/* Local includes */
#include "QIFileDialog.h"
#include "QIHotKeyEdit.h"
#include "QIHttp.h"
#include "QIStateIndicator.h"
#include "QIStatusBar.h"
#include "QIWidgetValidator.h"
#include "VBoxConsoleWnd.h"
#include "VBoxConsoleView.h"
#include "VBoxCloseVMDlg.h"
#include "VBoxDownloaderWgt.h"
#include "VBoxGlobal.h"
#include "VBoxMediaManagerDlg.h"
#include "VBoxMiniToolBar.h"
#include "VBoxProblemReporter.h"
#include "VBoxTakeSnapshotDlg.h"
#include "VBoxVMFirstRunWzd.h"
#include "VBoxVMSettingsHD.h"
#include "VBoxVMSettingsNetwork.h"
#include "VBoxVMSettingsSF.h"
#include "VBoxVMInformationDlg.h"

#ifdef Q_WS_X11
# include <X11/Xlib.h>
# include <XKeyboard.h>
#endif
#ifdef Q_WS_MAC
# include "VBoxUtils.h"
# include "VBoxIChatTheaterWrapper.h"
# include <ApplicationServices/ApplicationServices.h>
#endif
#ifdef VBOX_WITH_DEBUGGER_GUI
# include <VBox/err.h>
# include <iprt/ldr.h>
#endif

#include <VBox/VMMDev.h> /** @todo @bugref{4084} */
#include <iprt/buildconfig.h>
#include <iprt/param.h>
#include <iprt/path.h>

/* Global forwards */
extern void qt_set_sequence_auto_mnemonic (bool on);

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

                QString target = QIFileDialog::getExistingDirectory (
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

struct MountTarget
{
    MountTarget() : name (QString ("")), port (0), device (0), id (QString ("")), type (VBoxDefs::MediumType_Invalid) {}
    MountTarget (const QString &aName, LONG aPort, LONG aDevice)
        : name (aName), port (aPort), device (aDevice), id (QString ("")), type (VBoxDefs::MediumType_Invalid) {}
    MountTarget (const QString &aName, LONG aPort, LONG aDevice, const QString &aId)
        : name (aName), port (aPort), device (aDevice), id (aId), type (VBoxDefs::MediumType_Invalid) {}
    MountTarget (const QString &aName, LONG aPort, LONG aDevice, const QString &aId, VBoxDefs::MediumType aType)
        : name (aName), port (aPort), device (aDevice), id (aId), type (aType) {}
    QString name;
    LONG port;
    LONG device;
    QString id;
    VBoxDefs::MediumType type;
};
Q_DECLARE_METATYPE (MountTarget);

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
VBoxConsoleWnd::VBoxConsoleWnd (VBoxConsoleWnd **aSelf, QWidget* aParent, Qt::WindowFlags aFlags /* = Qt::Window */)
    : QIWithRetranslateUI2 <QMainWindow> (aParent, aFlags)
    /* Machine State */
    , mMachineState (KMachineState_Null)
    /* Window Variables */
    , mConsoleStyle (0)
    /* Menu Items */
    , mMainMenu (0)
    , mVMMenu (0)
    , mVMMenuMini (0)
    , mDevicesMenu (0)
    , mDevicesCDMenu (0)
    , mDevicesFDMenu (0)
    , mDevicesNetworkMenu (0)
    , mDevicesSFMenu (0)
    , mDevicesUSBMenu (0)
    , mVmDisMouseIntegrMenu (0)
#if 0 /* TODO: Allow to setup status-bar! */
    , mDevicesVRDPMenu (0)
    , mVmAutoresizeMenu (0)
#endif
#ifdef VBOX_WITH_DEBUGGER_GUI
    , mDbgMenu (0)
#endif
    , mHelpMenu (0)
    /* Action Groups */
    , mRunningActions (0)
    , mRunningOrPausedActions (0)
    /* Machine Menu Actions */
    , mVmFullscreenAction (0)
    , mVmSeamlessAction (0)
    , mVmAutoresizeGuestAction (0)
    , mVmAdjustWindowAction (0)
    , mVmDisableMouseIntegrAction (0)
    , mVmTypeCADAction (0)
#ifdef Q_WS_X11
    , mVmTypeCABSAction (0)
#endif
    , mVmTakeSnapshotAction (0)
    , mVmShowInformationDlgAction (0)
    , mVmResetAction (0)
    , mVmPauseAction (0)
    , mVmACPIShutdownAction (0)
    , mVmCloseAction (0)
    /* Device Menu Actions */
    , mDevicesNetworkDialogAction (0)
    , mDevicesSFDialogAction (0)
    , mDevicesSwitchVrdpSeparator (0)
    , mDevicesSwitchVrdpAction (0)
    , mDevicesInstallGuestToolsAction (0)
#ifdef VBOX_WITH_DEBUGGER_GUI
    /* Debug Menu Actions */
    , mDbgStatisticsAction (0)
    , mDbgCommandLineAction (0)
    , mDbgLoggingAction (0)
#endif
    /* Widgets */
    , mConsole (0)
    , mMiniToolBar (0)
#ifdef VBOX_WITH_DEBUGGER_GUI
    , mDbgGui (0)
    , mDbgGuiVT (0)
#endif
    /* LED Update Timer */
    , mIdleTimer (new QTimer (this))
    /* LEDs */
    , mHDLed (0)
    , mCDLed (0)
#if 0 /* TODO: Allow to setup status-bar! */
    , mFDLed (0)
#endif
    , mNetLed (0)
    , mUSBLed (0)
    , mSFLed (0)
    , mVirtLed (0)
    , mMouseLed (0)
    , mHostkeyLed (0)
    , mHostkeyLedContainer (0)
    , mHostkeyName (0)
#if 0 /* TODO: Allow to setup status-bar! */
    , mVrdpLed (0)
    , mAutoresizeLed (0)
#endif
    , mIsOpenViewFinished (false)
    , mIsFirstTimeStarted (false)
    , mIsAutoSaveMedia (true)
    , mNoAutoClose (false)
    , mIsFullscreen (false)
    , mIsSeamless (false)
    , mIsSeamlessSupported (false)
    , mIsGraphicsSupported (false)
    , mIsWaitingModeResize (false)
    , mWasMax (false)
{
    if (aSelf)
        *aSelf = this;

    /* Enumerate mediums to work with cached data */
    vboxGlobal().startEnumeratingMedia();

#if !(defined (Q_WS_WIN) || defined (Q_WS_MAC))
    /* The default application icon (will change to the VM-specific icon in
     * openView()). On Win32, it's built-in to the executable. On Mac OS X the
     * icon referenced in info.plist is used. */
    setWindowIcon (QIcon (":/VirtualBox_48px.png"));
#endif

    /* Ensure status bar is created */
    setStatusBar (new QIStatusBar (this));

    /* A group for all actions that are enabled only when the VM is running.
     * Note that only actions whose enabled state depends exclusively on the
     * execution state of the VM are added to this group. */
    mRunningActions = new QActionGroup (this);
    mRunningActions->setExclusive (false);

    /* A group for all actions that are enabled when the VM is running or
     * paused. Note that only actions whose enabled state depends exclusively
     * on the execution state of the VM are added to this group. */
    mRunningOrPausedActions = new QActionGroup (this);
    mRunningOrPausedActions->setExclusive (false);

    /* VM menu actions */
    mVmFullscreenAction = new QAction (this);
    mVmFullscreenAction->setIcon (VBoxGlobal::iconSetOnOff (
        ":/fullscreen_on_16px.png", ":/fullscreen_16px.png",
        ":/fullscreen_on_disabled_16px.png", ":/fullscreen_disabled_16px.png"));
    mVmFullscreenAction->setCheckable (true);

    mVmSeamlessAction = new QAction (this);
    mVmSeamlessAction->setIcon (VBoxGlobal::iconSetOnOff (
        ":/seamless_on_16px.png", ":/seamless_16px.png",
        ":/seamless_on_disabled_16px.png", ":/seamless_disabled_16px.png"));
    mVmSeamlessAction->setCheckable (true);

    mVmAutoresizeGuestAction = new QAction (mRunningActions);
    mVmAutoresizeGuestAction->setIcon (VBoxGlobal::iconSetOnOff (
        ":/auto_resize_on_on_16px.png", ":/auto_resize_on_16px.png",
        ":/auto_resize_on_on_disabled_16px.png", ":/auto_resize_on_disabled_16px.png"));
    mVmAutoresizeGuestAction->setCheckable (true);
    mVmAutoresizeGuestAction->setEnabled (false);

    mVmAdjustWindowAction = new QAction (this);
    mVmAdjustWindowAction->setIcon (VBoxGlobal::iconSet (
        ":/adjust_win_size_16px.png", ":/adjust_win_size_disabled_16px.png"));

    mVmDisableMouseIntegrAction = new QAction (this);
    mVmDisableMouseIntegrAction->setIcon (VBoxGlobal::iconSetOnOff (
        ":/mouse_can_seamless_on_16px.png", ":/mouse_can_seamless_16px.png",
        ":/mouse_can_seamless_on_disabled_16px.png", ":/mouse_can_seamless_disabled_16px.png"));
    mVmDisableMouseIntegrAction->setCheckable (true);

    mVmTypeCADAction = new QAction (mRunningActions);
    mVmTypeCADAction->setIcon (VBoxGlobal::iconSet (
        ":/hostkey_16px.png", ":/hostkey_disabled_16px.png"));

#if defined(Q_WS_X11)
    mVmTypeCABSAction = new QAction (mRunningActions);
    mVmTypeCABSAction->setIcon (VBoxGlobal::iconSet (
        ":/hostkey_16px.png", ":/hostkey_disabled_16px.png"));
#endif

    mVmTakeSnapshotAction = new QAction (mRunningOrPausedActions);
    mVmTakeSnapshotAction->setIcon (VBoxGlobal::iconSet (
        ":/take_snapshot_16px.png", ":/take_snapshot_dis_16px.png"));

    mVmShowInformationDlgAction = new QAction (this);
    mVmShowInformationDlgAction->setIcon (VBoxGlobal::iconSet (
        ":/session_info_16px.png", ":/session_info_disabled_16px.png"));

    mVmResetAction = new QAction (mRunningActions);
    mVmResetAction->setIcon (VBoxGlobal::iconSet (
        ":/reset_16px.png", ":/reset_disabled_16px.png"));

    mVmPauseAction = new QAction (this);
    mVmPauseAction->setIcon (VBoxGlobal::iconSet (
        ":/pause_16px.png", ":/pause_disabled_16px.png"));
    mVmPauseAction->setCheckable (true);

    mVmACPIShutdownAction = new QAction (mRunningActions);
    mVmACPIShutdownAction->setIcon (VBoxGlobal::iconSet (
        ":/acpi_16px.png", ":/acpi_disabled_16px.png"));

    mVmCloseAction = new QAction (this);
    mVmCloseAction->setMenuRole (QAction::QuitRole);
    mVmCloseAction->setIcon (VBoxGlobal::iconSet (":/exit_16px.png"));

    /* Devices menu actions */
    mDevicesNetworkDialogAction = new QAction (mRunningOrPausedActions);
    mDevicesNetworkDialogAction->setIcon (VBoxGlobal::iconSet (
        ":/nw_16px.png", ":/nw_disabled_16px.png"));

    mDevicesSFDialogAction = new QAction (mRunningOrPausedActions);
    mDevicesSFDialogAction->setIcon (VBoxGlobal::iconSet (
        ":/shared_folder_16px.png", ":/shared_folder_disabled_16px.png"));

    mDevicesSwitchVrdpAction = new QAction (mRunningOrPausedActions);
    mDevicesSwitchVrdpAction->setIcon (VBoxGlobal::iconSetOnOff (
        ":/vrdp_on_16px.png", ":/vrdp_16px.png",
        ":/vrdp_on_disabled_16px.png", ":/vrdp_disabled_16px.png"));
    mDevicesSwitchVrdpAction->setCheckable (true);

    mDevicesInstallGuestToolsAction = new QAction (mRunningActions);
    mDevicesInstallGuestToolsAction->setIcon (VBoxGlobal::iconSet (
        ":/guesttools_16px.png", ":/guesttools_disabled_16px.png"));

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* Debug menu actions */
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
        mDbgStatisticsAction = 0;
        mDbgCommandLineAction = 0;
        mDbgLoggingAction = 0;
    }
#endif

    /* Help menu actions */
    mHelpActions.setup (this);

    /* Menu Items */
    mMainMenu = new QIMenu (this);
    mDevicesCDMenu = new QMenu (this);
    mDevicesFDMenu = new QMenu (this);
    mDevicesNetworkMenu = new QMenu (this);
    mDevicesSFMenu = new QMenu (this);
    mDevicesUSBMenu = new VBoxUSBMenu (this);

    /* Machine submenu */
    mVMMenu = menuBar()->addMenu (QString::null);
    mMainMenu->addMenu (mVMMenu);
    mVmDisMouseIntegrMenu = new VBoxSwitchMenu (mVMMenu, mVmDisableMouseIntegrAction, true);
#if 0 /* TODO: Allow to setup status-bar! */
    mVmAutoresizeMenu = new VBoxSwitchMenu (mVMMenu, mVmAutoresizeGuestAction);
#endif

    mVMMenu->addAction (mVmFullscreenAction);
    mVMMenu->addAction (mVmSeamlessAction);
    mVMMenu->addAction (mVmAutoresizeGuestAction);
    mVMMenu->addAction (mVmAdjustWindowAction);
    mVMMenu->addSeparator();
    mVMMenu->addAction (mVmDisableMouseIntegrAction);
    mVMMenu->addSeparator();
    mVMMenu->addAction (mVmTypeCADAction);
#ifdef Q_WS_X11
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

    mDevicesCDMenu->setIcon (VBoxGlobal::iconSet (":/cd_16px.png", ":/cd_disabled_16px.png"));
    mDevicesFDMenu->setIcon (VBoxGlobal::iconSet (":/fd_16px.png", ":/fd_disabled_16px.png"));
    mDevicesUSBMenu->setIcon (VBoxGlobal::iconSet (":/usb_16px.png", ":/usb_disabled_16px.png"));

    mDevicesMenu->addMenu (mDevicesCDMenu);
    mDevicesMenu->addMenu (mDevicesFDMenu);
    mDevicesMenu->addAction (mDevicesNetworkDialogAction);
    mDevicesMenu->addAction (mDevicesSFDialogAction);
    mDevicesMenu->addMenu (mDevicesUSBMenu);

#if 0 /* TODO: Allow to setup status-bar! */
    mDevicesVRDPMenu = new VBoxSwitchMenu (mDevicesMenu, mDevicesSwitchVrdpAction);
#endif
    mDevicesSwitchVrdpSeparator = mDevicesMenu->addSeparator();
    mDevicesMenu->addAction (mDevicesSwitchVrdpAction);

    mDevicesMenu->addSeparator();
    mDevicesMenu->addAction (mDevicesInstallGuestToolsAction);

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* Debug submenu */
    if (vboxGlobal().isDebuggerEnabled())
    {
        mDbgMenu = menuBar()->addMenu (QString::null);
        mMainMenu->addMenu (mDbgMenu);
        mDbgMenu->addAction (mDbgStatisticsAction);
        mDbgMenu->addAction (mDbgCommandLineAction);
        mDbgMenu->addAction (mDbgLoggingAction);
    }
    else
        mDbgMenu = 0;
#endif

    /* Help submenu */
    mHelpMenu = menuBar()->addMenu (QString::null);
    mMainMenu->addMenu (mHelpMenu);
    mHelpActions.addTo (mHelpMenu);

    /* Machine submenu for mini-toolbar */
    mVMMenuMini = new QMenu (this);
    mVMMenuMini->addAction (mVmTypeCADAction);
#ifdef Q_WS_X11
    mVMMenuMini->addAction (mVmTypeCABSAction);
#endif
    mVMMenuMini->addSeparator();
    mVMMenuMini->addAction (mVmTakeSnapshotAction);
    mVMMenuMini->addSeparator();
    mVMMenuMini->addAction (mVmShowInformationDlgAction);
    mVMMenuMini->addSeparator();
    mVMMenuMini->addAction (mVmResetAction);
    mVMMenuMini->addAction (mVmPauseAction);
    mVMMenuMini->addAction (mVmACPIShutdownAction);

    /* Status bar */
    QWidget *indicatorBox = new QWidget;
    QHBoxLayout *indicatorBoxHLayout = new QHBoxLayout (indicatorBox);
    VBoxGlobal::setLayoutMargin (indicatorBoxHLayout, 0);
    indicatorBoxHLayout->setSpacing (5);

    /* i/o devices */
    mHDLed = new QIStateIndicator (KDeviceActivity_Idle);
    mHDLed->setStateIcon (KDeviceActivity_Idle, QPixmap (":/hd_16px.png"));
    mHDLed->setStateIcon (KDeviceActivity_Reading, QPixmap (":/hd_read_16px.png"));
    mHDLed->setStateIcon (KDeviceActivity_Writing, QPixmap (":/hd_write_16px.png"));
    mHDLed->setStateIcon (KDeviceActivity_Null, QPixmap (":/hd_disabled_16px.png"));
    indicatorBoxHLayout->addWidget (mHDLed);
    mCDLed = new QIStateIndicator (KDeviceActivity_Idle);
    mCDLed->setStateIcon (KDeviceActivity_Idle, QPixmap (":/cd_16px.png"));
    mCDLed->setStateIcon (KDeviceActivity_Reading, QPixmap (":/cd_read_16px.png"));
    mCDLed->setStateIcon (KDeviceActivity_Writing, QPixmap (":/cd_write_16px.png"));
    mCDLed->setStateIcon (KDeviceActivity_Null, QPixmap (":/cd_disabled_16px.png"));
    indicatorBoxHLayout->addWidget (mCDLed);
#if 0 /* TODO: Allow to setup status-bar! */
    mFDLed = new QIStateIndicator (KDeviceActivity_Idle);
    mFDLed->setStateIcon (KDeviceActivity_Idle, QPixmap (":/fd_16px.png"));
    mFDLed->setStateIcon (KDeviceActivity_Reading, QPixmap (":/fd_read_16px.png"));
    mFDLed->setStateIcon (KDeviceActivity_Writing, QPixmap (":/fd_write_16px.png"));
    mFDLed->setStateIcon (KDeviceActivity_Null, QPixmap (":/fd_disabled_16px.png"));
    indicatorBoxHLayout->addWidget (mFDLed);
#endif
    mNetLed = new QIStateIndicator (KDeviceActivity_Idle);
    mNetLed->setStateIcon (KDeviceActivity_Idle, QPixmap (":/nw_16px.png"));
    mNetLed->setStateIcon (KDeviceActivity_Reading, QPixmap (":/nw_read_16px.png"));
    mNetLed->setStateIcon (KDeviceActivity_Writing, QPixmap (":/nw_write_16px.png"));
    mNetLed->setStateIcon (KDeviceActivity_Null, QPixmap (":/nw_disabled_16px.png"));
    indicatorBoxHLayout->addWidget (mNetLed);
    mUSBLed = new QIStateIndicator (KDeviceActivity_Idle);
    mUSBLed->setStateIcon (KDeviceActivity_Idle, QPixmap (":/usb_16px.png"));
    mUSBLed->setStateIcon (KDeviceActivity_Reading, QPixmap (":/usb_read_16px.png"));
    mUSBLed->setStateIcon (KDeviceActivity_Writing, QPixmap (":/usb_write_16px.png"));
    mUSBLed->setStateIcon (KDeviceActivity_Null, QPixmap (":/usb_disabled_16px.png"));
    indicatorBoxHLayout->addWidget (mUSBLed);
    mSFLed = new QIStateIndicator (KDeviceActivity_Idle);
    mSFLed->setStateIcon (KDeviceActivity_Idle, QPixmap (":/shared_folder_16px.png"));
    mSFLed->setStateIcon (KDeviceActivity_Reading, QPixmap (":/shared_folder_read_16px.png"));
    mSFLed->setStateIcon (KDeviceActivity_Writing, QPixmap (":/shared_folder_write_16px.png"));
    mSFLed->setStateIcon (KDeviceActivity_Null, QPixmap (":/shared_folder_disabled_16px.png"));
    indicatorBoxHLayout->addWidget (mSFLed);

    /* virtualization */
    mVirtLed = new QIStateIndicator (0);
    mVirtLed->setStateIcon (0, QPixmap (":/vtx_amdv_disabled_16px.png"));
    mVirtLed->setStateIcon (1, QPixmap (":/vtx_amdv_16px.png"));
    indicatorBoxHLayout->addWidget (mVirtLed);

    QFrame *separator = new QFrame();
    separator->setFrameStyle (QFrame::VLine | QFrame::Sunken);
    indicatorBoxHLayout->addWidget (separator);

    /* mouse */
    mMouseLed = new QIStateIndicator (0);
    mMouseLed->setStateIcon (0, QPixmap (":/mouse_disabled_16px.png"));
    mMouseLed->setStateIcon (1, QPixmap (":/mouse_16px.png"));
    mMouseLed->setStateIcon (2, QPixmap (":/mouse_seamless_16px.png"));
    mMouseLed->setStateIcon (3, QPixmap (":/mouse_can_seamless_16px.png"));
    mMouseLed->setStateIcon (4, QPixmap (":/mouse_can_seamless_uncaptured_16px.png"));
    indicatorBoxHLayout->addWidget (mMouseLed);

    /* host key */
    mHostkeyLedContainer = new QWidget;
    QHBoxLayout *hostkeyLEDContainerLayout = new QHBoxLayout (mHostkeyLedContainer);
    VBoxGlobal::setLayoutMargin (hostkeyLEDContainerLayout, 0);
    hostkeyLEDContainerLayout->setSpacing (3);
    indicatorBoxHLayout->addWidget (mHostkeyLedContainer);

    mHostkeyLed = new QIStateIndicator (0);
    mHostkeyLed->setStateIcon (0, QPixmap (":/hostkey_16px.png"));
    mHostkeyLed->setStateIcon (1, QPixmap (":/hostkey_captured_16px.png"));
    mHostkeyLed->setStateIcon (2, QPixmap (":/hostkey_pressed_16px.png"));
    mHostkeyLed->setStateIcon (3, QPixmap (":/hostkey_captured_pressed_16px.png"));
    hostkeyLEDContainerLayout->addWidget (mHostkeyLed);
    mHostkeyName = new QLabel (QIHotKeyEdit::keyName (vboxGlobal().settings().hostKey()));
    hostkeyLEDContainerLayout->addWidget (mHostkeyName);

#if 0 /* TODO: Allow to setup status-bar! */
    /* VRDP Led */
    mVrdpLed = new QIStateIndicator (0, indicatorBox, "mVrdpLed", Qt::WNoAutoErase);
    mVrdpLed->setStateIcon (0, QPixmap (":/vrdp_disabled_16px.png"));
    mVrdpLed->setStateIcon (1, QPixmap (":/vrdp_16px.png"));
    /* Auto-Resize LED */
    mAutoresizeLed = new QIStateIndicator (1, indicatorBox, "mAutoresizeLed", Qt::WNoAutoErase);
    mAutoresizeLed->setStateIcon (0, QPixmap (":/auto_resize_off_disabled_16px.png"));
    mAutoresizeLed->setStateIcon (1, QPixmap (":/auto_resize_off_16px.png"));
    mAutoresizeLed->setStateIcon (2, QPixmap (":/auto_resize_on_disabled_16px.png"));
    mAutoresizeLed->setStateIcon (3, QPixmap (":/auto_resize_on_16px.png"));
#endif

    /* add to statusbar */
    statusBar()->addPermanentWidget (indicatorBox, 0);

    /* Retranslate UI */
    retranslateUi();

    setWindowTitle (mCaptionPrefix);

    /* Connections */
    connect (mVmFullscreenAction, SIGNAL (toggled (bool)), this, SLOT (vmFullscreen (bool)));
    connect (mVmSeamlessAction, SIGNAL (toggled (bool)), this, SLOT (vmSeamless (bool)));
    connect (mVmAutoresizeGuestAction, SIGNAL (toggled (bool)), this, SLOT (vmAutoresizeGuest (bool)));
    connect (mVmAdjustWindowAction, SIGNAL (triggered()), this, SLOT (vmAdjustWindow()));
    connect (mVmDisableMouseIntegrAction, SIGNAL (toggled (bool)), this, SLOT (vmDisableMouseIntegration (bool)));
    connect (mVmTypeCADAction, SIGNAL (triggered()), this, SLOT (vmTypeCAD()));
#ifdef Q_WS_X11
    connect (mVmTypeCABSAction, SIGNAL (triggered()), this, SLOT (vmTypeCABS()));
#endif
    connect (mVmTakeSnapshotAction, SIGNAL (triggered()), this, SLOT (vmTakeSnapshot()));
    connect (mVmShowInformationDlgAction, SIGNAL (triggered()), this, SLOT (vmShowInfoDialog()));
    connect (mVmResetAction, SIGNAL (triggered()), this, SLOT (vmReset()));
    connect (mVmPauseAction, SIGNAL (toggled (bool)), this, SLOT (vmPause (bool)));
    connect (mVmACPIShutdownAction, SIGNAL (triggered()), this, SLOT (vmACPIShutdown()));
    connect (mVmCloseAction, SIGNAL (triggered()), this, SLOT (vmClose()));

    connect (mDevicesCDMenu, SIGNAL (aboutToShow()), this, SLOT (prepareStorageMenu()));
    connect (mDevicesFDMenu, SIGNAL (aboutToShow()), this, SLOT (prepareStorageMenu()));
    connect (mDevicesNetworkMenu, SIGNAL (aboutToShow()), this, SLOT (prepareNetworkMenu()));
    connect (mDevicesSFMenu, SIGNAL (aboutToShow()), this, SLOT (prepareSFMenu()));
    connect (mDevicesUSBMenu, SIGNAL(triggered (QAction *)), this, SLOT(switchUSB (QAction *)));

    connect (mDevicesNetworkDialogAction, SIGNAL (triggered()), this, SLOT (devicesOpenNetworkDialog()));
    connect (mDevicesSFDialogAction, SIGNAL (triggered()), this, SLOT (devicesOpenSFDialog()));
    connect (mDevicesSwitchVrdpAction, SIGNAL (toggled (bool)), this, SLOT (devicesSwitchVrdp (bool)));
    connect (mDevicesInstallGuestToolsAction, SIGNAL (triggered()), this, SLOT (devicesInstallGuestAdditions()));

    connect (mCDLed, SIGNAL (contextMenuRequested (QIStateIndicator *, QContextMenuEvent *)),
             this, SLOT (showIndicatorContextMenu (QIStateIndicator *, QContextMenuEvent *)));
#if 0 /* TODO: Allow to setup status-bar! */
    connect (mFDLed, SIGNAL (contextMenuRequested (QIStateIndicator *, QContextMenuEvent *)),
             this, SLOT (showIndicatorContextMenu (QIStateIndicator *, QContextMenuEvent *)));
#endif
    connect (mNetLed, SIGNAL (contextMenuRequested (QIStateIndicator *, QContextMenuEvent *)),
             this, SLOT (showIndicatorContextMenu (QIStateIndicator *, QContextMenuEvent *)));
    connect (mUSBLed, SIGNAL (contextMenuRequested (QIStateIndicator *, QContextMenuEvent *)),
             this, SLOT (showIndicatorContextMenu (QIStateIndicator *, QContextMenuEvent *)));
    connect (mSFLed, SIGNAL (contextMenuRequested (QIStateIndicator *, QContextMenuEvent *)),
             this, SLOT (showIndicatorContextMenu (QIStateIndicator *, QContextMenuEvent *)));
    connect (mMouseLed, SIGNAL (contextMenuRequested (QIStateIndicator *, QContextMenuEvent *)),
             this, SLOT (showIndicatorContextMenu (QIStateIndicator *, QContextMenuEvent *)));
#if 0 /* TODO: Allow to setup status-bar! */
    connect (mVrdpLed, SIGNAL (contextMenuRequested (QIStateIndicator *, QContextMenuEvent *)),
             this, SLOT (showIndicatorContextMenu (QIStateIndicator *, QContextMenuEvent *)));
    connect (mAutoresizeLed, SIGNAL (contextMenuRequested (QIStateIndicator *, QContextMenuEvent *)),
             this, SLOT (showIndicatorContextMenu (QIStateIndicator *, QContextMenuEvent *)));
#endif

    /* Watch global settings changes */
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
    // int setAttr[] = { kHIWindowBitDoesNotShowBadgeInDock, 0 };
    // HIWindowChangeAttributes (window, setAttr, 0);
    initSharedAVManager();
# endif
#endif

    mMaskShift.scale (0, 0, Qt::IgnoreAspectRatio);
}

VBoxConsoleWnd::~VBoxConsoleWnd()
{
    closeView();

#ifdef VBOX_WITH_DEBUGGER_GUI
    dbgDestroy();
#endif
}

/**
 *  Opens a new console view to interact with a given VM.
 *  Does nothing if the console view is already opened.
 *  Used by VBoxGlobal::startMachine(), should not be called directly.
 */
bool VBoxConsoleWnd::openView (const CSession &aSession)
{
    LogFlowFuncEnter();

    if (mConsole)
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

    mSession = aSession;

    if (!centralWidget())
    {
        setCentralWidget (new QWidget (this));
        QGridLayout *pMainLayout = new QGridLayout (centralWidget());
        VBoxGlobal::setLayoutMargin (pMainLayout, 0);
        pMainLayout->setSpacing (0);

        mShiftingSpacerLeft = new QSpacerItem (0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        mShiftingSpacerTop = new QSpacerItem (0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        mShiftingSpacerRight = new QSpacerItem (0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        mShiftingSpacerBottom = new QSpacerItem (0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        pMainLayout->addItem (mShiftingSpacerTop, 0, 0, 1, -1);
        pMainLayout->addItem (mShiftingSpacerLeft, 1, 0);
        pMainLayout->addItem (mShiftingSpacerRight, 1, 2);
        pMainLayout->addItem (mShiftingSpacerBottom, 2, 0, 1, -1);
    }

    mVmPauseAction->setChecked (false);

    CConsole console = mSession.GetConsole();
    AssertWrapperOk (mSession);

    CMachine machine = mSession.GetMachine();

#ifdef VBOX_WITH_VIDEOHWACCEL
    /* Need to force the QGL framebuffer in case 2D Video Acceleration is supported & enabled */
    bool bAccelerate2DVideo = machine.GetAccelerate2DVideoEnabled() && VBoxGlobal::isAcceleration2DVideoAvailable();
#endif

    mConsole = new VBoxConsoleView (this, console, vboxGlobal().vmRenderMode(),
#ifdef VBOX_WITH_VIDEOHWACCEL
                                    bAccelerate2DVideo,
#endif
                                    centralWidget());
    qobject_cast <QGridLayout*> (centralWidget()->layout())->addWidget (mConsole, 1, 1, Qt::AlignVCenter | Qt::AlignHCenter);

    /* Mini toolbar */
    bool isActive = !(machine.GetExtraData (VBoxDefs::GUI_ShowMiniToolBar) == "no");
    bool isAtTop = (machine.GetExtraData (VBoxDefs::GUI_MiniToolBarAlignment) == "top");
    bool isAutoHide = !(machine.GetExtraData (VBoxDefs::GUI_MiniToolBarAutoHide) == "off");
    QList <QMenu*> menus (QList <QMenu*>() << mVMMenuMini << mDevicesMenu);
    mMiniToolBar = new VBoxMiniToolBar (centralWidget(), isAtTop ? VBoxMiniToolBar::AlignTop : VBoxMiniToolBar::AlignBottom,
                                        isActive, isAutoHide);
    *mMiniToolBar << menus;
    connect (mMiniToolBar, SIGNAL (exitAction()), this, SLOT (mtExitMode()));
    connect (mMiniToolBar, SIGNAL (closeAction()), this, SLOT (mtCloseVM()));
    connect (mMiniToolBar, SIGNAL (geometryUpdated()), this, SLOT (mtMaskUpdate()));
    connect (this, SIGNAL (closing()), mMiniToolBar, SLOT (close()));

    activateUICustomizations();

    /* Set the VM-specific application icon */
    /* Not on Mac OS X. The dock icon is handled below. */
#ifndef Q_WS_MAC
    setWindowIcon (vboxGlobal().vmGuestOSTypeIcon (machine.GetOSTypeId()));
#endif

    /* Restore the position of the window and some options */
    {
        QString str = machine.GetExtraData (VBoxDefs::GUI_LastWindowPosition);

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
            mConsole->normalizeGeometry (true /* adjustPosition */);

            if (max)
            {
                /* Maximize if needed */
                setWindowState (windowState() | Qt::WindowMaximized);
                mWasMax = max;
            }
        }
        else
        {
            /* Normalize to the optimal size */
            mConsole->normalizeGeometry (true /* adjustPosition */);

            /* Move newly created window to the screen center. */
            mNormalGeo = geometry();
            mNormalGeo.moveCenter (ar.center());
            setGeometry (mNormalGeo);
        }

        show();

        /* Process show & possible maximize events */
        qApp->processEvents();

        mVmSeamlessAction->setEnabled (false);
        str = machine.GetExtraData (VBoxDefs::GUI_Seamless);
        if (str == "on")
            mVmSeamlessAction->setChecked (true);

        str = machine.GetExtraData (VBoxDefs::GUI_AutoresizeGuest);
        if (str != "off")
            mVmAutoresizeGuestAction->setChecked (true);

        str = machine.GetExtraData (VBoxDefs::GUI_FirstRun);
        if (str == "yes")
            mIsFirstTimeStarted = true;
        else if (!str.isEmpty())
            machine.SetExtraData (VBoxDefs::GUI_FirstRun, QString::null);

        str = machine.GetExtraData (VBoxDefs::GUI_SaveMountedAtRuntime);
        if (str == "no")
            mIsAutoSaveMedia = false;

        /* Check if one of extended modes to be activated on loading */
        QString fsMode = machine.GetExtraData (VBoxDefs::GUI_Fullscreen);
        QString slMode = machine.GetExtraData (VBoxDefs::GUI_Seamless);
        bool extendedMode = fsMode == "on" || slMode == "on";

        /* If one of extended modes to be loaded we have to ignore default
         * console resize event which will come from VGA Device on loading. */
        if (extendedMode)
            mConsole->requestToResize (QSize (w, h - menuBar()->height() - statusBar()->height()));
    }

    /* initialize storage stuff */
    int cdDevicesCount = 0;
    int fdDevicesCount = 0;
    const CMediumAttachmentVector &attachments = machine.GetMediumAttachments();
    foreach (const CMediumAttachment &attachment, attachments)
    {
        if (attachment.GetType() == KDeviceType_DVD)
            ++ cdDevicesCount;
        if (attachment.GetType() == KDeviceType_Floppy)
            ++ fdDevicesCount;
    }
    mDevicesCDMenu->menuAction()->setData (cdDevicesCount);
    mDevicesFDMenu->menuAction()->setData (fdDevicesCount);
    mDevicesCDMenu->menuAction()->setVisible (cdDevicesCount);
    mDevicesFDMenu->menuAction()->setVisible (fdDevicesCount);

    /* initialize usb stuff */
    CUSBController usbctl = machine.GetUSBController();
    if (usbctl.isNull())
    {
        /* hide usb_menu & usb_separator & usb_status_led */
        mDevicesUSBMenu->setVisible (false);
        mUSBLed->setHidden (true);
    }
    else
    {
        bool isUSBEnabled = usbctl.GetEnabled();
        mDevicesUSBMenu->setEnabled (isUSBEnabled);
        mDevicesUSBMenu->setConsole (console);
        mUSBLed->setState (isUSBEnabled ? KDeviceActivity_Idle : KDeviceActivity_Null);
    }

    /* initialize vrdp stuff */
    CVRDPServer vrdpsrv = machine.GetVRDPServer();
    if (vrdpsrv.isNull())
    {
        /* hide vrdp_menu_action & vrdp_separator & vrdp_status_icon */
        mDevicesSwitchVrdpAction->setVisible (false);
        mDevicesSwitchVrdpSeparator->setVisible (false);
#if 0 /* TODO: Allow to setup status-bar! */
        mVrdpLed->setHidden (true);
#endif
    }

    /* start an idle timer that will update device lighths */
    connect (mIdleTimer, SIGNAL (timeout()), SLOT (updateDeviceLights()));
    mIdleTimer->start (50);

    connect (mConsole, SIGNAL (mouseStateChanged (int)), this, SLOT (updateMouseState (int)));
    connect (mConsole, SIGNAL (keyboardStateChanged (int)), mHostkeyLed, SLOT (setState (int)));
    connect (mConsole, SIGNAL (machineStateChanged (KMachineState)), this, SLOT (updateMachineState (KMachineState)));
    connect (mConsole, SIGNAL (additionsStateChanged (const QString&, bool, bool, bool)),
             this, SLOT (updateAdditionsState (const QString &, bool, bool, bool)));
    connect (mConsole, SIGNAL (mediaDriveChanged (VBoxDefs::MediumType)),
             this, SLOT (updateMediaDriveState (VBoxDefs::MediumType)));
    connect (mConsole, SIGNAL (usbStateChange()), this, SLOT (updateUsbState()));
    connect (mConsole, SIGNAL (networkStateChange()), this, SLOT (updateNetworkAdaptersState()));
    connect (mConsole, SIGNAL (sharedFoldersChanged()), this, SLOT (updateSharedFoldersState()));

#ifdef Q_WS_MAC
    QString testStr = vboxGlobal().virtualBox().GetExtraData (VBoxDefs::GUI_RealtimeDockIconUpdateEnabled).toLower();
    /* Default to true if it is an empty value */
    bool f = (testStr.isEmpty() || testStr == "true");
    mConsole->setDockIconEnabled (f);
    mConsole->updateDockOverlay();
#endif

    /* set the correct initial mMachineState value */
    mMachineState = console.GetState();

    mConsole->normalizeGeometry (false /* adjustPosition */);

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

void VBoxConsoleWnd::setMouseIntegrationLocked (bool aDisabled)
{
    mVmDisableMouseIntegrAction->setChecked (false);
    mVmDisableMouseIntegrAction->setEnabled (aDisabled);
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

void VBoxConsoleWnd::installGuestAdditionsFrom (const QString &aSource)
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    QString uuid;

    CMedium image = vbox.FindDVDImage (aSource);
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
                                               VBoxDefs::MediumType_DVD, aSource);

    Assert (!uuid.isNull());
    CMachine m = mSession.GetMachine();

    QString ctrName;
    LONG ctrPort = -1, ctrDevice = -1;
    /* Searching for the first suitable slot */
    {
        CStorageControllerVector controllers = m.GetStorageControllers();
        int i = 0;
        while (i < controllers.size() && ctrName.isNull())
        {
            CStorageController controller = controllers [i];
            CMediumAttachmentVector attachments = m.GetMediumAttachmentsOfController (controller.GetName());
            int j = 0;
            while (j < attachments.size() && ctrName.isNull())
            {
                CMediumAttachment attachment = attachments [j];
                if (attachment.GetType() == KDeviceType_DVD)
                {
                    ctrName = controller.GetName();
                    ctrPort = attachment.GetPort();
                    ctrDevice = attachment.GetDevice();
                }
                ++ j;
            }
            ++ i;
        }
    }

    if (!ctrName.isNull())
    {
        m.MountMedium (ctrName, ctrPort, ctrDevice, uuid);
        AssertWrapperOk (m);
        if (m.isOk())
        {
            if (mIsAutoSaveMedia)
            {
                m.SaveSettings();
                if (!m.isOk())
                    vboxProblem().cannotSaveMachineSettings (m);
            }
        }
    }
    else
    {
        /* TODO: Make warning about DVD is missing! */
    }
}

void VBoxConsoleWnd::setMask (const QRegion &aRegion)
{
    QRegion region = aRegion;

    /* The global mask shift cause of toolbars and such things. */
    region.translate (mMaskShift.width(), mMaskShift.height());

    /* Including mini toolbar area */
    QRegion toolBarRegion (mMiniToolBar->mask());
    toolBarRegion.translate (mMiniToolBar->mapToGlobal (toolBarRegion.boundingRect().topLeft()) - QPoint (1, 0));
    region += toolBarRegion;

    /* Restrict the drawing to the available space on the screen.
     * (The &operator is better than the previous used -operator,
     * because this excludes space around the real screen also.
     * This is necessary for the mac.) */
    region &= mStrictedRegion;

#ifdef Q_WS_WIN
    QRegion difference = mPrevRegion.subtract (region);

    /* Region offset calculation */
    int fleft = 0, ftop = 0;

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
    RedrawWindow (0, 0, diffReg, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    RedrawWindow (mConsole->viewport()->winId(), 0, 0, RDW_INVALIDATE);

    mPrevRegion = region;
#elif defined (Q_WS_MAC)
# if defined (VBOX_GUI_USE_QUARTZ2D)
    if (vboxGlobal().vmRenderMode() == VBoxDefs::Quartz2DMode)
    {
        /* If we are using the Quartz2D backend we have to trigger
         * an repaint only. All the magic clipping stuff is done
         * in the paint engine. */
        ::darwinWindowInvalidateShape (mConsole->viewport());
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
        // /* Save the current region for later processing in the darwin event handler. */
        // mCurrRegion = region;
        // /* We repaint the screen before the ReshapeCustomWindow command. Unfortunately
        //  * this command flushes a copy of the backbuffer to the screen after the new
        //  * mask is set. This leads into a missplaced drawing of the content. Currently
        //  * no alternative to this and also this is not 100% perfect. */
        // repaint();
        // qApp->processEvents();
        // /* Now force the reshaping of the window. This is definitly necessary. */
        // ReshapeCustomWindow (reinterpret_cast <WindowPtr> (winId()));
        QMainWindow::setMask (region);
        // HIWindowInvalidateShadow (::darwinToWindowRef (mConsole->viewport()));
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

bool VBoxConsoleWnd::event (QEvent *aEvent)
{
    switch (aEvent->type())
    {
        /* By handling every Resize and Move we keep track of the normal
         * (non-minimized and non-maximized) window geometry. Shame on Qt
         * that it doesn't provide this geometry in its public APIs. */

        case QEvent::Resize:
        {
            QResizeEvent *re = (QResizeEvent *) aEvent;

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
            StatusTipEvent *ev = (StatusTipEvent*) aEvent;
            statusBar()->showMessage (ev->mTip);
            break;
        }
        default:
            break;
    }

    return QMainWindow::event (aEvent);
}

void VBoxConsoleWnd::closeEvent (QCloseEvent *aEvent)
{
    LogFlowFuncEnter();

    static const char *kSave = "save";
    static const char *kShutdown = "shutdown";
    static const char *kPowerOff = "powerOff";
    static const char *kDiscardCurState = "discardCurState";

    if (!mConsole)
    {
        aEvent->accept();
        LogFlowFunc (("Console already destroyed!"));
        LogFlowFuncLeave();
        return;
    }

    if (mMachineState > KMachineState_Paused && mMachineState != KMachineState_Stuck)
    {
        /* The machine is in some temporary state like Saving or Stopping.
         * Ignore the close event. When it is Stopping, it will be soon closed anyway from updateMachineState().
         * In all other cases, an appropriate progress dialog will be shown within a few seconds. */
        aEvent->ignore();
    }
    else if (mMachineState < KMachineState_Running)
    {
        /* The machine has been already powered off or saved or aborted -- close the window immediately. */
        aEvent->accept();
    }
    else
    {
        /* Start with ignore the close event */
        aEvent->ignore();

        bool isACPIEnabled = mSession.GetConsole().GetGuestEnteredACPIMode();

        bool success = true;

        bool wasPaused = mMachineState == KMachineState_Paused || mMachineState == KMachineState_Stuck;
        if (!wasPaused)
        {
            /* Suspend the VM and ignore the close event if failed to do so.
             * pause() will show the error message to the user. */
            success = mConsole->pause (true);
        }

        if (success)
        {
            success = false;

            CMachine machine = mSession.GetMachine();
            VBoxCloseVMDlg dlg (this);
            QString typeId = machine.GetOSTypeId();
            dlg.pmIcon->setPixmap (vboxGlobal().vmGuestOSTypeIcon (typeId));

            /* Make the Discard checkbox invisible if there are no snapshots */
            dlg.mCbDiscardCurState->setVisible (machine.GetSnapshotCount() > 0);

            if (mMachineState != KMachineState_Stuck)
            {
                /* Read the last user's choice for the given VM */
                QStringList lastAction = machine.GetExtraData (VBoxDefs::GUI_LastCloseAction).split (',');
                AssertWrapperOk (machine);
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
                else /* The default is ACPI Shutdown */
                {
                    dlg.mRbShutdown->setChecked (true);
                    dlg.mRbShutdown->setFocus();
                }
                dlg.mCbDiscardCurState->setChecked (lastAction.count() > 1 && lastAction [1] == kDiscardCurState);
            }
            else
            {
                /* The stuck VM can only be powered off; disable anything else and choose PowerOff */
                dlg.mRbSave->setEnabled (false);
                dlg.mRbShutdown->setEnabled (false);
                dlg.mRbPowerOff->setChecked (true);
            }

            bool wasShutdown = false;

            if (dlg.exec() == QDialog::Accepted)
            {
                /* Disable auto closure because we want to have a chance to show
                 * the error dialog on save state / power off failure. */
                mNoAutoClose = true;

                CConsole console = mConsole->console();

                if (dlg.mRbSave->isChecked())
                {
                    CProgress progress = console.SaveState();

                    if (console.isOk())
                    {
                        /* Show the "VM saving" progress dialog */
                        vboxProblem().showModalProgressDialog (progress, machine.GetName(), this, 0);
                        if (progress.GetResultCode() != 0)
                            vboxProblem().cannotSaveMachineState (progress);
                        else
                            success = true;
                    }
                    else
                        vboxProblem().cannotSaveMachineState (console);
                }
                else if (dlg.mRbShutdown->isChecked())
                {
                    /* Unpause the VM to let it grab the ACPI shutdown event */
                    mConsole->pause (false);
                    /* Prevent the subsequent unpause request */
                    wasPaused = true;
                    /* Signal ACPI shutdown (if there is no ACPI device, the
                     * operation will fail) */
                    console.PowerButton();
                    wasShutdown = console.isOk();
                    if (!wasShutdown)
                        vboxProblem().cannotACPIShutdownMachine (console);
                    /* Success is always false because we never accept the close
                     * window action when doing ACPI shutdown */
                    success = false;
                }
                else if (dlg.mRbPowerOff->isChecked())
                {
                    CProgress progress = console.PowerDown();

                    if (console.isOk())
                    {
                        /* Show the power down progress dialog */
                        vboxProblem().showModalProgressDialog (progress, machine.GetName(), this, 0);
                        if (progress.GetResultCode() != 0)
                            vboxProblem().cannotStopMachine (progress);
                        else
                            success = true;
                    }
                    else
                        vboxProblem().cannotStopMachine (console);

                    if (success)
                    {
                        /* Note: leave success = true even if we fail to
                         * discard the current state later -- the console window
                         * will closed anyway */

                        /* Discard the current state if requested */
                        if (dlg.mCbDiscardCurState->isChecked() && dlg.mCbDiscardCurState->isVisibleTo (&dlg))
                        {
                            CSnapshot snapshot = machine.GetCurrentSnapshot();
                            CProgress progress = console.RestoreSnapshot (snapshot);
                            if (console.isOk())
                            {
                                /* Show the progress dialog */
                                vboxProblem().showModalProgressDialog (progress, machine.GetName(), this);
                                if (progress.GetResultCode() != 0)
                                    vboxProblem().cannotRestoreSnapshot (progress, snapshot.GetName());
                            }
                            else
                                vboxProblem().cannotRestoreSnapshot (console, snapshot.GetName());
                        }
                    }
                }

                if (success)
                {
                    /* Accept the close action on success */
                    aEvent->accept();
                }

                if (success || wasShutdown)
                {
                    /* Read the last user's choice for the given VM */
                    QStringList prevAction = machine.GetExtraData (VBoxDefs::GUI_LastCloseAction).split (',');
                    /* Memorize the last user's choice for the given VM */
                    QString lastAction = kPowerOff;
                    if (dlg.mRbSave->isChecked())
                        lastAction = kSave;
                    else if (dlg.mRbShutdown->isChecked() ||
                             (dlg.mRbPowerOff->isChecked() && prevAction [0] == kShutdown && !isACPIEnabled))
                        lastAction = kShutdown;
                    else if (dlg.mRbPowerOff->isChecked())
                        lastAction = kPowerOff;
                    else
                        AssertFailed();
                    if (dlg.mCbDiscardCurState->isChecked())
                        (lastAction += ",") += kDiscardCurState;
                    machine.SetExtraData (VBoxDefs::GUI_LastCloseAction, lastAction);
                    AssertWrapperOk (machine);
                }
            }
        }

        mNoAutoClose = false;

        if (mMachineState < KMachineState_Running)
        {
            /* The machine has been stopped while showing the Close or the Pause
             * failure dialog -- accept the close event immediately. */
            aEvent->accept();
        }
        else
        {
            if (!success)
            {
                /* Restore the running state if needed */
                if (!wasPaused && mMachineState == KMachineState_Paused)
                    mConsole->pause (false);
            }
        }
    }

    if (aEvent->isAccepted())
    {
#ifndef VBOX_GUI_SEPARATE_VM_PROCESS
        vboxGlobal().selectorWnd().show();
#endif

        /* Stop LED update timer */
        mIdleTimer->stop();
        mIdleTimer->disconnect (SIGNAL (timeout()), this, SLOT (updateDeviceLights()));

        /* Hide console window */
        hide();

        /* Save the position of the window and some options */
        CMachine machine = mSession.GetMachine();
        QString winPos = QString ("%1,%2,%3,%4")
            .arg (mNormalGeo.x()).arg (mNormalGeo.y())
            .arg (mNormalGeo.width()).arg (mNormalGeo.height());
        if (isWindowMaximized() || (mIsFullscreen && mWasMax) || (mIsSeamless && mWasMax))
            winPos += QString (",%1").arg (VBoxDefs::GUI_LastWindowPosition_Max);

        machine.SetExtraData (VBoxDefs::GUI_LastWindowPosition, winPos);

        machine.SetExtraData (VBoxDefs::GUI_Fullscreen,
                              mVmFullscreenAction->isChecked() ? "on" : "off");
        machine.SetExtraData (VBoxDefs::GUI_Seamless,
                              mVmSeamlessAction->isChecked() ? "on" : "off");
        machine.SetExtraData (VBoxDefs::GUI_AutoresizeGuest,
                              mVmAutoresizeGuestAction->isChecked() ? "on" : "off");
        machine.SetExtraData (VBoxDefs::GUI_MiniToolBarAutoHide,
                              mMiniToolBar->isAutoHide() ? "on" : "off");

#ifdef VBOX_WITH_DEBUGGER_GUI
        /* Close & destroy the debugger GUI */
        dbgDestroy();
#endif

        /* Make sure all events are delievered */
        qApp->processEvents();

        /* Notify all the top-level dialogs about closing */
        emit closing();
    }

    LogFlowFunc (("accepted=%d\n", aEvent->isAccepted()));
    LogFlowFuncLeave();
}

#ifdef Q_WS_X11
bool VBoxConsoleWnd::x11Event (XEvent *aEvent)
{
    /* Qt bug: when the console view grabs the keyboard, FocusIn, FocusOut,
     * WindowActivate and WindowDeactivate Qt events are not properly sent
     * on top level window (i.e. this) deactivation. The fix is to substiute
     * the mode in FocusOut X11 event structure to NotifyNormal to cause
     * Qt to process it as desired. */
    if (mConsole && aEvent->type == FocusOut)
    {
        if (aEvent->xfocus.mode == NotifyWhileGrabbed  &&
            (aEvent->xfocus.detail == NotifyAncestor ||
             aEvent->xfocus.detail == NotifyInferior ||
             aEvent->xfocus.detail == NotifyNonlinear))
        {
             aEvent->xfocus.mode = NotifyNormal;
        }
    }
    return false;
}
#endif

/**
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void VBoxConsoleWnd::retranslateUi()
{
#ifdef VBOX_OSE
    mCaptionPrefix = tr ("VirtualBox OSE");
#else
    mCaptionPrefix = tr ("Sun VirtualBox");
#endif

#ifdef VBOX_BLEEDING_EDGE
    mCaptionPrefix += tr (" EXPERIMENTAL build %1r%2 - %3").arg (RTBldCfgVersion()).arg (RTBldCfgRevisionStr()).arg (VBOX_BLEEDING_EDGE);
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

    mVmDisMouseIntegrMenu->setToolTip (tr ("Mouse Integration", "enable/disable..."));
#if 0 /* TODO: Allow to setup status-bar! */
    mVmAutoresizeMenu->setToolTip (tr ("Auto-resize Guest Display", "enable/disable..."));
#endif

    mVmFullscreenAction->setText (VBoxGlobal::insertKeyToActionText (tr ("&Fullscreen Mode"), "F"));
    mVmFullscreenAction->setStatusTip (tr ("Switch to fullscreen mode" ));

    mVmSeamlessAction->setText (VBoxGlobal::insertKeyToActionText (tr ("Seam&less Mode"), "L"));
    mVmSeamlessAction->setStatusTip (tr ("Switch to seamless desktop integration mode"));

    mVmAutoresizeGuestAction->setText (VBoxGlobal::insertKeyToActionText (tr ("Auto-resize &Guest Display"), "G"));
    mVmAutoresizeGuestAction->setStatusTip (tr ("Automatically resize the guest display when the "
                                                "window is resized (requires Guest Additions)"));

    mVmAdjustWindowAction->setText (VBoxGlobal::insertKeyToActionText (tr ("&Adjust Window Size"), "A"));
    mVmAdjustWindowAction->setStatusTip (tr ("Adjust window size and position to best fit the guest display"));

    /* mVmDisableMouseIntegrAction is set up in updateAppearanceOf() */

    mVmTypeCADAction->setText (VBoxGlobal::insertKeyToActionText (tr ("&Insert Ctrl-Alt-Del"), "Del"));
    mVmTypeCADAction->setStatusTip (tr ("Send the Ctrl-Alt-Del sequence to the virtual machine"));

#if defined(Q_WS_X11)
    mVmTypeCABSAction->setText (VBoxGlobal::insertKeyToActionText (tr ("&Insert Ctrl-Alt-Backspace"), "Backspace"));
    mVmTypeCABSAction->setStatusTip (tr ("Send the Ctrl-Alt-Backspace sequence to the virtual machine"));
#endif

    mVmTakeSnapshotAction->setText (VBoxGlobal::insertKeyToActionText (tr ("Take &Snapshot..."), "S"));
    mVmTakeSnapshotAction->setStatusTip (tr ("Take a snapshot of the virtual machine"));

    mVmShowInformationDlgAction->setText (VBoxGlobal::insertKeyToActionText (tr ("Session I&nformation Dialog"), "N"));
    mVmShowInformationDlgAction->setStatusTip (tr ("Show Session Information Dialog"));

    mVmResetAction->setText (VBoxGlobal::insertKeyToActionText (tr ("&Reset"), "R"));
    mVmResetAction->setStatusTip (tr ("Reset the virtual machine"));

    /* mVmPauseAction is set up in updateAppearanceOf() */

#ifdef Q_WS_MAC
    /* Host+H is Hide on the mac */
    mVmACPIShutdownAction->setText (VBoxGlobal::insertKeyToActionText (tr ("ACPI S&hutdown"), "U"));
#else /* Q_WS_MAC */
    mVmACPIShutdownAction->setText (VBoxGlobal::insertKeyToActionText (tr ("ACPI S&hutdown"), "H"));
#endif /* !Q_WS_MAC */
    mVmACPIShutdownAction->setStatusTip (tr ("Send the ACPI Power Button press event to the virtual machine"));

    mVmCloseAction->setText (VBoxGlobal::insertKeyToActionText (tr ("&Close..." ), "Q"));
    mVmCloseAction->setStatusTip (tr ("Close the virtual machine"));
    mVmCloseAction->setMenuRole (QAction::QuitRole);

    /* Devices actions */
    mDevicesCDMenu->setTitle (tr ("&CD/DVD Devices"));
    mDevicesFDMenu->setTitle (tr ("&Floppy Devices"));

    mDevicesNetworkDialogAction->setText (tr ("&Network Adapters..."));
    mDevicesNetworkDialogAction->setStatusTip (tr ("Change the settings of network adapters"));

    mDevicesSFDialogAction->setText (tr ("&Shared Folders..."));
    mDevicesSFDialogAction->setStatusTip (tr ("Create or modify shared folders"));

    mDevicesSwitchVrdpAction->setText (tr ("&Remote Display"));
    mDevicesSwitchVrdpAction->setStatusTip (tr ("Enable or disable remote desktop (RDP) connections to this machine"));
#if 0 /* TODO: Allow to setup status-bar! */
    mDevicesVRDPMenu->setToolTip (tr ("Remote Desktop (RDP) Server", "enable/disable..."));
#endif

    mDevicesInstallGuestToolsAction->setText (VBoxGlobal::insertKeyToActionText (tr ("&Install Guest Additions..."), "D"));
    mDevicesInstallGuestToolsAction->setStatusTip (tr ("Mount the Guest Additions installation image"));

    mDevicesUSBMenu->setTitle (tr ("&USB Devices"));

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

    /* Main menu & seamless popup menu */
    mVMMenu->setTitle (tr ("&Machine"));
    // mVMMenu->setIcon (VBoxGlobal::iconSet (":/machine_16px.png"));

    mVMMenuMini->setTitle (tr ("&Machine"));

    mDevicesMenu->setTitle (tr ("&Devices"));
    // mDevicesMenu->setIcon (VBoxGlobal::iconSet (":/settings_16px.png"));

#ifdef VBOX_WITH_DEBUGGER_GUI
    if (vboxGlobal().isDebuggerEnabled())
        mDbgMenu->setTitle (tr ("De&bug"));
#endif
    mHelpMenu->setTitle (tr ("&Help"));
    // mHelpMenu->setIcon (VBoxGlobal::iconSet (":/help_16px.png"));

    /* Status bar widgets */
    mMouseLed->setToolTip (
        tr ("Indicates whether the host mouse pointer is captured by the guest OS:<br>"
            "<nobr><img src=:/mouse_disabled_16px.png/>&nbsp;&nbsp;pointer is not captured</nobr><br>"
            "<nobr><img src=:/mouse_16px.png/>&nbsp;&nbsp;pointer is captured</nobr><br>"
            "<nobr><img src=:/mouse_seamless_16px.png/>&nbsp;&nbsp;mouse integration (MI) is On</nobr><br>"
            "<nobr><img src=:/mouse_can_seamless_16px.png/>&nbsp;&nbsp;MI is Off, pointer is captured</nobr><br>"
            "<nobr><img src=:/mouse_can_seamless_uncaptured_16px.png/>&nbsp;&nbsp;MI is Off, pointer is not captured</nobr><br>"
            "Note that the mouse integration feature requires Guest Additions to be installed in the guest OS."));
    mHostkeyLed->setToolTip (
        tr ("Indicates whether the keyboard is captured by the guest OS "
            "(<img src=:/hostkey_captured_16px.png/>) or not (<img src=:/hostkey_16px.png/>)."));
    mHostkeyName->setToolTip (
        tr ("Shows the currently assigned Host key.<br>"
            "This key, when pressed alone, toggles the keyboard and mouse "
            "capture state. It can also be used in combination with other keys "
            "to quickly perform actions from the main menu."));
    mHostkeyName->setText (QIHotKeyEdit::keyName (vboxGlobal().settings().hostKey()));

#if 0 /* TODO: Allow to setup status-bar! */
    mAutoresizeLed->setToolTip (
        tr ("Indicates whether the guest display auto-resize function is On "
            "(<img src=:/auto_resize_on_16px.png/>) or Off (<img src=:/auto_resize_off_16px.png/>). "
            "Note that this function requires Guest Additions to be installed in the guest OS."));
#endif

    updateAppearanceOf (AllStuff);
}

void VBoxConsoleWnd::finalizeOpenView()
{
    LogFlowFuncEnter();

    /* Notify the console scroll-view about the console-window is opened. */
    mConsole->onViewOpened();

    bool saved = mMachineState == KMachineState_Saved;

    CMachine machine = mSession.GetMachine();
    CConsole console = mConsole->console();

    if (mIsFirstTimeStarted)
    {
        VBoxVMFirstRunWzd wzd (machine, this);
        wzd.exec();

        /* Remove GUI_FirstRun extra data key from the machine settings
         * file after showing the wizard once. */
        machine.SetExtraData (VBoxDefs::GUI_FirstRun, QString::null);
    }

    /* Start the VM */
    CProgress progress = vboxGlobal().isStartPausedEnabled() || vboxGlobal().isDebuggerAutoShowEnabled() ?
                         console.PowerUpPaused() : console.PowerUp();

    /* Check for an immediate failure */
    if (!console.isOk())
    {
        vboxProblem().cannotStartMachine (console);
        /* close this window (this will call closeView()) */
        close();

        LogFlowFunc (("Error starting VM\n"));
        LogFlowFuncLeave();
        return;
    }

    mConsole->attach();

    /* Disable auto closure because we want to have a chance to show the
     * error dialog on startup failure */
    mNoAutoClose = true;

    /* show the "VM starting / restoring" progress dialog */

    if (saved)
        vboxProblem().showModalProgressDialog (progress, machine.GetName(), this, 0);
    else
        vboxProblem().showModalProgressDialog (progress, machine.GetName(), this);

    if (progress.GetResultCode() != 0)
    {
        vboxProblem().cannotStartMachine (progress);
        /* close this window (this will call closeView()) */
        close();

        LogFlowFunc (("Error starting VM\n"));
        LogFlowFuncLeave();
        return;
    }

    mNoAutoClose = false;

    /* Check if we missed a really quick termination after successful
     * startup, and process it if we did. */
    if (mMachineState < KMachineState_Running)
    {
        close();
        LogFlowFuncLeave();
        return;
    }

    /* Currently the machine is started and the guest API could be used...
     * Checking if the fullscreen mode should be activated */
    QString str = machine.GetExtraData (VBoxDefs::GUI_Fullscreen);
    if (str == "on")
        mVmFullscreenAction->setChecked (true);

    /* If seamless mode should be enabled then check if it is enabled
     * currently and re-enable it if seamless is supported */
    if (mVmSeamlessAction->isChecked() && mIsSeamlessSupported && mIsGraphicsSupported)
        toggleFullscreenMode (true, true);
#ifdef VBOX_WITH_DEBUGGER_GUI
    /* Open the debugger in "full screen" mode requested by the user. */
    else if (vboxGlobal().isDebuggerAutoShowEnabled())
    {
        /* console in upper left corner of the desktop. */
        QRect rct (0, 0, 0, 0);
        QDesktopWidget *desktop = QApplication::desktop();
        if (desktop)
            rct = desktop->availableGeometry(pos());
        move (QPoint (rct.x(), rct.y()));

        if (vboxGlobal().isDebuggerAutoShowStatisticsEnabled())
            dbgShowStatistics();
        if (vboxGlobal().isDebuggerAutoShowCommandLineEnabled())
            dbgShowCommandLine();

        if (!vboxGlobal().isStartPausedEnabled())
            mConsole->pause (false);
    }
#endif

    mIsOpenViewFinished = true;
    LogFlowFuncLeave();

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

void VBoxConsoleWnd::vmFullscreen (bool aOn)
{
    bool ok = toggleFullscreenMode (aOn, false /* aSeamless */);
    if (!ok)
    {
        /* On failure, restore the previous button state */
        mVmFullscreenAction->blockSignals (true);
        mVmFullscreenAction->setChecked (!aOn);
        mVmFullscreenAction->blockSignals (false);
    }
}

void VBoxConsoleWnd::vmSeamless (bool aOn)
{
    /* Check if it is possible to enter/leave seamless mode */
    if ((mIsSeamlessSupported && mIsGraphicsSupported) || !aOn)
    {
        bool ok = toggleFullscreenMode (aOn, true /* aSeamless */);
        if (!ok)
        {
            /* On failure, restore the previous button state */
            mVmSeamlessAction->blockSignals (true);
            mVmSeamlessAction->setChecked (!aOn);
            mVmSeamlessAction->blockSignals (false);
        }
    }
}

void VBoxConsoleWnd::vmAutoresizeGuest (bool on)
{
    if (!mConsole)
        return;

#if 0 /* TODO: Allow to setup status-bar! */
    mAutoresizeLed->setState (on ? 3 : 1);
#endif

    mConsole->setAutoresizeGuest (on);
}

void VBoxConsoleWnd::vmAdjustWindow()
{
    if (mConsole)
    {
        if (isWindowMaximized())
            showNormal();
        mConsole->normalizeGeometry (true /* adjustPosition */);
    }
}

void VBoxConsoleWnd::vmDisableMouseIntegration (bool aOff)
{
    if (mConsole)
    {
        mConsole->setMouseIntegrationEnabled (!aOff);
        updateAppearanceOf (DisableMouseIntegrAction);
    }
}

void VBoxConsoleWnd::vmTypeCAD()
{
    if (mConsole)
    {
        CKeyboard keyboard  = mConsole->console().GetKeyboard();
        Assert (!keyboard.isNull());
        keyboard.PutCAD();
        AssertWrapperOk (keyboard);
    }
}

#ifdef Q_WS_X11
void VBoxConsoleWnd::vmTypeCABS()
{
    if (mConsole)
    {
        CKeyboard keyboard  = mConsole->console().GetKeyboard();
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
}
#endif

void VBoxConsoleWnd::vmTakeSnapshot()
{
    AssertReturn (mConsole, (void) 0);

    /* remember the paused state */
    bool wasPaused = mMachineState == KMachineState_Paused;
    if (!wasPaused)
    {
        /* Suspend the VM and ignore the close event if failed to do so.
         * pause() will show the error message to the user. */
        if (!mConsole->pause (true))
            return;
    }

    CMachine machine = mSession.GetMachine();

    VBoxTakeSnapshotDlg dlg (this);

    QString typeId = machine.GetOSTypeId();
    dlg.mLbIcon->setPixmap (vboxGlobal().vmGuestOSTypeIcon (typeId));

    /* search for the max available filter index */
    int maxSnapShotIndex = 0;
    QString snapShotName = tr ("Snapshot %1");
    QRegExp regExp (QString ("^") + snapShotName.arg ("([0-9]+)") + QString ("$"));
    CSnapshot index = machine.GetSnapshot (QString::null);
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
            index = c [0];
        else
            break;
    }
    dlg.mLeName->setText (snapShotName.arg (maxSnapShotIndex + 1));

    if (dlg.exec() == QDialog::Accepted)
    {
        CConsole console = mSession.GetConsole();

        CProgress progress = console.TakeSnapshot (dlg.mLeName->text().trimmed(), dlg.mTeDescription->toPlainText());

        if (console.isOk())
        {
            /* Show the "Taking Snapshot" progress dialog */
            vboxProblem().showModalProgressDialog (progress, machine.GetName(), this, 0);

            if (progress.GetResultCode() != 0)
                vboxProblem().cannotTakeSnapshot (progress);
        }
        else
            vboxProblem().cannotTakeSnapshot (console);
    }

    /* Restore the running state if needed */
    if (!wasPaused)
        mConsole->pause (false);
}

void VBoxConsoleWnd::vmShowInfoDialog()
{
    VBoxVMInformationDlg::createInformationDlg (mSession, mConsole);
}

void VBoxConsoleWnd::vmReset()
{
    if (mConsole)
    {
        if (vboxProblem().confirmVMReset (this))
            mConsole->console().Reset();
    }
}

void VBoxConsoleWnd::vmPause (bool aOn)
{
    if (mConsole)
    {
        mConsole->pause (aOn);
        updateAppearanceOf (PauseAction);
    }
}

void VBoxConsoleWnd::vmACPIShutdown()
{
    if (!mSession.GetConsole().GetGuestEnteredACPIMode())
        return vboxProblem().cannotSendACPIToMachine();

    if (mConsole)
    {
        CConsole console = mConsole->console();
        console.PowerButton();
        if (!console.isOk())
            vboxProblem().cannotACPIShutdownMachine (console);
    }
}

void VBoxConsoleWnd::vmClose()
{
    if (mConsole)
        close();
}

void VBoxConsoleWnd::devicesSwitchVrdp (bool aOn)
{
    if (!mConsole) return;

    CVRDPServer vrdpServer = mSession.GetMachine().GetVRDPServer();
    /* This method should not be executed if vrdpServer is null */
    Assert (!vrdpServer.isNull());

    vrdpServer.SetEnabled (aOn);
    updateAppearanceOf (VRDPStuff);
}

void VBoxConsoleWnd::devicesOpenNetworkDialog()
{
    if (!mConsole) return;

    VBoxNetworkDialog dlg (mConsole, mSession);
    dlg.exec();
}

void VBoxConsoleWnd::devicesOpenSFDialog()
{
    if (!mConsole) return;

    VBoxSFDialog dlg (mConsole, mSession);
    dlg.exec();
}

void VBoxConsoleWnd::devicesInstallGuestAdditions()
{
    char szAppPrivPath [RTPATH_MAX];
    int rc = RTPathAppPrivateNoArch (szAppPrivPath, sizeof (szAppPrivPath));
    AssertRC (rc);

    QString src1 = QString (szAppPrivPath) + "/VBoxGuestAdditions.iso";
    QString src2 = qApp->applicationDirPath() + "/additions/VBoxGuestAdditions.iso";

    /* Check the standard image locations */
    if (QFile::exists (src1))
        return installGuestAdditionsFrom (src1);
    else if (QFile::exists (src2))
        return installGuestAdditionsFrom (src2);

    /* Check for the already registered image */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    QString name = QString ("VBoxGuestAdditions_%1.iso").arg (vbox.GetVersion().remove ("_OSE"));

    CMediumVector vec = vbox.GetDVDImages();
    for (CMediumVector::ConstIterator it = vec.begin(); it != vec.end(); ++ it)
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

void VBoxConsoleWnd::prepareStorageMenu()
{
    QMenu *menu = qobject_cast <QMenu*> (sender());
    Assert (menu);
    menu->clear();

    KDeviceType deviceType = menu == mDevicesCDMenu ? KDeviceType_DVD :
                             menu == mDevicesFDMenu ? KDeviceType_Floppy :
                                                      KDeviceType_Null;
    Assert (deviceType != KDeviceType_Null);

    VBoxDefs::MediumType mediumType = menu == mDevicesCDMenu ? VBoxDefs::MediumType_DVD :
                                      menu == mDevicesFDMenu ? VBoxDefs::MediumType_Floppy :
                                                               VBoxDefs::MediumType_Invalid;
    Assert (mediumType != VBoxDefs::MediumType_Invalid);

    const CMediumAttachmentVector &attachments = mSession.GetMachine().GetMediumAttachments();
    foreach (const CMediumAttachment &attachment, attachments)
    {
        if (attachment.GetType() == deviceType)
        {
            /* Attachment menu item */
            QMenu *attachmentMenu = 0;
            if (menu->menuAction()->data().toInt() > 1)
            {
                attachmentMenu = new QMenu (menu);
                attachmentMenu->setTitle (QString ("%1 (%2)").arg (attachment.GetController().GetName())
                                          .arg (vboxGlobal().toString (StorageSlot (attachment.GetController().GetBus(),
                                                                                    attachment.GetPort(),
                                                                                    attachment.GetDevice()))));
                switch (attachment.GetController().GetBus())
                {
                    case KStorageBus_IDE:
                        attachmentMenu->setIcon (QIcon (":/ide_16px.png")); break;
                    case KStorageBus_SATA:
                        attachmentMenu->setIcon (QIcon (":/sata_16px.png")); break;
                    case KStorageBus_SCSI:
                        attachmentMenu->setIcon (QIcon (":/scsi_16px.png")); break;
                    case KStorageBus_Floppy:
                        attachmentMenu->setIcon (QIcon (":/floppy_16px.png")); break;
                    default:
                        break;
                }
                menu->addMenu (attachmentMenu);
            }
            else attachmentMenu = menu;

            /* Related VBoxMedium item */
            VBoxMedium vboxMediumCurrent;
            vboxGlobal().findMedium (attachment.GetMedium(), vboxMediumCurrent);

            /* Mount Medium actions */
            int addedIntoList = 0;
            const VBoxMediaList &vboxMediums = vboxGlobal().currentMediaList();
            foreach (const VBoxMedium &vboxMedium, vboxMediums)
            {
                if (vboxMedium.type() == mediumType)
                {
                    bool isMediumUsed = false;
                    foreach (const CMediumAttachment &otherAttachment, attachments)
                    {
                        if (otherAttachment != attachment)
                        {
                            CMedium otherMedium = otherAttachment.GetMedium();
                            if (!otherMedium.isNull() && otherMedium.GetId() == vboxMedium.id())
                            {
                                isMediumUsed = true;
                                break;
                            }
                        }
                    }
                    if (!isMediumUsed)
                    {
                        QAction *mountMediumAction = new QAction (vboxMedium.name(), attachmentMenu);
                        mountMediumAction->setCheckable (true);
                        mountMediumAction->setChecked (vboxMedium.id() == vboxMediumCurrent.id());
                        mountMediumAction->setData (QVariant::fromValue (MountTarget (attachment.GetController().GetName(),
                                                                                      attachment.GetPort(),
                                                                                      attachment.GetDevice(),
                                                                                      vboxMedium.id())));
                        connect (mountMediumAction, SIGNAL (triggered (bool)), this, SLOT (mountMedium()));
                        attachmentMenu->addAction (mountMediumAction);
                        ++ addedIntoList;
                        if (addedIntoList == 5)
                            break;
                    }
                }
            }

            /* Virtual Media Manager action */
            QAction *callVMMAction = new QAction (attachmentMenu);
            callVMMAction->setIcon (QIcon (":/diskimage_16px.png"));
            callVMMAction->setData (QVariant::fromValue (MountTarget (attachment.GetController().GetName(),
                                                                      attachment.GetPort(),
                                                                      attachment.GetDevice(),
                                                                      QString (""),
                                                                      mediumType)));
            connect (callVMMAction, SIGNAL (triggered (bool)), this, SLOT (mountMedium()));
            attachmentMenu->addAction (callVMMAction);

            /* Separator */
            attachmentMenu->addSeparator();

            /* Unmount Medium action */
            QAction *unmountMediumAction = new QAction (attachmentMenu);
            unmountMediumAction->setEnabled (!vboxMediumCurrent.isNull());
            unmountMediumAction->setData (QVariant::fromValue (MountTarget (attachment.GetController().GetName(),
                                                                            attachment.GetPort(),
                                                                            attachment.GetDevice())));
            connect (unmountMediumAction, SIGNAL (triggered (bool)), this, SLOT (mountMedium()));
            attachmentMenu->addAction (unmountMediumAction);

            /* Switch CD/FD naming */
            switch (deviceType)
            {
                case KDeviceType_DVD:
                    callVMMAction->setText (tr ("More CD/DVD Images..."));
                    unmountMediumAction->setText (tr ("Unmount CD/DVD Device"));
                    unmountMediumAction->setIcon (VBoxGlobal::iconSet (":/cd_unmount_16px.png",
                                                                       ":/cd_unmount_dis_16px.png"));
                    break;
                case KDeviceType_Floppy:
                    callVMMAction->setText (tr ("More Floppy Images..."));
                    unmountMediumAction->setText (tr ("Unmount Floppy Device"));
                    unmountMediumAction->setIcon (VBoxGlobal::iconSet (":/fd_unmount_16px.png",
                                                                       ":/fd_unmount_dis_16px.png"));
                    break;
                default:
                    break;
            }
        }
    }

    if (menu->menuAction()->data().toInt() == 0)
    {
        /* Empty menu item */
        Assert (menu->isEmpty());
        QAction *emptyMenuAction = new QAction (menu);
        emptyMenuAction->setEnabled (false);
        switch (deviceType)
        {
            case KDeviceType_DVD:
                emptyMenuAction->setText (tr ("No CD/DVD Devices Attached"));
                break;
            case KDeviceType_Floppy:
                emptyMenuAction->setText (tr ("No Floppy Devices Attached"));
                break;
            default:
                break;
        }
        emptyMenuAction->setIcon (VBoxGlobal::iconSet (":/delete_16px.png", ":/delete_dis_16px.png"));
        menu->addAction (emptyMenuAction);
    }
}

void VBoxConsoleWnd::prepareNetworkMenu()
{
    mDevicesNetworkMenu->clear();
    mDevicesNetworkMenu->addAction (mDevicesNetworkDialogAction);
}

void VBoxConsoleWnd::prepareSFMenu()
{
    mDevicesSFMenu->clear();
    mDevicesSFMenu->addAction (mDevicesSFDialogAction);
}

void VBoxConsoleWnd::mountMedium()
{
    QAction *action = qobject_cast <QAction*> (sender());
    Assert (action);

    MountTarget target = action->data().value <MountTarget>();
    CMachine machine = mSession.GetMachine();
    CMediumAttachment attachment = machine.GetMediumAttachment (target.name, target.port, target.device);
    CMedium medium = attachment.GetMedium();

    if (target.type != VBoxDefs::MediumType_Invalid)
    {
        /* Search for already used images */
        QStringList usedImages;
        const CMediumAttachmentVector &attachments = mSession.GetMachine().GetMediumAttachments();
        foreach (const CMediumAttachment &index, attachments)
        {
            if (index != attachment && !index.GetMedium().isNull() && !index.GetMedium().GetHostDrive())
                usedImages << index.GetMedium().GetId();
        }
        /* Open VMM Dialog */
        VBoxMediaManagerDlg dlg (this);
        dlg.setup (target.type, true /* do select? */, false /* do refresh? */,
                   mSession.GetMachine(), QString(), true, usedImages);
        if (dlg.exec() == QDialog::Accepted)
            target.id = dlg.selectedId();
        else return;
    }

    machine.MountMedium (target.name, target.port, target.device,
                         target.id.isEmpty() || medium.isNull() || medium.GetId() != target.id ||
                         target.type != VBoxDefs::MediumType_Invalid ? target.id : QString (""));
}

/**
 *  Attach/Detach selected USB Device.
 */
void VBoxConsoleWnd::switchUSB (QAction *aAction)
{
    if (!mConsole) return;

    CConsole console = mSession.GetConsole();
    AssertWrapperOk (mSession);

    CUSBDevice usb = mDevicesUSBMenu->getUSB (aAction);
    /* if null then some other item but a USB device is selected */
    if (usb.isNull())
        return;

    if (!aAction->isChecked())
    {
        console.DetachUSBDevice (usb.GetId());
        if (!console.isOk())
        {
            /// @todo (r=dmik) the dialog should be either modeless
            //  or we have to pause the VM
            vboxProblem().cannotDetachUSBDevice (console, vboxGlobal().details (usb));
        }
    }
    else
    {
        console.AttachUSBDevice (usb.GetId());
        if (!console.isOk())
        {
            /// @todo (r=dmik) the dialog should be either modeless
            //  or we have to pause the VM
            vboxProblem().cannotAttachUSBDevice (console, vboxGlobal().details (usb));
        }
    }
}

void VBoxConsoleWnd::showIndicatorContextMenu (QIStateIndicator *aInd, QContextMenuEvent *aEvent)
{
    if (aInd == mCDLed)
    {
        mDevicesCDMenu->exec (aEvent->globalPos());
    }
#if 0 /* TODO: Allow to setup status-bar! */
    else if (aInd == mFDLed)
    {
        mDevicesFDMenu->exec (aEvent->globalPos());
    }
#endif
    else if (aInd == mNetLed)
    {
        if (mDevicesNetworkMenu->isEnabled())
            mDevicesNetworkMenu->exec (aEvent->globalPos());
    }
    else if (aInd == mUSBLed)
    {
        if (mDevicesUSBMenu->isEnabled())
            mDevicesUSBMenu->exec (aEvent->globalPos());
    }
    else if (aInd == mSFLed)
    {
        if (mDevicesSFMenu->isEnabled())
            mDevicesSFMenu->exec (aEvent->globalPos());
    }
    else if (aInd == mMouseLed)
    {
        mVmDisMouseIntegrMenu->exec (aEvent->globalPos());
    }
#if 0 /* TODO: Allow to setup status-bar! */
    else if (aInd == mVrdpLed)
    {
        mDevicesVRDPMenu->exec (aEvent->globalPos());
    }
    else if (aInd == mAutoresizeLed)
    {
        mVmAutoresizeMenu->exec (aEvent->globalPos());
    }
#endif
}

void VBoxConsoleWnd::updateDeviceLights()
{
    if (mConsole)
    {
        CConsole &console = mConsole->console();
        int st;
        if (mHDLed->state() != KDeviceActivity_Null)
        {
            st = console.GetDeviceActivity (KDeviceType_HardDisk);
            if (mHDLed->state() != st)
                mHDLed->setState (st);
        }
        if (mCDLed->state() != KDeviceActivity_Null)
        {
            st = console.GetDeviceActivity (KDeviceType_DVD);
            if (mCDLed->state() != st)
                mCDLed->setState (st);
        }
#if 0 /* TODO: Allow to setup status-bar! */
        if (mFDLed->state() != KDeviceActivity_Null)
        {
            st = console.GetDeviceActivity (KDeviceType_Floppy);
            if (mFDLed->state() != st)
                mFDLed->setState (st);
        }
#endif
        if (mNetLed->state() != KDeviceActivity_Null)
        {
            st = console.GetDeviceActivity (KDeviceType_Network);
            if (mNetLed->state() != st)
                mNetLed->setState (st);
        }
        if (mUSBLed->state() != KDeviceActivity_Null)
        {
            st = console.GetDeviceActivity (KDeviceType_USB);
            if (mUSBLed->state() != st)
                mUSBLed->setState (st);
        }
        if (mSFLed->state() != KDeviceActivity_Null)
        {
            st = console.GetDeviceActivity (KDeviceType_SharedFolder);
            if (mSFLed->state() != st)
                mSFLed->setState (st);
        }
    }
}

void VBoxConsoleWnd::updateMachineState (KMachineState aState)
{
    bool guruMeditation = false;

    if (mConsole && mMachineState != aState)
    {
        if (aState >= KMachineState_Running)
        {
            switch (aState)
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
                    if (mMachineState == KMachineState_Paused && mVmPauseAction->isChecked())
                        mVmPauseAction->setChecked (false);
                    break;
                }
#ifdef Q_WS_X11
                case KMachineState_Starting:
                {
                    /* The keyboard handler may wish to do some release logging
                       on startup.  Tell it that the logger is now active. */
                    doXKeyboardLogging (QX11Info::display());
                    break;
                }
#endif
                default:
                    break;
            }
        }

        bool isRunningOrPaused = aState == KMachineState_Running || aState == KMachineState_Paused;

        /* Enable/Disable actions that are not managed by updateAppearanceOf() */

        mRunningActions->setEnabled (aState == KMachineState_Running);
        mRunningOrPausedActions->setEnabled (isRunningOrPaused);

        mMachineState = aState;

        updateAppearanceOf (Caption |
                            HardDiskStuff | DVDStuff | FloppyStuff |
                            NetworkStuff | USBStuff | VRDPStuff |
                            PauseAction | DisableMouseIntegrAction);

        if (aState < KMachineState_Running)
        {
            /* VM has been powered off or saved or aborted, no matter
             * internally or externally -- we must *safely* close the console
             * window unless auto closure is disabled. */
            if (!mNoAutoClose)
                tryClose();
        }
    }

    if (guruMeditation)
    {
        CConsole console = mConsole->console();
        QString logFolder = console.GetMachine().GetLogFolder();

        /* Take the screenshot for debugging purposes and save it */
        QString fname = logFolder + "/VBox.png";

        CDisplay dsp = console.GetDisplay();
        QImage shot = QImage (dsp.GetWidth(), dsp.GetHeight(), QImage::Format_RGB32);
        dsp.TakeScreenShot (shot.bits(), shot.width(), shot.height());
        shot.save (QFile::encodeName (fname), "PNG");

        if (vboxProblem().remindAboutGuruMeditation (console, QDir::toNativeSeparators (logFolder)))
        {
            qApp->processEvents();
            console.PowerDown();
            if (!console.isOk())
                vboxProblem().cannotStopMachine (console);
        }
    }

#ifdef Q_WS_MAC
    if (mConsole)
        mConsole->updateDockOverlay();
#endif
}

void VBoxConsoleWnd::updateMouseState (int aState)
{
    mVmDisableMouseIntegrAction->setEnabled (aState & VBoxConsoleView::MouseAbsolute);

    if ((aState & VBoxConsoleView::MouseAbsoluteDisabled) &&
        (aState & VBoxConsoleView::MouseAbsolute) &&
        !(aState & VBoxConsoleView::MouseCaptured))
    {
        mMouseLed->setState (4);
    }
    else
    {
        mMouseLed->setState (aState & (VBoxConsoleView::MouseAbsolute | VBoxConsoleView::MouseCaptured));
    }
}

void VBoxConsoleWnd::updateAdditionsState (const QString &aVersion,
                                           bool aActive,
                                           bool aSeamlessSupported,
                                           bool aGraphicsSupported)
{
    mVmAutoresizeGuestAction->setEnabled (aActive && aGraphicsSupported);
    if ((mIsSeamlessSupported != aSeamlessSupported) ||
        (mIsGraphicsSupported != aGraphicsSupported))
    {
        mVmSeamlessAction->setEnabled (aSeamlessSupported && aGraphicsSupported);
        mIsSeamlessSupported = aSeamlessSupported;
        mIsGraphicsSupported = aGraphicsSupported;
        /* If seamless mode should be enabled then check if it is enabled
         * currently and re-enable it if open-view procedure is finished */
        if (mVmSeamlessAction->isChecked() && mIsOpenViewFinished && aSeamlessSupported && aGraphicsSupported)
            toggleFullscreenMode (true, true);
        /* Disable auto-resizing if advanced graphics are not available */
        mConsole->setAutoresizeGuest (mIsGraphicsSupported && mVmAutoresizeGuestAction->isChecked());
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
        .arg (VMMDEV_VERSION_MAJOR).arg (VMMDEV_VERSION_MINOR); /** @todo r=bird: This isn't want we want! We want the VirtualBox version of the additions, all three numbers. See @bugref{4084}.*/

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

void VBoxConsoleWnd::updateNetworkAdaptersState()
{
    updateAppearanceOf (NetworkStuff);
}

void VBoxConsoleWnd::updateUsbState()
{
    updateAppearanceOf (USBStuff);
}

void VBoxConsoleWnd::updateMediaDriveState (VBoxDefs::MediumType aType)
{
    Assert (aType == VBoxDefs::MediumType_DVD || aType == VBoxDefs::MediumType_Floppy);
    updateAppearanceOf (aType == VBoxDefs::MediumType_DVD ? DVDStuff :
                        aType == VBoxDefs::MediumType_Floppy ? FloppyStuff :
                        AllStuff);
}

void VBoxConsoleWnd::updateSharedFoldersState()
{
    updateAppearanceOf (SharedFolderStuff);
}

/**
 *  This slot is called just after leaving the fullscreen/seamless mode,
 *  when the console was resized to previous size.
 */
void VBoxConsoleWnd::onExitFullscreen()
{
    mConsole->setIgnoreMainwndResize (false);
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
    mConsole->setMouseCoalescingEnabled (true);
#endif
}

void VBoxConsoleWnd::mtExitMode()
{
    if (mIsSeamless)
        mVmSeamlessAction->toggle();
    else
        mVmFullscreenAction->toggle();
}

void VBoxConsoleWnd::mtCloseVM()
{
    mVmCloseAction->trigger();
}

void VBoxConsoleWnd::mtMaskUpdate()
{
    if (mIsSeamless)
        setMask (mConsole->lastVisibleRegion());
}

void VBoxConsoleWnd::changeDockIconUpdate (const VBoxChangeDockIconUpdateEvent &aEvent)
{
#ifdef Q_WS_MAC
    if (mConsole)
    {
        mConsole->setDockIconEnabled (aEvent.mChanged);
        mConsole->updateDockOverlay();
    }
#else
    Q_UNUSED (aEvent);
#endif
}

/**
 *  Called (on non-UI thread!) when a global GUI setting changes.
 */
void VBoxConsoleWnd::processGlobalSettingChange (const char * /* aPublicName */, const char * /* aName */)
{
    mHostkeyName->setText (QIHotKeyEdit::keyName (vboxGlobal().settings().hostKey()));
}

/**
 *  This function checks the status of required features and
 *  makes a warning and/or some action if something necessary
 *  is not in good condition.
 *  Does nothing if no console view was opened.
 */
void VBoxConsoleWnd::checkRequiredFeatures()
{
    if (!mConsole) return;

    CConsole console = mConsole->console();

    /* Check if the virtualization feature is required. */
    bool is64BitsGuest    = vboxGlobal().virtualBox().GetGuestOSType (
                            console.GetGuest().GetOSTypeId()).GetIs64Bit();
    bool fRecommendVirtEx = vboxGlobal().virtualBox().GetGuestOSType (
                            console.GetGuest().GetOSTypeId()).GetRecommendedVirtEx();
    Assert(!is64BitsGuest || fRecommendVirtEx);
    bool isVirtEnabled    = console.GetDebugger().GetHWVirtExEnabled();
    if (fRecommendVirtEx && !isVirtEnabled)
    {
        bool ret;

        vmPause (true);

        if (is64BitsGuest)
            ret = vboxProblem().warnAboutVirtNotEnabled64BitsGuest();
        else
            ret = vboxProblem().warnAboutVirtNotEnabledGuestRequired();

        if (ret == true)
            close();
        else
            vmPause (false);
    }
}

void VBoxConsoleWnd::activateUICustomizations()
{
    VBoxGlobalSettings settings = vboxGlobal().settings();
    /* Process known keys */
    menuBar()->setHidden (settings.isFeatureActive ("noMenuBar"));
    statusBar()->setHidden (settings.isFeatureActive ("noStatusBar"));
}

void VBoxConsoleWnd::updateAppearanceOf (int aElement)
{
    if (!mConsole) return;

    CMachine machine = mSession.GetMachine();
    CConsole console = mConsole->console();

    bool isRunningOrPaused = mMachineState == KMachineState_Running || mMachineState == KMachineState_Paused;

    if (aElement & Caption)
    {
        QString snapshotName;
        if (machine.GetSnapshotCount() > 0)
        {
            CSnapshot snapshot = machine.GetCurrentSnapshot();
            snapshotName = " (" + snapshot.GetName() + ")";
        }
        setWindowTitle (machine.GetName() + snapshotName +
                        " [" + vboxGlobal().toString (mMachineState) + "] - " +
                        mCaptionPrefix);
        mMiniToolBar->setDisplayText (machine.GetName() + snapshotName);
    }
    if (aElement & HardDiskStuff)
    {
        QString tip = tr ("<p style='white-space:pre'><nobr>Indicates the activity "
                          "of virtual hard disks:</nobr>%1</p>", "HDD tooltip");
        QString data;
        bool attachmentsPresent = false;

        CStorageControllerVector controllers = machine.GetStorageControllers();
        foreach (const CStorageController &controller, controllers)
        {
            QString attData;
            CMediumAttachmentVector attachments = machine.GetMediumAttachmentsOfController (controller.GetName());
            foreach (const CMediumAttachment &attachment, attachments)
            {
                if (attachment.GetType() != KDeviceType_HardDisk)
                    continue;
                attData += QString ("<br>&nbsp;<nobr>%1:&nbsp;%2</nobr>")
                    .arg (vboxGlobal().toString (StorageSlot (controller.GetBus(), attachment.GetPort(), attachment.GetDevice())))
                    .arg (vboxGlobal().findMedium (attachment.GetMedium().GetId()).location());
                attachmentsPresent = true;
            }
            if (!attData.isNull())
                data += QString ("<br><nobr><b>%1</b></nobr>").arg (controller.GetName()) + attData;
        }

        if (!attachmentsPresent)
            data += tr ("<br><nobr><b>No hard disks attached</b></nobr>", "HDD tooltip");

        mHDLed->setToolTip (tip.arg (data));
        mHDLed->setState (attachmentsPresent ? KDeviceActivity_Idle : KDeviceActivity_Null);
    }
    if (aElement & DVDStuff)
    {
        QString tip = tr ("<p style='white-space:pre'><nobr>Indicates the activity "
                          "of the CD/DVD devices:</nobr>%1</p>", "CD/DVD tooltip");
        QString data;
        bool attachmentsPresent = false;

        CStorageControllerVector controllers = machine.GetStorageControllers();
        foreach (const CStorageController &controller, controllers)
        {
            QString attData;
            CMediumAttachmentVector attachments = machine.GetMediumAttachmentsOfController (controller.GetName());
            foreach (const CMediumAttachment &attachment, attachments)
            {
                if (attachment.GetType() != KDeviceType_DVD)
                    continue;
                QString id (attachment.GetMedium().isNull() ? QString() : attachment.GetMedium().GetId());
                VBoxMedium vboxMedium = vboxGlobal().findMedium (id);
                attData += QString ("<br>&nbsp;<nobr>%1:&nbsp;%2</nobr>")
                    .arg (vboxGlobal().toString (StorageSlot (controller.GetBus(), attachment.GetPort(), attachment.GetDevice())))
                    .arg (vboxMedium.isNull() || vboxMedium.isHostDrive() ? vboxMedium.name() : vboxMedium.location());
                if (!vboxMedium.isNull())
                    attachmentsPresent = true;
            }
            if (!attData.isNull())
                data += QString ("<br><nobr><b>%1</b></nobr>").arg (controller.GetName()) + attData;
        }

        if (data.isNull())
            data = tr ("<br><nobr><b>No CD/DVD devices attached</b></nobr>", "CD/DVD tooltip");

        mCDLed->setToolTip (tip.arg (data));
        mCDLed->setState (attachmentsPresent ? KDeviceActivity_Idle : KDeviceActivity_Null);
    }
#if 0 /* TODO: Allow to setup status-bar! */
    if (aElement & FloppyStuff)
    {
        QString tip = tr ("<p style='white-space:pre'><nobr>Indicates the activity "
                          "of the floppy devices:</nobr>%1</p>", "FD tooltip");
        QString data;
        bool attachmentsPresent = false;

        CStorageControllerVector controllers = machine.GetStorageControllers();
        foreach (const CStorageController &controller, controllers)
        {
            QString attData;
            CMediumAttachmentVector attachments = machine.GetMediumAttachmentsOfController (controller.GetName());
            foreach (const CMediumAttachment &attachment, attachments)
            {
                if (attachment.GetType() != KDeviceType_Floppy)
                    continue;
                QString id (attachment.GetMedium().isNull() ? QString() : attachment.GetMedium().GetId());
                VBoxMedium vboxMedium = vboxGlobal().findMedium (id);
                attData += QString ("<br>&nbsp;<nobr>%1:&nbsp;%2</nobr>")
                    .arg (vboxGlobal().toString (StorageSlot (controller.GetBus(), attachment.GetPort(), attachment.GetDevice())))
                    .arg (vboxMedium.isNull() || vboxMedium.isHostDrive() ? vboxMedium.name() : vboxMedium.location());
                if (!vboxMedium.isNull())
                    attachmentsPresent = true;
            }
            if (!attData.isNull())
                data += QString ("<br><nobr><b>%1</b></nobr>").arg (controller.GetName()) + attData;
        }

        if (data.isNull())
            data = tr ("<br><nobr><b>No floppy devices attached</b></nobr>", "FD tooltip");

        mFDLed->setToolTip (tip.arg (data));
        mFDLed->setState (attachmentsPresent ? KDeviceActivity_Idle : KDeviceActivity_Null);
    }
#endif
    if (aElement & NetworkStuff)
    {
        ulong maxCount = vboxGlobal().virtualBox().GetSystemProperties().GetNetworkAdapterCount();
        ulong count = 0;
        for (ulong slot = 0; slot < maxCount; ++ slot)
            if (machine.GetNetworkAdapter (slot).GetEnabled())
                ++ count;
        mNetLed->setState (count > 0 ? KDeviceActivity_Idle : KDeviceActivity_Null);

        mDevicesNetworkDialogAction->setEnabled (isRunningOrPaused && count > 0);
        mDevicesNetworkMenu->setEnabled (isRunningOrPaused && count > 0);

        QString tip = tr ("<p style='white-space:pre'><nobr>Indicates the activity of the "
                           "network interfaces:</nobr>%1</p>", "Network adapters tooltip");
        QString info;

        for (ulong slot = 0; slot < maxCount; ++ slot)
        {
            CNetworkAdapter adapter = machine.GetNetworkAdapter (slot);
            if (adapter.GetEnabled())
                info += tr ("<br><nobr><b>Adapter %1 (%2)</b>: cable %3</nobr>", "Network adapters tooltip")
                    .arg (slot + 1)
                    .arg (vboxGlobal().toString (adapter.GetAttachmentType()))
                    .arg (adapter.GetCableConnected() ?
                          tr ("connected", "Network adapters tooltip") :
                          tr ("disconnected", "Network adapters tooltip"));
        }

        if (info.isNull())
            info = tr ("<br><nobr><b>All network adapters are disabled</b></nobr>", "Network adapters tooltip");

        mNetLed->setToolTip (tip.arg (info));
    }
    if (aElement & USBStuff)
    {
        if (!mUSBLed->isHidden())
        {
            QString tip = tr ("<p style='white-space:pre'><nobr>Indicates the activity of "
                              "the attached USB devices:</nobr>%1</p>", "USB device tooltip");
            QString info;

            CUSBController usbctl = machine.GetUSBController();
            if (!usbctl.isNull() && usbctl.GetEnabled())
            {
                mDevicesUSBMenu->setEnabled (isRunningOrPaused);

                CUSBDeviceVector devsvec = console.GetUSBDevices();
                for (int i = 0; i < devsvec.size(); ++ i)
                {
                    CUSBDevice usb = devsvec [i];
                    info += QString ("<br><b><nobr>%1</nobr></b>").arg (vboxGlobal().details (usb));
                }
                if (info.isNull())
                    info = tr ("<br><nobr><b>No USB devices attached</b></nobr>", "USB device tooltip");
            }
            else
            {
                mDevicesUSBMenu->setEnabled (false);
                info = tr ("<br><nobr><b>USB Controller is disabled</b></nobr>", "USB device tooltip");
            }

            mUSBLed->setToolTip (tip.arg (info));
        }
    }
    if (aElement & VRDPStuff)
    {
        CVRDPServer vrdpsrv = mSession.GetMachine().GetVRDPServer();
        if (!vrdpsrv.isNull())
        {
            /* update menu&status icon state */
            bool isVRDPEnabled = vrdpsrv.GetEnabled();
            mDevicesSwitchVrdpAction->setChecked (isVRDPEnabled);
#if 0 /* TODO: Allow to setup status-bar! */
            mVrdpLed->setState (isVRDPEnabled ? 1 : 0);

            QString tip = tr ("Indicates whether the Remote Display (VRDP Server) "
                              "is enabled (<img src=:/vrdp_16px.png/>) or not "
                              "(<img src=:/vrdp_disabled_16px.png/>).");
            if (vrdpsrv.GetEnabled())
                tip += tr ("<hr>VRDP Server is listening on port %1").arg (vrdpsrv.GetPort());
            mVrdpLed->setToolTip (tip);
#endif
        }
    }
    if (aElement & SharedFolderStuff)
    {
        QString tip = tr ("<p style='white-space:pre'><nobr>Indicates the activity of "
                          "shared folders:</nobr>%1</p>", "Shared folders tooltip");

        QString data;
        QMap <QString, QString> sfs;

        mDevicesSFMenu->setEnabled (true);

        /* Permanent folders */
        CSharedFolderVector psfvec = machine.GetSharedFolders();

        for (int i = 0; i < psfvec.size(); ++ i)
        {
            CSharedFolder sf = psfvec [i];
            sfs.insert (sf.GetName(), sf.GetHostPath());
        }

        /* Transient folders */
        CSharedFolderVector tsfvec = console.GetSharedFolders();

        for (int i = 0; i < tsfvec.size(); ++ i)
        {
            CSharedFolder sf = tsfvec[i];
            sfs.insert (sf.GetName(), sf.GetHostPath());
        }

        for (QMap <QString, QString>::const_iterator it = sfs.constBegin(); it != sfs.constEnd(); ++ it)
        {
            /* Select slashes depending on the OS type */
            if (VBoxGlobal::isDOSType (console.GetGuest().GetOSTypeId()))
                data += QString ("<br><nobr><b>\\\\vboxsvr\\%1&nbsp;</b></nobr><nobr>%2</nobr>")
                                 .arg (it.key(), it.value());
            else
                data += QString ("<br><nobr><b>%1&nbsp;</b></nobr><nobr>%2</nobr>")
                                 .arg (it.key(), it.value());
        }

        if (sfs.count() == 0)
            data = tr ("<br><nobr><b>No shared folders</b></nobr>", "Shared folders tooltip");

        mSFLed->setToolTip (tip.arg (data));
    }
    if (aElement & VirtualizationStuff)
    {
        bool virtEnabled = console.GetDebugger().GetHWVirtExEnabled();
        QString virtualization = virtEnabled ?
            VBoxGlobal::tr ("Enabled", "details report (VT-x/AMD-V)") :
            VBoxGlobal::tr ("Disabled", "details report (VT-x/AMD-V)");

        bool nestEnabled = console.GetDebugger().GetHWVirtExNestedPagingEnabled();
        QString nestedPaging = nestEnabled ?
            VBoxVMInformationDlg::tr ("Enabled", "nested paging") :
            VBoxVMInformationDlg::tr ("Disabled", "nested paging");

        QString tip (tr ("Indicates the status of the hardware virtualization "
                         "features used by this virtual machine:"
                         "<br><nobr><b>%1:</b>&nbsp;%2</nobr>"
                         "<br><nobr><b>%3:</b>&nbsp;%4</nobr>",
                         "Virtualization Stuff LED")
                         .arg (VBoxGlobal::tr ("VT-x/AMD-V", "details report"), virtualization)
                         .arg (VBoxVMInformationDlg::tr ("Nested Paging"), nestedPaging));

        int cpuCount = console.GetMachine().GetCPUCount();
        if (cpuCount > 1)
            tip += tr ("<br><nobr><b>%1:</b>&nbsp;%2</nobr>", "Virtualization Stuff LED")
                       .arg (VBoxGlobal::tr ("Processor(s)", "details report")).arg (cpuCount);

        mVirtLed->setToolTip (tip);
        mVirtLed->setState (virtEnabled);
    }
    if (aElement & PauseAction)
    {
        if (!mVmPauseAction->isChecked())
        {
            mVmPauseAction->setText (VBoxGlobal::insertKeyToActionText (tr ("&Pause"), "P"));
            mVmPauseAction->setStatusTip (tr ("Suspend the execution of the virtual machine"));
        }
        else
        {
            mVmPauseAction->setText (VBoxGlobal::insertKeyToActionText (tr ("R&esume"), "P"));
            mVmPauseAction->setStatusTip (tr ("Resume the execution of the virtual machine" ) );
        }
        mVmPauseAction->setEnabled (isRunningOrPaused);
    }
    if (aElement & DisableMouseIntegrAction)
    {
        if (!mVmDisableMouseIntegrAction->isChecked())
        {
            mVmDisableMouseIntegrAction->setText (VBoxGlobal::insertKeyToActionText (tr ("Disable &Mouse Integration"), "I"));
            mVmDisableMouseIntegrAction->setStatusTip (tr ("Temporarily disable host mouse pointer integration"));
        }
        else
        {
            mVmDisableMouseIntegrAction->setText (VBoxGlobal::insertKeyToActionText (tr ("Enable &Mouse Integration"), "I"));
            mVmDisableMouseIntegrAction->setStatusTip (tr ("Enable temporarily disabled host mouse pointer integration"));
        }
        if (mMachineState == KMachineState_Running)
            mVmDisableMouseIntegrAction->setEnabled (mConsole->isMouseAbsolute());
        else
            mVmDisableMouseIntegrAction->setEnabled (false);
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
    if (aSeamless || mConsole->isAutoresizeGuestActive())
    {
        QRect screen = aSeamless ?
            QApplication::desktop()->availableGeometry (this) :
            QApplication::desktop()->screenGeometry (this);
        ULONG64 availBits = mSession.GetMachine().GetVRAMSize() /* vram */
                          * _1M /* mb to bytes */
                          * 8; /* to bits */
        ULONG guestBpp = mConsole->console().GetDisplay().GetBitsPerPixel();
        ULONG64 usedBits = (screen.width() /* display width */
                         * screen.height() /* display height */
                         * guestBpp
                         + _1M * 8) /* current cache per screen - may be changed in future */
                         * mSession.GetMachine().GetMonitorCount() /**< @todo fix assumption that all screens have same resolution */
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

    AssertReturn (mConsole, false);
    AssertReturn ((mHiddenChildren.empty() == aOn), false);
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

        mConsole->console().GetDisplay().SetSeamlessMode (aOn);
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
        consoleSize = mConsole->frameSize();
        consoleSize -= QSize (mConsole->frameWidth() * 2, mConsole->frameWidth() * 2);

        /* Toggle console to manual resize mode. */
        mConsole->setIgnoreMainwndResize (true);

        /* Memorize the maximized state. */
        QDesktopWidget *dtw = QApplication::desktop();
        mWasMax = isWindowMaximized() &&
                  dtw->availableGeometry().width()  == frameSize().width() &&
                  dtw->availableGeometry().height() == frameSize().height();

        /* Save the previous scroll-view minimum size before entering
         * fullscreen/seamless state to restore this minimum size before
         * the exiting fullscreen. Required for correct scroll-view and
         * guest display update in SDL mode. */
        mPrevMinSize = mConsole->minimumSize();
        mConsole->setMinimumSize (0, 0);

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
        QList <QWidget*> list (findChildren <QWidget*> ());
        QList <QWidget*> excludes;
        excludes << centralWidget() << centralWidget()->findChildren <QWidget*> ();
        foreach (QWidget *w, list)
        {
            if (!excludes.contains (w))
            {
                if (!w->isHidden())
                {
                    w->hide();
                    mHiddenChildren.append (w);
                }
            }
        }

        /* Adjust colors and appearance. */
        mErasePalette = centralWidget()->palette();
        QPalette palette(mErasePalette);
        palette.setColor (centralWidget()->backgroundRole(), Qt::black);
        centralWidget()->setPalette (palette);
        centralWidget()->setAutoFillBackground (!aSeamless);
        mConsoleStyle = mConsole->frameStyle();
        mConsole->setFrameStyle (QFrame::NoFrame);
        mConsole->setMaximumSize (scrGeo.size());
        mConsole->setHorizontalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
        mConsole->setVerticalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
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
        mConsole->setMinimumSize (mPrevMinSize);

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
        mConsole->setFrameStyle (mConsoleStyle);
        mConsole->setMaximumSize (mConsole->sizeHint());
        mConsole->setHorizontalScrollBarPolicy (Qt::ScrollBarAsNeeded);
        mConsole->setVerticalScrollBarPolicy (Qt::ScrollBarAsNeeded);

        /* Show everything hidden when going fullscreen. */
        foreach (QPointer <QWidget> child, mHiddenChildren)
            if (child) child->show();
        mHiddenChildren.clear();
    }

    /* Set flag for waiting host resize if it awaited during mode entering */
    if ((mIsFullscreen || mIsSeamless) && (consoleSize != initialSize))
        mIsWaitingModeResize = true;

    if (!aOn)
    {
        /* Animation takes a bit long, the mini toolbar is still disappearing
         * when switched to normal mode so hide it completely */
        mMiniToolBar->hide();
        mMiniToolBar->updateDisplay (false, true);
    }

    /* Toggle qt full-screen mode */
    switchToFullscreen (aOn, aSeamless);

    if (aOn)
    {
        mMiniToolBar->setSeamlessMode (aSeamless);
        mMiniToolBar->updateDisplay (true, true);
    }

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
    mConsole->toggleFSMode (consoleSize);

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
    {
        /* Here we are going really fullscreen */
        setWindowState (windowState() ^ Qt::WindowFullScreen);
# ifdef QT_MAC_USE_COCOA
        /* Disable the auto show menubar feature of Qt in fullscreen. */
        if (aOn)
            SetSystemUIMode (kUIModeAllHidden, 0);
# endif /* QT_MAC_USE_COCOA */
    }

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

/**
 *  Closes the console view opened by openView().
 *  Does nothing if no console view was opened.
 */
void VBoxConsoleWnd::closeView()
{
    LogFlowFuncEnter();

    if (!mConsole)
    {
        LogFlow (("Already closed!\n"));
        LogFlowFuncLeave();
        return;
    }

    mConsole->detach();
    centralWidget()->layout()->removeWidget (mConsole);
    delete mConsole;
    mConsole = 0;
    mSession.Close();
    mSession.detach();

    LogFlowFuncLeave();
}

#ifdef VBOX_WITH_DEBUGGER_GUI

/**
 * Prepare the Debug menu.
 */
void VBoxConsoleWnd::dbgPrepareDebugMenu()
{
    /* The "Logging" item. */
    bool fEnabled = false;
    bool fChecked = false;
    CConsole console = mSession.GetConsole();
    if (console.isOk())
    {
        CMachineDebugger cdebugger = console.GetDebugger();
        if (console.isOk())
        {
            fEnabled = true;
            fChecked = cdebugger.GetLogEnabled() != FALSE;
        }
    }
    if (fEnabled != mDbgLoggingAction->isEnabled())
        mDbgLoggingAction->setEnabled (fEnabled);
    if (fChecked != mDbgLoggingAction->isChecked())
        mDbgLoggingAction->setChecked (fChecked);
}

/**
 * Called when the Debug->Statistics... menu item is selected.
 */
void VBoxConsoleWnd::dbgShowStatistics()
{
    if (dbgCreated())
        mDbgGuiVT->pfnShowStatistics (mDbgGui);
}

/**
 * Called when the Debug->Command Line... menu item is selected.
 */
void VBoxConsoleWnd::dbgShowCommandLine()
{
    if (dbgCreated())
        mDbgGuiVT->pfnShowCommandLine (mDbgGui);
}

/**
 * Called when the Debug->Logging menu item is selected.
 */
void VBoxConsoleWnd::dbgLoggingToggled (bool aState)
{
    NOREF(aState);
    CConsole console = mSession.GetConsole();
    if (console.isOk())
    {
        CMachineDebugger cdebugger = console.GetDebugger();
        if (console.isOk())
            cdebugger.SetLogEnabled (aState);
    }
}

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
    int rc = RTLdrGetSymbol (hLdrMod, "DBGGuiCreate", (void**) &pfnGuiCreate);
    if (RT_SUCCESS (rc))
    {
        ISession *pISession = mSession.raw();
        rc = pfnGuiCreate (pISession, &mDbgGui, &mDbgGuiVT);
        if (RT_SUCCESS (rc))
        {
            if (DBGGUIVT_ARE_VERSIONS_COMPATIBLE (mDbgGuiVT->u32Version, DBGGUIVT_VERSION) ||
                mDbgGuiVT->u32EndVersion == mDbgGuiVT->u32Version)
            {
                mDbgGuiVT->pfnSetParent (mDbgGui, (QWidget*) this);
                mDbgGuiVT->pfnSetMenu (mDbgGui, (QMenu*) mDbgMenu);
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

    mDbgGui = 0;
    mDbgGuiVT = 0;
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
        mDbgGui = 0;
        mDbgGuiVT = 0;
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

VBoxNetworkDialog::VBoxNetworkDialog (QWidget *aParent, CSession &aSession)
    : QIWithRetranslateUI <QDialog> (aParent)
    , mSettings (0)
    , mSession (aSession)
{
    setModal (true);
    /* Setup Dialog's options */
    setWindowIcon (QIcon (":/nw_16px.png"));
    setSizeGripEnabled (true);

    /* Setup main dialog's layout */
    QVBoxLayout *mainLayout = new QVBoxLayout (this);
    VBoxGlobal::setLayoutMargin (mainLayout, 10);
    mainLayout->setSpacing (10);

    /* Setup settings layout */
    mSettings = new VBoxVMSettingsNetworkPage (true);
    mSettings->setOrderAfter (this);
    VBoxGlobal::setLayoutMargin (mSettings->layout(), 0);
    mSettings->getFrom (aSession.GetMachine());
    mainLayout->addWidget (mSettings);

    /* Setup button's layout */
    QIDialogButtonBox *buttonBox = new QIDialogButtonBox (QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help);

    connect (buttonBox, SIGNAL (helpRequested()), &vboxProblem(), SLOT (showHelpHelpDialog()));
    connect (buttonBox, SIGNAL (accepted()), this, SLOT (accept()));
    connect (buttonBox, SIGNAL (rejected()), this, SLOT (reject()));
    mainLayout->addWidget (buttonBox);

    retranslateUi();
}

void VBoxNetworkDialog::retranslateUi()
{
    setWindowTitle (tr ("Network Adapters"));
}

void VBoxNetworkDialog::accept()
{
    mSettings->putBackTo();
    CMachine machine = mSession.GetMachine();
    machine.SaveSettings();
    if (!machine.isOk())
        vboxProblem().cannotSaveMachineSettings (machine);
    QDialog::accept();
}

void VBoxNetworkDialog::showEvent (QShowEvent *aEvent)
{
    resize (450, 300);
    VBoxGlobal::centerWidget (this, parentWidget());
    setMinimumWidth (400);
    QDialog::showEvent (aEvent);
}

VBoxSFDialog::VBoxSFDialog (QWidget *aParent, CSession &aSession)
    : QIWithRetranslateUI <QDialog> (aParent)
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
