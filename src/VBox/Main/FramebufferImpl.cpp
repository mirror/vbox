/** @file
 *
 * VirtualBox COM class implementation
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

#include "FramebufferImpl.h"
#include <iprt/semaphore.h>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

InternalFramebuffer::InternalFramebuffer()
{
    mData = NULL;
    RTSemMutexCreate(&mMutex);
}

InternalFramebuffer::~InternalFramebuffer()
{
    RTSemMutexDestroy(mMutex);
    if (mData)
        delete mData;
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

HRESULT InternalFramebuffer::init(ULONG width, ULONG height, ULONG depth)
{
    mWidth = width;
    mHeight = height;
    mBitsPerPixel = depth;
    mBytesPerLine = ((width * depth + 31) / 32) * 4;
    mData = new uint8_t [mBytesPerLine * height];
    memset (mData, 0, mBytesPerLine * height);

    return S_OK;
}

// IFramebuffer properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP InternalFramebuffer::COMGETTER(Address) (BYTE **address)
{
    CheckComArgOutPointerValid(address);
    *address = mData;
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::COMGETTER(Width) (ULONG *width)
{
    CheckComArgOutPointerValid(width);
    *width = mWidth;
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::COMGETTER(Height) (ULONG *height)
{
    CheckComArgOutPointerValid(height);
    *height = mHeight;
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::COMGETTER(BitsPerPixel) (ULONG *bitsPerPixel)
{
    CheckComArgOutPointerValid(bitsPerPixel);
    *bitsPerPixel = mBitsPerPixel;
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::COMGETTER(BytesPerLine) (ULONG *bytesPerLine)
{
    CheckComArgOutPointerValid(bytesPerLine);
    *bytesPerLine = mBytesPerLine;
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::COMGETTER(PixelFormat) (ULONG *pixelFormat)
{
    CheckComArgOutPointerValid(pixelFormat);
    *pixelFormat = FramebufferPixelFormat_FOURCC_RGB;
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::COMGETTER(UsesGuestVRAM) (BOOL *usesGuestVRAM)
{
    CheckComArgOutPointerValid(usesGuestVRAM);
    *usesGuestVRAM = FALSE;
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::COMGETTER(HeightReduction) (ULONG *heightReduction)
{
    CheckComArgOutPointerValid(heightReduction);
    /* no reduction */
    *heightReduction = 0;
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::COMGETTER(Overlay) (IFramebufferOverlay **aOverlay)
{
    CheckComArgOutPointerValid(aOverlay);
    /* no overlay */
    *aOverlay = 0;
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::COMGETTER(WinId) (ULONG64 *winId)
{
    CheckComArgOutPointerValid(winId);
    *winId = 0;
    return S_OK;
}

// IFramebuffer methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP InternalFramebuffer::Lock()
{
    RTSemMutexRequest(mMutex, RT_INDEFINITE_WAIT);
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::Unlock()
{
    RTSemMutexRelease(mMutex);
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::NotifyUpdate(ULONG x, ULONG y,
                                               ULONG w, ULONG h,
                                               BOOL *finished)
{
    CheckComArgOutPointerValid(finished);
    // no need for the caller to wait
    *finished = true;
    return S_OK;
}

STDMETHODIMP
InternalFramebuffer::RequestResize(ULONG iScreenId, ULONG pixelFormat, BYTE *vram,
                                   ULONG bpp, ULONG bpl, ULONG w, ULONG h,
                                   BOOL *finished)
{
    NOREF (bpp);
    NOREF (bpl);

    CheckComArgOutPointerValid(finished);
    // no need for the caller to wait
    *finished = true;

    // allocate a new buffer
    delete mData;
    mWidth = w;
    mHeight = h;
    mBytesPerLine = ((w * mBitsPerPixel + 31) / 32) * 4;
    mData = new uint8_t [mBytesPerLine * h];
    memset (mData, 0, mBytesPerLine * h);

    return S_OK;
}

STDMETHODIMP InternalFramebuffer::VideoModeSupported(ULONG width, ULONG height, ULONG bpp,
                                                     BOOL *supported)
{
    CheckComArgOutPointerValid(supported);
    /* whatever you want! */
    *supported = true;
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::GetVisibleRegion(BYTE *aRectangles, ULONG aCount,
                                                   ULONG *aCountCopied)
{
    CheckComArgOutPointerValid(aRectangles);

    PRTRECT rects = (PRTRECT)aRectangles;

    /// @todo

    NOREF(rects);
    NOREF(aCount);
    NOREF(aCountCopied);

    return S_OK;
}

STDMETHODIMP InternalFramebuffer::SetVisibleRegion(BYTE *aRectangles, ULONG aCount)
{
    CheckComArgOutPointerValid(aRectangles);

    PRTRECT rects = (PRTRECT)aRectangles;

    /// @todo

    NOREF(rects);
    NOREF(aCount);

    return S_OK;
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
