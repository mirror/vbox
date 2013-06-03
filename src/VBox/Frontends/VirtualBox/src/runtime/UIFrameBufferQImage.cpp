/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIFrameBufferQImage class implementation
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_GUI_USE_QIMAGE

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include "precomp.h"
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QPainter>
# include <QApplication>

/* GUI includes: */
# include "UIFrameBufferQImage.h"
# include "UIMachineView.h"
# include "UIMessageCenter.h"
# include "VBoxGlobal.h"
# include "UISession.h"
# include "UIMachineLogic.h"
# ifdef Q_WS_X11
#  include "UIMachineWindow.h"
# endif /* Q_WS_X11 */

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIFrameBufferQImage::UIFrameBufferQImage(UIMachineView *pMachineView)
    : UIFrameBuffer(pMachineView)
{
    /* Initialize the framebuffer: */
    UIResizeEvent event(FramebufferPixelFormat_Opaque, NULL, 0, 0, 640, 480);
    resizeEvent(&event);
}

void UIFrameBufferQImage::resizeEvent(UIResizeEvent *pEvent)
{
    /* Invalidate visible-region if necessary: */
    if (m_pMachineView->machineLogic()->visualStateType() == UIVisualStateType_Seamless &&
        (m_width != pEvent->width() || m_height != pEvent->height()))
    {
        lock();
        m_syncVisibleRegion = QRegion();
        m_asyncVisibleRegion = QRegion();
        unlock();
    }

    /* Remember new width/height: */
    m_width = pEvent->width();
    m_height = pEvent->height();

    /* Check if we support the pixel format and can use the guest VRAM directly: */
    bool bRemind = false;
    bool bFallback = false;
    ulong bitsPerLine = pEvent->bytesPerLine() * 8;
    if (pEvent->pixelFormat() == FramebufferPixelFormat_FOURCC_RGB)
    {
        QImage::Format format;
        switch (pEvent->bitsPerPixel())
        {
            /* 32-, 8- and 1-bpp are the only depths supported by QImage: */
            case 32:
                format = QImage::Format_RGB32;
                break;
            case 8:
                format = QImage::Format_Indexed8;
                bRemind = true;
                break;
            case 1:
                format = QImage::Format_Mono;
                bRemind = true;
                break;
            default:
                format = QImage::Format_Invalid;
                bRemind = true;
                bFallback = true;
                break;
        }

        if (!bFallback)
        {
            /* QImage only supports 32-bit aligned scan lines... */
            Assert((pEvent->bytesPerLine() & 3) == 0);
            bFallback = ((pEvent->bytesPerLine() & 3) != 0);
        }
        if (!bFallback)
        {
            /* ...and the scan lines ought to be a whole number of pixels. */
            Assert((bitsPerLine & (pEvent->bitsPerPixel() - 1)) == 0);
            bFallback = ((bitsPerLine & (pEvent->bitsPerPixel() - 1)) != 0);
        }
        if (!bFallback)
        {
            /* Make sure constraints are also passed: */
            Assert(bitsPerLine / pEvent->bitsPerPixel() >= m_width);
            bFallback = RT_BOOL(bitsPerLine / pEvent->bitsPerPixel() < m_width);
        }
        if (!bFallback)
        {
            /* Finally compose image using VRAM directly: */
            m_img = QImage((uchar *)pEvent->VRAM(), m_width, m_height, bitsPerLine / 8, format);
            m_uPixelFormat = FramebufferPixelFormat_FOURCC_RGB;
            m_bUsesGuestVRAM = true;
        }
    }
    else
        bFallback = true;

    /* Fallback if requested: */
    if (bFallback)
        goFallback();

    /* Remind if requested: */
    if (bRemind)
        msgCenter().remindAboutWrongColorDepth(pEvent->bitsPerPixel(), 32);
}

void UIFrameBufferQImage::paintEvent(QPaintEvent *pEvent)
{
    /* On mode switch the enqueued paint event may still come
     * while the view is already null (before the new view gets set),
     * ignore paint event in that case. */
    if (!m_pMachineView)
        return;

    /* If the machine is NOT in 'running', 'paused' or 'saving' state,
     * the link between the framebuffer and the video memory is broken.
     * We should go fallback in that case.
     * We should acquire actual machine-state to exclude
     * situations when the state was changed already but
     * GUI didn't received event about that or didn't processed it yet. */
    KMachineState machineState = m_pMachineView->uisession()->session().GetMachine().GetState();
    if (   m_bUsesGuestVRAM
        /* running */
        && machineState != KMachineState_Running
        && machineState != KMachineState_Teleporting
        && machineState != KMachineState_LiveSnapshotting
        /* paused */
        && machineState != KMachineState_Paused
        && machineState != KMachineState_TeleportingPausedVM
        /* saving */
        && machineState != KMachineState_Saving
        )
        goFallback();

    /* Depending on visual-state type: */
    switch (m_pMachineView->machineLogic()->visualStateType())
    {
        case UIVisualStateType_Seamless:
            paintSeamless(pEvent);
            break;
        case UIVisualStateType_Scale:
            paintScale(pEvent);
            break;
        default:
            paintDefault(pEvent);
            break;
    }
}

void UIFrameBufferQImage::applyVisibleRegion(const QRegion &region)
{
    /* Make sure async visible-region changed: */
    if (m_asyncVisibleRegion == region)
        return;

    /* We are accounting async visible-regions one-by-one
     * to keep corresponding viewport area always updated! */
    m_pMachineView->viewport()->update(region + m_asyncVisibleRegion);
    m_asyncVisibleRegion = region;

#ifdef Q_WS_X11
    /* Qt 4.8.3 under X11 has Qt::WA_TranslucentBackground window attribute broken,
     * so we are also have to use async visible-region to apply to [Q]Widget [set]Mask
     * which internally wraps old one known (approved) Xshape extension: */
    m_pMachineView->machineWindow()->setMask(m_asyncVisibleRegion);
#endif /* Q_WS_X11 */
}

void UIFrameBufferQImage::paintDefault(QPaintEvent *pEvent)
{
    /* Get rectangle to paint: */
    QRect paintRect = pEvent->rect().intersected(m_img.rect());
    if (paintRect.isEmpty())
        return;

    /* Create painter: */
    QPainter painter(m_pMachineView->viewport());

    /* Draw image rectangle depending on rectangle width: */
    if ((ulong)paintRect.width() < m_width * 2 / 3)
        drawImageRectNarrow(painter, m_img,
                            paintRect, m_pMachineView->contentsX(), m_pMachineView->contentsY());
    else
        drawImageRectWide(painter, m_img,
                          paintRect, m_pMachineView->contentsX(), m_pMachineView->contentsY());
}

void UIFrameBufferQImage::paintSeamless(QPaintEvent *pEvent)
{
    /* Get rectangle to paint: */
    QRect paintRect = pEvent->rect().intersected(m_img.rect());
    if (paintRect.isEmpty())
        return;

    /* Create painter: */
    QPainter painter(m_pMachineView->viewport());

    /* Clear paint rectangle first: */
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.eraseRect(paintRect);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    /* Manually clip paint rectangle using visible-region: */
    lock();
    QRegion visiblePaintRegion = m_syncVisibleRegion & paintRect;
    unlock();

    /* Repaint all the rectangles of visible-region: */
    foreach (const QRect &rect, visiblePaintRegion.rects())
    {
#ifdef Q_WS_WIN
        /* Replace translucent background with black one,
         * that is necessary for window with Qt::WA_TranslucentBackground: */
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect, QColor(Qt::black));
        painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
#endif /* Q_WS_WIN */

        /* Draw image rectangle depending on rectangle width: */
        if ((ulong)rect.width() < m_width * 2 / 3)
            drawImageRectNarrow(painter, m_img,
                                rect, m_pMachineView->contentsX(), m_pMachineView->contentsY());
        else
            drawImageRectWide(painter, m_img,
                              rect, m_pMachineView->contentsX(), m_pMachineView->contentsY());
    }
}

void UIFrameBufferQImage::paintScale(QPaintEvent *pEvent)
{
    /* Scaled image is NULL by default: */
    QImage scaledImage;
    /* But if scaled-factor is set and current image is NOT null: */
    if (m_scaledSize.isValid() && !m_img.isNull())
    {
        /* We are doing a deep copy of the image to make sure it will not be
         * detached during scale process, otherwise we can get a frozen frame-buffer. */
        scaledImage = m_img.copy();
        /* And scaling the image to predefined scaled-factor: */
        scaledImage = scaledImage.scaled(m_scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    /* Finally we are choosing image to paint from: */
    QImage &sourceImage = scaledImage.isNull() ? m_img : scaledImage;

    /* Get rectangle to paint: */
    QRect paintRect = pEvent->rect().intersected(sourceImage.rect());
    if (paintRect.isEmpty())
        return;

    /* Create painter: */
    QPainter painter(m_pMachineView->viewport());

    /* Draw image rectangle depending on rectangle width: */
    if ((ulong)paintRect.width() < m_width * 2 / 3)
        drawImageRectNarrow(painter, sourceImage,
                            paintRect, m_pMachineView->contentsX(), m_pMachineView->contentsY());
    else
        drawImageRectWide(painter, sourceImage,
                          paintRect, m_pMachineView->contentsX(), m_pMachineView->contentsY());
}

/* static */
void UIFrameBufferQImage::drawImageRectNarrow(QPainter &painter, const QImage &image,
                                              const QRect &rect, int iContentsShiftX, int iContentsShiftY)
{
    /* This method is faster for narrow updates: */
    QPixmap pm = QPixmap::fromImage(image.copy(rect.x() + iContentsShiftX,
                                               rect.y() + iContentsShiftY,
                                               rect.width(), rect.height()));
    painter.drawPixmap(rect.x(), rect.y(), pm);
}

/* static */
void UIFrameBufferQImage::drawImageRectWide(QPainter &painter, const QImage &image,
                                            const QRect &rect, int iContentsShiftX, int iContentsShiftY)
{
    /* This method is faster for wide updates: */
    QPixmap pm = QPixmap::fromImage(QImage(image.scanLine(rect.y() + iContentsShiftY),
                                           image.width(), rect.height(), image.bytesPerLine(),
                                           QImage::Format_RGB32));
    painter.drawPixmap(rect.x(), rect.y(), pm, rect.x() + iContentsShiftX, 0, 0, 0);
}

void UIFrameBufferQImage::goFallback()
{
    /* We calling for fallback when we:
     * 1. don't support either the pixel format or the color depth;
     * 2. or the machine is in the state which breaks link between
     *    the framebuffer and the actual video-memory: */
    m_img = QImage(m_width, m_height, QImage::Format_RGB32);
    m_img.fill(0);
    m_uPixelFormat = FramebufferPixelFormat_FOURCC_RGB;
    m_bUsesGuestVRAM = false;
}

#endif /* VBOX_GUI_USE_QIMAGE */

