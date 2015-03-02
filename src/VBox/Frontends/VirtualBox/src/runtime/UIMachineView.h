/** @file
 * VBox Qt GUI - UIMachineView class declaration.
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
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

/* Qt includes: */
#include <QAbstractScrollArea>
#include <QEventLoop>

/* GUI includes: */
#include "UIMachineDefs.h"
#include "UIExtraDataDefs.h"
#ifdef Q_WS_MAC
# include <CoreFoundation/CFBase.h>
#endif /* Q_WS_MAC */

/* COM includes: */
#include "COMEnums.h"

/* Other VBox includes: */
#include "VBox/com/ptr.h"

/* Forward declarations: */
class UISession;
class UIActionPool;
class UIMachineLogic;
class UIMachineWindow;
class UIFrameBuffer;
#ifdef VBOX_WITH_DRAG_AND_DROP
 class CDnDTarget;
#endif
class CSession;
class CMachine;
class CConsole;
class CDisplay;
class CGuest;

class UIMachineView : public QAbstractScrollArea
{
    Q_OBJECT;

signals:

    /** Notifies about frame-buffer resize. */
    void sigFrameBufferResize();

public:

    /** Policy for determining which guest resolutions we wish to
     * handle.  We also accept anything smaller than the current
     * resolution. */
    enum MaxGuestSizePolicy
    {
        /** Policy not set correctly. */
        MaxGuestSizePolicy_Invalid = 0,
        /** Anything up to a fixed size. */
        MaxGuestSizePolicy_Fixed,
        /** Anything up to available space on the host desktop. */
        MaxGuestSizePolicy_Automatic,
        /** We accept anything. */
        MaxGuestSizePolicy_Any
    };

    /* Factory function to create machine-view: */
    static UIMachineView* create(  UIMachineWindow *pMachineWindow
                                 , ulong uScreenId
                                 , UIVisualStateType visualStateType
#ifdef VBOX_WITH_VIDEOHWACCEL
                                 , bool bAccelerate2DVideo
#endif /* VBOX_WITH_VIDEOHWACCEL */
    );
    /* Factory function to destroy required machine-view: */
    static void destroy(UIMachineView *pMachineView);

    /* Public setters: */
    virtual void setGuestAutoresizeEnabled(bool /* fEnabled */) {}

    /** Adjusts guest-screen size to correspond current visual-style.
      * @note Reimplemented in sub-classes. Base implementation does nothing. */
    virtual void adjustGuestScreenSize() {}

    /** Applies machine-view scale-factor. */
    virtual void applyMachineViewScaleFactor();

    /* Framebuffer aspect ratio: */
    double aspectRatio() const;

protected slots:

    /* Slot to perform guest resize: */
    void sltPerformGuestResize(const QSize &aSize = QSize());

    /* Handler: Frame-buffer NotifyChange stuff: */
    virtual void sltHandleNotifyChange(int iWidth, int iHeight);

    /* Handler: Frame-buffer NotifyUpdate stuff: */
    virtual void sltHandleNotifyUpdate(int iX, int iY, int iWidth, int iHeight);

    /* Handler: Frame-buffer SetVisibleRegion stuff: */
    virtual void sltHandleSetVisibleRegion(QRegion region);

    /* Handler: Frame-buffer 3D overlay visibility stuff: */
    virtual void sltHandle3DOverlayVisibilityChange(bool fVisible);

    /* Watch dog for desktop resizes: */
    void sltDesktopResized();

    /** Handles the scale-factor change. */
    void sltHandleScaleFactorChange(const QString &strMachineID);

    /** Handles the unscaled HiDPI output mode change. */
    void sltHandleUnscaledHiDPIOutputModeChange(const QString &strMachineID);

    /* Console callback handlers: */
    virtual void sltMachineStateChanged();

protected:

    /* Machine-view constructor: */
    UIMachineView(  UIMachineWindow *pMachineWindow
                  , ulong uScreenId
#ifdef VBOX_WITH_VIDEOHWACCEL
                  , bool bAccelerate2DVideo
#endif /* VBOX_WITH_VIDEOHWACCEL */
    );
    /* Machine-view destructor: */
    virtual ~UIMachineView();

    /* Prepare routines: */
    void prepareViewport();
    void prepareFrameBuffer();
    virtual void prepareCommon();
    virtual void prepareFilters();
    virtual void prepareConnections();
    virtual void prepareConsoleConnections();
    void loadMachineViewSettings();

    /* Cleanup routines: */
    //virtual void saveMachineViewSettings() {}
    //virtual void cleanupConsoleConnections() {}
    //virtual void cleanupFilters() {}
    //virtual void cleanupCommon() {}
    virtual void cleanupFrameBuffer();
    //virtual void cleanupViewport();

    /** Returns the session UI reference. */
    UISession* uisession() const;

    /** Returns the session reference. */
    CSession& session() const;
    /** Returns the session's machine reference. */
    CMachine& machine() const;
    /** Returns the session's console reference. */
    CConsole& console() const;
    /** Returns the display's display reference. */
    CDisplay& display() const;
    /** Returns the console's guest reference. */
    CGuest& guest() const;

    /* Protected getters: */
    UIMachineWindow* machineWindow() const { return m_pMachineWindow; }
    UIActionPool* actionPool() const;
    UIMachineLogic* machineLogic() const;
    QSize sizeHint() const;
    int contentsX() const;
    int contentsY() const;
    int contentsWidth() const;
    int contentsHeight() const;
    int visibleWidth() const;
    int visibleHeight() const;
    ulong screenId() const { return m_uScreenId; }
    UIFrameBuffer* frameBuffer() const { return m_pFrameBuffer; }
    /** Atomically store the maximum guest resolution which we currently wish
     * to handle for @a maxGuestSize() to read.  Should be called if anything
     * happens (e.g. a screen hotplug) which might cause the value to change.
     * @sa m_u64MaxGuestSize. */
    void setMaxGuestSize(const QSize &minimumSizeHint = QSize());
    /** Atomically read the maximum guest resolution which we currently wish to
     * handle.  This may safely be called from another thread (called by
     * UIFramebuffer on EMT).
     * @sa m_u64MaxGuestSize. */
    QSize maxGuestSize();
    /** Retrieve the last non-fullscreen guest size hint (from extra data).
     */
    QSize guestSizeHint();

    /** Handles machine-view scale changes. */
    void handleScaleChange();

    /* Protected setters: */
    /** Store a guest size hint value to extra data, called on switching to
     * fullscreen. */
    void storeGuestSizeHint(const QSize &size);

    /** Returns the pause-pixmap: */
    const QPixmap& pausePixmap() const { return m_pausePixmap; }
    /** Returns the scaled pause-pixmap: */
    const QPixmap& pausePixmapScaled() const { return m_pausePixmapScaled; }
    /** Resets the pause-pixmap. */
    void resetPausePixmap();
    /** Acquires live pause-pixmap. */
    void takePausePixmapLive();
    /** Acquires snapshot pause-pixmap. */
    void takePausePixmapSnapshot();
    /** Updates the scaled pause-pixmap. */
    void updateScaledPausePixmap();

    /** The available area on the current screen for application windows. */
    virtual QRect workingArea() const = 0;
    /** Calculate how big the guest desktop can be while still fitting on one
     * host screen. */
    virtual QSize calculateMaxGuestSize() const = 0;
    virtual void updateSliders();
    QPoint viewportToContents(const QPoint &vp) const;
    void scrollBy(int dx, int dy);
    static void dimImage(QImage &img);
    void scrollContentsBy(int dx, int dy);
#ifdef Q_WS_MAC
    void updateDockIcon();
    CGImageRef vmContentImage();
    CGImageRef frameBuffertoCGImageRef(UIFrameBuffer *pFrameBuffer);
#endif /* Q_WS_MAC */
    /** What view mode (normal, fullscreen etc.) are we in? */
    UIVisualStateType visualStateType() const;
    /** Is this a fullscreen-type view? */
    bool isFullscreenOrSeamless() const;

    /* Cross-platforms event processors: */
    bool event(QEvent *pEvent);
    bool eventFilter(QObject *pWatched, QEvent *pEvent);
    void resizeEvent(QResizeEvent *pEvent);
    void moveEvent(QMoveEvent *pEvent);
    void paintEvent(QPaintEvent *pEvent);

#ifdef VBOX_WITH_DRAG_AND_DROP
    void dragEnterEvent(QDragEnterEvent *pEvent);
    void dragLeaveEvent(QDragLeaveEvent *pEvent);
    void dragMoveEvent(QDragMoveEvent *pEvent);
    void dragIsPending(void);
    void dropEvent(QDropEvent *pEvent);
#endif /* VBOX_WITH_DRAG_AND_DROP */

    /* Platform specific event processors: */
#if defined(Q_WS_WIN)
    bool winEvent(MSG *pMsg, long *puResult);
#elif defined(Q_WS_X11)
    bool x11Event(XEvent *event);
#endif

    /** Scales passed size forward. */
    QSize scaledForward(QSize size) const;
    /** Scales passed size backward. */
    QSize scaledBackward(QSize size) const;

    /* Protected members: */
    UIMachineWindow *m_pMachineWindow;
    ulong m_uScreenId;
    UIFrameBuffer *m_pFrameBuffer;
    KMachineState m_previousState;
    /** HACK: when switching out of fullscreen or seamless we wish to override
     * the default size hint to avoid short resizes back to fullscreen size.
     * Not explicitly initialised (i.e. invalid by default). */
    QSize m_sizeHintOverride;

    /** The policy for calculating the maximum guest resolution which we wish
     * to handle. */
    MaxGuestSizePolicy m_maxGuestSizePolicy;
    /** The maximum guest size for fixed size policy. */
    QSize m_fixedMaxGuestSize;
    /** Maximum guest resolution which we wish to handle.  Must be accessed
     * atomically.
     * @note The background for this variable is that we need this value to be
     * available to the EMT thread, but it can only be calculated by the
     * GUI, and GUI code can only safely be called on the GUI thread due to
     * (at least) X11 threading issues.  So we calculate the value in advance,
     * monitor things in case it changes and update it atomically when it does.
     */
    /** @todo This should be private. */
    volatile uint64_t m_u64MaxGuestSize;

#ifdef VBOX_WITH_VIDEOHWACCEL
    bool m_fAccelerate2DVideo : 1;
#endif /* VBOX_WITH_VIDEOHWACCEL */

    /** Holds the pause-pixmap. */
    QPixmap m_pausePixmap;
    /** Holds the scaled pause-pixmap. */
    QPixmap m_pausePixmapScaled;

    /* Friend classes: */
    friend class UIKeyboardHandler;
    friend class UIMouseHandler;
    friend class UIMachineLogic;
    friend class UIFrameBuffer;
    friend class UIFrameBufferPrivate;
    friend class VBoxOverlayFrameBuffer;
};

/* This maintenance class is a part of future roll-back mechanism.
 * It allows to block main GUI thread until specific event received.
 * Later it will become more abstract but now its just used to help
 * fullscreen & seamless modes to restore normal guest size hint. */
/** @todo This class is now unused - can it be removed altogether? */
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
