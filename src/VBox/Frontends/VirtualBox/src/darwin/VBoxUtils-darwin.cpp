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


/**
 * Callback for deleting the QImage object when CGImageCreate is done 
 * with it (which is probably not until the returned CFGImageRef is released).
 * 
 * @param   info        Pointer to the QImage.
 */
static void darwinDataProviderReleaseQImage(void *info, const void *, size_t)
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
CGImageRef DarwinQImageToCGImage(const QImage *aImage)
{
    QImage *imageCopy = new QImage(*aImage);
    /** @todo this code assumes 32-bit image input, the lazy bird convert image to 32-bit method is anything but optimal... */
    if (imageCopy->depth() != 32)
        *imageCopy = imageCopy->convertDepth (32);
    Assert (!imageCopy->isNull());

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGDataProviderRef dp = CGDataProviderCreateWithData (imageCopy, aImage->bits(), aImage->numBytes(), darwinDataProviderReleaseQImage);

    CGBitmapInfo bmpInfo = imageCopy->hasAlphaBuffer() ? kCGImageAlphaFirst : kCGImageAlphaNoneSkipFirst;
    bmpInfo |= kCGBitmapByteOrder32Host;
    CGImageRef ir = CGImageCreate (imageCopy->width(), imageCopy->height(), 8, 32, imageCopy->bytesPerLine(), cs,
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
CGImageRef DarwinQPixmapToCGImage(const QPixmap *aPixmap)
{
    QImage qimg = aPixmap->convertToImage();
    Assert (!qimg.isNull());
    return DarwinQImageToCGImage(&qimg);
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
    Assert (!qpm.isNull());
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
    Assert (!back.isNull());
    Assert (back.width() == 128 && back.height() == 128);

    /* load the badge */
    QPixmap badge = QPixmap::fromMimeSource (aSource);
    Assert (!badge.isNull());

    /* resize it and copy it onto the background. */
    if (badge.width() < 32)
        badge = badge.convertToImage().smoothScale (32, 32);
    copyBlt (&back, back.width() - badge.width(), back.height() - badge.height(),
             &badge, 0, 0,
             badge.width(), badge.height());
    Assert (!back.isNull());
    Assert (back.width() == 128 && back.height() == 128);

    /* Convert it to a CGImage. */
    return ::DarwinQPixmapToCGImage (&back);
}

/**
 * Creates a dock preview image.
 * 
 * Use this method to create a 128x128 preview image of the vm window.
 * 
 * @returns CGImageRef for the new image. (Remember to release it when finished with it.) 
 * @param   aFrameBuffer    The source name.
 */
CGImageRef DarwinCreateDockPreview(VBoxFrameBuffer *aFrameBuffer)
{
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    /* Create the image copy of the framebuffer */
    CGDataProviderRef dp = CGDataProviderCreateWithData(aFrameBuffer, aFrameBuffer->address(), aFrameBuffer->bitsPerPixel() / 8 * aFrameBuffer->width() * aFrameBuffer->height() , NULL);
    CGImageRef ir = CGImageCreate(aFrameBuffer->width(), aFrameBuffer->height(), 8, 32, aFrameBuffer->bytesPerLine(), cs,
                                  kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host, dp, 0 /*decode */, 0 /* shouldInterpolate */, 
                                  kCGRenderingIntentDefault);

    Assert(cs);
    Assert(dp);
    Assert(ir);
    /* Calc the size of the dock icon image and fit it into 128x128 */
    int targetWidth = 128;
    int targetHeight = 128;
    float aspect = static_cast<float>(aFrameBuffer->width()) / aFrameBuffer->height();
    CGRect rect;
    if(aspect > 1.0)
    {
      rect.origin.x = 0;
      rect.origin.y = (targetHeight-targetHeight/aspect)/2;
      rect.size.width = targetWidth;
      rect.size.height = targetHeight/aspect;
    }else
    {
      rect.origin.x = (targetWidth-targetWidth*aspect)/2;
      rect.origin.y = 0;
      rect.size.width = targetWidth*aspect;
      rect.size.height = targetHeight;
    }
    /* Create a bitmap context to draw on */
    int bitmapBytesPerRow = (targetWidth * 4);
    int bitmapByteCount = (bitmapBytesPerRow * targetHeight);
    void *bitmapData = malloc(bitmapByteCount);
    CGImageRef dockImage = NULL;
    if (bitmapData)
    {
      CGContextRef context = CGBitmapContextCreate(bitmapData, targetWidth, targetHeight, 8, bitmapBytesPerRow, cs, kCGImageAlphaPremultipliedLast);
      /* Draw on the bitmap */
      CGContextDrawImage(context, rect, ir);
      /* Create the preview image ref from the bitmap */
      dockImage = CGBitmapContextCreateImage(context);
      CGContextRelease(context);
      free(bitmapData); 
    }
    CGColorSpaceRelease(cs);
    CGDataProviderRelease(dp);
    CGImageRelease(ir);

    Assert (dockImage);
    return dockImage;
}
