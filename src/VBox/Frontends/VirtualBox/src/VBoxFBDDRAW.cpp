/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * DDRAW framebuffer implementation
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

#if defined (VBOX_GUI_USE_DDRAW)

#include "VBoxFrameBuffer.h"

#include "VBoxConsoleView.h"

#include <qapplication.h>

#include <iprt/param.h>
#include <iprt/alloc.h>

#define LOGDDRAW Log

/* @todo
 * - when paused in Guest VRAM mode after pause the screen is dimmed. because VRAM is dimmed.
 * - when GUI window is resized, somehow take this into account,  blit only visible parts.
 */

/*
 * Helpers.
 */
static LPDIRECTDRAW7 getDDRAW (void)
{
    LPDIRECTDRAW7 pDDRAW = NULL;
    LPDIRECTDRAW iface = NULL;

    HRESULT rc = DirectDrawCreate (NULL, &iface, NULL);

    if (rc != DD_OK)
    {
        LOGDDRAW(("DDRAW: Could not create DirectDraw interface rc= 0x%08X\n", rc));
    }
    else
    {
        rc = iface->QueryInterface (IID_IDirectDraw7, (void**)&pDDRAW);

        if (rc != DD_OK)
        {
            LOGDDRAW(("DDRAW: Could not query DirectDraw 7 interface rc = 0x%08X\n", rc));
        }
        else
        {
            rc = pDDRAW->SetCooperativeLevel (NULL, DDSCL_NORMAL);

            if (rc != DD_OK)
            {
                LOGDDRAW(("DDRAW: Could not set the DirectDraw cooperative level rc = 0x%08X\n", rc));
                pDDRAW->Release ();
            }
        }

        iface->Release();
    }

    return rc == DD_OK? pDDRAW: NULL;
}

static LPDIRECTDRAWSURFACE7 createPrimarySurface (LPDIRECTDRAW7 pDDRAW)
{
    LPDIRECTDRAWSURFACE7 pPrimarySurface = NULL;

    DDSURFACEDESC2 sd;
    memset (&sd, 0, sizeof (sd));
    sd.dwSize = sizeof (sd);
    sd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
    sd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    sd.dwBackBufferCount = 0;

    HRESULT rc = pDDRAW->CreateSurface (&sd, &pPrimarySurface, NULL);

    if (rc != DD_OK)
    {
        LOGDDRAW(("DDRAW: Could not create primary DirectDraw surface rc = 0x%08X\n", rc));
    }

    return rc == DD_OK? pPrimarySurface: NULL;
}

static LPDIRECTDRAWCLIPPER createClipper (LPDIRECTDRAW7 pDDRAW, HWND hwnd)
{
    LPDIRECTDRAWCLIPPER pClipper = NULL;

    HRESULT rc = pDDRAW->CreateClipper (0, &pClipper, NULL);

    if (rc != DD_OK)
    {
        LOGDDRAW(("DDRAW: Could not create DirectDraw clipper rc = 0x%08X\n", rc));
    }
    else
    {
        rc = pClipper->SetHWnd (0, hwnd);

        if (rc != DD_OK)
        {
            LOGDDRAW(("DDRAW: Could not set the HWND on clipper rc = 0x%08X\n", rc));
            pClipper->Release ();
        }
    }

    return rc == DD_OK? pClipper: NULL;
}

//
// VBoxDDRAWFrameBuffer class
/////////////////////////////////////////////////////////////////////////////

/** @class VBoxDDRAWFrameBuffer
 *
 *  The VBoxDDRAWFrameBuffer class is a class that implements the IFrameBuffer
 *  interface and uses Win32 DirectDraw to store and render VM display data.
 */

VBoxDDRAWFrameBuffer::VBoxDDRAWFrameBuffer (VBoxConsoleView *aView) :
    VBoxFrameBuffer (aView),
    mDDRAW (NULL),
    mClipper (NULL),
    mSurface (NULL),
    mPrimarySurface (NULL),
    mPixelFormat (FramebufferPixelFormat_FOURCC_RGB),
    mUsesGuestVRAM (false),
    mWndX (0),
    mWndY (0),
    mSynchronousUpdates (true)
{
    memset (&mSurfaceDesc, 0, sizeof (mSurfaceDesc));

    LOGDDRAW (("DDRAW: Creating\n"));

    /* Release all created objects if something will go wrong. */
    BOOL bReleaseObjects = TRUE;

    mDDRAW = getDDRAW ();

    if (mDDRAW)
    {
        mClipper = createClipper (mDDRAW, mView->viewport()->winId());

        if (mClipper)
        {
            mPrimarySurface = createPrimarySurface (mDDRAW);

            if (mPrimarySurface)
            {
                mPrimarySurface->SetClipper (mClipper);

                VBoxResizeEvent *re =
                    new VBoxResizeEvent (FramebufferPixelFormat_Opaque,
                                         NULL, 0, 0, 640, 480);

                if (re)
                {
                    resizeEvent (re);
                    delete re;

                    if (mSurface)
                    {
                        /* Everything was initialized. */
                        bReleaseObjects = FALSE;
                    }
                }
            }
        }
    }

    if (bReleaseObjects)
    {
        releaseObjects();
    }
}

VBoxDDRAWFrameBuffer::~VBoxDDRAWFrameBuffer()
{
    releaseObjects();
}

void VBoxDDRAWFrameBuffer::releaseObjects()
{
    deleteSurface ();

    if (mPrimarySurface)
    {
        if (mClipper)
        {
            mPrimarySurface->SetClipper (NULL);
        }

        mPrimarySurface->Release ();
        mPrimarySurface = NULL;
    }

    if (mClipper)
    {
        mClipper->Release();
        mClipper = NULL;
    }

    if (mDDRAW)
    {
        mDDRAW->Release();
        mDDRAW = NULL;
    }
}

/** @note This method is called on EMT from under this object's lock */
STDMETHODIMP VBoxDDRAWFrameBuffer::NotifyUpdate (ULONG aX, ULONG aY,
                                                 ULONG aW, ULONG aH,
                                                 BOOL *aFinished)
{
    LOGDDRAW(("DDRAW: NotifyUpdate %d,%d %dx%d\n", aX, aY, aW, aH));

    if (mSynchronousUpdates)
    {
        mView->updateContents (aX, aY, aW, aH);
    }
    else
    {
        drawRect (aX, aY, aW, aH);
    }

    *aFinished = TRUE;

    return S_OK;
}

void VBoxDDRAWFrameBuffer::paintEvent (QPaintEvent *pe)
{
    LOGDDRAW (("DDRAW: paintEvent %d,%d %dx%d\n",
               pe->rect().x(), pe->rect().y(),
               pe->rect().width(), pe->rect().height()));

    drawRect (pe->rect().x(), pe->rect().y(),
              pe->rect().width(), pe->rect().height());
}

void VBoxDDRAWFrameBuffer::resizeEvent (VBoxResizeEvent *re)
{
    LOGDDRAW (("DDRAW: resizeEvent %d, %p, %d %d %dx%d\n",
               re->pixelFormat(), re->VRAM(), re->bitsPerPixel(),
               re->bytesPerLine(), re->width(), re->height()));

    VBoxFrameBuffer::resizeEvent (re);

    bool ok = createSurface (re->pixelFormat(), re->VRAM (), re->bitsPerPixel(),
                             re->bytesPerLine (), re->width(), re->height());
    if (!ok && re->pixelFormat() != FramebufferPixelFormat_Opaque)
    {
        /* try to create a fallback surface with indirect buffer
         * (only if haven't done so already) */
        ok = createSurface (FramebufferPixelFormat_Opaque,
                            NULL, 0, 0, re->width(), re->height());
    }

    Assert (ok);

    getWindowPosition();

    mView->setBackgroundMode (Qt::NoBackground);
}

void VBoxDDRAWFrameBuffer::moveEvent (QMoveEvent *me)
{
    getWindowPosition();
}

/*
 * Private methods.
 */

/**
 * Creates a new surface in the requested format.
 * On success, returns @c true and assigns the created surface to mSurface
 * and its definition to mSurfaceDesc. On failure, returns @c false.
 *
 * If @a aPixelFormat is other than FramebufferPixelFormat_Opaque,
 * then the method will attempt to attach @a aVRAM directly to the created
 * surface. If this fails, the caller may call this method again with
 * @a aPixelFormat set to FramebufferPixelFormat_Opaque to try
 * setting up an indirect fallback buffer for the surface. This opeartion may
 * theoretically also fail.
 *
 * @note Deletes the existing surface before attemting to create a new one.
 */
bool VBoxDDRAWFrameBuffer::createSurface (ULONG aPixelFormat, uchar *aVRAM,
                                          ULONG aBitsPerPixel, ULONG aBytesPerLine,
                                          ULONG aWidth, ULONG aHeight)
{
    deleteSurface();

    DDSURFACEDESC2 sd;

    /* Prepare the surface description structure. */
    memset (&sd, 0, sizeof (sd));

    sd.dwSize  = sizeof (sd);
    sd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH |
                 DDSD_LPSURFACE | DDSD_PITCH | DDSD_PIXELFORMAT;

    sd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    sd.dwWidth = aWidth;
    sd.dwHeight = aHeight;

    /* Setup the desired pixel format on the surface. */

    sd.ddpfPixelFormat.dwSize = sizeof (sd.ddpfPixelFormat);
    sd.ddpfPixelFormat.dwFlags = DDPF_RGB;

    if (aPixelFormat == FramebufferPixelFormat_FOURCC_RGB)
    {
        /* Try to use the guest VRAM directly */

        switch (aBitsPerPixel)
        {
            case 32:
                sd.ddpfPixelFormat.dwRGBBitCount = 32;
                sd.ddpfPixelFormat.dwRBitMask = 0x00FF0000;
                sd.ddpfPixelFormat.dwGBitMask = 0x0000FF00;
                sd.ddpfPixelFormat.dwBBitMask = 0x000000FF;
                break;
            case 24:
                sd.ddpfPixelFormat.dwRGBBitCount = 24;
                sd.ddpfPixelFormat.dwRBitMask = 0x00FF0000;
                sd.ddpfPixelFormat.dwGBitMask = 0x0000FF00;
                sd.ddpfPixelFormat.dwBBitMask = 0x000000FF;
                break;
            case 16:
                sd.ddpfPixelFormat.dwRGBBitCount = 16;
                sd.ddpfPixelFormat.dwRBitMask = 0xF800;
                sd.ddpfPixelFormat.dwGBitMask = 0x07E0;
                sd.ddpfPixelFormat.dwBBitMask = 0x001F;
                break;
            default:
                /* we don't directly support any other color depth */
                return false;
        }

        sd.lPitch = (LONG) aBytesPerLine;

        sd.lpSurface = aVRAM;
        mUsesGuestVRAM = true;

        mPixelFormat = FramebufferPixelFormat_FOURCC_RGB;
    }
    else
    if (aPixelFormat != FramebufferPixelFormat_Opaque)
    {
        /* we don't directly support any other pixel format */
        return false;
    }
    else
    {
        /* for the Opaque format, we use the indirect memory buffer as a
         * 32 bpp surface. */

        sd.ddpfPixelFormat.dwRGBBitCount = 32;
        sd.ddpfPixelFormat.dwRBitMask = 0x00FF0000;
        sd.ddpfPixelFormat.dwGBitMask = 0x0000FF00;
        sd.ddpfPixelFormat.dwBBitMask = 0x000000FF;

        sd.lPitch = sd.dwWidth * 4;

        /* Allocate the memory buffer for the surface */
        sd.lpSurface = RTMemAlloc (sd.lPitch * sd.dwHeight);
        if (sd.lpSurface == NULL)
        {
            LOGDDRAW (("DDRAW: could not allocate memory for surface.\n"));
            return false;
        }
        mUsesGuestVRAM = false;

        mPixelFormat = FramebufferPixelFormat_FOURCC_RGB;
    }

    /* create the surface */
    HRESULT rc = mDDRAW->CreateSurface (&sd, &mSurface, NULL);

    if (rc != DD_OK)
    {
        LOGDDRAW (("DDRAW: Could not create DirectDraw surface, rc=0x%08X\n", rc));
        deleteSurface();
        return false;
    }

    /* Initialize the surface description member. It will be used to obtain
     * address, bpp and bpl. */
    mSurfaceDesc = sd;

    LOGDDRAW(("DDRAW: Created %s surface: format = %d, address = %p\n",
              mUsesGuestVRAM ? "GuestVRAM": "system memory",
              aPixelFormat, address ()));

    if (!mUsesGuestVRAM)
    {
        /* Clear just created surface. */
        memset (address(), 0, bytesPerLine() * height());
    }

    return true;
}

void VBoxDDRAWFrameBuffer::deleteSurface ()
{
    if (mSurface)
    {
        mSurface->Release ();
        mSurface = NULL;

        if (!mUsesGuestVRAM)
        {
            RTMemFree (mSurfaceDesc.lpSurface);
        }

        memset (&mSurfaceDesc, '\0', sizeof (mSurfaceDesc));
        mUsesGuestVRAM = false;
    }
}

/**
 * Draws a rectangular area of guest screen DDRAW surface onto the
 * host screen primary surface.
 */
void VBoxDDRAWFrameBuffer::drawRect (ULONG x, ULONG y, ULONG w, ULONG h)
{
    LOGDDRAW (("DDRAW: drawRect: %d,%d, %dx%d\n", x, y, w, h));

    if (mSurface && w > 0 && h > 0)
    {
        RECT rectSrc;
        RECT rectDst;

        rectSrc.left   = x;
        rectSrc.right  = x + w;
        rectSrc.top    = y;
        rectSrc.bottom = y + h;

        rectDst.left   = mWndX + x;
        rectDst.right  = rectDst.left + w;
        rectDst.top    = mWndY + y;
        rectDst.bottom = rectDst.top + h;

        /* DDBLT_ASYNC performs this blit asynchronously through the
         *   first in, first out (FIFO) hardware in the order received.
         *   If no room is available in the FIFO hardware, the call fails.
         * DDBLT_WAIT waits if blitter is busy, and returns as soon as the
         *   blit can be set up or another error occurs.
         *
         * I assume that DDBLT_WAIT will also wait for a room in the FIFO.
         */
        HRESULT rc = mPrimarySurface->Blt (&rectDst, mSurface, &rectSrc, DDBLT_ASYNC | DDBLT_WAIT, NULL);

        if (rc != DD_OK)
        {
            /* Repeat without DDBLT_ASYNC. */
            LOGDDRAW(("DDRAW: drawRect: async blit failed rc = 0x%08X\n", rc));
            rc = mPrimarySurface->Blt (&rectDst, mSurface, &rectSrc, DDBLT_WAIT, NULL);

            if (rc != DD_OK)
            {
                LOGDDRAW(("DDRAW: drawRect: sync blit failed rc = 0x%08X\n", rc));
            }
        }
    }

    return;
}

void VBoxDDRAWFrameBuffer::getWindowPosition (void)
{
//    if (mPrimarySurface)
//    {
//        /* Lock surface to synchronize with Blt in drawRect. */
//        DDSURFACEDESC2 sd;
//        memset (&sd, 0, sizeof (sd));
//        sd.dwSize  = sizeof (sd);
//
//        HRESULT rc = mPrimarySurface->Lock (NULL, &sd, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);
//        LOGDDRAW(("DDRAW: getWindowPosition rc = 0x%08X\n", rc));
//    }

    RECT rect;
    GetWindowRect (mView->viewport()->winId(), &rect);
    mWndX = rect.left;
    mWndY = rect.top;

//    if (mPrimarySurface)
//    {
//        mPrimarySurface->Unlock (NULL);
//    }
}

#endif /* defined (VBOX_GUI_USE_DDRAW) */
