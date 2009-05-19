/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Declaration of SDLFramebuffer class
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#ifndef __H_SDLFRAMEBUFFER
#define __H_SDLFRAMEBUFFER

#include "VBoxBFE.h"
#include "SDLConsole.h"

#include <iprt/thread.h>

#include <iprt/critsect.h>

#ifdef VBOX_SECURELABEL
#include <SDL_ttf.h>
#endif

#include "Framebuffer.h"

class SDLFramebuffer : public Framebuffer
{
public:
    SDLFramebuffer();
    virtual ~SDLFramebuffer();

    virtual HRESULT getWidth(ULONG *width);
    virtual HRESULT getHeight(ULONG *height);
    virtual HRESULT Lock();
    virtual HRESULT Unlock();
    virtual HRESULT getAddress(uintptr_t *address);
    virtual HRESULT getBitsPerPixel(ULONG *bitsPerPixel);
    virtual HRESULT getLineSize(ULONG *lineSize);
    virtual HRESULT NotifyUpdate(ULONG x, ULONG y, ULONG w, ULONG h);
    virtual HRESULT RequestResize(ULONG w, ULONG h, BOOL *finished);
    virtual HRESULT GetVisibleRegion(BYTE *aRectangles, ULONG aCount, ULONG *aCountCopied);
    virtual HRESULT SetVisibleRegion(BYTE *aRectangles, ULONG aCount);

    virtual void    repaint();
    virtual void    resize();

    virtual void    update(int x, int y, int w, int h);
    virtual bool    getFullscreen();
    virtual void    setFullscreen(bool fFullscreen);
    virtual int     getYOffset();
    virtual int     getHostXres();
    virtual int     getHostYres();
    virtual int     getHostBitsPerPixel();

#ifdef VBOX_SECURELABEL
    virtual int     initSecureLabel(uint32_t height, char *font, uint32_t pointsize);
    virtual void    setSecureLabelText(const char *text);
    virtual void    paintSecureLabel(int x, int y, int w, int h, bool fForce);
#endif

private:
    /** the sdl thread */
    RTNATIVETHREAD mSdlNativeThread;
    /** current SDL framebuffer pointer */
    SDL_Surface *mScreen;
    /** current guest screen width in pixels */
    ULONG mWidth;
    /** current guest screen height in pixels */
    ULONG mHeight;
    /** Y offset in pixels, i.e. screen_height - guest_screen_height */
    uint32_t mTopOffset;
    /** flag whether we're in fullscreen mode */
    bool  mfFullscreen;
    /** framebuffer update semaphore */
    RTCRITSECT mUpdateLock;
#ifdef VBOX_SECURELABEL
    /** current secure label text */
    Utf8Str mSecureLabelText;
    /** secure label font handle */
    TTF_Font *mLabelFont;
    /** secure label height in pixels */
    uint32_t mLabelHeight;
#endif
#ifdef RT_OS_WINDOWS
    long refcnt;
#endif
};

#endif // __H_SDLFRAMEBUFFER
