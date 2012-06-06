/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineView class declaration
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
#ifdef Q_WS_MAC
# include <CoreFoundation/CFBase.h>
#endif /* Q_WS_MAC */

/* COM includes: */
#include "COMEnums.h"

/* Forward declarations: */
class UISession;
class UIMachineLogic;
class UIMachineWindow;
class UIFrameBuffer;
class CSession;

class UIMachineView : public QAbstractScrollArea
{
    Q_OBJECT;

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

    /* Public members: */
    virtual void normalizeGeometry(bool /* bAdjustPosition = false */) = 0;

    /* Framebuffer aspect ratio: */
    double aspectRatio() const;

signals:

    /* Utility signals: */
    void resizeHintDone();

protected slots:

    /* Slot to perform guest resize: */
    void sltPerformGuestResize(const QSize &aSize = QSize());

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
    virtual void prepareViewport();
    virtual void prepareFrameBuffer();
    virtual void prepareCommon();
    virtual void prepareFilters();
    virtual void prepareConsoleConnections();
    virtual void loadMachineViewSettings();

    /* Cleanup routines: */
    //virtual void saveMachineViewSettings() {}
    //virtual void cleanupConsoleConnections() {}
    //virtual void cleanupFilters() {}
    //virtual void cleanupCommon() {}
    virtual void cleanupFrameBuffer();
    //virtual void cleanupViewport();

    /* Protected getters: */
    UIMachineWindow* machineWindow() const { return m_pMachineWindow; }
    UIMachineLogic* machineLogic() const;
    UISession* uisession() const;
    CSession& session();
    QSize sizeHint() const;
    int contentsX() const;
    int contentsY() const;
    int contentsWidth() const;
    int contentsHeight() const;
    int visibleWidth() const;
    int visibleHeight() const;
    ulong screenId() const { return m_uScreenId; }
    UIFrameBuffer* frameBuffer() const { return m_pFrameBuffer; }
    const QPixmap& pauseShot() const { return m_pauseShot; }
    /** Atomically store the maximum guest resolution which we currently wish
     * to handle for @a maxGuestSize() to read. */
    void setMaxGuestSize();
    /** Atomically read the maximum guest resolution which we currently wish to
     * handle.  This may safely be called from another thread (called by
     * UIFramebuffer on EMT). */
    QSize maxGuestSize();
    /** Retrieve the last non-fullscreen guest size hint (from extra data).
     */
    QSize guestSizeHint();

    /* Protected setters: */
    /** Store a guest size hint value to extra data, called on switching to
     * fullscreen. */
    void storeGuestSizeHint(const QSize &sizeHint);

    /* Protected helpers: */
    virtual void takePauseShotLive();
    virtual void takePauseShotSnapshot();
    virtual void resetPauseShot() { m_pauseShot = QPixmap(); }
    /** The available area on the current screen for application windows. */
    virtual QRect workingArea() const = 0;
    /** Calculate how big the guest desktop can be while still fitting on one
     * host screen. */
    virtual QSize calculateMaxGuestSize() const = 0;
    virtual void maybeRestrictMinimumSize() = 0;
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
    bool guestResizeEvent(QEvent *pEvent, bool fFullscreen);
    /** What view mode (normal, fullscreen etc.) are we in? */
    UIVisualStateType visualStateType() const;
    /** Is this a fullscreen-type view? */
    bool isFullscreenOrSeamless() const;
    /** Return a string consisting of @a base with a suffix for the active
     * virtual monitor.  Used for storing monitor-specific extra data. */
    QString makeExtraDataKeyPerMonitor(QString base) const;
    /** Returns the current rendering mode.
     * @note contains special case logic for scale mode. */
    VBoxDefs::RenderMode getRenderMode() const;

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
    void dropEvent(QDropEvent *pEvent);

    void handleGHDnd();
#endif /* VBOX_WITH_DRAG_AND_DROP */

    /* Platform specific event processors: */
#if defined(Q_WS_WIN)
    bool winEvent(MSG *pMsg, long *puResult);
#elif defined(Q_WS_X11)
    bool x11Event(XEvent *event);
#endif

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
     * atomically. */
    /** @todo This should be private. */
    volatile uint64_t m_u64MaxGuestSize;

#ifdef VBOX_WITH_VIDEOHWACCEL
    bool m_fAccelerate2DVideo : 1;
#endif /* VBOX_WITH_VIDEOHWACCEL */

    QPixmap m_pauseShot;

    /* Friend classes: */
    friend class UIKeyboardHandler;
    friend class UIMouseHandler;
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
