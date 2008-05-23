/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxConsoleWnd class declaration
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#ifndef __VBoxConsoleWnd_h__
#define __VBoxConsoleWnd_h__

#include "COMDefs.h"
#include "QIWithRetranslateUI.h"

/* Qt includes */
#include <QMainWindow>
#include <QMap>
#include <QColor>
#include <QDialog>


#ifdef VBOX_WITH_DEBUGGER_GUI
# include <VBox/dbggui.h>
#endif
#ifdef Q_WS_MAC
# undef PAGE_SIZE
# undef PAGE_SHIFT
# include <Carbon/Carbon.h>
#endif

class QAction;
class QActionGroup;
class QLabel;
class QSpacerItem;
class QMenu;

class VBoxConsoleView;
class QIStateIndicator;

class VBoxUSBMenu;
class VBoxSwitchMenu;

class VBoxConsoleWnd : public QIWithRetranslateUI2<QMainWindow>
{
    Q_OBJECT;

public:

    VBoxConsoleWnd (VBoxConsoleWnd **aSelf,
                     QWidget* aParent = 0,
                     Qt::WFlags aFlags = Qt::WType_TopLevel);
    virtual ~VBoxConsoleWnd();

    bool openView (const CSession &session);
    void closeView();

    void refreshView();

    bool isTrueFullscreen() const { return mIsFullscreen; }

    bool isTrueSeamless() const { return mIsSeamless; }

    void setMouseIntegrationLocked (bool aDisabled);

    void popupMainMenu (bool aCenter);

    void installGuestAdditionsFrom (const QString &aSource);

    void setMask (const QRegion &aRegion);

#ifdef Q_WS_MAC
    CGImageRef dockImageState () const;
#endif

public slots:

protected:

    // events
    bool event (QEvent *e);
    void closeEvent (QCloseEvent *e);
#if defined(Q_WS_X11)
    bool x11Event (XEvent *event);
#endif

    void retranslateUi();

#ifdef VBOX_WITH_DEBUGGER_GUI
    bool dbgCreated();
    void dbgDestroy();
    void dbgAdjustRelativePos();
#endif

protected slots:

private:

    enum /* Stuff */
    {
        FloppyStuff                 = 0x01,
        DVDStuff                    = 0x02,
        HardDiskStuff               = 0x04,
        PauseAction                 = 0x08,
        NetworkStuff                = 0x10,
        DisableMouseIntegrAction    = 0x20,
        Caption                     = 0x40,
        USBStuff                    = 0x80,
        VRDPStuff                   = 0x100,
        SharedFolderStuff           = 0x200,
        AllStuff                    = 0xFFFF,
    };

    void updateAppearanceOf (int element);

    bool toggleFullscreenMode (bool, bool);

private slots:

    void finalizeOpenView();

    void activateUICustomizations();

    void vmFullscreen (bool on);
    void vmSeamless (bool on);
    void vmAutoresizeGuest (bool on);
    void vmAdjustWindow();

    void vmTypeCAD();
    void vmTypeCABS();
    void vmReset();
    void vmPause(bool);
    void vmACPIShutdown();
    void vmClose();
    void vmTakeSnapshot();
    void vmShowInfoDialog();
    void vmDisableMouseIntegr (bool);

    void devicesMountFloppyImage();
    void devicesUnmountFloppy();
    void devicesMountDVDImage();
    void devicesUnmountDVD();
    void devicesSwitchVrdp (bool);
    void devicesOpenSFDialog();
    void devicesInstallGuestAdditions();

    void prepareFloppyMenu();
    void prepareDVDMenu();
    void prepareNetworkMenu();

    void setDynamicMenuItemStatusTip (QAction *aAction);

    void captureFloppy (QAction *aAction);
    void captureDVD (QAction *aAction);
    void activateNetworkMenu (QAction *aAction);
    void switchUSB (QAction *aAction);

    void statusTipChanged (const QString &);
    void clearStatusBar();

    void showIndicatorContextMenu (QIStateIndicator *ind, QContextMenuEvent *e);

    void updateDeviceLights();
    void updateMachineState (KMachineState state);
    void updateMouseState (int state);
    void updateAdditionsState (const QString&, bool, bool, bool);
    void updateNetworkAdarptersState();
    void updateUsbState();
    void updateMediaState (VBoxDefs::DiskType aType);
    void updateSharedFoldersState();

    void tryClose();

    void processGlobalSettingChange (const char *publicName, const char *name);

    void dbgShowStatistics();
    void dbgShowCommandLine();

    void onExitFullscreen();
    void unlockActionsSwitch();

    void switchToFullscreen (bool aOn, bool aSeamless);
    void setViewInSeamlessMode (const QRect &aTargetRect);

private:

    /** Popup version of the main menu */
    QMenu *mMainMenu;

    QActionGroup *mRunningActions;
    QActionGroup *mRunningOrPausedActions;

    // Machine actions
    QAction *vmFullscreenAction;
    QAction *vmSeamlessAction;
    QAction *vmAutoresizeGuestAction;
    QAction *vmAdjustWindowAction;
    QAction *vmTypeCADAction;
#if defined(Q_WS_X11)
    QAction *vmTypeCABSAction;
#endif
    QAction *vmResetAction;
    QAction *vmPauseAction;
    QAction *vmACPIShutdownAction;
    QAction *vmCloseAction;
    QAction *vmTakeSnapshotAction;
    QAction *vmDisableMouseIntegrAction;
    QAction *vmShowInformationDlgAction;

    // Devices actions
    QAction *devicesMountFloppyImageAction;
    QAction *devicesUnmountFloppyAction;
    QAction *devicesMountDVDImageAction;
    QAction *devicesUnmountDVDAction;
    QAction *devicesSwitchVrdpAction;
    QAction *devicesSFDialogAction;
    QAction *devicesInstallGuestToolsAction;

#ifdef VBOX_WITH_DEBUGGER_GUI
    // Debugger actions
    QAction *dbgStatisticsAction;
    QAction *dbgCommandLineAction;
#endif

    // Help actions
    QAction *helpContentsAction;
    QAction *helpWebAction;
    QAction *helpRegisterAction;
    QAction *helpAboutAction;
    QAction *helpResetMessagesAction;

    // Machine popup menus
    VBoxSwitchMenu *vmAutoresizeMenu;
    VBoxSwitchMenu *vmDisMouseIntegrMenu;

    // Devices popup menus
    bool waitForStatusBarChange;
    bool statusBarChangedInside;

    QAction *mDevicesUSBMenuSeparator;
    QAction *mDevicesVRDPMenuSeparator;
    QAction *mDevicesSFMenuSeparator;

    QMenu *mVMMenu;
    QMenu *mDevicesMenu;
    QMenu *mDevicesMountFloppyMenu;
    QMenu *mDevicesMountDVDMenu;
    /* see showIndicatorContextMenu for a description of mDevicesSFMenu */
    /* QMenu *mDevicesSFMenu; */
    QMenu *mDevicesNetworkMenu;
    VBoxUSBMenu *mDevicesUSBMenu;
    /* VBoxSwitchMenu *mDevicesVRDPMenu; */
#ifdef VBOX_WITH_DEBUGGER_GUI
    // Debugger popup menu
    QMenu *mDbgMenu;
#endif
    QMenu *mHelpMenu;

    QSpacerItem *mShiftingSpacerLeft;
    QSpacerItem *mShiftingSpacerTop;
    QSpacerItem *mShiftingSpacerRight;
    QSpacerItem *mShiftingSpacerBottom;
    QSize mMaskShift;

    CSession csession;

    // widgets
    VBoxConsoleView *console;
    QIStateIndicator *hd_light, *cd_light, *fd_light, *net_light, *usb_light, *sf_light;
    QIStateIndicator *mouse_state, *hostkey_state;
    QIStateIndicator *autoresize_state;
    QIStateIndicator *vrdp_state;
    QWidget *hostkey_hbox;
    QLabel *hostkey_name;

    QTimer *idle_timer;
    KMachineState machine_state;
    QString caption_prefix;

    bool no_auto_close : 1;

    QMap <QAction *, CHostDVDDrive> hostDVDMap;
    QMap <QAction *, CHostFloppyDrive> hostFloppyMap;

    QPoint normal_pos;
    QSize normal_size;
    QSize prev_min_size;

#ifdef Q_WS_WIN32
    QRegion mPrevRegion;
#endif

#ifdef Q_WS_MAC
    QRegion mCurrRegion;
    EventHandlerRef mDarwinRegionEventHandlerRef;
#endif

    // variables for dealing with true fullscreen
    QRegion mStrictedRegion;
    bool mIsFullscreen : 1;
    bool mIsSeamless : 1;
    bool mIsSeamlessSupported : 1;
    bool mIsGraphicsSupported : 1;
    bool mIsWaitingModeResize : 1;
    bool was_max : 1;
    QObjectList hidden_children;
    int console_style;
    QPalette mErasePalette;

    bool mIsOpenViewFinished : 1;
    bool mIsFirstTimeStarted : 1;
    bool mIsAutoSaveMedia : 1;

#ifdef VBOX_WITH_DEBUGGER_GUI
    // Debugger GUI
    PDBGGUI dbg_gui;
#endif

#ifdef Q_WS_MAC
    /* For seamless maximizing */
    QRect mNormalGeometry;
    Qt::WindowFlags mSavedFlags;
    /* Dock images */
    CGImageRef dockImgStatePaused;
    CGImageRef dockImgStateSaving;
    CGImageRef dockImgStateRestoring;
    CGImageRef dockImgBack100x75;
    CGImageRef dockImgOS;
    /* For the fade effect if the the window goes fullscreen */
    CGDisplayFadeReservationToken mFadeToken;
#endif
};


class VBoxSharedFoldersSettings;
class VBoxSFDialog : public QDialog
{
    Q_OBJECT

public:

    VBoxSFDialog (QWidget*, CSession&);

protected slots:

    virtual void accept();

protected:

    void showEvent (QShowEvent*);

private:

    VBoxSharedFoldersSettings *mSettings;
    CSession &mSession;
};


#endif // __VBoxConsoleWnd_h__
