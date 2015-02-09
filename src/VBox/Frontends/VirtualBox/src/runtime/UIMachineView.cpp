/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineView class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QDesktopWidget>
# include <QMainWindow>
# include <QTimer>
# include <QPainter>
# include <QScrollBar>
# include <QMainWindow>

/* GUI includes: */
# include "VBoxGlobal.h"
# include "UIMessageCenter.h"
# include "UIFrameBuffer.h"
# include "VBoxFBOverlay.h"
# include "UISession.h"
# include "UIKeyboardHandler.h"
# include "UIMouseHandler.h"
# include "UIMachineLogic.h"
# include "UIMachineWindow.h"
# include "UIMachineViewNormal.h"
# include "UIMachineViewFullscreen.h"
# include "UIMachineViewSeamless.h"
# include "UIMachineViewScale.h"
# include "UIExtraDataManager.h"
# ifdef VBOX_WITH_DRAG_AND_DROP
#  include "UIDnDHandler.h"
# endif /* VBOX_WITH_DRAG_AND_DROP */

/* COM includes: */
# include "CSession.h"
# include "CConsole.h"
# include "CDisplay.h"
# include "CFramebuffer.h"
# ifdef VBOX_WITH_DRAG_AND_DROP
#  include "CDnDSource.h"
#  include "CDnDTarget.h"
#  include "CGuest.h"
#  include "CGuestDnDSource.h"
#  include "CGuestDnDTarget.h"
# endif /* VBOX_WITH_DRAG_AND_DROP */

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Other VBox includes: */
#include <iprt/asm.h>
#include <VBox/VBoxOGL.h>
#include <VBox/VBoxVideo.h>

#ifdef Q_WS_X11
# include <X11/XKBlib.h>
# include <QX11Info>
# ifdef KeyPress
const int XFocusOut = FocusOut;
const int XFocusIn = FocusIn;
const int XKeyPress = KeyPress;
const int XKeyRelease = KeyRelease;
#  undef KeyRelease
#  undef KeyPress
#  undef FocusOut
#  undef FocusIn
# endif
#endif /* Q_WS_X11 */

#ifdef Q_WS_MAC
# include "DockIconPreview.h"
# include "DarwinKeyboard.h"
# include "UICocoaApplication.h"
# include <VBox/err.h>
# include <Carbon/Carbon.h>
#endif /* Q_WS_MAC */

/* Other includes: */
#include <math.h>


UIMachineView* UIMachineView::create(  UIMachineWindow *pMachineWindow
                                     , ulong uScreenId
                                     , UIVisualStateType visualStateType
#ifdef VBOX_WITH_VIDEOHWACCEL
                                     , bool bAccelerate2DVideo
#endif /* VBOX_WITH_VIDEOHWACCEL */
                                     )
{
    UIMachineView *pMachineView = 0;
    switch (visualStateType)
    {
        case UIVisualStateType_Normal:
            pMachineView = new UIMachineViewNormal(  pMachineWindow
                                                   , uScreenId
#ifdef VBOX_WITH_VIDEOHWACCEL
                                                   , bAccelerate2DVideo
#endif /* VBOX_WITH_VIDEOHWACCEL */
                                                   );
            break;
        case UIVisualStateType_Fullscreen:
            pMachineView = new UIMachineViewFullscreen(  pMachineWindow
                                                       , uScreenId
#ifdef VBOX_WITH_VIDEOHWACCEL
                                                       , bAccelerate2DVideo
#endif /* VBOX_WITH_VIDEOHWACCEL */
                                                       );
            break;
        case UIVisualStateType_Seamless:
            pMachineView = new UIMachineViewSeamless(  pMachineWindow
                                                     , uScreenId
#ifdef VBOX_WITH_VIDEOHWACCEL
                                                     , bAccelerate2DVideo
#endif /* VBOX_WITH_VIDEOHWACCEL */
                                                     );
            break;
        case UIVisualStateType_Scale:
            pMachineView = new UIMachineViewScale(  pMachineWindow
                                                  , uScreenId
#ifdef VBOX_WITH_VIDEOHWACCEL
                                                  , bAccelerate2DVideo
#endif
                                                  );
            break;
        default:
            break;
    }

    /* Prepare common things: */
    pMachineView->prepareCommon();

    /* Prepare event-filters: */
    pMachineView->prepareFilters();

    /* Prepare connections: */
    pMachineView->prepareConnections();

    /* Prepare console connections: */
    pMachineView->prepareConsoleConnections();

    /* Initialization: */
    pMachineView->sltMachineStateChanged();
    /** @todo Can we move the call to sltAdditionsStateChanged() from the
     * subclass constructors here too?  It is called for Normal and Seamless,
     * but not for Fullscreen and Scale.  However for Scale it is a no op.,
     * so it would not hurt.  Would it hurt for Fullscreen? */

    /* Set a preliminary maximum size: */
    pMachineView->setMaxGuestSize();

    return pMachineView;
}

void UIMachineView::destroy(UIMachineView *pMachineView)
{
    delete pMachineView;
}

void UIMachineView::applyMachineViewScaleFactor()
{
    /* Take the scale-factor into account: */
    const double dScaleFactor = gEDataManager->scaleFactor(vboxGlobal().managedVMUuid());
    frameBuffer()->setScaleFactor(dScaleFactor);
    /* Propagate scale-factor to 3D service if necessary: */
    if (machine().GetAccelerate3DEnabled() && vboxGlobal().is3DAvailable())
    {
        display().NotifyScaleFactorChange(m_uScreenId,
                                          (uint32_t)(dScaleFactor * VBOX_OGL_SCALE_FACTOR_MULTIPLIER),
                                          (uint32_t)(dScaleFactor * VBOX_OGL_SCALE_FACTOR_MULTIPLIER));
    }

    /* Take unscaled HiDPI output mode into account: */
    const bool fUseUnscaledHiDPIOutput = gEDataManager->useUnscaledHiDPIOutput(vboxGlobal().managedVMUuid());
    frameBuffer()->setUseUnscaledHiDPIOutput(fUseUnscaledHiDPIOutput);

    /* Perform frame-buffer rescaling: */
    frameBuffer()->performRescale();
}

double UIMachineView::aspectRatio() const
{
    return frameBuffer() ? (double)(frameBuffer()->width()) / frameBuffer()->height() : 0;
}

void UIMachineView::sltPerformGuestResize(const QSize &toSize)
{
    /* If this slot is invoked directly then use the passed size otherwise get
     * the available size for the guest display. We assume here that centralWidget()
     * contains this view only and gives it all available space: */
    QSize size(toSize.isValid() ? toSize : machineWindow()->centralWidget()->size());
    AssertMsg(size.isValid(), ("Size should be valid!\n"));

    /* Take the scale-factor(s) into account: */
    size = scaledBackward(size);

    /* Expand current limitations: */
    setMaxGuestSize(size);

    /* Send new size-hint to the guest: */
    LogRel(("UIMachineView::sltPerformGuestResize: "
            "Sending guest size-hint to screen %d as %dx%d\n",
            (int)screenId(), size.width(), size.height()));
    display().SetVideoModeHint(screenId(),
                               uisession()->isScreenVisible(screenId()),
                               false, 0, 0, size.width(), size.height(), 0);

    /* And track whether we have a "normal" or "fullscreen"/"seamless" size-hint sent: */
    gEDataManager->markLastGuestSizeHintAsFullScreen(m_uScreenId, isFullscreenOrSeamless(), vboxGlobal().managedVMUuid());
}

void UIMachineView::sltHandleNotifyChange(int iWidth, int iHeight)
{
    LogRel(("UIMachineView::sltHandleNotifyChange: Screen=%d, Size=%dx%d.\n",
            (unsigned long)m_uScreenId, iWidth, iHeight));

    // TODO: Move to appropriate place!
    /* Some situations require frame-buffer resize-events to be ignored at all,
     * leaving machine-window, machine-view and frame-buffer sizes preserved: */
    if (uisession()->isGuestResizeIgnored())
        return;

    /* If machine-window is visible: */
    if (uisession()->isScreenVisible(m_uScreenId))
    {
        /* Check if that notify-change brings actual resize-event: */
        const bool fActualResize = frameBuffer()->width() != (ulong)iWidth ||
                                   frameBuffer()->height() != (ulong)iHeight;

        /* Perform frame-buffer mode-change: */
        frameBuffer()->handleNotifyChange(iWidth, iHeight);

        /* For 'scale' mode: */
        if (visualStateType() == UIVisualStateType_Scale)
        {
            /* Assign new frame-buffer logical-size: */
            frameBuffer()->setScaledSize(size());
        }
        /* For other than 'scale' mode: */
        else
        {
            /* Adjust maximum-size restriction for machine-view: */
            setMaximumSize(sizeHint());

            /* Disable the resize hint override hack: */
            m_sizeHintOverride = QSize(-1, -1);

            /* Force machine-window update own layout: */
            QCoreApplication::sendPostedEvents(0, QEvent::LayoutRequest);

            /* Update machine-view sliders: */
            updateSliders();

            /* By some reason Win host forgets to update machine-window central-widget
             * after main-layout was updated, let's do it for all the hosts: */
            machineWindow()->centralWidget()->update();

            /* Normalize 'normal' machine-window geometry if necessary: */
            if (visualStateType() == UIVisualStateType_Normal && fActualResize)
                machineWindow()->normalizeGeometry(true /* adjust position */);
        }

        /* Perform frame-buffer rescaling: */
        frameBuffer()->performRescale();

#ifdef Q_WS_MAC
        /* Update MacOS X dock icon size: */
        machineLogic()->updateDockIconSize(screenId(), iWidth, iHeight);
#endif /* Q_WS_MAC */
    }

    /* Notify frame-buffer resize: */
    emit sigFrameBufferResize();

    /* Ask for just required guest display update (it will also update
     * the viewport through IFramebuffer::NotifyUpdate): */
    display().InvalidateAndUpdateScreen(m_uScreenId);

    LogRelFlow(("UIMachineView::sltHandleNotifyChange: Complete for Screen=%d, Size=%dx%d.\n",
                (unsigned long)m_uScreenId, iWidth, iHeight));
}

void UIMachineView::sltHandleNotifyUpdate(int iX, int iY, int iWidth, int iHeight)
{
    /* Prepare corresponding viewport part: */
    QRect rect(iX, iY, iWidth, iHeight);

    /* Take the scaling into account: */
    const double dScaleFactor = frameBuffer()->scaleFactor();
    const QSize scaledSize = frameBuffer()->scaledSize();
    if (scaledSize.isValid())
    {
        /* Calculate corresponding scale-factors: */
        const double xScaleFactor = visualStateType() == UIVisualStateType_Scale ?
                                    (double)scaledSize.width()  / frameBuffer()->width()  : dScaleFactor;
        const double yScaleFactor = visualStateType() == UIVisualStateType_Scale ?
                                    (double)scaledSize.height() / frameBuffer()->height() : dScaleFactor;
        /* Adjust corresponding viewport part: */
        rect.moveTo(floor((double)rect.x() * xScaleFactor) - 1,
                    floor((double)rect.y() * yScaleFactor) - 1);
        rect.setSize(QSize(ceil((double)rect.width()  * xScaleFactor) + 2,
                           ceil((double)rect.height() * yScaleFactor) + 2));
    }

    /* Shift has to be scaled by the backing-scale-factor
     * but not scaled by the scale-factor. */
    rect.translate(-contentsX(), -contentsY());

#ifdef Q_WS_MAC
    /* Take the backing-scale-factor into account: */
    if (frameBuffer()->useUnscaledHiDPIOutput())
    {
        const double dBackingScaleFactor = frameBuffer()->backingScaleFactor();
        if (dBackingScaleFactor > 1.0)
        {
            rect.moveTo(floor((double)rect.x() / dBackingScaleFactor) - 1,
                        floor((double)rect.y() / dBackingScaleFactor) - 1);
            rect.setSize(QSize(ceil((double)rect.width()  / dBackingScaleFactor) + 2,
                               ceil((double)rect.height() / dBackingScaleFactor) + 2));
        }
    }
#endif /* Q_WS_MAC */

    /* Limit the resulting part by the viewport rectangle: */
    rect &= viewport()->rect();

    /* Update corresponding viewport part: */
    viewport()->update(rect);
}

void UIMachineView::sltHandleSetVisibleRegion(QRegion region)
{
    /* Used only in seamless-mode. */
    Q_UNUSED(region);
}

void UIMachineView::sltHandle3DOverlayVisibilityChange(bool fVisible)
{
    machineLogic()->notifyAbout3DOverlayVisibilityChange(fVisible);
}

void UIMachineView::sltDesktopResized()
{
    setMaxGuestSize();
}

void UIMachineView::sltHandleScaleFactorChange(const QString &strMachineID)
{
    /* Skip unrelated machine IDs: */
    if (strMachineID != vboxGlobal().managedVMUuid())
        return;

    /* Take the scale-factor into account: */
    const double dScaleFactor = gEDataManager->scaleFactor(vboxGlobal().managedVMUuid());
    frameBuffer()->setScaleFactor(dScaleFactor);
    /* Propagate scale-factor to 3D service if necessary: */
    if (machine().GetAccelerate3DEnabled() && vboxGlobal().is3DAvailable())
    {
        display().NotifyScaleFactorChange(m_uScreenId,
                                          (uint32_t)(dScaleFactor * VBOX_OGL_SCALE_FACTOR_MULTIPLIER),
                                          (uint32_t)(dScaleFactor * VBOX_OGL_SCALE_FACTOR_MULTIPLIER));
    }

    /* Handle scale attributes change: */
    handleScaleChange();
    /* Adjust guest-screen size: */
    adjustGuestScreenSize();

    /* Update scaled pause pixmap, if necessary: */
    updateScaledPausePixmap();
    viewport()->update();
}

void UIMachineView::sltHandleUnscaledHiDPIOutputModeChange(const QString &strMachineID)
{
    /* Skip unrelated machine IDs: */
    if (strMachineID != vboxGlobal().managedVMUuid())
        return;

    /* Take unscaled HiDPI output mode into account: */
    const bool fUseUnscaledHiDPIOutput = gEDataManager->useUnscaledHiDPIOutput(vboxGlobal().managedVMUuid());
    frameBuffer()->setUseUnscaledHiDPIOutput(fUseUnscaledHiDPIOutput);

    /* Handle scale attributes change: */
    handleScaleChange();
    /* Adjust guest-screen size: */
    adjustGuestScreenSize();

    /* Update scaled pause pixmap, if necessary: */
    updateScaledPausePixmap();
    viewport()->update();
}

void UIMachineView::sltMachineStateChanged()
{
    /* Get machine state: */
    KMachineState state = uisession()->machineState();
    switch (state)
    {
        case KMachineState_Paused:
        case KMachineState_TeleportingPausedVM:
        {
            if (   m_pFrameBuffer
                && (   state           != KMachineState_TeleportingPausedVM
                    || m_previousState != KMachineState_Teleporting))
            {
                /* Take live pause-pixmap: */
                takePausePixmapLive();
                /* Fully repaint to pick up pause-pixmap: */
                viewport()->update();
            }
            break;
        }
        case KMachineState_Restoring:
        {
            /* Only works with the primary screen currently. */
            if (screenId() == 0)
            {
                /* Take snapshot pause-pixmap: */
                takePausePixmapSnapshot();
                /* Fully repaint to pick up pause-pixmap: */
                viewport()->update();
            }
            break;
        }
        case KMachineState_Running:
        {
            if (m_previousState == KMachineState_Paused ||
                m_previousState == KMachineState_TeleportingPausedVM ||
                m_previousState == KMachineState_Restoring)
            {
                if (m_pFrameBuffer)
                {
                    /* Reset pause-pixmap: */
                    resetPausePixmap();
                    /* Ask for full guest display update (it will also update
                     * the viewport through IFramebuffer::NotifyUpdate): */
                    display().InvalidateAndUpdate();
                }
            }
            /* Reapply machine-view scale-factor if necessary: */
            if (m_pFrameBuffer)
                applyMachineViewScaleFactor();
            break;
        }
        default:
            break;
    }

    m_previousState = state;
}

UIMachineView::UIMachineView(  UIMachineWindow *pMachineWindow
                             , ulong uScreenId
#ifdef VBOX_WITH_VIDEOHWACCEL
                             , bool bAccelerate2DVideo
#endif /* VBOX_WITH_VIDEOHWACCEL */
                             )
    : QAbstractScrollArea(pMachineWindow->centralWidget())
    , m_pMachineWindow(pMachineWindow)
    , m_uScreenId(uScreenId)
    , m_pFrameBuffer(0)
    , m_previousState(KMachineState_Null)
    , m_maxGuestSizePolicy(MaxGuestSizePolicy_Invalid)
    , m_u64MaxGuestSize(0)
#ifdef VBOX_WITH_VIDEOHWACCEL
    , m_fAccelerate2DVideo(bAccelerate2DVideo)
#endif /* VBOX_WITH_VIDEOHWACCEL */
{
    /* Load machine view settings: */
    loadMachineViewSettings();

    /* Prepare viewport: */
    prepareViewport();

    /* Prepare frame buffer: */
    prepareFrameBuffer();
}

UIMachineView::~UIMachineView()
{
}

void UIMachineView::prepareViewport()
{
    /* Prepare viewport: */
    AssertPtrReturnVoid(viewport());
    {
        /* Enable manual painting: */
        viewport()->setAttribute(Qt::WA_OpaquePaintEvent);
        /* Enable multi-touch support: */
        viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
    }
}

void UIMachineView::prepareFrameBuffer()
{
    /* Check whether we already have corresponding frame-buffer: */
    UIFrameBuffer *pFrameBuffer = uisession()->frameBuffer(screenId());

    /* If we do: */
    if (pFrameBuffer)
    {
        /* Assign it's view: */
        pFrameBuffer->setView(this);
        /* Mark frame-buffer as used again: */
        LogRelFlow(("UIMachineView::prepareFrameBuffer: Start EMT callbacks accepting for screen: %d.\n", screenId()));
        pFrameBuffer->setMarkAsUnused(false);
        /* And remember our choice: */
        m_pFrameBuffer = pFrameBuffer;
    }
    /* If we do not: */
    else
    {
#ifdef VBOX_WITH_VIDEOHWACCEL
        /* If 2D video acceleration is activated: */
        if (m_fAccelerate2DVideo)
        {
            /* Create new frame-buffer on the basis
             * of VBoxOverlayFrameBuffer template: */
            ComObjPtr<VBoxOverlayFrameBuffer> pOFB;
            pOFB.createObject();
            pOFB->init(this, &session(), (uint32_t)screenId());
            m_pFrameBuffer = pOFB;
        }
        /* If 2D video acceleration is not activated: */
        else
        {
            /* Create new default frame-buffer: */
            m_pFrameBuffer.createObject();
            m_pFrameBuffer->init(this);
        }
#else /* VBOX_WITH_VIDEOHWACCEL */
        /* Create new default frame-buffer: */
        m_pFrameBuffer.createObject();
        m_pFrameBuffer->init(this);
#endif /* !VBOX_WITH_VIDEOHWACCEL */

        /* Take HiDPI optimization type into account: */
        m_pFrameBuffer->setHiDPIOptimizationType(uisession()->hiDPIOptimizationType());

        /* Take scale-factor into account: */
        const double dScaleFactor = gEDataManager->scaleFactor(vboxGlobal().managedVMUuid());
        m_pFrameBuffer->setScaleFactor(dScaleFactor);
        /* Propagate scale-factor to 3D service if necessary: */
        if (machine().GetAccelerate3DEnabled() && vboxGlobal().is3DAvailable())
        {
            display().NotifyScaleFactorChange(m_uScreenId,
                                              (uint32_t)(dScaleFactor * VBOX_OGL_SCALE_FACTOR_MULTIPLIER),
                                              (uint32_t)(dScaleFactor * VBOX_OGL_SCALE_FACTOR_MULTIPLIER));
        }

#ifdef Q_WS_MAC
        /* Take backing scale-factor into account: */
        m_pFrameBuffer->setBackingScaleFactor(darwinBackingScaleFactor(machineWindow()));
#endif /* Q_WS_MAC */

        /* Take unscaled HiDPI output mode into account: */
        const bool fUseUnscaledHiDPIOutput = gEDataManager->useUnscaledHiDPIOutput(vboxGlobal().managedVMUuid());
        m_pFrameBuffer->setUseUnscaledHiDPIOutput(fUseUnscaledHiDPIOutput);

        /* Perform frame-buffer rescaling: */
        m_pFrameBuffer->performRescale();

        /* Associate uisession with frame-buffer finally: */
        uisession()->setFrameBuffer(screenId(), m_pFrameBuffer);
    }

    /* Make sure frame-buffer was prepared: */
    AssertReturnVoid(m_pFrameBuffer);

    /* Prepare display: */
    CFramebuffer fb = display().QueryFramebuffer(m_uScreenId);
    /* Always perform AttachFramebuffer to ensure 3D gets notified: */
    if (!fb.isNull())
        display().DetachFramebuffer(m_uScreenId);
    display().AttachFramebuffer(m_uScreenId, CFramebuffer(m_pFrameBuffer));

    /* Calculate frame-buffer size: */
    QSize size;
    {
#ifdef Q_WS_X11
        /* Processing pseudo resize-event to synchronize frame-buffer with stored framebuffer size.
         * On X11 this will be additional done when the machine state was 'saved'. */
        if (machine().GetState() == KMachineState_Saved)
            size = guestSizeHint();
#endif /* Q_WS_X11 */

        /* If there is a preview image saved,
         * we will resize the framebuffer to the size of that image: */
        ULONG uBuffer = 0, uWidth = 0, uHeight = 0;
        machine().QuerySavedScreenshotPNGSize(0, uBuffer, uWidth, uHeight);
        if (uBuffer > 0)
        {
            /* Init with the screenshot size: */
            size = QSize(uWidth, uHeight);
            /* Try to get the real guest dimensions from the save-state: */
            ULONG uGuestOriginX = 0, uGuestOriginY = 0, uGuestWidth = 0, uGuestHeight = 0;
            BOOL fEnabled = true;
            machine().QuerySavedGuestScreenInfo(m_uScreenId, uGuestOriginX, uGuestOriginY, uGuestWidth, uGuestHeight, fEnabled);
            if (uGuestWidth  > 0 && uGuestHeight > 0)
                size = QSize(uGuestWidth, uGuestHeight);
        }

        /* If we have a valid size, resize/rescale the frame-buffer. */
        if (size.width() > 0 && size.height() > 0)
        {
            frameBuffer()->performResize(size.width(), size.height());
            frameBuffer()->performRescale();
        }
    }
}

void UIMachineView::prepareCommon()
{
    /* Prepare view frame: */
    setFrameStyle(QFrame::NoFrame);

    /* Setup palette: */
    QPalette palette(viewport()->palette());
    palette.setColor(viewport()->backgroundRole(), Qt::black);
    viewport()->setPalette(palette);

    /* Setup focus policy: */
    setFocusPolicy(Qt::WheelFocus);

#ifdef VBOX_WITH_DRAG_AND_DROP
    /* Enable Drag & Drop. */
    setAcceptDrops(true);
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

void UIMachineView::prepareFilters()
{
    /* Enable MouseMove events: */
    viewport()->setMouseTracking(true);

    /* We have to watch for own events too: */
    installEventFilter(this);

    /* QScrollView does the below on its own, but let's
     * do it anyway for the case it will not do it in the future: */
    viewport()->installEventFilter(this);

    /* We want to be notified on some parent's events: */
    machineWindow()->installEventFilter(this);
}

void UIMachineView::prepareConnections()
{
    /* Desktop resolution change (e.g. monitor hotplug): */
    connect(QApplication::desktop(), SIGNAL(resized(int)), this,
            SLOT(sltDesktopResized()));
    /* Scale-factor change: */
    connect(gEDataManager, SIGNAL(sigScaleFactorChange(const QString&)),
            this, SLOT(sltHandleScaleFactorChange(const QString&)));
    /* Unscaled HiDPI output mode change: */
    connect(gEDataManager, SIGNAL(sigUnscaledHiDPIOutputModeChange(const QString&)),
            this, SLOT(sltHandleUnscaledHiDPIOutputModeChange(const QString&)));
}

void UIMachineView::prepareConsoleConnections()
{
    /* Machine state-change updater: */
    connect(uisession(), SIGNAL(sigMachineStateChange()), this, SLOT(sltMachineStateChanged()));
}

void UIMachineView::loadMachineViewSettings()
{
    /* Global settings: */
    {
        /* Remember the maximum guest size policy for telling the guest about
         * video modes we like: */
        QString maxGuestSize = vboxGlobal().settings().publicProperty("GUI/MaxGuestResolution");
        if ((maxGuestSize == QString::null) || (maxGuestSize == "auto"))
            m_maxGuestSizePolicy = MaxGuestSizePolicy_Automatic;
        else if (maxGuestSize == "any")
            m_maxGuestSizePolicy = MaxGuestSizePolicy_Any;
        else  /** @todo Mea culpa, but what about error checking? */
        {
            int width  = maxGuestSize.section(',', 0, 0).toInt();
            int height = maxGuestSize.section(',', 1, 1).toInt();
            m_maxGuestSizePolicy = MaxGuestSizePolicy_Fixed;
            m_fixedMaxGuestSize = QSize(width, height);
        }
    }
}

void UIMachineView::cleanupFrameBuffer()
{
    /* Make sure proper framebuffer assigned: */
    AssertReturnVoid(m_pFrameBuffer);
    AssertReturnVoid(m_pFrameBuffer == uisession()->frameBuffer(screenId()));

    /* Mark framebuffer as unused: */
    LogRelFlow(("UIMachineView::cleanupFrameBuffer: Stop EMT callbacks accepting for screen: %d.\n", screenId()));
    m_pFrameBuffer->setMarkAsUnused(true);

    /* Process pending framebuffer events: */
    QApplication::sendPostedEvents(this, QEvent::MetaCall);

#ifdef VBOX_WITH_VIDEOHWACCEL
    if (m_fAccelerate2DVideo)
        QApplication::sendPostedEvents(this, VHWACommandProcessType);
#endif /* VBOX_WITH_VIDEOHWACCEL */

    /* Temporarily detach the framebuffer from IDisplay before detaching
     * from view in order to respect the thread synchonisation logic (see UIFrameBuffer.h).
     * Note: VBOX_WITH_CROGL additionally requires us to call DetachFramebuffer
     * to ensure 3D gets notified of view being destroyed... */
    if (console().isOk() && !display().isNull())
        display().DetachFramebuffer(m_uScreenId);

    /* Detach framebuffer from view: */
    m_pFrameBuffer->setView(NULL);
}

UISession* UIMachineView::uisession() const
{
    return machineWindow()->uisession();
}

CSession& UIMachineView::session() const
{
    return uisession()->session();
}

CMachine& UIMachineView::machine() const
{
    return uisession()->machine();
}

CConsole& UIMachineView::console() const
{
    return uisession()->console();
}

CDisplay& UIMachineView::display() const
{
    return uisession()->display();
}

CGuest& UIMachineView::guest() const
{
    return uisession()->guest();
}

UIActionPool* UIMachineView::actionPool() const
{
    return machineWindow()->actionPool();
}

UIMachineLogic* UIMachineView::machineLogic() const
{
    return machineWindow()->machineLogic();
}

QSize UIMachineView::sizeHint() const
{
    if (m_sizeHintOverride.isValid())
        return m_sizeHintOverride;

    /* Get frame-buffer size-hint: */
    QSize size(m_pFrameBuffer->width(), m_pFrameBuffer->height());

    /* Take the scale-factor(s) into account: */
    size = scaledForward(size);

#ifdef VBOX_WITH_DEBUGGER_GUI
    // TODO: Fix all DEBUGGER stuff!
    /* HACK ALERT! Really ugly workaround for the resizing to 9x1 done by DevVGA if provoked before power on. */
    if (size.width() < 16 || size.height() < 16)
        if (vboxGlobal().isStartPausedEnabled() || vboxGlobal().isDebuggerAutoShowEnabled())
            size = QSize(640, 480);
#endif /* !VBOX_WITH_DEBUGGER_GUI */

    /* Return the resulting size-hint: */
    return QSize(size.width() + frameWidth() * 2, size.height() + frameWidth() * 2);
}

int UIMachineView::contentsX() const
{
    return horizontalScrollBar()->value();
}

int UIMachineView::contentsY() const
{
    return verticalScrollBar()->value();
}

int UIMachineView::contentsWidth() const
{
    return m_pFrameBuffer->width();
}

int UIMachineView::contentsHeight() const
{
    return m_pFrameBuffer->height();
}

int UIMachineView::visibleWidth() const
{
    return horizontalScrollBar()->pageStep();
}

int UIMachineView::visibleHeight() const
{
    return verticalScrollBar()->pageStep();
}

void UIMachineView::setMaxGuestSize(const QSize &minimumSizeHint /* = QSize() */)
{
    QSize maxSize;
    switch (m_maxGuestSizePolicy)
    {
        case MaxGuestSizePolicy_Fixed:
            maxSize = m_fixedMaxGuestSize;
            break;
        case MaxGuestSizePolicy_Automatic:
            maxSize = calculateMaxGuestSize().expandedTo(minimumSizeHint);
            break;
        case MaxGuestSizePolicy_Any:
        default:
            AssertMsg(m_maxGuestSizePolicy == MaxGuestSizePolicy_Any,
                      ("Invalid maximum guest size policy %d!\n",
                       m_maxGuestSizePolicy));
            /* (0, 0) means any of course. */
            maxSize = QSize(0, 0);
    }
    ASMAtomicWriteU64(&m_u64MaxGuestSize,
                      RT_MAKE_U64(maxSize.height(), maxSize.width()));
}

QSize UIMachineView::maxGuestSize()
{
    uint64_t u64Size = ASMAtomicReadU64(&m_u64MaxGuestSize);
    return QSize(int(RT_HI_U32(u64Size)), int(RT_LO_U32(u64Size)));
}

QSize UIMachineView::guestSizeHint()
{
    /* Load guest-screen size-hint: */
    QSize size = gEDataManager->lastGuestSizeHint(m_uScreenId, vboxGlobal().managedVMUuid());

    /* Invent the default if necessary: */
    if (!size.isValid())
        size = QSize(800, 600);

    /* Take the scale-factor(s) into account: */
    size = scaledForward(size);

    /* Return size: */
    return size;
}

void UIMachineView::handleScaleChange()
{
    LogRel(("UIMachineView::handleScaleChange: Screen=%d.\n",
            (unsigned long)m_uScreenId));

    /* If machine-window is visible: */
    if (uisession()->isScreenVisible(m_uScreenId))
    {
        /* For 'scale' mode: */
        if (visualStateType() == UIVisualStateType_Scale)
        {
            /* Assign new frame-buffer logical-size: */
            frameBuffer()->setScaledSize(size());
        }
        /* For other than 'scale' mode: */
        else
        {
            /* Adjust maximum-size restriction for machine-view: */
            setMaximumSize(sizeHint());

            /* Disable the resize hint override hack: */
            m_sizeHintOverride = QSize(-1, -1);

            /* Force machine-window update own layout: */
            QCoreApplication::sendPostedEvents(0, QEvent::LayoutRequest);

            /* Update machine-view sliders: */
            updateSliders();

            /* By some reason Win host forgets to update machine-window central-widget
             * after main-layout was updated, let's do it for all the hosts: */
            machineWindow()->centralWidget()->update();

            /* Normalize 'normal' machine-window geometry: */
            if (visualStateType() == UIVisualStateType_Normal)
                machineWindow()->normalizeGeometry(true /* adjust position */);
        }

        /* Perform frame-buffer rescaling: */
        frameBuffer()->performRescale();
    }

    LogRelFlow(("UIMachineView::handleScaleChange: Complete for Screen=%d.\n",
                (unsigned long)m_uScreenId));
}

void UIMachineView::storeGuestSizeHint(const QSize &size)
{
    /* Save guest-screen size-hint: */
    LogRel(("UIMachineView::storeGuestSizeHint: "
            "Storing guest size-hint for screen %d as %dx%d\n",
            (int)screenId(), size.width(), size.height()));
    gEDataManager->setLastGuestSizeHint(m_uScreenId, size, vboxGlobal().managedVMUuid());
}

void UIMachineView::resetPausePixmap()
{
    /* Reset pixmap(s): */
    m_pausePixmap = QPixmap();
    m_pausePixmapScaled = QPixmap();
}

void UIMachineView::takePausePixmapLive()
{
    /* Prepare a screen-shot: */
    QImage screenShot = QImage(m_pFrameBuffer->width(), m_pFrameBuffer->height(), QImage::Format_RGB32);
    /* Which will be a 'black image' by default. */
    screenShot.fill(0);

    /* For separate process: */
    if (vboxGlobal().isSeparateProcess())
    {
        /* Take screen-data to array: */
        const QVector<BYTE> screenData = display().TakeScreenShotToArray(screenId(), screenShot.width(), screenShot.height(), KBitmapFormat_BGR0);
        /* And copy that data to screen-shot if it is Ok: */
        if (display().isOk() && !screenData.isEmpty())
            memcpy(screenShot.bits(), screenData.data(), screenShot.width() * screenShot.height() * 4);
    }
    /* For the same process: */
    else
    {
        /* Take the screen-shot directly: */
        display().TakeScreenShot(screenId(), screenShot.bits(), screenShot.width(), screenShot.height(), KBitmapFormat_BGR0);
    }

    /* Dim screen-shot if it is Ok: */
    if (display().isOk() && !screenShot.isNull())
        dimImage(screenShot);

    /* Finally copy the screen-shot to pause-pixmap: */
    m_pausePixmap = QPixmap::fromImage(screenShot);
#ifdef Q_WS_MAC
# ifdef VBOX_GUI_WITH_HIDPI
    /* Adjust-backing-scale-factor if necessary: */
    const double dBackingScaleFactor = frameBuffer()->backingScaleFactor();
    if (dBackingScaleFactor > 1.0 && frameBuffer()->useUnscaledHiDPIOutput())
        m_pausePixmap.setDevicePixelRatio(dBackingScaleFactor);
# endif /* VBOX_GUI_WITH_HIDPI */
#endif /* Q_WS_MAC */

    /* Update scaled pause pixmap: */
    updateScaledPausePixmap();
}

void UIMachineView::takePausePixmapSnapshot()
{
    /* Acquire the screen-data from the saved-state: */
    ULONG uWidth = 0, uHeight = 0;
    const QVector<BYTE> screenData = machine().ReadSavedScreenshotPNGToArray(0, uWidth, uHeight);

    /* Make sure there is saved-state screen-data: */
    if (screenData.isEmpty())
        return;

    /* Acquire the screen-data properties from the saved-state: */
    ULONG uGuestOriginX = 0, uGuestOriginY = 0, uGuestWidth = 0, uGuestHeight = 0;
    BOOL fEnabled = true;
    machine().QuerySavedGuestScreenInfo(m_uScreenId, uGuestOriginX, uGuestOriginY, uGuestWidth, uGuestHeight, fEnabled);

    /* Create a screen-shot on the basis of the screen-data we have in saved-state: */
    QImage screenShot = QImage::fromData(screenData.data(), screenData.size(), "PNG").scaled(uGuestWidth > 0 ? QSize(uGuestWidth, uGuestHeight) : guestSizeHint());

    /* Dim screen-shot if it is Ok: */
    if (machine().isOk() && !screenShot.isNull())
        dimImage(screenShot);

    /* Finally copy the screen-shot to pause-pixmap: */
    m_pausePixmap = QPixmap::fromImage(screenShot);
#ifdef Q_WS_MAC
# ifdef VBOX_GUI_WITH_HIDPI
    /* Adjust-backing-scale-factor if necessary: */
    const double dBackingScaleFactor = frameBuffer()->backingScaleFactor();
    if (dBackingScaleFactor > 1.0 && frameBuffer()->useUnscaledHiDPIOutput())
        m_pausePixmap.setDevicePixelRatio(dBackingScaleFactor);
# endif /* VBOX_GUI_WITH_HIDPI */
#endif /* Q_WS_MAC */

    /* Update scaled pause pixmap: */
    updateScaledPausePixmap();
}

void UIMachineView::updateScaledPausePixmap()
{
    /* Make sure pause pixmap is not null: */
    if (pausePixmap().isNull())
        return;

    /* Make sure scaled-size is not null: */
    const QSize scaledSize = frameBuffer()->scaledSize();
    if (!scaledSize.isValid())
        return;

    /* Update pause pixmap finally: */
    m_pausePixmapScaled = pausePixmap().scaled(scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
#ifdef Q_WS_MAC
# ifdef VBOX_GUI_WITH_HIDPI
    /* Adjust-backing-scale-factor if necessary: */
    const double dBackingScaleFactor = frameBuffer()->backingScaleFactor();
    if (dBackingScaleFactor > 1.0 && frameBuffer()->useUnscaledHiDPIOutput())
        m_pausePixmapScaled.setDevicePixelRatio(dBackingScaleFactor);
# endif /* VBOX_GUI_WITH_HIDPI */
#endif /* Q_WS_MAC */
}

void UIMachineView::updateSliders()
{
    /* Get current viewport size: */
    QSize curViewportSize = viewport()->size();
    /* Get maximum viewport size: */
    const QSize maxViewportSize = maximumViewportSize();
    /* Get current frame-buffer size: */
    QSize frameBufferSize = QSize(frameBuffer()->width(), frameBuffer()->height());

    /* Take the scale-factor(s) into account: */
    frameBufferSize = scaledForward(frameBufferSize);

    /* If maximum viewport size can cover whole frame-buffer => no scroll-bars required: */
    if (maxViewportSize.expandedTo(frameBufferSize) == maxViewportSize)
        curViewportSize = maxViewportSize;

    /* What length we want scroll-bars of? */
    int xRange = frameBufferSize.width()  - curViewportSize.width();
    int yRange = frameBufferSize.height() - curViewportSize.height();

#ifdef Q_WS_MAC
    /* Due to Qt 4.x doesn't supports HiDPI directly
     * we should take the backing-scale-factor into account.
     * See also viewportToContents()... */
    if (frameBuffer()->useUnscaledHiDPIOutput())
    {
        const double dBackingScaleFactor = frameBuffer()->backingScaleFactor();
        if (dBackingScaleFactor > 1.0)
        {
            xRange *= dBackingScaleFactor;
            yRange *= dBackingScaleFactor;
        }
    }
#endif /* Q_WS_MAC */

    /* Configure scroll-bars: */
    horizontalScrollBar()->setRange(0, xRange);
    verticalScrollBar()->setRange(0, yRange);
    horizontalScrollBar()->setPageStep(curViewportSize.width());
    verticalScrollBar()->setPageStep(curViewportSize.height());
}

QPoint UIMachineView::viewportToContents(const QPoint &vp) const
{
    /* Get physical contents shifts of scroll-bars: */
    int iContentsX = contentsX();
    int iContentsY = contentsY();

#ifdef Q_WS_MAC
    /* Due to Qt 4.x doesn't supports HiDPI directly
     * we should take the backing-scale-factor into account.
     * See also updateSliders()... */
    if (frameBuffer()->useUnscaledHiDPIOutput())
    {
        const double dBackingScaleFactor = frameBuffer()->backingScaleFactor();
        if (dBackingScaleFactor > 1.0)
        {
            iContentsX /= dBackingScaleFactor;
            iContentsY /= dBackingScaleFactor;
        }
    }
#endif /* Q_WS_MAC */

    /* Return point shifted according scroll-bars: */
    return QPoint(vp.x() + iContentsX, vp.y() + iContentsY);
}

void UIMachineView::scrollBy(int dx, int dy)
{
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + dx);
    verticalScrollBar()->setValue(verticalScrollBar()->value() + dy);
}

void UIMachineView::dimImage(QImage &img)
{
    for (int y = 0; y < img.height(); ++ y)
    {
        if (y % 2)
        {
            if (img.depth() == 32)
            {
                for (int x = 0; x < img.width(); ++ x)
                {
                    int gray = qGray(img.pixel (x, y)) / 2;
                    img.setPixel(x, y, qRgb (gray, gray, gray));
                }
            }
            else
            {
                ::memset(img.scanLine (y), 0, img.bytesPerLine());
            }
        }
        else
        {
            if (img.depth() == 32)
            {
                for (int x = 0; x < img.width(); ++ x)
                {
                    int gray = (2 * qGray (img.pixel (x, y))) / 3;
                    img.setPixel(x, y, qRgb (gray, gray, gray));
                }
            }
        }
    }
}

void UIMachineView::scrollContentsBy(int dx, int dy)
{
#ifdef VBOX_WITH_VIDEOHWACCEL
    if (m_pFrameBuffer)
    {
        m_pFrameBuffer->viewportScrolled(dx, dy);
    }
#endif /* VBOX_WITH_VIDEOHWACCEL */
    QAbstractScrollArea::scrollContentsBy(dx, dy);

    display().ViewportChanged(screenId(),
                              contentsX(), contentsY(),
                              visibleWidth(), visibleHeight());
}


#ifdef Q_WS_MAC
void UIMachineView::updateDockIcon()
{
    machineLogic()->updateDockIcon();
}

CGImageRef UIMachineView::vmContentImage()
{
    /* Use pause-image if exists: */
    if (!pausePixmap().isNull())
        return darwinToCGImageRef(&pausePixmap());

    /* Create the image ref out of the frame-buffer: */
    return frameBuffertoCGImageRef(m_pFrameBuffer);
}

CGImageRef UIMachineView::frameBuffertoCGImageRef(UIFrameBuffer *pFrameBuffer)
{
    CGImageRef ir = 0;
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    if (cs)
    {
        /* Create the image copy of the framebuffer */
        CGDataProviderRef dp = CGDataProviderCreateWithData(pFrameBuffer, pFrameBuffer->address(), pFrameBuffer->bitsPerPixel() / 8 * pFrameBuffer->width() * pFrameBuffer->height(), NULL);
        if (dp)
        {
            ir = CGImageCreate(pFrameBuffer->width(), pFrameBuffer->height(), 8, 32, pFrameBuffer->bytesPerLine(), cs,
                               kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host, dp, 0, false,
                               kCGRenderingIntentDefault);
            CGDataProviderRelease(dp);
        }
        CGColorSpaceRelease(cs);
    }
    return ir;
}
#endif /* Q_WS_MAC */

UIVisualStateType UIMachineView::visualStateType() const
{
    return machineLogic()->visualStateType();
}

bool UIMachineView::isFullscreenOrSeamless() const
{
    return    visualStateType() == UIVisualStateType_Fullscreen
           || visualStateType() == UIVisualStateType_Seamless;
}

bool UIMachineView::event(QEvent *pEvent)
{
    switch (pEvent->type())
    {
#ifdef Q_WS_MAC
        /* Event posted OnShowWindow: */
        case ShowWindowEventType:
        {
            /* Dunno what Qt3 thinks a window that has minimized to the dock should be - it is not hidden,
             * neither is it minimized. OTOH it is marked shown and visible, but not activated.
             * This latter isn't of much help though, since at this point nothing is marked activated.
             * I might have overlooked something, but I'm buggered what if I know what. So, I'll just always
             * show & activate the stupid window to make it get out of the dock when the user wishes to show a VM: */
            window()->show();
            window()->activateWindow();
            return true;
        }
#endif /* Q_WS_MAC */

#ifdef VBOX_WITH_VIDEOHWACCEL
        case VHWACommandProcessType:
        {
            m_pFrameBuffer->doProcessVHWACommand(pEvent);
            return true;
        }
#endif /* VBOX_WITH_VIDEOHWACCEL */

        default:
            break;
    }

    return QAbstractScrollArea::event(pEvent);
}

bool UIMachineView::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    if (pWatched == viewport())
    {
        switch (pEvent->type())
        {
            case QEvent::Resize:
            {
#ifdef VBOX_WITH_VIDEOHWACCEL
                QResizeEvent* pResizeEvent = static_cast<QResizeEvent*>(pEvent);
                if (m_pFrameBuffer)
                    m_pFrameBuffer->viewportResized(pResizeEvent);
#endif /* VBOX_WITH_VIDEOHWACCEL */
                display().ViewportChanged(screenId(),
                                          contentsX(), contentsY(),
                                          visibleWidth(), visibleHeight());
                break;
            }
            default:
                break;
        }
    }

    if (pWatched == this)
    {
        switch (pEvent->type())
        {
            case QEvent::Move:
            {
                /* In some cases viewport resize-events can provoke the
                 * machine-view position changes inside the machine-window.
                 * We have to notify interested listeners like 3D service. */
                display().ViewportChanged(screenId(),
                                          contentsX(), contentsY(),
                                          visibleWidth(), visibleHeight());
                break;
            }
            default:
                break;
        }
    }

    if (pWatched == machineWindow())
    {
        switch (pEvent->type())
        {
            case QEvent::WindowStateChange:
            {
                /* During minimizing and state restoring machineWindow() gets
                 * the focus which belongs to console view window, so returning it properly. */
                QWindowStateChangeEvent *pWindowEvent = static_cast<QWindowStateChangeEvent*>(pEvent);
                if (pWindowEvent->oldState() & Qt::WindowMinimized)
                {
                    if (QApplication::focusWidget())
                    {
                        QApplication::focusWidget()->clearFocus();
                        qApp->processEvents();
                    }
                    QTimer::singleShot(0, this, SLOT(setFocus()));
                }
                break;
            }
#ifdef Q_WS_MAC
            case QEvent::Move:
            {
                if (m_pFrameBuffer)
                {
                    /* Update backing-scale-factor for underlying frame-buffer: */
                    m_pFrameBuffer->setBackingScaleFactor(darwinBackingScaleFactor(machineWindow()));
                    /* Perform frame-buffer rescaling: */
                    m_pFrameBuffer->performRescale();
                }
                break;
            }
#endif /* Q_WS_MAC */
            default:
                break;
        }
    }

    return QAbstractScrollArea::eventFilter(pWatched, pEvent);
}

void UIMachineView::resizeEvent(QResizeEvent *pEvent)
{
    updateSliders();
    return QAbstractScrollArea::resizeEvent(pEvent);
}

void UIMachineView::moveEvent(QMoveEvent *pEvent)
{
    return QAbstractScrollArea::moveEvent(pEvent);
}

void UIMachineView::paintEvent(QPaintEvent *pPaintEvent)
{
    /* Use pause-image if exists: */
    if (!pausePixmap().isNull())
    {
        /* We have a snapshot for the paused state: */
        QRect rect = pPaintEvent->rect().intersect(viewport()->rect());
        QPainter painter(viewport());
        /* Take the scale-factor into account: */
        if (frameBuffer()->scaleFactor() == 1.0 && !frameBuffer()->scaledSize().isValid())
            painter.drawPixmap(rect.topLeft(), pausePixmap());
        else
            painter.drawPixmap(rect.topLeft(), pausePixmapScaled());
#ifdef Q_WS_MAC
        /* Update the dock icon: */
        updateDockIcon();
#endif /* Q_WS_MAC */
        return;
    }

    /* Delegate the paint function to the UIFrameBuffer interface: */
    if (m_pFrameBuffer)
        m_pFrameBuffer->handlePaintEvent(pPaintEvent);
#ifdef Q_WS_MAC
    /* Update the dock icon if we are in the running state: */
    if (uisession()->isRunning())
        updateDockIcon();
#endif /* Q_WS_MAC */
}

#ifdef VBOX_WITH_DRAG_AND_DROP
void UIMachineView::dragEnterEvent(QDragEnterEvent *pEvent)
{
    AssertPtrReturnVoid(pEvent);

    /* Get mouse-pointer location. */
    const QPoint &cpnt = viewportToContents(pEvent->pos());

    CDnDTarget dndTarget = static_cast<CDnDTarget>(guest().GetDnDTarget());

    /* Ask the target for starting a DnD event. */
    Qt::DropAction result = DnDHandler()->dragEnter(dndTarget,
                                                    screenId(),
                                                    frameBuffer()->convertHostXTo(cpnt.x()),
                                                    frameBuffer()->convertHostYTo(cpnt.y()),
                                                    pEvent->proposedAction(),
                                                    pEvent->possibleActions(),
                                                    pEvent->mimeData(), this /* pParent */);

    /* Set the DnD action returned by the guest. */
    pEvent->setDropAction(result);
    pEvent->accept();
}

void UIMachineView::dragMoveEvent(QDragMoveEvent *pEvent)
{
    AssertPtrReturnVoid(pEvent);

    /* Get mouse-pointer location. */
    const QPoint &cpnt = viewportToContents(pEvent->pos());

    CDnDTarget dndTarget = static_cast<CDnDTarget>(guest().GetDnDTarget());

    /* Ask the guest for moving the drop cursor. */
    Qt::DropAction result = DnDHandler()->dragMove(dndTarget,
                                                   screenId(),
                                                   frameBuffer()->convertHostXTo(cpnt.x()),
                                                   frameBuffer()->convertHostYTo(cpnt.y()),
                                                   pEvent->proposedAction(),
                                                   pEvent->possibleActions(),
                                                   pEvent->mimeData(), this /* pParent */);

    /* Set the DnD action returned by the guest. */
    pEvent->setDropAction(result);
    pEvent->accept();
}

void UIMachineView::dragLeaveEvent(QDragLeaveEvent *pEvent)
{
    AssertPtrReturnVoid(pEvent);

    CDnDTarget dndTarget = static_cast<CDnDTarget>(guest().GetDnDTarget());

    /* Ask the guest for stopping this DnD event. */
    DnDHandler()->dragLeave(dndTarget,
                            screenId(), this /* pParent */);
    pEvent->accept();
}

void UIMachineView::dragIsPending(void)
{
    /* At the moment we only support guest->host DnD. */
    /** @todo Add guest->guest DnD functionality here by getting
     *        the source of guest B (when copying from B to A). */
    CDnDSource dndSource = static_cast<CDnDSource>(guest().GetDnDSource());

    /* Check for a pending DnD event within the guest and if so, handle all the
     * magic. */
    DnDHandler()->dragIsPending(session(), dndSource, screenId(), this /* pParent */);
}

void UIMachineView::dropEvent(QDropEvent *pEvent)
{
    AssertPtrReturnVoid(pEvent);

    /* Get mouse-pointer location. */
    const QPoint &cpnt = viewportToContents(pEvent->pos());

    CDnDTarget dndTarget = static_cast<CDnDTarget>(guest().GetDnDTarget());

    /* Ask the guest for dropping data. */
    Qt::DropAction result = DnDHandler()->dragDrop(session(),
                                                   dndTarget,
                                                   screenId(),
                                                   frameBuffer()->convertHostXTo(cpnt.x()),
                                                   frameBuffer()->convertHostYTo(cpnt.y()),
                                                   pEvent->proposedAction(),
                                                   pEvent->possibleActions(),
                                                   pEvent->mimeData(), this /* pParent */);

    /* Set the DnD action returned by the guest. */
    pEvent->setDropAction(result);
    pEvent->accept();
}
#endif /* VBOX_WITH_DRAG_AND_DROP */

#if defined(Q_WS_WIN)

bool UIMachineView::winEvent(MSG *pMsg, long* /* piResult */)
{
    AssertPtrReturn(pMsg, false);

    /* Check if some system event should be filtered out.
     * Returning @c true means filtering-out,
     * Returning @c false means passing event to Qt. */
    bool fResult = false; /* Pass to Qt by default. */
    switch (pMsg->message)
    {
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        {
            /* Can't do COM inter-process calls from a SendMessage handler,
             * see http://support.microsoft.com/kb/131056 */
            if (vboxGlobal().isSeparateProcess() && InSendMessage())
            {
                PostMessage(pMsg->hwnd, pMsg->message, pMsg->wParam, pMsg->lParam);
                fResult = true;
                break;
            }
            /* Filter using keyboard filter? */
            bool fKeyboardFilteringResult =
                machineLogic()->keyboardHandler()->winEventFilter(pMsg, screenId());
            /* Keyboard filter rules the result? */
            fResult = fKeyboardFilteringResult;
            break;
        }
        default:
            break;
    }

    return fResult;
}

#elif defined(Q_WS_X11)

bool UIMachineView::x11Event(XEvent *pEvent)
{
    AssertPtrReturn(pEvent, false);

    /* Check if some system event should be filtered out.
     * Returning @c true means filtering-out,
     * Returning @c false means passing event to Qt. */
    bool fResult = false; /* Pass to Qt by default. */
    switch (pEvent->type)
    {
        case XFocusOut:
        case XFocusIn:
        case XKeyPress:
        case XKeyRelease:
        {
            /* Filter using keyboard-filter? */
            bool fKeyboardFilteringResult =
                machineLogic()->keyboardHandler()->x11EventFilter(pEvent, screenId());
            /* Filter using mouse-filter? */
            bool fMouseFilteringResult =
                machineLogic()->mouseHandler()->x11EventFilter(pEvent, screenId());
            /* If at least one of filters wants to filter event out then the result is true. */
            fResult = fKeyboardFilteringResult || fMouseFilteringResult;
            break;
        }
        default:
            break;
    }

    return fResult;
}

#endif /* Q_WS_X11 */

QSize UIMachineView::scaledForward(QSize size) const
{
    /* Take the scale-factor into account: */
    const double dScaleFactor = frameBuffer()->scaleFactor();
    if (dScaleFactor != 1.0)
        size = QSize(size.width() * dScaleFactor, size.height() * dScaleFactor);

#ifdef Q_WS_MAC
    /* Take the backing-scale-factor into account: */
    if (frameBuffer()->useUnscaledHiDPIOutput())
    {
        const double dBackingScaleFactor = frameBuffer()->backingScaleFactor();
        if (dBackingScaleFactor > 1.0)
            size = QSize(size.width() / dBackingScaleFactor, size.height() / dBackingScaleFactor);
    }
#endif /* Q_WS_MAC */

    /* Return result: */
    return size;
}

QSize UIMachineView::scaledBackward(QSize size) const
{
#ifdef Q_WS_MAC
    /* Take the backing-scale-factor into account: */
    if (frameBuffer()->useUnscaledHiDPIOutput())
    {
        const double dBackingScaleFactor = frameBuffer()->backingScaleFactor();
        if (dBackingScaleFactor > 1.0)
            size = QSize(size.width() * dBackingScaleFactor, size.height() * dBackingScaleFactor);
    }
#endif /* Q_WS_MAC */

    /* Take the scale-factor into account: */
    const double dScaleFactor = frameBuffer()->scaleFactor();
    if (dScaleFactor != 1.0)
        size = QSize(size.width() / dScaleFactor, size.height() / dScaleFactor);

    /* Return result: */
    return size;
}

