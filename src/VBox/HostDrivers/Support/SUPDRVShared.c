/** @file
 *
 * VBox host drivers - Ring-0 support drivers - Shared code:
 * Driver code for all host platforms
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
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
#include "SUPDRV.h"
#ifndef PAGE_SHIFT
# include <iprt/param.h>
#endif
#include <iprt/alloc.h>
#include <iprt/semaphore.h>
#include <iprt/spinlock.h>
#include <iprt/thread.h>
#include <iprt/process.h>
#include <iprt/log.h>
#ifdef VBOX_WITHOUT_IDT_PATCHING
# include <VBox/vmm.h>
#endif


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/* from x86.h - clashes with linux thus this duplication */
#undef X86_CR0_PG
#define X86_CR0_PG                          BIT(31)
#undef X86_CR0_PE
#define X86_CR0_PE                          BIT(0)
#undef X86_CPUID_AMD_FEATURE_EDX_NX
#define X86_CPUID_AMD_FEATURE_EDX_NX        BIT(20)
#undef MSR_K6_EFER
#define MSR_K6_EFER                         0xc0000080
#undef MSR_K6_EFER_NXE
#define MSR_K6_EFER_NXE                     BIT(11)
#undef MSR_K6_EFER_LMA
#define  MSR_K6_EFER_LMA                    BIT(10)
#undef X86_CR4_PGE
#define X86_CR4_PGE                         BIT(7)
#undef X86_CR4_PAE
#define X86_CR4_PAE                         BIT(5)
#undef X86_CPUID_AMD_FEATURE_EDX_LONG_MODE
#define X86_CPUID_AMD_FEATURE_EDX_LONG_MODE BIT(29)


/** The frequency by which we recalculate the u32UpdateHz and
 * u32UpdateIntervalNS GIP members. The value must be a power of 2. */
#define GIP_UPDATEHZ_RECALC_FREQ            0x800


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * Array of the R0 SUP API.
 */
static SUPFUNC g_aFunctions[] =
{
    /* name                                     function */
    { "SUPR0ObjRegister",                       (void *)SUPR0ObjRegister },
    { "SUPR0ObjAddRef",                         (void *)SUPR0ObjAddRef },
    { "SUPR0ObjRelease",                        (void *)SUPR0ObjRelease },
    { "SUPR0ObjVerifyAccess",                   (void *)SUPR0ObjVerifyAccess },
    { "SUPR0LockMem",                           (void *)SUPR0LockMem },
    { "SUPR0UnlockMem",                         (void *)SUPR0UnlockMem },
    { "SUPR0ContAlloc",                         (void *)SUPR0ContAlloc },
    { "SUPR0ContFree",                          (void *)SUPR0ContFree },
    { "SUPR0MemAlloc",                          (void *)SUPR0MemAlloc },
    { "SUPR0MemGetPhys",                        (void *)SUPR0MemGetPhys },
    { "SUPR0MemFree",                           (void *)SUPR0MemFree },
    { "SUPR0Printf",                            (void *)SUPR0Printf },
    { "RTMemAlloc",                             (void *)RTMemAlloc },
    { "RTMemAllocZ",                            (void *)RTMemAllocZ },
    { "RTMemFree",                              (void *)RTMemFree },
/* These doesn't work yet on linux - use fast mutexes!
    { "RTSemMutexCreate",                       (void *)RTSemMutexCreate },
    { "RTSemMutexRequest",                      (void *)RTSemMutexRequest },
    { "RTSemMutexRelease",                      (void *)RTSemMutexRelease },
    { "RTSemMutexDestroy",                      (void *)RTSemMutexDestroy },
*/
    { "RTSemFastMutexCreate",                   (void *)RTSemFastMutexCreate },
    { "RTSemFastMutexDestroy",                  (void *)RTSemFastMutexDestroy },
    { "RTSemFastMutexRequest",                  (void *)RTSemFastMutexRequest },
    { "RTSemFastMutexRelease",                  (void *)RTSemFastMutexRelease },
    { "RTSemEventCreate",                       (void *)RTSemEventCreate },
    { "RTSemEventSignal",                       (void *)RTSemEventSignal },
    { "RTSemEventWait",                         (void *)RTSemEventWait },
    { "RTSemEventDestroy",                      (void *)RTSemEventDestroy },
    { "RTSpinlockCreate",                       (void *)RTSpinlockCreate },
    { "RTSpinlockDestroy",                      (void *)RTSpinlockDestroy },
    { "RTSpinlockAcquire",                      (void *)RTSpinlockAcquire },
    { "RTSpinlockRelease",                      (void *)RTSpinlockRelease },
    { "RTSpinlockAcquireNoInts",                (void *)RTSpinlockAcquireNoInts },
    { "RTSpinlockReleaseNoInts",                (void *)RTSpinlockReleaseNoInts },
    { "RTThreadNativeSelf",                     (void *)RTThreadNativeSelf },
    { "RTThreadSleep",                          (void *)RTThreadSleep },
    { "RTThreadYield",                          (void *)RTThreadYield },
#if 0 /* Thread APIs, Part 2. */
    { "RTThreadSelf",                           (void *)RTThreadSelf },
    { "RTThreadCreate",                         (void *)RTThreadCreate },
    { "RTThreadGetNative",                      (void *)RTThreadGetNative },
    { "RTThreadWait",                           (void *)RTThreadWait },
    { "RTThreadWaitNoResume",                   (void *)RTThreadWaitNoResume },
    { "RTThreadGetName",                        (void *)RTThreadGetName },
    { "RTThreadSelfName",                       (void *)RTThreadSelfName },
    { "RTThreadGetType",                        (void *)RTThreadGetType },
    { "RTThreadUserSignal",                     (void *)RTThreadUserSignal },
    { "RTThreadUserReset",                      (void *)RTThreadUserReset },
    { "RTThreadUserWait",                       (void *)RTThreadUserWait },
    { "RTThreadUserWaitNoResume",               (void *)RTThreadUserWaitNoResume },
#endif
    { "RTLogDefaultInstance",                   (void *)RTLogDefaultInstance },
    { "RTLogRelDefaultInstance",                (void *)RTLogRelDefaultInstance },
    { "RTLogSetDefaultInstanceThread",          (void *)RTLogSetDefaultInstanceThread },
    { "RTLogLogger",                            (void *)RTLogLogger },
    { "RTLogLoggerEx",                          (void *)RTLogLoggerEx },
    { "RTLogLoggerExV",                         (void *)RTLogLoggerExV },
    { "AssertMsg1",                             (void *)AssertMsg1 },
    { "AssertMsg2",                             (void *)AssertMsg2 },
};


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS
static int      supdrvMemAdd(PSUPDRVMEMREF pMem, PSUPDRVSESSION pSession);
static int      supdrvMemRelease(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr, SUPDRVMEMREFTYPE eType);
#ifndef VBOX_WITHOUT_IDT_PATCHING
static int      supdrvIOCtl_IdtInstall(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPIDTINSTALL_IN pIn, PSUPIDTINSTALL_OUT pOut);
static PSUPDRVPATCH supdrvIdtPatchOne(PSUPDRVDEVEXT pDevExt, PSUPDRVPATCH pPatch);
static int      supdrvIOCtl_IdtRemoveAll(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession);
static void     supdrvIdtRemoveOne(PSUPDRVDEVEXT pDevExt, PSUPDRVPATCH pPatch);
static void     supdrvIdtWrite(volatile void *pvIdtEntry, const SUPDRVIDTE *pNewIDTEntry);
#endif /* !VBOX_WITHOUT_IDT_PATCHING */
static int      supdrvIOCtl_LdrOpen(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDROPEN_IN pIn, PSUPLDROPEN_OUT pOut);
static int      supdrvIOCtl_LdrLoad(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDRLOAD_IN pIn);
static int      supdrvIOCtl_LdrFree(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDRFREE_IN pIn);
static int      supdrvIOCtl_LdrGetSymbol(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDRGETSYMBOL_IN pIn, PSUPLDRGETSYMBOL_OUT pOut);
static int      supdrvLdrSetR0EP(PSUPDRVDEVEXT pDevExt, void *pvVMMR0, void *pvVMMR0Entry);
static void     supdrvLdrUnsetR0EP(PSUPDRVDEVEXT pDevExt);
static void     supdrvLdrAddUsage(PSUPDRVSESSION pSession, PSUPDRVLDRIMAGE pImage);
static void     supdrvLdrFree(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage);
static int      supdrvIOCtl_GetPagingMode(PSUPGETPAGINGMODE_OUT pOut);
static SUPGIPMODE supdrvGipDeterminTscMode(void);
#ifdef USE_NEW_OS_INTERFACE
static int      supdrvGipCreate(PSUPDRVDEVEXT pDevExt);
static int      supdrvGipDestroy(PSUPDRVDEVEXT pDevExt);
static DECLCALLBACK(void) supdrvGipTimer(PRTTIMER pTimer, void *pvUser);
#endif

__END_DECLS


/**
 * Initializes the device extentsion structure.
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_ on failure.
 * @param   pDevExt     The device extension to initialize.
 */
int VBOXCALL supdrvInitDevExt(PSUPDRVDEVEXT pDevExt)
{
    /*
     * Initialize it.
     */
    int rc;
    memset(pDevExt, 0, sizeof(*pDevExt));
    rc = RTSpinlockCreate(&pDevExt->Spinlock);
    if (!rc)
    {
        rc = RTSemFastMutexCreate(&pDevExt->mtxLdr);
        if (!rc)
        {
            rc = RTSemFastMutexCreate(&pDevExt->mtxGip);
            if (!rc)
            {
#ifdef USE_NEW_OS_INTERFACE
                rc = supdrvGipCreate(pDevExt);
                if (RT_SUCCESS(rc))
                {
                    pDevExt->u32Cookie = BIRD;
                    return 0;
                }
#else
                pDevExt->u32Cookie = BIRD;
                return 0;
#endif
            }
            RTSemFastMutexDestroy(pDevExt->mtxLdr);
            pDevExt->mtxLdr = NIL_RTSEMFASTMUTEX;
        }
        RTSpinlockDestroy(pDevExt->Spinlock);
        pDevExt->Spinlock = NIL_RTSPINLOCK;
    }
    return rc;
}

/**
 * Delete the device extension (e.g. cleanup members).
 *
 * @returns 0.
 * @param   pDevExt     The device extension to delete.
 */
int VBOXCALL supdrvDeleteDevExt(PSUPDRVDEVEXT pDevExt)
{
#ifndef VBOX_WITHOUT_IDT_PATCHING
    PSUPDRVPATCH        pPatch;
#endif
    PSUPDRVOBJ          pObj;
    PSUPDRVUSAGE        pUsage;

    /*
     * Kill mutexes and spinlocks.
     */
    RTSemFastMutexDestroy(pDevExt->mtxGip);
    pDevExt->mtxGip = NIL_RTSEMFASTMUTEX;
    RTSemFastMutexDestroy(pDevExt->mtxLdr);
    pDevExt->mtxLdr = NIL_RTSEMFASTMUTEX;
    RTSpinlockDestroy(pDevExt->Spinlock);
    pDevExt->Spinlock = NIL_RTSPINLOCK;

    /*
     * Free lists.
     */

#ifndef VBOX_WITHOUT_IDT_PATCHING
    /* patches */
    /** @todo make sure we don't uninstall patches which has been patched by someone else. */
    pPatch = pDevExt->pIdtPatchesFree;
    pDevExt->pIdtPatchesFree = NULL;
    while (pPatch)
    {
        void *pvFree = pPatch;
        pPatch = pPatch->pNext;
        RTMemExecFree(pvFree);
    }
#endif /* !VBOX_WITHOUT_IDT_PATCHING */

    /* objects. */
    pObj = pDevExt->pObjs;
#if !defined(DEBUG_bird) || !defined(__LINUX__) /* breaks unloading, temporary, remove me! */
    Assert(!pObj);                      /* (can trigger on forced unloads) */
#endif
    pDevExt->pObjs = NULL;
    while (pObj)
    {
        void *pvFree = pObj;
        pObj = pObj->pNext;
        RTMemFree(pvFree);
    }

    /* usage records. */
    pUsage = pDevExt->pUsageFree;
    pDevExt->pUsageFree = NULL;
    while (pUsage)
    {
        void *pvFree = pUsage;
        pUsage = pUsage->pNext;
        RTMemFree(pvFree);
    }

#ifdef USE_NEW_OS_INTERFACE
    /* kill the GIP */
    supdrvGipDestroy(pDevExt);
#endif

    return 0;
}


/**
 * Create session.
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_ on failure.
 * @param   pDevExt     Device extension.
 * @param   ppSession   Where to store the pointer to the session data.
 */
int VBOXCALL supdrvCreateSession(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION *ppSession)
{
    /*
     * Allocate memory for the session data.
     */
    int rc = SUPDRV_ERR_NO_MEMORY;
    PSUPDRVSESSION pSession = *ppSession = (PSUPDRVSESSION)RTMemAllocZ(sizeof(*pSession));
    if (pSession)
    {
        /* Initialize session data. */
        rc = RTSpinlockCreate(&pSession->Spinlock);
        if (!rc)
        {
            Assert(pSession->Spinlock != NIL_RTSPINLOCK);
            pSession->pDevExt           = pDevExt;
            pSession->u32Cookie         = BIRD_INV;
            /*pSession->pLdrUsage         = NULL;
            pSession->pPatchUsage       = NULL;
            pSession->pUsage            = NULL;
            pSession->pGip              = NULL;
            pSession->fGipReferenced    = false;
            pSession->Bundle.cUsed      = 0 */

            dprintf(("Created session %p initial cookie=%#x\n", pSession, pSession->u32Cookie));
            return 0;
        }

        RTMemFree(pSession);
        *ppSession = NULL;
    }

    dprintf(("Failed to create spinlock, rc=%d!\n", rc));
    return rc;
}


/**
 * Shared code for cleaning up a session.
 *
 * @param   pDevExt     Device extension.
 * @param   pSession    Session data.
 *                      This data will be freed by this routine.
 */
void VBOXCALL supdrvCloseSession(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession)
{
    /*
     * Cleanup the session first.
     */
    supdrvCleanupSession(pDevExt, pSession);

    /*
     * Free the rest of the session stuff.
     */
    RTSpinlockDestroy(pSession->Spinlock);
    pSession->Spinlock = NIL_RTSPINLOCK;
    pSession->pDevExt = NULL;
    RTMemFree(pSession);
    dprintf2(("supdrvCloseSession: returns\n"));
}


/**
 * Shared code for cleaning up a session (but not quite freeing it).
 *
 * This is primarily intended for MAC OS X where we have to clean up the memory
 * stuff before the file handle is closed.
 *
 * @param   pDevExt     Device extension.
 * @param   pSession    Session data.
 *                      This data will be freed by this routine.
 */
void VBOXCALL supdrvCleanupSession(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession)
{
    PSUPDRVBUNDLE       pBundle;
    dprintf(("supdrvCleanupSession: pSession=%p\n", pSession));

    /*
     * Remove logger instances related to this session.
     * (This assumes the dprintf and dprintf2 macros doesn't use the normal logging.)
     */
    RTLogSetDefaultInstanceThread(NULL, (uintptr_t)pSession);

#ifndef VBOX_WITHOUT_IDT_PATCHING
    /*
     * Uninstall any IDT patches installed for this session.
     */
    supdrvIOCtl_IdtRemoveAll(pDevExt, pSession);
#endif

    /*
     * Release object references made in this session.
     * In theory there should be noone racing us in this session.
     */
    dprintf2(("release objects - start\n"));
    if (pSession->pUsage)
    {
        RTSPINLOCKTMP   SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;
        PSUPDRVUSAGE    pUsage;
        RTSpinlockAcquire(pDevExt->Spinlock, &SpinlockTmp);

        while ((pUsage = pSession->pUsage) != NULL)
        {
            PSUPDRVOBJ  pObj = pUsage->pObj;
            pSession->pUsage = pUsage->pNext;

            AssertMsg(pUsage->cUsage >= 1 && pObj->cUsage >= pUsage->cUsage, ("glob %d; sess %d\n", pObj->cUsage, pUsage->cUsage));
            if (pUsage->cUsage < pObj->cUsage)
            {
                pObj->cUsage -= pUsage->cUsage;
                RTSpinlockRelease(pDevExt->Spinlock, &SpinlockTmp);
            }
            else
            {
                /* Destroy the object and free the record. */
                if (pDevExt->pObjs == pObj)
                    pDevExt->pObjs = pObj->pNext;
                else
                {
                    PSUPDRVOBJ pObjPrev;
                    for (pObjPrev = pDevExt->pObjs; pObjPrev; pObjPrev = pObjPrev->pNext)
                        if (pObjPrev->pNext == pObj)
                        {
                            pObjPrev->pNext = pObj->pNext;
                            break;
                        }
                    Assert(pObjPrev);
                }
                RTSpinlockRelease(pDevExt->Spinlock, &SpinlockTmp);

                pObj->pfnDestructor(pObj, pObj->pvUser1, pObj->pvUser2);
                RTMemFree(pObj);
            }

            /* free it and continue. */
            RTMemFree(pUsage);

            RTSpinlockAcquire(pDevExt->Spinlock, &SpinlockTmp);
        }

        RTSpinlockRelease(pDevExt->Spinlock, &SpinlockTmp);
        AssertMsg(!pSession->pUsage, ("Some buster reregistered an object during desturction!\n"));
    }
    dprintf2(("release objects - done\n"));

    /*
     * Release memory allocated in the session.
     *
     * We do not serialize this as we assume that the application will
     * not allocated memory while closing the file handle object.
     */
    dprintf2(("freeing memory:\n"));
    pBundle = &pSession->Bundle;
    while (pBundle)
    {
        PSUPDRVBUNDLE   pToFree;
        unsigned        i;

        /*
         * Check and unlock all entries in the bundle.
         */
        for (i = 0; i < sizeof(pBundle->aMem) / sizeof(pBundle->aMem[0]); i++)
        {
#ifdef USE_NEW_OS_INTERFACE
            if (pBundle->aMem[i].MemObj != NIL_RTR0MEMOBJ)
            {
                int rc;
                if (pBundle->aMem[i].MapObjR3 != NIL_RTR0MEMOBJ)
                {
                    rc = RTR0MemObjFree(pBundle->aMem[i].MapObjR3, false);
                    AssertRC(rc); /** @todo figure out how to handle this. */
                    pBundle->aMem[i].MapObjR3 = NIL_RTR0MEMOBJ;
                }
                rc = RTR0MemObjFree(pBundle->aMem[i].MemObj, false);
                AssertRC(rc); /** @todo figure out how to handle this. */
                pBundle->aMem[i].MemObj = NIL_RTR0MEMOBJ;
                pBundle->aMem[i].eType = MEMREF_TYPE_UNUSED;
            }

#else /* !USE_NEW_OS_INTERFACE */
            if (    pBundle->aMem[i].pvR0
                ||  pBundle->aMem[i].pvR3)
            {
                dprintf2(("eType=%d pvR0=%p pvR3=%p cb=%d\n", pBundle->aMem[i].eType,
                          pBundle->aMem[i].pvR0, pBundle->aMem[i].pvR3, pBundle->aMem[i].cb));
                switch (pBundle->aMem[i].eType)
                {
                    case MEMREF_TYPE_LOCKED:
                        supdrvOSUnlockMemOne(&pBundle->aMem[i]);
                        break;
                    case MEMREF_TYPE_CONT:
                        supdrvOSContFreeOne(&pBundle->aMem[i]);
                        break;
                    case MEMREF_TYPE_LOW:
                        supdrvOSLowFreeOne(&pBundle->aMem[i]);
                        break;
                    case MEMREF_TYPE_MEM:
                        supdrvOSMemFreeOne(&pBundle->aMem[i]);
                        break;
                    default:
                        break;
                }
                pBundle->aMem[i].eType = MEMREF_TYPE_UNUSED;
            }
#endif /* !USE_NEW_OS_INTERFACE */
        }

        /*
         * Advance and free previous bundle.
         */
        pToFree = pBundle;
        pBundle = pBundle->pNext;

        pToFree->pNext = NULL;
        pToFree->cUsed = 0;
        if (pToFree != &pSession->Bundle)
            RTMemFree(pToFree);
    }
    dprintf2(("freeing memory - done\n"));

    /*
     * Loaded images needs to be dereferenced and possibly freed up.
     */
    RTSemFastMutexRequest(pDevExt->mtxLdr);
    dprintf2(("freeing images:\n"));
    if (pSession->pLdrUsage)
    {
        PSUPDRVLDRUSAGE pUsage = pSession->pLdrUsage;
        pSession->pLdrUsage = NULL;
        while (pUsage)
        {
            void           *pvFree = pUsage;
            PSUPDRVLDRIMAGE pImage = pUsage->pImage;
            if (pImage->cUsage > pUsage->cUsage)
                pImage->cUsage -= pUsage->cUsage;
            else
                supdrvLdrFree(pDevExt, pImage);
            pUsage->pImage = NULL;
            pUsage = pUsage->pNext;
            RTMemFree(pvFree);
        }
    }
    RTSemFastMutexRelease(pDevExt->mtxLdr);
    dprintf2(("freeing images - done\n"));

    /*
     * Unmap the GIP.
     */
    dprintf2(("umapping GIP:\n"));
#ifdef USE_NEW_OS_INTERFACE
    if (pSession->GipMapObjR3 != NIL_RTR0MEMOBJ)
#else
    if (pSession->pGip)
#endif
    {
        SUPR0GipUnmap(pSession);
#ifndef USE_NEW_OS_INTERFACE
        pSession->pGip = NULL;
#endif
        pSession->fGipReferenced = 0;
    }
    dprintf2(("umapping GIP - done\n"));
}


#ifdef VBOX_WITHOUT_IDT_PATCHING
/**
 * Fast path I/O Control worker.
 *
 * @returns 0 on success.
 * @returns One of the SUPDRV_ERR_* on failure.
 * @param   uIOCtl      Function number.
 * @param   pDevExt     Device extention.
 * @param   pSession    Session data.
 */
int  VBOXCALL   supdrvIOCtlFast(unsigned uIOCtl, PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession)
{
    /*
     * Disable interrupts before invoking VMMR0Entry() because it ASSUMES
     * that interrupts are disabled. (We check the two prereqs after doing
     * this only to allow the compiler to optimize things better.)
     */
    int         rc;
    RTCCUINTREG uFlags = ASMGetFlags();
    ASMIntDisable();

    if (RT_LIKELY(pSession->pVM && pDevExt->pfnVMMR0Entry))
    {
        switch (uIOCtl)
        {
            case SUP_IOCTL_FAST_DO_RAW_RUN:
                rc = pDevExt->pfnVMMR0Entry(pSession->pVM, VMMR0_DO_RAW_RUN, NULL);
                break;
            case SUP_IOCTL_FAST_DO_HWACC_RUN:
                rc = pDevExt->pfnVMMR0Entry(pSession->pVM, VMMR0_DO_HWACC_RUN, NULL);
                break;
            case SUP_IOCTL_FAST_DO_NOP:
                rc = pDevExt->pfnVMMR0Entry(pSession->pVM, VMMR0_DO_NOP, NULL);
                break;
            default:
                rc = VERR_INTERNAL_ERROR;
                break;
        }
    }
    else
        rc = VERR_INTERNAL_ERROR;

    ASMSetFlags(uFlags);
    return rc;
}
#endif /* VBOX_WITHOUT_IDT_PATCHING */


/**
 * I/O Control worker.
 *
 * @returns 0 on success.
 * @returns One of the SUPDRV_ERR_* on failure.
 * @param   uIOCtl      Function number.
 * @param   pDevExt     Device extention.
 * @param   pSession    Session data.
 * @param   pvIn        Input data.
 * @param   cbIn        Size of input data.
 * @param   pvOut       Output data.
 *                      IMPORTANT! This buffer may be shared with the input
 *                                 data, thus no writing before done reading
 *                                 input data!!!
 * @param   cbOut       Size of output data.
 * @param   pcbReturned Size of the returned data.
 */
int VBOXCALL supdrvIOCtl(unsigned int uIOCtl, PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession,
                         void *pvIn, unsigned cbIn, void *pvOut, unsigned cbOut, unsigned *pcbReturned)
{
    *pcbReturned = 0;
    switch (uIOCtl)
    {
        case SUP_IOCTL_COOKIE:
        {
            PSUPCOOKIE_IN  pIn = (PSUPCOOKIE_IN)pvIn;
            PSUPCOOKIE_OUT pOut = (PSUPCOOKIE_OUT)pvOut;

            /*
             * Validate.
             */
            if (    cbIn != sizeof(*pIn)
                ||  cbOut != sizeof(*pOut))
            {
                OSDBGPRINT(("SUP_IOCTL_COOKIE: Invalid input/output sizes. cbIn=%ld expected %ld. cbOut=%ld expected %ld.\n",
                            (long)cbIn, (long)sizeof(*pIn), (long)cbOut, (long)sizeof(*pOut)));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (strncmp(pIn->szMagic, SUPCOOKIE_MAGIC, sizeof(pIn->szMagic)))
            {
                OSDBGPRINT(("SUP_IOCTL_COOKIE: invalid magic %.16s\n", pIn->szMagic));
                return SUPDRV_ERR_INVALID_MAGIC;
            }

            /*
             * Match the version.
             * The current logic is very simple, match the major interface version.
             */
            if (    pIn->u32MinVersion > SUPDRVIOC_VERSION
                ||  (pIn->u32MinVersion & 0xffff0000) != (SUPDRVIOC_VERSION & 0xffff0000))
            {
                OSDBGPRINT(("SUP_IOCTL_COOKIE: Version mismatch. Requested: %#x  Min: %#x  Current: %#x\n", 
                            pIn->u32ReqVersion, pIn->u32MinVersion, SUPDRVIOC_VERSION));
                pOut->u32Cookie         = 0xffffffff;
                pOut->u32SessionCookie  = 0xffffffff;
                pOut->u32SessionVersion = 0xffffffff;
                pOut->u32DriverVersion  = SUPDRVIOC_VERSION;
                pOut->pSession          = NULL;
                pOut->cFunctions        = 0;
                *pcbReturned = sizeof(*pOut);
                return SUPDRV_ERR_VERSION_MISMATCH;
            }

            /*
             * Fill in return data and be gone.
             * N.B. The first one to change SUPDRVIOC_VERSION shall makes sure that 
             *      u32SessionVersion <= u32ReqVersion!
             */
            /** @todo A more secure cookie negotiation? */
            pOut->u32Cookie         = pDevExt->u32Cookie;
            pOut->u32SessionCookie  = pSession->u32Cookie;
            pOut->u32SessionVersion = SUPDRVIOC_VERSION;
            pOut->u32DriverVersion  = SUPDRVIOC_VERSION; 
            pOut->pSession          = pSession;
            pOut->cFunctions        = sizeof(g_aFunctions) / sizeof(g_aFunctions[0]);
            *pcbReturned = sizeof(*pOut);
            return 0;
        }


        case SUP_IOCTL_QUERY_FUNCS:
        {
            unsigned            cFunctions;
            PSUPQUERYFUNCS_IN   pIn = (PSUPQUERYFUNCS_IN)pvIn;
            PSUPQUERYFUNCS_OUT  pOut = (PSUPQUERYFUNCS_OUT)pvOut;

            /*
             * Validate.
             */
            if (    cbIn != sizeof(*pIn)
                ||  cbOut < sizeof(*pOut))
            {
                dprintf(("SUP_IOCTL_QUERY_FUNCS: Invalid input/output sizes. cbIn=%ld expected %ld. cbOut=%ld expected %ld.\n",
                         (long)cbIn, (long)sizeof(*pIn), (long)cbOut, (long)sizeof(*pOut)));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (    pIn->u32Cookie != pDevExt->u32Cookie
                ||  pIn->u32SessionCookie != pSession->u32Cookie )
            {
                dprintf(("SUP_IOCTL_QUERY_FUNCS: Cookie mismatch {%#x,%#x} != {%#x,%#x}!\n",
                         pIn->u32Cookie, pDevExt->u32Cookie, pIn->u32SessionCookie, pSession->u32Cookie));
                return SUPDRV_ERR_INVALID_MAGIC;
            }

            /*
             * Copy the functions.
             */
            cFunctions = (cbOut - RT_OFFSETOF(SUPQUERYFUNCS_OUT, aFunctions)) / sizeof(pOut->aFunctions[0]);
            cFunctions = RT_MIN(cFunctions, ELEMENTS(g_aFunctions));
            AssertMsg(cFunctions == ELEMENTS(g_aFunctions),
                      ("Why aren't R3 querying all the functions!?! cFunctions=%d while there are %d available\n",
                       cFunctions, ELEMENTS(g_aFunctions)));
            pOut->cFunctions = cFunctions;
            memcpy(&pOut->aFunctions[0], g_aFunctions, sizeof(pOut->aFunctions[0]) * cFunctions);
            *pcbReturned = RT_OFFSETOF(SUPQUERYFUNCS_OUT, aFunctions[cFunctions]);
            return 0;
        }


        case SUP_IOCTL_IDT_INSTALL:
        {
            PSUPIDTINSTALL_IN   pIn = (PSUPIDTINSTALL_IN)pvIn;
            PSUPIDTINSTALL_OUT  pOut = (PSUPIDTINSTALL_OUT)pvOut;

            /*
             * Validate.
             */
            if (    cbIn != sizeof(*pIn)
                ||  cbOut != sizeof(*pOut))
            {
                dprintf(("SUP_IOCTL_INSTALL: Invalid input/output sizes. cbIn=%ld expected %ld. cbOut=%ld expected %ld.\n",
                         (long)cbIn, (long)sizeof(*pIn), (long)cbOut, (long)sizeof(*pOut)));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (    pIn->u32Cookie != pDevExt->u32Cookie
                ||  pIn->u32SessionCookie != pSession->u32Cookie )
            {
                dprintf(("SUP_IOCTL_INSTALL: Cookie mismatch {%#x,%#x} != {%#x,%#x}!\n",
                         pIn->u32Cookie,  pDevExt->u32Cookie,
                         pIn->u32SessionCookie, pSession->u32Cookie));
                return SUPDRV_ERR_INVALID_MAGIC;
            }

            *pcbReturned = sizeof(*pOut);
#ifndef VBOX_WITHOUT_IDT_PATCHING
            return supdrvIOCtl_IdtInstall(pDevExt, pSession, pIn, pOut);
#else
            pOut->u8Idt = 3;
            return 0;
#endif
        }


        case SUP_IOCTL_IDT_REMOVE:
        {
            PSUPIDTREMOVE_IN   pIn = (PSUPIDTREMOVE_IN)pvIn;

            /*
             * Validate.
             */
            if (    cbIn != sizeof(*pIn)
                ||  cbOut != 0)
            {
                dprintf(("SUP_IOCTL_REMOVE: Invalid input/output sizes. cbIn=%ld expected %ld. cbOut=%ld expected %ld.\n",
                         (long)cbIn, (long)sizeof(*pIn), (long)cbOut, (long)0));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (    pIn->u32Cookie != pDevExt->u32Cookie
                ||  pIn->u32SessionCookie != pSession->u32Cookie )
            {
                dprintf(("SUP_IOCTL_REMOVE: Cookie mismatch {%#x,%#x} != {%#x,%#x}!\n",
                         pIn->u32Cookie, pDevExt->u32Cookie, pIn->u32SessionCookie, pSession->u32Cookie));
                return SUPDRV_ERR_INVALID_MAGIC;
            }

#ifndef VBOX_WITHOUT_IDT_PATCHING
            return supdrvIOCtl_IdtRemoveAll(pDevExt, pSession);
#else
            return 0;
#endif
        }


        case SUP_IOCTL_PINPAGES:
        {
            int                 rc;
            PSUPPINPAGES_IN     pIn = (PSUPPINPAGES_IN)pvIn;
            PSUPPINPAGES_OUT    pOut = (PSUPPINPAGES_OUT)pvOut;

            /*
             * Validate.
             */
            if (    cbIn != sizeof(*pIn)
                ||  cbOut < sizeof(*pOut))
            {
                dprintf(("SUP_IOCTL_PINPAGES: Invalid input/output sizes. cbIn=%ld expected %ld. cbOut=%ld expected %ld.\n",
                         (long)cbIn, (long)sizeof(*pIn), (long)cbOut, (long)sizeof(*pOut)));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (    pIn->u32Cookie != pDevExt->u32Cookie
                ||  pIn->u32SessionCookie != pSession->u32Cookie )
            {
                dprintf(("SUP_IOCTL_PINPAGES: Cookie mismatch {%#x,%#x} != {%#x,%#x}!\n",
                         pIn->u32Cookie,  pDevExt->u32Cookie, pIn->u32SessionCookie, pSession->u32Cookie));
                return SUPDRV_ERR_INVALID_MAGIC;
            }
            if (pIn->cPages <= 0 || !pIn->pvR3)
            {
                dprintf(("SUP_IOCTL_PINPAGES: Illegal request %p %d\n", (void *)pIn->pvR3, pIn->cPages));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if ((unsigned)RT_OFFSETOF(SUPPINPAGES_OUT, aPages[pIn->cPages]) > cbOut)
            {
                dprintf(("SUP_IOCTL_PINPAGES: Output buffer is too small! %d required %d passed in.\n",
                         RT_OFFSETOF(SUPPINPAGES_OUT, aPages[pIn->cPages]), cbOut));
                return SUPDRV_ERR_INVALID_PARAM;
            }

            /*
             * Execute.
             */
            *pcbReturned = RT_OFFSETOF(SUPPINPAGES_OUT, aPages[pIn->cPages]);
            rc = SUPR0LockMem(pSession, pIn->pvR3, pIn->cPages, &pOut->aPages[0]);
            if (rc)
                *pcbReturned = 0;
            return rc;
        }


        case SUP_IOCTL_UNPINPAGES:
        {
            PSUPUNPINPAGES_IN   pIn = (PSUPUNPINPAGES_IN)pvIn;

            /*
             * Validate.
             */
            if (    cbIn != sizeof(*pIn)
                ||  cbOut != 0)
            {
                dprintf(("SUP_IOCTL_UNPINPAGES: Invalid input/output sizes. cbIn=%ld expected %ld. cbOut=%ld expected %ld.\n",
                         (long)cbIn, (long)sizeof(*pIn), (long)cbOut, (long)0));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (    pIn->u32Cookie != pDevExt->u32Cookie
                ||  pIn->u32SessionCookie != pSession->u32Cookie)
            {
                dprintf(("SUP_IOCTL_UNPINPAGES: Cookie mismatch {%#x,%#x} != {%#x,%#x}!\n",
                         pIn->u32Cookie, pDevExt->u32Cookie, pIn->u32SessionCookie, pSession->u32Cookie));
                return SUPDRV_ERR_INVALID_MAGIC;
            }

            /*
             * Execute.
             */
            return SUPR0UnlockMem(pSession, pIn->pvR3);
        }

        case SUP_IOCTL_CONT_ALLOC:
        {
            int                 rc;
            PSUPCONTALLOC_IN    pIn = (PSUPCONTALLOC_IN)pvIn;
            PSUPCONTALLOC_OUT   pOut = (PSUPCONTALLOC_OUT)pvOut;

            /*
             * Validate.
             */
            if (    cbIn != sizeof(*pIn)
                ||  cbOut < sizeof(*pOut))
            {
                dprintf(("SUP_IOCTL_CONT_ALLOC: Invalid input/output sizes. cbIn=%ld expected %ld. cbOut=%ld expected %ld.\n",
                         (long)cbIn, (long)sizeof(*pIn), (long)cbOut, (long)sizeof(*pOut)));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (    pIn->u32Cookie != pDevExt->u32Cookie
                ||  pIn->u32SessionCookie != pSession->u32Cookie )
            {
                dprintf(("SUP_IOCTL_CONT_ALLOC: Cookie mismatch {%#x,%#x} != {%#x,%#x}!\n",
                         pIn->u32Cookie, pDevExt->u32Cookie, pIn->u32SessionCookie, pSession->u32Cookie));
                return SUPDRV_ERR_INVALID_MAGIC;
            }

            /*
             * Execute.
             */
            rc = SUPR0ContAlloc(pSession, pIn->cPages, &pOut->pvR0, &pOut->pvR3, &pOut->HCPhys);
            if (!rc)
                *pcbReturned = sizeof(*pOut);
            return rc;
        }


        case SUP_IOCTL_CONT_FREE:
        {
            PSUPCONTFREE_IN   pIn = (PSUPCONTFREE_IN)pvIn;

            /*
             * Validate.
             */
            if (    cbIn != sizeof(*pIn)
                ||  cbOut != 0)
            {
                dprintf(("SUP_IOCTL_CONT_FREE: Invalid input/output sizes. cbIn=%ld expected %ld. cbOut=%ld expected %ld.\n",
                         (long)cbIn, (long)sizeof(*pIn), (long)cbOut, (long)0));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (    pIn->u32Cookie != pDevExt->u32Cookie
                ||  pIn->u32SessionCookie != pSession->u32Cookie)
            {
                dprintf(("SUP_IOCTL_CONT_FREE: Cookie mismatch {%#x,%#x} != {%#x,%#x}!\n",
                         pIn->u32Cookie, pDevExt->u32Cookie, pIn->u32SessionCookie, pSession->u32Cookie));
                return SUPDRV_ERR_INVALID_MAGIC;
            }

            /*
             * Execute.
             */
            return SUPR0ContFree(pSession, (RTHCUINTPTR)pIn->pvR3);
        }


        case SUP_IOCTL_LDR_OPEN:
        {
            PSUPLDROPEN_IN  pIn = (PSUPLDROPEN_IN)pvIn;
            PSUPLDROPEN_OUT pOut = (PSUPLDROPEN_OUT)pvOut;

            /*
             * Validate.
             */
            if (    cbIn != sizeof(*pIn)
                ||  cbOut != sizeof(*pOut))
            {
                dprintf(("SUP_IOCTL_LDR_OPEN: Invalid input/output sizes. cbIn=%ld expected %ld. cbOut=%ld expected %ld.\n",
                         (long)cbIn, (long)sizeof(*pIn), (long)cbOut, (long)sizeof(*pOut)));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (    pIn->u32Cookie != pDevExt->u32Cookie
                ||  pIn->u32SessionCookie != pSession->u32Cookie)
            {
                dprintf(("SUP_IOCTL_LDR_OPEN: Cookie mismatch {%#x,%#x} != {%#x,%#x}!\n",
                         pIn->u32Cookie, pDevExt->u32Cookie, pIn->u32SessionCookie, pSession->u32Cookie));
                return SUPDRV_ERR_INVALID_MAGIC;
            }
            if (    pIn->cbImage <= 0
                ||  pIn->cbImage >= 16*1024*1024 /*16MB*/)
            {
                dprintf(("SUP_IOCTL_LDR_OPEN: Invalid size %d. (max is 16MB)\n", pIn->cbImage));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (!memchr(pIn->szName, '\0', sizeof(pIn->szName)))
            {
                dprintf(("SUP_IOCTL_LDR_OPEN: The image name isn't terminated!\n"));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (!pIn->szName[0])
            {
                dprintf(("SUP_IOCTL_LDR_OPEN: The image name is too short\n"));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (strpbrk(pIn->szName, ";:()[]{}/\\|&*%#@!~`\"'"))
            {
                dprintf(("SUP_IOCTL_LDR_OPEN: The name is invalid '%s'\n", pIn->szName));
                return SUPDRV_ERR_INVALID_PARAM;
            }

            *pcbReturned = sizeof(*pOut);
            return supdrvIOCtl_LdrOpen(pDevExt, pSession, pIn, pOut);
        }


        case SUP_IOCTL_LDR_LOAD:
        {
            PSUPLDRLOAD_IN  pIn = (PSUPLDRLOAD_IN)pvIn;

            /*
             * Validate.
             */
            if (    cbIn <= sizeof(*pIn)
                ||  cbOut != 0)
            {
                dprintf(("SUP_IOCTL_LDR_LOAD: Invalid input/output sizes. cbIn=%ld expected greater than %ld. cbOut=%ld expected %ld.\n",
                         (long)cbIn, (long)sizeof(*pIn), (long)cbOut, (long)0));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (    pIn->u32Cookie != pDevExt->u32Cookie
                ||  pIn->u32SessionCookie != pSession->u32Cookie)
            {
                dprintf(("SUP_IOCTL_LDR_LOAD: Cookie mismatch {%#x,%#x} != {%#x,%#x}!\n",
                         pIn->u32Cookie, pDevExt->u32Cookie, pIn->u32SessionCookie, pSession->u32Cookie));
                return SUPDRV_ERR_INVALID_MAGIC;
            }
            if ((unsigned)RT_OFFSETOF(SUPLDRLOAD_IN, achImage[pIn->cbImage]) > cbIn)
            {
                dprintf(("SUP_IOCTL_LDR_LOAD: Invalid size %d. InputBufferLength=%d\n",
                         pIn->cbImage, cbIn));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (pIn->cSymbols > 16384)
            {
                dprintf(("SUP_IOCTL_LDR_LOAD: Too many symbols. cSymbols=%u max=16384\n", pIn->cSymbols));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (    pIn->cSymbols
                &&  (   pIn->offSymbols >= pIn->cbImage
                     || pIn->offSymbols + pIn->cSymbols * sizeof(SUPLDRSYM) > pIn->cbImage)
               )
            {
                dprintf(("SUP_IOCTL_LDR_LOAD: symbol table is outside the image bits! offSymbols=%u cSymbols=%d cbImage=%d\n",
                         pIn->offSymbols, pIn->cSymbols, pIn->cbImage));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (    pIn->cbStrTab
                &&  (   pIn->offStrTab >= pIn->cbImage
                     || pIn->offStrTab + pIn->cbStrTab > pIn->cbImage
                     || pIn->offStrTab + pIn->cbStrTab < pIn->offStrTab)
               )
            {
                dprintf(("SUP_IOCTL_LDR_LOAD: string table is outside the image bits! offStrTab=%u cbStrTab=%d cbImage=%d\n",
                         pIn->offStrTab, pIn->cbStrTab, pIn->cbImage));
                return SUPDRV_ERR_INVALID_PARAM;
            }

            if (pIn->cSymbols)
            {
                uint32_t    i;
                PSUPLDRSYM  paSyms = (PSUPLDRSYM)&pIn->achImage[pIn->offSymbols];
                for (i = 0; i < pIn->cSymbols; i++)
                {
                    if (paSyms[i].offSymbol >= pIn->cbImage)
                    {
                        dprintf(("SUP_IOCTL_LDR_LOAD: symbol i=%d has an invalid symbol offset: %#x (max=%#x)\n",
                                 i, paSyms[i].offSymbol, pIn->cbImage));
                        return SUPDRV_ERR_INVALID_PARAM;
                    }
                    if (paSyms[i].offName >= pIn->cbStrTab)
                    {
                        dprintf(("SUP_IOCTL_LDR_LOAD: symbol i=%d has an invalid name offset: %#x (max=%#x)\n",
                                 i, paSyms[i].offName, pIn->cbStrTab));
                        return SUPDRV_ERR_INVALID_PARAM;
                    }
                    if (!memchr(&pIn->achImage[pIn->offStrTab + paSyms[i].offName], '\0', pIn->cbStrTab - paSyms[i].offName))
                    {
                        dprintf(("SUP_IOCTL_LDR_LOAD: symbol i=%d has an unterminated name! offName=%#x (max=%#x)\n",
                                 i, paSyms[i].offName, pIn->cbStrTab));
                        return SUPDRV_ERR_INVALID_PARAM;
                    }
                }
            }

            return supdrvIOCtl_LdrLoad(pDevExt, pSession, pIn);
        }


        case SUP_IOCTL_LDR_FREE:
        {
            PSUPLDRFREE_IN  pIn = (PSUPLDRFREE_IN)pvIn;

            /*
             * Validate.
             */
            if (    cbIn != sizeof(*pIn)
                ||  cbOut != 0)
            {
                dprintf(("SUP_IOCTL_LDR_FREE: Invalid input/output sizes. cbIn=%ld expected %ld. cbOut=%ld expected %ld.\n",
                         (long)cbIn, (long)sizeof(*pIn), (long)cbOut, (long)0));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (    pIn->u32Cookie != pDevExt->u32Cookie
                ||  pIn->u32SessionCookie != pSession->u32Cookie)
            {
                dprintf(("SUP_IOCTL_LDR_FREE: Cookie mismatch {%#x,%#x} != {%#x,%#x}!\n",
                         pIn->u32Cookie, pDevExt->u32Cookie, pIn->u32SessionCookie, pSession->u32Cookie));
                return SUPDRV_ERR_INVALID_MAGIC;
            }

            return supdrvIOCtl_LdrFree(pDevExt, pSession, pIn);
        }


        case SUP_IOCTL_LDR_GET_SYMBOL:
        {
            PSUPLDRGETSYMBOL_IN  pIn  = (PSUPLDRGETSYMBOL_IN)pvIn;
            PSUPLDRGETSYMBOL_OUT pOut = (PSUPLDRGETSYMBOL_OUT)pvOut;
            char                *pszEnd;

            /*
             * Validate.
             */
            if (    cbIn < (unsigned)RT_OFFSETOF(SUPLDRGETSYMBOL_IN, szSymbol[2])
                ||  cbOut != sizeof(*pOut))
            {
                dprintf(("SUP_IOCTL_LDR_GET_SYMBOL: Invalid input/output sizes. cbIn=%d expected >=%d. cbOut=%d expected at%d.\n",
                         cbIn, RT_OFFSETOF(SUPLDRGETSYMBOL_IN, szSymbol[2]), cbOut, 0));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (    pIn->u32Cookie != pDevExt->u32Cookie
                ||  pIn->u32SessionCookie != pSession->u32Cookie)
            {
                dprintf(("SUP_IOCTL_LDR_GET_SYMBOL: Cookie mismatch {%#x,%#x} != {%#x,%#x}!\n",
                         pIn->u32Cookie, pDevExt->u32Cookie, pIn->u32SessionCookie, pSession->u32Cookie));
                return SUPDRV_ERR_INVALID_MAGIC;
            }
            pszEnd = memchr(pIn->szSymbol, '\0', cbIn - RT_OFFSETOF(SUPLDRGETSYMBOL_IN, szSymbol));
            if (!pszEnd)
            {
                dprintf(("SUP_IOCTL_LDR_GET_SYMBOL: The symbol name isn't terminated!\n"));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (pszEnd - &pIn->szSymbol[0] >= 1024)
            {
                dprintf(("SUP_IOCTL_LDR_GET_SYMBOL: The symbol name too long (%ld chars, max is %d)!\n",
                         (long)(pszEnd - &pIn->szSymbol[0]), 1024));
                return SUPDRV_ERR_INVALID_PARAM;
            }

            pOut->pvSymbol = NULL;
            *pcbReturned = sizeof(*pOut);
            return supdrvIOCtl_LdrGetSymbol(pDevExt, pSession, pIn, pOut);
        }


        /** @todo this interface needs re-doing, we're accessing Ring-3 buffers directly here! */
        case SUP_IOCTL_CALL_VMMR0:
        {
            PSUPCALLVMMR0_IN    pIn = (PSUPCALLVMMR0_IN)pvIn;
            PSUPCALLVMMR0_OUT   pOut = (PSUPCALLVMMR0_OUT)pvOut;

            /*
             * Validate.
             */
            if (    cbIn != sizeof(*pIn)
                ||  cbOut != sizeof(*pOut))
            {
                dprintf(("SUP_IOCTL_CALL_VMMR0: Invalid input/output sizes. cbIn=%ld expected %ld. cbOut=%ld expected %ld.\n",
                         (long)cbIn, (long)sizeof(*pIn), (long)cbOut, (long)sizeof(*pOut)));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (    pIn->u32Cookie != pDevExt->u32Cookie
                ||  pIn->u32SessionCookie != pSession->u32Cookie )
            {
                dprintf(("SUP_IOCTL_CALL_VMMR0: Cookie mismatch {%#x,%#x} != {%#x,%#x}!\n",
                         pIn->u32Cookie, pDevExt->u32Cookie, pIn->u32SessionCookie, pSession->u32Cookie));
                return SUPDRV_ERR_INVALID_MAGIC;
            }

            /*
             * Do we have an entrypoint?
             */
            if (!pDevExt->pfnVMMR0Entry)
                return SUPDRV_ERR_GENERAL_FAILURE;

            /*
             * Execute.
             */
            pOut->rc = pDevExt->pfnVMMR0Entry(pIn->pVMR0, pIn->uOperation, (void *)pIn->pvArg); /** @todo address the pvArg problem! */
            *pcbReturned = sizeof(*pOut);
            return 0;
        }


        case SUP_IOCTL_GET_PAGING_MODE:
        {
            int                     rc;
            PSUPGETPAGINGMODE_IN    pIn = (PSUPGETPAGINGMODE_IN)pvIn;
            PSUPGETPAGINGMODE_OUT   pOut = (PSUPGETPAGINGMODE_OUT)pvOut;

            /*
             * Validate.
             */
            if (    cbIn != sizeof(*pIn)
                ||  cbOut != sizeof(*pOut))
            {
                dprintf(("SUP_IOCTL_GET_PAGING_MODE: Invalid input/output sizes. cbIn=%ld expected %ld. cbOut=%ld expected %ld.\n",
                         (long)cbIn, (long)sizeof(*pIn), (long)cbOut, (long)sizeof(*pOut)));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (    pIn->u32Cookie != pDevExt->u32Cookie
                ||  pIn->u32SessionCookie != pSession->u32Cookie )
            {
                dprintf(("SUP_IOCTL_GET_PAGING_MODE: Cookie mismatch {%#x,%#x} != {%#x,%#x}!\n",
                         pIn->u32Cookie, pDevExt->u32Cookie, pIn->u32SessionCookie, pSession->u32Cookie));
                return SUPDRV_ERR_INVALID_MAGIC;
            }

            /*
             * Execute.
             */
            *pcbReturned = sizeof(*pOut);
            rc = supdrvIOCtl_GetPagingMode(pOut);
            if (rc)
                *pcbReturned = 0;
            return rc;
        }


        case SUP_IOCTL_LOW_ALLOC:
        {
            int                 rc;
            PSUPLOWALLOC_IN     pIn = (PSUPLOWALLOC_IN)pvIn;
            PSUPLOWALLOC_OUT    pOut = (PSUPLOWALLOC_OUT)pvOut;

            /*
             * Validate.
             */
            if (    cbIn != sizeof(*pIn)
                ||  cbOut < sizeof(*pOut))
            {
                dprintf(("SUP_IOCTL_LOW_ALLOC: Invalid input/output sizes. cbIn=%ld expected %ld. cbOut=%ld expected %ld.\n",
                         (long)cbIn, (long)sizeof(*pIn), (long)cbOut, (long)sizeof(*pOut)));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (    pIn->u32Cookie != pDevExt->u32Cookie
                ||  pIn->u32SessionCookie != pSession->u32Cookie )
            {
                dprintf(("SUP_IOCTL_LOW_ALLOC: Cookie mismatch {%#x,%#x} != {%#x,%#x}!\n",
                         pIn->u32Cookie,  pDevExt->u32Cookie, pIn->u32SessionCookie, pSession->u32Cookie));
                return SUPDRV_ERR_INVALID_MAGIC;
            }
            if ((unsigned)RT_OFFSETOF(SUPLOWALLOC_OUT, aPages[pIn->cPages]) > cbOut)
            {
                dprintf(("SUP_IOCTL_LOW_ALLOC: Output buffer is too small! %d required %d passed in.\n",
                         RT_OFFSETOF(SUPLOWALLOC_OUT, aPages[pIn->cPages]), cbOut));
                return SUPDRV_ERR_INVALID_PARAM;
            }

            /*
             * Execute.
             */
            *pcbReturned = RT_OFFSETOF(SUPLOWALLOC_OUT, aPages[pIn->cPages]);
            rc = SUPR0LowAlloc(pSession, pIn->cPages, &pOut->pvR0, &pOut->pvR3, &pOut->aPages[0]);
            if (rc)
                *pcbReturned = 0;
            return rc;
        }


        case SUP_IOCTL_LOW_FREE:
        {
            PSUPLOWFREE_IN  pIn = (PSUPLOWFREE_IN)pvIn;

            /*
             * Validate.
             */
            if (    cbIn != sizeof(*pIn)
                ||  cbOut != 0)
            {
                dprintf(("SUP_IOCTL_LOW_FREE: Invalid input/output sizes. cbIn=%ld expected %ld. cbOut=%ld expected %ld.\n",
                         (long)cbIn, (long)sizeof(*pIn), (long)cbOut, (long)0));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (    pIn->u32Cookie != pDevExt->u32Cookie
                ||  pIn->u32SessionCookie != pSession->u32Cookie)
            {
                dprintf(("SUP_IOCTL_LOW_FREE: Cookie mismatch {%#x,%#x} != {%#x,%#x}!\n",
                         pIn->u32Cookie, pDevExt->u32Cookie, pIn->u32SessionCookie, pSession->u32Cookie));
                return SUPDRV_ERR_INVALID_MAGIC;
            }

            /*
             * Execute.
             */
            return SUPR0LowFree(pSession, (RTHCUINTPTR)pIn->pvR3);
        }


        case SUP_IOCTL_GIP_MAP:
        {
            int             rc;
            PSUPGIPMAP_IN   pIn = (PSUPGIPMAP_IN)pvIn;
            PSUPGIPMAP_OUT  pOut = (PSUPGIPMAP_OUT)pvOut;

            /*
             * Validate.
             */
            if (    cbIn != sizeof(*pIn)
                ||  cbOut != sizeof(*pOut))
            {
                dprintf(("SUP_IOCTL_GIP_MAP: Invalid input/output sizes. cbIn=%ld expected %ld. cbOut=%ld expected %ld.\n",
                         (long)cbIn, (long)sizeof(*pIn), (long)cbOut, (long)0));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (    pIn->u32Cookie != pDevExt->u32Cookie
                ||  pIn->u32SessionCookie != pSession->u32Cookie)
            {
                dprintf(("SUP_IOCTL_GIP_MAP: Cookie mismatch {%#x,%#x} != {%#x,%#x}!\n",
                         pIn->u32Cookie, pDevExt->u32Cookie, pIn->u32SessionCookie, pSession->u32Cookie));
                return SUPDRV_ERR_INVALID_MAGIC;
            }

            /*
             * Execute.
             */
            rc = SUPR0GipMap(pSession, &pOut->pGipR3, &pOut->HCPhysGip);
            if (!rc)
            {
                pOut->pGipR0 = pDevExt->pGip;
                *pcbReturned = sizeof(*pOut);
            }
            return rc;
        }


        case SUP_IOCTL_GIP_UNMAP:
        {
            PSUPGIPUNMAP_IN pIn = (PSUPGIPUNMAP_IN)pvIn;

            /*
             * Validate.
             */
            if (    cbIn != sizeof(*pIn)
                ||  cbOut != 0)
            {
                dprintf(("SUP_IOCTL_GIP_UNMAP: Invalid input/output sizes. cbIn=%ld expected %ld. cbOut=%ld expected %ld.\n",
                         (long)cbIn, (long)sizeof(*pIn), (long)cbOut, (long)0));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (    pIn->u32Cookie != pDevExt->u32Cookie
                ||  pIn->u32SessionCookie != pSession->u32Cookie)
            {
                dprintf(("SUP_IOCTL_GIP_UNMAP: Cookie mismatch {%#x,%#x} != {%#x,%#x}!\n",
                         pIn->u32Cookie, pDevExt->u32Cookie, pIn->u32SessionCookie, pSession->u32Cookie));
                return SUPDRV_ERR_INVALID_MAGIC;
            }

            /*
             * Execute.
             */
            return SUPR0GipUnmap(pSession);
        }


        case SUP_IOCTL_SET_VM_FOR_FAST:
        {
            PSUPSETVMFORFAST_IN pIn = (PSUPSETVMFORFAST_IN)pvIn;

            /*
             * Validate.
             */
            if (    cbIn != sizeof(*pIn)
                ||  cbOut != 0)
            {
                dprintf(("SUP_IOCTL_SET_VM_FOR_FAST: Invalid input/output sizes. cbIn=%ld expected %ld. cbOut=%ld expected %ld.\n",
                         (long)cbIn, (long)sizeof(*pIn), (long)cbOut, (long)0));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if (    pIn->u32Cookie != pDevExt->u32Cookie
                ||  pIn->u32SessionCookie != pSession->u32Cookie)
            {
                dprintf(("SUP_IOCTL_SET_VM_FOR_FAST: Cookie mismatch {%#x,%#x} != {%#x,%#x}!\n",
                         pIn->u32Cookie, pDevExt->u32Cookie, pIn->u32SessionCookie, pSession->u32Cookie));
                return SUPDRV_ERR_INVALID_MAGIC;
            }
            if (    pIn->pVMR0 != NULL
                && (    !VALID_PTR(pIn->pVMR0)
                    ||  ((uintptr_t)pIn->pVMR0 & (PAGE_SIZE - 1))
                   )
                )
            {
                dprintf(("SUP_IOCTL_SET_VM_FOR_FAST: pVMR0=%p! Must be a valid, page aligned, pointer.\n", pIn->pVMR0));
                return SUPDRV_ERR_INVALID_POINTER;
            }

            /*
             * Execute.
             */
#ifndef VBOX_WITHOUT_IDT_PATCHING
            OSDBGPRINT(("SUP_IOCTL_SET_VM_FOR_FAST: !VBOX_WITHOUT_IDT_PATCHING\n"));
            return SUPDRV_ERR_GENERAL_FAILURE;
#else
            pSession->pVM = pIn->pVMR0;
            return 0;
#endif
        }


        default:
            dprintf(("Unknown IOCTL %#x\n", uIOCtl));
            break;
    }
    return SUPDRV_ERR_GENERAL_FAILURE;
}


/**
 * Register a object for reference counting.
 * The object is registered with one reference in the specified session.
 *
 * @returns Unique identifier on success (pointer).
 *          All future reference must use this identifier.
 * @returns NULL on failure.
 * @param   pfnDestructor   The destructore function which will be called when the reference count reaches 0.
 * @param   pvUser1         The first user argument.
 * @param   pvUser2         The second user argument.
 */
SUPR0DECL(void *) SUPR0ObjRegister(PSUPDRVSESSION pSession, SUPDRVOBJTYPE enmType, PFNSUPDRVDESTRUCTOR pfnDestructor, void *pvUser1, void *pvUser2)
{
    RTSPINLOCKTMP   SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;
    PSUPDRVDEVEXT   pDevExt     = pSession->pDevExt;
    PSUPDRVOBJ      pObj;
    PSUPDRVUSAGE    pUsage;

    /*
     * Validate the input.
     */
    if (!pSession)
    {
        AssertMsgFailed(("Invalid pSession=%p\n", pSession));
        return NULL;
    }
    if (    enmType <= SUPDRVOBJTYPE_INVALID
        ||  enmType >= SUPDRVOBJTYPE_END)
    {
        AssertMsgFailed(("Invalid enmType=%d\n", enmType));
        return NULL;
    }
    if (!pfnDestructor)
    {
        AssertMsgFailed(("Invalid pfnDestructor=%d\n", pfnDestructor));
        return NULL;
    }

    /*
     * Allocate and initialize the object.
     */
    pObj = (PSUPDRVOBJ)RTMemAlloc(sizeof(*pObj));
    if (!pObj)
        return NULL;
    pObj->u32Magic      = SUPDRVOBJ_MAGIC;
    pObj->enmType       = enmType;
    pObj->pNext         = NULL;
    pObj->cUsage        = 1;
    pObj->pfnDestructor = pfnDestructor;
    pObj->pvUser1       = pvUser1;
    pObj->pvUser2       = pvUser2;
    pObj->CreatorUid    = pSession->Uid;
    pObj->CreatorGid    = pSession->Gid;
    pObj->CreatorProcess= pSession->Process;
    supdrvOSObjInitCreator(pObj, pSession);

    /*
     * Allocate the usage record.
     * (We keep freed usage records around to simplity SUPR0ObjAddRef().)
     */
    RTSpinlockAcquire(pDevExt->Spinlock, &SpinlockTmp);

    pUsage = pDevExt->pUsageFree;
    if (pUsage)
        pDevExt->pUsageFree = pUsage->pNext;
    else
    {
        RTSpinlockRelease(pDevExt->Spinlock, &SpinlockTmp);
        pUsage = (PSUPDRVUSAGE)RTMemAlloc(sizeof(*pUsage));
        if (!pUsage)
        {
            RTMemFree(pObj);
            return NULL;
        }
        RTSpinlockAcquire(pDevExt->Spinlock, &SpinlockTmp);
    }

    /*
     * Insert the object and create the session usage record.
     */
    /* The object. */
    pObj->pNext         = pDevExt->pObjs;
    pDevExt->pObjs      = pObj;

    /* The session record. */
    pUsage->cUsage      = 1;
    pUsage->pObj        = pObj;
    pUsage->pNext       = pSession->pUsage;
    dprintf(("SUPR0ObjRegister: pUsage=%p:{.pObj=%p, .pNext=%p}\n", pUsage, pUsage->pObj, pUsage->pNext));
    pSession->pUsage    = pUsage;

    RTSpinlockRelease(pDevExt->Spinlock, &SpinlockTmp);

    dprintf(("SUPR0ObjRegister: returns %p (pvUser1=%p, pvUser=%p)\n", pObj, pvUser1, pvUser2));
    return pObj;
}


/**
 * Increment the reference counter for the object associating the reference
 * with the specified session.
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pvObj           The identifier returned by SUPR0ObjRegister().
 * @param   pSession        The session which is referencing the object.
 */
SUPR0DECL(int) SUPR0ObjAddRef(void *pvObj, PSUPDRVSESSION pSession)
{
    RTSPINLOCKTMP   SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;
    PSUPDRVDEVEXT   pDevExt     = pSession->pDevExt;
    PSUPDRVOBJ      pObj        = (PSUPDRVOBJ)pvObj;
    PSUPDRVUSAGE    pUsagePre;
    PSUPDRVUSAGE    pUsage;

    /*
     * Validate the input.
     */
    if (!pSession)
    {
        AssertMsgFailed(("Invalid pSession=%p\n", pSession));
        return SUPDRV_ERR_INVALID_PARAM;
    }
    if (!pObj || pObj->u32Magic != SUPDRVOBJ_MAGIC)
    {
        AssertMsgFailed(("Invalid pvObj=%p magic=%#x (exepcted %#x)\n",
                         pvObj, pObj ? pObj->u32Magic : 0, SUPDRVOBJ_MAGIC));
        return SUPDRV_ERR_INVALID_PARAM;
    }

    /*
     * Preallocate the usage record.
     */
    RTSpinlockAcquire(pDevExt->Spinlock, &SpinlockTmp);

    pUsagePre = pDevExt->pUsageFree;
    if (pUsagePre)
        pDevExt->pUsageFree = pUsagePre->pNext;
    else
    {
        RTSpinlockRelease(pDevExt->Spinlock, &SpinlockTmp);
        pUsagePre = (PSUPDRVUSAGE)RTMemAlloc(sizeof(*pUsagePre));
        if (!pUsagePre)
            return SUPDRV_ERR_NO_MEMORY;
        RTSpinlockAcquire(pDevExt->Spinlock, &SpinlockTmp);
    }

    /*
     * Reference the object.
     */
    pObj->cUsage++;

    /*
     * Look for the session record.
     */
    for (pUsage = pSession->pUsage; pUsage; pUsage = pUsage->pNext)
    {
        dprintf(("SUPR0AddRef: pUsage=%p:{.pObj=%p, .pNext=%p}\n", pUsage, pUsage->pObj, pUsage->pNext));
        if (pUsage->pObj == pObj)
            break;
    }
    if (pUsage)
        pUsage->cUsage++;
    else
    {
        /* create a new session record. */
        pUsagePre->cUsage   = 1;
        pUsagePre->pObj     = pObj;
        pUsagePre->pNext    = pSession->pUsage;
        pSession->pUsage    = pUsagePre;
        dprintf(("SUPR0ObjRelease: pUsagePre=%p:{.pObj=%p, .pNext=%p}\n", pUsagePre, pUsagePre->pObj, pUsagePre->pNext));

        pUsagePre = NULL;
    }

    /*
     * Put any unused usage record into the free list..
     */
    if (pUsagePre)
    {
        pUsagePre->pNext = pDevExt->pUsageFree;
        pDevExt->pUsageFree = pUsagePre;
    }

    RTSpinlockRelease(pDevExt->Spinlock, &SpinlockTmp);

    return 0;
}


/**
 * Decrement / destroy a reference counter record for an object.
 *
 * The object is uniquely identified by pfnDestructor+pvUser1+pvUser2.
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pvObj           The identifier returned by SUPR0ObjRegister().
 * @param   pSession        The session which is referencing the object.
 */
SUPR0DECL(int) SUPR0ObjRelease(void *pvObj, PSUPDRVSESSION pSession)
{
    RTSPINLOCKTMP       SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;
    PSUPDRVDEVEXT       pDevExt     = pSession->pDevExt;
    PSUPDRVOBJ          pObj        = (PSUPDRVOBJ)pvObj;
    bool                fDestroy    = false;
    PSUPDRVUSAGE        pUsage;
    PSUPDRVUSAGE        pUsagePrev;

    /*
     * Validate the input.
     */
    if (!pSession)
    {
        AssertMsgFailed(("Invalid pSession=%p\n", pSession));
        return SUPDRV_ERR_INVALID_PARAM;
    }
    if (!pObj || pObj->u32Magic != SUPDRVOBJ_MAGIC)
    {
        AssertMsgFailed(("Invalid pvObj=%p magic=%#x (exepcted %#x)\n",
                         pvObj, pObj ? pObj->u32Magic : 0, SUPDRVOBJ_MAGIC));
        return SUPDRV_ERR_INVALID_PARAM;
    }

    /*
     * Acquire the spinlock and look for the usage record.
     */
    RTSpinlockAcquire(pDevExt->Spinlock, &SpinlockTmp);

    for (pUsagePrev = NULL, pUsage = pSession->pUsage;
         pUsage;
         pUsagePrev = pUsage, pUsage = pUsage->pNext)
    {
        dprintf(("SUPR0ObjRelease: pUsage=%p:{.pObj=%p, .pNext=%p}\n", pUsage, pUsage->pObj, pUsage->pNext));
        if (pUsage->pObj == pObj)
        {
            AssertMsg(pUsage->cUsage >= 1 && pObj->cUsage >= pUsage->cUsage, ("glob %d; sess %d\n", pObj->cUsage, pUsage->cUsage));
            if (pUsage->cUsage > 1)
            {
                pObj->cUsage--;
                pUsage->cUsage--;
            }
            else
            {
                /*
                 * Free the session record.
                 */
                if (pUsagePrev)
                    pUsagePrev->pNext = pUsage->pNext;
                else
                    pSession->pUsage = pUsage->pNext;
                pUsage->pNext = pDevExt->pUsageFree;
                pDevExt->pUsageFree = pUsage;

                /* What about the object? */
                if (pObj->cUsage > 1)
                    pObj->cUsage--;
                else
                {
                    /*
                     * Object is to be destroyed, unlink it.
                     */
                    fDestroy = true;
                    if (pDevExt->pObjs == pObj)
                        pDevExt->pObjs = pObj->pNext;
                    else
                    {
                        PSUPDRVOBJ pObjPrev;
                        for (pObjPrev = pDevExt->pObjs; pObjPrev; pObjPrev = pObjPrev->pNext)
                            if (pObjPrev->pNext == pObj)
                            {
                                pObjPrev->pNext = pObj->pNext;
                                break;
                            }
                        Assert(pObjPrev);
                    }
                }
            }
            break;
        }
    }

    RTSpinlockRelease(pDevExt->Spinlock, &SpinlockTmp);

    /*
     * Call the destructor and free the object if required.
     */
    if (fDestroy)
    {
        pObj->u32Magic++;
        pObj->pfnDestructor(pObj, pObj->pvUser1, pObj->pvUser2);
        RTMemFree(pObj);
    }

    AssertMsg(pUsage, ("pvObj=%p\n", pvObj));
    return pUsage ? 0 : SUPDRV_ERR_INVALID_PARAM;
}

/**
 * Verifies that the current process can access the specified object.
 *
 * @returns 0 if access is granted.
 * @returns SUPDRV_ERR_PERMISSION_DENIED if denied access.
 * @returns SUPDRV_ERR_INVALID_PARAM if invalid parameter.
 *
 * @param   pvObj           The identifier returned by SUPR0ObjRegister().
 * @param   pSession        The session which wishes to access the object.
 * @param   pszObjName      Object string name. This is optional and depends on the object type.
 *
 * @remark  The caller is responsible for making sure the object isn't removed while
 *          we're inside this function. If uncertain about this, just call AddRef before calling us.
 */
SUPR0DECL(int) SUPR0ObjVerifyAccess(void *pvObj, PSUPDRVSESSION pSession, const char *pszObjName)
{
    PSUPDRVOBJ  pObj = (PSUPDRVOBJ)pvObj;
    int         rc   = SUPDRV_ERR_GENERAL_FAILURE;

    /*
     * Validate the input.
     */
    if (!pSession)
    {
        AssertMsgFailed(("Invalid pSession=%p\n", pSession));
        return SUPDRV_ERR_INVALID_PARAM;
    }
    if (!pObj || pObj->u32Magic != SUPDRVOBJ_MAGIC)
    {
        AssertMsgFailed(("Invalid pvObj=%p magic=%#x (exepcted %#x)\n",
                         pvObj, pObj ? pObj->u32Magic : 0, SUPDRVOBJ_MAGIC));
        return SUPDRV_ERR_INVALID_PARAM;
    }

    /*
     * Check access. (returns true if a decision has been made.)
     */
    if (supdrvOSObjCanAccess(pObj, pSession, pszObjName, &rc))
        return rc;

    /*
     * Default policy is to allow the user to access his own
     * stuff but nothing else.
     */
    if (pObj->CreatorUid == pSession->Uid)
        return 0;
    return SUPDRV_ERR_PERMISSION_DENIED;
}


/**
 * Lock pages.
 *
 * @param   pSession    Session to which the locked memory should be associated.
 * @param   pvR3        Start of the memory range to lock.
 *                      This must be page aligned.
 * @param   cb          Size of the memory range to lock.
 *                      This must be page aligned.
 */
SUPR0DECL(int) SUPR0LockMem(PSUPDRVSESSION pSession, RTR3PTR pvR3, uint32_t cPages, PSUPPAGE paPages)
{
    int             rc;
    SUPDRVMEMREF    Mem = {0};
    const size_t    cb = (size_t)cPages << PAGE_SHIFT;
    dprintf(("SUPR0LockMem: pSession=%p pvR3=%p cPages=%d paPages=%p\n",
             pSession, (void *)pvR3, cPages, paPages));

    /*
     * Verify input.
     */
    if (RT_ALIGN_R3PT(pvR3, PAGE_SIZE, RTR3PTR) != pvR3 || !pvR3)
    {
        dprintf(("pvR3 (%p) must be page aligned and not NULL!\n", (void *)pvR3));
        return SUPDRV_ERR_INVALID_PARAM;
    }
    if (!paPages)
    {
        dprintf(("paPages is NULL!\n"));
        return SUPDRV_ERR_INVALID_PARAM;
    }

#ifdef USE_NEW_OS_INTERFACE
    /*
     * Let IPRT do the job.
     */
    Mem.eType = MEMREF_TYPE_LOCKED;
    rc = RTR0MemObjLockUser(&Mem.MemObj, pvR3, cb, RTR0ProcHandleSelf());
    if (RT_SUCCESS(rc))
    {
        AssertMsg(RTR0MemObjAddress(Mem.MemObj) == pvR3, ("%p == %p\n", RTR0MemObjAddress(Mem.MemObj), pvR3));
        AssertMsg(RTR0MemObjSize(Mem.MemObj) == cb, ("%x == %x\n", RTR0MemObjSize(Mem.MemObj), cb));

        unsigned iPage = cPages;
        while (iPage-- > 0)
        {
            paPages[iPage].uReserved = 0;
            paPages[iPage].Phys = RTR0MemObjGetPagePhysAddr(Mem.MemObj, iPage);
            if (RT_UNLIKELY(paPages[iPage].Phys == NIL_RTCCPHYS))
            {
                AssertMsgFailed(("iPage=%d\n", iPage));
                rc = VERR_INTERNAL_ERROR;
                break;
            }
        }
        if (RT_SUCCESS(rc))
            rc = supdrvMemAdd(&Mem, pSession);
        if (RT_FAILURE(rc))
        {
            int rc2 = RTR0MemObjFree(Mem.MemObj, false);
            AssertRC(rc2);
        }
    }

#else /* !USE_NEW_OS_INTERFACE */

    /*
     * Let the OS specific code have a go.
     */
    Mem.pvR0    = NULL;
    Mem.pvR3    = pvR3;
    Mem.eType   = MEMREF_TYPE_LOCKED;
    Mem.cb      = cb;
    rc = supdrvOSLockMemOne(&Mem, paPages);
    if (rc)
        return rc;

    /*
     * Everything when fine, add the memory reference to the session.
     */
    rc = supdrvMemAdd(&Mem, pSession);
    if (rc)
        supdrvOSUnlockMemOne(&Mem);
#endif /* !USE_NEW_OS_INTERFACE */
    return rc;
}


/**
 * Unlocks the memory pointed to by pv.
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure
 * @param   pSession    Session to which the memory was locked.
 * @param   pvR3        Memory to unlock.
 */
SUPR0DECL(int) SUPR0UnlockMem(PSUPDRVSESSION pSession, RTR3PTR pvR3)
{
    dprintf(("SUPR0UnlockMem: pSession=%p pvR3=%p\n", pSession, (void *)pvR3));
    return supdrvMemRelease(pSession, (RTHCUINTPTR)pvR3, MEMREF_TYPE_LOCKED);
}


/**
 * Allocates a chunk of page aligned memory with contiguous and fixed physical
 * backing.
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pSession    Session data.
 * @param   cb          Number of bytes to allocate.
 * @param   ppvR0       Where to put the address of Ring-0 mapping the allocated memory.
 * @param   ppvR3       Where to put the address of Ring-3 mapping the allocated memory.
 * @param   pHCPhys     Where to put the physical address of allocated memory.
 */
SUPR0DECL(int) SUPR0ContAlloc(PSUPDRVSESSION pSession, uint32_t cPages, PRTR0PTR ppvR0, PRTR3PTR ppvR3, PRTHCPHYS pHCPhys)
{
    int             rc;
    SUPDRVMEMREF    Mem = {0};
    dprintf(("SUPR0ContAlloc: pSession=%p cPages=%d ppvR0=%p ppvR3=%p pHCPhys=%p\n", pSession, cPages, ppvR0, ppvR3, pHCPhys));

    /*
     * Validate input.
     */
    if (!pSession || !ppvR3 || !ppvR0 || !pHCPhys)
    {
        dprintf(("Null pointer. All of these should be set: pSession=%p ppvR0=%p ppvR3=%p pHCPhys=%p\n",
                 pSession, ppvR0, ppvR3, pHCPhys));
        return SUPDRV_ERR_INVALID_PARAM;

    }
    if (cPages == 0 || cPages >= 256)
    {
        dprintf(("Illegal request cPages=%d, must be greater than 0 and smaller than 256\n", cPages));
        return SUPDRV_ERR_INVALID_PARAM;
    }

#ifdef USE_NEW_OS_INTERFACE
    /*
     * Let IPRT do the job.
     */
    rc = RTR0MemObjAllocCont(&Mem.MemObj, cPages << PAGE_SHIFT, true /* executable R0 mapping */);
    if (RT_SUCCESS(rc))
    {
        int rc2;
        rc = RTR0MemObjMapUser(&Mem.MapObjR3, Mem.MemObj, (void *)-1, 0,
                               RTMEM_PROT_EXEC | RTMEM_PROT_WRITE | RTMEM_PROT_READ, RTR0ProcHandleSelf());
        if (RT_SUCCESS(rc))
        {
            Mem.eType = MEMREF_TYPE_CONT;
            rc = supdrvMemAdd(&Mem, pSession);
            if (!rc)
            {
                *ppvR0 = RTR0MemObjAddress(Mem.MemObj);
                *ppvR3 = (RTR3PTR)RTR0MemObjAddress(Mem.MapObjR3);
                *pHCPhys = RTR0MemObjGetPagePhysAddr(Mem.MemObj, 0);
                return 0;
            }

            rc2 = RTR0MemObjFree(Mem.MapObjR3, false);
            AssertRC(rc2);
        }
        rc2 = RTR0MemObjFree(Mem.MemObj, false);
        AssertRC(rc2);
    }

#else /* !USE_NEW_OS_INTERFACE */

    /*
     * Let the OS specific code have a go.
     */
    Mem.pvR0    = NULL;
    Mem.pvR3    = NIL_RTR3PTR;
    Mem.eType   = MEMREF_TYPE_CONT;
    Mem.cb      = cPages << PAGE_SHIFT;
    rc = supdrvOSContAllocOne(&Mem, ppvR0, ppvR3, pHCPhys);
    if (rc)
        return rc;
    AssertMsg(!((uintptr_t)*ppvR3 & (PAGE_SIZE - 1)) || !(*pHCPhys & (PAGE_SIZE - 1)),
              ("Memory is not page aligned! *ppvR0=%p *ppvR3=%p phys=%VHp\n", ppvR0 ? *ppvR0 : NULL, *ppvR3, *pHCPhys));

    /*
     * Everything when fine, add the memory reference to the session.
     */
    rc = supdrvMemAdd(&Mem, pSession);
    if (rc)
        supdrvOSContFreeOne(&Mem);
#endif /* !USE_NEW_OS_INTERFACE */

    return rc;
}


/**
 * Frees memory allocated using SUPR0ContAlloc().
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pSession    The session to which the memory was allocated.
 * @param   uPtr        Pointer to the memory (ring-3 or ring-0).
 */
SUPR0DECL(int) SUPR0ContFree(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr)
{
    dprintf(("SUPR0ContFree: pSession=%p uPtr=%p\n", pSession, (void *)uPtr));
    return supdrvMemRelease(pSession, uPtr, MEMREF_TYPE_CONT);
}


/**
 * Allocates a chunk of page aligned memory with fixed physical backing below 4GB.
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pSession    Session data.
 * @param   cPages      Number of pages to allocate.
 * @param   ppvR0       Where to put the address of Ring-0 mapping of the allocated memory.
 * @param   ppvR3       Where to put the address of Ring-3 mapping of the allocated memory.
 * @param   paPages     Where to put the physical addresses of allocated memory.
 */
SUPR0DECL(int) SUPR0LowAlloc(PSUPDRVSESSION pSession, uint32_t cPages, PRTR0PTR ppvR0, PRTR3PTR ppvR3, PSUPPAGE paPages)
{
    unsigned        iPage;
    int             rc;
    SUPDRVMEMREF    Mem = {0};
    dprintf(("SUPR0LowAlloc: pSession=%p cPages=%d ppvR3=%p ppvR0=%p paPages=%p\n", pSession, cPages, ppvR3, ppvR0, paPages));

    /*
     * Validate input.
     */
    if (!pSession || !ppvR3 || !ppvR0 || !paPages)
    {
        dprintf(("Null pointer. All of these should be set: pSession=%p ppvR3=%p ppvR0=%p paPages=%p\n",
                 pSession, ppvR3, ppvR0, paPages));
        return SUPDRV_ERR_INVALID_PARAM;

    }
    if (cPages < 1 || cPages > 256)
    {
        dprintf(("Illegal request cPages=%d, must be greater than 0 and smaller than 256.\n", cPages));
        return SUPDRV_ERR_INVALID_PARAM;
    }

#ifdef USE_NEW_OS_INTERFACE
    /*
     * Let IPRT do the work.
     */
    rc = RTR0MemObjAllocLow(&Mem.MemObj, cPages << PAGE_SHIFT, true /* executable ring-0 mapping */);
    if (RT_SUCCESS(rc))
    {
        int rc2;
        rc = RTR0MemObjMapUser(&Mem.MapObjR3, Mem.MemObj, (void *)-1, 0,
                               RTMEM_PROT_EXEC | RTMEM_PROT_WRITE | RTMEM_PROT_READ, RTR0ProcHandleSelf());
        if (RT_SUCCESS(rc))
        {
            Mem.eType = MEMREF_TYPE_LOW;
            rc = supdrvMemAdd(&Mem, pSession);
            if (!rc)
            {
                for (iPage = 0; iPage < cPages; iPage++)
                {
                    paPages[iPage].Phys = RTR0MemObjGetPagePhysAddr(Mem.MemObj, iPage);
                    paPages[iPage].uReserved = 0;
                    AssertMsg(!(paPages[iPage].Phys & (PAGE_SIZE - 1)), ("iPage=%d Phys=%VHp\n", paPages[iPage].Phys));
                }
                *ppvR0 = RTR0MemObjAddress(Mem.MemObj);
                *ppvR3 = RTR0MemObjAddress(Mem.MapObjR3);
                return 0;
            }

            rc2 = RTR0MemObjFree(Mem.MapObjR3, false);
            AssertRC(rc2);
        }

        rc2 = RTR0MemObjFree(Mem.MemObj, false);
        AssertRC(rc2);
    }

#else /* !USE_NEW_OS_INTERFACE */

    /*
     * Let the OS specific code have a go.
     */
    Mem.pvR0    = NULL;
    Mem.pvR3    = NIL_RTR3PTR;
    Mem.eType   = MEMREF_TYPE_LOW;
    Mem.cb      = cPages << PAGE_SHIFT;
    rc = supdrvOSLowAllocOne(&Mem, ppvR0, ppvR3, paPages);
    if (rc)
        return rc;
    AssertMsg(!((uintptr_t)*ppvR3 & (PAGE_SIZE - 1)), ("Memory is not page aligned! virt=%p\n", *ppvR3));
    AssertMsg(!((uintptr_t)*ppvR0 & (PAGE_SIZE - 1)), ("Memory is not page aligned! virt=%p\n", *ppvR0));
    for (iPage = 0; iPage < cPages; iPage++)
        AssertMsg(!(paPages[iPage].Phys & (PAGE_SIZE - 1)), ("iPage=%d Phys=%VHp\n", paPages[iPage].Phys));

    /*
     * Everything when fine, add the memory reference to the session.
     */
    rc = supdrvMemAdd(&Mem, pSession);
    if (rc)
        supdrvOSLowFreeOne(&Mem);
#endif /* !USE_NEW_OS_INTERFACE */
    return rc;
}


/**
 * Frees memory allocated using SUPR0LowAlloc().
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pSession    The session to which the memory was allocated.
 * @param   uPtr        Pointer to the memory (ring-3 or ring-0).
 */
SUPR0DECL(int) SUPR0LowFree(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr)
{
    dprintf(("SUPR0LowFree: pSession=%p uPtr=%p\n", pSession, (void *)uPtr));
    return supdrvMemRelease(pSession, uPtr, MEMREF_TYPE_LOW);
}


/**
 * Allocates a chunk of memory with both R0 and R3 mappings.
 * The memory is fixed and it's possible to query the physical addresses using SUPR0MemGetPhys().
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pSession    The session to associated the allocation with.
 * @param   cb          Number of bytes to allocate.
 * @param   ppvR0       Where to store the address of the Ring-0 mapping.
 * @param   ppvR3       Where to store the address of the Ring-3 mapping.
 */
SUPR0DECL(int) SUPR0MemAlloc(PSUPDRVSESSION pSession, uint32_t cb, PRTR0PTR ppvR0, PRTR3PTR ppvR3)
{
    int             rc;
    SUPDRVMEMREF    Mem = {0};
    dprintf(("SUPR0MemAlloc: pSession=%p cb=%d ppvR0=%p ppvR3=%p\n", pSession, cb, ppvR0, ppvR3));

    /*
     * Validate input.
     */
    if (!pSession || !ppvR0 || !ppvR3)
    {
        dprintf(("Null pointer. All of these should be set: pSession=%p ppvR0=%p ppvR3=%p\n",
                 pSession, ppvR0, ppvR3));
        return SUPDRV_ERR_INVALID_PARAM;

    }
    if (cb < 1 || cb >= PAGE_SIZE * 256)
    {
        dprintf(("Illegal request cb=%u; must be greater than 0 and smaller than 4MB.\n", cb));
        return SUPDRV_ERR_INVALID_PARAM;
    }

#ifdef USE_NEW_OS_INTERFACE
    /*
     * Let IPRT do the work.
     */
    rc = RTR0MemObjAllocPage(&Mem.MemObj, cb, true /* executable ring-0 mapping */);
    if (RT_SUCCESS(rc))
    {
        int rc2;
        rc = RTR0MemObjMapUser(&Mem.MapObjR3, Mem.MemObj, (void*)-1, 0,
                               RTMEM_PROT_EXEC | RTMEM_PROT_WRITE | RTMEM_PROT_READ, RTR0ProcHandleSelf());
        if (RT_SUCCESS(rc))
        {
            Mem.eType = MEMREF_TYPE_MEM;
            rc = supdrvMemAdd(&Mem, pSession);
            if (!rc)
            {
                *ppvR0 = RTR0MemObjAddress(Mem.MemObj);
                *ppvR3 = (RTR3PTR)RTR0MemObjAddress(Mem.MapObjR3);
                return 0;
            }
            rc2 = RTR0MemObjFree(Mem.MapObjR3, false);
            AssertRC(rc2);
        }

        rc2 = RTR0MemObjFree(Mem.MemObj, false);
        AssertRC(rc2);
    }

#else /* !USE_NEW_OS_INTERFACE */

    /*
     * Let the OS specific code have a go.
     */
    Mem.pvR0    = NULL;
    Mem.pvR3    = NIL_RTR3PTR;
    Mem.eType   = MEMREF_TYPE_MEM;
    Mem.cb      = cb;
    rc = supdrvOSMemAllocOne(&Mem, ppvR0, ppvR3);
    if (rc)
        return rc;
    AssertMsg(!((uintptr_t)*ppvR0 & (PAGE_SIZE - 1)), ("Memory is not page aligned! pvR0=%p\n", *ppvR0));
    AssertMsg(!((uintptr_t)*ppvR3 & (PAGE_SIZE - 1)), ("Memory is not page aligned! pvR3=%p\n", *ppvR3));

    /*
     * Everything when fine, add the memory reference to the session.
     */
    rc = supdrvMemAdd(&Mem, pSession);
    if (rc)
        supdrvOSMemFreeOne(&Mem);
#endif /* !USE_NEW_OS_INTERFACE */
    return rc;
}


/**
 * Get the physical addresses of memory allocated using SUPR0MemAlloc().
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pSession        The session to which the memory was allocated.
 * @param   uPtr            The Ring-0 or Ring-3 address returned by SUPR0MemAlloc().
 * @param   paPages         Where to store the physical addresses.
 */
SUPR0DECL(int) SUPR0MemGetPhys(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr, PSUPPAGE paPages)
{
    PSUPDRVBUNDLE pBundle;
    RTSPINLOCKTMP SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;
    dprintf(("SUPR0MemGetPhys: pSession=%p uPtr=%p paPages=%p\n", pSession, (void *)uPtr, paPages));

    /*
     * Validate input.
     */
    if (!pSession)
    {
        dprintf(("pSession must not be NULL!"));
        return SUPDRV_ERR_INVALID_PARAM;
    }
    if (!uPtr || !paPages)
    {
        dprintf(("Illegal address uPtr=%p or/and paPages=%p\n", (void *)uPtr, paPages));
        return SUPDRV_ERR_INVALID_PARAM;
    }

    /*
     * Search for the address.
     */
    RTSpinlockAcquire(pSession->Spinlock, &SpinlockTmp);
    for (pBundle = &pSession->Bundle; pBundle; pBundle = pBundle->pNext)
    {
        if (pBundle->cUsed > 0)
        {
            unsigned i;
            for (i = 0; i < sizeof(pBundle->aMem) / sizeof(pBundle->aMem[0]); i++)
            {
#ifdef USE_NEW_OS_INTERFACE
                if (    pBundle->aMem[i].eType == MEMREF_TYPE_MEM
                    &&  pBundle->aMem[i].MemObj != NIL_RTR0MEMOBJ
                    &&  (   (RTHCUINTPTR)RTR0MemObjAddress(pBundle->aMem[i].MemObj) == uPtr
                         || (   pBundle->aMem[i].MapObjR3 != NIL_RTR0MEMOBJ
                             && (RTHCUINTPTR)RTR0MemObjAddress(pBundle->aMem[i].MapObjR3) == uPtr)
                        )
                   )
                {
                    const unsigned cPages = RTR0MemObjSize(pBundle->aMem[i].MemObj) >> PAGE_SHIFT;
                    unsigned iPage;
                    for (iPage = 0; iPage < cPages; iPage++)
                    {
                        paPages[iPage].Phys = RTR0MemObjGetPagePhysAddr(pBundle->aMem[i].MemObj, iPage);
                        paPages[iPage].uReserved = 0;
                    }
                    RTSpinlockRelease(pSession->Spinlock, &SpinlockTmp);
                    return 0;
                }
#else /* !USE_NEW_OS_INTERFACE */
                if (    pBundle->aMem[i].eType == MEMREF_TYPE_MEM
                    &&  (   (RTHCUINTPTR)pBundle->aMem[i].pvR0 == uPtr
                         || (RTHCUINTPTR)pBundle->aMem[i].pvR3 == uPtr))
                {
                    supdrvOSMemGetPages(&pBundle->aMem[i], paPages);
                    RTSpinlockRelease(pSession->Spinlock, &SpinlockTmp);
                    return 0;
                }
#endif
            }
        }
    }
    RTSpinlockRelease(pSession->Spinlock, &SpinlockTmp);
    dprintf(("Failed to find %p!!!\n", (void *)uPtr));
    return SUPDRV_ERR_INVALID_PARAM;
}


/**
 * Free memory allocated by SUPR0MemAlloc().
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pSession        The session owning the allocation.
 * @param   uPtr            The Ring-0 or Ring-3 address returned by SUPR0MemAlloc().
 */
SUPR0DECL(int) SUPR0MemFree(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr)
{
    dprintf(("SUPR0MemFree: pSession=%p uPtr=%p\n", pSession, (void *)uPtr));
    return supdrvMemRelease(pSession, uPtr, MEMREF_TYPE_MEM);
}


/**
 * Maps the GIP into userspace and/or get the physical address of the GIP.
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pSession        Session to which the GIP mapping should belong.
 * @param   ppGipR3         Where to store the address of the ring-3 mapping. (optional)
 * @param   pHCPhysGip      Where to store the physical address. (optional)
 *
 * @remark  There is no reference counting on the mapping, so one call to this function
 *          count globally as one reference. One call to SUPR0GipUnmap() is will unmap GIP
 *          and remove the session as a GIP user.
 */
SUPR0DECL(int) SUPR0GipMap(PSUPDRVSESSION pSession, PRTR3PTR ppGipR3, PRTHCPHYS pHCPhysGid)
{
    int             rc = 0;
    PSUPDRVDEVEXT   pDevExt = pSession->pDevExt;
    RTR3PTR         pGip = NIL_RTR3PTR;
    RTHCPHYS        HCPhys = NIL_RTHCPHYS;
    dprintf(("SUPR0GipMap: pSession=%p ppGipR3=%p pHCPhysGid=%p\n", pSession, ppGipR3, pHCPhysGid));

    /*
     * Validate
     */
    if (!ppGipR3 && !pHCPhysGid)
        return 0;

    RTSemFastMutexRequest(pDevExt->mtxGip);
    if (pDevExt->pGip)
    {
        /*
         * Map it?
         */
        if (ppGipR3)
        {
#ifdef USE_NEW_OS_INTERFACE
            if (pSession->GipMapObjR3 == NIL_RTR0MEMOBJ)
                rc = RTR0MemObjMapUser(&pSession->GipMapObjR3, pDevExt->GipMemObj, (void*)-1, 0,
                                       RTMEM_PROT_READ, RTR0ProcHandleSelf());
            if (RT_SUCCESS(rc))
            {
                pGip = (RTR3PTR)RTR0MemObjAddress(pSession->GipMapObjR3);
                rc = VINF_SUCCESS; /** @todo remove this and replace the !rc below with RT_SUCCESS(rc). */
            }
#else /* !USE_NEW_OS_INTERFACE */
            if (!pSession->pGip)
                rc = supdrvOSGipMap(pSession->pDevExt, &pSession->pGip);
            if (!rc)
                pGip = (RTR3PTR)pSession->pGip;
#endif /* !USE_NEW_OS_INTERFACE */
        }

        /*
         * Get physical address.
         */
        if (pHCPhysGid && !rc)
            HCPhys = pDevExt->HCPhysGip;

        /*
         * Reference globally.
         */
        if (!pSession->fGipReferenced && !rc)
        {
            pSession->fGipReferenced = 1;
            pDevExt->cGipUsers++;
            if (pDevExt->cGipUsers == 1)
            {
                PSUPGLOBALINFOPAGE pGip = pDevExt->pGip;
                unsigned i;

                dprintf(("SUPR0GipMap: Resumes GIP updating\n"));

                for (i = 0; i < RT_ELEMENTS(pGip->aCPUs); i++)
                    ASMAtomicXchgU32(&pGip->aCPUs[i].u32TransactionId, pGip->aCPUs[i].u32TransactionId & ~(GIP_UPDATEHZ_RECALC_FREQ * 2 - 1));
                ASMAtomicXchgU64(&pGip->u64NanoTSLastUpdateHz, 0);

#ifdef USE_NEW_OS_INTERFACE
                rc = RTTimerStart(pDevExt->pGipTimer, 0);
                AssertRC(rc); rc = 0;
#else
                supdrvOSGipResume(pDevExt);
#endif
            }
        }
    }
    else
    {
        rc = SUPDRV_ERR_GENERAL_FAILURE;
        dprintf(("SUPR0GipMap: GIP is not available!\n"));
    }
    RTSemFastMutexRelease(pDevExt->mtxGip);

    /*
     * Write returns.
     */
    if (pHCPhysGid)
        *pHCPhysGid = HCPhys;
    if (ppGipR3)
        *ppGipR3 = pGip;

#ifdef DEBUG_DARWIN_GIP
    OSDBGPRINT(("SUPR0GipMap: returns %d *pHCPhysGid=%lx *ppGip=%p GipMapObjR3\n", rc, (unsigned long)HCPhys, pGip, pSession->GipMapObjR3));
#else
    dprintf(("SUPR0GipMap: returns %d *pHCPhysGid=%lx *ppGipR3=%p\n", rc, (unsigned long)HCPhys, (void *)(uintptr_t)pGip));
#endif 
    return rc;
}


/**
 * Unmaps any user mapping of the GIP and terminates all GIP access
 * from this session.
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pSession        Session to which the GIP mapping should belong.
 */
SUPR0DECL(int) SUPR0GipUnmap(PSUPDRVSESSION pSession)
{
    int                     rc = 0;
    PSUPDRVDEVEXT           pDevExt = pSession->pDevExt;
#ifdef DEBUG_DARWIN_GIP
    OSDBGPRINT(("SUPR0GipUnmap: pSession=%p pGip=%p GipMapObjR3=%p\n", 
                pSession, 
                pSession->GipMapObjR3 != NIL_RTR0MEMOBJ ? RTR0MemObjAddress(pSession->GipMapObjR3) : NULL, 
                pSession->GipMapObjR3));
#else
    dprintf(("SUPR0GipUnmap: pSession=%p\n", pSession));
#endif 

    RTSemFastMutexRequest(pDevExt->mtxGip);

    /*
     * Unmap anything?
     */
#ifdef USE_NEW_OS_INTERFACE
    if (pSession->GipMapObjR3 != NIL_RTR0MEMOBJ)
    {
        rc = RTR0MemObjFree(pSession->GipMapObjR3, false);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
            pSession->GipMapObjR3 = NIL_RTR0MEMOBJ;
    }
#else
    if (pSession->pGip)
    {
        rc = supdrvOSGipUnmap(pDevExt, pSession->pGip);
        if (!rc)
            pSession->pGip = NULL;
    }
#endif

    /*
     * Dereference global GIP.
     */
    if (pSession->fGipReferenced && !rc)
    {
        pSession->fGipReferenced = 0;
        if (    pDevExt->cGipUsers > 0
            &&  !--pDevExt->cGipUsers)
        {
            dprintf(("SUPR0GipUnmap: Suspends GIP updating\n"));
#ifdef USE_NEW_OS_INTERFACE
            rc = RTTimerStop(pDevExt->pGipTimer); AssertRC(rc); rc = 0;
#else
            supdrvOSGipSuspend(pDevExt);
#endif
        }
    }

    RTSemFastMutexRelease(pDevExt->mtxGip);

    return rc;
}


/**
 * Adds a memory object to the session.
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pMem        Memory tracking structure containing the
 *                      information to track.
 * @param   pSession    The session.
 */
static int supdrvMemAdd(PSUPDRVMEMREF pMem, PSUPDRVSESSION pSession)
{
    PSUPDRVBUNDLE pBundle;
    RTSPINLOCKTMP SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;

    /*
     * Find free entry and record the allocation.
     */
    RTSpinlockAcquire(pSession->Spinlock, &SpinlockTmp);
    for (pBundle = &pSession->Bundle; pBundle; pBundle = pBundle->pNext)
    {
        if (pBundle->cUsed < sizeof(pBundle->aMem) / sizeof(pBundle->aMem[0]))
        {
            unsigned i;
            for (i = 0; i < sizeof(pBundle->aMem) / sizeof(pBundle->aMem[0]); i++)
            {
#ifdef USE_NEW_OS_INTERFACE
                if (pBundle->aMem[i].MemObj == NIL_RTR0MEMOBJ)
#else  /* !USE_NEW_OS_INTERFACE */
                if (    !pBundle->aMem[i].pvR0
                    &&  !pBundle->aMem[i].pvR3)
#endif /* !USE_NEW_OS_INTERFACE */
                {
                    pBundle->cUsed++;
                    pBundle->aMem[i] = *pMem;
                    RTSpinlockRelease(pSession->Spinlock, &SpinlockTmp);
                    return 0;
                }
            }
            AssertFailed();             /* !!this can't be happening!!! */
        }
    }
    RTSpinlockRelease(pSession->Spinlock, &SpinlockTmp);

    /*
     * Need to allocate a new bundle.
     * Insert into the last entry in the bundle.
     */
    pBundle = (PSUPDRVBUNDLE)RTMemAllocZ(sizeof(*pBundle));
    if (!pBundle)
        return SUPDRV_ERR_NO_MEMORY;

    /* take last entry. */
    pBundle->cUsed++;
    pBundle->aMem[sizeof(pBundle->aMem) / sizeof(pBundle->aMem[0]) - 1] = *pMem;

    /* insert into list. */
    RTSpinlockAcquire(pSession->Spinlock, &SpinlockTmp);
    pBundle->pNext = pSession->Bundle.pNext;
    pSession->Bundle.pNext = pBundle;
    RTSpinlockRelease(pSession->Spinlock, &SpinlockTmp);

    return 0;
}


/**
 * Releases a memory object referenced by pointer and type.
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_INVALID_PARAM on failure.
 * @param   pSession    Session data.
 * @param   uPtr        Pointer to memory. This is matched against both the R0 and R3 addresses.
 * @param   eType       Memory type.
 */
static int supdrvMemRelease(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr, SUPDRVMEMREFTYPE eType)
{
    PSUPDRVBUNDLE pBundle;
    RTSPINLOCKTMP SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;

    /*
     * Validate input.
     */
    if (!pSession)
    {
        dprintf(("pSession must not be NULL!"));
        return SUPDRV_ERR_INVALID_PARAM;
    }
    if (!uPtr)
    {
        dprintf(("Illegal address %p\n", (void *)uPtr));
        return SUPDRV_ERR_INVALID_PARAM;
    }

    /*
     * Search for the address.
     */
    RTSpinlockAcquire(pSession->Spinlock, &SpinlockTmp);
    for (pBundle = &pSession->Bundle; pBundle; pBundle = pBundle->pNext)
    {
        if (pBundle->cUsed > 0)
        {
            unsigned i;
            for (i = 0; i < sizeof(pBundle->aMem) / sizeof(pBundle->aMem[0]); i++)
            {
#ifdef USE_NEW_OS_INTERFACE
                if (    pBundle->aMem[i].eType == eType
                    &&  pBundle->aMem[i].MemObj != NIL_RTR0MEMOBJ
                    &&  (   (RTHCUINTPTR)RTR0MemObjAddress(pBundle->aMem[i].MemObj) == uPtr
                         || (   pBundle->aMem[i].MapObjR3 != NIL_RTR0MEMOBJ
                             && (RTHCUINTPTR)RTR0MemObjAddress(pBundle->aMem[i].MapObjR3) == uPtr))
                   )
                {
                    /* Make a copy of it and release it outside the spinlock. */
                    SUPDRVMEMREF Mem = pBundle->aMem[i];
                    pBundle->aMem[i].eType = MEMREF_TYPE_UNUSED;
                    pBundle->aMem[i].MemObj = NIL_RTR0MEMOBJ;
                    pBundle->aMem[i].MapObjR3 = NIL_RTR0MEMOBJ;
                    RTSpinlockRelease(pSession->Spinlock, &SpinlockTmp);

                    if (Mem.MapObjR3)
                    {
                        int rc = RTR0MemObjFree(Mem.MapObjR3, false);
                        AssertRC(rc); /** @todo figure out how to handle this. */
                    }
                    if (Mem.MemObj)
                    {
                        int rc = RTR0MemObjFree(Mem.MemObj, false);
                        AssertRC(rc); /** @todo figure out how to handle this. */
                    }
                    return 0;
                }
#else /* !USE_NEW_OS_INTERFACE */
                if (    pBundle->aMem[i].eType == eType
                    &&  (   (RTHCUINTPTR)pBundle->aMem[i].pvR0 == uPtr
                         || (RTHCUINTPTR)pBundle->aMem[i].pvR3 == uPtr))
                {
                    /* Make a copy of it and release it outside the spinlock. */
                    SUPDRVMEMREF Mem = pBundle->aMem[i];
                    pBundle->aMem[i].eType = MEMREF_TYPE_UNUSED;
                    pBundle->aMem[i].pvR0  = NULL;
                    pBundle->aMem[i].pvR3  = NIL_RTR3PTR;
                    pBundle->aMem[i].cb    = 0;
                    RTSpinlockRelease(pSession->Spinlock, &SpinlockTmp);

                    /* Type specific free operation. */
                    switch (Mem.eType)
                    {
                        case MEMREF_TYPE_LOCKED:
                            supdrvOSUnlockMemOne(&Mem);
                            break;
                        case MEMREF_TYPE_CONT:
                            supdrvOSContFreeOne(&Mem);
                            break;
                        case MEMREF_TYPE_LOW:
                            supdrvOSLowFreeOne(&Mem);
                            break;
                        case MEMREF_TYPE_MEM:
                            supdrvOSMemFreeOne(&Mem);
                            break;
                        default:
                        case MEMREF_TYPE_UNUSED:
                            break;
                    }
                    return 0;
               }
#endif /* !USE_NEW_OS_INTERFACE */
            }
        }
    }
    RTSpinlockRelease(pSession->Spinlock, &SpinlockTmp);
    dprintf(("Failed to find %p!!! (eType=%d)\n", (void *)uPtr, eType));
    return SUPDRV_ERR_INVALID_PARAM;
}


#ifndef VBOX_WITHOUT_IDT_PATCHING
/**
 * Install IDT for the current CPU.
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_NO_MEMORY or SUPDRV_ERROR_IDT_FAILED on failure.
 * @param   pIn         Input data.
 * @param   pOut        Output data.
 */
static int supdrvIOCtl_IdtInstall(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPIDTINSTALL_IN pIn, PSUPIDTINSTALL_OUT pOut)
{
    PSUPDRVPATCHUSAGE   pUsagePre;
    PSUPDRVPATCH        pPatchPre;
    RTIDTR              Idtr;
    PSUPDRVPATCH        pPatch;
    RTSPINLOCKTMP       SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;
    dprintf(("supdrvIOCtl_IdtInstall\n"));

    /*
     * Preallocate entry for this CPU cause we don't wanna do
     * that inside the spinlock!
     */
    pUsagePre = (PSUPDRVPATCHUSAGE)RTMemAlloc(sizeof(*pUsagePre));
    if (!pUsagePre)
        return SUPDRV_ERR_NO_MEMORY;

    /*
     * Take the spinlock and see what we need to do.
     */
    RTSpinlockAcquireNoInts(pDevExt->Spinlock, &SpinlockTmp);

    /* check if we already got a free patch. */
    if (!pDevExt->pIdtPatchesFree)
    {
        /*
         * Allocate a patch - outside the spinlock of course.
         */
        RTSpinlockReleaseNoInts(pDevExt->Spinlock, &SpinlockTmp);

        pPatchPre = (PSUPDRVPATCH)RTMemExecAlloc(sizeof(*pPatchPre));
        if (!pPatchPre)
            return SUPDRV_ERR_NO_MEMORY;

        RTSpinlockAcquireNoInts(pDevExt->Spinlock, &SpinlockTmp);
    }
    else
    {
        pPatchPre = pDevExt->pIdtPatchesFree;
        pDevExt->pIdtPatchesFree = pPatchPre->pNext;
    }

    /* look for matching patch entry */
    ASMGetIDTR(&Idtr);
    pPatch = pDevExt->pIdtPatches;
    while (pPatch && pPatch->pvIdt != (void *)Idtr.pIdt)
        pPatch = pPatch->pNext;

    if (!pPatch)
    {
        /*
         * Create patch.
         */
        pPatch = supdrvIdtPatchOne(pDevExt, pPatchPre);
        if (pPatch)
            pPatchPre = NULL;           /* mark as used. */
    }
    else
    {
        /*
         * Simply increment patch usage.
         */
        pPatch->cUsage++;
    }

    if (pPatch)
    {
        /*
         * Increment and add if need be the session usage record for this patch.
         */
        PSUPDRVPATCHUSAGE   pUsage = pSession->pPatchUsage;
        while (pUsage && pUsage->pPatch != pPatch)
            pUsage = pUsage->pNext;

        if (!pUsage)
        {
            /*
             * Add usage record.
             */
            pUsagePre->cUsage     = 1;
            pUsagePre->pPatch     = pPatch;
            pUsagePre->pNext      = pSession->pPatchUsage;
            pSession->pPatchUsage = pUsagePre;
            pUsagePre = NULL;           /* mark as used. */
        }
        else
        {
            /*
             * Increment usage count.
             */
            pUsage->cUsage++;
        }
    }

    /* free patch - we accumulate them for paranoid saftly reasons. */
    if (pPatchPre)
    {
        pPatchPre->pNext = pDevExt->pIdtPatchesFree;
        pDevExt->pIdtPatchesFree = pPatchPre;
    }

    RTSpinlockReleaseNoInts(pDevExt->Spinlock, &SpinlockTmp);

    /*
     * Free unused preallocated buffers.
     */
    if (pUsagePre)
        RTMemFree(pUsagePre);

    pOut->u8Idt = pDevExt->u8Idt;

    return pPatch ? 0 : SUPDRV_ERR_IDT_FAILED;
}


/**
 * This creates a IDT patch entry.
 * If the first patch being installed it'll also determin the IDT entry
 * to use.
 *
 * @returns pPatch on success.
 * @returns NULL on failure.
 * @param   pDevExt     Pointer to globals.
 * @param   pPatch      Patch entry to use.
 *                      This will be linked into SUPDRVDEVEXT::pIdtPatches on
 *                      successful return.
 * @remark  Call must be owning the SUPDRVDEVEXT::Spinlock!
 */
static PSUPDRVPATCH supdrvIdtPatchOne(PSUPDRVDEVEXT pDevExt, PSUPDRVPATCH pPatch)
{
    RTIDTR      Idtr;
    PSUPDRVIDTE paIdt;
    dprintf(("supdrvIOCtl_IdtPatchOne: pPatch=%p\n", pPatch));

    /*
     * Get IDT.
     */
    ASMGetIDTR(&Idtr);
    paIdt = (PSUPDRVIDTE)Idtr.pIdt;
    /*
     * Recent Linux kernels can be configured to 1G user /3G kernel.
     */
    if ((uintptr_t)paIdt < 0x40000000)
    {
        AssertMsgFailed(("bad paIdt=%p\n", paIdt));
        return NULL;
    }

    if (!pDevExt->u8Idt)
    {
        /*
         * Test out the alternatives.
         *
         * At the moment we do not support chaining thus we ASSUME that one of
         * these 48 entries is unused (which is not a problem on Win32 and
         * Linux to my knowledge).
         */
        /** @todo we MUST change this detection to try grab an entry which is NOT in use. This can be
         * combined with gathering info about which guest system call gates we can hook up directly. */
        unsigned    i;
        uint8_t     u8Idt = 0;
        static uint8_t au8Ints[] =
        {
#ifdef __WIN__   /* We don't use 0xef and above because they are system stuff on linux (ef is IPI,
                  * local apic timer, or some other frequently fireing thing). */
            0xef, 0xee, 0xed, 0xec,
#endif
            0xeb, 0xea, 0xe9, 0xe8,
            0xdf, 0xde, 0xdd, 0xdc,
            0x7b, 0x7a, 0x79, 0x78,
            0xbf, 0xbe, 0xbd, 0xbc,
        };
#if defined(__AMD64__) && defined(DEBUG)
        static int  s_iWobble = 0;
        unsigned    iMax = !(s_iWobble++ % 2) ? 0x80 : 0x100;
        dprintf(("IDT: Idtr=%p:%#x\n", (void *)Idtr.pIdt, (unsigned)Idtr.cbIdt));
        for (i = iMax - 0x80; i*16+15 < Idtr.cbIdt && i < iMax; i++)
        {
            dprintf(("%#x: %04x:%08x%04x%04x P=%d DPL=%d IST=%d Type1=%#x u32Reserved=%#x u5Reserved=%#x\n",
                     i, paIdt[i].u16SegSel, paIdt[i].u32OffsetTop, paIdt[i].u16OffsetHigh, paIdt[i].u16OffsetLow,
                     paIdt[i].u1Present, paIdt[i].u2DPL, paIdt[i].u3IST, paIdt[i].u5Type2,
                     paIdt[i].u32Reserved, paIdt[i].u5Reserved));
        }
#endif
        /* look for entries which are not present or otherwise unused. */
        for (i = 0; i < sizeof(au8Ints) / sizeof(au8Ints[0]); i++)
        {
            u8Idt = au8Ints[i];
            if (    u8Idt * sizeof(SUPDRVIDTE) < Idtr.cbIdt
                &&  (   !paIdt[u8Idt].u1Present
                     || paIdt[u8Idt].u5Type2 == 0))
                break;
            u8Idt = 0;
        }
        if (!u8Idt)
        {
            /* try again, look for a compatible entry .*/
            for (i = 0; i < sizeof(au8Ints) / sizeof(au8Ints[0]); i++)
            {
                u8Idt = au8Ints[i];
                if (    u8Idt * sizeof(SUPDRVIDTE) < Idtr.cbIdt
                    &&  paIdt[u8Idt].u1Present
                    &&  paIdt[u8Idt].u5Type2 == SUPDRV_IDTE_TYPE2_INTERRUPT_GATE
                    &&  !(paIdt[u8Idt].u16SegSel & 3))
                    break;
                u8Idt = 0;
            }
            if (!u8Idt)
            {
                dprintf(("Failed to find appropirate IDT entry!!\n"));
                return NULL;
            }
        }
        pDevExt->u8Idt = u8Idt;
        dprintf(("supdrvIOCtl_IdtPatchOne: u8Idt=%x\n", u8Idt));
    }

    /*
     * Prepare the patch
     */
    memset(pPatch, 0, sizeof(*pPatch));
    pPatch->pvIdt                       = paIdt;
    pPatch->cUsage                      = 1;
    pPatch->pIdtEntry                   = &paIdt[pDevExt->u8Idt];
    pPatch->SavedIdt                    = paIdt[pDevExt->u8Idt];
    pPatch->ChangedIdt.u16OffsetLow     = (uint32_t)((uintptr_t)&pPatch->auCode[0] & 0xffff);
    pPatch->ChangedIdt.u16OffsetHigh    = (uint32_t)((uintptr_t)&pPatch->auCode[0] >> 16);
#ifdef __AMD64__
    pPatch->ChangedIdt.u32OffsetTop     = (uint32_t)((uintptr_t)&pPatch->auCode[0] >> 32);
#endif
    pPatch->ChangedIdt.u16SegSel        = ASMGetCS();
#ifdef __AMD64__
    pPatch->ChangedIdt.u3IST            = 0;
    pPatch->ChangedIdt.u5Reserved       = 0;
#else /* x86 */
    pPatch->ChangedIdt.u5Reserved       = 0;
    pPatch->ChangedIdt.u3Type1          = 0;
#endif /* x86 */
    pPatch->ChangedIdt.u5Type2          = SUPDRV_IDTE_TYPE2_INTERRUPT_GATE;
    pPatch->ChangedIdt.u2DPL            = 3;
    pPatch->ChangedIdt.u1Present        = 1;

    /*
     * Generate the patch code.
     */
  {
#ifdef __AMD64__
    union
    {
        uint8_t    *pb;
        uint32_t   *pu32;
        uint64_t   *pu64;
    } u, uFixJmp, uFixCall, uNotNested;
    u.pb = &pPatch->auCode[0];

    /* check the cookie */
    *u.pb++ = 0x3d;                     //  cmp     eax, GLOBALCOOKIE
    *u.pu32++ = pDevExt->u32Cookie;

    *u.pb++ = 0x74;                     //  jz      @VBoxCall
    *u.pb++ = 2;

    /* jump to forwarder code. */
    *u.pb++ = 0xeb;
    uFixJmp = u;
    *u.pb++ = 0xfe;

                                        //  @VBoxCall:
    *u.pb++ = 0x0f;                     //  swapgs
    *u.pb++ = 0x01;
    *u.pb++ = 0xf8;

    /*
     * Call VMMR0Entry
     *      We don't have to push the arguments here, but we have top
     *      reserve some stack space for the interrupt forwarding.
     */
# ifdef __WIN__
    *u.pb++ = 0x50;                     //  push    rax                             ; alignment filler.
    *u.pb++ = 0x41;                     //  push    r8                              ; uArg
    *u.pb++ = 0x50;
    *u.pb++ = 0x52;                     //  push    rdx                             ; uOperation
    *u.pb++ = 0x51;                     //  push    rcx                             ; pVM
# else
    *u.pb++ = 0x51;                     //  push    rcx                             ; alignment filler.
    *u.pb++ = 0x52;                     //  push    rdx                             ; uArg
    *u.pb++ = 0x56;                     //  push    rsi                             ; uOperation
    *u.pb++ = 0x57;                     //  push    rdi                             ; pVM
# endif

    *u.pb++ = 0xff;                     //  call    qword [pfnVMMR0Entry wrt rip]
    *u.pb++ = 0x15;
    uFixCall = u;
    *u.pu32++ = 0;

    *u.pb++ = 0x48;                     //  add     rsp, 20h                        ; remove call frame.
    *u.pb++ = 0x81;
    *u.pb++ = 0xc4;
    *u.pu32++ = 0x20;

    *u.pb++ = 0x0f;                     // swapgs
    *u.pb++ = 0x01;
    *u.pb++ = 0xf8;

    /* Return to R3. */
    uNotNested = u;
    *u.pb++ = 0x48;                     //  iretq
    *u.pb++ = 0xcf;

    while ((uintptr_t)u.pb & 0x7)       //  align 8
        *u.pb++ = 0xcc;

    /* Pointer to the VMMR0Entry. */    //  pfnVMMR0Entry dq StubVMMR0Entry
    *uFixCall.pu32 = (uint32_t)(u.pb - uFixCall.pb - 4);                uFixCall.pb = NULL;
    pPatch->offVMMR0EntryFixup = (uint16_t)(u.pb - &pPatch->auCode[0]);
    *u.pu64++ = pDevExt->pvVMMR0 ? (uint64_t)pDevExt->pfnVMMR0Entry : (uint64_t)u.pb + 8;

    /* stub entry. */                   //  StubVMMR0Entry:
    pPatch->offStub = (uint16_t)(u.pb - &pPatch->auCode[0]);
    *u.pb++ = 0x33;                     //  xor     eax, eax
    *u.pb++ = 0xc0;

    *u.pb++ = 0x48;                     //  dec     rax
    *u.pb++ = 0xff;
    *u.pb++ = 0xc8;

    *u.pb++ = 0xc3;                     //  ret

    /* forward to the original handler using a retf. */
    *uFixJmp.pb = (uint8_t)(u.pb - uFixJmp.pb - 1);                     uFixJmp.pb = NULL;

    *u.pb++ = 0x68;                     //  push    <target cs>
    *u.pu32++ = !pPatch->SavedIdt.u5Type2 ? ASMGetCS() : pPatch->SavedIdt.u16SegSel;

    *u.pb++ = 0x68;                     //  push    <low target rip>
    *u.pu32++ = !pPatch->SavedIdt.u5Type2
              ? (uint32_t)(uintptr_t)uNotNested.pb
              : (uint32_t)pPatch->SavedIdt.u16OffsetLow
              | (uint32_t)pPatch->SavedIdt.u16OffsetHigh << 16;

    *u.pb++ = 0xc7;                     //  mov     dword [rsp + 4], <high target rip>
    *u.pb++ = 0x44;
    *u.pb++ = 0x24;
    *u.pb++ = 0x04;
    *u.pu32++ = !pPatch->SavedIdt.u5Type2
              ? (uint32_t)((uint64_t)uNotNested.pb >> 32)
              : pPatch->SavedIdt.u32OffsetTop;

    *u.pb++ = 0x48;                     //  retf        ; does this require prefix?
    *u.pb++ = 0xcb;

#else /* __X86__ */

    union
    {
        uint8_t    *pb;
        uint16_t   *pu16;
        uint32_t   *pu32;
    } u, uFixJmpNotNested, uFixJmp, uFixCall, uNotNested;
    u.pb = &pPatch->auCode[0];

    /* check the cookie */
    *u.pb++ = 0x81;                     //  cmp     esi, GLOBALCOOKIE
    *u.pb++ = 0xfe;
    *u.pu32++ = pDevExt->u32Cookie;

    *u.pb++ = 0x74;                     //  jz      VBoxCall
    uFixJmp = u;
    *u.pb++ = 0;

    /* jump (far) to the original handler / not-nested-stub. */
    *u.pb++ = 0xea;                     //  jmp far NotNested
    uFixJmpNotNested = u;
    *u.pu32++ = 0;
    *u.pu16++ = 0;

    /* save selector registers. */      // VBoxCall:
    *uFixJmp.pb = (uint8_t)(u.pb - uFixJmp.pb - 1);
    *u.pb++ = 0x0f;                     //  push    fs
    *u.pb++ = 0xa0;

    *u.pb++ = 0x1e;                     //  push    ds

    *u.pb++ = 0x06;                     //  push    es

    /* call frame */
    *u.pb++ = 0x51;                     //  push    ecx

    *u.pb++ = 0x52;                     //  push    edx

    *u.pb++ = 0x50;                     //  push    eax

    /* load ds, es and perhaps fs before call. */
    *u.pb++ = 0xb8;                     //  mov     eax, KernelDS
    *u.pu32++ = ASMGetDS();

    *u.pb++ = 0x8e;                     //  mov     ds, eax
    *u.pb++ = 0xd8;

    *u.pb++ = 0x8e;                     //  mov     es, eax
    *u.pb++ = 0xc0;

#ifdef __WIN__
    *u.pb++ = 0xb8;                     //  mov     eax, KernelFS
    *u.pu32++ = ASMGetFS();

    *u.pb++ = 0x8e;                     //  mov     fs, eax
    *u.pb++ = 0xe0;
#endif

    /* do the call. */
    *u.pb++ = 0xe8;                     //  call    _VMMR0Entry / StubVMMR0Entry
    uFixCall = u;
    pPatch->offVMMR0EntryFixup = (uint16_t)(u.pb - &pPatch->auCode[0]);
    *u.pu32++ = 0xfffffffb;

    *u.pb++ = 0x83;                     //  add     esp, 0ch   ; cdecl
    *u.pb++ = 0xc4;
    *u.pb++ = 0x0c;

    /* restore selector registers. */
    *u.pb++ = 0x07;                     //  pop     es
                                        //
    *u.pb++ = 0x1f;                     //  pop     ds

    *u.pb++ = 0x0f;                     //  pop     fs
    *u.pb++ = 0xa1;

    uNotNested = u;                     // NotNested:
    *u.pb++ = 0xcf;                     //  iretd

    /* the stub VMMR0Entry. */          // StubVMMR0Entry:
    pPatch->offStub = (uint16_t)(u.pb - &pPatch->auCode[0]);
    *u.pb++ = 0x33;                     //  xor     eax, eax
    *u.pb++ = 0xc0;

    *u.pb++ = 0x48;                     //  dec     eax

    *u.pb++ = 0xc3;                     //  ret

    /* Fixup the VMMR0Entry call. */
    if (pDevExt->pvVMMR0)
        *uFixCall.pu32 = (uint32_t)pDevExt->pfnVMMR0Entry - (uint32_t)(uFixCall.pu32 + 1);
    else
        *uFixCall.pu32 = (uint32_t)&pPatch->auCode[pPatch->offStub] - (uint32_t)(uFixCall.pu32 + 1);

    /* Fixup the forward / nested far jump. */
    if (!pPatch->SavedIdt.u5Type2)
    {
        *uFixJmpNotNested.pu32++ = (uint32_t)uNotNested.pb;
        *uFixJmpNotNested.pu16++ = ASMGetCS();
    }
    else
    {
        *uFixJmpNotNested.pu32++ = ((uint32_t)pPatch->SavedIdt.u16OffsetHigh << 16) | pPatch->SavedIdt.u16OffsetLow;
        *uFixJmpNotNested.pu16++ = pPatch->SavedIdt.u16SegSel;
    }
#endif /* __X86__ */
    Assert(u.pb <= &pPatch->auCode[sizeof(pPatch->auCode)]);
#if 0
    /* dump the patch code */
    dprintf(("patch code: %p\n", &pPatch->auCode[0]));
    for (uFixCall.pb = &pPatch->auCode[0]; uFixCall.pb < u.pb; uFixCall.pb++)
        dprintf(("0x%02x,\n", *uFixCall.pb));
#endif
  }

    /*
     * Install the patch.
     */
    supdrvIdtWrite(pPatch->pIdtEntry, &pPatch->ChangedIdt);
    AssertMsg(!memcmp((void *)pPatch->pIdtEntry, &pPatch->ChangedIdt, sizeof(pPatch->ChangedIdt)), ("The stupid change code didn't work!!!!!\n"));

    /*
     * Link in the patch.
     */
    pPatch->pNext = pDevExt->pIdtPatches;
    pDevExt->pIdtPatches = pPatch;

    return pPatch;
}


/**
 * Removes the sessions IDT references.
 * This will uninstall our IDT patch if we left unreferenced.
 *
 * @returns 0 indicating success.
 * @param   pDevExt     Device globals.
 * @param   pSession    Session data.
 */
static int supdrvIOCtl_IdtRemoveAll(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession)
{
    PSUPDRVPATCHUSAGE   pUsage;
    RTSPINLOCKTMP       SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;
    dprintf(("supdrvIOCtl_IdtRemoveAll: pSession=%p\n", pSession));

    /*
     * Take the spinlock.
     */
    RTSpinlockAcquireNoInts(pDevExt->Spinlock, &SpinlockTmp);

    /*
     * Walk usage list.
     */
    pUsage = pSession->pPatchUsage;
    while (pUsage)
    {
        if (pUsage->pPatch->cUsage <= pUsage->cUsage)
            supdrvIdtRemoveOne(pDevExt, pUsage->pPatch);
        else
            pUsage->pPatch->cUsage -= pUsage->cUsage;

        /* next */
        pUsage = pUsage->pNext;
    }

    /*
     * Empty the usage chain and we're done inside the spinlock.
     */
    pUsage = pSession->pPatchUsage;
    pSession->pPatchUsage = NULL;

    RTSpinlockReleaseNoInts(pDevExt->Spinlock, &SpinlockTmp);

    /*
     * Free usage entries.
     */
    while (pUsage)
    {
        void *pvToFree = pUsage;
        pUsage->cUsage = 0;
        pUsage->pPatch = NULL;
        pUsage = pUsage->pNext;
        RTMemFree(pvToFree);
    }

    return 0;
}


/**
 * Remove one patch.
 *
 * @param   pDevExt     Device globals.
 * @param   pPatch      Patch entry to remove.
 * @remark  Caller must own SUPDRVDEVEXT::Spinlock!
 */
static void supdrvIdtRemoveOne(PSUPDRVDEVEXT pDevExt, PSUPDRVPATCH pPatch)
{
    dprintf(("supdrvIdtRemoveOne: pPatch=%p\n", pPatch));

    pPatch->cUsage = 0;

    /*
     * If the IDT entry was changed it have to kick around for ever!
     * This will be attempted freed again, perhaps next time we'll succeed :-)
     */
    if (memcmp((void *)pPatch->pIdtEntry, &pPatch->ChangedIdt, sizeof(pPatch->ChangedIdt)))
    {
        AssertMsgFailed(("The hijacked IDT entry has CHANGED!!!\n"));
        return;
    }

    /*
     * Unlink it.
     */
    if (pDevExt->pIdtPatches != pPatch)
    {
        PSUPDRVPATCH pPatchPrev = pDevExt->pIdtPatches;
        while (pPatchPrev)
        {
            if (pPatchPrev->pNext == pPatch)
            {
                pPatchPrev->pNext = pPatch->pNext;
                break;
            }
            pPatchPrev = pPatchPrev->pNext;
        }
        Assert(!pPatchPrev);
    }
    else
        pDevExt->pIdtPatches = pPatch->pNext;
    pPatch->pNext = NULL;


    /*
     * Verify and restore the IDT.
     */
    AssertMsg(!memcmp((void *)pPatch->pIdtEntry, &pPatch->ChangedIdt, sizeof(pPatch->ChangedIdt)), ("The hijacked IDT entry has CHANGED!!!\n"));
    supdrvIdtWrite(pPatch->pIdtEntry, &pPatch->SavedIdt);
    AssertMsg(!memcmp((void *)pPatch->pIdtEntry, &pPatch->SavedIdt,   sizeof(pPatch->SavedIdt)),   ("The hijacked IDT entry has CHANGED!!!\n"));

    /*
     * Put it in the free list.
     * (This free list stuff is to calm my paranoia.)
     */
    pPatch->pvIdt     = NULL;
    pPatch->pIdtEntry = NULL;

    pPatch->pNext = pDevExt->pIdtPatchesFree;
    pDevExt->pIdtPatchesFree = pPatch;
}


/**
 * Write to an IDT entry.
 *
 * @param   pvIdtEntry      Where to write.
 * @param   pNewIDTEntry    What to write.
 */
static void supdrvIdtWrite(volatile void *pvIdtEntry, const SUPDRVIDTE *pNewIDTEntry)
{
    RTUINTREG   uCR0;
    RTUINTREG   uFlags;

    /*
     * On SMP machines (P4 hyperthreading included) we must preform a
     * 64-bit locked write when updating the IDT entry.
     *
     * The F00F bugfix for linux (and probably other OSes) causes
     * the IDT to be pointing to an readonly mapping. We get around that
     * by temporarily turning of WP. Since we're inside a spinlock at this
     * point, interrupts are disabled and there isn't any way the WP bit
     * flipping can cause any trouble.
     */

    /* Save & Clear interrupt flag; Save & clear WP. */
    uFlags = ASMGetFlags();
    ASMSetFlags(uFlags & ~(RTUINTREG)(1 << 9)); /*X86_EFL_IF*/
    Assert(!(ASMGetFlags() & (1 << 9)));
    uCR0 = ASMGetCR0();
    ASMSetCR0(uCR0 & ~(RTUINTREG)(1 << 16));    /*X86_CR0_WP*/

    /* Update IDT Entry */
#ifdef __AMD64__
    ASMAtomicXchgU128((volatile uint128_t *)pvIdtEntry, *(uint128_t *)(uintptr_t)pNewIDTEntry);
#else
    ASMAtomicXchgU64((volatile uint64_t *)pvIdtEntry, *(uint64_t *)(uintptr_t)pNewIDTEntry);
#endif

    /* Restore CR0 & Flags */
    ASMSetCR0(uCR0);
    ASMSetFlags(uFlags);
}
#endif /* !VBOX_WITHOUT_IDT_PATCHING */


/**
 * Opens an image. If it's the first time it's opened the call must upload
 * the bits using the supdrvIOCtl_LdrLoad() / SUPDRV_IOCTL_LDR_LOAD function.
 *
 * This is the 1st step of the loading.
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pDevExt     Device globals.
 * @param   pSession    Session data.
 * @param   pIn         Input.
 * @param   pOut        Output. (May overlap pIn.)
 */
static int supdrvIOCtl_LdrOpen(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDROPEN_IN pIn, PSUPLDROPEN_OUT pOut)
{
    PSUPDRVLDRIMAGE pImage;
    unsigned        cb;
    void           *pv;
    dprintf(("supdrvIOCtl_LdrOpen: szName=%s cbImage=%d\n", pIn->szName, pIn->cbImage));

    /*
     * Check if we got an instance of the image already.
     */
    RTSemFastMutexRequest(pDevExt->mtxLdr);
    for (pImage = pDevExt->pLdrImages; pImage; pImage = pImage->pNext)
    {
        if (!strcmp(pImage->szName, pIn->szName))
        {
            pImage->cUsage++;
            pOut->pvImageBase   = pImage->pvImage;
            pOut->fNeedsLoading = pImage->uState == SUP_IOCTL_LDR_OPEN;
            supdrvLdrAddUsage(pSession, pImage);
            RTSemFastMutexRelease(pDevExt->mtxLdr);
            return 0;
        }
    }
    /* (not found - add it!) */

    /*
     * Allocate memory.
     */
    cb = pIn->cbImage + sizeof(SUPDRVLDRIMAGE) + 31;
    pv = RTMemExecAlloc(cb);
    if (!pv)
    {
        RTSemFastMutexRelease(pDevExt->mtxLdr);
        return SUPDRV_ERR_NO_MEMORY;
    }

    /*
     * Setup and link in the LDR stuff.
     */
    pImage = (PSUPDRVLDRIMAGE)pv;
    pImage->pvImage         = ALIGNP(pImage + 1, 32);
    pImage->cbImage         = pIn->cbImage;
    pImage->pfnModuleInit   = NULL;
    pImage->pfnModuleTerm   = NULL;
    pImage->uState          = SUP_IOCTL_LDR_OPEN;
    pImage->cUsage          = 1;
    strcpy(pImage->szName, pIn->szName);

    pImage->pNext           = pDevExt->pLdrImages;
    pDevExt->pLdrImages     = pImage;

    supdrvLdrAddUsage(pSession, pImage);

    pOut->pvImageBase       = pImage->pvImage;
    pOut->fNeedsLoading     = 1;
    RTSemFastMutexRelease(pDevExt->mtxLdr);
    return 0;
}


/**
 * Loads the image bits.
 *
 * This is the 2nd step of the loading.
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pDevExt     Device globals.
 * @param   pSession    Session data.
 * @param   pIn         Input.
 */
static int supdrvIOCtl_LdrLoad(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDRLOAD_IN pIn)
{
    PSUPDRVLDRUSAGE pUsage;
    PSUPDRVLDRIMAGE pImage;
    int             rc;
    dprintf(("supdrvIOCtl_LdrLoad: pvImageBase=%p cbImage=%d\n", pIn->pvImageBase, pIn->cbImage));

    /*
     * Find the ldr image.
     */
    RTSemFastMutexRequest(pDevExt->mtxLdr);
    pUsage = pSession->pLdrUsage;
    while (pUsage && pUsage->pImage->pvImage != pIn->pvImageBase)
        pUsage = pUsage->pNext;
    if (!pUsage)
    {
        RTSemFastMutexRelease(pDevExt->mtxLdr);
        dprintf(("SUP_IOCTL_LDR_LOAD: couldn't find image!\n"));
        return SUPDRV_ERR_INVALID_HANDLE;
    }
    pImage = pUsage->pImage;
    if (pImage->cbImage != pIn->cbImage)
    {
        RTSemFastMutexRelease(pDevExt->mtxLdr);
        dprintf(("SUP_IOCTL_LDR_LOAD: image size mismatch!! %d(prep) != %d(load)\n", pImage->cbImage, pIn->cbImage));
        return SUPDRV_ERR_INVALID_HANDLE;
    }
    if (pImage->uState != SUP_IOCTL_LDR_OPEN)
    {
        unsigned uState = pImage->uState;
        RTSemFastMutexRelease(pDevExt->mtxLdr);
        if (uState != SUP_IOCTL_LDR_LOAD)
            AssertMsgFailed(("SUP_IOCTL_LDR_LOAD: invalid image state %d (%#x)!\n", uState, uState));
        return SUPDRV_ERR_ALREADY_LOADED;
    }
    switch (pIn->eEPType)
    {
        case EP_NOTHING:
            break;
        case EP_VMMR0:
            if (!pIn->EP.VMMR0.pvVMMR0 || !pIn->EP.VMMR0.pvVMMR0Entry)
            {
                RTSemFastMutexRelease(pDevExt->mtxLdr);
                dprintf(("pvVMMR0=%p or pIn->EP.VMMR0.pvVMMR0Entry=%p is NULL!\n",
                         pIn->EP.VMMR0.pvVMMR0, pIn->EP.VMMR0.pvVMMR0Entry));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            if ((uintptr_t)pIn->EP.VMMR0.pvVMMR0Entry - (uintptr_t)pImage->pvImage >= pIn->cbImage)
            {
                RTSemFastMutexRelease(pDevExt->mtxLdr);
                dprintf(("SUP_IOCTL_LDR_LOAD: pvVMMR0Entry=%p is outside the image (%p %d bytes)\n",
                         pIn->EP.VMMR0.pvVMMR0Entry, pImage->pvImage, pIn->cbImage));
                return SUPDRV_ERR_INVALID_PARAM;
            }
            break;
        default:
            RTSemFastMutexRelease(pDevExt->mtxLdr);
            dprintf(("Invalid eEPType=%d\n", pIn->eEPType));
            return SUPDRV_ERR_INVALID_PARAM;
    }
    if (    pIn->pfnModuleInit
        &&  (uintptr_t)pIn->pfnModuleInit - (uintptr_t)pImage->pvImage >= pIn->cbImage)
    {
        RTSemFastMutexRelease(pDevExt->mtxLdr);
        dprintf(("SUP_IOCTL_LDR_LOAD: pfnModuleInit=%p is outside the image (%p %d bytes)\n",
                 pIn->pfnModuleInit, pImage->pvImage, pIn->cbImage));
        return SUPDRV_ERR_INVALID_PARAM;
    }
    if (    pIn->pfnModuleTerm
        &&  (uintptr_t)pIn->pfnModuleTerm - (uintptr_t)pImage->pvImage >= pIn->cbImage)
    {
        RTSemFastMutexRelease(pDevExt->mtxLdr);
        dprintf(("SUP_IOCTL_LDR_LOAD: pfnModuleTerm=%p is outside the image (%p %d bytes)\n",
                 pIn->pfnModuleTerm, pImage->pvImage, pIn->cbImage));
        return SUPDRV_ERR_INVALID_PARAM;
    }

    /*
     * Copy the memory.
     */
    /* no need to do try/except as this is a buffered request. */
    memcpy(pImage->pvImage, &pIn->achImage[0], pImage->cbImage);
    pImage->uState = SUP_IOCTL_LDR_LOAD;
    pImage->pfnModuleInit = pIn->pfnModuleInit;
    pImage->pfnModuleTerm = pIn->pfnModuleTerm;
    pImage->offSymbols    = pIn->offSymbols;
    pImage->cSymbols      = pIn->cSymbols;
    pImage->offStrTab     = pIn->offStrTab;
    pImage->cbStrTab      = pIn->cbStrTab;

    /*
     * Update any entry points.
     */
    switch (pIn->eEPType)
    {
        default:
        case EP_NOTHING:
            rc = 0;
            break;
        case EP_VMMR0:
            rc = supdrvLdrSetR0EP(pDevExt, pIn->EP.VMMR0.pvVMMR0, pIn->EP.VMMR0.pvVMMR0Entry);
            break;
    }

    /*
     * On success call the module initialization.
     */
    dprintf(("supdrvIOCtl_LdrLoad: pfnModuleInit=%p\n", pImage->pfnModuleInit));
    if (!rc && pImage->pfnModuleInit)
    {
        dprintf(("supdrvIOCtl_LdrLoad: calling pfnModuleInit=%p\n", pImage->pfnModuleInit));
        rc = pImage->pfnModuleInit();
        if (rc && pDevExt->pvVMMR0 == pImage->pvImage)
            supdrvLdrUnsetR0EP(pDevExt);
    }

    if (rc)
        pImage->uState = SUP_IOCTL_LDR_OPEN;

    RTSemFastMutexRelease(pDevExt->mtxLdr);
    return rc;
}


/**
 * Frees a previously loaded (prep'ed) image.
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pDevExt     Device globals.
 * @param   pSession    Session data.
 * @param   pIn         Input.
 */
static int supdrvIOCtl_LdrFree(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDRFREE_IN pIn)
{
    PSUPDRVLDRUSAGE pUsagePrev;
    PSUPDRVLDRUSAGE pUsage;
    PSUPDRVLDRIMAGE pImage;
    dprintf(("supdrvIOCtl_LdrFree: pvImageBase=%p\n", pIn->pvImageBase));

    /*
     * Find the ldr image.
     */
    RTSemFastMutexRequest(pDevExt->mtxLdr);
    pUsagePrev = NULL;
    pUsage = pSession->pLdrUsage;
    while (pUsage && pUsage->pImage->pvImage != pIn->pvImageBase)
    {
        pUsagePrev = pUsage;
        pUsage = pUsage->pNext;
    }
    if (!pUsage)
    {
        RTSemFastMutexRelease(pDevExt->mtxLdr);
        dprintf(("SUP_IOCTL_LDR_FREE: couldn't find image!\n"));
        return SUPDRV_ERR_INVALID_HANDLE;
    }

    /*
     * Check if we can remove anything.
     */
    pImage = pUsage->pImage;
    if (pImage->cUsage <= 1 || pUsage->cUsage <= 1)
    {
        /* unlink it */
        if (pUsagePrev)
            pUsagePrev->pNext = pUsage->pNext;
        else
            pSession->pLdrUsage = pUsage->pNext;
        /* free it */
        pUsage->pImage = NULL;
        pUsage->pNext = NULL;
        RTMemFree(pUsage);

        /*
         * Derefrence the image.
         */
        if (pImage->cUsage <= 1)
            supdrvLdrFree(pDevExt, pImage);
        else
            pImage->cUsage--;
    }
    else
    {
        /*
         * Dereference both image and usage.
         */
        pImage->cUsage--;
        pUsage->cUsage--;
    }

    RTSemFastMutexRelease(pDevExt->mtxLdr);
    return 0;
}


/**
 * Gets the address of a symbol in an open image.
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pDevExt     Device globals.
 * @param   pSession    Session data.
 * @param   pIn         Input.
 * @param   pOut        Output. (May overlap pIn.)
 */
static int supdrvIOCtl_LdrGetSymbol(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDRGETSYMBOL_IN pIn, PSUPLDRGETSYMBOL_OUT pOut)
{
    PSUPDRVLDRIMAGE pImage;
    PSUPDRVLDRUSAGE pUsage;
    uint32_t        i;
    PSUPLDRSYM      paSyms;
    const char     *pchStrings;
    const size_t    cbSymbol = strlen(pIn->szSymbol) + 1;
    void           *pvSymbol = NULL;
    int             rc = SUPDRV_ERR_GENERAL_FAILURE; /** @todo better error code. */
    dprintf2(("supdrvIOCtl_LdrGetSymbol: pvImageBase=%p szSymbol=\"%s\"\n", pIn->pvImageBase, pIn->szSymbol));

    /*
     * Find the ldr image.
     */
    RTSemFastMutexRequest(pDevExt->mtxLdr);
    pUsage = pSession->pLdrUsage;
    while (pUsage && pUsage->pImage->pvImage != pIn->pvImageBase)
        pUsage = pUsage->pNext;
    if (!pUsage)
    {
        RTSemFastMutexRelease(pDevExt->mtxLdr);
        dprintf(("SUP_IOCTL_LDR_GET_SYMBOL: couldn't find image!\n"));
        return SUPDRV_ERR_INVALID_HANDLE;
    }
    pImage = pUsage->pImage;
    if (pImage->uState != SUP_IOCTL_LDR_LOAD)
    {
        unsigned uState = pImage->uState;
        RTSemFastMutexRelease(pDevExt->mtxLdr);
        dprintf(("SUP_IOCTL_LDR_GET_SYMBOL: invalid image state %d (%#x)!\n", uState, uState)); NOREF(uState);
        return SUPDRV_ERR_ALREADY_LOADED;
    }

    /*
     * Search the symbol string.
     */
    pchStrings = (const char *)((uint8_t *)pImage->pvImage + pImage->offStrTab);
    paSyms     =   (PSUPLDRSYM)((uint8_t *)pImage->pvImage + pImage->offSymbols);
    for (i = 0; i < pImage->cSymbols; i++)
    {
        if (    paSyms[i].offSymbol < pImage->cbImage /* paranoia */
            &&  paSyms[i].offName + cbSymbol <= pImage->cbStrTab
            &&  !memcmp(pchStrings + paSyms[i].offName, pIn->szSymbol, cbSymbol))
        {
            pvSymbol = (uint8_t *)pImage->pvImage + paSyms[i].offSymbol;
            rc = 0;
            break;
        }
    }
    RTSemFastMutexRelease(pDevExt->mtxLdr);
    pOut->pvSymbol = pvSymbol;
    return rc;
}


/**
 * Updates the IDT patches to point to the specified VMM R0 entry
 * point (i.e. VMMR0Enter()).
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pDevExt     Device globals.
 * @param   pSession    Session data.
 * @param   pVMMR0      VMMR0 image handle.
 * @param   pVMMR0Entry VMMR0Entry address.
 * @remark  Caller must own the loader mutex.
 */
static int supdrvLdrSetR0EP(PSUPDRVDEVEXT pDevExt, void *pvVMMR0, void *pvVMMR0Entry)
{
    int     rc;
    dprintf(("supdrvLdrSetR0EP pvVMMR0=%p pvVMMR0Entry=%p\n", pvVMMR0, pvVMMR0Entry));


    /*
     * Check if not yet set.
     */
    rc = 0;
    if (!pDevExt->pvVMMR0)
    {
#ifndef VBOX_WITHOUT_IDT_PATCHING
        PSUPDRVPATCH pPatch;
#endif

        /*
         * Set it and update IDT patch code.
         */
        pDevExt->pvVMMR0        = pvVMMR0;
        pDevExt->pfnVMMR0Entry  = pvVMMR0Entry;
#ifndef VBOX_WITHOUT_IDT_PATCHING
        for (pPatch = pDevExt->pIdtPatches; pPatch; pPatch = pPatch->pNext)
        {
# ifdef __AMD64__
            ASMAtomicXchgU64((volatile uint64_t *)&pPatch->auCode[pPatch->offVMMR0EntryFixup], (uint64_t)pvVMMR0);
# else /* __X86__ */
            ASMAtomicXchgU32((volatile uint32_t *)&pPatch->auCode[pPatch->offVMMR0EntryFixup],
                             (uint32_t)pvVMMR0 - (uint32_t)&pPatch->auCode[pPatch->offVMMR0EntryFixup + 4]);
# endif
        }
#endif /* !VBOX_WITHOUT_IDT_PATCHING */
    }
    else
    {
        /*
         * Return failure or success depending on whether the
         * values match or not.
         */
        if (    pDevExt->pvVMMR0 != pvVMMR0
            ||  (void *)pDevExt->pfnVMMR0Entry != pvVMMR0Entry)
        {
            AssertMsgFailed(("SUP_IOCTL_LDR_SETR0EP: Already set pointing to a different module!\n"));
            rc = SUPDRV_ERR_INVALID_PARAM;
        }
    }
    return rc;
}


/**
 * Unsets the R0 entry point installed by supdrvLdrSetR0EP.
 *
 * @param   pDevExt     Device globals.
 */
static void supdrvLdrUnsetR0EP(PSUPDRVDEVEXT pDevExt)
{
#ifndef VBOX_WITHOUT_IDT_PATCHING
    PSUPDRVPATCH pPatch;
#endif

    pDevExt->pvVMMR0        = NULL;
    pDevExt->pfnVMMR0Entry  = NULL;

#ifndef VBOX_WITHOUT_IDT_PATCHING
    for (pPatch = pDevExt->pIdtPatches; pPatch; pPatch = pPatch->pNext)
    {
# ifdef __AMD64__
        ASMAtomicXchgU64((volatile uint64_t *)&pPatch->auCode[pPatch->offVMMR0EntryFixup],
                         (uint64_t)&pPatch->auCode[pPatch->offStub]);
# else /* __X86__ */
        ASMAtomicXchgU32((volatile uint32_t *)&pPatch->auCode[pPatch->offVMMR0EntryFixup],
                         (uint32_t)&pPatch->auCode[pPatch->offStub] - (uint32_t)&pPatch->auCode[pPatch->offVMMR0EntryFixup + 4]);
# endif
    }
#endif /* !VBOX_WITHOUT_IDT_PATCHING */
}


/**
 * Adds a usage reference in the specified session of an image.
 *
 * @param   pSession    Session in question.
 * @param   pImage      Image which the session is using.
 */
static void supdrvLdrAddUsage(PSUPDRVSESSION pSession, PSUPDRVLDRIMAGE pImage)
{
    PSUPDRVLDRUSAGE pUsage;
    dprintf(("supdrvLdrAddUsage: pImage=%p\n", pImage));

    /*
     * Referenced it already?
     */
    pUsage = pSession->pLdrUsage;
    while (pUsage)
    {
        if (pUsage->pImage == pImage)
        {
            pUsage->cUsage++;
            return;
        }
        pUsage = pUsage->pNext;
    }

    /*
     * Allocate new usage record.
     */
    pUsage = (PSUPDRVLDRUSAGE)RTMemAlloc(sizeof(*pUsage));
    Assert(pUsage);
    if (pUsage)
    {
        pUsage->cUsage = 1;
        pUsage->pImage = pImage;
        pUsage->pNext  = pSession->pLdrUsage;
        pSession->pLdrUsage = pUsage;
    }
    /* ignore errors... */
}


/**
 * Frees a load image.
 *
 * @param   pDevExt     Pointer to device extension.
 * @param   pImage      Pointer to the image we're gonna free.
 *                      This image must exit!
 * @remark  The caller MUST own SUPDRVDEVEXT::mtxLdr!
 */
static void supdrvLdrFree(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage)
{
    PSUPDRVLDRIMAGE pImagePrev;
    dprintf(("supdrvLdrFree: pImage=%p\n", pImage));

    /* find it - arg. should've used doubly linked list. */
    Assert(pDevExt->pLdrImages);
    pImagePrev = NULL;
    if (pDevExt->pLdrImages != pImage)
    {
        pImagePrev = pDevExt->pLdrImages;
        while (pImagePrev->pNext != pImage)
            pImagePrev = pImagePrev->pNext;
        Assert(pImagePrev->pNext == pImage);
    }

    /* unlink */
    if (pImagePrev)
        pImagePrev->pNext = pImage->pNext;
    else
        pDevExt->pLdrImages = pImage->pNext;

    /* check if this is VMMR0.r0 and fix the Idt patches if it is. */
    if (pDevExt->pvVMMR0 == pImage->pvImage)
        supdrvLdrUnsetR0EP(pDevExt);

    /* call termination function if fully loaded. */
    if (    pImage->pfnModuleTerm
        &&  pImage->uState == SUP_IOCTL_LDR_LOAD)
    {
        dprintf(("supdrvIOCtl_LdrLoad: calling pfnModuleTerm=%p\n", pImage->pfnModuleTerm));
        pImage->pfnModuleTerm();
    }

    /* free the image */
    pImage->cUsage = 0;
    pImage->pNext  = 0;
    pImage->uState = SUP_IOCTL_LDR_FREE;
    RTMemExecFree(pImage);
}


/**
 * Gets the current paging mode of the CPU and stores in in pOut.
 */
static int supdrvIOCtl_GetPagingMode(PSUPGETPAGINGMODE_OUT pOut)
{
    RTUINTREG cr0 = ASMGetCR0();
    if ((cr0 & (X86_CR0_PG | X86_CR0_PE)) != (X86_CR0_PG | X86_CR0_PE))
        pOut->enmMode = SUPPAGINGMODE_INVALID;
    else
    {
        RTUINTREG cr4 = ASMGetCR4();
        uint32_t fNXEPlusLMA = 0;
        if (cr4 & X86_CR4_PAE)
        {
            uint32_t fAmdFeatures = ASMCpuId_EDX(0x80000001);
            if (fAmdFeatures & (X86_CPUID_AMD_FEATURE_EDX_NX | X86_CPUID_AMD_FEATURE_EDX_LONG_MODE))
            {
                uint64_t efer = ASMRdMsr(MSR_K6_EFER);
                if ((fAmdFeatures & X86_CPUID_AMD_FEATURE_EDX_NX)        && (efer & MSR_K6_EFER_NXE))
                    fNXEPlusLMA |= BIT(0);
                if ((fAmdFeatures & X86_CPUID_AMD_FEATURE_EDX_LONG_MODE) && (efer & MSR_K6_EFER_LMA))
                    fNXEPlusLMA |= BIT(1);
            }
        }

        switch ((cr4 & (X86_CR4_PAE | X86_CR4_PGE)) | fNXEPlusLMA)
        {
            case 0:
                pOut->enmMode = SUPPAGINGMODE_32_BIT;
                break;

            case X86_CR4_PGE:
                pOut->enmMode = SUPPAGINGMODE_32_BIT_GLOBAL;
                break;

            case X86_CR4_PAE:
                pOut->enmMode = SUPPAGINGMODE_PAE;
                break;

            case X86_CR4_PAE | BIT(0):
                pOut->enmMode = SUPPAGINGMODE_PAE_NX;
                break;

            case X86_CR4_PAE | X86_CR4_PGE:
                pOut->enmMode = SUPPAGINGMODE_PAE_GLOBAL;
                break;

            case X86_CR4_PAE | X86_CR4_PGE | BIT(0):
                pOut->enmMode = SUPPAGINGMODE_PAE_GLOBAL;
                break;

            case BIT(1) | X86_CR4_PAE:
                pOut->enmMode = SUPPAGINGMODE_AMD64;
                break;

            case BIT(1) | X86_CR4_PAE | BIT(0):
                pOut->enmMode = SUPPAGINGMODE_AMD64_NX;
                break;

            case BIT(1) | X86_CR4_PAE | X86_CR4_PGE:
                pOut->enmMode = SUPPAGINGMODE_AMD64_GLOBAL;
                break;

            case BIT(1) | X86_CR4_PAE | X86_CR4_PGE | BIT(0):
                pOut->enmMode = SUPPAGINGMODE_AMD64_GLOBAL_NX;
                break;

            default:
                AssertMsgFailed(("Cannot happen! cr4=%#x fNXEPlusLMA=%d\n", cr4, fNXEPlusLMA));
                pOut->enmMode = SUPPAGINGMODE_INVALID;
                break;
        }
    }
    return 0;
}


#if !defined(SUPDRV_OS_HAVE_LOW) && !defined(USE_NEW_OS_INTERFACE)  /* Use same backend as the contiguous stuff */
/**
 * OS Specific code for allocating page aligned memory with fixed
 * physical backing below 4GB.
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pMem        Memory reference record of the memory to be allocated.
 *                      (This is not linked in anywhere.)
 * @param   ppvR3       Where to store the Ring-0 mapping of the allocated memory.
 * @param   ppvR3       Where to store the Ring-3 mapping of the allocated memory.
 * @param   paPagesOut  Where to store the physical addresss.
 */
int VBOXCALL supdrvOSLowAllocOne(PSUPDRVMEMREF pMem, PRTR0PTR ppvR0, PRTR3PTR ppvR3, PSUPPAGE paPagesOut)
{
    RTHCPHYS HCPhys;
    int rc = supdrvOSContAllocOne(pMem, ppvR0, ppvR3, &HCPhys);
    if (!rc)
    {
        unsigned iPage = pMem->cb >> PAGE_SHIFT;
        while (iPage-- > 0)
        {
            paPagesOut[iPage].Phys = HCPhys + (iPage << PAGE_SHIFT);
            paPagesOut[iPage].uReserved = 0;
        }
    }
    return rc;
}


/**
 * Frees low memory.
 *
 * @param   pMem    Memory reference record of the memory to be freed.
 */
void VBOXCALL supdrvOSLowFreeOne(PSUPDRVMEMREF pMem)
{
    supdrvOSContFreeOne(pMem);
}
#endif /* !SUPDRV_OS_HAVE_LOW */


#ifdef USE_NEW_OS_INTERFACE
/**
 * Creates the GIP.
 *
 * @returns negative errno.
 * @param   pDevExt     Instance data. GIP stuff may be updated.
 */
static int supdrvGipCreate(PSUPDRVDEVEXT pDevExt)
{
    PSUPGLOBALINFOPAGE pGip;
    RTHCPHYS HCPhysGip;
    uint32_t u32SystemResolution;
    uint32_t u32Interval;
    int rc;

    dprintf(("supdrvGipCreate:\n"));

    /* assert order */
    Assert(pDevExt->u32SystemTimerGranularityGrant == 0);
    Assert(pDevExt->GipMemObj == NIL_RTR0MEMOBJ);
    Assert(!pDevExt->pGipTimer);

    /*
     * Allocate a suitable page with a default kernel mapping.
     */
    rc = RTR0MemObjAllocLow(&pDevExt->GipMemObj, PAGE_SIZE, false);
    if (RT_FAILURE(rc))
    {
        OSDBGPRINT(("supdrvGipCreate: failed to allocate the GIP page. rc=%d\n", rc));
        return rc;
    }
    pGip = (PSUPGLOBALINFOPAGE)RTR0MemObjAddress(pDevExt->GipMemObj); AssertPtr(pGip);
    HCPhysGip = RTR0MemObjGetPagePhysAddr(pDevExt->GipMemObj, 0); Assert(HCPhysGip != NIL_RTHCPHYS);

    /*
     * Try bump up the system timer resolution.
     * The more interrupts the better...
     */
    if (   RT_SUCCESS(RTTimerRequestSystemGranularity(  976563 /* 1024 HZ */, &u32SystemResolution))
        || RT_SUCCESS(RTTimerRequestSystemGranularity( 1000000 /* 1000 HZ */, &u32SystemResolution))
        || RT_SUCCESS(RTTimerRequestSystemGranularity( 3906250 /*  256 HZ */, &u32SystemResolution))
        || RT_SUCCESS(RTTimerRequestSystemGranularity( 4000000 /*  250 HZ */, &u32SystemResolution))
        || RT_SUCCESS(RTTimerRequestSystemGranularity( 7812500 /*  128 HZ */, &u32SystemResolution))
        || RT_SUCCESS(RTTimerRequestSystemGranularity(10000000 /*  100 HZ */, &u32SystemResolution))
        || RT_SUCCESS(RTTimerRequestSystemGranularity(15625000 /*   64 HZ */, &u32SystemResolution))
        || RT_SUCCESS(RTTimerRequestSystemGranularity(31250000 /*   32 HZ */, &u32SystemResolution))
       )
    {
        Assert(RTTimerGetSystemGranularity() <= u32SystemResolution);
        pDevExt->u32SystemTimerGranularityGrant = u32SystemResolution;
    }

    /*
     * Find a reasonable update interval, something close to 10ms would be nice,
     * and create a recurring timer.
     */
    u32Interval = u32SystemResolution = RTTimerGetSystemGranularity();
    while (u32Interval < 10000000 /* 10 ms */)
        u32Interval += u32SystemResolution;

    rc = RTTimerCreateEx(&pDevExt->pGipTimer, u32Interval, 0, supdrvGipTimer, pDevExt);
    if (RT_FAILURE(rc))
    {
        OSDBGPRINT(("supdrvGipCreate: failed create GIP timer at %RU32 ns interval. rc=%d\n", u32Interval, rc));
        Assert(!pDevExt->pGipTimer);
        supdrvGipDestroy(pDevExt);
        return rc;
    }

    /*
     * We're good.
     */
    supdrvGipInit(pDevExt, pGip, HCPhysGip, RTTimeSystemNanoTS(), 1000000000 / u32Interval /*=Hz*/);
    return 0;
}


/**
 * Terminates the GIP.
 *
 * @returns negative errno.
 * @param   pDevExt     Instance data. GIP stuff may be updated.
 */
static int supdrvGipDestroy(PSUPDRVDEVEXT pDevExt)
{
    int rc;
#ifdef DEBUG_DARWIN_GIP
    OSDBGPRINT(("supdrvGipDestroy: pDevExt=%p pGip=%p pGipTimer=%p GipMemObj=%p\n", pDevExt, 
                pDevExt->GipMemObj != NIL_RTR0MEMOBJ ? RTR0MemObjAddress(pDevExt->GipMemObj) : NULL, 
                pDevExt->pGipTimer, pDevExt->GipMemObj));
#endif

    /*
     * Invalid the GIP data.
     */
    if (pDevExt->pGip)
    {
        supdrvGipTerm(pDevExt->pGip);
        pDevExt->pGip = 0;
    }

    /*
     * Destroy the timer and free the GIP memory object.
     */
    if (pDevExt->pGipTimer)
    {
        rc = RTTimerDestroy(pDevExt->pGipTimer); AssertRC(rc);
        pDevExt->pGipTimer = NULL;
    }

    if (pDevExt->GipMemObj != NIL_RTR0MEMOBJ)
    {
        rc = RTR0MemObjFree(pDevExt->GipMemObj, true /* free mappings */); AssertRC(rc);
        pDevExt->GipMemObj = NIL_RTR0MEMOBJ;
    }

    /*
     * Finally, release the system timer resolution request if one succeeded.
     */
    if (pDevExt->u32SystemTimerGranularityGrant)
    {
        rc = RTTimerReleaseSystemGranularity(pDevExt->u32SystemTimerGranularityGrant); AssertRC(rc);
        pDevExt->u32SystemTimerGranularityGrant = 0;
    }

    return 0;
}


/**
 * Timer callback function.
 * @param   pTimer      The timer.
 * @param   pvUser      The device extension.
 */
static DECLCALLBACK(void) supdrvGipTimer(PRTTIMER pTimer, void *pvUser)
{
    PSUPDRVDEVEXT pDevExt  = (PSUPDRVDEVEXT)pvUser;
    supdrvGipUpdate(pDevExt->pGip, RTTimeSystemNanoTS());
}
#endif /* USE_NEW_OS_INTERFACE */


/**
 * Initializes the GIP data.
 *
 * @returns VBox status code.
 * @param   pDevExt     Pointer to the device instance data.
 * @param   pGip        Pointer to the read-write kernel mapping of the GIP.
 * @param   HCPhys      The physical address of the GIP.
 * @param   u64NanoTS   The current nanosecond timestamp.
 * @param   uUpdateHz   The update freqence.
 */
int VBOXCALL supdrvGipInit(PSUPDRVDEVEXT pDevExt, PSUPGLOBALINFOPAGE pGip, RTHCPHYS HCPhys, uint64_t u64NanoTS, unsigned uUpdateHz)
{
    unsigned i;
#ifdef DEBUG_DARWIN_GIP
    OSDBGPRINT(("supdrvGipInit: pGip=%p HCPhys=%lx u64NanoTS=%llu uUpdateHz=%d\n", pGip, (long)HCPhys, u64NanoTS, uUpdateHz));
#else
    dprintf(("supdrvGipInit: pGip=%p HCPhys=%lx u64NanoTS=%llu uUpdateHz=%d\n", pGip, (long)HCPhys, u64NanoTS, uUpdateHz));
#endif

    /*
     * Initialize the structure.
     */
    memset(pGip, 0, PAGE_SIZE);
    pGip->u32Magic          = SUPGLOBALINFOPAGE_MAGIC;
    pGip->u32Version        = SUPGLOBALINFOPAGE_VERSION;
    pGip->u32Mode           = supdrvGipDeterminTscMode();
    pGip->u32UpdateHz       = uUpdateHz;
    pGip->u32UpdateIntervalNS = 1000000000 / uUpdateHz;
    pGip->u64NanoTSLastUpdateHz = u64NanoTS;

    for (i = 0; i < RT_ELEMENTS(pGip->aCPUs); i++)
    {
        pGip->aCPUs[i].u32TransactionId  = 2;
        pGip->aCPUs[i].u64NanoTS         = u64NanoTS;
        pGip->aCPUs[i].u64TSC            = ASMReadTSC();

        /*
         * We don't know the following values until we've executed updates.
         * So, we'll just insert very high values.
         */
        pGip->aCPUs[i].u64CpuHz          = _4G + 1;
        pGip->aCPUs[i].u32UpdateIntervalTSC = _2G / 4;
        pGip->aCPUs[i].au32TSCHistory[0] = _2G / 4;
        pGip->aCPUs[i].au32TSCHistory[1] = _2G / 4;
        pGip->aCPUs[i].au32TSCHistory[2] = _2G / 4;
        pGip->aCPUs[i].au32TSCHistory[3] = _2G / 4;
        pGip->aCPUs[i].au32TSCHistory[4] = _2G / 4;
        pGip->aCPUs[i].au32TSCHistory[5] = _2G / 4;
        pGip->aCPUs[i].au32TSCHistory[6] = _2G / 4;
        pGip->aCPUs[i].au32TSCHistory[7] = _2G / 4;
    }

    /*
     * Link it to the device extension.
     */
    pDevExt->pGip = pGip;
    pDevExt->HCPhysGip = HCPhys;
    pDevExt->cGipUsers = 0;

    return 0;
}


/**
 * Determin the GIP TSC mode.
 *
 * @returns The most suitable TSC mode.
 */
static SUPGIPMODE supdrvGipDeterminTscMode(void)
{
#ifndef USE_NEW_OS_INTERFACE
    /*
     * The problem here is that AMD processors with power management features
     * may easily end up with different TSCs because the CPUs or even cores 
     * on the same physical chip run at different frequencies to save power.
     *
     * It is rumoured that this will be corrected with Barcelona and it's 
     * expected that this will be indicated by the TscInvariant bit in 
     * cpuid(0x80000007). So, the "difficult" bit here is to correctly 
     * identify the older CPUs which don't do different frequency and
     * can be relied upon to have somewhat uniform TSC between the cpus.
     */
    if (supdrvOSGetCPUCount() > 1)
    {
        uint32_t uEAX, uEBX, uECX, uEDX;

        /* Permit user users override. */
        if (supdrvOSGetForcedAsyncTscMode())
            return SUPGIPMODE_ASYNC_TSC;
    
        /* Check for "AuthenticAMD" */
        ASMCpuId(0, &uEAX, &uEBX, &uECX, &uEDX);
        if (uEAX >= 1 && uEBX == 0x68747541 && uECX == 0x444d4163 && uEDX == 0x69746e65)
        {
            /* Check for APM support and that TscInvariant is cleared. */
            ASMCpuId(0x80000000, &uEAX, &uEBX, &uECX, &uEDX);
            if (uEAX >= 0x80000007)
            {
                ASMCpuId(0x80000007, &uEAX, &uEBX, &uECX, &uEDX);
                if (    !(uEDX & BIT(8))/* TscInvariant */
                    &&  (uEDX & 0x3e))  /* STC|TM|THERMTRIP|VID|FID. Ignore TS. */
                    return SUPGIPMODE_ASYNC_TSC;
            }
        }
    }
#endif
    return SUPGIPMODE_SYNC_TSC;
}


/**
 * Invalidates the GIP data upon termination.
 *
 * @param   pGip        Pointer to the read-write kernel mapping of the GIP.
 */
void VBOXCALL supdrvGipTerm(PSUPGLOBALINFOPAGE pGip)
{
    unsigned i;
    pGip->u32Magic = 0;
    for (i = 0; i < RT_ELEMENTS(pGip->aCPUs); i++)
    {
        pGip->aCPUs[i].u64NanoTS = 0;
        pGip->aCPUs[i].u64TSC = 0;
        pGip->aCPUs[i].iTSCHistoryHead = 0;
    }
}


/**
 * Worker routine for supdrvGipUpdate and supdrvGipUpdatePerCpu that
 * updates all the per cpu data except the transaction id.
 *
 * @param   pGip            The GIP.
 * @param   pGipCpu         Pointer to the per cpu data.
 * @param   u64NanoTS       The current time stamp.
 */
static void supdrvGipDoUpdateCpu(PSUPGLOBALINFOPAGE pGip, PSUPGIPCPU pGipCpu, uint64_t u64NanoTS)
{
    uint64_t    u64TSC;
    uint64_t    u64TSCDelta;
    uint32_t    u32UpdateIntervalTSC;
    uint32_t    u32UpdateIntervalTSCSlack;
    unsigned    iTSCHistoryHead;
    uint64_t    u64CpuHz;

    /*
     * Update the NanoTS.
     */
    ASMAtomicXchgU64(&pGipCpu->u64NanoTS, u64NanoTS);

    /*
     * Calc TSC delta.
     */
    /** @todo validate the NanoTS delta, don't trust the OS to call us when it should... */
    u64TSC = ASMReadTSC();
    u64TSCDelta = u64TSC - pGipCpu->u64TSC;
    ASMAtomicXchgU64(&pGipCpu->u64TSC, u64TSC);

    if (u64TSCDelta >> 32)
    {
        u64TSCDelta = pGipCpu->u32UpdateIntervalTSC;
        pGipCpu->cErrors++;
    }

    /*
     * TSC History.
     */
    Assert(ELEMENTS(pGipCpu->au32TSCHistory) == 8);

    iTSCHistoryHead = (pGipCpu->iTSCHistoryHead + 1) & 7;
    ASMAtomicXchgU32(&pGipCpu->iTSCHistoryHead, iTSCHistoryHead);
    ASMAtomicXchgU32(&pGipCpu->au32TSCHistory[iTSCHistoryHead], (uint32_t)u64TSCDelta);

    /*
     * UpdateIntervalTSC = average of last 8,2,1 intervals depending on update HZ.
     */
    if (pGip->u32UpdateHz >= 1000)
    {
        uint32_t u32;
        u32  = pGipCpu->au32TSCHistory[0];
        u32 += pGipCpu->au32TSCHistory[1];
        u32 += pGipCpu->au32TSCHistory[2];
        u32 += pGipCpu->au32TSCHistory[3];
        u32 >>= 2;
        u32UpdateIntervalTSC  = pGipCpu->au32TSCHistory[4];
        u32UpdateIntervalTSC += pGipCpu->au32TSCHistory[5];
        u32UpdateIntervalTSC += pGipCpu->au32TSCHistory[6];
        u32UpdateIntervalTSC += pGipCpu->au32TSCHistory[7];
        u32UpdateIntervalTSC >>= 2;
        u32UpdateIntervalTSC += u32;
        u32UpdateIntervalTSC >>= 1;

        /* Value choosen for a 2GHz Athlon64 running linux 2.6.10/11, . */
        u32UpdateIntervalTSCSlack = u32UpdateIntervalTSC >> 14;
    }
    else if (pGip->u32UpdateHz >= 90)
    {
        u32UpdateIntervalTSC  = (uint32_t)u64TSCDelta;
        u32UpdateIntervalTSC += pGipCpu->au32TSCHistory[(iTSCHistoryHead - 1) & 7];
        u32UpdateIntervalTSC >>= 1;

        /* value choosen on a 2GHz thinkpad running windows */
        u32UpdateIntervalTSCSlack = u32UpdateIntervalTSC >> 7;
    }
    else
    {
        u32UpdateIntervalTSC  = (uint32_t)u64TSCDelta;

        /* This value hasn't be checked yet.. waiting for OS/2 and 33Hz timers.. :-) */
        u32UpdateIntervalTSCSlack = u32UpdateIntervalTSC >> 6;
    }
    ASMAtomicXchgU32(&pGipCpu->u32UpdateIntervalTSC, u32UpdateIntervalTSC + u32UpdateIntervalTSCSlack);

    /*
     * CpuHz.
     */
    u64CpuHz = ASMMult2xU32RetU64(u32UpdateIntervalTSC, pGip->u32UpdateHz);
    ASMAtomicXchgU64(&pGipCpu->u64CpuHz, u64CpuHz);
}


/**
 * Updates the GIP.
 *
 * @param   pGip        Pointer to the GIP.
 * @param   u64NanoTS   The current nanosecond timesamp.
 */
void VBOXCALL   supdrvGipUpdate(PSUPGLOBALINFOPAGE pGip, uint64_t u64NanoTS)
{
    /*
     * Determin the relevant CPU data.
     */
    PSUPGIPCPU pGipCpu;
    if (pGip->u32Mode != SUPGIPMODE_ASYNC_TSC)
        pGipCpu = &pGip->aCPUs[0];
    else
    {
        unsigned iCpu = ASMGetApicId();
        if (RT_LIKELY(iCpu >= RT_ELEMENTS(pGip->aCPUs)))
            return;
        pGipCpu = &pGip->aCPUs[iCpu];
    }

    /*
     * Start update transaction.
     */
    if (!(ASMAtomicIncU32(&pGipCpu->u32TransactionId) & 1))
    {
        /* this can happen on win32 if we're taking to long and there are more CPUs around. shouldn't happen though. */
        AssertMsgFailed(("Invalid transaction id, %#x, not odd!\n", pGipCpu->u32TransactionId));
        ASMAtomicIncU32(&pGipCpu->u32TransactionId);
        pGipCpu->cErrors++;
        return;
    }

    /*
     * Recalc the update frequency every 0x800th time.
     */
    if (!(pGipCpu->u32TransactionId & (GIP_UPDATEHZ_RECALC_FREQ * 2 - 2)))
    {
        if (pGip->u64NanoTSLastUpdateHz)
        {
#ifdef __AMD64__ /** @todo fix 64-bit div here to work on x86 linux. */
            uint64_t u64Delta = u64NanoTS - pGip->u64NanoTSLastUpdateHz;
            uint32_t u32UpdateHz = (uint32_t)((UINT64_C(1000000000) * GIP_UPDATEHZ_RECALC_FREQ) / u64Delta);
            if (u32UpdateHz <= 2000 && u32UpdateHz >= 30)
            {
                ASMAtomicXchgU32(&pGip->u32UpdateHz, u32UpdateHz);
                ASMAtomicXchgU32(&pGip->u32UpdateIntervalNS, 1000000000 / u32UpdateHz);
            }
#endif
        }
        ASMAtomicXchgU64(&pGip->u64NanoTSLastUpdateHz, u64NanoTS);
    }

    /*
     * Update the data.
     */
    supdrvGipDoUpdateCpu(pGip, pGipCpu, u64NanoTS);

    /*
     * Complete transaction.
     */
    ASMAtomicIncU32(&pGipCpu->u32TransactionId);
}


/**
 * Updates the per cpu GIP data for the calling cpu.
 *
 * @param   pGip        Pointer to the GIP.
 * @param   u64NanoTS   The current nanosecond timesamp.
 * @param   iCpu        The CPU index.
 */
void VBOXCALL   supdrvGipUpdatePerCpu(PSUPGLOBALINFOPAGE pGip, uint64_t u64NanoTS, unsigned iCpu)
{
    PSUPGIPCPU  pGipCpu;

    if (RT_LIKELY(iCpu <= RT_ELEMENTS(pGip->aCPUs)))
    {
        pGipCpu = &pGip->aCPUs[iCpu];

        /*
         * Start update transaction.
         */
        if (!(ASMAtomicIncU32(&pGipCpu->u32TransactionId) & 1))
        {
            AssertMsgFailed(("Invalid transaction id, %#x, not odd!\n", pGipCpu->u32TransactionId));
            ASMAtomicIncU32(&pGipCpu->u32TransactionId);
            pGipCpu->cErrors++;
            return;
        }

        /*
         * Update the data.
         */
        supdrvGipDoUpdateCpu(pGip, pGipCpu, u64NanoTS);

        /*
         * Complete transaction.
         */
        ASMAtomicIncU32(&pGipCpu->u32TransactionId);
    }
}


#ifndef DEBUG /** @todo change #ifndef DEBUG -> #ifdef LOG_ENABLED */
/**
 * Stub function for non-debug builds.
 */
RTDECL(PRTLOGGER) RTLogDefaultInstance(void)
{
    return NULL;
}

RTDECL(PRTLOGGER) RTLogRelDefaultInstance(void)
{
    return NULL;
}

/**
 * Stub function for non-debug builds.
 */
RTDECL(int) RTLogSetDefaultInstanceThread(PRTLOGGER pLogger, uintptr_t uKey)
{
    return 0;
}

/**
 * Stub function for non-debug builds.
 */
RTDECL(void) RTLogLogger(PRTLOGGER pLogger, void *pvCallerRet, const char *pszFormat, ...)
{
}

/**
 * Stub function for non-debug builds.
 */
RTDECL(void) RTLogLoggerEx(PRTLOGGER pLogger, unsigned fFlags, unsigned iGroup, const char *pszFormat, ...)
{
}

/**
 * Stub function for non-debug builds.
 */
RTDECL(void) RTLogLoggerExV(PRTLOGGER pLogger, unsigned fFlags, unsigned iGroup, const char *pszFormat, va_list args)
{
}
#endif /* !DEBUG */

