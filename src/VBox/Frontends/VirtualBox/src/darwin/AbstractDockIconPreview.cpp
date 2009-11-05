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

/* VBox includes */
#include "AbstractDockIconPreview.h"
#include "VBoxConsoleWnd.h"
#include "VBoxFrameBuffer.h"

AbstractDockIconPreview::AbstractDockIconPreview (VBoxConsoleWnd * /* aMainWnd */, const QPixmap& /* aOverlayImage */)
{
}

void AbstractDockIconPreview::updateDockPreview (VBoxFrameBuffer *aFrameBuffer)
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

AbstractDockIconPreviewHelper::AbstractDockIconPreviewHelper (VBoxConsoleWnd *aMainWnd, const QPixmap& aOverlayImage)
    :  mMainWnd (aMainWnd)
     , mDockIconRect (CGRectMake (0, 0, 128, 128))
     , mDockMonitor (NULL)
     , mDockMonitorGlossy (NULL)
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
}

AbstractDockIconPreviewHelper::~AbstractDockIconPreviewHelper()
{
    CGImageRelease (mOverlayImage);
    if (mDockMonitor)
        CGImageRelease (mDockMonitor);
    if (mDockMonitorGlossy)
        CGImageRelease (mDockMonitorGlossy);

    CGImageRelease (mStatePaused);
    CGImageRelease (mStateSaving);
    CGImageRelease (mStateRestoring);
}

void AbstractDockIconPreviewHelper::initPreviewImages()
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

CGImageRef AbstractDockIconPreviewHelper::stateImage() const
{
    CGImageRef img;
    if (   mMainWnd->machineState() == KMachineState_Paused
        || mMainWnd->machineState() == KMachineState_TeleportingPausedVM)
        img = mStatePaused;
    else if (   mMainWnd->machineState() == KMachineState_Restoring
             || mMainWnd->machineState() == KMachineState_TeleportingIn)
        img = mStateRestoring;
    else if (   mMainWnd->machineState() == KMachineState_Saving
             || mMainWnd->machineState() == KMachineState_LiveSnapshotting)
        img = mStateSaving;
    else
        img = NULL;
    return img;
}

void AbstractDockIconPreviewHelper::drawOverlayIcons (CGContextRef aContext)
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

