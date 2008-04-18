/* $Id$ */
/** @file
 * Qt GUI (aka VirtualBox) - Quartz2D framebuffer implementation.
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

#if defined (VBOX_GUI_USE_QUARTZ2D)

/* VBox includes */
#include "VBoxFrameBuffer.h"
#include "VBoxConsoleView.h"
#include "VBoxProblemReporter.h"
#include "VBoxGlobal.h"
/* Needed for checking against seamless mode */
#include "VBoxConsoleWnd.h"
#include "VBoxIChatTheaterWrapper.h"

/* Qt includes */
#include <qapplication.h>
#include <qmainwindow.h>
#include <qstatusbar.h>

/** @class VBoxQuartz2DFrameBuffer
 *
 *  The VBoxQuartz2dFrameBuffer class is a class that implements the IFrameBuffer
 *  interface and uses Apples Quartz2D to store and render VM display data.
 */

VBoxQuartz2DFrameBuffer::VBoxQuartz2DFrameBuffer (VBoxConsoleView *aView) :
    VBoxFrameBuffer(aView),
    mDataAddress(NULL),
    mBitmapData(NULL),
    mPixelFormat(FramebufferPixelFormat_FOURCC_RGB),
    mImage(NULL),
    mRegion (NULL),
    mRegionUnused (NULL)
{
    Log (("Quartz2D: Creating\n"));
    resizeEvent (new VBoxResizeEvent (FramebufferPixelFormat_Opaque,
                                      NULL, 0, 0, 640, 480));
}

VBoxQuartz2DFrameBuffer::~VBoxQuartz2DFrameBuffer()
{
    Log (("Quartz2D: Deleting\n"));
    clean();
}

/** @note This method is called on EMT from under this object's lock */
STDMETHODIMP VBoxQuartz2DFrameBuffer::NotifyUpdate (ULONG aX, ULONG aY,
                                                 ULONG aW, ULONG aH,
                                                 BOOL *aFinished)
{
/*    Log (("Quartz2D: NotifyUpdate %d,%d %dx%d\n", aX, aY, aW, aH));*/

    QApplication::postEvent (mView,
                             new VBoxRepaintEvent (aX, aY, aW, aH));
    /* the update has been finished, return TRUE */
    *aFinished = TRUE;

    return S_OK;
}

STDMETHODIMP VBoxQuartz2DFrameBuffer::SetVisibleRegion (BYTE *aRectangles, ULONG aCount)
{
    PRTRECT rects = (PRTRECT)aRectangles;

    if (!rects)
        return E_POINTER;

    /** @todo r=bird: Is this thread safe? If I remember the code flow correctly, the
     * GUI thread could be happily jogging along paintEvent now on another cpu core.
     * This function is called on the EMT (emulation thread). Which means, blocking
     * execution waiting for a lock is out of the question. A quick solution using
     * ASMAtomic(Cmp)XchgPtr and a struct { cAllocated; cRects; aRects[1]; }
     * *mRegion, *mUnusedRegion; should suffice (and permit you to reuse allocations). */
    RegionRects *rgnRcts = (RegionRects *) ASMAtomicXchgPtr ((void * volatile *) &mRegionUnused, NULL);
    if (rgnRcts && rgnRcts->allocated < aCount)
    {
        RTMemFree (rgnRcts);
        rgnRcts = NULL;
    }
    if (!rgnRcts)
    {
        ULONG allocated = RT_ALIGN_32 (aCount + 1, 32);
        allocated = RT_MAX (128, allocated);
        rgnRcts = (RegionRects *) RTMemAlloc (RT_OFFSETOF (RegionRects, rcts[allocated]));
        if (!rgnRcts)
            return E_OUTOFMEMORY;
        rgnRcts->allocated = allocated;
    }
    rgnRcts->used = 0;

    QRegion reg;
//    printf ("Region rects follow...\n");
    QRect vmScreenRect (0, 0, width(), height());
    for (ULONG ind = 0; ind < aCount; ++ ind)
    {
        QRect rect;
        rect.setLeft (rects->xLeft);
        rect.setTop (rects->yTop);
        /* QRect are inclusive */
        rect.setRight (rects->xRight - 1);
        rect.setBottom (rects->yBottom - 1);

        /* The rect should intersect with the vm screen. */
        rect = vmScreenRect.intersect (rect);
        ++ rects;
        /* Make sure only valid rects are distributed */
        /* todo: Test if the other framebuffer implementation have the same
         * problem with invalid rects (In Linux/Windows) */
        if (rect.isValid() &&
           rect.width() > 0 && rect.height() > 0)
            reg += rect;
        else
            continue;

        CGRect *cgRct = &rgnRcts->rcts[rgnRcts->used];
        cgRct->origin.x = rect.x();
        cgRct->origin.y = rect.y();
        cgRct->size.width = rect.width();
        cgRct->size.height = rect.height();
//        printf ("Region rect[%d - %d]: %d %d %d %d\n", rgnRcts->used, aCount, rect.x(), rect.y(), rect.height(), rect.width());
        rgnRcts->used++;
    }
//    printf ("..................................\n");

    void *pvOld = ASMAtomicXchgPtr ((void * volatile *) &mRegion, rgnRcts);
    if (    pvOld
        &&  !ASMAtomicCmpXchgPtr ((void * volatile *) &mRegionUnused, pvOld, NULL))
        RTMemFree (pvOld);

    QApplication::postEvent (mView, new VBoxSetRegionEvent (reg));

    return S_OK;
}

void VBoxQuartz2DFrameBuffer::paintEvent (QPaintEvent *pe)
{
    /* Some general hints at the beginning:
     * The console is not a real sub window of the main window. There is
     * one real mac window only. This means all the drawing on the context has
     * to pay attention to the statusbar, the scrollbars, the shifting spacers
     * and any frame borders.
     * Secondly the origin of the coordinate system is differently defined in
     * Qt and Quartz2D. In Qt the point (0, 0) means top/left where this is
     * bottom/left in Quartz2D. Use mapYOrigin to map from Qt to Quartz2D.
     *
     * For debugging /Developer/Applications/Performance Tools/Quartz Debug.app
     * is a nice tool to see which parts of the screen are updated. */

    Assert (mImage);

    QWidget *pMain = qApp->mainWidget();
    Assert (pMain);
    /* Calculate the view rect to draw in */
    QPoint p = mView->viewport()->mapTo (pMain, QPoint (0, 0));
    QRect Q2DViewRect = mapYOrigin (QRect (p.x(), p.y(), mView->width(), mView->height()), pMain->height());
    /* We have to pay special attention to the scrollbars */
    if (mView->horizontalScrollBar()->isVisible())
        Q2DViewRect.setY (Q2DViewRect.y() + (mView->horizontalScrollBar()->frameSize().height() + 2));
    if (mView->verticalScrollBar()->isVisible())
        Q2DViewRect.setWidth (Q2DViewRect.width() - (mView->verticalScrollBar()->frameSize().width() + 2));

    /* Create the context to draw on */
    WindowPtr window = static_cast <WindowPtr> (mView->viewport()->handle());
    SetPortWindowPort (window);
    CGContextRef ctx;
    QDBeginCGContext (GetWindowPort (window), &ctx);
    /* We handle the seamless mode as a special case. */
    if (static_cast <VBoxConsoleWnd*> (pMain)->isTrueSeamless())
    {
        /* Here we paint the windows without any wallpaper.
         * So the background would be set transparently. */

        /* Create a subimage of the current view.
         * Currently this subimage is the whole screen. */
        CGImageRef subImage = CGImageCreateWithImageInRect (mImage, CGRectMake (mView->contentsX(), mView->contentsY(), mView->visibleWidth(), mView->visibleHeight()));
        Assert (subImage);
        /* Clear the background (Make the rect fully transparent) */
        Rect winRect;
        GetPortBounds (GetWindowPort (window), &winRect);
        CGContextClearRect (ctx, CGRectMake (winRect.left, winRect.top, winRect.right - winRect.left, winRect.bottom - winRect.top));
        /* Grab the current visible region. */
        RegionRects *rgnRcts = (RegionRects *) ASMAtomicXchgPtr ((void * volatile *) &mRegion, NULL);
        if (rgnRcts)
        {
            if (rgnRcts->used > 0)
            {
                /* Save state for display fliping */
                CGContextSaveGState (ctx);
                /* Flip the y-coord */
                CGContextScaleCTM (ctx, 1.0, -1.0);
                CGContextTranslateCTM (ctx, Q2DViewRect.x(), -Q2DViewRect.height() - Q2DViewRect.y());
                /* Add the clipping rects all at once. They are defined in
                 * SetVisibleRegion. */
                CGContextBeginPath (ctx);
                CGContextAddRects (ctx, rgnRcts->rcts, rgnRcts->used);
                /* Restore the context state. Note that the
                 * current path isn't destroyed. */
                CGContextRestoreGState (ctx);
                /* Now convert the path to a clipping path. */
                CGContextClip (ctx);
            }
            /* Put back the visible region, free if we cannot (2+ SetVisibleRegion calls). */
            if (    !ASMAtomicCmpXchgPtr ((void * volatile *) &mRegion, rgnRcts, NULL)
                &&  !ASMAtomicCmpXchgPtr ((void * volatile *) &mRegionUnused, rgnRcts, NULL))
                RTMemFree (rgnRcts);
        }
        /* In any case clip the drawing to the view window */
        CGContextClipToRect (ctx, QRectToCGRect (Q2DViewRect));
        /* At this point draw the real vm image */
        CGContextDrawImage (ctx, QRectToCGRect (Q2DViewRect), subImage);
    }
    else
    {
        /* Here we paint if we didn't care about any masks */

        /* Create a subimage of the current view in the size
         * of the bounding box of the current paint event */
        QRect ir = pe->rect();
        QRect is = QRect (ir.x() + mView->contentsX(), ir.y() + mView->contentsY(), ir.width(), ir.height());
        CGImageRef subImage = CGImageCreateWithImageInRect (mImage, QRectToCGRect (is));
        Assert (subImage);

        /* Ok, for more performance we set a clipping path of the
         * regions given by this paint event. */
        QMemArray <QRect> a = pe->region().rects();
        if (a.size() > 0)
        {
            /* Save state for display fliping */
            CGContextSaveGState (ctx);
            /* Flip the y-coord */
            CGContextScaleCTM (ctx, 1.0, -1.0);
            CGContextTranslateCTM (ctx, Q2DViewRect.x(), -Q2DViewRect.height() - Q2DViewRect.y());
            CGContextBeginPath (ctx);
            /* Add all region rects to the current context as path components */
            for (unsigned int i = 0; i < a.size(); ++i)
                CGContextAddRect (ctx, QRectToCGRect (a[i]));
            CGContextRestoreGState (ctx);
            /* Now convert the path to a clipping path. */
            CGContextClip (ctx);
        }

        /* In any case clip the drawing to the view window */
        CGContextClipToRect (ctx, QRectToCGRect (Q2DViewRect));
        /* Draw the sub image to the right position */
        QPoint p = mView->viewport()->mapTo (pMain, QPoint (ir.x(), ir.y()));
        QRect cr = mapYOrigin (QRect (p.x(), p.y(), ir.width(), ir.height()), pMain->height());
        CGContextDrawImage (ctx, QRectToCGRect (cr), subImage);
    }
    QDEndCGContext (GetWindowPort (window), &ctx);
}
/* Save for later shadow stuff ... */
//        CGContextSetShadow (myContext, myShadowOffset, 10);
//        CGContextBeginTransparencyLayer (myContext, NULL);
//        CGContextSetShadow (myContext, CGSizeMake (10, 5), 1);
//        CGContextClipToRect (myContext, rect);
//        QRect ir = pe->rect();
//        CGContextClipToRect (myContext, CGRectMake (ir.y(), ir.x(), ir.width(), ir.height()));
//        CGContextEndTransparencyLayer (myContext);

void VBoxQuartz2DFrameBuffer::resizeEvent (VBoxResizeEvent *re)
{
#if 0
    printf ("fmt=%lu, vram=%X, bpp=%lu, bpl=%lu, width=%lu, height=%lu\n",
           re->pixelFormat(), (unsigned int)re->VRAM(),
           re->bitsPerPixel(), re->bytesPerLine(),
           re->width(), re->height());
#endif

    /* Clean out old stuff */
    clean();

    mWdt = re->width();
    mHgt = re->height();

    bool remind = false;

    /* We need a color space in any case */
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    /* Check if we support the pixel format/colordepth and can use the guest VRAM directly.
     * Mac OS X supports 16 bit also but not in the 565 mode. So we could use
     * 32 bit only. */
    if (   re->pixelFormat() == FramebufferPixelFormat_FOURCC_RGB
        && re->bitsPerPixel() == 32)
    {
//        printf ("VRAM\n");
        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
        /* Create the image copy of the framebuffer */
        CGDataProviderRef dp = CGDataProviderCreateWithData (NULL, re->VRAM(), re->bitsPerPixel() / 8 * mWdt * mHgt, NULL);
        mImage = CGImageCreate (mWdt, mHgt, 8, re->bitsPerPixel(), re->bytesPerLine(), cs,
                                kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little, dp, 0, false,
                                kCGRenderingIntentDefault);
        mDataAddress = re->VRAM();
        CGDataProviderRelease (dp);
    }
    else
    {
        remind = true;
//        printf ("No VRAM\n");
        /* Create the memory we need for our image copy
         * Read somewhere that an alignment of 16 is
         * best for optimal performance. So why not. */
//        int bitmapBytesPerRow = RT_ALIGN (mWdt * 4, 16);
        int bitmapBytesPerRow = mWdt * 4;
        int bitmapByteCount = (bitmapBytesPerRow * mHgt);
        mBitmapData = RTMemAlloc (bitmapByteCount);
        CGDataProviderRef dp = CGDataProviderCreateWithData (NULL, mBitmapData, bitmapByteCount, NULL);
        mImage = CGImageCreate (mWdt, mHgt, 8, 32, bitmapBytesPerRow, cs,
                               kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little, dp, 0, false,
                               kCGRenderingIntentDefault);
        mDataAddress = static_cast <uchar*> (mBitmapData);
        CGDataProviderRelease (dp);
    }
    CGColorSpaceRelease (cs);
#ifdef VBOX_WITH_ICHAT_THEATER
    setImageRef (mImage);
#endif

//    if (remind)
//    {
//        class RemindEvent : public VBoxAsyncEvent
//        {
//            ulong mRealBPP;
//        public:
//            RemindEvent (ulong aRealBPP)
//                : mRealBPP (aRealBPP) {}
//            void handle()
//            {
//                vboxProblem().remindAboutWrongColorDepth (mRealBPP, 32);
//            }
//        };
//        (new RemindEvent (re->bitsPerPixel()))->post();
//    }
}

void VBoxQuartz2DFrameBuffer::clean()
{
    if (mImage)
    {
        CGImageRelease (mImage);
        mImage = NULL;
    }
    if (mBitmapData)
    {
        RTMemFree (mBitmapData);
        mBitmapData = NULL;
    }
    if (mRegion)
    {
        RTMemFree ((void *) mRegion);
        mRegion = NULL;
    }
    if (mRegionUnused)
    {
        RTMemFree ((void *) mRegionUnused);
        mRegionUnused = NULL;
    }
}

#endif /* defined (VBOX_GUI_USE_QUARTZ2D) */
