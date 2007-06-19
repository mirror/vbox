/** @file
 *
 * VBox frontends: Framebuffer (FB, DirectFB):
 * Declaration of VBoxDirectFB class
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
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
    NS_IMETHOD GetColorDepth(uint32_t *colorDepth);
    NS_IMETHOD GetLineSize(uint32_t *lineSize);
    NS_IMETHOD GetPixelFormat(FramebufferPixelFormat_T *pixelFormat);
    NS_IMETHOD NotifyUpdate(uint32_t x, uint32_t y,
                           uint32_t w, uint32_t h, PRBool *finished);
    NS_IMETHOD RequestResize(ULONG aScreenId, FramebufferPixelFormat_T pixelFormat, uint32_t vram, uint32_t lineSize, uint32_t w, uint32_t h,
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

