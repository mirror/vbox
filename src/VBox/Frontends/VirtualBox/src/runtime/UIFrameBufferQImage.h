/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIFrameBufferQImage class declarations
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIFrameBufferQImage_h___
#define ___UIFrameBufferQImage_h___

#ifdef VBOX_GUI_USE_QIMAGE

/* Local includes */
#include "UIFrameBuffer.h"

/* Global includes */
#include <QImage>
#include <QPixmap>

class UIFrameBufferQImage : public UIFrameBuffer
{
public:

    UIFrameBufferQImage(UIMachineView *pMachineView);

    STDMETHOD(NotifyUpdate) (ULONG uX, ULONG uY, ULONG uW, ULONG uH);

    ulong pixelFormat() { return m_uPixelFormat; }
    bool usesGuestVRAM() { return m_bUsesGuestVRAM; }

    uchar *address() { return m_img.bits(); }
    ulong bitsPerPixel() { return m_img.depth(); }
    ulong bytesPerLine() { return m_img.bytesPerLine(); }

    void paintEvent(QPaintEvent *pEvent);
    void resizeEvent(UIResizeEvent *pEvent);

private:

    void goFallback();

    QPixmap m_PM;
    QImage m_img;
    ulong m_uPixelFormat;
    bool m_bUsesGuestVRAM;
};

#endif /* VBOX_GUI_USE_QIMAGE */

#endif /* !___UIFrameBufferQImage_h___ */

