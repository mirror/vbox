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

#ifndef ___VBoxDockIconPreview_h___
#define ___VBoxDockIconPreview_h___

#include "VBoxUtils-darwin.h"

RT_C_DECLS_BEGIN
void darwinCreateVBoxDockIconTileView (void);
void darwinDestroyVBoxDockIconTileView (void);

CGContextRef darwinBeginCGContextForApplicationDockTile (void);
void darwinEndCGContextForApplicationDockTile (CGContextRef aContext);

void darwinOverlayApplicationDockTileImage (CGImageRef pImage);
void darwinRestoreApplicationDockTileImage (void);
RT_C_DECLS_END

#ifndef __OBJC__
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
    inline CGRect flipRect (CGRect aRect) const { return ::darwinFlipCGRect (aRect, mDockIconRect); }
    inline CGRect centerRect (CGRect aRect) const { return ::darwinCenterRectTo (aRect, mDockIconRect); }
    inline CGRect centerRectTo (CGRect aRect, const CGRect& aToRect) const { return ::darwinCenterRectTo (aRect, aToRect); }

    void updateDockPreviewImpl (CGContextRef aContext, CGImageRef aVMImage);

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
#endif /* !__OBJC__ */

#endif /* !___VBoxDockIconPreview_h___ */

