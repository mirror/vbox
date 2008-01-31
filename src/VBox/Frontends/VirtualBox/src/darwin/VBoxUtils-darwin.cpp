/* $Id: $ */
/** @file
 * Qt GUI - Utility Classes and Functions specific to Darwin.
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



#include "VBoxUtils.h"
#include "VBoxFrameBuffer.h"
#include <qimage.h>
#include <qpixmap.h>

#include <iprt/assert.h>
#include <iprt/mem.h>


/**
 * Callback for deleting the QImage object when CGImageCreate is done
 * with it (which is probably not until the returned CFGImageRef is released).
 *
 * @param   info        Pointer to the QImage.
 */
static void darwinDataProviderReleaseQImage (void *info, const void *, size_t)
{
    QImage *qimg = (QImage *)info;
    delete qimg;
}

/**
 * Converts a QPixmap to a CGImage.
 *
 * @returns CGImageRef for the new image. (Remember to release it when finished with it.)
 * @param   aPixmap     Pointer to the QPixmap instance to convert.
 */
CGImageRef DarwinQImageToCGImage (const QImage *aImage)
{
    QImage *imageCopy = new QImage (*aImage);
    /** @todo this code assumes 32-bit image input, the lazy bird convert image to 32-bit method is anything but optimal... */
    if (imageCopy->depth () != 32)
        *imageCopy = imageCopy->convertDepth (32);
    Assert (!imageCopy->isNull ());

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB ();
    CGDataProviderRef dp = CGDataProviderCreateWithData (imageCopy, aImage->bits (), aImage->numBytes (), darwinDataProviderReleaseQImage);

    CGBitmapInfo bmpInfo = imageCopy->hasAlphaBuffer () ? kCGImageAlphaFirst : kCGImageAlphaNoneSkipFirst;
    bmpInfo |= kCGBitmapByteOrder32Host;
    CGImageRef ir = CGImageCreate (imageCopy->width (), imageCopy->height (), 8, 32, imageCopy->bytesPerLine (), cs,
                                   bmpInfo, dp, 0 /*decode */, 0 /* shouldInterpolate */,
                                   kCGRenderingIntentDefault);
    CGColorSpaceRelease (cs);
    CGDataProviderRelease (dp);

    Assert (ir);
    return ir;
}

/**
 * Converts a QPixmap to a CGImage.
 *
 * @returns CGImageRef for the new image. (Remember to release it when finished with it.)
 * @param   aPixmap     Pointer to the QPixmap instance to convert.
 */
CGImageRef DarwinQPixmapToCGImage (const QPixmap *aPixmap)
{
    QImage qimg = aPixmap->convertToImage ();
    Assert (!qimg.isNull ());
    return DarwinQImageToCGImage (&qimg);
}

/**
 * Loads an image using Qt and converts it to a CGImage.
 *
 * @returns CGImageRef for the new image. (Remember to release it when finished with it.)
 * @param   aSource     The source name.
 */
CGImageRef DarwinQPixmapFromMimeSourceToCGImage (const char *aSource)
{
    QPixmap qpm = QPixmap::fromMimeSource (aSource);
    Assert (!qpm.isNull ());
    return DarwinQPixmapToCGImage (&qpm);
}

/**
 * Creates a dock badge image.
 *
 * The badge will be placed on the right hand size and vertically centered
 * after having been scaled up to 32x32.
 *
 * @returns CGImageRef for the new image. (Remember to release it when finished with it.)
 * @param   aSource     The source name.
 */
CGImageRef DarwinCreateDockBadge (const char *aSource)
{
    /* instead of figuring out how to create a transparent 128x128 pixmap I've
       just created one that I can load. The Qt gurus can fix this if they like :-) */
    QPixmap back (QPixmap::fromMimeSource ("dock_128x128_transparent.png"));
    Assert (!back.isNull ());
    Assert (back.width () == 128 && back.height () == 128);

    /* load the badge */
    QPixmap badge = QPixmap::fromMimeSource (aSource);
    Assert (!badge.isNull ());

    /* resize it and copy it onto the background. */
    if (badge.width () < 32)
        badge = badge.convertToImage ().smoothScale (32, 32);
    copyBlt (&back, back.width () - badge.width (), back.height () - badge.height (),
             &badge, 0, 0,
             badge.width (), badge.height ());
    Assert (!back.isNull ());
    Assert (back.width () == 128 && back.height () == 128);

    /* Convert it to a CGImage. */
    return ::DarwinQPixmapToCGImage (&back);
}


/**
 * Creates a dock preview image.
 *
 * This method is a callback that creates a 128x128 preview image of the VM window.
 *
 * @returns CGImageRef for the new image. (Remember to release it when finished with it.)
 * @param   aVMImage   the vm screen as a CGImageRef
 * @param   aOverlayImage   an optional overlay image to add at the bottom right of the icon
 */
CGImageRef DarwinCreateDockPreview (CGImageRef aVMImage, CGImageRef aOverlayImage)
{
    Assert (aVMImage);

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB ();
    Assert (cs);

    /* Calc the size of the dock icon image and fit it into 128x128 */
    int targetWidth = 128;
    int targetHeight = 128;
    int scaledWidth;
    int scaledHeight;
    float aspect = static_cast<float>(CGImageGetWidth (aVMImage)) / CGImageGetHeight (aVMImage);
    if (aspect > 1.0)
    {
        scaledWidth = targetWidth;
        scaledHeight = targetHeight / aspect;
    }
    else
    {
        scaledWidth = targetWidth * aspect;
        scaledHeight = targetHeight;
    }
    CGRect iconRect = CGRectMake ((targetWidth - scaledWidth) / 2.0, 
                                  (targetHeight - scaledHeight) / 2.0, 
                                  scaledWidth, scaledHeight);
    /* Create a bitmap context to draw on */
    CGImageRef dockImage = NULL;
    int bitmapBytesPerRow = targetWidth * 4;
    int bitmapByteCount = bitmapBytesPerRow * targetHeight;
    void *bitmapData = RTMemAlloc (bitmapByteCount);
    if (bitmapData)
    {
        CGContextRef context = CGBitmapContextCreate (bitmapData, targetWidth, targetHeight, 8, bitmapBytesPerRow, cs, kCGImageAlphaPremultipliedLast);
        /* rounded corners */
//        CGContextSetLineJoin (context, kCGLineJoinRound);
//        CGContextSetShadow (context, CGSizeMake (10, 5), 1);
//        CGContextSetAllowsAntialiasing (context, true);
        /* some little boarder */
        iconRect = CGRectInset (iconRect, 1, 1);
        /* gray stroke */
        CGContextSetRGBStrokeColor (context, 225.0/255.0, 218.0/255.0, 211.0/255.0, 1);
        iconRect = CGRectInset (iconRect, 6, 6);
        CGContextStrokeRectWithWidth (context, iconRect, 12);
        iconRect = CGRectInset (iconRect, 5, 5);
        /* black stroke */
        CGContextSetRGBStrokeColor (context, 0.0, 0.0, 0.0, 1.0);
        CGContextStrokeRectWithWidth (context, iconRect, 2);
        /* vm content */
        iconRect = CGRectInset (iconRect, 1, 1);
        CGContextDrawImage (context, iconRect, aVMImage);
        /* the overlay image */
        if (aOverlayImage)
        {
            CGRect overlayRect = CGRectMake (targetWidth - CGImageGetWidth (aOverlayImage), 0, CGImageGetWidth (aOverlayImage), CGImageGetHeight (aOverlayImage));
            CGContextDrawImage (context, overlayRect, aOverlayImage);
        }
        /* Create the preview image ref from the bitmap */
        dockImage = CGBitmapContextCreateImage (context);
        CGContextRelease (context);
        RTMemFree (bitmapData);
    }

    CGColorSpaceRelease (cs);

    Assert (dockImage);
    return dockImage;
}

/**
 * Creates a dock preview image.
 *
 * This method is a callback that creates a 128x128 preview image of the VM window.
 *
 * @returns CGImageRef for the new image. (Remember to release it when finished with it.)
 * @param   aFrameBuffer    The guest frame buffer.
 * @param   aOverlayImage   an optional overlay image to add at the bottom right of the icon
 */
CGImageRef DarwinCreateDockPreview (VBoxFrameBuffer *aFrameBuffer, CGImageRef aOverlayImage)
{
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB ();
    Assert (cs);
    /* Create the image copy of the framebuffer */
    CGDataProviderRef dp = CGDataProviderCreateWithData (aFrameBuffer, aFrameBuffer->address (), aFrameBuffer->bitsPerPixel () / 8 * aFrameBuffer->width () * aFrameBuffer->height (), NULL);
    Assert (dp);
    CGImageRef ir = CGImageCreate (aFrameBuffer->width (), aFrameBuffer->height (), 8, 32, aFrameBuffer->bytesPerLine (), cs,
                                   kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host, dp, 0, false,
                                   kCGRenderingIntentDefault);
    /* Create the preview icon */
    CGImageRef dockImage = DarwinCreateDockPreview (ir, aOverlayImage);
    /* Release the temp data and image */
    CGDataProviderRelease (dp);
    CGImageRelease (ir);
    CGColorSpaceRelease (cs);

    return dockImage;
}

OSStatus DarwinRegionHandler (EventHandlerCallRef aInHandlerCallRef, EventRef aInEvent, void *aInUserData)
{
    NOREF (aInHandlerCallRef);

    OSStatus status = eventNotHandledErr;
    
    switch (GetEventKind (aInEvent))
    {
        case kEventWindowGetRegion:
        {
            WindowRegionCode code;
            RgnHandle rgn;
            
            /* which region code is being queried? */
            GetEventParameter (aInEvent, kEventParamWindowRegionCode, typeWindowRegionCode, NULL, sizeof (code), NULL, &code);

            /* if it is the opaque region code then set the region to Empty and return noErr to stop the propagation */
            if (code == kWindowOpaqueRgn)
            {
                GetEventParameter (aInEvent, kEventParamRgnHandle, typeQDRgnHandle, NULL, sizeof (rgn), NULL, &rgn);
                SetEmptyRgn (rgn);
                status = noErr;
            }
            /* if the content of the whole window is queried return a copy of our saved region. */
            else if (code == (kWindowStructureRgn))// || kWindowGlobalPortRgn || kWindowUpdateRgn))
            {
                GetEventParameter (aInEvent, kEventParamRgnHandle, typeQDRgnHandle, NULL, sizeof (rgn), NULL, &rgn);
                QRegion *pRegion = static_cast <QRegion*> (aInUserData);
                if (!pRegion->isNull () && pRegion)
                {
                    CopyRgn (pRegion->handle (), rgn);
                    status = noErr;
                }
            }
            break;
        }
    }
    
    return status;
}

