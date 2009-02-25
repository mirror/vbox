/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Declarations of utility classes and functions for handling Darwin Carbon
 * specific tasks
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

#include "VBoxUtils-darwin.h"

#include <iprt/assert.h>

#include <Carbon/Carbon.h>

NativeWindowRef darwinToNativeWindowImpl (NativeViewRef aView)
{
    return reinterpret_cast<WindowRef> (::HIViewGetWindow (aView));
}

void darwinSetShowsToolbarButtonImpl (NativeWindowRef aWindow, bool aEnabled)
{
    int err = ::ChangeWindowAttributes (aWindow, aEnabled ? kWindowToolbarButtonAttribute:kWindowNoAttributes,
                                                 aEnabled ? kWindowNoAttributes:kWindowToolbarButtonAttribute);
    AssertCarbonOSStatus (err);
}

void darwinSetMouseCoalescingEnabled (bool aEnabled)
{
    ::SetMouseCoalescingEnabled (aEnabled, NULL);
}





/********************************************************************************
 *
 * Old carbon stuff. Have to converted soon!
 *
 ********************************************************************************/
#include "VBoxConsoleView.h"

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
CGImageRef darwinToCGImageRef (const QImage *aImage)
{
    QImage *imageCopy = new QImage (*aImage);
    /** @todo this code assumes 32-bit image input, the lazy bird convert image to 32-bit method is anything but optimal... */
    if (imageCopy->format() != QImage::Format_ARGB32)
        *imageCopy = imageCopy->convertToFormat (QImage::Format_ARGB32);
    Assert (!imageCopy->isNull());

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGDataProviderRef dp = CGDataProviderCreateWithData (imageCopy, aImage->bits(), aImage->numBytes(), darwinDataProviderReleaseQImage);

    CGBitmapInfo bmpInfo = kCGImageAlphaFirst | kCGBitmapByteOrder32Host;
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
CGImageRef darwinToCGImageRef (const QPixmap *aPixmap)
{
    return aPixmap->toMacCGImageRef();
}

/**
 * Loads an image using Qt and converts it to a CGImage.
 *
 * @returns CGImageRef for the new image. (Remember to release it when finished with it.)
 * @param   aSource     The source name.
 */
CGImageRef darwinToCGImageRef (const char *aSource)
{
    QPixmap qpm (QString(":/") + aSource);
    Assert (!qpm.isNull());
    return ::darwinToCGImageRef (&qpm);
}

void darwinWindowAnimateResize (QWidget *aWidget, const QRect &aTarget)
{
    HIRect r = ::darwinToHIRect (aTarget);
    TransitionWindowWithOptions (::darwinToNativeWindow (aWidget), kWindowSlideTransitionEffect, kWindowResizeTransitionAction, &r, false, NULL);
}

bool darwinIsMenuOpen (void)
{
    MenuTrackingData outData;
    return (GetMenuTrackingData (NULL, &outData) != menuNotFoundErr);
}

/* Currently not used! */
OSStatus darwinRegionHandler (EventHandlerCallRef aInHandlerCallRef, EventRef aInEvent, void *aInUserData)
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
                printf("test1\n");
                GetEventParameter (aInEvent, kEventParamRgnHandle, typeQDRgnHandle, NULL, sizeof (rgn), NULL, &rgn);
                SetEmptyRgn (rgn);
                status = noErr;
            }
            /* if the content of the whole window is queried return a copy of our saved region. */
            else if (code == (kWindowStructureRgn))// || kWindowGlobalPortRgn || kWindowUpdateRgn))
            {
                printf("test2\n");
                GetEventParameter (aInEvent, kEventParamRgnHandle, typeQDRgnHandle, NULL, sizeof (rgn), NULL, &rgn);
                QRegion *pRegion = static_cast <QRegion*> (aInUserData);
                if (!pRegion->isEmpty() && pRegion)
                {
                    //CopyRgn (pRegion->handle(), rgn);
                    status = noErr;
                }
            }
            break;
        }
        case kEventControlDraw:
        {
            printf("test3\n");
            CGContextRef ctx;
            HIRect bounds;

            GetEventParameter (aInEvent, kEventParamCGContextRef, typeCGContextRef, NULL, sizeof (ctx), NULL, &ctx);
            HIViewGetBounds ((HIViewRef)aInUserData, &bounds);

            CGContextClearRect (ctx, bounds);
            status = noErr;
            break;
        }
    }

    return status;
}

OSStatus darwinOverlayWindowHandler (EventHandlerCallRef aInHandlerCallRef, EventRef aInEvent, void *aInUserData)
{
    if (!aInUserData)
        return ::CallNextEventHandler (aInHandlerCallRef, aInEvent);

    UInt32 eventClass = ::GetEventClass (aInEvent);
    UInt32 eventKind = ::GetEventKind (aInEvent);
    /* For debugging events */
    /*
    if (!(eventClass == 'cute'))
        ::darwinDebugPrintEvent ("view: ", aInEvent);
    */
    VBoxConsoleView *view = static_cast<VBoxConsoleView *> (aInUserData);

    if (eventClass == kEventClassVBox)
    {
        if (eventKind == kEventVBoxShowWindow)
        {
//            printf ("ShowWindow requested\n");
            WindowRef w;
            if (GetEventParameter (aInEvent, kEventParamWindowRef, typeWindowRef, NULL, sizeof (w), NULL, &w) != noErr)
                return noErr;
            ShowWindow (w);
            SelectWindow (w);
            return noErr;
        }
        if (eventKind == kEventVBoxMoveWindow)
        {
//            printf ("MoveWindow requested\n");
            WindowPtr w;
            if (GetEventParameter (aInEvent, kEventParamWindowRef, typeWindowRef, NULL, sizeof (w), NULL, &w) != noErr)
                return noErr;
            HIPoint p;
            if (GetEventParameter (aInEvent, kEventParamOrigin, typeHIPoint, NULL, sizeof (p), NULL, &p) != noErr)
                return noErr;
            ChangeWindowGroupAttributes (GetWindowGroup (w), 0, kWindowGroupAttrMoveTogether);
            QPoint p1 = view->mapToGlobal (QPoint (p.x, p.y));
            MoveWindow (w, p1.x(), p1.y(), true);
            ChangeWindowGroupAttributes (GetWindowGroup (w), kWindowGroupAttrMoveTogether, 0);
            return noErr;
        }
        if (eventKind == kEventVBoxResizeWindow)
        {
//            printf ("ResizeWindow requested\n");
            WindowPtr w;
            if (GetEventParameter (aInEvent, kEventParamWindowRef, typeWindowRef, NULL, sizeof (w), NULL, &w) != noErr)
                return noErr;
            HISize s;
            if (GetEventParameter (aInEvent, kEventParamDimensions, typeHISize, NULL, sizeof (s), NULL, &s) != noErr)
                return noErr;
            ChangeWindowGroupAttributes (GetWindowGroup (w), 0, kWindowGroupAttrMoveTogether);
            SizeWindow (w, s.width, s.height, true);
            ChangeWindowGroupAttributes (GetWindowGroup (w), kWindowGroupAttrMoveTogether, 0);
            return noErr;
        }
        if (eventKind == kEventVBoxUpdateDock)
        {
//            printf ("UpdateDock requested\n");
            view->updateDockIcon();
            return noErr;
        }
    }

    return ::CallNextEventHandler (aInHandlerCallRef, aInEvent);
}

