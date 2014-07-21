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

/* Qt includes: */
#include <QDesktopWidget>
#include <QDir>
#include <QFileInfo>
#include <QImageWriter>
#include <QPainter>
#include <QTimer>
#include <QDateTime>
#ifdef Q_WS_MAC
# include <QMenuBar>
#endif /* Q_WS_MAC */

/* GUI includes: */
#include "QIFileDialog.h"
#include "UIActionPoolRuntime.h"
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
# include "UINetworkManager.h"
# include "UIDownloaderAdditions.h"
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
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
#include "UIPopupCenter.h"
#include "VBoxTakeSnapshotDlg.h"
#include "UIVMInfoDialog.h"
#include "UISettingsDialogSpecific.h"
#include "UIVMLogViewer.h"
#include "UIConverter.h"
#include "UIModalWindowManager.h"
#include "UIMedium.h"
#include "UIExtraDataManager.h"
#ifdef Q_WS_MAC
# include "DockIconPreview.h"
# include "UIExtraDataManager.h"
#endif /* Q_WS_MAC */

/* COM includes: */
#include "CVirtualBoxErrorInfo.h"
#include "CMachineDebugger.h"
#include "CSnapshot.h"
#include "CDisplay.h"
#include "CStorageController.h"
#include "CMediumAttachment.h"
#include "CHostUSBDevice.h"
#include "CUSBDevice.h"
#include "CVRDEServer.h"
#include "CSystemProperties.h"
#include "CHostVideoInputDevice.h"
#include "CEmulatedUSB.h"
#include "CNetworkAdapter.h"
#ifdef Q_WS_MAC
# include "CGuest.h"
#endif /* Q_WS_MAC */

/* Other VBox includes: */
#include <iprt/path.h>
#ifdef VBOX_WITH_DEBUGGER_GUI
# include <iprt/ldr.h>
#endif /* VBOX_WITH_DEBUGGER_GUI */
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

    /* Prepare machine window(s): */
    prepareMachineWindows();

    /* Prepare menu: */
    prepareMenu();

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

CSession& UIMachineLogic::session() const
{
    return uisession()->session();
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

/** Adjusts guest screen size for each the machine-window we have. */
void UIMachineLogic::maybeAdjustGuestScreenSize()
{
    foreach(UIMachineWindow *pMachineWindow, machineWindows())
        pMachineWindow->machineView()->maybeAdjustGuestScreenSize();
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
    CConsole console = session().GetConsole();
    if (!console.GetGuestEnteredACPIMode())
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
            QString strLogFolder = session().GetMachine().GetLogFolder();
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
            /* Is it allowed to close Runtime UI? */
            if (!isPreventAutoClose())
            {
                /* VM has been powered off, saved, teleported or aborted.
                 * We must close Runtime UI: */
                uisession()->closeRuntimeUI();
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
    gActionPool->action(UIActionIndexRuntime_Toggle_Seamless)->setEnabled(uisession()->isVisualStateAllowed(UIVisualStateType_Seamless) &&
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
    QAction *pAction = gActionPool->action(UIActionIndexRuntime_Toggle_MouseIntegration);
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
            msgCenter().cannotAttachUSBDevice(error, vboxGlobal().details(device), session().GetMachine().GetName());
        else
            msgCenter().cannotDetachUSBDevice(error, vboxGlobal().details(device), session().GetMachine().GetName());
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

void UIMachineLogic::sltGuestMonitorChange(KGuestMonitorChangedEventType, ulong, QRect)
{
    LogRel(("UIMachineLogic: Guest-screen count changed.\n"));

    /* Make sure all machine-window(s) have proper geometry: */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        pMachineWindow->showInNecessaryMode();
}

void UIMachineLogic::sltHostScreenCountChanged()
{
    LogRel(("UIMachineLogic: Host-screen count changed.\n"));

    /* Make sure all machine-window(s) have proper geometry: */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        pMachineWindow->showInNecessaryMode();
}

void UIMachineLogic::sltHostScreenGeometryChanged()
{
    LogRel(("UIMachineLogic: Host-screen geometry changed.\n"));

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
    , m_pMenuBar(0)
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
    connect(uisession(), SIGNAL(sigStarted()), this, SLOT(sltCheckForRequestedVisualStateType()));
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
    connect(uisession(), SIGNAL(sigHostScreenCountChanged()),
            this, SLOT(sltHostScreenCountChanged()));
    connect(uisession(), SIGNAL(sigHostScreenGeometryChanged()),
            this, SLOT(sltHostScreenGeometryChanged()));
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

    /* Create group for all actions that are enabled when the VM is running or paused or stucked.
     * Note that only actions whose enabled state depends exclusively on the
     * execution state of the VM are added to this group. */
    m_pRunningOrPausedOrStackedActions = new QActionGroup(this);
    m_pRunningOrPausedOrStackedActions->setExclusive(false);

    /* Move actions into running actions group: */
    m_pRunningActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_TypeCAD));
#ifdef Q_WS_X11
    m_pRunningActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_TypeCABS));
#endif
    m_pRunningActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_Reset));
    m_pRunningActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_Shutdown));
    m_pRunningActions->addAction(gActionPool->action(UIActionIndexRuntime_Menu_ViewPopup));
    m_pRunningActions->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_Fullscreen));
    m_pRunningActions->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_Seamless));
    m_pRunningActions->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_Scale));
    m_pRunningActions->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_GuestAutoresize));
    m_pRunningActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_AdjustWindow));

    /* Move actions into running-n-paused actions group: */
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_Save));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_SettingsDialog));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_TakeSnapshot));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_TakeScreenshot));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_InformationDialog));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Menu_Keyboard));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_KeyboardSettings));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Menu_MouseIntegration));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_MouseIntegration));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_Pause));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Menu_StatusBar));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_StatusBarSettings));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_StatusBar));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Menu_HardDisks));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_StorageSettings));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Menu_OpticalDevices));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Menu_FloppyDevices));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Menu_USBDevices));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Menu_WebCams));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Menu_SharedClipboard));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Menu_DragAndDrop));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Menu_Network));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_NetworkSettings));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Menu_SharedFolders));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_SharedFoldersSettings));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_VRDEServer));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Menu_VideoCapture));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_VideoCapture));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_VideoCaptureSettings));
    m_pRunningOrPausedActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_InstallGuestTools));

    /* Move actions into running-n-paused-n-stucked actions group: */
    m_pRunningOrPausedOrStackedActions->addAction(gActionPool->action(UIActionIndexRuntime_Simple_PowerOff));
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
    connect(gActionPool->action(UIActionIndexRuntime_Simple_KeyboardSettings), SIGNAL(triggered()),
            this, SLOT(sltShowKeyboardSettings()));
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
    connect(gActionPool->action(UIActionIndexRuntime_Simple_Save), SIGNAL(triggered()),
            this, SLOT(sltSaveState()));
    connect(gActionPool->action(UIActionIndexRuntime_Simple_Shutdown), SIGNAL(triggered()),
            this, SLOT(sltShutdown()));
    connect(gActionPool->action(UIActionIndexRuntime_Simple_PowerOff), SIGNAL(triggered()),
            this, SLOT(sltPowerOff()));
    connect(gActionPool->action(UIActionIndexRuntime_Simple_Close), SIGNAL(triggered()),
            this, SLOT(sltClose()));

    /* "View" actions connections: */
    connect(gActionPool->action(UIActionIndexRuntime_Toggle_GuestAutoresize), SIGNAL(toggled(bool)),
            this, SLOT(sltToggleGuestAutoresize(bool)));
    connect(gActionPool->action(UIActionIndexRuntime_Simple_AdjustWindow), SIGNAL(triggered()),
            this, SLOT(sltAdjustWindow()));

    /* "Devices" actions connections: */
    connect(gActionPool->action(UIActionIndexRuntime_Simple_StorageSettings), SIGNAL(triggered()),
            this, SLOT(sltOpenStorageSettingsDialog()));
    connect(gActionPool->action(UIActionIndexRuntime_Menu_OpticalDevices)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareStorageMenu()));
    connect(gActionPool->action(UIActionIndexRuntime_Menu_FloppyDevices)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareStorageMenu()));
    connect(gActionPool->action(UIActionIndexRuntime_Menu_USBDevices)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareUSBMenu()));
    connect(gActionPool->action(UIActionIndexRuntime_Menu_WebCams)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareWebCamMenu()));
    connect(gActionPool->action(UIActionIndexRuntime_Menu_SharedClipboard)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareSharedClipboardMenu()));
    connect(gActionPool->action(UIActionIndexRuntime_Menu_DragAndDrop)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareDragAndDropMenu()));
    connect(gActionPool->action(UIActionIndexRuntime_Menu_Network)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareNetworkMenu()));
    connect(gActionPool->action(UIActionIndexRuntime_Simple_NetworkSettings), SIGNAL(triggered()),
            this, SLOT(sltOpenNetworkAdaptersDialog()));
    connect(gActionPool->action(UIActionIndexRuntime_Simple_SharedFoldersSettings), SIGNAL(triggered()),
            this, SLOT(sltOpenSharedFoldersDialog()));
    connect(gActionPool->action(UIActionIndexRuntime_Toggle_VRDEServer), SIGNAL(toggled(bool)),
            this, SLOT(sltToggleVRDE(bool)));
    connect(gActionPool->action(UIActionIndexRuntime_Toggle_VideoCapture), SIGNAL(toggled(bool)),
            this, SLOT(sltToggleVideoCapture(bool)));
    connect(gActionPool->action(UIActionIndexRuntime_Simple_VideoCaptureSettings), SIGNAL(triggered()),
            this, SLOT(sltOpenVideoCaptureOptions()));
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
    /* Create handlers: */
    setKeyboardHandler(UIKeyboardHandler::create(this, visualStateType()));
    setMouseHandler(UIMouseHandler::create(this, visualStateType()));

    /* Update UI session values with current: */
    uisession()->setKeyboardState(keyboardHandler()->state());
    uisession()->setMouseState(mouseHandler()->state());
}

void UIMachineLogic::prepareMenu()
{
#ifdef Q_WS_MAC
    /* Prepare native menu-bar: */
    RuntimeMenuType restrictedMenus = gEDataManager->restrictedRuntimeMenuTypes(vboxGlobal().managedVMUuid());
    RuntimeMenuType allowedMenus = static_cast<RuntimeMenuType>(RuntimeMenuType_All ^ restrictedMenus);
    m_pMenuBar = uisession()->newMenuBar(allowedMenus);
#endif /* Q_WS_MAC */
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
    connect(gEDataManager, SIGNAL(sigDockIconAppearanceChange(bool)),
            this, SLOT(sltChangeDockIconUpdate(bool)));

    /* Monitor selection if there are more than one monitor */
    int cGuestScreens = uisession()->session().GetMachine().GetMonitorCount();
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
    QString osTypeId = session().GetConsole().GetGuest().GetOSTypeId();
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

void UIMachineLogic::cleanupMenu()
{
#ifdef Q_WS_MAC
    delete m_pMenuBar;
    m_pMenuBar = 0;
#endif /* Q_WS_MAC */
}

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
        pMachineWindow->normalizeGeometry(true);
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

    /* Remember the paused state: */
    bool fWasPaused = uisession()->isPaused();
    if (!fWasPaused)
    {
        /* Suspend the VM and ignore the close event if failed to do so.
         * pause() will show the error message to the user. */
        if (!uisession()->pause())
            return;
    }

    /* Get current machine: */
    CMachine machine = session().GetMachine();

    /* Create take-snapshot dialog: */
    QWidget *pDlgParent = windowManager().realParentWindow(activeMachineWindow());
    QPointer<VBoxTakeSnapshotDlg> pDlg = new VBoxTakeSnapshotDlg(pDlgParent, machine);
    windowManager().registerNewParent(pDlg, pDlgParent);

    /* Assign corresponding icon: */
    QString strTypeId = machine.GetOSTypeId();
    pDlg->mLbIcon->setPixmap(vboxGlobal().vmGuestOSTypeIcon(strTypeId));

    /* Search for the max available filter index: */
    QString strNameTemplate = QApplication::translate("UIMachineLogic", "Snapshot %1");
    int iMaxSnapshotIndex = searchMaxSnapshotIndex(machine, machine.FindSnapshot(QString()), strNameTemplate);
    pDlg->mLeName->setText(strNameTemplate.arg(++ iMaxSnapshotIndex));

    /* Exec the dialog: */
    bool fDialogAccepted = pDlg->exec() == QDialog::Accepted;

    /* Is the dialog still valid? */
    if (pDlg)
    {
        /* Acquire variables: */
        QString strSnapshotName = pDlg->mLeName->text().trimmed();
        QString strSnapshotDescription = pDlg->mTeDescription->toPlainText();

        /* Destroy dialog early: */
        delete pDlg;

        /* Was the dialog accepted? */
        if (fDialogAccepted)
        {
            /* Prepare the take-snapshot progress: */
            CConsole console = session().GetConsole();
            CProgress progress = console.TakeSnapshot(strSnapshotName, strSnapshotDescription);
            if (console.isOk())
            {
                /* Show the take-snapshot progress: */
                msgCenter().showModalProgressDialog(progress, machine.GetName(), ":/progress_snapshot_create_90px.png");
                if (!progress.isOk() || progress.GetResultCode() != 0)
                    msgCenter().cannotTakeSnapshot(progress, machine.GetName());
            }
            else
                msgCenter().cannotTakeSnapshot(console, machine.GetName());
        }
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
    CMachine machine = session().GetMachine();
    if (msgCenter().confirmResetMachine(machine.GetName()))
        session().GetConsole().Reset();

    /* TODO_NEW_CORE: On reset the additional screens didn't get a display
       update. Emulate this for now until it get fixed. */
    ulong uMonitorCount = machine.GetMonitorCount();
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

    powerOff(session().GetMachine().GetSnapshotCount() > 0);
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
                                                                            session().GetMachine().GetId(),
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

void UIMachineLogic::sltOpenNetworkAdaptersDialog()
{
    /* Open VM settings : Network page: */
    sltOpenVMSettingsDialog("#network");
}

void UIMachineLogic::sltOpenSharedFoldersDialog()
{
    /* Do not process if additions are not loaded! */
    if (!uisession()->isGuestAdditionsActive())
        msgCenter().remindAboutGuestAdditionsAreNotActive();

    /* Open VM settings : Shared folders page: */
    sltOpenVMSettingsDialog("#sharedFolders");
}

void UIMachineLogic::sltPrepareStorageMenu()
{
    /* Get the sender() menu: */
    QMenu *pMenu = qobject_cast<QMenu*>(sender());
    AssertMsg(pMenu, ("This slot should only be called on hovering storage menu!\n"));
    pMenu->clear();

    /* Determine device-type: */
    const QMenu *pOpticalDevicesMenu = gActionPool->action(UIActionIndexRuntime_Menu_OpticalDevices)->menu();
    const QMenu *pFloppyDevicesMenu = gActionPool->action(UIActionIndexRuntime_Menu_FloppyDevices)->menu();
    const KDeviceType deviceType = pMenu == pOpticalDevicesMenu ? KDeviceType_DVD :
                                   pMenu == pFloppyDevicesMenu  ? KDeviceType_Floppy :
                                                                  KDeviceType_Null;
    AssertMsgReturnVoid(deviceType != KDeviceType_Null, ("Incorrect storage device-type!\n"));

    /* Prepare/fill all storage menus: */
    const CMachine machine = session().GetMachine();
    foreach (const CMediumAttachment &attachment, machine.GetMediumAttachments())
    {
        /* Current controller: */
        const CStorageController controller = machine.GetStorageControllerByName(attachment.GetController());
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
                                            machine, strControllerName, storageSlot);
        }
    }
}

void UIMachineLogic::sltMountStorageMedium()
{
    /* Sender action: */
    QAction *pAction = qobject_cast<QAction*>(sender());
    AssertMsgReturnVoid(pAction, ("This slot should only be called by menu action!\n"));

    /* Current machine: */
    const CMachine machine = session().GetMachine();
    /* Current mount-target: */
    const UIMediumTarget target = pAction->data().value<UIMediumTarget>();

    /* Update current machine mount-target: */
    vboxGlobal().updateMachineStorage(machine, target);
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
        pEmptyMenuAction->setIcon(UIIconPool::iconSet(":/usb_unavailable_16px.png", ":/usb_unavailable_disabled_16px.png"));
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

/**
 * Prepares menu content when user hovers <b>Webcam</b> submenu of the <b>Devices</b> menu.
 * @note If host currently have no webcams attached there will be just one dummy action
 *       called <i>No Webcams Connected</i>. Otherwise there will be actions corresponding
 *       to existing webcams allowing user to attach/detach them within the guest.
 * @note In order to enumerate webcams GUI assigns #WebCamTarget object as internal data
 *       for each the enumerated webcam menu action. Corresponding #sltAttachWebCamDevice
 *       slot will be called on action triggering. It will parse assigned #WebCamTarget data.
 */
void UIMachineLogic::sltPrepareWebCamMenu()
{
    /* Get and check the sender menu object: */
    QMenu *pMenu = qobject_cast<QMenu*>(sender());
    QMenu *pWebCamMenu = gActionPool->action(UIActionIndexRuntime_Menu_WebCams)->menu();
    AssertReturnVoid(pMenu == pWebCamMenu); Q_UNUSED(pWebCamMenu);

    /* Clear menu initially: */
    pMenu->clear();

    /* Get current host: */
    const CHost &host = vboxGlobal().host();

    /* Get host webcams list: */
    const CHostVideoInputDeviceVector &webcams = host.GetVideoInputDevices();

    /* If webcam list is empty: */
    if (webcams.isEmpty())
    {
        /* Add only one - "empty" action: */
        QAction *pEmptyMenuAction = new QAction(pMenu);
        pEmptyMenuAction->setEnabled(false);
        pEmptyMenuAction->setText(tr("No Webcams Connected"));
        pEmptyMenuAction->setToolTip(tr("No supported webcams connected to the host PC"));
        pEmptyMenuAction->setIcon(UIIconPool::iconSet(":/web_camera_unavailable_16px.png", ":/web_camera_unavailable_disabled_16px.png"));
        pMenu->addAction(pEmptyMenuAction);
    }
    /* If webcam list is NOT empty: */
    else
    {
        /* Populate menu with host webcams: */
        const QVector<QString> &attachedWebcamPaths = session().GetConsole().GetEmulatedUSB().GetWebcams();
        foreach (const CHostVideoInputDevice &webcam, webcams)
        {
            /* Get webcam data: */
            const QString &strWebcamName = webcam.GetName();
            const QString &strWebcamPath = webcam.GetPath();

            /* Create/configure webcam action: */
            QAction *pAttachWebcamAction = new QAction(strWebcamName, pMenu);
            pAttachWebcamAction->setToolTip(vboxGlobal().toolTip(webcam));
            pAttachWebcamAction->setCheckable(true);
            pAttachWebcamAction->setChecked(attachedWebcamPaths.contains(strWebcamPath));
            pAttachWebcamAction->setData(QVariant::fromValue(WebCamTarget(!pAttachWebcamAction->isChecked(), strWebcamName, strWebcamPath)));
            connect(pAttachWebcamAction, SIGNAL(triggered(bool)), this, SLOT(sltAttachWebCamDevice()));
            pMenu->addAction(pAttachWebcamAction);
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

/**
 * Attaches/detaches webcam within the guest.
 * @note In order to attach/detach webcams #sltPrepareWebCamMenu assigns #WebCamTarget object
 *       as internal data for each the enumerated webcam menu action. Corresponding data
 *       will be parsed here resulting in device attaching/detaching.
 */
void UIMachineLogic::sltAttachWebCamDevice()
{
    /* Get and check sender action object: */
    QAction *pAction = qobject_cast<QAction*>(sender());
    AssertReturnVoid(pAction);

    /* Get operation target: */
    WebCamTarget target = pAction->data().value<WebCamTarget>();

    /* Get current emulated USB: */
    const CConsole &console = session().GetConsole();
    CEmulatedUSB dispatcher = console.GetEmulatedUSB();

    /* Attach webcam device: */
    if (target.attach)
    {
        /* Try to attach corresponding device: */
        dispatcher.WebcamAttach(target.path, "");
        /* Check if dispatcher is OK: */
        if (!dispatcher.isOk())
            msgCenter().cannotAttachWebCam(dispatcher, target.name, console.GetMachine().GetName());
    }
    /* Detach webcam device: */
    else
    {
        /* Try to detach corresponding device: */
        dispatcher.WebcamDetach(target.path);
        /* Check if dispatcher is OK: */
        if (!dispatcher.isOk())
            msgCenter().cannotDetachWebCam(dispatcher, target.name, console.GetMachine().GetName());
    }
}

void UIMachineLogic::sltPrepareSharedClipboardMenu()
{
    /* Get and check the sender menu object: */
    QMenu *pMenu = qobject_cast<QMenu*>(sender());
    QMenu *pSharedClipboardMenu = gActionPool->action(UIActionIndexRuntime_Menu_SharedClipboard)->menu();
    AssertMsg(pMenu == pSharedClipboardMenu, ("This slot should only be called on hovering Shared Clipboard menu!\n"));
    Q_UNUSED(pSharedClipboardMenu);

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
            pAction->setChecked(session().GetMachine().GetClipboardMode() == mode);
        }
        connect(m_pSharedClipboardActions, SIGNAL(triggered(QAction*)),
                this, SLOT(sltChangeSharedClipboardType(QAction*)));
    }
    /* Subsequent runs: */
    else
        foreach (QAction *pAction, m_pSharedClipboardActions->actions())
            if (pAction->data().value<KClipboardMode>() == session().GetMachine().GetClipboardMode())
                pAction->setChecked(true);
}

void UIMachineLogic::sltChangeSharedClipboardType(QAction *pAction)
{
    /* Assign new mode (without save): */
    KClipboardMode mode = pAction->data().value<KClipboardMode>();
    session().GetMachine().SetClipboardMode(mode);
}

void UIMachineLogic::sltPrepareDragAndDropMenu()
{
    /* Get and check the sender menu object: */
    QMenu *pMenu = qobject_cast<QMenu*>(sender());
    QMenu *pDragAndDropMenu = gActionPool->action(UIActionIndexRuntime_Menu_DragAndDrop)->menu();
    AssertMsg(pMenu == pDragAndDropMenu, ("This slot should only be called on hovering Drag'n'drop menu!\n"));
    Q_UNUSED(pDragAndDropMenu);

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
            pAction->setChecked(session().GetMachine().GetDnDMode() == mode);
        }
        connect(m_pDragAndDropActions, SIGNAL(triggered(QAction*)),
                this, SLOT(sltChangeDragAndDropType(QAction*)));
    }
    /* Subsequent runs: */
    else
        foreach (QAction *pAction, m_pDragAndDropActions->actions())
            if (pAction->data().value<KDnDMode>() == session().GetMachine().GetDnDMode())
                pAction->setChecked(true);
}

/** Prepares menu content when user hovers <b>Network</b> submenu of the <b>Devices</b> menu. */
void UIMachineLogic::sltPrepareNetworkMenu()
{
    /* Get and check 'the sender' menu object: */
    QMenu *pMenu = qobject_cast<QMenu*>(sender());
    QMenu *pNetworkMenu = gActionPool->action(UIActionIndexRuntime_Menu_Network)->menu();
    AssertReturnVoid(pMenu == pNetworkMenu);
    Q_UNUSED(pNetworkMenu);

    /* Get and check current machine: */
    const CMachine &machine = session().GetMachine();
    AssertReturnVoid(!machine.isNull());

    /* Determine how many adapters we should display: */
    KChipsetType chipsetType = machine.GetChipsetType();
    ULONG uCount = qMin((ULONG)4, vboxGlobal().virtualBox().GetSystemProperties().GetMaxNetworkAdapters(chipsetType));

    /* Enumerate existing network adapters: */
    QMap<int, bool> adapterData;
    for (ULONG uSlot = 0; uSlot < uCount; ++uSlot)
    {
        /* Get and check iterated adapter: */
        const CNetworkAdapter &adapter = machine.GetNetworkAdapter(uSlot);
        AssertReturnVoid(machine.isOk());
        Assert(!adapter.isNull());
        if (adapter.isNull())
            continue;

        /* Remember adapter data if it is enabled: */
        if (adapter.GetEnabled())
            adapterData.insert((int)uSlot, (bool)adapter.GetCableConnected());
    }
    AssertReturnVoid(!adapterData.isEmpty());

    /* Delete all "temporary" actions: */
    QList<QAction*> actions = pMenu->actions();
    foreach (QAction *pAction, actions)
        if (pAction->property("temporary").toBool())
            delete pAction;

    /* Add new "temporary" actions: */
    foreach (int iSlot, adapterData.keys())
    {
        QAction *pAction = pMenu->addAction(QIcon(adapterData[iSlot] ? ":/connect_16px.png": ":/disconnect_16px.png"),
                                            adapterData.size() == 1 ? tr("Connect Network Adapter") : tr("Connect Network Adapter %1").arg(iSlot + 1),
                                            this, SLOT(sltToggleNetworkAdapterConnection()));
        pAction->setProperty("temporary", true);
        pAction->setProperty("slot", iSlot);
        pAction->setCheckable(true);
        pAction->setChecked(adapterData[iSlot]);
    }
}

/** Toggles network adapter's <i>Cable Connected</i> state. */
void UIMachineLogic::sltToggleNetworkAdapterConnection()
{
    /* Get and check 'the sender' action object: */
    QAction *pAction = qobject_cast<QAction*>(sender());
    AssertReturnVoid(pAction);

    /* Get and check current machine: */
    CMachine machine = session().GetMachine();
    AssertReturnVoid(!machine.isNull());

    /* Get operation target: */
    CNetworkAdapter adapter = machine.GetNetworkAdapter((ULONG)pAction->property("slot").toInt());
    AssertReturnVoid(machine.isOk() && !adapter.isNull());

    /* Connect/disconnect cable to/from target: */
    adapter.SetCableConnected(!adapter.GetCableConnected());
    machine.SaveSettings();
    if (!machine.isOk())
        msgCenter().cannotSaveMachineSettings(machine);
}

void UIMachineLogic::sltChangeDragAndDropType(QAction *pAction)
{
    /* Assign new mode (without save): */
    KDnDMode mode = pAction->data().value<KDnDMode>();
    session().GetMachine().SetDnDMode(mode);
}

void UIMachineLogic::sltToggleVRDE(bool fEnabled)
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    /* Access VRDE server: */
    CMachine machine = session().GetMachine();
    CVRDEServer server = machine.GetVRDEServer();
    AssertMsg(!server.isNull(), ("VRDE server should NOT be null!\n"));
    if (!machine.isOk() || server.isNull())
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
            machine.SaveSettings();
            /* Machine still OK? */
            if (!machine.isOk())
            {
                /* Notify about the error: */
                msgCenter().cannotSaveMachineSettings(machine);
                /* Make sure action is updated! */
                uisession()->updateStatusVRDE();
            }
        }
        else
        {
            /* Notify about the error: */
            msgCenter().cannotToggleVRDEServer(server, machine.GetName(), fEnabled);
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

    /* Access machine: */
    CMachine machine = session().GetMachine();
    AssertMsg(!machine.isNull(), ("Machine should NOT be null!\n"));
    if (machine.isNull())
        return;

    /* Make sure something had changed: */
    if (machine.GetVideoCaptureEnabled() == static_cast<BOOL>(fEnabled))
        return;

    /* Update Video Capture state: */
    AssertMsg(machine.isOk(), ("Machine should be OK!\n"));
    machine.SetVideoCaptureEnabled(fEnabled);
    /* Machine is not OK? */
    if (!machine.isOk())
    {
        /* Notify about the error: */
        msgCenter().cannotToggleVideoCapture(machine, fEnabled);
        /* Make sure action is updated! */
        uisession()->updateStatusVideoCapture();
    }
    /* Machine is OK? */
    else
    {
        /* Save machine-settings: */
        machine.SaveSettings();
        /* Machine is not OK? */
        if (!machine.isOk())
        {
            /* Notify about the error: */
            msgCenter().cannotSaveMachineSettings(machine);
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
        bool fEnabled = pAction != gActionPool->action(UIActionIndexRuntime_Toggle_DockDisableMonitor);
        gEDataManager->setRealtimeDockIconUpdateEnabled(fEnabled, vboxGlobal().managedVMUuid());
        updateDockOverlay();
    }
}

void UIMachineLogic::sltDockPreviewMonitorChanged(QAction *pAction)
{
    CMachine machine = session().GetMachine();
    if (!machine.isNull())
    {
        gEDataManager->setRealtimeDockIconUpdateMonitor(pAction->data().toInt(), vboxGlobal().managedVMUuid());
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
            m_DockIconPreviewMonitor = qMin(gEDataManager->realtimeDockIconUpdateMonitor(vboxGlobal().managedVMUuid()),
                                            (int)machine.GetMonitorCount() - 1);
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
//           session().GetMachine().GetName().toAscii().constData());

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
//           session().GetMachine().GetName().toAscii().constData());

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

void UIMachineLogic::showGlobalPreferences(const QString &strCategory /* = QString() */, const QString &strControl /* = QString() */)
{
    /* Do not process if window(s) missed! */
    if (!isMachineWindowsCreated())
        return;

    /* Check that we do NOT handling that already: */
    if (gActionPool->action(UIActionIndex_Simple_Preferences)->data().toBool())
        return;
    /* Remember that we handling that already: */
    gActionPool->action(UIActionIndex_Simple_Preferences)->setData(true);

    /* Create and execute global settings window: */
    QPointer<UISettingsDialogGlobal> pDialog = new UISettingsDialogGlobal(activeMachineWindow(),
                                                                          strCategory, strControl);
    pDialog->execute();
    if (pDialog)
        delete pDialog;

    /* Remember that we do NOT handling that already: */
    gActionPool->action(UIActionIndex_Simple_Preferences)->setData(false);
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
        LONG xOrigin = 0;
        LONG yOrigin = 0;
        display.GetScreenResolution(i, width, height, bpp, xOrigin, yOrigin);
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

