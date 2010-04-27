/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineView class declaration
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

#ifndef ___UIMachineView_h___
#define ___UIMachineView_h___

/* Global includes */
#include <QAbstractScrollArea>
#include <QEventLoop>

/* Local includes */
#include "COMDefs.h"
#include "UIMachineDefs.h"

#ifdef Q_WS_MAC
# include <CoreFoundation/CFBase.h>
#endif /* Q_WS_MAC */

/* Local forwards */
class UISession;
class UIFrameBuffer;
class UIMachineWindow;
class UIMachineLogic;
class VBoxGlobalSettings;

class UIMachineView : public QAbstractScrollArea
{
    Q_OBJECT;

public:

    /* Desktop geometry types: */
    enum DesktopGeo { DesktopGeo_Invalid = 0, DesktopGeo_Fixed, DesktopGeo_Automatic, DesktopGeo_Any };

    /* Factory function to create required view sub-child: */
    static UIMachineView* create(  UIMachineWindow *pMachineWindow
                                 , VBoxDefs::RenderMode renderMode
#ifdef VBOX_WITH_VIDEOHWACCEL
                                 , bool bAccelerate2DVideo
#endif
                                 , UIVisualStateType visualStateType
                                 , ulong uScreenId);
    static void destroy(UIMachineView *pWhichView);

    /* Public getters: */
    int keyboardState() const;
    int mouseState() const;
    virtual QRegion lastVisibleRegion() const { return QRegion(); }

    /* Public setters: */
    virtual void setGuestAutoresizeEnabled(bool /* fEnabled */) {}
    virtual void setMouseIntegrationEnabled(bool fEnabled);

    /* Public members: */
    virtual void normalizeGeometry(bool /* bAdjustPosition = false */) = 0;

signals:

    /* Mouse/Keyboard state-change signals: */
    void keyboardStateChanged(int iState);
    void mouseStateChanged(int iState);

    /* Utility signals: */
    void resizeHintDone();

protected:

    /* Machine view constructor/destructor: */
    UIMachineView(  UIMachineWindow *pMachineWindow
                  , VBoxDefs::RenderMode renderMode
#ifdef VBOX_WITH_VIDEOHWACCEL
                  , bool bAccelerate2DVideo
#endif
                  , ulong uScreenId);
    virtual ~UIMachineView();

    /* Protected getters: */
    UISession* uisession() const;
    CSession& session();
    QSize sizeHint() const;
    int contentsX() const;
    int contentsY() const;
    int contentsWidth() const;
    int contentsHeight() const;
    int visibleWidth() const;
    int visibleHeight() const;
    VBoxDefs::RenderMode mode() const { return m_mode; }
    ulong screenId() const { return m_uScreenId; }
    UIFrameBuffer* frameBuffer() const { return m_pFrameBuffer; }
    UIMachineWindow* machineWindowWrapper() const { return m_pMachineWindow; }
    UIMachineLogic* machineLogic() const;
    bool isHostKeyPressed() const { return m_bIsHostkeyPressed; }
    bool isMachineWindowResizeIgnored() const { return m_bIsMachineWindowResizeIgnored; }
    const QPixmap& pauseShot() const { return m_pauseShot; }
    QSize storedConsoleSize() const { return m_storedConsoleSize; }
    DesktopGeo desktopGeometryType() const { return m_desktopGeometryType; }
    QSize desktopGeometry() const;
    QSize guestSizeHint();

    /* Protected setters: */
    void setDesktopGeometry(DesktopGeo geometry, int iWidth, int iHeight);
    void storeConsoleSize(int iWidth, int iHeight);
    void setMachineWindowResizeIgnored(bool fIgnore = true) { m_bIsMachineWindowResizeIgnored = fIgnore; }
    void storeGuestSizeHint(const QSize &sizeHint);

    /* Protected helpers: */
    void updateMouseCursorShape();
#ifdef Q_WS_WIN32
    void updateMouseCursorClipping();
#endif
    virtual QRect workingArea() = 0;
    virtual void calculateDesktopGeometry() = 0;
    virtual void maybeRestrictMinimumSize() = 0;
    virtual void updateSliders();

#ifdef Q_WS_MAC
    void updateDockIcon();
    void setMouseCoalescingEnabled(bool fOn);
    CGImageRef vmContentImage();
    CGImageRef frameBuffertoCGImageRef(UIFrameBuffer *pFrameBuffer);
#endif /* Q_WS_MAC */

    /* Prepare routines: */
    virtual void prepareFrameBuffer();
    virtual void prepareCommon();
    virtual void prepareFilters();
    virtual void prepareConsoleConnections();
    virtual void loadMachineViewSettings();

    /* Cleanup routines: */
    //virtual void saveMachineViewSettings() {}
    //virtual void cleanupConsoleConnections() {}
    //virtual void cleanupFilters() {}
    virtual void cleanupCommon();
    virtual void cleanupFrameBuffer();

    /* Cross-platforms event processors: */
    bool event(QEvent *pEvent);
    bool eventFilter(QObject *pWatched, QEvent *pEvent);

    /* Protected variables: */
    QSize m_desktopGeometry;

protected slots:

    /* Console callback handlers: */
    virtual void sltMachineStateChanged();
    virtual void sltMousePointerShapeChanged();
    virtual void sltMouseCapabilityChanged();
    virtual void sltPerformGuestResize(const QSize & /* toSize */) {};

    /* Session callback handlers: */
    virtual void sltMouseCapturedStatusChanged();

    /* Various helper slots: */
    virtual void sltNormalizeGeometry() { normalizeGeometry(true); }

private:

    /* Cross-platforms event processors: */
    void focusEvent(bool aHasFocus, bool aReleaseHostKey = true);
    bool keyEvent(int aKey, uint8_t aScan, int aFlags, wchar_t *aUniKey = NULL);
    bool mouseEvent(int aType, const QPoint &aPos, const QPoint &aGlobalPos,
                    Qt::MouseButtons aButtons, Qt::KeyboardModifiers aModifiers,
                    int aWheelDelta, Qt::Orientation aWheelDir);
    void resizeEvent(QResizeEvent *pEvent);
    void moveEvent(QMoveEvent *pEvent);
    void paintEvent(QPaintEvent *pEvent);

    /* Platform specific event processors: */
#if defined(Q_WS_WIN32)
    static LRESULT CALLBACK lowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    bool winLowKeyboardEvent(UINT msg, const KBDLLHOOKSTRUCT &event);
    bool winEvent(MSG *aMsg, long *aResult);
#elif defined(Q_WS_PM)
    bool pmEvent(QMSG *aMsg);
#elif defined(Q_WS_X11)
    bool x11Event(XEvent *event);
#elif defined(Q_WS_MAC)
    bool darwinKeyboardEvent(const void *pvCocoaEvent, EventRef inEvent);
    void darwinGrabKeyboardEvents(bool fGrab);
# ifdef QT_MAC_USE_COCOA
    static bool darwinEventHandlerProc(const void *pvCocoaEvent, const void *pvCarbonEvent, void *pvUser);
# else /* QT_MAC_USE_COCOA */
    static pascal OSStatus darwinEventHandlerProc(EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData);
# endif /* !QT_MAC_USE_COCOA */
#endif

    /* Private helpers: */
    void fixModifierState(LONG *piCodes, uint *puCount);
    QPoint viewportToContents(const QPoint &vp) const;
    void scrollBy(int dx, int dy);
#ifdef VBOX_WITH_VIDEOHWACCEL
    void scrollContentsBy(int dx, int dy);
#endif
    void emitKeyboardStateChanged();
    void emitMouseStateChanged();
    void captureKbd(bool fCapture, bool fEmitSignal = true);
    void captureMouse(bool fCapture, bool fEmitSignal = true);
    void saveKeyStates();
    void releaseAllPressedKeys(bool aReleaseHostKey = true);
    void sendChangedKeyStates();

    static void dimImage(QImage &img);

    /* Private members: */
    UIMachineWindow *m_pMachineWindow;
    VBoxDefs::RenderMode m_mode;
    ulong m_uScreenId;
    const VBoxGlobalSettings &m_globalSettings;
    UIFrameBuffer *m_pFrameBuffer;
    KMachineState m_previousState;

    DesktopGeo m_desktopGeometryType;
    QSize m_storedConsoleSize;

    QPoint m_lastMousePos;
    QPoint m_capturedMousePos;
    int m_iLastMouseWheelDelta;

    uint8_t m_pressedKeys[128];
    uint8_t m_pressedKeysCopy[128];

    bool m_bIsAutoCaptureDisabled : 1;
    bool m_bIsKeyboardCaptured : 1;
    bool m_bIsHostkeyPressed : 1;
    bool m_bIsHostkeyAlone : 1;
    bool m_bIsHostkeyInCapture : 1;
    bool m_bIsMachineWindowResizeIgnored : 1;
    bool m_fPassCAD;
#ifdef VBOX_WITH_VIDEOHWACCEL
    bool m_fAccelerate2DVideo;
#endif

#ifdef Q_WS_MAC
    /** The current modifier key mask. Used to figure out which modifier
     *  key was pressed when we get a kEventRawKeyModifiersChanged event. */
    UInt32 m_darwinKeyModifiers;
    bool m_fKeyboardGrabbed;
#endif /* Q_WS_MAC */

    QPixmap m_pauseShot;

    /* Friend classes: */
    friend class UIMachineLogic;
    friend class UIFrameBuffer;
    friend class UIFrameBufferQImage;
    friend class UIFrameBufferQuartz2D;
    friend class UIFrameBufferQGL;
    template<class, class, class> friend class VBoxOverlayFrameBuffer;
};

/* This maintenance class is a part of future roll-back mechanism.
 * It allows to block main GUI thread until specific event received.
 * Later it will become more abstract but now its just used to help
 * fullscreen & seamless modes to restore normal guest size hint. */
class UIMachineViewBlocker : public QEventLoop
{
    Q_OBJECT;

public:

    UIMachineViewBlocker()
        : QEventLoop(0)
        , m_iTimerId(0)
    {
        /* Also start timer to unlock pool in case of
         * required condition doesn't happens by some reason: */
        m_iTimerId = startTimer(3000);
    }

    virtual ~UIMachineViewBlocker()
    {
        /* Kill the timer: */
        killTimer(m_iTimerId);
    }

protected:

    void timerEvent(QTimerEvent *pEvent)
    {
        /* If that timer event occurs => it seems
         * guest resize event doesn't comes in time,
         * shame on it, but we just unlocking 'this': */
        QEventLoop::timerEvent(pEvent);
        exit();
    }

    int m_iTimerId;
};

#endif // !___UIMachineView_h___

