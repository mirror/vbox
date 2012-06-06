/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineViewScale class implementation
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

/* GUI includes */
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

/* COM includes: */
#include "CConsole.h"
#include "CDisplay.h"

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
    , m_pPauseImage(0)
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

    /* Prepare console connections: */
    prepareConsoleConnections();

    /* Initialization: */
    sltMachineStateChanged();
}

UIMachineViewScale::~UIMachineViewScale()
{
    /* Save machine view settings: */
    saveMachineViewSettings();

    /* Disable scaling: */
    frameBuffer()->setScaledSize(QSize());

    /* Cleanup frame buffer: */
    cleanupFrameBuffer();
}

void UIMachineViewScale::takePauseShotLive()
{
    /* Take a screen snapshot. Note that TakeScreenShot() always needs a 32bpp image: */
    QImage shot = QImage(m_pFrameBuffer->width(), m_pFrameBuffer->height(), QImage::Format_RGB32);
    /* If TakeScreenShot fails or returns no image, just show a black image. */
    shot.fill(0);
    CDisplay dsp = session().GetConsole().GetDisplay();
    dsp.TakeScreenShot(screenId(), shot.bits(), shot.width(), shot.height());
    m_pPauseImage = new QImage(shot);
    scalePauseShot();
}

void UIMachineViewScale::takePauseShotSnapshot()
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
        m_pPauseImage = new QImage(shot);
        scalePauseShot();
    }
}

void UIMachineViewScale::resetPauseShot()
{
    /* Call the base class */
    UIMachineView::resetPauseShot();

    if (m_pPauseImage)
    {
        delete m_pPauseImage;
        m_pPauseImage = 0;
    }
}

void UIMachineViewScale::scalePauseShot()
{
    if (m_pPauseImage)
    {
        QSize scaledSize = frameBuffer()->scaledSize();
        if (scaledSize.isValid())
        {
            QImage tmpImg = m_pPauseImage->scaled(scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            dimImage(tmpImg);
            m_pauseShot = QPixmap::fromImage(tmpImg);
        }
    }
}

void UIMachineViewScale::sltPerformGuestScale()
{
    /* Check if scale is requested: */
    /* Set new frame-buffer scale-factor: */
    frameBuffer()->setScaledSize(viewport()->size());

    /* Scale the pause image if necessary */
    scalePauseShot();

    /* Update viewport: */
    viewport()->repaint();

    /* Update machine-view sliders: */
    updateSliders();
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

            /* Get guest resize-event: */
            UIResizeEvent *pResizeEvent = static_cast<UIResizeEvent*>(pEvent);

            /* Perform framebuffer resize: */
            frameBuffer()->setScaledSize(size());
            frameBuffer()->resizeEvent(pResizeEvent);

            /* Let our toplevel widget calculate its sizeHint properly: */
            QCoreApplication::sendPostedEvents(0, QEvent::LayoutRequest);

#ifdef Q_WS_MAC
            machineLogic()->updateDockIconSize(screenId(), pResizeEvent->width(), pResizeEvent->height());
#endif /* Q_WS_MAC */

            /* Report to the VM thread that we finished resizing: */
            session().GetConsole().GetDisplay().ResizeCompleted(screenId());

            /* Emit a signal about guest was resized: */
            emit resizeHintDone();

            pEvent->accept();
            return true;
        }

        case VBoxDefs::RepaintEventType:
        {
            UIRepaintEvent *pPaintEvent = static_cast<UIRepaintEvent*>(pEvent);
            QSize scaledSize = frameBuffer()->scaledSize();
            double xRatio = (double)scaledSize.width() / frameBuffer()->width();
            double yRatio = (double)scaledSize.height() / frameBuffer()->height();
            AssertMsg(contentsX() == 0, ("This can't be, else notify Dsen!\n"));
            AssertMsg(contentsY() == 0, ("This can't be, else notify Dsen!\n"));

            /* Make sure we update always a bigger rectangle than requested to
             * catch all rounding errors. (use 1 time the ratio factor and
             * round down on top/left, but round up for the width/height) */
            viewport()->update((int)(pPaintEvent->x() * xRatio) - ((int)xRatio) - 1,
                               (int)(pPaintEvent->y() * yRatio) - ((int)yRatio) - 1,
                               (int)(pPaintEvent->width() * xRatio) + ((int)xRatio + 2) * 2,
                               (int)(pPaintEvent->height() * yRatio) + ((int)yRatio + 2) * 2);
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
    if (pWatched != 0 && pWatched == viewport())
    {
        switch (pEvent->type())
        {
            case QEvent::Resize:
            {
                /* Perform the actually resize */
                sltPerformGuestScale();
                break;
            }
            default:
                break;
        }
    }
    else if (pWatched != 0 && pWatched == machineWindow())
    {
        switch (pEvent->type())
        {
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
# else
            case 0: /* Fixes compiler warning, fall through. */
# endif /* defined (VBOX_GUI_USE_DDRAW) */

#else
            case 0: /* Fixes compiler warning, fall through. */
#endif /* defined (Q_WS_WIN32) */
            default:
                break;
        }
    }

    return UIMachineView::eventFilter(pWatched, pEvent);
}

void UIMachineViewScale::saveMachineViewSettings()
{
    /* Store guest size in case we are switching to fullscreen: */
    storeGuestSizeHint(QSize(frameBuffer()->width(), frameBuffer()->height()));
}

QSize UIMachineViewScale::sizeHint() const
{
    /* Base-class have its own thoughts about size-hint
     * but scale-mode needs no size-hint to be set: */
    return QSize();
}

QRect UIMachineViewScale::workingArea() const
{
    return QApplication::desktop()->availableGeometry(this);
}

QSize UIMachineViewScale::calculateMaxGuestSize() const
{
    /* The area taken up by the machine window on the desktop,
     * including window frame, title, menu bar and status bar: */
    QRect windowGeo = machineWindow()->frameGeometry();
    /* The area taken up by the machine central widget, so excluding all decorations: */
    QRect centralWidgetGeo = machineWindow()->centralWidget()->geometry();
    /* To work out how big we can make the console window while still fitting on the desktop,
     * we calculate workingArea() - (windowGeo - centralWidgetGeo).
     * This works because the difference between machine window and machine central widget
     * (or at least its width and height) is a constant. */
    return QSize(  workingArea().width()
                 - (windowGeo.width() - centralWidgetGeo.width()),
                   workingArea().height()
                 - (windowGeo.height() - centralWidgetGeo.height()));
}

void UIMachineViewScale::updateSliders()
{
    if (horizontalScrollBarPolicy() != Qt::ScrollBarAlwaysOff)
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    if (verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff)
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

