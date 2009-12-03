/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Abstract class for the dock icon preview
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

#ifndef ___AbstractVBoxDockIconPreview_h___
#define ___AbstractVBoxDockIconPreview_h___

/* System includes */
#include <ApplicationServices/ApplicationServices.h>

/* VBox includes */
#include "VBoxUtils-darwin.h"

class VBoxConsoleWnd;
class VBoxFrameBuffer;

class QPixmap;

class AbstractDockIconPreview
{
public:
    AbstractDockIconPreview (VBoxConsoleWnd *aMainWnd, const QPixmap& aOverlayImage);
    virtual ~AbstractDockIconPreview() {};

    virtual void updateDockOverlay() = 0;
    virtual void updateDockPreview (CGImageRef aVMImage) = 0;
    virtual void updateDockPreview (VBoxFrameBuffer *aFrameBuffer);

    virtual void setOriginalSize (int /* aWidth */, int /* aHeight */) {}
};

class AbstractDockIconPreviewHelper
{
public:
    AbstractDockIconPreviewHelper (VBoxConsoleWnd *aMainWnd, const QPixmap& aOverlayImage);
    virtual ~AbstractDockIconPreviewHelper();
    void initPreviewImages();
    inline CGImageRef stateImage() const;
    void drawOverlayIcons (CGContextRef aContext);

    /* Flipping is necessary cause the drawing context in Carbon is flipped by 180 degree */
    inline CGRect flipRect (CGRect aRect) const { return ::darwinFlipCGRect (aRect, mDockIconRect); }
    inline CGRect centerRect (CGRect aRect) const { return ::darwinCenterRectTo (aRect, mDockIconRect); }
    inline CGRect centerRectTo (CGRect aRect, const CGRect& aToRect) const { return ::darwinCenterRectTo (aRect, aToRect); }

    /* Private member vars */
    VBoxConsoleWnd *mMainWnd;
    const CGRect mDockIconRect;

    CGImageRef mOverlayImage;
    CGImageRef mDockMonitor;
    CGImageRef mDockMonitorGlossy;

    CGImageRef mStatePaused;
    CGImageRef mStateSaving;
    CGImageRef mStateRestoring;

    CGRect mUpdateRect;
    CGRect mMonitorRect;
};

#endif /* ___AbstractVBoxDockIconPreview_h___ */

