/* $Id$ */
/** @file
 * PDM Critical Sections
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PDM//_CRITSECT
#include "PDMInternal.h"
#include <VBox/pdm.h>
#include <VBox/mm.h>
#include <VBox/vm.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#ifdef IN_RING3
# include <iprt/semaphore.h>
#endif 


/**
 * Leaves a critical section entered with PDMCritSectEnter().
 *
 * @returns VINF_SUCCESS if entered successfully.
 * @returns rcBusy when encountering a busy critical section in GC/R0.
 * @returns VERR_SEM_DESTROYED if the critical section is dead.
 *
 * @param   pCritSect           The PDM critical section to enter.
 * @param   rcBusy              The status code to return when we're in GC or R0
 *                              and the section is busy.
 */
PDMDECL(int) PDMCritSectEnter(PPDMCRITSECT pCritSect, int rcBusy)
{
    Assert(pCritSect->s.Core.cNestings < 8);  /* useful to catch incorrect locking */
#ifdef IN_RING3
    NOREF(rcBusy);

    STAM_STATS({ if (pCritSect->s.Core.cLockers >= 0 && !RTCritSectIsOwner(&pCritSect->s.Core)) STAM_COUNTER_INC(&pCritSect->s.StatContentionR3); });
    int rc = RTCritSectEnter(&pCritSect->s.Core);
    STAM_STATS({ if (pCritSect->s.Core.cNestings == 1) STAM_PROFILE_ADV_START(&pCritSect->s.StatLocked, l); });
    return rc;

#else
    AssertMsgReturn(pCritSect->s.Core.u32Magic == RTCRITSECT_MAGIC, ("%RX32\n", pCritSect->s.Core.u32Magic),
                    VERR_SEM_DESTROYED);
    PVM pVM = pCritSect->s.CTXALLSUFF(pVM);
    Assert(pVM);

    /*
     * Try take the lock.
     */
    if (ASMAtomicCmpXchgS32(&pCritSect->s.Core.cLockers, 0, -1))
    {
        pCritSect->s.Core.cNestings = 1;
        ASMAtomicXchgSize(&pCritSect->s.Core.NativeThreadOwner, pVM->NativeThreadEMT);
        STAM_PROFILE_ADV_START(&pCritSect->s.StatLocked, l);
        return VINF_SUCCESS;
    }

    /*
     * Nested?
     */
    if (pCritSect->s.Core.NativeThreadOwner == pVM->NativeThreadEMT)
    {
        pCritSect->s.Core.cNestings++;
        ASMAtomicIncS32(&pCritSect->s.Core.cLockers);
        return VINF_SUCCESS;
    }

    /*
     * Failed.
     */
    LogFlow(("pcnetLock: locked => HC (%Vrc)\n", rcBusy));
    STAM_COUNTER_INC(&pCritSect->s.StatContentionR0GCLock);
    return rcBusy;
#endif
}


/**
 * Leaves a critical section entered with PDMCritSectEnter().
 *
 * @param   pCritSect           The PDM critical section to leave.
 */
PDMDECL(void) PDMCritSectLeave(PPDMCRITSECT pCritSect)
{
#ifdef IN_RING3
# ifdef VBOX_WITH_STATISTICS
    if (pCritSect->s.Core.cNestings == 1)
        STAM_PROFILE_ADV_STOP(&pCritSect->s.StatLocked, l);
# endif
    RTSEMEVENT EventToSignal = pCritSect->s.EventToSignal;
    if (RT_LIKELY(EventToSignal == NIL_RTSEMEVENT))
    {
        int rc = RTCritSectLeave(&pCritSect->s.Core);
        AssertRC(rc);
    }
    else
    {
        pCritSect->s.EventToSignal = NIL_RTSEMEVENT;
        int rc = RTCritSectLeave(&pCritSect->s.Core);
        AssertRC(rc);
        LogBird(("signalling %#x\n", EventToSignal));
        rc = RTSemEventSignal(EventToSignal);
        AssertRC(rc);
    }

#else /* !IN_RING3 */
    Assert(VALID_PTR(pCritSect));
    Assert(pCritSect->s.Core.u32Magic == RTCRITSECT_MAGIC);
    Assert(pCritSect->s.Core.cNestings > 0);
    Assert(pCritSect->s.Core.cLockers >= 0);
    PVM pVM = pCritSect->s.CTXALLSUFF(pVM);
    Assert(pVM);
    Assert(pCritSect->s.Core.NativeThreadOwner == pVM->NativeThreadEMT);

    /*
     * Deal with nested attempts first.
     * (We're exploiting nesting to avoid queuing multiple R3 leaves for the same section.)
     */
    pCritSect->s.Core.cNestings--;
    if (pCritSect->s.Core.cNestings > 0)
    {
        ASMAtomicDecS32(&pCritSect->s.Core.cLockers);
        return;
    }

    /*
     * Try leave it.
     */
    if (pCritSect->s.Core.cLockers == 0)
    {
        STAM_PROFILE_ADV_STOP(&pCritSect->s.StatLocked, l);
        ASMAtomicXchgSize(&pCritSect->s.Core.NativeThreadOwner, NIL_RTNATIVETHREAD);
        if (ASMAtomicCmpXchgS32(&pCritSect->s.Core.cLockers, -1, 0))
            return;

        /* darn, someone raced in on us. */
        ASMAtomicXchgSize(&pCritSect->s.Core.NativeThreadOwner, pVM->NativeThreadEMT);
        STAM_PROFILE_ADV_START(&pCritSect->s.StatLocked, l);
    }
    pCritSect->s.Core.cNestings = 1;

    /*
     * Queue the request.
     */
    RTUINT i = pVM->pdm.s.cQueuedCritSectLeaves++;
    LogFlow(("PDMCritSectLeave: [%d]=%p => R3\n", i, pCritSect));
    AssertFatal(i < RT_ELEMENTS(pVM->pdm.s.apQueuedCritSectsLeaves));
    pVM->pdm.s.apQueuedCritSectsLeaves[i] = MMHyperCCToR3(pVM, pCritSect);
    VM_FF_SET(pVM, VM_FF_PDM_CRITSECT);
    VM_FF_SET(pVM, VM_FF_TO_R3);
    STAM_COUNTER_INC(&pVM->pdm.s.StatQueuedCritSectLeaves);
    STAM_COUNTER_INC(&pCritSect->s.StatContentionR0GCUnlock);
#endif
}


/**
 * Checks the caller is the owner of the critical section.
 *
 * @returns true if owner.
 * @returns false if not owner.
 * @param   pCritSect   The critical section.
 */
PDMDECL(bool) PDMCritSectIsOwner(PCPDMCRITSECT pCritSect)
{
#ifdef IN_RING3
    return RTCritSectIsOwner(&pCritSect->s.Core);
#else
    PVM pVM = pCritSect->s.CTXALLSUFF(pVM);
    Assert(pVM);
    return pCritSect->s.Core.NativeThreadOwner == pVM->NativeThreadEMT;
#endif
}


/**
 * Checks if a critical section is initialized or not.
 *
 * @returns true if initialized.
 * @returns false if not initialized.
 * @param   pCritSect   The critical section.
 */
PDMDECL(bool) PDMCritSectIsInitialized(PCPDMCRITSECT pCritSect)
{
    return pCritSect->s.Core.u32Magic == RTCRITSECT_MAGIC;
}

