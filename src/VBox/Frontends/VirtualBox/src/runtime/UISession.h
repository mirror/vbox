/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UISession class declaration
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIConsole_h___
#define ___UIConsole_h___

/* Global includes */
#include <QObject>
#include <QCursor>

/* Local includes */
#include "COMDefs.h"
#include "UIMachineDefs.h"

/* Global forwards */
class QMenu;
class QMenuBar;

/* Local forwards */
class UIActionsPool;
class UIConsoleCallback;
class UIFrameBuffer;
class UIMachine;
class UIMachineLogic;
class UIMachineMenuBar;

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
    UIConsoleEventType_VRDPServerChange,
    UIConsoleEventType_RemoteDisplayInfoChange,
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

    /* Common members: */
    void powerUp();

    /* Common getters: */
    CSession& session() { return m_session; }
    KMachineState machineState() const { return m_machineState; }
    UIActionsPool* actionsPool() const;
    QWidget* mainMachineWindow() const;
    UIMachineLogic* machineLogic() const;
    QMenu* newMenu(UIMainMenuType fOptions = UIMainMenuType_All);
    QMenuBar* newMenuBar(UIMainMenuType fOptions = UIMainMenuType_All);
    QCursor cursor() const { return m_cursor; }

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
    bool isFirstTimeStarted() const { return m_fIsFirstTimeStarted; }
    bool isIgnoreRuntimeMediumsChanging() const { return m_fIsIgnoreRuntimeMediumsChanging; }
    bool isGuestResizeIgnored() const { return m_fIsGuestResizeIgnored; }
    bool isSeamlessModeRequested() const { return m_fIsSeamlessModeRequested; }

    /* Guest additions state getters: */
    bool isGuestAdditionsActive() const { return m_fIsGuestAdditionsActive; }
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
    void setSeamlessModeRequested(bool fIsSeamlessModeRequested) { m_fIsSeamlessModeRequested = fIsSeamlessModeRequested; }

    /* Keyboard setters: */
    void setNumLockAdaptionCnt(uint uNumLockAdaptionCnt) { m_uNumLockAdaptionCnt = uNumLockAdaptionCnt; }
    void setCapsLockAdaptionCnt(uint uCapsLockAdaptionCnt) { m_uCapsLockAdaptionCnt = uCapsLockAdaptionCnt; }

    /* Mouse setters: */
    void setMouseCaptured(bool fIsMouseCaptured) { m_fIsMouseCaptured = fIsMouseCaptured; emit sigMouseCapturedStatusChanged(); }
    void setMouseIntegrated(bool fIsMouseIntegrated) { m_fIsMouseIntegrated = fIsMouseIntegrated; }

#ifdef VBOX_WITH_VIDEOHWACCEL
    /* return a persisted framebuffer for the given screen
     * see comment below for the m_FrameBufferVector field */
    UIFrameBuffer* frameBuffer(ulong screenId) const;
    /* @return VINF_SUCCESS - on success
     * VERR_INVALID_PARAMETER - if screenId is invalid */
    int setFrameBuffer(ulong screenId, UIFrameBuffer* pFrameBuffer);
#endif

signals:

    /* Console callback signals: */
    void sigMousePointerShapeChange();
    void sigMouseCapabilityChange();
    void sigKeyboardLedsChange();
    void sigMachineStateChange();
    void sigAdditionsStateChange();
    void sigNetworkAdapterChange(const CNetworkAdapter &networkAdapter);
    /* Not used: void sigSerialPortChange(const CSerialPort &serialPort); */
    /* Not used: void sigParallelPortChange(const CParallelPort &parallelPort); */
    /* Not used: void sigStorageControllerChange(); */
    void sigMediumChange(const CMediumAttachment &mediumAttachment);
    /* Not used: void sigCPUChange(ulong uCPU, bool bRemove); */
    /* Not used: void sigVRDPServerChange(); */
    /* Not used: void sigRemoteDisplayInfoChange(); */
    void sigUSBControllerChange();
    void sigUSBDeviceStateChange(const CUSBDevice &device, bool bIsAttached, const CVirtualBoxErrorInfo &error);
    void sigSharedFolderChange();
    void sigRuntimeError(bool bIsFatal, const QString &strErrorId, const QString &strMessage);
#ifdef RT_OS_DARWIN
    void sigShowWindows();
#endif /* RT_OS_DARWIN */

    /* Session signals: */
    void sigMachineStarted();
    void sigMouseCapturedStatusChanged();

public slots:

    void sltInstallGuestAdditionsFrom(const QString &strSource);

private slots:

    /* Close uisession handler: */
    void sltCloseVirtualSession();

private:

    /* Private getters: */
    UIMachine* uimachine() const { return m_pMachine; }

    /* Event handlers: */
    bool event(QEvent *pEvent);

    /* Prepare helpers: */
    void prepareMenuPool();
    void loadSessionSettings();

    /* Cleanup helpers: */
    void saveSessionSettings();
    void cleanupMenuPool();

    /* Common helpers: */
    WId winId() const;
    void setPointerShape(const uchar *pShapeData, bool fHasAlpha, uint uXHot, uint uYHot, uint uWidth, uint uHeight);
    void reinitMenuPool();
    void preparePowerUp();

    /* Private variables: */
    UIMachine *m_pMachine;
    CSession &m_session;
    const CConsoleCallback m_callback;

    UIMachineMenuBar *m_pMenuPool;

#ifdef VBOX_WITH_VIDEOHWACCEL
    /* When 2D is enabled we do not re-create Framebuffers. This is done
     * 1. to avoid 2D command loss during the time slot when no framebuffer is
     *    assigned to the display
     * 2. to make it easier to preserve the current 2D state */
    QVector<UIFrameBuffer*> m_FrameBufferVector;
#endif

    /* Common variables: */
    KMachineState m_machineState;
    QCursor m_cursor;
#if defined(Q_WS_WIN)
    HCURSOR m_alphaCursor;
#endif

    /* Common flags: */
    bool m_fIsFirstTimeStarted : 1;
    bool m_fIsIgnoreRuntimeMediumsChanging : 1;
    bool m_fIsGuestResizeIgnored : 1;
    bool m_fIsSeamlessModeRequested : 1;

    /* Guest additions flags: */
    bool m_fIsGuestAdditionsActive : 1;
    bool m_fIsGuestSupportsGraphics : 1;
    bool m_fIsGuestSupportsSeamless : 1;

    /* Keyboard flags: */
    bool m_fNumLock : 1;
    bool m_fCapsLock : 1;
    bool m_fScrollLock : 1;
    uint m_uNumLockAdaptionCnt;
    uint m_uCapsLockAdaptionCnt;

    /* Mouse flags: */
    bool m_fIsMouseSupportsAbsolute : 1;
    bool m_fIsMouseSupportsRelative : 1;
    bool m_fIsMouseHostCursorNeeded : 1;
    bool m_fIsMouseCaptured : 1;
    bool m_fIsMouseIntegrated : 1;
    bool m_fIsValidPointerShapePresent : 1;
    bool m_fIsHidingHostPointer : 1;

    /* Friend classes: */
    friend class UIConsoleCallback;
};

#endif // !___UIConsole_h___

