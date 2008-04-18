/** @file
 *
 * VBox frontends: Framebuffer (FB, DirectFB):
 * Declaration of VBoxDirectFB class
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

#include "VBoxFB.h"

class VBoxDirectFB : public IFramebuffer
{
public:
    VBoxDirectFB(IDirectFB *aDFB, IDirectFBSurface *aSurface);
    virtual ~VBoxDirectFB();

    NS_DECL_ISUPPORTS

    NS_IMETHOD GetWidth(uint32 *width);
    NS_IMETHOD GetHeight(uint32_t *height);
    NS_IMETHOD Lock();
    NS_IMETHOD Unlock();
    NS_IMETHOD GetAddress(uint32_t *address);
    NS_IMETHOD GetBitsPerPixel(uint32_t *bitsPerPixel);
    NS_IMETHOD GetBytesPerLine(uint32_t *bytesPerLine);
    NS_IMETHOD GetPixelFormat(ULONG *pixelFormat);
    NS_IMETHOD GetUsesGuestVRAM(BOOL *usesGuestVRAM);
    NS_IMETHOD NotifyUpdate(uint32_t x, uint32_t y,
                           uint32_t w, uint32_t h, PRBool *finished);
    NS_IMETHOD RequestResize(ULONG aScreenId, ULONG pixelFormat, uint32_t vram,
                             uint32_t bitsPerPixel, uint32_t bytesPerLine,
                             uint32_t w, uint32_t h,
                             PRBool *finished);
private:
    int createSurface(uint32_t w, uint32_t h);

    IDirectFB *dfb;
    IDirectFBSurface *surface;
    uint32_t screenWidth;
    uint32_t screenHeight;
    IDirectFBSurface *fbInternalSurface;
    void *fbBufferAddress;
    uint32_t fbWidth;
    uint32_t fbHeight;
    uint32_t fbPitch;
    int fbSurfaceLocked;
};


#endif // __H_FRAMEBUFFER

