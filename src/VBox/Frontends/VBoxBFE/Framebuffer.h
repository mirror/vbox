/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Declaration of Framebuffer class
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

#ifndef __H_FRAMEBUFFER
#define __H_FRAMEBUFFER

#ifdef VBOXBFE_WITHOUT_COM
# include "COMDefs.h"
#else
# include <VBox/com/defs.h>
#endif

#include "VBoxBFE.h"

class Framebuffer
{
public:
    virtual ~Framebuffer() {}

    virtual HRESULT getWidth(ULONG *width) = 0;
    virtual HRESULT getHeight(ULONG *height) = 0;
    virtual HRESULT Lock() = 0;
    virtual HRESULT Unlock() = 0;
    virtual HRESULT getAddress(uintptr_t *address) = 0;
    virtual HRESULT getBitsPerPixel(ULONG *bitsPerPixel) = 0;
    virtual HRESULT getLineSize(ULONG *lineSize) = 0;
    virtual HRESULT NotifyUpdate(ULONG x, ULONG y,
                            ULONG w, ULONG h, BOOL *finished) = 0;
    virtual HRESULT RequestResize(ULONG w, ULONG h, BOOL *finished) = 0;

    virtual HRESULT GetVisibleRegion(BYTE *aRectangles, ULONG aCount, ULONG *aCountCopied) = 0;
    virtual HRESULT SetVisibleRegion(BYTE *aRectangles, ULONG aCount) = 0;

    virtual void    repaint() = 0;
    virtual void    resize() = 0;

    virtual void    update(int x, int y, int w, int h) = 0;
    virtual bool    getFullscreen() = 0;
    virtual void    setFullscreen(bool fFullscreen) = 0;
    virtual int     getYOffset() = 0;
    virtual int     getHostXres() = 0;
    virtual int     getHostYres() = 0;
    virtual int     getHostBitsPerPixel() = 0;

#ifdef VBOX_SECURELABEL
    virtual int     initSecureLabel(uint32_t height, char *font, uint32_t pointsize) = 0;
    virtual void    setSecureLabelText(const char *text) = 0;
    virtual void    paintSecureLabel(int x, int y, int w, int h, bool fForce) = 0;
#endif


private:
};

extern Framebuffer *gFramebuffer;

#endif // __H_FRAMEBUFFER
