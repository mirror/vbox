/* $Id$ */
/** @file
 * VBox Qt GUI - UISession class implementation.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/* Qt includes: */
#include <QApplication>
#include <QWidget>
#ifdef VBOX_WS_WIN
# include <iprt/win/windows.h> /* Workaround for compile errors if included directly by QtWin. */
# include <QtWin>
#endif

/* GUI includes: */
#include "UICommon.h"
#include "UIExtraDataManager.h"
#include "UISession.h"
#include "UIMachine.h"
#include "UIMedium.h"
#include "UIActionPoolRuntime.h"
#include "UIMachineLogic.h"
#include "UIMachineView.h"
#include "UIMachineWindow.h"
#include "UIMessageCenter.h"
#include "UIMousePointerShapeData.h"
#include "UINotificationCenter.h"
#include "UIConsoleEventHandler.h"
#include "UIFrameBuffer.h"
#include "UISettingsDialogSpecific.h"
#ifdef VBOX_GUI_WITH_KEYS_RESET_HANDLER
# include "UIKeyboardHandler.h"
# include <signal.h>
#endif

/* COM includes: */
#include "CGraphicsAdapter.h"
#include "CHostNetworkInterface.h"
#include "CHostUSBDevice.h"
#include "CMedium.h"
#include "CMediumAttachment.h"
#include "CSnapshot.h"
#include "CStorageController.h"
#include "CSystemProperties.h"
#include "CUSBController.h"
#include "CUSBDeviceFilter.h"
#include "CUSBDeviceFilters.h"
#ifdef VBOX_WITH_NETFLT
# include "CNetworkAdapter.h"
#endif

/* External includes: */
#ifdef VBOX_WS_X11
# include <X11/Xlib.h>
# include <X11/Xutil.h>
#endif


#ifdef VBOX_GUI_WITH_KEYS_RESET_HANDLER
static void signalHandlerSIGUSR1(int sig, siginfo_t *, void *);
#endif

/* static */
bool UISession::create(UISession *&pSession, UIMachine *pMachine)
{
    /* Make sure NULL pointer passed: */
    AssertReturn(!pSession, false);

    /* Create session UI: */
    pSession = new UISession(pMachine);
    AssertPtrReturn(pSession, false);

    /* Make sure it's prepared: */
    if (!pSession->prepare())
    {
        /* Destroy session UI otherwise: */
        destroy(pSession);
        /* False in that case: */
        return false;
    }

    /* True by default: */
    return true;
}

/* static */
void UISession::destroy(UISession *&pSession)
{
    /* Make sure valid pointer passed: */
    AssertPtrReturnVoid(pSession);

    /* Delete session: */
    delete pSession;
    pSession = 0;
}

bool UISession::initialize()
{
    /* Preprocess initialization: */
    if (!preprocessInitialization())
        return false;

    /* Notify user about mouse&keyboard auto-capturing: */
    if (gEDataManager->autoCaptureEnabled())
        UINotificationMessage::remindAboutAutoCapture();

    m_machineState = machine().GetState();

    /* Apply debug settings from the command line. */
    if (!debugger().isNull() && debugger().isOk())
    {
        if (uiCommon().areWeToExecuteAllInIem())
            debugger().SetExecuteAllInIEM(true);
        if (!uiCommon().isDefaultWarpPct())
            debugger().SetVirtualTimeRate(uiCommon().getWarpPct());
    }

    /* Apply ad-hoc reconfigurations from the command line: */
    if (uiCommon().hasFloppyImageToMount())
        mountAdHocImage(KDeviceType_Floppy, UIMediumDeviceType_Floppy, uiCommon().getFloppyImage().toString());
    if (uiCommon().hasDvdImageToMount())
        mountAdHocImage(KDeviceType_DVD, UIMediumDeviceType_DVD, uiCommon().getDvdImage().toString());

    /* Power UP if this is NOT separate process: */
    if (!uiCommon().isSeparateProcess())
        if (!powerUp())
            return false;

    /* Make sure all the pending Console events converted to signals
     * during the powerUp() progress above reached their destinations.
     * That is necessary to make sure all the pending machine state change events processed.
     * We can't just use the machine state directly acquired from IMachine because there
     * will be few places which are using stale machine state, not just this one. */
    QApplication::sendPostedEvents(0, QEvent::MetaCall);

    /* Check if we missed a really quick termination after successful startup: */
    if (isTurnedOff())
    {
        LogRel(("GUI: Aborting startup due to invalid machine state detected: %d\n", machineState()));
        return false;
    }

    /* Postprocess initialization: */
    if (!postprocessInitialization())
        return false;

    /* Fetch corresponding states: */
    if (uiCommon().isSeparateProcess())
    {
        sltAdditionsChange();
    }
    machineLogic()->initializePostPowerUp();

    /* Load VM settings: */
    loadVMSettings();

#ifdef VBOX_GUI_WITH_PIDFILE
    uiCommon().createPidfile();
#endif /* VBOX_GUI_WITH_PIDFILE */

    /* Warn listeners about we are initialized: */
    m_fInitialized = true;
    emit sigInitialized();

    /* True by default: */
    return true;
}

bool UISession::powerUp()
{
    /* Power UP machine: */
    CProgress progress = uiCommon().shouldStartPaused() ? console().PowerUpPaused() : console().PowerUp();

    /* Check for immediate failure: */
    if (!console().isOk() || progress.isNull())
    {
        if (uiCommon().showStartVMErrors())
            msgCenter().cannotStartMachine(console(), machineName());
        LogRel(("GUI: Aborting startup due to power up issue detected...\n"));
        return false;
    }

    /* Some logging right after we powered up: */
    LogRel(("Qt version: %s\n", UICommon::qtRTVersionString().toUtf8().constData()));
#ifdef VBOX_WS_X11
    LogRel(("X11 Window Manager code: %d\n", (int)uiCommon().typeOfWindowManager()));
#endif

    /* Enable 'manual-override',
     * preventing automatic Runtime UI closing
     * and visual representation mode changes: */
    uimachine()->setManualOverrideMode(true);

    /* Show "Starting/Restoring" progress dialog: */
    if (isSaved())
    {
        msgCenter().showModalProgressDialog(progress, machineName(), ":/progress_state_restore_90px.png", 0, 0);
        /* After restoring from 'saved' state, machine-window(s) geometry should be adjusted: */
        machineLogic()->adjustMachineWindowsGeometry();
    }
    else
    {
#ifdef VBOX_IS_QT6_OR_LATER /** @todo why is this any problem on qt6? */
        msgCenter().showModalProgressDialog(progress, machineName(), ":/progress_start_90px.png", 0, 0);
#else
        msgCenter().showModalProgressDialog(progress, machineName(), ":/progress_start_90px.png");
#endif
        /* After VM start, machine-window(s) size-hint(s) should be sent: */
        machineLogic()->sendMachineWindowsSizeHints();
    }

    /* Check for progress failure: */
    if (!progress.isOk() || progress.GetResultCode() != 0)
    {
        if (uiCommon().showStartVMErrors())
            msgCenter().cannotStartMachine(progress, machineName());
        LogRel(("GUI: Aborting startup due to power up progress issue detected...\n"));
        return false;
    }

    /* Disable 'manual-override' finally: */
    uimachine()->setManualOverrideMode(false);

    /* True by default: */
    return true;
}

UIMachineLogic* UISession::machineLogic() const
{
    return uimachine() ? uimachine()->machineLogic() : 0;
}

QWidget* UISession::mainMachineWindow() const
{
    return machineLogic() ? machineLogic()->mainMachineWindow() : 0;
}

WId UISession::mainMachineWindowId() const
{
    return mainMachineWindow()->winId();
}

UIMachineWindow *UISession::activeMachineWindow() const
{
    return machineLogic() ? machineLogic()->activeMachineWindow() : 0;
}

bool UISession::isVisualStateAllowed(UIVisualStateType state) const
{
    return uimachine()->isVisualStateAllowed(state);
}

void UISession::changeVisualState(UIVisualStateType visualStateType)
{
    uimachine()->asyncChangeVisualState(visualStateType);
}

void UISession::setRequestedVisualState(UIVisualStateType visualStateType)
{
    uimachine()->setRequestedVisualState(visualStateType);
}

UIVisualStateType UISession::requestedVisualState() const
{
    return uimachine()->requestedVisualState();
}

bool UISession::guestAdditionsUpgradable()
{
    if (!machine().isOk())
        return false;

    /* Auto GA update is currently for Windows and Linux guests only */
    const CGuestOSType osType = uiCommon().vmGuestOSType(machine().GetOSTypeId());
    if (!osType.isOk())
        return false;

    const QString strGuestFamily = osType.GetFamilyId();
    bool fIsWindowOrLinux = strGuestFamily.contains("windows", Qt::CaseInsensitive) || strGuestFamily.contains("linux", Qt::CaseInsensitive);

    if (!fIsWindowOrLinux)
        return false;

    /* Also check whether we have something to update automatically: */
    if (m_ulGuestAdditionsRunLevel < (ULONG)KAdditionsRunLevelType_Userland)
        return false;

    return true;
}

bool UISession::setPause(bool fOn)
{
    if (fOn)
        console().Pause();
    else
        console().Resume();

    bool ok = console().isOk();
    if (!ok)
    {
        if (fOn)
            UINotificationMessage::cannotPauseMachine(console());
        else
            UINotificationMessage::cannotResumeMachine(console());
    }

    return ok;
}

void UISession::sltInstallGuestAdditionsFrom(const QString &strSource)
{
    if (!guestAdditionsUpgradable())
        return sltMountDVDAdHoc(strSource);

    /* Update guest additions automatically: */
    UINotificationProgressGuestAdditionsInstall *pNotification =
            new UINotificationProgressGuestAdditionsInstall(guest(), strSource);
    connect(pNotification, &UINotificationProgressGuestAdditionsInstall::sigGuestAdditionsInstallationFailed,
            this, &UISession::sltMountDVDAdHoc);
    gpNotificationCenter->append(pNotification);
}

void UISession::sltMountDVDAdHoc(const QString &strSource)
{
    mountAdHocImage(KDeviceType_DVD, UIMediumDeviceType_DVD, strSource);
}

void UISession::sltDetachCOM()
{
    /* Cleanup everything COM related: */
    cleanupFramebuffers();
    cleanupConsoleEventHandlers();
    cleanupNotificationCenter();
    cleanupSession();
}

void UISession::sltStateChange(KMachineState state)
{
    /* Check if something had changed: */
    if (m_machineState != state)
    {
        /* Store new data: */
        m_machineStatePrevious = m_machineState;
        m_machineState = state;

        /* Notify listeners about machine state changed: */
        emit sigMachineStateChange();
    }
}

void UISession::sltAdditionsChange()
{
    /* Acquire actual states: */
    const ULONG ulGuestAdditionsRunLevel = guest().GetAdditionsRunLevel();
    LONG64 lLastUpdatedIgnored;
    const bool fIsGuestSupportsGraphics = guest().GetFacilityStatus(KAdditionsFacilityType_Graphics, lLastUpdatedIgnored)
                                        == KAdditionsFacilityStatus_Active;
    const bool fIsGuestSupportsSeamless = guest().GetFacilityStatus(KAdditionsFacilityType_Seamless, lLastUpdatedIgnored)
                                        == KAdditionsFacilityStatus_Active;

    /* Check if something had changed: */
    if (   m_ulGuestAdditionsRunLevel != ulGuestAdditionsRunLevel
        || m_fIsGuestSupportsGraphics != fIsGuestSupportsGraphics
        || m_fIsGuestSupportsSeamless != fIsGuestSupportsSeamless)
    {
        /* Store new data: */
        m_ulGuestAdditionsRunLevel = ulGuestAdditionsRunLevel;
        m_fIsGuestSupportsGraphics = fIsGuestSupportsGraphics;
        m_fIsGuestSupportsSeamless = fIsGuestSupportsSeamless;

        /* Notify listeners about GA state really changed: */
        LogRel(("GUI: UISession::sltAdditionsChange: GA state really changed, notifying listeners.\n"));
        emit sigAdditionsStateActualChange();
    }
    else
        LogRel(("GUI: UISession::sltAdditionsChange: GA state doesn't really changed, still notifying listeners.\n"));

    /* Notify listeners about GA state change event came: */
    emit sigAdditionsStateChange();
}

UISession::UISession(UIMachine *pMachine)
    : QObject(pMachine)
    /* Base variables: */
    , m_pMachine(pMachine)
    , m_pConsoleEventhandler(0)
    /* Common variables: */
    , m_machineStatePrevious(KMachineState_Null)
    , m_machineState(KMachineState_Null)
    /* Common flags: */
    , m_fInitialized(false)
    , m_fIsGuestResizeIgnored(false)
    , m_fIsAutoCaptureDisabled(false)
    /* Guest additions flags: */
    , m_ulGuestAdditionsRunLevel(0)
    , m_fIsGuestSupportsGraphics(false)
    , m_fIsGuestSupportsSeamless(false)
    /* CPU hardware virtualization features for VM: */
    , m_enmVMExecutionEngine(KVMExecutionEngine_NotSet)
    , m_fIsHWVirtExNestedPagingEnabled(false)
    , m_fIsHWVirtExUXEnabled(false)
    /* VM's effective paravirtualization provider: */
    , m_paraVirtProvider(KParavirtProvider_None)
{
}

UISession::~UISession()
{
}

bool UISession::prepare()
{
    /* Prepare COM stuff: */
    if (!prepareSession())
        return false;
    prepareNotificationCenter();
    prepareConsoleEventHandlers();
    prepareFramebuffers();

    /* Prepare GUI stuff: */
    prepareConnections();
    prepareSignalHandling();

    /* True by default: */
    return true;
}

bool UISession::prepareSession()
{
    /* Open session: */
    m_session = uiCommon().openSession(uiCommon().managedVMUuid(),
                                         uiCommon().isSeparateProcess()
                                       ? KLockType_Shared
                                       : KLockType_VM);
    if (m_session.isNull())
        return false;

    /* Get machine: */
    m_machine = m_session.GetMachine();
    if (m_machine.isNull())
        return false;

    /* Get console: */
    m_console = m_session.GetConsole();
    if (m_console.isNull())
        return false;

    /* Get display: */
    m_display = m_console.GetDisplay();
    if (m_display.isNull())
        return false;

    /* Get guest: */
    m_guest = m_console.GetGuest();
    if (m_guest.isNull())
        return false;

    /* Get mouse: */
    m_mouse = m_console.GetMouse();
    if (m_mouse.isNull())
        return false;

    /* Get keyboard: */
    m_keyboard = m_console.GetKeyboard();
    if (m_keyboard.isNull())
        return false;

    /* Get debugger: */
    m_debugger = m_console.GetDebugger();
    if (m_debugger.isNull())
        return false;

    /* Update machine-name: */
    m_strMachineName = machine().GetName();

    /* Update machine-state: */
    m_machineState = machine().GetState();

    /* True by default: */
    return true;
}

void UISession::prepareNotificationCenter()
{
    UINotificationCenter::create();
}

void UISession::prepareConsoleEventHandlers()
{
    /* Create console event-handler: */
    m_pConsoleEventhandler = new UIConsoleEventHandler(this);

    /* Console event connections: */
    connect(m_pConsoleEventhandler, &UIConsoleEventHandler::sigAdditionsChange,
            this, &UISession::sltAdditionsChange);
    connect(m_pConsoleEventhandler, &UIConsoleEventHandler::sigAudioAdapterChange,
            this, &UISession::sigAudioAdapterChange);
    connect(m_pConsoleEventhandler, &UIConsoleEventHandler::sigClipboardModeChange,
            this, &UISession::sigClipboardModeChange);
    connect(m_pConsoleEventhandler, &UIConsoleEventHandler::sigCPUExecutionCapChange,
            this, &UISession::sigCPUExecutionCapChange);
    connect(m_pConsoleEventhandler, &UIConsoleEventHandler::sigDnDModeChange,
            this, &UISession::sigDnDModeChange);
    connect(m_pConsoleEventhandler, &UIConsoleEventHandler::sigGuestMonitorChange,
            this, &UISession::sigGuestMonitorChange);
    connect(m_pConsoleEventhandler, &UIConsoleEventHandler::sigMediumChange,
            this, &UISession::sigMediumChange);
    connect(m_pConsoleEventhandler, &UIConsoleEventHandler::sigNetworkAdapterChange,
            this, &UISession::sigNetworkAdapterChange);
    connect(m_pConsoleEventhandler, &UIConsoleEventHandler::sigRecordingChange,
            this, &UISession::sigRecordingChange);
    connect(m_pConsoleEventhandler, &UIConsoleEventHandler::sigSharedFolderChange,
            this, &UISession::sigSharedFolderChange);
    connect(m_pConsoleEventhandler, &UIConsoleEventHandler::sigStateChange,
            this, &UISession::sltStateChange);
    connect(m_pConsoleEventhandler, &UIConsoleEventHandler::sigStorageDeviceChange,
            this, &UISession::sigStorageDeviceChange);
    connect(m_pConsoleEventhandler, &UIConsoleEventHandler::sigUSBControllerChange,
            this, &UISession::sigUSBControllerChange);
    connect(m_pConsoleEventhandler, &UIConsoleEventHandler::sigUSBDeviceStateChange,
            this, &UISession::sigUSBDeviceStateChange);
    connect(m_pConsoleEventhandler, &UIConsoleEventHandler::sigVRDEChange,
            this, &UISession::sigVRDEChange);
    connect(m_pConsoleEventhandler, &UIConsoleEventHandler::sigRuntimeError,
            this, &UISession::sigRuntimeError);

#ifdef VBOX_WS_MAC
    connect(m_pConsoleEventhandler, &UIConsoleEventHandler::sigShowWindow,
            this, &UISession::sigShowWindows, Qt::QueuedConnection);
#endif

    /* Console keyboard connections: */
    connect(m_pConsoleEventhandler, &UIConsoleEventHandler::sigKeyboardLedsChange,
            this, &UISession::sigKeyboardLedsChange);

    /* Console mouse connections: */
    connect(m_pConsoleEventhandler, &UIConsoleEventHandler::sigMousePointerShapeChange,
            this, &UISession::sigMousePointerShapeChange);
    connect(m_pConsoleEventhandler, &UIConsoleEventHandler::sigMouseCapabilityChange,
            this, &UISession::sigMouseCapabilityChange);
    connect(m_pConsoleEventhandler, &UIConsoleEventHandler::sigCursorPositionChange,
            this, &UISession::sigCursorPositionChange);
}

void UISession::prepareFramebuffers()
{
    /* Each framebuffer will be really prepared on first UIMachineView creation: */
    m_frameBufferVector.resize(machine().GetGraphicsAdapter().GetMonitorCount());
}

void UISession::prepareConnections()
{
    /* UICommon connections: */
    connect(&uiCommon(), &UICommon::sigAskToDetachCOM, this, &UISession::sltDetachCOM);
}

void UISession::prepareSignalHandling()
{
#ifdef VBOX_GUI_WITH_KEYS_RESET_HANDLER
    struct sigaction sa;
    sa.sa_sigaction = &signalHandlerSIGUSR1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sigaction(SIGUSR1, &sa, NULL);
#endif /* VBOX_GUI_WITH_KEYS_RESET_HANDLER */
}

void UISession::cleanupFramebuffers()
{
    /* Cleanup framebuffers finally: */
    for (int i = m_frameBufferVector.size() - 1; i >= 0; --i)
    {
        UIFrameBuffer *pFrameBuffer = m_frameBufferVector[i];
        if (pFrameBuffer)
        {
            /* Mark framebuffer as unused: */
            pFrameBuffer->setMarkAsUnused(true);
            /* Detach framebuffer from Display: */
            pFrameBuffer->detach();
            /* Delete framebuffer reference: */
            delete pFrameBuffer;
        }
    }
    m_frameBufferVector.clear();
}

void UISession::cleanupConsoleEventHandlers()
{
    /* Destroy console event-handler: */
    delete m_pConsoleEventhandler;
    m_pConsoleEventhandler = 0;
}

void UISession::cleanupNotificationCenter()
{
    UINotificationCenter::destroy();
}

void UISession::cleanupSession()
{
    /* Detach debugger: */
    if (!m_debugger.isNull())
        m_debugger.detach();

    /* Detach keyboard: */
    if (!m_keyboard.isNull())
        m_keyboard.detach();

    /* Detach mouse: */
    if (!m_mouse.isNull())
        m_mouse.detach();

    /* Detach guest: */
    if (!m_guest.isNull())
        m_guest.detach();

    /* Detach display: */
    if (!m_display.isNull())
        m_display.detach();

    /* Detach console: */
    if (!m_console.isNull())
        m_console.detach();

    /* Detach machine: */
    if (!m_machine.isNull())
        m_machine.detach();

    /* Close session: */
    if (!m_session.isNull() && uiCommon().isVBoxSVCAvailable())
    {
        m_session.UnlockMachine();
        m_session.detach();
    }
}

bool UISession::preprocessInitialization()
{
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
    foreach (const CHostNetworkInterface &iface, uiCommon().host().GetNetworkInterfaces())
    {
        availableInterfaceNames << iface.GetName();
        availableInterfaceNames << iface.GetShortName();
    }

    ulong cCount = uiCommon().virtualBox().GetSystemProperties().GetMaxNetworkAdapters(machine().GetChipsetType());
    for (ulong uAdapterIndex = 0; uAdapterIndex < cCount; ++uAdapterIndex)
    {
        CNetworkAdapter na = machine().GetNetworkAdapter(uAdapterIndex);

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
#ifndef VBOX_WITH_VMNET
                case KNetworkAttachmentType_HostOnly:
                    strIfName = na.GetHostOnlyInterface();
                    break;
#endif /* !VBOX_WITH_VMNET */
                default: break; /* Shut up, MSC! */
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
        if (msgCenter().warnAboutNetworkInterfaceNotFound(machineName(), failedInterfaceNames.join(", ")))
            machineLogic()->openNetworkSettingsDialog();
        else
        {
            LogRel(("GUI: Aborting startup due to preprocess initialization issue detected...\n"));
            return false;
        }
    }
#endif /* VBOX_WITH_NETFLT */

    /* Check for USB enumeration warning. Don't return false even if we have a warning: */
    CHost comHost = uiCommon().host();
    if (comHost.GetUSBDevices().isEmpty() && comHost.isWarning())
    {
        /* Do not bitch if USB disabled: */
        if (!machine().GetUSBControllers().isEmpty())
        {
            /* Do not bitch if there are no filters (check if enabled too?): */
            if (!machine().GetUSBDeviceFilters().GetDeviceFilters().isEmpty())
                UINotificationMessage::cannotEnumerateHostUSBDevices(comHost);
        }
    }

    /* True by default: */
    return true;
}

bool UISession::mountAdHocImage(KDeviceType enmDeviceType, UIMediumDeviceType enmMediumType, const QString &strMediumName)
{
    /* Get VBox: */
    CVirtualBox comVBox = uiCommon().virtualBox();

    /* Prepare medium to mount: */
    UIMedium guiMedium;

    /* The 'none' medium name means ejecting what ever is in the drive,
     * in that case => leave the guiMedium variable null. */
    if (strMediumName != "none")
    {
        /* Open the medium: */
        const CMedium comMedium = comVBox.OpenMedium(strMediumName, enmDeviceType, KAccessMode_ReadWrite, false /* fForceNewUuid */);
        if (!comVBox.isOk() || comMedium.isNull())
        {
            UINotificationMessage::cannotOpenMedium(comVBox, strMediumName);
            return false;
        }

        /* Make sure medium ID is valid: */
        const QUuid uMediumId = comMedium.GetId();
        AssertReturn(!uMediumId.isNull(), false);

        /* Try to find UIMedium among cached: */
        guiMedium = uiCommon().medium(uMediumId);
        if (guiMedium.isNull())
        {
            /* Cache new one if necessary: */
            guiMedium = UIMedium(comMedium, enmMediumType, KMediumState_Created);
            uiCommon().createMedium(guiMedium);
        }
    }

    /* Search for a suitable storage slots: */
    QList<ExactStorageSlot> aFreeStorageSlots;
    QList<ExactStorageSlot> aBusyStorageSlots;
    foreach (const CStorageController &comController, machine().GetStorageControllers())
    {
        foreach (const CMediumAttachment &comAttachment, machine().GetMediumAttachmentsOfController(comController.GetName()))
        {
            /* Look for an optical devices only: */
            if (comAttachment.GetType() == enmDeviceType)
            {
                /* Append storage slot to corresponding list: */
                if (comAttachment.GetMedium().isNull())
                    aFreeStorageSlots << ExactStorageSlot(comController.GetName(), comController.GetBus(),
                                                          comAttachment.GetPort(), comAttachment.GetDevice());
                else
                    aBusyStorageSlots << ExactStorageSlot(comController.GetName(), comController.GetBus(),
                                                          comAttachment.GetPort(), comAttachment.GetDevice());
            }
        }
    }

    /* Make sure at least one storage slot found: */
    QList<ExactStorageSlot> sStorageSlots = aFreeStorageSlots + aBusyStorageSlots;
    if (sStorageSlots.isEmpty())
    {
        UINotificationMessage::cannotMountImage(machineName(), strMediumName);
        return false;
    }

    /* Try to mount medium into first available storage slot: */
    while (!sStorageSlots.isEmpty())
    {
        const ExactStorageSlot storageSlot = sStorageSlots.takeFirst();
        machine().MountMedium(storageSlot.controller, storageSlot.port, storageSlot.device, guiMedium.medium(), false /* force */);
        if (machine().isOk())
            break;
    }

    /* Show error message if necessary: */
    if (!machine().isOk())
    {
        msgCenter().cannotRemountMedium(machine(), guiMedium, true /* mount? */, false /* retry? */, activeMachineWindow());
        return false;
    }

    /* Save machine settings: */
    machine().SaveSettings();

    /* Show error message if necessary: */
    if (!machine().isOk())
    {
        UINotificationMessage::cannotSaveMachineSettings(machine());
        return false;
    }

    /* True by default: */
    return true;
}

bool UISession::postprocessInitialization()
{
    /* There used to be some raw-mode warnings here for raw-mode incompatible
       guests (64-bit ones and OS/2).  Nothing to do at present. */
    return true;
}

CMediumVector UISession::machineMedia() const
{
    CMediumVector comMedia;
    /* Enumerate all the controllers: */
    foreach (const CStorageController &comController, m_machine.GetStorageControllers())
    {
        /* Enumerate all the attachments: */
        foreach (const CMediumAttachment &comAttachment, m_machine.GetMediumAttachmentsOfController(comController.GetName()))
        {
            /* Skip unrelated device types: */
            const KDeviceType enmDeviceType = comAttachment.GetType();
            if (   enmDeviceType != KDeviceType_HardDisk
                && enmDeviceType != KDeviceType_Floppy
                && enmDeviceType != KDeviceType_DVD)
                continue;
            if (   comAttachment.GetIsEjected()
                || comAttachment.GetMedium().isNull())
                continue;
            comMedia.append(comAttachment.GetMedium());
        }
    }
    return comMedia;
}

bool UISession::prepareToBeSaved()
{
    return    isPaused()
           || (isRunning() && pause());
}

bool UISession::prepareToBeShutdowned()
{
    const bool fValidMode = console().GetGuestEnteredACPIMode();
    if (!fValidMode)
        UINotificationMessage::cannotSendACPIToMachine();
    return fValidMode;
}

void UISession::loadVMSettings()
{
    /* Cache IMachine::ExecutionEngine value. */
    m_enmVMExecutionEngine = m_debugger.GetExecutionEngine();
    /* Load nested-paging CPU hardware virtualization extension: */
    m_fIsHWVirtExNestedPagingEnabled = m_debugger.GetHWVirtExNestedPagingEnabled();
    /* Load whether the VM is currently making use of the unrestricted execution feature of VT-x: */
    m_fIsHWVirtExUXEnabled = m_debugger.GetHWVirtExUXEnabled();
    /* Load VM's effective paravirtualization provider: */
    m_paraVirtProvider = m_machine.GetEffectiveParavirtProvider();
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
    /* Only SIGUSR1 is interesting: */
    if (sig == SIGUSR1)
        if (gpMachine)
            gpMachine->uisession()->machineLogic()->keyboardHandler()->releaseAllPressedKeys();
}
#endif /* VBOX_GUI_WITH_KEYS_RESET_HANDLER */
