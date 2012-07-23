/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineView class implementation
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

/* Qt includes: */
#include <QDesktopWidget>
#include <QMainWindow>
#include <QTimer>
#include <QPainter>
#include <QScrollBar>
#include <QMainWindow>
#include <VBox/VBoxVideo.h>
#include <iprt/asm.h>
#ifdef Q_WS_X11
# include <QX11Info>
#endif /* Q_WS_X11 */

/* GUI includes: */
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "UIFrameBuffer.h"
#include "UIFrameBufferQGL.h"
#include "UIFrameBufferQImage.h"
#include "UIFrameBufferQuartz2D.h"
#include "UIFrameBufferSDL.h"
#include "VBoxFBOverlay.h"
#include "UISession.h"
#include "UIKeyboardHandler.h"
#include "UIMouseHandler.h"
#include "UIMachineLogic.h"
#include "UIMachineWindow.h"
#include "UIMachineViewNormal.h"
#include "UIMachineViewFullscreen.h"
#include "UIMachineViewSeamless.h"
#include "UIMachineViewScale.h"

#ifdef VBOX_WITH_DRAG_AND_DROP
# include "UIDnDHandler.h"
#endif /* VBOX_WITH_DRAG_AND_DROP */

/* COM includes: */
#include "CSession.h"
#include "CConsole.h"
#include "CDisplay.h"
#include "CFramebuffer.h"
#ifdef VBOX_WITH_DRAG_AND_DROP
# include "CGuest.h"
#endif /* VBOX_WITH_DRAG_AND_DROP */

/* Other VBox includes: */
#ifdef Q_WS_X11
# include <X11/XKBlib.h>
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

class UIViewport: public QWidget
{
public:

    UIViewport(QWidget *pParent) : QWidget(pParent)
    {
        /* No need for background drawing: */
        setAttribute(Qt::WA_OpaquePaintEvent);
    }

    QPaintEngine *paintEngine() const
    {
        if (testAttribute(Qt::WA_PaintOnScreen))
            return NULL;
        else
            return QWidget::paintEngine();
    }
};

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
    return pMachineView;
}

void UIMachineView::destroy(UIMachineView *pMachineView)
{
    delete pMachineView;
}

double UIMachineView::aspectRatio() const
{
    return frameBuffer() ? (double)(frameBuffer()->width()) / frameBuffer()->height() : 0;
}

void UIMachineView::sltPerformGuestResize(const QSize &toSize)
{
    /* Get the current machine: */
    CMachine machine = session().GetMachine();

    /* If this slot is invoked directly then use the passed size otherwise get
     * the available size for the guest display. We assume here that centralWidget()
     * contains this view only and gives it all available space: */
    QSize newSize(toSize.isValid() ? toSize : machineWindow()->centralWidget()->size());
    AssertMsg(newSize.isValid(), ("Size should be valid!\n"));

    /* Send new size-hint to the guest: */
    session().GetConsole().GetDisplay().SetVideoModeHint(screenId(), true, false, 0, 0, newSize.width(), newSize.height(), 0);
    /* And track whether we have had a "normal" resize since the last
     * fullscreen resize hint was sent: */
    QString strKey = makeExtraDataKeyPerMonitor(GUI_LastGuestSizeHintWasFullscreen);
    machine.SetExtraData(strKey, isFullscreenOrSeamless() ? "true" : "");
}

void UIMachineView::sltDesktopResized()
{
    setMaxGuestSize();
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
            if (   vboxGlobal().vmRenderMode() != TimerMode
                && m_pFrameBuffer
                &&
                (   state           != KMachineState_TeleportingPausedVM
                 || m_previousState != KMachineState_Teleporting))
            {
                takePauseShotLive();
                /* Fully repaint to pick up m_pauseShot: */
                viewport()->update();
            }
            break;
        }
        case KMachineState_Restoring:
        {
            /* Only works with the primary screen currently. */
            if (screenId() == 0)
            {
                takePauseShotSnapshot();
                /* Fully repaint to pick up m_pauseShot: */
                viewport()->update();
            }
            break;
        }
        case KMachineState_Running:
        {
            if (   m_previousState == KMachineState_Paused
                || m_previousState == KMachineState_TeleportingPausedVM
                || m_previousState == KMachineState_Restoring)
            {
                if (vboxGlobal().vmRenderMode() != TimerMode && m_pFrameBuffer)
                {
                    /* Reset the pixmap to free memory: */
                    resetPauseShot();
                    /* Ask for full guest display update (it will also update
                     * the viewport through IFramebuffer::NotifyUpdate): */
                    CDisplay dsp = session().GetConsole().GetDisplay();
                    dsp.InvalidateAndUpdate();
                }
            }
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
    : QAbstractScrollArea(pMachineWindow)
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
}

UIMachineView::~UIMachineView()
{
}

void UIMachineView::prepareViewport()
{
    /* Prepare viewport: */
#ifdef VBOX_GUI_USE_QGLFB
    QWidget *pViewport = 0;
    switch (vboxGlobal().vmRenderMode())
    {
        case QGLMode:
            pViewport = new VBoxGLWidget(session().GetConsole(), this, NULL);
            break;
        default:
            pViewport = new UIViewport(this);
    }
#else /* VBOX_GUI_USE_QGLFB */
    UIViewport *pViewport = new UIViewport(this);
#endif /* !VBOX_GUI_USE_QGLFB */
    setViewport(pViewport);
}

void UIMachineView::prepareFrameBuffer()
{
    /* Prepare frame-buffer depending on render-mode: */
    switch (getRenderMode())
    {
#ifdef VBOX_GUI_USE_QIMAGE
        case QImageMode:
        {
            UIFrameBuffer* pFrameBuffer = uisession()->frameBuffer(screenId());
            if (pFrameBuffer)
                pFrameBuffer->setView(this);
            else
            {
# ifdef VBOX_WITH_VIDEOHWACCEL
                if (m_fAccelerate2DVideo)
                {
                    /** these two additional template args is a workaround to
                     * this [VBox|UI] duplication
                     * @todo: they are to be removed once VBox stuff is gone */
                    pFrameBuffer = new VBoxOverlayFrameBuffer<UIFrameBufferQImage, UIMachineView, UIResizeEvent>(this, &session(), (uint32_t)screenId());
                }
                else
                    pFrameBuffer = new UIFrameBufferQImage(this);
# else /* VBOX_WITH_VIDEOHWACCEL */
                pFrameBuffer = new UIFrameBufferQImage(this);
# endif /* !VBOX_WITH_VIDEOHWACCEL */
                uisession()->setFrameBuffer(screenId(), pFrameBuffer);
            }
            m_pFrameBuffer = pFrameBuffer;
            break;
        }
#endif /* VBOX_GUI_USE_QIMAGE */
#ifdef VBOX_GUI_USE_QGLFB
        case QGLMode:
            m_pFrameBuffer = new UIFrameBufferQGL(this);
            break;
//        case QGLOverlayMode:
//            m_pFrameBuffer = new UIQGLOverlayFrameBuffer(this);
//            break;
#endif /* VBOX_GUI_USE_QGLFB */
#ifdef VBOX_GUI_USE_SDL
        case SDLMode:
        {
            /* Indicate that we are doing all drawing stuff ourself: */
            // TODO_NEW_CORE
            viewport()->setAttribute(Qt::WA_PaintOnScreen);
# ifdef Q_WS_X11
            /* This is somehow necessary to prevent strange X11 warnings on i386 and segfaults on x86_64: */
            XFlush(QX11Info::display());
# endif /* Q_WS_X11 */
            UIFrameBuffer* pFrameBuffer = uisession()->frameBuffer(screenId());
            if (pFrameBuffer)
                pFrameBuffer->setView(this);
            else
            {
# if defined(VBOX_WITH_VIDEOHWACCEL) && defined(DEBUG_misha) /* not tested yet */
                if (m_fAccelerate2DVideo)
                {
                    /** these two additional template args is a workaround to
                     * this [VBox|UI] duplication
                     * @todo: they are to be removed once VBox stuff is gone */
                    pFrameBuffer = new VBoxOverlayFrameBuffer<UIFrameBufferSDL, UIMachineView, UIResizeEvent>(this, &session(), (uint32_t)screenId());
                }
                else
                    pFrameBuffer = new UIFrameBufferSDL(this);
# else /* VBOX_WITH_VIDEOHWACCEL */
                pFrameBuffer = new UIFrameBufferSDL(this);
# endif /* !VBOX_WITH_VIDEOHWACCEL */
                uisession()->setFrameBuffer(screenId(), pFrameBuffer);
            }
            m_pFrameBuffer = pFrameBuffer;
            /* Disable scrollbars because we cannot correctly draw in a scrolled window using SDL: */
            horizontalScrollBar()->setEnabled(false);
            verticalScrollBar()->setEnabled(false);
            break;
        }
#endif /* VBOX_GUI_USE_SDL */
#if 0 // TODO: Enable DDraw frame buffer!
#ifdef VBOX_GUI_USE_DDRAW
        case DDRAWMode:
            m_pFrameBuffer = new UIDDRAWFrameBuffer(this);
            if (!m_pFrameBuffer || m_pFrameBuffer->address() == NULL)
            {
                if (m_pFrameBuffer)
                    delete m_pFrameBuffer;
                m_mode = QImageMode;
                m_pFrameBuffer = new UIFrameBufferQImage(this);
            }
            break;
#endif /* VBOX_GUI_USE_DDRAW */
#endif
#ifdef VBOX_GUI_USE_QUARTZ2D
        case Quartz2DMode:
        {
            /* Indicate that we are doing all drawing stuff ourself: */
            viewport()->setAttribute(Qt::WA_PaintOnScreen);
            UIFrameBuffer* pFrameBuffer = uisession()->frameBuffer(screenId());
            if (pFrameBuffer)
                pFrameBuffer->setView(this);
            else
            {
# ifdef VBOX_WITH_VIDEOHWACCEL
                if (m_fAccelerate2DVideo)
                {
                    /** these two additional template args is a workaround to
                     * this [VBox|UI] duplication
                     * @todo: they are to be removed once VBox stuff is gone */
                    pFrameBuffer = new VBoxOverlayFrameBuffer<UIFrameBufferQuartz2D, UIMachineView, UIResizeEvent>(this, &session(), (uint32_t)screenId());
                }
                else
                    pFrameBuffer = new UIFrameBufferQuartz2D(this);
# else /* VBOX_WITH_VIDEOHWACCEL */
                pFrameBuffer = new UIFrameBufferQuartz2D(this);
# endif /* !VBOX_WITH_VIDEOHWACCEL */
                uisession()->setFrameBuffer(screenId(), pFrameBuffer);
            }
            m_pFrameBuffer = pFrameBuffer;
            break;
        }
#endif /* VBOX_GUI_USE_QUARTZ2D */
        default:
            AssertReleaseMsgFailed(("Render mode must be valid: %d\n", vboxGlobal().vmRenderMode()));
            LogRel(("Invalid render mode: %d\n", vboxGlobal().vmRenderMode()));
            qApp->exit(1);
            break;
    }

    /* If frame-buffer was prepared: */
    if (m_pFrameBuffer)
    {
        /* Prepare display: */
        CDisplay display = session().GetConsole().GetDisplay();
        Assert(!display.isNull());
        CFramebuffer fb(NULL);
        LONG XOrigin, YOrigin;
        /* Check if the framebuffer is already assigned;
         * in this case we do not need to re-assign it neither do we need to AddRef. */
        display.GetFramebuffer(m_uScreenId, fb, XOrigin, YOrigin);
        if (fb.raw() != m_pFrameBuffer) /* <-this will evaluate to true iff no framebuffer is yet assigned */
        {
            m_pFrameBuffer->AddRef();
        }
        /* Always perform SetFramebuffer to ensure 3D gets notified: */
        display.SetFramebuffer(m_uScreenId, CFramebuffer(m_pFrameBuffer));
    }

    QSize size;
#ifdef Q_WS_X11
    /* Processing pseudo resize-event to synchronize frame-buffer with stored
     * framebuffer size. On X11 this will be additional done when the machine
     * state was 'saved'. */
    if (session().GetMachine().GetState() == KMachineState_Saved)
        size = guestSizeHint();
#endif /* Q_WS_X11 */
    /* If there is a preview image saved, we will resize the framebuffer to the
     * size of that image. */
    ULONG buffer = 0, width = 0, height = 0;
    CMachine machine = session().GetMachine();
    machine.QuerySavedScreenshotPNGSize(0, buffer, width, height);
    if (buffer > 0)
    {
        /* Init with the screenshot size */
        size = QSize(width, height);
        /* Try to get the real guest dimensions from the save state */
        ULONG guestOriginX = 0, guestOriginY = 0, guestWidth = 0, guestHeight = 0;
        BOOL fEnabled = true;
        machine.QuerySavedGuestScreenInfo(0, guestOriginX, guestOriginY, guestWidth, guestHeight, fEnabled);
        if (   guestWidth  > 0
            && guestHeight > 0)
            size = QSize(guestWidth, guestHeight);
    }
    /* If we have a valid size, resize the framebuffer. */
    if (   size.width() > 0
        && size.height() > 0)
    {
        UIResizeEvent event(FramebufferPixelFormat_Opaque, NULL, 0, 0, size.width(), size.height());
        frameBuffer()->resizeEvent(&event);
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
    if (m_pFrameBuffer)
    {
        /* Process pending frame-buffer resize events: */
        QApplication::sendPostedEvents(this, ResizeEventType);
        if (   0
#ifdef VBOX_GUI_USE_QIMAGE
            || vboxGlobal().vmRenderMode() == QImageMode
#endif
#ifdef VBOX_GUI_USE_SDL
            || vboxGlobal().vmRenderMode() == SDLMode
#endif
#ifdef VBOX_GUI_USE_QUARTZ2D
            || vboxGlobal().vmRenderMode() == Quartz2DMode
#endif
#ifdef VBOX_WITH_VIDEOHWACCEL
            || m_fAccelerate2DVideo
#endif
           )
        {
            Assert(m_pFrameBuffer == uisession()->frameBuffer(screenId()));
            CDisplay display = session().GetConsole().GetDisplay();
            /* Temporarily remove the framebuffer in Display while unsetting
             * the view in order to respect the thread synchonisation logic
             * (see UIFrameBuffer.h). */
            /** @note VBOX_WITH_CROGL additionally requires us to call
             * SetFramebuffer to ensure 3D gets notified of view being
             * destroyed */
            display.SetFramebuffer(m_uScreenId, CFramebuffer(NULL));
            m_pFrameBuffer->setView(NULL);
            display.SetFramebuffer(m_uScreenId, CFramebuffer(m_pFrameBuffer));
        }
        else
        {
            /* Warn framebuffer about its no more necessary: */
            m_pFrameBuffer->setDeleted(true);
            /* Detach framebuffer from Display: */
            CDisplay display = session().GetConsole().GetDisplay();
            display.SetFramebuffer(m_uScreenId, CFramebuffer(NULL));
            /* Release the reference: */
            m_pFrameBuffer->Release();
//          delete m_pFrameBuffer; // TODO_NEW_CORE: possibly necessary to really cleanup
            m_pFrameBuffer = NULL;
        }
    }
}

UIMachineLogic* UIMachineView::machineLogic() const
{
    return machineWindow()->machineLogic();
}

UISession* UIMachineView::uisession() const
{
    return machineLogic()->uisession();
}

CSession& UIMachineView::session()
{
    return uisession()->session();
}

QSize UIMachineView::sizeHint() const
{
    if (m_sizeHintOverride.isValid())
        return m_sizeHintOverride;
#ifdef VBOX_WITH_DEBUGGER
    // TODO: Fix all DEBUGGER stuff!
    /* HACK ALERT! Really ugly workaround for the resizing to 9x1 done by DevVGA if provoked before power on. */
    QSize fb(m_pFrameBuffer->width(), m_pFrameBuffer->height());
    if (fb.width() < 16 || fb.height() < 16)
    {
        CMachine machine = uisession()->session().GetMachine();
        if (   vboxGlobal().isStartPausedEnabled()
            || vboxGlobal().isDebuggerAutoShowEnabled(machine))
        fb = QSize(640, 480);
    }
    return QSize(fb.width() + frameWidth() * 2, fb.height() + frameWidth() * 2);
#else /* VBOX_WITH_DEBUGGER */
    return QSize(m_pFrameBuffer->width() + frameWidth() * 2, m_pFrameBuffer->height() + frameWidth() * 2);
#endif /* !VBOX_WITH_DEBUGGER */
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

void UIMachineView::setMaxGuestSize()
{
    QSize maxSize;
    switch (m_maxGuestSizePolicy)
    {
        case MaxGuestSizePolicy_Fixed:
            maxSize = m_fixedMaxGuestSize;
            break;
        case MaxGuestSizePolicy_Automatic:
            maxSize = calculateMaxGuestSize();
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
    /* Result: */
    QSize sizeHint;

    /* Get current machine: */
    CMachine machine = session().GetMachine();

    /* Load machine view hint: */
    QString strKey = makeExtraDataKeyPerMonitor(GUI_LastGuestSizeHint);
    QString strValue = machine.GetExtraData(strKey);

    bool ok = true;
    int width = 0, height = 0;
    if (ok)
        width = strValue.section(',', 0, 0).toInt(&ok);
    if (ok)
        height = strValue.section(',', 1, 1).toInt(&ok);

    if (ok /* If previous parameters were read correctly! */)
    {
        /* Compose guest size hint from loaded values: */
        sizeHint = QSize(width, height);
    }
    else
    {
        /* Compose guest size hint from default attributes: */
        sizeHint = QSize(800, 600);
    }

    /* Return result: */
    return sizeHint;
}

void UIMachineView::storeGuestSizeHint(const QSize &sizeHint)
{
    /* Get current machine: */
    CMachine machine = session().GetMachine();

    /* Save machine view hint: */
    QString strKey = makeExtraDataKeyPerMonitor(GUI_LastGuestSizeHint);
    QString strValue = QString("%1,%2").arg(sizeHint.width()).arg(sizeHint.height());
    machine.SetExtraData(strKey, strValue);
}

void UIMachineView::takePauseShotLive()
{
    /* Take a screen snapshot. Note that TakeScreenShot() always needs a 32bpp image: */
    QImage shot = QImage(m_pFrameBuffer->width(), m_pFrameBuffer->height(), QImage::Format_RGB32);
    /* If TakeScreenShot fails or returns no image, just show a black image. */
    shot.fill(0);
    CDisplay dsp = session().GetConsole().GetDisplay();
    dsp.TakeScreenShot(screenId(), shot.bits(), shot.width(), shot.height());
    /* TakeScreenShot() may fail if, e.g. the Paused notification was delivered
     * after the machine execution was resumed. It's not fatal: */
    if (dsp.isOk())
        dimImage(shot);
    m_pauseShot = QPixmap::fromImage(shot);
}

void UIMachineView::takePauseShotSnapshot()
{
    CMachine machine = session().GetMachine();
    ULONG width = 0, height = 0;
    QVector<BYTE> screenData = machine.ReadSavedScreenshotPNGToArray(0, width, height);
    if (screenData.size() != 0)
    {
        ULONG guestOriginX = 0, guestOriginY = 0, guestWidth = 0, guestHeight = 0;
        BOOL fEnabled = true;
        machine.QuerySavedGuestScreenInfo(0, guestOriginX, guestOriginY, guestWidth, guestHeight, fEnabled);
        QImage shot = QImage::fromData(screenData.data(), screenData.size(), "PNG").scaled(guestWidth > 0 ? QSize(guestWidth, guestHeight) : guestSizeHint());
        dimImage(shot);
        m_pauseShot = QPixmap::fromImage(shot);
    }
}

void UIMachineView::updateSliders()
{
    QSize p = viewport()->size();
    QSize m = maximumViewportSize();

    QSize v = QSize(frameBuffer()->width(), frameBuffer()->height());
    /* No scroll bars needed: */
    if (m.expandedTo(v) == m)
        p = m;

    horizontalScrollBar()->setRange(0, v.width() - p.width());
    verticalScrollBar()->setRange(0, v.height() - p.height());
    horizontalScrollBar()->setPageStep(p.width());
    verticalScrollBar()->setPageStep(p.height());
}

QPoint UIMachineView::viewportToContents(const QPoint &vp) const
{
    return QPoint(vp.x() + contentsX(), vp.y() + contentsY());
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

    session().GetConsole().GetDisplay().ViewportChanged(screenId(),
                            contentsX(),
                            contentsY(),
                            contentsWidth(),
                            contentsHeight());
}


#ifdef Q_WS_MAC
void UIMachineView::updateDockIcon()
{
    machineLogic()->updateDockIcon();
}

CGImageRef UIMachineView::vmContentImage()
{
    if (!m_pauseShot.isNull())
    {
        CGImageRef pauseImg = ::darwinToCGImageRef(&m_pauseShot);
        /* Use the pause image as background */
        return pauseImg;
    }
    else
    {
# ifdef VBOX_GUI_USE_QUARTZ2D
        if (vboxGlobal().vmRenderMode() == Quartz2DMode)
        {
            /* If the render mode is Quartz2D we could use the CGImageRef
             * of the framebuffer for the dock icon creation. This saves
             * some conversion time. */
            CGImageRef image = static_cast<UIFrameBufferQuartz2D*>(m_pFrameBuffer)->imageRef();
            CGImageRetain(image); /* Retain it, cause the consumer will release it. */
            return image;
        }
        else
# endif /* VBOX_GUI_USE_QUARTZ2D */
            /* In image mode we have to create the image ref out of the
             * framebuffer */
            return frameBuffertoCGImageRef(m_pFrameBuffer);
    }
    return 0;
}

CGImageRef UIMachineView::frameBuffertoCGImageRef(UIFrameBuffer *pFrameBuffer)
{
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    Assert(cs);
    /* Create the image copy of the framebuffer */
    CGDataProviderRef dp = CGDataProviderCreateWithData(pFrameBuffer, pFrameBuffer->address(), pFrameBuffer->bitsPerPixel() / 8 * pFrameBuffer->width() * pFrameBuffer->height(), NULL);
    Assert(dp);
    CGImageRef ir = CGImageCreate(pFrameBuffer->width(), pFrameBuffer->height(), 8, 32, pFrameBuffer->bytesPerLine(), cs,
                                  kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host, dp, 0, false,
                                  kCGRenderingIntentDefault);
    Assert(ir);
    CGDataProviderRelease(dp);
    CGColorSpaceRelease(cs);

    return ir;
}
#endif /* Q_WS_MAC */

bool UIMachineView::guestResizeEvent(QEvent *pEvent,
                                     bool fFullscreenOrSeamless)
{
    /* Some situations require framebuffer resize events to be ignored at all,
     * leaving machine-window, machine-view and framebuffer sizes preserved: */
    if (uisession()->isGuestResizeIgnored())
        return true;

    /* Get guest resize-event: */
    UIResizeEvent *pResizeEvent = static_cast<UIResizeEvent*>(pEvent);

    /** If only the pitch has changed (or nothing at all!) we only update the
     * framebuffer and don't touch the window.  This prevents unwanted resizes
     * when entering or exiting fullscreen on X.Org guests and when
     * re-attaching the framebuffer on a view switch. */
    bool fResize =    pResizeEvent->width() != frameBuffer()->width()
                   || pResizeEvent->height() != frameBuffer()->height();

    /* Perform framebuffer resize: */
    frameBuffer()->resizeEvent(pResizeEvent);

    if (fResize)
    {
        /* Reapply maximum size restriction for machine-view: */
        setMaximumSize(sizeHint());

        /* Disable the resize hint override hack: */
        m_sizeHintOverride = QSize(-1, -1);

        /* Perform machine-view resize: */
        resize(pResizeEvent->width(), pResizeEvent->height());

        /* May be we have to restrict minimum size? */
        maybeRestrictMinimumSize();

        /* Let our toplevel widget calculate its sizeHint properly: */
        QCoreApplication::sendPostedEvents(0, QEvent::LayoutRequest);

#ifdef Q_WS_MAC
        machineLogic()->updateDockIconSize(screenId(), pResizeEvent->width(), pResizeEvent->height());
#endif /* Q_WS_MAC */

        /* Update machine-view sliders: */
        updateSliders();

        /* Normalize machine-window geometry: */
        if (!fFullscreenOrSeamless)
            normalizeGeometry(true /* Adjust Position? */);
    }

    /* Report to the VM thread that we finished resizing: */
    session().GetConsole().GetDisplay().ResizeCompleted(screenId());

    /* Emit a signal about guest was resized: */
    emit resizeHintDone();

    pEvent->accept();
    return true;
}

UIVisualStateType UIMachineView::visualStateType() const
{
    return machineLogic()->visualStateType();
}

bool UIMachineView::isFullscreenOrSeamless() const
{
    return    visualStateType() == UIVisualStateType_Fullscreen
           || visualStateType() == UIVisualStateType_Seamless;
}

QString UIMachineView::makeExtraDataKeyPerMonitor(QString base) const
{
    return m_uScreenId == 0 ? QString("%1").arg(base)
                            : QString("%1%2").arg(base).arg(m_uScreenId);
}

RenderMode UIMachineView::getRenderMode() const
{
    if (visualStateType() != UIVisualStateType_Scale)
        return vboxGlobal().vmRenderMode();
    /* This part of the method is temporary since not all of our framebuffer
     * modes currently support scale view mode.  Once they do it will be
     * removed. */
    /** @note this could have been a mini-class which would be easier to
     * unit test. */
    /* Prepare frame-buffer depending on render-mode: */
    switch (vboxGlobal().vmRenderMode())
    {
#ifdef VBOX_GUI_USE_QUARTZ2D
        case Quartz2DMode:
            return Quartz2DMode;
#endif /* VBOX_GUI_USE_QUARTZ2D */
        default:
#ifdef VBOX_GUI_USE_QIMAGE
        case QImageMode:
            return QImageMode;
#endif /* VBOX_GUI_USE_QIMAGE */
        break;
    }
    AssertReleaseMsgFailed(("Scale-mode currently does NOT supporting render-mode %d\n", vboxGlobal().vmRenderMode()));
    qApp->exit(1);
}

bool UIMachineView::event(QEvent *pEvent)
{
    switch (pEvent->type())
    {
        case RepaintEventType:
        {
            UIRepaintEvent *pPaintEvent = static_cast<UIRepaintEvent*>(pEvent);
            viewport()->update(pPaintEvent->x() - contentsX(), pPaintEvent->y() - contentsY(),
                                pPaintEvent->width(), pPaintEvent->height());
            return true;
        }

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
                session().GetConsole().GetDisplay().ViewportChanged(screenId(),
                        contentsX(),
                        contentsY(),
                        contentsWidth(),
                        contentsHeight());
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
            default:
                break;
        }
    }

    return QAbstractScrollArea::eventFilter(pWatched, pEvent);
}

void UIMachineView::resizeEvent(QResizeEvent *pEvent)
{
    updateSliders();
    /* We call this on every resize as on X11 it sets information which becomes
     * available asynchronously at an unknown time after window creation.  As
     * long as the information is not available we make a best guess. */
    setMaxGuestSize();
    return QAbstractScrollArea::resizeEvent(pEvent);
}

void UIMachineView::moveEvent(QMoveEvent *pEvent)
{
    return QAbstractScrollArea::moveEvent(pEvent);
}

void UIMachineView::paintEvent(QPaintEvent *pPaintEvent)
{
    if (m_pauseShot.isNull())
    {
        /* Delegate the paint function to the VBoxFrameBuffer interface: */
        if (m_pFrameBuffer && !uisession()->isTurnedOff())
            m_pFrameBuffer->paintEvent(pPaintEvent);
#ifdef Q_WS_MAC
        /* Update the dock icon if we are in the running state */
        if (uisession()->isRunning())
            updateDockIcon();
#endif /* Q_WS_MAC */
        return;
    }

#ifdef VBOX_GUI_USE_QUARTZ2D
    if (vboxGlobal().vmRenderMode() == Quartz2DMode && m_pFrameBuffer)
    {
        m_pFrameBuffer->paintEvent(pPaintEvent);
        updateDockIcon();
    }
    else
#endif /* VBOX_GUI_USE_QUARTZ2D */
    {
        /* We have a snapshot for the paused state: */
        QRect r = pPaintEvent->rect().intersect(viewport()->rect());
        /* We have to disable paint on screen if we are using the regular painter: */
        bool paintOnScreen = viewport()->testAttribute(Qt::WA_PaintOnScreen);
        viewport()->setAttribute(Qt::WA_PaintOnScreen, false);
        QPainter pnt(viewport());
        pnt.drawPixmap(r, m_pauseShot, QRect(r.x() + contentsX(), r.y() + contentsY(), r.width(), r.height()));
        /* Restore the attribute to its previous state: */
        viewport()->setAttribute(Qt::WA_PaintOnScreen, paintOnScreen);
#ifdef Q_WS_MAC
        updateDockIcon();
#endif /* Q_WS_MAC */
    }
}

#ifdef VBOX_WITH_DRAG_AND_DROP

void UIMachineView::dragEnterEvent(QDragEnterEvent *pEvent)
{
    /* The guest object to talk to. */
    CGuest guest = session().GetConsole().GetGuest();

    /* Get mouse-pointer location */
    const QPoint &cpnt = viewportToContents(pEvent->pos());

    /* Ask the guest for starting a DnD event. */
    Qt::DropAction result = gDnD->dragHGEnter(guest,
                                              screenId(),
                                              frameBuffer()->convertHostXTo(cpnt.x()),
                                              frameBuffer()->convertHostYTo(cpnt.y()),
                                              pEvent->proposedAction(),
                                              pEvent->possibleActions(),
                                              pEvent->mimeData(), this);

    /* Set the DnD action returned by the guest. */
    pEvent->setDropAction(result);
    pEvent->accept();
}

void UIMachineView::dragMoveEvent(QDragMoveEvent *pEvent)
{
    /* The guest object to talk to. */
    CGuest guest = session().GetConsole().GetGuest();

    /* Get mouse-pointer location */
    const QPoint &cpnt = viewportToContents(pEvent->pos());

    /* Ask the guest for moving the drop cursor. */
    Qt::DropAction result = gDnD->dragHGMove(guest,
                                             screenId(),
                                             frameBuffer()->convertHostXTo(cpnt.x()),
                                             frameBuffer()->convertHostYTo(cpnt.y()),
                                             pEvent->proposedAction(),
                                             pEvent->possibleActions(),
                                             pEvent->mimeData(), this);

    /* Set the DnD action returned by the guest. */
    pEvent->setDropAction(result);
    pEvent->accept();
}

void UIMachineView::dragLeaveEvent(QDragLeaveEvent *pEvent)
{
    /* The guest object to talk to. */
    CGuest guest = session().GetConsole().GetGuest();

    /* Ask the guest for stopping this DnD event. */
    gDnD->dragHGLeave(guest, screenId(), this);
    pEvent->accept();
}

void UIMachineView::dropEvent(QDropEvent *pEvent)
{
    /* The guest object to talk to. */
    CGuest guest = session().GetConsole().GetGuest();

    /* Get mouse-pointer location */
    const QPoint &cpnt = viewportToContents(pEvent->pos());

    /* Ask the guest for dropping data. */
    Qt::DropAction result = gDnD->dragHGDrop(guest,
                                             screenId(),
                                             frameBuffer()->convertHostXTo(cpnt.x()),
                                             frameBuffer()->convertHostYTo(cpnt.y()),
                                             pEvent->proposedAction(),
                                             pEvent->possibleActions(),
                                             pEvent->mimeData(), this);

    /* Set the DnD action returned by the guest. */
    pEvent->setDropAction(result);
    pEvent->accept();
}

void UIMachineView::handleGHDnd()
{
    /* The guest object to talk to. */
    CGuest guest = session().GetConsole().GetGuest();

    /* Check for a pending DnD event within the guest and if so, handle all the
     * magic. */
    gDnD->dragGHPending(session(), screenId(), this);
}

#endif /* VBOX_WITH_DRAG_AND_DROP */

#if defined(Q_WS_WIN)

bool UIMachineView::winEvent(MSG *pMsg, long* /* piResult */)
{
    /* Check if some system event should be filtered-out.
     * Returning 'true' means filtering-out,
     * Returning 'false' means passing event to Qt. */
    bool fResult = false; /* Pass to Qt by default: */
    switch (pMsg->message)
    {
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        {
            /* Filter using keyboard-filter: */
            bool fKeyboardFilteringResult = machineLogic()->keyboardHandler()->winEventFilter(pMsg, screenId());
            /* Keyboard filter rules the result: */
            fResult = fKeyboardFilteringResult;
            break;
        }
        default:
            break;
    }
    /* Return result: */
    return fResult;
}

#elif defined(Q_WS_X11)

bool UIMachineView::x11Event(XEvent *pEvent)
{
    /* Check if some system event should be filtered-out.
     * Returning 'true' means filtering-out,
     * Returning 'false' means passing event to Qt. */
    bool fResult = false; /* Pass to Qt by default: */
    switch (pEvent->type)
    {
        case XFocusOut:
        case XFocusIn:
        case XKeyPress:
        case XKeyRelease:
        {
            /* Filter using keyboard-filter: */
            bool fKeyboardFilteringResult = machineLogic()->keyboardHandler()->x11EventFilter(pEvent, screenId());
            /* Filter using mouse-filter: */
            bool fMouseFilteringResult = machineLogic()->mouseHandler()->x11EventFilter(pEvent, screenId());
            /* If at least one of filters wants to filter event out then the result is 'true': */
            fResult = fKeyboardFilteringResult || fMouseFilteringResult;
            break;
        }
        default:
            break;
    }
    /* Return result: */
    return fResult;
}

#endif

