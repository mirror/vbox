/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: UIHotKeyEditor class implementation
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
#include "UIHotKeyEditor.h"
#include "VBoxDefs.h"
#include "VBoxGlobal.h"

/* Global includes */
#include <QApplication>
#include <QStyleOption>
#include <QStylePainter>
#include <QKeyEvent>

#ifdef Q_WS_WIN
# undef LOWORD
# undef HIWORD
# undef LOBYTE
# undef HIBYTE
# include <windows.h>
#endif /* Q_WS_WIN */

#ifdef Q_WS_X11
# include <QX11Info>
# include <X11/Xutil.h>
# include <X11/Xlib.h>
# include <X11/keysymdef.h>
# include "XKeyboard.h"
# ifdef KeyPress
   const int XKeyPress = KeyPress;
   const int XKeyRelease = KeyRelease;
#  undef KeyPress
#  undef KeyRelease
# endif /* KeyPress */
#endif /* Q_WS_X11 */

#ifdef Q_WS_MAC
# include "UICocoaApplication.h"
# include "DarwinKeyboard.h"
# include "VBoxUtils.h"
# include <Carbon/Carbon.h>
#endif /* Q_WS_MAC */


#ifdef Q_WS_X11
namespace UIHotKey
{
    QMap<QString, QString> m_keyNames;
}
#endif /* Q_WS_X11 */

QString UIHotKey::toString(int iKeyCode)
{
    QString strKeyName;

#ifdef Q_WS_WIN
    /* MapVirtualKey doesn't distinguish between right and left vkeys,
     * even under XP, despite that it stated in MSDN. Do it by hands.
     * Besides that it can't recognize such virtual keys as
     * VK_DIVIDE & VK_PAUSE, this is also known bug. */
    int iScan;
    switch (iKeyCode)
    {
        /* Processing special keys... */
        case VK_PAUSE: iScan = 0x45 << 16; break;
        case VK_RSHIFT: iScan = 0x36 << 16; break;
        case VK_RCONTROL: iScan = (0x1D << 16) | (1 << 24); break;
        case VK_RMENU: iScan = (0x38 << 16) | (1 << 24); break;
        /* Processing extended keys... */
        case VK_APPS:
        case VK_LWIN:
        case VK_RWIN:
        case VK_NUMLOCK: iScan = (::MapVirtualKey(iKeyCode, 0) | 256) << 16; break;
        default: iScan = ::MapVirtualKey(iKeyCode, 0) << 16;
    }
    TCHAR *pKeyName = new TCHAR[256];
    if (::GetKeyNameText(iScan, pKeyName, 256))
    {
        strKeyName = QString::fromUtf16(pKeyName);
    }
    else
    {
        AssertMsgFailed(("That key have no name!\n"));
        strKeyName = UIHotKeyEditor::tr("<key_%1>").arg(iKeyCode);
    }
    delete[] pKeyName;
#endif /* Q_WS_WIN */

#ifdef Q_WS_X11
    if (char *pNativeKeyName = ::XKeysymToString((KeySym)iKeyCode))
    {
        strKeyName = m_keyNames[pNativeKeyName].isEmpty() ?
                     QString(pNativeKeyName) : m_keyNames[pNativeKeyName];
    }
    else
    {
        AssertMsgFailed(("That key have no name!\n"));
        strKeyName = UIHotKeyEditor::tr("<key_%1>").arg(iKeyCode);
    }
#endif /* Q_WS_X11 */

#ifdef Q_WS_MAC
    UInt32 modMask = DarwinKeyCodeToDarwinModifierMask(iKeyCode);
    switch (modMask)
    {
        case shiftKey:
        case optionKey:
        case controlKey:
        case cmdKey:
            strKeyName = tr("Left ");
            break;
        case rightShiftKey:
        case rightOptionKey:
        case rightControlKey:
        case kEventKeyModifierRightCmdKeyMask:
            strKeyName = tr("Right ");
            break;
        default:
            AssertMsgFailedReturn(("modMask=%#x\n", modMask), QString());
    }
    switch (modMask)
    {
        case shiftKey:
        case rightShiftKey:
            strKeyName += QChar(kShiftUnicode);
            break;
        case optionKey:
        case rightOptionKey:
            strKeyName += QChar(kOptionUnicode);
            break;
        case controlKey:
        case rightControlKey:
            strKeyName += QChar(kControlUnicode);
            break;
        case cmdKey:
        case kEventKeyModifierRightCmdKeyMask:
            strKeyName += QChar(kCommandUnicode);
            break;
    }
#endif /* Q_WS_MAC */

    return strKeyName;
}

bool UIHotKey::isValidKey(int iKeyCode)
{
#ifdef Q_WS_WIN
    return ((iKeyCode >= VK_SHIFT && iKeyCode <= VK_CAPITAL) ||
            (iKeyCode >= VK_LSHIFT && iKeyCode <= VK_RMENU) ||
            (iKeyCode >= VK_F1 && iKeyCode <= VK_F24) ||
            iKeyCode == VK_NUMLOCK || iKeyCode == VK_SCROLL ||
            iKeyCode == VK_LWIN || iKeyCode == VK_RWIN ||
            iKeyCode == VK_APPS ||
            iKeyCode == VK_PRINT);
#endif /* Q_WS_WIN */

#ifdef Q_WS_X11
    return (IsModifierKey(iKeyCode) /* allow modifiers */ ||
            IsFunctionKey(iKeyCode) /* allow function keys */ ||
            IsMiscFunctionKey(iKeyCode) /* allow miscellaneous function keys */ ||
            iKeyCode == XK_Scroll_Lock /* allow 'Scroll Lock' missed in IsModifierKey() */) &&
           (iKeyCode != NoSymbol /* ignore some special symbol */ &&
            iKeyCode != XK_Insert /* ignore 'insert' included into IsMiscFunctionKey */);
#endif /* Q_WS_X11 */

#ifdef Q_WS_MAC
    UInt32 modMask = ::DarwinKeyCodeToDarwinModifierMask(iKeyCode);
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
#endif /* Q_WS_MAC */

    return false;
}

#ifdef Q_WS_WIN
int UIHotKey::distinguishModifierVKey(int wParam)
{
    int iKeyCode = wParam;
    switch (iKeyCode)
    {
        case VK_SHIFT:
            if (::GetKeyState(VK_LSHIFT) & 0x8000) iKeyCode = VK_LSHIFT;
            else if (::GetKeyState(VK_RSHIFT) & 0x8000) iKeyCode = VK_RSHIFT;
            break;
        case VK_CONTROL:
            if (::GetKeyState(VK_LCONTROL) & 0x8000) iKeyCode = VK_LCONTROL;
            else if (::GetKeyState(VK_RCONTROL) & 0x8000) iKeyCode = VK_RCONTROL;
            break;
        case VK_MENU:
            if (::GetKeyState(VK_LMENU) & 0x8000) iKeyCode = VK_LMENU;
            else if (::GetKeyState(VK_RMENU) & 0x8000) iKeyCode = VK_RMENU;
            break;
    }
    return iKeyCode;
}
#endif /* Q_WS_WIN */

#ifdef Q_WS_X11
void UIHotKey::retranslateKeyNames()
{
    m_keyNames["Shift_L"]          = UIHotKeyEditor::tr("Left Shift");
    m_keyNames["Shift_R"]          = UIHotKeyEditor::tr("Right Shift");
    m_keyNames["Control_L"]        = UIHotKeyEditor::tr("Left Ctrl");
    m_keyNames["Control_R"]        = UIHotKeyEditor::tr("Right Ctrl");
    m_keyNames["Alt_L"]            = UIHotKeyEditor::tr("Left Alt");
    m_keyNames["Alt_R"]            = UIHotKeyEditor::tr("Right Alt");
    m_keyNames["Super_L"]          = UIHotKeyEditor::tr("Left WinKey");
    m_keyNames["Super_R"]          = UIHotKeyEditor::tr("Right WinKey");
    m_keyNames["Menu"]             = UIHotKeyEditor::tr("Menu key");
    m_keyNames["ISO_Level3_Shift"] = UIHotKeyEditor::tr("Alt Gr");
    m_keyNames["Caps_Lock"]        = UIHotKeyEditor::tr("Caps Lock");
    m_keyNames["Scroll_Lock"]      = UIHotKeyEditor::tr("Scroll Lock");
}
#endif /* Q_WS_X11 */


QString UIHotKeyCombination::toReadableString(const QString &strKeyCombo)
{
    QStringList encodedKeyList = strKeyCombo.split(',');
    QStringList readableKeyList;
    for (int i = 0; i < encodedKeyList.size(); ++i)
        if (int iKeyCode = encodedKeyList[i].toInt())
            readableKeyList << UIHotKey::toString(iKeyCode);
    return readableKeyList.isEmpty() ? UIHotKeyEditor::tr("None") : readableKeyList.join(" + ");
}

QList<int> UIHotKeyCombination::toKeyCodeList(const QString &strKeyCombo)
{
    QStringList encodedKeyList = strKeyCombo.split(',');
    QList<int> keyCodeList;
    for (int i = 0; i < encodedKeyList.size(); ++i)
        if (int iKeyCode = encodedKeyList[i].toInt())
            keyCodeList << iKeyCode;
    return keyCodeList;
}

bool UIHotKeyCombination::isValidKeyCombo(const QString &strKeyCombo)
{
    QList<int> keyCodeList = toKeyCodeList(strKeyCombo);
    for (int i = 0; i < keyCodeList.size(); ++i)
        if (!UIHotKey::isValidKey(keyCodeList[i]))
            return false;
    return true;
}


UIHotKeyEditor::UIHotKeyEditor(QWidget *pParent)
    : QLabel(pParent)
    , m_fStartNewSequence(true)
{
    /* Configure widget: */
    setAttribute(Qt::WA_NativeWindow);
    setFrameStyle(QFrame::StyledPanel | Sunken);
    setAlignment(Qt::AlignCenter);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(true);

    /* Setup palette: */
    QPalette p = palette();
    p.setColor(QPalette::Active, QPalette::Foreground, p.color(QPalette::Active, QPalette::Text));
    p.setColor(QPalette::Active, QPalette::Background, p.color(QPalette::Active, QPalette::Base));
    setPalette(p);

#ifdef Q_WS_X11
    /* Initialize the X keyboard subsystem: */
    initMappedX11Keyboard(QX11Info::display(), vboxGlobal().settings().publicProperty("GUI/RemapScancodes"));
#endif /* Q_WS_X11 */

#ifdef Q_WS_MAC
    m_uDarwinKeyModifiers = GetCurrentEventKeyModifiers();
    UICocoaApplication::instance()->registerForNativeEvents(RT_BIT_32(10) | RT_BIT_32(11) | RT_BIT_32(12) /* NSKeyDown  | NSKeyUp | | NSFlagsChanged */, UIHotKeyEditor::darwinEventHandlerProc, this);
    ::DarwinGrabKeyboard(false /* just modifiers */);
#endif /* Q_WS_MAC */
}

UIHotKeyEditor::~UIHotKeyEditor()
{
#ifdef Q_WS_MAC
    ::DarwinReleaseKeyboard();
    UICocoaApplication::instance()->unregisterForNativeEvents(RT_BIT_32(10) | RT_BIT_32(11) | RT_BIT_32(12) /* NSKeyDown  | NSKeyUp | | NSFlagsChanged */, UIHotKeyEditor::darwinEventHandlerProc, this);
#endif /* Q_WS_MAC */
}

void UIHotKeyEditor::setCombo(const QString &strKeyCombo)
{
    /* Cleanup old combo: */
    m_shownKeys.clear();
    /* Parse newly passed combo: */
    QList<int> keyCodeList = UIHotKeyCombination::toKeyCodeList(strKeyCombo);
    for (int i = 0; i < keyCodeList.size(); ++i)
        if (int iKeyCode = keyCodeList[i])
            m_shownKeys.insert(iKeyCode, UIHotKey::toString(iKeyCode));
    /* Update text: */
    updateText();
}

QString UIHotKeyEditor::combo() const
{
    /* Compose current combination: */
    QStringList keyCodeStringList;
    QList<int> keyCodeList = m_shownKeys.keys();
    for (int i = 0; i < keyCodeList.size(); ++i)
        keyCodeStringList << QString::number(keyCodeList[i]);
    /* Return current combination or "0" for "None": */
    return keyCodeStringList.isEmpty() ? "0" : keyCodeStringList.join(",");
}

QSize UIHotKeyEditor::sizeHint() const
{
    ensurePolished();
    QFontMetrics fm(font());
    int h = qMax(fm.lineSpacing(), 14) + 2;
    int w = fm.width('x') * 17;
    int m = frameWidth() * 2;
    QStyleOption option;
    option.initFrom(this);
    return (style()->sizeFromContents(QStyle::CT_LineEdit, &option,
                                      QSize(w + m, h + m).expandedTo(QApplication::globalStrut()),
                                      this));
}

QSize UIHotKeyEditor::minimumSizeHint() const
{
    ensurePolished();
    QFontMetrics fm = fontMetrics();
    int h = fm.height() + qMax(2, fm.leading());
    int w = fm.maxWidth();
    int m = frameWidth() * 2;
    return QSize(w + m, h + m);
}

#ifdef Q_WS_WIN
bool UIHotKeyEditor::winEvent(MSG *pMsg, long* /* pResult */)
{
    switch (pMsg->message)
    {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            /* Get key-code: */
            int iKeyCode = UIHotKey::distinguishModifierVKey((int)pMsg->wParam);

            /* Check if symbol is valid else pass it to Qt: */
            if (!UIHotKey::isValidKey(iKeyCode))
                return false;

            /* Key press: */
            if (pMsg->message == WM_KEYDOWN || pMsg->message == WM_SYSKEYDOWN)
            {
                /* Clear reflected symbols if new sequence started: */
                if (m_fStartNewSequence)
                    m_shownKeys.clear();

                /* Remember pressed symbol: */
                m_pressedKeys << pMsg->wParam;
                m_shownKeys.insert(iKeyCode, UIHotKey::toString(iKeyCode));

                /* Remember what we already started a sequence: */
                m_fStartNewSequence = false;
            }
            /* Key release: */
            else if (pMsg->message == WM_KEYUP || pMsg->message == WM_SYSKEYUP)
            {
                /* Remove pressed symbol: */
                m_pressedKeys.remove(pMsg->wParam);

                /* If pressed keys map is empty => start new sequence: */
                if (m_pressedKeys.isEmpty())
                    m_fStartNewSequence = true;
            }

            /* Update text: */
            updateText();

            /* Prevent passing to Qt: */
            return true;
        }
        default:
            break;
    }

    return false;
}
#endif /* Q_WS_WIN */

#ifdef Q_WS_X11
bool UIHotKeyEditor::x11Event(XEvent *pEvent)
{
    switch (pEvent->type)
    {
        case XKeyPress:
        case XKeyRelease:
        {
            /* Get key-code: */
            XKeyEvent *pKeyEvent = (XKeyEvent*)pEvent;
            KeySym ks = ::XKeycodeToKeysym(pKeyEvent->display, pKeyEvent->keycode, 0);
            int iKeySym = (int)ks;

            /* Check if symbol is valid else pass it to Qt: */
            if (!UIHotKey::isValidKey(iKeySym))
                return false;

            /* Key press: */
            if (pEvent->type == XKeyPress)
            {
                /* Clear reflected symbols if new sequence started: */
                if (m_fStartNewSequence)
                    m_shownKeys.clear();

                /* Remember pressed symbol: */
                m_pressedKeys << iKeySym;
                m_shownKeys.insert(iKeySym, UIHotKey::toString(iKeySym));

                /* Remember what we already started a sequence: */
                m_fStartNewSequence = false;
            }
            /* Key release: */
            else if (pEvent->type == XKeyRelease)
            {
                /* Remove pressed symbol: */
                m_pressedKeys.remove(iKeySym);

                /* If pressed keys map is empty => start new sequence: */
                if (m_pressedKeys.isEmpty())
                    m_fStartNewSequence = true;
            }

            /* Update text: */
            updateText();

            /* Prevent passing to Qt: */
            return true;
        }
    }

    return false;
}
#endif /* Q_WS_X11 */

#ifdef Q_WS_MAC
/* static */
bool UIHotKeyEditor::darwinEventHandlerProc(const void *pvCocoaEvent, const void *pvCarbonEvent, void *pvUser)
{
    UIHotKeyEditor *edit = (UIHotKeyEditor*)pvUser;
    EventRef inEvent = (EventRef)pvCarbonEvent;
    UInt32 EventClass = ::GetEventClass(inEvent);
    if (EventClass == kEventClassKeyboard)
        return edit->darwinKeyboardEvent(pvCocoaEvent, inEvent);
    return false;
}

bool UIHotKeyEditor::darwinKeyboardEvent(const void *pvCocoaEvent, EventRef inEvent)
{
    /* Ignore key changes unless we're the focus widget: */
    if (!hasFocus())
        return false;

    UInt32 eventKind = ::GetEventKind(inEvent);
    switch (eventKind)
    {
        //case kEventRawKeyDown:
        //case kEventRawKeyUp:
        //case kEventRawKeyRepeat:
        case kEventRawKeyModifiersChanged:
        {
            /* Get modifier mask: */
            UInt32 modifierMask = 0;
            ::GetEventParameter(inEvent, kEventParamKeyModifiers, typeUInt32, NULL,
                                sizeof(modifierMask), NULL, &modifierMask);
            modifierMask = ::DarwinAdjustModifierMask(modifierMask, pvCocoaEvent);
            UInt32 changed = m_uDarwinKeyModifiers ^ modifierMask;
            m_uDarwinKeyModifiers = modifierMask;

            /* Convert to keycode: */
            int iKeyCode = ::DarwinModifierMaskToDarwinKeycode(changed);

            /* Check if symbol is valid else pass it to Qt: */
            if (!iKeyCode || iKeyCode == ~0U || !UIHotKey::isValidKey(iKeyCode))
                break;

            if (changed)
            {
                /* Key release: */
                if (changed & modifierMask)
                {
                    /* Remove pressed symbol: */
                    m_pressedKeys.remove(iKeyCode);

                    /* If pressed key map is empty => start new sequence: */
                    if (m_pressedKeys.isEmpty())
                        m_fStartNewSequence = true;
                }
                /* Key press: */
                else
                {
                    /* Clear reflected symbols if new sequence started: */
                    if (m_fStartNewSequence)
                        m_shownKeys.clear();

                    /* Remember pressed symbol: */
                    m_pressedKeys << iKeyCode;
                    m_shownKeys.insert(iKeyCode, UIHotKey::toString(iKeyCode));

                    /* Remember what we already started a sequence: */
                    m_fStartNewSequence = false;
                }
            }

            /* Update text: */
            updateText();
        }
        break;
    }
    return false;
}
#endif /* Q_WS_MAC */

void UIHotKeyEditor::keyPressEvent(QKeyEvent *pEvent)
{
    switch (pEvent->key())
    {
        case Qt::Key_Delete:
        case Qt::Key_Backspace:
            m_shownKeys.clear();
            updateText();
            break;
        default:
            QWidget::keyPressEvent(pEvent);
            break;
    }
}

void UIHotKeyEditor::focusInEvent(QFocusEvent *pEvent)
{
    QLabel::focusInEvent(pEvent);

    QPalette p = palette();
    p.setColor(QPalette::Active, QPalette::Foreground, p.color(QPalette::Active, QPalette::HighlightedText));
    p.setColor(QPalette::Active, QPalette::Background, p.color(QPalette::Active, QPalette::Highlight));
    setPalette(p);
}

void UIHotKeyEditor::focusOutEvent(QFocusEvent *pEvent)
{
    QLabel::focusOutEvent(pEvent);

    QPalette p = palette();
    p.setColor(QPalette::Active, QPalette::Foreground, p.color(QPalette::Active, QPalette::Text));
    p.setColor(QPalette::Active, QPalette::Background, p.color(QPalette::Active, QPalette::Base));
    setPalette(p);
}

void UIHotKeyEditor::paintEvent(QPaintEvent *pEvent)
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

void UIHotKeyEditor::updateText()
{
    QStringList shownKeyNames(m_shownKeys.values());
    setText(shownKeyNames.isEmpty() ? tr("None") : shownKeyNames.join(" + "));
}

