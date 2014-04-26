/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIFrameBufferQImage class declarations
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

#ifndef ___UIFrameBufferQImage_h___
#define ___UIFrameBufferQImage_h___

/* Qt includes: */
#include <QImage>
#include <QPixmap>

/* GUI includes: */
#include "UIFrameBuffer.h"

/* QImage frame-buffer prototype: */
class UIFrameBufferQImage : public UIFrameBuffer
{
public:

    /* Constructor: */
    UIFrameBufferQImage(UIMachineView *pMachineView);

    /* API: Frame-buffer stuff: */
    ulong pixelFormat() { return m_uPixelFormat; }
    bool usesGuestVRAM() { return m_bUsesGuestVRAM; }
    uchar *address() { return m_img.bits(); }
    ulong bitsPerPixel() { return m_img.depth(); }
    ulong bytesPerLine() { return m_img.bytesPerLine(); }

    /* API: Event-delegate stuff: */
    void resizeEvent(UIResizeEvent *pEvent);
    void paintEvent(QPaintEvent *pEvent);

private:

    /* Helpers: Visual-mode paint stuff: */
    void paintDefault(QPaintEvent *pEvent);
    void paintSeamless(QPaintEvent *pEvent);
    void paintScale(QPaintEvent *pEvent);

    /** Draws corresponding @a rect of passed @a image with @a painter. */
    static void drawImageRect(QPainter &painter, const QImage &image, const QRect &rect,
                              int iContentsShiftX, int iContentsShiftY,
                              HiDPIOptimizationType hiDPIOptimizationType,
                              double dBackingScaleFactor);

    /* Helper: Fallback stuff: */
    void goFallback();

    /* Variables: */
    QImage m_img;
    ulong m_uPixelFormat;
    bool m_bUsesGuestVRAM;
};

#endif /* !___UIFrameBufferQImage_h___ */

#endif /* VBOX_GUI_USE_QIMAGE */

