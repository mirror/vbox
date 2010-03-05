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

/* Global includes */
#include <QAbstractScrollArea>

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
#ifdef Q_WS_MAC
class VBoxChangeDockIconUpdateEvent;
class VBoxDockIconPreview;
#endif /* Q_WS_MAC */

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

    /* Public setters: */
    virtual void setGuestAutoresizeEnabled(bool /* fEnabled */) {}
    virtual void setMouseIntegrationEnabled(bool fEnabled);

    /* Public members: */
    virtual void normalizeGeometry(bool /* bAdjustPosition = false */) {}

#if defined(Q_WS_MAC)
    void updateDockIcon();
    void updateDockOverlay();
    void setMouseCoalescingEnabled(bool fOn);
    void setDockIconEnabled(bool aOn) { mDockIconEnabled = aOn; };
#endif

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
    bool isFrameBufferResizeIgnored() const { return m_bIsFrameBufferResizeIgnored; }
    const QPixmap& pauseShot() const { return m_pauseShot; }
    QSize storedConsoleSize() const { return m_storedConsoleSize; }
    virtual QSize desktopGeometry() const;

    /* Protected setters: */
    void setDesktopGeometry(DesktopGeo geometry, int iWidth, int iHeight);
    void storeConsoleSize(int iWidth, int iHeight);
    void setMachineWindowResizeIgnored(bool fIgnore = true) { m_bIsMachineWindowResizeIgnored = fIgnore; }
    void setFrameBufferResizeIgnored(bool fIgnore = true) { m_bIsFrameBufferResizeIgnored = fIgnore; }

    /* Protected helpers: */
    void calculateDesktopGeometry();
    void updateMouseClipping();
    void updateSliders();

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

protected slots:

    /* Console callback handlers: */
    virtual void sltMachineStateChanged();
    virtual void sltMousePointerShapeChanged();
    virtual void sltMouseCapabilityChanged();
    virtual void sltPerformGuestResize(const QSize & /* toSize */) {};

    /* Session callback handlers: */
    virtual void sltMouseCapturedStatusChanged();

private slots:

#ifdef Q_WS_MAC
    /* Dock icon update handler */
    void sltChangeDockIconUpdate(const VBoxChangeDockIconUpdateEvent &event);
#endif

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

    virtual QRect availableGeometry() = 0;

    static void dimImage(QImage &img);

    /* Private members: */
    UIMachineWindow *m_pMachineWindow;
    VBoxDefs::RenderMode m_mode;
    ulong m_uScreenId;
    const VBoxGlobalSettings &m_globalSettings;
    UIFrameBuffer *m_pFrameBuffer;
    KMachineState m_previousState;

    DesktopGeo m_desktopGeometryType;
    QSize m_desktopGeometry;
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
    bool m_bIsFrameBufferResizeIgnored : 1;
    bool m_fPassCAD;
#ifdef VBOX_WITH_VIDEOHWACCEL
    bool m_fAccelerate2DVideo;
#endif

#if defined(Q_WS_MAC)
# ifndef QT_MAC_USE_COCOA
    /** Event handler reference. NULL if the handler isn't installed. */
    EventHandlerRef m_darwinEventHandlerRef;
# endif /* !QT_MAC_USE_COCOA */
    /** The current modifier key mask. Used to figure out which modifier
     *  key was pressed when we get a kEventRawKeyModifiersChanged event. */
    UInt32 m_darwinKeyModifiers;
    bool m_fKeyboardGrabbed;
#endif

    QPixmap m_pauseShot;
#if defined(Q_WS_MAC)
# if !defined (QT_MAC_USE_COCOA)
    EventHandlerRef mDarwinWindowOverlayHandlerRef;
# endif
    VBoxDockIconPreview *mDockIconPreview;
    bool mDockIconEnabled;
#endif

    /* Friend classes: */
    friend class UIMachineWindowFullscreen;
    friend class UIFrameBuffer;
    friend class UIFrameBufferQImage;
    friend class UIFrameBufferQuartz2D;
    friend class UIFrameBufferQGL;
};

#endif // !___UIMachineViewNormal_h___

