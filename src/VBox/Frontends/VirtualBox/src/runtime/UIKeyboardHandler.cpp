/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIKeyboardHandler class implementation
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

/* Global includes */
#include <QKeyEvent>

/* Local includes */
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "UIActionsPool.h"
#include "UIKeyboardHandlerNormal.h"
#include "UIKeyboardHandlerFullscreen.h"
#include "UIKeyboardHandlerSeamless.h"
#include "UIKeyboardHandlerScale.h"
#include "UIMouseHandler.h"
#include "UISession.h"
#include "UIMachineLogic.h"
#include "UIMachineWindow.h"
#include "UIMachineView.h"
#include "UIHotKeyEditor.h"

#ifdef Q_WS_X11
# include <QX11Info>
# include <X11/XKBlib.h>
# include <X11/keysym.h>
# ifdef KeyPress
const int XFocusOut = FocusOut;
const int XFocusIn = FocusIn;
const int XKeyPress = KeyPress;
const int XKeyRelease = KeyRelease;
#  undef KeyRelease
#  undef KeyPress
#  undef FocusOut
#  undef FocusIn
# endif /* KeyPress */
# include "XKeyboard.h"
#endif /* Q_WS_X11 */

#ifdef Q_WS_MAC
# include "VBoxUtils-darwin.h"
# include "DarwinKeyboard.h"
# include "UICocoaApplication.h"
# include <Carbon/Carbon.h>
#endif /* Q_WS_MAC */

/* Enums representing different keyboard-states: */
enum { KeyExtended = 0x01, KeyPressed = 0x02, KeyPause = 0x04, KeyPrint = 0x08 };
enum { IsKeyPressed = 0x01, IsExtKeyPressed = 0x02, IsKbdCaptured = 0x80 };

#ifdef Q_WS_WIN
UIKeyboardHandler* UIKeyboardHandler::m_spKeyboardHandler = 0;
#endif /* Q_WS_WIN */

/* Factory function to create keyboard-handler: */
UIKeyboardHandler* UIKeyboardHandler::create(UIMachineLogic *pMachineLogic,
                                             UIVisualStateType visualStateType)
{
    /* Prepare keyboard-handler: */
    UIKeyboardHandler *pKeyboardHandler = 0;
    /* Depending on visual-state type: */
    switch (visualStateType)
    {
        case UIVisualStateType_Normal:
            pKeyboardHandler = new UIKeyboardHandlerNormal(pMachineLogic);
            break;
        case UIVisualStateType_Fullscreen:
            pKeyboardHandler = new UIKeyboardHandlerFullscreen(pMachineLogic);
            break;
        case UIVisualStateType_Seamless:
            pKeyboardHandler = new UIKeyboardHandlerSeamless(pMachineLogic);
            break;
        case UIVisualStateType_Scale:
            pKeyboardHandler = new UIKeyboardHandlerScale(pMachineLogic);
            break;
        default:
            break;
    }
#ifdef Q_WS_WIN
    /* Its required to have static pointer to created handler
     * because windows keyboard-hook works only with static members: */
    m_spKeyboardHandler = pKeyboardHandler;
#endif /* Q_WS_WIN */
    /* Return prepared keyboard-handler: */
    return pKeyboardHandler;
}

/* Factory function to destroy keyboard-handler: */
void UIKeyboardHandler::destroy(UIKeyboardHandler *pKeyboardHandler)
{
    /* Delete keyboard-handler: */
#ifdef Q_WS_WIN
    m_spKeyboardHandler = 0;
#endif /* Q_WS_WIN */
    delete pKeyboardHandler;
}

/* Prepare listened objects: */
void UIKeyboardHandler::prepareListener(ulong uIndex, UIMachineWindow *pMachineWindow)
{
    /* If that window is NOT registered yet: */
    if (!m_windows.contains(uIndex))
    {
        /* Add window: */
        m_windows.insert(uIndex, pMachineWindow);
        /* Install event-filter for window: */
        m_windows[uIndex]->machineWindow()->installEventFilter(this);
    }

    /* If that view is NOT registered yet: */
    if (!m_views.contains(uIndex))
    {
        /* Add view: */
        m_views.insert(uIndex, pMachineWindow->machineView());
        /* Install event-filter for view: */
        m_views[uIndex]->installEventFilter(this);
    }
}

/* Cleanup listened objects: */
void UIKeyboardHandler::cleanupListener(ulong uIndex)
{
    /* Check if we should release keyboard first: */
    if ((int)uIndex == m_iKeyboardCaptureViewIndex)
        releaseKeyboard();

    /* If window still registered: */
    if (m_windows.contains(uIndex))
    {
        /* Remove window: */
        m_windows.remove(uIndex);
    }

    /* If view still registered: */
    if (m_views.contains(uIndex))
    {
        /* Remove view: */
        m_views.remove(uIndex);
    }
}

void UIKeyboardHandler::captureKeyboard(ulong uScreenId)
{
    /* Do NOT capture keyboard if its captured already: */
    if (m_fIsKeyboardCaptured)
        return;

    /* If such view exists: */
    if (m_views.contains(uScreenId))
    {
        /* Store new keyboard-captured state value: */
        m_fIsKeyboardCaptured = true;

        /* Remember which screen had captured keyboard: */
        m_iKeyboardCaptureViewIndex = uScreenId;

#if defined(Q_WS_WIN)
        /* On Win, keyboard grabbing is ineffective, a low-level keyboard hook is used instead. */
#elif defined(Q_WS_X11)
        /* On X11, we are using passive XGrabKey for normal (windowed) mode
         * instead of XGrabKeyboard (called by QWidget::grabKeyboard())
         * because XGrabKeyboard causes a problem under metacity - a window cannot be moved
         * using the mouse if it is currently actively grabbing the keyboard;
         * For static modes we are using usual (active) keyboard grabbing. */
        switch (machineLogic()->visualStateType())
        {
            /* If window is moveable we are making passive keyboard grab: */
            case UIVisualStateType_Normal:
            case UIVisualStateType_Scale:
            {
                XGrabKey(QX11Info::display(), AnyKey, AnyModifier, m_windows[m_iKeyboardCaptureViewIndex]->machineWindow()->winId(), False, GrabModeAsync, GrabModeAsync);
                break;
            }
            /* If window is NOT moveable we are making active keyboard grab: */
            case UIVisualStateType_Fullscreen:
            case UIVisualStateType_Seamless:
            {
                /* Keyboard grabbing can fail because of some keyboard shortcut is still grabbed by window manager.
                 * We can't be sure this shortcut will be released at all, so we will retry to grab keyboard for 50 times,
                 * and after we will just ignore that issue: */
                int cTriesLeft = 50;
                while (cTriesLeft && XGrabKeyboard(QX11Info::display(), m_windows[m_iKeyboardCaptureViewIndex]->machineWindow()->winId(), False, GrabModeAsync, GrabModeAsync, CurrentTime)) { --cTriesLeft; }
                break;
            }
            /* Should we try to grab keyboard in default case? I think - NO. */
            default:
                break;
        }
#elif defined(Q_WS_MAC)
        /* On Mac, we use the Qt methods + disabling global hot keys + watching modifiers (for right/left separation). */
        ::DarwinDisableGlobalHotKeys(true);
        m_views[m_iKeyboardCaptureViewIndex]->grabKeyboard();
#else
        /* On other platforms we are just praying Qt method will work. */
        m_views[m_iKeyboardCaptureViewIndex]->grabKeyboard();
#endif

        /* Notify all the listeners: */
        emit keyboardStateChanged(keyboardState());
    }
}

void UIKeyboardHandler::releaseKeyboard()
{
    /* Do NOT capture keyboard if its captured already: */
    if (!m_fIsKeyboardCaptured)
        return;

    /* If such view exists: */
    if (m_views.contains(m_iKeyboardCaptureViewIndex))
    {
        /* Store new keyboard-captured state value: */
        m_fIsKeyboardCaptured = false;

#if defined(Q_WS_WIN)
        /* On Win, keyboard grabbing is ineffective, a low-level keyboard hook is used instead. */
#elif defined(Q_WS_X11)
        /* On X11, we are using passive XGrabKey for normal (windowed) mode
         * instead of XGrabKeyboard (called by QWidget::grabKeyboard())
         * because XGrabKeyboard causes a problem under metacity - a window cannot be moved
         * using the mouse if it is currently actively grabbing the keyboard;
         * For static modes we are using usual (active) keyboard grabbing. */
        switch (machineLogic()->visualStateType())
        {
            /* If window is moveable we are making passive keyboard ungrab: */
            case UIVisualStateType_Normal:
            case UIVisualStateType_Scale:
            {
                XUngrabKey(QX11Info::display(), AnyKey, AnyModifier, m_windows[m_iKeyboardCaptureViewIndex]->machineWindow()->winId());
                break;
            }
            /* If window is NOT moveable we are making active keyboard ungrab: */
            case UIVisualStateType_Fullscreen:
            case UIVisualStateType_Seamless:
            {
                XUngrabKeyboard(QX11Info::display(), CurrentTime);
                break;
            }
            /* Should we try to release keyboard in default case? I think - NO. */
            default:
                break;
        }
#elif defined(Q_WS_MAC)
        ::DarwinDisableGlobalHotKeys(false);
        m_views[m_iKeyboardCaptureViewIndex]->releaseKeyboard();
#else
        m_views[m_iKeyboardCaptureViewIndex]->releaseKeyboard();
#endif

        /* Reset keyboard-capture index: */
        m_iKeyboardCaptureViewIndex = -1;

        /* Notify all the listeners: */
        emit keyboardStateChanged(keyboardState());
    }
}

void UIKeyboardHandler::releaseAllPressedKeys(bool aReleaseHostKey /* = true */)
{
    CKeyboard keyboard = session().GetConsole().GetKeyboard();
    bool fSentRESEND = false;

    /* Send a dummy scan code (RESEND) to prevent the guest OS from recognizing
     * a single key click (for ex., Alt) and performing an unwanted action
     * (for ex., activating the menu) when we release all pressed keys below.
     * Note, that it's just a guess that sending RESEND will give the desired
     * effect :), but at least it works with NT and W2k guests. */
    for (uint i = 0; i < SIZEOF_ARRAY (m_pressedKeys); i++)
    {
        if (m_pressedKeys[i] & IsKeyPressed)
        {
            if (!fSentRESEND)
            {
                keyboard.PutScancode (0xFE);
                fSentRESEND = true;
            }
            keyboard.PutScancode(i | 0x80);
        }
        else if (m_pressedKeys[i] & IsExtKeyPressed)
        {
            if (!fSentRESEND)
            {
                keyboard.PutScancode(0xFE);
                fSentRESEND = true;
            }
            QVector <LONG> codes(2);
            codes[0] = 0xE0;
            codes[1] = i | 0x80;
            keyboard.PutScancodes(codes);
        }
        m_pressedKeys[i] = 0;
    }

    if (aReleaseHostKey)
    {
        m_bIsHostComboPressed = false;
        m_pressedHostComboKeys.clear();
    }

#ifdef Q_WS_MAC
    unsigned int hostComboModifierMask = 0;
    QList<int> hostCombo = UIHotKeyCombination::toKeyCodeList(m_globalSettings.hostCombo());
    for (int i = 0; i < hostCombo.size(); ++i)
        hostComboModifierMask |= ::DarwinKeyCodeToDarwinModifierMask(hostCombo.at(i));
    /* Clear most of the modifiers: */
    m_darwinKeyModifiers &=
        alphaLock | kEventKeyModifierNumLockMask |
        (aReleaseHostKey ? 0 : hostComboModifierMask);
#endif

    emit keyboardStateChanged(keyboardState());
}

/* Current keyboard state: */
int UIKeyboardHandler::keyboardState() const
{
    return (m_fIsKeyboardCaptured ? UIViewStateType_KeyboardCaptured : 0) |
           (m_bIsHostComboPressed ? UIViewStateType_HostKeyPressed : 0);
}

#if defined(Q_WS_WIN)

bool UIKeyboardHandler::winEventFilter(MSG *pMsg, ulong uScreenId)
{
    /* Check if some system event should be filtered-out.
     * Returning 'true' means filtering-out,
     * Returning 'false' means passing event to Qt. */
    bool fResult = false; /* Pass to Qt by default: */
    switch (pMsg->message)
    {
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        {
            /* Check for the special flag possibly set at the end of this function: */
            if (pMsg->lParam & (0x1 << 25))
            {
                pMsg->lParam &= ~(0x1 << 25);
                fResult = false;
                break;
            }

            /* Scancodes 0x80 and 0x00 are ignored: */
            unsigned scan = (pMsg->lParam >> 16) & 0x7F;
            if (!scan)
            {
                fResult = true;
                break;
            }

            int vkey = pMsg->wParam;

            int flags = 0;
            if (pMsg->lParam & 0x1000000)
                flags |= KeyExtended;
            if (!(pMsg->lParam & 0x80000000))
                flags |= KeyPressed;

            /* Check for special Korean keys. Based on the keyboard layout selected
             * on the host, the scancode in lParam might be 0x71/0x72 or 0xF1/0xF2.
             * In either case, we must deliver 0xF1/0xF2 scancode to the guest when
             * the key is pressed and nothing when it's released. */
            if (scan == 0x71 || scan == 0x72)
            {
                scan |= 0x80;
                flags = KeyPressed;     /* Because a release would be ignored. */
                vkey  = VK_PROCESSKEY;  /* In case it was 0xFF */
            }

            /* When one of the SHIFT keys is held and one of the cursor movement
             * keys is pressed, Windows duplicates SHIFT press/release messages,
             * but with the virtual key code set to 0xFF. These virtual keys are also
             * sent in some other situations (Pause, PrtScn, etc.). Ignore suc messages. */
            if (vkey == 0xFF)
            {
                fResult = true;
                break;
            }

            switch (vkey)
            {
                case VK_SHIFT:
                case VK_CONTROL:
                case VK_MENU:
                {
                    /* Overcome Win32 modifier key generalization: */
                    int keyscan = scan;
                    if (flags & KeyExtended)
                        keyscan |= 0xE000;
                    switch (keyscan)
                    {
                        case 0x002A: vkey = VK_LSHIFT; break;
                        case 0x0036: vkey = VK_RSHIFT; break;
                        case 0x001D: vkey = VK_LCONTROL; break;
                        case 0xE01D: vkey = VK_RCONTROL; break;
                        case 0x0038: vkey = VK_LMENU; break;
                        case 0xE038: vkey = VK_RMENU; break;
                    }
                    break;
                }
                case VK_NUMLOCK:
                    /* Win32 sets the extended bit for the NumLock key. Reset it: */
                    flags &= ~KeyExtended;
                    break;
                case VK_SNAPSHOT:
                    flags |= KeyPrint;
                    break;
                case VK_PAUSE:
                    flags |= KeyPause;
                    break;
            }

            bool result = keyEvent(vkey, scan, flags, uScreenId);
            if (!result && m_fIsKeyboardCaptured)
            {
                /* keyEvent() returned that it didn't process the message, but since the
                 * keyboard is captured, we don't want to pass it to Windows. We just want
                 * to let Qt process the message (to handle non-alphanumeric <HOST>+key
                 * shortcuts for example). So send it directly to the window with the
                 * special flag in the reserved area of lParam (to avoid recursion). */
                ::SendMessage(pMsg->hwnd, pMsg->message,
                              pMsg->wParam, pMsg->lParam | (0x1 << 25));
                fResult = true;
                break;
            }

            /* These special keys have to be handled by Windows as well to update the
             * internal modifier state and to enable/disable the keyboard LED */
            if (vkey == VK_NUMLOCK || vkey == VK_CAPITAL || vkey == VK_LSHIFT || vkey == VK_RSHIFT)
            {
                fResult = false;
                break;
            }

            fResult = result;
            break;
        }
        default:
            break;
    }
    /* Return result: */
    return fResult;
}

#elif defined(Q_WS_X11)

static Bool UIKeyboardHandlerCompEvent(Display*, XEvent *pEvent, XPointer pvArg)
{
    XEvent *pKeyEvent = (XEvent*)pvArg;
    if ((pEvent->type == XKeyPress) && (pEvent->xkey.keycode == pKeyEvent->xkey.keycode))
        return True;
    else
        return False;
}

bool UIKeyboardHandler::x11EventFilter(XEvent *pEvent, ulong uScreenId)
{
    /* Check if some system event should be filtered-out.
     * Returning 'true' means filtering-out,
     * Returning 'false' means passing event to Qt. */
    bool fResult = false; /* Pass to Qt by default: */
    switch (pEvent->type)
    {
        /* We have to handle XFocusOut right here as this event is not passed to UIMachineView::event().
         * Handling this event is important for releasing the keyboard before the screen saver gets active.
         * See public ticket #3894: Apparently this makes problems with newer versions of Qt
         * and this hack is probably not necessary anymore. So disable it for Qt >= 4.5.0. */
        case XFocusOut:
        case XFocusIn:
        {
            if (isSessionRunning())
            {
                if (VBoxGlobal::qtRTVersion() < ((4 << 16) | (5 << 8) | 0))
                {
                    if (pEvent->type == XFocusIn)
                    {
                        /* Capture keyboard by chosen view number: */
                        captureKeyboard(uScreenId);
                        /* Reset the single-time disable capture flag: */
                        if (isAutoCaptureDisabled())
                            setAutoCaptureDisabled(false);
                    }
                    else
                    {
                        /* Release keyboard: */
                        releaseKeyboard();
                        /* And all pressed keys including host-one: */
                        releaseAllPressedKeys(true);
                    }
                }
            }
            fResult = false;
        }
        case XKeyPress:
        case XKeyRelease:
        {
            /* Translate the keycode to a PC scan code. */
            unsigned scan = handleXKeyEvent(pEvent);

            /* Scancodes 0x00 (no valid translation) and 0x80 are ignored: */
            if (!scan & 0x7F)
            {
                fResult = true;
                break;
            }

            /* Fix for http://www.virtualbox.org/ticket/1296:
             * when X11 sends events for repeated keys, it always inserts an XKeyRelease before the XKeyPress. */
            XEvent returnEvent;
            if ((pEvent->type == XKeyRelease) && (XCheckIfEvent(pEvent->xkey.display, &returnEvent,
                UIKeyboardHandlerCompEvent, (XPointer)pEvent) == True))
            {
                XPutBackEvent(pEvent->xkey.display, &returnEvent);
                fResult = true;
                break;
            }

            KeySym ks = ::XKeycodeToKeysym(pEvent->xkey.display, pEvent->xkey.keycode, 0);

            int flags = 0;
            if (scan >> 8)
                flags |= KeyExtended;
            if (pEvent->type == XKeyPress)
                flags |= KeyPressed;

            /* Remove the extended flag: */
            scan &= 0x7F;

            /* Special Korean keys must send scancode 0xF1/0xF2 when pressed and nothing
             * when released.
             */
            if (scan == 0x71 || scan == 0x72)
            {
                if (pEvent->type == XKeyRelease)  /* Ignore. */
                {
                    fResult = true;
                    break;
                }
                scan |= 0x80;   /* Re-create the bizarre scancode. */
            }

            switch (ks)
            {
                case XK_Print:
                    flags |= KeyPrint;
                    break;
                case XK_Pause:
                    if (pEvent->xkey.state & ControlMask) /* Break */
                    {
                        ks = XK_Break;
                        flags |= KeyExtended;
                        scan = 0x46;
                    }
                    else
                        flags |= KeyPause;
                    break;
            }

            fResult = keyEvent(ks, scan, flags, uScreenId);
        }
        default:
            break;
    }
    /* Return result: */
    return fResult;
}

#endif

/* Machine state-change handler: */
void UIKeyboardHandler::sltMachineStateChanged()
{
    /* Get machine state: */
    KMachineState state = uisession()->machineState();
    /* Handle particular machine states: */
    switch (state)
    {
        case KMachineState_Paused:
        case KMachineState_TeleportingPausedVM:
        case KMachineState_Stuck:
        {
            /* Release the keyboard: */
            releaseKeyboard();
            /* And all pressed keys except the host-one : */
            releaseAllPressedKeys(false /* release host-key? */);
            break;
        }
        case KMachineState_Running:
        {
            /* Capture the keyboard by the first focused view: */
            QList<ulong> theListOfViewIds = m_views.keys();
            for (int i = 0; i < theListOfViewIds.size(); ++i)
            {
                if (viewHasFocus(theListOfViewIds[i]))
                {
                    /* Capture keyboard: */
#ifdef Q_WS_WIN
                    if (!isAutoCaptureDisabled() && autoCaptureSetGlobally() &&
                        GetAncestor(m_views[theListOfViewIds[i]]->winId(), GA_ROOT) == GetForegroundWindow())
#else /* Q_WS_WIN */
                    if (!isAutoCaptureDisabled() && autoCaptureSetGlobally())
#endif /* !Q_WS_WIN */
                        captureKeyboard(theListOfViewIds[i]);
                    /* Reset the single-time disable capture flag: */
                    if (isAutoCaptureDisabled())
                        setAutoCaptureDisabled(false);
                    break;
                }
            }
            break;
        }
        default:
            break;
    }
}

/* Keyboard-handler constructor: */
UIKeyboardHandler::UIKeyboardHandler(UIMachineLogic *pMachineLogic)
    : QObject(pMachineLogic)
    , m_pMachineLogic(pMachineLogic)
    , m_iKeyboardCaptureViewIndex(-1)
    , m_globalSettings(vboxGlobal().settings())
    , m_fIsKeyboardCaptured(false)
    , m_bIsHostComboPressed(false)
    , m_bIsHostComboAlone(false)
    , m_bIsHostComboProcessed(false)
    , m_fPassCAD(false)
#if defined(Q_WS_WIN)
    , m_bIsHostkeyInCapture(false)
    , m_iKeyboardHookViewIndex(-1)
#elif defined(Q_WS_MAC)
    , m_darwinKeyModifiers(0)
    , m_fKeyboardGrabbed(false)
    , m_iKeyboardGrabViewIndex(-1)
#endif
{
    /* Prepare: */
    prepareCommon();

    /* Load settings: */
    loadSettings();

    /* Initialize: */
    sltMachineStateChanged();
}

/* Keyboard-handler destructor: */
UIKeyboardHandler::~UIKeyboardHandler()
{
    /* Cleanup: */
    cleanupCommon();
}

void UIKeyboardHandler::prepareCommon()
{
    /* Machine state-change updater: */
    connect(uisession(), SIGNAL(sigMachineStateChange()), this, SLOT(sltMachineStateChanged()));

    /* Pressed keys: */
    ::memset(m_pressedKeys, 0, sizeof(m_pressedKeys));
}

void UIKeyboardHandler::loadSettings()
{
    /* Global settings: */
#ifdef Q_WS_X11
    /* Initialize the X keyboard subsystem: */
    initMappedX11Keyboard(QX11Info::display(), vboxGlobal().settings().publicProperty("GUI/RemapScancodes"));
#endif

    /* Extra data settings: */
    {
        /* CAD settings: */
        QString passCAD = session().GetConsole().GetMachine().GetExtraData(VBoxDefs::GUI_PassCAD);
        if (!passCAD.isEmpty() && passCAD != "false" && passCAD != "no")
            m_fPassCAD = true;
    }
}

void UIKeyboardHandler::cleanupCommon()
{
#if defined(Q_WS_WIN)
    /* Cleaning keyboard-hook: */
    if (m_keyboardHook)
    {
        UnhookWindowsHookEx(m_keyboardHook);
        m_keyboardHook = NULL;
    }
#elif defined(Q_WS_MAC)
    /* We have to make sure the callback for the keyboard events
     * is released when closing this view. */
    if (m_fKeyboardGrabbed)
        darwinGrabKeyboardEvents(false);
#endif
}

/* Machine-logic getter: */
UIMachineLogic* UIKeyboardHandler::machineLogic() const
{
    return m_pMachineLogic;
}

/* UI Session getter: */
UISession* UIKeyboardHandler::uisession() const
{
    return machineLogic()->uisession();
}

/* Main Session getter: */
CSession& UIKeyboardHandler::session() const
{
    return uisession()->session();
}

/* Event handler for prepared listener(s): */
bool UIKeyboardHandler::eventFilter(QObject *pWatchedObject, QEvent *pEvent)
{
    /* Check if pWatchedObject object is window: */
    if (UIMachineWindow *pWatchedWindow = isItListenedWindow(pWatchedObject))
    {
        /* Get corresponding screen index: */
        ulong uScreenId = m_windows.key(pWatchedWindow);
        NOREF(uScreenId);
        /* Handle window events: */
        switch (pEvent->type())
        {
#if defined(Q_WS_WIN)
            /* Install/uninstall low-level keyboard-hook on every activation/deactivation to:
             * a) avoid excess hook calls when we're not active and;
             * b) be always in front of any other possible hooks. */
            case QEvent::WindowActivate:
            {
                /* If keyboard hook is NOT currently created;
                 * Or created but NOT for that window: */
                if (!m_keyboardHook || m_iKeyboardHookViewIndex != uScreenId)
                {
                    /* If keyboard-hook present: */
                    if (m_keyboardHook)
                    {
                        /* We should remove existing keyboard-hook first: */
                        UnhookWindowsHookEx(m_keyboardHook);
                        m_keyboardHook = NULL;
                    }
                    /* Register new keyboard-hook: */
                    m_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, lowLevelKeyboardProc, GetModuleHandle(NULL), 0);
                    AssertMsg(m_keyboardHook, ("SetWindowsHookEx(): err=%d", GetLastError()));
                    /* Remember which view had captured keyboard: */
                    m_iKeyboardHookViewIndex = uScreenId;
                }
                break;
            }
            case QEvent::WindowDeactivate:
            {
                /* If keyboard is currently captured: */
                if (m_keyboardHook && m_iKeyboardHookViewIndex == uScreenId)
                {
                    /* We should remove existing keyboard-hook: */
                    UnhookWindowsHookEx(m_keyboardHook);
                    m_keyboardHook = NULL;
                    /* Remember what there is no window captured keyboard: */
                    m_iKeyboardHookViewIndex = -1;
                }
                break;
            }
#elif defined(Q_WS_MAC)
            case QEvent::WindowActivate:
            {
                /* If keyboard event handler is NOT currently installed;
                 * Or installed but NOT for that window: */
                if (m_iKeyboardGrabViewIndex != (int)uScreenId)
                {
                    /* If keyboard event handler is NOT currently installed: */
                    if (m_iKeyboardGrabViewIndex == -1)
                    {
                        /* Install the keyboard event handler: */
                        darwinGrabKeyboardEvents(true);
                    }
                    /* Update the id: */
                    m_iKeyboardGrabViewIndex = uScreenId;
                }
                break;
            }
            case QEvent::WindowDeactivate:
            {
                /* If keyboard event handler is installed exactly for that window: */
                if (m_iKeyboardGrabViewIndex == (int)uScreenId)
                {
                    /* Remove the keyboard event handler: */
                    darwinGrabKeyboardEvents(false);
                    /* Update the id: */
                    m_iKeyboardGrabViewIndex = -1;
                }
                break;
            }
#endif
            default:
                break;
        }
    }

    else

    /* Check if pWatchedObject object is view: */
    if (UIMachineView *pWatchedView = isItListenedView(pWatchedObject))
    {
        /* Get corresponding screen index: */
        ulong uScreenId = m_views.key(pWatchedView);
        NOREF(uScreenId);
        /* Handle view events: */
        switch (pEvent->type())
        {
            case QEvent::FocusIn:
                if (isSessionRunning())
                {
                    /* Capture keyboard: */
#ifdef Q_WS_WIN
                    if (!isAutoCaptureDisabled() && autoCaptureSetGlobally() &&
                        GetAncestor(pWatchedView->winId(), GA_ROOT) == GetForegroundWindow())
#else /* Q_WS_WIN */
                    if (!isAutoCaptureDisabled() && autoCaptureSetGlobally())
#endif /* !Q_WS_WIN */
                        captureKeyboard(uScreenId);
                    /* Reset the single-time disable capture flag: */
                    if (isAutoCaptureDisabled())
                        setAutoCaptureDisabled(false);
                }
                break;
            case QEvent::FocusOut:
                /* Release keyboard: */
                if (isSessionRunning())
                    releaseKeyboard();
                /* And all pressed keys: */
                releaseAllPressedKeys(true);
                break;
            case QEvent::KeyPress:
            case QEvent::KeyRelease:
            {
                QKeyEvent *pKeyEvent = static_cast<QKeyEvent*>(pEvent);

                if (m_bIsHostComboPressed && pEvent->type() == QEvent::KeyPress)
                {
                    /* Passing F1-F12 keys to the guest: */
                    if (pKeyEvent->key() >= Qt::Key_F1 && pKeyEvent->key() <= Qt::Key_F12)
                    {
                        QVector <LONG> combo(6);
                        combo[0] = 0x1d; /* Ctrl down */
                        combo[1] = 0x38; /* Alt  down */
                        combo[4] = 0xb8; /* Alt  up   */
                        combo[5] = 0x9d; /* Ctrl up   */
                        if (pKeyEvent->key() >= Qt::Key_F1 && pKeyEvent->key() <= Qt::Key_F10)
                        {
                            combo[2] = 0x3b + (pKeyEvent->key() - Qt::Key_F1); /* F1-F10 down */
                            combo[3] = 0xbb + (pKeyEvent->key() - Qt::Key_F1); /* F1-F10 up   */
                        }
                        /* There is some scan slice between F10 and F11 keys, so its separated: */
                        else if (pKeyEvent->key() >= Qt::Key_F11 && pKeyEvent->key() <= Qt::Key_F12)
                        {
                            combo[2] = 0x57 + (pKeyEvent->key() - Qt::Key_F11); /* F11-F12 down */
                            combo[3] = 0xd7 + (pKeyEvent->key() - Qt::Key_F11); /* F11-F12 up   */
                        }
                        CKeyboard keyboard = session().GetConsole().GetKeyboard();
                        keyboard.PutScancodes(combo);
                    }
                    /* Process hot keys not processed in keyEvent() (as in case of non-alphanumeric keys): */
                    machineLogic()->actionsPool()->processHotKey(QKeySequence(pKeyEvent->key()));
                }
                else if (!m_bIsHostComboPressed && pEvent->type() == QEvent::KeyRelease)
                {
                    /* Show a possible warning on key release which seems to be more expected by the end user: */
                    if (uisession()->isPaused())
                    {
                        /* If the reminder is disabled we pass the event to Qt to enable normal
                         * keyboard functionality (for example, menu access with Alt+Letter): */
                        if (!vboxProblem().remindAboutPausedVMInput())
                            break;
                    }
                }
                break;
            }
            default:
                break;
        }
    }

    /* Else just propagate to base-class: */
    return QObject::eventFilter(pWatchedObject, pEvent);
}

#if defined(Q_WS_WIN)

LRESULT CALLBACK UIKeyboardHandler::lowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && m_spKeyboardHandler && m_spKeyboardHandler->winLowKeyboardEvent(wParam, *(KBDLLHOOKSTRUCT*)lParam))
        return 1;

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

bool UIKeyboardHandler::winLowKeyboardEvent(UINT msg, const KBDLLHOOKSTRUCT &event)
{
    /* Check what related machine-view was NOT unregistered yet: */
    if (!m_views.contains(m_iKeyboardHookViewIndex))
        return false;

    if (!m_fIsKeyboardCaptured)
        return false;

    /* Sometimes it happens that Win inserts additional events on some key
     * press/release. For example, it prepends ALT_GR in German layout with
     * the VK_LCONTROL vkey with curious 0x21D scan code (seems to be necessary
     * to specially treat ALT_GR to enter additional chars to regular apps).
     * These events are definitely unwanted in VM, so filter them out. */
    /* Note (michael): it also sometimes sends the VK_CAPITAL vkey with scan
     * code 0x23a. If this is not passed through then it is impossible to
     * cancel CapsLock on a French keyboard.  I didn't find any other examples
     * of these strange events.  Let's hope we are not missing anything else
     * of importance! */
    if (m_views[m_iKeyboardHookViewIndex]->hasFocus() && (event.scanCode & ~0xFF))
    {
        if (event.vkCode == VK_CAPITAL)
            return false;
        else
            return true;
    }

    /* It's possible that a key has been pressed while the keyboard was not
     * captured, but is being released under the capture. Detect this situation
     * and return false to let Windows process the message normally and update
     * its key state table (to avoid the stuck key effect). */
    uint8_t what_pressed = (event.flags & 0x01) && (event.vkCode != VK_RSHIFT) ? IsExtKeyPressed : IsKeyPressed;
    if ((event.flags & 0x80) /* released */ &&
        ((UIHotKeyCombination::toKeyCodeList(m_globalSettings.hostCombo()).contains(event.vkCode) && !m_bIsHostkeyInCapture) ||
         (m_pressedKeys[event.scanCode] & (IsKbdCaptured | what_pressed)) == what_pressed))
        return false;

    MSG message;
    message.hwnd = m_views[m_iKeyboardHookViewIndex]->winId();
    message.message = msg;
    message.wParam = event.vkCode;
    message.lParam = 1 | (event.scanCode & 0xFF) << 16 | (event.flags & 0xFF) << 24;

    /* Windows sets here the extended bit when the Right Shift key is pressed,
     * which is totally wrong. Undo it. */
    if (event.vkCode == VK_RSHIFT)
        message.lParam &= ~0x1000000;

    /* We suppose here that this hook is always called on the main GUI thread */
    long dummyResult;
    return m_views[m_iKeyboardHookViewIndex]->winEvent(&message, &dummyResult);
}

#elif defined(Q_WS_MAC)

void UIKeyboardHandler::darwinGrabKeyboardEvents(bool fGrab)
{
    m_fKeyboardGrabbed = fGrab;
    if (fGrab)
    {
        /* Disable mouse and keyboard event compression/delaying to make sure we *really* get all of the events. */
        ::CGSetLocalEventsSuppressionInterval(0.0);
        ::darwinSetMouseCoalescingEnabled(false);

        /* Register the event callback/hook and grab the keyboard. */
        UICocoaApplication::instance()->registerForNativeEvents(RT_BIT_32(10) | RT_BIT_32(11) | RT_BIT_32(12) /* NSKeyDown  | NSKeyUp | | NSFlagsChanged */,
                                                                UIKeyboardHandler::darwinEventHandlerProc, this);

        ::DarwinGrabKeyboard (false);
    }
    else
    {
        ::DarwinReleaseKeyboard();
        UICocoaApplication::instance()->unregisterForNativeEvents(RT_BIT_32(10) | RT_BIT_32(11) | RT_BIT_32(12) /* NSKeyDown  | NSKeyUp | | NSFlagsChanged */,
                                                                  UIKeyboardHandler::darwinEventHandlerProc, this);
    }
}

bool UIKeyboardHandler::darwinEventHandlerProc(const void *pvCocoaEvent, const void *pvCarbonEvent, void *pvUser)
{
    UIKeyboardHandler *pKeyboardHandler = (UIKeyboardHandler*)pvUser;
    EventRef inEvent = (EventRef)pvCarbonEvent;
    UInt32 eventClass = ::GetEventClass(inEvent);

    /* Check if this is an application key combo. In that case we will not pass
     * the event to the guest, but let the host process it. */
    if (::darwinIsApplicationCommand(pvCocoaEvent))
        return false;

    /* All keyboard class events needs to be handled. */
    if (eventClass == kEventClassKeyboard)
    {
        if (pKeyboardHandler->darwinKeyboardEvent (pvCocoaEvent, inEvent))
            return true;
    }
    /* Pass the event along. */
    return false;
}

bool UIKeyboardHandler::darwinKeyboardEvent(const void *pvCocoaEvent, EventRef inEvent)
{
    bool ret = false;
    UInt32 EventKind = ::GetEventKind(inEvent);
    if (EventKind != kEventRawKeyModifiersChanged)
    {
        /* Convert keycode to set 1 scan code. */
        UInt32 keyCode = ~0U;
        ::GetEventParameter(inEvent, kEventParamKeyCode, typeUInt32, NULL, sizeof (keyCode), NULL, &keyCode);
        /* The usb keyboard driver translates these codes to different virtual
         * key codes depending of the keyboard type. There are ANSI, ISO, JIS
         * and unknown. For European keyboards (ISO) the key 0xa and 0x32 have
         * to be switched. Here we are doing this at runtime, cause the user
         * can have more than one keyboard (of different type), where he may
         * switch at will all the time. Default is the ANSI standard as defined
         * in g_aDarwinToSet1. Please note that the "~" on some English ISO
         * keyboards will be wrongly swapped. This can maybe fixed by
         * using a Apple keyboard layout in the guest. */
        if (   (keyCode == 0xa || keyCode == 0x32)
            && KBGetLayoutType(LMGetKbdType()) == kKeyboardISO)
            keyCode = 0x3c - keyCode;
        unsigned scanCode = ::DarwinKeycodeToSet1Scancode(keyCode);
        if (scanCode)
        {
            /* Calc flags. */
            int flags = 0;
            if (EventKind != kEventRawKeyUp)
                flags |= KeyPressed;
            if (scanCode & VBOXKEY_EXTENDED)
                flags |= KeyExtended;
            /** @todo KeyPause, KeyPrint. */
            scanCode &= VBOXKEY_SCANCODE_MASK;

            /* Get the unicode string (if present). */
            AssertCompileSize(wchar_t, 2);
            AssertCompileSize(UniChar, 2);
            ByteCount cbWritten = 0;
            wchar_t ucs[8];
            if (::GetEventParameter(inEvent, kEventParamKeyUnicodes, typeUnicodeText, NULL,
                                    sizeof(ucs), &cbWritten, &ucs[0]) != 0)
                cbWritten = 0;
            ucs[cbWritten / sizeof(wchar_t)] = 0; /* The api doesn't terminate it. */

            ret = keyEvent(keyCode, scanCode, flags, m_iKeyboardGrabViewIndex, ucs[0] ? ucs : NULL);
        }
    }
    else
    {
        /* May contain multiple modifier changes, kind of annoying. */
        UInt32 newMask = 0;
        ::GetEventParameter(inEvent, kEventParamKeyModifiers, typeUInt32, NULL,
                            sizeof(newMask), NULL, &newMask);
        newMask = ::DarwinAdjustModifierMask(newMask, pvCocoaEvent);
        UInt32 changed = newMask ^ m_darwinKeyModifiers;
        if (changed)
        {
            for (UInt32 bit = 0; bit < 32; bit++)
            {
                if (!(changed & (1 << bit)))
                    continue;
                unsigned scanCode = ::DarwinModifierMaskToSet1Scancode(1 << bit);
                if (!scanCode)
                    continue;
                unsigned keyCode = ::DarwinModifierMaskToDarwinKeycode(1 << bit);
                Assert(keyCode);

                if (!(scanCode & VBOXKEY_LOCK))
                {
                    unsigned flags = (newMask & (1 << bit)) ? KeyPressed : 0;
                    if (scanCode & VBOXKEY_EXTENDED)
                        flags |= KeyExtended;
                    scanCode &= VBOXKEY_SCANCODE_MASK;
                    ret |= keyEvent(keyCode, scanCode & 0xff, flags, m_iKeyboardGrabViewIndex);
                }
                else
                {
                    unsigned flags = 0;
                    if (scanCode & VBOXKEY_EXTENDED)
                        flags |= KeyExtended;
                    scanCode &= VBOXKEY_SCANCODE_MASK;
                    keyEvent(keyCode, scanCode, flags | KeyPressed, m_iKeyboardGrabViewIndex);
                    keyEvent(keyCode, scanCode, flags, m_iKeyboardGrabViewIndex);
                }
            }
        }

        m_darwinKeyModifiers = newMask;

        /* Always return true here because we'll otherwise getting a Qt event
           we don't want and that will only cause the Pause warning to pop up. */
        ret = true;
    }

    return ret;
}

#endif

/**
 * If the user has just completed a control-alt-del combination then handle
 * that.
 * @returns true if handling should stop here, false otherwise
 */
bool UIKeyboardHandler::keyEventCADHandled(uint8_t uScan)
{
    /* Check if it's C-A-D and GUI/PassCAD is not true: */
    if (!m_fPassCAD &&
        uScan == 0x53 /* Del */ &&
        ((m_pressedKeys[0x38] & IsKeyPressed) /* Alt */ ||
         (m_pressedKeys[0x38] & IsExtKeyPressed)) &&
        ((m_pressedKeys[0x1d] & IsKeyPressed) /* Ctrl */ ||
         (m_pressedKeys[0x1d] & IsExtKeyPressed)))
    {
        /* Use the C-A-D combination as a last resort to get the keyboard and mouse back
         * to the host when the user forgets the Host Key. Note that it's always possible
         * to send C-A-D to the guest using the Host+Del combination: */
        if (isSessionRunning() && m_fIsKeyboardCaptured)
        {
            releaseKeyboard();
            if (!uisession()->isMouseSupportsAbsolute() || !uisession()->isMouseIntegrated())
                machineLogic()->mouseHandler()->releaseMouse();
        }
        return true;
    }
    return false;
}

/**
 * Handle a non-special (C-A-D, pause, print) key press or release
 * @returns true if handling should stop here, false otherwise
 */
bool UIKeyboardHandler::keyEventHandleNormal(int iKey, uint8_t uScan, int fFlags, LONG *pCodes, uint *puCodesCount)
{
    /* Get host-combo key list: */
    QSet<int> allHostComboKeys = UIHotKeyCombination::toKeyCodeList(m_globalSettings.hostCombo()).toSet();
    /* Get the type of key - simple or extended: */
    uint8_t uWhatPressed = fFlags & KeyExtended ? IsExtKeyPressed : IsKeyPressed;

    /* If some key was pressed or some previously pressed key was released =>
     * we are updating the list of pressed keys and preparing scancodes: */
    if ((fFlags & KeyPressed) || (m_pressedKeys[uScan] & uWhatPressed))
    {
        /* Check if the guest has the same view on the modifier keys
         * (NumLock, CapsLock, ScrollLock) as the X server.
         * If not, send KeyPress events to synchronize the state: */
        if (fFlags & KeyPressed)
            fixModifierState(pCodes, puCodesCount);

        /* Prepend 'extended' scancode if needed: */
        if (fFlags & KeyExtended)
            pCodes[(*puCodesCount)++] = 0xE0;

        /* Process key-press: */
        if (fFlags & KeyPressed)
        {
            /* Append scancode: */
            pCodes[(*puCodesCount)++] = uScan;
            m_pressedKeys[uScan] |= uWhatPressed;
        }
        /* Process key-release if that key was pressed before: */
        else if (m_pressedKeys[uScan] & uWhatPressed)
        {
            /* Append scancode: */
            pCodes[(*puCodesCount)++] = uScan | 0x80;
            m_pressedKeys[uScan] &= ~uWhatPressed;
        }

        /* Update keyboard-captured flag: */
        if (m_fIsKeyboardCaptured)
            m_pressedKeys[uScan] |= IsKbdCaptured;
        else
            m_pressedKeys[uScan] &= ~IsKbdCaptured;
    }
    /* Ignore key-release if that key was NOT pressed before,
     * but only if thats not one of the host-combination keys: */
    else if (!allHostComboKeys.contains(iKey))
        return true;
    return false;
}

/**
 * Check whether the key pressed results in a host key combination being
 * handled.
 * @returns true if a combination was handled, false otherwise
 * @param pfResult  where to store the result of the handling
 */
bool UIKeyboardHandler::keyEventHostComboHandled(int iKey, wchar_t *pUniKey, bool isHostComboStateChanged, bool *pfResult)
{
    if (isHostComboStateChanged)
    {
        if (!m_bIsHostComboPressed)
        {
            m_bIsHostComboPressed = true;
            m_bIsHostComboAlone = true;
            m_bIsHostComboProcessed = false;
            if (isSessionRunning())
                saveKeyStates();
        }
    }
    else
    {
        if (m_bIsHostComboPressed)
        {
            if (m_bIsHostComboAlone)
            {
                m_bIsHostComboAlone = false;
                m_bIsHostComboProcessed = true;
                /* Process Host+<key> shortcuts.
                 * Currently, <key> is limited to alphanumeric chars.
                 * Other Host+<key> combinations are handled in Qt event(): */
                *pfResult = processHotKey(iKey, pUniKey);
                return true;
            }
        }
    }
    return false;
}

/**
 * Handle a key event that releases the host key combination
 */
void UIKeyboardHandler::keyEventHandleHostComboRelease(ulong uScreenId)
{
    if (m_bIsHostComboPressed)
    {
        m_bIsHostComboPressed = false;
        /* Capturing/releasing keyboard/mouse if necessary: */
        if (m_bIsHostComboAlone && !m_bIsHostComboProcessed)
        {
            if (isSessionRunning())
            {
                bool ok = true;
                if (!m_fIsKeyboardCaptured)
                {
                    /* Temporarily disable auto-capture that will take place after
                     * this dialog is dismissed because the capture state is to be
                     * defined by the dialog result itself: */
                    setAutoCaptureDisabled(true);
                    bool fIsAutoConfirmed = false;
                    ok = vboxProblem().confirmInputCapture(&fIsAutoConfirmed);
                    if (fIsAutoConfirmed)
                        setAutoCaptureDisabled(false);
                    /* Otherwise, the disable flag will be reset in the next
                     * machine-view's focus-in event (since may happen asynchronously
                     * on some platforms, after we return from this code): */
                }
                if (ok)
                {
                    if (m_fIsKeyboardCaptured)
                        releaseKeyboard();
                    else
                        captureKeyboard(uScreenId);
                    if (!uisession()->isMouseSupportsAbsolute() || !uisession()->isMouseIntegrated())
                    {
#ifdef Q_WS_X11
                        /* Make sure that pending FocusOut events from the
                         * previous message box are handled, otherwise the
                         * mouse is immediately ungrabbed: */
                        qApp->processEvents();
#endif /* Q_WS_X11 */
                        if (m_fIsKeyboardCaptured)
                            machineLogic()->mouseHandler()->captureMouse(uScreenId);
                        else
                            machineLogic()->mouseHandler()->releaseMouse();
                    }
                }
            }
        }
        if (isSessionRunning())
            sendChangedKeyStates();
    }
}

void UIKeyboardHandler::keyEventReleaseHostComboKeys(CKeyboard keyboard)
{
    /* We have to make guest to release pressed keys from the host-combination: */
    QList<uint8_t> hostComboScans = m_pressedHostComboKeys.values();
    for (int i = 0 ; i < hostComboScans.size(); ++i)
    {
        uint8_t uScan = hostComboScans[i];
        if (m_pressedKeys[uScan] & IsKeyPressed)
        {
            keyboard.PutScancode(uScan | 0x80);
        }
        else if (m_pressedKeys[uScan] & IsExtKeyPressed)
        {
            QVector<LONG> scancodes(2);
            scancodes[0] = 0xE0;
            scancodes[1] = uScan | 0x80;
            keyboard.PutScancodes(scancodes);
        }
        m_pressedKeys[uScan] = 0;
    }
}

bool UIKeyboardHandler::keyEvent(int iKey, uint8_t uScan, int fFlags, ulong uScreenId, wchar_t *pUniKey /* = 0 */)
{
    /* Get host-combo key list: */
    QSet<int> allHostComboKeys = UIHotKeyCombination::toKeyCodeList(m_globalSettings.hostCombo()).toSet();

    /* Update the map of pressed host-combo keys: */
    if (allHostComboKeys.contains(iKey))
    {
        if (fFlags & KeyPressed)
        {
            if (!m_pressedHostComboKeys.contains(iKey))
                m_pressedHostComboKeys.insert(iKey, uScan);
            else if (m_bIsHostComboPressed)
                return true;
        }
        else
        {
            if (m_pressedHostComboKeys.contains(iKey))
                m_pressedHostComboKeys.remove(iKey);
        }
    }
    /* Check if we are currently holding FULL host-combo: */
    bool fIsFullHostComboPresent = !allHostComboKeys.isEmpty() && allHostComboKeys == m_pressedHostComboKeys.keys().toSet();
    /* Check if currently pressed/released key had changed host-combo state: */
    const bool isHostComboStateChanged = (!m_bIsHostComboPressed &&  fIsFullHostComboPresent) ||
                                         ( m_bIsHostComboPressed && !fIsFullHostComboPresent);

#ifdef Q_WS_WIN
    if (m_bIsHostComboPressed || isHostComboStateChanged)
    {
        /* Currently this is used in winLowKeyboardEvent() only: */
        m_bIsHostkeyInCapture = m_fIsKeyboardCaptured;
    }
#endif /* Q_WS_WIN */

    if (keyEventCADHandled(uScan))
        return true;

    /* Preparing the press/release scan-codes array for sending to the guest:
     * 1. if host-combo is NOT pressed, taking into account currently pressed key too,
     * 2. if currently released key releases host-combo too.
     * Using that rule, we are NOT sending to the guest:
     * 1. the last key-press of host-combo,
     * 2. all keys pressed while the host-combo being held. */
    LONG aCodesBuffer[16];
    LONG *pCodes = aCodesBuffer;
    uint uCodesCount = 0;
    if ((!m_bIsHostComboPressed && !isHostComboStateChanged) ||
        ( m_bIsHostComboPressed &&  isHostComboStateChanged))
    {
        /* Special flags handling (KeyPrint): */
        if (fFlags & KeyPrint)
        {
            if (fFlags & KeyPressed)
            {
                static LONG PrintMake[] = { 0xE0, 0x2A, 0xE0, 0x37 };
                pCodes = PrintMake;
                uCodesCount = SIZEOF_ARRAY(PrintMake);
            }
            else
            {
                static LONG PrintBreak[] = { 0xE0, 0xB7, 0xE0, 0xAA };
                pCodes = PrintBreak;
                uCodesCount = SIZEOF_ARRAY(PrintBreak);
            }
        }
        /* Special flags handling (KeyPause): */
        else if (fFlags & KeyPause)
        {
            if (fFlags & KeyPressed)
            {
                static LONG Pause[] = { 0xE1, 0x1D, 0x45, 0xE1, 0x9D, 0xC5 };
                pCodes = Pause;
                uCodesCount = SIZEOF_ARRAY(Pause);
            }
            else
            {
                /* Pause shall not produce a break code: */
                return true;
            }
        }
        /* Common flags handling: */
        else
            if (keyEventHandleNormal(iKey, uScan, fFlags, pCodes, &uCodesCount))
                return true;
    }

    /* Process the host-combo funtionality: */
    if (fFlags & KeyPressed)
    {
        bool fResult;
        if (keyEventHostComboHandled(iKey, pUniKey, isHostComboStateChanged, &fResult))
            return fResult;
    }
    else
    {
        if (isHostComboStateChanged)
            keyEventHandleHostComboRelease(uScreenId);
        else
        {
            if (m_bIsHostComboPressed)
                m_bIsHostComboAlone = true;
        }
    }

    /* Notify all listeners: */
    emit keyboardStateChanged(keyboardState());

    /* If the VM is NOT paused: */
    if (!uisession()->isPaused())
    {
        /* Get the VM keyboard: */
        CKeyboard keyboard = session().GetConsole().GetKeyboard();
        Assert(!keyboard.isNull());

        /* If there are scan-codes to send: */
        if (uCodesCount)
        {
            /* Send prepared scan-codes to the guest: */
            std::vector<LONG> scancodes(pCodes, &pCodes[uCodesCount]);
            keyboard.PutScancodes(QVector<LONG>::fromStdVector(scancodes));
        }

        /* If full host-key sequence was just finalized: */
        if (isHostComboStateChanged && m_bIsHostComboPressed)
        {
            keyEventReleaseHostComboKeys(keyboard);
        }
    }

    /* Prevent the key from going to Qt: */
    return true;
}

bool UIKeyboardHandler::processHotKey(int iHotKey, wchar_t *pHotKey)
{
    /* Prepare processing result: */
    bool fWasProcessed = false;

#ifdef Q_WS_WIN
    Q_UNUSED(pHotKey);
    int iKeyboardLayout = GetKeyboardLayoutList(0, NULL);
    Assert(iKeyboardLayout);
    HKL *pList = new HKL[iKeyboardLayout];
    GetKeyboardLayoutList(iKeyboardLayout, pList);
    for (int i = 0; i < iKeyboardLayout && !fWasProcessed; ++i)
    {
        wchar_t symbol;
        static BYTE keys[256] = {0};
        if (!ToUnicodeEx(iHotKey, 0, keys, &symbol, 1, 0, pList[i]) == 1)
            symbol = 0;
        if (symbol)
            fWasProcessed = machineLogic()->actionsPool()->processHotKey(QKeySequence((Qt::UNICODE_ACCEL + QChar(symbol).toUpper().unicode())));
    }
    delete[] pList;
#endif /* Q_WS_WIN */

#ifdef Q_WS_X11
    Q_UNUSED(pHotKey);
    Display *pDisplay = QX11Info::display();
    int iKeysymsPerKeycode = getKeysymsPerKeycode();
    KeyCode keyCode = XKeysymToKeycode(pDisplay, iHotKey);
    for (int i = 0; i < iKeysymsPerKeycode && !fWasProcessed; i += 2)
    {
        KeySym ks = XKeycodeToKeysym(pDisplay, keyCode, i);
        char symbol = 0;
        if (!XkbTranslateKeySym(pDisplay, &ks, 0, &symbol, 1, NULL) == 1)
            symbol = 0;
        if (symbol)
        {
            QChar qtSymbol = QString::fromLocal8Bit(&symbol, 1)[0];
            fWasProcessed = machineLogic()->actionsPool()->processHotKey(QKeySequence((Qt::UNICODE_ACCEL + qtSymbol.toUpper().unicode())));
        }
    }
#endif /* Q_WS_X11 */

#ifdef Q_WS_MAC
    Q_UNUSED(iHotKey);
    if (pHotKey && pHotKey[0] && !pHotKey[1])
        fWasProcessed = machineLogic()->actionsPool()->processHotKey(QKeySequence(Qt::UNICODE_ACCEL + QChar(pHotKey[0]).toUpper().unicode()));
#endif /* Q_WS_MAC */

    /* Grab the key from the Qt if it was processed, or pass it to the Qt otherwise
     * in order to process non-alphanumeric keys in event(), after they are converted to Qt virtual keys: */
    return fWasProcessed;
}

void UIKeyboardHandler::fixModifierState(LONG *piCodes, uint *puCount)
{
    /* Synchronize the views of the host and the guest to the modifier keys.
     * This function will add up to 6 additional keycodes to codes. */
#if defined(Q_WS_X11)
    Window   wDummy1, wDummy2;
    int      iDummy3, iDummy4, iDummy5, iDummy6;
    unsigned uMask;
    unsigned uKeyMaskNum = 0, uKeyMaskCaps = 0, uKeyMaskScroll = 0;

    uKeyMaskCaps          = LockMask;
    XModifierKeymap* map  = XGetModifierMapping(QX11Info::display());
    KeyCode keyCodeNum    = XKeysymToKeycode(QX11Info::display(), XK_Num_Lock);
    KeyCode keyCodeScroll = XKeysymToKeycode(QX11Info::display(), XK_Scroll_Lock);

    for (int i = 0; i < 8; ++ i)
    {
        if (keyCodeNum != NoSymbol && map->modifiermap[map->max_keypermod * i] == keyCodeNum)
            uKeyMaskNum = 1 << i;
        else if (keyCodeScroll != NoSymbol && map->modifiermap[map->max_keypermod * i] == keyCodeScroll)
            uKeyMaskScroll = 1 << i;
    }
    XQueryPointer(QX11Info::display(), DefaultRootWindow(QX11Info::display()), &wDummy1, &wDummy2,
                  &iDummy3, &iDummy4, &iDummy5, &iDummy6, &uMask);
    XFreeModifiermap(map);

    if (uisession()->numLockAdaptionCnt() && (uisession()->isNumLock() ^ !!(uMask & uKeyMaskNum)))
    {
        uisession()->setNumLockAdaptionCnt(uisession()->numLockAdaptionCnt() - 1);
        piCodes[(*puCount)++] = 0x45;
        piCodes[(*puCount)++] = 0x45 | 0x80;
    }
    if (uisession()->capsLockAdaptionCnt() && (uisession()->isCapsLock() ^ !!(uMask & uKeyMaskCaps)))
    {
        uisession()->setCapsLockAdaptionCnt(uisession()->capsLockAdaptionCnt() - 1);
        piCodes[(*puCount)++] = 0x3a;
        piCodes[(*puCount)++] = 0x3a | 0x80;
        /* Some keyboard layouts require shift to be pressed to break
         * capslock.  For simplicity, only do this if shift is not
         * already held down. */
        if (uisession()->isCapsLock() && !(m_pressedKeys[0x2a] & IsKeyPressed))
        {
            piCodes[(*puCount)++] = 0x2a;
            piCodes[(*puCount)++] = 0x2a | 0x80;
        }
    }
#elif defined(Q_WS_WIN)
    if (uisession()->numLockAdaptionCnt() && (uisession()->isNumLock() ^ !!(GetKeyState(VK_NUMLOCK))))
    {
        uisession()->setNumLockAdaptionCnt(uisession()->numLockAdaptionCnt() - 1);
        piCodes[(*puCount)++] = 0x45;
        piCodes[(*puCount)++] = 0x45 | 0x80;
    }
    if (uisession()->capsLockAdaptionCnt() && (uisession()->isCapsLock() ^ !!(GetKeyState(VK_CAPITAL))))
    {
        uisession()->setCapsLockAdaptionCnt(uisession()->capsLockAdaptionCnt() - 1);
        piCodes[(*puCount)++] = 0x3a;
        piCodes[(*puCount)++] = 0x3a | 0x80;
        /* Some keyboard layouts require shift to be pressed to break
         * capslock.  For simplicity, only do this if shift is not
         * already held down. */
        if (uisession()->isCapsLock() && !(m_pressedKeys[0x2a] & IsKeyPressed))
        {
            piCodes[(*puCount)++] = 0x2a;
            piCodes[(*puCount)++] = 0x2a | 0x80;
        }
    }
#elif defined(Q_WS_MAC)
    /* if (uisession()->numLockAdaptionCnt()) ... - NumLock isn't implemented by Mac OS X so ignore it. */
    if (uisession()->capsLockAdaptionCnt() && (uisession()->isCapsLock() ^ !!(::GetCurrentEventKeyModifiers() & alphaLock)))
    {
        uisession()->setCapsLockAdaptionCnt(uisession()->capsLockAdaptionCnt() - 1);
        piCodes[(*puCount)++] = 0x3a;
        piCodes[(*puCount)++] = 0x3a | 0x80;
        /* Some keyboard layouts require shift to be pressed to break
         * capslock.  For simplicity, only do this if shift is not
         * already held down. */
        if (uisession()->isCapsLock() && !(m_pressedKeys[0x2a] & IsKeyPressed))
        {
            piCodes[(*puCount)++] = 0x2a;
            piCodes[(*puCount)++] = 0x2a | 0x80;
        }
    }
#else
//#warning Adapt UIKeyboardHandler::fixModifierState
#endif
}

void UIKeyboardHandler::saveKeyStates()
{
    ::memcpy(m_pressedKeysCopy, m_pressedKeys, sizeof(m_pressedKeys));
}

void UIKeyboardHandler::sendChangedKeyStates()
{
    QVector <LONG> codes(2);
    CKeyboard keyboard = session().GetConsole().GetKeyboard();
    for (uint i = 0; i < SIZEOF_ARRAY(m_pressedKeys); ++ i)
    {
        uint8_t os = m_pressedKeysCopy[i];
        uint8_t ns = m_pressedKeys[i];
        if ((os & IsKeyPressed) != (ns & IsKeyPressed))
        {
            codes[0] = i;
            if (!(ns & IsKeyPressed))
                codes[0] |= 0x80;
            keyboard.PutScancode(codes[0]);
        }
        else if ((os & IsExtKeyPressed) != (ns & IsExtKeyPressed))
        {
            codes[0] = 0xE0;
            codes[1] = i;
            if (!(ns & IsExtKeyPressed))
                codes[1] |= 0x80;
            keyboard.PutScancodes(codes);
        }
    }
}

bool UIKeyboardHandler::isAutoCaptureDisabled()
{
    return uisession()->isAutoCaptureDisabled();
}

void UIKeyboardHandler::setAutoCaptureDisabled(bool fIsAutoCaptureDisabled)
{
    uisession()->setAutoCaptureDisabled(fIsAutoCaptureDisabled);
}

bool UIKeyboardHandler::autoCaptureSetGlobally()
{
    return m_globalSettings.autoCapture();
}

bool UIKeyboardHandler::viewHasFocus(ulong uScreenId)
{
    return m_views[uScreenId]->hasFocus();
}

bool UIKeyboardHandler::isSessionRunning()
{
    return uisession()->isRunning();
}

UIMachineWindow* UIKeyboardHandler::isItListenedWindow(QObject *pWatchedObject) const
{
    UIMachineWindow *pResultWindow = 0;
    QMap<ulong, UIMachineWindow*>::const_iterator i = m_windows.constBegin();
    while (!pResultWindow && i != m_windows.constEnd())
    {
        UIMachineWindow *pIteratedWindow = i.value();
        if (pIteratedWindow->machineWindow() == pWatchedObject)
        {
            pResultWindow = pIteratedWindow;
            continue;
        }
        ++i;
    }
    return pResultWindow;
}

UIMachineView* UIKeyboardHandler::isItListenedView(QObject *pWatchedObject) const
{
    UIMachineView *pResultView = 0;
    QMap<ulong, UIMachineView*>::const_iterator i = m_views.constBegin();
    while (!pResultView && i != m_views.constEnd())
    {
        UIMachineView *pIteratedView = i.value();
        if (pIteratedView == pWatchedObject)
        {
            pResultView = pIteratedView;
            continue;
        }
        ++i;
    }
    return pResultView;
}
