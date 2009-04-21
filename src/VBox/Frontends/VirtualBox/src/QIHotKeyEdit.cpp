/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIHotKeyEdit class implementation
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "QIHotKeyEdit.h"
#include "VBoxDefs.h"

/* Qt includes */
#include <QApplication>
#include <QStyleOption>
#include <QStylePainter>

#ifdef Q_WS_WIN
/* VBox/cdefs.h defines these: */
#undef LOWORD
#undef HIWORD
#undef LOBYTE
#undef HIBYTE
#include <windows.h>
#endif

#if defined (Q_WS_PM)
QMap<int, QString> QIHotKeyEdit::sKeyNames;
#endif

#ifdef Q_WS_X11
/* We need to capture some X11 events directly which
 * requires the XEvent structure to be defined. However,
 * including the Xlib header file will cause some nasty
 * conflicts with Qt. Therefore we use the following hack
 * to redefine those conflicting identifiers. */
#define XK_XKB_KEYS
#define XK_MISCELLANY
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysymdef.h>
#ifdef KeyPress
const int XFocusOut = FocusOut;
const int XFocusIn = FocusIn;
const int XKeyPress = KeyPress;
const int XKeyRelease = KeyRelease;
#undef KeyRelease
#undef KeyPress
#undef FocusOut
#undef FocusIn
#endif
#include "XKeyboard.h"
QMap<QString, QString> QIHotKeyEdit::sKeyNames;
#include <QX11Info>
#endif

#ifdef Q_WS_MAC
# include "DarwinKeyboard.h"
# include <Carbon/Carbon.h>
# ifdef QT_MAC_USE_COCOA
#  include "darwin/VBoxCocoaApplication.h"
#  include "VBoxUtils.h"
# endif
#endif


#if defined (Q_WS_WIN32)
/**
 *  Returns the correct modifier vkey for the *last* keyboard message,
 *  distinguishing between left and right keys. If both are pressed
 *  the left key wins. If the pressed key not a modifier, wParam is returned
 *  unchanged.
 */
int qi_distinguish_modifier_vkey (WPARAM wParam)
{
    int keyval = wParam;
    switch (wParam)
    {
        case VK_SHIFT:
            if (::GetKeyState (VK_LSHIFT) & 0x8000) keyval = VK_LSHIFT;
            else if (::GetKeyState (VK_RSHIFT) & 0x8000) keyval = VK_RSHIFT;
            break;
        case VK_CONTROL:
            if (::GetKeyState (VK_LCONTROL) & 0x8000) keyval = VK_LCONTROL;
            else if (::GetKeyState (VK_RCONTROL) & 0x8000) keyval = VK_RCONTROL;
            break;
        case VK_MENU:
            if (::GetKeyState (VK_LMENU) & 0x8000) keyval = VK_LMENU;
            else if (::GetKeyState (VK_RMENU) & 0x8000) keyval = VK_RMENU;
            break;
    }
    return keyval;
}
#endif

/** @class QIHotKeyEdit
 *
 *  The QIHotKeyEdit widget is a hot key editor.
 */

const char *QIHotKeyEdit::kNoneSymbName = "<none>";

QIHotKeyEdit::QIHotKeyEdit (QWidget *aParent) :
    QLabel (aParent)
{
#ifdef Q_WS_X11
    /* Initialize the X keyboard subsystem */
    initXKeyboard (QX11Info::display());
#endif

    clear();

    setAttribute (Qt::WA_NativeWindow);
    setFrameStyle (QFrame::StyledPanel | Sunken);
    setAlignment (Qt::AlignCenter);
    setFocusPolicy (Qt::StrongFocus);
    setAutoFillBackground (true);

    QPalette p = palette();
    p.setColor (QPalette::Active, QPalette::Foreground,
                p.color (QPalette::Active, QPalette::Text));
    p.setColor (QPalette::Active, QPalette::Background,
                p.color (QPalette::Active, QPalette::Base));
    setPalette (p);

#ifdef Q_WS_MAC
    mDarwinKeyModifiers = GetCurrentEventKeyModifiers();
# ifdef QT_MAC_USE_COCOA
    ::VBoxCocoaApplication_setCallback (UINT32_MAX, QIHotKeyEdit::darwinEventHandlerProc, this);
# else  /* !QT_MAC_USE_COCOA */
    EventTypeSpec eventTypes [4];
    eventTypes [0].eventClass = kEventClassKeyboard;
    eventTypes [0].eventKind  = kEventRawKeyDown;
    eventTypes [1].eventClass = kEventClassKeyboard;
    eventTypes [1].eventKind  = kEventRawKeyUp;
    eventTypes [2].eventClass = kEventClassKeyboard;
    eventTypes [2].eventKind  = kEventRawKeyRepeat;
    eventTypes [3].eventClass = kEventClassKeyboard;
    eventTypes [3].eventKind  = kEventRawKeyModifiersChanged;

    EventHandlerUPP eventHandler = ::NewEventHandlerUPP (QIHotKeyEdit::darwinEventHandlerProc);

    mDarwinEventHandlerRef = NULL;
    ::InstallApplicationEventHandler (eventHandler, RT_ELEMENTS (eventTypes), &eventTypes [0],
                                      this, &mDarwinEventHandlerRef);
    ::DisposeEventHandlerUPP (eventHandler);
# endif /* !QT_MAC_USE_COCOA */
    ::DarwinGrabKeyboard (false /* just modifiers */);
#endif
}

QIHotKeyEdit::~QIHotKeyEdit()
{
#ifdef Q_WS_MAC
    ::DarwinReleaseKeyboard();
# ifdef QT_MAC_USE_COCOA
    ::VBoxCocoaApplication_unsetCallback (UINT32_MAX, QIHotKeyEdit::darwinEventHandlerProc, this);
# else
    ::RemoveEventHandler (mDarwinEventHandlerRef);
    mDarwinEventHandlerRef = NULL;
# endif
#endif
}

// Public members
/////////////////////////////////////////////////////////////////////////////

/**
 *  Set the hot key value. O means there is no hot key.
 *
 *  @note
 *      The key value is platform-dependent. On Win32 it is the
 *      virtial key, on Linux it is the first (0) keysym corresponding
 *      to the keycode.
 */
void QIHotKeyEdit::setKey (int aKeyVal)
{
    mKeyVal = aKeyVal;
    mSymbName = QIHotKeyEdit::keyName (aKeyVal);
    updateText();
}

/**@@ QIHotKeyEdit::key() const
 *
 *  Returns the value of the last recodred hot key.
 *  O means there is no hot key.
 *
 *  @note
 *      The key value is platform-dependent. On Win32 it is the
 *      virtial key, on Linux it is the first (0) keysym corresponding
 *      to the keycode.
 */

/**
 *  Stolen from QLineEdit.
 */
QSize QIHotKeyEdit::sizeHint() const
{
    ensurePolished();
    QFontMetrics fm (font());
    int h = qMax (fm.lineSpacing(), 14) + 2;
    int w = fm.width ('x') * 17; // "some"
    int m = frameWidth() * 2;
    QStyleOption option;
    option.initFrom (this);
    return (style()->sizeFromContents (QStyle::CT_LineEdit, &option,
                                       QSize (w + m, h + m)
                                       .expandedTo (QApplication::globalStrut()),
                                       this));
}

/**
 *  Stolen from QLineEdit.
 */
QSize QIHotKeyEdit::minimumSizeHint() const
{
    ensurePolished();
    QFontMetrics fm = fontMetrics();
    int h = fm.height() + qMax (2, fm.leading());
    int w = fm.maxWidth();
    int m = frameWidth() * 2;
    return QSize (w + m, h + m);
}

#if defined (Q_WS_PM)
/**
 *  Returns the virtual key extracted from the QMSG structure.
 *
 *  This function tries to detect some extra virtual keys definitions missing
 *  in PM (like Left Shift, Left Ctrl, Win keys). In all other cases it simply
 *  returns SHORT2FROMMP (aMsg->mp2).
 *
 *  @param aMsg  Pointer to the QMSG structure to extract the virtual key from.
 *  @return The extracted virtual key code or zero if there is no virtual key.
 */
/* static */
int QIHotKeyEdit::virtualKey (QMSG *aMsg)
{
    USHORT f = SHORT1FROMMP (aMsg->mp1);
    CHAR scan = CHAR4FROMMP (aMsg->mp1);
    USHORT ch = SHORT1FROMMP (aMsg->mp2);
    int vkey = (unsigned int) SHORT2FROMMP (aMsg->mp2);

    if (f & KC_VIRTUALKEY)
    {
        /* distinguish Left Shift from Right Shift) */
        if (vkey == VK_SHIFT && scan == 0x2A)
            vkey = VK_LSHIFT;
        /* distinguish Left Ctrl from Right Ctrl */
        else if (vkey == VK_CTRL && scan == 0x1D)
            vkey = VK_LCTRL;
        /* distinguish Ctrl+ScrLock from Ctrl+Break */
        else if (vkey == VK_BREAK && scan == 0x46 && f & KC_CTRL)
            vkey = VK_SCRLLOCK;
    }
    else if (!(f & KC_CHAR))
    {
        /* detect some special keys that have a pseudo char code in the high
         * byte of ch (probably this is less device-dependent than
         * scancode) */
        switch (ch)
        {
            case 0xEC00: vkey = VK_LWIN; break;
            case 0xED00: vkey = VK_RWIN; break;
            case 0xEE00: vkey = VK_WINMENU; break;
            case 0xF900: vkey = VK_FORWARD; break;
            case 0xFA00: vkey = VK_BACKWARD; break;
            default: vkey = 0;
        }
    }

    return vkey;
}
#endif

#if defined (Q_WS_PM)
/**
 *  Updates the associative array containing the translations of PM virtual
 *  keys to human readable key names.
 */
void QIHotKeyEdit::retranslateUi()
{
    /* Note: strings for the same key must match strings in retranslateUi()
     * versions for all platforms, to keep translators happy. */

    sKeyNames [VK_LSHIFT]        = tr ("Left Shift");
    sKeyNames [VK_SHIFT]         = tr ("Right Shift");
    sKeyNames [VK_LCTRL]         = tr ("Left Ctrl");
    sKeyNames [VK_CTRL]          = tr ("Right Ctrl");
    sKeyNames [VK_ALT]           = tr ("Left Alt");
    sKeyNames [VK_ALTGRAF]       = tr ("Right Alt");
    sKeyNames [VK_LWIN]          = tr ("Left WinKey");
    sKeyNames [VK_RWIN]          = tr ("Right WinKey");
    sKeyNames [VK_WINMENU]       = tr ("Menu key");
    sKeyNames [VK_CAPSLOCK]      = tr ("Caps Lock");
    sKeyNames [VK_SCRLLOCK]      = tr ("Scroll Lock");

    sKeyNames [VK_PAUSE]         = tr ("Pause");
    sKeyNames [VK_PRINTSCRN]     = tr ("Print Screen");

    sKeyNames [VK_F1]            = tr ("F1");
    sKeyNames [VK_F2]            = tr ("F2");
    sKeyNames [VK_F3]            = tr ("F3");
    sKeyNames [VK_F4]            = tr ("F4");
    sKeyNames [VK_F5]            = tr ("F5");
    sKeyNames [VK_F6]            = tr ("F6");
    sKeyNames [VK_F7]            = tr ("F7");
    sKeyNames [VK_F8]            = tr ("F8");
    sKeyNames [VK_F9]            = tr ("F9");
    sKeyNames [VK_F10]           = tr ("F10");
    sKeyNames [VK_F11]           = tr ("F11");
    sKeyNames [VK_F12]           = tr ("F12");
    sKeyNames [VK_F13]           = tr ("F13");
    sKeyNames [VK_F14]           = tr ("F14");
    sKeyNames [VK_F15]           = tr ("F15");
    sKeyNames [VK_F16]           = tr ("F16");
    sKeyNames [VK_F17]           = tr ("F17");
    sKeyNames [VK_F18]           = tr ("F18");
    sKeyNames [VK_F19]           = tr ("F19");
    sKeyNames [VK_F20]           = tr ("F20");
    sKeyNames [VK_F21]           = tr ("F21");
    sKeyNames [VK_F22]           = tr ("F22");
    sKeyNames [VK_F23]           = tr ("F23");
    sKeyNames [VK_F24]           = tr ("F24");

    sKeyNames [VK_NUMLOCK]       = tr ("Num Lock");
    sKeyNames [VK_FORWARD]       = tr ("Forward");
    sKeyNames [VK_BACKWARD]      = tr ("Back");
}
#elif defined (Q_WS_X11)
/**
 *  Updates the associative array containing the translations of X11 key strings to human
 *  readable key names.
 */
void QIHotKeyEdit::retranslateUi()
{
    /* Note: strings for the same key must match strings in retranslateUi()
     * versions for all platforms, to keep translators happy. */

    sKeyNames ["Shift_L"]          = tr ("Left Shift");
    sKeyNames ["Shift_R"]          = tr ("Right Shift");
    sKeyNames ["Control_L"]        = tr ("Left Ctrl");
    sKeyNames ["Control_R"]        = tr ("Right Ctrl");
    sKeyNames ["Alt_L"]            = tr ("Left Alt");
    sKeyNames ["Alt_R"]            = tr ("Right Alt");
    sKeyNames ["Super_L"]          = tr ("Left WinKey");
    sKeyNames ["Super_R"]          = tr ("Right WinKey");
    sKeyNames ["Menu"]             = tr ("Menu key");
    sKeyNames ["ISO_Level3_Shift"] = tr ("Alt Gr");
    sKeyNames ["Caps_Lock"]        = tr ("Caps Lock");
    sKeyNames ["Scroll_Lock"]      = tr ("Scroll Lock");
}
#endif

/**
 *  Returns the string representation of a given key.
 *
 *  @note
 *      The key value is platform-dependent. On Win32 it is the
 *      virtial key, on Linux it is the first (0) keysym corresponding
 *      to the keycode.
 */
/* static */
QString QIHotKeyEdit::keyName (int aKeyVal)
{
    QString name;

    if (!aKeyVal)
    {
        name = tr (kNoneSymbName);
    }
    else
    {
#if defined (Q_WS_WIN32)
        /* Stupid MapVirtualKey doesn't distinguish between right and left
         * vkeys, even under XP, despite that it stated in msdn. Do it by
         * hands. Besides that it can't recognize such virtual keys as
         * VK_DIVIDE & VK_PAUSE, this is also known bug. */
        int scan;
        switch (aKeyVal)
        {
            /* Processing special keys... */
            case VK_PAUSE: scan = 0x45 << 16; break;
            case VK_RSHIFT: scan = 0x36 << 16; break;
            case VK_RCONTROL: scan = (0x1D << 16) | (1 << 24); break;
            case VK_RMENU: scan = (0x38 << 16) | (1 << 24); break;
            /* Processing extended keys... */
            case VK_APPS:
            case VK_LWIN:
            case VK_RWIN:
            case VK_NUMLOCK: scan = (::MapVirtualKey (aKeyVal, 0) | 256) << 16; break;
            default: scan = ::MapVirtualKey (aKeyVal, 0) << 16;
        }
        TCHAR *str = new TCHAR [256];
        if (::GetKeyNameText (scan, str, 256))
        {
            name = QString::fromUtf16 (str);
        }
        else
        {
            AssertFailed();
            name = QString (tr ("<key_%1>")).arg (aKeyVal);
        }
        delete[] str;
#elif defined (Q_WS_PM)
        name = sKeyNames [aKeyVal];
        if (name.isNull())
        {
            AssertFailed();
            name = QString (tr ("<key_%1>")).arg (aKeyVal);
        }
#elif defined (Q_WS_X11)
        char *sn = ::XKeysymToString ((KeySym) aKeyVal);
        if (sn)
        {
            name = sKeyNames [sn];
            if (name.isEmpty())
                name = sn;
        }
        else
        {
            AssertFailed();
            name = QString (tr ("<key_%1>")).arg (aKeyVal);
        }
#elif defined(Q_WS_MAC)
        UInt32 modMask = DarwinKeyCodeToDarwinModifierMask (aKeyVal);
        switch (modMask)
        {
            case shiftKey:
            case optionKey:
            case controlKey:
            case cmdKey:
                name = tr ("Left ");
                break;
            case rightShiftKey:
            case rightOptionKey:
            case rightControlKey:
            case kEventKeyModifierRightCmdKeyMask:
                name = tr ("Right ");
                break;
            default:
                AssertMsgFailedReturn (("modMask=%#x\n", modMask), QString());
        }
        switch (modMask)
        {
            case shiftKey:
            case rightShiftKey:
                name += QChar (kShiftUnicode);
                break;
            case optionKey:
            case rightOptionKey:
                name += QChar (kOptionUnicode);
                break;
            case controlKey:
            case rightControlKey:
                name += QChar (kControlUnicode);
                break;
            case cmdKey:
            case kEventKeyModifierRightCmdKeyMask:
                name += QChar (kCommandUnicode);
                break;
        }
#else
        AssertFailed();
        name = QString (tr ("<key_%1>")).arg (aKeyVal);
#endif
    }

    return name;
}

/* static */
bool QIHotKeyEdit::isValidKey (int aKeyVal)
{
#if defined(Q_WS_WIN32)
    return (
        (aKeyVal >= VK_SHIFT && aKeyVal <= VK_CAPITAL) ||
        aKeyVal == VK_PRINT ||
        aKeyVal == VK_LWIN || aKeyVal == VK_RWIN ||
        aKeyVal == VK_APPS ||
        (aKeyVal >= VK_F1 && aKeyVal <= VK_F24) ||
        aKeyVal == VK_NUMLOCK || aKeyVal == VK_SCROLL ||
        (aKeyVal >= VK_LSHIFT && aKeyVal <= VK_RMENU));
#elif defined(Q_WS_PM)
    return (
        (aKeyVal >= VK_SHIFT && aKeyVal <= VK_CAPSLOCK) ||
        aKeyVal == VK_PRINTSCRN ||
        (aKeyVal >= VK_F1 && aKeyVal <= VK_F24) ||
        aKeyVal == VK_NUMLOCK || aKeyVal == VK_SCRLLOCK ||
        (aKeyVal >= VK_LSHIFT && aKeyVal <= VK_BACKWARD));
#elif defined(Q_WS_X11)
    KeySym ks = (KeySym) aKeyVal;
    return
        (
            ks != NoSymbol &&
            ks != XK_Insert
        ) && (
            ks == XK_Scroll_Lock ||
            IsModifierKey (ks) ||
            IsFunctionKey (ks) ||
            IsMiscFunctionKey (ks)
        );
#elif defined(Q_WS_MAC)
    UInt32 modMask = ::DarwinKeyCodeToDarwinModifierMask (aKeyVal);
    switch (modMask)
    {
        case shiftKey:
        case optionKey:
        case controlKey:
        case rightShiftKey:
        case rightOptionKey:
        case rightControlKey:
        case cmdKey:
        case kEventKeyModifierRightCmdKeyMask:
            return true;
        default:
            return false;
    }
#else
    Q_UNUSED (aKeyVal);
    return true;
#endif
}

// Public slots
/////////////////////////////////////////////////////////////////////////////

void QIHotKeyEdit::clear()
{
    mKeyVal = 0;
    mSymbName = tr (kNoneSymbName);
    updateText();
}

// Protected members
/////////////////////////////////////////////////////////////////////////////

// Protected events
/////////////////////////////////////////////////////////////////////////////

#if defined (Q_WS_WIN32)

bool QIHotKeyEdit::winEvent (MSG *aMsg, long* /* aResult */)
{
    if (!(aMsg->message == WM_KEYDOWN || aMsg->message == WM_SYSKEYDOWN ||
          aMsg->message == WM_KEYUP || aMsg->message == WM_SYSKEYUP ||
          aMsg->message == WM_CHAR || aMsg->message == WM_SYSCHAR ||
          aMsg->message == WM_DEADCHAR || aMsg->message == WM_SYSDEADCHAR ||
          aMsg->message == WM_CONTEXTMENU))
        return false;

    /* ignore if not a valid hot key */
    if (!isValidKey (aMsg->wParam))
        return false;

#if 0
    LogFlow (("%WM_%04X: vk=%04X rep=%05d scan=%02X ext=%01d"
              "rzv=%01X ctx=%01d prev=%01d tran=%01d\n",
              aMsg->message, aMsg->wParam,
              (aMsg->lParam & 0xFFFF),
              ((aMsg->lParam >> 16) & 0xFF),
              ((aMsg->lParam >> 24) & 0x1),
              ((aMsg->lParam >> 25) & 0xF),
              ((aMsg->lParam >> 29) & 0x1),
              ((aMsg->lParam >> 30) & 0x1),
              ((aMsg->lParam >> 31) & 0x1)));
#endif

    if (aMsg->message == WM_KEYDOWN || aMsg->message == WM_SYSKEYDOWN)
    {
        /* determine platform-dependent key */
        mKeyVal = qi_distinguish_modifier_vkey (aMsg->wParam);
        /* determine symbolic name */
        TCHAR *str = new TCHAR [256];
        if (::GetKeyNameText (aMsg->lParam, str, 256))
        {
            mSymbName = QString::fromUtf16 (str);
        }
        else
        {
            AssertFailed();
            mSymbName = QString (tr ("<key_%1>")).arg (mKeyVal);
        }
        delete[] str;
        /* update the display */
        updateText();
    }

    return true;
}

#elif defined (Q_WS_PM)

bool QIHotKeyEdit::pmEvent (QMSG *aMsg)
{
    if (aMsg->msg != WM_CHAR)
        return false;

    USHORT f = SHORT1FROMMP (aMsg->mp1);

    int vkey = QIHotKeyEdit::virtualKey (aMsg);

    /* ignore if not a valid hot key */
    if (!isValidKey (vkey))
        return false;

    if (!(f & KC_KEYUP))
    {
        /* determine platform-dependent key */
        mKeyVal = vkey;
        /* determine symbolic name */
        mSymbName = QIHotKeyEdit::keyName (mKeyVal);
        /* update the display */
        updateText();
    }

    return true;
}

#elif defined (Q_WS_X11)

bool QIHotKeyEdit::x11Event (XEvent *event)
{
    switch (event->type)
    {
        case XKeyPress:
        case XKeyRelease:
        {
            XKeyEvent *ke = (XKeyEvent *) event;
            KeySym ks = ::XKeycodeToKeysym (ke->display, ke->keycode, 0);
            /* ignore if not a valid hot key */
            if (!isValidKey ((int) ks))
                return false;

            /* skip key releases */
            if (event->type == XKeyRelease)
                return true;

            /* determine platform-dependent key */
            mKeyVal = (int) ks;
            /* determine symbolic name */
            mSymbName = QIHotKeyEdit::keyName (mKeyVal);
            /* update the display */
            updateText();
#if 0
            LogFlow (("%s: state=%08X keycode=%08X keysym=%08X symb=%s\n",
                      event->type == XKeyPress ? "XKeyPress" : "XKeyRelease",
                      ke->state, ke->keycode, ks,
                      symbname.latin1()));
#endif
            return true;
        }
    }

    return false;
}

#elif defined (Q_WS_MAC)
# ifdef QT_MAC_USE_COCOA
/* static */
bool QIHotKeyEdit::darwinEventHandlerProc (const void *pvCocoaEvent, const void *pvCarbonEvent, void *pvUser)
{
    QIHotKeyEdit *edit = (QIHotKeyEdit *) pvUser;
    EventRef inEvent = (EventRef)pvCarbonEvent;
    UInt32 EventClass = ::GetEventClass (inEvent);
    if (EventClass == kEventClassKeyboard)
        return edit->darwinKeyboardEvent (pvCocoaEvent, inEvent);
    return false;
}

# else  /* !QT_MAC_USE_COCOA */
/* static */
pascal OSStatus QIHotKeyEdit::darwinEventHandlerProc (EventHandlerCallRef inHandlerCallRef,
                                                      EventRef inEvent, void *inUserData)
{
    QIHotKeyEdit *edit = (QIHotKeyEdit *) inUserData;
    UInt32 EventClass = ::GetEventClass (inEvent);
    if (EventClass == kEventClassKeyboard)
    {
        if (edit->darwinKeyboardEvent (NULL, inEvent))
            return 0;
    }
    return CallNextEventHandler (inHandlerCallRef, inEvent);
}
# endif /* !QT_MAC_USE_COCOA */

bool QIHotKeyEdit::darwinKeyboardEvent (const void *pvCocoaEvent, EventRef inEvent)
{
#if 0 /* for debugging */
    ::darwinDebugPrintEvent("QIHotKeyEdit", inEvent);
#endif

    /* ignore key changes unless we're the focus widget */
    if (!hasFocus())
        return false;

    UInt32 eventKind = ::GetEventKind (inEvent);
    switch (eventKind)
    {
        /*case kEventRawKeyDown:
        case kEventRawKeyUp:
        case kEventRawKeyRepeat:*/
        case kEventRawKeyModifiersChanged:
        {
            UInt32 modifierMask = 0;
            ::GetEventParameter (inEvent, kEventParamKeyModifiers, typeUInt32, NULL,
                                 sizeof (modifierMask), NULL, &modifierMask);

            modifierMask = ::DarwinAdjustModifierMask (modifierMask, pvCocoaEvent);
            UInt32 changed = mDarwinKeyModifiers ^ modifierMask;
            mDarwinKeyModifiers = modifierMask;

            /* skip key releases */
            if (changed && (changed & modifierMask))
                break;

            /* convert to keycode and skip keycodes we don't care about. */
            unsigned keyCode = ::DarwinModifierMaskToDarwinKeycode (changed);
            if (!keyCode || keyCode == ~0U || !isValidKey (keyCode))
                break;

            /* update key current key. */
            mKeyVal = keyCode;
            mSymbName = QIHotKeyEdit::keyName (keyCode);
            updateText();
            break; //return true;
        }
        break;
    }
    return false;
}

#else
# warning "Port me!"
#endif

void QIHotKeyEdit::focusInEvent (QFocusEvent *aEvent)
{
    QLabel::focusInEvent (aEvent);

    QPalette p = palette();
    p.setColor (QPalette::Active, QPalette::Foreground,
                p.color (QPalette::Active, QPalette::HighlightedText));
    p.setColor (QPalette::Active, QPalette::Background,
                p.color (QPalette::Active, QPalette::Highlight));
    setPalette (p);
}

void QIHotKeyEdit::focusOutEvent (QFocusEvent *aEvent)
{
    QLabel::focusOutEvent (aEvent);

    QPalette p = palette();
    p.setColor (QPalette::Active, QPalette::Foreground,
                p.color (QPalette::Active, QPalette::Text));
    p.setColor (QPalette::Active, QPalette::Background,
                p.color (QPalette::Active, QPalette::Base));
    setPalette (p);
}

void QIHotKeyEdit::paintEvent (QPaintEvent *aEvent)
{
    if (hasFocus())
    {
        QStylePainter painter (this);
        QStyleOptionFocusRect option;
        option.initFrom (this);
        option.backgroundColor = palette().color (QPalette::Background);
        option.rect = contentsRect();
        painter.drawPrimitive (QStyle::PE_FrameFocusRect, option);
    }
    QLabel::paintEvent (aEvent);
}

// Private members
/////////////////////////////////////////////////////////////////////////////

void QIHotKeyEdit::updateText()
{
    setText (QString (" %1 ").arg (mSymbName));
}

