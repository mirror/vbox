/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIFrameBufferQImage class implementation
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

#ifdef VBOX_GUI_USE_QIMAGE

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include "precomp.h"
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Local includes */
# include "UIFrameBufferQImage.h"
# include "UIMachineView.h"
# include "VBoxProblemReporter.h"
# include "VBoxGlobal.h"

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
    const QRect &r = pEvent->rect().intersected(m_pMachineView->viewport()->rect());

    /* Some outdated rectangle during processing UIResizeEvent */
    if (r.isEmpty())
        return;

#if 0
    LogFlowFunc (("%dx%d-%dx%d (img=%dx%d)\n", r.x(), r.y(), r.width(), r.height(), img.width(), img.height()));
#endif

    QPainter painter(m_pMachineView->viewport());

    if (r.width() < m_width * 2 / 3)
    {
        /* This method is faster for narrow updates */
        m_PM = QPixmap::fromImage(m_img.copy(r.x() + m_pMachineView->contentsX(),
                                             r.y() + m_pMachineView->contentsY(),
                                             r.width(), r.height()));
        painter.drawPixmap(r.x(), r.y(), m_PM);
    }
    else
    {
        /* This method is faster for wide updates */
        m_PM = QPixmap::fromImage(QImage(m_img.scanLine (r.y() + m_pMachineView->contentsY()),
                                  m_img.width(), r.height(), m_img.bytesPerLine(),
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
            ulong virtWdt = bitsPerLine / pEvent->bitsPerPixel();
            m_img = QImage ((uchar *) pEvent->VRAM(), virtWdt, m_height, format);
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
        /* we don't support either the pixel format or the color depth,
         * bFallback to a self-provided 32bpp RGB buffer */
        m_img = QImage (m_width, m_height, QImage::Format_RGB32);
        m_uPixelFormat = FramebufferPixelFormat_FOURCC_RGB;
        m_bUsesGuestVRAM = false;
    }

    if (bRemind)
    {
        class RemindEvent : public VBoxAsyncEvent
        {
            ulong mRealBPP;
        public:
            RemindEvent (ulong aRealBPP)
                : mRealBPP (aRealBPP) {}
            void handle()
            {
                vboxProblem().remindAboutWrongColorDepth (mRealBPP, 32);
            }
        };
        (new RemindEvent (pEvent->bitsPerPixel()))->post();
    }
}

#endif /* VBOX_GUI_USE_QIMAGE */

