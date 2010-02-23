/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineView class declaration
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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

#ifndef ___UIMachineView_h___
#define ___UIMachineView_h___

/* Local forwards */
class UIFrameBuffer;
class UIMachineWindow;
class VBoxGlobalSettings;

/* Global includes */
#include <QAbstractScrollArea>

/* Local includes */
#include "COMDefs.h"
#include "UIMachineDefs.h"

class UIMachineView : public QAbstractScrollArea
{
    Q_OBJECT;

public:

    /* Factory function to create required view sub-child: */
    static UIMachineView* create(  UIMachineWindow *pMachineWindow
                                 , VBoxDefs::RenderMode renderMode
#ifdef VBOX_WITH_VIDEOHWACCEL
                                 , bool bAccelerate2DVideo
#endif
                                 , UIVisualStateType visualStateType);

    /* Adjust view geometry: */
    virtual void normalizeGeometry(bool bAdjustPosition = false) = 0;

    /* Public members: */
    // TODO: Is it needed now?
    void onViewOpened();

    /* Public getters: */
    int contentsX() const;
    int contentsY() const;
    QRect desktopGeometry() const;
    bool isMouseAbsolute() const { return m_bIsMouseAbsolute; }

    /* Public setters: */
    void setIgnoreGuestResize(bool bIgnore);
    void setMouseIntegrationEnabled(bool bEnabled);

signals:

    void keyboardStateChanged(int iState);
    void mouseStateChanged(int iState);
    void machineStateChanged(KMachineState state);
    void additionsStateChanged(const QString &strVersion, bool bIsActive, bool bIsGraphicSupported, bool bIsSeamlessSupported);
    void mediaDriveChanged(VBoxDefs::MediumType type);
    void networkStateChange();
    void usbStateChange();
    void sharedFoldersChanged();
    void resizeHintDone();

protected slots:

    virtual void doResizeHint(const QSize &aSize = QSize()) = 0;

protected:

    UIMachineView(  UIMachineWindow *pMachineWindow
                  , VBoxDefs::RenderMode renderMode
#ifdef VBOX_WITH_VIDEOHWACCEL
                  , bool bAccelerate2DVideo
#endif
    );
    virtual ~UIMachineView();

    /* Protected members: */
    void calculateDesktopGeometry();

    /* Protected getters: */
    UIMachineWindow* machineWindowWrapper() { return m_pMachineWindow; }
    QSize sizeHint() const;

    /* Protected variables: */
    VBoxDefs::RenderMode mode;
    bool m_bIsGuestSupportsGraphics : 1;

private slots:

#ifdef Q_WS_MAC
    /* Dock icon update handler */
    void sltChangeDockIconUpdate(const VBoxChangeDockIconUpdateEvent &event);

# ifdef QT_MAC_USE_COCOA
    /* Presentation mode handler */
    void sltChangePresentationMode(const VBoxChangePresentationModeEvent &event);
# endif
#endif

private:

    /* Private getters: */
    int contentsWidth() const;
    int contentsHeight() const;
    int visibleWidth() const;
    int visibleHeight() const;
    const QPixmap& pauseShot() const { return mPausedShot; }
    bool shouldHideHostPointer() const { return m_bIsMouseCaptured || (m_bIsMouseAbsolute && mHideHostPointer); }
    bool isRunning() { return mLastState == KMachineState_Running || mLastState == KMachineState_Teleporting || mLastState == KMachineState_LiveSnapshotting; }
    CConsole &console() { return m_console; }

    /* Event processors: */
    bool event(QEvent *pEvent);
    bool eventFilter(QObject *pWatched, QEvent *pEvent);
#if defined(Q_WS_WIN32)
    bool winLowKeyboardEvent(UINT msg, const KBDLLHOOKSTRUCT &event);
    bool winEvent (MSG *aMsg, long *aResult);
#elif defined(Q_WS_PM)
    bool pmEvent (QMSG *aMsg);
#elif defined(Q_WS_X11)
    bool x11Event (XEvent *event);
#elif defined(Q_WS_MAC)
    bool darwinKeyboardEvent (const void *pvCocoaEvent, EventRef inEvent);
    void darwinGrabKeyboardEvents (bool fGrab);
#endif
#if defined (Q_WS_WIN32)
    static LRESULT CALLBACK lowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
#elif defined (Q_WS_MAC)
# if defined (QT_MAC_USE_COCOA)
    static bool darwinEventHandlerProc(const void *pvCocoaEvent, const void *pvCarbonEvent, void *pvUser);
# elif !defined (VBOX_WITH_HACKED_QT)
    static pascal OSStatus darwinEventHandlerProc(EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData);
# else  /* VBOX_WITH_HACKED_QT */
    static bool macEventFilter(EventRef inEvent, void *inUserData);
# endif /* VBOX_WITH_HACKED_QT */
#endif

    /* Flags for keyEvent() */
    enum { KeyExtended = 0x01, KeyPressed = 0x02, KeyPause = 0x04, KeyPrint = 0x08 };
    void emitKeyboardStateChanged()
    {
        emit keyboardStateChanged((m_bIsKeyboardCaptured ? UIViewStateType_KeyboardCaptured : 0) |
                                  (m_bIsHostkeyPressed ? UIViewStateType_HostKeyPressed : 0));
    }
    void emitMouseStateChanged()
    {
        emit mouseStateChanged((m_bIsMouseCaptured ? UIMouseStateType_MouseCaptured : 0) |
                               (m_bIsMouseAbsolute ? UIMouseStateType_MouseAbsolute : 0) |
                               (!m_bIsMouseIntegrated ? UIMouseStateType_MouseAbsoluteDisabled : 0));
    }

    void focusEvent(bool aHasFocus, bool aReleaseHostKey = true);
    bool keyEvent(int aKey, uint8_t aScan, int aFlags, wchar_t *aUniKey = NULL);
    bool mouseEvent(int aType, const QPoint &aPos, const QPoint &aGlobalPos,
                    Qt::MouseButtons aButtons, Qt::KeyboardModifiers aModifiers,
                    int aWheelDelta, Qt::Orientation aWheelDir);
    void resizeEvent(QResizeEvent *pEvent);
    void moveEvent(QMoveEvent *pEvent);
    void paintEvent(QPaintEvent *pEvent);

    /* Private members: */
    void fixModifierState(LONG *piCodes, uint *puCount);
    void scrollBy(int dx, int dy);
    QPoint viewportToContents(const QPoint &vp) const;
    void updateSliders();
#ifdef VBOX_WITH_VIDEOHWACCEL
    void scrollContentsBy(int dx, int dy);
#endif
#if defined(Q_WS_MAC)
    void updateDockIcon();
    void updateDockOverlay();
    void setDockIconEnabled(bool aOn) { mDockIconEnabled = aOn; };
    void setMouseCoalescingEnabled(bool aOn);
#endif
    void onStateChange(KMachineState state);
    void captureKbd(bool aCapture, bool aEmitSignal = true);
    void captureMouse(bool aCapture, bool aEmitSignal = true);
    bool processHotKey(const QKeySequence &key, const QList<QAction*> &data);
    void releaseAllPressedKeys(bool aReleaseHostKey = true);
    void saveKeyStates();
    void sendChangedKeyStates();
    void updateMouseClipping();
    //void setPointerShape(MousePointerChangeEvent *pEvent);

    enum DesktopGeo { DesktopGeo_Invalid = 0, DesktopGeo_Fixed, DesktopGeo_Automatic, DesktopGeo_Any };
    void storeConsoleSize(int aWidth, int aHeight);
    void setMouseIntegrationLocked(bool bDisabled);
    void setDesktopGeometry(DesktopGeo aGeo, int aWidth, int aHeight);
    virtual void maybeRestrictMinimumSize() = 0;
    QRect availableGeometry();

    static void dimImage(QImage &img);

    /* Private members: */
    UIMachineWindow *m_pMachineWindow;
    CConsole m_console;
    const VBoxGlobalSettings &m_globalSettings;
    KMachineState mLastState;

    QPoint mLastPos;
    QPoint mCapturedPos;
    int m_iLastMouseWheelDelta;

    enum { IsKeyPressed = 0x01, IsExtKeyPressed = 0x02, IsKbdCaptured = 0x80 };
    uint8_t mPressedKeys[128];
    uint8_t mPressedKeysCopy[128];

    long muNumLockAdaptionCnt;
    long muCapsLockAdaptionCnt;

    bool m_bIsAutoCaptureDisabled : 1;
    bool m_bIsKeyboardCaptured : 1;
    bool m_bIsMouseCaptured : 1;
    bool m_bIsMouseAbsolute : 1;
    bool m_bIsMouseIntegrated : 1;
    bool m_bIsHostkeyPressed : 1;
    bool m_bIsHostkeyAlone : 1;
    bool hostkey_in_capture : 1;
    bool m_bIsMachineWindowResizeIgnored : 1;
    bool m_bIsFrameBufferResizeIgnored : 1;
    bool m_bIsGuestResizeIgnored : 1;
    bool mDoResize : 1;
    bool mNumLock : 1;
    bool mScrollLock : 1;
    bool mCapsLock : 1;

#ifdef VBOX_WITH_VIDEOHWACCEL
    bool mAccelerate2DVideo;
#endif

#if defined(Q_WS_WIN)
    HCURSOR mAlphaCursor;
#endif

#if defined(Q_WS_MAC)
# if !defined (VBOX_WITH_HACKED_QT) && !defined (QT_MAC_USE_COCOA)
    /** Event handler reference. NULL if the handler isn't installed. */
    EventHandlerRef mDarwinEventHandlerRef;
# endif
    /** The current modifier key mask. Used to figure out which modifier
     *  key was pressed when we get a kEventRawKeyModifiersChanged event. */
    UInt32 mDarwinKeyModifiers;
    bool mKeyboardGrabbed;
#endif

    UIFrameBuffer *mFrameBuf;

    QPixmap mPausedShot;
#if defined(Q_WS_MAC)
# if !defined (QT_MAC_USE_COCOA)
    EventHandlerRef mDarwinWindowOverlayHandlerRef;
# endif
    VBoxDockIconPreview *mDockIconPreview;
    bool mDockIconEnabled;
#endif
    DesktopGeo mDesktopGeo;
    QRect mDesktopGeometry;
    QRect mStoredConsoleSize;
    bool mPassCAD;
    bool mHideHostPointer;
    QCursor mLastCursor;
};

#endif // !___UIMachineViewNormal_h___
