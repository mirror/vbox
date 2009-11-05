/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Cocoa helper for the dock icon preview
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
#include "CocoaDockIconPreview.h"
#include "VBoxCocoaHelper.h"

/* System includes */
#import <Cocoa/Cocoa.h>

@interface DockTileMonitor: NSView
{
    CocoaDockIconPreviewPrivate *p;

    NSImageView *mScreenContent;
    NSImageView *mMonitorGlossy;
}
- (id)initWithFrame:(NSRect)frame parent:(CocoaDockIconPreviewPrivate*)parent;
- (NSImageView*)screenContent;
- (void)resize:(NSSize)size;
@end

@interface DockTileOverlay: NSView
{
    CocoaDockIconPreviewPrivate *p;
}
- (id)initWithFrame:(NSRect)frame parent:(CocoaDockIconPreviewPrivate*)parent;
@end

@interface DockTile: NSView
{
    CocoaDockIconPreviewPrivate *p;

    DockTileMonitor *mMonitor;
    NSImageView     *mAppIcon;

    DockTileOverlay *mOverlay;
}
- (id)initWithParent:(CocoaDockIconPreviewPrivate*)parent;
- (NSView*)screenContent;
- (void)cleanup;
- (void)restoreAppIcon;
- (void)updateAppIcon;
- (void)restoreMonitor;
- (void)updateMonitorWithImage:(CGImageRef)image;
- (void)resizeMonitor:(NSSize)size;
@end

/* 
 * Helper class which allow us to access all members/methods of AbstractDockIconPreviewHelper
 * from any Cocoa class.
 */
class CocoaDockIconPreviewPrivate: public AbstractDockIconPreviewHelper
{
public:
    inline CocoaDockIconPreviewPrivate (VBoxConsoleWnd *aMainWnd, const QPixmap& aOverlayImage)
      :AbstractDockIconPreviewHelper (aMainWnd, aOverlayImage) 
    {
        mDockTile = [[DockTile alloc] initWithParent:this];
    }

    inline ~CocoaDockIconPreviewPrivate()
    {
        [mDockTile release];
    }
      
    DockTile *mDockTile;
};

/* 
 * Cocoa wrapper for the abstract dock icon preview class 
 */
CocoaDockIconPreview::CocoaDockIconPreview (VBoxConsoleWnd *aMainWnd, const QPixmap& aOverlayImage)
  : AbstractDockIconPreview (aMainWnd, aOverlayImage)
{
    CocoaAutoreleasePool pool;

    d = new CocoaDockIconPreviewPrivate (aMainWnd, aOverlayImage);
}

CocoaDockIconPreview::~CocoaDockIconPreview()
{
    CocoaAutoreleasePool pool;

    delete d;
}

void CocoaDockIconPreview::updateDockOverlay()
{
    CocoaAutoreleasePool pool;

    [d->mDockTile updateAppIcon];
}

void CocoaDockIconPreview::updateDockPreview (CGImageRef aVMImage)
{
    CocoaAutoreleasePool pool;

    [d->mDockTile updateMonitorWithImage:aVMImage];
}

void CocoaDockIconPreview::updateDockPreview (VBoxFrameBuffer *aFrameBuffer)
{
    CocoaAutoreleasePool pool;

    AbstractDockIconPreview::updateDockPreview (aFrameBuffer);
}


void CocoaDockIconPreview::setOriginalSize (int aWidth, int aHeight)
{
    CocoaAutoreleasePool pool;

    [d->mDockTile resizeMonitor:NSMakeSize (aWidth, aHeight)];
}

/* 
 * Class for arranging/updating the layers for the glossy monitor preview.
 */
@implementation DockTileMonitor;
- (id)initWithFrame:(NSRect)frame parent:(CocoaDockIconPreviewPrivate*)parent
{
    self = [super initWithFrame:frame];

    if (self != nil)
    {
        p = parent;
        /* The screen content view */
        mScreenContent = [[NSImageView alloc] initWithFrame:NSRectFromCGRect (p->flipRect (p->mUpdateRect))];
//        [mScreenContent setImageAlignment: NSImageAlignCenter];
        [mScreenContent setImageAlignment: NSImageAlignTop| NSImageAlignLeft];
        [mScreenContent setImageScaling: NSScaleToFit];
        [self addSubview: mScreenContent];
        /* The state view */
        mMonitorGlossy = [[NSImageView alloc] initWithFrame:NSRectFromCGRect (p->flipRect (p->mMonitorRect))];
        [mMonitorGlossy setImage: darwinCGImageToNSImage (p->mDockMonitorGlossy)];
        [self addSubview: mMonitorGlossy];
    }

    return self;
}

- (void)drawRect:(NSRect)aRect;
{
    NSImage *dockMonitor = darwinCGImageToNSImage (p->mDockMonitor);
    [dockMonitor drawInRect:NSRectFromCGRect (p->flipRect (p->mMonitorRect)) fromRect:aRect operation:NSCompositeSourceOver fraction:1.0];
    [dockMonitor release];
}

- (NSImageView*)screenContent
{
    return mScreenContent;
}

- (void)resize:(NSSize)size;
{
    /* Calculate the new size based on the aspect ratio of the original screen
       size */
    float w, h;
    if (size.width > size.height)
    {
        w = p->mUpdateRect.size.width;
        h = ((float)size.height / size.width * p->mUpdateRect.size.height);
    }
    else
    {
        w = ((float)size.width / size.height * p->mUpdateRect.size.width);
        h = p->mUpdateRect.size.height;
    }
    CGRect r = (p->flipRect (p->centerRectTo (CGRectMake (0, 0, (int)w, (int)h), p->mUpdateRect)));
    r.origin.x = (int)r.origin.x;
    r.origin.y = (int)r.origin.y;
    r.size.width = (int)r.size.width;
    r.size.height = (int)r.size.height;
//    printf("gui %f %f %f %f\n", r.origin.x, r.origin.y, r.size.width, r.size.height);
    /* Center within the update rect */
    [mScreenContent setFrame:NSRectFromCGRect (r)];
}
@end

/* 
 * Simple implementation for the overlay of the OS & the state icon. Is used both
 * in the application icon & preview mode.
 */
@implementation DockTileOverlay
- (id)initWithFrame:(NSRect)frame parent:(CocoaDockIconPreviewPrivate*)parent
{
    self = [super initWithFrame:frame];

    if (self != nil)
        p = parent;

    return self;
}

- (void)drawRect:(NSRect)aRect;
{
    NSGraphicsContext *nsContext = [NSGraphicsContext currentContext];
    CGContextRef pCGContext = (CGContextRef)[nsContext graphicsPort];
    p->drawOverlayIcons (pCGContext);
}
@end

/* 
 * VirtualBox Dock Tile implementation. Manage the switching between the icon
 * and preview mode & forwards all update request to the appropriate methods.
 */
@implementation DockTile
- (id)initWithParent:(CocoaDockIconPreviewPrivate*)parent
{
    self = [super init];

    if (self != nil)
    {
        p = parent;
        /* Add self as the content view of the dock tile */
        NSDockTile *dock = [[NSApplication sharedApplication] dockTile];
        [dock setContentView: self];
        /* App icon is default */
        [self restoreAppIcon];
        /* The overlay */
        mOverlay = [[DockTileOverlay alloc] initWithFrame:NSRectFromCGRect(p->flipRect (p->mDockIconRect)) parent:p];
        [self addSubview: mOverlay];
    }

    return self;
}

- (NSView*)screenContent
{
    return [mMonitor screenContent];
}

- (void)cleanup
{
    if (mAppIcon != nil)
    {
        [mAppIcon removeFromSuperview];
        [mAppIcon release];
        mAppIcon = nil;
    }
    if (mMonitor != nil)
    {
        [mMonitor removeFromSuperview];
        [mMonitor release];
        mMonitor = nil;
    }
}

- (void)restoreAppIcon
{
    if (mAppIcon == nil)
    {
        [self cleanup];
        mAppIcon = [[NSImageView alloc] initWithFrame:NSRectFromCGRect (p->flipRect (p->mDockIconRect))];
        [mAppIcon setImage: [NSImage imageNamed:@"NSApplicationIcon"]];
        [self addSubview: mAppIcon positioned:NSWindowBelow relativeTo:mOverlay];
    }
}

- (void)updateAppIcon
{
    [self restoreAppIcon];
    [[[NSApplication sharedApplication] dockTile] display];
}

- (void)restoreMonitor
{
    if (mMonitor == nil)
    {
        p->initPreviewImages();
        [self cleanup];
        mMonitor = [[DockTileMonitor alloc] initWithFrame:NSRectFromCGRect (p->flipRect (p->mDockIconRect)) parent:p];
        [self addSubview: mMonitor positioned:NSWindowBelow relativeTo:mOverlay];
    }
}

- (void)updateMonitorWithImage:(CGImageRef)image
{
    [self restoreMonitor];
    NSImage *nsimage = darwinCGImageToNSImage (image);
    [[mMonitor screenContent] setImage: nsimage];
    [nsimage release];
    [[[NSApplication sharedApplication] dockTile] display];
}

- (void)resizeMonitor:(NSSize)size;
{
    [mMonitor resize:size];
}
@end

