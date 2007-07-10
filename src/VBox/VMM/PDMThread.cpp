/* $Id$ */
/** @file
 * PDM Thread - VM Thread Management.
 */

/*
 * Copyright (C) 2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
///@todo #define LOG_GROUP LOG_GROUP_PDM_THREAD
#include "PDMInternal.h"
#include <VBox/pdm.h>
#include <VBox/mm.h>
#include <VBox/vm.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/thread.h>


/** @group grp_pdm_thread
 * @{
 */

/** 
 * The thread state
 */
typedef enum PDMTHREADSTATE
{
    /** The usual invalid 0 entry. */
    PDMTHREADSTATE_INVALID = 0,
    /** The thread is initializing.
     * Prev state: none
     * Next state: suspended, terminating (error) */
    PDMTHREADSTATE_INITIALIZING,
    /** The thread has been asked to suspend.
     * Prev state: running
     * Next state: suspended */
    PDMTHREADSTATE_SUSPENDING,
    /** The thread is supended.
     * Prev state: suspending, initializing
     * Next state: resuming, terminated. */
    PDMTHREADSTATE_SUSPENDED,
    /** The thread is active.
     * Prev state: suspended
     * Next state: running, terminating. */
    PDMTHREADSTATE_RESUMING,
    /** The thread is active.
     * Prev state: resuming
     * Next state: suspending, terminating. */
    PDMTHREADSTATE_RUNNING,
    /** The thread has been asked to terminate. 
     * Prev state: initializing, suspended, resuming, running
     * Next state: terminated. */
    PDMTHREADSTATE_TERMINATING,
    /** The thread is terminating / has terminated.
     * Prev state: terminating
     * Next state: none */
    PDMTHREADSTATE_TERMINATED,
    /** The usual 32-bit hack. */
    PDMTHREADSTATE_32BIT_HACK = 0x7fffffff
} PDMTHREADSTATE;

/** A pointer to an PDM thread. */
typedef HCPTRTYPE(struct PDMTHREAD *) PPDMTHREAD;

/**
 * PDM thread, device variation.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pThread     The PDM thread data.
 */
typedef int FNPDMTHREADDEV(PPDMDEVINS pDevIns, PPDMTHREAD pThread);
/** Pointer to a FNPDMTHREADDEV(). */
typedef FNPDMTHREADDEV *PFNPDMTHREADDEV;

/**
 * PDM thread, driver variation.
 *
 * @returns VBox status code.
 * @param   pDrvIns     The driver instance.
 * @param   pThread     The PDM thread data.
 */
typedef int FNPDMTHREADDRV(PPDMDEVINS pDrvIns, PPDMTHREAD pThread);
/** Pointer to a FNPDMTHREADDRV(). */
typedef FNPDMTHREADDRV *PFNPDMTHREADDRV;

/**
 * PDM thread, driver variation.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pThread     The PDM thread data.
 */
typedef int FNPDMTHREADINT(PVM pVM, PPDMTHREAD pThread);
/** Pointer to a FNPDMTHREADINT(). */
typedef FNPDMTHREADINT *PFNPDMTHREADINT;

/**
 * PDM thread, driver variation.
 *
 * @returns VBox status code.
 * @param   pThread     The PDM thread data.
 */
typedef int FNPDMTHREADEXT(PPDMTHREAD pThread);
/** Pointer to a FNPDMTHREADEXT(). */
typedef FNPDMTHREADEXT *PFNPDMTHREADEXT;



/**
 * PDM thread wakup call, device variation.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pThread     The PDM thread data.
 */
typedef int FNPDMTHREADWAKEUPDEV(PPDMDEVINS pDevIns, PPDMTHREAD pThread);
/** Pointer to a FNPDMTHREADDEV(). */
typedef FNPDMTHREADWAKEUPDEV *PFNPDMTHREADWAKEUPDEV;

/**
 * PDM thread wakup call, driver variation.
 *
 * @returns VBox status code.
 * @param   pDrvIns     The driver instance.
 * @param   pThread     The PDM thread data.
 */
typedef int FNPDMTHREADWAKEUPDRV(PPDMDEVINS pDrvIns, PPDMTHREAD pThread);
/** Pointer to a FNPDMTHREADDRV(). */
typedef FNPDMTHREADWAKEUPDRV *PFNPDMTHREADWAKEUPDRV;

/**
 * PDM thread wakup call, internal variation.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pThread     The PDM thread data.
 */
typedef int FNPDMTHREADWAKEUPINT(PVM pVM, PPDMTHREAD pThread);
/** Pointer to a FNPDMTHREADWAKEUPINT(). */
typedef FNPDMTHREADWAKEUPINT *PFNPDMTHREADWAKEUPINT;

/**
 * PDM thread wakup call, external variation.
 *
 * @returns VBox status code.
 * @param   pThread     The PDM thread data.
 */
typedef int FNPDMTHREADWAKEUPEXT(PPDMTHREAD pThread);
/** Pointer to a FNPDMTHREADEXT(). */
typedef FNPDMTHREADWAKEUPEXT *PFNPDMTHREADWAKEUPEXT;


/** The usual device/driver/internal/external stuff. */
typedef enum 
{
    /** The usual invalid entry. */
    PDMTHREADTYPE_INVALID = 0,
    /** Device type. */
    PDMTHREADTYPE_DEVICE,
    /** Driver type. */
    PDMTHREADTYPE_DRIVER, 
    /** Internal type. */
    PDMTHREADTYPE_INTERNAL, 
    /** External type. */
    PDMTHREADTYPE_EXTERNAL,
    /** The usual 32-bit hack. */
    PDMTHREADTYPE_32BIT_HACK = 0x7fffffff
} PDMTHREADTYPE;

/**
 * The internal structure for the thread.
 */
typedef struct PDMTHREADINT
{
    /** The VM pointer. */
    PVMR3               pVM;
    /** The event semaphore the thread blocks on. */
    RTSEMEVENTMULTI     BlockEvent;
    /** The thread type. */
    PDMTHREADTYPE       enmType;
    union
    {
        struct
        {
            /** The device instance. */
            PPDMDEVINSR3                        pDevIns;
            /** The thread function. */
            R3PTRTYPE(PFNPDMTHREADDEV)          pfnThread;
            /** Thread. */
            R3PTRTYPE(PFNPDMTHREADWAKEUPDEV)    pfnWakeup;
        } Dev;

        struct
        {
            /** The driver instance. */
            PPDMDRVINSR3                        pDrvIns;
            /** The thread function. */
            R3PTRTYPE(PFNPDMTHREADDRV)          pfnThread;
            /** Thread. */
            R3PTRTYPE(PFNPDMTHREADWAKEUPDRV)    pfnWakeup;
        } Drv;

    } u;
} PDMTHREADINT;


typedef struct PDMTHREAD
{
#define PDMTHREAD_VERSION       0x10000000
    /** PDMTHREAD_VERSION. */
    uint32_t                    u32Version;
    /** The thread state. */
    PDMTHREADSTATE volatile     enmState;
    /** The thread handle. */
    RTTHREAD                    Thread;
    /** The user parameter. */
    R3PTRTYPE(void *)           pvUser;

    /** Internal data. */
    union
    {
#ifdef PDMTHREADINT_DECLARED
        PDMTHREADINT            s;
#endif 
        uint8_t                 padding[64];
    } Internal;
} PDMTHREAD;



/** @} */


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) pdmR3ThreadMain(RTTHREAD Thread, void *pvUser);



/**
 * Does the wakeup call.
 *
 * @returns VBox status code. Already asserted on failure.
 * @param   pThread     The PDM thread.
 */
static DECLCALLBACK(int) pdmR3ThreadWakeup(PPDMTHREAD pThread)
{
    int rc;
    switch (pThread->Internal.s.enmType)
    {
        case PDMTHREADTYPE_DEVICE:
            rc = pThread->Internal.s.u.Dev.pfnWakeup(pThread->Internal.s.u.Dev.pDevIns, pThread);
            break;

        case PDMTHREADTYPE_DRIVER:
            rc = pThread->Internal.s.u.Drv.pfnWakeup(pThread->Internal.s.u.Drv.pDrvIns, pThread);
            break;

        case PDMTHREADTYPE_INTERNAL:
            rc = pThread->Internal.s.u.Int.pfnWakeup(pThread->Internal.s.pVM, pThread);
            break;

        case PDMTHREADTYPE_EXTERNAL:
            rc = pThread->Internal.s.u.Int.pfnWakeup(pThread);
            break;

        default:
            AssertMsgFailed(("%d\n", pThread->Internal.s.enmType));
            rc = VERR_INTERNAL_ERROR;
            break;
    }
    AssertRC(rc);
    return rc;
}


/**
 * Allocates new thread instance.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   ppThread    Where to store the pointer to the instance.
 */
static int pdmR3ThreadNew(PVM pVM, PPDMTHREAD ppThread)
{
    PPDMTHREAD pThread;
    int rc = MMR3HeapAllocZEx(pVM, MM_TAG_PDM_THREAD, sizeof(*pQueue), (void **)&pThread);
    if (RT_FAILURE(rc))
        return rc;

    pThread->u32Version     = PDMTHREAD_VERSION;
    pThread->enmState       = PDMTHREADSTATE_INITIALIZING;
    pThread->Thread         = RT_NILTHREAD;
    pThread->Internal.s.pVM = pVM;

    *ppThread = pThread;
    return VINF_SUCCESS;
}



/**
 * Initialize a new thread, this actually creates the thread.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   enmType     The thread type, see RTThreadCreate().
 * @param   cbStack     The stack size, see RTThreadCreate().
 * @param   pszName     The thread name, see RTThreadCreate().
 * @param   ppThread    Where the thread instance data handle is.
 */
static int pdmR3ThreadInit(PVM pVM, RTTHREADTYPE enmType, size_t cbStack, const char *pszName, PPPDMTHREAD ppThread)
{
    PPDMTHREAD pThread = *ppThread;
    
    /*
     * Initialize the remainder of the structure.
     */
    pThread->Internal.s.pVM = pVM;

    int rc = RTSemEventMultiCreate(&pThread->Internal.s.BlockEvent);
    if (RT_SUCCESS(rc))
    {
        /* 
         * Create the thread and wait for it to initialize.
         */
        rc = RTThreadCreate(&pThread->Thread, pdmR3ThreadMain, pThread, cbStack, enmType, RTTHREADFLAGS_WAITABLE, pszName);
        if (RT_SUCCESS(rc))
        {
            rc = RTThreadWait(pThread->Thread, 60*1000);
            if (    RT_SUCCESS(rc) 
                &&  pThread->enmState != PDMTHREADSTATE_SUSPEND)
                rc = VERR_INTERNAL_ERROR;
            if (RT_SUCCESS(rc))
            {
                rc = RTThreadReset(pThread->Thread);
                AssertRC(rc);
                return rc;
            }

            /* bailout */
            RTThreadWait(pThread->Thread, 60*1000, NULL);
        }
        RTSemEventMultiDestroy(pThread->Internal.s.BlockEvent);
        pThread->Internal.s.BlockEvent = NIL_RTSEMEVENTMULTI;
    }
    MMHyperFree(pVM, pThread);
    *ppThread = NULL;
     
    return rc;
}



PDMR3DECL(int) PDMR3ThreadCreateDevice(PVM pVM, PPDMDEVINS pDevIns, void *pvUser, PFNPDMTHREADDEV pfnThread, 
                                       PFNPDMTHREADWAKEUPDEV pfnWakeup, size_t cbStack, RTTHREADTYPE enmType, const char *pszName)
{
    int rc = pdmR3ThreadNew(pVM, ppThread);
    if (RT_SUCCESS(rc))
    {
        (*ppThread)->Internal.s.enmType = PDMTHREADTYPE_DEVICE;
        (*ppThread)->Internal.s.u.Dev.pDevIns = pDevIns;
        (*ppThread)->Internal.s.u.Dev.pfnThread = pfnThread;
        (*ppThread)->Internal.s.u.Dev.pfnWakeup = pfnWakeup;
        rc = pdmR3ThreadInit(pVM, pThread);
    }
    return rc;
}


/**
 * Destroys a PDM thread.
 *
 * This will wakeup the thread, tell it to terminate, and wait for it terminate.
 *
 * @returns VBox status code.
 *          This reflects the success off destroying the thread and not the exit code 
 *          of the thread as this is stored in *pRcThread.
 * @param   pThread         The thread to destroy.
 * @param   pRcThread       Where to store the thread exit code. Optional.
 * @thread  The emulation thread (EMT).
 */
PDMR3DECL(int) PDMR3ThreadDestroy(PPDMTHREAD pThread, int *pRcThread)
{
    /*
     * Assert sanity.
     */
    AssertPtrReturn(pThread, VERR_INVALID_POINTER);
    AssertReturn(pThread->u32Version == PDMTHREAD_VERSION, VERR_INVALID_MAGIC);
    Assert(pThread->Thread != RTThreadSelf());
    AssertPtrNullReturn(pRcThread, VERR_INVALID_POINTER);
    PVM pVM = pThread->Internal.s.pVM;
    VM_ASSERT_EMT(pVM);
    
    /*
     * Advance the thread to the terminating state.
     */
    int rc = VINF_SUCCESS;
    if (pThread->enmState <= PDMTHREADSTATE_TERMINATING)
    {
        for (;;)
        {
            PDMTHREADSTATE enmState = pThread->enmState;
            switch (enmState)
            {
                case PDMTHREADSTATE_RUNNING:
                    if (!ASMAtomicCmpXchgU32(&pThread->enmState, PDMTHREADSTATE_TERMINATING, enmState))
                        continue;
                    rc = pdmR3ThreadWakeup(pThread);
                    break;

                case PDMTHREADSTATE_SUSPENDING:
                case PDMTHREADSTATE_SUSPENDED:
                case PDMTHREADSTATE_RESUMING:
                case PDMTHREADSTATE_INITIALIZING:
                    if (!ASMAtomicCmpXchgU32(&pThread->enmState, PDMTHREADSTATE_TERMINATING, enmState))
                        continue;
                    break;

                case PDMTHREADSTATE_TERMINATING:
                case PDMTHREADSTATE_TERMINATED:
                    break;

                default:
                    AssertMsgFailed(("enmState=%d\n", enmState));
                    rc = VERR_INTERNAL_ERROR;
                    break;
            }
            break;
        }
    }

    /*
     * Wait for it to terminate and the do cleanups.
     */
    int rc2 = RTThreadWait(pThread->Thread, RT_SUCCESS(rc) ? 60*1000 : 150, pRcThread);
    if (RT_SUCCESS(rc2))
    {
        /* make it invalid. */
        pThread->u32Version = 0xffffffff;
        pThread->enmState = PDMTHREADSTATE_INVALID;
        pThread->Thread = NIL_RTTHREAD;

        /* unlink */
        if (pVM->pdm.s.pThreads == pThread)
            pVM->pdm.s.pThreads = pThread->Internal.s.pNext;
        else
        {
            PPDMTHREAD pPrev = pVM->pdm.s.pThreads;
            while (pPrev && pPrev->pNext != pThread)
                pPrev = pPrev->Internal.s.pNext;
            Assert(pPrev);
            if (pPrev)
                pPrev->Internal.s.pNext = pThread->Internal.s.pNext;
        }
        if (pVM->pdm.s.pThreadsTail == pThread)
        {
            Assert(pVM->pdm.s.pThreads == NULL);
            pVM->pdm.s.pThreadsTail = NULL;
        }
        pThread->Internal.s.pNext = NULL;

        /* free it */
        MMR3HeapFree(pThread);
    }
    else if (RT_SUCCESS(rc))
        rc = rc2;

    return rc;
}



PDMR3DECL(int) PDMR3ThreadDestroyDevice(PVM pVM, PPDMDEVINS pDevIns)
{
    int rc = VINF_SUCCESS;

    AssertPtr(pDevIns);
    PPDMTHREAD pThread = pVM->pdm.s.pThreads; 
    while (pThread)
    {
        PPDMTHREAD pNext = pThread->Internal.s.pNext;
        if (    pThread->Internal.s.enmType == PDMTHREADTYPE_DEVICE
            &&  pThread->Internal.s.u.Dev.pDevIns == pDevIns)
        {
            int rc2 = PDMR3ThreadDestroy(pThread, NULL);
            if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                rc = rc2;
        }
    }

    return rc;
}


PDMR3DECL(int) PDMR3ThreadDestroyDriver(PVM pVM, PPDMDRVINS pDrvIns)
{
    int rc = VINF_SUCCESS;

    AssertPtr(pDrvIns);
    PPDMTHREAD pThread = pVM->pdm.s.pThreads; 
    while (pThread)
    {
        PPDMTHREAD pNext = pThread->Internal.s.pNext;
        if (    pThread->Internal.s.enmType == PDMTHREADTYPE_DRIVER
            &&  pThread->Internal.s.u.Drv.pDrvIns == pDrvIns)
        {
            int rc2 = PDMR3ThreadDestroy(pThread, NULL);
            if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                rc = rc2;
        }
    }

    return rc;
}


/**
 * Called For VM power off.
 *
 * @param   pVM 
 */
void pdmR3ThreadDestroyAll(PVM pVM)
{
    AssertPtr(pDevIns);
    PPDMTHREAD pThread = pVM->pdm.s.pThreads; 
    while (pThread)
    {
        PPDMTHREAD pNext = pThread->Internal.s.pNext;
        int rc2 = PDMR3ThreadDestroy(pThread, NULL);
        if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
            rc = rc2;
    }

    return rc;
}




/**
 * Initiate termination of the thread (self) because something failed in a bad way. 
 *
 * @param   pThread         The PDM thread.
 */
static void pdmR3ThreadBailMeOut(PPDMTHREAD pThread)
{
    for (;;)
    {
        PDMTHREADSTATE enmState = pThread->enmState;
        switch (enmState)
        {
            case PDMTHREADSTATE_SUSPENDING:
            case PDMTHREADSTATE_SUSPENDED:
            case PDMTHREADSTATE_RESUMING:
            case PDMTHREADSTATE_RUNNING:
                if (!ASMAtomicCmpXchgU32(&pThread->enmState, PDMTHREADSTATE_TERMINATING, enmState))
                    continue;
                break;

            case PDMTHREADSTATE_TERMINATING:
            case PDMTHREADSTATE_TERMINATED:
                break;

            case PDMTHREADSTATE_INITIALIZING:
            default:
                AssertMsgFailed(("enmState=%d\n", enmState));
                break;
        }
        break;
    }
}


/**
 * Called by the PDM thread in response to a wakeup call with 
 * suspending as the new state.
 *
 * The thread will block in side this call until the state is changed in 
 * response to a VM state change or to the device/driver/whatever calling the 
 * PDMR3ThreadResume API.
 *
 * @returns VBox status code.
 *          On failure, terminate the thread.
 * @param   pThread     The PDM thread.
 */
PDMR3DECL(int) PDMR3ThreadIAmSuspending(PPDMTHREAD pThread)
{
    /*
     * Assert sanity.
     */
    AssertPtr(pThread);
    Assert(pThread->u32Version == PGMTHREAD_VERSION, VERR_INVALID_MAGIC);
    Assert(pThread->Thread == RTThreadSelf() || pThread->enmState == PDMTHREADSTATE_INITIALIZING);
    PDMTHREADSTATE enmState = pThread->enmState;
    Assert(     enmState == PDMTHREADSTATE_SUSPENDING
           ||   enmState == PDMTHREADSTATE_INITIALIZING);

    /*
     * Update the state, notify the control thread (the API caller) and go to sleep.
     */
    int rc = VERR_WRONG_ORDER;
    if (ASMAtomicCmpXchgU32(&pThread->enmState, PDMTHREADSTATE_SUSPENDED, enmState))
    {
        rc = RTThreadUserSignal(pThread->Thread);
        if (RT_SUCCESS(rc))
        {
            rc = RTSemEventMultiWait(pThread->Internal.s.BlockEvent, RT_INFINITE_WAIT);
            if (    RT_SUCCESS(rc)
                &&  pThread->enmState != PDMTHREADSTATE_SUSPENDED)
                return rc;
            
            if (RT_SUCCESS(rc))
                rc = VERR_INTERNAL_ERROR;
        }
    }

    AssertMsgFailed(("rc=%d enmState=%d\n", rc, pThread->enmState)):
    pdmR3ThreadBailMeOut(pThread);
    return rc;
}


/**
 * Called by the PDM thread in response to a resuming state.
 *
 * The purpose of this API is to tell the PDMR3ThreadResume caller that
 * the the PDM thread has successfully resumed. It will also do the
 * state transition from the resuming to the running state.
 *
 * @returns VBox status code.
 *          On failure, terminate the thread.
 * @param   pThread     The PDM thread. 
 */
PDMR3DECL(int) PDMR3ThreadIAmRunning(PPDMTHREAD pThread)
{
    /*
     * Assert sanity.
     */
    Assert(pThread->enmState == PDMTHREADSTATE_RESUMING);
    Assert(pThread->Thread == RTThreadSelf());

    /*
     * Update the state and tell the control thread (the guy calling the resume API).
     */
    int rc = VERR_WRONG_ORDER;
    if (ASMAtomicCmpXchgU32(&pThread->enmState, PDMTHREADSTATE_RUNNING, PDMTHREADSTATE_RESUMING))
    {
        rc = RTThreadUserSignal(pThread->Thread);
        if (RT_SUCCESS(rc))
            return rc;
    }
    
    AssertMsgFailed(("rc=%d enmState=%d\n", rc, pThread->enmState)):
    pdmR3ThreadBailMeOut(pThread);
    return rc;
}


/**
 * The PDM thread function.
 *
 * @returns return from pfnThread. 
 *
 * @param   Thread  The thread handle.
 * @param   pvUser  Pointer to the PDMTHREAD structure.
 */
static DECLCALLBACK(int) pdmR3ThreadMain(RTTHREAD Thread, void *pvUser)
{
    PPDMTHREAD pThread = (PPDMTHREAD)pvUser;
    Log(("PDMThread: Initializing thread %RTthrd / %p / '%s'...\n", Thread, pThread, RTThreadGetName(Thread)));

    /*
     * The run loop.
     *
     * It handles simple thread functions which returns when they see a suspending 
     * request and leaves the PDMR3ThreadIAmSuspending and PDMR3ThreadIAmRunning 
     * parts to us.
     */
    int rc;
    for (;;)
    {
        switch (pThread->Internal.s.enmType)
        {
            case PDMTHREADTYPE_DEVICE:
                rc = pThread->Internal.s.u.Dev.pfnThread(pThread->Internal.s.u.Dev.pDevIns, pThread);
                break;
    
            case PDMTHREADTYPE_DRIVER:
                rc = pThread->Internal.s.u.Drv.pfnThread(pThread->Internal.s.u.Drv.pDrvIns, pThread);
                break;
    
            case PDMTHREADTYPE_INTERNAL:
                rc = pThread->Internal.s.u.Int.pfnThread(pThread->Internal.s.pVM, pThread);
                break;
    
            case PDMTHREADTYPE_EXTERNAL:
                rc = pThread->Internal.s.u.Int.pfnThread(pThread);
                break;
    
            default:
                AssertMsgFailed(("%d\n", pThread->Internal.s.enmType));
                rc = VERR_INTERNAL_ERROR;
                break;
        }
        if (RT_FAILURE(rc))
            break;

        /* 
         * If this is a simple thread function, the state will be suspending 
         * or initializing now. If it isn't we're supposed to terminate.
         */
        if (    pThread->enmState != PDMTHREADSTATE_SUSPENDING
            &&  pThread->enmState != PDMTHREADSTATE_INITIALIZING)
        {
            Assert(pThread->enmState == PDMTHREADSTATE_TERMINATING);
            break;
        }
        rc = PDMR3ThreadIAmSuspending(pThread);
        if (RT_FAILURE(rc))
            break;
        if (pThread->enmState != PDMTHREADSTATE_RESUMING)
        {
            Assert(pThread->enmState == PDMTHREADSTATE_TERMINATING);
            break;
        }

        rc = PDMR3ThreadIAmRunning(pThread);
        if (RT_FAILURE(rc))
            break;
    }

    /*
     * Advance the state to terminating and then on to terminated.
     */
    for (;;)
    {
        PDMTHREADSTATE enmState = pThread->enmState;
        if (    enmState == PDMTHREADSTATE_TERMINATING
            ||  ASMAtomicCmpXchgU32(&pThread->enmState, PDMTHREADSTATE_TERMINATING, enmState))
            break;
    }

    ASMAtomicXchgU32(&pThread->enmState, PDMTHREADSTATE_TERMINATED);
    int rc2 = RTThreadUserSignal(Thread); AssertRC(rc2);
    
    Log(("PDMThread: Terminating thread %RTthrd / %p / '%s': %Rrc\n", Thread, pThread, RTThreadGetName(Thread), rc));
    return rc;
}


/**
 * Initiate termination of the thread because something failed in a bad way. 
 *
 * @param   pThread         The PDM thread.
 */
static void pdmR3ThreadBailOut(PPDMTHREAD pThread)
{
    for (;;)
    {
        PDMTHREADSTATE enmState = pThread->enmState;
        switch (enmState)
        {
            case PDMTHREADSTATE_SUSPENDING:
            case PDMTHREADSTATE_SUSPENDED:
                if (!ASMAtomicCmpXchgU32(&pThread->enmState, PDMTHREADSTATE_TERMINATING, enmState))
                    continue;
                RTSemEventMultiSignal(pThread->Internal.s.BlockEvent);
                break;

            case PDMTHREADSTATE_RESUMING:
                if (!ASMAtomicCmpXchgU32(&pThread->enmState, PDMTHREADSTATE_TERMINATING, enmState))
                    continue;
                break;

            case PDMTHREADSTATE_RUNNING:
                if (!ASMAtomicCmpXchgU32(&pThread->enmState, PDMTHREADSTATE_TERMINATING, enmState))
                    continue;
                pdmR3ThreadWakeup(pThread);
                break;

            case PDMTHREADSTATE_TERMINATING:
            case PDMTHREADSTATE_TERMINATED:
                break;

            case PDMTHREADSTATE_INITIALIZING:
            default:
                AssertMsgFailed(("enmState=%d\n", enmState));
                break;
        }
        break;
    }
}


/**
 * Suspends the thread.
 *
 * This can be called the power off / suspend notifications to suspend the 
 * PDM thread a bit early. The thread will be automatically suspend upon
 * return from these two notification callbacks (devices/drivers).
 *
 * The caller is responsible for serializing the control operations on the 
 * thread. That basically means, always do these calls from the EMT.
 *
 * @returns VBox status code.
 * @param   pThread     The PDM thread.
 */
PDMR3DECL(int) PDMR3ThreadSuspend(PPDMTHREAD pThread)
{
    /*
     * Assert sanity.
     */
    AssertPtrReturn(pThread, VERR_INVALID_POINTER);
    AssertReturn(pThread->u32Version == PGMTHREAD_VERSION, VERR_INVALID_MAGIC);
    Assert(pThread->Thread != RTThreadSelf());

    /*
     * Change the state to resuming and kick the thread.
     */
    int rc = RTSemEventMultiReset(pThread->Internal.s.BlockEvent);
    if (RT_SUCCESS(rc))
    {
        rc = RTThreadUserReset(pThread->Thread);
        if (RT_SUCCESS(rc))
        {
            rc = VERR_WRONG_ORDER;
            if (ASMAtomicCmpXchgU32(&pThread->enmState, PDMTHREADSTATE_SUSPENDING, PDMTHREADSTATE_RUNNING))
            {
                rc = pdmR3ThreadWakeup(pThread);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Wait for the thread to reach the suspended state.
                     */
                    if (pThread->enmState != PDMTHREADSTATE_SUSPENDED)
                        rc = RTThreadUserWait(pThread->Thread, 60*1000);
                    if (    RT_SUCCESS(rc) 
                        &&  pThread->enmState != PDMTHREADSTATE_SUSPENDED)
                        rc = VERR_INTERNAL_ERROR;
                    if (RT_SUCCESS(rc))
                        return rc;
                }
            }
        }
    }

    /*
     * Something failed, initialize termination.
     */
    AssertMsgFailed(("PDMR3ThreadSuspend -> rc=%Rrc enmState=%d\n", rc, pThread->enmState));
    pdmR3ThreadBailOut(pThread);
    return rc;
}


/**
 * Suspend all running threads.
 *
 * This is called by PDMR3Suspend() and PDMR3PowerOff() after all the devices 
 * and drivers have been notified about the suspend / power off.
 *           
 * @return VBox status code.
 * @param   pVM         The VM handle.
 */
int pdmR3ThreadSuspendAll(PVM pVM)
{
    for (PPDMTHREAD pThread = pVM->pdm.s.pThreads; pThread = pThread->Internal.s.pNext)
        switch (pThread->enmState)
        {
            case PDMTHREADSTATE_RUNNING:
            {
                int rc = PDMR3ThreadSuspend(pThread);
                AssertRCReturn(rc, rc);
                break;
            }

            default:
                AssertMsgFailed(("pThread=%p enmState=%d\n", pThread, pThread->enmState));
                break;
        }
    return VINF_SUCCESS;
}


/**
 * Resumes the thread.
 *
 * This can be called the power on / resume notifications to resume the 
 * PDM thread a bit early. The thread will be automatically resumed upon
 * return from these two notification callbacks (devices/drivers).
 *
 * The caller is responsible for serializing the control operations on the 
 * thread. That basically means, always do these calls from the EMT.
 *
 * @returns VBox status code.
 * @param   pThread     The PDM thread.
 */
PDMR3DECL(int) PDMR3ThreadResume(PPDMTHREAD pThread)
{
    /*
     * Assert sanity.
     */
    AssertPtrReturn(pThread, VERR_INVALID_POINTER);
    AssertReturn(pThread->u32Version == PGMTHREAD_VERSION, VERR_INVALID_MAGIC);
    Assert(pThread->Thread != RTThreadSelf());

    /*
     * Change the state to resuming and kick the thread.
     */
    int rc = RTThreadUserReset(pThread->Thread);
    if (RT_SUCCESS(rc))
    {
        rc = VERR_WRONG_ORDER;
        if (ASMAtomicCmpXchgU32(&pThread->enmState, PDMTHREADSTATE_RESUMING, PDMTHREADSTATE_SUSPENDED))
        {
            rc = RTSemEventMultiSignal(pThread->Internal.s.BlockEvent);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Wait for the thread to reach the running state.
                 */
                rc = RTThreadUserWait(pThread->Thread, 60*1000);
                if (    RT_SUCCESS(rc) 
                    &&  pThread->emState != PDMTHREADSTATE_RUNNING)
                    rc = VERR_INTERNAL_ERROR;
                if (RT_SUCCESS(rc))
                    return rc;
            }
        }
    }

    /*
     * Something failed, initialize termination.
     */
    AssertMsgFailed(("PDMR3ThreadResume -> rc=%Rrc enmState=%d\n", rc, pThread->enmState));
    pdmR3ThreadBailOut(pThread);
    return rc;
}


/**
 * Resumes all threads not running.
 *
 * This is called by PDMR3Resume() and PDMR3PowerOn() after all the devices 
 * and drivers have been notified about the resume / power on .
 *           
 * @return VBox status code.
 * @param   pVM         The VM handle.
 */
int pdmR3ThreadResumeAll(PVM pVM)
{
    for (PPDMTHREAD pThread = pVM->pdm.s.pThreads; pThread = pThread->Internal.s.pNext)
        switch (pThread->enmState)
        {
            case PDMTHREADSTATE_SUSPENDED:
            {
                int rc = PDMR3ThreadResume(pThread);
                AssertRCReturn(rc, rc);
                break;
            }

            default:
                AssertMsgFailed(("pThread=%p enmState=%d\n", pThread, pThread->enmState));
                break;
        }
    return VINF_SUCCESS;
}

