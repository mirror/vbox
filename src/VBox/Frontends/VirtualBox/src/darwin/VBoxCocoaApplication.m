/* $Id$ */
/** @file
 * VBoxCocoaApplication - NSApplication subclass for handling -sendEvent.
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
 *
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "VBoxCocoaApplication.h"
#include "DarwinKeyboard.h"
#include <iprt/assert.h>
#import <AppKit/NSEvent.h>

#include <stdio.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Pointer to the VBoxCocoaApplication instance.
 * This is also available thru NSApp, but this way there is no need to cast any thing.
 */
VBoxCocoaApplication *g_pVBoxCocoaApp = NULL;

@implementation VBoxCocoaApplication


-(id) init;
{
    self = [super init];
    self->m_cCallbacks = 0;
    return self;
}


-(void) sendEvent:(NSEvent *)pEvent;
{
    /*
     * Check if the type matches any of the registered callbacks.
     */
    uint32_t const  fMask = self->m_fMask;
#if 0 /* for debugging */
    VBoxCocoaApplication_printEvent("sendEvent: ", pEvent);
#endif
    if (fMask != 0)
    {
        NSEventType EvtType = [pEvent type];
        uint32_t    fEvtMask = RT_LIKELY(EvtType < 32) ? RT_BIT_32(EvtType) : 0;
        if (fMask & fEvtMask)
        {
            /*
             * Do the callouts in LIFO order.
             */
            int i = self->m_cCallbacks;
            PCVBOXCAENTRY pCur = &self->m_aCallbacks[i];
            while (i-- > 0)
            {
                pCur--;
                if (pCur->fMask & fEvtMask)
                {
                    if (pCur->pfnCallback(pEvent, [pEvent eventRef], pCur->pvUser))
                        return;
                }
            }
        }
    }

    /*
     * Get on with it.
     */
    [super sendEvent:pEvent];
}


/**
 * Register an event callback.
 *
 * @param   fMask           The event mask for which the callback is to be invoked.
 * @param   pfnCallback     The callback function.
 * @param   pvUser          The user argument.
 */
-(void) setCallback: (uint32_t)fMask :(PFNVBOXCACALLBACK)pfnCallback :(void *)pvUser
{
    unsigned i = self->m_cCallbacks;
    AssertReleaseReturnVoid(i < RT_ELEMENTS(self->m_aCallbacks));

    self->m_aCallbacks[i].pfnCallback = pfnCallback;
    self->m_aCallbacks[i].pvUser      = pvUser;
    self->m_aCallbacks[i].fMask       = fMask;
    self->m_cCallbacks++;
    self->m_fMask |= fMask;
}


/**
 * Deregister an event callback.
 *
 * @param   fMask           Same as setCallback.
 * @param   pfnCallback     Same as setCallback.
 * @param   pvUser          Same as setCallback.
 */
-(void) unsetCallback: (uint32_t)fMask :(PFNVBOXCACALLBACK)pfnCallback :(void *)pvUser
{
    /*
     * Loop the event array LIFO fashion searching for a matching callback.
     */
    int i = self->m_cCallbacks;
    while (i-- > 0)
    {
        if (    self->m_aCallbacks[i].pfnCallback == pfnCallback
            &&  self->m_aCallbacks[i].pvUser      == pvUser
            &&  self->m_aCallbacks[i].fMask       == fMask)
        {
            uint32_t fNewMask;

            if (i + 1 != self->m_cCallbacks)
                self->m_aCallbacks[i] = self->m_aCallbacks[self->m_cCallbacks - 1];
            self->m_cCallbacks--;

            /*
             * Recalculate the event type mask.
             */
            fNewMask = 0;
            i = self->m_cCallbacks;
            while (i-- > 0)
                fNewMask |= self->m_aCallbacks[i].fMask;
            self->m_fMask = fNewMask;
            return;
        }
    }
    AssertFailed();
}


@end /* @implementation VBoxCocoaApplication */


/**
 * C/C++ interface for calling VBoxCocoaApplication::sharedApplication.
 *
 */
void VBoxCocoaApplication_sharedApplication(void)
{
    if (!g_pVBoxCocoaApp)
    {
        /*
         * It is essential that we use the inherited sharedApplication class
         * method, otherwise we'll be screwed later by static variables in it.
         */
        NSApplication *pApp = [VBoxCocoaApplication sharedApplication];
        g_pVBoxCocoaApp = (VBoxCocoaApplication *)pApp;
    }
}


/**
 * Register an event callback.
 *
 * @param   fMask           The event mask for which the callback is to be invoked.
 * @param   pfnCallback     The callback function.
 * @param   pvUser          The user argument.
 */
void VBoxCocoaApplication_setCallback(uint32_t fMask, PFNVBOXCACALLBACK pfnCallback, void *pvUser)
{
    [g_pVBoxCocoaApp setCallback:fMask :pfnCallback :pvUser];
}


/**
 * Deregister an event callback.
 *
 * @param   fMask           Same as setCallback.
 * @param   pfnCallback     Same as setCallback.
 * @param   pvUser          Same as setCallback.
 */
void VBoxCocoaApplication_unsetCallback(uint32_t fMask, PFNVBOXCACALLBACK pfnCallback, void *pvUser)
{
    [g_pVBoxCocoaApp unsetCallback:fMask :pfnCallback :pvUser];
}


/**
 * Calls the -(NSUInteger)modifierFlags method on a NSEvent object.
 *
 * @return  The Cocoa event modifier mask.
 * @param   pvEvent     The NSEvent object.
 */
unsigned long VBoxCocoaApplication_getEventModifierFlags(const void *pvEvent)
{
    NSEvent *pEvent = (NSEvent *)pvEvent;
    return [pEvent modifierFlags];
}


/**
 * Calls the -(NSUInteger)modifierFlags method on a NSEvent object and
 * converts the flags to carbon style.
 *
 * @return  The Carbon modifier mask.
 * @param   pvEvent     The NSEvent object.
 */
uint32_t VBoxCocoaApplication_getEventModifierFlagsXlated(const void *pvEvent)
{
    NSEvent    *pEvent  = (NSEvent *)pvEvent;
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

        //if (fCocoa & NSNumericPadKeyMask)
        //    fCarbon |= ???;

        //if (fCocoa & NSHelpKeyMask)
        //    fCarbon |= ???;

        //if (fCocoa & NSFunctionKeyMask)
        //    fCarbon |= ???;
    }

    return fCarbon;
}


/**
 * Get the name for a Cocoa event type.
 *
 * @returns Read-only name string.
 * @param   eEvtType        The Cocoa event type.
 */
const char *VBoxCocoaApplication_eventTypeName(unsigned long eEvtType)
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
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5
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
 * @param   pvEvent         The Cocoa event.
 */
void VBoxCocoaApplication_printEvent(const char *pszPrefix, const void *pvEvent)
{
    NSEvent            *pEvent = (NSEvent *)pvEvent;
    NSEventType         eEvtType = [pEvent type];
    NSUInteger          fEvtMask = [pEvent modifierFlags];
    NSWindow           *pEvtWindow = [pEvent window];
    NSInteger           iEvtWindow = [pEvent windowNumber];
    NSGraphicsContext  *pEvtGraphCtx = [pEvent context];

    printf("%s%p: Type=%lu Modifiers=%08lx pWindow=%p #Wnd=%ld pGraphCtx=%p %s\n",
           pszPrefix, pvEvent, (unsigned long)eEvtType, (unsigned long)fEvtMask, pEvtWindow,
           (long)iEvtWindow, pEvtGraphCtx, VBoxCocoaApplication_eventTypeName(eEvtType));

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
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5
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

