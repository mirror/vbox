/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIFrameBufferDirectDraw class declarations
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

#ifndef ___UIFrameBufferDirectDraw_h___
#define ___UIFrameBufferDirectDraw_h___

#ifdef VBOX_GUI_USE_DDRAW

/* Local includes */
#include "UIFrameBuffer.h"

/* VBox/cdefs.h defines these: */
#undef LOWORD
#undef HIWORD
#undef LOBYTE
#undef HIBYTE
#include <ddraw.h>

/* TODO_NEW_CORE */
#if 0
class UIFrameBufferDDRAW : public UIFrameBuffer
{
public:

    UIFrameBufferDDRAW(UIMachineView *pMachineView);
    virtual ~UIFrameBufferDDRAW();

    STDMETHOD(NotifyUpdate) (ULONG uX, ULONG uY, ULONG uW, ULONG uH);

    uchar* address() { return (uchar*) m_surfaceDesc.lpSurface; }
    ulong bitsPerPixel() { return m_surfaceDesc.ddpfPixelFormat.dwRGBBitCount; }
    ulong bytesPerLine() { return (ulong) m_surfaceDesc.lPitch; }

    ulong pixelFormat() { return m_uPixelFormat; };

    bool usesGuestVRAM() { return m_bUsesGuestVRAM; }

    void paintEvent(QPaintEvent *pEvent);
    void resizeEvent(UIResizeEvent *pEvent);
    void moveEvent(QMoveEvent *pEvent);

private:

    void releaseObjects();

    bool createSurface(ULONG uPixelFormat, uchar *pvVRAM,
                       ULONG uBitsPerPixel, ULONG uBytesPerLine,
                       ULONG uWidth, ULONG uHeight);
    void deleteSurface();
    void drawRect(ULONG uX, ULONG uY, ULONG uW, ULONG uH);
    void getWindowPosition(void);

    LPDIRECTDRAW7        m_DDRAW;
    LPDIRECTDRAWCLIPPER  m_clipper;
    LPDIRECTDRAWSURFACE7 m_surface;
    DDSURFACEDESC2       m_surfaceDesc;
    LPDIRECTDRAWSURFACE7 m_primarySurface;

    ulong m_uPixelFormat;

    bool m_bUsesGuestVRAM;

    int m_iWndX;
    int m_iWndY;

    bool m_bSynchronousUpdates;
};
#endif

#endif /* VBOX_GUI_USE_DDRAW */

#endif /* !___UIFrameBufferDirectDraw_h___ */

