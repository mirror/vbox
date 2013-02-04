/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIFrameBufferSDL class
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

#ifdef VBOX_GUI_USE_SDL

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include "precomp.h"
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Local includes */
# include "UIFrameBufferSDL.h"
# include "UIMachineView.h"
# include "VBoxX11Helper.h"

/* Global includes */
# include <QApplication>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/** @class UIFrameBufferSDL
 *
 *  The UIFrameBufferSDL class is a class that implements the IFrameBuffer
 *  interface and uses SDL to store and render VM display data.
 */
UIFrameBufferSDL::UIFrameBufferSDL(UIMachineView *pMachineView)
    : UIFrameBuffer(pMachineView)
{
    m_pScreen = NULL;
    m_uPixelFormat = FramebufferPixelFormat_FOURCC_RGB;
    m_pSurfVRAM = NULL;

    X11ScreenSaverSettingsInit();
    UIResizeEvent event(FramebufferPixelFormat_Opaque, NULL, 0, 0, 640, 480);
    resizeEvent(&event);
}

UIFrameBufferSDL::~UIFrameBufferSDL()
{
    if (m_pSurfVRAM)
    {
        SDL_FreeSurface(m_pSurfVRAM);
        m_pSurfVRAM = NULL;
    }
    X11ScreenSaverSettingsSave();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    X11ScreenSaverSettingsRestore();
}

/** @note This method is called on EMT from under this object's lock */
STDMETHODIMP UIFrameBufferSDL::NotifyUpdate(ULONG uX, ULONG uY, ULONG uW, ULONG uH)
{
#if !defined (Q_WS_WIN) && !defined (Q_WS_PM)
    /* we're not on the GUI thread and update() isn't thread safe in Qt 3.3.x
       on the Mac (4.2.x is), on Linux (didn't check Qt 4.x there) and
       probably on other non-DOS platforms, so post the event instead. */
    QApplication::postEvent (m_pMachineView, new UIRepaintEvent(uX, uY, uW, uH));
#else
    /* we're not on the GUI thread, so update() instead of repaint()! */
    m_pMachineView->viewport()->update(uX - m_pMachineView->contentsX(), uY - m_pMachineView->contentsY(), uW, uH);
#endif
    return S_OK;
}

void UIFrameBufferSDL::paintEvent(QPaintEvent *pEvent)
{
    if (m_pScreen)
    {
        if (m_pScreen->pixels)
        {
            QRect r = pEvent->rect();

            if (m_pSurfVRAM)
            {
                SDL_Rect rect = { (Sint16)r.x(), (Sint16)r.y(), (Uint16)r.width(), (Uint16)r.height() };
                SDL_BlitSurface(m_pSurfVRAM, &rect, m_pScreen, &rect);
                /** @todo may be: if ((m_pScreen->flags & SDL_HWSURFACE) == 0) */
                SDL_UpdateRect(m_pScreen, r.x(), r.y(), r.width(), r.height());
            }
            else
            {
                SDL_UpdateRect(m_pScreen, r.x(), r.y(), r.width(), r.height());
            }
        }
    }
}

void UIFrameBufferSDL::resizeEvent(UIResizeEvent *pEvent)
{
    /* Check whether the guest resolution has not been changed. */
    bool fSameResolutionRequested = (width()  == pEvent->width() && height() == pEvent->height());

    /* Check if the guest VRAM can be used as the source bitmap. */
    bool bFallback = false;

    Uint32 Rmask = 0;
    Uint32 Gmask = 0;
    Uint32 Bmask = 0;

    if (pEvent->pixelFormat() == FramebufferPixelFormat_FOURCC_RGB)
    {
        switch (pEvent->bitsPerPixel())
        {
            case 32:
                Rmask = 0x00FF0000;
                Gmask = 0x0000FF00;
                Bmask = 0x000000FF;
                break;
            case 24:
                Rmask = 0x00FF0000;
                Gmask = 0x0000FF00;
                Bmask = 0x000000FF;
                break;
            case 16:
                Rmask = 0xF800;
                Gmask = 0x07E0;
                Bmask = 0x001F;
                break;
            default:
                /* Unsupported format leads to the indirect buffer */
                bFallback = true;
                break;
        }
    }
    else
    {
        /* Unsupported format leads to the indirect buffer */
        bFallback = true;
    }

    m_width = pEvent->width();
    m_height = pEvent->height();

    /* Recreate the source surface. */
    if (m_pSurfVRAM)
    {
        SDL_FreeSurface(m_pSurfVRAM);
        m_pSurfVRAM = NULL;
    }

    if (!bFallback)
    {
        /* It is OK to create the source surface from the guest VRAM. */
        m_pSurfVRAM = SDL_CreateRGBSurfaceFrom(pEvent->VRAM(), pEvent->width(), pEvent->height(),
                                               pEvent->bitsPerPixel(),
                                               pEvent->bytesPerLine(),
                                               Rmask, Gmask, Bmask, 0);
        LogFlowFunc(("Created VRAM surface %p\n", m_pSurfVRAM));
        if (m_pSurfVRAM == NULL)
        {
            bFallback = true;
        }
    }

    if (fSameResolutionRequested)
    {
        LogFlowFunc(("the same resolution requested, skipping the resize.\n"));
        return;
    }

    /* close SDL so we can init it again */
    if (m_pScreen)
    {
        X11ScreenSaverSettingsSave();
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        X11ScreenSaverSettingsRestore();
        m_pScreen = NULL;
    }

    /*
     *  initialize the SDL library, use its super hack to integrate it with our client window
     */
    static char sdlHack[64];
    LogFlowFunc(("Using client window 0x%08lX to initialize SDL\n", m_pMachineView->viewport()->winId()));
    /* Note: SDL_WINDOWID must be decimal (not hex) to work on Win32 */
    sprintf(sdlHack, "SDL_WINDOWID=%lu", m_pMachineView->viewport()->winId());
    putenv(sdlHack);
    X11ScreenSaverSettingsSave();
    int rc = SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);
    X11ScreenSaverSettingsRestore();
    AssertMsg(rc == 0, ("SDL initialization failed: %s\n", SDL_GetError()));
    NOREF(rc);

#ifdef Q_WS_X11
    /* undo signal redirections from SDL, it'd steal keyboard events from us! */
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
#endif

    LogFlowFunc(("Setting SDL video mode to %d x %d\n", m_width, m_height));

    /* Pixel format is RGB in any case */
    m_uPixelFormat = FramebufferPixelFormat_FOURCC_RGB;

    m_pScreen = SDL_SetVideoMode(m_width, m_height, 0, SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL);
    AssertMsg(m_pScreen, ("SDL video mode could not be set!\n"));
}

#endif /* VBOX_GUI_USE_SDL */

