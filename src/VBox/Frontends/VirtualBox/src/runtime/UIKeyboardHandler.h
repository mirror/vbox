/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIKeyboardHandler class declaration
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

#ifndef ___UIKeyboardHandler_h___
#define ___UIKeyboardHandler_h___

/* Qt includes: */
#include <QObject>
#include <QMap>

/* GUI includes: */
#include "UIMachineDefs.h"
#ifdef Q_WS_MAC
# include <CoreFoundation/CFBase.h>
# include <Carbon/Carbon.h>
#endif /* Q_WS_MAC */

/* COM includes: */
#include "CKeyboard.h"

/* Forward declarations: */
class QWidget;
class CSession;
class UISession;
class UIMachineLogic;
class UIMachineWindow;
class UIMachineView;
class VBoxGlobalSettings;
#ifdef Q_WS_X11
typedef union  _XEvent XEvent;
#endif /* Q_WS_X11 */

/* Delegate to control VM keyboard functionality: */
class UIKeyboardHandler : public QObject
{
    Q_OBJECT;

public:

    /* Factory functions to create/destroy keyboard-handler: */
    static UIKeyboardHandler* create(UIMachineLogic *pMachineLogic, UIVisualStateType visualStateType);
    static void destroy(UIKeyboardHandler *pKeyboardHandler);

    /* Prepare/cleanup listeners: */
    void prepareListener(ulong uIndex, UIMachineWindow *pMachineWindow);
    void cleanupListener(ulong uIndex);

    /* Commands to capture/release keyboard: */
    void captureKeyboard(ulong uScreenId);
    void releaseKeyboard();
    void releaseAllPressedKeys(bool aReleaseHostKey = true);

    /* Current keyboard state: */
    int keyboardState() const;

    /* Some getters required by side-code: */
    bool isHostKeyPressed() const { return m_bIsHostComboPressed; }
#ifdef Q_WS_MAC
    bool isHostKeyAlone() const { return m_bIsHostComboAlone; }
    bool isKeyboardGrabbed() const { return m_fKeyboardGrabbed; }
#endif /* Q_WS_MAC */

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* For the debugger. */
    void setDebuggerActive(bool aActive = true);
#endif

    /* External event-filters: */
#if defined(Q_WS_WIN)
    bool winEventFilter(MSG *pMsg, ulong uScreenId);
#elif defined(Q_WS_X11)
    bool x11EventFilter(XEvent *pEvent, ulong uScreenId);
#endif

signals:

    /* Notifies listeners about keyboard state-change: */
    void keyboardStateChanged(int iNewState);

protected slots:

    /* Machine state-change handler: */
    virtual void sltMachineStateChanged();

protected:

    /* Keyboard-handler constructor/destructor: */
    UIKeyboardHandler(UIMachineLogic *pMachineLogic);
    virtual ~UIKeyboardHandler();

    /* Prepare helpers: */
    virtual void prepareCommon();
    virtual void loadSettings();

    /* Cleanup helpers: */
    //virtual void saveSettings() {}
    virtual void cleanupCommon();

    /* Common getters: */
    UIMachineLogic* machineLogic() const;
    UISession* uisession() const;
    CSession& session() const;

    /* Event handler for registered machine-view(s): */
    bool eventFilter(QObject *pWatchedObject, QEvent *pEvent);
#if defined(Q_WS_WIN)
    static LRESULT CALLBACK lowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    bool winLowKeyboardEvent(UINT msg, const KBDLLHOOKSTRUCT &event);
#elif defined(Q_WS_MAC)
    void darwinGrabKeyboardEvents(bool fGrab);
    bool darwinKeyboardEvent(const void *pvCocoaEvent, EventRef inEvent);
    static bool darwinEventHandlerProc(const void *pvCocoaEvent, const void *pvCarbonEvent, void *pvUser);
#endif

    bool keyEventCADHandled(uint8_t uScan);
    bool keyEventHandleNormal(int iKey, uint8_t uScan, int fFlags, LONG *pCodes, uint *puCodesCount);
    bool keyEventHostComboHandled(int iKey, wchar_t *pUniKey, bool isHostComboStateChanged, bool *pfResult);
    void keyEventHandleHostComboRelease(ulong uScreenId);
    void keyEventReleaseHostComboKeys(CKeyboard keyboard);
    /* Separate function to handle most of existing keyboard-events: */
    bool keyEvent(int iKey, uint8_t uScan, int fFlags, ulong uScreenId, wchar_t *pUniKey = 0);
    bool processHotKey(int iHotKey, wchar_t *pUniKey);

    /* Private helpers: */
    void fixModifierState(LONG *piCodes, uint *puCount);
    void saveKeyStates();
    void sendChangedKeyStates();
    bool isAutoCaptureDisabled();
    void setAutoCaptureDisabled(bool fIsAutoCaptureDisabled);
    bool autoCaptureSetGlobally();
    bool viewHasFocus(ulong uScreenId);
    bool isSessionRunning();

    UIMachineWindow* isItListenedWindow(QObject *pWatchedObject) const;
    UIMachineView* isItListenedView(QObject *pWatchedObject) const;

    /* Machine logic parent: */
    UIMachineLogic *m_pMachineLogic;

    /* Registered machine-window(s): */
    QMap<ulong, UIMachineWindow*> m_windows;
    /* Registered machine-view(s): */
    QMap<ulong, UIMachineView*> m_views;

    /* Other keyboard variables: */
    int m_iKeyboardCaptureViewIndex;
    const VBoxGlobalSettings &m_globalSettings;

    uint8_t m_pressedKeys[128];
    uint8_t m_pressedKeysCopy[128];

    QMap<int, uint8_t> m_pressedHostComboKeys;

    bool m_fIsKeyboardCaptured : 1;
    bool m_bIsHostComboPressed : 1;
    bool m_bIsHostComboAlone : 1;
    bool m_bIsHostComboProcessed : 1;
    bool m_fPassCAD : 1;
    /** Whether the debugger is active.
     * Currently only affects auto capturing. */
    bool m_fDebuggerActive : 1;

#if defined(Q_WS_WIN)
    /* Currently this is used in winLowKeyboardEvent() only: */
    bool m_bIsHostkeyInCapture;
    /* Keyboard hook required to capture keyboard event under windows. */
    static UIKeyboardHandler *m_spKeyboardHandler;
    HHOOK m_keyboardHook;
    int m_iKeyboardHookViewIndex;
#elif defined(Q_WS_MAC)
    /* The current modifier key mask. Used to figure out which modifier
     * key was pressed when we get a kEventRawKeyModifiersChanged event. */
    UInt32 m_darwinKeyModifiers;
    bool m_fKeyboardGrabbed;
    int m_iKeyboardGrabViewIndex;
#endif
};

#endif // !___UIKeyboardHandler_h___

