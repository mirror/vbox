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

printf("EvtType=%d modifierFlags=%#x\n", (int)[pEvent type], (unsigned)[pEvent modifierFlags]);
//    NSKeyDown                   = 10,
//    NSKeyUp                     = 11,
//    NSFlagsChanged              = 12,

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

