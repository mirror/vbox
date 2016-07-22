/* $Id$ */
/** @file
 * VBox Qt GUI - UIKeyboardHandler class declaration.
 */

/*
 * Copyright (C) 2010-2016 Oracle Corporation
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
#include <QMap>
#include <QObject>

/* GUI includes: */
#include "UIExtraDataDefs.h"

/* Other VBox includes: */
#include <VBox/com/defs.h>

/* External includes: */
#ifdef VBOX_WS_MAC
# include <Carbon/Carbon.h>
# include <CoreFoundation/CFBase.h>
#endif /* VBOX_WS_MAC */

/* Forward declarations: */
class QWidget;
class VBoxGlobalSettings;
class UIActionPool;
class UISession;
class UIMachineLogic;
class UIMachineWindow;
class UIMachineView;
class CKeyboard;
#ifdef VBOX_WS_WIN
class WinAltGrMonitor;
#endif /* VBOX_WS_WIN */
#ifdef VBOX_WS_X11
# if QT_VERSION < 0x050000
typedef union _XEvent XEvent;
# endif /* QT_VERSION < 0x050000 */
#endif /* VBOX_WS_X11 */
#if QT_VERSION >= 0x050000
class KeyboardHandlerEventFilter;
#endif /* QT_VERSION >= 0x050000 */


/* Delegate to control VM keyboard functionality: */
class UIKeyboardHandler : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies listeners about state-change. */
    void sigStateChange(int iState);

public:

    /* Factory functions to create/destroy keyboard-handler: */
    static UIKeyboardHandler* create(UIMachineLogic *pMachineLogic, UIVisualStateType visualStateType);
    static void destroy(UIKeyboardHandler *pKeyboardHandler);

    /* Prepare/cleanup listeners: */
    void prepareListener(ulong uScreenId, UIMachineWindow *pMachineWindow);
    void cleanupListener(ulong uScreenId);

    /* Commands to capture/release keyboard: */
#ifdef VBOX_WS_X11
# if QT_VERSION < 0x050000
    bool checkForX11FocusEvents(unsigned long hWindow);
# endif /* QT_VERSION < 0x050000 */
#endif /* VBOX_WS_X11 */
    void captureKeyboard(ulong uScreenId);
    void releaseKeyboard();
    void releaseAllPressedKeys(bool aReleaseHostKey = true);

    /* Current keyboard state: */
    int state() const;

    /* Some getters required by side-code: */
    bool isHostKeyPressed() const { return m_bIsHostComboPressed; }
#ifdef VBOX_WS_MAC
    bool isHostKeyAlone() const { return m_bIsHostComboAlone; }
    bool isKeyboardGrabbed() const { return m_iKeyboardHookViewIndex != -1; }
#endif /* VBOX_WS_MAC */

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* For the debugger. */
    void setDebuggerActive(bool aActive = true);
#endif

#ifdef VBOX_WS_WIN
    /** Tells the keyboard event handler to skip host keyboard events.
      * Used for HID LEDs sync when on Windows host a keyboard event
      * is generated in order to change corresponding LED. */
    void winSkipKeyboardEvents(bool fSkip);
#endif /* VBOX_WS_WIN */

#if QT_VERSION < 0x050000
# if defined(VBOX_WS_MAC)
    /** Qt4: Mac: Performs final pre-processing of all the native events. */
    bool macEventFilter(const void *pvCocoaEvent, EventRef event, ulong uScreenId);
# elif defined(VBOX_WS_WIN)
    /** Qt4: Win: Performs final pre-processing of all the native events. */
    bool winEventFilter(MSG *pMsg, ulong uScreenId);
# elif defined(VBOX_WS_X11)
    /** Qt4: X11: Performs final pre-processing of all the native events. */
    bool x11EventFilter(XEvent *pEvent, ulong uScreenId);
# endif /* VBOX_WS_X11 */
#else /* QT_VERSION >= 0x050000 */
    /** Qt5: Performs pre-processing of all the native events. */
    bool nativeEventPreprocessor(const QByteArray &eventType, void *pMessage);
    /** Qt5: Performs post-processing of all the native events. */
    bool nativeEventPostprocessor(void *pMessage, ulong uScreenId);
#endif /* QT_VERSION >= 0x050000 */

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
    UIActionPool* actionPool() const;
    UISession* uisession() const;

    /** Returns the console's keyboard reference. */
    CKeyboard& keyboard() const;

    /* Event handler for registered machine-view(s): */
    bool eventFilter(QObject *pWatchedObject, QEvent *pEvent);

#if defined(VBOX_WS_MAC)
    /** Mac: Performs initial pre-processing of all the native keyboard events. */
    static bool macKeyboardProc(const void *pvCocoaEvent, const void *pvCarbonEvent, void *pvUser);
    /** Mac: Performs initial pre-processing of all the native keyboard events. */
    bool macKeyboardEvent(const void *pvCocoaEvent, EventRef inEvent);
#elif defined(VBOX_WS_WIN)
    /** Win: Performs initial pre-processing of all the native keyboard events. */
    static LRESULT CALLBACK winKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    /** Win: Performs initial pre-processing of all the native keyboard events. */
    bool winKeyboardEvent(UINT msg, const KBDLLHOOKSTRUCT &event);
#endif /* VBOX_WS_WIN */

    bool keyEventCADHandled(uint8_t uScan);
    bool keyEventHandleNormal(int iKey, uint8_t uScan, int fFlags, LONG *pCodes, uint *puCodesCount);
    bool keyEventHostComboHandled(int iKey, wchar_t *pUniKey, bool isHostComboStateChanged, bool *pfResult);
    void keyEventHandleHostComboRelease(ulong uScreenId);
    void keyEventReleaseHostComboKeys(const CKeyboard &keyboard);
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
#if defined(VBOX_WS_X11) && QT_VERSION >= 0x050000
    /* Holds the index of the screen to capture keyboard when ready. */
    int m_idxDelayedKeyboardCaptureView;
#endif /* VBOX_WS_X11 && QT_VERSION >= 0x050000 */
    const VBoxGlobalSettings &m_globalSettings;

    uint8_t m_pressedKeys[128];
    uint8_t m_pressedKeysCopy[128];

    QMap<int, uint8_t> m_pressedHostComboKeys;

    bool m_fIsKeyboardCaptured : 1;
    bool m_bIsHostComboPressed : 1;
    bool m_bIsHostComboAlone : 1;
    bool m_bIsHostComboProcessed : 1;
    bool m_fPassCADtoGuest : 1;
    /** Whether the debugger is active.
     * Currently only affects auto capturing. */
    bool m_fDebuggerActive : 1;

    /** Holds the keyboard hook view index. */
    int m_iKeyboardHookViewIndex;

#if defined(VBOX_WS_MAC)
    /** Mac: Holds the current modifiers key mask. */
    UInt32 m_uDarwinKeyModifiers;
#elif defined(VBOX_WS_WIN)
    /** Win: Currently this is used in winKeyboardEvent() only. */
    bool m_fIsHostkeyInCapture;
    /** Win: Holds whether the keyboard event filter should ignore keyboard events. */
    bool m_fSkipKeyboardEvents;
    /** Win: Holds the keyboard hook instance. */
    HHOOK m_keyboardHook;
    /** Win: Holds the object monitoring key event stream for problematic AltGr events. */
    WinAltGrMonitor *m_pAltGrMonitor;
    /** Win: Holds the keyboard handler reference to be accessible from the keyboard hook. */
    static UIKeyboardHandler *m_spKeyboardHandler;
#endif /* VBOX_WS_WIN */

#if QT_VERSION >= 0x050000
    /** Win: Holds the native event filter instance. */
    KeyboardHandlerEventFilter *m_pPrivateEventFilter;
    /** Win: Allows the native event filter to
      * redirect events directly to nativeEventPreprocessor handler. */
    friend class KeyboardHandlerEventFilter;
#endif /* QT_VERSION >= 0x050000 */

    ULONG m_cMonitors;
};

#endif // !___UIKeyboardHandler_h___

