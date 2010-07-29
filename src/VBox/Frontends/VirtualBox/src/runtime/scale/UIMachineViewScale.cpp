/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineViewScale class implementation
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

/* Local includes */
#include "VBoxGlobal.h"
#include "UISession.h"
#include "UIMachineLogic.h"
#include "UIMachineWindow.h"
#include "UIMachineViewScale.h"
#include "UIFrameBuffer.h"
#include "UIFrameBufferQImage.h"
#ifdef VBOX_GUI_USE_QUARTZ2D
# include "UIFrameBufferQuartz2D.h"
#endif /* VBOX_GUI_USE_QUARTZ2D */

/* Global includes */
#include <QDesktopWidget>
#include <QMainWindow>
#include <QTimer>

UIMachineViewScale::UIMachineViewScale(  UIMachineWindow *pMachineWindow
                                       , ulong uScreenId
#ifdef VBOX_WITH_VIDEOHWACCEL
                                       , bool bAccelerate2DVideo
#endif
                                       )
    : UIMachineView(  pMachineWindow
                    , uScreenId
#ifdef VBOX_WITH_VIDEOHWACCEL
                    , bAccelerate2DVideo
#endif
                    )
    , m_fShouldWeDoScale(false)
{
    /* Load machine view settings: */
    loadMachineViewSettings();

    /* Prepare viewport: */
    prepareViewport();

    /* Prepare frame buffer: */
    prepareFrameBuffer();

    /* Prepare common things: */
    prepareCommon();

    /* Prepare event-filters: */
    prepareFilters();

    /* Prepare connections: */
    prepareConnections();

    /* Prepare console connections: */
    prepareConsoleConnections();

    /* Initialization: */
    sltMachineStateChanged();
}

UIMachineViewScale::~UIMachineViewScale()
{
    /* Save machine view settings: */
    saveMachineViewSettings();

    /* Cleanup frame buffer: */
    cleanupFrameBuffer();
}

void UIMachineViewScale::sltMachineStateChanged()
{
    /* Base-class processing: */
    UIMachineView::sltMachineStateChanged();

    /* Get machine state: */
    KMachineState state = uisession()->machineState();
    switch (state)
    {
        case KMachineState_Paused:
        case KMachineState_TeleportingPausedVM:
        {
            /* Scale stored pause-shot, it will be repainted on the next event-processing step: */
            m_pauseShot = m_pauseShot.scaled(frameBuffer()->scaledSize(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            break;
        }
        default:
            break;
    }
}

void UIMachineViewScale::sltPerformGuestScale()
{
    /* Check if scale is requested: */
    if (!m_fShouldWeDoScale)
        return;

    /* Set new frame-buffer scale-factor: */
    frameBuffer()->setScaledSize(viewport()->size());

    /* Update viewport: */
    viewport()->repaint();

    /* Update machine-view sliders: */
    updateSliders();

    /* Set request reflector to 'false' after scaling is done: */
    m_fShouldWeDoScale = false;
}

void UIMachineViewScale::sltDesktopResized()
{
    /* If the desktop geometry is set automatically, this will update it: */
    calculateDesktopGeometry();
}

bool UIMachineViewScale::event(QEvent *pEvent)
{
    switch (pEvent->type())
    {
        case VBoxDefs::ResizeEventType:
        {
            /* Some situations require framebuffer resize events to be ignored at all,
             * leaving machine-window, machine-view and framebuffer sizes preserved: */
            if (uisession()->isGuestResizeIgnored())
                return true;

            /* We are starting to perform machine-view resize: */
            bool oldIgnoreMainwndResize = isMachineWindowResizeIgnored();
            setMachineWindowResizeIgnored(true);

            /* Get guest resize-event: */
            UIResizeEvent *pResizeEvent = static_cast<UIResizeEvent*>(pEvent);

            /* Perform framebuffer resize: */
            frameBuffer()->setScaledSize(size());
            frameBuffer()->resizeEvent(pResizeEvent);

            /* Store the new size to prevent unwanted resize hints being sent back: */
            storeConsoleSize(pResizeEvent->width(), pResizeEvent->height());

            /* Let our toplevel widget calculate its sizeHint properly. */
#ifdef Q_WS_X11
            /* We use processEvents rather than sendPostedEvents & set the time out value to max cause on X11 otherwise
             * the layout isn't calculated correctly. Dosn't find the bug in Qt, but this could be triggered through
             * the async nature of the X11 window event system. */
            QCoreApplication::processEvents(QEventLoop::AllEvents, INT_MAX);
#else /* Q_WS_X11 */
            QCoreApplication::sendPostedEvents(0, QEvent::LayoutRequest);
#endif /* Q_WS_X11 */

#ifdef Q_WS_MAC
            machineLogic()->updateDockIconSize(screenId(), pResizeEvent->width(), pResizeEvent->height());
#endif /* Q_WS_MAC */

            /* Report to the VM thread that we finished resizing */
            session().GetConsole().GetDisplay().ResizeCompleted(screenId());

            /* We are finishing to perform machine-view resize: */
            setMachineWindowResizeIgnored(oldIgnoreMainwndResize);

            /* Make sure that all posted signals are processed: */
            qApp->processEvents();

            /* We also recalculate the desktop geometry if this is determined
             * automatically.  In fact, we only need this on the first resize,
             * but it is done every time to keep the code simpler. */
            calculateDesktopGeometry();

            /* Emit a signal about guest was resized: */
            emit resizeHintDone();

            return true;
        }

        case VBoxDefs::RepaintEventType:
        {
            UIRepaintEvent *pPaintEvent = static_cast<UIRepaintEvent*>(pEvent);
            QSize scaledSize = frameBuffer()->scaledSize();
            double xRatio = (double)scaledSize.width() / frameBuffer()->width();
            double yRatio = (double)scaledSize.height() / frameBuffer()->height();
            AssertMsg(contentsX() == 0, ("Thats can't be, else notify Dsen!\n"));
            AssertMsg(contentsY() == 0, ("Thats can't be, else notify Dsen!\n"));
            viewport()->repaint((pPaintEvent->x() - 10 /* 10 pixels left to cover xRatio*10 */) * xRatio,
                                (pPaintEvent->y() - 10 /* 10 pixels left to cover yRatio*10 */) * yRatio,
                                (pPaintEvent->width() + 10 /* 10 pixels right to cover x shift */ + 10 /* 10 pixels right to cover xRatio*10 */) * xRatio,
                                (pPaintEvent->height() + 10 /* 10 pixels right to cover y shift */ + 10 /* 10 pixels right to cover yRatio*10 */) * yRatio);

//            viewport()->repaint((int)(pPaintEvent->x()  /* 10 pixels left to cover xRatio*10 */ * xRatio),
//                                (int)(pPaintEvent->y()  /* 10 pixels left to cover yRatio*10 */ * yRatio),
//                                (int)(pPaintEvent->width()  /* 10 pixels right to cover x shift */  /* 10 pixels right to cover xRatio*10 */ * xRatio) + 2,
//                                (int)(pPaintEvent->height()  /* 10 pixels right to cover y shift */  /* 10 pixels right to cover yRatio*10 */ * yRatio) + 2);
            pEvent->accept();
            return true;
        }

        default:
            break;
    }
    return UIMachineView::event(pEvent);
}

bool UIMachineViewScale::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* Who are we watching? */
    QMainWindow *pMainDialog = machineWindowWrapper() && machineWindowWrapper()->machineWindow() ?
                               qobject_cast<QMainWindow*>(machineWindowWrapper()->machineWindow()) : 0;

    if (pWatched != 0 && pWatched == pMainDialog)
    {
        switch (pEvent->type())
        {
            case QEvent::Resize:
            {
                /* Set the "guest needs to resize" hint.
                 * This hint is acted upon when (and only when) the autoresize property is "true": */
                m_fShouldWeDoScale = true;
                QTimer::singleShot(300, this, SLOT(sltPerformGuestScale()));
                break;
            }
#if defined (Q_WS_WIN32)
# if defined (VBOX_GUI_USE_DDRAW)
            case QEvent::Move:
            {
                /* Notification from our parent that it has moved. We need this in order
                 * to possibly adjust the direct screen blitting: */
                if (frameBuffer())
                    frameBuffer()->moveEvent(static_cast<QMoveEvent*>(pEvent));
                break;
            }
# endif /* defined (VBOX_GUI_USE_DDRAW) */
#endif /* defined (Q_WS_WIN32) */
            default:
                break;
        }
    }

    return UIMachineView::eventFilter(pWatched, pEvent);
}

void UIMachineViewScale::prepareFrameBuffer()
{
    /* That method is partial copy-paste of UIMachineView::prepareFrameBuffer()
     * and its temporary here just because not all of our frame-buffers are currently supports scale-mode;
     * When all of our frame-buffers will be supporting scale-mode, method will be removed!
     * Here we are processing only these frame-buffer types, which knows scale-mode! */

    /* Prepare frame-buffer depending on render-mode: */
    switch (vboxGlobal().vmRenderMode())
    {
#ifdef VBOX_GUI_USE_QUARTZ2D
        case VBoxDefs::Quartz2DMode:
            /* Indicate that we are doing all drawing stuff ourself: */
            viewport()->setAttribute(Qt::WA_PaintOnScreen);
            m_pFrameBuffer = new UIFrameBufferQuartz2D(this);
            break;
#endif /* VBOX_GUI_USE_QUARTZ2D */
        default:
#ifdef VBOX_GUI_USE_QIMAGE
        case VBoxDefs::QImageMode:
            m_pFrameBuffer = new UIFrameBufferQImage(this);
            break;
#endif /* VBOX_GUI_USE_QIMAGE */
            AssertReleaseMsgFailed(("Scale-mode is currently NOT supporting that render-mode: %d\n", vboxGlobal().vmRenderMode()));
            LogRel(("Scale-mode is currently NOT supporting that render-mode: %d\n", vboxGlobal().vmRenderMode()));
            qApp->exit(1);
            break;
    }

    /* If frame-buffer was prepared: */
    if (m_pFrameBuffer)
    {
        /* Prepare display: */
        CDisplay display = session().GetConsole().GetDisplay();
        Assert(!display.isNull());
        m_pFrameBuffer->AddRef();
        display.SetFramebuffer(m_uScreenId, CFramebuffer(m_pFrameBuffer));
    }
}

void UIMachineViewScale::prepareConnections()
{
    connect(QApplication::desktop(), SIGNAL(resized(int)), this, SLOT(sltDesktopResized()));
}

void UIMachineViewScale::saveMachineViewSettings()
{
    /* Store guest size hint: */
    storeGuestSizeHint(QSize(frameBuffer()->width(), frameBuffer()->height()));
}

QSize UIMachineViewScale::sizeHint() const
{
    /* Base-class have its own thoughts about size-hint
     * but scale-mode needs no size-hint to be set: */
    return QSize();
}

QRect UIMachineViewScale::workingArea()
{
    return QApplication::desktop()->availableGeometry(this);
}

void UIMachineViewScale::calculateDesktopGeometry()
{
    /* This method should not get called until we have initially set up the desktop geometry type: */
    Assert((desktopGeometryType() != DesktopGeo_Invalid));
    /* If we are not doing automatic geometry calculation then there is nothing to do: */
    if (desktopGeometryType() == DesktopGeo_Automatic)
    {
        /* The area taken up by the machine window on the desktop,
         * including window frame, title, menu bar and status bar: */
        QRect windowGeo = machineWindowWrapper()->machineWindow()->frameGeometry();
        /* The area taken up by the machine central widget, so excluding all decorations: */
        QRect centralWidgetGeo = static_cast<QMainWindow*>(machineWindowWrapper()->machineWindow())->centralWidget()->geometry();
        /* To work out how big we can make the console window while still fitting on the desktop,
         * we calculate workingArea() - (windowGeo - centralWidgetGeo).
         * This works because the difference between machine window and machine central widget
         * (or at least its width and height) is a constant. */
        m_desktopGeometry = QSize(workingArea().width() - (windowGeo.width() - centralWidgetGeo.width()),
                                  workingArea().height() - (windowGeo.height() - centralWidgetGeo.height()));
    }
}

void UIMachineViewScale::updateSliders()
{
    if (horizontalScrollBarPolicy() != Qt::ScrollBarAlwaysOff)
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    if (verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff)
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

