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
#define LOG_GROUP LOG_GROUP_GVM
#include <VBox/gvmm.h>
/* #include "GVMMInternal.h" */
#include <VBox/vm.h>
#include <VBox/err.h>
#include <iprt/alloc.h>
#include <iprt/semaphore.h>
#include <iprt/log.h>
#include <iprt/thread.h>
#include <iprt/param.h>
#include <iprt/string.h>


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
    /** Whether to free the VM structure or not. */
    bool                fFreeVM;
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


#if 0 /* not currently used */
/**
 * Allocates the VM structure and registers it with GVM.
 *
 * @returns VBox status code.
 * @param   pSession    The support driver session.
 * @param   ppVM        Where to store the pointer to the VM structure.
 *
 * @thread  EMT.
 */
GVMMR0DECL(int) GVMMR0CreateVM(PSUPDRVSESSION pSession, PVM *ppVM)
{
    SUPR0Printf("GVMMR0CreateVM: pSession=%p\n", pSession);
    PGVMM pGVMM = g_pGVMM;
    AssertPtrReturn(pGVMM, VERR_INTERNAL_ERROR);

    AssertPtrReturn(ppVM, VERR_INVALID_POINTER);
    *ppVM = NULL;

    RTNATIVETHREAD hEMT = RTThreadNativeSelf();
    AssertReturn(hEMT != NIL_RTNATIVETHREAD, VERR_INTERNAL_ERROR);

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
        PGVMMHANDLE pHandle = &pGVMM->aHandles[iHandle];

        /* consistency checks, a bit paranoid as always. */
        if (    !pHandle->pVM
            /*&&  !pHandle->pGVM */
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

                pHandle->fFreeVM = true;
                pHandle->pVM = NULL;
                pHandle->pGVM = NULL; /* to be allocated */
                pHandle->pSession = pSession;
                pHandle->hEMT = hEMT;

                rc = SUPR0ObjVerifyAccess(pHandle->pvObj, pSession, NULL);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Allocate and initialize the VM structure.
                     */
                    RTR3PTR     paPagesR3;
                    PRTHCPHYS   paPagesR0;
                    size_t      cPages = RT_ALIGN_Z(sizeof(VM), PAGE_SIZE) >> PAGE_SHIFT;
                    rc = SUPR0MemAlloc(pSession, cPages * sizeof(RTHCPHYS), (void **)&paPagesR0, &paPagesR3);
                    if (RT_SUCCESS(rc))
                    {
                        PVM pVM;
                        PVMR3 pVMR3;
                        rc = SUPR0LowAlloc(pSession, cPages, (void **)&pVM, &pVMR3, paPagesR0);
                        if (RT_SUCCESS(rc))
                        {
                            memset(pVM, 0, cPages * PAGE_SIZE);
                            pVM->enmVMState = VMSTATE_CREATING;
                            pVM->paVMPagesR3 = paPagesR3;
                            pVM->pVMR3 = pVMR3;
                            pVM->pVMR0 = pVM;
                            pVM->pSession = pSession;
                            pVM->hSelf = iHandle;
                            RTSemFastMutexRelease(pGVMM->Lock);

                            *ppVM = pVM;
                            SUPR0Printf("GVMMR0CreateVM: pVM=%p pVMR3=%p\n", pVM, pVMR3);
                            return VINF_SUCCESS;
                        }

                        SUPR0MemFree(pSession, (uintptr_t)paPagesR0);
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
#endif /* not used yet. */


/**
 * Deregistres the VM and destroys the VM structure.
 *
 * @returns VBox status code.
 * @param   pVM         Where to store the pointer to the VM structure.
 *
 * @thread  EMT.
 */
GVMMR0DECL(int) GVMMR0DestroyVM(PVM pVM)
{
    SUPR0Printf("GVMMR0DestroyVM: pVM=%p\n", pVM);

    PGVMM pGVMM = g_pGVMM;
    AssertPtrReturn(pGVMM, VERR_INTERNAL_ERROR);

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
    AssertReturn(pHandle->hEMT != NIL_RTNATIVETHREAD, VERR_WRONG_ORDER);
    AssertReturn(pHandle->pVM == pVM, VERR_NOT_OWNER);

    RTNATIVETHREAD hSelf = RTThreadNativeSelf();
    AssertReturn(pHandle->hEMT == hSelf, VERR_NOT_OWNER);

    /*
     * Lookup the handle and destroy the object.
     * Since the lock isn't recursive, we have to make sure nobody can
     * race us as we leave the lock and call SUPR0ObjRelease.
     */
    int rc = RTSemFastMutexRequest(pGVMM->Lock);
    AssertRC(rc);

    /* be very careful here because we might be racing someone else cleaning up... */
    if (    pHandle->pVM == pVM
        &&  pHandle->hEMT == hSelf
        &&  VALID_PTR(pHandle->pvObj))
    {
        void *pvObj = pHandle->pvObj;
        pHandle->pvObj = NULL;
        RTSemFastMutexRelease(pGVMM->Lock);

        SUPR0ObjRelease(pvObj, pVM->pSession);
    }
    else
    {
        RTSemFastMutexRelease(pGVMM->Lock);
        rc = VERR_INTERNAL_ERROR;
    }

    return rc;
}


#if 1 /* this approach is unsafe wrt to the freeing of pVM. Keeping it as a possible fallback for 1.5.x. */
/**
 * Register the specified VM with the GGVM.
 *
 * Permission polices and resource consumption polices may or may
 * not be checked that this poin, be ready to deald nicely with failure.
 *
 * @returns VBox status code.
 *
 * @param   pVM         The VM instance data (aka handle), ring-0 mapping of ccourse.
 *                      The VM::hGVM field may be updated by this call.
 * @thread  EMT.
 */
GVMMR0DECL(int) GVMMR0RegisterVM(PVM pVM)
{
    SUPR0Printf("GVMMR0RegisterVM: pVM=%p\n", pVM);
    PGVMM pGVMM = g_pGVMM;
    AssertPtrReturn(pGVMM, VERR_INTERNAL_ERROR);

    /*
     * Validate the VM structure and state.
     */
    AssertPtrReturn(pVM, VERR_INVALID_POINTER);
    AssertReturn(!((uintptr_t)pVM & PAGE_OFFSET_MASK), VERR_INVALID_POINTER);
    AssertMsgReturn(pVM->enmVMState == VMSTATE_CREATING, ("%d\n", pVM->enmVMState), VERR_WRONG_ORDER);

    RTNATIVETHREAD hEMT = RTThreadNativeSelf();
    AssertReturn(hEMT != NIL_RTNATIVETHREAD, VERR_NOT_SUPPORTED);

    /*
     * Take the lock and call the worker function.
     */
    int rc = RTSemFastMutexRequest(pGVMM->Lock);
    AssertRCReturn(rc, rc);

    /*
     * Allocate a handle.
     */
    uint16_t iHandle = pGVMM->iFreeHead;
    if (iHandle)
    {
        PGVMHANDLE pHandle = &pGVMM->aHandles[iHandle];

        /* consistency checks, a bit paranoid as always. */
        if (    !pHandle->pVM
            &&  !pHandle->pvObj
            &&  pHandle->iSelf == iHandle)
        {
            pHandle->pvObj = SUPR0ObjRegister(pVM->pSession, SUPDRVOBJTYPE_VM, gvmmR0HandleObjDestructor, pGVMM, pHandle);
            if (pHandle->pvObj)
            {
                /*
                 * Move the handle from the free to used list and
                 * perform permission checks.
                 */
                pGVMM->iFreeHead = pHandle->iNext;
                pHandle->iNext = pGVMM->iUsedHead;
                pGVMM->iUsedHead = iHandle;

                pHandle->fFreeVM = false;
                pHandle->pVM = pVM;
                pHandle->pGVM = NULL; /** @todo to be allocated */
                pHandle->pSession = pVM->pSession;
                pHandle->hEMT = hEMT;

                rc = SUPR0ObjVerifyAccess(pHandle->pvObj, pVM->pSession, NULL);
                if (RT_SUCCESS(rc))
                {
                    pVM->hSelf = iHandle;
                    RTSemFastMutexRelease(pGVMM->Lock);
                }
                else
                {
                    /*
                     * The user wasn't permitted to create this VM.
                     * Must use gvmmR0HandleObjDestructor via SUPR0ObjRelease to do the
                     * cleanups. The lock isn't recursive, thus the extra mess.
                     */
                    void *pvObj = pHandle->pvObj;
                    pHandle->pvObj = NULL;
                    RTSemFastMutexRelease(pGVMM->Lock);

                    SUPR0ObjRelease(pvObj, pVM->pSession);
                }
                if (RT_FAILURE(rc))
                    SUPR0Printf("GVMMR0RegisterVM: permission denied, rc=%d\n", rc);
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
    SUPR0Printf("GVMMR0RegisterVM: failed, rc=%d, iHandle=%d\n", rc, iHandle);
    return rc;
}


/**
 * Deregister a VM previously registered using the GVMMR0RegisterVM API.
 *
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @thread  EMT.
 */
GVMMR0DECL(int) GVMMR0DeregisterVM(PVM pVM)
{
    SUPR0Printf("GVMMR0DeregisterVM: pVM=%p\n", pVM);
    return GVMMR0DestroyVM(pVM);
}
#endif /* ... */


/**
 * Handle destructor.
 *
 * @param   pvGVMM       The GVM instance pointer.
 * @param   pvHandle    The handle pointer.
 */
static DECLCALLBACK(void) gvmmR0HandleObjDestructor(void *pvObj, void *pvGVMM, void *pvHandle)
{
    SUPR0Printf("gvmmR0HandleObjDestructor: %p %p %p\n", pvObj, pvGVMM, pvHandle);

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
     * Do the global cleanup round, currently only GMM.
     * Can't trust the VM pointer unless it was allocated in ring-0...
     */
    PVM pVM = pHandle->pVM;
    if (    VALID_PTR(pVM)
        &&  VALID_PTR(pHandle->pSession)
        &&  pHandle->fFreeVM)
    {
        /// @todo GMMR0CleanupVM(pVM);

        /*
         * Free the VM structure.
         */
        ASMAtomicXchgU32((uint32_t volatile *)&pVM->hSelf, NIL_GVM_HANDLE);
        SUPR0MemFree(pHandle->pSession, pVM->paVMPagesR3);
        SUPR0LowFree(pHandle->pSession, (uintptr_t)pVM);
    }

    /*
     * Free the handle.
     */
    pHandle->iNext = pGVMM->iFreeHead;
    pHandle->fFreeVM = false;
    pGVMM->iFreeHead = iHandle;
    ASMAtomicXchgPtr((void * volatile *)&pHandle->pVM, NULL);
    ASMAtomicXchgPtr((void * volatile *)&pHandle->pvObj, NULL);
    ASMAtomicXchgPtr((void * volatile *)&pHandle->pSession, NULL);
    ASMAtomicXchgSize(&pHandle->hEMT, NIL_RTNATIVETHREAD);

    RTSemFastMutexRelease(pGVMM->Lock);
    SUPR0Printf("gvmmR0HandleObjDestructor: returns\n");
}


/**
 * Lookup a GVM pointer by its handle.
 *
 * @returns The GVM pointer on success, NULL on failure.
 * @param   hGVM    The global VM handle. Asserts on bad handle.
 */
GVMMR0DECL(PGVM) GVMMR0ByHandle(uint32_t hGVM)
{
    PGVMM pGVMM = g_pGVMM;
    AssertPtrReturn(pGVMM, NULL);

    /*
     * Validate.
     */
    AssertReturn(hGVM != NIL_GVM_HANDLE, NULL);
    AssertReturn(hGVM < RT_ELEMENTS(pGVMM->aHandles), NULL);

    /*
     * Look it up.
     */
    AssertReturn(VALID_PTR(pGVMM->aHandles[hGVM].pvObj), NULL);
    AssertReturn(pGVMM->aHandles[hGVM].hEMT != NIL_RTNATIVETHREAD, NULL);
    Assert(VALID_PTR(pGVMM->aHandles[hGVM].pVM));

    return pGVMM->aHandles[hGVM].pGVM;
}


/**
 * Lookup a VM by its global handle.
 *
 * @returns The VM handle on success, NULL on failure.
 * @param   hGVM    The global VM handle. Asserts on bad handle.
 */
GVMMR0DECL(PVM) GVMMR0GetVMByHandle(uint32_t hGVM)
{
    PGVMM pGVMM = g_pGVMM;
    AssertPtrReturn(pGVMM, NULL);

    /*
     * Validate.
     */
    AssertReturn(hGVM != NIL_GVM_HANDLE, NULL);
    AssertReturn(hGVM < RT_ELEMENTS(pGVMM->aHandles), NULL);

    /*
     * Look it up.
     */
    AssertReturn(VALID_PTR(pGVMM->aHandles[hGVM].pvObj), NULL);
    AssertReturn(pGVMM->aHandles[hGVM].hEMT != NIL_RTNATIVETHREAD, NULL);
    Assert(VALID_PTR(pGVMM->aHandles[hGVM].pVM));

    return pGVMM->aHandles[hGVM].pVM;
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
     * Be very careful here as we're called in AssertMsgN context.
     */
    PGVMM pGVMM = g_pGVMM;
    if (!VALID_PTR(pGVMM))
        return NULL;

    if (hEMT == NIL_RTNATIVETHREAD)
        hEMT = RTThreadNativeSelf();

    /*
     * Search the handles, we don't dare take the lock (assert).
     */
    for (unsigned i = 1; i < RT_ELEMENTS(pGVMM->aHandles); i++)
        if (    pGVMM->aHandles[i].hEMT == hEMT
            &&  pGVMM->aHandles[i].iSelf == i
            &&  VALID_PTR(pGVMM->aHandles[i].pvObj)
            &&  VALID_PTR(pGVMM->aHandles[i].pVM))
            return pGVMM->aHandles[i].pVM;

    return NULL;
}


