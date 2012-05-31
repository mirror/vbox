/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineLogic class implementation
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Local includes */
#include "COMDefs.h"
#include "QIFileDialog.h"
#include "UIActionPoolRuntime.h"
#include "UINetworkManager.h"
#include "UIDownloaderAdditions.h"
#include "UIIconPool.h"
#include "UIKeyboardHandler.h"
#include "UIMouseHandler.h"
#include "UIMachineLogic.h"
#include "UIMachineLogicFullscreen.h"
#include "UIMachineLogicNormal.h"
#include "UIMachineLogicSeamless.h"
#include "UIMachineLogicScale.h"
#include "UIMachineView.h"
#include "UIMachineWindow.h"
#include "UISession.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "VBoxTakeSnapshotDlg.h"
#include "VBoxVMInformationDlg.h"
#include "UISettingsDialogSpecific.h"
#include "UIVMLogViewer.h"
#ifdef Q_WS_MAC
# include "DockIconPreview.h"
# include "UIExtraDataEventHandler.h"
#endif /* Q_WS_MAC */

/* Global includes */
#include <iprt/path.h>

#ifdef VBOX_WITH_DEBUGGER_GUI
# include <iprt/ldr.h>
#endif /* VBOX_WITH_DEBUGGER_GUI */

#ifdef Q_WS_X11
# include <XKeyboard.h>
# include <QX11Info>
#endif /* Q_WS_X11 */

#include <QDesktopWidget>
#include <QDir>
#include <QFileInfo>
#include <QImageWriter>
#include <QPainter>
#include <QTimer>

struct MediumTarget
{
    MediumTarget() : name(QString("")), port(0), device(0), id(QString()), type(VBoxDefs::MediumType_Invalid) {}
    MediumTarget(const QString &strName, LONG iPort, LONG iDevice)
        : name(strName), port(iPort), device(iDevice), id(QString()), type(VBoxDefs::MediumType_Invalid) {}
    MediumTarget(const QString &strName, LONG iPort, LONG iDevice, const QString &strId)
        : name(strName), port(iPort), device(iDevice), id(strId), type(VBoxDefs::MediumType_Invalid) {}
    MediumTarget(const QString &strName, LONG iPort, LONG iDevice, VBoxDefs::MediumType eType)
        : name(strName), port(iPort), device(iDevice), id(QString()), type(eType) {}
    QString name;
    LONG port;
    LONG device;
    QString id;
    VBoxDefs::MediumType type;
};
Q_DECLARE_METATYPE(MediumTarget);

struct RecentMediumTarget
{
    RecentMediumTarget() : name(QString("")), port(0), device(0), location(QString()), type(VBoxDefs::MediumType_Invalid) {}
    RecentMediumTarget(const QString &strName, LONG iPort, LONG iDevice, const QString &strLocation, VBoxDefs::MediumType eType)
        : name(strName), port(iPort), device(iDevice), location(strLocation), type(eType) {}
    QString name;
    LONG port;
    LONG device;
    QString location;
    VBoxDefs::MediumType type;
};
Q_DECLARE_METATYPE(RecentMediumTarget);

struct USBTarget
{
    USBTarget() : attach(false), id(QString()) {}
    USBTarget(bool fAttach, const QString &strId)
        : attach(fAttach), id(strId) {}
    bool attach;
    QString id;
};
Q_DECLARE_METATYPE(USBTarget);

/* static */
UIMachineLogic* UIMachineLogic::create(QObject *pParent,
                                       UISession *pSession,
                                       UIVisualStateType visualStateType)
{
    UIMachineLogic *pLogic = 0;
    switch (visualStateType)
    {
        case UIVisualStateType_Normal:
            pLogic = new UIMachineLogicNormal(pParent, pSession);
            break;
        case UIVisualStateType_Fullscreen:
            pLogic = new UIMachineLogicFullscreen(pParent, pSession);
            break;
        case UIVisualStateType_Seamless:
            pLogic = new UIMachineLogicSeamless(pParent, pSession);
            break;
        case UIVisualStateType_Scale:
            pLogic = new UIMachineLogicScale(pParent, pSession);
            break;
    }
    return pLogic;
}

/* static */
void UIMachineLogic::destroy(UIMachineLogic *pWhichLogic)
{
    delete pWhichLogic;
}

void UIMachineLogic::prepare()
{
    /* Prepare required features: */
    prepareRequiredFeatures();

    /* Prepare session connections: */
    prepareSessionConnections();

    /* Prepare action groups:
     * Note: This has to be done before prepareActionConnections
     * cause here actions/menus are recreated. */
    prepareActionGroups();
    /* Prepare action connections: */
    prepareActionConnections();

    /* Prepare handlers: */
    prepareHandlers();

    /* Prepare machine window(s): */
    prepareMachineWindows();

#ifdef Q_WS_MAC
    /* Prepare dock: */
    prepareDock();
#endif /* Q_WS_MAC */

    /* Power up machine: */
    uisession()->powerUp();

    /* Initialization: */
    sltMachineStateChanged();
    sltAdditionsStateChanged();
    sltMouseCapabilityChanged();

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* Prepare debugger: */
    prepareDebugger();
#endif /* VBOX_WITH_DEBUGGER_GUI */

    /* Retranslate logic part: */
    retranslateUi();
}

void UIMachineLogic::cleanup()
{
#ifdef VBOX_WITH_DEBUGGER_GUI
    /* Cleanup debugger: */
    cleanupDebugger();
#endif /* VBOX_WITH_DEBUGGER_GUI */

#ifdef Q_WS_MAC
    /* Cleanup dock: */
    cleanupDock();
#endif /* Q_WS_MAC */

    /* Cleanup machine window(s): */
    cleanupMachineWindows();

    /* Cleanup handlers: */
    cleanupHandlers();

    /* Cleanup action groups: */
    cleanupActionGroups();
}

CSession& UIMachineLogic::session() const
{
    return uisession()->session();
}

UIMachineWindow* UIMachineLogic::mainMachineWindow() const
{
    /* Return null if windows are not created yet: */
    if (!isMachineWindowsCreated())
        return 0;

    /* Otherwise return first of windows: */
    return machineWindows()[0];
}

UIMachineWindow* UIMachineLogic::activeMachineWindow() const
{
    /* Return null if windows are not created yet: */
    if (!isMachineWindowsCreated())
        return 0;

    /* Check if there is an active window present: */
    for (int i = 0; i < machineWindows().size(); ++i)
    {
        UIMachineWindow *pIteratedWindow = machineWindows()[i];
        if (pIteratedWindow->isActiveWindow())
            return pIteratedWindow;
    }

    /* Return main machine window: */
    return mainMachineWindow();
}

#ifdef Q_WS_MAC
void UIMachineLogic::updateDockIcon()
{
    if (!isMachineWindowsCreated())
        return;

    if (   m_fIsDockIconEnabled
        && m_pDockIconPreview)
        if(UIMachineView *pView = machineWindows().at(m_DockIconPreviewMonitor)->machineView())
            if (CGImageRef image = pView->vmContentImage())
            {
                m_pDockIconPreview->updateDockPreview(image);
                CGImageRelease(image);
            }
}

void UIMachineLogic::updateDockIconSize(int screenId, int width, int height)
{
    if (!isMachineWindowsCreated())
        return;

    if (   m_fIsDockIconEnabled
        && m_pDockIconPreview
        && m_DockIconPreviewMonitor == screenId)
        m_pDockIconPreview->setOriginalSize(width, height);
}

UIMachineView* UIMachineLogic::dockPreviewView() const
{
    if (   m_fIsDockIconEnabled
        && m_pDockIconPreview)
        return machineWindows().at(m_DockIconPreviewMonitor)->machineView();
    return 0;
}
#endif /* Q_WS_MAC */

void UIMachineLogic::sltMachineStateChanged()
{
    /* Get machine state: */
    KMachineState state = uisession()->machineState();

    /* Update action groups: */
    m_pRunningActions->setEnabled(uisession()->isRunning());
    m_pRunningOrPausedActions->setEnabled(uisession()->isRunning() || uisession()->isPaused());

    switch (state)
    {
        case KMachineState_Stuck: // TODO: Test it!
        {
            /* Prevent machine view from resizing: */
            uisession()->setGuestResizeIgnored(true);

            /* Get console and log folder. */
            CConsole console = session().GetConsole();
            const QString &strLogFolder = console.GetMachine().GetLogFolder();

            /* Take the screenshot for debugging purposes and save it. */
            takeScreenshot(strLogFolder + "/VBox.png", "png");

            /* Warn the user about GURU: */
            if (msgCenter().remindAboutGuruMeditation(console, QDir::toNativeSeparators(strLogFolder)))
            {
                console.PowerDown();
                if (!console.isOk())
                    msgCenter().cannotStopMachine(console);
            }
            break;
        }
        case KMachineState_Paused:
        case KMachineState_TeleportingPausedVM:
        {
            QAction *pPauseAction = gActionPool->action(UIActionIndexRuntime_Toggle_Pause);
            if (!pPauseAction->isChecked())
            {
                /* Was paused from CSession side: */
                pPauseAction->blockSignals(true);
                pPauseAction->setChecked(true);
                pPauseAction->blockSignals(false);
            }
            break;
        }
        case KMachineState_Running:
        case KMachineState_Teleporting:
        case KMachineState_LiveSnapshotting:
        {
            QAction *pPauseAction = gActionPool->action(UIActionIndexRuntime_Toggle_Pause);
            if (pPauseAction->isChecked())
            {
                /* Was resumed from CSession side: */
                pPauseAction->blockSignals(true);
                pPauseAction->setChecked(false);
                pPauseAction->blockSignals(false);
            }
            break;
        }
        case KMachineState_PoweredOff:
        case KMachineState_Saved:
        case KMachineState_Teleported:
        case KMachineState_Aborted:
        {
            /* Close VM if it was turned off and closure allowed: */
            if (!isPreventAutoClose())
            {
                /* VM has been powered off, saved or aborted, no matter
                 * internally or externally. We must *safely* close VM window(s): */
                QTimer::singleShot(0, uisession(), SLOT(sltCloseVirtualSession()));
            }
            break;
        }
#ifdef Q_WS_X11
        case KMachineState_Starting:
        case KMachineState_Restoring:
        case KMachineState_TeleportingIn:
        {
            /* The keyboard handler may wish to do some release logging on startup.
             * Tell it that the logger is now active. */
            doXKeyboardLogging(QX11Info::display());
            break;
        }
#endif
        default:
            break;
    }

#ifdef Q_WS_MAC
    /* Update Dock Overlay: */
    updateDockOverlay();
#endif /* Q_WS_MAC */
}

void UIMachineLogic::sltAdditionsStateChanged()
{
    /* Update action states: */
    gActionPool->action(UIActionIndexRuntime_Toggle_GuestAutoresize)->setEnabled(uisession()->isGuestSupportsGraphics());
    gActionPool->action(UIActionIndexRuntime_Toggle_Seamless)->setEnabled(uisession()->isGuestSupportsSeamless());
}

void UIMachineLogic::sltMouseCapabilityChanged()
{
    /* Variable falgs: */
    bool fIsMouseSupportsAbsolute = uisession()->isMouseSupportsAbsolute();
    bool fIsMouseSupportsRelative = uisession()->isMouseSupportsRelative();
    bool fIsMouseHostCursorNeeded = uisession()->isMouseHostCursorNeeded();

    /* Update action state: */
    QAction *pAction = gActionPool->action(UIActionIndexRuntime_Toggle_MouseIntegration);
    pAction->setEnabled(fIsMouseSupportsAbsolute && fIsMouseSupportsRelative && !fIsMouseHostCursorNeeded);
    if (fIsMouseHostCursorNeeded)
        pAction->setChecked(false);
}

void UIMachineLogic::sltUSBDeviceStateChange(const CUSBDevice &device, bool fIsAttached, const CVirtualBoxErrorInfo &error)
{
    /* Check if USB device have anything to tell us: */
    if (!error.isNull())
    {
        if (fIsAttached)
            msgCenter().cannotAttachUSBDevice(session().GetConsole(), vboxGlobal().details(device), error);
        else
            msgCenter().cannotDetachUSBDevice(session().GetConsole(), vboxGlobal().details(device), error);
    }
}

void UIMachineLogic::sltRuntimeError(bool fIsFatal, const QString &strErrorId, const QString &strMessage)
{
    msgCenter().showRuntimeError(session().GetConsole(), fIsFatal, strErrorId, strMessage);
}

#ifdef Q_WS_MAC
void UIMachineLogic::sltShowWindows()
{
    for (int i=0; i < machineWindows().size(); ++i)
    {
        UIMachineWindow *pMachineWindow = machineWindows().at(i);
        /* Dunno what Qt thinks a window that has minimized to the dock
         * should be - it is not hidden, neither is it minimized. OTOH it is
         * marked shown and visible, but not activated. This latter isn't of
         * much help though, since at this point nothing is marked activated.
         * I might have overlooked something, but I'm buggered what if I know
         * what. So, I'll just always show & activate the stupid window to
         * make it get out of the dock when the user wishes to show a VM. */
        pMachineWindow->raise();
        pMachineWindow->activateWindow();
    }
}
#endif /* Q_WS_MAC */

UIMachineLogic::UIMachineLogic(QObject *pParent, UISession *pSession, UIVisualStateType visualStateType)
    : QIWithRetranslateUI3<QObject>(pParent)
    , m_pSession(pSession)
    , m_visualStateType(visualStateType)
    , m_pKeyboardHandler(0)
    , m_pMouseHandler(0)
    , m_pRunningActions(0)
    , m_pRunningOrPausedActions(0)
    , m_fIsWindowsCreated(false)
    , m_fIsPreventAutoClose(false)
#ifdef VBOX_WITH_DEBUGGER_GUI
    , m_pDbgGui(0)
    , m_pDbgGuiVT(0)
#endif /* VBOX_WITH_DEBUGGER_GUI */
#ifdef Q_WS_MAC
    , m_fIsDockIconEnabled(true)
    , m_pDockIconPreview(0)
    , m_pDockPreviewSelectMonitorGroup(0)
    , m_DockIconPreviewMonitor(0)
#endif /* Q_WS_MAC */
{
}

void UIMachineLogic::addMachineWindow(UIMachineWindow *pMachineWindow)
{
    m_machineWindowsList << pMachineWindow;
}

void UIMachineLogic::setKeyboardHandler(UIKeyboardHandler *pKeyboardHandler)
{
    m_pKeyboardHandler = pKeyboardHandler;
}

void UIMachineLogic::setMouseHandler(UIMouseHandler *pMouseHandler)
{
    m_pMouseHandler = pMouseHandler;
}

void UIMachineLogic::retranslateUi()
{
#ifdef Q_WS_MAC
    if (m_pDockPreviewSelectMonitorGroup)
    {
        const QList<QAction*> &actions = m_pDockPreviewSelectMonitorGroup->actions();
        for (int i = 0; i < actions.size(); ++i)
        {
            QAction *pAction = actions.at(i);
            pAction->setText(QApplication::translate("UIMachineLogic", "Preview Monitor %1").arg(pAction->data().toInt() + 1));
        }
    }
#endif /* Q_WS_MAC */
}

#ifdef Q_WS_MAC
void UIMachineLogic::updateDockOverlay()
{
    /* Only to an update to the realtime preview if this is enabled by the user
     * & we are in an state where the framebuffer is likely valid. Otherwise to
     * the overlay stuff only. */
    KMachineState state = uisession()->machineState();
    if (m_fIsDockIconEnabled &&
        (state == KMachineState_Running ||
         state == KMachineState_Paused ||
         state == KMachineState_Teleporting ||
         state == KMachineState_LiveSnapshotting ||
         state == KMachineState_Restoring ||
         state == KMachineState_TeleportingPausedVM ||
         state == KMachineState_TeleportingIn ||
         state == KMachineState_Saving ||
         state == KMachineState_DeletingSnapshotOnline ||
         state == KMachineState_DeletingSnapshotPaused))
        updateDockIcon();
    else if (m_pDockIconPreview)
        m_pDockIconPreview->updateDockOverlay();
}
#endif /* Q_WS_MAC */

void UIMachineLogic::prepareRequiredFeatures()
{
#ifdef Q_WS_MAC
# ifdef VBOX_WITH_ICHAT_THEATER
    /* Init shared AV manager: */
    initSharedAVManager();
# endif /* VBOX_WITH_ICHAT_THEATER */
#endif /* Q_WS_MAC */
}

void UIMachineLogic::prepareSessionConnections()
{
    /* We should check for entering/exiting requested modes: */
    connect(uisession(), SIGNAL(sigMachineStarted()), this, SLOT(sltCheckRequestedModes()));
    connect(uisession(), SIGNAL(sigAdditionsStateChange()), this, SLOT(sltCheckRequestedModes()));

    /* Machine state-change updater: */
    connect(uisession(), SIGNAL(sigMachineStateChange()), this, SLOT(sltMachineStateChanged()));

    /* Guest additions state-change updater: */
    connect(uisession(), SIGNAL(sigAdditionsStateChange()), this, SLOT(sltAdditionsStateChanged()));

    /* Mouse capability state-change updater: */
    connect(uisession(), SIGNAL(sigMouseCapabilityChange()), this, SLOT(sltMouseCapabilityChanged()));

    /* USB devices state-change updater: */
    connect(uisession(), SIGNAL(sigUSBDeviceStateChange(const CUSBDevice &, bool, const CVirtualBoxErrorInfo &)),
            this, SLOT(sltUSBDeviceStateChange(const CUSBDevice &, bool, const CVirtualBoxErrorInfo &)));

    /* Runtime errors notifier: */
    connect(uisession(), SIGNAL(sigRuntimeError(bool, const QString &, const QString &)),
            this, SLOT(sltRuntimeError(bool, const QString &, const QString &)));

#ifdef Q_WS_MAC
    /* Show windows: */
    connect(uisession(), SIGNAL(sigShowWindows()), this, SLOT(sltShowWindows()));
#endif /* Q_WS_MAC */
}

void UIMachineLogic::prepareActionGroups()
{
#ifdef Q_WS_MAC
    /* On Mac OS X, all QMenu's are consumed by Qt after they are added to
     * another QMenu or a QMenuBar. This means we have to recreate all QMenus
     * when creating a new QMenuBar. */
    gActionPool->recreateMenus();
#endif /* Q_WS_MAC */

    /* Create group for all actions that are enabled only when the VM is running.
     * Note that only actions whose enabled state depends exclusively on the
     * execution state of the VM are added to this group. */
    m_pRunningActions = new QActionGroup(this);
    m_pRunningActions->setExclusive(false);

    /* Create group for all actions that are enabled when the VM is running or paused.
     * Note that only actions whose enabled state depends exclusively on the
     * execution state of the VM are added to this group. */
    m_pRunningOrPausedActions = new QActionGroup(this);
    m_pRunningOrPausedActions->setExclusive(false);

    /* Move actions into running actions group: */
    m_pRunningActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_TypeCAD));
#ifdef Q_WS_X11
    m_pRunningActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_TypeCABS));
#endif
    m_pRunningActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_Reset));
    m_pRunningActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_Shutdown));
    m_pRunningActions->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_Fullscreen));
    m_pRunningActions->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_Seamless));
    m_pRunningActions->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_Scale));
    m_pRunningActions->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_GuestAutoresize));
    m_pRunningActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_AdjustWindow));

    /* Move actions into running-n-paused actions group: */
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_SettingsDialog));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_TakeSnapshot));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_TakeScreenshot));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_InformationDialog));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Menu_MouseIntegration));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_MouseIntegration));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_Pause));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Menu_OpticalDevices));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Menu_FloppyDevices));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Menu_USBDevices));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Menu_NetworkAdapters));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_NetworkAdaptersDialog));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Menu_SharedFolders));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_SharedFoldersDialog));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_VRDEServer));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_InstallGuestTools));
}

void UIMachineLogic::prepareActionConnections()
{
    /* "Machine" actions connections: */
    connect(gActionPool->action(UIActionIndexRuntime_Simple_SettingsDialog), SIGNAL(triggered()),
            this, SLOT(sltOpenVMSettingsDialog()));
    connect(gActionPool->action(UIActionIndexRuntime_Simple_TakeSnapshot), SIGNAL(triggered()),
            this, SLOT(sltTakeSnapshot()));
    connect(gActionPool->action(UIActionIndexRuntime_Simple_TakeScreenshot), SIGNAL(triggered()),
            this, SLOT(sltTakeScreenshot()));
    connect(gActionPool->action(UIActionIndexRuntime_Simple_InformationDialog), SIGNAL(triggered()),
            this, SLOT(sltShowInformationDialog()));
    connect(gActionPool->action(UIActionIndexRuntime_Toggle_MouseIntegration), SIGNAL(toggled(bool)),
            this, SLOT(sltToggleMouseIntegration(bool)));
    connect(gActionPool->action(UIActionIndexRuntime_Simple_TypeCAD), SIGNAL(triggered()),
            this, SLOT(sltTypeCAD()));
#ifdef Q_WS_X11
    connect(gActionPool->action(UIActionIndexRuntime_Simple_TypeCABS), SIGNAL(triggered()),
            this, SLOT(sltTypeCABS()));
#endif
    connect(gActionPool->action(UIActionIndexRuntime_Toggle_Pause), SIGNAL(toggled(bool)),
            this, SLOT(sltPause(bool)));
    connect(gActionPool->action(UIActionIndexRuntime_Simple_Reset), SIGNAL(triggered()),
            this, SLOT(sltReset()));
    connect(gActionPool->action(UIActionIndexRuntime_Simple_Shutdown), SIGNAL(triggered()),
            this, SLOT(sltACPIShutdown()));
    connect(gActionPool->action(UIActionIndexRuntime_Simple_Close), SIGNAL(triggered()),
            this, SLOT(sltClose()));

    /* "View" actions connections: */
    connect(gActionPool->action(UIActionIndexRuntime_Toggle_GuestAutoresize), SIGNAL(toggled(bool)),
            this, SLOT(sltToggleGuestAutoresize(bool)));
    connect(gActionPool->action(UIActionIndexRuntime_Simple_AdjustWindow), SIGNAL(triggered()),
            this, SLOT(sltAdjustWindow()));

    /* "Devices" actions connections: */
    connect(gActionPool->action(UIActionIndexRuntime_Menu_OpticalDevices)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareStorageMenu()));
    connect(gActionPool->action(UIActionIndexRuntime_Menu_FloppyDevices)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareStorageMenu()));
    connect(gActionPool->action(UIActionIndexRuntime_Menu_USBDevices)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareUSBMenu()));
    connect(gActionPool->action(UIActionIndexRuntime_Simple_NetworkAdaptersDialog), SIGNAL(triggered()),
            this, SLOT(sltOpenNetworkAdaptersDialog()));
    connect(gActionPool->action(UIActionIndexRuntime_Simple_SharedFoldersDialog), SIGNAL(triggered()),
            this, SLOT(sltOpenSharedFoldersDialog()));
    connect(gActionPool->action(UIActionIndexRuntime_Toggle_VRDEServer), SIGNAL(toggled(bool)),
            this, SLOT(sltSwitchVrde(bool)));
    connect(gActionPool->action(UIActionIndexRuntime_Simple_InstallGuestTools), SIGNAL(triggered()),
            this, SLOT(sltInstallGuestAdditions()));

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* "Debug" actions connections: */
    connect(gActionPool->action(UIActionIndexRuntime_Menu_Debug)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareDebugMenu()));
    connect(gActionPool->action(UIActionIndexRuntime_Simple_Statistics), SIGNAL(triggered()),
            this, SLOT(sltShowDebugStatistics()));
    connect(gActionPool->action(UIActionIndexRuntime_Simple_CommandLine), SIGNAL(triggered()),
            this, SLOT(sltShowDebugCommandLine()));
    connect(gActionPool->action(UIActionIndexRuntime_Toggle_Logging), SIGNAL(toggled(bool)),
            this, SLOT(sltLoggingToggled(bool)));
    connect(gActionPool->action(UIActionIndex_Simple_LogDialog), SIGNAL(triggered()),
            this, SLOT(sltShowLogDialog()));
#endif
}

void UIMachineLogic::prepareHandlers()
{
    /* Create keyboard-handler: */
    setKeyboardHandler(UIKeyboardHandler::create(this, visualStateType()));

    /* Create mouse-handler: */
    setMouseHandler(UIMouseHandler::create(this, visualStateType()));
}

#ifdef Q_WS_MAC
void UIMachineLogic::prepareDock()
{
    QMenu *pDockMenu = gActionPool->action(UIActionIndexRuntime_Menu_Dock)->menu();

    /* Add all VM menu entries to the dock menu. Leave out close and stuff like
     * this. */
    QList<QAction*> actions = gActionPool->action(UIActionIndexRuntime_Menu_Machine)->menu()->actions();
    for (int i=0; i < actions.size(); ++i)
        if (actions.at(i)->menuRole() == QAction::NoRole)
            pDockMenu->addAction(actions.at(i));
    pDockMenu->addSeparator();

    QMenu *pDockSettingsMenu = gActionPool->action(UIActionIndexRuntime_Menu_DockSettings)->menu();
    QActionGroup *pDockPreviewModeGroup = new QActionGroup(this);
    QAction *pDockDisablePreview = gActionPool->action(UIActionIndexRuntime_Toggle_DockDisableMonitor);
    pDockPreviewModeGroup->addAction(pDockDisablePreview);
    QAction *pDockEnablePreviewMonitor = gActionPool->action(UIActionIndexRuntime_Toggle_DockPreviewMonitor);
    pDockPreviewModeGroup->addAction(pDockEnablePreviewMonitor);
    pDockSettingsMenu->addActions(pDockPreviewModeGroup->actions());

    connect(pDockPreviewModeGroup, SIGNAL(triggered(QAction*)),
            this, SLOT(sltDockPreviewModeChanged(QAction*)));
    connect(gEDataEvents, SIGNAL(sigDockIconAppearanceChange(bool)),
            this, SLOT(sltChangeDockIconUpdate(bool)));

    /* Monitor selection if there are more than one monitor */
    int cGuestScreens = uisession()->session().GetMachine().GetMonitorCount();
    if (cGuestScreens > 1)
    {
        pDockSettingsMenu->addSeparator();
        m_DockIconPreviewMonitor = qMin(session().GetMachine().GetExtraData(VBoxDefs::GUI_RealtimeDockIconUpdateMonitor).toInt(), cGuestScreens - 1);
        m_pDockPreviewSelectMonitorGroup = new QActionGroup(this);
        for (int i = 0; i < cGuestScreens; ++i)
        {
            QAction *pAction = new QAction(m_pDockPreviewSelectMonitorGroup);
            pAction->setCheckable(true);
            pAction->setData(i);
            if (m_DockIconPreviewMonitor == i)
                pAction->setChecked(true);
        }
        pDockSettingsMenu->addActions(m_pDockPreviewSelectMonitorGroup->actions());
        connect(m_pDockPreviewSelectMonitorGroup, SIGNAL(triggered(QAction*)),
                this, SLOT(sltDockPreviewMonitorChanged(QAction*)));
    }

    pDockMenu->addMenu(pDockSettingsMenu);

    /* Add it to the dock. */
    ::darwinSetDockIconMenu(pDockMenu);

    /* Now the dock icon preview */
    QString osTypeId = session().GetConsole().GetGuest().GetOSTypeId();
    m_pDockIconPreview = new UIDockIconPreview(uisession(), vboxGlobal().vmGuestOSTypeIcon(osTypeId));

    QString strTest = session().GetMachine().GetExtraData(VBoxDefs::GUI_RealtimeDockIconUpdateEnabled).toLower();
    /* Default to true if it is an empty value */
    bool f = (strTest.isEmpty() || strTest == "true");
    if (f)
        pDockEnablePreviewMonitor->setChecked(true);
    else
    {
        pDockDisablePreview->setChecked(true);
        if(m_pDockPreviewSelectMonitorGroup)
            m_pDockPreviewSelectMonitorGroup->setEnabled(false);
    }

    /* Default to true if it is an empty value */
    setDockIconPreviewEnabled(f);
    updateDockOverlay();
}
#endif /* Q_WS_MAC */

#ifdef VBOX_WITH_DEBUGGER_GUI
void UIMachineLogic::prepareDebugger()
{
    CMachine machine = uisession()->session().GetMachine();
    if (!machine.isNull() && vboxGlobal().isDebuggerAutoShowEnabled(machine))
    {
        /* console in upper left corner of the desktop. */
//        QRect rct (0, 0, 0, 0);
//        QDesktopWidget *desktop = QApplication::desktop();
//        if (desktop)
//            rct = desktop->availableGeometry(pos());
//        move (QPoint (rct.x(), rct.y()));

        if (vboxGlobal().isDebuggerAutoShowStatisticsEnabled(machine))
            sltShowDebugStatistics();
        if (vboxGlobal().isDebuggerAutoShowCommandLineEnabled(machine))
            sltShowDebugCommandLine();

        if (!vboxGlobal().isStartPausedEnabled())
            sltPause(false);
    }
}
#endif /* VBOX_WITH_DEBUGGER_GUI */

#ifdef VBOX_WITH_DEBUGGER_GUI
void UIMachineLogic::cleanupDebugger()
{
    /* Close debugger: */
    dbgDestroy();
}
#endif /* VBOX_WITH_DEBUGGER_GUI */

#ifdef Q_WS_MAC
void UIMachineLogic::cleanupDock()
{
    if (m_pDockIconPreview)
    {
        delete m_pDockIconPreview;
        m_pDockIconPreview = 0;
    }
}
#endif /* Q_WS_MAC */

void UIMachineLogic::cleanupHandlers()
{
    /* Cleanup mouse-handler: */
    UIMouseHandler::destroy(mouseHandler());

    /* Cleanup keyboard-handler: */
    UIKeyboardHandler::destroy(keyboardHandler());
}

void UIMachineLogic::cleanupActionGroups()
{
}

void UIMachineLogic::sltCheckRequestedModes()
{
    /* Do not try to enter extended mode if machine was not started yet: */
    if (!uisession()->isRunning() && !uisession()->isPaused())
        return;

    /* If seamless mode is requested, supported and we are NOT currently in seamless mode: */
    if (uisession()->isSeamlessModeRequested() &&
        uisession()->isGuestSupportsSeamless() &&
        visualStateType() != UIVisualStateType_Seamless)
    {
        uisession()->setSeamlessModeRequested(false);
        QAction *pSeamlessModeAction = gActionPool->action(UIActionIndexRuntime_Toggle_Seamless);
        AssertMsg(!pSeamlessModeAction->isChecked(), ("Seamless action should not be triggered before us!\n"));
        QTimer::singleShot(0, pSeamlessModeAction, SLOT(trigger()));
    }
    /* If seamless mode is NOT requested, NOT supported and we are currently in seamless mode: */
    else if (!uisession()->isSeamlessModeRequested() &&
             !uisession()->isGuestSupportsSeamless() &&
             visualStateType() == UIVisualStateType_Seamless)
    {
        uisession()->setSeamlessModeRequested(true);
        QAction *pSeamlessModeAction = gActionPool->action(UIActionIndexRuntime_Toggle_Seamless);
        AssertMsg(pSeamlessModeAction->isChecked(), ("Seamless action should not be triggered before us!\n"));
        QTimer::singleShot(0, pSeamlessModeAction, SLOT(trigger()));
    }
}

void UIMachineLogic::sltToggleGuestAutoresize(bool fEnabled)
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    /* Toggle guest-autoresize feature for all view(s)! */
    foreach(UIMachineWindow *pMachineWindow, machineWindows())
        pMachineWindow->machineView()->setGuestAutoresizeEnabled(fEnabled);
}

void UIMachineLogic::sltAdjustWindow()
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    /* Adjust all window(s)! */
    foreach(UIMachineWindow *pMachineWindow, machineWindows())
    {
        /* Exit maximized window state if actual: */
        if (pMachineWindow->isMaximized())
            pMachineWindow->showNormal();

        /* Normalize view's geometry: */
        pMachineWindow->machineView()->normalizeGeometry(true);
    }
}

void UIMachineLogic::sltToggleMouseIntegration(bool fOff)
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    /* Disable/Enable mouse-integration for all view(s): */
    mouseHandler()->setMouseIntegrationEnabled(!fOff);
}

void UIMachineLogic::sltTypeCAD()
{
    CKeyboard keyboard = session().GetConsole().GetKeyboard();
    Assert(!keyboard.isNull());
    keyboard.PutCAD();
    AssertWrapperOk(keyboard);
}

#ifdef Q_WS_X11
void UIMachineLogic::sltTypeCABS()
{
    CKeyboard keyboard = session().GetConsole().GetKeyboard();
    Assert(!keyboard.isNull());
    static QVector<LONG> aSequence(6);
    aSequence[0] = 0x1d; /* Ctrl down */
    aSequence[1] = 0x38; /* Alt down */
    aSequence[2] = 0x0E; /* Backspace down */
    aSequence[3] = 0x8E; /* Backspace up */
    aSequence[4] = 0xb8; /* Alt up */
    aSequence[5] = 0x9d; /* Ctrl up */
    keyboard.PutScancodes(aSequence);
    AssertWrapperOk(keyboard);
}
#endif /* Q_WS_X11 */

void UIMachineLogic::sltTakeSnapshot()
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    /* Remember the paused state. */
    bool fWasPaused = uisession()->isPaused();
    if (!fWasPaused)
    {
        /* Suspend the VM and ignore the close event if failed to do so.
         * pause() will show the error message to the user. */
        if (!uisession()->pause())
            return;
    }

    CMachine machine = session().GetMachine();

    VBoxTakeSnapshotDlg dlg(activeMachineWindow(), machine);

    QString strTypeId = machine.GetOSTypeId();
    dlg.mLbIcon->setPixmap(vboxGlobal().vmGuestOSTypeIcon(strTypeId));

    /* Search for the max available filter index. */
    QString strNameTemplate = QApplication::translate("UIMachineLogic", "Snapshot %1");
    int iMaxSnapshotIndex = searchMaxSnapshotIndex(machine, machine.FindSnapshot(QString()), strNameTemplate);
    dlg.mLeName->setText(strNameTemplate.arg(++ iMaxSnapshotIndex));

    if (dlg.exec() == QDialog::Accepted)
    {
        CConsole console = session().GetConsole();

        CProgress progress = console.TakeSnapshot(dlg.mLeName->text().trimmed(), dlg.mTeDescription->toPlainText());

        if (console.isOk())
        {
            /* Show the "Taking Snapshot" progress dialog */
            msgCenter().showModalProgressDialog(progress, machine.GetName(), ":/progress_snapshot_create_90px.png", 0, true);

            if (progress.GetResultCode() != 0)
                msgCenter().cannotTakeSnapshot(progress);
        }
        else
            msgCenter().cannotTakeSnapshot(console);
    }

    /* Restore the running state if needed. */
    if (!fWasPaused)
    {
        /* Make sure machine-state-change callback is processed: */
        QApplication::sendPostedEvents(uisession(), UIConsoleEventType_StateChange);
        /* Unpause VM: */
        uisession()->unpause();
    }
}

void UIMachineLogic::sltTakeScreenshot()
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    /* Which image formats for writing does this Qt version know of? */
    QList<QByteArray> formats = QImageWriter::supportedImageFormats();
    QStringList filters;
    /* Build a filters list out of it. */
    for (int i = 0; i < formats.size(); ++i)
    {
        const QString &s = formats.at(i) + " (*." + formats.at(i).toLower() + ")";
        /* Check there isn't an entry already (even if it just uses another capitalization) */
        if (filters.indexOf(QRegExp(QRegExp::escape(s), Qt::CaseInsensitive)) == -1)
            filters << s;
    }
    /* Try to select some common defaults. */
    QString strFilter;
    int i = filters.indexOf(QRegExp(".*png.*", Qt::CaseInsensitive));
    if (i == -1)
    {
        i = filters.indexOf(QRegExp(".*jpe+g.*", Qt::CaseInsensitive));
        if (i == -1)
            i = filters.indexOf(QRegExp(".*bmp.*", Qt::CaseInsensitive));
    }
    if (i != -1)
    {
        filters.prepend(filters.takeAt(i));
        strFilter = filters.first();
    }
    /* Request the filename from the user. */
    const CMachine &machine = session().GetMachine();
    QFileInfo fi(machine.GetSettingsFilePath());
    QString strAbsolutePath(fi.absolutePath());
    QString strCompleteBaseName(fi.completeBaseName());
    QString strStart = QDir(strAbsolutePath).absoluteFilePath(strCompleteBaseName);
    QString strFilename = QIFileDialog::getSaveFileName(strStart,
                                                        filters.join(";;"),
                                                        activeMachineWindow(),
                                                        tr("Select a filename for the screenshot ..."),
                                                        &strFilter,
                                                        true /* resolve symlinks */,
                                                        true /* confirm overwrite */);
    /* Do the screenshot. */
    if (!strFilename.isEmpty())
        takeScreenshot(strFilename, strFilter.split(" ").value(0, "png"));
}

void UIMachineLogic::sltShowInformationDialog()
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    VBoxVMInformationDlg::createInformationDlg(mainMachineWindow());
}

void UIMachineLogic::sltReset()
{
    /* Confirm/Reset current console: */
    if (msgCenter().confirmVMReset(0))
        session().GetConsole().Reset();

    /* TODO_NEW_CORE: On reset the additional screens didn't get a display
       update. Emulate this for now until it get fixed. */
    ulong uMonitorCount = session().GetMachine().GetMonitorCount();
    for (ulong uScreenId = 1; uScreenId < uMonitorCount; ++uScreenId)
        machineWindows().at(uScreenId)->update();
}

void UIMachineLogic::sltPause(bool fOn)
{
    uisession()->setPause(fOn);
}

void UIMachineLogic::sltACPIShutdown()
{
    /* Get console: */
    CConsole console = session().GetConsole();

    /* Warn the user about ACPI is not available if so: */
    if (!console.GetGuestEnteredACPIMode())
        return msgCenter().cannotSendACPIToMachine();

    /* Send ACPI shutdown signal, warn if failed: */
    console.PowerButton();
    if (!console.isOk())
        msgCenter().cannotACPIShutdownMachine(console);
}

void UIMachineLogic::sltClose()
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    /* Do not try to close machine-window if restricted: */
    if (isPreventAutoClose())
        return;

    /* First, we have to close/hide any opened modal & popup application widgets.
     * We have to make sure such window is hidden even if close-event was rejected.
     * We are re-throwing this slot if any widget present to test again.
     * If all opened widgets are closed/hidden, we can try to close machine-window: */
    QWidget *pWidget = QApplication::activeModalWidget() ? QApplication::activeModalWidget() :
                       QApplication::activePopupWidget() ? QApplication::activePopupWidget() : 0;
    if (pWidget)
    {
        /* Closing/hiding all we found: */
        pWidget->close();
        if (!pWidget->isHidden())
            pWidget->hide();
        QTimer::singleShot(0, this, SLOT(sltClose()));
        return;
    }

    /* Try to close active machine-window: */
    activeMachineWindow()->close();
}

void UIMachineLogic::sltOpenVMSettingsDialog(const QString &strCategory /* = QString() */)
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    /* Create VM settings dialog on the heap!
     * Its necessary to allow QObject hierarchy cleanup to delete this dialog if necessary: */
    QPointer<UISettingsDialogMachine> pDialog = new UISettingsDialogMachine(activeMachineWindow(),
                                                                            session().GetMachine().GetId(),
                                                                            strCategory, QString());
    /* Executing VM settings dialog.
     * This blocking function calls for the internal event-loop to process all further events,
     * including event which can delete the dialog itself. */
    pDialog->execute();
    /* Delete dialog if its still valid: */
    if (pDialog)
        delete pDialog;
}

void UIMachineLogic::sltOpenNetworkAdaptersDialog()
{
    /* Open VM settings : Network page: */
    sltOpenVMSettingsDialog("#network");
}

void UIMachineLogic::sltOpenSharedFoldersDialog()
{
    /* Do not process if additions are not loaded! */
    if (!uisession()->isGuestAdditionsActive())
        msgCenter().remindAboutGuestAdditionsAreNotActive(activeMachineWindow());

    /* Open VM settings : Shared folders page: */
    sltOpenVMSettingsDialog("#sfolders");
}

void UIMachineLogic::sltPrepareStorageMenu()
{
    /* Get the sender() menu: */
    QMenu *pMenu = qobject_cast<QMenu*>(sender());
    AssertMsg(pMenu, ("This slot should only be called on hovering storage menu!\n"));
    pMenu->clear();

    /* Short way to common storage menus: */
    QMenu *pOpticalDevicesMenu = gActionPool->action(UIActionIndexRuntime_Menu_OpticalDevices)->menu();
    QMenu *pFloppyDevicesMenu = gActionPool->action(UIActionIndexRuntime_Menu_FloppyDevices)->menu();

    /* Determine medium & device types: */
    VBoxDefs::MediumType mediumType = pMenu == pOpticalDevicesMenu ? VBoxDefs::MediumType_DVD :
                                      pMenu == pFloppyDevicesMenu  ? VBoxDefs::MediumType_Floppy :
                                                                     VBoxDefs::MediumType_Invalid;
    KDeviceType deviceType = vboxGlobal().mediumTypeToGlobal(mediumType);
    AssertMsg(mediumType != VBoxDefs::MediumType_Invalid, ("Incorrect storage medium type!\n"));
    AssertMsg(deviceType != KDeviceType_Null, ("Incorrect storage device type!\n"));

    /* Fill attachments menu: */
    const CMachine &machine = session().GetMachine();
    const CMediumAttachmentVector &attachments = machine.GetMediumAttachments();
    for (int iAttachmentIndex = 0; iAttachmentIndex < attachments.size(); ++iAttachmentIndex)
    {
        /* Current attachment: */
        const CMediumAttachment &attachment = attachments[iAttachmentIndex];
        /* Current controller: */
        const CStorageController &controller = machine.GetStorageControllerByName(attachment.GetController());
        /* If controller present and device type correct: */
        if (!controller.isNull() && (attachment.GetType() == deviceType))
        {
            /* Current attachment attributes: */
            const CMedium &currentMedium = attachment.GetMedium();
            QString strCurrentId = currentMedium.isNull() ? QString::null : currentMedium.GetId();
            QString strCurrentLocation = currentMedium.isNull() ? QString::null : currentMedium.GetLocation();

            /* Attachment menu item: */
            QMenu *pAttachmentMenu = 0;
            if (pMenu->menuAction()->data().toInt() > 1)
            {
                pAttachmentMenu = new QMenu(pMenu);
                pAttachmentMenu->setTitle(QString("%1 (%2)").arg(controller.GetName())
                                          .arg(vboxGlobal().toString(StorageSlot(controller.GetBus(),
                                                                                 attachment.GetPort(),
                                                                                 attachment.GetDevice()))));
                switch (controller.GetBus())
                {
                    case KStorageBus_IDE:
                        pAttachmentMenu->setIcon(QIcon(":/ide_16px.png")); break;
                    case KStorageBus_SATA:
                        pAttachmentMenu->setIcon(QIcon(":/sata_16px.png")); break;
                    case KStorageBus_SCSI:
                        pAttachmentMenu->setIcon(QIcon(":/scsi_16px.png")); break;
                    case KStorageBus_Floppy:
                        pAttachmentMenu->setIcon(QIcon(":/floppy_16px.png")); break;
                    default:
                        break;
                }
                pMenu->addMenu(pAttachmentMenu);
            }
            else pAttachmentMenu = pMenu;

            /* Prepare choose-existing-medium action: */
            QAction *pChooseExistingMediumAction = pAttachmentMenu->addAction(QIcon(":/select_file_16px.png"), QString(),
                                                                              this, SLOT(sltMountStorageMedium()));
            pChooseExistingMediumAction->setData(QVariant::fromValue(MediumTarget(controller.GetName(), attachment.GetPort(),
                                                                                  attachment.GetDevice(), mediumType)));

            /* Prepare choose-particular-medium actions: */
            CMediumVector mediums;
            QString strRecentMediumAddress;
            switch (mediumType)
            {
                case VBoxDefs::MediumType_DVD:
                    mediums = vboxGlobal().host().GetDVDDrives();
                    strRecentMediumAddress = VBoxDefs::GUI_RecentListCD;
                    break;
                case VBoxDefs::MediumType_Floppy:
                    mediums = vboxGlobal().host().GetFloppyDrives();
                    strRecentMediumAddress = VBoxDefs::GUI_RecentListFD;
                    break;
                default:
                    break;
            }

            /* Prepare choose-host-drive actions: */
            for (int iHostDriveIndex = 0; iHostDriveIndex < mediums.size(); ++iHostDriveIndex)
            {
                const CMedium &medium = mediums[iHostDriveIndex];
                bool fIsHostDriveUsed = false;
                for (int iOtherAttachmentIndex = 0; iOtherAttachmentIndex < attachments.size(); ++iOtherAttachmentIndex)
                {
                    const CMediumAttachment &otherAttachment = attachments[iOtherAttachmentIndex];
                    if (otherAttachment != attachment)
                    {
                        const CMedium &otherMedium = otherAttachment.GetMedium();
                        if (!otherMedium.isNull() && otherMedium.GetId() == medium.GetId())
                        {
                            fIsHostDriveUsed = true;
                            break;
                        }
                    }
                }
                if (!fIsHostDriveUsed)
                {
                    QAction *pChooseHostDriveAction = pAttachmentMenu->addAction(VBoxMedium(medium, mediumType).name(),
                                                                                 this, SLOT(sltMountStorageMedium()));
                    pChooseHostDriveAction->setCheckable(true);
                    pChooseHostDriveAction->setChecked(!currentMedium.isNull() && medium.GetId() == strCurrentId);
                    pChooseHostDriveAction->setData(QVariant::fromValue(MediumTarget(controller.GetName(), attachment.GetPort(),
                                                                                     attachment.GetDevice(), medium.GetId())));
                }
            }

            /* Prepare choose-recent-medium actions: */
            QStringList recentMediumList = vboxGlobal().virtualBox().GetExtraData(strRecentMediumAddress).split(';');
            /* For every list-item: */
            for (int i = 0; i < recentMediumList.size(); ++i)
            {
                QString strRecentMediumLocation = QDir::toNativeSeparators(recentMediumList[i]);
                if (QFile::exists(strRecentMediumLocation))
                {
                    bool fIsRecentMediumUsed = false;
                    for (int iOtherAttachmentIndex = 0; iOtherAttachmentIndex < attachments.size(); ++iOtherAttachmentIndex)
                    {
                        const CMediumAttachment &otherAttachment = attachments[iOtherAttachmentIndex];
                        if (otherAttachment != attachment)
                        {
                            const CMedium &otherMedium = otherAttachment.GetMedium();
                            if (!otherMedium.isNull() && otherMedium.GetLocation() == strRecentMediumLocation)
                            {
                                fIsRecentMediumUsed = true;
                                break;
                            }
                        }
                    }
                    if (!fIsRecentMediumUsed)
                    {
                        QAction *pChooseRecentMediumAction = pAttachmentMenu->addAction(QFileInfo(strRecentMediumLocation).fileName(),
                                                                                        this, SLOT(sltMountRecentStorageMedium()));
                        pChooseRecentMediumAction->setCheckable(true);
                        pChooseRecentMediumAction->setChecked(!currentMedium.isNull() && strRecentMediumLocation == strCurrentLocation);
                        pChooseRecentMediumAction->setData(QVariant::fromValue(RecentMediumTarget(controller.GetName(), attachment.GetPort(),
                                                                                                  attachment.GetDevice(), strRecentMediumLocation, mediumType)));
                        pChooseRecentMediumAction->setToolTip(strRecentMediumLocation);

                    }
                }
            }

            /* Insert separator: */
            pAttachmentMenu->addSeparator();

            /* Unmount Medium action: */
            QAction *unmountMediumAction = new QAction(pAttachmentMenu);
            unmountMediumAction->setEnabled(!currentMedium.isNull());
            unmountMediumAction->setData(QVariant::fromValue(MediumTarget(controller.GetName(),
                                                                          attachment.GetPort(),
                                                                          attachment.GetDevice())));
            connect(unmountMediumAction, SIGNAL(triggered(bool)), this, SLOT(sltMountStorageMedium()));
            pAttachmentMenu->addAction(unmountMediumAction);

            /* Switch CD/FD naming */
            switch (mediumType)
            {
                case VBoxDefs::MediumType_DVD:
                    pChooseExistingMediumAction->setText(QApplication::translate("UIMachineSettingsStorage", "Choose a virtual CD/DVD disk file..."));
                    unmountMediumAction->setText(QApplication::translate("UIMachineSettingsStorage", "Remove disk from virtual drive"));
                    unmountMediumAction->setIcon(UIIconPool::iconSet(":/cd_unmount_16px.png",
                                                                     ":/cd_unmount_dis_16px.png"));
                    break;
                case VBoxDefs::MediumType_Floppy:
                    pChooseExistingMediumAction->setText(QApplication::translate("UIMachineSettingsStorage", "Choose a virtual floppy disk file..."));
                    unmountMediumAction->setText(QApplication::translate("UIMachineSettingsStorage", "Remove disk from virtual drive"));
                    unmountMediumAction->setIcon(UIIconPool::iconSet(":/fd_unmount_16px.png",
                                                                     ":/fd_unmount_dis_16px.png"));
                    break;
                default:
                    break;
            }
        }
    }

    if (pMenu->menuAction()->data().toInt() == 0)
    {
        /* Empty menu item */
        Assert(pMenu->isEmpty());
        QAction *pEmptyMenuAction = new QAction(pMenu);
        pEmptyMenuAction->setEnabled(false);
        switch (mediumType)
        {
            case VBoxDefs::MediumType_DVD:
                pEmptyMenuAction->setText(QApplication::translate("UIMachineLogic", "No CD/DVD Devices Attached"));
                pEmptyMenuAction->setToolTip(QApplication::translate("UIMachineLogic", "No CD/DVD devices attached to that VM"));
                break;
            case VBoxDefs::MediumType_Floppy:
                pEmptyMenuAction->setText(QApplication::translate("UIMachineLogic", "No Floppy Devices Attached"));
                pEmptyMenuAction->setToolTip(QApplication::translate("UIMachineLogic", "No floppy devices attached to that VM"));
                break;
            default:
                break;
        }
        pEmptyMenuAction->setIcon(UIIconPool::iconSet(":/delete_16px.png", ":/delete_dis_16px.png"));
        pMenu->addAction(pEmptyMenuAction);
    }
}

void UIMachineLogic::sltMountStorageMedium()
{
    /* Get sender action: */
    QAction *action = qobject_cast<QAction*>(sender());
    AssertMsg(action, ("This slot should only be called on selecting storage menu item!\n"));

    /* Get current machine: */
    CMachine machine = session().GetMachine();

    /* Get mount-target: */
    MediumTarget target = action->data().value<MediumTarget>();

    /* Current mount-target attributes: */
    CMediumAttachment currentAttachment = machine.GetMediumAttachment(target.name, target.port, target.device);
    CMedium currentMedium = currentAttachment.GetMedium();
    QString currentId = currentMedium.isNull() ? QString("") : currentMedium.GetId();

    /* New mount-target attributes: */
    QString newId = QString("");
    bool fSelectWithMediaManager = target.type != VBoxDefs::MediumType_Invalid;

    /* Open Virtual Media Manager to select image id: */
    if (fSelectWithMediaManager)
    {
        /* Search for already used images: */
        QStringList usedImages;
        foreach (const CMediumAttachment &attachment, machine.GetMediumAttachments())
        {
            CMedium medium = attachment.GetMedium();
            if (attachment != currentAttachment && !medium.isNull() && !medium.GetHostDrive())
                usedImages << medium.GetId();
        }
        /* To that moment application focus already returned to machine-view,
         * so the keyboard already captured too.
         * We should clear application focus from machine-view now
         * to let file-open dialog get it. That way the keyboard will be released too: */
        if (QApplication::focusWidget())
            QApplication::focusWidget()->clearFocus();
        /* Call for file-open window: */
        QString strMachineFolder(QFileInfo(machine.GetSettingsFilePath()).absolutePath());
        QString strMediumId = vboxGlobal().openMediumWithFileOpenDialog(target.type, activeMachineWindow(),
                                                                        strMachineFolder);
        activeMachineWindow()->machineView()->setFocus();
        if (!strMediumId.isNull())
            newId = strMediumId;
        else return;
    }
    /* Use medium which was sent: */
    else if (!target.id.isNull() && target.id != currentId)
        newId = target.id;

    bool fMount = !newId.isEmpty();

    VBoxMedium vmedium = vboxGlobal().findMedium(newId);
    CMedium medium = vmedium.medium();              // @todo r=dj can this be cached somewhere?

    /* Remount medium to the predefined port/device: */
    bool fWasMounted = false;
    machine.MountMedium(target.name, target.port, target.device, medium, false /* force */);
    if (machine.isOk())
        fWasMounted = true;
    else
    {
        /* Ask for force remounting: */
        if (msgCenter().cannotRemountMedium(0, machine, vboxGlobal().findMedium (fMount ? newId : currentId), fMount, true /* retry? */) == QIMessageBox::Ok)
        {
            /* Force remount medium to the predefined port/device: */
            machine.MountMedium(target.name, target.port, target.device, medium, true /* force */);
            if (machine.isOk())
                fWasMounted = true;
            else
                msgCenter().cannotRemountMedium(0, machine, vboxGlobal().findMedium (fMount ? newId : currentId), fMount, false /* retry? */);
        }
    }

    /* Save medium mounted at runtime */
    if (fWasMounted && !uisession()->isIgnoreRuntimeMediumsChanging())
    {
        machine.SaveSettings();
        if (!machine.isOk())
            msgCenter().cannotSaveMachineSettings(machine);
    }
}

void UIMachineLogic::sltMountRecentStorageMedium()
{
    /* Get sender action: */
    QAction *pSender = qobject_cast<QAction*>(sender());
    AssertMsg(pSender, ("This slot should only be called on selecting storage menu item!\n"));

    /* Get mount-target: */
    RecentMediumTarget target = pSender->data().value<RecentMediumTarget>();

    /* Get new medium id: */
    QString strNewId = vboxGlobal().openMedium(target.type, target.location);

    if (!strNewId.isEmpty())
    {
        /* Get current machine: */
        CMachine machine = session().GetMachine();

        /* Get current medium id: */
        const CMediumAttachment &currentAttachment = machine.GetMediumAttachment(target.name, target.port, target.device);
        CMedium currentMedium = currentAttachment.GetMedium();
        QString strCurrentId = currentMedium.isNull() ? QString("") : currentMedium.GetId();

        /* Should we mount or unmount? */
        bool fMount = strNewId != strCurrentId;

        /* Prepare target medium: */
        const VBoxMedium &vboxMedium = fMount ? vboxGlobal().findMedium(strNewId) : VBoxMedium();
        const CMedium &comMedium = fMount ? vboxMedium.medium() : CMedium();

        /* 'Mounted' flag: */
        bool fWasMounted = false;

        /* Try to mount medium to the predefined port/device: */
        machine.MountMedium(target.name, target.port, target.device, comMedium, false /* force? */);
        if (machine.isOk())
            fWasMounted = true;
        else
        {
            /* Ask for force remounting: */
            if (msgCenter().cannotRemountMedium(0, machine, vboxGlobal().findMedium(fMount ? strNewId : strCurrentId), fMount, true /* retry? */) == QIMessageBox::Ok)
            {
                /* Force remount medium to the predefined port/device: */
                machine.MountMedium(target.name, target.port, target.device, comMedium, true /* force? */);
                if (machine.isOk())
                    fWasMounted = true;
                else
                    msgCenter().cannotRemountMedium(0, machine, vboxGlobal().findMedium(fMount ? strNewId : strCurrentId), fMount, false /* retry? */);
            }
        }

        /* Save medium mounted at runtime if necessary: */
        if (fWasMounted && !uisession()->isIgnoreRuntimeMediumsChanging())
        {
            machine.SaveSettings();
            if (!machine.isOk())
                msgCenter().cannotSaveMachineSettings(machine);
        }
    }
}

void UIMachineLogic::sltPrepareUSBMenu()
{
    /* Get and check the sender menu object: */
    QMenu *pMenu = qobject_cast<QMenu*>(sender());
    QMenu *pUSBDevicesMenu = gActionPool->action(UIActionIndexRuntime_Menu_USBDevices)->menu();
    AssertMsg(pMenu == pUSBDevicesMenu, ("This slot should only be called on hovering USB menu!\n"));
    Q_UNUSED(pUSBDevicesMenu);

    /* Clear menu initially: */
    pMenu->clear();

    /* Get current host: */
    CHost host = vboxGlobal().host();

    /* Get host USB device list: */
    CHostUSBDeviceVector devices = host.GetUSBDevices();

    /* Fill USB device menu: */
    bool fIsUSBListEmpty = devices.size() == 0;
    /* If device list is empty: */
    if (fIsUSBListEmpty)
    {
        /* Add only one - "empty" action: */
        QAction *pEmptyMenuAction = new QAction(pMenu);
        pEmptyMenuAction->setEnabled(false);
        pEmptyMenuAction->setText(tr("No USB Devices Connected"));
        pEmptyMenuAction->setToolTip(tr("No supported devices connected to the host PC"));
        pEmptyMenuAction->setIcon(UIIconPool::iconSet(":/delete_16px.png", ":/delete_dis_16px.png"));
        pMenu->addAction(pEmptyMenuAction);
    }
    /* If device list is NOT empty: */
    else
    {
        /* Populate menu with host USB devices: */
        for (int i = 0; i < devices.size(); ++i)
        {
            /* Get current host USB device: */
            const CHostUSBDevice& hostDevice = devices[i];
            /* Get USB device from current host USB device: */
            CUSBDevice device(hostDevice);

            /* Create USB device action: */
            QAction *pAttachUSBAction = new QAction(vboxGlobal().details(device), pMenu);
            pAttachUSBAction->setCheckable(true);
            connect(pAttachUSBAction, SIGNAL(triggered(bool)), this, SLOT(sltAttachUSBDevice()));
            pMenu->addAction(pAttachUSBAction);

            /* Check if that USB device was already attached to this session: */
            CConsole console = session().GetConsole();
            CUSBDevice attachedDevice = console.FindUSBDeviceById(device.GetId());
            pAttachUSBAction->setChecked(!attachedDevice.isNull());
            pAttachUSBAction->setEnabled(hostDevice.GetState() != KUSBDeviceState_Unavailable);

            /* Set USB attach data: */
            pAttachUSBAction->setData(QVariant::fromValue(USBTarget(!pAttachUSBAction->isChecked(), device.GetId())));
            pAttachUSBAction->setToolTip(vboxGlobal().toolTip(device));
        }
    }
}

void UIMachineLogic::sltAttachUSBDevice()
{
    /* Get and check sender action object: */
    QAction *pAction = qobject_cast<QAction*>(sender());
    AssertMsg(pAction, ("This slot should only be called on selecting USB menu item!\n"));

    /* Get operation target: */
    USBTarget target = pAction->data().value<USBTarget>();

    /* Get current console: */
    CConsole console = session().GetConsole();

    /* Attach USB device: */
    if (target.attach)
    {
        /* Try to attach corresponding device: */
        console.AttachUSBDevice(target.id);
        /* Check if console is OK: */
        if (!console.isOk())
        {
            /* Get current host: */
            CHost host = vboxGlobal().host();
            /* Search the host for the corresponding USB device: */
            CHostUSBDevice hostDevice = host.FindUSBDeviceById(target.id);
            /* Get USB device from host USB device: */
            CUSBDevice device(hostDevice);
            /* Show a message about procedure failure: */
            msgCenter().cannotAttachUSBDevice(console, vboxGlobal().details(device));
        }
    }
    /* Detach USB device: */
    else
    {
        /* Search the console for the corresponding USB device: */
        CUSBDevice device = console.FindUSBDeviceById(target.id);
        /* Try to detach corresponding device: */
        console.DetachUSBDevice(target.id);
        /* Check if console is OK: */
        if (!console.isOk())
        {
            /* Show a message about procedure failure: */
            msgCenter().cannotDetachUSBDevice(console, vboxGlobal().details(device));
        }
    }
}

void UIMachineLogic::sltSwitchVrde(bool fOn)
{
    /* Enable VRDE server if possible: */
    CVRDEServer server = session().GetMachine().GetVRDEServer();
    AssertMsg(!server.isNull(), ("VRDE server should not be null!\n"));
    server.SetEnabled(fOn);
}

void UIMachineLogic::sltInstallGuestAdditions()
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    char strAppPrivPath[RTPATH_MAX];
    int rc = RTPathAppPrivateNoArch(strAppPrivPath, sizeof(strAppPrivPath));
    AssertRC (rc);

    QString strSrc1 = QString(strAppPrivPath) + "/VBoxGuestAdditions.iso";
    QString strSrc2 = qApp->applicationDirPath() + "/additions/VBoxGuestAdditions.iso";

    /* Check the standard image locations */
    if (QFile::exists(strSrc1))
        return uisession()->sltInstallGuestAdditionsFrom(strSrc1);
    else if (QFile::exists(strSrc2))
        return uisession()->sltInstallGuestAdditionsFrom(strSrc2);

    /* Check for the already registered image */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    const QString &name = QString("VBoxGuestAdditions_%1.iso").arg(vboxGlobal().vboxVersionStringNormalized());

    CMediumVector vec = vbox.GetDVDImages();
    for (CMediumVector::ConstIterator it = vec.begin(); it != vec.end(); ++ it)
    {
        QString path = it->GetLocation();
        /* Compare the name part ignoring the file case */
        QString fn = QFileInfo(path).fileName();
        if (RTPathCompare(name.toUtf8().constData(), fn.toUtf8().constData()) == 0)
            return uisession()->sltInstallGuestAdditionsFrom(path);
    }

    /* If downloader is running already: */
    if (UIDownloaderAdditions::current())
    {
        /* Just show network access manager: */
        gNetworkManager->show();
    }
    /* Else propose to download additions: */
    else if (msgCenter().cannotFindGuestAdditions(QDir::toNativeSeparators(strSrc1), QDir::toNativeSeparators(strSrc2)))
    {
        /* Create Additions downloader: */
        UIDownloaderAdditions *pDl = UIDownloaderAdditions::create();
        /* After downloading finished => propose to install the Additions: */
        connect(pDl, SIGNAL(sigDownloadFinished(const QString&)), uisession(), SLOT(sltInstallGuestAdditionsFrom(const QString&)));
        /* Start downloading: */
        pDl->start();
    }
}

#ifdef VBOX_WITH_DEBUGGER_GUI

void UIMachineLogic::sltPrepareDebugMenu()
{
    /* The "Logging" item. */
    bool fEnabled = false;
    bool fChecked = false;
    CConsole console = session().GetConsole();
    if (console.isOk())
    {
        CMachineDebugger cdebugger = console.GetDebugger();
        if (console.isOk())
        {
            fEnabled = true;
            fChecked = cdebugger.GetLogEnabled() != FALSE;
        }
    }
    if (fEnabled != gActionPool->action(UIActionIndexRuntime_Toggle_Logging)->isEnabled())
        gActionPool->action(UIActionIndexRuntime_Toggle_Logging)->setEnabled(fEnabled);
    if (fChecked != gActionPool->action(UIActionIndexRuntime_Toggle_Logging)->isChecked())
        gActionPool->action(UIActionIndexRuntime_Toggle_Logging)->setChecked(fChecked);
}

void UIMachineLogic::sltShowDebugStatistics()
{
    if (dbgCreated())
    {
        keyboardHandler()->setDebuggerActive();
        m_pDbgGuiVT->pfnShowStatistics(m_pDbgGui);
    }
}

void UIMachineLogic::sltShowDebugCommandLine()
{
    if (dbgCreated())
    {
        keyboardHandler()->setDebuggerActive();
        m_pDbgGuiVT->pfnShowCommandLine(m_pDbgGui);
    }
}

void UIMachineLogic::sltLoggingToggled(bool fState)
{
    NOREF(fState);
    CConsole console = session().GetConsole();
    if (console.isOk())
    {
        CMachineDebugger cdebugger = console.GetDebugger();
        if (console.isOk())
            cdebugger.SetLogEnabled(fState);
    }
}

void UIMachineLogic::sltShowLogDialog()
{
    /* Show VM Log Viewer: */
    UIVMLogViewer::showLogViewerFor(activeMachineWindow(), session().GetMachine());
}

#endif /* VBOX_WITH_DEBUGGER_GUI */

#ifdef Q_WS_MAC
void UIMachineLogic::sltDockPreviewModeChanged(QAction *pAction)
{
    CMachine machine = session().GetMachine();
    if (!machine.isNull())
    {
        bool fEnabled = true;
        if (pAction == gActionPool->action(UIActionIndexRuntime_Toggle_DockDisableMonitor))
            fEnabled = false;

        machine.SetExtraData(VBoxDefs::GUI_RealtimeDockIconUpdateEnabled, fEnabled ? "true" : "false");
        updateDockOverlay();
    }
}

void UIMachineLogic::sltDockPreviewMonitorChanged(QAction *pAction)
{
    CMachine machine = session().GetMachine();
    if (!machine.isNull())
    {
        int monitor = pAction->data().toInt();
        machine.SetExtraData(VBoxDefs::GUI_RealtimeDockIconUpdateMonitor, QString::number(monitor));
        updateDockOverlay();
    }
}

void UIMachineLogic::sltChangeDockIconUpdate(bool fEnabled)
{
    if (isMachineWindowsCreated())
    {
        setDockIconPreviewEnabled(fEnabled);
        if (m_pDockPreviewSelectMonitorGroup)
        {
            m_pDockPreviewSelectMonitorGroup->setEnabled(fEnabled);
            CMachine machine = session().GetMachine();
            m_DockIconPreviewMonitor = qMin(machine.GetExtraData(VBoxDefs::GUI_RealtimeDockIconUpdateMonitor).toInt(), (int)machine.GetMonitorCount() - 1);
        }
        /* Resize the dock icon in the case the preview monitor has changed. */
        QSize size = machineWindows().at(m_DockIconPreviewMonitor)->machineView()->size();
        updateDockIconSize(m_DockIconPreviewMonitor, size.width(), size.height());
        updateDockOverlay();
    }
}
#endif /* Q_WS_MAC */

int UIMachineLogic::searchMaxSnapshotIndex(const CMachine &machine,
                                           const CSnapshot &snapshot,
                                           const QString &strNameTemplate)
{
    int iMaxIndex = 0;
    QRegExp regExp(QString("^") + strNameTemplate.arg("([0-9]+)") + QString("$"));
    if (!snapshot.isNull())
    {
        /* Check the current snapshot name */
        QString strName = snapshot.GetName();
        int iPos = regExp.indexIn(strName);
        if (iPos != -1)
            iMaxIndex = regExp.cap(1).toInt() > iMaxIndex ? regExp.cap(1).toInt() : iMaxIndex;
        /* Traversing all the snapshot children */
        foreach (const CSnapshot &child, snapshot.GetChildren())
        {
            int iMaxIndexOfChildren = searchMaxSnapshotIndex(machine, child, strNameTemplate);
            iMaxIndex = iMaxIndexOfChildren > iMaxIndex ? iMaxIndexOfChildren : iMaxIndex;
        }
    }
    return iMaxIndex;
}

void UIMachineLogic::takeScreenshot(const QString &strFile, const QString &strFormat /* = "png" */) const
{
    /* Get console: */
    const CConsole &console = session().GetConsole();
    CDisplay display = console.GetDisplay();
    const int cGuestScreens = uisession()->session().GetMachine().GetMonitorCount();
    QList<QImage> images;
    ULONG uMaxWidth  = 0;
    ULONG uMaxHeight = 0;
    /* First create screenshots of all guest screens and save them in a list.
     * Also sum the width of all images and search for the biggest image height. */
    for (int i = 0; i < cGuestScreens; ++i)
    {
        ULONG width  = 0;
        ULONG height = 0;
        ULONG bpp    = 0;
        display.GetScreenResolution(i, width, height, bpp);
        uMaxWidth  += width;
        uMaxHeight  = RT_MAX(uMaxHeight, height);
        QImage shot = QImage(width, height, QImage::Format_RGB32);
        display.TakeScreenShot(i, shot.bits(), shot.width(), shot.height());
        images << shot;
    }
    /* Create a image which will hold all sub images vertically. */
    QImage bigImg = QImage(uMaxWidth, uMaxHeight, QImage::Format_RGB32);
    QPainter p(&bigImg);
    ULONG w = 0;
    /* Paint them. */
    for (int i = 0; i < images.size(); ++i)
    {
        p.drawImage(w, 0, images.at(i));
        w += images.at(i).width();
    }
    p.end();

    /* Save the big image in the requested format. */
    const QFileInfo fi(strFile);
    const QString &strPath   = fi.absolutePath() + "/" + fi.baseName();
    const QString &strSuffix = fi.suffix().isEmpty() ? strFormat : fi.suffix();
    bigImg.save(QFile::encodeName(QString("%1.%2").arg(strPath).arg(strSuffix)),
                strFormat.toAscii().constData());
}

#ifdef VBOX_WITH_DEBUGGER_GUI
bool UIMachineLogic::dbgCreated()
{
    if (m_pDbgGui)
        return true;

    RTLDRMOD hLdrMod = vboxGlobal().getDebuggerModule();
    if (hLdrMod == NIL_RTLDRMOD)
        return false;

    PFNDBGGUICREATE pfnGuiCreate;
    int rc = RTLdrGetSymbol(hLdrMod, "DBGGuiCreate", (void**)&pfnGuiCreate);
    if (RT_SUCCESS(rc))
    {
        ISession *pISession = session().raw();
        rc = pfnGuiCreate(pISession, &m_pDbgGui, &m_pDbgGuiVT);
        if (RT_SUCCESS(rc))
        {
            if (   DBGGUIVT_ARE_VERSIONS_COMPATIBLE(m_pDbgGuiVT->u32Version, DBGGUIVT_VERSION)
                || m_pDbgGuiVT->u32EndVersion == m_pDbgGuiVT->u32Version)
            {
                m_pDbgGuiVT->pfnSetParent(m_pDbgGui, activeMachineWindow());
                m_pDbgGuiVT->pfnSetMenu(m_pDbgGui, gActionPool->action(UIActionIndexRuntime_Menu_Debug));
                dbgAdjustRelativePos();
                return true;
            }

            LogRel(("DBGGuiCreate failed, incompatible versions (loaded %#x/%#x, expected %#x)\n",
                    m_pDbgGuiVT->u32Version, m_pDbgGuiVT->u32EndVersion, DBGGUIVT_VERSION));
        }
        else
            LogRel(("DBGGuiCreate failed, rc=%Rrc\n", rc));
    }
    else
        LogRel(("RTLdrGetSymbol(,\"DBGGuiCreate\",) -> %Rrc\n", rc));

    m_pDbgGui = 0;
    m_pDbgGuiVT = 0;
    return false;
}

void UIMachineLogic::dbgDestroy()
{
    if (m_pDbgGui)
    {
        m_pDbgGuiVT->pfnDestroy(m_pDbgGui);
        m_pDbgGui = 0;
        m_pDbgGuiVT = 0;
    }
}

void UIMachineLogic::dbgAdjustRelativePos()
{
    if (m_pDbgGui)
    {
        QRect rct = activeMachineWindow()->frameGeometry();
        m_pDbgGuiVT->pfnAdjustRelativePos(m_pDbgGui, rct.x(), rct.y(), rct.width(), rct.height());
    }
}
#endif

