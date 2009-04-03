/* $Id$ */
/** @file
 * Qt GUI - Realtime Dock Icon Preview
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#include "VBoxDockIconPreview.h"
#include "VBoxUtils.h"
#include "VBoxConsoleWnd.h"
#include "VBoxFrameBuffer.h"

#include "iprt/assert.h"

#include <QPixmap>

#ifndef QT_MAC_USE_COCOA
/* Import private function to capture the window content of any given window. */
CG_EXTERN_C_BEGIN
typedef int CGSWindowID;
typedef void *CGSConnectionID;
CG_EXTERN CGSWindowID GetNativeWindowFromWindowRef(WindowRef ref);
CG_EXTERN CGSConnectionID CGSMainConnectionID(void);
CG_EXTERN void CGContextCopyWindowCaptureContentsToRect(CGContextRef c, CGRect dstRect, CGSConnectionID connection, CGSWindowID window, int zero);
CG_EXTERN_C_END
#endif /* !QT_MAC_USE_COCOA */

VBoxDockIconPreview::VBoxDockIconPreview (VBoxConsoleWnd *aMainWnd, const QPixmap& aOverlayImage)
    :  mMainWnd (aMainWnd)
     , mDockIconRect (CGRectMake (0, 0, 128, 128))
     , mDockMonitor (NULL)
     , mDockMonitorGlossy (NULL)
     , mBitmapData (NULL)
     , mUpdateRect (CGRectMake (0, 0, 0, 0))
     , mMonitorRect (CGRectMake (0, 0, 0, 0))
{
    mOverlayImage   = ::darwinToCGImageRef (&aOverlayImage);
    Assert (mOverlayImage);

    mStatePaused    = ::darwinToCGImageRef ("state_paused_16px.png");
    Assert (mStatePaused);
    mStateSaving    = ::darwinToCGImageRef ("state_saving_16px.png");
    Assert (mStateSaving);
    mStateRestoring = ::darwinToCGImageRef ("state_restoring_16px.png");
    Assert (mStateRestoring);

#ifdef QT_MAC_USE_COCOA
    ::darwinCreateVBoxDockIconTileView();
#endif /* QT_MAC_USE_COCOA */
}

VBoxDockIconPreview::~VBoxDockIconPreview()
{
#ifdef QT_MAC_USE_COCOA
    ::darwinDestroyVBoxDockIconTileView();
#endif /* QT_MAC_USE_COCOA */

    CGImageRelease (mOverlayImage);
    if (mDockMonitor)
        CGImageRelease (mDockMonitor);
    if (mDockMonitorGlossy)
        CGImageRelease (mDockMonitorGlossy);

    if (mBitmapData)
        RTMemFree (mBitmapData);

    CGImageRelease (mStatePaused);
    CGImageRelease (mStateSaving);
    CGImageRelease (mStateRestoring);
}

void VBoxDockIconPreview::initPreviewImages()
{
    if (!mDockMonitor)
    {
        mDockMonitor = ::darwinToCGImageRef ("monitor.png");
        Assert (mDockMonitor);
        /* Center it on the dock icon context */
        mMonitorRect = centerRect (CGRectMake (0, 0,
                                               CGImageGetWidth (mDockMonitor),
                                               CGImageGetWidth (mDockMonitor)));
    }

    if (!mDockMonitorGlossy)
    {
        mDockMonitorGlossy = ::darwinToCGImageRef ("monitor_glossy.png");
        Assert (mDockMonitorGlossy);
        /* This depends on the content of monitor.png */
        mUpdateRect = CGRectMake (mMonitorRect.origin.x + 7 + 1,
                                  mMonitorRect.origin.y + 8 + 1,
                                  118 - 7 - 2,
                                  103 - 8 - 2);
    }
}

void VBoxDockIconPreview::initOverlayData (int aBitmapByteCount)
{
    if (!mBitmapData)
        mBitmapData = RTMemAlloc (aBitmapByteCount);
}

CGImageRef VBoxDockIconPreview::stateImage() const
{
    CGImageRef img;
    if (mMainWnd->machineState() == KMachineState_Paused)
        img = mStatePaused;
    else if (mMainWnd->machineState() == KMachineState_Restoring)
        img = mStateRestoring;
    else if (mMainWnd->machineState() == KMachineState_Saving)
        img = mStateSaving;
    else
        img = NULL;
    return img;
}

void VBoxDockIconPreview::drawOverlayIcons (CGContextRef aContext)
{
    CGRect overlayRect = CGRectMake (0, 0, 0, 0);
    /* The overlay image at bottom/right */
    if (mOverlayImage)
    {
        overlayRect = CGRectMake (mDockIconRect.size.width - CGImageGetWidth (mOverlayImage),
                                  mDockIconRect.size.height - CGImageGetHeight (mOverlayImage),
                                  CGImageGetWidth (mOverlayImage),
                                  CGImageGetHeight (mOverlayImage));
        CGContextDrawImage (aContext, flipRect (overlayRect), mOverlayImage);
    }
    CGImageRef sImage = stateImage();
    /* The state image at bottom/right */
    if (sImage)
    {
        CGRect stateRect = CGRectMake (overlayRect.origin.x - CGImageGetWidth (sImage) / 2.0,
                                       overlayRect.origin.y - CGImageGetHeight (sImage) / 2.0,
                                       CGImageGetWidth (sImage),
                                       CGImageGetHeight (sImage));
        CGContextDrawImage (aContext, flipRect (stateRect), sImage);
    }
}

void VBoxDockIconPreview::updateDockOverlay()
{
    /* Remove all previously set tile images */
#ifdef QT_MAC_USE_COCOA
    ::darwinRestoreApplicationDockTileImage();
#else /* QT_MAC_USE_COCOA */
    ::RestoreApplicationDockTileImage();
#endif /* QT_MAC_USE_COCOA */

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    Assert (cs);

    /* Four bytes per color */
    int bitmapBytesPerRow = mDockIconRect.size.width * 4;
    int bitmapByteCount = bitmapBytesPerRow * mDockIconRect.size.height;

    initOverlayData (bitmapByteCount);
    Assert (mBitmapData);

    CGContextRef context = CGBitmapContextCreate (mBitmapData,
                                                  mDockIconRect.size.width,
                                                  mDockIconRect.size.height,
                                                  8,
                                                  bitmapBytesPerRow,
                                                  cs,
                                                  kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
    /* Clear the background to be transparent */
    CGContextSetBlendMode (context, kCGBlendModeNormal);
    CGContextClearRect (context, (mDockIconRect));

    /* Draw the state image & the overlay image */
    drawOverlayIcons (context);

    /* Flush the content */
    CGContextFlush (context);

    /* Create a image out of the bitmap context */
    CGImageRef overlayImage = CGBitmapContextCreateImage (context);
    Assert (overlayImage);

    /* Update the dock overlay icon */
#ifdef QT_MAC_USE_COCOA
    ::darwinOverlayApplicationDockTileImage (overlayImage);
#else /* QT_MAC_USE_COCOA */
    ::OverlayApplicationDockTileImage (overlayImage);
#endif /* QT_MAC_USE_COCOA */

    /* Release the temp image */
    CGImageRelease (overlayImage);
    CGColorSpaceRelease (cs);
}

void VBoxDockIconPreview::updateDockPreview (CGImageRef aVMImage)
{
    Assert (aVMImage);

#ifdef QT_MAC_USE_COCOA
    /* Create the context to draw on */
    CGContextRef context = ::darwinBeginCGContextForApplicationDockTile();
    updateDockPreviewImpl (context, aVMImage);
    /* This flush updates the dock icon */
    CGContextFlush (context);
    ::darwinEndCGContextForApplicationDockTile (context);
#else /* QT_MAC_USE_COCOA */
    /* Create the context to draw on */
    CGContextRef context = BeginCGContextForApplicationDockTile ();
    updateDockPreviewImpl (context, aVMImage);
    /* This flush updates the dock icon */
    CGContextFlush (context);
    EndCGContextForApplicationDockTile (context);
#endif /* QT_MAC_USE_COCOA */
}

void VBoxDockIconPreview::updateDockPreviewImpl (CGContextRef aContext, CGImageRef aVMImage)
{
    Assert (aContext);

    /* Init all dependend images in the case it wasn't done already */
    initPreviewImages();

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    Assert (cs);

    /* Clear the background to be transparent */
    CGContextSetBlendMode (aContext, kCGBlendModeNormal);
    CGContextClearRect (aContext, flipRect (mDockIconRect));

    /* Draw the monitor as the background */
    CGContextDrawImage (aContext, flipRect (mMonitorRect), mDockMonitor);

    /* Calc the size of the dock icon image and fit it into 128x128 */
    int scaledWidth;
    int scaledHeight;
    float aspect = static_cast <float> (CGImageGetWidth (aVMImage)) / CGImageGetHeight (aVMImage);
    if (aspect > 1.0)
    {
        scaledWidth = mUpdateRect.size.width;
        scaledHeight = mUpdateRect.size.height / aspect;
    }
    else
    {
        scaledWidth = mUpdateRect.size.width * aspect;
        scaledHeight = mUpdateRect.size.height;
    }

    CGRect iconRect = centerRectTo (CGRectMake (0, 0,
                                                scaledWidth, scaledHeight),
                                    mUpdateRect);
    /* Draw the VM content */
    CGContextDrawImage (aContext, flipRect (iconRect), aVMImage);

#if 0 // ndef QT_MAC_USE_COCOA
    /* Process the content of any external OpenGL window. */
    WindowRef w = darwinToNativeWindow (mMainWnd);
    WindowGroupRef g = GetWindowGroup (w);
    WindowGroupContentOptions wgco = kWindowGroupContentsReturnWindows | kWindowGroupContentsRecurse | kWindowGroupContentsVisible;
    ItemCount c = CountWindowGroupContents (g, wgco);
    float a1 = iconRect.size.width / static_cast <float> (CGImageGetWidth (aVMImage));
    float a2 = iconRect.size.height / static_cast <float> (CGImageGetHeight (aVMImage));
    Rect tmpR;
    GetWindowBounds (w, kWindowContentRgn, &tmpR);
    HIRect mainRect = CGRectMake (tmpR.left, tmpR.top, tmpR.right-tmpR.left, tmpR.bottom-tmpR.top);
    /* Iterate over every window in the returned window group. */
    for (ItemCount i = 0; i <= c; ++i)
    {
        WindowRef wc;
        OSStatus status = GetIndexedWindow (g, i, wgco, &wc);
        /* Skip the main window */
        if (status == noErr &&
            wc != w)
        {
            WindowClass winClass;
            status = GetWindowClass (wc, &winClass);
            /* Check that the class is of type overlay window */
            if (status == noErr &&
                winClass == kOverlayWindowClass)
            {
                Rect tmpR1;
                GetWindowBounds (wc, kWindowContentRgn, &tmpR1);
                HIRect rect;
                rect.size.width = (tmpR1.right-tmpR1.left) * a1;
                rect.size.height = (tmpR1.bottom-tmpR1.top) * a2;
                rect.origin.x = iconRect.origin.x + (tmpR1.left - mainRect.origin.x) * a1;
                rect.origin.y = iconRect.origin.y + (tmpR1.top  - mainRect.origin.y) * a2;
                /* This is a big, bad hack. The following functions aren't
                 * documented nor official supported by apple. But its the only way
                 * to capture the OpenGL content of a window without fiddling
                 * around with gPixelRead or something like that. */
                CGSWindowID wid = GetNativeWindowFromWindowRef (wc);
                CGContextCopyWindowCaptureContentsToRect(aContext, flipRect (rect), CGSMainConnectionID(), wid, 0);
            }
        }
    }
#endif /* QT_MAC_USE_COCOA */

    /* Draw the glossy overlay */
    CGContextDrawImage (aContext, flipRect (mMonitorRect), mDockMonitorGlossy);

    /* Draw the state image & the overlay image */
    drawOverlayIcons (aContext);

    CGColorSpaceRelease (cs);
}

void VBoxDockIconPreview::updateDockPreview (VBoxFrameBuffer *aFrameBuffer)
{
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    Assert (cs);
    /* Create the image copy of the framebuffer */
    CGDataProviderRef dp = CGDataProviderCreateWithData (aFrameBuffer, aFrameBuffer->address(), aFrameBuffer->bitsPerPixel() / 8 * aFrameBuffer->width() * aFrameBuffer->height(), NULL);
    Assert (dp);
    CGImageRef ir = CGImageCreate (aFrameBuffer->width(), aFrameBuffer->height(), 8, 32, aFrameBuffer->bytesPerLine(), cs,
                                   kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host, dp, 0, false,
                                   kCGRenderingIntentDefault);
    Assert (ir);

    /* Update the dock preview icon */
    updateDockPreview (ir);

    /* Release the temp data and image */
    CGImageRelease (ir);
    CGDataProviderRelease (dp);
    CGColorSpaceRelease (cs);
}

