/* $Id$ */
/** @file
 * CarbonDockIconPreview class implementation
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

/* VBox includes */
#include "CarbonDockIconPreview.h"
#include "VBoxUtils-darwin.h"

#include "iprt/assert.h"
#include "iprt/mem.h"

RT_C_DECLS_BEGIN
void darwinCreateVBoxDockIconTileView (void);
void darwinDestroyVBoxDockIconTileView (void);

CGContextRef darwinBeginCGContextForApplicationDockTile (void);
void darwinEndCGContextForApplicationDockTile (CGContextRef aContext);

void darwinOverlayApplicationDockTileImage (CGImageRef pImage);
void darwinRestoreApplicationDockTileImage (void);

void darwinDrawMainWindow (NativeWindowRef pMainWin, CGContextRef aContext);
RT_C_DECLS_END

/* Import private function to capture the window content of any given window. */
CG_EXTERN_C_BEGIN
typedef int CGSWindowID;
typedef void *CGSConnectionID;
CG_EXTERN CGSWindowID GetNativeWindowFromWindowRef(WindowRef ref);
CG_EXTERN CGSConnectionID CGSMainConnectionID(void);
CG_EXTERN void CGContextCopyWindowCaptureContentsToRect(CGContextRef c, CGRect dstRect, CGSConnectionID connection, CGSWindowID window, int zero);
CG_EXTERN_C_END

CarbonDockIconPreview::CarbonDockIconPreview (VBoxConsoleWnd *aMainWnd, const QPixmap& aOverlayImage)
  : AbstractDockIconPreview (aMainWnd, aOverlayImage)
  , AbstractDockIconPreviewHelper (aMainWnd, aOverlayImage)
  , mBitmapData (NULL)
{
}

CarbonDockIconPreview::~CarbonDockIconPreview()
{
    if (mBitmapData)
        RTMemFree (mBitmapData);
}

void CarbonDockIconPreview::updateDockOverlay()
{
    /* Remove all previously set tile images */
    ::RestoreApplicationDockTileImage();

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
    ::OverlayApplicationDockTileImage (overlayImage);

    /* Release the temp image */
    CGImageRelease (overlayImage);
    CGColorSpaceRelease (cs);
}

void CarbonDockIconPreview::updateDockPreview (CGImageRef aVMImage)
{
    Assert (aVMImage);

    /* Create the context to draw on */
    CGContextRef context = BeginCGContextForApplicationDockTile ();
    updateDockPreviewImpl (context, aVMImage);
    /* This flush updates the dock icon */
    CGContextFlush (context);
    EndCGContextForApplicationDockTile (context);
}

void CarbonDockIconPreview::updateDockPreview (VBoxFrameBuffer *aFrameBuffer)
{
    AbstractDockIconPreview::updateDockPreview (aFrameBuffer);
}

void CarbonDockIconPreview::updateDockPreviewImpl (CGContextRef aContext, CGImageRef aVMImage)
{
    Assert (aContext);

    /* Initialize all dependent images in the case it wasn't done already */
    initPreviewImages();

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    Assert (cs);

    /* Clear the background to be transparent */
    CGContextSetBlendMode (aContext, kCGBlendModeNormal);
    CGContextClearRect (aContext, flipRect (mDockIconRect));

    /* Draw the monitor as the background */
    CGContextDrawImage (aContext, flipRect (mMonitorRect), mDockMonitor);

    /* Calculate the size of the dock icon image and fit it into 128x128 */
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

//    darwinDrawMainWindow(darwinToNativeWindow (mMainWnd), aContext);
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

void CarbonDockIconPreview::initOverlayData (int aBitmapByteCount)
{
    if (!mBitmapData)
        mBitmapData = RTMemAlloc (aBitmapByteCount);
}

