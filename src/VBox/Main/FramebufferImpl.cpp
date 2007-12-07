/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
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
    if (!address)
        return E_POINTER;
    *address = mData;
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::COMGETTER(Width) (ULONG *width)
{
    if (!width)
        return E_POINTER;
    *width = mWidth;
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::COMGETTER(Height) (ULONG *height)
{
    if (!height)
        return E_POINTER;
    *height = mHeight;
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::COMGETTER(BitsPerPixel) (ULONG *bitsPerPixel)
{
    if (!bitsPerPixel)
        return E_POINTER;
    *bitsPerPixel = mBitsPerPixel;
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::COMGETTER(BytesPerLine) (ULONG *bytesPerLine)
{
    if (!bytesPerLine)
        return E_POINTER;
    *bytesPerLine = mBytesPerLine;
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::COMGETTER(PixelFormat) (ULONG *pixelFormat)
{
    if (!pixelFormat)
        return E_POINTER;
    *pixelFormat = FramebufferPixelFormat_FOURCC_RGB;
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::COMGETTER(UsesGuestVRAM) (BOOL *usesGuestVRAM)
{
    if (!usesGuestVRAM)
        return E_POINTER;
    *usesGuestVRAM = FALSE;
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::COMGETTER(HeightReduction) (ULONG *heightReduction)
{
    if (!heightReduction)
        return E_POINTER;
    /* no reduction */
    *heightReduction = 0;
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::COMGETTER(Overlay) (IFramebufferOverlay **aOverlay)
{
    if (!aOverlay)
        return E_POINTER;
    /* no overlay */
    *aOverlay = 0;
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
    if (!finished)
        return E_POINTER;
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

    if (!finished)
        return E_POINTER;
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

STDMETHODIMP InternalFramebuffer::OperationSupported(FramebufferAccelerationOperation_T operation,
                                                     BOOL *supported)
{
    if (!supported)
        return E_POINTER;
    /* no acceleration please, we're a slow fallback implementation! */
    *supported = false;
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::VideoModeSupported(ULONG width, ULONG height, ULONG bpp,
                                                     BOOL *supported)
{
    if (!supported)
        return E_POINTER;
    /* whatever you want! */
    *supported = true;
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::SolidFill(ULONG x, ULONG y, ULONG width, ULONG height,
                                             ULONG color, BOOL *handled)
{
    if (!handled)
        return E_POINTER;
    /* eek, what do you expect from us?! */
    *handled = false;
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::CopyScreenBits(ULONG xDst, ULONG yDst, ULONG xSrc, ULONG ySrc,
                                                 ULONG width, ULONG height, BOOL *handled)
{
    if (!handled)
        return E_POINTER;
    /* eek, what do you expect from us?! */
    *handled = false;
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::GetVisibleRegion(BYTE *aRectangles, ULONG aCount,
                                                   ULONG *aCountCopied)
{
    PRTRECT rects = (PRTRECT)aRectangles;

    if (!rects)
        return E_POINTER;

	/// @todo

	NOREF(rects);
	NOREF(aCount);
	NOREF(aCountCopied);

    return S_OK;
}

STDMETHODIMP InternalFramebuffer::SetVisibleRegion(BYTE *aRectangles, ULONG aCount)
{
    PRTRECT rects = (PRTRECT)aRectangles;

    if (!rects)
        return E_POINTER;

	/// @todo

	NOREF(rects);
	NOREF(aCount);

    return S_OK;
}
