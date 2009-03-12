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

#include "VBoxProblemReporter.h"
#include "VBoxHelpActions.h"

/* Qt includes */
#include <QMainWindow>
#include <QMap>
#include <QColor>
#include <QDialog>
#include <QMenu>

#ifdef VBOX_WITH_DEBUGGER_GUI
# include <VBox/dbggui.h>
#endif
#ifdef Q_WS_MAC
# include <ApplicationServices/ApplicationServices.h>
#endif

class QAction;
class QActionGroup;
class QLabel;
class QSpacerItem;

class VBoxConsoleView;
class QIStateIndicator;

class VBoxUSBMenu;
class VBoxSwitchMenu;

class VBoxChangeDockIconUpdateEvent;

/* We want to make the first action highlighted but not
 * selected, but Qt makes the both or neither one of this,
 * so, just move the focus to the next eligible object,
 * which will be the first menu action. This little
 * subclass made only for that purpose. */
class QIMenu : public QMenu
{
    Q_OBJECT;

public:

    QIMenu (QWidget *aParent) : QMenu (aParent) {}

    void selectFirstAction() { QMenu::focusNextChild(); }
};

class VBoxConsoleWnd : public QIWithRetranslateUI2<QMainWindow>
{
    Q_OBJECT;

public:

    VBoxConsoleWnd (VBoxConsoleWnd **aSelf,
                     QWidget* aParent = 0,
                     Qt::WindowFlags aFlags = Qt::Window);
    virtual ~VBoxConsoleWnd();

    bool openView (const CSession &session);

    void refreshView();

    bool isWindowMaximized() const
    {
#ifdef Q_WS_MAC
        /* On Mac OS X we didn't really jump to the fullscreen mode but
         * maximize the window. This situation has to be considered when
         * checking for maximized or fullscreen mode. */
        return !(isTrueSeamless()) && QMainWindow::isMaximized();
#else /* Q_WS_MAC */
        return QMainWindow::isMaximized();
#endif /* Q_WS_MAC */
    }
    bool isWindowFullScreen() const
    {
#ifdef Q_WS_MAC
        /* On Mac OS X we didn't really jump to the fullscreen mode but
         * maximize the window. This situation has to be considered when
         * checking for maximized or fullscreen mode. */
        return isTrueFullscreen() || isTrueSeamless();
#else /* Q_WS_MAC */
        return QMainWindow::isFullScreen();
#endif /* Q_WS_MAC */
    }

    bool isTrueFullscreen() const { return mIsFullscreen; }

    bool isTrueSeamless() const { return mIsSeamless; }

    void setMouseIntegrationLocked (bool aDisabled);

    void popupMainMenu (bool aCenter);

    void installGuestAdditionsFrom (const QString &aSource);

    void setMask (const QRegion &aRegion);

    void clearMask();

    KMachineState machineState() const { return machine_state; }

public slots:

    void changeDockIconUpdate (const VBoxChangeDockIconUpdateEvent &e);

signals:

    void closing();

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

    void closeView();

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
        VirtualizationStuff         = 0x400,
        AllStuff                    = 0xFFFF,
    };

    void updateAppearanceOf (int element);

    bool toggleFullscreenMode (bool, bool);

    void checkRequiredFeatures();

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
    void updateMediaDriveState (VBoxDefs::MediaType aType);
    void updateSharedFoldersState();

    void tryClose();

    void processGlobalSettingChange (const char *publicName, const char *name);

    void dbgPrepareDebugMenu();
    void dbgShowStatistics();
    void dbgShowCommandLine();
    void dbgLoggingToggled(bool aBool);

    void onExitFullscreen();
    void unlockActionsSwitch();

    void switchToFullscreen (bool aOn, bool aSeamless);
    void setViewInSeamlessMode (const QRect &aTargetRect);

private:

    /** Popup version of the main menu */
    QIMenu *mMainMenu;

    QActionGroup *mRunningActions;
    QActionGroup *mRunningOrPausedActions;

    /* Machine actions */
    QAction *mVmFullscreenAction;
    QAction *mVmSeamlessAction;
    QAction *mVmAutoresizeGuestAction;
    QAction *mVmAdjustWindowAction;
    QAction *mVmTypeCADAction;
#if defined(Q_WS_X11)
    QAction *mVmTypeCABSAction;
#endif
    QAction *mVmResetAction;
    QAction *mVmPauseAction;
    QAction *mVmACPIShutdownAction;
    QAction *mVmCloseAction;
    QAction *mVmTakeSnapshotAction;
    QAction *mVmDisableMouseIntegrAction;
    QAction *mVmShowInformationDlgAction;

    /* Devices actions */
    QAction *mDevicesMountFloppyImageAction;
    QAction *mDevicesUnmountFloppyAction;
    QAction *mDevicesMountDVDImageAction;
    QAction *mDevicesUnmountDVDAction;
    QAction *mDevicesSwitchVrdpAction;
    QAction *mDevicesSFDialogAction;
    QAction *mDevicesInstallGuestToolsAction;

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* Debugger actions */
    QAction *mDbgStatisticsAction;
    QAction *mDbgCommandLineAction;
    QAction *mDbgLoggingAction;
#endif

    /* Help actions */
    VBoxHelpActions mHelpActions;

    /* Machine popup menus */
    VBoxSwitchMenu *mVmAutoresizeMenu;
    VBoxSwitchMenu *mVmDisMouseIntegrMenu;

    /* Devices popup menus */
    bool mWaitForStatusBarChange : 1;
    bool mStatusBarChangedInside : 1;

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
    QIStateIndicator *mVirtLed;
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

    QRect mNormalGeo;
    QSize prev_min_size;

#ifdef Q_WS_WIN
    QRegion mPrevRegion;
#endif

#ifdef Q_WS_MAC
    QRegion mCurrRegion;
# ifndef QT_MAC_USE_COCOA
    EventHandlerRef mDarwinRegionEventHandlerRef;
# endif
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
    /** The handle to the debugger gui. */
    PDBGGUI mDbgGui;
    /** The virtual method table for the debugger GUI. */
    PCDBGGUIVT mDbgGuiVT;
#endif

#ifdef Q_WS_MAC
    /* For seamless maximizing */
    QRect mNormalGeometry;
    Qt::WindowFlags mSavedFlags;
    /* For the fade effect if the the window goes fullscreen */
    CGDisplayFadeReservationToken mFadeToken;
#endif
};


class VBoxVMSettingsSF;
class VBoxSFDialog : public QIWithRetranslateUI<QDialog>
{
    Q_OBJECT;

public:

    VBoxSFDialog (QWidget*, CSession&);

protected:

    void retranslateUi();

protected slots:

    virtual void accept();

protected:

    void showEvent (QShowEvent*);

private:

    VBoxVMSettingsSF *mSettings;
    CSession &mSession;
};


#endif // __VBoxConsoleWnd_h__
