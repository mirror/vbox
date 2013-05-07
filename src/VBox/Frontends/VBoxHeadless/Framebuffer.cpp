/** @file
 *
 * VBox Remote Desktop Framebuffer
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

#include "Framebuffer.h"

#include <iprt/alloc.h>

#define LOG_GROUP LOG_GROUP_GUI
#include <VBox/log.h>

/*
 * VRDP server frame buffer
 */

#ifdef VBOX_WITH_XPCOM
#include <nsMemory.h>
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(VRDPFramebuffer, IFramebuffer)
NS_DECL_CLASSINFO(VRDPFramebuffer)
#endif

VRDPFramebuffer::VRDPFramebuffer()
{
#if defined (RT_OS_WINDOWS)
    refcnt = 0;
#endif /* RT_OS_WINDOWS */

    mBuffer = NULL;

    RTCritSectInit (&m_CritSect);

    // start with a standard size
    RequestResize(0, FramebufferPixelFormat_Opaque,
                  (ULONG) NULL, 0, 0, 640, 480, NULL);
}

VRDPFramebuffer::~VRDPFramebuffer()
{
    if (mBuffer)
    {
        RTMemFree (mBuffer);
    }

    RTCritSectDelete (&m_CritSect);
}

STDMETHODIMP VRDPFramebuffer::COMGETTER(Width)(ULONG *width)
{
    if (!width)
        return E_INVALIDARG;
    *width = mWidth;
    return S_OK;
}

STDMETHODIMP VRDPFramebuffer::COMGETTER(Height)(ULONG *height)
{
    if (!height)
        return E_INVALIDARG;
    *height = mHeight;
    return S_OK;
}

STDMETHODIMP VRDPFramebuffer::Lock()
{
    RTCritSectEnter (&m_CritSect);
    return S_OK;
}

STDMETHODIMP VRDPFramebuffer::Unlock()
{
    RTCritSectLeave (&m_CritSect);
    return S_OK;
}

STDMETHODIMP VRDPFramebuffer::COMGETTER(Address)(BYTE **address)
{
    if (!address)
        return E_INVALIDARG;
    *address = mScreen;
    return S_OK;
}

STDMETHODIMP VRDPFramebuffer::COMGETTER(BitsPerPixel)(ULONG *bitsPerPixel)
{
    if (!bitsPerPixel)
        return E_INVALIDARG;
    *bitsPerPixel = mBitsPerPixel;
    return S_OK;
}

STDMETHODIMP VRDPFramebuffer::COMGETTER(BytesPerLine)(ULONG *bytesPerLine)
{
    if (!bytesPerLine)
        return E_INVALIDARG;
    *bytesPerLine = mBytesPerLine;
    return S_OK;
}

STDMETHODIMP VRDPFramebuffer::COMGETTER(PixelFormat) (ULONG *pixelFormat)
{
    if (!pixelFormat)
        return E_POINTER;
    *pixelFormat = mPixelFormat;
    return S_OK;
}

STDMETHODIMP VRDPFramebuffer::COMGETTER(UsesGuestVRAM) (BOOL *usesGuestVRAM)
{
    if (!usesGuestVRAM)
        return E_POINTER;
    *usesGuestVRAM = mUsesGuestVRAM;
    return S_OK;
}

STDMETHODIMP VRDPFramebuffer::COMGETTER(HeightReduction) (ULONG *heightReduction)
{
    if (!heightReduction)
        return E_POINTER;
    /* no reduction at all */
    *heightReduction = 0;
    return S_OK;
}

STDMETHODIMP VRDPFramebuffer::COMGETTER(Overlay) (IFramebufferOverlay **aOverlay)
{
    if (!aOverlay)
        return E_POINTER;
    /* overlays are not yet supported */
    *aOverlay = 0;
    return S_OK;
}

STDMETHODIMP VRDPFramebuffer::COMGETTER(WinId) (LONG64 *winId)
{
    if (!winId)
        return E_POINTER;
    *winId = 0;
    return S_OK;
}

STDMETHODIMP VRDPFramebuffer::NotifyUpdate(ULONG x, ULONG y,
                                           ULONG w, ULONG h)
{
    return S_OK;
}

STDMETHODIMP VRDPFramebuffer::RequestResize(ULONG aScreenId, ULONG pixelFormat, BYTE *vram,
                                            ULONG bitsPerPixel, ULONG bytesPerLine,
                                            ULONG w, ULONG h,
                                            BOOL *finished)
{
    /* Agree to requested format for LFB modes and use guest VRAM directly, thus avoiding
     * unnecessary memcpy in VGA device.
     */

    Log(("pixelFormat = %08X, vram = %p, bpp = %d, bpl = 0x%08X, %dx%d\n",
         pixelFormat, vram, bitsPerPixel, bytesPerLine, w, h));

    /* Free internal buffer. */
    if (mBuffer)
    {
        RTMemFree (mBuffer);
        mBuffer = NULL;
    }

    mUsesGuestVRAM = FALSE;

    mWidth = w;
    mHeight = h;

    if (pixelFormat == FramebufferPixelFormat_FOURCC_RGB)
    {
        switch (bitsPerPixel)
        {
            case 32:
            case 24:
            case 16:
                mUsesGuestVRAM = TRUE;
                break;

            default:
                break;
        }
    }

    if (mUsesGuestVRAM)
    {
        mScreen = vram;
        mBitsPerPixel = bitsPerPixel;
        mBytesPerLine = bytesPerLine;
        mPixelFormat = FramebufferPixelFormat_FOURCC_RGB;

        Log (("Using guest VRAM directly, %d BPP\n", mBitsPerPixel));
    }
    else
    {
        mBitsPerPixel = 32;
        mBytesPerLine = w * 4; /* Here we have 32 BPP */

        if (mBytesPerLine > 0 && h > 0) /* Check for nul dimensions. */
        {
            mBuffer = RTMemAllocZ(mBytesPerLine * h);
        }

        mScreen = (uint8_t *)mBuffer;

        Log(("Using internal buffer, %d BPP\n", mBitsPerPixel));
    }

    if (!mScreen)
    {
        Log(("No screen. BPP = %d, w = %d, h = %d!!!\n", mBitsPerPixel, w, h));

        /* Just reset everything. */
        mPixelFormat = FramebufferPixelFormat_Opaque;

        mWidth = 0;
        mHeight = 0;
        mBitsPerPixel = 0;
        mBytesPerLine = 0;
        mUsesGuestVRAM = FALSE;
    }

    /* Inform the caller that the operation was successful. */

    if (finished)
        *finished = TRUE;

    return S_OK;
}

/**
 * Returns whether we like the given video mode.
 *
 * @returns COM status code
 * @param   width     video mode width in pixels
 * @param   height    video mode height in pixels
 * @param   bpp       video mode bit depth in bits per pixel
 * @param   supported pointer to result variable
 */
STDMETHODIMP VRDPFramebuffer::VideoModeSupported(ULONG width, ULONG height, ULONG bpp, BOOL *supported)
{
    if (!supported)
        return E_POINTER;
    *supported = TRUE;
    return S_OK;
}

STDMETHODIMP VRDPFramebuffer::GetVisibleRegion(BYTE *aRectangles, ULONG aCount,
                                               ULONG *aCountCopied)
{
    PRTRECT rects = (PRTRECT)aRectangles;

    if (!rects)
        return E_POINTER;

    /// @todo

    NOREF(aCount);
    NOREF(aCountCopied);

    return S_OK;
}

STDMETHODIMP VRDPFramebuffer::SetVisibleRegion(BYTE *aRectangles, ULONG aCount)
{
    PRTRECT rects = (PRTRECT)aRectangles;

    if (!rects)
        return E_POINTER;

    /// @todo

    NOREF(aCount);

    return S_OK;
}

STDMETHODIMP VRDPFramebuffer::ProcessVHWACommand(BYTE *pCommand)
{
    return E_NOTIMPL;
}

STDMETHODIMP VRDPFramebuffer::Notify3DEvent(ULONG uType, BYTE *pReserved)
{
    return E_NOTIMPL;
}
