/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxConsoleView class declaration
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxConsoleView_h__
#define __VBoxConsoleView_h__

#include "COMDefs.h"

#include "VBoxDefs.h"
#include "VBoxGlobalSettings.h"

#include <qdatetime.h>
#include <qscrollview.h>
#include <qpixmap.h>
#include <qimage.h>

#include <qkeysequence.h>

#if defined (Q_WS_MAC)
# include <Carbon/Carbon.h>
# include "DarwinCursor.h"
/** @todo remove this hack when somebody get around fixing the conflicting typedef/enum OSType. */
# define OSType VBoxOSType
#endif

class VBoxConsoleWnd;
class MousePointerChangeEvent;
class VBoxFrameBuffer;

class QPainter;
class QLabel;
class QMenuData;

class VBoxConsoleView : public QScrollView
{
    Q_OBJECT

public:

    enum {
        MouseCaptured = 0x01,
        MouseAbsolute = 0x02,
        MouseAbsoluteDisabled = 0x04,
        MouseNeedsHostCursor = 0x08,
        KeyboardCaptured = 0x01,
        HostKeyPressed = 0x02,
    };

    VBoxConsoleView (VBoxConsoleWnd *mainWnd,
                     const CConsole &console,
                     VBoxDefs::RenderMode rm,
                     QWidget *parent = 0, const char *name = 0, WFlags f = 0);
    ~VBoxConsoleView();

    QSize sizeHint() const;

    void attach();
    void detach();
    void refresh() { doRefresh(); }
    void normalizeGeometry (bool adjustPosition = false);

    CConsole &console() { return cconsole; }

    bool pause (bool on);

    void setMouseIntegrationEnabled (bool enabled);

    bool isMouseAbsolute() const { return mouse_absolute; }

    void setAutoresizeGuest (bool on);

    void onFullscreenChange (bool on);

    void onViewOpened();

    void fixModifierState (LONG *codes, uint *count);

    void toggleFSMode (const QSize &aFeatSize);

    void setIgnoreMainwndResize (bool aYes) { mIgnoreMainwndResize = aYes; }

signals:

    void keyboardStateChanged (int state);
    void mouseStateChanged (int state);
    void machineStateChanged (CEnums::MachineState state);
    void additionsStateChanged (const QString &, bool, bool);
    void mediaChanged (VBoxDefs::DiskType aType);
    void networkStateChange();
    void usbStateChange();
    void sharedFoldersChanged();
    void resizeHintDone();

protected:

    // events
    bool event (QEvent *e);
    bool eventFilter (QObject *watched, QEvent *e);

#if defined(Q_WS_WIN32)
    bool winLowKeyboardEvent (UINT msg, const KBDLLHOOKSTRUCT &event);
    bool winEvent (MSG *msg);
#elif defined(Q_WS_PM)
    bool pmEvent (QMSG *aMsg);
#elif defined(Q_WS_X11)
    bool x11Event (XEvent *event);
#elif defined(Q_WS_MAC)
    bool darwinKeyboardEvent (EventRef inEvent);
    void darwinGrabKeyboardEvents (bool fGrab);
#endif

private:

    /** Flags for keyEvent(). */
    enum {
        KeyExtended = 0x01,
        KeyPressed = 0x02,
        KeyPause = 0x04,
        KeyPrint = 0x08,
    };

    void focusEvent (bool focus);
    bool keyEvent (int aKey, uint8_t aScan, int aFlags,
                   wchar_t *aUniKey = NULL);
    bool mouseEvent (int aType, const QPoint &aPos, const QPoint &aGlobalPos,
                     ButtonState aButton,
                     ButtonState aState, ButtonState aStateAfter,
                     int aWheelDelta, Orientation aWheelDir);

    void emitKeyboardStateChanged() {
        emit keyboardStateChanged (
            (kbd_captured ? KeyboardCaptured : 0) |
            (hostkey_pressed ? HostKeyPressed : 0));
    }
    void emitMouseStateChanged() {
        emit mouseStateChanged ((mouse_captured ? MouseCaptured : 0) |
                                (mouse_absolute ? MouseAbsolute : 0) |
                                (!mouse_integration ? MouseAbsoluteDisabled : 0));
    }

    // IConsoleCallback event handlers
    void onStateChange (CEnums::MachineState state);

    void doRefresh();

    void viewportPaintEvent( QPaintEvent * );
#ifdef VBOX_GUI_USE_REFRESH_TIMER
    void timerEvent( QTimerEvent * );
#endif

    void captureKbd (bool capture, bool emit_signal = true);
    void captureMouse (bool capture, bool emit_signal = true);

    bool processHotKey (const QKeySequence &key, QMenuData *data);
    void updateModifiers (bool fNumLock, bool fCapsLock, bool fScrollLock);

    void releaseAllKeysPressed (bool release_hostkey = true);
    void saveKeyStates();
    void sendChangedKeyStates();
    void updateMouseClipping();

    void setPointerShape (MousePointerChangeEvent *me);

    bool isPaused() { return last_state == CEnums::Paused; }
    bool isRunning() { return last_state == CEnums::Running; }

    static void dimImage (QImage &img);

private slots:

    void doResizeHint (const QSize &aSize = QSize());

private:

    void maybeRestrictMinimumSize();

    VBoxConsoleWnd *mainwnd;

    CConsole cconsole;

    const VBoxGlobalSettings &gs;

    CEnums::MachineState last_state;

    bool attached : 1;
    bool kbd_captured : 1;
    bool mouse_captured : 1;
    bool mouse_absolute : 1;
    bool mouse_integration : 1;
    QPoint last_pos;
    QPoint captured_pos;

    enum { IsKeyPressed = 0x01, IsExtKeyPressed = 0x02, IsKbdCaptured = 0x80 };
    uint8_t keys_pressed[128];
    uint8_t keys_pressed_copy[128];

    bool hostkey_pressed : 1;
    bool hostkey_alone : 1;
    /** kbd_captured value during the the last host key press or release */
    bool hostkey_in_capture : 1;

    bool mIgnoreMainwndResize : 1;
    bool mAutoresizeGuest : 1;

    bool mIsAdditionsActive : 1;

    bool mfNumLock : 1;
    bool mfScrollLock : 1;
    bool mfCapsLock : 1;
    long muNumLockAdaptionCnt;
    long muCapsLockAdaptionCnt;

    QTimer *resize_hint_timer;
    QTimer *mToggleFSModeTimer;

    VBoxDefs::RenderMode mode;

    QRegion mLastVisibleRegion;

#if defined(Q_WS_WIN)
    HCURSOR mAlphaCursor;
#endif

#if defined(Q_WS_MAC)
# ifndef VBOX_WITH_HACKED_QT
    /** Event handler reference. NULL if the handler isn't installed. */
    EventHandlerRef m_darwinEventHandlerRef;
# endif
    /** The current modifier key mask. Used to figure out which modifier
     *  key was pressed when we get a kEventRawKeyModifiersChanged event. */
    UInt32 m_darwinKeyModifiers;
    /** The darwin cursor handle (see DarwinCursor.h/.cpp). */
    DARWINCURSOR m_darwinCursor;
#endif

#if defined (VBOX_GUI_USE_REFRESH_TIMER)
    QPixmap pm;
    int tid;        /**< Timer id */
#endif

    VBoxFrameBuffer *mFrameBuf;
    CConsoleCallback mCallback;

    friend class VBoxConsoleCallback;

#if defined (Q_WS_WIN32)
    static LRESULT CALLBACK lowLevelKeyboardProc (int nCode,
                                                  WPARAM wParam, LPARAM lParam);
#elif defined (Q_WS_MAC)
# ifndef VBOX_WITH_HACKED_QT
    static pascal OSStatus darwinEventHandlerProc (EventHandlerCallRef inHandlerCallRef,
                                                   EventRef inEvent, void *inUserData);
# else  /* VBOX_WITH_HACKED_QT */
    static bool macEventFilter (EventRef inEvent, void *inUserData);
# endif /* VBOX_WITH_HACKED_QT */
#endif

    QPixmap mPausedShot;
};

#endif // __VBoxConsoleView_h__

