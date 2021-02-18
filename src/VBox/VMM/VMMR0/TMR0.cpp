/* $Id$ */
/** @file
 * TM - Timeout Manager, host ring-0 context.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_TM
#include <VBox/vmm/tm.h>
#include "TMInternal.h"
#include <VBox/vmm/gvm.h>

#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/param.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/memobj.h>
#include <iprt/process.h>
#include <iprt/string.h>



/**
 * Initializes the per-VM data for the TM.
 *
 * This is called from under the GVMM lock, so it should only initialize the
 * data so TMR0CleanupVM and others will work smoothly.
 *
 * @param   pGVM    Pointer to the global VM structure.
 */
VMMR0_INT_DECL(void)    TMR0InitPerVMData(PGVM pGVM)
{
    for (uint32_t idxQueue = 0; idxQueue < RT_ELEMENTS(pGVM->tmr0.s.aTimerQueues); idxQueue++)
    {
        pGVM->tmr0.s.aTimerQueues[idxQueue].hMemObj = NIL_RTR0MEMOBJ;
        pGVM->tmr0.s.aTimerQueues[idxQueue].hMapObj = NIL_RTR0MEMOBJ;
    }
}


/**
 * Cleans up any loose ends before the GVM structure is destroyed.
 */
VMMR0_INT_DECL(void)    TMR0CleanupVM(PGVM pGVM)
{
    for (uint32_t idxQueue = 0; idxQueue < RT_ELEMENTS(pGVM->tmr0.s.aTimerQueues); idxQueue++)
    {
        if (pGVM->tmr0.s.aTimerQueues[idxQueue].hMapObj == NIL_RTR0MEMOBJ)
        {
            RTR0MemObjFree(pGVM->tmr0.s.aTimerQueues[idxQueue].hMapObj, true /*fFreeMappings*/);
            pGVM->tmr0.s.aTimerQueues[idxQueue].hMapObj = NIL_RTR0MEMOBJ;
        }

        if (pGVM->tmr0.s.aTimerQueues[idxQueue].hMemObj != NIL_RTR0MEMOBJ)
        {
            RTR0MemObjFree(pGVM->tmr0.s.aTimerQueues[idxQueue].hMemObj, true /*fFreeMappings*/);
            pGVM->tmr0.s.aTimerQueues[idxQueue].hMemObj = NIL_RTR0MEMOBJ;
        }
    }
}


VMMR0_INT_DECL(int) TMR0TimerQueueGrow(PGVM pGVM, uint32_t idxQueue, uint32_t cMinTimers)
{
    /*
     * Validate input and state.
     */
    VM_ASSERT_EMT0_RETURN(pGVM, VERR_VM_THREAD_NOT_EMT);
    VM_ASSERT_STATE_RETURN(pGVM, VMSTATE_CREATING, VERR_VM_INVALID_VM_STATE); /** @todo must do better than this! */
    AssertReturn(idxQueue <= RT_ELEMENTS(pGVM->tmr0.s.aTimerQueues), VERR_TM_INVALID_TIMER_QUEUE);
    AssertCompile(RT_ELEMENTS(pGVM->tmr0.s.aTimerQueues) == RT_ELEMENTS(pGVM->tm.s.aTimerQueues));
    PTMTIMERQUEUER0 pQueueR0     = &pGVM->tmr0.s.aTimerQueues[idxQueue];
    PTMTIMERQUEUE   pQueueShared = &pGVM->tm.s.aTimerQueues[idxQueue];

    uint32_t cNewTimers = cMinTimers;
    AssertReturn(cNewTimers <= _32K, VERR_TM_TOO_MANY_TIMERS);
    uint32_t const cOldEntries = pQueueR0->cTimersAlloc;
    ASMCompilerBarrier();
    AssertReturn(cNewTimers >= cOldEntries, VERR_TM_IPE_1);
    AssertReturn(cOldEntries == pQueueShared->cTimersAlloc, VERR_TM_IPE_2);

    /*
     * Round up the request to the nearest page and do the allocation.
     */
    size_t cbNew = sizeof(TMTIMER) * cNewTimers;
    cbNew = RT_ALIGN_Z(cbNew, PAGE_SIZE);
    cNewTimers = (uint32_t)(cbNew / sizeof(TMTIMER));

    RTR0MEMOBJ hMemObj;
    int rc = RTR0MemObjAllocPage(&hMemObj, cbNew, false /*fExecutable*/);
    if (RT_SUCCESS(rc))
    {
        /*
         * Zero and map it.
         */
        PTMTIMER paTimers = (PTMTIMER)RTR0MemObjAddress(hMemObj);
        RT_BZERO(paTimers, cbNew);

        RTR0MEMOBJ hMapObj;
        rc = RTR0MemObjMapUser(&hMapObj, hMemObj, (RTR3PTR)-1, PAGE_SIZE, RTMEM_PROT_READ | RTMEM_PROT_WRITE, RTR0ProcHandleSelf());
        if (RT_SUCCESS(rc))
        {
            /*
             * Copy over the old info and initialize the new handles.
             */
            if (cOldEntries > 0)
                memcpy(paTimers, pQueueR0->paTimers, sizeof(TMTIMER) * cOldEntries);

            size_t i = cNewTimers;
            while (i-- > cOldEntries)
            {
                paTimers[i].u64Expire       = UINT64_MAX;
                paTimers[i].enmClock        = TMCLOCK_MAX;
                paTimers[i].enmType         = TMTIMERTYPE_INVALID;
                paTimers[i].enmState        = TMTIMERSTATE_FREE;
                paTimers[i].idxScheduleNext = UINT32_MAX;
                paTimers[i].idxNext         = UINT32_MAX;
                paTimers[i].idxPrev         = UINT32_MAX;
                paTimers[i].hSelf           = NIL_TMTIMERHANDLE;
            }

            /*
             * Mark the zero'th entry as allocated but invalid if we just allocated it.
             */
            if (cOldEntries == 0)
            {
                paTimers[0].enmState = TMTIMERSTATE_INVALID;
                paTimers[0].szName[0] = 'n';
                paTimers[0].szName[1] = 'i';
                paTimers[0].szName[2] = 'l';
                paTimers[0].szName[3] = '\0';
            }

            /*
             * Switch the memory handles.
             */
            RTR0MEMOBJ hTmp = pQueueR0->hMapObj;
            pQueueR0->hMapObj = hMapObj;
            hMapObj = hTmp;

            hTmp = pQueueR0->hMemObj;
            pQueueR0->hMemObj = hMemObj;
            hMemObj = hTmp;

            /*
             * Update the variables.
             */
            pQueueR0->paTimers         = paTimers;
            pQueueR0->cTimersAlloc     = cNewTimers;
            pQueueShared->paTimers     = RTR0MemObjAddressR3(pQueueR0->hMapObj);
            pQueueShared->cTimersAlloc = cNewTimers;
            pQueueShared->cTimersFree += cNewTimers - (cOldEntries ? cOldEntries : 1);

            /*
             * Free the old allocation.
             */
            RTR0MemObjFree(hMapObj, true /*fFreeMappings*/);
        }
        RTR0MemObjFree(hMemObj, true /*fFreeMappings*/);
    }

    return rc;
}


