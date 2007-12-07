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
#include <iprt/thread.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int pdmR3CritSectDeleteOne(PVM pVM, PPDMCRITSECTINT pCritSect, PPDMCRITSECTINT pPrev, bool fFinal);



/**
 * Initializes the critical section subcomponent.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @remark  Not to be confused with PDMR3CritSectInit and pdmR3CritSectInitDevice which are
 *          for initializing a critical section.
 */
int pdmR3CritSectInit(PVM pVM)
{
    STAM_REG(pVM, &pVM->pdm.s.StatQueuedCritSectLeaves, STAMTYPE_COUNTER, "/PDM/QueuedCritSectLeaves", STAMUNIT_OCCURENCES,
             "Number of times a critical section leave requesed needed to be queued for ring-3 execution.");
    return VINF_SUCCESS;
}


/**
 * Relocates all the critical sections.
 *
 * @param   pVM         The VM handle.
 */
void pdmR3CritSectRelocate(PVM pVM)
{
    for (PPDMCRITSECTINT pCur = pVM->pdm.s.pCritSects;
         pCur;
         pCur = pCur->pNext)
        pCur->pVMGC = pVM->pVMGC;
}


/**
 * Deletes all remaining critical sections.
 *
 * This is called at the end of the termination process.
 *
 * @returns VBox status.
 *          First error code, rest is lost.
 * @param   pVM         The VM handle.
 * @remark  Don't confuse this with PDMR3CritSectDelete.
 */
PDMDECL(int) PDMR3CritSectTerm(PVM pVM)
{
    int rc = VINF_SUCCESS;
    while (pVM->pdm.s.pCritSects)
    {
        int rc2 = pdmR3CritSectDeleteOne(pVM, pVM->pdm.s.pCritSects, NULL, true /* final */);
        AssertRC(rc2);
        if (VBOX_FAILURE(rc2) && VBOX_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}



/**
 * Initalizes a critical section and inserts it into the list.
 *
 * @returns VBox status code.
 * @param   pVM             The Vm handle.
 * @param   pCritSect       The critical section.
 * @param   pvKey           The owner key.
 * @param   pszName         The name of the critical section (for statistics).
 */
static int pdmR3CritSectInitOne(PVM pVM, PPDMCRITSECTINT pCritSect, void *pvKey, const char *pszName)
{
    VM_ASSERT_EMT(pVM);
    int rc = RTCritSectInit(&pCritSect->Core);
    if (VBOX_SUCCESS(rc))
    {
        pCritSect->pVMR3 = pVM;
        pCritSect->pVMR0 = (RTR0PTR)pVM;//pVM->pVMR0;
        pCritSect->pVMGC = pVM->pVMGC;
        pCritSect->pvKey = pvKey;
        pCritSect->EventToSignal = NIL_RTSEMEVENT;
        pCritSect->pNext = pVM->pdm.s.pCritSects;
        pVM->pdm.s.pCritSects = pCritSect;
#ifdef VBOX_WITH_STATISTICS
        STAMR3RegisterF(pVM, &pCritSect->StatContentionR0GCLock,   STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,          NULL, "/PDM/CritSects/%s/ContentionR0GCLock", pszName);
        STAMR3RegisterF(pVM, &pCritSect->StatContentionR0GCUnlock, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,          NULL, "/PDM/CritSects/%s/ContentionR0GCUnlock", pszName);
        STAMR3RegisterF(pVM, &pCritSect->StatContentionR3,         STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,          NULL, "/PDM/CritSects/%s/ContentionR3", pszName);
        STAMR3RegisterF(pVM, &pCritSect->StatLocked,           STAMTYPE_PROFILE_ADV, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_OCCURENCE, NULL, "/PDM/CritSects/%s/Locked", pszName);
#endif
    }
    return rc;
}


/**
 * Initializes a PDM critical section for internal use.
 *
 * The PDM critical sections are derived from the IPRT critical sections, but
 * works in GC as well.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pDevIns         Device instance.
 * @param   pCritSect       Pointer to the critical section.
 * @param   pszName         The name of the critical section (for statistics).
 */
PDMR3DECL(int) PDMR3CritSectInit(PVM pVM, PPDMCRITSECT pCritSect, const char *pszName)
{
    AssertCompile(sizeof(pCritSect->padding) >= sizeof(pCritSect->s));
    return pdmR3CritSectInitOne(pVM, &pCritSect->s, pCritSect, pszName);
}


/**
 * Initializes a PDM critical section.
 *
 * The PDM critical sections are derived from the IPRT critical sections, but
 * works in GC as well.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pDevIns         Device instance.
 * @param   pCritSect       Pointer to the critical section.
 * @param   pszName         The name of the critical section (for statistics).
 */
int pdmR3CritSectInitDevice(PVM pVM, PPDMDEVINS pDevIns, PPDMCRITSECT pCritSect, const char *pszName)
{
    return pdmR3CritSectInitOne(pVM, &pCritSect->s, pDevIns, pszName);
}


/**
 * Deletes one critical section.
 *
 * @returns Return code from RTCritSectDelete.
 * @param   pVM         The VM handle.
 * @param   pCritSect   The critical section.
 * @param   pPrev       The previous critical section in the list.
 * @param   fFinal      Set if this is the final call and statistics shouldn't be deregistered.
 */
static int pdmR3CritSectDeleteOne(PVM pVM, PPDMCRITSECTINT pCritSect, PPDMCRITSECTINT pPrev, bool fFinal)
{
    /* ulink */
    if (pPrev)
        pPrev->pNext = pCritSect->pNext;
    else
        pVM->pdm.s.pCritSects = pCritSect->pNext;

    /* delete */
    pCritSect->pNext = NULL;
    pCritSect->pVMGC = 0;
    pCritSect->pVMR3 = NULL;
    pCritSect->pVMR0 = NIL_RTR0PTR;
    pCritSect->pvKey = NULL;
#ifdef VBOX_WITH_STATISTICS
    if (!fFinal)
    {
        STAMR3Deregister(pVM, &pCritSect->StatContentionR0GCLock);
        STAMR3Deregister(pVM, &pCritSect->StatContentionR0GCUnlock);
        STAMR3Deregister(pVM, &pCritSect->StatContentionR3);
        STAMR3Deregister(pVM, &pCritSect->StatLocked);
    }
#endif
    return RTCritSectDelete(&pCritSect->Core);
}


/**
 * Deletes all critical sections with a give initializer key.
 *
 * @returns VBox status code.
 *          The entire list is processed on failure, so we'll only
 *          return the first error code. This shouldn't be a problem
 *          since errors really shouldn't happen here.
 * @param   pVM     The VM handle.
 * @param   pvKey   The initializer key.
 */
static int pdmR3CritSectDeleteByKey(PVM pVM, void *pvKey)
{
    /*
     * Iterate the list and match key.
     */
    int             rc = VINF_SUCCESS;
    PPDMCRITSECTINT pPrev = NULL;
    PPDMCRITSECTINT pCur = pVM->pdm.s.pCritSects;
    while (pCur)
    {
        if (pCur->pvKey == pvKey)
        {
            int rc2 = pdmR3CritSectDeleteOne(pVM, pCur, pPrev, false /* not final */);
            AssertRC(rc2);
            if (VBOX_FAILURE(rc2) && VBOX_SUCCESS(rc))
                rc = rc2;
        }

        /* next */
        pPrev = pCur;
        pCur = pCur->pNext;
    }
    return rc;
}


/**
 * Deletes all undeleted critical sections initalized by a given device.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pDevIns     The device handle.
 */
int pdmR3CritSectDeleteDevice(PVM pVM, PPDMDEVINS pDevIns)
{
    return pdmR3CritSectDeleteByKey(pVM, pDevIns);
}


/**
 * Deletes the critical section.
 *
 * @returns VBox status code.
 * @param   pCritSect           The PDM critical section to destroy.
 */
PDMR3DECL(int) PDMR3CritSectDelete(PPDMCRITSECT pCritSect)
{
    if (!RTCritSectIsInitialized(&pCritSect->s.Core))
        return VINF_SUCCESS;

    /*
     * Find and unlink it.
     */
    PVM             pVM = pCritSect->s.pVMR3;
    AssertReleaseReturn(pVM, VERR_INTERNAL_ERROR);
    PPDMCRITSECTINT pPrev = NULL;
    PPDMCRITSECTINT pCur = pVM->pdm.s.pCritSects;
    while (pCur)
    {
        if (pCur == &pCritSect->s)
            return pdmR3CritSectDeleteOne(pVM, pCur, pPrev, false /* not final */);

        /* next */
        pPrev = pCur;
        pCur = pCur->pNext;
    }
    AssertReleaseMsgFailed(("pCritSect=%p wasn't found!\n", pCritSect));
    return VERR_INTERNAL_ERROR;
}


/**
 * Process the critical sections queued for ring-3 'leave'.
 *
 * @param   pVM         The VM handle.
 */
PDMR3DECL(void) PDMR3CritSectFF(PVM pVM)
{
    Assert(pVM->pdm.s.cQueuedCritSectLeaves > 0);

    const RTUINT c = pVM->pdm.s.cQueuedCritSectLeaves;
    for (RTUINT i = 0; i < c; i++)
    {
        PPDMCRITSECT pCritSect = pVM->pdm.s.apQueuedCritSectsLeaves[i];
        int rc = RTCritSectLeave(&pCritSect->s.Core);
        LogFlow(("PDMR3CritSectFF: %p - %Vrc\n", pCritSect, rc));
        AssertRC(rc);
    }

    pVM->pdm.s.cQueuedCritSectLeaves = 0;
    VM_FF_CLEAR(pVM, VM_FF_PDM_CRITSECT);
}


/**
 * Try enter a critical section.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_SEM_BUSY if the critsect was owned.
 * @returns VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @returns VERR_SEM_DESTROYED if RTCritSectDelete was called while waiting.
 * @param   pCritSect   The critical section.
 */
PDMR3DECL(int) PDMR3CritSectTryEnter(PPDMCRITSECT pCritSect)
{
    return RTCritSectTryEnter(&pCritSect->s.Core);
}


/**
 * Schedule a event semaphore for signalling upon critsect exit.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_TOO_MANY_SEMAPHORES if an event was already scheduled.
 * @returns VERR_NOT_OWNER if we're not the critsect owner.
 * @returns VERR_SEM_DESTROYED if RTCritSectDelete was called while waiting.
 * @param   pCritSect       The critical section.
 * @param   EventToSignal     The semapore that should be signalled.
 */
PDMR3DECL(int) PDMR3CritSectScheduleExitEvent(PPDMCRITSECT pCritSect, RTSEMEVENT EventToSignal)
{
    Assert(EventToSignal != NIL_RTSEMEVENT);
    if (RT_UNLIKELY(!RTCritSectIsOwner(&pCritSect->s.Core)))
        return VERR_NOT_OWNER;
    if (RT_LIKELY(   pCritSect->s.EventToSignal == NIL_RTSEMEVENT
                  || pCritSect->s.EventToSignal == EventToSignal))
    {
        pCritSect->s.EventToSignal = EventToSignal;
        return VINF_SUCCESS;
    }
    return VERR_TOO_MANY_SEMAPHORES;
}

