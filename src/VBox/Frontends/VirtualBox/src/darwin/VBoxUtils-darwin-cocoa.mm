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

NativeWindowRef darwinToNativeWindowImpl(NativeViewRef aView)
{
    NativeWindowRef window = NULL;
    if (aView)
        window = [aView window];

    return window;
}

NativeViewRef darwinToNativeViewImpl(NativeWindowRef aWindow)
{
    NativeViewRef view = NULL;
    if (aWindow)
        view = [aWindow contentView];

    return view;
}

void darwinSetShowsToolbarButtonImpl(NativeWindowRef aWindow, bool aEnabled)
{
    [aWindow setShowsToolbarButton:aEnabled];
}

void darwinSetShowsResizeIndicatorImpl(NativeWindowRef aWindow, bool aEnabled)
{
    [aWindow setShowsResizeIndicator:aEnabled];
}

void darwinSetHidesAllTitleButtonsImpl(NativeWindowRef aWindow)
{
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

void darwinSetShowsWindowTransparentImpl(NativeWindowRef aWindow, bool aEnabled)
{
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
    extern void qt_mac_set_dock_menu(QMenu *);
    qt_mac_set_dock_menu(pMenu);
}

/**
 * Calls the + (void)setMouseCoalescingEnabled:(BOOL)flag class method.
 *
 * @param   fEnabled    Whether to enable or disable coalescing.
 */
void darwinSetMouseCoalescingEnabled(bool aEnabled)
{
    [NSEvent setMouseCoalescingEnabled:aEnabled];
}

void darwinWindowAnimateResizeImpl(NativeWindowRef aWindow, int x, int y, int width, int height)
{
    /* It seems that Qt doesn't return the height of the window with the
     * toolbar height included. So add this size manually. Could easily be that
     * the Trolls fix this in the final release. */
    NSToolbar *toolbar = [aWindow toolbar];
    NSRect windowFrame = [aWindow frame];
    int toolbarHeight = 0;
    if(toolbar && [toolbar isVisible])
        toolbarHeight = NSHeight(windowFrame) - NSHeight([[aWindow contentView] frame]);
    int h = height + toolbarHeight;
    int h1 = h - NSHeight(windowFrame);
    windowFrame.size.height = h;
    windowFrame.origin.y -= h1;

    [aWindow setFrame:windowFrame display:YES animate:YES];
}

void darwinWindowInvalidateShadowImpl(NativeWindowRef aWindow)
{
    [aWindow invalidateShadow];
}

int darwinWindowToolBarHeight(NativeWindowRef aWindow)
{
    NSToolbar *toolbar = [aWindow toolbar];
    NSRect windowFrame = [aWindow frame];
    int toolbarHeight = 0;
    int theight = (NSHeight([NSWindow contentRectForFrameRect:[aWindow frame] styleMask:[aWindow styleMask]]) - NSHeight([[aWindow contentView] frame]));
    /* toolbar height: */
    if(toolbar && [toolbar isVisible])
        /* title bar height: */
        toolbarHeight = NSHeight(windowFrame) - NSHeight([[aWindow contentView] frame]) - theight;

    return toolbarHeight;
}

bool darwinIsToolbarVisible(NativeWindowRef pWindow)
{
    NSToolbar *pToolbar = [pWindow toolbar];

    return [pToolbar isVisible] == YES;
}

bool darwinIsWindowMaximized(NativeWindowRef aWindow)
{
    bool fResult = [aWindow isZoomed];

    return fResult;
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
 * Calls the -(NSUInteger)modifierFlags method on a NSEvent object.
 *
 * @return  The Cocoa event modifier mask.
 * @param   pEvent          The Cocoa event.
 */
unsigned long darwinEventModifierFlags(ConstNativeNSEventRef pEvent)
{
    return [pEvent modifierFlags];
}

/**
 * Calls the -(NSUInteger)modifierFlags method on a NSEvent object and
 * converts the flags to carbon style.
 *
 * @return  The Carbon modifier mask.
 * @param   pEvent          The Cocoa event.
 */
uint32_t darwinEventModifierFlagsXlated(ConstNativeNSEventRef pEvent)
{
    NSUInteger  fCocoa  = [pEvent modifierFlags];
    uint32_t    fCarbon = 0;
    if (fCocoa)
    {
        if (fCocoa & NSAlphaShiftKeyMask)
            fCarbon |= alphaLock;
        if (fCocoa & (NSShiftKeyMask | NX_DEVICELSHIFTKEYMASK | NX_DEVICERSHIFTKEYMASK))
        {
            if (fCocoa & (NX_DEVICERSHIFTKEYMASK | NX_DEVICERSHIFTKEYMASK))
            {
                if (fCocoa & NX_DEVICERSHIFTKEYMASK)
                    fCarbon |= rightShiftKey;
                if (fCocoa & NX_DEVICELSHIFTKEYMASK)
                    fCarbon |= shiftKey;
            }
            else
                fCarbon |= shiftKey;
        }

        if (fCocoa & (NSControlKeyMask | NX_DEVICELCTLKEYMASK | NX_DEVICERCTLKEYMASK))
        {
            if (fCocoa & (NX_DEVICELCTLKEYMASK | NX_DEVICERCTLKEYMASK))
            {
                if (fCocoa & NX_DEVICERCTLKEYMASK)
                    fCarbon |= rightControlKey;
                if (fCocoa & NX_DEVICELCTLKEYMASK)
                    fCarbon |= controlKey;
            }
            else
                fCarbon |= controlKey;
        }

        if (fCocoa & (NSAlternateKeyMask | NX_DEVICELALTKEYMASK | NX_DEVICERALTKEYMASK))
        {
            if (fCocoa & (NX_DEVICELALTKEYMASK | NX_DEVICERALTKEYMASK))
            {
                if (fCocoa & NX_DEVICERALTKEYMASK)
                    fCarbon |= rightOptionKey;
                if (fCocoa & NX_DEVICELALTKEYMASK)
                    fCarbon |= optionKey;
            }
            else
                fCarbon |= optionKey;
        }

        if (fCocoa & (NSCommandKeyMask | NX_DEVICELCMDKEYMASK | NX_DEVICERCMDKEYMASK))
        {
            if (fCocoa & (NX_DEVICELCMDKEYMASK | NX_DEVICERCMDKEYMASK))
            {
                if (fCocoa & NX_DEVICERCMDKEYMASK)
                    fCarbon |= kEventKeyModifierRightCmdKeyMask;
                if (fCocoa & NX_DEVICELCMDKEYMASK)
                    fCarbon |= cmdKey;
            }
            else
                fCarbon |= cmdKey;
        }

        /*
        if (fCocoa & NSNumericPadKeyMask)
            fCarbon |= ???;

        if (fCocoa & NSHelpKeyMask)
            fCarbon |= ???;

        if (fCocoa & NSFunctionKeyMask)
            fCarbon |= ???;
        */
    }

    return fCarbon;
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

/**
 * Get the name for a Cocoa event type.
 *
 * @returns Read-only name string.
 * @param   eEvtType        The Cocoa event type.
 */
const char *darwinEventTypeName(unsigned long eEvtType)
{
    switch (eEvtType)
    {
#define EVT_CASE(nm) case nm: return #nm
        EVT_CASE(NSLeftMouseDown);
        EVT_CASE(NSLeftMouseUp);
        EVT_CASE(NSRightMouseDown);
        EVT_CASE(NSRightMouseUp);
        EVT_CASE(NSMouseMoved);
        EVT_CASE(NSLeftMouseDragged);
        EVT_CASE(NSRightMouseDragged);
        EVT_CASE(NSMouseEntered);
        EVT_CASE(NSMouseExited);
        EVT_CASE(NSKeyDown);
        EVT_CASE(NSKeyUp);
        EVT_CASE(NSFlagsChanged);
        EVT_CASE(NSAppKitDefined);
        EVT_CASE(NSSystemDefined);
        EVT_CASE(NSApplicationDefined);
        EVT_CASE(NSPeriodic);
        EVT_CASE(NSCursorUpdate);
        EVT_CASE(NSScrollWheel);
        EVT_CASE(NSTabletPoint);
        EVT_CASE(NSTabletProximity);
        EVT_CASE(NSOtherMouseDown);
        EVT_CASE(NSOtherMouseUp);
        EVT_CASE(NSOtherMouseDragged);
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
        EVT_CASE(NSEventTypeGesture);
        EVT_CASE(NSEventTypeMagnify);
        EVT_CASE(NSEventTypeSwipe);
        EVT_CASE(NSEventTypeRotate);
        EVT_CASE(NSEventTypeBeginGesture);
        EVT_CASE(NSEventTypeEndGesture);
#endif
#undef EVT_CASE
        default:
            return "Unknown!";
    }
}

/**
 * Debug helper function for dumping a Cocoa event to stdout.
 *
 * @param   pszPrefix       Message prefix.
 * @param   pEvent          The Cocoa event.
 */
void darwinPrintEvent(const char *pszPrefix, ConstNativeNSEventRef pEvent)
{
    NSEventType         eEvtType = [pEvent type];
    NSUInteger          fEvtMask = [pEvent modifierFlags];
    NSWindow           *pEvtWindow = [pEvent window];
    NSInteger           iEvtWindow = [pEvent windowNumber];
    NSGraphicsContext  *pEvtGraphCtx = [pEvent context];

    printf("%s%p: Type=%lu Modifiers=%08lx pWindow=%p #Wnd=%ld pGraphCtx=%p %s\n",
           pszPrefix, (void*)pEvent, (unsigned long)eEvtType, (unsigned long)fEvtMask, (void*)pEvtWindow,
           (long)iEvtWindow, (void*)pEvtGraphCtx, darwinEventTypeName(eEvtType));

    /* dump type specific into. */
    switch (eEvtType)
    {
        case NSLeftMouseDown:
        case NSLeftMouseUp:
        case NSRightMouseDown:
        case NSRightMouseUp:
        case NSMouseMoved:

        case NSLeftMouseDragged:
        case NSRightMouseDragged:
        case NSMouseEntered:
        case NSMouseExited:
            break;

        case NSKeyDown:
        case NSKeyUp:
        {
            NSUInteger i;
            NSUInteger cch;
            NSString *pChars = [pEvent characters];
            NSString *pCharsIgnMod = [pEvent charactersIgnoringModifiers];
            BOOL fIsARepeat = [pEvent isARepeat];
            unsigned short KeyCode = [pEvent keyCode];

            printf("    KeyCode=%04x isARepeat=%d", KeyCode, fIsARepeat);
            if (pChars)
            {
                cch = [pChars length];
                printf(" characters={");
                for (i = 0; i < cch; i++)
                    printf(i == 0 ? "%02x" : ",%02x", [pChars characterAtIndex: i]);
                printf("}");
            }

            if (pCharsIgnMod)
            {
                cch = [pCharsIgnMod length];
                printf(" charactersIgnoringModifiers={");
                for (i = 0; i < cch; i++)
                    printf(i == 0 ? "%02x" : ",%02x", [pCharsIgnMod characterAtIndex: i]);
                printf("}");
            }
            printf("\n");
            break;
        }

        case NSFlagsChanged:
        {
            NSUInteger fOddBits = NSAlphaShiftKeyMask | NSShiftKeyMask | NSControlKeyMask | NSAlternateKeyMask
                                | NSCommandKeyMask | NSNumericPadKeyMask | NSHelpKeyMask | NSFunctionKeyMask
                                | NX_DEVICELCTLKEYMASK | NX_DEVICELSHIFTKEYMASK | NX_DEVICERSHIFTKEYMASK
                                | NX_DEVICELCMDKEYMASK | NX_DEVICERCMDKEYMASK | NX_DEVICELALTKEYMASK
                                | NX_DEVICERALTKEYMASK | NX_DEVICERCTLKEYMASK;

            printf("    KeyCode=%04x", (int)[pEvent keyCode]);
#define PRINT_MOD(cnst, nm) do { if (fEvtMask & (cnst)) printf(" %s", #nm); } while (0)
            /* device-independent: */
            PRINT_MOD(NSAlphaShiftKeyMask, "AlphaShift");
            PRINT_MOD(NSShiftKeyMask, "Shift");
            PRINT_MOD(NSControlKeyMask, "Ctrl");
            PRINT_MOD(NSAlternateKeyMask, "Alt");
            PRINT_MOD(NSCommandKeyMask, "Cmd");
            PRINT_MOD(NSNumericPadKeyMask, "NumLock");
            PRINT_MOD(NSHelpKeyMask, "Help");
            PRINT_MOD(NSFunctionKeyMask, "Fn");
            /* device-dependent (sort of): */
            PRINT_MOD(NX_DEVICELCTLKEYMASK,   "$L-Ctrl");
            PRINT_MOD(NX_DEVICELSHIFTKEYMASK, "$L-Shift");
            PRINT_MOD(NX_DEVICERSHIFTKEYMASK, "$R-Shift");
            PRINT_MOD(NX_DEVICELCMDKEYMASK,   "$L-Cmd");
            PRINT_MOD(NX_DEVICERCMDKEYMASK,   "$R-Cmd");
            PRINT_MOD(NX_DEVICELALTKEYMASK,   "$L-Alt");
            PRINT_MOD(NX_DEVICERALTKEYMASK,   "$R-Alt");
            PRINT_MOD(NX_DEVICERCTLKEYMASK,   "$R-Ctrl");
#undef  PRINT_MOD

            fOddBits = fEvtMask & ~fOddBits;
            if (fOddBits)
                printf(" fOddBits=%#08lx", (unsigned long)fOddBits);
#undef  KNOWN_BITS
            printf("\n");
            break;
        }

        case NSAppKitDefined:
        case NSSystemDefined:
        case NSApplicationDefined:
        case NSPeriodic:
        case NSCursorUpdate:
        case NSScrollWheel:
        case NSTabletPoint:
        case NSTabletProximity:
        case NSOtherMouseDown:
        case NSOtherMouseUp:
        case NSOtherMouseDragged:
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
        case NSEventTypeGesture:
        case NSEventTypeMagnify:
        case NSEventTypeSwipe:
        case NSEventTypeRotate:
        case NSEventTypeBeginGesture:
        case NSEventTypeEndGesture:
#endif
        default:
            printf(" Unknown!\n");
            break;
    }
}

