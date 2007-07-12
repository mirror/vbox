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
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#include "FramebufferImpl.h"
#include <iprt/semaphore.h>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

InternalFramebuffer::InternalFramebuffer()
{
    mData = NULL;
    RTSemMutexCreate(&mMutex);

    /* Default framebuffer render mode is normal (draw the entire framebuffer) */
    mRenderMode = FramebufferRenderMode_RenderModeNormal;
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
    mDepth = depth;
    mLineSize = ((width * depth + 31) / 32) * 4;
    mData = new uint8_t[mLineSize * height];
    memset(mData, 0, mLineSize * height);

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

STDMETHODIMP InternalFramebuffer::COMGETTER(ColorDepth) (ULONG *colorDepth)
{
    if (!colorDepth)
        return E_POINTER;
    *colorDepth = mDepth;
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::COMGETTER(LineSize) (ULONG *lineSize)
{
    if (!lineSize)
        return E_POINTER;
    *lineSize = mLineSize;
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::COMGETTER(PixelFormat) (FramebufferPixelFormat_T *pixelFormat)
{
    if (!pixelFormat)
        return E_POINTER;
    *pixelFormat = FramebufferPixelFormat_PixelFormatDefault;
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

/**
 * Return the current framebuffer render mode
 *
 * @returns COM status code
 * @param   renderMode  framebuffer render mode
 */
STDMETHODIMP InternalFramebuffer::COMGETTER(RenderMode) (FramebufferRenderMode_T *renderMode)
{
    if (!renderMode)
        return E_POINTER;
    *renderMode = mRenderMode;
    return S_OK;
}

/**
 * Change the current framebuffer render mode
 *
 * @returns COM status code
 * @param   renderMode  framebuffer render mode
 */
STDMETHODIMP InternalFramebuffer::COMSETTER(RenderMode) (FramebufferRenderMode_T renderMode)
{
    if (!renderMode)
        return E_POINTER;
    mRenderMode = renderMode;
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
InternalFramebuffer::RequestResize(ULONG iScreenId, FramebufferPixelFormat_T pixelFormat, BYTE *vram,
                                   ULONG lineSize, ULONG w, ULONG h,
                                   BOOL *finished)
{
    if (!finished)
        return E_POINTER;
    // no need for the caller to wait
    *finished = true;

    // allocate a new buffer
    delete mData;
    mWidth = w;
    mHeight = h;
    mLineSize = ((w * mDepth + 31) / 32) * 4;
    mData = new uint8_t[mLineSize * h];
    memset(mData, 0, mLineSize * h);

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

STDMETHODIMP InternalFramebuffer::GetVisibleRegion(ULONG * aPcRect, BYTE * aPRect)
{
    PRTRECT paRect = (PRTRECT)aPRect;

    if (!aPcRect)
        return E_POINTER;

    NOREF(paRect);
    return S_OK;
}

STDMETHODIMP InternalFramebuffer::SetVisibleRegion(ULONG aCRect, BYTE * aPRect)
{
    PRTRECT paRect = (PRTRECT)aPRect;

    if (!paRect)
        return E_POINTER;

    NOREF(paRect);
    return S_OK;
}
