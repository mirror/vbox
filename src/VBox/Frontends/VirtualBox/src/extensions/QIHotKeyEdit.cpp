/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIHotKeyEdit class implementation
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Local includes */
#include "QIHotKeyEdit.h"
#include "VBoxDefs.h"
#include "VBoxGlobal.h"

/* Global includes */
#include <QApplication>
#include <QStyleOption>
#include <QStylePainter>

#ifdef Q_WS_WIN
/* VBox/cdefs.h defines these: */
# undef LOWORD
# undef HIWORD
# undef LOBYTE
# undef HIBYTE
# include <windows.h>
#endif /* Q_WS_WIN */

#ifdef Q_WS_X11
/* We need to capture some X11 events directly which
 * requires the XEvent structure to be defined. However,
 * including the Xlib header file will cause some nasty
 * conflicts with Qt. Therefore we use the following hack
 * to redefine those conflicting identifiers. */
# define XK_XKB_KEYS
# define XK_MISCELLANY
# include <X11/Xlib.h>
# include <X11/Xutil.h>
# include <X11/keysymdef.h>
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
QMap<QString, QString> QIHotKeyEdit::s_keyNames;
# include <QX11Info>
#endif /* Q_WS_X11 */

#ifdef Q_WS_MAC
# include "UICocoaApplication.h"
# include "DarwinKeyboard.h"
# include "VBoxUtils.h"
# include <Carbon/Carbon.h>
#endif


#ifdef Q_WS_WIN
/* Returns the correct modifier vkey for the *last* keyboard message,
 * distinguishing between left and right keys. If both are pressed
 * the left key wins. If the pressed key not a modifier, wParam is returned
 * unchanged. */
int qi_distinguish_modifier_vkey(WPARAM wParam)
{
    int keyval = wParam;
    switch (wParam)
    {
        case VK_SHIFT:
            if (::GetKeyState(VK_LSHIFT) & 0x8000) keyval = VK_LSHIFT;
            else if (::GetKeyState(VK_RSHIFT) & 0x8000) keyval = VK_RSHIFT;
            break;
        case VK_CONTROL:
            if (::GetKeyState(VK_LCONTROL) & 0x8000) keyval = VK_LCONTROL;
            else if (::GetKeyState(VK_RCONTROL) & 0x8000) keyval = VK_RCONTROL;
            break;
        case VK_MENU:
            if (::GetKeyState(VK_LMENU) & 0x8000) keyval = VK_LMENU;
            else if (::GetKeyState(VK_RMENU) & 0x8000) keyval = VK_RMENU;
            break;
    }
    return keyval;
}
#endif /* Q_WS_WIN */


const char *QIHotKeyEdit::m_spNoneSymbName = "None";

QIHotKeyEdit::QIHotKeyEdit(QWidget *pParent)
    : QLabel(pParent)
{
#ifdef Q_WS_X11
    /* Initialize the X keyboard subsystem: */
    initMappedX11Keyboard(QX11Info::display(), vboxGlobal().settings().publicProperty("GUI/RemapScancodes"));
#endif /* Q_WS_X11 */

    clear();

#ifdef Q_WS_WIN
    /* Qt documentation hasn't mentioned this is windows-only flag,
     * but looks like that is so, anyway it is required for winEvent() handler only: */
    setAttribute(Qt::WA_NativeWindow);
#endif /* Q_WS_WIN */
    setFrameStyle(QFrame::StyledPanel | Sunken);
    setAlignment(Qt::AlignCenter);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(true);

    QPalette p = palette();
    p.setColor(QPalette::Active, QPalette::Foreground, p.color(QPalette::Active, QPalette::Text));
    p.setColor(QPalette::Active, QPalette::Background, p.color(QPalette::Active, QPalette::Base));
    setPalette(p);

#ifdef Q_WS_MAC
    m_uDarwinKeyModifiers = GetCurrentEventKeyModifiers();
    UICocoaApplication::instance()->registerForNativeEvents(RT_BIT_32(10) | RT_BIT_32(11) | RT_BIT_32(12) /* NSKeyDown  | NSKeyUp | | NSFlagsChanged */, QIHotKeyEdit::darwinEventHandlerProc, this);
    ::DarwinGrabKeyboard(false /* just modifiers */);
#endif /* Q_WS_MAC */
}

QIHotKeyEdit::~QIHotKeyEdit()
{
#ifdef Q_WS_MAC
    ::DarwinReleaseKeyboard();
    UICocoaApplication::instance()->unregisterForNativeEvents(RT_BIT_32(10) | RT_BIT_32(11) | RT_BIT_32(12) /* NSKeyDown  | NSKeyUp | | NSFlagsChanged */, QIHotKeyEdit::darwinEventHandlerProc, this);
#endif
}

/**
 *  Set the hot key value. O means there is no hot key.
 *
 *  @note
 *      The key value is platform-dependent. On Win32 it is the
 *      virtual key, on Linux it is the first (0) keysym corresponding
 *      to the keycode.
 */
void QIHotKeyEdit::setKey(int iKeyVal)
{
    m_iKeyVal = iKeyVal;
    m_strSymbName = QIHotKeyEdit::keyName(iKeyVal);
    updateText();
}

/**@@ QIHotKeyEdit::key() const
 *
 *  Returns the value of the last recorded hot key.
 *  O means there is no hot key.
 *
 *  @note
 *      The key value is platform-dependent. On Win32 it is the
 *      virtual key, on Linux it is the first (0) keysym corresponding
 *      to the keycode.
 */

QSize QIHotKeyEdit::sizeHint() const
{
    ensurePolished();
    QFontMetrics fm(font());
    int h = qMax(fm.lineSpacing(), 14) + 2;
    int w = fm.width('x') * 17;
    int m = frameWidth() * 2;
    QStyleOption option;
    option.initFrom(this);
    return (style()->sizeFromContents(QStyle::CT_LineEdit, &option,
                                      QSize(w + m, h + m)
                                      .expandedTo(QApplication::globalStrut()),
                                      this));
}

QSize QIHotKeyEdit::minimumSizeHint() const
{
    ensurePolished();
    QFontMetrics fm = fontMetrics();
    int h = fm.height() + qMax(2, fm.leading());
    int w = fm.maxWidth();
    int m = frameWidth() * 2;
    return QSize(w + m, h + m);
}

/**
 *  Returns the string representation of a given key.
 *
 *  @note
 *      The key value is platform-dependent. On Win32 it is the
 *      virtual key, on Linux it is the first (0) keysym corresponding
 *      to the keycode.
 */
/* static */
QString QIHotKeyEdit::keyName(int iKeyVal)
{
    QString strName;

    if (!iKeyVal)
    {
        strName = tr(m_spNoneSymbName);
    }
    else
    {
#if defined (Q_WS_WIN)
        /* Stupid MapVirtualKey doesn't distinguish between right and left
         * vkeys, even under XP, despite that it stated in msdn. Do it by
         * hands. Besides that it can't recognize such virtual keys as
         * VK_DIVIDE & VK_PAUSE, this is also known bug. */
        int scan;
        switch (iKeyVal)
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
            case VK_NUMLOCK: scan = (::MapVirtualKey(iKeyVal, 0) | 256) << 16; break;
            default: scan = ::MapVirtualKey(iKeyVal, 0) << 16;
        }
        TCHAR *str = new TCHAR[256];
        if (::GetKeyNameText(scan, str, 256))
        {
            strName = QString::fromUtf16(str);
        }
        else
        {
            AssertFailed();
            strName = QString(tr("<key_%1>")).arg(iKeyVal);
        }
        delete[] str;
#elif defined (Q_WS_X11)
        char *sn = ::XKeysymToString((KeySym)iKeyVal);
        if (sn)
        {
            strName = s_keyNames[sn];
            if (strName.isEmpty())
                strName = sn;
        }
        else
        {
            AssertFailed();
            strName = QString(tr("<key_%1>")).arg(iKeyVal);
        }
#elif defined(Q_WS_MAC)
        UInt32 modMask = DarwinKeyCodeToDarwinModifierMask(iKeyVal);
        switch (modMask)
        {
            case shiftKey:
            case optionKey:
            case controlKey:
            case cmdKey:
                strName = tr("Left ");
                break;
            case rightShiftKey:
            case rightOptionKey:
            case rightControlKey:
            case kEventKeyModifierRightCmdKeyMask:
                strName = tr("Right ");
                break;
            default:
                AssertMsgFailedReturn(("modMask=%#x\n", modMask), QString());
        }
        switch (modMask)
        {
            case shiftKey:
            case rightShiftKey:
                strName += QChar(kShiftUnicode);
                break;
            case optionKey:
            case rightOptionKey:
                strName += QChar(kOptionUnicode);
                break;
            case controlKey:
            case rightControlKey:
                strName += QChar(kControlUnicode);
                break;
            case cmdKey:
            case kEventKeyModifierRightCmdKeyMask:
                strName += QChar(kCommandUnicode);
                break;
        }
#else
        AssertFailed();
        strName = QString(tr("<key_%1>")).arg(iKeyVal);
#endif
    }

    return strName;
}

/* static */
bool QIHotKeyEdit::isValidKey(int iKeyVal)
{
    /* Empty value is correct: */
    if (iKeyVal == 0)
        return true;
#if defined(Q_WS_WIN)
    return ((iKeyVal >= VK_SHIFT && iKeyVal <= VK_CAPITAL) ||
            iKeyVal == VK_PRINT ||
            iKeyVal == VK_LWIN || iKeyVal == VK_RWIN ||
            iKeyVal == VK_APPS ||
            (iKeyVal >= VK_F1 && iKeyVal <= VK_F24) ||
            iKeyVal == VK_NUMLOCK || iKeyVal == VK_SCROLL ||
            (iKeyVal >= VK_LSHIFT && iKeyVal <= VK_RMENU));
#elif defined(Q_WS_X11)
    KeySym ks = (KeySym)iKeyVal;
    return (ks != NoSymbol && ks != XK_Insert) &&
           (ks == XK_Scroll_Lock || IsModifierKey(ks) || IsFunctionKey(ks) || IsMiscFunctionKey(ks));
#elif defined(Q_WS_MAC)
    UInt32 modMask = ::DarwinKeyCodeToDarwinModifierMask(iKeyVal);
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
    Q_UNUSED(iKeyVal);
    return true;
#endif
}

#ifdef Q_WS_X11
/* Updates the associative array containing the translations
 * of X11 key strings to human readable key names. */
void QIHotKeyEdit::retranslateUi()
{
    /* Note: strings for the same key must match strings in retranslateUi()
     * versions for all platforms, to keep translators happy. */

    s_keyNames["Shift_L"]          = tr("Left Shift");
    s_keyNames["Shift_R"]          = tr("Right Shift");
    s_keyNames["Control_L"]        = tr("Left Ctrl");
    s_keyNames["Control_R"]        = tr("Right Ctrl");
    s_keyNames["Alt_L"]            = tr("Left Alt");
    s_keyNames["Alt_R"]            = tr("Right Alt");
    s_keyNames["Super_L"]          = tr("Left WinKey");
    s_keyNames["Super_R"]          = tr("Right WinKey");
    s_keyNames["Menu"]             = tr("Menu key");
    s_keyNames["ISO_Level3_Shift"] = tr("Alt Gr");
    s_keyNames["Caps_Lock"]        = tr("Caps Lock");
    s_keyNames["Scroll_Lock"]      = tr("Scroll Lock");
}
#endif /* Q_WS_X11 */

void QIHotKeyEdit::clear()
{
    m_iKeyVal = 0;
    m_strSymbName = tr(m_spNoneSymbName);
    updateText();
}

#if defined (Q_WS_WIN)

bool QIHotKeyEdit::winEvent(MSG *pMsg, long* /* pResult */)
{
    if (!(pMsg->message == WM_KEYDOWN || pMsg->message == WM_SYSKEYDOWN ||
          pMsg->message == WM_KEYUP || pMsg->message == WM_SYSKEYUP ||
          pMsg->message == WM_CHAR || pMsg->message == WM_SYSCHAR ||
          pMsg->message == WM_DEADCHAR || pMsg->message == WM_SYSDEADCHAR ||
          pMsg->message == WM_CONTEXTMENU))
        return false;

    /* Ignore if not a valid hot key: */
    if (!isValidKey(pMsg->wParam))
        return false;

#if 0
    LogFlow(("%WM_%04X: vk=%04X rep=%05d scan=%02X ext=%01d"
             "rzv=%01X ctx=%01d prev=%01d tran=%01d\n",
             pMsg->message, pMsg->wParam,
             (pMsg->lParam & 0xFFFF),
             ((pMsg->lParam >> 16) & 0xFF),
             ((pMsg->lParam >> 24) & 0x1),
             ((pMsg->lParam >> 25) & 0xF),
             ((pMsg->lParam >> 29) & 0x1),
             ((pMsg->lParam >> 30) & 0x1),
             ((pMsg->lParam >> 31) & 0x1)));
#endif

    if (pMsg->message == WM_KEYDOWN || pMsg->message == WM_SYSKEYDOWN)
    {
        /* Determine platform-dependent key: */
        m_iKeyVal = qi_distinguish_modifier_vkey(pMsg->wParam);
        /* Determine symbolic name: */
        TCHAR *pStr = new TCHAR[256];
        if (::GetKeyNameText(pMsg->lParam, pStr, 256))
        {
            m_strSymbName = QString::fromUtf16(pStr);
        }
        else
        {
            AssertFailed();
            m_strSymbName = QString(tr("<key_%1>")).arg(m_iKeyVal);
        }
        delete[] pStr;
        /* Update the display: */
        updateText();
    }

    return true;
}

#elif defined (Q_WS_X11)

bool QIHotKeyEdit::x11Event(XEvent *pEvent)
{
    switch (pEvent->type)
    {
        case XKeyPress:
        case XKeyRelease:
        {
            XKeyEvent *pKeyEvent = (XKeyEvent*)pEvent;
            KeySym ks = ::XKeycodeToKeysym(pKeyEvent->display, pKeyEvent->keycode, 0);
            /* Ignore if not a valid hot key: */
            if (!isValidKey((int)ks))
                return false;

            /* Skip key releases: */
            if (pEvent->type == XKeyRelease)
                return true;

            /* Determine platform-dependent key: */
            m_iKeyVal = (int)ks;
            /* Determine symbolic name: */
            m_strSymbName = QIHotKeyEdit::keyName(m_iKeyVal);
            /* Update the display: */
            updateText();
#if 0
            LogFlow(("%s: state=%08X keycode=%08X keysym=%08X symb=%s\n",
                     pEvent->type == XKeyPress ? "XKeyPress" : "XKeyRelease",
                     pKeyEvent->state, pKeyEvent->keycode, ks,
                     symbname.latin1()));
#endif
            return true;
        }
    }

    return false;
}

#elif defined (Q_WS_MAC)

/* static */
bool QIHotKeyEdit::darwinEventHandlerProc(const void *pvCocoaEvent, const void *pvCarbonEvent, void *pvUser)
{
    QIHotKeyEdit *edit = (QIHotKeyEdit*)pvUser;
    EventRef inEvent = (EventRef)pvCarbonEvent;
    UInt32 EventClass = ::GetEventClass(inEvent);
    if (EventClass == kEventClassKeyboard)
        return edit->darwinKeyboardEvent(pvCocoaEvent, inEvent);
    return false;
}

bool QIHotKeyEdit::darwinKeyboardEvent(const void *pvCocoaEvent, EventRef inEvent)
{
#if 0 /* for debugging */
    ::darwinDebugPrintEvent("QIHotKeyEdit", inEvent);
#endif

    /* Ignore key changes unless we're the focus widget: */
    if (!hasFocus())
        return false;

    UInt32 eventKind = ::GetEventKind(inEvent);
    switch (eventKind)
    {
        /*case kEventRawKeyDown:
        case kEventRawKeyUp:
        case kEventRawKeyRepeat:*/
        case kEventRawKeyModifiersChanged:
        {
            UInt32 modifierMask = 0;
            ::GetEventParameter(inEvent, kEventParamKeyModifiers, typeUInt32, NULL,
                                sizeof(modifierMask), NULL, &modifierMask);

            modifierMask = ::DarwinAdjustModifierMask(modifierMask, pvCocoaEvent);
            UInt32 changed = m_uDarwinKeyModifiers ^ modifierMask;
            m_uDarwinKeyModifiers = modifierMask;

            /* Skip key releases: */
            if (changed && (changed & modifierMask))
                break;

            /* Convert to keycode and skip keycodes we don't care about. */
            unsigned keyCode = ::DarwinModifierMaskToDarwinKeycode(changed);
            if (!keyCode || keyCode == ~0U || !isValidKey(keyCode))
                break;

            /* Update key current key: */
            m_iKeyVal = keyCode;
            m_strSymbName = QIHotKeyEdit::keyName(keyCode);
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

void QIHotKeyEdit::focusInEvent(QFocusEvent *pEvent)
{
    QLabel::focusInEvent(pEvent);

    QPalette p = palette();
    p.setColor(QPalette::Active, QPalette::Foreground, p.color(QPalette::Active, QPalette::HighlightedText));
    p.setColor(QPalette::Active, QPalette::Background, p.color(QPalette::Active, QPalette::Highlight));
    setPalette(p);
}

void QIHotKeyEdit::focusOutEvent(QFocusEvent *pEvent)
{
    QLabel::focusOutEvent(pEvent);

    QPalette p = palette();
    p.setColor(QPalette::Active, QPalette::Foreground, p.color(QPalette::Active, QPalette::Text));
    p.setColor(QPalette::Active, QPalette::Background, p.color(QPalette::Active, QPalette::Base));
    setPalette(p);
}

void QIHotKeyEdit::paintEvent(QPaintEvent *pEvent)
{
    if (hasFocus())
    {
        QStylePainter painter(this);
        QStyleOptionFocusRect option;
        option.initFrom(this);
        option.backgroundColor = palette().color(QPalette::Background);
        option.rect = contentsRect();
        painter.drawPrimitive(QStyle::PE_FrameFocusRect, option);
    }
    QLabel::paintEvent(pEvent);
}

void QIHotKeyEdit::updateText()
{
    setText(QString(" %1 ").arg(m_strSymbName));
}

