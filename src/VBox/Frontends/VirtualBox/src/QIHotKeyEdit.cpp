/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * InnoTek Qt extensions: QIHotKeyEdit class implementation
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#include "QIHotKeyEdit.h"

#include "VBoxDefs.h"

#include <qapplication.h>
#include <qstyle.h>
#include <qlineedit.h>

#ifdef Q_WS_WIN
// VBox/cdefs.h defines these:
#undef LOWORD
#undef HIWORD
#undef LOBYTE
#undef HIBYTE
#include <windows.h>
#endif

#ifdef Q_WS_X11
// We need to capture some X11 events directly which
// requires the XEvent structure to be defined. However,
// including the Xlib header file will cause some nasty
// conflicts with Qt. Therefore we use the following hack
// to redefine those conflicting identifiers.
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
QMap<QString, QString> QIHotKeyEdit::keyNames;
#endif

#ifdef Q_WS_MAC
#include "DarwinKeyboard.h"
#endif


#ifdef Q_WS_WIN32
/**
 *  Returns the correct modifier vkey for the *last* keyboard message,
 *  distinguishing between left and right keys. If both are pressed
 *  the left key wins. If the pressed key not a modifier, wParam is returned
 *  unchanged.
 */
int qi_distinguish_modifier_vkey( WPARAM wParam )
{
    int keyval = wParam;
    switch ( wParam ) {
        case VK_SHIFT:
            if ( ::GetKeyState( VK_LSHIFT ) & 0x8000 ) keyval = VK_LSHIFT;
            else if ( ::GetKeyState( VK_RSHIFT ) & 0x8000 ) keyval = VK_RSHIFT;
            break;
        case VK_CONTROL:
            if ( ::GetKeyState( VK_LCONTROL ) & 0x8000 ) keyval = VK_LCONTROL;
            else if ( ::GetKeyState( VK_RCONTROL ) & 0x8000 ) keyval = VK_RCONTROL;
            break;
        case VK_MENU:
            if ( ::GetKeyState( VK_LMENU ) & 0x8000 ) keyval = VK_LMENU;
            else if ( ::GetKeyState( VK_RMENU ) & 0x8000 ) keyval = VK_RMENU;
            break;
    }
    return keyval;
}
#endif

/** @class QIHotKeyEdit
 *
 *  The QIHotKeyEdit widget is a hot key editor.
 */

const char *QIHotKeyEdit::NoneSymbName = "<none>";

QIHotKeyEdit::QIHotKeyEdit( QWidget * parent, const char * name ) :
    QLabel( parent, name )
{
#ifdef Q_WS_X11
    // initialize the X keyboard subsystem
    initXKeyboard( this->x11Display() );
#endif

    clear();

    setFrameStyle( LineEditPanel | Sunken );
    setAlignment( AlignHCenter | AlignBottom );
    setFocusPolicy( StrongFocus );

    QPalette p = palette();
    p.setColor( QPalette::Active, QColorGroup::Foreground,
        p.color( QPalette::Active, QColorGroup::HighlightedText )
    );
    p.setColor( QPalette::Active, QColorGroup::Background,
        p.color( QPalette::Active, QColorGroup::Highlight )
    );
    p.setColor( QPalette::Inactive, QColorGroup::Foreground,
        p.color( QPalette::Active, QColorGroup::Text )
    );
    p.setColor( QPalette::Inactive, QColorGroup::Background,
        p.color( QPalette::Active, QColorGroup::Base )
    );

    true_acg = p.active();
    p.setActive( p.inactive() );
    setPalette( p );

#ifdef Q_WS_MAC
    m_darwinKeyModifiers = GetCurrentEventKeyModifiers();

    EventTypeSpec eventTypes[4];
    eventTypes[0].eventClass = kEventClassKeyboard;
    eventTypes[0].eventKind  = kEventRawKeyDown;
    eventTypes[1].eventClass = kEventClassKeyboard;
    eventTypes[1].eventKind  = kEventRawKeyUp;
    eventTypes[2].eventClass = kEventClassKeyboard;
    eventTypes[2].eventKind  = kEventRawKeyRepeat;
    eventTypes[3].eventClass = kEventClassKeyboard;
    eventTypes[3].eventKind  = kEventRawKeyModifiersChanged;

    EventHandlerUPP eventHandler = ::NewEventHandlerUPP( QIHotKeyEdit::darwinEventHandlerProc );

    m_darwinEventHandlerRef = NULL;
    ::InstallApplicationEventHandler( eventHandler, RT_ELEMENTS( eventTypes ), &eventTypes[0],
                                      this, &m_darwinEventHandlerRef );
    ::DisposeEventHandlerUPP( eventHandler );
    ::DarwinGrabKeyboard( false /* just modifiers */ );

#endif

}

QIHotKeyEdit::~QIHotKeyEdit()
{
#ifdef Q_WS_MAC
    ::DarwinReleaseKeyboard();
    ::RemoveEventHandler( m_darwinEventHandlerRef );
    m_darwinEventHandlerRef = NULL;
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
void QIHotKeyEdit::setKey (int k)
{
    keyval = k;
    symbname = QIHotKeyEdit::keyName (k);
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
    constPolish();
    QFontMetrics fm (font());
    int h = QMAX(fm.lineSpacing(), 14) + 2;
    int w = fm.width( 'x' ) * 17; // "some"
    int m = frameWidth() * 2;
    return (style().sizeFromContents(QStyle::CT_LineEdit, this,
                                     QSize (w + m, h + m)
                                     .expandedTo(QApplication::globalStrut())));
}

/**
 *  Stolen from QLineEdit.
 */
QSize QIHotKeyEdit::minimumSizeHint() const
{
    constPolish();
    QFontMetrics fm = fontMetrics();
    int h = fm.height() + QMAX (2, fm.leading());
    int w = fm.maxWidth();
    int m = frameWidth() * 2;
    return QSize (w + m, h + m);
}

#if defined(Q_WS_X11)
/**
 * Updates the associative array containing the translations of X11 key strings to human
 * readable key names.
 */
// static
void QIHotKeyEdit::languageChange(void)
{
    keyNames ["Shift_L"]          = tr ("Left Shift");
    keyNames ["Shift_R"]          = tr ("Right Shift");
    keyNames ["Control_L"]        = tr ("Left Ctrl");
    keyNames ["Control_R"]        = tr ("Right Ctrl");
    keyNames ["Alt_L"]            = tr ("Left Alt");
    keyNames ["Alt_R"]            = tr ("Right Alt");
    keyNames ["Super_L"]          = tr ("Left WinKey");
    keyNames ["Super_R"]          = tr ("Right WinKey");
    keyNames ["Menu"]             = tr ("Menu key");
    keyNames ["ISO_Level3_Shift"] = tr ("Alt Gr");
    keyNames ["Caps_Lock"]        = tr ("Caps Lock");
    keyNames ["Scroll_Lock"]      = tr ("Scroll Lock");
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
// static
QString QIHotKeyEdit::keyName (int key)
{
    QString name;

    if ( !key )
    {
        name = tr (NoneSymbName);
    }
    else
    {
#if defined (Q_WS_WIN32)
        /* stupid MapVirtualKey doesn't distinguish between right and left
         * vkeys, even under XP, despite that it stated in msdn. do it by
         * hand. */
        int scan;
        switch (key)
        {
            case VK_RSHIFT: scan = 0x36 << 16; break;
            case VK_RCONTROL: scan = (0x1D << 16) | (1 << 24); break;
            case VK_RMENU: scan = (0x38 << 16) | (1 << 24); break;
            default: scan = ::MapVirtualKey( key, 0 ) << 16;
        }
        TCHAR *str = new TCHAR[256];
        if (::GetKeyNameText (scan, str, 256))
            name = QString::fromUcs2 (str);
        else
            name = QString (tr ("<key_%1>")).arg (key);
        delete[] str;
#elif defined (Q_WS_X11)
        char *sn = ::XKeysymToString( (KeySym) key );
        if (sn)
        {
            name = keyNames [sn];
            if (name.isEmpty())
                name = sn;
        }
        else
            name = QString (tr ("<key_%1>")).arg (key);
#elif defined(Q_WS_MAC)
        UInt32 modMask = DarwinKeyCodeToDarwinModifierMask (key);
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
        name = QString ("<key_%1>").arg (key);
#endif
    }

    return name;
}

// static
bool QIHotKeyEdit::isValidKey( int k )
{
#if defined(Q_WS_WIN32)
    return (
        (k >= VK_SHIFT && k <= VK_CAPITAL) ||
        k == VK_PRINT ||
        k == VK_LWIN || k == VK_RWIN ||
        k == VK_APPS ||
        (k >= VK_F1 && k <= VK_F24) ||
        k == VK_NUMLOCK || k == VK_SCROLL ||
        (k >= VK_LSHIFT && k <= VK_RMENU)
    );
#elif defined(Q_WS_X11)
    KeySym ks = (KeySym) k;
    return
        (
            ks != NoSymbol &&
            ks != XK_Insert
        ) && (
            ks == XK_Scroll_Lock ||
            IsModifierKey( ks ) ||
            IsFunctionKey( ks ) ||
            IsMiscFunctionKey( ks )
        );
#elif defined(Q_WS_MAC)
    UInt32 modMask = ::DarwinKeyCodeToDarwinModifierMask( k );
    switch ( modMask ) {
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
    Q_UNUSED( k );
    return true;
#endif
}

// Public slots
/////////////////////////////////////////////////////////////////////////////

void QIHotKeyEdit::clear()
{
    keyval = 0;
    symbname = tr (NoneSymbName);
    updateText();
}

// Protected members
/////////////////////////////////////////////////////////////////////////////

// Protected events
/////////////////////////////////////////////////////////////////////////////

#if defined(Q_WS_WIN32)

bool QIHotKeyEdit::winEvent( MSG *msg )
{
    if ( !(
        msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN ||
        msg->message == WM_KEYUP || msg->message == WM_SYSKEYUP ||
        msg->message == WM_CHAR || msg->message == WM_SYSCHAR ||
        msg->message == WM_DEADCHAR || msg->message == WM_SYSDEADCHAR ||
        msg->message == WM_CONTEXTMENU
    ) )
        return false;

    // ignore if not valid hot key
    if ( !isValidKey( msg->wParam ) )
        return false;

//    V_DEBUG((
//        "%WM_%04X: vk=%04X rep=%05d scan=%02X ext=%01d rzv=%01X ctx=%01d prev=%01d tran=%01d",
//        msg->message, msg->wParam,
//        (msg->lParam & 0xFFFF),
//        ((msg->lParam >> 16) & 0xFF),
//        ((msg->lParam >> 24) & 0x1),
//        ((msg->lParam >> 25) & 0xF),
//        ((msg->lParam >> 29) & 0x1),
//        ((msg->lParam >> 30) & 0x1),
//        ((msg->lParam >> 31) & 0x1)
//    ));

    if ( msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN) {
        // determine platform-dependent key
        keyval = qi_distinguish_modifier_vkey( msg->wParam );
        // determine symbolic name
        TCHAR *str = new TCHAR[256];
        if ( ::GetKeyNameText( msg->lParam, str, 256 ) )
            symbname = QString::fromUcs2( str );
        else
            symbname = QString( "<key_%1>" ).arg( keyval );
        delete[] str;
        // update the display
        updateText();
    }

    return true;
}

#elif defined(Q_WS_X11)

bool QIHotKeyEdit::x11Event( XEvent *event )
{
    switch ( event->type ) {
        case XKeyPress:
        case XKeyRelease: {
            XKeyEvent *ke = (XKeyEvent *) event;
            KeySym ks = ::XKeycodeToKeysym( ke->display, ke->keycode, 0 );
            // ignore if not valid hot key
            if ( !isValidKey( (int) ks ) )
                return false;

            // skip key releases
            if ( event->type == XKeyRelease )
                return true;

            // determine platform-dependent key
            keyval = (int) ks;
            // determine symbolic name
            char *name = ::XKeysymToString( ks );
            if ( name )
            {
                if ( keyNames.contains(name) )
                    symbname = keyNames[name];
                else
                    symbname = name;
            }
            else
                symbname = QString( tr ("<key_%1>") ).arg( (int) ks );
            // update the display
            updateText();
//V_DEBUG((
//    "%s: state=%08X keycode=%08X keysym=%08X symb=%s",
//    event->type == XKeyPress ? "XKeyPress" : "XKeyRelease",
//    ke->state, ke->keycode, ks,
//    symbname.latin1()
//));
            return true;
        }
    }

    return false;
}

#elif defined(Q_WS_MAC)

/* static */
pascal OSStatus QIHotKeyEdit::darwinEventHandlerProc( EventHandlerCallRef inHandlerCallRef,
                                                      EventRef inEvent, void *inUserData )
{
    QIHotKeyEdit *edit = (QIHotKeyEdit *)inUserData;
    UInt32 EventClass = ::GetEventClass( inEvent );
    if (EventClass == kEventClassKeyboard)
    {
        if (edit->darwinKeyboardEvent( inEvent ))
            return 0;
    }
    return CallNextEventHandler (inHandlerCallRef, inEvent);
}

bool QIHotKeyEdit::darwinKeyboardEvent( EventRef inEvent )
{
    /* ignore key changes unless we're the focus widget */
    if (!hasFocus())
        return false;

    UInt32 eventKind = ::GetEventKind( inEvent );
    switch ( eventKind ) {
        /*case kEventRawKeyDown:
        case kEventRawKeyUp:
        case kEventRawKeyRepeat:*/
        case kEventRawKeyModifiersChanged: {
            UInt32 modifierMask = 0;
            ::GetEventParameter( inEvent, kEventParamKeyModifiers, typeUInt32, NULL,
                                 sizeof( modifierMask ), NULL, &modifierMask );

            modifierMask = ::DarwinAdjustModifierMask(modifierMask);
            UInt32 changed = m_darwinKeyModifiers ^ modifierMask;
            m_darwinKeyModifiers = modifierMask;

            // skip key releases
            if ( changed && ( changed & modifierMask ) )
                break;

            // convert to keycode and skip keycodes we don't care about.
            unsigned keyCode = ::DarwinModifierMaskToDarwinKeycode( changed );
            if ( !keyCode || keyCode == ~0U || !isValidKey( keyCode ) )
                break;

            // update key current key.
            keyval = keyCode;
            symbname = QIHotKeyEdit::keyName( keyCode );
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

void QIHotKeyEdit::focusInEvent( QFocusEvent * ) {
    QPalette p = palette();
    p.setActive( true_acg );
    setPalette( p );
}

void QIHotKeyEdit::focusOutEvent( QFocusEvent * ) {
    QPalette p = palette();
    p.setActive( p.inactive() );
    setPalette( p );
}

void QIHotKeyEdit::drawContents( QPainter * p )
{
    QLabel::drawContents( p );
    if ( hasFocus() ) {
        style().drawPrimitive(
            QStyle::PE_FocusRect, p, contentsRect(), colorGroup(),
            QStyle::Style_Default,
            QStyleOption( colorGroup().background() )
        );
    }
}

// Private members
/////////////////////////////////////////////////////////////////////////////

void QIHotKeyEdit::updateText()
{
    setText( QString( " %1 " ).arg( symbname ) );
}

