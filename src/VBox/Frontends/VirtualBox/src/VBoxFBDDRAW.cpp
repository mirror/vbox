/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * DDRAW framebuffer implementation
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
    sd.dwFlags = DDSD_CAPS;
    sd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

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
    mPixelFormat (FramebufferPixelFormat_PixelFormatOpaque),
    mGuestVRAMSurface (FALSE),
    mWndX (0),
    mWndY (0),
    mSynchronousUpdates (TRUE)
{
    memset (&mSurfaceDesc, 0, sizeof (mSurfaceDesc));

    LOGDDRAW(("DDRAW: Creating\n"));

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
                    new VBoxResizeEvent (FramebufferPixelFormat_PixelFormatOpaque,
                                         NULL, 0, 640, 480);

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
    LOGDDRAW(("DDRAW: paintEvent %d,%d %dx%d\n", pe->rect().x(), pe->rect().y(), pe->rect().width(), pe->rect().height()));

    drawRect (pe->rect().x(), pe->rect().y(), pe->rect().width(), pe->rect().height());
}

void VBoxDDRAWFrameBuffer::resizeEvent (VBoxResizeEvent *re)
{
    LOGDDRAW(("DDRAW: resizeEvent %d, %p, %d %dx%d\n", re->pixelFormat (), re->vram (), re->lineSize (), re->width(), re->height()));

    VBoxFrameBuffer::resizeEvent (re);

    setupSurface (re->pixelFormat (), re->vram (), re->lineSize (), re->width(), re->height());

    getWindowPosition ();

    mView->setBackgroundMode (Qt::NoBackground);
}

void VBoxDDRAWFrameBuffer::moveEvent (QMoveEvent *me)
{
    getWindowPosition ();
}


/*
 * Private methods.
 */

/* Setups a surface with requested format or a default surface if
 * requested format is not supportred.
 * Assigns VBoxDDRAWFrameBuffer::mSurface to the new surface
 * and VBoxDDRAWFrameBuffer::mPixelFormat to format of the created surface.
 */
void VBoxDDRAWFrameBuffer::setupSurface (FramebufferPixelFormat_T pixelFormat, uchar *pvVRAM, ULONG lineSize, ULONG w, ULONG h)
{
    /* Check requested pixel format. */
    switch (pixelFormat)
    {
        case FramebufferPixelFormat_PixelFormatRGB32:
        case FramebufferPixelFormat_PixelFormatRGB24:
        case FramebufferPixelFormat_PixelFormatRGB16:
        {
            /* Supported formats. Do nothing. */
            Assert(lineSize >= w);
        } break;

        default:
        {
            /* Unsupported format leads to use of the default format. */
            pixelFormat = FramebufferPixelFormat_PixelFormatOpaque;
        }
    }

    recreateSurface (pixelFormat, pvVRAM, lineSize, w, h);

    if (!mSurface && pixelFormat != FramebufferPixelFormat_PixelFormatOpaque)
    {
        /* Unable to create a new surface. Try to create a default format surface. */
        pixelFormat = FramebufferPixelFormat_PixelFormatOpaque;
        recreateSurface (pixelFormat, NULL, 0, w, h);
    }

    mPixelFormat = pixelFormat;
}

/* Deletes existing and creates a new surface with requested format.
 * Assigns VBoxDDRAWFrameBuffer::mSurface to the new surface if
 * the requested format is supported and the surface was successfully created.
 */
void VBoxDDRAWFrameBuffer::recreateSurface (FramebufferPixelFormat_T pixelFormat, uchar *pvVRAM, ULONG lineSize, ULONG w, ULONG h)
{
    HRESULT rc;

    deleteSurface ();

    DDSURFACEDESC2 sd;

    /* Prepare the surface description structure. */
    memset (&sd, 0, sizeof (sd));

    sd.dwSize  = sizeof (sd);
    sd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH |
                 DDSD_LPSURFACE | DDSD_PITCH | DDSD_PIXELFORMAT;

    sd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    sd.dwWidth = w;
    sd.dwHeight = h;

    if (pixelFormat == FramebufferPixelFormat_PixelFormatOpaque)
    {
        /* Default format is a 32 bpp surface. */
        sd.lPitch = sd.dwWidth * 4;
    }
    else
    {
        sd.lPitch = lineSize;
    }

    /* Setup the desired pixel format on the surface. */

    sd.ddpfPixelFormat.dwSize = sizeof (sd.ddpfPixelFormat);
    sd.ddpfPixelFormat.dwFlags = DDPF_RGB;

    switch (pixelFormat)
    {
        case FramebufferPixelFormat_PixelFormatOpaque:
        case FramebufferPixelFormat_PixelFormatRGB32:
        {
            sd.ddpfPixelFormat.dwRGBBitCount = 32;
            sd.ddpfPixelFormat.dwRBitMask = 0x00FF0000;
            sd.ddpfPixelFormat.dwGBitMask = 0x0000FF00;
            sd.ddpfPixelFormat.dwBBitMask = 0x000000FF;
        } break;

        case FramebufferPixelFormat_PixelFormatRGB24:
        {
            sd.ddpfPixelFormat.dwRGBBitCount = 24;
            sd.ddpfPixelFormat.dwRBitMask = 0x00FF0000;
            sd.ddpfPixelFormat.dwGBitMask = 0x0000FF00;
            sd.ddpfPixelFormat.dwBBitMask = 0x000000FF;
        } break;

        case FramebufferPixelFormat_PixelFormatRGB16:
        {
            sd.ddpfPixelFormat.dwRGBBitCount = 16;
            sd.ddpfPixelFormat.dwRBitMask = 0xF800;
            sd.ddpfPixelFormat.dwGBitMask = 0x07E0;
            sd.ddpfPixelFormat.dwBBitMask = 0x001F;
        } break;
    }

    /* Allocate surface memory. */
    if (pvVRAM != NULL && pixelFormat != FramebufferPixelFormat_PixelFormatOpaque)
    {
        sd.lpSurface = pvVRAM;
        mGuestVRAMSurface = TRUE;
    }
    else
    {
        sd.lpSurface = RTMemAlloc (sd.lPitch * sd.dwHeight);
    }

    if (!sd.lpSurface)
    {
        LOGDDRAW(("DDRAW: could not allocate memory for surface.\n"));
        return;
    }

    rc = mDDRAW->CreateSurface (&sd, &mSurface, NULL);

    if (rc != DD_OK)
    {
        LOGDDRAW(("DDRAW: Could not create DirectDraw surface rc = 0x%08X\n", rc));
    }
    else
    {
        /* Initialize the surface description. It will be used to obtain address, lineSize and bpp. */
        mSurfaceDesc = sd;

        LOGDDRAW(("DDRAW: Created %s surface: format = %d, address = %p\n",
                  mGuestVRAMSurface ? "GuestVRAM": "system memory",
                  pixelFormat, address ()));
    }

    if (rc != DD_OK)
    {
        deleteSurface ();
    }
    else
    {
        if (!mGuestVRAMSurface)
        {
            /* Clear just created surface. */
            memset (this->address (), 0, this->lineSize () * this->height ());
        }
    }
}

void VBoxDDRAWFrameBuffer::deleteSurface ()
{
    if (mSurface)
    {
        mSurface->Release ();
        mSurface = NULL;

        if (!mGuestVRAMSurface)
        {
            RTMemFree (mSurfaceDesc.lpSurface);
        }

        memset (&mSurfaceDesc, '\0', sizeof (mSurfaceDesc));
        mGuestVRAMSurface = FALSE;
    }
}

/* Draw a rectangular area of guest screen DDRAW surface onto the
 * host screen primary surface.
 */
void VBoxDDRAWFrameBuffer::drawRect (ULONG x, ULONG y, ULONG w, ULONG h)
{
    LOGDDRAW(("DDRAW: drawRect: %d,%d, %dx%d\n", x, y, w, h));

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
