/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxDockIconPreview class declaration
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

#ifndef __VBoxDockIconPreview_h__
#define __VBoxDockIconPreview_h__

#include <Carbon/Carbon.h>

class VBoxConsoleWnd;
class VBoxFrameBuffer;

class QPixmap;

class VBoxDockIconPreview
{
public:
    VBoxDockIconPreview (VBoxConsoleWnd *aMainWnd, const QPixmap& aOverlayImage);
    ~VBoxDockIconPreview();

    void updateDockOverlay();
    void updateDockPreview (CGImageRef aVMImage);
    void updateDockPreview (VBoxFrameBuffer *aFrameBuffer);

private:
    inline void initPreviewImages();
    inline void initOverlayData (int aBitmapByteCount);
    inline CGImageRef stateImage() const;
    void drawOverlayIcons (CGContextRef aContext);

    /* Flipping is necessary cause the drawing context in Carbon is flipped by 180 degree */
    inline CGRect flipRect (CGRect aRect) const { aRect.origin.y = mDockIconRect.size.height - aRect.origin.y - aRect.size.height; return aRect; }
    inline CGRect centerRect (CGRect aRect) const { return centerRectTo (aRect, mDockIconRect); }
    inline CGRect centerRectTo (CGRect aRect, const CGRect& aToRect) const
    {
        aRect.origin.x = aToRect.origin.x + (aToRect.size.width  - aRect.size.width)  / 2.0;
        aRect.origin.y = aToRect.origin.y + (aToRect.size.height - aRect.size.height) / 2.0;
        return aRect;
    }

    /* Private member vars */
    VBoxConsoleWnd *mMainWnd;
    const CGRect mDockIconRect;

    CGImageRef mOverlayImage;
    CGImageRef mDockMonitor;
    CGImageRef mDockMonitorGlossy;

    void *mBitmapData;

    CGImageRef mStatePaused;
    CGImageRef mStateSaving;
    CGImageRef mStateRestoring;

    CGRect mUpdateRect;
    CGRect mMonitorRect;
};

#endif /* __VBoxDockIconPreview_h__ */

