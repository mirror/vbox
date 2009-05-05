/* $Id$ */
/** @file
 * GVMM - Global VM Manager.
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/** @page pg_gvmm   GVMM - The Global VM Manager
 *
 * The Global VM Manager lives in ring-0. It's main function at the moment
 * is to manage a list of all running VMs, keep a ring-0 only structure (GVM)
 * for each of them, and assign them unique identifiers (so GMM can track
 * page owners). The idea for the future is to add an idle priority kernel
 * thread that can take care of tasks like page sharing.
 *
 * The GVMM will create a ring-0 object for each VM when it's registered,
 * this is both for session cleanup purposes and for having a point where
 * it's possible to implement usage polices later (in SUPR0ObjRegister).
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_GVMM
#include <VBox/gvmm.h>
#include <VBox/gmm.h>
#include "GVMMR0Internal.h"
#include <VBox/gvm.h>
#include <VBox/vm.h>
#include <VBox/vmm.h>
#include <VBox/err.h>
#include <iprt/alloc.h>
#include <iprt/semaphore.h>
#include <iprt/time.h>
#include <VBox/log.h>
#include <iprt/thread.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/memobj.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * Global VM handle.
 */
typedef struct GVMHANDLE
{
    /** The index of the next handle in the list (free or used). (0 is nil.) */
    uint16_t volatile   iNext;
    /** Our own index / handle value. */
    uint16_t            iSelf;
    /** The pointer to the ring-0 only (aka global) VM structure. */
    PGVM                pGVM;
    /** The ring-0 mapping of the shared VM instance data. */
    PVM                 pVM;
    /** The virtual machine object. */
    void               *pvObj;
    /** The session this VM is associated with. */
    PSUPDRVSESSION      pSession;
    /** The ring-0 handle of the EMT thread (VCPU 0).
     * This is used for assertions and similar cases where we need to find the VM handle. */
    RTNATIVETHREAD      hEMTCpu0;
} GVMHANDLE;
/** Pointer to a global VM handle. */
typedef GVMHANDLE *PGVMHANDLE;

/** Number of GVM handles (including the NIL handle). */
#if HC_ARCH_BITS == 64
# define GVMM_MAX_HANDLES   1024
#else
# define GVMM_MAX_HANDLES   128
#endif

/**
 * The GVMM instance data.
 */
typedef struct GVMM
{
    /** Eyecatcher / magic. */
    uint32_t            u32Magic;
    /** The index of the head of the free handle chain. (0 is nil.) */
    uint16_t volatile   iFreeHead;
    /** The index of the head of the active handle chain. (0 is nil.) */
    uint16_t volatile   iUsedHead;
    /** The number of VMs. */
    uint16_t volatile   cVMs;
//    /** The number of halted EMT threads. */
//    uint16_t volatile   cHaltedEMTs;
    /** The lock used to serialize VM creation, destruction and associated events that
     * isn't performance critical. Owners may acquire the list lock. */
    RTSEMFASTMUTEX      CreateDestroyLock;
    /** The lock used to serialize used list updates and accesses.
     * This indirectly includes scheduling since the scheduler will have to walk the
     * used list to examin running VMs. Owners may not acquire any other locks. */
    RTSEMFASTMUTEX      UsedLock;
    /** The handle array.
     * The size of this array defines the maximum number of currently running VMs.
     * The first entry is unused as it represents the NIL handle. */
    GVMHANDLE           aHandles[GVMM_MAX_HANDLES];

    /** @gcfgm{/GVMM/cVMsMeansCompany, 32-bit, 0, UINT32_MAX, 1}
     * The number of VMs that means we no longer consider ourselves alone on a CPU/Core.
     */
    uint32_t            cVMsMeansCompany;
    /** @gcfgm{/GVMM/MinSleepAlone,32-bit, 0, 100000000, 750000, ns}
     * The minimum sleep time for when we're alone, in nano seconds.
     */
    uint32_t            nsMinSleepAlone;
    /** @gcfgm{/GVMM/MinSleepCompany,32-bit,0, 100000000, 15000, ns}
     * The minimum sleep time for when we've got company, in nano seconds.
     */
    uint32_t            nsMinSleepCompany;
    /** @gcfgm{/GVMM/EarlyWakeUp1, 32-bit, 0, 100000000, 25000, ns}
     * The limit for the first round of early wakeups, given in nano seconds.
     */
    uint32_t            nsEarlyWakeUp1;
    /** @gcfgm{/GVMM/EarlyWakeUp2, 32-bit, 0, 100000000, 50000, ns}
     * The limit for the second round of early wakeups, given in nano seconds.
     */
    uint32_t            nsEarlyWakeUp2;
} GVMM;
/** Pointer to the GVMM instance data. */
typedef GVMM *PGVMM;

/** The GVMM::u32Magic value (Charlie Haden). */
#define GVMM_MAGIC      0x19370806



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Pointer to the GVMM instance data.
 * (Just my general dislike for global variables.) */
static PGVMM g_pGVMM = NULL;

/** Macro for obtaining and validating the g_pGVMM pointer.
 * On failure it will return from the invoking function with the specified return value.
 *
 * @param   pGVMM   The name of the pGVMM variable.
 * @param   rc      The return value on failure. Use VERR_INTERNAL_ERROR for
 *                  VBox status codes.
 */
#define GVMM_GET_VALID_INSTANCE(pGVMM, rc) \
    do { \
        (pGVMM) = g_pGVMM;\
        AssertPtrReturn((pGVMM), (rc)); \
        AssertMsgReturn((pGVMM)->u32Magic == GVMM_MAGIC, ("%p - %#x\n", (pGVMM), (pGVMM)->u32Magic), (rc)); \
    } while (0)

/** Macro for obtaining and validating the g_pGVMM pointer, void function variant.
 * On failure it will return from the invoking function.
 *
 * @param   pGVMM   The name of the pGVMM variable.
 */
#define GVMM_GET_VALID_INSTANCE_VOID(pGVMM) \
    do { \
        (pGVMM) = g_pGVMM;\
        AssertPtrReturnVoid((pGVMM)); \
        AssertMsgReturnVoid((pGVMM)->u32Magic == GVMM_MAGIC, ("%p - %#x\n", (pGVMM), (pGVMM)->u32Magic)); \
    } while (0)


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void gvmmR0InitPerVMData(PGVM pGVM);
static DECLCALLBACK(void) gvmmR0HandleObjDestructor(void *pvObj, void *pvGVMM, void *pvHandle);
static int gvmmR0ByVM(PVM pVM, PGVM *ppGVM, PGVMM *ppGVMM, bool fTakeUsedLock);
static int gvmmR0ByVMAndEMT(PVM pVM, VMCPUID idCpu, PGVM *ppGVM, PGVMM *ppGVMM);


/**
 * Initializes the GVMM.
 *
 * This is called while owninng the loader sempahore (see supdrvIOCtl_LdrLoad()).
 *
 * @returns VBox status code.
 */
GVMMR0DECL(int) GVMMR0Init(void)
{
    LogFlow(("GVMMR0Init:\n"));

    /*
     * Allocate and initialize the instance data.
     */
    PGVMM pGVMM = (PGVMM)RTMemAllocZ(sizeof(*pGVMM));
    if (!pGVMM)
        return VERR_NO_MEMORY;
    int rc = RTSemFastMutexCreate(&pGVMM->CreateDestroyLock);
    if (RT_SUCCESS(rc))
    {
        rc = RTSemFastMutexCreate(&pGVMM->UsedLock);
        if (RT_SUCCESS(rc))
        {
            pGVMM->u32Magic = GVMM_MAGIC;
            pGVMM->iUsedHead = 0;
            pGVMM->iFreeHead = 1;

            /* the nil handle */
            pGVMM->aHandles[0].iSelf = 0;
            pGVMM->aHandles[0].iNext = 0;

            /* the tail */
            unsigned i = RT_ELEMENTS(pGVMM->aHandles) - 1;
            pGVMM->aHandles[i].iSelf = i;
            pGVMM->aHandles[i].iNext = 0; /* nil */

            /* the rest */
            while (i-- > 1)
            {
                pGVMM->aHandles[i].iSelf = i;
                pGVMM->aHandles[i].iNext = i + 1;
            }

            /* The default configuration values. */
            pGVMM->cVMsMeansCompany  = 1;                           /** @todo should be adjusted to relative to the cpu count or something... */
            pGVMM->nsMinSleepAlone   = 750000 /* ns (0.750 ms) */;  /** @todo this should be adjusted to be 75% (or something) of the scheduler granularity... */
            pGVMM->nsMinSleepCompany =  15000 /* ns (0.015 ms) */;
            pGVMM->nsEarlyWakeUp1    =  25000 /* ns (0.025 ms) */;
            pGVMM->nsEarlyWakeUp2    =  50000 /* ns (0.050 ms) */;

            g_pGVMM = pGVMM;
            LogFlow(("GVMMR0Init: pGVMM=%p\n", pGVMM));
            return VINF_SUCCESS;
        }

        RTSemFastMutexDestroy(pGVMM->CreateDestroyLock);
    }

    RTMemFree(pGVMM);
    return rc;
}


/**
 * Terminates the GVM.
 *
 * This is called while owning the loader semaphore (see supdrvLdrFree()).
 * And unless something is wrong, there should be absolutely no VMs
 * registered at this point.
 */
GVMMR0DECL(void) GVMMR0Term(void)
{
    LogFlow(("GVMMR0Term:\n"));

    PGVMM pGVMM = g_pGVMM;
    g_pGVMM = NULL;
    if (RT_UNLIKELY(!VALID_PTR(pGVMM)))
    {
        SUPR0Printf("GVMMR0Term: pGVMM=%p\n", pGVMM);
        return;
    }

    pGVMM->u32Magic++;

    RTSemFastMutexDestroy(pGVMM->UsedLock);
    pGVMM->UsedLock = NIL_RTSEMFASTMUTEX;
    RTSemFastMutexDestroy(pGVMM->CreateDestroyLock);
    pGVMM->CreateDestroyLock = NIL_RTSEMFASTMUTEX;

    pGVMM->iFreeHead = 0;
    if (pGVMM->iUsedHead)
    {
        SUPR0Printf("GVMMR0Term: iUsedHead=%#x! (cVMs=%#x)\n", pGVMM->iUsedHead, pGVMM->cVMs);
        pGVMM->iUsedHead = 0;
    }

    RTMemFree(pGVMM);
}


/**
 * A quick hack for setting global config values.
 *
 * @returns VBox status code.
 *
 * @param   pSession    The session handle. Used for authentication.
 * @param   pszName     The variable name.
 * @param   u64Value    The new value.
 */
GVMMR0DECL(int) GVMMR0SetConfig(PSUPDRVSESSION pSession, const char *pszName, uint64_t u64Value)
{
    /*
     * Validate input.
     */
    PGVMM pGVMM;
    GVMM_GET_VALID_INSTANCE(pGVMM, VERR_INTERNAL_ERROR);
    AssertPtrReturn(pSession, VERR_INVALID_HANDLE);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);

    /*
     * String switch time!
     */
    if (strncmp(pszName, "/GVMM/", sizeof("/GVMM/") - 1))
        return VERR_CFGM_VALUE_NOT_FOUND; /* borrow status codes from CFGM... */
    int rc = VINF_SUCCESS;
    pszName += sizeof("/GVMM/") - 1;
    if (!strcmp(pszName, "cVMsMeansCompany"))
    {
        if (u64Value <= UINT32_MAX)
            pGVMM->cVMsMeansCompany = u64Value;
        else
            rc = VERR_OUT_OF_RANGE;
    }
    else if (!strcmp(pszName, "MinSleepAlone"))
    {
        if (u64Value <= 100000000)
            pGVMM->nsMinSleepAlone = u64Value;
        else
            rc = VERR_OUT_OF_RANGE;
    }
    else if (!strcmp(pszName, "MinSleepCompany"))
    {
        if (u64Value <= 100000000)
            pGVMM->nsMinSleepCompany = u64Value;
        else
            rc = VERR_OUT_OF_RANGE;
    }
    else if (!strcmp(pszName, "EarlyWakeUp1"))
    {
        if (u64Value <= 100000000)
            pGVMM->nsEarlyWakeUp1 = u64Value;
        else
            rc = VERR_OUT_OF_RANGE;
    }
    else if (!strcmp(pszName, "EarlyWakeUp2"))
    {
        if (u64Value <= 100000000)
            pGVMM->nsEarlyWakeUp2 = u64Value;
        else
            rc = VERR_OUT_OF_RANGE;
    }
    else
        rc = VERR_CFGM_VALUE_NOT_FOUND;
    return rc;
}


/**
 * A quick hack for getting global config values.
 *
 * @returns VBox status code.
 *
 * @param   pSession    The session handle. Used for authentication.
 * @param   pszName     The variable name.
 * @param   u64Value    The new value.
 */
GVMMR0DECL(int) GVMMR0QueryConfig(PSUPDRVSESSION pSession, const char *pszName, uint64_t *pu64Value)
{
    /*
     * Validate input.
     */
    PGVMM pGVMM;
    GVMM_GET_VALID_INSTANCE(pGVMM, VERR_INTERNAL_ERROR);
    AssertPtrReturn(pSession, VERR_INVALID_HANDLE);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertPtrReturn(pu64Value, VERR_INVALID_POINTER);

    /*
     * String switch time!
     */
    if (strncmp(pszName, "/GVMM/", sizeof("/GVMM/") - 1))
        return VERR_CFGM_VALUE_NOT_FOUND; /* borrow status codes from CFGM... */
    int rc = VINF_SUCCESS;
    pszName += sizeof("/GVMM/") - 1;
    if (!strcmp(pszName, "cVMsMeansCompany"))
        *pu64Value = pGVMM->cVMsMeansCompany;
    else if (!strcmp(pszName, "MinSleepAlone"))
        *pu64Value = pGVMM->nsMinSleepAlone;
    else if (!strcmp(pszName, "MinSleepCompany"))
        *pu64Value = pGVMM->nsMinSleepCompany;
    else if (!strcmp(pszName, "EarlyWakeUp1"))
        *pu64Value = pGVMM->nsEarlyWakeUp1;
    else if (!strcmp(pszName, "EarlyWakeUp2"))
        *pu64Value = pGVMM->nsEarlyWakeUp2;
    else
        rc = VERR_CFGM_VALUE_NOT_FOUND;
    return rc;
}


/**
 * Try acquire the 'used' lock.
 *
 * @returns IPRT status code, see RTSemFastMutexRequest.
 * @param   pGVMM   The GVMM instance data.
 */
DECLINLINE(int) gvmmR0UsedLock(PGVMM pGVMM)
{
    LogFlow(("++gvmmR0UsedLock(%p)\n", pGVMM));
    int rc = RTSemFastMutexRequest(pGVMM->UsedLock);
    LogFlow(("gvmmR0UsedLock(%p)->%Rrc\n", pGVMM, rc));
    return rc;
}


/**
 * Release the 'used' lock.
 *
 * @returns IPRT status code, see RTSemFastMutexRelease.
 * @param   pGVMM   The GVMM instance data.
 */
DECLINLINE(int) gvmmR0UsedUnlock(PGVMM pGVMM)
{
    LogFlow(("--gvmmR0UsedUnlock(%p)\n", pGVMM));
    int rc = RTSemFastMutexRelease(pGVMM->UsedLock);
    AssertRC(rc);
    return rc;
}


/**
 * Try acquire the 'create & destroy' lock.
 *
 * @returns IPRT status code, see RTSemFastMutexRequest.
 * @param   pGVMM   The GVMM instance data.
 */
DECLINLINE(int) gvmmR0CreateDestroyLock(PGVMM pGVMM)
{
    LogFlow(("++gvmmR0CreateDestroyLock(%p)\n", pGVMM));
    int rc = RTSemFastMutexRequest(pGVMM->CreateDestroyLock);
    LogFlow(("gvmmR0CreateDestroyLock(%p)->%Rrc\n", pGVMM, rc));
    return rc;
}


/**
 * Release the 'create & destroy' lock.
 *
 * @returns IPRT status code, see RTSemFastMutexRequest.
 * @param   pGVMM   The GVMM instance data.
 */
DECLINLINE(int) gvmmR0CreateDestroyUnlock(PGVMM pGVMM)
{
    LogFlow(("--gvmmR0CreateDestroyUnlock(%p)\n", pGVMM));
    int rc = RTSemFastMutexRelease(pGVMM->CreateDestroyLock);
    AssertRC(rc);
    return rc;
}


/**
 * Request wrapper for the GVMMR0CreateVM API.
 *
 * @returns VBox status code.
 * @param   pReq        The request buffer.
 */
GVMMR0DECL(int) GVMMR0CreateVMReq(PGVMMCREATEVMREQ pReq)
{
    /*
     * Validate the request.
     */
    if (!VALID_PTR(pReq))
        return VERR_INVALID_POINTER;
    if (pReq->Hdr.cbReq != sizeof(*pReq))
        return VERR_INVALID_PARAMETER;
    if (!VALID_PTR(pReq->pSession))
        return VERR_INVALID_POINTER;

    /*
     * Execute it.
     */
    PVM pVM;
    pReq->pVMR0 = NULL;
    pReq->pVMR3 = NIL_RTR3PTR;
    int rc = GVMMR0CreateVM(pReq->pSession, pReq->cCpus, &pVM);
    if (RT_SUCCESS(rc))
    {
        pReq->pVMR0 = pVM;
        pReq->pVMR3 = pVM->pVMR3;
    }
    return rc;
}


/**
 * Allocates the VM structure and registers it with GVM.
 *
 * The caller will become the VM owner and there by the EMT.
 *
 * @returns VBox status code.
 * @param   pSession    The support driver session.
 * @param   cCpus       Number of virtual CPUs for the new VM.
 * @param   ppVM        Where to store the pointer to the VM structure.
 *
 * @thread  EMT.
 */
GVMMR0DECL(int) GVMMR0CreateVM(PSUPDRVSESSION pSession, uint32_t cCpus, PVM *ppVM)
{
    LogFlow(("GVMMR0CreateVM: pSession=%p\n", pSession));
    PGVMM pGVMM;
    GVMM_GET_VALID_INSTANCE(pGVMM, VERR_INTERNAL_ERROR);

    AssertPtrReturn(ppVM, VERR_INVALID_POINTER);
    *ppVM = NULL;

    if (    cCpus == 0
        ||  cCpus > VMCPU_MAX_CPU_COUNT)
        return VERR_INVALID_PARAMETER;

    RTNATIVETHREAD hEMT = RTThreadNativeSelf();
    AssertReturn(hEMT != NIL_RTNATIVETHREAD, VERR_INTERNAL_ERROR);

    /*
     * The whole allocation process is protected by the lock.
     */
    int rc = gvmmR0CreateDestroyLock(pGVMM);
    AssertRCReturn(rc, rc);

    /*
     * Allocate a handle first so we don't waste resources unnecessarily.
     */
    uint16_t iHandle = pGVMM->iFreeHead;
    if (iHandle)
    {
        PGVMHANDLE pHandle = &pGVMM->aHandles[iHandle];

        /* consistency checks, a bit paranoid as always. */
        if (    !pHandle->pVM
            &&  !pHandle->pGVM
            &&  !pHandle->pvObj
            &&  pHandle->iSelf == iHandle)
        {
            pHandle->pvObj = SUPR0ObjRegister(pSession, SUPDRVOBJTYPE_VM, gvmmR0HandleObjDestructor, pGVMM, pHandle);
            if (pHandle->pvObj)
            {
                /*
                 * Move the handle from the free to used list and perform permission checks.
                 */
                rc = gvmmR0UsedLock(pGVMM);
                AssertRC(rc);

                pGVMM->iFreeHead = pHandle->iNext;
                pHandle->iNext = pGVMM->iUsedHead;
                pGVMM->iUsedHead = iHandle;
                pGVMM->cVMs++;

                pHandle->pVM      = NULL;
                pHandle->pGVM     = NULL;
                pHandle->pSession = pSession;
                pHandle->hEMTCpu0 = NIL_RTNATIVETHREAD;

                gvmmR0UsedUnlock(pGVMM);

                rc = SUPR0ObjVerifyAccess(pHandle->pvObj, pSession, NULL);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Allocate the global VM structure (GVM) and initialize it.
                     */
                    PGVM pGVM = (PGVM)RTMemAllocZ(RT_UOFFSETOF(GVM, aCpus[cCpus]));
                    if (pGVM)
                    {
                        pGVM->u32Magic  = GVM_MAGIC;
                        pGVM->hSelf     = iHandle;
                        pGVM->pVM       = NULL;
                        pGVM->cCpus     = cCpus;

                        gvmmR0InitPerVMData(pGVM);
                        GMMR0InitPerVMData(pGVM);

                        /*
                         * Allocate the shared VM structure and associated page array.
                         */
                        const uint32_t  cbVM   = RT_UOFFSETOF(VM, aCpus[cCpus]);
                        const uint32_t  cPages = RT_ALIGN_32(cbVM, PAGE_SIZE) >> PAGE_SHIFT;
                        rc = RTR0MemObjAllocLow(&pGVM->gvmm.s.VMMemObj, cPages << PAGE_SHIFT, false /* fExecutable */);
                        if (RT_SUCCESS(rc))
                        {
                            PVM pVM = (PVM)RTR0MemObjAddress(pGVM->gvmm.s.VMMemObj); AssertPtr(pVM);
                            memset(pVM, 0, cPages << PAGE_SHIFT);
                            pVM->enmVMState = VMSTATE_CREATING;
                            pVM->pVMR0      = pVM;
                            pVM->pSession   = pSession;
                            pVM->hSelf      = iHandle;
                            pVM->cbSelf     = cbVM;
                            pVM->cCPUs      = cCpus;
                            pVM->offVMCPU   = RT_UOFFSETOF(VM, aCpus);

                            rc = RTR0MemObjAllocPage(&pGVM->gvmm.s.VMPagesMemObj, cPages * sizeof(SUPPAGE), false /* fExecutable */);
                            if (RT_SUCCESS(rc))
                            {
                                PSUPPAGE paPages = (PSUPPAGE)RTR0MemObjAddress(pGVM->gvmm.s.VMPagesMemObj); AssertPtr(paPages);
                                for (uint32_t iPage = 0; iPage < cPages; iPage++)
                                {
                                    paPages[iPage].uReserved = 0;
                                    paPages[iPage].Phys = RTR0MemObjGetPagePhysAddr(pGVM->gvmm.s.VMMemObj, iPage);
                                    Assert(paPages[iPage].Phys != NIL_RTHCPHYS);
                                }

                                /*
                                 * Map them into ring-3.
                                 */
                                rc = RTR0MemObjMapUser(&pGVM->gvmm.s.VMMapObj, pGVM->gvmm.s.VMMemObj, (RTR3PTR)-1, 0,
                                                       RTMEM_PROT_READ | RTMEM_PROT_WRITE, NIL_RTR0PROCESS);
                                if (RT_SUCCESS(rc))
                                {
                                    pVM->pVMR3 = RTR0MemObjAddressR3(pGVM->gvmm.s.VMMapObj);
                                    AssertPtr((void *)pVM->pVMR3);

                                    /* Initialize all the VM pointers. */
                                    for (uint32_t i = 0; i < cCpus; i++)
                                    {
                                        pVM->aCpus[i].pVMR0 = pVM;
                                        pVM->aCpus[i].pVMR3 = pVM->pVMR3;
                                    }

                                    rc = RTR0MemObjMapUser(&pGVM->gvmm.s.VMPagesMapObj, pGVM->gvmm.s.VMPagesMemObj, (RTR3PTR)-1, 0,
                                                           RTMEM_PROT_READ | RTMEM_PROT_WRITE, NIL_RTR0PROCESS);
                                    if (RT_SUCCESS(rc))
                                    {
                                        pVM->paVMPagesR3 = RTR0MemObjAddressR3(pGVM->gvmm.s.VMPagesMapObj);
                                        AssertPtr((void *)pVM->paVMPagesR3);

                                        /* complete the handle - take the UsedLock sem just to be careful. */
                                        rc = gvmmR0UsedLock(pGVMM);
                                        AssertRC(rc);

                                        pHandle->pVM        = pVM;
                                        pHandle->pGVM       = pGVM;
                                        pHandle->hEMTCpu0   = hEMT;
                                        pGVM->pVM           = pVM;
                                        pGVM->aCpus[0].hEMT = hEMT;

                                        gvmmR0UsedUnlock(pGVMM);
                                        gvmmR0CreateDestroyUnlock(pGVMM);

                                        *ppVM = pVM;
                                        Log(("GVMMR0CreateVM: pVM=%p pVMR3=%p pGVM=%p hGVM=%d\n", pVM, pVM->pVMR3, pGVM, iHandle));
                                        return VINF_SUCCESS;
                                    }

                                    RTR0MemObjFree(pGVM->gvmm.s.VMMapObj, false /* fFreeMappings */);
                                    pGVM->gvmm.s.VMMapObj = NIL_RTR0MEMOBJ;
                                }
                                RTR0MemObjFree(pGVM->gvmm.s.VMPagesMemObj, false /* fFreeMappings */);
                                pGVM->gvmm.s.VMPagesMemObj = NIL_RTR0MEMOBJ;
                            }
                            RTR0MemObjFree(pGVM->gvmm.s.VMMemObj, false /* fFreeMappings */);
                            pGVM->gvmm.s.VMMemObj = NIL_RTR0MEMOBJ;
                        }
                    }
                }
                /* else: The user wasn't permitted to create this VM. */

                /*
                 * The handle will be freed by gvmmR0HandleObjDestructor as we release the
                 * object reference here. A little extra mess because of non-recursive lock.
                 */
                void *pvObj = pHandle->pvObj;
                pHandle->pvObj = NULL;
                gvmmR0CreateDestroyUnlock(pGVMM);

                SUPR0ObjRelease(pvObj, pSession);

                SUPR0Printf("GVMMR0CreateVM: failed, rc=%d\n", rc);
                return rc;
            }

            rc = VERR_NO_MEMORY;
        }
        else
            rc = VERR_INTERNAL_ERROR;
    }
    else
        rc = VERR_GVM_TOO_MANY_VMS;

    gvmmR0CreateDestroyUnlock(pGVMM);
    return rc;
}


/**
 * Initializes the per VM data belonging to GVMM.
 *
 * @param   pGVM        Pointer to the global VM structure.
 */
static void gvmmR0InitPerVMData(PGVM pGVM)
{
    AssertCompile(RT_SIZEOFMEMB(GVM,gvmm.s) <= RT_SIZEOFMEMB(GVM,gvmm.padding));
    AssertCompile(RT_SIZEOFMEMB(GVMCPU,gvmm.s) <= RT_SIZEOFMEMB(GVMCPU,gvmm.padding));
    pGVM->gvmm.s.VMMemObj = NIL_RTR0MEMOBJ;
    pGVM->gvmm.s.VMMapObj = NIL_RTR0MEMOBJ;
    pGVM->gvmm.s.VMPagesMemObj = NIL_RTR0MEMOBJ;
    pGVM->gvmm.s.VMPagesMapObj = NIL_RTR0MEMOBJ;
    pGVM->gvmm.s.fDoneVMMR0Init = false;
    pGVM->gvmm.s.fDoneVMMR0Term = false;

    for (VMCPUID i = 0; i < pGVM->cCpus; i++)
    {
        pGVM->aCpus[i].gvmm.s.HaltEventMulti = NIL_RTSEMEVENTMULTI;
        pGVM->aCpus[i].hEMT                  = NIL_RTNATIVETHREAD;
    }
}


/**
 * Does the VM initialization.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the shared VM structure.
 */
GVMMR0DECL(int) GVMMR0InitVM(PVM pVM)
{
    LogFlow(("GVMMR0InitVM: pVM=%p\n", pVM));

    /*
     * Validate the VM structure, state and handle.
     */
    PGVM pGVM;
    PGVMM pGVMM;
    int rc = gvmmR0ByVMAndEMT(pVM, 0 /* idCpu */, &pGVM, &pGVMM);
    if (RT_SUCCESS(rc))
    {
        if (   !pGVM->gvmm.s.fDoneVMMR0Init
            && pGVM->aCpus[0].gvmm.s.HaltEventMulti == NIL_RTSEMEVENTMULTI)
        {
            for (VMCPUID i = 0; i < pGVM->cCpus; i++)
            {
                rc = RTSemEventMultiCreate(&pGVM->aCpus[i].gvmm.s.HaltEventMulti);
                if (RT_FAILURE(rc))
                {
                    pGVM->aCpus[i].gvmm.s.HaltEventMulti = NIL_RTSEMEVENTMULTI;
                    break;
                }
            }
        }
        else
            rc = VERR_WRONG_ORDER;
    }

    LogFlow(("GVMMR0InitVM: returns %Rrc\n", rc));
    return rc;
}


/**
 * Indicates that we're done with the ring-0 initialization
 * of the VM.
 *
 * @param   pVM         Pointer to the shared VM structure.
 * @thread  EMT(0)
 */
GVMMR0DECL(void) GVMMR0DoneInitVM(PVM pVM)
{
    /* Validate the VM structure, state and handle. */
    PGVM pGVM;
    PGVMM pGVMM;
    int rc = gvmmR0ByVMAndEMT(pVM, 0 /* idCpu */, &pGVM, &pGVMM);
    AssertRCReturnVoid(rc);

    /* Set the indicator. */
    pGVM->gvmm.s.fDoneVMMR0Init = true;
}


/**
 * Indicates that we're doing the ring-0 termination of the VM.
 *
 * @returns true if termination hasn't been done already, false if it has.
 * @param   pVM         Pointer to the shared VM structure.
 * @param   pGVM        Pointer to the global VM structure. Optional.
 * @thread  EMT(0)
 */
GVMMR0DECL(bool) GVMMR0DoingTermVM(PVM pVM, PGVM pGVM)
{
    /* Validate the VM structure, state and handle. */
    AssertPtrNullReturn(pGVM, false);
    AssertReturn(!pGVM || pGVM->u32Magic == GVM_MAGIC, false);
    if (!pGVM)
    {
        PGVMM pGVMM;
        int rc = gvmmR0ByVMAndEMT(pVM, 0 /* idCpu */, &pGVM, &pGVMM);
        AssertRCReturn(rc, false);
    }

    /* Set the indicator. */
    if (pGVM->gvmm.s.fDoneVMMR0Term)
        return false;
    pGVM->gvmm.s.fDoneVMMR0Term = true;
    return true;
}


/**
 * Destroys the VM, freeing all associated resources (the ring-0 ones anyway).
 *
 * This is call from the vmR3DestroyFinalBit and from a error path in VMR3Create,
 * and the caller is not the EMT thread, unfortunately. For security reasons, it
 * would've been nice if the caller was actually the EMT thread or that we somehow
 * could've associated the calling thread with the VM up front.
 *
 * @returns VBox status code.
 * @param   pVM         Where to store the pointer to the VM structure.
 *
 * @thread  EMT(0) if it's associated with the VM, otherwise any thread.
 */
GVMMR0DECL(int) GVMMR0DestroyVM(PVM pVM)
{
    LogFlow(("GVMMR0DestroyVM: pVM=%p\n", pVM));
    PGVMM pGVMM;
    GVMM_GET_VALID_INSTANCE(pGVMM, VERR_INTERNAL_ERROR);


    /*
     * Validate the VM structure, state and caller.
     */
    AssertPtrReturn(pVM, VERR_INVALID_POINTER);
    AssertReturn(!((uintptr_t)pVM & PAGE_OFFSET_MASK), VERR_INVALID_POINTER);
    AssertMsgReturn(pVM->enmVMState >= VMSTATE_CREATING && pVM->enmVMState <= VMSTATE_TERMINATED, ("%d\n", pVM->enmVMState), VERR_WRONG_ORDER);

    uint32_t hGVM = pVM->hSelf;
    AssertReturn(hGVM != NIL_GVM_HANDLE, VERR_INVALID_HANDLE);
    AssertReturn(hGVM < RT_ELEMENTS(pGVMM->aHandles), VERR_INVALID_HANDLE);

    PGVMHANDLE pHandle = &pGVMM->aHandles[hGVM];
    AssertReturn(pHandle->pVM == pVM, VERR_NOT_OWNER);

    RTNATIVETHREAD hSelf = RTThreadNativeSelf();
    AssertReturn(pHandle->hEMTCpu0 == hSelf || pHandle->hEMTCpu0 == NIL_RTNATIVETHREAD, VERR_NOT_OWNER);

    /*
     * Lookup the handle and destroy the object.
     * Since the lock isn't recursive and we'll have to leave it before dereferencing the
     * object, we take some precautions against racing callers just in case...
     */
    int rc = gvmmR0CreateDestroyLock(pGVMM);
    AssertRC(rc);

    /* be careful here because we might theoretically be racing someone else cleaning up. */
    if (    pHandle->pVM == pVM
        &&  (   pHandle->hEMTCpu0 == hSelf
             || pHandle->hEMTCpu0 == NIL_RTNATIVETHREAD)
        &&  VALID_PTR(pHandle->pvObj)
        &&  VALID_PTR(pHandle->pSession)
        &&  VALID_PTR(pHandle->pGVM)
        &&  pHandle->pGVM->u32Magic == GVM_MAGIC)
    {
        void *pvObj = pHandle->pvObj;
        pHandle->pvObj = NULL;
        gvmmR0CreateDestroyUnlock(pGVMM);

        SUPR0ObjRelease(pvObj, pHandle->pSession);
    }
    else
    {
        SUPR0Printf("GVMMR0DestroyVM: pHandle=%p:{.pVM=%p, hEMTCpu0=%p, .pvObj=%p} pVM=%p hSelf=%p\n",
                    pHandle, pHandle->pVM, pHandle->hEMTCpu0, pHandle->pvObj, pVM, hSelf);
        gvmmR0CreateDestroyUnlock(pGVMM);
        rc = VERR_INTERNAL_ERROR;
    }

    return rc;
}


/**
 * Performs VM cleanup task as part of object destruction.
 *
 * @param   pGVM        The GVM pointer.
 */
static void gvmmR0CleanupVM(PGVM pGVM)
{
    if (    pGVM->gvmm.s.fDoneVMMR0Init
        &&  !pGVM->gvmm.s.fDoneVMMR0Term)
    {
        if (    pGVM->gvmm.s.VMMemObj != NIL_RTR0MEMOBJ
            &&  RTR0MemObjAddress(pGVM->gvmm.s.VMMemObj) == pGVM->pVM)
        {
            LogFlow(("gvmmR0CleanupVM: Calling VMMR0TermVM\n"));
            VMMR0TermVM(pGVM->pVM, pGVM);
        }
        else
            AssertMsgFailed(("gvmmR0CleanupVM: VMMemObj=%p pVM=%p\n", pGVM->gvmm.s.VMMemObj, pGVM->pVM));
    }

    GMMR0CleanupVM(pGVM);
}


/**
 * Handle destructor.
 *
 * @param   pvGVMM      The GVM instance pointer.
 * @param   pvHandle    The handle pointer.
 */
static DECLCALLBACK(void) gvmmR0HandleObjDestructor(void *pvObj, void *pvGVMM, void *pvHandle)
{
    LogFlow(("gvmmR0HandleObjDestructor: %p %p %p\n", pvObj, pvGVMM, pvHandle));

    /*
     * Some quick, paranoid, input validation.
     */
    PGVMHANDLE pHandle = (PGVMHANDLE)pvHandle;
    AssertPtr(pHandle);
    PGVMM pGVMM = (PGVMM)pvGVMM;
    Assert(pGVMM == g_pGVMM);
    const uint16_t iHandle = pHandle - &pGVMM->aHandles[0];
    if (    !iHandle
        ||  iHandle >= RT_ELEMENTS(pGVMM->aHandles)
        ||  iHandle != pHandle->iSelf)
    {
        SUPR0Printf("GVM: handle %d is out of range or corrupt (iSelf=%d)!\n", iHandle, pHandle->iSelf);
        return;
    }

    int rc = gvmmR0CreateDestroyLock(pGVMM);
    AssertRC(rc);
    rc = gvmmR0UsedLock(pGVMM);
    AssertRC(rc);

    /*
     * This is a tad slow but a doubly linked list is too much hazzle.
     */
    if (RT_UNLIKELY(pHandle->iNext >= RT_ELEMENTS(pGVMM->aHandles)))
    {
        SUPR0Printf("GVM: used list index %d is out of range!\n", pHandle->iNext);
        gvmmR0UsedUnlock(pGVMM);
        gvmmR0CreateDestroyUnlock(pGVMM);
        return;
    }

    if (pGVMM->iUsedHead == iHandle)
        pGVMM->iUsedHead = pHandle->iNext;
    else
    {
        uint16_t iPrev = pGVMM->iUsedHead;
        int c = RT_ELEMENTS(pGVMM->aHandles) + 2;
        while (iPrev)
        {
            if (RT_UNLIKELY(iPrev >= RT_ELEMENTS(pGVMM->aHandles)))
            {
                SUPR0Printf("GVM: used list index %d is out of range!\n");
                gvmmR0UsedUnlock(pGVMM);
                gvmmR0CreateDestroyUnlock(pGVMM);
                return;
            }
            if (RT_UNLIKELY(c-- <= 0))
            {
                iPrev = 0;
                break;
            }

            if (pGVMM->aHandles[iPrev].iNext == iHandle)
                break;
            iPrev = pGVMM->aHandles[iPrev].iNext;
        }
        if (!iPrev)
        {
            SUPR0Printf("GVM: can't find the handle previous previous of %d!\n", pHandle->iSelf);
            gvmmR0UsedUnlock(pGVMM);
            gvmmR0CreateDestroyUnlock(pGVMM);
            return;
        }

        Assert(pGVMM->aHandles[iPrev].iNext == iHandle);
        pGVMM->aHandles[iPrev].iNext = pHandle->iNext;
    }
    pHandle->iNext = 0;
    pGVMM->cVMs--;

    gvmmR0UsedUnlock(pGVMM);

    /*
     * Do the global cleanup round.
     */
    PGVM pGVM = pHandle->pGVM;
    if (    VALID_PTR(pGVM)
        &&  pGVM->u32Magic == GVM_MAGIC)
    {
        gvmmR0CleanupVM(pGVM);

        /*
         * Do the GVMM cleanup - must be done last.
         */
        /* The VM and VM pages mappings/allocations. */
        if (pGVM->gvmm.s.VMPagesMapObj != NIL_RTR0MEMOBJ)
        {
            rc = RTR0MemObjFree(pGVM->gvmm.s.VMPagesMapObj, false /* fFreeMappings */); AssertRC(rc);
            pGVM->gvmm.s.VMPagesMapObj = NIL_RTR0MEMOBJ;
        }

        if (pGVM->gvmm.s.VMMapObj != NIL_RTR0MEMOBJ)
        {
            rc = RTR0MemObjFree(pGVM->gvmm.s.VMMapObj, false /* fFreeMappings */); AssertRC(rc);
            pGVM->gvmm.s.VMMapObj = NIL_RTR0MEMOBJ;
        }

        if (pGVM->gvmm.s.VMPagesMemObj != NIL_RTR0MEMOBJ)
        {
            rc = RTR0MemObjFree(pGVM->gvmm.s.VMPagesMemObj, false /* fFreeMappings */); AssertRC(rc);
            pGVM->gvmm.s.VMPagesMemObj = NIL_RTR0MEMOBJ;
        }

        if (pGVM->gvmm.s.VMMemObj != NIL_RTR0MEMOBJ)
        {
            rc = RTR0MemObjFree(pGVM->gvmm.s.VMMemObj, false /* fFreeMappings */); AssertRC(rc);
            pGVM->gvmm.s.VMMemObj = NIL_RTR0MEMOBJ;
        }

        for (VMCPUID i = 0; i < pGVM->cCpus; i++)
        {
            if (pGVM->aCpus[i].gvmm.s.HaltEventMulti != NIL_RTSEMEVENTMULTI)
            {
                rc = RTSemEventMultiDestroy(pGVM->aCpus[i].gvmm.s.HaltEventMulti); AssertRC(rc);
                pGVM->aCpus[i].gvmm.s.HaltEventMulti = NIL_RTSEMEVENTMULTI;
            }
        }

        /* the GVM structure itself. */
        pGVM->u32Magic |= UINT32_C(0x80000000);
        RTMemFree(pGVM);
    }
    /* else: GVMMR0CreateVM cleanup.  */

    /*
     * Free the handle.
     * Reacquire the UsedLock here to since we're updating handle fields.
     */
    rc = gvmmR0UsedLock(pGVMM);
    AssertRC(rc);

    pHandle->iNext = pGVMM->iFreeHead;
    pGVMM->iFreeHead = iHandle;
    ASMAtomicXchgPtr((void * volatile *)&pHandle->pGVM, NULL);
    ASMAtomicXchgPtr((void * volatile *)&pHandle->pVM, NULL);
    ASMAtomicXchgPtr((void * volatile *)&pHandle->pvObj, NULL);
    ASMAtomicXchgPtr((void * volatile *)&pHandle->pSession, NULL);
    ASMAtomicXchgSize(&pHandle->hEMTCpu0, NIL_RTNATIVETHREAD);

    gvmmR0UsedUnlock(pGVMM);
    gvmmR0CreateDestroyUnlock(pGVMM);
    LogFlow(("gvmmR0HandleObjDestructor: returns\n"));
}


/**
 * Registers the calling thread as the EMT of a Virtual CPU.
 *
 * Note that VCPU 0 is automatically registered during VM creation.
 *
 * @returns VBox status code
 * @param   pVM             The shared VM structure (the ring-0 mapping).
 * @param   idCpu           VCPU id.
 */
GVMMR0DECL(int) GVMMR0RegisterVCpu(PVM pVM, VMCPUID idCpu)
{
    AssertReturn(idCpu != 0, VERR_NOT_OWNER);

    /*
     * Validate the VM structure, state and handle.
     */
    PGVM pGVM;
    PGVMM pGVMM;
    int rc = gvmmR0ByVM(pVM, &pGVM, &pGVMM, false /* fTakeUsedLock */);
    if (RT_FAILURE(rc))
        return rc;

    AssertReturn(idCpu < pVM->cCPUs, VERR_INVALID_CPU_ID);
    AssertReturn(pGVM->aCpus[idCpu].hEMT == NIL_RTNATIVETHREAD, VERR_ACCESS_DENIED);

    pGVM->aCpus[idCpu].hEMT = RTThreadNativeSelf();
    return VINF_SUCCESS;
}


/**
 * Lookup a GVM structure by its handle.
 *
 * @returns The GVM pointer on success, NULL on failure.
 * @param   hGVM    The global VM handle. Asserts on bad handle.
 */
GVMMR0DECL(PGVM) GVMMR0ByHandle(uint32_t hGVM)
{
    PGVMM pGVMM;
    GVMM_GET_VALID_INSTANCE(pGVMM, NULL);

    /*
     * Validate.
     */
    AssertReturn(hGVM != NIL_GVM_HANDLE, NULL);
    AssertReturn(hGVM < RT_ELEMENTS(pGVMM->aHandles), NULL);

    /*
     * Look it up.
     */
    PGVMHANDLE pHandle = &pGVMM->aHandles[hGVM];
    AssertPtrReturn(pHandle->pVM, NULL);
    AssertPtrReturn(pHandle->pvObj, NULL);
    PGVM pGVM = pHandle->pGVM;
    AssertPtrReturn(pGVM, NULL);
    AssertReturn(pGVM->pVM == pHandle->pVM, NULL);

    return pHandle->pGVM;
}


/**
 * Lookup a GVM structure by the shared VM structure.
 *
 * @returns VBox status code.
 * @param   pVM             The shared VM structure (the ring-0 mapping).
 * @param   ppGVM           Where to store the GVM pointer.
 * @param   ppGVMM          Where to store the pointer to the GVMM instance data.
 * @param   fTakeUsedLock   Whether to take the used lock or not.
 *                          Be very careful if not taking the lock as it's possible that
 *                          the VM will disappear then.
 *
 * @remark  This will not assert on an invalid pVM but try return sliently.
 */
static int gvmmR0ByVM(PVM pVM, PGVM *ppGVM, PGVMM *ppGVMM, bool fTakeUsedLock)
{
    PGVMM pGVMM;
    GVMM_GET_VALID_INSTANCE(pGVMM, VERR_INTERNAL_ERROR);

    /*
     * Validate.
     */
    if (RT_UNLIKELY(    !VALID_PTR(pVM)
                    ||  ((uintptr_t)pVM & PAGE_OFFSET_MASK)))
        return VERR_INVALID_POINTER;
    if (RT_UNLIKELY(    pVM->enmVMState < VMSTATE_CREATING
                    ||  pVM->enmVMState >= VMSTATE_TERMINATED))
        return VERR_INVALID_POINTER;

    uint16_t hGVM = pVM->hSelf;
    if (RT_UNLIKELY(    hGVM == NIL_GVM_HANDLE
                    ||  hGVM >= RT_ELEMENTS(pGVMM->aHandles)))
        return VERR_INVALID_HANDLE;

    /*
     * Look it up.
     */
    PGVMHANDLE pHandle = &pGVMM->aHandles[hGVM];
    PGVM pGVM;
    if (fTakeUsedLock)
    {
        int rc = gvmmR0UsedLock(pGVMM);
        AssertRCReturn(rc, rc);

        pGVM = pHandle->pGVM;
        if (RT_UNLIKELY(    pHandle->pVM != pVM
                        ||  !VALID_PTR(pHandle->pvObj)
                        ||  !VALID_PTR(pGVM)
                        ||  pGVM->pVM != pVM))
        {
            gvmmR0UsedUnlock(pGVMM);
            return VERR_INVALID_HANDLE;
        }
    }
    else
    {
        if (RT_UNLIKELY(pHandle->pVM != pVM))
            return VERR_INVALID_HANDLE;
        if (RT_UNLIKELY(!VALID_PTR(pHandle->pvObj)))
            return VERR_INVALID_HANDLE;

        pGVM = pHandle->pGVM;
        if (RT_UNLIKELY(!VALID_PTR(pGVM)))
            return VERR_INVALID_HANDLE;
        if (RT_UNLIKELY(pGVM->pVM != pVM))
            return VERR_INVALID_HANDLE;
    }

    *ppGVM = pGVM;
    *ppGVMM = pGVMM;
    return VINF_SUCCESS;
}


/**
 * Lookup a GVM structure by the shared VM structure.
 *
 * @returns The GVM pointer on success, NULL on failure.
 * @param   pVM     The shared VM structure (the ring-0 mapping).
 *
 * @remark  This will not take the 'used'-lock because it doesn't do
 *          nesting and this function will be used from under the lock.
 */
GVMMR0DECL(PGVM) GVMMR0ByVM(PVM pVM)
{
    PGVMM pGVMM;
    PGVM pGVM;
    int rc = gvmmR0ByVM(pVM, &pGVM, &pGVMM, false /* fTakeUsedLock */);
    if (RT_SUCCESS(rc))
        return pGVM;
    AssertRC(rc);
    return NULL;
}


/**
 * Lookup a GVM structure by the shared VM structure
 * and ensuring that the caller is the EMT thread.
 *
 * @returns VBox status code.
 * @param   pVM         The shared VM structure (the ring-0 mapping).
 * @param   idCpu       The Virtual CPU ID of the calling EMT.
 * @param   ppGVM       Where to store the GVM pointer.
 * @param   ppGVMM      Where to store the pointer to the GVMM instance data.
 * @thread  EMT
 *
 * @remark  This will assert in all failure paths.
 */
static int gvmmR0ByVMAndEMT(PVM pVM, VMCPUID idCpu, PGVM *ppGVM, PGVMM *ppGVMM)
{
    PGVMM pGVMM;
    GVMM_GET_VALID_INSTANCE(pGVMM, VERR_INTERNAL_ERROR);

    /*
     * Validate.
     */
    AssertPtrReturn(pVM, VERR_INVALID_POINTER);
    AssertReturn(!((uintptr_t)pVM & PAGE_OFFSET_MASK), VERR_INVALID_POINTER);

    uint16_t hGVM = pVM->hSelf;
    AssertReturn(hGVM != NIL_GVM_HANDLE, VERR_INVALID_HANDLE);
    AssertReturn(hGVM < RT_ELEMENTS(pGVMM->aHandles), VERR_INVALID_HANDLE);

    /*
     * Look it up.
     */
    PGVMHANDLE pHandle = &pGVMM->aHandles[hGVM];
    AssertReturn(pHandle->pVM == pVM, VERR_NOT_OWNER);
    AssertPtrReturn(pHandle->pvObj, VERR_INTERNAL_ERROR);

    PGVM pGVM = pHandle->pGVM;
    AssertPtrReturn(pGVM, VERR_INTERNAL_ERROR);
    AssertReturn(pGVM->pVM == pVM, VERR_INTERNAL_ERROR);
    RTNATIVETHREAD hAllegedEMT = RTThreadNativeSelf();
    AssertReturn(idCpu < pGVM->cCpus, VERR_INVALID_CPU_ID);
    AssertReturn(pGVM->aCpus[idCpu].hEMT == hAllegedEMT, VERR_INTERNAL_ERROR);

    *ppGVM = pGVM;
    *ppGVMM = pGVMM;
    return VINF_SUCCESS;
}


/**
 * Lookup a GVM structure by the shared VM structure
 * and ensuring that the caller is the EMT thread.
 *
 * @returns VBox status code.
 * @param   pVM         The shared VM structure (the ring-0 mapping).
 * @param   idCpu       The Virtual CPU ID of the calling EMT.
 * @param   ppGVM       Where to store the GVM pointer.
 * @thread  EMT
 */
GVMMR0DECL(int) GVMMR0ByVMAndEMT(PVM pVM, VMCPUID idCpu, PGVM *ppGVM)
{
    AssertPtrReturn(ppGVM, VERR_INVALID_POINTER);
    PGVMM pGVMM;
    return gvmmR0ByVMAndEMT(pVM, idCpu, ppGVM, &pGVMM);
}


/**
 * Lookup a VM by its global handle.
 *
 * @returns The VM handle on success, NULL on failure.
 * @param   hGVM    The global VM handle. Asserts on bad handle.
 */
GVMMR0DECL(PVM) GVMMR0GetVMByHandle(uint32_t hGVM)
{
    PGVM pGVM = GVMMR0ByHandle(hGVM);
    return pGVM ? pGVM->pVM : NULL;
}


/**
 * Looks up the VM belonging to the specified EMT thread.
 *
 * This is used by the assertion machinery in VMMR0.cpp to avoid causing
 * unnecessary kernel panics when the EMT thread hits an assertion. The
 * call may or not be an EMT thread.
 *
 * @returns The VM handle on success, NULL on failure.
 * @param   hEMT    The native thread handle of the EMT.
 *                  NIL_RTNATIVETHREAD means the current thread
 */
GVMMR0DECL(PVM) GVMMR0GetVMByEMT(RTNATIVETHREAD hEMT)
{
    /*
     * No Assertions here as we're usually called in a AssertMsgN or
     * RTAssert* context.
     */
    PGVMM pGVMM = g_pGVMM;
    if (    !VALID_PTR(pGVMM)
        ||  pGVMM->u32Magic != GVMM_MAGIC)
        return NULL;

    if (hEMT == NIL_RTNATIVETHREAD)
        hEMT = RTThreadNativeSelf();

    /*
     * Search the handles in a linear fashion as we don't dare to take the lock (assert).
     */
    for (unsigned i = 1; i < RT_ELEMENTS(pGVMM->aHandles); i++)
    {
        if (    pGVMM->aHandles[i].iSelf == i
            &&  VALID_PTR(pGVMM->aHandles[i].pvObj)
            &&  VALID_PTR(pGVMM->aHandles[i].pVM)
            &&  VALID_PTR(pGVMM->aHandles[i].pGVM))
        {
            if (pGVMM->aHandles[i].hEMTCpu0 == hEMT)
                return pGVMM->aHandles[i].pVM;

            /** @todo this isn't safe as GVM may be deallocated while we're running.
             * Will change this to use RTPROCESS on the handle level as storing all the
             * thread handles there doesn't scale very well. */
            PGVM pGVM = pGVMM->aHandles[i].pGVM;
            for (VMCPUID idCpu = 0; idCpu < pGVM->cCpus; idCpu++)
                if (pGVM->aCpus[idCpu].hEMT == hEMT)
                    return pGVMM->aHandles[i].pVM;
        }
    }
    return NULL;
}


/**
 * This is will wake up expired and soon-to-be expired VMs.
 *
 * @returns Number of VMs that has been woken up.
 * @param   pGVMM       Pointer to the GVMM instance data.
 * @param   u64Now      The current time.
 */
static unsigned gvmmR0SchedDoWakeUps(PGVMM pGVMM, uint64_t u64Now)
{
/** @todo Rewrite this algorithm. See performance defect XYZ. */

    /*
     * The first pass will wake up VMs which have actually expired
     * and look for VMs that should be woken up in the 2nd and 3rd passes.
     */
    unsigned cWoken = 0;
    unsigned cHalted = 0;
    unsigned cTodo2nd = 0;
    unsigned cTodo3rd = 0;
    for (unsigned i = pGVMM->iUsedHead, cGuard = 0;
         i != NIL_GVM_HANDLE && i < RT_ELEMENTS(pGVMM->aHandles);
         i = pGVMM->aHandles[i].iNext)
    {
        PGVM pCurGVM = pGVMM->aHandles[i].pGVM;
        if (    VALID_PTR(pCurGVM)
            &&  pCurGVM->u32Magic == GVM_MAGIC)
        {
            for (VMCPUID idCpu = 0; idCpu < pCurGVM->cCpus; idCpu++)
            {
                PGVMCPU pCurGVCpu = &pCurGVM->aCpus[idCpu];

                uint64_t u64 = pCurGVCpu->gvmm.s.u64HaltExpire;
                if (u64)
                {
                    if (u64 <= u64Now)
                    {
                        if (ASMAtomicXchgU64(&pCurGVCpu->gvmm.s.u64HaltExpire, 0))
                        {
                            int rc = RTSemEventMultiSignal(pCurGVCpu->gvmm.s.HaltEventMulti);
                            AssertRC(rc);
                            cWoken++;
                        }
                    }
                    else
                    {
                        cHalted++;
                        if (u64 <= u64Now + pGVMM->nsEarlyWakeUp1)
                            cTodo2nd++;
                        else if (u64 <= u64Now + pGVMM->nsEarlyWakeUp2)
                            cTodo3rd++;
                    }
                }
            }
        }
        AssertLogRelBreak(cGuard++ < RT_ELEMENTS(pGVMM->aHandles));
    }

    if (cTodo2nd)
    {
        for (unsigned i = pGVMM->iUsedHead, cGuard = 0;
             i != NIL_GVM_HANDLE && i < RT_ELEMENTS(pGVMM->aHandles);
             i = pGVMM->aHandles[i].iNext)
        {
            PGVM pCurGVM = pGVMM->aHandles[i].pGVM;
            if (    VALID_PTR(pCurGVM)
                &&  pCurGVM->u32Magic == GVM_MAGIC)
            {
                for (VMCPUID idCpu = 0; idCpu < pCurGVM->cCpus; idCpu++)
                {
                    PGVMCPU pCurGVCpu = &pCurGVM->aCpus[idCpu];

                    if (    pCurGVCpu->gvmm.s.u64HaltExpire
                        &&  pCurGVCpu->gvmm.s.u64HaltExpire <= u64Now + pGVMM->nsEarlyWakeUp1)
                    {
                        if (ASMAtomicXchgU64(&pCurGVCpu->gvmm.s.u64HaltExpire, 0))
                        {
                            int rc = RTSemEventMultiSignal(pCurGVCpu->gvmm.s.HaltEventMulti);
                            AssertRC(rc);
                            cWoken++;
                        }
                    }
                }
            }
            AssertLogRelBreak(cGuard++ < RT_ELEMENTS(pGVMM->aHandles));
        }
    }

    if (cTodo3rd)
    {
        for (unsigned i = pGVMM->iUsedHead, cGuard = 0;
             i != NIL_GVM_HANDLE && i < RT_ELEMENTS(pGVMM->aHandles);
             i = pGVMM->aHandles[i].iNext)
        {
            PGVM pCurGVM = pGVMM->aHandles[i].pGVM;
            if (    VALID_PTR(pCurGVM)
                &&  pCurGVM->u32Magic == GVM_MAGIC)
            {
                for (VMCPUID idCpu = 0; idCpu < pCurGVM->cCpus; idCpu++)
                {
                    PGVMCPU pCurGVCpu = &pCurGVM->aCpus[idCpu];

                    if (    pCurGVCpu->gvmm.s.u64HaltExpire
                        &&  pCurGVCpu->gvmm.s.u64HaltExpire <= u64Now + pGVMM->nsEarlyWakeUp2)
                    {
                        if (ASMAtomicXchgU64(&pCurGVCpu->gvmm.s.u64HaltExpire, 0))
                        {
                            int rc = RTSemEventMultiSignal(pCurGVCpu->gvmm.s.HaltEventMulti);
                            AssertRC(rc);
                            cWoken++;
                        }
                    }
                }
            }
            AssertLogRelBreak(cGuard++ < RT_ELEMENTS(pGVMM->aHandles));
        }
    }

    return cWoken;
}


/**
 * Halt the EMT thread.
 *
 * @returns VINF_SUCCESS normal wakeup (timeout or kicked by other thread).
 *          VERR_INTERRUPTED if a signal was scheduled for the thread.
 * @param   pVM                 Pointer to the shared VM structure.
 * @param   idCpu               The Virtual CPU ID of the calling EMT.
 * @param   u64ExpireGipTime    The time for the sleep to expire expressed as GIP time.
 * @thread  EMT(idCpu).
 */
GVMMR0DECL(int) GVMMR0SchedHalt(PVM pVM, VMCPUID idCpu, uint64_t u64ExpireGipTime)
{
    LogFlow(("GVMMR0SchedHalt: pVM=%p\n", pVM));

    /*
     * Validate the VM structure, state and handle.
     */
    PGVM pGVM;
    PGVMM pGVMM;
    int rc = gvmmR0ByVMAndEMT(pVM, idCpu, &pGVM, &pGVMM);
    if (RT_FAILURE(rc))
        return rc;
    pGVM->gvmm.s.StatsSched.cHaltCalls++;

    PGVMCPU pCurGVCpu = &pGVM->aCpus[idCpu];
    Assert(idCpu < pGVM->cCpus);
    Assert(!pCurGVCpu->gvmm.s.u64HaltExpire);

    /*
     * Take the UsedList semaphore, get the current time
     * and check if anyone needs waking up.
     * Interrupts must NOT be disabled at this point because we ask for GIP time!
     */
    rc = gvmmR0UsedLock(pGVMM);
    AssertRC(rc);

    pCurGVCpu->gvmm.s.iCpuEmt = ASMGetApicId();

    Assert(ASMGetFlags() & X86_EFL_IF);
    const uint64_t u64Now = RTTimeNanoTS(); /* (GIP time) */
    pGVM->gvmm.s.StatsSched.cHaltWakeUps += gvmmR0SchedDoWakeUps(pGVMM, u64Now);

    /*
     * Go to sleep if we must...
     */
    if (    u64Now < u64ExpireGipTime
        &&  u64ExpireGipTime - u64Now > (pGVMM->cVMs > pGVMM->cVMsMeansCompany
                                         ? pGVMM->nsMinSleepCompany
                                         : pGVMM->nsMinSleepAlone))
    {
        pGVM->gvmm.s.StatsSched.cHaltBlocking++;
        ASMAtomicXchgU64(&pCurGVCpu->gvmm.s.u64HaltExpire, u64ExpireGipTime);
        gvmmR0UsedUnlock(pGVMM);

        uint32_t cMillies = (u64ExpireGipTime - u64Now) / 1000000;
        rc = RTSemEventMultiWaitNoResume(pCurGVCpu->gvmm.s.HaltEventMulti, cMillies ? cMillies : 1);
        ASMAtomicXchgU64(&pCurGVCpu->gvmm.s.u64HaltExpire, 0);
        if (rc == VERR_TIMEOUT)
        {
            pGVM->gvmm.s.StatsSched.cHaltTimeouts++;
            rc = VINF_SUCCESS;
        }
    }
    else
    {
        pGVM->gvmm.s.StatsSched.cHaltNotBlocking++;
        gvmmR0UsedUnlock(pGVMM);
    }

    /* Make sure false wake up calls (gvmmR0SchedDoWakeUps) cause us to spin. */
    RTSemEventMultiReset(pCurGVCpu->gvmm.s.HaltEventMulti);

    return rc;
}


/**
 * Wakes up the halted EMT thread so it can service a pending request.
 *
 * @returns VINF_SUCCESS if not yielded.
 *          VINF_GVM_NOT_BLOCKED if the EMT thread wasn't blocked.
 * @param   pVM                 Pointer to the shared VM structure.
 * @param   idCpu               The Virtual CPU ID of the EMT to wake up.
 * @thread  Any but EMT.
 */
GVMMR0DECL(int) GVMMR0SchedWakeUp(PVM pVM, VMCPUID idCpu)
{
    /*
     * Validate input and take the UsedLock.
     */
    PGVM pGVM;
    PGVMM pGVMM;
    int rc = gvmmR0ByVM(pVM, &pGVM, &pGVMM, true /* fTakeUsedLock */);
    if (RT_SUCCESS(rc))
    {
        if (idCpu < pGVM->cCpus)
        {
            PGVMCPU pCurGVCpu = &pGVM->aCpus[idCpu];
            pGVM->gvmm.s.StatsSched.cWakeUpCalls++;

            /*
             * Signal the semaphore regardless of whether it's current blocked on it.
             *
             * The reason for this is that there is absolutely no way we can be 100%
             * certain that it isn't *about* go to go to sleep on it and just got
             * delayed a bit en route. So, we will always signal the semaphore when
             * the it is flagged as halted in the VMM.
             */
            if (pCurGVCpu->gvmm.s.u64HaltExpire)
            {
                rc = VINF_SUCCESS;
                ASMAtomicXchgU64(&pCurGVCpu->gvmm.s.u64HaltExpire, 0);
            }
            else
            {
                rc = VINF_GVM_NOT_BLOCKED;
                pGVM->gvmm.s.StatsSched.cWakeUpNotHalted++;
            }

            int rc2 = RTSemEventMultiSignal(pCurGVCpu->gvmm.s.HaltEventMulti);
            AssertRC(rc2);

            /*
             * While we're here, do a round of scheduling.
             */
            Assert(ASMGetFlags() & X86_EFL_IF);
            const uint64_t u64Now = RTTimeNanoTS(); /* (GIP time) */
            pGVM->gvmm.s.StatsSched.cWakeUpWakeUps += gvmmR0SchedDoWakeUps(pGVMM, u64Now);

        }
        else
            rc = VERR_INVALID_CPU_ID;

        int rc2 = gvmmR0UsedUnlock(pGVMM);
        AssertRC(rc2);
    }

    LogFlow(("GVMMR0SchedWakeUp: returns %Rrc\n", rc));
    return rc;
}


/**
 * Poll the schedule to see if someone else should get a chance to run.
 *
 * This is a bit hackish and will not work too well if the machine is
 * under heavy load from non-VM processes.
 *
 * @returns VINF_SUCCESS if not yielded.
 *          VINF_GVM_YIELDED if an attempt to switch to a different VM task was made.
 * @param   pVM                 Pointer to the shared VM structure.
 * @param   idCpu               The Virtual CPU ID of the calling EMT.
 * @param   u64ExpireGipTime    The time for the sleep to expire expressed as GIP time.
 * @param   fYield              Whether to yield or not.
 *                              This is for when we're spinning in the halt loop.
 * @thread  EMT(idCpu).
 */
GVMMR0DECL(int) GVMMR0SchedPoll(PVM pVM, VMCPUID idCpu, bool fYield)
{
    /*
     * Validate input.
     */
    PGVM pGVM;
    PGVMM pGVMM;
    int rc = gvmmR0ByVMAndEMT(pVM, idCpu, &pGVM, &pGVMM);
    if (RT_SUCCESS(rc))
    {
        rc = gvmmR0UsedLock(pGVMM);
        AssertRC(rc);
        pGVM->gvmm.s.StatsSched.cPollCalls++;

        Assert(ASMGetFlags() & X86_EFL_IF);
        const uint64_t u64Now = RTTimeNanoTS(); /* (GIP time) */

        if (!fYield)
            pGVM->gvmm.s.StatsSched.cPollWakeUps += gvmmR0SchedDoWakeUps(pGVMM, u64Now);
        else
        {
            /** @todo implement this... */
            rc = VERR_NOT_IMPLEMENTED;
        }

        gvmmR0UsedUnlock(pGVMM);
    }

    LogFlow(("GVMMR0SchedWakeUp: returns %Rrc\n", rc));
    return rc;
}



/**
 * Retrieves the GVMM statistics visible to the caller.
 *
 * @returns VBox status code.
 *
 * @param   pStats      Where to put the statistics.
 * @param   pSession    The current session.
 * @param   pVM         The VM to obtain statistics for. Optional.
 */
GVMMR0DECL(int) GVMMR0QueryStatistics(PGVMMSTATS pStats, PSUPDRVSESSION pSession, PVM pVM)
{
    LogFlow(("GVMMR0QueryStatistics: pStats=%p pSession=%p pVM=%p\n", pStats, pSession, pVM));

    /*
     * Validate input.
     */
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pStats, VERR_INVALID_POINTER);
    pStats->cVMs = 0; /* (crash before taking the sem...) */

    /*
     * Take the lock and get the VM statistics.
     */
    PGVMM pGVMM;
    if (pVM)
    {
        PGVM pGVM;
        int rc = gvmmR0ByVM(pVM, &pGVM, &pGVMM, true /*fTakeUsedLock*/);
        if (RT_FAILURE(rc))
            return rc;
        pStats->SchedVM = pGVM->gvmm.s.StatsSched;
    }
    else
    {
        GVMM_GET_VALID_INSTANCE(pGVMM, VERR_INTERNAL_ERROR);
        memset(&pStats->SchedVM, 0, sizeof(pStats->SchedVM));

        int rc = gvmmR0UsedLock(pGVMM);
        AssertRCReturn(rc, rc);
    }

    /*
     * Enumerate the VMs and add the ones visibile to the statistics.
     */
    pStats->cVMs = 0;
    memset(&pStats->SchedSum, 0, sizeof(pStats->SchedSum));

    for (unsigned i = pGVMM->iUsedHead;
         i != NIL_GVM_HANDLE && i < RT_ELEMENTS(pGVMM->aHandles);
         i = pGVMM->aHandles[i].iNext)
    {
        PGVM pGVM = pGVMM->aHandles[i].pGVM;
        void *pvObj = pGVMM->aHandles[i].pvObj;
        if (    VALID_PTR(pvObj)
            &&  VALID_PTR(pGVM)
            &&  pGVM->u32Magic == GVM_MAGIC
            &&  RT_SUCCESS(SUPR0ObjVerifyAccess(pvObj, pSession, NULL)))
        {
            pStats->cVMs++;

            pStats->SchedSum.cHaltCalls        += pGVM->gvmm.s.StatsSched.cHaltCalls;
            pStats->SchedSum.cHaltBlocking     += pGVM->gvmm.s.StatsSched.cHaltBlocking;
            pStats->SchedSum.cHaltTimeouts     += pGVM->gvmm.s.StatsSched.cHaltTimeouts;
            pStats->SchedSum.cHaltNotBlocking  += pGVM->gvmm.s.StatsSched.cHaltNotBlocking;
            pStats->SchedSum.cHaltWakeUps      += pGVM->gvmm.s.StatsSched.cHaltWakeUps;

            pStats->SchedSum.cWakeUpCalls      += pGVM->gvmm.s.StatsSched.cWakeUpCalls;
            pStats->SchedSum.cWakeUpNotHalted  += pGVM->gvmm.s.StatsSched.cWakeUpNotHalted;
            pStats->SchedSum.cWakeUpWakeUps    += pGVM->gvmm.s.StatsSched.cWakeUpWakeUps;

            pStats->SchedSum.cPollCalls        += pGVM->gvmm.s.StatsSched.cPollCalls;
            pStats->SchedSum.cPollHalts        += pGVM->gvmm.s.StatsSched.cPollHalts;
            pStats->SchedSum.cPollWakeUps      += pGVM->gvmm.s.StatsSched.cPollWakeUps;
        }
    }

    gvmmR0UsedUnlock(pGVMM);

    return VINF_SUCCESS;
}


/**
 * VMMR0 request wrapper for GVMMR0QueryStatistics.
 *
 * @returns see GVMMR0QueryStatistics.
 * @param   pVM             Pointer to the shared VM structure. Optional.
 * @param   pReq            The request packet.
 */
GVMMR0DECL(int) GVMMR0QueryStatisticsReq(PVM pVM, PGVMMQUERYSTATISTICSSREQ pReq)
{
    /*
     * Validate input and pass it on.
     */
    AssertPtrReturn(pReq, VERR_INVALID_POINTER);
    AssertMsgReturn(pReq->Hdr.cbReq == sizeof(*pReq), ("%#x != %#x\n", pReq->Hdr.cbReq, sizeof(*pReq)), VERR_INVALID_PARAMETER);

    return GVMMR0QueryStatistics(&pReq->Stats, pReq->pSession, pVM);
}


/**
 * Resets the specified GVMM statistics.
 *
 * @returns VBox status code.
 *
 * @param   pStats      Which statistics to reset, that is, non-zero fields indicates which to reset.
 * @param   pSession    The current session.
 * @param   pVM         The VM to reset statistics for. Optional.
 */
GVMMR0DECL(int) GVMMR0ResetStatistics(PCGVMMSTATS pStats, PSUPDRVSESSION pSession, PVM pVM)
{
    LogFlow(("GVMMR0ResetStatistics: pStats=%p pSession=%p pVM=%p\n", pStats, pSession, pVM));

    /*
     * Validate input.
     */
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pStats, VERR_INVALID_POINTER);

    /*
     * Take the lock and get the VM statistics.
     */
    PGVMM pGVMM;
    if (pVM)
    {
        PGVM pGVM;
        int rc = gvmmR0ByVM(pVM, &pGVM, &pGVMM, true /*fTakeUsedLock*/);
        if (RT_FAILURE(rc))
            return rc;
#       define MAYBE_RESET_FIELD(field) \
            do { if (pStats->SchedVM. field ) { pGVM->gvmm.s.StatsSched. field = 0; } } while (0)
        MAYBE_RESET_FIELD(cHaltCalls);
        MAYBE_RESET_FIELD(cHaltBlocking);
        MAYBE_RESET_FIELD(cHaltTimeouts);
        MAYBE_RESET_FIELD(cHaltNotBlocking);
        MAYBE_RESET_FIELD(cHaltWakeUps);
        MAYBE_RESET_FIELD(cWakeUpCalls);
        MAYBE_RESET_FIELD(cWakeUpNotHalted);
        MAYBE_RESET_FIELD(cWakeUpWakeUps);
        MAYBE_RESET_FIELD(cPollCalls);
        MAYBE_RESET_FIELD(cPollHalts);
        MAYBE_RESET_FIELD(cPollWakeUps);
#       undef MAYBE_RESET_FIELD
    }
    else
    {
        GVMM_GET_VALID_INSTANCE(pGVMM, VERR_INTERNAL_ERROR);

        int rc = gvmmR0UsedLock(pGVMM);
        AssertRCReturn(rc, rc);
    }

    /*
     * Enumerate the VMs and add the ones visibile to the statistics.
     */
    if (ASMMemIsAll8(&pStats->SchedSum, sizeof(pStats->SchedSum), 0))
    {
        for (unsigned i = pGVMM->iUsedHead;
             i != NIL_GVM_HANDLE && i < RT_ELEMENTS(pGVMM->aHandles);
             i = pGVMM->aHandles[i].iNext)
        {
            PGVM pGVM = pGVMM->aHandles[i].pGVM;
            void *pvObj = pGVMM->aHandles[i].pvObj;
            if (    VALID_PTR(pvObj)
                &&  VALID_PTR(pGVM)
                &&  pGVM->u32Magic == GVM_MAGIC
                &&  RT_SUCCESS(SUPR0ObjVerifyAccess(pvObj, pSession, NULL)))
            {
#               define MAYBE_RESET_FIELD(field) \
                    do { if (pStats->SchedSum. field ) { pGVM->gvmm.s.StatsSched. field = 0; } } while (0)
                MAYBE_RESET_FIELD(cHaltCalls);
                MAYBE_RESET_FIELD(cHaltBlocking);
                MAYBE_RESET_FIELD(cHaltTimeouts);
                MAYBE_RESET_FIELD(cHaltNotBlocking);
                MAYBE_RESET_FIELD(cHaltWakeUps);
                MAYBE_RESET_FIELD(cWakeUpCalls);
                MAYBE_RESET_FIELD(cWakeUpNotHalted);
                MAYBE_RESET_FIELD(cWakeUpWakeUps);
                MAYBE_RESET_FIELD(cPollCalls);
                MAYBE_RESET_FIELD(cPollHalts);
                MAYBE_RESET_FIELD(cPollWakeUps);
#               undef MAYBE_RESET_FIELD
            }
        }
    }

    gvmmR0UsedUnlock(pGVMM);

    return VINF_SUCCESS;
}


/**
 * VMMR0 request wrapper for GVMMR0ResetStatistics.
 *
 * @returns see GVMMR0ResetStatistics.
 * @param   pVM             Pointer to the shared VM structure. Optional.
 * @param   pReq            The request packet.
 */
GVMMR0DECL(int) GVMMR0ResetStatisticsReq(PVM pVM, PGVMMRESETSTATISTICSSREQ pReq)
{
    /*
     * Validate input and pass it on.
     */
    AssertPtrReturn(pReq, VERR_INVALID_POINTER);
    AssertMsgReturn(pReq->Hdr.cbReq == sizeof(*pReq), ("%#x != %#x\n", pReq->Hdr.cbReq, sizeof(*pReq)), VERR_INVALID_PARAMETER);

    return GVMMR0ResetStatistics(&pReq->Stats, pReq->pSession, pVM);
}

