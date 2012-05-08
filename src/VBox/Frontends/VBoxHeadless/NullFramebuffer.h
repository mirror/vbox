/*
 * Copyright (C) 2007-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __NULL_FRAMEBUFFER_H
#define __NULL_FRAMEBUFFER_H

#include <VBox/com/VirtualBox.h>
#include <iprt/alloc.h>

class NullFB : VBOX_SCRIPTABLE_IMPL(IFramebuffer)
{
public:
    NullFB()
    :
    mScreen(NULL), mBuffer(NULL),
    mUsesGuestVRAM(false),
    mWidth(0), mHeight(0)
#ifndef VBOX_WITH_XPCOM
    , refcnt(0)
#endif
    {}
    virtual ~NullFB()
    {
        if (mBuffer)
            RTMemFree(mBuffer);
    }

#ifndef VBOX_WITH_XPCOM
    STDMETHOD_(ULONG, AddRef)() {
        return ::InterlockedIncrement (&refcnt);
    }
    STDMETHOD_(ULONG, Release)() {
        long cnt = ::InterlockedDecrement (&refcnt);
        if (cnt == 0)
            delete this;
        return cnt;
    }
#endif

    BEGIN_COM_MAP(NullFB)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IFramebuffer)
    END_COM_MAP()

    // public methods only for internal purposes
    HRESULT init ()
    {

        RequestResize(0, FramebufferPixelFormat_Opaque, 0, 0, 0, 640, 480, 0);
        return S_OK;
    }

    STDMETHOD(COMGETTER(Width))(ULONG *width)
    {
        if (!width)
            return E_POINTER;
        *width = mWidth;
        return S_OK;
    }
    STDMETHOD(COMGETTER(Height))(ULONG *height)
    {
        if (!height)
            return E_POINTER;
        *height = mHeight;
        return S_OK;
    }
    STDMETHOD(Lock)()
    {
        return S_OK;
    }
    STDMETHOD(Unlock)()
    {
        return S_OK;
    }
    STDMETHOD(COMGETTER(Address))(BYTE **address)
    {
        if (!address)
            return E_POINTER;
        *address = mScreen;
        return S_OK;
    }
    STDMETHOD(COMGETTER(BitsPerPixel))(ULONG *bitsPerPixel)
    {
        if (!bitsPerPixel)
            return E_POINTER;
        *bitsPerPixel = mBitsPerPixel;
        return S_OK;
    }
    STDMETHOD(COMGETTER(BytesPerLine))(ULONG *bytesPerLine)
    {
        if (!bytesPerLine)
            return E_POINTER;
        *bytesPerLine = mBytesPerLine;
        return S_OK;
    }
    STDMETHOD(COMGETTER(PixelFormat)) (ULONG *pixelFormat)
    {
        if (!pixelFormat)
            return E_POINTER;
        *pixelFormat = mPixelFormat;
        return S_OK;
    }
    STDMETHOD(COMGETTER(UsesGuestVRAM)) (BOOL *usesGuestVRAM)
    {
        if (!usesGuestVRAM)
            return E_POINTER;
        *usesGuestVRAM = mUsesGuestVRAM;
        return S_OK;
    }
    STDMETHOD(COMGETTER(HeightReduction)) (ULONG *heightReduction)
    {
        if (!heightReduction)
            return E_POINTER;
        /* no reduction */
        *heightReduction = 0;
        return S_OK;
    }
    STDMETHOD(COMGETTER(Overlay)) (IFramebufferOverlay **aOverlay)
    {
        if (!aOverlay)
            return E_POINTER;
        *aOverlay = 0;
        return S_OK;
    }
    STDMETHOD(COMGETTER(WinId)) (LONG64 *winId)
    {
        if (!winId)
            return E_POINTER;
        *winId = 0;
        return S_OK;
    }

    STDMETHOD(NotifyUpdate)(ULONG x, ULONG y, ULONG w, ULONG h)
    {
        return S_OK;
    }
    STDMETHOD(RequestResize)(ULONG aScreenId,
                             ULONG pixelFormat,
                             BYTE *vram,
                             ULONG bitsPerPixel,
                             ULONG bytesPerLine,
                             ULONG w,
                             ULONG h,
                             BOOL *finished)
    {
        if (mBuffer)
        {
            RTMemFree(mBuffer);
            mBuffer = NULL;
        }

        mWidth = w;
        mHeight = h;
        mPixelFormat = pixelFormat;
        if (mPixelFormat == FramebufferPixelFormat_Opaque)
        {
            mBitsPerPixel = 32;
            mBytesPerLine = w * 4;
            mBuffer = (uint8_t*)RTMemAllocZ(mBytesPerLine * h);
            mScreen = mBuffer;
            mUsesGuestVRAM = false;
        }
        else
        {
            mBytesPerLine = bytesPerLine;
            mBitsPerPixel = bitsPerPixel;
            mScreen       = (uint8_t*)vram;
            mUsesGuestVRAM = true;
        }

        if (finished)
            *finished = true;
        return S_OK;
    }
    STDMETHOD(VideoModeSupported)(ULONG width, ULONG height, ULONG bpp, BOOL *supported)
    {
        if (!supported)
            return E_POINTER;
        *supported = true;
        return S_OK;
    }
    STDMETHOD(GetVisibleRegion)(BYTE *rectangles, ULONG count, ULONG *countCopied)
    {
        if (!rectangles)
            return E_POINTER;
        *rectangles = 0;
        return S_OK;
    }
    STDMETHOD(SetVisibleRegion)(BYTE *rectangles, ULONG count)
    {
        if (!rectangles)
            return E_POINTER;
        return S_OK;
    }

    STDMETHOD(ProcessVHWACommand)(BYTE *pCommand)
    {
        return E_NOTIMPL;
    }

private:
    /** Guest framebuffer pixel format */
    ULONG mPixelFormat;
    /** Guest framebuffer color depth */
    ULONG mBitsPerPixel;
    /** Guest framebuffer line length */
    ULONG mBytesPerLine;
    /* VRAM pointer */
    uint8_t *mScreen;
    /* VRAM buffer */
    uint8_t *mBuffer;
    bool     mUsesGuestVRAM;

    ULONG mWidth, mHeight;

#ifndef VBOX_WITH_XPCOM
    long refcnt;
#endif
};

#endif // __NULL_FRAMEBUFFER_H
