/** @file
 *
 * VBox Remote Desktop Framebuffer.
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

#ifndef __VRDP__FRAMEBUFFER__H
#define __VRDP__FRAMEBUFFER__H

#include <VBox/com/VirtualBox.h>

#include <iprt/critsect.h>

class VRDPFramebuffer :
    VBOX_SCRIPTABLE_IMPL(IFramebuffer)
{
public:
    VRDPFramebuffer();
    virtual ~VRDPFramebuffer();

#ifndef VBOX_WITH_XPCOM
    STDMETHOD_(ULONG, AddRef)() {
        return ::InterlockedIncrement (&refcnt);
    }
    STDMETHOD_(ULONG, Release)()
    {
        long cnt = ::InterlockedDecrement (&refcnt);
        if (cnt == 0)
            delete this;
        return cnt;
    }
#endif
    VBOX_SCRIPTABLE_DISPATCH_IMPL(IFramebuffer)

    NS_DECL_ISUPPORTS

    STDMETHOD(COMGETTER(Width))(ULONG *width);
    STDMETHOD(COMGETTER(Height))(ULONG *height);
    STDMETHOD(Lock)();
    STDMETHOD(Unlock)();
    STDMETHOD(COMGETTER(Address))(BYTE **address);
    STDMETHOD(COMGETTER(BitsPerPixel))(ULONG *bitsPerPixel);
    STDMETHOD(COMGETTER(BytesPerLine))(ULONG *bytesPerLine);
    STDMETHOD(COMGETTER(PixelFormat)) (ULONG *pixelFormat);
    STDMETHOD(COMGETTER(UsesGuestVRAM)) (BOOL *usesGuestVRAM);
    STDMETHOD(COMGETTER(HeightReduction)) (ULONG *heightReduction);
    STDMETHOD(COMGETTER(Overlay)) (IFramebufferOverlay **aOverlay);
    STDMETHOD(COMGETTER(WinId)) (LONG64 *winId);

    STDMETHOD(NotifyUpdate)(ULONG x, ULONG y, ULONG w, ULONG h);
    STDMETHOD(RequestResize)(ULONG aScreenId, ULONG pixelFormat, BYTE *vram,
                             ULONG bitsPerPixel, ULONG bytesPerLine, ULONG w, ULONG h,
                             BOOL *finished);
    STDMETHOD(VideoModeSupported)(ULONG width, ULONG height, ULONG bpp, BOOL *supported);

    STDMETHOD(GetVisibleRegion)(BYTE *aRectangles, ULONG aCount, ULONG *aCountCopied);
    STDMETHOD(SetVisibleRegion)(BYTE *aRectangles, ULONG aCount);

    STDMETHOD(ProcessVHWACommand)(BYTE *pCommand);

private:
    /* If the format is Opaque, then internal memory buffer is used.
     * Otherwise guest VRAM is used directly.
     */
    ULONG mPixelFormat;

    void *mBuffer;

    uint8_t *mScreen;
    ULONG mWidth;
    ULONG mHeight;
    ULONG mBitsPerPixel;
    ULONG mBytesPerLine;

    BOOL mUsesGuestVRAM;

    RTCRITSECT m_CritSect;

#ifndef VBOX_WITH_XPCOM
    long refcnt;
#endif
};


#endif /* __VRDP__FRAMEBUFFER__H */
