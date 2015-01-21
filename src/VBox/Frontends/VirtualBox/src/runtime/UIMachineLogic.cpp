/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineLogic class implementation.
 */

/*
 * Copyright (C) 2010-2014 Oracle Corporation
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
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QDesktopWidget>
# include <QDir>
# include <QFileInfo>
# include <QPainter>
# include <QTimer>
# include <QDateTime>
# ifdef Q_WS_MAC
#  include <QMenuBar>
# endif /* Q_WS_MAC */

/* GUI includes: */
# include "QIFileDialog.h"
# include "UIActionPoolRuntime.h"
# ifdef VBOX_GUI_WITH_NETWORK_MANAGER
#  include "UINetworkManager.h"
#  include "UIDownloaderAdditions.h"
# endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
# include "UIIconPool.h"
# include "UIKeyboardHandler.h"
# include "UIMouseHandler.h"
# include "UIMachineLogic.h"
# include "UIMachineLogicFullscreen.h"
# include "UIMachineLogicNormal.h"
# include "UIMachineLogicSeamless.h"
# include "UIMachineLogicScale.h"
# include "UIFrameBuffer.h"
# include "UIMachineView.h"
# include "UIMachineWindow.h"
# include "UISession.h"
# include "VBoxGlobal.h"
# include "UIMessageCenter.h"
# include "UIPopupCenter.h"
# include "VBoxTakeSnapshotDlg.h"
# include "UIVMInfoDialog.h"
# include "UISettingsDialogSpecific.h"
# include "UIVMLogViewer.h"
# include "UIConverter.h"
# include "UIModalWindowManager.h"
# include "UIMedium.h"
# include "UIExtraDataManager.h"
# ifdef Q_WS_MAC
#  include "DockIconPreview.h"
#  include "UIExtraDataManager.h"
# endif /* Q_WS_MAC */

/* COM includes: */
# include "CVirtualBoxErrorInfo.h"
# include "CMachineDebugger.h"
# include "CSnapshot.h"
# include "CDisplay.h"
# include "CStorageController.h"
# include "CMediumAttachment.h"
# include "CHostUSBDevice.h"
# include "CUSBDevice.h"
# include "CVRDEServer.h"
# include "CSystemProperties.h"
# include "CHostVideoInputDevice.h"
# include "CEmulatedUSB.h"
# include "CNetworkAdapter.h"
# ifdef Q_WS_MAC
#  include "CGuest.h"
# endif /* Q_WS_MAC */

/* Other VBox includes: */
# include <iprt/path.h>
# ifdef VBOX_WITH_DEBUGGER_GUI
#  include <iprt/ldr.h>
# endif /* VBOX_WITH_DEBUGGER_GUI */

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

#include <QImageWriter>

#ifdef Q_WS_MAC
# include "DarwinKeyboard.h"
#endif
#ifdef Q_WS_WIN
# include "WinKeyboard.h"
#endif

/* External includes: */
#ifdef Q_WS_X11
# include <XKeyboard.h>
# include <QX11Info>
#endif /* Q_WS_X11 */


struct USBTarget
{
    USBTarget() : attach(false), id(QString()) {}
    USBTarget(bool fAttach, const QString &strId)
        : attach(fAttach), id(strId) {}
    bool attach;
    QString id;
};
Q_DECLARE_METATYPE(USBTarget);

/** Describes enumerated webcam item. */
struct WebCamTarget
{
    WebCamTarget() : attach(false), name(QString()), path(QString()) {}
    WebCamTarget(bool fAttach, const QString &strName, const QString &strPath)
        : attach(fAttach), name(strName), path(strPath) {}
    bool attach;
    QString name;
    QString path;
};
Q_DECLARE_METATYPE(WebCamTarget);

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

    /* Prepare other connections: */
    prepareOtherConnections();

    /* Prepare handlers: */
    prepareHandlers();

    /* Prepare menu: */
    prepareMenu();

    /* Prepare machine window(s): */
    prepareMachineWindows();

#ifdef Q_WS_MAC
    /* Prepare dock: */
    prepareDock();
#endif /* Q_WS_MAC */

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* Prepare debugger: */
    prepareDebugger();
#endif /* VBOX_WITH_DEBUGGER_GUI */

    /* Load settings: */
    loadSettings();

    /* Retranslate logic part: */
    retranslateUi();
}

void UIMachineLogic::cleanup()
{
    /* Save settings: */
    saveSettings();

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* Cleanup debugger: */
    cleanupDebugger();
#endif /* VBOX_WITH_DEBUGGER_GUI */

#ifdef Q_WS_MAC
    /* Cleanup dock: */
    cleanupDock();
#endif /* Q_WS_MAC */

    /* Cleanup menu: */
    cleanupMenu();

    /* Cleanup machine window(s): */
    cleanupMachineWindows();

    /* Cleanup handlers: */
    cleanupHandlers();

    /* Cleanup action connections: */
    cleanupActionConnections();
    /* Cleanup action groups: */
    cleanupActionGroups();
}

void UIMachineLogic::initializePostPowerUp()
{
    sltMachineStateChanged();
    sltAdditionsStateChanged();
    sltMouseCapabilityChanged();
}

UIActionPool* UIMachineLogic::actionPool() const
{
    return uisession()->actionPool();
}

CSession& UIMachineLogic::session() const
{
    return uisession()->session();
}

CMachine& UIMachineLogic::machine() const
{
    return uisession()->machine();
}

CConsole& UIMachineLogic::console() const
{
    return uisession()->console();
}

CDisplay& UIMachineLogic::display() const
{
    return uisession()->display();
}

CGuest& UIMachineLogic::guest() const
{
    return uisession()->guest();
}

CMouse& UIMachineLogic::mouse() const
{
    return uisession()->mouse();
}

CKeyboard& UIMachineLogic::keyboard() const
{
    return uisession()->keyboard();
}

CMachineDebugger& UIMachineLogic::debugger() const
{
    return uisession()->debugger();
}

const QString& UIMachineLogic::machineName() const
{
    return uisession()->machineName();
}

UIMachineWindow* UIMachineLogic::mainMachineWindow() const
{
    /* Null if machine-window(s) not yet created: */
    if (!isMachineWindowsCreated())
        return 0;
    /* First machine-window otherwise: */
    return machineWindows().first();
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

void UIMachineLogic::adjustMachineWindowsGeometry()
{
    /* By default, the only thing we need is to
     * adjust machine-view size(s) if necessary: */
    foreach(UIMachineWindow *pMachineWindow, machineWindows())
        pMachineWindow->adjustMachineViewSize();
}

void UIMachineLogic::applyMachineWindowsScaleFactor()
{
    /* By default, the only thing we need is to
     * apply machine-window scale-factor(s) if necessary: */
    foreach(UIMachineWindow *pMachineWindow, machineWindows())
        pMachineWindow->applyMachineWindowScaleFactor();
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

void UIMachineLogic::saveState()
{
    /* Prevent auto-closure: */
    setPreventAutoClose(true);

    /* Was the step successful? */
    bool fSuccess = true;
    /* If VM is not paused, we should pause it: */
    bool fWasPaused = uisession()->isPaused();
    if (fSuccess && !fWasPaused)
        fSuccess = uisession()->pause();
    /* Save-state: */
    if (fSuccess)
        fSuccess = uisession()->saveState();

    /* Allow auto-closure: */
    setPreventAutoClose(false);

    /* Manually close Runtime UI: */
    if (fSuccess)
        uisession()->closeRuntimeUI();
}

void UIMachineLogic::shutdown()
{
    /* Warn the user about ACPI is not available if so: */
    if (!console().GetGuestEnteredACPIMode())
        return popupCenter().cannotSendACPIToMachine(activeMachineWindow());

    /* Shutdown: */
    uisession()->shutdown();
}

void UIMachineLogic::powerOff(bool fDiscardingState)
{
    /* Prevent auto-closure: */
    setPreventAutoClose(true);

    /* Was the step successful? */
    bool fSuccess = true;
    /* Power-off: */
    bool fServerCrashed = false;
    fSuccess = uisession()->powerOff(fDiscardingState, fServerCrashed) || fServerCrashed;

    /* Allow auto-closure: */
    setPreventAutoClose(false);

    /* Manually close Runtime UI: */
    if (fSuccess)
        uisession()->closeRuntimeUI();
}

void UIMachineLogic::notifyAbout3DOverlayVisibilityChange(bool fVisible)
{
    /* If active machine-window is defined now: */
    if (activeMachineWindow())
    {
        /* Reinstall corresponding popup-stack according 3D overlay visibility status: */
        popupCenter().hidePopupStack(activeMachineWindow());
        popupCenter().setPopupStackType(activeMachineWindow(), fVisible ? UIPopupStackType_Separate : UIPopupStackType_Embedded);
        popupCenter().showPopupStack(activeMachineWindow());
    }

    /* Notify other listeners: */
    emit sigNotifyAbout3DOverlayVisibilityChange(fVisible);
}

void UIMachineLogic::sltChangeVisualStateToNormal()
{
    uisession()->setRequestedVisualState(UIVisualStateType_Invalid);
    uisession()->changeVisualState(UIVisualStateType_Normal);
}

void UIMachineLogic::sltChangeVisualStateToFullscreen()
{
    uisession()->setRequestedVisualState(UIVisualStateType_Invalid);
    uisession()->changeVisualState(UIVisualStateType_Fullscreen);
}

void UIMachineLogic::sltChangeVisualStateToSeamless()
{
    uisession()->setRequestedVisualState(UIVisualStateType_Invalid);
    uisession()->changeVisualState(UIVisualStateType_Seamless);
}

void UIMachineLogic::sltChangeVisualStateToScale()
{
    uisession()->setRequestedVisualState(UIVisualStateType_Invalid);
    uisession()->changeVisualState(UIVisualStateType_Scale);
}

void UIMachineLogic::sltMachineStateChanged()
{
    /* Get machine state: */
    KMachineState state = uisession()->machineState();

    /* Update action groups: */
    m_pRunningActions->setEnabled(uisession()->isRunning());
    m_pRunningOrPausedActions->setEnabled(uisession()->isRunning() || uisession()->isPaused());
    m_pRunningOrPausedOrStackedActions->setEnabled(uisession()->isRunning() || uisession()->isPaused() || uisession()->isStuck());

    switch (state)
    {
        case KMachineState_Stuck:
        {
            /* Prevent machine-view from resizing: */
            uisession()->setGuestResizeIgnored(true);
            /* Get log-folder: */
            QString strLogFolder = machine().GetLogFolder();
            /* Take the screenshot for debugging purposes: */
            takeScreenshot(strLogFolder + "/VBox.png", "png");
            /* How should we handle Guru Meditation? */
            switch (uisession()->guruMeditationHandlerType())
            {
                /* Ask how to proceed; Power off VM if proposal accepted: */
                case GuruMeditationHandlerType_Default:
                {
                    if (msgCenter().remindAboutGuruMeditation(QDir::toNativeSeparators(strLogFolder)))
                        powerOff(false /* do NOT restore current snapshot */);
                    break;
                }
                /* Power off VM silently: */
                case GuruMeditationHandlerType_PowerOff:
                {
                    powerOff(false /* do NOT restore current snapshot */);
                    break;
                }
                /* Just ignore it: */
                case GuruMeditationHandlerType_Ignore:
                default:
                    break;
            }
            break;
        }
        case KMachineState_Paused:
        case KMachineState_TeleportingPausedVM:
        {
            QAction *pPauseAction = actionPool()->action(UIActionIndexRT_M_Machine_T_Pause);
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
            QAction *pPauseAction = actionPool()->action(UIActionIndexRT_M_Machine_T_Pause);
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
            /* Is it allowed to close Runtime UI? */
            if (!isPreventAutoClose())
            {
                /* VM has been powered off, saved, teleported or aborted.
                 * We must close Runtime UI: */
                uisession()->closeRuntimeUI();
                return;
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
    actionPool()->action(UIActionIndexRT_M_View_T_GuestAutoresize)->setEnabled(uisession()->isGuestSupportsGraphics());
    actionPool()->action(UIActionIndexRT_M_View_T_Seamless)->setEnabled(uisession()->isVisualStateAllowed(UIVisualStateType_Seamless) &&
                                                                          uisession()->isGuestSupportsSeamless());
}

void UIMachineLogic::sltMouseCapabilityChanged()
{
    /* Variable falgs: */
    bool fIsMouseSupportsAbsolute = uisession()->isMouseSupportsAbsolute();
    bool fIsMouseSupportsRelative = uisession()->isMouseSupportsRelative();
    bool fIsMouseSupportsMultiTouch = uisession()->isMouseSupportsMultiTouch();
    bool fIsMouseHostCursorNeeded = uisession()->isMouseHostCursorNeeded();

    /* For now MT stuff is not important for MI action: */
    Q_UNUSED(fIsMouseSupportsMultiTouch);

    /* Update action state: */
    QAction *pAction = actionPool()->action(UIActionIndexRT_M_Input_M_Mouse_T_Integration);
    pAction->setEnabled(fIsMouseSupportsAbsolute && fIsMouseSupportsRelative && !fIsMouseHostCursorNeeded);
    if (fIsMouseHostCursorNeeded)
        pAction->setChecked(false);
}

void UIMachineLogic::sltHidLedsSyncStateChanged(bool fEnabled)
{
    m_fIsHidLedsSyncEnabled = fEnabled;
}

void UIMachineLogic::sltKeyboardLedsChanged()
{
    /* Here we have to update host LED lock states using values provided by UISession:
     * [bool] uisession() -> isNumLock(), isCapsLock(), isScrollLock() can be used for that. */

    if (!isHidLedsSyncEnabled())
        return;

#if defined(Q_WS_MAC)
    DarwinHidDevicesBroadcastLeds(m_pHostLedsState, uisession()->isNumLock(), uisession()->isCapsLock(), uisession()->isScrollLock());
#elif defined(Q_WS_WIN)
    if (!winHidLedsInSync(uisession()->isNumLock(), uisession()->isCapsLock(), uisession()->isScrollLock()))
    {
        keyboardHandler()->winSkipKeyboardEvents(true);
        WinHidDevicesBroadcastLeds(uisession()->isNumLock(), uisession()->isCapsLock(), uisession()->isScrollLock());
        keyboardHandler()->winSkipKeyboardEvents(false);
    }
    else
        LogRel2(("HID LEDs Sync: already in sync\n"));
#else
    LogRelFlow(("UIMachineLogic::sltKeyboardLedsChanged: Updating host LED lock states does not supported on this platform.\n"));
#endif
}

void UIMachineLogic::sltUSBDeviceStateChange(const CUSBDevice &device, bool fIsAttached, const CVirtualBoxErrorInfo &error)
{
    /* Check if USB device have anything to tell us: */
    if (!error.isNull())
    {
        if (fIsAttached)
            msgCenter().cannotAttachUSBDevice(error, vboxGlobal().details(device), machineName());
        else
            msgCenter().cannotDetachUSBDevice(error, vboxGlobal().details(device), machineName());
    }
}

void UIMachineLogic::sltRuntimeError(bool fIsFatal, const QString &strErrorId, const QString &strMessage)
{
    msgCenter().showRuntimeError(console(), fIsFatal, strErrorId, strMessage);
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

void UIMachineLogic::sltGuestMonitorChange(KGuestMonitorChangedEventType, ulong, QRect)
{
    LogRel(("UIMachineLogic: Guest-screen count changed.\n"));

    /* Make sure all machine-window(s) have proper geometry: */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        pMachineWindow->showInNecessaryMode();
}

void UIMachineLogic::sltHostScreenCountChange()
{
    LogRel(("UIMachineLogic: Host-screen count changed.\n"));

    /* Make sure all machine-window(s) have proper geometry: */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        pMachineWindow->showInNecessaryMode();
}

void UIMachineLogic::sltHostScreenGeometryChange()
{
    LogRel(("UIMachineLogic: Host-screen geometry changed.\n"));

    /* Make sure all machine-window(s) have proper geometry: */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        pMachineWindow->showInNecessaryMode();
}

void UIMachineLogic::sltHostScreenAvailableAreaChange()
{
    LogRel(("UIMachineLogic: Host-screen available-area changed.\n"));

    /* Make sure all machine-window(s) have proper geometry: */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        pMachineWindow->showInNecessaryMode();
}

UIMachineLogic::UIMachineLogic(QObject *pParent, UISession *pSession, UIVisualStateType visualStateType)
    : QIWithRetranslateUI3<QObject>(pParent)
    , m_pSession(pSession)
    , m_visualStateType(visualStateType)
    , m_pKeyboardHandler(0)
    , m_pMouseHandler(0)
    , m_pRunningActions(0)
    , m_pRunningOrPausedActions(0)
    , m_pRunningOrPausedOrStackedActions(0)
    , m_pSharedClipboardActions(0)
    , m_pDragAndDropActions(0)
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
    , m_pHostLedsState(NULL)
    , m_fIsHidLedsSyncEnabled(false)
{
}

void UIMachineLogic::setMachineWindowsCreated(bool fIsWindowsCreated)
{
    /* Make sure something changed: */
    if (m_fIsWindowsCreated == fIsWindowsCreated)
        return;

    /* Special handling for 'destroyed' case: */
    if (!fIsWindowsCreated)
    {
        /* We ask popup-center to hide corresponding popup-stack *before* the remembering new value
         * because we want UIMachineLogic::activeMachineWindow() to be yet alive. */
        popupCenter().hidePopupStack(activeMachineWindow());
    }

    /* Remember new value: */
    m_fIsWindowsCreated = fIsWindowsCreated;

    /* Special handling for 'created' case: */
    if (fIsWindowsCreated)
    {
        /* We ask popup-center to show corresponding popup-stack *after* the remembering new value
         * because we want UIMachineLogic::activeMachineWindow() to be already alive. */
        popupCenter().setPopupStackType(activeMachineWindow(),
                                        visualStateType() == UIVisualStateType_Seamless ?
                                        UIPopupStackType_Separate : UIPopupStackType_Embedded);
        popupCenter().showPopupStack(activeMachineWindow());
    }
}

void UIMachineLogic::addMachineWindow(UIMachineWindow *pMachineWindow)
{
    m_machineWindowsList << pMachineWindow;
}

void UIMachineLogic::setKeyboardHandler(UIKeyboardHandler *pKeyboardHandler)
{
    /* Set new handler: */
    m_pKeyboardHandler = pKeyboardHandler;
    /* Connect to session: */
    connect(m_pKeyboardHandler, SIGNAL(sigStateChange(int)),
            uisession(), SLOT(setKeyboardState(int)));
}

void UIMachineLogic::setMouseHandler(UIMouseHandler *pMouseHandler)
{
    /* Set new handler: */
    m_pMouseHandler = pMouseHandler;
    /* Connect to session: */
    connect(m_pMouseHandler, SIGNAL(sigStateChange(int)),
            uisession(), SLOT(setMouseState(int)));
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
    /* Shared Clipboard actions: */
    if (m_pSharedClipboardActions)
    {
        foreach (QAction *pAction, m_pSharedClipboardActions->actions())
            pAction->setText(gpConverter->toString(pAction->data().value<KClipboardMode>()));
    }
    if (m_pDragAndDropActions)
    {
        foreach (QAction *pAction, m_pDragAndDropActions->actions())
            pAction->setText(gpConverter->toString(pAction->data().value<KDnDMode>()));
    }
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
    connect(uisession(), SIGNAL(sigInitialized()), this, SLOT(sltCheckForRequestedVisualStateType()));
    connect(uisession(), SIGNAL(sigAdditionsStateChange()), this, SLOT(sltCheckForRequestedVisualStateType()));

    /* Machine state-change updater: */
    connect(uisession(), SIGNAL(sigMachineStateChange()), this, SLOT(sltMachineStateChanged()));

    /* Guest additions state-change updater: */
    connect(uisession(), SIGNAL(sigAdditionsStateChange()), this, SLOT(sltAdditionsStateChanged()));

    /* Mouse capability state-change updater: */
    connect(uisession(), SIGNAL(sigMouseCapabilityChange()), this, SLOT(sltMouseCapabilityChanged()));

    /* Keyboard LEDs state-change updater: */
    connect(uisession(), SIGNAL(sigKeyboardLedsChange()), this, SLOT(sltKeyboardLedsChanged()));

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

    /* Guest-monitor-change updater: */
    connect(uisession(), SIGNAL(sigGuestMonitorChange(KGuestMonitorChangedEventType, ulong, QRect)),
            this, SLOT(sltGuestMonitorChange(KGuestMonitorChangedEventType, ulong, QRect)));

    /* Host-screen-change updaters: */
    connect(uisession(), SIGNAL(sigHostScreenCountChange()), this, SLOT(sltHostScreenCountChange()));
    connect(uisession(), SIGNAL(sigHostScreenGeometryChange()), this, SLOT(sltHostScreenGeometryChange()));
    connect(uisession(), SIGNAL(sigHostScreenAvailableAreaChange()), this, SLOT(sltHostScreenAvailableAreaChange()));

    /* Frame-buffer connections: */
    connect(this, SIGNAL(sigFrameBufferResize()), uisession(), SIGNAL(sigFrameBufferResize()));
}

void UIMachineLogic::prepareActionGroups()
{
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

    /* Create group for all actions that are enabled when the VM is running or paused or stucked.
     * Note that only actions whose enabled state depends exclusively on the
     * execution state of the VM are added to this group. */
    m_pRunningOrPausedOrStackedActions = new QActionGroup(this);
    m_pRunningOrPausedOrStackedActions->setExclusive(false);

    /* Move actions into running actions group: */
    m_pRunningActions->addAction(actionPool()->action(UIActionIndexRT_M_Machine_S_Reset));
    m_pRunningActions->addAction(actionPool()->action(UIActionIndexRT_M_Machine_S_Shutdown));
    m_pRunningActions->addAction(actionPool()->action(UIActionIndexRT_M_View_T_Fullscreen));
    m_pRunningActions->addAction(actionPool()->action(UIActionIndexRT_M_View_T_Seamless));
    m_pRunningActions->addAction(actionPool()->action(UIActionIndexRT_M_View_T_Scale));
    m_pRunningActions->addAction(actionPool()->action(UIActionIndexRT_M_View_T_GuestAutoresize));
    m_pRunningActions->addAction(actionPool()->action(UIActionIndexRT_M_View_S_AdjustWindow));
    m_pRunningActions->addAction(actionPool()->action(UIActionIndexRT_M_View_M_ScaleFactor));
    m_pRunningActions->addAction(actionPool()->action(UIActionIndexRT_M_Input_S_TypeCAD));
#ifdef Q_WS_X11
    m_pRunningActions->addAction(actionPool()->action(UIActionIndexRT_M_Input_S_TypeCABS));
#endif /* Q_WS_X11 */

    /* Move actions into running-n-paused actions group: */
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Machine_S_Save));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Machine_S_Settings));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Machine_S_TakeSnapshot));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Machine_S_TakeScreenshot));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Machine_S_ShowInformation));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Machine_T_Pause));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_View_M_MenuBar));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_View_M_MenuBar_S_Settings));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_View_M_MenuBar_T_Visibility));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_View_M_StatusBar));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_View_M_StatusBar_S_Settings));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_View_M_StatusBar_T_Visibility));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Input_M_Keyboard));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Input_M_Keyboard_S_Settings));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Input_M_Mouse));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Input_M_Mouse_T_Integration));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Devices_M_HardDrives));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Devices_M_HardDrives_S_Settings));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Devices_M_OpticalDevices));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Devices_M_FloppyDevices));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Devices_M_Network));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Devices_M_Network_S_Settings));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Devices_M_USBDevices));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Devices_M_USBDevices_S_Settings));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Devices_M_WebCams));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Devices_M_SharedClipboard));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Devices_M_DragAndDrop));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Devices_M_SharedFolders));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Devices_M_SharedFolders_S_Settings));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Devices_T_VRDEServer));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Devices_M_VideoCapture));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Devices_M_VideoCapture_T_Start));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Devices_M_VideoCapture_S_Settings));
    m_pRunningOrPausedActions->addAction(actionPool()->action(UIActionIndexRT_M_Devices_S_InstallGuestTools));

    /* Move actions into running-n-paused-n-stucked actions group: */
    m_pRunningOrPausedOrStackedActions->addAction(actionPool()->action(UIActionIndexRT_M_Machine_S_PowerOff));
}

void UIMachineLogic::prepareActionConnections()
{
    /* 'Machine' actions connections: */
    connect(actionPool()->action(UIActionIndexRT_M_Machine_S_Settings), SIGNAL(triggered()),
            this, SLOT(sltOpenVMSettingsDialog()));
    connect(actionPool()->action(UIActionIndexRT_M_Machine_S_TakeSnapshot), SIGNAL(triggered()),
            this, SLOT(sltTakeSnapshot()));
    connect(actionPool()->action(UIActionIndexRT_M_Machine_S_TakeScreenshot), SIGNAL(triggered()),
            this, SLOT(sltTakeScreenshot()));
    connect(actionPool()->action(UIActionIndexRT_M_Machine_S_ShowInformation), SIGNAL(triggered()),
            this, SLOT(sltShowInformationDialog()));
    connect(actionPool()->action(UIActionIndexRT_M_Machine_T_Pause), SIGNAL(toggled(bool)),
            this, SLOT(sltPause(bool)));
    connect(actionPool()->action(UIActionIndexRT_M_Machine_S_Reset), SIGNAL(triggered()),
            this, SLOT(sltReset()));
    connect(actionPool()->action(UIActionIndexRT_M_Machine_S_Save), SIGNAL(triggered()),
            this, SLOT(sltSaveState()));
    connect(actionPool()->action(UIActionIndexRT_M_Machine_S_Shutdown), SIGNAL(triggered()),
            this, SLOT(sltShutdown()));
    connect(actionPool()->action(UIActionIndexRT_M_Machine_S_PowerOff), SIGNAL(triggered()),
            this, SLOT(sltPowerOff()));
#ifdef RT_OS_DARWIN
    connect(actionPool()->action(UIActionIndex_M_Application_S_Close), SIGNAL(triggered()),
            this, SLOT(sltClose()));
#else /* !RT_OS_DARWIN */
    connect(actionPool()->action(UIActionIndexRT_M_Machine_S_Close), SIGNAL(triggered()),
            this, SLOT(sltClose()));
#endif /* !RT_OS_DARWIN */

    /* 'View' actions connections: */
    connect(actionPool()->action(UIActionIndexRT_M_View_T_GuestAutoresize), SIGNAL(toggled(bool)),
            this, SLOT(sltToggleGuestAutoresize(bool)));
    connect(actionPool()->action(UIActionIndexRT_M_View_S_AdjustWindow), SIGNAL(triggered()),
            this, SLOT(sltAdjustWindow()));

    /* 'Input' actions connections: */
    connect(actionPool()->action(UIActionIndexRT_M_Input_M_Keyboard_S_Settings), SIGNAL(triggered()),
            this, SLOT(sltShowKeyboardSettings()));
    connect(actionPool()->action(UIActionIndexRT_M_Input_M_Mouse_T_Integration), SIGNAL(toggled(bool)),
            this, SLOT(sltToggleMouseIntegration(bool)));
    connect(actionPool()->action(UIActionIndexRT_M_Input_S_TypeCAD), SIGNAL(triggered()),
            this, SLOT(sltTypeCAD()));
#ifdef Q_WS_X11
    connect(actionPool()->action(UIActionIndexRT_M_Input_S_TypeCABS), SIGNAL(triggered()),
            this, SLOT(sltTypeCABS()));
#endif /* Q_WS_X11 */

    /* 'Devices' actions connections: */
    connect(actionPool(), SIGNAL(sigNotifyAboutMenuPrepare(int, QMenu*)), this, SLOT(sltHandleMenuPrepare(int, QMenu*)));
    connect(actionPool()->action(UIActionIndexRT_M_Devices_M_HardDrives_S_Settings), SIGNAL(triggered()),
            this, SLOT(sltOpenStorageSettingsDialog()));
    connect(actionPool()->action(UIActionIndexRT_M_Devices_M_Network_S_Settings), SIGNAL(triggered()),
            this, SLOT(sltOpenNetworkSettingsDialog()));
    connect(actionPool()->action(UIActionIndexRT_M_Devices_M_USBDevices_S_Settings), SIGNAL(triggered()),
            this, SLOT(sltOpenUSBDevicesSettingsDialog()));
    connect(actionPool()->action(UIActionIndexRT_M_Devices_M_SharedFolders_S_Settings), SIGNAL(triggered()),
            this, SLOT(sltOpenSharedFoldersSettingsDialog()));
    connect(actionPool()->action(UIActionIndexRT_M_Devices_T_VRDEServer), SIGNAL(toggled(bool)),
            this, SLOT(sltToggleVRDE(bool)));
    connect(actionPool()->action(UIActionIndexRT_M_Devices_M_VideoCapture_T_Start), SIGNAL(toggled(bool)),
            this, SLOT(sltToggleVideoCapture(bool)));
    connect(actionPool()->action(UIActionIndexRT_M_Devices_M_VideoCapture_S_Settings), SIGNAL(triggered()),
            this, SLOT(sltOpenVideoCaptureOptions()));
    connect(actionPool()->action(UIActionIndexRT_M_Devices_S_InstallGuestTools), SIGNAL(triggered()),
            this, SLOT(sltInstallGuestAdditions()));

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* 'Debug' actions connections: */
    connect(actionPool()->action(UIActionIndexRT_M_Debug_S_ShowStatistics), SIGNAL(triggered()),
            this, SLOT(sltShowDebugStatistics()));
    connect(actionPool()->action(UIActionIndexRT_M_Debug_S_ShowCommandLine), SIGNAL(triggered()),
            this, SLOT(sltShowDebugCommandLine()));
    connect(actionPool()->action(UIActionIndexRT_M_Debug_T_Logging), SIGNAL(toggled(bool)),
            this, SLOT(sltLoggingToggled(bool)));
    connect(actionPool()->action(UIActionIndexRT_M_Debug_S_ShowLogDialog), SIGNAL(triggered()),
            this, SLOT(sltShowLogDialog()));
#endif /* VBOX_WITH_DEBUGGER_GUI */

    /* 'Help' actions connections: */
#ifdef RT_OS_DARWIN
    connect(actionPool()->action(UIActionIndex_M_Application_S_Preferences), SIGNAL(triggered()),
            this, SLOT(sltShowGlobalPreferences()), Qt::UniqueConnection);
#else /* !RT_OS_DARWIN */
    connect(actionPool()->action(UIActionIndex_Simple_Preferences), SIGNAL(triggered()),
            this, SLOT(sltShowGlobalPreferences()), Qt::UniqueConnection);
#endif /* !RT_OS_DARWIN */
}

void UIMachineLogic::prepareHandlers()
{
    /* Prepare menu update-handlers: */
    m_menuUpdateHandlers[UIActionIndexRT_M_Devices_M_OpticalDevices] =  &UIMachineLogic::updateMenuDevicesStorage;
    m_menuUpdateHandlers[UIActionIndexRT_M_Devices_M_FloppyDevices] =   &UIMachineLogic::updateMenuDevicesStorage;
    m_menuUpdateHandlers[UIActionIndexRT_M_Devices_M_Network] =         &UIMachineLogic::updateMenuDevicesNetwork;
    m_menuUpdateHandlers[UIActionIndexRT_M_Devices_M_USBDevices] =      &UIMachineLogic::updateMenuDevicesUSB;
    m_menuUpdateHandlers[UIActionIndexRT_M_Devices_M_WebCams] =         &UIMachineLogic::updateMenuDevicesWebCams;
    m_menuUpdateHandlers[UIActionIndexRT_M_Devices_M_SharedClipboard] = &UIMachineLogic::updateMenuDevicesSharedClipboard;
    m_menuUpdateHandlers[UIActionIndexRT_M_Devices_M_DragAndDrop] =     &UIMachineLogic::updateMenuDevicesDragAndDrop;
#ifdef VBOX_WITH_DEBUGGER_GUI
    m_menuUpdateHandlers[UIActionIndexRT_M_Debug] =                     &UIMachineLogic::updateMenuDebug;
#endif /* VBOX_WITH_DEBUGGER_GUI */

    /* Create keyboard/mouse handlers: */
    setKeyboardHandler(UIKeyboardHandler::create(this, visualStateType()));
    setMouseHandler(UIMouseHandler::create(this, visualStateType()));
    /* Update UI session values with current: */
    uisession()->setKeyboardState(keyboardHandler()->state());
    uisession()->setMouseState(mouseHandler()->state());
}

#ifdef Q_WS_MAC
void UIMachineLogic::prepareDock()
{
    QMenu *pDockMenu = actionPool()->action(UIActionIndexRT_M_Dock)->menu();

    /* Add all the 'Machine' menu entries to the 'Dock' menu: */
    QList<QAction*> actions = actionPool()->action(UIActionIndexRT_M_Machine)->menu()->actions();
    for (int i=0; i < actions.size(); ++i)
    {
        /* Check if we really have correct action: */
        UIAction *pAction = qobject_cast<UIAction*>(actions.at(i));
        /* Skip incorrect actions: */
        if (!pAction)
            continue;
        /* Skip actions which have 'role' (to prevent consuming): */
        if (pAction->menuRole() != QAction::NoRole)
            continue;
        /* Skip actions which have menu (to prevent consuming): */
        if (qobject_cast<UIActionMenu*>(pAction))
            continue;
        pDockMenu->addAction(actions.at(i));
    }
    pDockMenu->addSeparator();

    QMenu *pDockSettingsMenu = actionPool()->action(UIActionIndexRT_M_Dock_M_DockSettings)->menu();
    QActionGroup *pDockPreviewModeGroup = new QActionGroup(this);
    QAction *pDockDisablePreview = actionPool()->action(UIActionIndexRT_M_Dock_M_DockSettings_T_DisableMonitor);
    pDockPreviewModeGroup->addAction(pDockDisablePreview);
    QAction *pDockEnablePreviewMonitor = actionPool()->action(UIActionIndexRT_M_Dock_M_DockSettings_T_PreviewMonitor);
    pDockPreviewModeGroup->addAction(pDockEnablePreviewMonitor);
    pDockSettingsMenu->addActions(pDockPreviewModeGroup->actions());

    connect(pDockPreviewModeGroup, SIGNAL(triggered(QAction*)),
            this, SLOT(sltDockPreviewModeChanged(QAction*)));
    connect(gEDataManager, SIGNAL(sigDockIconAppearanceChange(bool)),
            this, SLOT(sltChangeDockIconUpdate(bool)));

    /* Monitor selection if there are more than one monitor */
    int cGuestScreens = machine().GetMonitorCount();
    if (cGuestScreens > 1)
    {
        pDockSettingsMenu->addSeparator();
        m_DockIconPreviewMonitor = qMin(gEDataManager->realtimeDockIconUpdateMonitor(vboxGlobal().managedVMUuid()),
                                        cGuestScreens - 1);
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
    QString osTypeId = guest().GetOSTypeId();
    m_pDockIconPreview = new UIDockIconPreview(uisession(), vboxGlobal().vmGuestOSTypeIcon(osTypeId));

    /* Should the dock-icon be updated at runtime? */
    bool fEnabled = gEDataManager->realtimeDockIconUpdateEnabled(vboxGlobal().managedVMUuid());
    if (fEnabled)
        pDockEnablePreviewMonitor->setChecked(true);
    else
    {
        pDockDisablePreview->setChecked(true);
        if(m_pDockPreviewSelectMonitorGroup)
            m_pDockPreviewSelectMonitorGroup->setEnabled(false);
    }
    setDockIconPreviewEnabled(fEnabled);
    updateDockOverlay();
}
#endif /* Q_WS_MAC */

#ifdef VBOX_WITH_DEBUGGER_GUI
void UIMachineLogic::prepareDebugger()
{
    if (vboxGlobal().isDebuggerAutoShowEnabled())
    {
        if (vboxGlobal().isDebuggerAutoShowStatisticsEnabled())
            sltShowDebugStatistics();
        if (vboxGlobal().isDebuggerAutoShowCommandLineEnabled())
            sltShowDebugCommandLine();

        if (!vboxGlobal().isStartPausedEnabled())
            sltPause(false);
    }
}
#endif /* VBOX_WITH_DEBUGGER_GUI */

void UIMachineLogic::loadSettings()
{
#if defined(Q_WS_MAC) || defined(Q_WS_WIN)
    /* Read cached extra-data value: */
    m_fIsHidLedsSyncEnabled = gEDataManager->hidLedsSyncState(vboxGlobal().managedVMUuid());
    /* Subscribe to extra-data changes to be able to enable/disable feature dynamically: */
    connect(gEDataManager, SIGNAL(sigHidLedsSyncStateChange(bool)), this, SLOT(sltHidLedsSyncStateChanged(bool)));
#endif /* Q_WS_MAC || Q_WS_WIN */
    /* HID LEDs sync initialization: */
    sltSwitchKeyboardLedsToGuestLeds();
}

void UIMachineLogic::saveSettings()
{
    /* HID LEDs sync deinitialization: */
    sltSwitchKeyboardLedsToPreviousLeds();
}

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

bool UIMachineLogic::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* Handle machine-window events: */
    if (UIMachineWindow *pMachineWindow = qobject_cast<UIMachineWindow*>(pWatched))
    {
        /* Make sure this window still registered: */
        if (isMachineWindowsCreated() && m_machineWindowsList.contains(pMachineWindow))
        {
            switch (pEvent->type())
            {
                /* Handle *window activated* event: */
                case QEvent::WindowActivate:
                {
#ifdef Q_WS_WIN
                    /* We should save current lock states as *previous* and
                     * set current lock states to guest values we have,
                     * As we have no ipc between threads of different VMs
                     * we are using 100ms timer as lazy sync timout: */

                    /* On Windows host we should do that only in case if sync
                     * is enabled. Otherwise, keyboardHandler()->winSkipKeyboardEvents(false)
                     * won't be called in sltSwitchKeyboardLedsToGuestLeds() and guest
                     * will loose keyboard input forever. */
                    if (isHidLedsSyncEnabled())
                    {
                        keyboardHandler()->winSkipKeyboardEvents(true);
                        QTimer::singleShot(100, this, SLOT(sltSwitchKeyboardLedsToGuestLeds()));
                    }
#else /* Q_WS_WIN */
                    /* Trigger callback synchronously for now! */
                    sltSwitchKeyboardLedsToGuestLeds();
#endif /* !Q_WS_WIN */
                    break;
                }
                /* Handle *window deactivated* event: */
                case QEvent::WindowDeactivate:
                {
                    /* We should restore lock states to *previous* known: */
                    sltSwitchKeyboardLedsToPreviousLeds();
                    break;
                }
                /* Default: */
                default: break;
            }
        }
    }
    /* Call to base-class: */
    return QIWithRetranslateUI3<QObject>::eventFilter(pWatched, pEvent);
}

void UIMachineLogic::sltHandleMenuPrepare(int iIndex, QMenu *pMenu)
{
    /* Update if there is update-handler: */
    if (m_menuUpdateHandlers.contains(iIndex))
        (this->*(m_menuUpdateHandlers.value(iIndex)))(pMenu);
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

        /* Normalize window geometry: */
        pMachineWindow->normalizeGeometry(true /* adjust position */);
    }
}

void UIMachineLogic::sltShowKeyboardSettings()
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    /* Open Global Preferences: Input page: */
    showGlobalPreferences("#input", "m_pMachineTable");
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
    keyboard().PutCAD();
    AssertWrapperOk(keyboard());
}

#ifdef Q_WS_X11
void UIMachineLogic::sltTypeCABS()
{
    static QVector<LONG> sequence(6);
    sequence[0] = 0x1d; /* Ctrl down */
    sequence[1] = 0x38; /* Alt down */
    sequence[2] = 0x0E; /* Backspace down */
    sequence[3] = 0x8E; /* Backspace up */
    sequence[4] = 0xb8; /* Alt up */
    sequence[5] = 0x9d; /* Ctrl up */
    keyboard().PutScancodes(sequence);
    AssertWrapperOk(keyboard());
}
#endif /* Q_WS_X11 */

void UIMachineLogic::sltTakeSnapshot()
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    /* Remember the paused state: */
    bool fWasPaused = uisession()->isPaused();
    if (!fWasPaused)
    {
        /* Suspend the VM and ignore the close event if failed to do so.
         * pause() will show the error message to the user. */
        if (!uisession()->pause())
            return;
    }

    /* Create take-snapshot dialog: */
    QWidget *pDlgParent = windowManager().realParentWindow(activeMachineWindow());
    QPointer<VBoxTakeSnapshotDlg> pDlg = new VBoxTakeSnapshotDlg(pDlgParent, machine());
    windowManager().registerNewParent(pDlg, pDlgParent);

    /* Assign corresponding icon: */
    QString strTypeId = machine().GetOSTypeId();
    pDlg->mLbIcon->setPixmap(vboxGlobal().vmGuestOSTypeIcon(strTypeId));

    /* Search for the max available filter index: */
    QString strNameTemplate = QApplication::translate("UIMachineLogic", "Snapshot %1");
    int iMaxSnapshotIndex = searchMaxSnapshotIndex(machine(), machine().FindSnapshot(QString()), strNameTemplate);
    pDlg->mLeName->setText(strNameTemplate.arg(++ iMaxSnapshotIndex));

    /* Exec the dialog: */
    bool fDialogAccepted = pDlg->exec() == QDialog::Accepted;

    /* Make sure dialog still valid: */
    if (!pDlg)
        return;

    /* Acquire variables: */
    QString strSnapshotName = pDlg->mLeName->text().trimmed();
    QString strSnapshotDescription = pDlg->mTeDescription->toPlainText();

    /* Destroy dialog early: */
    delete pDlg;

    /* Was the dialog accepted? */
    if (fDialogAccepted)
    {
        /* Prepare the take-snapshot progress: */
        CProgress progress = console().TakeSnapshot(strSnapshotName, strSnapshotDescription);
        if (console().isOk())
        {
            /* Show the take-snapshot progress: */
            msgCenter().showModalProgressDialog(progress, machineName(), ":/progress_snapshot_create_90px.png");
            if (!progress.isOk() || progress.GetResultCode() != 0)
                msgCenter().cannotTakeSnapshot(progress, machineName());
        }
        else
            msgCenter().cannotTakeSnapshot(console(), machineName());
    }

    /* Restore the running state if needed: */
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

#ifdef Q_WS_WIN
    /* Due to Qt bug, modal QFileDialog appeared above the active machine-window
     * does not retreive the focus from the currently focused machine-view,
     * as the result guest keyboard remains captured, so we should
     * clear the focus from this machine-view initially: */
    if (activeMachineWindow())
        activeMachineWindow()->machineView()->clearFocus();
#endif /* Q_WS_WIN */

    /* Request the filename from the user. */
    QFileInfo fi(machine().GetSettingsFilePath());
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

#ifdef Q_WS_WIN
    /* Due to Qt bug, modal QFileDialog appeared above the active machine-window
     * does not retreive the focus from the currently focused machine-view,
     * as the result guest keyboard remains captured, so we already
     * cleared the focus from this machine-view and should return
     * that focus finally: */
    if (activeMachineWindow())
        activeMachineWindow()->machineView()->setFocus();
#endif /* Q_WS_WIN */

    /* Do the screenshot. */
    if (!strFilename.isEmpty())
        takeScreenshot(strFilename, strFilter.split(" ").value(0, "png"));
}

void UIMachineLogic::sltShowInformationDialog()
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    /* Invoke VM information dialog: */
    UIVMInfoDialog::invoke(mainMachineWindow());
}

void UIMachineLogic::sltReset()
{
    /* Confirm/Reset current console: */
    if (msgCenter().confirmResetMachine(machineName()))
        console().Reset();

    /* TODO_NEW_CORE: On reset the additional screens didn't get a display
       update. Emulate this for now until it get fixed. */
    ulong uMonitorCount = machine().GetMonitorCount();
    for (ulong uScreenId = 1; uScreenId < uMonitorCount; ++uScreenId)
        machineWindows().at(uScreenId)->update();
}

void UIMachineLogic::sltPause(bool fOn)
{
    uisession()->setPause(fOn);
}

void UIMachineLogic::sltSaveState()
{
    /* Make sure machine is in one of the allowed states: */
    if (!uisession()->isRunning() && !uisession()->isPaused())
    {
        AssertMsgFailed(("Invalid machine-state. Action should be prohibited!"));
        return;
    }

    saveState();
}

void UIMachineLogic::sltShutdown()
{
    /* Make sure machine is in one of the allowed states: */
    if (!uisession()->isRunning())
    {
        AssertMsgFailed(("Invalid machine-state. Action should be prohibited!"));
        return;
    }

    shutdown();
}

void UIMachineLogic::sltPowerOff()
{
    /* Make sure machine is in one of the allowed states: */
    if (!uisession()->isRunning() && !uisession()->isPaused() && !uisession()->isStuck())
    {
        AssertMsgFailed(("Invalid machine-state. Action should be prohibited!"));
        return;
    }

    powerOff(machine().GetSnapshotCount() > 0);
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

void UIMachineLogic::sltOpenVMSettingsDialog(const QString &strCategory /* = QString() */,
                                             const QString &strControl /* = QString()*/)
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    /* Create VM settings window on the heap!
     * Its necessary to allow QObject hierarchy cleanup to delete this dialog if necessary: */
    QPointer<UISettingsDialogMachine> pDialog = new UISettingsDialogMachine(activeMachineWindow(),
                                                                            machine().GetId(),
                                                                            strCategory, strControl);
    /* Executing VM settings window.
     * This blocking function calls for the internal event-loop to process all further events,
     * including event which can delete the dialog itself. */
    pDialog->execute();
    /* Delete dialog if its still valid: */
    if (pDialog)
        delete pDialog;

    /* We can't rely on MediumChange events as they are not yet properly implemented within Main.
     * We can't watch for MachineData change events as well as they are of broadcast type
     * and console event-handler do not processing broadcast events.
     * But we still want to be updated after possible medium changes at least if they were
     * originated from our side. */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        pMachineWindow->updateAppearanceOf(UIVisualElement_HDStuff | UIVisualElement_CDStuff | UIVisualElement_FDStuff);
}

void UIMachineLogic::sltOpenStorageSettingsDialog()
{
    /* Machine settings: Storage page: */
    sltOpenVMSettingsDialog("#storage");
}

void UIMachineLogic::sltOpenNetworkSettingsDialog()
{
    /* Open VM settings : Network page: */
    sltOpenVMSettingsDialog("#network");
}

void UIMachineLogic::sltOpenUSBDevicesSettingsDialog()
{
    /* Machine settings: Storage page: */
    sltOpenVMSettingsDialog("#usb");
}

void UIMachineLogic::sltOpenSharedFoldersSettingsDialog()
{
    /* Do not process if additions are not loaded! */
    if (!uisession()->isGuestAdditionsActive())
        msgCenter().remindAboutGuestAdditionsAreNotActive();

    /* Open VM settings : Shared folders page: */
    sltOpenVMSettingsDialog("#sharedFolders");
}

void UIMachineLogic::sltMountStorageMedium()
{
    /* Sender action: */
    QAction *pAction = qobject_cast<QAction*>(sender());
    AssertMsgReturnVoid(pAction, ("This slot should only be called by menu action!\n"));

    /* Current mount-target: */
    const UIMediumTarget target = pAction->data().value<UIMediumTarget>();

    /* Update current machine mount-target: */
    vboxGlobal().updateMachineStorage(machine(), target);
}

void UIMachineLogic::sltAttachUSBDevice()
{
    /* Get and check sender action object: */
    QAction *pAction = qobject_cast<QAction*>(sender());
    AssertMsg(pAction, ("This slot should only be called on selecting USB menu item!\n"));

    /* Get operation target: */
    USBTarget target = pAction->data().value<USBTarget>();

    /* Attach USB device: */
    if (target.attach)
    {
        /* Try to attach corresponding device: */
        console().AttachUSBDevice(target.id, QString(""));
        /* Check if console is OK: */
        if (!console().isOk())
        {
            /* Get current host: */
            CHost host = vboxGlobal().host();
            /* Search the host for the corresponding USB device: */
            CHostUSBDevice hostDevice = host.FindUSBDeviceById(target.id);
            /* Get USB device from host USB device: */
            CUSBDevice device(hostDevice);
            /* Show a message about procedure failure: */
            msgCenter().cannotAttachUSBDevice(console(), vboxGlobal().details(device));
        }
    }
    /* Detach USB device: */
    else
    {
        /* Search the console for the corresponding USB device: */
        CUSBDevice device = console().FindUSBDeviceById(target.id);
        /* Try to detach corresponding device: */
        console().DetachUSBDevice(target.id);
        /* Check if console is OK: */
        if (!console().isOk())
        {
            /* Show a message about procedure failure: */
            msgCenter().cannotDetachUSBDevice(console(), vboxGlobal().details(device));
        }
    }
}

void UIMachineLogic::sltAttachWebCamDevice()
{
    /* Get and check sender action object: */
    QAction *pAction = qobject_cast<QAction*>(sender());
    AssertReturnVoid(pAction);

    /* Get operation target: */
    WebCamTarget target = pAction->data().value<WebCamTarget>();

    /* Get current emulated USB: */
    CEmulatedUSB dispatcher = console().GetEmulatedUSB();

    /* Attach webcam device: */
    if (target.attach)
    {
        /* Try to attach corresponding device: */
        dispatcher.WebcamAttach(target.path, "");
        /* Check if dispatcher is OK: */
        if (!dispatcher.isOk())
            msgCenter().cannotAttachWebCam(dispatcher, target.name, machineName());
    }
    /* Detach webcam device: */
    else
    {
        /* Try to detach corresponding device: */
        dispatcher.WebcamDetach(target.path);
        /* Check if dispatcher is OK: */
        if (!dispatcher.isOk())
            msgCenter().cannotDetachWebCam(dispatcher, target.name, machineName());
    }
}

void UIMachineLogic::sltChangeSharedClipboardType(QAction *pAction)
{
    /* Assign new mode (without save): */
    KClipboardMode mode = pAction->data().value<KClipboardMode>();
    machine().SetClipboardMode(mode);
}

/** Toggles network adapter's <i>Cable Connected</i> state. */
void UIMachineLogic::sltToggleNetworkAdapterConnection()
{
    /* Get and check 'the sender' action object: */
    QAction *pAction = qobject_cast<QAction*>(sender());
    AssertReturnVoid(pAction);

    /* Get operation target: */
    CNetworkAdapter adapter = machine().GetNetworkAdapter((ULONG)pAction->property("slot").toInt());
    AssertReturnVoid(machine().isOk() && !adapter.isNull());

    /* Connect/disconnect cable to/from target: */
    adapter.SetCableConnected(!adapter.GetCableConnected());
    machine().SaveSettings();
    if (!machine().isOk())
        msgCenter().cannotSaveMachineSettings(machine());
}

void UIMachineLogic::sltChangeDragAndDropType(QAction *pAction)
{
    /* Assign new mode (without save): */
    KDnDMode mode = pAction->data().value<KDnDMode>();
    machine().SetDnDMode(mode);
}

void UIMachineLogic::sltToggleVRDE(bool fEnabled)
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    /* Access VRDE server: */
    CVRDEServer server = machine().GetVRDEServer();
    AssertMsg(!server.isNull(), ("VRDE server should NOT be null!\n"));
    if (!machine().isOk() || server.isNull())
        return;

    /* Make sure something had changed: */
    if (server.GetEnabled() == static_cast<BOOL>(fEnabled))
        return;

    /* Server is OK? */
    if (server.isOk())
    {
        /* Update VRDE server state: */
        server.SetEnabled(fEnabled);
        /* Server still OK? */
        if (server.isOk())
        {
            /* Save machine-settings: */
            machine().SaveSettings();
            /* Machine still OK? */
            if (!machine().isOk())
            {
                /* Notify about the error: */
                msgCenter().cannotSaveMachineSettings(machine());
                /* Make sure action is updated! */
                uisession()->updateStatusVRDE();
            }
        }
        else
        {
            /* Notify about the error: */
            msgCenter().cannotToggleVRDEServer(server, machineName(), fEnabled);
            /* Make sure action is updated! */
            uisession()->updateStatusVRDE();
        }
    }
}

void UIMachineLogic::sltToggleVideoCapture(bool fEnabled)
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    /* Make sure something had changed: */
    if (machine().GetVideoCaptureEnabled() == static_cast<BOOL>(fEnabled))
        return;

    /* Update Video Capture state: */
    AssertMsg(machine().isOk(), ("Machine should be OK!\n"));
    machine().SetVideoCaptureEnabled(fEnabled);
    /* Machine is not OK? */
    if (!machine().isOk())
    {
        /* Notify about the error: */
        msgCenter().cannotToggleVideoCapture(machine(), fEnabled);
        /* Make sure action is updated! */
        uisession()->updateStatusVideoCapture();
    }
    /* Machine is OK? */
    else
    {
        /* Save machine-settings: */
        machine().SaveSettings();
        /* Machine is not OK? */
        if (!machine().isOk())
        {
            /* Notify about the error: */
            msgCenter().cannotSaveMachineSettings(machine());
            /* Make sure action is updated! */
            uisession()->updateStatusVideoCapture();
        }
    }
}

void UIMachineLogic::sltOpenVideoCaptureOptions()
{
    /* Open VM settings : Display page : Video Capture tab: */
    sltOpenVMSettingsDialog("#display", "m_pCheckboxVideoCapture");
}

void UIMachineLogic::sltInstallGuestAdditions()
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    CSystemProperties systemProperties = vboxGlobal().virtualBox().GetSystemProperties();
    QString strAdditions = systemProperties.GetDefaultAdditionsISO();
    if (systemProperties.isOk() && !strAdditions.isEmpty())
        return uisession()->sltInstallGuestAdditionsFrom(strAdditions);

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

#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    /* If downloader is running already: */
    if (UIDownloaderAdditions::current())
    {
        /* Just show network access manager: */
        gNetworkManager->show();
    }
    /* Else propose to download additions: */
    else if (msgCenter().cannotFindGuestAdditions())
    {
        /* Create Additions downloader: */
        UIDownloaderAdditions *pDl = UIDownloaderAdditions::create();
        /* After downloading finished => propose to install the Additions: */
        connect(pDl, SIGNAL(sigDownloadFinished(const QString&)), uisession(), SLOT(sltInstallGuestAdditionsFrom(const QString&)));
        /* Start downloading: */
        pDl->start();
    }
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
}

#ifdef VBOX_WITH_DEBUGGER_GUI

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
    if (!debugger().isNull() && debugger().isOk())
        debugger().SetLogEnabled(fState);
}

void UIMachineLogic::sltShowLogDialog()
{
    /* Show VM Log Viewer: */
    UIVMLogViewer::showLogViewerFor(activeMachineWindow(), machine());
}

#endif /* VBOX_WITH_DEBUGGER_GUI */

#ifdef Q_WS_MAC
void UIMachineLogic::sltDockPreviewModeChanged(QAction *pAction)
{
    bool fEnabled = pAction != actionPool()->action(UIActionIndexRT_M_Dock_M_DockSettings_T_DisableMonitor);
    gEDataManager->setRealtimeDockIconUpdateEnabled(fEnabled, vboxGlobal().managedVMUuid());
    updateDockOverlay();
}

void UIMachineLogic::sltDockPreviewMonitorChanged(QAction *pAction)
{
    gEDataManager->setRealtimeDockIconUpdateMonitor(pAction->data().toInt(), vboxGlobal().managedVMUuid());
    updateDockOverlay();
}

void UIMachineLogic::sltChangeDockIconUpdate(bool fEnabled)
{
    if (isMachineWindowsCreated())
    {
        setDockIconPreviewEnabled(fEnabled);
        if (m_pDockPreviewSelectMonitorGroup)
        {
            m_pDockPreviewSelectMonitorGroup->setEnabled(fEnabled);
            m_DockIconPreviewMonitor = qMin(gEDataManager->realtimeDockIconUpdateMonitor(vboxGlobal().managedVMUuid()),
                                            (int)machine().GetMonitorCount() - 1);
        }
        /* Resize the dock icon in the case the preview monitor has changed. */
        QSize size = machineWindows().at(m_DockIconPreviewMonitor)->machineView()->size();
        updateDockIconSize(m_DockIconPreviewMonitor, size.width(), size.height());
        updateDockOverlay();
    }
}
#endif /* Q_WS_MAC */

void UIMachineLogic::sltSwitchKeyboardLedsToGuestLeds()
{
//    /* Log statement (printf): */
//    QString strDt = QDateTime::currentDateTime().toString("HH:mm:ss:zzz");
//    printf("%s: UIMachineLogic: sltSwitchKeyboardLedsToGuestLeds called, machine name is {%s}\n",
//           strDt.toAscii().constData(),
//           machineName().toAscii().constData());

    /* Here we have to store host LED lock states. */

    /* Here we have to update host LED lock states using values provided by UISession registry.
     * [bool] uisession() -> isNumLock(), isCapsLock(), isScrollLock() can be used for that. */

    if (!isHidLedsSyncEnabled())
        return;

#if defined(Q_WS_MAC)
    if (m_pHostLedsState == NULL)
        m_pHostLedsState = DarwinHidDevicesKeepLedsState();
    DarwinHidDevicesBroadcastLeds(m_pHostLedsState, uisession()->isNumLock(), uisession()->isCapsLock(), uisession()->isScrollLock());
#elif defined(Q_WS_WIN)
    if (m_pHostLedsState == NULL)
        m_pHostLedsState = WinHidDevicesKeepLedsState();
    keyboardHandler()->winSkipKeyboardEvents(true);
    WinHidDevicesBroadcastLeds(uisession()->isNumLock(), uisession()->isCapsLock(), uisession()->isScrollLock());
    keyboardHandler()->winSkipKeyboardEvents(false);
#else
    LogRelFlow(("UIMachineLogic::sltSwitchKeyboardLedsToGuestLeds: keep host LED lock states and broadcast guest's ones does not supported on this platform.\n"));
#endif
}

void UIMachineLogic::sltSwitchKeyboardLedsToPreviousLeds()
{
//    /* Log statement (printf): */
//    QString strDt = QDateTime::currentDateTime().toString("HH:mm:ss:zzz");
//    printf("%s: UIMachineLogic: sltSwitchKeyboardLedsToPreviousLeds called, machine name is {%s}\n",
//           strDt.toAscii().constData(),
//           machineName().toAscii().constData());

    if (!isHidLedsSyncEnabled())
        return;

    /* Here we have to restore host LED lock states. */
    if (m_pHostLedsState)
    {
#if defined(Q_WS_MAC)
        DarwinHidDevicesApplyAndReleaseLedsState(m_pHostLedsState);
#elif defined(Q_WS_WIN)
        keyboardHandler()->winSkipKeyboardEvents(true);
        WinHidDevicesApplyAndReleaseLedsState(m_pHostLedsState);
        keyboardHandler()->winSkipKeyboardEvents(false);
#else
        LogRelFlow(("UIMachineLogic::sltSwitchKeyboardLedsToPreviousLeds: restore host LED lock states does not supported on this platform.\n"));
#endif
        m_pHostLedsState = NULL;
    }
}

void UIMachineLogic::sltShowGlobalPreferences()
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    /* Just show Global Preferences: */
    showGlobalPreferences();
}

void UIMachineLogic::updateMenuDevicesStorage(QMenu *pMenu)
{
    /* Clear contents: */
    pMenu->clear();

    /* Determine device-type: */
    const QMenu *pOpticalDevicesMenu = actionPool()->action(UIActionIndexRT_M_Devices_M_OpticalDevices)->menu();
    const QMenu *pFloppyDevicesMenu = actionPool()->action(UIActionIndexRT_M_Devices_M_FloppyDevices)->menu();
    const KDeviceType deviceType = pMenu == pOpticalDevicesMenu ? KDeviceType_DVD :
                                   pMenu == pFloppyDevicesMenu  ? KDeviceType_Floppy :
                                                                  KDeviceType_Null;
    AssertMsgReturnVoid(deviceType != KDeviceType_Null, ("Incorrect storage device-type!\n"));

    /* Prepare/fill all storage menus: */
    foreach (const CMediumAttachment &attachment, machine().GetMediumAttachments())
    {
        /* Current controller: */
        const CStorageController controller = machine().GetStorageControllerByName(attachment.GetController());
        /* If controller present and device-type correct: */
        if (!controller.isNull() && attachment.GetType() == deviceType)
        {
            /* Current controller/attachment attributes: */
            const QString strControllerName = controller.GetName();
            const StorageSlot storageSlot(controller.GetBus(), attachment.GetPort(), attachment.GetDevice());

            /* Prepare current storage menu: */
            QMenu *pStorageMenu = 0;
            /* If it will be more than one storage menu: */
            if (pMenu->menuAction()->data().toInt() > 1)
            {
                /* We have to create sub-menu for each of them: */
                pStorageMenu = new QMenu(QString("%1 (%2)").arg(strControllerName).arg(gpConverter->toString(storageSlot)), pMenu);
                switch (controller.GetBus())
                {
                    case KStorageBus_IDE:    pStorageMenu->setIcon(QIcon(":/ide_16px.png")); break;
                    case KStorageBus_SATA:   pStorageMenu->setIcon(QIcon(":/sata_16px.png")); break;
                    case KStorageBus_SCSI:   pStorageMenu->setIcon(QIcon(":/scsi_16px.png")); break;
                    case KStorageBus_Floppy: pStorageMenu->setIcon(QIcon(":/floppy_16px.png")); break;
                    case KStorageBus_SAS:    pStorageMenu->setIcon(QIcon(":/sata_16px.png")); break;
                    case KStorageBus_USB:    pStorageMenu->setIcon(QIcon(":/usb_16px.png")); break;
                    default: break;
                }
                pMenu->addMenu(pStorageMenu);
            }
            /* Otherwise just use existing one: */
            else pStorageMenu = pMenu;

            /* Fill current storage menu: */
            vboxGlobal().prepareStorageMenu(*pStorageMenu,
                                            this, SLOT(sltMountStorageMedium()),
                                            machine(), strControllerName, storageSlot);
        }
    }
}

void UIMachineLogic::updateMenuDevicesNetwork(QMenu *pMenu)
{
    /* Determine how many adapters we should display: */
    const KChipsetType chipsetType = machine().GetChipsetType();
    const ULONG uCount = qMin((ULONG)4, vboxGlobal().virtualBox().GetSystemProperties().GetMaxNetworkAdapters(chipsetType));

    /* Enumerate existing network adapters: */
    QMap<int, bool> adapterData;
    for (ULONG uSlot = 0; uSlot < uCount; ++uSlot)
    {
        /* Get and check iterated adapter: */
        const CNetworkAdapter adapter = machine().GetNetworkAdapter(uSlot);
        AssertReturnVoid(machine().isOk() && !adapter.isNull());

        /* Skip disabled adapters: */
        if (!adapter.GetEnabled())
            continue;

        /* Remember adapter data: */
        adapterData.insert((int)uSlot, (bool)adapter.GetCableConnected());
    }

    /* Make sure at least one adapter was enabled: */
    if (adapterData.isEmpty())
        return;

    /* Add new actions: */
    foreach (int iSlot, adapterData.keys())
    {
        QAction *pAction = pMenu->addAction(UIIconPool::iconSet(adapterData[iSlot] ? ":/connect_16px.png": ":/disconnect_16px.png"),
                                            adapterData.size() == 1 ? tr("Connect Network Adapter") : tr("Connect Network Adapter %1").arg(iSlot + 1),
                                            this, SLOT(sltToggleNetworkAdapterConnection()));
        pAction->setProperty("slot", iSlot);
        pAction->setCheckable(true);
        pAction->setChecked(adapterData[iSlot]);
    }
}

void UIMachineLogic::updateMenuDevicesUSB(QMenu *pMenu)
{
    /* Get current host: */
    const CHost host = vboxGlobal().host();
    /* Get host USB device list: */
    const CHostUSBDeviceVector devices = host.GetUSBDevices();

    /* If device list is empty: */
    if (devices.isEmpty())
    {
        /* Add only one - "empty" action: */
        QAction *pEmptyMenuAction = pMenu->addAction(UIIconPool::iconSet(":/usb_unavailable_16px.png",
                                                                         ":/usb_unavailable_disabled_16px.png"),
                                                     tr("No USB Devices Connected"));
        pEmptyMenuAction->setToolTip(tr("No supported devices connected to the host PC"));
        pEmptyMenuAction->setEnabled(false);
    }
    /* If device list is NOT empty: */
    else
    {
        /* Populate menu with host USB devices: */
        foreach (const CHostUSBDevice& hostDevice, devices)
        {
            /* Get USB device from current host USB device: */
            const CUSBDevice device(hostDevice);

            /* Create USB device action: */
            QAction *pAttachUSBAction = pMenu->addAction(vboxGlobal().details(device),
                                                         this, SLOT(sltAttachUSBDevice()));
            pAttachUSBAction->setToolTip(vboxGlobal().toolTip(device));
            pAttachUSBAction->setCheckable(true);

            /* Check if that USB device was already attached to this session: */
            const CUSBDevice attachedDevice = console().FindUSBDeviceById(device.GetId());
            pAttachUSBAction->setChecked(!attachedDevice.isNull());
            pAttachUSBAction->setEnabled(hostDevice.GetState() != KUSBDeviceState_Unavailable);

            /* Set USB attach data: */
            pAttachUSBAction->setData(QVariant::fromValue(USBTarget(!pAttachUSBAction->isChecked(), device.GetId())));
        }
    }
}

void UIMachineLogic::updateMenuDevicesWebCams(QMenu *pMenu)
{
    /* Clear contents: */
    pMenu->clear();

    /* Get current host: */
    const CHost host = vboxGlobal().host();
    /* Get host webcam list: */
    const CHostVideoInputDeviceVector webcams = host.GetVideoInputDevices();

    /* If webcam list is empty: */
    if (webcams.isEmpty())
    {
        /* Add only one - "empty" action: */
        QAction *pEmptyMenuAction = pMenu->addAction(UIIconPool::iconSet(":/web_camera_unavailable_16px.png",
                                                                         ":/web_camera_unavailable_disabled_16px.png"),
                                                     tr("No Webcams Connected"));
        pEmptyMenuAction->setToolTip(tr("No supported webcams connected to the host PC"));
        pEmptyMenuAction->setEnabled(false);
    }
    /* If webcam list is NOT empty: */
    else
    {
        /* Populate menu with host webcams: */
        const QVector<QString> attachedWebcamPaths = console().GetEmulatedUSB().GetWebcams();
        foreach (const CHostVideoInputDevice &webcam, webcams)
        {
            /* Get webcam data: */
            const QString strWebcamName = webcam.GetName();
            const QString strWebcamPath = webcam.GetPath();

            /* Create/configure webcam action: */
            QAction *pAttachWebcamAction = pMenu->addAction(strWebcamName,
                                                            this, SLOT(sltAttachWebCamDevice()));
            pAttachWebcamAction->setToolTip(vboxGlobal().toolTip(webcam));
            pAttachWebcamAction->setCheckable(true);

            /* Check if that webcam was already attached to this session: */
            pAttachWebcamAction->setChecked(attachedWebcamPaths.contains(strWebcamPath));

            /* Set USB attach data: */
            pAttachWebcamAction->setData(QVariant::fromValue(WebCamTarget(!pAttachWebcamAction->isChecked(), strWebcamName, strWebcamPath)));
        }
    }
}

void UIMachineLogic::updateMenuDevicesSharedClipboard(QMenu *pMenu)
{
    /* First run: */
    if (!m_pSharedClipboardActions)
    {
        m_pSharedClipboardActions = new QActionGroup(this);
        for (int i = KClipboardMode_Disabled; i < KClipboardMode_Max; ++i)
        {
            KClipboardMode mode = (KClipboardMode)i;
            QAction *pAction = new QAction(gpConverter->toString(mode), m_pSharedClipboardActions);
            pMenu->addAction(pAction);
            pAction->setData(QVariant::fromValue(mode));
            pAction->setCheckable(true);
            pAction->setChecked(machine().GetClipboardMode() == mode);
        }
        connect(m_pSharedClipboardActions, SIGNAL(triggered(QAction*)),
                this, SLOT(sltChangeSharedClipboardType(QAction*)));
    }
    /* Subsequent runs: */
    else
        foreach (QAction *pAction, m_pSharedClipboardActions->actions())
            if (pAction->data().value<KClipboardMode>() == machine().GetClipboardMode())
                pAction->setChecked(true);
}

void UIMachineLogic::updateMenuDevicesDragAndDrop(QMenu *pMenu)
{
    /* First run: */
    if (!m_pDragAndDropActions)
    {
        m_pDragAndDropActions = new QActionGroup(this);
        for (int i = KDnDMode_Disabled; i < KDnDMode_Max; ++i)
        {
            KDnDMode mode = (KDnDMode)i;
            QAction *pAction = new QAction(gpConverter->toString(mode), m_pDragAndDropActions);
            pMenu->addAction(pAction);
            pAction->setData(QVariant::fromValue(mode));
            pAction->setCheckable(true);
            pAction->setChecked(machine().GetDnDMode() == mode);
        }
        connect(m_pDragAndDropActions, SIGNAL(triggered(QAction*)),
                this, SLOT(sltChangeDragAndDropType(QAction*)));
    }
    /* Subsequent runs: */
    else
        foreach (QAction *pAction, m_pDragAndDropActions->actions())
            if (pAction->data().value<KDnDMode>() == machine().GetDnDMode())
                pAction->setChecked(true);
}

#ifdef VBOX_WITH_DEBUGGER_GUI
void UIMachineLogic::updateMenuDebug(QMenu*)
{
    /* The "Logging" item. */
    bool fEnabled = false;
    bool fChecked = false;
    if (!debugger().isNull() && debugger().isOk())
    {
        fEnabled = true;
        fChecked = debugger().GetLogEnabled() != FALSE;
    }
    if (fEnabled != actionPool()->action(UIActionIndexRT_M_Debug_T_Logging)->isEnabled())
        actionPool()->action(UIActionIndexRT_M_Debug_T_Logging)->setEnabled(fEnabled);
    if (fChecked != actionPool()->action(UIActionIndexRT_M_Debug_T_Logging)->isChecked())
        actionPool()->action(UIActionIndexRT_M_Debug_T_Logging)->setChecked(fChecked);
}
#endif /* VBOX_WITH_DEBUGGER_GUI */

void UIMachineLogic::showGlobalPreferences(const QString &strCategory /* = QString() */, const QString &strControl /* = QString() */)
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

#ifdef RT_OS_DARWIN
    /* Check that we do NOT handling that already: */
    if (actionPool()->action(UIActionIndex_M_Application_S_Preferences)->data().toBool())
        return;
    /* Remember that we handling that already: */
    actionPool()->action(UIActionIndex_M_Application_S_Preferences)->setData(true);
#else /* !RT_OS_DARWIN */
    /* Check that we do NOT handling that already: */
    if (actionPool()->action(UIActionIndex_Simple_Preferences)->data().toBool())
        return;
    /* Remember that we handling that already: */
    actionPool()->action(UIActionIndex_Simple_Preferences)->setData(true);
#endif /* !RT_OS_DARWIN */

    /* Create and execute global settings window: */
    QPointer<UISettingsDialogGlobal> pDialog = new UISettingsDialogGlobal(activeMachineWindow(),
                                                                          strCategory, strControl);
    pDialog->execute();
    if (pDialog)
        delete pDialog;

#ifdef RT_OS_DARWIN
    /* Remember that we do NOT handling that already: */
    actionPool()->action(UIActionIndex_M_Application_S_Preferences)->setData(false);
#else /* !RT_OS_DARWIN */
    /* Remember that we do NOT handling that already: */
    actionPool()->action(UIActionIndex_Simple_Preferences)->setData(false);
#endif /* !RT_OS_DARWIN */
}

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
    const int cGuestScreens = machine().GetMonitorCount();
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
        LONG xOrigin = 0;
        LONG yOrigin = 0;
        KGuestMonitorStatus monitorStatus = KGuestMonitorStatus_Enabled;
        display().GetScreenResolution(i, width, height, bpp, xOrigin, yOrigin, monitorStatus);
        uMaxWidth  += width;
        uMaxHeight  = RT_MAX(uMaxHeight, height);
        QImage shot = QImage(width, height, QImage::Format_RGB32);
        display().TakeScreenShot(i, shot.bits(), shot.width(), shot.height(), KBitmapFormat_BGR0);
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

    /* Save the big image in the requested format: */
    const QFileInfo fi(strFile);
    const QString &strPathWithoutSuffix = QDir(fi.absolutePath()).absoluteFilePath(fi.baseName());
    const QString &strSuffix = fi.suffix().isEmpty() ? strFormat : fi.suffix();
    bigImg.save(QDir::toNativeSeparators(QFile::encodeName(QString("%1.%2").arg(strPathWithoutSuffix, strSuffix))),
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
                m_pDbgGuiVT->pfnSetMenu(m_pDbgGui, actionPool()->action(UIActionIndexRT_M_Debug));
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

