/** @file
 * VBox Qt GUI - UISession class declaration.
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UISession_h___
#define ___UISession_h___

/* Qt includes: */
#include <QObject>
#include <QCursor>
#include <QEvent>
#include <QMap>

/* GUI includes: */
#include "UIDefs.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declarations: */
class QMenu;
class QMenuBar;
class UIFrameBuffer;
class UIMachine;
class UIMachineLogic;
class UIMachineMenuBar;
class CSession;
class CUSBDevice;
class CNetworkAdapter;
class CMediumAttachment;

/* CConsole callback event types: */
enum UIConsoleEventType
{
    UIConsoleEventType_MousePointerShapeChange = QEvent::User + 1,
    UIConsoleEventType_MouseCapabilityChange,
    UIConsoleEventType_KeyboardLedsChange,
    UIConsoleEventType_StateChange,
    UIConsoleEventType_AdditionsStateChange,
    UIConsoleEventType_NetworkAdapterChange,
    /* Not used: UIConsoleEventType_SerialPortChange, */
    /* Not used: UIConsoleEventType_ParallelPortChange, */
    /* Not used: UIConsoleEventType_StorageControllerChange, */
    UIConsoleEventType_MediumChange,
    /* Not used: UIConsoleEventType_CPUChange, */
    UIConsoleEventType_VRDEServerChange,
    UIConsoleEventType_VRDEServerInfoChange,
    UIConsoleEventType_USBControllerChange,
    UIConsoleEventType_USBDeviceStateChange,
    UIConsoleEventType_SharedFolderChange,
    UIConsoleEventType_RuntimeError,
    UIConsoleEventType_CanShowWindow,
    UIConsoleEventType_ShowWindow,
    UIConsoleEventType_MAX
};

class UISession : public QObject
{
    Q_OBJECT;

public:

    /* Machine uisession constructor/destructor: */
    UISession(UIMachine *pMachine, CSession &session);
    virtual ~UISession();

    /* API: Runtime UI stuff: */
    void powerUp();
    bool saveState();
    bool shutdown();
    bool powerOff(bool fIncludingDiscard, bool &fServerCrashed);
    void closeRuntimeUI();

    /* Common getters: */
    CSession& session() { return m_session; }
    KMachineState machineStatePrevious() const { return m_machineStatePrevious; }
    KMachineState machineState() const { return m_machineState; }
    UIMachineLogic* machineLogic() const;
    QWidget* mainMachineWindow() const;
    QMenu* newMenu(RuntimeMenuType fOptions = RuntimeMenuType_All);
    QMenuBar* newMenuBar(RuntimeMenuType fOptions = RuntimeMenuType_All);
    QCursor cursor() const { return m_cursor; }

    /** @name Extension Pack stuff.
     ** @{ */
    /** Determines whether extension pack installed and usable. */
    bool isExtensionPackUsable() const { return m_fIsExtensionPackUsable; }
    /** @} */

    /** @name Runtime menus configuration stuff.
     ** @{ */
#ifdef Q_WS_MAC
    /** Determines Application menu allowed actions. */
    RuntimeMenuApplicationActionType allowedActionsMenuApplication() const { return m_allowedActionsMenuApplication; }
#endif /* Q_WS_MAC */
    /** Determines Machine menu allowed actions. */
    RuntimeMenuMachineActionType allowedActionsMenuMachine() const { return m_allowedActionsMenuMachine; }
    /** Determines View menu allowed actions. */
    RuntimeMenuViewActionType allowedActionsMenuView() const { return m_allowedActionsMenuView; }
    /** Determines Devices menu allowed actions. */
    RuntimeMenuDevicesActionType allowedActionsMenuDevices() const { return m_allowedActionsMenuDevices; }
#ifdef VBOX_WITH_DEBUGGER_GUI
    /** Determines Debugger menu allowed actions. */
    RuntimeMenuDebuggerActionType allowedActionsMenuDebugger() const { return m_allowedActionsMenuDebugger; }
#endif /* VBOX_WITH_DEBUGGER_GUI */
    /** Determines Help menu allowed actions. */
    RuntimeMenuHelpActionType allowedActionsMenuHelp() const { return m_allowedActionsMenuHelp; }
    /** @} */

    /** @name Application Close configuration stuff.
     * @{ */
    /** Returns default close action. */
    MachineCloseAction defaultCloseAction() const { return m_defaultCloseAction; }
    /** Returns merged restricted close actions. */
    MachineCloseAction restrictedCloseActions() const { return m_restrictedCloseActions; }
    /** Returns whether all the close actions are restricted. */
    bool isAllCloseActionsRestricted() const { return m_fAllCloseActionsRestricted; }
    /** @} */

    /** @name Snapshot Operations configuration stuff.
     * @{ */
    /** Returns whether we should allow snapshot operations. */
    bool isSnapshotOperationsAllowed() const { return m_fSnapshotOperationsAllowed; }
    /** @} */

    /* API: Visual-state stuff: */
    bool isVisualStateAllowedFullscreen() const;
    bool isVisualStateAllowedSeamless() const;
    bool isVisualStateAllowedScale() const;
    /** Requests visual-state change. */
    void changeVisualState(UIVisualStateType visualStateType);
    /** Requests visual-state to be entered when possible. */
    void setRequestedVisualState(UIVisualStateType visualStateType) { m_requestedVisualStateType = visualStateType; }
    /** Returns requested visual-state to be entered when possible. */
    UIVisualStateType requestedVisualState() const { return m_requestedVisualStateType; }

    bool isSaved() const { return machineState() == KMachineState_Saved; }
    bool isTurnedOff() const { return machineState() == KMachineState_PoweredOff ||
                                      machineState() == KMachineState_Saved ||
                                      machineState() == KMachineState_Teleported ||
                                      machineState() == KMachineState_Aborted; }
    bool isPaused() const { return machineState() == KMachineState_Paused ||
                                   machineState() == KMachineState_TeleportingPausedVM; }
    bool isRunning() const { return machineState() == KMachineState_Running ||
                                    machineState() == KMachineState_Teleporting ||
                                    machineState() == KMachineState_LiveSnapshotting; }
    bool isStuck() const { return machineState() == KMachineState_Stuck; }
    bool wasPaused() const { return machineStatePrevious() == KMachineState_Paused ||
                                    machineStatePrevious() == KMachineState_TeleportingPausedVM; }
    bool isFirstTimeStarted() const { return m_fIsFirstTimeStarted; }
    bool isIgnoreRuntimeMediumsChanging() const { return m_fIsIgnoreRuntimeMediumsChanging; }
    bool isGuestResizeIgnored() const { return m_fIsGuestResizeIgnored; }
    bool isAutoCaptureDisabled() const { return m_fIsAutoCaptureDisabled; }

    /* Guest additions state getters: */
    bool isGuestAdditionsActive() const { return (m_ulGuestAdditionsRunLevel > AdditionsRunLevelType_None); }
    bool isGuestSupportsGraphics() const { return isGuestAdditionsActive() && m_fIsGuestSupportsGraphics; }
    bool isGuestSupportsSeamless() const { return isGuestSupportsGraphics() && m_fIsGuestSupportsSeamless; }

    /* Keyboard getters: */
    bool isNumLock() const { return m_fNumLock; }
    bool isCapsLock() const { return m_fCapsLock; }
    bool isScrollLock() const { return m_fScrollLock; }
    uint numLockAdaptionCnt() const { return m_uNumLockAdaptionCnt; }
    uint capsLockAdaptionCnt() const { return m_uCapsLockAdaptionCnt; }

    /* Mouse getters: */
    bool isMouseSupportsAbsolute() const { return m_fIsMouseSupportsAbsolute; }
    bool isMouseSupportsRelative() const { return m_fIsMouseSupportsRelative; }
    bool isMouseSupportsMultiTouch() const { return m_fIsMouseSupportsMultiTouch; }
    bool isMouseHostCursorNeeded() const { return m_fIsMouseHostCursorNeeded; }
    bool isMouseCaptured() const { return m_fIsMouseCaptured; }
    bool isMouseIntegrated() const { return m_fIsMouseIntegrated; }
    bool isValidPointerShapePresent() const { return m_fIsValidPointerShapePresent; }
    bool isHidingHostPointer() const { return m_fIsHidingHostPointer; }

    /* Common setters: */
    bool pause() { return setPause(true); }
    bool unpause() { return setPause(false); }
    bool setPause(bool fOn);
    void setGuestResizeIgnored(bool fIsGuestResizeIgnored) { m_fIsGuestResizeIgnored = fIsGuestResizeIgnored; }
    void setAutoCaptureDisabled(bool fIsAutoCaptureDisabled) { m_fIsAutoCaptureDisabled = fIsAutoCaptureDisabled; }
    void forgetPreviousMachineState() { m_machineStatePrevious = m_machineState; }

    /* Keyboard setters: */
    void setNumLockAdaptionCnt(uint uNumLockAdaptionCnt) { m_uNumLockAdaptionCnt = uNumLockAdaptionCnt; }
    void setCapsLockAdaptionCnt(uint uCapsLockAdaptionCnt) { m_uCapsLockAdaptionCnt = uCapsLockAdaptionCnt; }

    /* Mouse setters: */
    void setMouseCaptured(bool fIsMouseCaptured) { m_fIsMouseCaptured = fIsMouseCaptured; }
    void setMouseIntegrated(bool fIsMouseIntegrated) { m_fIsMouseIntegrated = fIsMouseIntegrated; }

    /* Screen visibility status: */
    bool isScreenVisible(ulong uScreenId) const;
    void setScreenVisible(ulong uScreenId, bool fIsMonitorVisible);

    /* Returns existing framebuffer for the given screen-number;
     * Returns 0 (asserts) if screen-number attribute is out of bounds: */
    UIFrameBuffer* frameBuffer(ulong uScreenId) const;
    /* Sets framebuffer for the given screen-number;
     * Ignores (asserts) if screen-number attribute is out of bounds: */
    void setFrameBuffer(ulong uScreenId, UIFrameBuffer* pFrameBuffer);

    /* Temporary API: */
    void updateStatusVRDE() { sltVRDEChange(); }
    void updateStatusVideoCapture() { sltVideoCaptureChange(); }

signals:

    /* Notifier: Close Runtime UI stuff: */
    void sigCloseRuntimeUI();

    /* Console callback signals: */
    void sigMousePointerShapeChange();
    void sigMouseCapabilityChange();
    void sigKeyboardLedsChange();
    void sigMachineStateChange();
    void sigAdditionsStateChange();
    void sigNetworkAdapterChange(const CNetworkAdapter &networkAdapter);
    void sigMediumChange(const CMediumAttachment &mediumAttachment);
    void sigVRDEChange();
    void sigVideoCaptureChange();
    void sigUSBControllerChange();
    void sigUSBDeviceStateChange(const CUSBDevice &device, bool bIsAttached, const CVirtualBoxErrorInfo &error);
    void sigSharedFolderChange();
    void sigRuntimeError(bool bIsFatal, const QString &strErrorId, const QString &strMessage);
#ifdef RT_OS_DARWIN
    void sigShowWindows();
#endif /* RT_OS_DARWIN */
    void sigCPUExecutionCapChange();
    void sigGuestMonitorChange(KGuestMonitorChangedEventType changeType, ulong uScreenId, QRect screenGeo);

    /* Notifiers: Qt callback stuff: */
    void sigHostScreenCountChanged();
    void sigHostScreenGeometryChanged();

    /* Session signals: */
    void sigMachineStarted();

public slots:

    void sltInstallGuestAdditionsFrom(const QString &strSource);

private slots:

    /** Requests visual-state change to 'normal' (window). */
    void sltChangeVisualStateToNormal();
    /** Requests visual-state change to 'fullscreen'. */
    void sltChangeVisualStateToFullscreen();
    /** Requests visual-state change to 'seamless'. */
    void sltChangeVisualStateToSeamless();
    /** Requests visual-state change to 'scale'. */
    void sltChangeVisualStateToScale();

    /* Handler: Close Runtime UI stuff: */
    void sltCloseRuntimeUI();

    /* Console events slots */
    void sltMousePointerShapeChange(bool fVisible, bool fAlpha, QPoint hotCorner, QSize size, QVector<uint8_t> shape);
    void sltMouseCapabilityChange(bool fSupportsAbsolute, bool fSupportsRelative, bool fSupportsMultiTouch, bool fNeedsHostCursor);
    void sltKeyboardLedsChangeEvent(bool fNumLock, bool fCapsLock, bool fScrollLock);
    void sltStateChange(KMachineState state);
    void sltAdditionsChange();
    void sltVRDEChange();
    void sltVideoCaptureChange();
    void sltGuestMonitorChange(KGuestMonitorChangedEventType changeType, ulong uScreenId, QRect screenGeo);

    /* Handlers: Display reconfiguration stuff: */
#ifdef RT_OS_DARWIN
    void sltHandleHostDisplayAboutToChange();
    void sltCheckIfHostDisplayChanged();
#endif /* RT_OS_DARWIN */
    void sltHandleHostScreenCountChange();
    void sltHandleHostScreenGeometryChange();

private:

    /* Private getters: */
    UIMachine* uimachine() const { return m_pMachine; }

    /* Prepare helpers: */
    void prepareConnections();
    void prepareConsoleEventHandlers();
    void prepareScreens();
    void prepareFramebuffers();
    void prepareMenuPool();
    void loadSessionSettings();

    /* Cleanup helpers: */
    void saveSessionSettings();
    void cleanupMenuPool();
    void cleanupFramebuffers();
    //void cleanupScreens() {}
    void cleanupConsoleEventHandlers();
    void cleanupConnections();

    /* Update helpers: */
    void updateSessionSettings();

    /* Common helpers: */
    WId winId() const;
    void setPointerShape(const uchar *pShapeData, bool fHasAlpha, uint uXHot, uint uYHot, uint uWidth, uint uHeight);
    void reinitMenuPool();
    bool preparePowerUp();
    int countOfVisibleWindows();

#ifdef Q_WS_MAC
    /* Helper: Display reconfiguration stuff: */
    void recacheDisplayData();
#endif /* Q_WS_MAC */

    /* Private variables: */
    UIMachine *m_pMachine;
    CSession &m_session;

    UIMachineMenuBar *m_pMenuPool;

    /* Screen visibility vector: */
    QVector<bool> m_monitorVisibilityVector;

    /* Frame-buffers vector: */
    QVector<UIFrameBuffer*> m_frameBufferVector;

    /* Common variables: */
    KMachineState m_machineStatePrevious;
    KMachineState m_machineState;
    QCursor m_cursor;

    /** @name Extension Pack variables.
     ** @{ */
    /** Determines whether extension pack installed and usable. */
    bool m_fIsExtensionPackUsable;
    /** @} */

    /** @name Runtime menus configuration variables.
     ** @{ */
#ifdef Q_WS_MAC
    /** Determines Application menu allowed actions. */
    RuntimeMenuApplicationActionType m_allowedActionsMenuApplication;
#endif /* Q_WS_MAC */
    /** Determines Machine menu allowed actions. */
    RuntimeMenuMachineActionType m_allowedActionsMenuMachine;
    /** Determines View menu allowed actions. */
    RuntimeMenuViewActionType m_allowedActionsMenuView;
    /** Determines Devices menu allowed actions. */
    RuntimeMenuDevicesActionType m_allowedActionsMenuDevices;
#ifdef VBOX_WITH_DEBUGGER_GUI
    /** Determines Debugger menu allowed actions. */
    RuntimeMenuDebuggerActionType m_allowedActionsMenuDebugger;
#endif /* VBOX_WITH_DEBUGGER_GUI */
    /** Determines Help menu allowed actions. */
    RuntimeMenuHelpActionType m_allowedActionsMenuHelp;
    /** @} */

    /** @name Visual-state configuration variables.
     ** @{ */
    /** Determines which visual-state should be entered when possible. */
    UIVisualStateType m_requestedVisualStateType;
    /** @} */

#if defined(Q_WS_WIN)
    HCURSOR m_alphaCursor;
#endif

#ifdef Q_WS_MAC
    /** @name MacOS X: Display reconfiguration variables.
     * @{ */
    /** MacOS X: Watchdog timer looking for display reconfiguration. */
    QTimer *m_pWatchdogDisplayChange;
    /** MacOS X: A list of display geometries we currently have. */
    QList<QRect> m_screens;
    /** @} */
#endif /* Q_WS_MAC */

    /** @name Application Close configuration variables.
     * @{ */
    /** Default close action. */
    MachineCloseAction m_defaultCloseAction;
    /** Merged restricted close actions. */
    MachineCloseAction m_restrictedCloseActions;
    /** Determines whether all the close actions are restricted. */
    bool m_fAllCloseActionsRestricted;
    /** @} */

    /** @name Snapshot Operations configuration variables.
     * @{ */
    /** Determines whether we should allow snapshot operations. */
    bool m_fSnapshotOperationsAllowed;
    /** @} */

    /* Common flags: */
    bool m_fIsFirstTimeStarted : 1;
    bool m_fIsIgnoreRuntimeMediumsChanging : 1;
    bool m_fIsGuestResizeIgnored : 1;
    bool m_fIsAutoCaptureDisabled : 1;
    bool m_fReconfigurable : 1;

    /* Guest additions flags: */
    ULONG m_ulGuestAdditionsRunLevel;
    bool  m_fIsGuestSupportsGraphics : 1;
    bool  m_fIsGuestSupportsSeamless : 1;

    /* Keyboard flags: */
    bool m_fNumLock : 1;
    bool m_fCapsLock : 1;
    bool m_fScrollLock : 1;
    uint m_uNumLockAdaptionCnt;
    uint m_uCapsLockAdaptionCnt;

    /* Mouse flags: */
    bool m_fIsMouseSupportsAbsolute : 1;
    bool m_fIsMouseSupportsRelative : 1;
    bool m_fIsMouseSupportsMultiTouch: 1;
    bool m_fIsMouseHostCursorNeeded : 1;
    bool m_fIsMouseCaptured : 1;
    bool m_fIsMouseIntegrated : 1;
    bool m_fIsValidPointerShapePresent : 1;
    bool m_fIsHidingHostPointer : 1;

    /* Friend classes: */
    friend class UIConsoleEventHandler;
};

#endif /* !___UISession_h___ */

