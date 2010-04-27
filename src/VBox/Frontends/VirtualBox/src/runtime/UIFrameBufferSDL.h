/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIFrameBufferSDL class declarations
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

#ifndef ___UIFrameBufferSDL_h___
#define ___UIFrameBufferSDL_h___

#ifdef VBOX_GUI_USE_SDL

/* Local includes */
# include "UIFrameBuffer.h"

/* Global includes */
# include <SDL.h>
# include <signal.h>

class UIFrameBufferSDL : public UIFrameBuffer
{
public:

    UIFrameBufferSDL(UIMachineView *pMachineView);
    virtual ~UIFrameBufferSDL();

    STDMETHOD(NotifyUpdate) (ULONG aX, ULONG aY, ULONG aW, ULONG aH);

    uchar* address()
    {
        SDL_Surface *surf = m_pSurfVRAM ? m_pSurfVRAM : m_pScreen;
        return surf ? (uchar*) (uintptr_t) surf->pixels : 0;
    }

    ulong bitsPerPixel()
    {
        SDL_Surface *surf = m_pSurfVRAM ? m_pSurfVRAM : m_pScreen;
        return surf ? surf->format->BitsPerPixel : 0;
    }

    ulong bytesPerLine()
    {
        SDL_Surface *surf = m_pSurfVRAM ? m_pSurfVRAM : m_pScreen;
        return surf ? surf->pitch : 0;
    }

    ulong pixelFormat()
    {
        return m_uPixelFormat;
    }

    bool usesGuestVRAM()
    {
        return m_pSurfVRAM != NULL;
    }

    void paintEvent(QPaintEvent *pEvent);
    void resizeEvent(UIResizeEvent *pEvent);

private:

    SDL_Surface *m_pScreen;
    SDL_Surface *m_pSurfVRAM;

    ulong m_uPixelFormat;
};

#endif /* VBOX_GUI_USE_SDL */

#endif /* !___UIFrameBufferSDL_h___ */

