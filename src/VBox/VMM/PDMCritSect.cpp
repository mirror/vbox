/* $Id$ */
/** @file
 * PDM - Critical Sections, Ring-3.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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
 */

//#define PDM_WITH_R3R0_CRIT_SECT

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
#ifdef PDM_WITH_R3R0_CRIT_SECT
# include <VBox/sup.h>
#endif
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/string.h>
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
        pCur->pVMRC = pVM->pVMRC;
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
VMMDECL(int) PDMR3CritSectTerm(PVM pVM)
{
    int rc = VINF_SUCCESS;
    while (pVM->pdm.s.pCritSects)
    {
        int rc2 = pdmR3CritSectDeleteOne(pVM, pVM->pdm.s.pCritSects, NULL, true /* final */);
        AssertRC(rc2);
        if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
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

#ifdef PDM_WITH_R3R0_CRIT_SECT
    /*
     * Allocate the semaphore.
     */
    AssertCompile(sizeof(SUPSEMEVENT) == sizeof(pCritSect->Core.EventSem));
    int rc = SUPSemEventCreate(pVM->pSession, (PSUPSEMEVENT)&pCritSect->Core.EventSem);
#else
    int rc = RTCritSectInit(&pCritSect->Core);
#endif
    if (RT_SUCCESS(rc))
    {
#ifdef PDM_WITH_R3R0_CRIT_SECT
        /*
         * Initialize the structure (first bit is c&p from RTCritSectInitEx).
         */
        pCritSect->Core.u32Magic             = RTCRITSECT_MAGIC;
        pCritSect->Core.fFlags               = 0;
        pCritSect->Core.cNestings            = 0;
        pCritSect->Core.cLockers             = -1;
        pCritSect->Core.NativeThreadOwner    = NIL_RTNATIVETHREAD;
        pCritSect->Core.Strict.ThreadOwner   = NIL_RTTHREAD;
        pCritSect->Core.Strict.pszEnterFile  = NULL;
        pCritSect->Core.Strict.u32EnterLine  = 0;
        pCritSect->Core.Strict.uEnterId      = 0;
#endif
        pCritSect->pVMR3                     = pVM;
        pCritSect->pVMR0                     = pVM->pVMR0;
        pCritSect->pVMRC                     = pVM->pVMRC;
        pCritSect->pvKey                     = pvKey;
        pCritSect->EventToSignal             = NIL_RTSEMEVENT;
        pCritSect->pNext                     = pVM->pdm.s.pCritSects;
        pCritSect->pszName                   = RTStrDup(pszName);
        pVM->pdm.s.pCritSects = pCritSect;
        STAMR3RegisterF(pVM, &pCritSect->StatContentionRZLock,  STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,          NULL, "/PDM/CritSects/%s/ContentionRZLock", pszName);
        STAMR3RegisterF(pVM, &pCritSect->StatContentionRZUnlock,STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,          NULL, "/PDM/CritSects/%s/ContentionRZUnlock", pszName);
        STAMR3RegisterF(pVM, &pCritSect->StatContentionR3,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,          NULL, "/PDM/CritSects/%s/ContentionR3", pszName);
#ifdef VBOX_WITH_STATISTICS
        STAMR3RegisterF(pVM, &pCritSect->StatLocked,        STAMTYPE_PROFILE_ADV, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_OCCURENCE, NULL, "/PDM/CritSects/%s/Locked", pszName);
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
VMMR3DECL(int) PDMR3CritSectInit(PVM pVM, PPDMCRITSECT pCritSect, const char *pszName)
{
#if HC_ARCH_BITS == 64 && GC_ARCH_BITS == 32
    AssertCompile(sizeof(pCritSect->padding) >= sizeof(pCritSect->s));
#endif
    Assert(RT_ALIGN_P(pCritSect, sizeof(uintptr_t)) == pCritSect);
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
#ifdef PDM_WITH_R3R0_CRIT_SECT
    /*
     * Assert free waiters and so on (c&p from RTCritSectDelete).
     */
    Assert(pCritSect->Core.u32Magic == RTCRITSECT_MAGIC);
    Assert(pCritSect->Core.cNestings == 0);
    Assert(pCritSect->Core.cLockers == -1);
    Assert(pCritSect->Core.NativeThreadOwner == NIL_RTNATIVETHREAD);
#endif

    /*
     * Unlink it.
     */
    if (pPrev)
        pPrev->pNext = pCritSect->pNext;
    else
        pVM->pdm.s.pCritSects = pCritSect->pNext;

    /*
     * Delete it (parts taken from RTCritSectDelete).
     * In case someone is waiting we'll signal the semaphore cLockers + 1 times.
     */
#ifdef PDM_WITH_R3R0_CRIT_SECT
    ASMAtomicWriteU32(&pCritSect->Core.u32Magic, 0);
    SUPSEMEVENT hEvent = (SUPSEMEVENT)pCritSect->Core.EventSem;
    pCritSect->Core.EventSem = NIL_RTSEMEVENT;
    while (pCritSect->Core.cLockers-- >= 0)
        SUPSemEventSignal(pVM->pSession, hEvent);
    ASMAtomicWriteS32(&pCritSect->Core.cLockers, -1);
    int rc = SUPSemEventClose(pVM->pSession, hEvent);
    AssertRC(rc);
#endif
    pCritSect->pNext   = NULL;
    pCritSect->pvKey   = NULL;
    pCritSect->pVMR3   = NULL;
    pCritSect->pVMR0   = NIL_RTR0PTR;
    pCritSect->pVMRC   = NIL_RTRCPTR;
    RTStrFree((char *)pCritSect->pszName);
    pCritSect->pszName = NULL;
    if (!fFinal)
    {
        STAMR3Deregister(pVM, &pCritSect->StatContentionRZLock);
        STAMR3Deregister(pVM, &pCritSect->StatContentionRZUnlock);
        STAMR3Deregister(pVM, &pCritSect->StatContentionR3);
#ifdef VBOX_WITH_STATISTICS
        STAMR3Deregister(pVM, &pCritSect->StatLocked);
#endif
    }
#ifndef PDM_WITH_R3R0_CRIT_SECT
    int rc = RTCritSectDelete(&pCritSect->Core);
#endif
    return rc;
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
            if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
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
VMMR3DECL(int) PDMR3CritSectDelete(PPDMCRITSECT pCritSect)
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
 * @param   pVCpu         The VMCPU handle.
 */
VMMR3DECL(void) PDMR3CritSectFF(PVMCPU pVCpu)
{
    Assert(pVCpu->pdm.s.cQueuedCritSectLeaves > 0);

    const RTUINT c = pVCpu->pdm.s.cQueuedCritSectLeaves;
    for (RTUINT i = 0; i < c; i++)
    {
        PPDMCRITSECT pCritSect = pVCpu->pdm.s.apQueuedCritSectsLeaves[i];
#ifdef PDM_WITH_R3R0_CRIT_SECT
        int rc = pdmCritSectLeave(pCritSect);
#else
        int rc = RTCritSectLeave(&pCritSect->s.Core);
#endif
        LogFlow(("PDMR3CritSectFF: %p - %Rrc\n", pCritSect, rc));
        AssertRC(rc);
    }

    pVCpu->pdm.s.cQueuedCritSectLeaves = 0;
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_PDM_CRITSECT);
}


/**
 * Schedule a event semaphore for signalling upon critsect exit.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_TOO_MANY_SEMAPHORES if an event was already scheduled.
 * @returns VERR_NOT_OWNER if we're not the critsect owner.
 * @returns VERR_SEM_DESTROYED if RTCritSectDelete was called while waiting.
 * @param   pCritSect       The critical section.
 * @param   EventToSignal   The semapore that should be signalled.
 */
VMMR3DECL(int) PDMR3CritSectScheduleExitEvent(PPDMCRITSECT pCritSect, RTSEMEVENT EventToSignal)
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


/**
 * Counts the critical sections owned by the calling thread, optionally
 * returning a comma separated list naming them.
 *
 * This is for diagnostic purposes only.
 *
 * @returns Lock count.
 *
 * @param   pVM             The VM handle.
 * @param   pszNames        Where to return the critical section names.
 * @param   cbNames         The size of the buffer.
 */
VMMR3DECL(uint32_t) PDMR3CritSectCountOwned(PVM pVM, char *pszNames, size_t cbNames)
{
    /*
     * Init the name buffer.
     */
    size_t cchLeft = cbNames;
    if (cchLeft)
    {
        cchLeft--;
        pszNames[0] = pszNames[cchLeft] = '\0';
    }

    /*
     * Iterate the critical sections.
     */
    /* This is unsafe, but wtf. */
    RTNATIVETHREAD const    hNativeThread = RTThreadNativeSelf();
    uint32_t                cCritSects = 0;
    for (PPDMCRITSECTINT pCur = pVM->pdm.s.pCritSects;
         pCur;
         pCur = pCur->pNext)
    {
        /* Same as RTCritSectIsOwner(). */
        if (pCur->Core.NativeThreadOwner == hNativeThread)
        {
            cCritSects++;

            /*
             * Copy the name if there is space. Fun stuff.
             */
            if (cchLeft)
            {
                /* try add comma. */
                if (cCritSects != 1)
                {
                    *pszNames++ = ',';
                    if (--cchLeft)
                    {
                        *pszNames++ = ' ';
                        cchLeft--;
                    }
                }

                /* try copy the name. */
                if (cchLeft)
                {
                    size_t const cchName = strlen(pCur->pszName);
                    if (cchName < cchLeft)
                    {
                        memcpy(pszNames, pCur->pszName, cchName);
                        pszNames += cchName;
                        cchLeft -= cchName;
                    }
                    else
                    {
                        if (cchLeft > 2)
                        {
                            memcpy(pszNames, pCur->pszName, cchLeft - 2);
                            pszNames += cchLeft - 2;
                            cchLeft = 2;
                        }
                        while (cchLeft-- > 0)
                            *pszNames++ = '+';
                    }
                }
                *pszNames = '\0';
            }
        }
    }

    return cCritSects;
}
