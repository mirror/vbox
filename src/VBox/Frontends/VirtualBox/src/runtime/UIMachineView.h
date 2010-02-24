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

#ifdef Q_WS_MAC
# include <CoreFoundation/CFBase.h>
#endif /* Q_WS_MAC */

/* Local forwards */
#ifdef Q_WS_MAC
class VBoxChangeDockIconUpdateEvent;
class VBoxChangePresentationModeEvent;
class VBoxDockIconPreview;
#endif /* Q_WS_MAC */

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

    /* Public virtual members: */
    virtual void normalizeGeometry(bool bAdjustPosition = false) = 0;

    /* Public setters: */
    void setIgnoreGuestResize(bool bIgnore) { m_bIsGuestResizeIgnored = bIgnore; }
    void setMouseIntegrationEnabled(bool bEnabled);
    //void setMachineViewFinalized(bool fTrue = true) { m_bIsMachineWindowResizeIgnored = !fTrue; }

#if defined(Q_WS_MAC)
    void updateDockIcon();
    void updateDockOverlay();
    void setMouseCoalescingEnabled(bool aOn);
    void setDockIconEnabled(bool aOn) { mDockIconEnabled = aOn; };
#endif

signals:

    /* Mouse/Keyboard state-change signals: */
    void keyboardStateChanged(int iState);
    void mouseStateChanged(int iState);

    /* Utility signals: */
    void resizeHintDone();

protected slots:

    /* Initiate resize request to guest: */
    virtual void doResizeHint(const QSize &aSize = QSize()) = 0;

protected:

    UIMachineView(  UIMachineWindow *pMachineWindow
                  , VBoxDefs::RenderMode renderMode
#ifdef VBOX_WITH_VIDEOHWACCEL
                  , bool bAccelerate2DVideo
#endif
    );
    virtual ~UIMachineView();

    /* Desktop geometry types: */
    enum DesktopGeo { DesktopGeo_Invalid = 0, DesktopGeo_Fixed, DesktopGeo_Automatic, DesktopGeo_Any };

    /* Protected getters: */
    UIMachineWindow* machineWindowWrapper() { return m_pMachineWindow; }
    CConsole &console() { return m_console; }

    /* Protected getters: */
    VBoxDefs::RenderMode mode() const { return m_mode; }
    QSize sizeHint() const;
    int contentsX() const;
    int contentsY() const;
    int contentsWidth() const;
    int contentsHeight() const;
    int visibleWidth() const;
    int visibleHeight() const;
    QRect desktopGeometry() const;
    bool isGuestSupportsGraphics() const { return m_bIsGuestSupportsGraphics; }
    const QPixmap& pauseShot() const { return m_pauseShot; }
    //bool isMouseAbsolute() const { return m_bIsMouseAbsolute; }

    /* Protected members: */
    void calculateDesktopGeometry();
    void setDesktopGeometry(DesktopGeo geometry, int iWidth, int iHeight);
    void storeConsoleSize(int iWidth, int iHeight);
    QRect availableGeometry();
    virtual void maybeRestrictMinimumSize() = 0;

    /* Prepare routines: */
    virtual void prepareFrameBuffer();
    virtual void prepareCommon();
    virtual void prepareFilters();
    virtual void prepareConsoleConnections();
    virtual void loadMachineViewSettings();

    /* Cleanup routines: */
    //virtual void saveMachineViewSettings();
    //virtual void cleanupConsoleConnections();
    //virtual void cleanupFilters();
    virtual void cleanupCommon();
    virtual void cleanupFrameBuffer();

private slots:

    void sltMousePointerShapeChange(bool fIsVisible, bool fHasAlpha,
                                    uint uXHot, uint uYHot, uint uWidth, uint uHeight,
                                    const uchar *pShapeData);
    void sltMouseCapabilityChange(bool bIsSupportsAbsolute, bool bNeedsHostCursor);
    void sltKeyboardLedsChange(bool bNumLock, bool bCapsLock, bool bScrollLock);
    void sltStateChange(KMachineState state);
    void sltAdditionsStateChange();

#ifdef Q_WS_MAC
    /* Dock icon update handler */
    void sltChangeDockIconUpdate(const VBoxChangeDockIconUpdateEvent &event);
# ifdef QT_MAC_USE_COCOA
    /* Presentation mode handler */
    void sltChangePresentationMode(const VBoxChangePresentationModeEvent &event);
# endif
#endif

private:

    /* Cross-platforms event processors: */
    bool event(QEvent *pEvent);
    bool eventFilter(QObject *pWatched, QEvent *pEvent);
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
    void updateSliders();
    void scrollBy(int dx, int dy);
#ifdef VBOX_WITH_VIDEOHWACCEL
    void scrollContentsBy(int dx, int dy);
#endif
    void emitKeyboardStateChanged();
    void emitMouseStateChanged();
    void captureKbd(bool fCapture, bool fEmitSignal = true);
    void captureMouse(bool fCapture, bool fEmitSignal = true);
    void saveKeyStates();
    bool processHotKey(const QKeySequence &key, const QList<QAction*> &data);
    void releaseAllPressedKeys(bool aReleaseHostKey = true);
    void sendChangedKeyStates();
    void updateMouseClipping();
    void setPointerShape(const uchar *pShapeData, bool fHasAlpha,
                         uint uXHot, uint uYHot, uint uWidth, uint uHeight);
    void setMouseIntegrationLocked(bool fDisabled);

    /* Private getters: */
    bool isRunning() { return m_machineState == KMachineState_Running || m_machineState == KMachineState_Teleporting || m_machineState == KMachineState_LiveSnapshotting; }
    bool shouldHideHostPointer() const { return m_bIsMouseCaptured || (m_bIsMouseAbsolute && m_fHideHostPointer); }

    static void dimImage(QImage &img);

    /* Private members: */
    UIMachineWindow *m_pMachineWindow;
    CConsole m_console;
    VBoxDefs::RenderMode m_mode;
    const VBoxGlobalSettings &m_globalSettings;
    KMachineState m_machineState;
    UIFrameBuffer *m_pFrameBuffer;

    QCursor m_lastCursor;
#if defined(Q_WS_WIN)
    HCURSOR m_alphaCursor;
#endif
    QPoint m_lastMousePos;
    QPoint m_capturedMousePos;
    int m_iLastMouseWheelDelta;

    uint8_t m_pressedKeys[128];
    uint8_t m_pressedKeysCopy[128];

    long m_uNumLockAdaptionCnt;
    long m_uCapsLockAdaptionCnt;

    bool m_bIsAutoCaptureDisabled : 1;
    bool m_bIsKeyboardCaptured : 1;
    bool m_bIsMouseCaptured : 1;
    bool m_bIsMouseAbsolute : 1;
    bool m_bIsMouseIntegrated : 1;
    bool m_bIsHostkeyPressed : 1;
    bool m_bIsHostkeyAlone : 1;
    bool m_bHostkeyInCapture : 1;
    bool m_bIsGuestSupportsGraphics : 1;
    bool m_bIsMachineWindowResizeIgnored : 1;
    bool m_bIsFrameBufferResizeIgnored : 1;
    bool m_bIsGuestResizeIgnored : 1;
    bool m_numLock : 1;
    bool m_scrollLock : 1;
    bool m_capsLock : 1;
    bool m_fPassCAD;
    bool m_fHideHostPointer;
#ifdef VBOX_WITH_VIDEOHWACCEL
    bool m_fAccelerate2DVideo;
#endif
#if 0 // TODO: Do we need this flag?
    bool mDoResize : 1;
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
    DesktopGeo m_desktopGeometryType;
    QRect m_desktopGeometry;
    QRect m_storedConsoleSize;

    /* Friend classes: */
    friend class UIFrameBuffer;
    friend class UIFrameBufferQImage;
    friend class UIFrameBufferQuartz2D;
};

#endif // !___UIMachineViewNormal_h___

