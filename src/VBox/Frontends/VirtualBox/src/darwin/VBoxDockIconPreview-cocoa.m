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

#import "VBoxDockIconPreview.h"

#import <AppKit/NSView.h>
#import <AppKit/NSDockTile.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSGraphicsContext.h>
#import <AppKit/NSImage.h>

static NSImage *gDockIconImage = NULL;

/********************************************************************************
 *
 * C-Helper: This is the external interface to the Cocoa dock tile handling.
 *
 ********************************************************************************/

void darwinCreateVBoxDockIconTileView (void)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    if (gDockIconImage == NULL)
        gDockIconImage = [[NSImage imageNamed:@"NSApplicationIcon"] copy];

    [pool release];
}

void darwinDestroyVBoxDockIconTileView (void)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    if (gDockIconImage != NULL)
    {
        [gDockIconImage release];
        gDockIconImage = NULL;
    }

    [pool release];
}

CGContextRef darwinBeginCGContextForApplicationDockTile (void)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    [gDockIconImage lockFocus];

    NSGraphicsContext *nsContext = [NSGraphicsContext currentContext];
    CGContextRef pCGContext = [nsContext graphicsPort];

    [pool release];
    return pCGContext;
}

void darwinEndCGContextForApplicationDockTile (CGContextRef aContext)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    [gDockIconImage unlockFocus];

    [NSApp setApplicationIconImage:gDockIconImage];

    [pool release];
}

void darwinOverlayApplicationDockTileImage (CGImageRef pImage)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    /* Convert the CGImage to an NSImage */
    NSBitmapImageRep *bitmapImageRep = [[NSBitmapImageRep alloc] initWithCGImage:pImage];
    if (bitmapImageRep) 
    {
        NSImage *badgeImage = [[NSImage alloc] initWithSize:[bitmapImageRep size]];
        [badgeImage addRepresentation:bitmapImageRep];
        [bitmapImageRep release];
        /* Make subsequent drawing operations on the icon */
        [gDockIconImage lockFocus];
        /* Draw the overlay bottom left */
        [badgeImage drawAtPoint:NSZeroPoint fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1.0];
        [gDockIconImage unlockFocus];
        [badgeImage release];
    }
    /* Set the new application icon */
    [NSApp setApplicationIconImage:gDockIconImage];
    
    [pool release];
}

void darwinRestoreApplicationDockTileImage (void)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    /* Reset all */
    darwinDestroyVBoxDockIconTileView();
    darwinCreateVBoxDockIconTileView();

    [pool release];
}

