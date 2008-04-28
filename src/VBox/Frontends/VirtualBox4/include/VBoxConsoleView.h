/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxConsoleView class declaration
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

#ifndef __VBoxConsoleView_h__
#define __VBoxConsoleView_h__

#include "COMDefs.h"

#include "VBoxDefs.h"
#include "VBoxGlobalSettings.h"

/* Qt includes */
#include <QAbstractScrollArea>
#include <QScrollBar>

#if defined (Q_WS_PM)
#include "src/os2/VBoxHlp.h"
#define UM_PREACCEL_CHAR WM_USER
#endif

#if defined (Q_WS_MAC)
# include <Carbon/Carbon.h>
# include "DarwinCursor.h"
#endif

class VBoxConsoleWnd;
class MousePointerChangeEvent;
class VBoxFrameBuffer;

class QPainter;
class QLabel;
class QMenuData;

class VBoxConsoleView : public QAbstractScrollArea
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
                     QWidget *parent = 0);
    ~VBoxConsoleView();

    QSize sizeHint() const;

    void attach();
    void detach();
    void refresh() { doRefresh(); }
    void normalizeGeometry (bool adjustPosition = false);

    CConsole &console() { return mConsole; }

    bool pause (bool on);

    void setMouseIntegrationEnabled (bool enabled);

    bool isMouseAbsolute() const { return mMouseAbsolute; }

    void setAutoresizeGuest (bool on);

    void onFullscreenChange (bool on);

    void onViewOpened();

    void fixModifierState (LONG *codes, uint *count);

    void toggleFSMode();

    void setIgnoreMainwndResize (bool aYes) { mIgnoreMainwndResize = aYes; }

    QRect getDesktopGeometry();

    /* todo: This are some support functions for the qt4 port. Maybe we get rid
     * of them some day. */
    int contentsX() const { return horizontalScrollBar()->value(); }
    int contentsY() const { return verticalScrollBar()->value(); }
    int contentsWidth() const;
    int contentsHeight() const;
    int visibleWidth() const { return horizontalScrollBar()->pageStep(); }
    int visibleHeight() const { return verticalScrollBar()->pageStep(); }
    void scrollBy (int dx, int dy)
    {
        horizontalScrollBar()->setValue (horizontalScrollBar()->value() + dx);
        verticalScrollBar()->setValue (verticalScrollBar()->value() + dy);
    }
    QPoint viewportToContents ( const QPoint & vp ) const
    {
        return QPoint (vp.x() + contentsX(),
                       vp.y() + contentsY());
    }
    void updateSliders();

signals:

    void keyboardStateChanged (int state);
    void mouseStateChanged (int state);
    void machineStateChanged (KMachineState state);
    void additionsStateChanged (const QString &, bool, bool, bool);
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

    void focusEvent (bool aHasFocus, bool aReleaseHostKey = true);
    bool keyEvent (int aKey, uint8_t aScan, int aFlags,
                   wchar_t *aUniKey = NULL);
    bool mouseEvent (int aType, const QPoint &aPos, const QPoint &aGlobalPos,
                     Qt::ButtonState aButton,
                     Qt::MouseButtons aButtons, Qt::KeyboardModifiers aModifiers,
                     int aWheelDelta, Qt::Orientation aWheelDir);

    void emitKeyboardStateChanged()
    {
        emit keyboardStateChanged (
            (mKbdCaptured ? KeyboardCaptured : 0) |
            (mIsHostkeyPressed ? HostKeyPressed : 0));
    }

    void emitMouseStateChanged() {
        emit mouseStateChanged ((mMouseCaptured ? MouseCaptured : 0) |
                                (mMouseAbsolute ? MouseAbsolute : 0) |
                                (!mMouseIntegration ? MouseAbsoluteDisabled : 0));
    }

    // IConsoleCallback event handlers
    void onStateChange (KMachineState state);

    void doRefresh();

    void resizeEvent (QResizeEvent *);
    void paintEvent (QPaintEvent *);

    void captureKbd (bool aCapture, bool aEmitSignal = true);
    void captureMouse (bool aCapture, bool aEmitSignal = true);

    bool processHotKey (const QKeySequence &key, const QList<QAction*>& data);
    void updateModifiers (bool fNumLock, bool fCapsLock, bool fScrollLock);

    void releaseAllPressedKeys (bool aReleaseHostKey = true);
    void saveKeyStates();
    void sendChangedKeyStates();
    void updateMouseClipping();

    void setPointerShape (MousePointerChangeEvent *me);

    bool isPaused() { return mLastState == KMachineState_Paused; }
    bool isRunning() { return mLastState == KMachineState_Running; }

    static void dimImage (QImage &img);

private slots:

    void doResizeHint (const QSize &aSize = QSize());
    void doResizeDesktop (int);

private:

    void setDesktopGeometry(int minWidth, int minHeight);
    void maybeRestrictMinimumSize();

    VBoxConsoleWnd *mMainWnd;

    CConsole mConsole;

    const VBoxGlobalSettings &gs;

    KMachineState mLastState;

    bool mAttached : 1;
    bool mKbdCaptured : 1;
    bool mMouseCaptured : 1;
    bool mMouseAbsolute : 1;
    bool mMouseIntegration : 1;
    QPoint mLastPos;
    QPoint mCapturedPos;

	bool mDisableAutoCapture : 1;

    enum { IsKeyPressed = 0x01, IsExtKeyPressed = 0x02, IsKbdCaptured = 0x80 };
    uint8_t mPressedKeys [128];
    uint8_t mPressedKeysCopy [128];

    bool mIsHostkeyPressed : 1;
    bool mIsHostkeyAlone : 1;

    /** mKbdCaptured value during the the last host key press or release */
    bool hostkey_in_capture : 1;

    bool mIgnoreMainwndResize : 1;
    bool mAutoresizeGuest : 1;

    bool mIsAdditionsActive : 1;

    bool mNumLock : 1;
    bool mScrollLock : 1;
    bool mCapsLock : 1;
    long muNumLockAdaptionCnt;
    long muCapsLockAdaptionCnt;

    VBoxDefs::RenderMode mode;

    QRegion mLastVisibleRegion;
    QSize mNormalSize;

#if defined(Q_WS_WIN)
    HCURSOR mAlphaCursor;
#endif

#if defined(Q_WS_MAC)
# ifndef VBOX_WITH_HACKED_QT
    /** Event handler reference. NULL if the handler isn't installed. */
    EventHandlerRef mDarwinEventHandlerRef;
# endif
    /** The current modifier key mask. Used to figure out which modifier
     *  key was pressed when we get a kEventRawKeyModifiersChanged event. */
    UInt32 mDarwinKeyModifiers;
    /** The darwin cursor handle (see DarwinCursor.h/.cpp). */
    DARWINCURSOR mDarwinCursor;
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
#if defined(Q_WS_MAC)
    CGImageRef mVirtualBoxLogo;
#endif
    QRect mDesktopGeometry;
};

#endif // __VBoxConsoleView_h__

