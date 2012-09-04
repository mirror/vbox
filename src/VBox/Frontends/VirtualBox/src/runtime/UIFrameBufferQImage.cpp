/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIFrameBufferQImage class implementation
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
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

/* Local includes */
# include "UIFrameBufferQImage.h"
# include "UIMachineView.h"
# include "UIMessageCenter.h"
# include "VBoxGlobal.h"
# include "UISession.h"

/* Global includes */
# include <QPainter>
# include <QApplication>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/** @class UIFrameBufferQImage
 *
 *  The UIFrameBufferQImage class is a class that implements the IFrameBuffer
 *  interface and uses QImage as the direct storage for VM display data. QImage
 *  is then converted to QPixmap and blitted to the console view widget.
 */
UIFrameBufferQImage::UIFrameBufferQImage(UIMachineView *pMachineView)
    : UIFrameBuffer(pMachineView)
{
    /* Initialize the framebuffer the first time */
    UIResizeEvent event(FramebufferPixelFormat_Opaque, NULL, 0, 0, 640, 480);
    resizeEvent(&event);
}

/** @note This method is called on EMT from under this object's lock */
STDMETHODIMP UIFrameBufferQImage::NotifyUpdate(ULONG uX, ULONG uY, ULONG uW, ULONG uH)
{
    /* We're not on the GUI thread and update() isn't thread safe in
     * Qt 4.3.x on the Win, Qt 3.3.x on the Mac (4.2.x is),
     * on Linux (didn't check Qt 4.x there) and probably on other
     * non-DOS platforms, so post the event instead. */
    QApplication::postEvent(m_pMachineView, new UIRepaintEvent(uX, uY, uW, uH));

    return S_OK;
}

void UIFrameBufferQImage::paintEvent(QPaintEvent *pEvent)
{
    /* on mode switch the paint event may come while the view is null (before the new view gets set)
     * this is seen on Windows hosts with 3D enabled,
     * ignore paint events in that case */
    if (!m_pMachineView)
        return;
    /* If the machine is NOT in 'running' state,
     * the link between framebuffer and video memory
     * is broken, we should go fallback now... */
    if (m_bUsesGuestVRAM &&
        !m_pMachineView->uisession()->isRunning() &&
        /* Online snapshotting: */
        m_pMachineView->uisession()->machineState() != KMachineState_Saving)
        goFallback();

    /* Scaled image by default is empty: */
    QImage scaledImage;
    /* If scaled-factor is set and current image is NOT null: */
    if (m_scaledSize.isValid() && !m_img.isNull())
    {
        /* We are doing a deep copy of image to make sure it will not be
         * detached during scale process, otherwise we can get a frozen frame-buffer. */
        scaledImage = m_img.copy();
        /* Scale image to scaled-factor: */
        scaledImage = scaledImage.scaled(m_scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    /* Choose required image: */
    QImage *pSourceImage = scaledImage.isNull() ? &m_img : &scaledImage;

    /* Apply image-size restriction: */
    const QRect &r = pEvent->rect().intersected(pSourceImage->rect());
    /* Some outdated rectangle during processing UIResizeEvent */
    if (r.isEmpty())
        return;

#if 0
    LogFlowFunc (("%dx%d-%dx%d (img=%dx%d)\n", r.x(), r.y(), r.width(), r.height(), img.width(), img.height()));
#endif

    QPainter painter(m_pMachineView->viewport());
    if ((ulong)r.width() < m_width * 2 / 3)
    {
        /* This method is faster for narrow updates */
        m_PM = QPixmap::fromImage(pSourceImage->copy(r.x() + m_pMachineView->contentsX(),
                                                     r.y() + m_pMachineView->contentsY(),
                                                     r.width(), r.height()));
        painter.drawPixmap(r.x(), r.y(), m_PM);
    }
    else
    {
        /* This method is faster for wide updates */
        m_PM = QPixmap::fromImage(QImage(pSourceImage->scanLine(r.y() + m_pMachineView->contentsY()),
                                         pSourceImage->width(), r.height(), pSourceImage->bytesPerLine(),
                                         QImage::Format_RGB32));
        painter.drawPixmap(r.x(), r.y(), m_PM, r.x() + m_pMachineView->contentsX(), 0, 0, 0);
    }
}

void UIFrameBufferQImage::resizeEvent(UIResizeEvent *pEvent)
{
#if 0
    LogFlowFunc (("fmt=%d, vram=%p, bpp=%d, bpl=%d, width=%d, height=%d\n",
                  pEvent->pixelFormat(), pEvent->VRAM(),
                  pEvent->bitsPerPixel(), pEvent->bytesPerLine(),
                  pEvent->width(), pEvent->height()));
#endif

    m_width = pEvent->width();
    m_height = pEvent->height();

    bool bRemind = false;
    bool bFallback = false;
    ulong bitsPerLine = pEvent->bytesPerLine() * 8;

    /* check if we support the pixel format and can use the guest VRAM directly */
    if (pEvent->pixelFormat() == FramebufferPixelFormat_FOURCC_RGB)
    {
        QImage::Format format;
        switch (pEvent->bitsPerPixel())
        {
            /* 32-, 8- and 1-bpp are the only depths supported by QImage */
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
                format = QImage::Format_Invalid; /* set it to something so gcc keeps quiet. */
                bRemind = true;
                bFallback = true;
                break;
        }

        if (!bFallback)
        {
            /* QImage only supports 32-bit aligned scan lines... */
            Assert ((pEvent->bytesPerLine() & 3) == 0);
            bFallback = ((pEvent->bytesPerLine() & 3) != 0);
        }
        if (!bFallback)
        {
            /* ...and the scan lines ought to be a whole number of pixels. */
            Assert ((bitsPerLine & (pEvent->bitsPerPixel() - 1)) == 0);
            bFallback = ((bitsPerLine & (pEvent->bitsPerPixel() - 1)) != 0);
        }
        if (!bFallback)
        {
            Assert (bitsPerLine / pEvent->bitsPerPixel() >= m_width);
            bFallback = RT_BOOL (bitsPerLine / pEvent->bitsPerPixel() < m_width);
        }
        if (!bFallback)
        {
            m_img = QImage ((uchar *) pEvent->VRAM(), m_width, m_height, bitsPerLine / 8, format);
            m_uPixelFormat = FramebufferPixelFormat_FOURCC_RGB;
            m_bUsesGuestVRAM = true;
        }
    }
    else
    {
        bFallback = true;
    }

    if (bFallback)
    {
        goFallback();
    }

    if (bRemind)
        msgCenter().remindAboutWrongColorDepth(pEvent->bitsPerPixel(), 32);
}

void UIFrameBufferQImage::goFallback()
{
    /* We don't support either the pixel format or the color depth;
     * or the machine is in the state which breaks link between
     * the framebuffer and the actual video-memory: */
    m_img = QImage(m_width, m_height, QImage::Format_RGB32);
    m_img.fill(0);
    m_uPixelFormat = FramebufferPixelFormat_FOURCC_RGB;
    m_bUsesGuestVRAM = false;
}

#endif /* VBOX_GUI_USE_QIMAGE */

