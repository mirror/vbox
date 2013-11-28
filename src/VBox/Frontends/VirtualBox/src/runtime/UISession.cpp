/* $Id$ */
/** @file
 * VBox Qt GUI - UISession class implementation.
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QApplication>
#include <QDesktopWidget>
#include <QWidget>
#ifdef Q_WS_MAC
# include <QTimer>
#endif /* Q_WS_MAC */

/* GUI includes: */
#include "VBoxGlobal.h"
#include "UISession.h"
#include "UIMachine.h"
#include "UIMedium.h"
#include "UIActionPoolRuntime.h"
#include "UIMachineLogic.h"
#include "UIMachineView.h"
#include "UIMachineWindow.h"
#include "UIMachineMenuBar.h"
#include "UIMessageCenter.h"
#include "UIPopupCenter.h"
#include "UIWizardFirstRun.h"
#include "UIConsoleEventHandler.h"
#include "UIFrameBuffer.h"
#ifdef VBOX_WITH_VIDEOHWACCEL
# include "VBoxFBOverlay.h"
#endif /* VBOX_WITH_VIDEOHWACCEL */
#ifdef Q_WS_MAC
# include "VBoxUtils-darwin.h"
#endif /* Q_WS_MAC */

#ifdef Q_WS_X11
# include <QX11Info>
# include <X11/Xlib.h>
# include <X11/Xutil.h>
# ifndef VBOX_WITHOUT_XCURSOR
#  include <X11/Xcursor/Xcursor.h>
# endif /* VBOX_WITHOUT_XCURSOR */
#endif /* Q_WS_X11 */

#ifdef VBOX_GUI_WITH_KEYS_RESET_HANDLER
# include "UIKeyboardHandler.h"
# include <signal.h>
#endif /* VBOX_GUI_WITH_KEYS_RESET_HANDLER */

/* COM includes: */
#include "CConsole.h"
#include "CSystemProperties.h"
#include "CMachineDebugger.h"
#include "CGuest.h"
#include "CStorageController.h"
#include "CMediumAttachment.h"
#include "CDisplay.h"
#include "CFramebuffer.h"
#include "CNetworkAdapter.h"
#include "CHostNetworkInterface.h"
#include "CVRDEServer.h"
#include "CUSBController.h"
#include "CUSBDeviceFilters.h"
#include "CHostVideoInputDevice.h"
#include "CSnapshot.h"
#include "CMedium.h"

#ifdef VBOX_GUI_WITH_KEYS_RESET_HANDLER
static void signalHandlerSIGUSR1(int sig, siginfo_t *, void *);
#endif

#ifdef Q_WS_MAC
/**
 * MacOS X: Application Services: Core Graphics: Display reconfiguration callback.
 *
 * Notifies UISession about @a display configuration change.
 * Corresponding change described by Core Graphics @a flags.
 * Uses UISession @a pHandler to process this change.
 *
 * @note Last argument (@a pHandler) must always be valid pointer to UISession object.
 * @note Calls for UISession::sltHandleHostDisplayAboutToChange() slot if display configuration changed.
 */
void cgDisplayReconfigurationCallback(CGDirectDisplayID display, CGDisplayChangeSummaryFlags flags, void *pHandler)
{
    /* Which flags we are handling? */
    int iHandledFlags = kCGDisplayAddFlag     /* display added */
                      | kCGDisplayRemoveFlag  /* display removed */
                      | kCGDisplaySetModeFlag /* display mode changed */;

    /* Handle 'display-add' case: */
    if (flags & kCGDisplayAddFlag)
        LogRelFlow(("UISession::cgDisplayReconfigurationCallback: Display added.\n"));
    /* Handle 'display-remove' case: */
    else if (flags & kCGDisplayRemoveFlag)
        LogRelFlow(("UISession::cgDisplayReconfigurationCallback: Display removed.\n"));
    /* Handle 'mode-set' case: */
    else if (flags & kCGDisplaySetModeFlag)
        LogRelFlow(("UISession::cgDisplayReconfigurationCallback: Display mode changed.\n"));

    /* Ask handler to process our callback: */
    if (flags & iHandledFlags)
        QTimer::singleShot(0, static_cast<UISession*>(pHandler),
                           SLOT(sltHandleHostDisplayAboutToChange()));

    Q_UNUSED(display);
}
#endif /* Q_WS_MAC */

UISession::UISession(UIMachine *pMachine, CSession &sessionReference)
    : QObject(pMachine)
    /* Base variables: */
    , m_pMachine(pMachine)
    , m_session(sessionReference)
    /* Common variables: */
    , m_pMenuPool(0)
    , m_machineStatePrevious(KMachineState_Null)
    , m_machineState(session().GetMachine().GetState())
    , m_requestedVisualStateType(UIVisualStateType_Invalid)
#ifdef Q_WS_WIN
    , m_alphaCursor(0)
#endif /* Q_WS_WIN */
#ifdef Q_WS_MAC
    , m_pWatchdogDisplayChange(0)
#endif /* Q_WS_MAC */
    , m_defaultCloseAction(MachineCloseAction_Invalid)
    , m_restrictedCloseActions(MachineCloseAction_Invalid)
    , m_fAllCloseActionsRestricted(false)
    , m_fSnapshotOperationsAllowed(true)
    /* Common flags: */
    , m_fIsFirstTimeStarted(false)
    , m_fIsIgnoreRuntimeMediumsChanging(false)
    , m_fIsGuestResizeIgnored(false)
    , m_fIsAutoCaptureDisabled(false)
    , m_fReconfigurable(false)
    /* Guest additions flags: */
    , m_ulGuestAdditionsRunLevel(0)
    , m_fIsGuestSupportsGraphics(false)
    , m_fIsGuestSupportsSeamless(false)
    /* Mouse flags: */
    , m_fNumLock(false)
    , m_fCapsLock(false)
    , m_fScrollLock(false)
    , m_uNumLockAdaptionCnt(2)
    , m_uCapsLockAdaptionCnt(2)
    /* Mouse flags: */
    , m_fIsMouseSupportsAbsolute(false)
    , m_fIsMouseSupportsRelative(false)
    , m_fIsMouseSupportsMultiTouch(false)
    , m_fIsMouseHostCursorNeeded(false)
    , m_fIsMouseCaptured(false)
    , m_fIsMouseIntegrated(true)
    , m_fIsValidPointerShapePresent(false)
    , m_fIsHidingHostPointer(true)
{
    /* Prepare connections: */
    prepareConnections();

    /* Prepare console event-handlers: */
    prepareConsoleEventHandlers();

    /* Prepare screens: */
    prepareScreens();

    /* Prepare framebuffers: */
    prepareFramebuffers();

    /* Prepare main-menu: */
    prepareMenuPool();

    /* Load settings: */
    loadSessionSettings();

#ifdef VBOX_GUI_WITH_KEYS_RESET_HANDLER
    struct sigaction sa;
    sa.sa_sigaction = &signalHandlerSIGUSR1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sigaction(SIGUSR1, &sa, NULL);
#endif /* VBOX_GUI_WITH_KEYS_RESET_HANDLER */
}

UISession::~UISession()
{
    /* Save settings: */
    saveSessionSettings();

    /* Cleanup main-menu: */
    cleanupMenuPool();

    /* Cleanup framebuffers: */
    cleanupFramebuffers();

    /* Cleanup console event-handlers: */
    cleanupConsoleEventHandlers();

#ifdef Q_WS_WIN
    /* Destroy alpha cursor: */
    if (m_alphaCursor)
        DestroyIcon(m_alphaCursor);
#endif /* Q_WS_WIN */
}

void UISession::powerUp()
{
    /* Do nothing if we had started already: */
    if (isRunning() || isPaused())
        return;

    /* Prepare powerup: */
    bool fPrepared = preparePowerUp();
    if (!fPrepared)
        return;

    /* Get current machine/console: */
    CMachine machine = session().GetMachine();
    CConsole console = session().GetConsole();

    /* Apply debug settings from the command line. */
    CMachineDebugger debugger = console.GetDebugger();
    if (debugger.isOk())
    {
        if (vboxGlobal().isPatmDisabled())
            debugger.SetPATMEnabled(false);
        if (vboxGlobal().isCsamDisabled())
            debugger.SetCSAMEnabled(false);
        if (vboxGlobal().isSupervisorCodeExecedRecompiled())
            debugger.SetRecompileSupervisor(true);
        if (vboxGlobal().isUserCodeExecedRecompiled())
            debugger.SetRecompileUser(true);
        if (vboxGlobal().areWeToExecuteAllInIem())
            debugger.SetExecuteAllInIEM(true);
        if (!vboxGlobal().isDefaultWarpPct())
            debugger.SetVirtualTimeRate(vboxGlobal().getWarpPct());
    }

    /* Power UP machine: */
    CProgress progress = vboxGlobal().isStartPausedEnabled() || vboxGlobal().isDebuggerAutoShowEnabled(machine) ?
                         console.PowerUpPaused() : console.PowerUp();

    /* Check for immediate failure: */
    if (!console.isOk())
    {
        if (vboxGlobal().showStartVMErrors())
            msgCenter().cannotStartMachine(console, machine.GetName());
        closeRuntimeUI();
        return;
    }

    /* Guard progressbar warnings from auto-closing: */
    if (uimachine()->machineLogic())
        uimachine()->machineLogic()->setPreventAutoClose(true);

    /* Show "Starting/Restoring" progress dialog: */
    if (isSaved())
    {
        msgCenter().showModalProgressDialog(progress, machine.GetName(), ":/progress_state_restore_90px.png", 0, 0);
        /* After restoring from 'saved' state, guest screen size should be adjusted: */
        machineLogic()->maybeAdjustGuestScreenSize();
    }
    else
        msgCenter().showModalProgressDialog(progress, machine.GetName(), ":/progress_start_90px.png");

    /* Check for a progress failure: */
    if (!progress.isOk() || progress.GetResultCode() != 0)
    {
        if (vboxGlobal().showStartVMErrors())
            msgCenter().cannotStartMachine(progress, machine.GetName());
        closeRuntimeUI();
        return;
    }

    /* Allow further auto-closing: */
    if (uimachine()->machineLogic())
        uimachine()->machineLogic()->setPreventAutoClose(false);

    /* Check if we missed a really quick termination after successful startup, and process it if we did: */
    if (isTurnedOff())
    {
        closeRuntimeUI();
        return;
    }

    /* Check if the required virtualization features are active. We get this
     * info only when the session is active. */
    bool fIs64BitsGuest = vboxGlobal().virtualBox().GetGuestOSType(console.GetGuest().GetOSTypeId()).GetIs64Bit();
    bool fRecommendVirtEx = vboxGlobal().virtualBox().GetGuestOSType(console.GetGuest().GetOSTypeId()).GetRecommendedVirtEx();
    AssertMsg(!fIs64BitsGuest || fRecommendVirtEx, ("Virtualization support missed for 64bit guest!\n"));
    bool fIsVirtEnabled = console.GetDebugger().GetHWVirtExEnabled();
    if (fRecommendVirtEx && !fIsVirtEnabled)
    {
        bool fShouldWeClose;

        bool fVTxAMDVSupported = vboxGlobal().host().GetProcessorFeature(KProcessorFeature_HWVirtEx);

        QApplication::processEvents();
        setPause(true);

        if (fIs64BitsGuest)
            fShouldWeClose = msgCenter().warnAboutVirtNotEnabled64BitsGuest(fVTxAMDVSupported);
        else
            fShouldWeClose = msgCenter().warnAboutVirtNotEnabledGuestRequired(fVTxAMDVSupported);

        if (fShouldWeClose)
        {
            /* At this point the console is powered up. So we have to close
             * this session again. */
            CProgress progress = console.PowerDown();
            if (console.isOk())
            {
                /* Guard progressbar warnings from auto-closing: */
                if (uimachine()->machineLogic())
                    uimachine()->machineLogic()->setPreventAutoClose(true);
                /* Show the power down progress dialog */
                msgCenter().showModalProgressDialog(progress, machine.GetName(), ":/progress_poweroff_90px.png");
                if (!progress.isOk() || progress.GetResultCode() != 0)
                    msgCenter().cannotPowerDownMachine(progress, machine.GetName());
                /* Allow further auto-closing: */
                if (uimachine()->machineLogic())
                    uimachine()->machineLogic()->setPreventAutoClose(false);
            }
            else
                msgCenter().cannotPowerDownMachine(console);
            closeRuntimeUI();
            return;
        }

        setPause(false);
    }

#ifdef VBOX_WITH_VIDEOHWACCEL
    LogRel(("2D video acceleration is %s.\n",
           machine.GetAccelerate2DVideoEnabled() && VBoxGlobal::isAcceleration2DVideoAvailable()
                 ? "enabled"
                 : "disabled"));
#endif

#ifdef VBOX_GUI_WITH_PIDFILE
    vboxGlobal().createPidfile();
#endif

    /* Warn listeners about machine was started: */
    emit sigMachineStarted();
}

bool UISession::saveState()
{
    /* Prepare the saving progress: */
    CMachine machine = m_session.GetMachine();
    CConsole console = m_session.GetConsole();
    CProgress progress = console.SaveState();
    if (console.isOk())
    {
        /* Show the saving progress: */
        msgCenter().showModalProgressDialog(progress, machine.GetName(), ":/progress_state_save_90px.png");
        if (!progress.isOk() || progress.GetResultCode() != 0)
        {
            /* Failed in progress: */
            msgCenter().cannotSaveMachineState(progress, machine.GetName());
            return false;
        }
    }
    else
    {
        /* Failed in console: */
        msgCenter().cannotSaveMachineState(console);
        return false;
    }
    /* Passed: */
    return true;
}

bool UISession::shutdown()
{
    /* Send ACPI shutdown signal if possible: */
    CConsole console = m_session.GetConsole();
    console.PowerButton();
    if (!console.isOk())
    {
        /* Failed in console: */
        msgCenter().cannotACPIShutdownMachine(console);
        return false;
    }
    /* Passed: */
    return true;
}

bool UISession::powerOff(bool fIncludingDiscard, bool &fServerCrashed)
{
    /* Prepare the power-off progress: */
    CMachine machine = m_session.GetMachine();
    CConsole console = m_session.GetConsole();
    CProgress progress = console.PowerDown();
    if (console.isOk())
    {
        /* Show the power-off progress: */
        msgCenter().showModalProgressDialog(progress, machine.GetName(), ":/progress_poweroff_90px.png");
        if (progress.isOk() && progress.GetResultCode() == 0)
        {
            /* Discard the current state if requested: */
            if (fIncludingDiscard)
            {
                /* Prepare the snapshot-discard progress: */
                CSnapshot snapshot = machine.GetCurrentSnapshot();
                CProgress progress = console.RestoreSnapshot(snapshot);
                if (!console.isOk())
                    return msgCenter().cannotRestoreSnapshot(console, snapshot.GetName(), machine.GetName());

                /* Show the snapshot-discard progress: */
                msgCenter().showModalProgressDialog(progress, machine.GetName(), ":/progress_snapshot_discard_90px.png");
                if (progress.GetResultCode() != 0)
                    return msgCenter().cannotRestoreSnapshot(progress, snapshot.GetName(), machine.GetName());
            }
        }
        else
        {
            /* Failed in progress: */
            msgCenter().cannotPowerDownMachine(progress, machine.GetName());
            return false;
        }
    }
    else
    {
        /* Failed in console: */
        COMResult res(console);
        /* This can happen if VBoxSVC is not running: */
        if (FAILED_DEAD_INTERFACE(res.rc()))
            fServerCrashed = true;
        else
            msgCenter().cannotPowerDownMachine(console);
        return false;
    }
    /* Passed: */
    return true;
}

void UISession::closeRuntimeUI()
{
    /* Start corresponding slot asynchronously: */
    emit sigCloseRuntimeUI();
}

UIMachineLogic* UISession::machineLogic() const
{
    return uimachine()->machineLogic();
}

QWidget* UISession::mainMachineWindow() const
{
    return machineLogic()->mainMachineWindow();
}

QMenu* UISession::newMenu(RuntimeMenuType fOptions /* = RuntimeMenuType_ALL */)
{
    /* Create new menu: */
    QMenu *pMenu = m_pMenuPool->createMenu(fOptions);

    /* Re-init menu pool for the case menu were recreated: */
    reinitMenuPool();

    /* Return newly created menu: */
    return pMenu;
}

QMenuBar* UISession::newMenuBar(RuntimeMenuType fOptions /* = RuntimeMenuType_ALL */)
{
    /* Create new menubar: */
    QMenuBar *pMenuBar = m_pMenuPool->createMenuBar(fOptions);

    /* Re-init menu pool for the case menu were recreated: */
    reinitMenuPool();

    /* Return newly created menubar: */
    return pMenuBar;
}

bool UISession::isVisualStateAllowedFullscreen() const
{
    return m_pMachine->isVisualStateAllowedFullscreen();
}

bool UISession::isVisualStateAllowedSeamless() const
{
    return m_pMachine->isVisualStateAllowedSeamless();
}

bool UISession::isVisualStateAllowedScale() const
{
    return m_pMachine->isVisualStateAllowedScale();
}

void UISession::changeVisualState(UIVisualStateType visualStateType)
{
    m_pMachine->asyncChangeVisualState(visualStateType);
}

bool UISession::setPause(bool fOn)
{
    CConsole console = session().GetConsole();

    if (fOn)
        console.Pause();
    else
        console.Resume();

    bool ok = console.isOk();
    if (!ok)
    {
        if (fOn)
            msgCenter().cannotPauseMachine(console);
        else
            msgCenter().cannotResumeMachine(console);
    }

    return ok;
}

void UISession::sltInstallGuestAdditionsFrom(const QString &strSource)
{
    CMachine machine = session().GetMachine();
    CVirtualBox vbox = vboxGlobal().virtualBox();

    /*
     * Flag indicating whether we want to do the usual .ISO mounting or not.
     * First try updating the Guest Additions directly without mounting the .ISO.
     */
    bool fDoMount = false;
    /* Auto-update in GUI currently is disabled. */
#ifndef VBOX_WITH_ADDITIONS_AUTOUPDATE_UI
    fDoMount = true;
#else
    CGuest guest = session().GetConsole().GetGuest();
    QVector<KAdditionsUpdateFlag> aFlagsUpdate;
    QVector<QString> aArgs;
    CProgress progressInstall = guest.UpdateGuestAdditions(strSource,
                                                           aArgs, aFlagsUpdate);
    bool fResult = guest.isOk();
    if (fResult)
    {
        msgCenter().showModalProgressDialog(progressInstall, tr("Updating Guest Additions"),
                                            ":/progress_install_guest_additions_90px.png",
                                            0, 500 /* 500ms delay. */);
        if (progressInstall.GetCanceled())
            return;

        HRESULT rc = progressInstall.GetResultCode();
        if (!progressInstall.isOk() || rc != S_OK)
        {
            /* If we got back a VBOX_E_NOT_SUPPORTED we don't complain (guest OS
             * simply isn't supported yet), so silently fall back to "old" .ISO
             * mounting method. */
            if (   !SUCCEEDED_WARNING(rc)
                && rc != VBOX_E_NOT_SUPPORTED)
            {
                msgCenter().cannotUpdateGuestAdditions(progressInstall);

                /* Log the error message in the release log. */
                QString strErr = progressInstall.GetErrorInfo().GetText();
                if (!strErr.isEmpty())
                    LogRel(("%s\n", strErr.toLatin1().constData()));
            }
            fDoMount = true; /* Since automatic updating failed, fall back to .ISO mounting. */
        }
    }
#endif /* VBOX_WITH_ADDITIONS_AUTOUPDATE_UI */

    if (fDoMount) /* Fallback to only mounting the .ISO file. */
    {
        QString strUuid;
        CMedium image = vbox.OpenMedium(strSource, KDeviceType_DVD, KAccessMode_ReadWrite, false /* fForceNewUuid */);
        if (image.isNull())
        {
            image = vbox.OpenMedium(strSource, KDeviceType_DVD, KAccessMode_ReadWrite, false /* fForceNewUuid */);
            if (vbox.isOk())
                strUuid = image.GetId();
        }
        else
            strUuid = image.GetId();

        if (!vbox.isOk())
        {
            msgCenter().cannotOpenMedium(vbox, UIMediumType_DVD, strSource, mainMachineWindow());
            return;
        }

        AssertMsg(!strUuid.isNull(), ("Guest Additions image UUID should be valid!\n"));

        QString strCntName;
        LONG iCntPort = -1, iCntDevice = -1;
        /* Searching for the first suitable slot */
        {
            CStorageControllerVector controllers = machine.GetStorageControllers();
            int i = 0;
            while (i < controllers.size() && strCntName.isNull())
            {
                CStorageController controller = controllers[i];
                CMediumAttachmentVector attachments = machine.GetMediumAttachmentsOfController(controller.GetName());
                int j = 0;
                while (j < attachments.size() && strCntName.isNull())
                {
                    CMediumAttachment attachment = attachments[j];
                    if (attachment.GetType() == KDeviceType_DVD)
                    {
                        strCntName = controller.GetName();
                        iCntPort = attachment.GetPort();
                        iCntDevice = attachment.GetDevice();
                    }
                    ++ j;
                }
                ++ i;
            }
        }

        if (!strCntName.isNull())
        {
            /* Create new UIMedium: */
            UIMedium medium(image, UIMediumType_DVD, KMediumState_Created);

            /* Inform VBoxGlobal about it: */
            vboxGlobal().createMedium(medium);

            /* Mount medium to the predefined port/device: */
            machine.MountMedium(strCntName, iCntPort, iCntDevice, medium.medium(), false /* force */);
            if (!machine.isOk())
            {
                /* Ask for force mounting: */
                if (msgCenter().cannotRemountMedium(machine, medium, true /* mount? */,
                                                    true /* retry? */, mainMachineWindow()))
                {
                    /* Force mount medium to the predefined port/device: */
                    machine.MountMedium(strCntName, iCntPort, iCntDevice, medium.medium(), true /* force */);
                    if (!machine.isOk())
                        msgCenter().cannotRemountMedium(machine, medium, true /* mount? */,
                                                        false /* retry? */, mainMachineWindow());
                }
            }
        }
        else
            msgCenter().cannotMountGuestAdditions(machine.GetName());
    }
}

void UISession::sltChangeVisualStateToNormal()
{
    /* Reset requests: */
    setRequestedVisualState(UIVisualStateType_Invalid);
    /* Request change: */
    m_pMachine->asyncChangeVisualState(UIVisualStateType_Normal);
}

void UISession::sltChangeVisualStateToFullscreen()
{
    /* Reset requests: */
    setRequestedVisualState(UIVisualStateType_Invalid);
    /* Request change: */
    m_pMachine->asyncChangeVisualState(UIVisualStateType_Fullscreen);
}

void UISession::sltChangeVisualStateToSeamless()
{
    /* Reset requests: */
    setRequestedVisualState(UIVisualStateType_Invalid);
    /* Request change: */
    m_pMachine->asyncChangeVisualState(UIVisualStateType_Seamless);
}

void UISession::sltChangeVisualStateToScale()
{
    /* Reset requests: */
    setRequestedVisualState(UIVisualStateType_Invalid);
    /* Request change: */
    m_pMachine->asyncChangeVisualState(UIVisualStateType_Scale);
}

void UISession::sltCloseRuntimeUI()
{
    /* First, we have to hide any opened modal/popup widgets.
     * They then should unlock their event-loops synchronously.
     * If all such loops are unlocked, we can close Runtime UI: */
    if (QWidget *pWidget = QApplication::activeModalWidget() ?
                           QApplication::activeModalWidget() :
                           QApplication::activePopupWidget() ?
                           QApplication::activePopupWidget() : 0)
    {
        /* First we should try to close this widget: */
        pWidget->close();
        /* If widget rejected the 'close-event' we can
         * still hide it and hope it will behave correctly
         * and unlock his event-loop if any: */
        if (!pWidget->isHidden())
            pWidget->hide();
        /* Restart this slot asynchronously: */
        emit sigCloseRuntimeUI();
        return;
    }

    /* Finally close the Runtime UI: */
    m_pMachine->deleteLater();
}

void UISession::sltMousePointerShapeChange(bool fVisible, bool fAlpha, QPoint hotCorner, QSize size, QVector<uint8_t> shape)
{
    /* In case of shape data is present: */
    if (shape.size() > 0)
    {
        /* We are ignoring visibility flag: */
        m_fIsHidingHostPointer = false;

        /* And updating current cursor shape: */
        setPointerShape(shape.data(), fAlpha,
                        hotCorner.x(), hotCorner.y(),
                        size.width(), size.height());
    }
    /* In case of shape data is NOT present: */
    else
    {
        /* Remember if we should hide the cursor: */
        m_fIsHidingHostPointer = !fVisible;
    }

    /* Notify listeners about mouse capability changed: */
    emit sigMousePointerShapeChange();

}

void UISession::sltMouseCapabilityChange(bool fSupportsAbsolute, bool fSupportsRelative, bool fSupportsMultiTouch, bool fNeedsHostCursor)
{
    LogRelFlow(("UISession::sltMouseCapabilityChange: "
                "Supports absolute: %s, Supports relative: %s, "
                "Supports multi-touch: %s, Needs host cursor: %s\n",
                fSupportsAbsolute ? "TRUE" : "FALSE", fSupportsRelative ? "TRUE" : "FALSE",
                fSupportsMultiTouch ? "TRUE" : "FALSE", fNeedsHostCursor ? "TRUE" : "FALSE"));

    /* Check if something had changed: */
    if (   m_fIsMouseSupportsAbsolute != fSupportsAbsolute
        || m_fIsMouseSupportsRelative != fSupportsRelative
        || m_fIsMouseSupportsMultiTouch != fSupportsMultiTouch
        || m_fIsMouseHostCursorNeeded != fNeedsHostCursor)
    {
        /* Store new data: */
        m_fIsMouseSupportsAbsolute = fSupportsAbsolute;
        m_fIsMouseSupportsRelative = fSupportsRelative;
        m_fIsMouseSupportsMultiTouch = fSupportsMultiTouch;
        m_fIsMouseHostCursorNeeded = fNeedsHostCursor;

        /* Notify listeners about mouse capability changed: */
        emit sigMouseCapabilityChange();
    }
}

void UISession::sltKeyboardLedsChangeEvent(bool fNumLock, bool fCapsLock, bool fScrollLock)
{
    /* Check if something had changed: */
    if (   m_fNumLock != fNumLock
        || m_fCapsLock != fCapsLock
        || m_fScrollLock != fScrollLock)
    {
        /* Store new num lock data: */
        if (m_fNumLock != fNumLock)
        {
            m_fNumLock = fNumLock;
            m_uNumLockAdaptionCnt = 2;
        }

        /* Store new caps lock data: */
        if (m_fCapsLock != fCapsLock)
        {
            m_fCapsLock = fCapsLock;
            m_uCapsLockAdaptionCnt = 2;
        }

        /* Store new scroll lock data: */
        if (m_fScrollLock != fScrollLock)
        {
            m_fScrollLock = fScrollLock;
        }

        /* Notify listeners about mouse capability changed: */
        emit sigKeyboardLedsChange();
    }
}

void UISession::sltStateChange(KMachineState state)
{
    /* Check if something had changed: */
    if (m_machineState != state)
    {
        /* Store new data: */
        m_machineStatePrevious = m_machineState;
        m_machineState = state;

        /* Update session settings: */
        updateSessionSettings();

        /* Notify listeners about machine state changed: */
        emit sigMachineStateChange();
    }
}

void UISession::sltVRDEChange()
{
    /* Get machine: */
    const CMachine machine = session().GetMachine();
    /* Get VRDE server: */
    const CVRDEServer &server = machine.GetVRDEServer();
    bool fIsVRDEServerAvailable = !server.isNull();
    /* Show/Hide VRDE action depending on VRDE server availability status: */
    gActionPool->action(UIActionIndexRuntime_Toggle_VRDEServer)->setVisible(fIsVRDEServerAvailable);
    /* Check/Uncheck VRDE action depending on VRDE server activity status: */
    if (fIsVRDEServerAvailable)
        gActionPool->action(UIActionIndexRuntime_Toggle_VRDEServer)->setChecked(server.GetEnabled());
    /* Notify listeners about VRDE change: */
    emit sigVRDEChange();
}

void UISession::sltVideoCaptureChange()
{
    /* Get machine: */
    const CMachine machine = session().GetMachine();
    /* Check/Uncheck Video Capture action depending on feature status: */
    gActionPool->action(UIActionIndexRuntime_Toggle_VideoCapture)->setChecked(machine.GetVideoCaptureEnabled());
    /* Notify listeners about Video Capture change: */
    emit sigVideoCaptureChange();
}

void UISession::sltGuestMonitorChange(KGuestMonitorChangedEventType changeType, ulong uScreenId, QRect screenGeo)
{
    /* Ignore KGuestMonitorChangedEventType_NewOrigin change event: */
    if (changeType == KGuestMonitorChangedEventType_NewOrigin)
        return;
    /* Ignore KGuestMonitorChangedEventType_Disabled event if there is only one window visible: */
    AssertMsg(countOfVisibleWindows() > 0, ("All machine windows are hidden!"));
    if (   changeType == KGuestMonitorChangedEventType_Disabled
        && countOfVisibleWindows() == 1
        && isScreenVisible(uScreenId))
        return;

    /* Process KGuestMonitorChangedEventType_Enabled change event: */
    if (   !isScreenVisible(uScreenId)
        && changeType == KGuestMonitorChangedEventType_Enabled)
        setScreenVisible(uScreenId, true);
    /* Process KGuestMonitorChangedEventType_Disabled change event: */
    else if (   isScreenVisible(uScreenId)
             && changeType == KGuestMonitorChangedEventType_Disabled)
        setScreenVisible(uScreenId, false);

    /* Notify listeners about the change: */
    emit sigGuestMonitorChange(changeType, uScreenId, screenGeo);
}

#ifdef RT_OS_DARWIN
/**
 * MacOS X: Restarts display-reconfiguration watchdog timer from the beginning.
 * @note Watchdog is trying to determine display reconfiguration in
 *       UISession::sltCheckIfHostDisplayChanged() slot every 500ms for 40 tries.
 */
void UISession::sltHandleHostDisplayAboutToChange()
{
    LogRelFlow(("UISession::sltHandleHostDisplayAboutToChange()\n"));

    if (m_pWatchdogDisplayChange->isActive())
        m_pWatchdogDisplayChange->stop();
    m_pWatchdogDisplayChange->setProperty("tryNumber", 1);
    m_pWatchdogDisplayChange->start();
}

/**
 * MacOS X: Determines display reconfiguration.
 * @note Calls for UISession::sltHandleHostScreenCountChange() if screen count changed.
 * @note Calls for UISession::sltHandleHostScreenGeometryChange() if screen geometry changed.
 */
void UISession::sltCheckIfHostDisplayChanged()
{
    LogRelFlow(("UISession::sltCheckIfHostDisplayChanged()\n"));

    /* Acquire desktop wrapper: */
    QDesktopWidget *pDesktop = QApplication::desktop();

    /* Check if display count changed: */
    if (pDesktop->screenCount() != m_screens.size())
    {
        /* Recache display data: */
        recacheDisplayData();
        /* Reset watchdog: */
        m_pWatchdogDisplayChange->setProperty("tryNumber", 0);
        /* Notify listeners about screen-count changed: */
        return sltHandleHostScreenCountChange();
    }
    else
    {
        /* Check if at least one display geometry changed: */
        for (int iScreenIndex = 0; iScreenIndex < pDesktop->screenCount(); ++iScreenIndex)
        {
            if (pDesktop->screenGeometry(iScreenIndex) != m_screens.at(iScreenIndex))
            {
                /* Recache display data: */
                recacheDisplayData();
                /* Reset watchdog: */
                m_pWatchdogDisplayChange->setProperty("tryNumber", 0);
                /* Notify listeners about screen-geometry changed: */
                return sltHandleHostScreenGeometryChange();
            }
        }
    }

    /* Check if watchdog expired, restart if not: */
    int cTryNumber = m_pWatchdogDisplayChange->property("tryNumber").toInt();
    if (cTryNumber > 0 && cTryNumber < 40)
    {
        /* Restart watchdog again: */
        m_pWatchdogDisplayChange->setProperty("tryNumber", ++cTryNumber);
        m_pWatchdogDisplayChange->start();
    }
    else
    {
        /* Reset watchdog: */
        m_pWatchdogDisplayChange->setProperty("tryNumber", 0);
    }
}
#endif /* RT_OS_DARWIN */

void UISession::sltHandleHostScreenCountChange()
{
    LogRelFlow(("UISession: Host-screen count changed.\n"));

    /* Notify current machine-logic: */
    emit sigHostScreenCountChanged();
}

void UISession::sltHandleHostScreenGeometryChange()
{
    LogRelFlow(("UISession: Host-screen geometry changed.\n"));

    /* Notify current machine-logic: */
    emit sigHostScreenGeometryChanged();
}

void UISession::sltAdditionsChange()
{
    /* Get our guest: */
    CGuest guest = session().GetConsole().GetGuest();

    /* Variable flags: */
    ULONG ulGuestAdditionsRunLevel = guest.GetAdditionsRunLevel();
    LONG64 lLastUpdatedIgnored;
    bool fIsGuestSupportsGraphics = guest.GetFacilityStatus(KAdditionsFacilityType_Graphics, lLastUpdatedIgnored)
                                    == KAdditionsFacilityStatus_Active;
    bool fIsGuestSupportsSeamless = guest.GetFacilityStatus(KAdditionsFacilityType_Seamless, lLastUpdatedIgnored)
                                    == KAdditionsFacilityStatus_Active;
    /* Check if something had changed: */
    if (m_ulGuestAdditionsRunLevel != ulGuestAdditionsRunLevel ||
        m_fIsGuestSupportsGraphics != fIsGuestSupportsGraphics ||
        m_fIsGuestSupportsSeamless != fIsGuestSupportsSeamless)
    {
        /* Store new data: */
        m_ulGuestAdditionsRunLevel = ulGuestAdditionsRunLevel;
        m_fIsGuestSupportsGraphics = fIsGuestSupportsGraphics;
        m_fIsGuestSupportsSeamless = fIsGuestSupportsSeamless;

        /* Notify listeners about guest additions state changed: */
        emit sigAdditionsStateChange();
    }
}

void UISession::prepareConsoleEventHandlers()
{
    /* Initialize console event-handler: */
    UIConsoleEventHandler::instance(this);

    /* Add console event connections: */
    connect(gConsoleEvents, SIGNAL(sigMousePointerShapeChange(bool, bool, QPoint, QSize, QVector<uint8_t>)),
            this, SLOT(sltMousePointerShapeChange(bool, bool, QPoint, QSize, QVector<uint8_t>)));

    connect(gConsoleEvents, SIGNAL(sigMouseCapabilityChange(bool, bool, bool, bool)),
            this, SLOT(sltMouseCapabilityChange(bool, bool, bool, bool)));

    connect(gConsoleEvents, SIGNAL(sigKeyboardLedsChangeEvent(bool, bool, bool)),
            this, SLOT(sltKeyboardLedsChangeEvent(bool, bool, bool)));

    connect(gConsoleEvents, SIGNAL(sigStateChange(KMachineState)),
            this, SLOT(sltStateChange(KMachineState)));

    connect(gConsoleEvents, SIGNAL(sigAdditionsChange()),
            this, SLOT(sltAdditionsChange()));

    connect(gConsoleEvents, SIGNAL(sigVRDEChange()),
            this, SLOT(sltVRDEChange()));

    connect(gConsoleEvents, SIGNAL(sigVideoCaptureChange()),
            this, SLOT(sltVideoCaptureChange()));

    connect(gConsoleEvents, SIGNAL(sigNetworkAdapterChange(CNetworkAdapter)),
            this, SIGNAL(sigNetworkAdapterChange(CNetworkAdapter)));

    connect(gConsoleEvents, SIGNAL(sigMediumChange(CMediumAttachment)),
            this, SIGNAL(sigMediumChange(CMediumAttachment)));

    connect(gConsoleEvents, SIGNAL(sigUSBControllerChange()),
            this, SIGNAL(sigUSBControllerChange()));

    connect(gConsoleEvents, SIGNAL(sigUSBDeviceStateChange(CUSBDevice, bool, CVirtualBoxErrorInfo)),
            this, SIGNAL(sigUSBDeviceStateChange(CUSBDevice, bool, CVirtualBoxErrorInfo)));

    connect(gConsoleEvents, SIGNAL(sigSharedFolderChange()),
            this, SIGNAL(sigSharedFolderChange()));

    connect(gConsoleEvents, SIGNAL(sigRuntimeError(bool, QString, QString)),
            this, SIGNAL(sigRuntimeError(bool, QString, QString)));

#ifdef Q_WS_MAC
    connect(gConsoleEvents, SIGNAL(sigShowWindow()),
            this, SIGNAL(sigShowWindows()), Qt::QueuedConnection);
#endif /* Q_WS_MAC */

    connect(gConsoleEvents, SIGNAL(sigCPUExecutionCapChange()),
            this, SIGNAL(sigCPUExecutionCapChange()));

    connect(gConsoleEvents, SIGNAL(sigGuestMonitorChange(KGuestMonitorChangedEventType, ulong, QRect)),
            this, SLOT(sltGuestMonitorChange(KGuestMonitorChangedEventType, ulong, QRect)));
}

void UISession::prepareConnections()
{
    connect(this, SIGNAL(sigCloseRuntimeUI()), this, SLOT(sltCloseRuntimeUI()));

#ifdef Q_WS_MAC
    /* Install native display reconfiguration callback: */
    CGDisplayRegisterReconfigurationCallback(cgDisplayReconfigurationCallback, this);
#else /* !Q_WS_MAC */
    /* Install Qt display reconfiguration callbacks: */
    connect(QApplication::desktop(), SIGNAL(screenCountChanged(int)),
            this, SLOT(sltHandleHostScreenCountChange()));
    connect(QApplication::desktop(), SIGNAL(resized(int)),
            this, SLOT(sltHandleHostScreenGeometryChange()));
    connect(QApplication::desktop(), SIGNAL(workAreaResized(int)),
            this, SLOT(sltHandleHostScreenGeometryChange()));
#endif /* !Q_WS_MAC */
}

void UISession::prepareScreens()
{
#ifdef Q_WS_MAC
    /* Recache display data: */
    recacheDisplayData();
    /* Prepare display-change watchdog: */
    m_pWatchdogDisplayChange = new QTimer(this);
    {
        m_pWatchdogDisplayChange->setInterval(500);
        m_pWatchdogDisplayChange->setSingleShot(true);
        connect(m_pWatchdogDisplayChange, SIGNAL(timeout()),
                this, SLOT(sltCheckIfHostDisplayChanged()));
    }
#endif /* Q_WS_MAC */

    /* Get machine: */
    CMachine machine = m_session.GetMachine();

    /* Prepare initial screen visibility status: */
    m_monitorVisibilityVector.resize(machine.GetMonitorCount());
    m_monitorVisibilityVector.fill(false);
    m_monitorVisibilityVector[0] = true;

    /* If machine is in 'saved' state: */
    if (isSaved())
    {
        /* Update screen visibility status from saved-state: */
        for (int i = 0; i < m_monitorVisibilityVector.size(); ++i)
        {
            BOOL fEnabled = true;
            ULONG guestOriginX = 0, guestOriginY = 0, guestWidth = 0, guestHeight = 0;
            machine.QuerySavedGuestScreenInfo(i, guestOriginX, guestOriginY, guestWidth, guestHeight, fEnabled);
            m_monitorVisibilityVector[i] = fEnabled;
        }
        /* And make sure at least one of them is visible (primary if others are hidden): */
        if (countOfVisibleWindows() < 1)
            m_monitorVisibilityVector[0] = true;
    }
}

void UISession::prepareFramebuffers()
{
    /* Each framebuffer will be really prepared on first UIMachineView creation: */
    m_frameBufferVector.resize(m_session.GetMachine().GetMonitorCount());
}

void UISession::prepareMenuPool()
{
    m_pMenuPool = new UIMachineMenuBar(this);
}

void UISession::loadSessionSettings()
{
   /* Get uisession machine: */
    CMachine machine = session().GetConsole().GetMachine();

    /* Load extra-data settings: */
    {
        /* Runtime menu settings: */
#ifdef Q_WS_MAC
        m_allowedActionsMenuApplication = (RuntimeMenuApplicationActionType)
                                          (vboxGlobal().restrictedRuntimeMenuApplicationActionTypes(machine) ^
                                           RuntimeMenuApplicationActionType_All);
#endif /* Q_WS_MAC */
        m_allowedActionsMenuMachine     = (RuntimeMenuMachineActionType)
                                          (vboxGlobal().restrictedRuntimeMenuMachineActionTypes(machine) ^
                                           RuntimeMenuMachineActionType_All);
        m_allowedActionsMenuView        = (RuntimeMenuViewActionType)
                                          (vboxGlobal().restrictedRuntimeMenuViewActionTypes(machine) ^
                                           RuntimeMenuViewActionType_All);
        m_allowedActionsMenuDevices     = (RuntimeMenuDevicesActionType)
                                          (vboxGlobal().restrictedRuntimeMenuDevicesActionTypes(machine) ^
                                           RuntimeMenuDevicesActionType_All);
#ifdef VBOX_WITH_DEBUGGER_GUI
        m_allowedActionsMenuDebugger    = (RuntimeMenuDebuggerActionType)
                                          (vboxGlobal().restrictedRuntimeMenuDebuggerActionTypes(machine) ^
                                           RuntimeMenuDebuggerActionType_All);
#endif /* VBOX_WITH_DEBUGGER_GUI */
        m_allowedActionsMenuHelp        = (RuntimeMenuHelpActionType)
                                          (vboxGlobal().restrictedRuntimeMenuHelpActionTypes(machine) ^
                                           RuntimeMenuHelpActionType_All);

        /* Temporary: */
        QString strSettings;

        /* Is there should be First RUN Wizard? */
        strSettings = machine.GetExtraData(GUI_FirstRun);
        if (strSettings == "yes")
            m_fIsFirstTimeStarted = true;

        /* Ignore mediums mounted at runtime? */
        strSettings = machine.GetExtraData(GUI_SaveMountedAtRuntime);
        if (strSettings == "no")
            m_fIsIgnoreRuntimeMediumsChanging = true;

        /* Should guest autoresize? */
        strSettings = machine.GetExtraData(GUI_AutoresizeGuest);
        QAction *pGuestAutoresizeSwitch = gActionPool->action(UIActionIndexRuntime_Toggle_GuestAutoresize);
        pGuestAutoresizeSwitch->setChecked(strSettings != "off");

        /* Should we allow reconfiguration? */
        m_fReconfigurable = VBoxGlobal::shouldWeAllowMachineReconfiguration(machine);
        updateSessionSettings();

        /* What is the default close action and the restricted are? */
        m_defaultCloseAction = vboxGlobal().defaultMachineCloseAction(machine);
        m_restrictedCloseActions = vboxGlobal().restrictedMachineCloseActions(machine);
        m_fAllCloseActionsRestricted =  (m_restrictedCloseActions & MachineCloseAction_SaveState)
                                     && (m_restrictedCloseActions & MachineCloseAction_Shutdown)
                                     && (m_restrictedCloseActions & MachineCloseAction_PowerOff);
                                     // Close VM Dialog hides PowerOff_RestoringSnapshot implicitly if PowerOff is hidden..
                                     // && (m_restrictedCloseActions & MachineCloseAction_PowerOff_RestoringSnapshot);

        /* Should we allow snapshot operations? */
        m_fSnapshotOperationsAllowed = vboxGlobal().shouldWeAllowSnapshotOperations(machine);

#if 0 /* Disabled for now! */
# ifdef Q_WS_WIN
        /* Disable host screen-saver if requested: */
        if (vboxGlobal().settings().hostScreenSaverDisabled())
            SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, false, 0, 0);
# endif /* Q_WS_WIN */
#endif
    }
}

void UISession::saveSessionSettings()
{
    /* Get uisession machine: */
    CMachine machine = session().GetConsole().GetMachine();

    /* Save extra-data settings: */
    {
        /* Disable First RUN Wizard for the since now: */
        machine.SetExtraData(GUI_FirstRun, QString());

        /* Remember if guest should autoresize: */
        machine.SetExtraData(GUI_AutoresizeGuest,
                             gActionPool->action(UIActionIndexRuntime_Toggle_GuestAutoresize)->isChecked() ?
                             QString() : "off");

#if 0 /* Disabled for now! */
# ifdef Q_WS_WIN
        /* Restore screen-saver activity to system default: */
        SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, true, 0, 0);
# endif /* Q_WS_WIN */
#endif
    }
}

void UISession::cleanupMenuPool()
{
    delete m_pMenuPool;
    m_pMenuPool = 0;
}

void UISession::cleanupFramebuffers()
{
    /* Cleanup framebuffers finally: */
    for (int i = m_frameBufferVector.size() - 1; i >= 0; --i)
    {
        UIFrameBuffer *pFb = m_frameBufferVector[i];
        if (pFb)
        {
            /* Mark framebuffer as unused: */
            pFb->setMarkAsUnused(true);
            /* Detach framebuffer from Display: */
            CDisplay display = session().GetConsole().GetDisplay();
            display.SetFramebuffer(i, CFramebuffer(NULL));
            /* Release framebuffer reference: */
            pFb->Release();
        }
    }
    m_frameBufferVector.clear();
}

void UISession::cleanupConsoleEventHandlers()
{
    /* Destroy console event-handler: */
    UIConsoleEventHandler::destroy();
}

void UISession::cleanupConnections()
{
#ifdef Q_WS_MAC
    /* Remove display reconfiguration callback: */
    CGDisplayRemoveReconfigurationCallback(cgDisplayReconfigurationCallback, this);
#endif /* Q_WS_MAC */
}

void UISession::updateSessionSettings()
{
    bool fAllowReconfiguration = m_machineState != KMachineState_Stuck && m_fReconfigurable;
    gActionPool->action(UIActionIndexRuntime_Simple_SettingsDialog)->setEnabled(fAllowReconfiguration);
    gActionPool->action(UIActionIndexRuntime_Simple_SharedFoldersSettings)->setEnabled(fAllowReconfiguration);
    gActionPool->action(UIActionIndexRuntime_Simple_VideoCaptureSettings)->setEnabled(fAllowReconfiguration);
    gActionPool->action(UIActionIndexRuntime_Simple_NetworkSettings)->setEnabled(fAllowReconfiguration);
}

WId UISession::winId() const
{
    return mainMachineWindow()->winId();
}

void UISession::setPointerShape(const uchar *pShapeData, bool fHasAlpha,
                                uint uXHot, uint uYHot, uint uWidth, uint uHeight)
{
    AssertMsg(pShapeData, ("Shape data must not be NULL!\n"));

    m_fIsValidPointerShapePresent = false;
    const uchar *srcAndMaskPtr = pShapeData;
    uint andMaskSize = (uWidth + 7) / 8 * uHeight;
    const uchar *srcShapePtr = pShapeData + ((andMaskSize + 3) & ~3);
    uint srcShapePtrScan = uWidth * 4;

#if defined (Q_WS_WIN)

    BITMAPV5HEADER bi;
    HBITMAP hBitmap;
    void *lpBits;

    ::ZeroMemory(&bi, sizeof (BITMAPV5HEADER));
    bi.bV5Size = sizeof(BITMAPV5HEADER);
    bi.bV5Width = uWidth;
    bi.bV5Height = - (LONG)uHeight;
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask   = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask  = 0x000000FF;
    if (fHasAlpha)
        bi.bV5AlphaMask = 0xFF000000;
    else
        bi.bV5AlphaMask = 0;

    HDC hdc = GetDC(NULL);

    /* Create the DIB section with an alpha channel: */
    hBitmap = CreateDIBSection(hdc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, (void **)&lpBits, NULL, (DWORD) 0);

    ReleaseDC(NULL, hdc);

    HBITMAP hMonoBitmap = NULL;
    if (fHasAlpha)
    {
        /* Create an empty mask bitmap: */
        hMonoBitmap = CreateBitmap(uWidth, uHeight, 1, 1, NULL);
    }
    else
    {
        /* Word aligned AND mask. Will be allocated and created if necessary. */
        uint8_t *pu8AndMaskWordAligned = NULL;

        /* Width in bytes of the original AND mask scan line. */
        uint32_t cbAndMaskScan = (uWidth + 7) / 8;

        if (cbAndMaskScan & 1)
        {
            /* Original AND mask is not word aligned. */

            /* Allocate memory for aligned AND mask. */
            pu8AndMaskWordAligned = (uint8_t *)RTMemTmpAllocZ((cbAndMaskScan + 1) * uHeight);

            Assert(pu8AndMaskWordAligned);

            if (pu8AndMaskWordAligned)
            {
                /* According to MSDN the padding bits must be 0.
                 * Compute the bit mask to set padding bits to 0 in the last byte of original AND mask. */
                uint32_t u32PaddingBits = cbAndMaskScan * 8  - uWidth;
                Assert(u32PaddingBits < 8);
                uint8_t u8LastBytesPaddingMask = (uint8_t)(0xFF << u32PaddingBits);

                Log(("u8LastBytesPaddingMask = %02X, aligned w = %d, width = %d, cbAndMaskScan = %d\n",
                      u8LastBytesPaddingMask, (cbAndMaskScan + 1) * 8, uWidth, cbAndMaskScan));

                uint8_t *src = (uint8_t *)srcAndMaskPtr;
                uint8_t *dst = pu8AndMaskWordAligned;

                unsigned i;
                for (i = 0; i < uHeight; i++)
                {
                    memcpy(dst, src, cbAndMaskScan);

                    dst[cbAndMaskScan - 1] &= u8LastBytesPaddingMask;

                    src += cbAndMaskScan;
                    dst += cbAndMaskScan + 1;
                }
            }
        }

        /* Create the AND mask bitmap: */
        hMonoBitmap = ::CreateBitmap(uWidth, uHeight, 1, 1,
                                     pu8AndMaskWordAligned? pu8AndMaskWordAligned: srcAndMaskPtr);

        if (pu8AndMaskWordAligned)
        {
            RTMemTmpFree(pu8AndMaskWordAligned);
        }
    }

    Assert(hBitmap);
    Assert(hMonoBitmap);
    if (hBitmap && hMonoBitmap)
    {
        DWORD *dstShapePtr = (DWORD *) lpBits;

        for (uint y = 0; y < uHeight; y ++)
        {
            memcpy(dstShapePtr, srcShapePtr, srcShapePtrScan);
            srcShapePtr += srcShapePtrScan;
            dstShapePtr += uWidth;
        }

        ICONINFO ii;
        ii.fIcon = FALSE;
        ii.xHotspot = uXHot;
        ii.yHotspot = uYHot;
        ii.hbmMask = hMonoBitmap;
        ii.hbmColor = hBitmap;

        HCURSOR hAlphaCursor = CreateIconIndirect(&ii);
        Assert(hAlphaCursor);
        if (hAlphaCursor)
        {
            /* Set the new cursor: */
            m_cursor = QCursor(hAlphaCursor);
            if (m_alphaCursor)
                DestroyIcon(m_alphaCursor);
            m_alphaCursor = hAlphaCursor;
            m_fIsValidPointerShapePresent = true;
        }
    }

    if (hMonoBitmap)
        DeleteObject(hMonoBitmap);
    if (hBitmap)
        DeleteObject(hBitmap);

#elif defined (Q_WS_X11) && !defined (VBOX_WITHOUT_XCURSOR)

    XcursorImage *img = XcursorImageCreate(uWidth, uHeight);
    Assert(img);
    if (img)
    {
        img->xhot = uXHot;
        img->yhot = uYHot;

        XcursorPixel *dstShapePtr = img->pixels;

        for (uint y = 0; y < uHeight; y ++)
        {
            memcpy (dstShapePtr, srcShapePtr, srcShapePtrScan);

            if (!fHasAlpha)
            {
                /* Convert AND mask to the alpha channel: */
                uchar byte = 0;
                for (uint x = 0; x < uWidth; x ++)
                {
                    if (!(x % 8))
                        byte = *(srcAndMaskPtr ++);
                    else
                        byte <<= 1;

                    if (byte & 0x80)
                    {
                        /* Linux doesn't support inverted pixels (XOR ops,
                         * to be exact) in cursor shapes, so we detect such
                         * pixels and always replace them with black ones to
                         * make them visible at least over light colors */
                        if (dstShapePtr [x] & 0x00FFFFFF)
                            dstShapePtr [x] = 0xFF000000;
                        else
                            dstShapePtr [x] = 0x00000000;
                    }
                    else
                        dstShapePtr [x] |= 0xFF000000;
                }
            }

            srcShapePtr += srcShapePtrScan;
            dstShapePtr += uWidth;
        }

        /* Set the new cursor: */
        m_cursor = QCursor(XcursorImageLoadCursor(QX11Info::display(), img));
        m_fIsValidPointerShapePresent = true;

        XcursorImageDestroy(img);
    }

#elif defined(Q_WS_MAC)

    /* Create a ARGB image out of the shape data. */
    QImage image  (uWidth, uHeight, QImage::Format_ARGB32);
    const uint8_t* pbSrcMask = static_cast<const uint8_t*> (srcAndMaskPtr);
    unsigned cbSrcMaskLine = RT_ALIGN (uWidth, 8) / 8;
    for (unsigned int y = 0; y < uHeight; ++y)
    {
        for (unsigned int x = 0; x < uWidth; ++x)
        {
           unsigned int color = ((unsigned int*)srcShapePtr)[y*uWidth+x];
           /* If the alpha channel isn't in the shape data, we have to
            * create them from the and-mask. This is a bit field where 1
            * represent transparency & 0 opaque respectively. */
           if (!fHasAlpha)
           {
               if (!(pbSrcMask[x / 8] & (1 << (7 - (x % 8)))))
                   color  |= 0xff000000;
               else
               {
                   /* This isn't quite right, but it's the best we can do I think... */
                   if (color & 0x00ffffff)
                       color = 0xff000000;
                   else
                       color = 0x00000000;
               }
           }
           image.setPixel (x, y, color);
        }
        /* Move one scanline forward. */
        pbSrcMask += cbSrcMaskLine;
    }

    /* Set the new cursor: */
    m_cursor = QCursor(QPixmap::fromImage(image), uXHot, uYHot);
    m_fIsValidPointerShapePresent = true;
    NOREF(srcShapePtrScan);

#else

# warning "port me"

#endif
}

void UISession::reinitMenuPool()
{
    /* Get host: */
    const CHost &host = vboxGlobal().host();

    /* Get uisession machine: */
    const CMachine &machine = session().GetConsole().GetMachine();

    /* Storage stuff: */
    {
        /* Initialize CD/FD menus: */
        int iDevicesCountCD = 0;
        int iDevicesCountFD = 0;
        const CMediumAttachmentVector &attachments = machine.GetMediumAttachments();
        for (int i = 0; i < attachments.size(); ++i)
        {
            const CMediumAttachment &attachment = attachments[i];
            if (attachment.GetType() == KDeviceType_DVD)
                ++iDevicesCountCD;
            if (attachment.GetType() == KDeviceType_Floppy)
                ++iDevicesCountFD;
        }
        QAction *pOpticalDevicesMenu = gActionPool->action(UIActionIndexRuntime_Menu_OpticalDevices);
        QAction *pFloppyDevicesMenu = gActionPool->action(UIActionIndexRuntime_Menu_FloppyDevices);
        pOpticalDevicesMenu->setData(iDevicesCountCD);
        pOpticalDevicesMenu->setVisible(iDevicesCountCD);
        pFloppyDevicesMenu->setData(iDevicesCountFD);
        pFloppyDevicesMenu->setVisible(iDevicesCountFD);
    }

    /* Network stuff: */
    {
        bool fAtLeastOneAdapterActive = false;
        ULONG uSlots = vboxGlobal().virtualBox().GetSystemProperties().GetMaxNetworkAdapters(KChipsetType_PIIX3);
        for (ULONG uSlot = 0; uSlot < uSlots; ++uSlot)
        {
            const CNetworkAdapter &adapter = machine.GetNetworkAdapter(uSlot);
            if (adapter.GetEnabled())
            {
                fAtLeastOneAdapterActive = true;
                break;
            }
        }

        /* Show/Hide Network Adapters action depending on overall adapters activity status: */
        gActionPool->action(UIActionIndexRuntime_Simple_NetworkSettings)->setVisible(fAtLeastOneAdapterActive);
    }

    /* USB stuff: */
    {
        /* Check whether there is at least one OHCI USB controllers with an available proxy. */
        const CUSBDeviceFilters &filters = machine.GetUSBDeviceFilters();
        ULONG cOhciCtls = machine.GetUSBControllerCountByType(KUSBControllerType_OHCI);
        bool fUSBEnabled = !filters.isNull() && cOhciCtls && machine.GetUSBProxyAvailable();

        /* Show/Hide USB menu depending on controller availability, activity and USB-proxy presence: */
        gActionPool->action(UIActionIndexRuntime_Menu_USBDevices)->setVisible(fUSBEnabled);
    }

    /* WebCams stuff: */
    {
        /* Check whether there is an accessible video input devices pool: */
        const CHostVideoInputDeviceVector &webcams = host.GetVideoInputDevices(); Q_UNUSED(webcams);
        ULONG cOhciCtls = machine.GetUSBControllerCountByType(KUSBControllerType_OHCI);
        bool fWebCamsEnabled = host.isOk() && cOhciCtls;

        /* Show/Hide WebCams menu depending on ExtPack availability: */
        gActionPool->action(UIActionIndexRuntime_Menu_WebCams)->setVisible(fWebCamsEnabled);
    }
}

bool UISession::preparePowerUp()
{
    /* Notify user about mouse&keyboard auto-capturing: */
    if (vboxGlobal().settings().autoCapture())
        popupCenter().remindAboutAutoCapture(machineLogic()->activeMachineWindow());

    /* Shows First Run wizard if necessary: */
    const CMachine &machine = session().GetMachine();
    /* Check if we are in teleportation waiting mode.
     * In that case no first run wizard is necessary. */
    m_machineState = machine.GetState();
    if (   isFirstTimeStarted()
        && !((   m_machineState == KMachineState_PoweredOff
              || m_machineState == KMachineState_Aborted
              || m_machineState == KMachineState_Teleported)
             && machine.GetTeleporterEnabled()))
    {
        UISafePointerWizard pWizard = new UIWizardFirstRun(mainMachineWindow(), session().GetMachine());
        pWizard->prepare();
        pWizard->exec();
        if (pWizard)
            delete pWizard;
    }

#ifdef VBOX_WITH_NETFLT

    /* Skip further checks if VM in saved state */
    if (isSaved())
        return true;

    /* Make sure all the attached and enabled network
     * adapters are present on the host. This check makes sense
     * in two cases only - when attachement type is Bridged Network
     * or Host-only Interface. NOTE: Only currently enabled
     * attachement type is checked (incorrect parameters check for
     * currently disabled attachement types is skipped). */
    QStringList failedInterfaceNames;
    QStringList availableInterfaceNames;

    /* Create host network interface names list */
    foreach (const CHostNetworkInterface &iface, vboxGlobal().host().GetNetworkInterfaces())
    {
        availableInterfaceNames << iface.GetName();
        availableInterfaceNames << iface.GetShortName();
    }

    ulong cCount = vboxGlobal().virtualBox().GetSystemProperties().GetMaxNetworkAdapters(machine.GetChipsetType());
    for (ulong uAdapterIndex = 0; uAdapterIndex < cCount; ++uAdapterIndex)
    {
        CNetworkAdapter na = machine.GetNetworkAdapter(uAdapterIndex);

        if (na.GetEnabled())
        {
            QString strIfName = QString();

            /* Get physical network interface name for currently
             * enabled network attachement type */
            switch (na.GetAttachmentType())
            {
                case KNetworkAttachmentType_Bridged:
                    strIfName = na.GetBridgedInterface();
                    break;
                case KNetworkAttachmentType_HostOnly:
                    strIfName = na.GetHostOnlyInterface();
                    break;
            }

            if (!strIfName.isEmpty() &&
                !availableInterfaceNames.contains(strIfName))
            {
                LogFlow(("Found invalid network interface: %s\n", strIfName.toStdString().c_str()));
                failedInterfaceNames << QString("%1 (adapter %2)").arg(strIfName).arg(uAdapterIndex + 1);
            }
        }
    }

    /* Check if non-existent interfaces found */
    if (!failedInterfaceNames.isEmpty())
    {
        if (msgCenter().UIMessageCenter::cannotStartWithoutNetworkIf(machine.GetName(), failedInterfaceNames.join(", ")))
            machineLogic()->openNetworkAdaptersDialog();
        else
        {
            closeRuntimeUI();
            return false;
        }
    }

#endif

    return true;
}

bool UISession::isScreenVisible(ulong uScreenId) const
{
    Assert(uScreenId < (ulong)m_monitorVisibilityVector.size());
    return m_monitorVisibilityVector.value((int)uScreenId, false);
}

void UISession::setScreenVisible(ulong uScreenId, bool fIsMonitorVisible)
{
    Assert(uScreenId < (ulong)m_monitorVisibilityVector.size());
    if (uScreenId < (ulong)m_monitorVisibilityVector.size())
        m_monitorVisibilityVector[(int)uScreenId] = fIsMonitorVisible;
}

int UISession::countOfVisibleWindows()
{
    int cCountOfVisibleWindows = 0;
    for (int i = 0; i < m_monitorVisibilityVector.size(); ++i)
        if (m_monitorVisibilityVector[i])
            ++cCountOfVisibleWindows;
    return cCountOfVisibleWindows;
}

UIFrameBuffer* UISession::frameBuffer(ulong uScreenId) const
{
    Assert(uScreenId < (ulong)m_frameBufferVector.size());
    return m_frameBufferVector.value((int)uScreenId, 0);
}

void UISession::setFrameBuffer(ulong uScreenId, UIFrameBuffer* pFrameBuffer)
{
    Assert(uScreenId < (ulong)m_frameBufferVector.size());
    if (uScreenId < (ulong)m_frameBufferVector.size())
        m_frameBufferVector[(int)uScreenId] = pFrameBuffer;
}

#ifdef Q_WS_MAC
/** MacOS X: Recaches display-configuration data. */
void UISession::recacheDisplayData()
{
    /* Recache display data: */
    m_screens.clear();
    QDesktopWidget *pDesktop = QApplication::desktop();
    for (int iScreenIndex = 0; iScreenIndex < pDesktop->screenCount(); ++iScreenIndex)
        m_screens << pDesktop->screenGeometry(iScreenIndex);
}
#endif /* Q_WS_MAC */

#ifdef VBOX_GUI_WITH_KEYS_RESET_HANDLER
/**
 * Custom signal handler. When switching VTs, we might not get release events
 * for Ctrl-Alt and in case a savestate is performed on the new VT, the VM will
 * be saved with modifier keys stuck. This is annoying enough for introducing
 * this hack.
 */
/* static */
static void signalHandlerSIGUSR1(int sig, siginfo_t * /* pInfo */, void * /*pSecret */)
{
    /* only SIGUSR1 is interesting */
    if (sig == SIGUSR1)
        if (UIMachine *pMachine = vboxGlobal().virtualMachine())
            pMachine->uisession()->machineLogic()->keyboardHandler()->releaseAllPressedKeys();
}
#endif /* VBOX_GUI_WITH_KEYS_RESET_HANDLER */

