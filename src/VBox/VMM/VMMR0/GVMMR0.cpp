/* $Id$ */
/** @file
 * GVMM - Global VM Manager.
 */

/*
 * Copyright (C) 2007 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 */


/** @page pg_GVMM   GVMM - The Global VM Manager
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
#include "GVMMR0Internal.h"
#include <VBox/gvm.h>
#include <VBox/vm.h>
#include <VBox/err.h>
#include <iprt/alloc.h>
#include <iprt/semaphore.h>
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
    /** The ring-0 handle of the EMT thread.
     * This is used for assertions and similar cases where we need to find the VM handle. */
    RTNATIVETHREAD      hEMT;
} GVMHANDLE;
/** Pointer to a global VM handle. */
typedef GVMHANDLE *PGVMHANDLE;

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
    /** The lock used to serialize registration and deregistration. */
    RTSEMFASTMUTEX      Lock;
    /** The handle array.
     * The size of this array defines the maximum number of currently running VMs.
     * The first entry is unused as it represents the NIL handle. */
    GVMHANDLE           aHandles[128];
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
static DECLCALLBACK(void) gvmmR0HandleObjDestructor(void *pvObj, void *pvGVMM, void *pvHandle);


/**
 * Initializes the GVMM.
 *
 * This is called while owninng the loader sempahore (see supdrvIOCtl_LdrLoad()).
 *
 * @returns VBox status code.
 */
GVMMR0DECL(int) GVMMR0Init(void)
{
    SUPR0Printf("GVMMR0Init:\n");

    /*
     * Allocate and initialize the instance data.
     */
    PGVMM pGVMM = (PGVMM)RTMemAllocZ(sizeof(*pGVMM));
    if (!pGVMM)
        return VERR_NO_MEMORY;
    int rc = RTSemFastMutexCreate(&pGVMM->Lock);
    if (RT_SUCCESS(rc))
    {
        pGVMM->u32Magic = GVMM_MAGIC;
        pGVMM->iUsedHead = 0;
        pGVMM->iFreeHead = 1;

        /* the nil handle */
        pGVMM->aHandles[0].iSelf = 0;
        pGVMM->aHandles[0].iNext = 0;

        /* the tail */
        unsigned i = RT_ELEMENTS(pGVMM->aHandles);
        pGVMM->aHandles[i].iSelf = i;
        pGVMM->aHandles[i].iNext = 0; /* nil */

        /* the rest */
        while (i-- > 1)
        {
            pGVMM->aHandles[i].iSelf = i;
            pGVMM->aHandles[i].iNext = i + 1;
        }

        g_pGVMM = pGVMM;
        SUPR0Printf("GVMMR0Init: pGVMM=%p\n", pGVMM);
        return VINF_SUCCESS;
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
    SUPR0Printf("GVMMR0Term:\n");

    PGVMM pGVMM = g_pGVMM;
    g_pGVMM = NULL;
    if (RT_UNLIKELY(!VALID_PTR(pGVMM)))
    {
        SUPR0Printf("GVMMR0Term: pGVMM=%p\n", pGVMM);
        return;
    }

    RTSemFastMutexDestroy(pGVMM->Lock);
    pGVMM->Lock = NIL_RTSEMFASTMUTEX;
    pGVMM->u32Magic++;
    pGVMM->iFreeHead = 0;
    if (pGVMM->iUsedHead)
    {
        SUPR0Printf("GVMMR0Term: iUsedHead=%#x!\n", pGVMM->iUsedHead);
        pGVMM->iUsedHead = 0;
    }

    RTMemFree(pGVMM);
}


/**
 * Request wrapper for the GVMMR0CreateVM API.
 *
 * @returns VBox status code.
 * @param   pReqHdr     The request buffer.
 */
GVMMR0DECL(int) GVMMR0CreateVMReq(PSUPVMMR0REQHDR pReqHdr)
{
    PGVMMCREATEVMREQ pReq = (PGVMMCREATEVMREQ)pReqHdr;

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
    int rc = GVMMR0CreateVM(pReq->pSession, &pVM);
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
 * @returns VBox status code.
 * @param   pSession    The support driver session.
 * @param   ppVM        Where to store the pointer to the VM structure.
 *
 * @thread  Any thread.
 */
GVMMR0DECL(int) GVMMR0CreateVM(PSUPDRVSESSION pSession, PVM *ppVM)
{
    LogFlow(("GVMMR0CreateVM: pSession=%p\n", pSession));
    PGVMM pGVMM;
    GVMM_GET_VALID_INSTANCE(pGVMM, VERR_INTERNAL_ERROR);

    AssertPtrReturn(ppVM, VERR_INVALID_POINTER);
    *ppVM = NULL;

    AssertReturn(RTThreadNativeSelf() != NIL_RTNATIVETHREAD, VERR_INTERNAL_ERROR);

    /*
     * The whole allocation process is protected by the lock.
     */
    int rc = RTSemFastMutexRequest(pGVMM->Lock);
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
                pGVMM->iFreeHead = pHandle->iNext;
                pHandle->iNext = pGVMM->iUsedHead;
                pGVMM->iUsedHead = iHandle;

                pHandle->pVM = NULL;
                pHandle->pGVM = NULL;
                pHandle->pSession = pSession;
                pHandle->hEMT = NIL_RTNATIVETHREAD;

                rc = SUPR0ObjVerifyAccess(pHandle->pvObj, pSession, NULL);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Allocate the global VM structure (GVM) and initialize it.
                     */
                    PGVM pGVM = (PGVM)RTMemAllocZ(sizeof(*pGVM));
                    if (pGVM)
                    {
                        pGVM->u32Magic = GVM_MAGIC;
                        pGVM->hSelf = iHandle;
                        pGVM->hEMT = NIL_RTNATIVETHREAD;
                        pGVM->pVM = NULL;

                        /* gvmmR0InitPerVMData: */
                        AssertCompile(RT_SIZEOFMEMB(GVM,gvmm.s) <= RT_SIZEOFMEMB(GVM,gvmm.padding));
                        Assert(RT_SIZEOFMEMB(GVM,gvmm.s) <= RT_SIZEOFMEMB(GVM,gvmm.padding));
                        pGVM->gvmm.s.VMMemObj = NIL_RTR0MEMOBJ;
                        pGVM->gvmm.s.VMMapObj = NIL_RTR0MEMOBJ;
                        pGVM->gvmm.s.VMPagesMemObj = NIL_RTR0MEMOBJ;
                        pGVM->gvmm.s.VMPagesMapObj = NIL_RTR0MEMOBJ;

                        /* GVMMR0InitPerVMData(pGVM); - later */

                        /*
                         * Allocate the shared VM structure and associated page array.
                         */
                        const size_t cPages = RT_ALIGN(sizeof(VM), PAGE_SIZE) >> PAGE_SHIFT;
                        rc = RTR0MemObjAllocLow(&pGVM->gvmm.s.VMMemObj, cPages << PAGE_SHIFT, false /* fExecutable */);
                        if (RT_SUCCESS(rc))
                        {
                            PVM pVM = (PVM)RTR0MemObjAddress(pGVM->gvmm.s.VMMemObj); AssertPtr(pVM);
                            memset(pVM, 0, cPages << PAGE_SHIFT);
                            pVM->enmVMState = VMSTATE_CREATING;
                            pVM->pVMR0 = pVM;
                            pVM->pSession = pSession;
                            pVM->hSelf = iHandle;

                            rc = RTR0MemObjAllocPage(&pGVM->gvmm.s.VMPagesMemObj, cPages * sizeof(SUPPAGE), false /* fExecutable */);
                            if (RT_SUCCESS(rc))
                            {
                                PSUPPAGE paPages = (PSUPPAGE)RTR0MemObjAddress(pGVM->gvmm.s.VMPagesMemObj); AssertPtr(paPages);
                                for (size_t iPage = 0; iPage < cPages; iPage++)
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

                                    rc = RTR0MemObjMapUser(&pGVM->gvmm.s.VMPagesMapObj, pGVM->gvmm.s.VMPagesMemObj, (RTR3PTR)-1, 0,
                                                           RTMEM_PROT_READ | RTMEM_PROT_WRITE, NIL_RTR0PROCESS);
                                    if (RT_SUCCESS(rc))
                                    {
                                        pVM->paVMPagesR3 = RTR0MemObjAddressR3(pGVM->gvmm.s.VMPagesMapObj);
                                        AssertPtr((void *)pVM->paVMPagesR3);

                                        /* complete the handle. */
                                        pHandle->pVM = pVM;
                                        pHandle->pGVM = pGVM;

                                        RTSemFastMutexRelease(pGVMM->Lock);

                                        *ppVM = pVM;
                                        SUPR0Printf("GVMMR0CreateVM: pVM=%p pVMR3=%p pGVM=%p hGVM=%d\n", pVM, pVM->pVMR3, pGVM, iHandle);
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
                RTSemFastMutexRelease(pGVMM->Lock);

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

    RTSemFastMutexRelease(pGVMM->Lock);
    return rc;
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
 * @thread  EMT if it's associated with the VM, otherwise any thread.
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
    AssertReturn(pHandle->hEMT == hSelf || pHandle->hEMT == NIL_RTNATIVETHREAD, VERR_NOT_OWNER);

    /*
     * Lookup the handle and destroy the object.
     * Since the lock isn't recursive and we'll have to leave it before dereferencing the
     * object, we take some precautions against racing callers just in case...
     */
    int rc = RTSemFastMutexRequest(pGVMM->Lock);
    AssertRC(rc);

    /* be careful here because we might theoretically be racing someone else cleaning up. */
    if (    pHandle->pVM == pVM
        &&  (   pHandle->hEMT == hSelf
             || pHandle->hEMT == NIL_RTNATIVETHREAD)
        &&  VALID_PTR(pHandle->pvObj)
        &&  VALID_PTR(pHandle->pSession)
        &&  VALID_PTR(pHandle->pGVM)
        &&  pHandle->pGVM->u32Magic == GVM_MAGIC)
    {
        void *pvObj = pHandle->pvObj;
        pHandle->pvObj = NULL;
        RTSemFastMutexRelease(pGVMM->Lock);

        SUPR0ObjRelease(pvObj, pHandle->pSession);
    }
    else
    {
        SUPR0Printf("GVMMR0DestroyVM: pHandle=%p:{.pVM=%p, hEMT=%p, .pvObj=%p} pVM=%p hSelf=%p\n",
                    pHandle, pHandle->pVM, pHandle->hEMT, pHandle->pvObj, pVM, hSelf);
        RTSemFastMutexRelease(pGVMM->Lock);
        rc = VERR_INTERNAL_ERROR;
    }

    return rc;
}


/**
 * Handle destructor.
 *
 * @param   pvGVMM       The GVM instance pointer.
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

    int rc = RTSemFastMutexRequest(pGVMM->Lock);
    AssertRC(rc);

    /*
     * This is a tad slow but a doubly linked list is too much hazzle.
     */
    if (RT_UNLIKELY(pHandle->iNext >= RT_ELEMENTS(pGVMM->aHandles)))
    {
        SUPR0Printf("GVM: used list index %d is out of range!\n", pHandle->iNext);
        RTSemFastMutexRelease(pGVMM->Lock);
        return;
    }

    if (pGVMM->iUsedHead == iHandle)
        pGVMM->iUsedHead = pHandle->iNext;
    else
    {
        uint16_t iPrev = pGVMM->iUsedHead;
        int c = RT_ELEMENTS(pGVMM->aHandles) + 2;
        while (!iPrev)
        {
            if (RT_UNLIKELY(iPrev >= RT_ELEMENTS(pGVMM->aHandles)))
            {
                SUPR0Printf("GVM: used list index %d is out of range!\n");
                RTSemFastMutexRelease(pGVMM->Lock);
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
            RTSemFastMutexRelease(pGVMM->Lock);
            return;
        }

        pGVMM->aHandles[iPrev].iNext = pHandle->iNext;
    }
    pHandle->iNext = 0;

    /*
     * Do the global cleanup round.
     */
    PGVM pGVM = pHandle->pGVM;
    if (    VALID_PTR(pGVM)
        &&  pGVM->u32Magic == GVM_MAGIC)
    {
        /// @todo GMMR0CleanupVM(pGVM);

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

        /* the GVM structure itself. */
        pGVM->u32Magic++;
        RTMemFree(pGVM);
    }
    /* else: GVMMR0CreateVM cleanup.  */

    /*
     * Free the handle.
     */
    pHandle->iNext = pGVMM->iFreeHead;
    pGVMM->iFreeHead = iHandle;
    ASMAtomicXchgPtr((void * volatile *)&pHandle->pGVM, NULL);
    ASMAtomicXchgPtr((void * volatile *)&pHandle->pVM, NULL);
    ASMAtomicXchgPtr((void * volatile *)&pHandle->pvObj, NULL);
    ASMAtomicXchgPtr((void * volatile *)&pHandle->pSession, NULL);
    ASMAtomicXchgSize(&pHandle->hEMT, NIL_RTNATIVETHREAD);

    RTSemFastMutexRelease(pGVMM->Lock);
    SUPR0Printf("gvmmR0HandleObjDestructor: returns\n");
}


/**
 * Associates an EMT thread with a VM.
 *
 * This is called early during the ring-0 VM initialization so assertions later in
 * the process can be handled gracefully.
 *
 * @returns VBox status code.
 *
 * @param   pVM         The VM instance data (aka handle), ring-0 mapping of course.
 * @thread  EMT.
 */
GVMMR0DECL(int) GVMMR0AssociateEMTWithVM(PVM pVM)
{
    LogFlow(("GVMMR0AssociateEMTWithVM: pVM=%p\n", pVM));
    PGVMM pGVMM;
    GVMM_GET_VALID_INSTANCE(pGVMM, VERR_INTERNAL_ERROR);

    /*
     * Validate the VM structure, state and handle.
     */
    AssertPtrReturn(pVM, VERR_INVALID_POINTER);
    AssertReturn(!((uintptr_t)pVM & PAGE_OFFSET_MASK), VERR_INVALID_POINTER);
    AssertMsgReturn(pVM->enmVMState == VMSTATE_CREATING, ("%d\n", pVM->enmVMState), VERR_WRONG_ORDER);

    RTNATIVETHREAD hEMT = RTThreadNativeSelf();
    AssertReturn(hEMT != NIL_RTNATIVETHREAD, VERR_NOT_SUPPORTED);

    const uint16_t hGVM = pVM->hSelf;
    AssertReturn(hGVM != NIL_GVM_HANDLE, VERR_INVALID_HANDLE);
    AssertReturn(hGVM < RT_ELEMENTS(pGVMM->aHandles), VERR_INVALID_HANDLE);

    PGVMHANDLE pHandle = &pGVMM->aHandles[hGVM];
    AssertReturn(pHandle->pVM == pVM, VERR_NOT_OWNER);

    /*
     * Take the lock, validate the handle and update the structure members.
     */
    int rc = RTSemFastMutexRequest(pGVMM->Lock);
    AssertRCReturn(rc, rc);

    if (    pHandle->pVM == pVM
        &&  VALID_PTR(pHandle->pvObj)
        &&  VALID_PTR(pHandle->pSession)
        &&  VALID_PTR(pHandle->pGVM)
        &&  pHandle->pGVM->u32Magic == GVM_MAGIC)
    {
        pHandle->hEMT = hEMT;
        pHandle->pGVM->hEMT = hEMT;
    }
    else
        rc = VERR_INTERNAL_ERROR;

    RTSemFastMutexRelease(pGVMM->Lock);
    LogFlow(("GVMMR0AssociateEMTWithVM: returns %Vrc (hEMT=%RTnthrd)\n", rc, hEMT));
    return rc;
}


/**
 * Disassociates the EMT thread from a VM.
 *
 * This is called last in the ring-0 VM termination. After this point anyone is
 * allowed to destroy the VM. Ideally, we should associate the VM with the thread
 * that's going to call GVMMR0DestroyVM for optimal security, but that's impractical
 * at present.
 *
 * @returns VBox status code.
 *
 * @param   pVM         The VM instance data (aka handle), ring-0 mapping of course.
 * @thread  EMT.
 */
GVMMR0DECL(int) GVMMR0DisassociateEMTFromVM(PVM pVM)
{
    LogFlow(("GVMMR0DisassociateEMTFromVM: pVM=%p\n", pVM));
    PGVMM pGVMM;
    GVMM_GET_VALID_INSTANCE(pGVMM, VERR_INTERNAL_ERROR);

    /*
     * Validate the VM structure, state and handle.
     */
    AssertPtrReturn(pVM, VERR_INVALID_POINTER);
    AssertReturn(!((uintptr_t)pVM & PAGE_OFFSET_MASK), VERR_INVALID_POINTER);
    AssertMsgReturn(pVM->enmVMState >= VMSTATE_CREATING && pVM->enmVMState <= VMSTATE_DESTROYING, ("%d\n", pVM->enmVMState), VERR_WRONG_ORDER);

    RTNATIVETHREAD hEMT = RTThreadNativeSelf();
    AssertReturn(hEMT != NIL_RTNATIVETHREAD, VERR_NOT_SUPPORTED);

    const uint16_t hGVM = pVM->hSelf;
    AssertReturn(hGVM != NIL_GVM_HANDLE, VERR_INVALID_HANDLE);
    AssertReturn(hGVM < RT_ELEMENTS(pGVMM->aHandles), VERR_INVALID_HANDLE);

    PGVMHANDLE pHandle = &pGVMM->aHandles[hGVM];
    AssertReturn(pHandle->pVM == pVM, VERR_NOT_OWNER);

    /*
     * Take the lock, validate the handle and update the structure members.
     */
    int rc = RTSemFastMutexRequest(pGVMM->Lock);
    AssertRCReturn(rc, rc);

    if (    VALID_PTR(pHandle->pvObj)
        &&  VALID_PTR(pHandle->pSession)
        &&  VALID_PTR(pHandle->pGVM)
        &&  pHandle->pGVM->u32Magic == GVM_MAGIC)
    {
        if (    pHandle->pVM == pVM
            &&  pHandle->hEMT == hEMT)
        {
            pHandle->hEMT = NIL_RTNATIVETHREAD;
            pHandle->pGVM->hEMT = NIL_RTNATIVETHREAD;
        }
        else
            rc = VERR_NOT_OWNER;
    }
    else
        rc = VERR_INVALID_HANDLE;

    RTSemFastMutexRelease(pGVMM->Lock);
    LogFlow(("GVMMR0DisassociateEMTFromVM: returns %Vrc (hEMT=%RTnthrd)\n", rc, hEMT));
    return rc;
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
 * @returns The GVM pointer on success, NULL on failure.
 * @param   pVM     The shared VM structure (the ring-0 mapping).
 */
GVMMR0DECL(PGVM) GVMMR0ByVM(PVM pVM)
{
    PGVMM pGVMM;
    GVMM_GET_VALID_INSTANCE(pGVMM, NULL);

    /*
     * Validate.
     */
    AssertPtrReturn(pVM, NULL);
    AssertReturn(!((uintptr_t)pVM & PAGE_OFFSET_MASK), NULL);

    uint16_t hGVM = pVM->hSelf;
    AssertReturn(hGVM != NIL_GVM_HANDLE, NULL);
    AssertReturn(hGVM < RT_ELEMENTS(pGVMM->aHandles), NULL);

    /*
     * Look it up.
     */
    PGVMHANDLE pHandle = &pGVMM->aHandles[hGVM];
    AssertReturn(pHandle->pVM == pVM, NULL);
    AssertPtrReturn(pHandle->pvObj, NULL);
    PGVM pGVM = pHandle->pGVM;
    AssertPtrReturn(pGVM, NULL);
    AssertReturn(pGVM->pVM == pVM, NULL);

    return pGVM;
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
     * Search the handles in a linear fashion as we don't dare take the lock (assert).
     */
    for (unsigned i = 1; i < RT_ELEMENTS(pGVMM->aHandles); i++)
        if (    pGVMM->aHandles[i].hEMT == hEMT
            &&  pGVMM->aHandles[i].iSelf == i
            &&  VALID_PTR(pGVMM->aHandles[i].pvObj)
            &&  VALID_PTR(pGVMM->aHandles[i].pVM))
            return pGVMM->aHandles[i].pVM;

    return NULL;
}


