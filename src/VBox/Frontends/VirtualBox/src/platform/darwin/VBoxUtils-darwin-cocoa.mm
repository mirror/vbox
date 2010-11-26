/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Declarations of utility classes and functions for handling Darwin Cocoa
 * specific tasks
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxUtils-darwin.h"
#include "VBoxCocoaHelper.h"

#include <QMenu>

#include <iprt/assert.h>

#import <AppKit/NSEvent.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSFont.h>

/* For the keyboard stuff */
#include <Carbon/Carbon.h>
#include "DarwinKeyboard.h"

NativeNSWindowRef darwinToNativeWindowImpl(NativeNSViewRef pView)
{
    NativeNSWindowRef window = NULL;
    if (pView)
        window = [pView window];

    return window;
}

NativeNSViewRef darwinToNativeViewImpl(NativeNSWindowRef pWindow)
{
    NativeNSViewRef view = NULL;
    if (pWindow)
        view = [pWindow contentView];

    return view;
}

NativeNSImageRef darwinToNSImageRef(const CGImageRef pImage)
{
    /* Create a bitmap rep from the image. */
    NSBitmapImageRep *bitmapRep = [[[NSBitmapImageRep alloc] initWithCGImage:pImage] autorelease];
    /* Create an NSImage and add the bitmap rep to it */
    NSImage *image = [[NSImage alloc] init];
    [image addRepresentation:bitmapRep];
    return image;
}

NativeNSImageRef darwinToNSImageRef(const QImage *pImage)
{
   CGImageRef pCGImage = ::darwinToCGImageRef(pImage);
   NativeNSImageRef pNSImage = ::darwinToNSImageRef(pCGImage);
   CGImageRelease(pCGImage);
   return pNSImage;
}

NativeNSImageRef darwinToNSImageRef(const QPixmap *pPixmap)
{
   CGImageRef pCGImage = ::darwinToCGImageRef(pPixmap);
   NativeNSImageRef pNSImage = ::darwinToNSImageRef(pCGImage);
   CGImageRelease(pCGImage);
   return pNSImage;
}

NativeNSImageRef darwinToNSImageRef(const char *pczSource)
{
   CGImageRef pCGImage = ::darwinToCGImageRef(pczSource);
   NativeNSImageRef pNSImage = ::darwinToNSImageRef(pCGImage);
   CGImageRelease(pCGImage);
   return pNSImage;
}

NativeNSStringRef darwinToNativeString(const char* pcszString)
{
    return [NSString stringWithUTF8String: pcszString];
}

void darwinSetShowsToolbarButtonImpl(NativeNSWindowRef pWindow, bool fEnabled)
{
    [pWindow setShowsToolbarButton:fEnabled];
}

void darwinSetShowsResizeIndicatorImpl(NativeNSWindowRef pWindow, bool fEnabled)
{
    [pWindow setShowsResizeIndicator:fEnabled];
}

void darwinSetHidesAllTitleButtonsImpl(NativeNSWindowRef pWindow)
{
    /* Remove all title buttons by changing the style mask. This method is
       available from 10.6 on only. */
    if ([pWindow respondsToSelector: @selector(setStyleMask:)])
        [pWindow performSelector: @selector(setStyleMask:) withObject: (id)NSTitledWindowMask];
    else
    {
        /* On pre 10.6 disable all the buttons currently displayed. Don't use
           setHidden cause this remove the buttons, but didn't release the
           place used for the buttons. */
        NSButton *pButton = [pWindow standardWindowButton:NSWindowCloseButton];
        if (pButton != Nil)
            [pButton setEnabled: NO];
        pButton = [pWindow standardWindowButton:NSWindowMiniaturizeButton];
        if (pButton != Nil)
            [pButton setEnabled: NO];
        pButton = [pWindow standardWindowButton:NSWindowZoomButton];
        if (pButton != Nil)
            [pButton setEnabled: NO];
        pButton = [pWindow standardWindowButton:NSWindowDocumentIconButton];
        if (pButton != Nil)
            [pButton setEnabled: NO];
    }
}

void darwinSetShowsWindowTransparentImpl(NativeNSWindowRef pWindow, bool fEnabled)
{
    if (fEnabled)
    {
        [pWindow setOpaque:NO];
        [pWindow setBackgroundColor:[NSColor clearColor]];
        [pWindow setHasShadow:NO];
    }
    else
    {
        [pWindow setOpaque:YES];
        [pWindow setBackgroundColor:[NSColor windowBackgroundColor]];
        [pWindow setHasShadow:YES];
    }
}

void darwinMinaturizeWindow(NativeNSWindowRef pWindow)
{
//    [[NSApplication sharedApplication] miniaturizeAll];
//    printf("bla\n");
//    [pWindow miniaturize:pWindow];
//    [[NSApplication sharedApplication] deactivate];
//    [pWindow performMiniaturize:nil];
}

void darwinSetDockIconMenu(QMenu* pMenu)
{
    extern void qt_mac_set_dock_menu(QMenu *);
    qt_mac_set_dock_menu(pMenu);
}

/**
 * Calls the + (void)setMouseCoalescingEnabled:(BOOL)flag class method.
 *
 * @param   fEnabled    Whether to enable or disable coalescing.
 */
void darwinSetMouseCoalescingEnabled(bool fEnabled)
{
    [NSEvent setMouseCoalescingEnabled:fEnabled];
}

void darwinWindowAnimateResizeImpl(NativeNSWindowRef pWindow, int x, int y, int width, int height)
{
    /* It seems that Qt doesn't return the height of the window with the
     * toolbar height included. So add this size manually. Could easily be that
     * the Trolls fix this in the final release. */
    NSToolbar *toolbar = [pWindow toolbar];
    NSRect windowFrame = [pWindow frame];
    int toolbarHeight = 0;
    if(toolbar && [toolbar isVisible])
        toolbarHeight = NSHeight(windowFrame) - NSHeight([[pWindow contentView] frame]);
    int h = height + toolbarHeight;
    int h1 = h - NSHeight(windowFrame);
    windowFrame.size.height = h;
    windowFrame.origin.y -= h1;

    [pWindow setFrame:windowFrame display:YES animate: YES];
}

void darwinWindowAnimateResizeNewImpl(NativeNSWindowRef pWindow, int height, bool fAnimate)
{
    /* It seems that Qt doesn't return the height of the window with the
     * toolbar height included. So add this size manually. Could easily be that
     * the Trolls fix this in the final release. */
    NSToolbar *toolbar = [pWindow toolbar];
    NSRect windowFrame = [pWindow frame];
    int toolbarHeight = 0;
    if(toolbar && [toolbar isVisible])
        toolbarHeight = NSHeight(windowFrame) - NSHeight([[pWindow contentView] frame]);
    int h = height + toolbarHeight;
    int h1 = h - NSHeight(windowFrame);
    windowFrame.size.height = h;
    windowFrame.origin.y -= h1;

    [pWindow setFrame:windowFrame display:YES animate: fAnimate ? YES : NO];
}

void darwinTest(NativeNSViewRef pViewOld, NativeNSViewRef pViewNew, int h)
{
    NSMutableDictionary *pDicts[3] = { nil, nil, nil };
    int c = 0;

    /* Scaling necessary? */
    if (h != -1)
    {
        NSWindow *pWindow  = [(pViewOld ? pViewOld : pViewNew) window];
        NSToolbar *toolbar = [pWindow toolbar];
        NSRect windowFrame = [pWindow frame];
        /* Dictionary containing all animation parameters. */
        pDicts[c] = [NSMutableDictionary dictionaryWithCapacity:2];
        /* Specify the animation target. */
        [pDicts[c] setObject:pWindow forKey:NSViewAnimationTargetKey];
        /* Scaling effect. */
        [pDicts[c] setObject:[NSValue valueWithRect:windowFrame] forKey:NSViewAnimationStartFrameKey];
        int toolbarHeight = 0;
        if(toolbar && [toolbar isVisible])
            toolbarHeight = NSHeight(windowFrame) - NSHeight([[pWindow contentView] frame]);
        int h1 = h + toolbarHeight;
        int h2 = h1 - NSHeight(windowFrame);
        windowFrame.size.height = h1;
        windowFrame.origin.y -= h2;
        [pDicts[c] setObject:[NSValue valueWithRect:windowFrame] forKey:NSViewAnimationEndFrameKey];
        ++c;
    }
    /* Fade out effect. */
    if (pViewOld)
    {
        /* Dictionary containing all animation parameters. */
        pDicts[c] = [NSMutableDictionary dictionaryWithCapacity:2];
        /* Specify the animation target. */
        [pDicts[c] setObject:pViewOld forKey:NSViewAnimationTargetKey];
        /* Fade out effect. */
        [pDicts[c] setObject:NSViewAnimationFadeOutEffect forKey:NSViewAnimationEffectKey];
        ++c;
    }
    /* Fade in effect. */
    if (pViewNew)
    {
        /* Dictionary containing all animation parameters. */
        pDicts[c] = [NSMutableDictionary dictionaryWithCapacity:2];
        /* Specify the animation target. */
        [pDicts[c] setObject:pViewNew forKey:NSViewAnimationTargetKey];
        /* Fade in effect. */
        [pDicts[c] setObject:NSViewAnimationFadeInEffect forKey:NSViewAnimationEffectKey];
        ++c;
    }
    /* Create our animation object. */
    NSViewAnimation *pAni = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObjects:pDicts count:c]];
    [pAni setDuration:.15];
    [pAni setAnimationCurve:NSAnimationEaseIn];
    [pAni setAnimationBlockingMode:NSAnimationBlocking];
//    [pAni setAnimationBlockingMode:NSAnimationNonblockingThreaded];

    /* Run the animation. */
    [pAni startAnimation];
    /* Cleanup */
    [pAni release];
}

void darwinWindowInvalidateShadowImpl(NativeNSWindowRef pWindow)
{
    [pWindow invalidateShadow];
}

int darwinWindowToolBarHeight(NativeNSWindowRef pWindow)
{
    NSToolbar *toolbar = [pWindow toolbar];
    NSRect windowFrame = [pWindow frame];
    int toolbarHeight = 0;
    int theight = (NSHeight([NSWindow contentRectForFrameRect:[pWindow frame] styleMask:[pWindow styleMask]]) - NSHeight([[pWindow contentView] frame]));
    /* toolbar height: */
    if(toolbar && [toolbar isVisible])
        /* title bar height: */
        toolbarHeight = NSHeight(windowFrame) - NSHeight([[pWindow contentView] frame]) - theight;

    return toolbarHeight;
}

bool darwinIsToolbarVisible(NativeNSWindowRef pWindow)
{
    NSToolbar *pToolbar = [pWindow toolbar];

    return [pToolbar isVisible] == YES;
}

bool darwinIsWindowMaximized(NativeNSWindowRef pWindow)
{
    bool fResult = [pWindow isZoomed];

    return fResult;
}

bool darwinOpenFile(NativeNSStringRef pstrFile)
{
    return [[NSWorkspace sharedWorkspace] openFile:pstrFile];
}

float darwinSmallFontSize()
{
    float size = [NSFont systemFontSizeForControlSize: NSSmallControlSize];

    return size;
}

/* Cocoa event handler which checks if the user right clicked at the unified
   toolbar or the title area. */
bool darwinUnifiedToolbarEvents(const void *pvCocoaEvent, const void *pvCarbonEvent, void *pvUser)
{
    NSEvent *pEvent = (NSEvent*)pvCocoaEvent;
    NSEventType EvtType = [pEvent type];
    NSWindow *pWin = ::darwinToNativeWindow((QWidget*)pvUser);
    /* First check for the right event type and that we are processing events
       from the window which was registered by the user. */
    if (   EvtType == NSRightMouseDown
        && pWin == [pEvent window])
    {
        /* Get the mouse position of the event (screen coordinates) */
        NSPoint point = [NSEvent mouseLocation];
        /* Get the frame rectangle of the window (screen coordinates) */
        NSRect winFrame = [pWin frame];
        /* Calculate the height of the title and the toolbar */
        int i = NSHeight(winFrame) - NSHeight([[pWin contentView] frame]);
        /* Based on that height create a rectangle of the unified toolbar + title */
        winFrame.origin.y += winFrame.size.height - i;
        winFrame.size.height = i;
        /* Check if the mouse press event was on the unified toolbar or title */
        if (NSMouseInRect(point, winFrame, NO))
            /* Create a Qt context menu event, with flipped screen coordinates */
            ::darwinCreateContextMenuEvent(pvUser, point.x, NSHeight([[pWin screen] frame]) - point.y);
    }
    return false;
}

/**
 * Check for some default application key combinations a Mac user expect, like
 * CMD+Q or CMD+H.
 *
 * @returns true if such a key combo was hit, false otherwise.
 * @param   pEvent          The Cocoa event.
 */
bool darwinIsApplicationCommand(ConstNativeNSEventRef pEvent)
{
    NSEventType  eEvtType = [pEvent type];
    bool         fGlobalHotkey = false;
//
//    if (   (eEvtType == NSKeyDown || eEvtType == NSKeyUp)
//        && [[NSApp mainMenu] performKeyEquivalent:pEvent])
//        return true;
//    return false;
//        && [[[NSApp mainMenu] delegate] menuHasKeyEquivalent:[NSApp mainMenu] forEvent:pEvent target:b action:a])

    switch (eEvtType)
    {
        case NSKeyDown:
        case NSKeyUp:
        {
            NSUInteger fEvtMask = [pEvent modifierFlags];
            unsigned short KeyCode = [pEvent keyCode];
            if (   ((fEvtMask & (NX_NONCOALSESCEDMASK | NX_COMMANDMASK | NX_DEVICELCMDKEYMASK)) == (NX_NONCOALSESCEDMASK | NX_COMMANDMASK | NX_DEVICELCMDKEYMASK))  /* L+CMD */
                || ((fEvtMask & (NX_NONCOALSESCEDMASK | NX_COMMANDMASK | NX_DEVICERCMDKEYMASK)) == (NX_NONCOALSESCEDMASK | NX_COMMANDMASK | NX_DEVICERCMDKEYMASK))) /* R+CMD */
            {
                if (   KeyCode == 0x0c  /* CMD+Q (Quit) */
                    || KeyCode == 0x04) /* CMD+H (Hide) */
                    fGlobalHotkey = true;
            }
            else if (   ((fEvtMask & (NX_NONCOALSESCEDMASK | NX_ALTERNATEMASK | NX_DEVICELALTKEYMASK | NX_COMMANDMASK | NX_DEVICELCMDKEYMASK)) == (NX_NONCOALSESCEDMASK | NX_ALTERNATEMASK | NX_DEVICELALTKEYMASK | NX_COMMANDMASK | NX_DEVICELCMDKEYMASK)) /* L+ALT+CMD */
                     || ((fEvtMask & (NX_NONCOALSESCEDMASK | NX_ALTERNATEMASK | NX_DEVICERCMDKEYMASK | NX_COMMANDMASK | NX_DEVICERCMDKEYMASK)) == (NX_NONCOALSESCEDMASK | NX_ALTERNATEMASK | NX_DEVICERCMDKEYMASK | NX_COMMANDMASK | NX_DEVICERCMDKEYMASK))) /* R+ALT+CMD */
            {
                if (KeyCode == 0x04)    /* ALT+CMD+H (Hide-Others) */
                    fGlobalHotkey = true;
            }
            break;
        }
        default: break;
    }
    return fGlobalHotkey;
}

void darwinRetranslateAppMenu()
{
    /* This is purely Qt internal. If the Trolls change something here, it will
       not work anymore, but at least it will not be a burning man. */
    if ([NSApp respondsToSelector:@selector(qt_qcocoamenuLoader)])
    {
        id loader = [NSApp performSelector:@selector(qt_qcocoamenuLoader)];
        if ([loader respondsToSelector:@selector(qtTranslateApplicationMenu)])
            [loader performSelector:@selector(qtTranslateApplicationMenu)];
    }
}

