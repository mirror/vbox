/* $Id$ */
/** @file
 * IPRT - Execute Once.
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/once.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#include <iprt/asm.h>



RTDECL(int) RTOnce(PRTONCE pOnce, PFNRTONCE pfnOnce, void *pvUser1, void *pvUser2)
{
    /*
     * Validate input (strict builds only).
     */
    AssertPtr(pOnce);
    AssertPtr(pfnOnce);

    /*
     * Deal with the 'initialized' case first
     */
    int32_t iState = ASMAtomicUoReadS32(&pOnce->iState);
    if (RT_LIKELY(iState == 2))
        return ASMAtomicUoReadS32(&pOnce->rc);
    AssertReturn(iState == -1 || iState == 1, VERR_INTERNAL_ERROR);

    /*
     * Do we initialize it?
     */
    if (iState == -1)
    {
        RTSEMEVENTMULTI hEventMulti;
        int rc = RTSemEventMultiCreate(&hEventMulti);
        if (RT_FAILURE(rc))
            hEventMulti = NIL_RTSEMEVENTMULTI;

        if (ASMAtomicCmpXchgS32(&pOnce->iState, 1, -1))
        {
            ASMAtomicWriteHandle(&pOnce->hEventMulti, hEventMulti);
            ASMAtomicIncS32(&pOnce->cEventRefs);

            /* do the execute once stuff. */
            int32_t rcOnce = pfnOnce(pvUser1, pvUser2);

            /* set the return code, change the state and signal any waiters. */
            ASMAtomicWriteS32(&pOnce->rc, rcOnce);
            ASMAtomicWriteS32(&pOnce->iState, 2);
            if (hEventMulti != NIL_RTSEMEVENTMULTI)
                RTSemEventMultiSignal(hEventMulti);

            /* last guy destroys the semaphore. */
            if (ASMAtomicDecS32(&pOnce->cEventRefs) == 0)
                ASMAtomicWriteSize(&pOnce->hEventMulti, NIL_RTSEMEVENTMULTI);
            else
                hEventMulti = NIL_RTSEMEVENTMULTI;
        }
        if (hEventMulti != NIL_RTSEMEVENTMULTI)
            RTSemEventMultiDestroy(hEventMulti);
    }

    /*
     * Wait for it to finish initializing.
     */
    if (ASMAtomicReadS32(&pOnce->iState) == 1)
    {
        int i = 0;
        while (ASMAtomicReadS32(&pOnce->iState) == 1)
        {
            bool fYieldSleep = true;

            /*
             * Take care not to increment the counter if it's 0, that indicates
             * that RTONCE::hEventMulti isn't valid either because it's not set
             * yet, or because it's being destroyed.
             */
            int32_t cEventRefs = ASMAtomicUoReadS32(&pOnce->cEventRefs);
            while (   cEventRefs > 0
                   && !ASMAtomicCmpXchgS32(&pOnce->cEventRefs, cEventRefs + 1, cEventRefs))
                cEventRefs = ASMAtomicUoReadS32(&pOnce->cEventRefs);
            if (cEventRefs > 1)
            {
                /*
                 * The hEventMulti might be NIL for two reasons, see above in
                 * the init code, if it isn't valid just do the yield/sleep thing.
                 */
                RTSEMEVENTMULTI hEventMulti;
                ASMAtomicUoReadSize(&pOnce->hEventMulti, &hEventMulti);
                if (hEventMulti != NIL_RTSEMEVENTMULTI)
                {
                    fYieldSleep = false;
                    RTSemEventMultiWait(hEventMulti, RT_INDEFINITE_WAIT);
                }

                /*
                 * Last thread cleans up.
                 */
                if (ASMAtomicDecS32(&pOnce->cEventRefs) == 0)
                {
                    ASMAtomicXchgHandle(&pOnce->hEventMulti, NIL_RTSEMEVENTMULTI, &hEventMulti);
                    if (hEventMulti != NIL_RTSEMEVENTMULTI)
                        RTSemEventMultiDestroy(hEventMulti);
                }
            }

            /*
             * If we didn't block, yield or sleep for a bit.
             *
             * The sleep is essential to prevent higher priority threads from spinning wildly
             * and preventing a lower priority thread from completing the pfnOnce operation
             * in a timely manner.
             */
            if (fYieldSleep)
            {
                if (ASMAtomicReadS32(&pOnce->iState) != 1)
                    break;
                if (!(++i % 8) )
                    RTThreadSleep(1);
                else
                    RTThreadYield();
            }
        }
    }

    /*
     * Finally, return the status code from the execute once function.
     */
    return ASMAtomicUoReadS32(&pOnce->rc);
}


RTDECL(void) RTOnceReset(PRTONCE pOnce)
{
    /* Cannot be done while busy! */
    AssertPtr(pOnce);
    Assert(pOnce->hEventMulti == NIL_RTSEMEVENTMULTI);
    Assert(pOnce->iState != 1);

    /* Do the same as RTONCE_INITIALIZER does. */
    ASMAtomicWriteS32(&pOnce->rc, VERR_INTERNAL_ERROR);
    ASMAtomicWriteS32(&pOnce->iState, -1);
}

