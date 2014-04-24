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
# include "UIPopupCenter.h"
# include "VBoxGlobal.h"
# include "UISession.h"
# include "UIMachineLogic.h"
# include "UIMachineWindow.h"
# include "UIMachineView.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"
#include "CConsole.h"

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
    {
        LogRelFlow(("UIFrameBufferQImage::resizeEvent: "
                    "Going fallback due to frame-buffer format become invalid: "
                    "Format=%lu, BitsPerPixel=%lu, BytesPerLine=%lu, Size=%lux%lu\n",
                    (unsigned long)pEvent->pixelFormat(),
                    (unsigned long)pEvent->bitsPerPixel(),
                    (unsigned long)pEvent->bytesPerLine(),
                    (unsigned long)pEvent->width(),
                    (unsigned long)pEvent->height()));
        goFallback();
    }

    /* Remind if requested: */
    if (bRemind && m_pMachineView->uisession()->isGuestAdditionsActive())
        popupCenter().remindAboutWrongColorDepth(m_pMachineView->machineWindow(),
                                                      pEvent->bitsPerPixel(), 32);
    else
        popupCenter().forgetAboutWrongColorDepth(m_pMachineView->machineWindow());
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
    KMachineState machineState = m_pMachineView->uisession()->session().GetConsole().GetState();
    if (   m_bUsesGuestVRAM
        /* running */
        && machineState != KMachineState_Running
        && machineState != KMachineState_Teleporting
        && machineState != KMachineState_LiveSnapshotting
        && machineState != KMachineState_DeletingSnapshotOnline
        /* paused */
        && machineState != KMachineState_Paused
        && machineState != KMachineState_TeleportingPausedVM
        /* saving */
        && machineState != KMachineState_Saving
        /* guru */
        && machineState != KMachineState_Stuck
        )
    {
        LogRelFlow(("UIFrameBufferQImage::paintEvent: "
                    "Going fallback due to machine-state become invalid: "
                    "%d.\n", (int)machineState));
        goFallback();
    }

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

void UIFrameBufferQImage::paintDefault(QPaintEvent *pEvent)
{
    /* Get rectangle to paint: */
    QRect paintRect = pEvent->rect().intersected(m_img.rect());
    if (paintRect.isEmpty())
        return;

    /* Create painter: */
    QPainter painter(m_pMachineView->viewport());

    /* Draw image rectangle: */
    drawImageRect(painter, m_img, paintRect,
                  m_pMachineView->contentsX(), m_pMachineView->contentsY(),
                  backingScaleFactor());
}

void UIFrameBufferQImage::paintSeamless(QPaintEvent *pEvent)
{
    /* Get rectangle to paint: */
    QRect paintRect = pEvent->rect().intersected(m_img.rect());
    if (paintRect.isEmpty())
        return;

    /* Create painter: */
    QPainter painter(m_pMachineView->viewport());

    /* Determine the region to erase: */
    lock();
    QRegion regionToErase = (QRegion)paintRect - m_syncVisibleRegion;
    unlock();
    if (!regionToErase.isEmpty())
    {
        /* Optimize composition-mode: */
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        /* Erase required region, slowly, rectangle-by-rectangle: */
        foreach (const QRect &rect, regionToErase.rects())
            painter.eraseRect(rect);
        /* Restore composition-mode: */
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

    /* Determine the region to paint: */
    lock();
    QRegion regionToPaint = (QRegion)paintRect & m_syncVisibleRegion;
    unlock();
    if (!regionToPaint.isEmpty())
    {
        /* Paint required region, slowly, rectangle-by-rectangle: */
        foreach (const QRect &rect, regionToPaint.rects())
        {
#if defined(VBOX_WITH_TRANSLUCENT_SEAMLESS) && defined(Q_WS_WIN)
            /* Replace translucent background with black one,
             * that is necessary for window with Qt::WA_TranslucentBackground: */
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.fillRect(rect, QColor(Qt::black));
            painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
#endif /* VBOX_WITH_TRANSLUCENT_SEAMLESS && Q_WS_WIN */

            /* Draw image rectangle: */
            drawImageRect(painter, m_img, rect,
                          m_pMachineView->contentsX(), m_pMachineView->contentsY(),
                          backingScaleFactor());
        }
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

    /* Draw image rectangle: */
    drawImageRect(painter, sourceImage, paintRect,
                  m_pMachineView->contentsX(), m_pMachineView->contentsY(),
                  backingScaleFactor());
}

/* static */
void UIFrameBufferQImage::drawImageRect(QPainter &painter, const QImage &image, const QRect &rect,
                                        int iContentsShiftX, int iContentsShiftY,
                                        double dBackingScaleFactor)
{
    /* Calculate offset: */
    size_t offset = (rect.x() + iContentsShiftX) * image.depth() / 8 +
                    (rect.y() + iContentsShiftY) * image.bytesPerLine();

    /* Create sub-image (no copy involved): */
    QImage subImage = QImage(image.bits() + offset,
                             rect.width(), rect.height(),
                             image.bytesPerLine(), image.format());

#ifndef QIMAGE_FRAMEBUFFER_WITH_DIRECT_OUTPUT
    /* Create sub-pixmap on the basis of sub-image above (1st copy involved): */
    QPixmap subPixmap = QPixmap::fromImage(subImage);

    /* If HiDPI 'backing scale factor' defined: */
    if (dBackingScaleFactor > 1.0)
    {
        /* Scale sub-pixmap (2nd copy involved): */
        subPixmap = subPixmap.scaled(subPixmap.size() * dBackingScaleFactor,
                                     Qt::IgnoreAspectRatio, Qt::FastTransformation);
        subPixmap.setDevicePixelRatio(dBackingScaleFactor);
    }

    /* Draw sub-pixmap: */
    painter.drawPixmap(rect.x(), rect.y(), subPixmap);
#else /* QIMAGE_FRAMEBUFFER_WITH_DIRECT_OUTPUT */
    /* If HiDPI 'backing scale factor' defined: */
    if (dBackingScaleFactor > 1.0)
    {
        /* Create scaled-sub-image (1st copy involved): */
        QImage scaledSubImage = subImage.scaled(subImage.size() * dBackingScaleFactor,
                                                Qt::IgnoreAspectRatio, Qt::FastTransformation);
        scaledSubImage.setDevicePixelRatio(dBackingScaleFactor);
        /* Directly draw scaled-sub-image: */
        painter.drawImage(rect.x(), rect.y(), scaledSubImage);
    }
    else
    {
        /* Directly draw sub-image: */
        painter.drawImage(rect.x(), rect.y(), subImage);
    }
#endif /* QIMAGE_FRAMEBUFFER_WITH_DIRECT_OUTPUT */
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

