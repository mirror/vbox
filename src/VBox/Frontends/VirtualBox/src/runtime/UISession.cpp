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
#include "UIActionPoolRuntime.h"
#include "UICommon.h"
#include "UIConsoleEventHandler.h"
#include "UIDetailsGenerator.h"
#include "UIExtraDataManager.h"
#include "UIFrameBuffer.h"
#include "UIMachine.h"
#include "UIMachineLogic.h"
#include "UIMachineView.h"
#include "UIMachineWindow.h"
#include "UIMedium.h"
#include "UIMessageCenter.h"
#include "UIMousePointerShapeData.h"
#include "UINotificationCenter.h"
#include "UISession.h"
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

/* Other VBox includes: */
#ifdef VBOX_WITH_DEBUGGER_GUI
# include <VBox/dbggui.h>
# include <iprt/ldr.h>
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

    m_enmMachineState = machine().GetState();

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

    /* Fetch corresponding states: */
    if (uiCommon().isSeparateProcess())
    {
        sltAdditionsChange();
    }
    machineLogic()->initializePostPowerUp();

#ifdef VBOX_GUI_WITH_PIDFILE
    uiCommon().createPidfile();
#endif /* VBOX_GUI_WITH_PIDFILE */

    /* True by default: */
    return true;
}

bool UISession::powerUp()
{
    /* Power UP machine: */
    CProgress comProgress = uiCommon().shouldStartPaused() ? console().PowerUpPaused() : console().PowerUp();

    /* Check for immediate failure: */
    if (!console().isOk() || comProgress.isNull())
    {
        if (uiCommon().showStartVMErrors())
            msgCenter().cannotStartMachine(console(), machineName());
        LogRel(("GUI: Aborting startup due to power up issue detected...\n"));
        return false;
    }

    /* Some logging right after we powered up: */
    LogRel(("GUI: Qt version: %s\n", UICommon::qtRTVersionString().toUtf8().constData()));
#ifdef VBOX_WS_X11
    LogRel(("GUI: X11 Window Manager code: %d\n", (int)uiCommon().typeOfWindowManager()));
#endif
#if defined(VBOX_WS_MAC) || defined(VBOX_WS_WIN)
    LogRel(("GUI: HID LEDs sync is %s\n", uimachine()->isHidLedsSyncEnabled() ? "enabled" : "disabled"));
#else
    LogRel(("GUI: HID LEDs sync is not supported on this platform\n"));
#endif

    /* Enable 'manual-override',
     * preventing automatic Runtime UI closing
     * and visual representation mode changes: */
    uimachine()->setManualOverrideMode(true);

    /* Show "Starting/Restoring" progress dialog: */
    if (isSaved())
    {
        msgCenter().showModalProgressDialog(comProgress, machineName(), ":/progress_state_restore_90px.png", 0, 0);
        /* After restoring from 'saved' state, machine-window(s) geometry should be adjusted: */
        machineLogic()->adjustMachineWindowsGeometry();
    }
    else
    {
#ifdef VBOX_IS_QT6_OR_LATER /** @todo why is this any problem on qt6? */
        msgCenter().showModalProgressDialog(comProgress, machineName(), ":/progress_start_90px.png", 0, 0);
#else
        msgCenter().showModalProgressDialog(comProgress, machineName(), ":/progress_start_90px.png");
#endif
        /* After VM start, machine-window(s) size-hint(s) should be sent: */
        machineLogic()->sendMachineWindowsSizeHints();
    }

    /* Check for progress failure: */
    if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
    {
        if (uiCommon().showStartVMErrors())
            msgCenter().cannotStartMachine(comProgress, machineName());
        LogRel(("GUI: Aborting startup due to power up progress issue detected...\n"));
        return false;
    }

    /* Disable 'manual-override' finally: */
    uimachine()->setManualOverrideMode(false);

    /* True by default: */
    return true;
}

WId UISession::mainMachineWindowId() const
{
    return mainMachineWindow() ? mainMachineWindow()->winId() : 0;
}

bool UISession::setPause(bool fPause)
{
    CConsole comConsole = console();
    if (fPause)
        comConsole.Pause();
    else
        comConsole.Resume();
    const bool fSuccess = comConsole.isOk();
    if (!fSuccess)
    {
        if (fPause)
            UINotificationMessage::cannotPauseMachine(comConsole);
        else
            UINotificationMessage::cannotResumeMachine(comConsole);
    }
    return fSuccess;
}

bool UISession::putScancode(LONG iCode)
{
    CKeyboard comKeyboard = keyboard();
    comKeyboard.PutScancode(iCode);
    const bool fSuccess = comKeyboard.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotChangeKeyboardParameter(comKeyboard);
    return fSuccess;
}

bool UISession::putScancodes(const QVector<LONG> &codes)
{
    CKeyboard comKeyboard = keyboard();
    comKeyboard.PutScancodes(codes);
    const bool fSuccess = comKeyboard.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotChangeKeyboardParameter(comKeyboard);
    return fSuccess;
}

bool UISession::putCAD()
{
    CKeyboard comKeyboard = keyboard();
    comKeyboard.PutCAD();
    const bool fSuccess = comKeyboard.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotChangeKeyboardParameter(comKeyboard);
    return fSuccess;
}

bool UISession::releaseKeys()
{
    CKeyboard comKeyboard = keyboard();
    comKeyboard.ReleaseKeys();
    const bool fSuccess = comKeyboard.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotChangeKeyboardParameter(comKeyboard);
    return fSuccess;
}

bool UISession::putUsageCode(LONG iUsageCode, LONG iUsagePage, bool fKeyRelease)
{
    CKeyboard comKeyboard = keyboard();
    comKeyboard.PutUsageCode(iUsageCode, iUsagePage, fKeyRelease);
    const bool fSuccess = comKeyboard.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotChangeKeyboardParameter(comKeyboard);
    return fSuccess;
}

bool UISession::getAbsoluteSupported()
{
    return mouse().GetAbsoluteSupported();
}

bool UISession::getRelativeSupported()
{
    return mouse().GetRelativeSupported();
}

bool UISession::getTouchScreenSupported()
{
    return mouse().GetTouchScreenSupported();
}

bool UISession::getTouchPadSupported()
{
    return mouse().GetTouchPadSupported();
}

bool UISession::getNeedsHostCursor()
{
    return mouse().GetNeedsHostCursor();
}

bool UISession::putMouseEvent(long iDx, long iDy, long iDz, long iDw, long iButtonState)
{
    CMouse comMouse = mouse();
    comMouse.PutMouseEvent(iDx, iDy, iDz, iDw, iButtonState);
    const bool fSuccess = comMouse.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotChangeMouseParameter(comMouse);
    return fSuccess;
}

bool UISession::putMouseEventAbsolute(long iX, long iY, long iDz, long iDw, long iButtonState)
{
    CMouse comMouse = mouse();
    comMouse.PutMouseEventAbsolute(iX, iY, iDz, iDw, iButtonState);
    const bool fSuccess = comMouse.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotChangeMouseParameter(comMouse);
    return fSuccess;
}

bool UISession::putEventMultiTouch(long iCount, const QVector<LONG64> &contacts, bool fIsTouchScreen, ulong uScanTime)
{
    CMouse comMouse = mouse();
    comMouse.PutEventMultiTouch(iCount, contacts, fIsTouchScreen, uScanTime);
    const bool fSuccess = comMouse.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotChangeMouseParameter(comMouse);
    return fSuccess;
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
    if (m_ulGuestAdditionsRunLevel < (ulong)KAdditionsRunLevelType_Userland)
        return false;

    return true;
}

UIFrameBuffer *UISession::frameBuffer(ulong uScreenId) const
{
    Assert(uScreenId < (ulong)m_frameBufferVector.size());
    return m_frameBufferVector.value((int)uScreenId, 0);
}

void UISession::setFrameBuffer(ulong uScreenId, UIFrameBuffer *pFrameBuffer)
{
    Assert(uScreenId < (ulong)m_frameBufferVector.size());
    if (uScreenId < (ulong)m_frameBufferVector.size())
        m_frameBufferVector[(int)uScreenId] = pFrameBuffer;
}

QSize UISession::frameBufferSize(ulong uScreenId) const
{
    UIFrameBuffer *pFramebuffer = frameBuffer(uScreenId);
    return pFramebuffer ? QSize(pFramebuffer->width(), pFramebuffer->height()) : QSize();
}

bool UISession::acquireGuestScreenParameters(ulong uScreenId,
                                             ulong &uWidth, ulong &uHeight, ulong &uBitsPerPixel,
                                             long &xOrigin, long &yOrigin, KGuestMonitorStatus &enmMonitorStatus)
{
    CDisplay comDisplay = display();
    ULONG uGuestWidth = 0, uGuestHeight = 0, uGuestBitsPerPixel = 0;
    LONG iGuestXOrigin = 0, iGuestYOrigin = 0;
    KGuestMonitorStatus enmGuestMonitorStatus = KGuestMonitorStatus_Disabled;
    comDisplay.GetScreenResolution(uScreenId, uGuestWidth, uGuestHeight, uGuestBitsPerPixel,
                                   iGuestXOrigin, iGuestYOrigin, enmGuestMonitorStatus);
    const bool fSuccess = comDisplay.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotAcquireDisplayParameter(comDisplay);
    uWidth = uGuestWidth;
    uHeight = uGuestHeight;
    uBitsPerPixel = uGuestBitsPerPixel;
    xOrigin = iGuestXOrigin;
    yOrigin = iGuestYOrigin;
    enmMonitorStatus = enmGuestMonitorStatus;
    return fSuccess;
}

bool UISession::setVideoModeHint(ulong uScreenId, bool fEnabled, bool fChangeOrigin, long xOrigin, long yOrigin,
                                 ulong uWidth, ulong uHeight, ulong uBitsPerPixel, bool fNotify)
{
    CDisplay comDisplay = display();
    comDisplay.SetVideoModeHint(uScreenId, fEnabled, fChangeOrigin, xOrigin, yOrigin,
                                uWidth, uHeight, uBitsPerPixel, fNotify);
    const bool fSuccess = comDisplay.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotChangeDisplayParameter(comDisplay);
    return fSuccess;
}

bool UISession::acquireVideoModeHint(ulong uScreenId, bool &fEnabled, bool &fChangeOrigin,
                                     long &xOrigin, long &yOrigin, ulong &uWidth, ulong &uHeight,
                                     ulong &uBitsPerPixel)
{
    CDisplay comDisplay = display();
    BOOL fGuestEnabled = false, fGuestChangeOrigin = false;
    LONG iGuestXOrigin = 0, iGuestYOrigin = 0;
    ULONG uGuestWidth = 0, uGuestHeight = 0, uGuestBitsPerPixel = 0;
    comDisplay.GetVideoModeHint(uScreenId, fGuestEnabled, fGuestChangeOrigin,
                                iGuestXOrigin, iGuestYOrigin, uGuestWidth, uGuestHeight,
                                uGuestBitsPerPixel);
    const bool fSuccess = comDisplay.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotAcquireDisplayParameter(comDisplay);
    fEnabled = fGuestEnabled;
    fChangeOrigin = fGuestChangeOrigin;
    xOrigin = iGuestXOrigin;
    yOrigin = iGuestYOrigin;
    uWidth = uGuestWidth;
    uHeight = uGuestHeight;
    uBitsPerPixel = uGuestBitsPerPixel;
    return fSuccess;
}

bool UISession::acquireScreenShot(ulong uScreenId, ulong uWidth, ulong uHeight, KBitmapFormat enmFormat, uchar *pBits)
{
    CDisplay comDisplay = display();
    bool fSuccess = false;
    /* For separate process: */
    if (uiCommon().isSeparateProcess())
    {
        /* Take screen-data to array first: */
        const QVector<BYTE> screenData = comDisplay.TakeScreenShotToArray(uScreenId, uWidth, uHeight, enmFormat);
        fSuccess = comDisplay.isOk();
        if (!fSuccess)
            UINotificationMessage::cannotAcquireDisplayParameter(comDisplay);
        else
        {
            /* And copy that data to screen-shot if it is Ok: */
            if (!screenData.isEmpty())
                memcpy(pBits, screenData.data(), uWidth * uHeight * 4);
        }
    }
    /* For the same process: */
    else
    {
        /* Take the screen-shot directly: */
        comDisplay.TakeScreenShot(uScreenId, pBits, uWidth, uHeight, enmFormat);
        fSuccess = comDisplay.isOk();
        if (!fSuccess)
            UINotificationMessage::cannotAcquireDisplayParameter(comDisplay);
    }
    return fSuccess;
}

bool UISession::notifyScaleFactorChange(ulong uScreenId, ulong uScaleFactorWMultiplied, ulong uScaleFactorHMultiplied)
{
    CDisplay comDisplay = display();
    comDisplay.NotifyScaleFactorChange(uScreenId, uScaleFactorWMultiplied, uScaleFactorHMultiplied);
    const bool fSuccess = comDisplay.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotChangeDisplayParameter(comDisplay);
    return fSuccess;
}

bool UISession::notifyHiDPIOutputPolicyChange(bool fUnscaledHiDPI)
{
    CDisplay comDisplay = display();
    comDisplay.NotifyHiDPIOutputPolicyChange(fUnscaledHiDPI);
    const bool fSuccess = comDisplay.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotChangeDisplayParameter(comDisplay);
    return fSuccess;
}

bool UISession::setSeamlessMode(bool fEnabled)
{
    CDisplay comDisplay = display();
    comDisplay.SetSeamlessMode(fEnabled);
    const bool fSuccess = comDisplay.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotChangeDisplayParameter(comDisplay);
    return fSuccess;
}

bool UISession::viewportChanged(ulong uScreenId, ulong xOrigin, ulong yOrigin, ulong uWidth, ulong uHeight)
{
    CDisplay comDisplay = display();
    comDisplay.ViewportChanged(uScreenId, xOrigin, yOrigin, uWidth, uHeight);
    const bool fSuccess = comDisplay.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotChangeDisplayParameter(comDisplay);
    return fSuccess;
}

bool UISession::invalidateAndUpdate()
{
    CDisplay comDisplay = display();
    comDisplay.InvalidateAndUpdate();
    const bool fSuccess = comDisplay.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotChangeDisplayParameter(comDisplay);
    return fSuccess;
}

bool UISession::invalidateAndUpdateScreen(ulong uScreenId)
{
    CDisplay comDisplay = display();
    comDisplay.InvalidateAndUpdateScreen(uScreenId);
    const bool fSuccess = comDisplay.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotChangeDisplayParameter(comDisplay);
    return fSuccess;
}

bool UISession::acquireDeviceActivity(const QVector<KDeviceType> &deviceTypes, QVector<KDeviceActivity> &states)
{
    CConsole comConsole = console();
    states = comConsole.GetDeviceActivity(deviceTypes);
    const bool fSuccess = comConsole.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotAcquireConsoleParameter(comConsole);
    return fSuccess;
}

void UISession::acquireHardDiskStatusInfo(QString &strInfo, bool &fAttachmentsPresent)
{
    CMachine comMachine = machine();
    UIDetailsGenerator::acquireHardDiskStatusInfo(comMachine, strInfo, fAttachmentsPresent);
}

void UISession::acquireOpticalDiskStatusInfo(QString &strInfo, bool &fAttachmentsPresent, bool &fAttachmentsMounted)
{
    CMachine comMachine = machine();
    UIDetailsGenerator::acquireOpticalDiskStatusInfo(comMachine, strInfo, fAttachmentsPresent, fAttachmentsMounted);
}

void UISession::acquireFloppyDiskStatusInfo(QString &strInfo, bool &fAttachmentsPresent, bool &fAttachmentsMounted)
{
    CMachine comMachine = machine();
    UIDetailsGenerator::acquireFloppyDiskStatusInfo(comMachine, strInfo, fAttachmentsPresent, fAttachmentsMounted);
}

void UISession::acquireAudioStatusInfo(QString &strInfo, bool &fAudioEnabled, bool &fEnabledOutput, bool &fEnabledInput)
{
    CMachine comMachine = machine();
    UIDetailsGenerator::acquireAudioStatusInfo(comMachine, strInfo, fAudioEnabled, fEnabledOutput, fEnabledInput);
}

void UISession::acquireNetworkStatusInfo(QString &strInfo, bool &fAdaptersPresent, bool &fCablesDisconnected)
{
    CMachine comMachine = machine();
    UIDetailsGenerator::acquireNetworkStatusInfo(comMachine, strInfo, fAdaptersPresent, fCablesDisconnected);
}

void UISession::acquireUsbStatusInfo(QString &strInfo, bool &fUsbEnableds)
{
    CMachine comMachine = machine();
    CConsole comConsole = console();
    UIDetailsGenerator::acquireUsbStatusInfo(comMachine, comConsole, strInfo, fUsbEnableds);
}

void UISession::acquireSharedFoldersStatusInfo(QString &strInfo, bool &fFoldersPresent)
{
    CMachine comMachine = machine();
    CConsole comConsole = console();
    CGuest comGuest = guest();
    UIDetailsGenerator::acquireSharedFoldersStatusInfo(comMachine, comConsole, comGuest, strInfo, fFoldersPresent);
}

void UISession::acquireDisplayStatusInfo(QString &strInfo, bool &fAcceleration3D)
{
    CMachine comMachine = machine();
    UIDetailsGenerator::acquireDisplayStatusInfo(comMachine, strInfo, fAcceleration3D);
}

void UISession::acquireRecordingStatusInfo(QString &strInfo, bool &fRecordingEnabled, bool &fMachinePaused)
{
    CMachine comMachine = machine();
    fMachinePaused = isPaused();
    UIDetailsGenerator::acquireRecordingStatusInfo(comMachine, strInfo, fRecordingEnabled);
}

void UISession::acquireFeaturesStatusInfo(QString &strInfo, KVMExecutionEngine &enmEngine,
                                          bool fNestedPagingEnabled, bool fUxEnabled,
                                          KParavirtProvider enmProvider)
{
    CMachine comMachine = machine();
    UIDetailsGenerator::acquireFeaturesStatusInfo(comMachine, strInfo,
                                                  enmEngine,
                                                  fNestedPagingEnabled, fUxEnabled,
                                                  enmProvider);
}

bool UISession::setLogEnabled(bool fEnabled)
{
    CMachineDebugger comDebugger = debugger();
    comDebugger.SetLogEnabled(fEnabled ? TRUE : FALSE);
    const bool fSuccess = comDebugger.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotChangeMachineDebuggerParameter(comDebugger);
    return fSuccess;
}

bool UISession::acquireWhetherLogEnabled(bool &fEnabled)
{
    CMachineDebugger comDebugger = debugger();
    const BOOL fLogEnabled = comDebugger.GetLogEnabled();
    const bool fSuccess = comDebugger.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotAcquireMachineDebuggerParameter(comDebugger);
    else
        fEnabled = fLogEnabled == TRUE;
    return fSuccess;
}

bool UISession::acquireExecutionEngineType(KVMExecutionEngine &enmType)
{
    CMachineDebugger comDebugger = debugger();
    const KVMExecutionEngine enmEngineType = comDebugger.GetExecutionEngine();
    const bool fSuccess = comDebugger.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotAcquireMachineDebuggerParameter(comDebugger);
    else
        enmType = enmEngineType;
    return fSuccess;
}

bool UISession::acquireWhetherHwVirtExNestedPagingEnabled(bool &fEnabled)
{
    CMachineDebugger comDebugger = debugger();
    const BOOL fFeatureEnabled = comDebugger.GetHWVirtExNestedPagingEnabled();
    const bool fSuccess = comDebugger.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotAcquireMachineDebuggerParameter(comDebugger);
    else
        fEnabled = fFeatureEnabled == TRUE;
    return fSuccess;
}

bool UISession::acquireWhetherHwVirtExUXEnabled(bool &fEnabled)
{
    CMachineDebugger comDebugger = debugger();
    const BOOL fFeatureEnabled = comDebugger.GetHWVirtExUXEnabled();
    const bool fSuccess = comDebugger.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotAcquireMachineDebuggerParameter(comDebugger);
    else
        fEnabled = fFeatureEnabled == TRUE;
    return fSuccess;
}

bool UISession::acquireEffectiveCPULoad(ulong &uLoad)
{
    CMachineDebugger comDebugger = debugger();
    ULONG uPctExecuting;
    ULONG uPctHalted;
    ULONG uPctOther;
    comDebugger.GetCPULoad(0x7fffffff, uPctExecuting, uPctHalted, uPctOther);
    const bool fSuccess = comDebugger.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotAcquireMachineDebuggerParameter(comDebugger);
    else
        uLoad = uPctExecuting + uPctOther;
    return fSuccess;
}

#ifdef VBOX_WITH_DEBUGGER_GUI
bool UISession::dbgCreated(void *pActionDebug)
{
    if (m_pDbgGui)
        return true;

    RTLDRMOD hLdrMod = uiCommon().getDebuggerModule();
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
                m_pDbgGuiVT->pfnSetMenu(m_pDbgGui, pActionDebug);
                dbgAdjustRelativePos();
                return true;
            }

            LogRel(("GUI: DBGGuiCreate failed, incompatible versions (loaded %#x/%#x, expected %#x)\n",
                    m_pDbgGuiVT->u32Version, m_pDbgGuiVT->u32EndVersion, DBGGUIVT_VERSION));
        }
        else
            LogRel(("GUI: DBGGuiCreate failed, rc=%Rrc\n", rc));
    }
    else
        LogRel(("GUI: RTLdrGetSymbol(,\"DBGGuiCreate\",) -> %Rrc\n", rc));

    m_pDbgGui = 0;
    m_pDbgGuiVT = 0;
    return false;
}

void UISession::dbgDestroy()
{
    if (m_pDbgGui)
    {
        m_pDbgGuiVT->pfnDestroy(m_pDbgGui);
        m_pDbgGui = 0;
        m_pDbgGuiVT = 0;
    }
}

void UISession::dbgShowStatistics()
{
    const QByteArray &expandBytes = uiCommon().getDebuggerStatisticsExpand().toUtf8();
    const QByteArray &filterBytes = uiCommon().getDebuggerStatisticsFilter().toUtf8();
    m_pDbgGuiVT->pfnShowStatistics(m_pDbgGui, filterBytes.constData(), expandBytes.constData());
}

void UISession::dbgShowCommandLine()
{
    m_pDbgGuiVT->pfnShowCommandLine(m_pDbgGui);
}

void UISession::dbgAdjustRelativePos()
{
    if (m_pDbgGui)
    {
        const QRect rct = activeMachineWindow()->frameGeometry();
        m_pDbgGuiVT->pfnAdjustRelativePos(m_pDbgGui, rct.x(), rct.y(), rct.width(), rct.height());
    }
}
#endif /* VBOX_WITH_DEBUGGER_GUI */

bool UISession::acquireWhetherGuestEnteredACPIMode(bool &fEntered)
{
    CConsole comConsole = console();
    const BOOL fGuestEntered = comConsole.GetGuestEnteredACPIMode();
    const bool fSuccess = comConsole.isOk();
    if (!fSuccess)
        UINotificationMessage::cannotAcquireConsoleParameter(comConsole);
    else
        fEntered = fGuestEntered == TRUE;
    return fSuccess;
}

bool UISession::prepareToBeSaved()
{
    return    isPaused()
           || (isRunning() && pause());
}

bool UISession::prepareToBeShutdowned()
{
    bool fValidMode = false;
    acquireWhetherGuestEnteredACPIMode(fValidMode);
    if (!fValidMode)
        UINotificationMessage::cannotSendACPIToMachine();
    return fValidMode;
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

void UISession::sltStateChange(KMachineState enmState)
{
    /* Check if something had changed: */
    if (m_enmMachineState != enmState)
    {
        /* Store new data: */
        m_enmMachineStatePrevious = m_enmMachineState;
        m_enmMachineState = enmState;

        /* Notify listeners about machine state changed: */
        emit sigMachineStateChange();
    }
}

void UISession::sltAdditionsChange()
{
    /* Acquire actual states: */
    const ulong ulGuestAdditionsRunLevel = guest().GetAdditionsRunLevel();
    LONG64 iLastUpdatedIgnored;
    const bool fIsGuestSupportsGraphics = guest().GetFacilityStatus(KAdditionsFacilityType_Graphics, iLastUpdatedIgnored)
                                        == KAdditionsFacilityStatus_Active;
    const bool fIsGuestSupportsSeamless = guest().GetFacilityStatus(KAdditionsFacilityType_Seamless, iLastUpdatedIgnored)
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
    , m_enmMachineStatePrevious(KMachineState_Null)
    , m_enmMachineState(KMachineState_Null)
    /* Guest additions flags: */
    , m_ulGuestAdditionsRunLevel(0)
    , m_fIsGuestSupportsGraphics(false)
    , m_fIsGuestSupportsSeamless(false)
#ifdef VBOX_WITH_DEBUGGER_GUI
    /* Debug UI stuff: */
    , m_pDbgGui(0)
    , m_pDbgGuiVT(0)
#endif /* VBOX_WITH_DEBUGGER_GUI */
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

    /* Cache media early if requested: */
    if (uiCommon().agressiveCaching())
        recacheMachineMedia();

    /* Prepare GUI stuff: */
    prepareNotificationCenter();
    prepareConsoleEventHandlers();
    prepareFramebuffers();
    prepareConnections();
    prepareSignalHandling();

    /* True by default: */
    return true;
}

bool UISession::prepareSession()
{
    /* Open session: */
    m_comSession = uiCommon().openSession(uiCommon().managedVMUuid(),
                                            uiCommon().isSeparateProcess()
                                          ? KLockType_Shared
                                          : KLockType_VM);
    if (m_comSession.isNull())
        return false;

    /* Get machine: */
    m_comMachine = m_comSession.GetMachine();
    if (m_comMachine.isNull())
        return false;

    /* Get console: */
    m_comConsole = m_comSession.GetConsole();
    if (m_comConsole.isNull())
        return false;

    /* Get display: */
    m_comDisplay = m_comConsole.GetDisplay();
    if (m_comDisplay.isNull())
        return false;

    /* Get guest: */
    m_comGuest = m_comConsole.GetGuest();
    if (m_comGuest.isNull())
        return false;

    /* Get mouse: */
    m_comMouse = m_comConsole.GetMouse();
    if (m_comMouse.isNull())
        return false;

    /* Get keyboard: */
    m_comKeyboard = m_comConsole.GetKeyboard();
    if (m_comKeyboard.isNull())
        return false;

    /* Get debugger: */
    m_comDebugger = m_comConsole.GetDebugger();
    if (m_comDebugger.isNull())
        return false;

    /* Update machine-name: */
    m_strMachineName = machine().GetName();

    /* Update machine-state: */
    m_enmMachineState = machine().GetState();

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
    if (!m_comDebugger.isNull())
        m_comDebugger.detach();

    /* Detach keyboard: */
    if (!m_comKeyboard.isNull())
        m_comKeyboard.detach();

    /* Detach mouse: */
    if (!m_comMouse.isNull())
        m_comMouse.detach();

    /* Detach guest: */
    if (!m_comGuest.isNull())
        m_comGuest.detach();

    /* Detach display: */
    if (!m_comDisplay.isNull())
        m_comDisplay.detach();

    /* Detach console: */
    if (!m_comConsole.isNull())
        m_comConsole.detach();

    /* Detach machine: */
    if (!m_comMachine.isNull())
        m_comMachine.detach();

    /* Close session: */
    if (!m_comSession.isNull() && uiCommon().isVBoxSVCAvailable())
    {
        m_comSession.UnlockMachine();
        m_comSession.detach();
    }
}

UIMachineLogic *UISession::machineLogic() const
{
    return uimachine() ? uimachine()->machineLogic() : 0;
}

UIMachineWindow *UISession::activeMachineWindow() const
{
    return machineLogic() ? machineLogic()->activeMachineWindow() : 0;
}

QWidget *UISession::mainMachineWindow() const
{
    return machineLogic() ? machineLogic()->mainMachineWindow() : 0;
}

bool UISession::preprocessInitialization()
{
#ifdef VBOX_WITH_NETFLT
    /* Skip network interface name checks if VM in saved state: */
    if (!isSaved())
    {
        /* Make sure all the attached and enabled network
         * adapters are present on the host. This check makes sense
         * in two cases only - when attachement type is Bridged Network
         * or Host-only Interface. NOTE: Only currently enabled
         * attachement type is checked (incorrect parameters check for
         * currently disabled attachement types is skipped). */
        QStringList failedInterfaceNames;
        QStringList availableInterfaceNames;

        /* Create host network interface names list: */
        foreach (const CHostNetworkInterface &comNetIface, uiCommon().host().GetNetworkInterfaces())
        {
            availableInterfaceNames << comNetIface.GetName();
            availableInterfaceNames << comNetIface.GetShortName();
        }

        /* Enumerate all the virtual network adapters: */
        const ulong cCount = uiCommon().virtualBox().GetSystemProperties().GetMaxNetworkAdapters(machine().GetChipsetType());
        for (ulong uAdapterIndex = 0; uAdapterIndex < cCount; ++uAdapterIndex)
        {
            CNetworkAdapter comNetworkAdapter = machine().GetNetworkAdapter(uAdapterIndex);
            if (comNetworkAdapter.GetEnabled())
            {
                /* Get physical network interface name for
                 * currently enabled network attachement type: */
                QString strInterfaceName;
                switch (comNetworkAdapter.GetAttachmentType())
                {
                    case KNetworkAttachmentType_Bridged:
                        strInterfaceName = comNetworkAdapter.GetBridgedInterface();
                        break;
#ifndef VBOX_WITH_VMNET
                    case KNetworkAttachmentType_HostOnly:
                        strInterfaceName = comNetworkAdapter.GetHostOnlyInterface();
                        break;
#endif /* !VBOX_WITH_VMNET */
                    default:
                        break;
                }

                if (   !strInterfaceName.isEmpty()
                    && !availableInterfaceNames.contains(strInterfaceName))
                {
                    LogRel(("GUI: Invalid network interface found: %s\n", strInterfaceName.toUtf8().constData()));
                    failedInterfaceNames << QString("%1 (adapter %2)").arg(strInterfaceName).arg(uAdapterIndex + 1);
                }
            }
        }

        /* Check if non-existent interfaces found: */
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

void UISession::recacheMachineMedia()
{
    /* Compose a list of machine media: */
    CMediumVector comMedia;

    /* Enumerate all the controllers: */
    foreach (const CStorageController &comController, machine().GetStorageControllers())
    {
        /* Enumerate all the attachments: */
        foreach (const CMediumAttachment &comAttachment, machine().GetMediumAttachmentsOfController(comController.GetName()))
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

    /* Start media enumeration: */
    uiCommon().enumerateMedia(comMedia);
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
            gpMachine->machineLogic()->keyboardHandler()->releaseAllPressedKeys();
}
#endif /* VBOX_GUI_WITH_KEYS_RESET_HANDLER */
