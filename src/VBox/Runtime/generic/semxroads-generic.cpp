/* $Id$ */
/** @file
 * IPRT Testcase - RTSemXRoads, generic implementation.
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
#include <iprt/semaphore.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/thread.h>

#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct RTSEMXROADSINTERNAL
{
    /** Magic value (RTSEMXROADS_MAGIC).  */
    uint32_t volatile   u32Magic;
    /** The state variable.
     * All accesses are atomic and it bits are defined like this:
     *      Bits 0..14  - cNorthSouth.
     *      Bits 15..30 - cEastWest.
     *      Bit 30      - fDirection; 0=NS, 1=EW.
     *      Bit 31      - Unused.
     */
    uint32_t volatile   u32State;
    /** What the north/south bound threads are blocking on when waiting for
     * east/west traffic to stop. */
    RTSEMEVENTMULTI     hEvtNS;
    /** What the east/west bound threads are blocking on when waiting for
     * north/south traffic to stop. */
    RTSEMEVENTMULTI     hEvtEW;
} RTSEMXROADSINTERNAL;


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define RTSEMXROADS_CNT_NS_SHIFT    0
#define RTSEMXROADS_CNT_NS_MASK     UINT32_C(0x00007fff)
#define RTSEMXROADS_CNT_EW_SHIFT    15
#define RTSEMXROADS_CNT_EW_MASK     UINT32_C(0x3fff8000)
#define RTSEMXROADS_DIR_SHIFT       30
#define RTSEMXROADS_DIR_MASK        UINT32_C(0x40000000)


RTDECL(int) RTSemXRoadsCreate(PRTSEMXROADS phXRoads)
{
    RTSEMXROADSINTERNAL *pThis = (RTSEMXROADSINTERNAL *)RTMemAlloc(sizeof(*pThis));
    if (!pThis)
        return VERR_NO_MEMORY;

    int rc = RTSemEventMultiCreate(&pThis->hEvtNS);
    if (RT_SUCCESS(rc))
    {
        rc = RTSemEventMultiSignal(pThis->hEvtNS);
        if (RT_SUCCESS(rc))
        {
            rc = RTSemEventMultiCreate(&pThis->hEvtEW);
            if (RT_SUCCESS(rc))
            {
                pThis->u32Magic = RTSEMXROADS_MAGIC;
                pThis->u32State = 0;
                *phXRoads = pThis;
                return VINF_SUCCESS;
            }
        }
        RTSemEventMultiDestroy(pThis->hEvtNS);
    }
    return rc;
}


RTDECL(int) RTSemXRoadsDestroy(RTSEMXROADS hXRoads)
{
    /*
     * Validate input.
     */
    RTSEMXROADSINTERNAL *pThis = hXRoads;
    if (pThis == NIL_RTSEMXROADS)
        return VINF_SUCCESS;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSEMXROADS_MAGIC, VERR_INVALID_HANDLE);
    Assert(!(ASMAtomicReadU32(&pThis->u32State) & (RTSEMXROADS_CNT_NS_MASK | RTSEMXROADS_CNT_EW_MASK)));

    /*
     * Invalidate the object and free up the resources.
     */
    AssertReturn(ASMAtomicCmpXchgU32(&pThis->u32Magic, RTSEMXROADS_MAGIC_DEAD, RTSEMXROADS_MAGIC), VERR_INVALID_HANDLE);

    RTSEMEVENTMULTI hEvt;
    ASMAtomicXchgHandle(&pThis->hEvtNS, NIL_RTSEMEVENTMULTI, &hEvt);
    int rc = RTSemEventMultiDestroy(hEvt);
    AssertRC(rc);

    ASMAtomicXchgHandle(&pThis->hEvtEW, NIL_RTSEMEVENTMULTI, &hEvt);
    rc = RTSemEventMultiDestroy(hEvt);
    AssertRC(rc);

    return VINF_SUCCESS;
}


/**
 * Internal worker for RTSemXRoadsNSEnter and RTSemXRoadsEWEnter.
 *
 * @returns IPRT status code.
 * @param   pThis               The semaphore instace.
 * @param   hEvtBlock           Which semaphore to wait on.
 * @param   hEvtReset           Which semaphore to reset.
 * @param   uCountShift         The shift count for getting the count.
 * @param   fCountMask          The mask for getting the count.
 * @param   fDirection          The direction state value.
 */
DECL_FORCE_INLINE(int) rtSemXRoadsEnter(RTSEMXROADSINTERNAL *pThis,
                                        RTSEMEVENTMULTI hEvtBlock, RTSEMEVENTMULTI hEvtReset,
                                        uint32_t uCountShift, uint32_t fCountMask, uint32_t fDirection)
{
    for (;;)
    {
        uint32_t u32OldState;
        uint32_t u32State;

        u32State = ASMAtomicReadU32(&pThis->u32State);
        u32OldState = u32State;

        if ((u32State & RTSEMXROADS_DIR_MASK) == (fDirection << RTSEMXROADS_DIR_SHIFT))
        {
            /* It's flows in the right direction, try add ourselves to the flow before it changes. */
            uint32_t c = (u32State & fCountMask) >> uCountShift;
            c++;
            Assert(c < 8*_1K);
            u32State &= ~fCountMask;
            u32State |= c << uCountShift;
            if (ASMAtomicCmpXchgU32(&pThis->u32State, u32State, u32OldState))
                return VINF_SUCCESS;
        }
        else if ((u32State & (RTSEMXROADS_CNT_NS_MASK | RTSEMXROADS_CNT_EW_MASK)) == 0)
        {
            /* Wrong direction, but we're alone here and can simply try switch the direction. */
            RTSemEventMultiReset(hEvtReset);
            if (pThis->u32Magic != RTSEMXROADS_MAGIC)
                return VERR_SEM_DESTROYED;

            u32State = (UINT32_C(1) << uCountShift) | (fDirection << RTSEMXROADS_DIR_SHIFT);
            if (ASMAtomicCmpXchgU32(&pThis->u32State, u32State, u32OldState))
                return VINF_SUCCESS;
        }
        else
        {
            /* Add ourselves to the waiting threads and wait for the direction to change. */
            uint32_t c = (u32State & fCountMask) >> uCountShift;
            c++;
            Assert(c < 8*_1K);
            u32State &= ~fCountMask;
            u32State |= c << uCountShift;
            if (ASMAtomicCmpXchgU32(&pThis->u32State, u32State, u32OldState))
            {
                for (uint32_t iLoop = 0; ; iLoop++)
                {
                    int rc = RTSemEventMultiWait(hEvtBlock, RT_INDEFINITE_WAIT);
                    AssertRCReturn(rc, rc);

                    if (pThis->u32Magic != RTSEMXROADS_MAGIC)
                        return VERR_SEM_DESTROYED;

                    if ((ASMAtomicReadU32(&pThis->u32State) & RTSEMXROADS_DIR_MASK) == (fDirection << RTSEMXROADS_DIR_SHIFT))
                        return VINF_SUCCESS;

                    AssertMsgFailed(("%u\n", iLoop));
                }
            }
        }

        ASMNopPause();
        if (pThis->u32Magic != RTSEMXROADS_MAGIC)
            return VERR_SEM_DESTROYED;
    }
}


/**
 * Internal worker for RTSemXRoadsNSLeave and RTSemXRoadsEWLeave.
 *
 * @returns IPRT status code.
 * @param   pThis               The semaphore instace.
 * @param   hEvtReset           Which semaphore to reset.
 * @param   hEvtSignal          Which semaphore to signal.
 * @param   uCountShift         The shift count for getting the count.
 * @param   fCountMask          The mask for getting the count.
 * @param   fDirection          The direction state value.
 */
DECL_FORCE_INLINE(int) rtSemXRoadsLeave(RTSEMXROADSINTERNAL *pThis,
                                        RTSEMEVENTMULTI hEvtReset, RTSEMEVENTMULTI hEvtSignal,
                                        uint32_t uCountShift, uint32_t fCountMask, uint32_t fDirection)
{
    for (;;)
    {
        uint32_t u32OldState;
        uint32_t u32State;
        uint32_t c;

        u32State = ASMAtomicReadU32(&pThis->u32State);
        u32OldState = u32State;

        /* The direction cannot change until we've left or we'll crash. */
        Assert((u32State & RTSEMXROADS_DIR_MASK) == (fDirection << RTSEMXROADS_DIR_SHIFT));

        c = (u32State & fCountMask) >> uCountShift;
        Assert(c > 0);
        c--;

        if (    c > 0
            ||  (u32State & ((RTSEMXROADS_CNT_NS_MASK | RTSEMXROADS_CNT_EW_MASK) & ~fCountMask)) == 0)
        {
            /* We're not the last one across or there aren't any one waiting in the other direction.  */
            u32State &= ~fCountMask;
            u32State |= c << uCountShift;
            if (ASMAtomicCmpXchgU32(&pThis->u32State, u32State, u32OldState))
                return VINF_SUCCESS;
        }
        else
        {
            /* Reverse the direction. */
            RTSemEventMultiReset(hEvtReset);
            if (pThis->u32Magic != RTSEMXROADS_MAGIC)
                return VERR_SEM_DESTROYED;

            u32State &= ~(fCountMask | RTSEMXROADS_DIR_MASK);
            u32State |= !fDirection << RTSEMXROADS_DIR_SHIFT;
            if (ASMAtomicCmpXchgU32(&pThis->u32State, u32State, u32OldState))
            {
                int rc = RTSemEventMultiSignal(hEvtSignal);
                AssertRC(rc);
                return VINF_SUCCESS;
            }
        }

        ASMNopPause();
        if (pThis->u32Magic != RTSEMXROADS_MAGIC)
            return VERR_SEM_DESTROYED;
    }
}


RTDECL(int) RTSemXRoadsNSEnter(RTSEMXROADS hXRoads)
{
    /*
     * Validate input.
     */
    RTSEMXROADSINTERNAL *pThis = hXRoads;
    if (pThis == NIL_RTSEMXROADS)
        return VINF_SUCCESS;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSEMXROADS_MAGIC, VERR_INVALID_HANDLE);

    return rtSemXRoadsEnter(pThis, pThis->hEvtNS, pThis->hEvtEW, RTSEMXROADS_CNT_NS_SHIFT, RTSEMXROADS_CNT_NS_MASK, 0);
}


RTDECL(int) RTSemXRoadsNSLeave(RTSEMXROADS hXRoads)
{
    /*
     * Validate input.
     */
    RTSEMXROADSINTERNAL *pThis = hXRoads;
    if (pThis == NIL_RTSEMXROADS)
        return VINF_SUCCESS;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSEMXROADS_MAGIC, VERR_INVALID_HANDLE);

    return rtSemXRoadsLeave(pThis, pThis->hEvtNS, pThis->hEvtEW, RTSEMXROADS_CNT_NS_SHIFT, RTSEMXROADS_CNT_NS_MASK, 0);
}


RTDECL(int) RTSemXRoadsEWEnter(RTSEMXROADS hXRoads)
{
    /*
     * Validate input.
     */
    RTSEMXROADSINTERNAL *pThis = hXRoads;
    if (pThis == NIL_RTSEMXROADS)
        return VINF_SUCCESS;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSEMXROADS_MAGIC, VERR_INVALID_HANDLE);

    return rtSemXRoadsEnter(pThis, pThis->hEvtEW, pThis->hEvtNS, RTSEMXROADS_CNT_EW_SHIFT, RTSEMXROADS_CNT_EW_MASK, 1);
}


RTDECL(int) RTSemXRoadsEWLeave(RTSEMXROADS hXRoads)
{
    /*
     * Validate input.
     */
    RTSEMXROADSINTERNAL *pThis = hXRoads;
    if (pThis == NIL_RTSEMXROADS)
        return VINF_SUCCESS;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSEMXROADS_MAGIC, VERR_INVALID_HANDLE);

    return rtSemXRoadsLeave(pThis, pThis->hEvtEW, pThis->hEvtNS, RTSEMXROADS_CNT_EW_SHIFT, RTSEMXROADS_CNT_EW_MASK, 1);
}

