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

NativeWindowRef darwinToNativeWindowImpl (NativeViewRef aView)
{
    CocoaAutoreleasePool pool;

    NativeWindowRef window = NULL;
    if (aView)
        window = [aView window];

    return window;
}

NativeViewRef darwinToNativeViewImpl (NativeWindowRef aWindow)
{
    CocoaAutoreleasePool pool;

    NativeViewRef view = NULL;
    if (aWindow)
        view = [aWindow contentView];

    return view;
}

void darwinSetShowsToolbarButtonImpl (NativeWindowRef aWindow, bool aEnabled)
{
    CocoaAutoreleasePool pool;

    [aWindow setShowsToolbarButton:aEnabled];
}

void darwinSetShowsResizeIndicatorImpl (NativeWindowRef aWindow, bool aEnabled)
{
    CocoaAutoreleasePool pool;

    [aWindow setShowsResizeIndicator:aEnabled];
}

void darwinSetHidesAllTitleButtonsImpl (NativeWindowRef aWindow)
{
    CocoaAutoreleasePool pool;

    /* Remove all title buttons by changing the style mask. This method is
       available from 10.6 on only. */
    if ([aWindow respondsToSelector: @selector(setStyleMask:)])
        [aWindow performSelector: @selector(setStyleMask:) withObject: (id)NSTitledWindowMask];
    else
    {
        /* On pre 10.6 disable all the buttons currently displayed. Don't use
           setHidden cause this remove the buttons, but didn't release the
           place used for the buttons. */
        NSButton *pButton = [aWindow standardWindowButton:NSWindowCloseButton];
        if (pButton != Nil)
            [pButton setEnabled: NO];
        pButton = [aWindow standardWindowButton:NSWindowMiniaturizeButton];
        if (pButton != Nil)
            [pButton setEnabled: NO];
        pButton = [aWindow standardWindowButton:NSWindowZoomButton];
        if (pButton != Nil)
            [pButton setEnabled: NO];
        pButton = [aWindow standardWindowButton:NSWindowDocumentIconButton];
        if (pButton != Nil)
            [pButton setEnabled: NO];
    }
}

void darwinSetShowsWindowTransparentImpl (NativeWindowRef aWindow, bool aEnabled)
{
    CocoaAutoreleasePool pool;

    if (aEnabled)
    {
        [aWindow setOpaque:NO];
        [aWindow setBackgroundColor:[NSColor clearColor]];
        [aWindow setHasShadow:NO];
    }
    else
    {
        [aWindow setOpaque:YES];
        [aWindow setBackgroundColor:[NSColor windowBackgroundColor]];
        [aWindow setHasShadow:YES];
    }
}

void darwinSetDockIconMenu(QMenu* pMenu)
{
    CocoaAutoreleasePool pool;

    extern void qt_mac_set_dock_menu(QMenu *);
    qt_mac_set_dock_menu(pMenu);
}

/**
 * Calls the + (void)setMouseCoalescingEnabled:(BOOL)flag class method.
 *
 * @param   fEnabled    Whether to enable or disable coalescing.
 */
void darwinSetMouseCoalescingEnabled (bool aEnabled)
{
    CocoaAutoreleasePool pool;

    [NSEvent setMouseCoalescingEnabled:aEnabled];
}

void darwinWindowAnimateResizeImpl (NativeWindowRef aWindow, int x, int y, int width, int height)
{
    CocoaAutoreleasePool pool;

    /* It seems that Qt doesn't return the height of the window with the
     * toolbar height included. So add this size manually. Could easily be that
     * the Trolls fix this in the final release. */
    NSToolbar *toolbar = [aWindow toolbar];
    NSRect windowFrame = [aWindow frame];
    int toolbarHeight = 0;
    if(toolbar && [toolbar isVisible])
        toolbarHeight = NSHeight (windowFrame) - NSHeight ([[aWindow contentView] frame]);
    int h = height + toolbarHeight;
    int h1 = h - NSHeight (windowFrame);
    windowFrame.size.height = h;
    windowFrame.origin.y -= h1;

    [aWindow setFrame:windowFrame display:YES animate:YES];
}

void darwinWindowInvalidateShadowImpl (NativeWindowRef aWindow)
{
    CocoaAutoreleasePool pool;

    [aWindow invalidateShadow];
}

int darwinWindowToolBarHeight (NativeWindowRef aWindow)
{
    CocoaAutoreleasePool pool;

    NSToolbar *toolbar = [aWindow toolbar];
    NSRect windowFrame = [aWindow frame];
    int toolbarHeight = 0;
    int theight = (NSHeight([NSWindow contentRectForFrameRect:[aWindow frame] styleMask:[aWindow styleMask]]) - NSHeight([[aWindow contentView] frame]));
    /* toolbar height: */
    if(toolbar && [toolbar isVisible])
        /* title bar height: */
        toolbarHeight = NSHeight (windowFrame) - NSHeight ([[aWindow contentView] frame]) - theight;

    return toolbarHeight;
}

bool darwinIsToolbarVisible(NativeWindowRef pWindow)
{
    CocoaAutoreleasePool pool;

    NSToolbar *pToolbar = [pWindow toolbar];

    return [pToolbar isVisible] == YES;
}

bool darwinIsWindowMaximized(NativeWindowRef aWindow)
{
    CocoaAutoreleasePool pool;

    bool fResult = [aWindow isZoomed];

    return fResult;
}

float darwinSmallFontSize()
{
    CocoaAutoreleasePool pool;

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

