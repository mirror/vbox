/* $Id$ */
/** @file
 * DBGF - Debugger Facility, Breakpoint Management.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/** @page pg_dbgf_bp            DBGF - The Debugger Facility, Breakpoint Management
 *
 * The debugger facilities breakpoint managers purpose is to efficiently manage
 * large amounts of breakpoints for various use cases like dtrace like operations
 * or execution flow tracing for instance. Especially execution flow tracing can
 * require thousands of breakpoints which need to be managed efficiently to not slow
 * down guest operation too much. Before the rewrite starting end of 2020, DBGF could
 * only handle 32 breakpoints (+ 4 hardware assisted breakpoints). The new
 * manager is supposed to be able to handle up to one million breakpoints.
 *
 * @see grp_dbgf
 *
 *
 * @section sec_dbgf_bp_owner   Breakpoint owners
 *
 * A single breakpoint owner has a mandatory ring-3 callback and an optional ring-0
 * callback assigned which is called whenever a breakpoint with the owner assigned is hit.
 * The common part of the owner is managed by a single table mapped into both ring-0
 * and ring-3 and the handle being the index into the table. This allows resolving
 * the handle to the internal structure efficiently. Searching for a free entry is
 * done using a bitmap indicating free and occupied entries. For the optional
 * ring-0 owner part there is a separate ring-0 only table for security reasons.
 *
 * The callback of the owner can be used to gather and log guest state information
 * and decide whether to continue guest execution or stop and drop into the debugger.
 * Breakpoints which don't have an owner assigned will always drop the VM right into
 * the debugger.
 *
 *
 * @section sec_dbgf_bp_bps     Breakpoints
 *
 * Breakpoints are referenced by an opaque handle which acts as an index into a global table
 * mapped into ring-3 and ring-0. Each entry contains the necessary state to manage the breakpoint
 * like trigger conditions, type, owner, etc. If an owner is given an optional opaque user argument
 * can be supplied which is passed in the respective owner callback. For owners with ring-0 callbacks
 * a dedicated ring-0 table is held saving possible ring-0 user arguments.
 *
 * To keep memory consumption under control and still support large amounts of
 * breakpoints the table is split into fixed sized chunks and the chunk index and index
 * into the chunk can be derived from the handle with only a few logical operations.
 *
 *
 * @section sec_dbgf_bp_resolv  Resolving breakpoint addresses
 *
 * Whenever a \#BP(0) event is triggered DBGF needs to decide whether the event originated
 * from within the guest or whether a DBGF breakpoint caused it. This has to happen as fast
 * as possible. The following scheme is employed to achieve this:
 *
 * @verbatim
 *                       7   6   5   4   3   2   1   0
 *                     +---+---+---+---+---+---+---+---+
 *                     |   |   |   |   |   |   |   |   | BP address
 *                     +---+---+---+---+---+---+---+---+
 *                      \_____________________/ \_____/
 *                                 |               |
 *                                 |               +---------------+
 *                                 |                               |
 *    BP table                     |                               v
 * +------------+                  |                         +-----------+
 * |   hBp 0    |                  |                    X <- | 0 | xxxxx |
 * |   hBp 1    | <----------------+------------------------ | 1 | hBp 1 |
 * |            |                  |                    +--- | 2 | idxL2 |
 * |   hBp <m>  | <---+            v                    |    |...|  ...  |
 * |            |     |      +-----------+              |    |...|  ...  |
 * |            |     |      |           |              |    |...|  ...  |
 * |   hBp <n>  | <-+ +----- | +> leaf   |              |    |     .     |
 * |            |   |        | |         |              |    |     .     |
 * |            |   |        | + root +  | <------------+    |     .     |
 * |            |   |        |        |  |                   +-----------+
 * |            |   +------- |   leaf<+  |                     L1: 65536
 * |     .      |            |     .     |
 * |     .      |            |     .     |
 * |     .      |            |     .     |
 * +------------+            +-----------+
 *                            L2 idx AVL
 * @endverbatim
 *
 *     -# Take the lowest 16 bits of the breakpoint address and use it as an direct index
 *        into the L1 table. The L1 table is contiguous and consists of 4 byte entries
 *        resulting in 256KiB of memory used. The topmost 4 bits indicate how to proceed
 *        and the meaning of the remaining 28bits depends on the topmost 4 bits:
 *            - A 0 type entry means no breakpoint is registered with the matching lowest 16bits,
 *              so forward the event to the guest.
 *            - A 1 in the topmost 4 bits means that the remaining 28bits directly denote a breakpoint
 *              handle which can be resolved by extracting the chunk index and index into the chunk
 *              of the global breakpoint table. If the address matches the breakpoint is processed
 *              according to the configuration. Otherwise the breakpoint is again forwarded to the guest.
 *            - A 2 in the topmost 4 bits means that there are multiple breakpoints registered
 *              matching the lowest 16bits and the search must continue in the L2 table with the
 *              remaining 28bits acting as an index into the L2 table indicating the search root.
 *     -# The L2 table consists of multiple index based AVL trees, there is one for each reference
 *        from the L1 table. The key for the table are the upper 6 bytes of the breakpoint address
 *        used for searching. This tree is traversed until either a matching address is found and
 *        the breakpoint is being processed or again forwarded to the guest if it isn't successful.
 *        Each entry in the L2 table is 16 bytes big and densly packed to avoid excessive memory usage.
 *
 *
 * @section sec_dbgf_bp_note    Random thoughts and notes for the implementation
 *
 * - The assumption for this approach is that the lowest 16bits of the breakpoint address are
 *   hopefully the ones being the most varying ones across breakpoints so the traversal
 *   can skip the L2 table in most of the cases. Even if the L2 table must be taken the
 *   individual trees should be quite shallow resulting in low overhead when walking it
 *   (though only real world testing can assert this assumption).
 * - Index based tables and trees are used instead of pointers because the tables
 *   are always mapped into ring-0 and ring-3 with different base addresses.
 * - Efficent breakpoint allocation is done by having a global bitmap indicating free
 *   and occupied breakpoint entries. Same applies for the L2 AVL table.
 * - Special care must be taken when modifying the L1 and L2 tables as other EMTs
 *   might still access it (want to try a lockless approach first using
 *   atomic updates, have to resort to locking if that turns out to be too difficult).
 * - Each BP entry is supposed to be 64 byte big and each chunk should contain 65536
 *   breakpoints which results in 4MiB for each chunk plus the allocation bitmap.
 * - ring-0 has to take special care when traversing the L2 AVL tree to not run into cycles
 *   and do strict bounds checking before accessing anything. The L1 and L2 table
 *   are written to from ring-3 only. Same goes for the breakpoint table with the
 *   exception being the opaque user argument for ring-0 which is stored in ring-0 only
 *   memory.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGF
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/selm.h>
#include <VBox/vmm/iem.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/iom.h>
#include <VBox/vmm/hm.h>
#include "DBGFInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/vmm/uvm.h>

#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/mem.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
RT_C_DECLS_BEGIN
RT_C_DECLS_END


/**
 * Initialize the breakpoint mangement.
 *
 * @returns VBox status code.
 * @param   pUVM                The user mode VM handle.
 */
DECLHIDDEN(int) dbgfR3BpInit(PUVM pUVM)
{
    PVM pVM = pUVM->pVM;

    /* Init hardware breakpoint states. */
    for (uint32_t i = 0; i < RT_ELEMENTS(pVM->dbgf.s.aHwBreakpoints); i++)
    {
        PDBGFBPHW pHwBp = &pVM->dbgf.s.aHwBreakpoints[i];

        AssertCompileSize(DBGFBP, sizeof(uint32_t));
        pHwBp->hBp      = NIL_DBGFBP;
        //pHwBp->fEnabled = false;
    }

    /* Now the global breakpoint table chunks. */
    for (uint32_t i = 0; i < RT_ELEMENTS(pUVM->dbgf.s.aBpChunks); i++)
    {
        PDBGFBPCHUNKR3 pBpChunk = &pUVM->dbgf.s.aBpChunks[i];

        //pBpChunk->pBpBaseR3 = NULL;
        //pBpChunk->pbmAlloc  = NULL;
        //pBpChunk->cBpsFree  = 0;
        pBpChunk->idChunk = DBGF_BP_CHUNK_ID_INVALID; /* Not allocated. */
    }

    //pUVM->dbgf.s.paBpLocL1R3 = NULL;
    return VINF_SUCCESS;
}


/**
 * Terminates the breakpoint mangement.
 *
 * @returns VBox status code.
 * @param   pUVM                The user mode VM handle.
 */
DECLHIDDEN(int) dbgfR3BpTerm(PUVM pUVM)
{
    /* Free all allocated chunk bitmaps (the chunks itself are destroyed during ring-0 VM destruction). */
    for (uint32_t i = 0; i < RT_ELEMENTS(pUVM->dbgf.s.aBpChunks); i++)
    {
        PDBGFBPCHUNKR3 pBpChunk = &pUVM->dbgf.s.aBpChunks[i];

        if (pBpChunk->idChunk != DBGF_BP_CHUNK_ID_INVALID)
        {
            AssertPtr(pBpChunk->pbmAlloc);
            RTMemFree((void *)pBpChunk->pbmAlloc);
            pBpChunk->pbmAlloc = NULL;
            pBpChunk->idChunk = DBGF_BP_CHUNK_ID_INVALID;
        }
    }

    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNVMMEMTRENDEZVOUS}
 */
static DECLCALLBACK(VBOXSTRICTRC) dbgfR3BpInitEmtWorker(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    RT_NOREF(pvUser);

    VMCPU_ASSERT_EMT(pVCpu);
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);

    /*
     * The initialization will be done on EMT(0). It is possible that multiple
     * initialization attempts are done because dbgfR3BpEnsureInit() can be called
     * from racing non EMT threads when trying to set a breakpoint for the first time.
     * Just fake success if the L1 is already present which means that a previous rendezvous
     * successfully initialized the breakpoint manager.
     */
    PUVM pUVM = pVM->pUVM;
    if (   pVCpu->idCpu == 0
        && !pUVM->dbgf.s.paBpLocL1R3)
    {
        DBGFBPINITREQ Req;
        Req.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
        Req.Hdr.cbReq    = sizeof(Req);
        Req.paBpLocL1R3  = NULL;
        int rc = VMMR3CallR0Emt(pVM, pVCpu, VMMR0_DO_DBGF_BP_INIT, 0 /*u64Arg*/, &Req.Hdr);
        AssertLogRelMsgRCReturn(rc, ("VMMR0_DO_DBGF_BP_INIT failed: %Rrc\n", rc), rc);
        pUVM->dbgf.s.paBpLocL1R3 = Req.paBpLocL1R3;
    }

    return VINF_SUCCESS;
}


/**
 * Ensures that the breakpoint manager is fully initialized.
 *
 * @returns VBox status code.
 * @param   pUVM                The user mode VM handle.
 *
 * @thread Any thread.
 */
static int dbgfR3BpEnsureInit(PUVM pUVM)
{
    /* If the L1 lookup table is allocated initialization succeeded before. */
    if (RT_LIKELY(pUVM->dbgf.s.paBpLocL1R3))
        return VINF_SUCCESS;

    /* Gather all EMTs and call into ring-0 to initialize the breakpoint manager. */
    return VMMR3EmtRendezvous(pUVM->pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ALL_AT_ONCE, dbgfR3BpInitEmtWorker, NULL /*pvUser*/);
}


/**
 * Returns the internal breakpoint state for the given handle.
 *
 * @returns Pointer to the internal breakpoint state or NULL if the handle is invalid.
 * @param   pUVM                The user mode VM handle.
 * @param   hBp                 The breakpoint handle to resolve.
 */
DECLINLINE(PDBGFBPINT) dbgfR3BpGetByHnd(PUVM pUVM, DBGFBP hBp)
{
    uint32_t idChunk  = DBGF_BP_HND_GET_CHUNK_ID(hBp);
    uint32_t idxEntry = DBGF_BP_HND_GET_ENTRY(hBp);

    AssertReturn(idChunk < DBGF_BP_CHUNK_COUNT, NULL);
    AssertReturn(idxEntry < DBGF_BP_COUNT_PER_CHUNK, NULL);

    PDBGFBPCHUNKR3 pBpChunk = &pUVM->dbgf.s.aBpChunks[idChunk];
    AssertReturn(pBpChunk->idChunk == idChunk, NULL);
    AssertPtrReturn(pBpChunk->pbmAlloc, NULL);
    AssertReturn(ASMBitTest(pBpChunk->pbmAlloc, idxEntry), NULL);

    return &pBpChunk->pBpBaseR3[idxEntry];
}


/**
 * Get a breakpoint give by address.
 *
 * @returns The breakpoint handle on success or NIL_DBGF if not found.
 * @param   pUVM                The user mode VM handle.
 * @param   enmType             The breakpoint type.
 * @param   GCPtr               The breakpoint address.
 * @param   ppBp                Where to store the pointer to the internal breakpoint state on success, optional.
 */
static DBGFBP dbgfR3BpGetByAddr(PUVM pUVM, DBGFBPTYPE enmType, RTGCUINTPTR GCPtr, PDBGFBPINT *ppBp)
{
    DBGFBP hBp = NIL_DBGFBP;

    switch (enmType)
    {
        case DBGFBPTYPE_REG:
        {
            PVM pVM = pUVM->pVM;
            VM_ASSERT_VALID_EXT_RETURN(pVM, NIL_DBGFBP);

            for (uint32_t i = 0; i < RT_ELEMENTS(pVM->dbgf.s.aHwBreakpoints); i++)
            {
                PDBGFBPHW pHwBp = &pVM->dbgf.s.aHwBreakpoints[i];

                AssertCompileSize(DBGFBP, sizeof(uint32_t));
                DBGFBP hBpTmp = ASMAtomicReadU32(&pHwBp->hBp);
                if (   pHwBp->GCPtr == GCPtr
                    && hBpTmp != NIL_DBGFBP)
                {
                    hBp = hBpTmp;
                    break;
                }
            }

            break;
        }

        case DBGFBPTYPE_INT3:
            break;

        default:
            AssertMsgFailed(("enmType=%d\n", enmType));
            break;
    }

    if (   hBp != NIL_DBGFBP
        && ppBp)
        *ppBp =  dbgfR3BpGetByHnd(pUVM, hBp);
    return hBp;
}


/**
 * @callback_method_impl{FNVMMEMTRENDEZVOUS}
 */
static DECLCALLBACK(VBOXSTRICTRC) dbgfR3BpChunkAllocEmtWorker(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    uint32_t idChunk = (uint32_t)(uintptr_t)pvUser;

    VMCPU_ASSERT_EMT(pVCpu);
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);

    AssertReturn(idChunk < DBGF_BP_CHUNK_COUNT, VERR_DBGF_BP_IPE_1);

    PUVM pUVM = pVM->pUVM;
    PDBGFBPCHUNKR3 pBpChunk = &pUVM->dbgf.s.aBpChunks[idChunk];

    AssertReturn(   pBpChunk->idChunk == DBGF_BP_CHUNK_ID_INVALID
                 || pBpChunk->idChunk == idChunk,
                 VERR_DBGF_BP_IPE_2);

    /*
     * The initialization will be done on EMT(0). It is possible that multiple
     * allocation attempts are done when multiple racing non EMT threads try to
     * allocate a breakpoint and a new chunk needs to be allocated.
     * Ignore the request and succeed if the chunk is allocated meaning that a
     * previous rendezvous successfully allocated the chunk.
     */
    int rc = VINF_SUCCESS;
    if (   pVCpu->idCpu == 0
        && pBpChunk->idChunk == DBGF_BP_CHUNK_ID_INVALID)
    {
        /* Allocate the bitmap first so we can skip calling into VMMR0 if it fails. */
        AssertCompile(!(DBGF_BP_COUNT_PER_CHUNK % 8));
        volatile void *pbmAlloc = RTMemAllocZ(DBGF_BP_COUNT_PER_CHUNK / 8);
        if (RT_LIKELY(pbmAlloc))
        {
            DBGFBPCHUNKALLOCREQ Req;
            Req.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
            Req.Hdr.cbReq    = sizeof(Req);
            Req.idChunk      = idChunk;
            Req.pChunkBaseR3 = NULL;
            rc = VMMR3CallR0Emt(pVM, pVCpu, VMMR0_DO_DBGF_BP_CHUNK_ALLOC, 0 /*u64Arg*/, &Req.Hdr);
            AssertLogRelMsgRC(rc, ("VMMR0_DO_DBGF_BP_CHUNK_ALLOC failed: %Rrc\n", rc));
            if (RT_SUCCESS(rc))
            {
                pBpChunk->pBpBaseR3 = (PDBGFBPINT)Req.pChunkBaseR3;
                pBpChunk->pbmAlloc   = pbmAlloc;
                pBpChunk->cBpsFree  = DBGF_BP_COUNT_PER_CHUNK;
                pBpChunk->idChunk   = idChunk;
                return VINF_SUCCESS;
            }

            RTMemFree((void *)pbmAlloc);
        }
        else
            rc = VERR_NO_MEMORY;
    }

    return rc;
}


/**
 * Tries to allocate the given chunk which requires an EMT rendezvous.
 *
 * @returns VBox status code.
 * @param   pUVM                The user mode VM handle.
 * @param   idChunk             The chunk to allocate.
 *
 * @thread Any thread.
 */
DECLINLINE(int) dbgfR3BpChunkAlloc(PUVM pUVM, uint32_t idChunk)
{
    return VMMR3EmtRendezvous(pUVM->pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ALL_AT_ONCE, dbgfR3BpChunkAllocEmtWorker, (void *)(uintptr_t)idChunk);
}


/**
 * Tries to allocate a new breakpoint of the given type.
 *
 * @returns VBox status code.
 * @param   pUVM                The user mode VM handle.
 * @param   hOwner              The owner handle, NIL_DBGFBPOWNER if none assigned.
 * @param   pvUser              Opaque user data passed in the owner callback.
 * @param   enmType             Breakpoint type to allocate.
 * @param   iHitTrigger         The hit count at which the breakpoint start triggering.
 *                              Use 0 (or 1) if it's gonna trigger at once.
 * @param   iHitDisable         The hit count which disables the breakpoint.
 *                              Use ~(uint64_t) if it's never gonna be disabled.
 * @param   phBp                Where to return the opaque breakpoint handle on success.
 * @param   ppBp                Where to return the pointer to the internal breakpoint state on success.
 *
 * @thread Any thread.
 */
static int dbgfR3BpAlloc(PUVM pUVM, DBGFBPOWNER hOwner, void *pvUser, DBGFBPTYPE enmType,
                         uint64_t iHitTrigger, uint64_t iHitDisable, PDBGFBP phBp,
                         PDBGFBPINT *ppBp)
{
    /*
     * Search for a chunk having a free entry, allocating new chunks
     * if the encountered ones are full.
     *
     * This can be called from multiple threads at the same time so special care
     * has to be taken to not require any locking here.
     */
    for (uint32_t i = 0; i < RT_ELEMENTS(pUVM->dbgf.s.aBpChunks); i++)
    {
        PDBGFBPCHUNKR3 pBpChunk = &pUVM->dbgf.s.aBpChunks[i];

        uint32_t idChunk  = ASMAtomicReadU32(&pBpChunk->idChunk);
        if (idChunk == DBGF_BP_CHUNK_ID_INVALID)
        {
            int rc = dbgfR3BpChunkAlloc(pUVM, i);
            if (RT_FAILURE(rc))
            {
                LogRel(("DBGF/Bp: Allocating new breakpoint table chunk failed with %Rrc\n", rc));
                break;
            }

            idChunk = ASMAtomicReadU32(&pBpChunk->idChunk);
            Assert(idChunk == i);
        }

        /** @todo Optimize with some hinting if this turns out to be too slow. */
        for (;;)
        {
            uint32_t cBpsFree = ASMAtomicReadU32(&pBpChunk->cBpsFree);
            if (cBpsFree)
            {
                /*
                 * Scan the associated bitmap for a free entry, if none can be found another thread
                 * raced us and we go to the next chunk.
                 */
                int32_t iClr = ASMBitFirstClear(pBpChunk->pbmAlloc, DBGF_BP_COUNT_PER_CHUNK);
                if (iClr != -1)
                {
                    /*
                     * Try to allocate, we could get raced here as well. In that case
                     * we try again.
                     */
                    if (!ASMAtomicBitTestAndSet(pBpChunk->pbmAlloc, iClr))
                    {
                        /* Success, immediately mark as allocated, initialize the breakpoint state and return. */
                        ASMAtomicDecU32(&pBpChunk->cBpsFree);

                        PDBGFBPINT pBp = &pBpChunk->pBpBaseR3[iClr];
                        pBp->Pub.cHits = 0;
                        pBp->Pub.iHitTrigger   = iHitTrigger;
                        pBp->Pub.iHitDisable   = iHitDisable;
                        pBp->Pub.hOwner        = hOwner;
                        pBp->Pub.fFlagsAndType = DBGF_BP_PUB_SET_FLAGS_AND_TYPE(enmType, DBGF_BP_F_DEFAULT);
                        pBp->pvUserR3          = pvUser;

                        /** @todo Owner handling (reference and call ring-0 if it has an ring-0 callback). */

                        *phBp = DBGF_BP_HND_CREATE(idChunk, iClr);
                        *ppBp = pBp;
                        return VINF_SUCCESS;
                    }
                    /* else Retry with another spot. */
                }
                else /* no free entry in bitmap, go to the next chunk */
                    break;
            }
            else /* !cBpsFree, go to the next chunk */
                break;
        }
    }

    return VERR_DBGF_NO_MORE_BP_SLOTS;
}


/**
 * Frees the given breakpoint handle.
 *
 * @returns nothing.
 * @param   pUVM                The user mode VM handle.
 * @param   hBp                 The breakpoint handle to free.
 * @param   pBp                 The internal breakpoint state pointer.
 */
static void dbgfR3BpFree(PUVM pUVM, DBGFBP hBp, PDBGFBPINT pBp)
{
    uint32_t idChunk  = DBGF_BP_HND_GET_CHUNK_ID(hBp);
    uint32_t idxEntry = DBGF_BP_HND_GET_ENTRY(hBp);

    AssertReturnVoid(idChunk < DBGF_BP_CHUNK_COUNT);
    AssertReturnVoid(idxEntry < DBGF_BP_COUNT_PER_CHUNK);

    PDBGFBPCHUNKR3 pBpChunk = &pUVM->dbgf.s.aBpChunks[idChunk];
    AssertPtrReturnVoid(pBpChunk->pbmAlloc);
    AssertReturnVoid(ASMBitTest(pBpChunk->pbmAlloc, idxEntry));

    /** @todo Need a trip to Ring-0 if an owner is assigned with a Ring-0 part to clear the breakpoint. */
    /** @todo Release owner. */
    memset(pBp, 0, sizeof(*pBp));

    ASMAtomicBitClear(pBpChunk->pbmAlloc, idxEntry);
    ASMAtomicIncU32(&pBpChunk->cBpsFree);
}


/**
 * Sets the enabled flag of the given breakpoint to the given value.
 *
 * @returns nothing.
 * @param   pBp                 The breakpoint to set the state.
 * @param   fEnabled            Enabled status.
 */
DECLINLINE(void) dbgfR3BpSetEnabled(PDBGFBPINT pBp, bool fEnabled)
{
    DBGFBPTYPE enmType = DBGF_BP_PUB_GET_TYPE(pBp->Pub.fFlagsAndType);
    if (fEnabled)
        pBp->Pub.fFlagsAndType = DBGF_BP_PUB_SET_FLAGS_AND_TYPE(enmType, DBGF_BP_F_ENABLED);
    else
        pBp->Pub.fFlagsAndType = DBGF_BP_PUB_SET_FLAGS_AND_TYPE(enmType, 0 /*fFlags*/);
}


/**
 * Assigns a hardware breakpoint state to the given register breakpoint.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross-context VM structure pointer.
 * @param   hBp                 The breakpoint handle to assign.
 * @param   pBp                 The internal breakpoint state.
 *
 * @thread Any thread.
 */
static int dbgfR3BpRegAssign(PVM pVM, DBGFBP hBp, PDBGFBPINT pBp)
{
    AssertReturn(pBp->Pub.u.Reg.iReg == UINT8_MAX, VERR_DBGF_BP_IPE_3);

    for (uint8_t i = 0; i < RT_ELEMENTS(pVM->dbgf.s.aHwBreakpoints); i++)
    {
        PDBGFBPHW pHwBp = &pVM->dbgf.s.aHwBreakpoints[i];

        AssertCompileSize(DBGFBP, sizeof(uint32_t));
        if (ASMAtomicCmpXchgU32(&pHwBp->hBp, hBp, NIL_DBGFBP))
        {
            pHwBp->GCPtr    = pBp->Pub.u.Reg.GCPtr;
            pHwBp->fType    = pBp->Pub.u.Reg.fType;
            pHwBp->cb       = pBp->Pub.u.Reg.cb;
            pHwBp->fEnabled = DBGF_BP_PUB_IS_ENABLED(pBp->Pub.fFlagsAndType);

            pBp->Pub.u.Reg.iReg = i;
            return VINF_SUCCESS;
        }
    }

    return VERR_DBGF_NO_MORE_BP_SLOTS;
}


/**
 * Removes the assigned hardware breakpoint state from the given register breakpoint.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross-context VM structure pointer.
 * @param   hBp                 The breakpoint handle to remove.
 * @param   pBp                 The internal breakpoint state.
 *
 * @thread Any thread.
 */
static int dbgfR3BpRegRemove(PVM pVM, DBGFBP hBp, PDBGFBPINT pBp)
{
    AssertReturn(pBp->Pub.u.Reg.iReg < RT_ELEMENTS(pVM->dbgf.s.aHwBreakpoints), VERR_DBGF_BP_IPE_3);

    PDBGFBPHW pHwBp = &pVM->dbgf.s.aHwBreakpoints[pBp->Pub.u.Reg.iReg];
    AssertReturn(pHwBp->hBp == hBp, VERR_DBGF_BP_IPE_4);
    AssertReturn(!pHwBp->fEnabled, VERR_DBGF_BP_IPE_5);

    pHwBp->GCPtr = 0;
    pHwBp->fType = 0;
    pHwBp->cb    = 0;
    ASMCompilerBarrier();

    ASMAtomicWriteU32(&pHwBp->hBp, NIL_DBGFBP);
    return VINF_SUCCESS;
}


/**
 * Adds the given int3 breakpoint to the appropriate lookup tables.
 *
 * @returns VBox status code.
 * @param   pUVM                The user mode VM handle.
 * @param   hBp                 The breakpoint handle to add.
 * @param   pBp                 The internal breakpoint state.
 */
static int dbgfR3BpInt3Add(PUVM pUVM, DBGFBP hBp, PDBGFBPINT pBp)
{
    AssertReturn(DBGF_BP_PUB_GET_TYPE(pBp->Pub.fFlagsAndType) == DBGFBPTYPE_INT3, VERR_DBGF_BP_IPE_3);

    int rc = VINF_SUCCESS;
    uint16_t idxL1 = DBGF_BP_INT3_L1_IDX_EXTRACT_FROM_ADDR(pBp->Pub.u.Int3.GCPtr);
    uint8_t  cTries = 16;

    while (cTries--)
    {
        uint32_t u32Entry = ASMAtomicReadU32(&pUVM->dbgf.s.paBpLocL1R3[idxL1]);

        if (u32Entry == DBGF_BP_INT3_L1_ENTRY_TYPE_NULL)
        {
            /*
             * No breakpoint assigned so far for this entry, create an entry containing
             * the direct breakpoint handle and try to exchange it atomically.
             */
            u32Entry = DBGF_BP_INT3_L1_ENTRY_CREATE_BP_HND(hBp);
            if (ASMAtomicCmpXchgU32(&pUVM->dbgf.s.paBpLocL1R3[idxL1], u32Entry, DBGF_BP_INT3_L1_ENTRY_TYPE_NULL))
                break;
        }
        else
        {
            uint8_t u8Type = DBGF_BP_INT3_L1_ENTRY_GET_TYPE(u32Entry);
            if (u8Type == DBGF_BP_INT3_L1_ENTRY_TYPE_BP_HND)
            {
                /** @todo Allocate a new root entry and one leaf to accomodate for the two handles,
                 * then replace the new entry. */
                rc = VERR_NOT_IMPLEMENTED;
                break;
            }
            else if (u8Type == DBGF_BP_INT3_L1_ENTRY_TYPE_L2_IDX)
            {
                /** @todo Walk the L2 tree searching for the correct spot, add the new entry
                 * and rebalance the tree. */
                rc = VERR_NOT_IMPLEMENTED;
                break;
            }
        }
    }

    if (   RT_SUCCESS(rc)
        && !cTries) /* Too much contention, abort with an error. */
        rc = VERR_DBGF_BP_INT3_ADD_TRIES_REACHED;

    return rc;
}


/**
 * Removes the given int3 breakpoint from all lookup tables.
 *
 * @returns VBox status code.
 * @param   pUVM                The user mode VM handle.
 * @param   hBp                 The breakpoint handle to remove.
 * @param   pBp                 The internal breakpoint state.
 */
static int dbgfR3BpInt3Remove(PUVM pUVM, DBGFBP hBp, PDBGFBPINT pBp)
{
    AssertReturn(DBGF_BP_PUB_GET_TYPE(pBp->Pub.fFlagsAndType) == DBGFBPTYPE_INT3, VERR_DBGF_BP_IPE_3);

    int rc = VINF_SUCCESS;
    uint16_t idxL1 = DBGF_BP_INT3_L1_IDX_EXTRACT_FROM_ADDR(pBp->Pub.u.Int3.GCPtr);

    for (;;)
    {
        uint32_t u32Entry = ASMAtomicReadU32(&pUVM->dbgf.s.paBpLocL1R3[idxL1]);
        AssertReturn(u32Entry != DBGF_BP_INT3_L1_ENTRY_TYPE_NULL, VERR_DBGF_BP_IPE_6);

        uint8_t u8Type = DBGF_BP_INT3_L1_ENTRY_GET_TYPE(u32Entry);
        if (u8Type == DBGF_BP_INT3_L1_ENTRY_TYPE_BP_HND)
        {
            /* Single breakpoint, just exchange atomically with the null value. */
            if (ASMAtomicCmpXchgU32(&pUVM->dbgf.s.paBpLocL1R3[idxL1], DBGF_BP_INT3_L1_ENTRY_TYPE_NULL, u32Entry))
                break;
            break;
        }
        else if (u8Type == DBGF_BP_INT3_L1_ENTRY_TYPE_L2_IDX)
        {
            /** @todo Walk the L2 tree searching for the correct spot, remove the entry
             * and rebalance the tree. */
            RT_NOREF(hBp);
            rc = VERR_NOT_IMPLEMENTED;
            break;
        }
    }

    return rc;
}


/**
 * @callback_method_impl{FNVMMEMTRENDEZVOUS}
 */
static DECLCALLBACK(VBOXSTRICTRC) dbgfR3BpRegRecalcOnCpu(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    RT_NOREF(pvUser);

    /*
     * CPU 0 updates the enabled hardware breakpoint counts.
     */
    if (pVCpu->idCpu == 0)
    {
        pVM->dbgf.s.cEnabledHwBreakpoints   = 0;
        pVM->dbgf.s.cEnabledHwIoBreakpoints = 0;

        for (uint32_t iBp = 0; iBp < RT_ELEMENTS(pVM->dbgf.s.aHwBreakpoints); iBp++)
        {
            if (pVM->dbgf.s.aHwBreakpoints[iBp].fEnabled)
            {
                pVM->dbgf.s.cEnabledHwBreakpoints   += 1;
                pVM->dbgf.s.cEnabledHwIoBreakpoints += pVM->dbgf.s.aHwBreakpoints[iBp].fType == X86_DR7_RW_IO;
            }
        }
    }

    return CPUMRecalcHyperDRx(pVCpu, UINT8_MAX, false);
}


/**
 * Arms the given breakpoint.
 *
 * @returns VBox status code.
 * @param   pUVM                The user mode VM handle.
 * @param   hBp                 The breakpoint handle to arm.
 * @param   pBp                 The internal breakpoint state pointer for the handle.
 *
 * @thread Any thread.
 */
static int dbgfR3BpArm(PUVM pUVM, DBGFBP hBp, PDBGFBPINT pBp)
{
    int rc = VINF_SUCCESS;
    PVM pVM = pUVM->pVM;

    Assert(!DBGF_BP_PUB_IS_ENABLED(pBp->Pub.fFlagsAndType));
    switch (DBGF_BP_PUB_GET_TYPE(pBp->Pub.fFlagsAndType))
    {
        case DBGFBPTYPE_REG:
        {
            Assert(pBp->Pub.u.Reg.iReg < RT_ELEMENTS(pVM->dbgf.s.aHwBreakpoints));
            PDBGFBPHW pBpHw = &pVM->dbgf.s.aHwBreakpoints[pBp->Pub.u.Reg.iReg];
            Assert(pBpHw->hBp == hBp); RT_NOREF(hBp);

            dbgfR3BpSetEnabled(pBp, true /*fEnabled*/);
            ASMAtomicWriteBool(&pBpHw->fEnabled, true);
            rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ALL_AT_ONCE, dbgfR3BpRegRecalcOnCpu, NULL);
            if (RT_FAILURE(rc))
            {
                ASMAtomicWriteBool(&pBpHw->fEnabled, false);
                dbgfR3BpSetEnabled(pBp, false /*fEnabled*/);
            }
            break;
        }
        case DBGFBPTYPE_INT3:
        {
            dbgfR3BpSetEnabled(pBp, true /*fEnabled*/);

            /** @todo When we enable the first int3 breakpoint we should do this in an EMT rendezvous
             * as the VMX code intercepts #BP only when at least one int3 breakpoint is enabled.
             * A racing vCPU might trigger it and forward it to the guest causing panics/crashes/havoc. */
            /*
             * Save current byte and write the int3 instruction byte.
             */
            rc = PGMPhysSimpleReadGCPhys(pVM, &pBp->Pub.u.Int3.bOrg, pBp->Pub.u.Int3.PhysAddr, sizeof(pBp->Pub.u.Int3.bOrg));
            if (RT_SUCCESS(rc))
            {
                static const uint8_t s_bInt3 = 0xcc;
                rc = PGMPhysSimpleWriteGCPhys(pVM, pBp->Pub.u.Int3.PhysAddr, &s_bInt3, sizeof(s_bInt3));
                if (RT_SUCCESS(rc))
                {
                    ASMAtomicIncU32(&pVM->dbgf.s.cEnabledInt3Breakpoints);
                    Log(("DBGF: Set breakpoint at %RGv (Phys %RGp)\n", pBp->Pub.u.Int3.GCPtr, pBp->Pub.u.Int3.PhysAddr));
                }
            }

            if (RT_FAILURE(rc))
                dbgfR3BpSetEnabled(pBp, false /*fEnabled*/);

            break;
        }
        case DBGFBPTYPE_PORT_IO:
        case DBGFBPTYPE_MMIO:
            rc = VERR_NOT_IMPLEMENTED;
            break;
        default:
            AssertMsgFailedReturn(("Invalid breakpoint type %d\n", DBGF_BP_PUB_GET_TYPE(pBp->Pub.fFlagsAndType)),
                                  VERR_IPE_NOT_REACHED_DEFAULT_CASE);
    }

    return rc;
}


/**
 * Disarms the given breakpoint.
 *
 * @returns VBox status code.
 * @param   pUVM                The user mode VM handle.
 * @param   hBp                 The breakpoint handle to disarm.
 * @param   pBp                 The internal breakpoint state pointer for the handle.
 *
 * @thread Any thread.
 */
static int dbgfR3BpDisarm(PUVM pUVM, DBGFBP hBp, PDBGFBPINT pBp)
{
    int rc = VINF_SUCCESS;
    PVM pVM = pUVM->pVM;

    Assert(DBGF_BP_PUB_IS_ENABLED(pBp->Pub.fFlagsAndType));
    switch (DBGF_BP_PUB_GET_TYPE(pBp->Pub.fFlagsAndType))
    {
        case DBGFBPTYPE_REG:
        {
            Assert(pBp->Pub.u.Reg.iReg < RT_ELEMENTS(pVM->dbgf.s.aHwBreakpoints));
            PDBGFBPHW pBpHw = &pVM->dbgf.s.aHwBreakpoints[pBp->Pub.u.Reg.iReg];
            Assert(pBpHw->hBp == hBp); RT_NOREF(hBp);

            dbgfR3BpSetEnabled(pBp, false /*fEnabled*/);
            ASMAtomicWriteBool(&pBpHw->fEnabled, false);
            rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ALL_AT_ONCE, dbgfR3BpRegRecalcOnCpu, NULL);
            if (RT_FAILURE(rc))
            {
                ASMAtomicWriteBool(&pBpHw->fEnabled, true);
                dbgfR3BpSetEnabled(pBp, true /*fEnabled*/);
            }
            break;
        }
        case DBGFBPTYPE_INT3:
        {
            dbgfR3BpSetEnabled(pBp, false /*fEnabled*/);

            /*
             * Check that the current byte is the int3 instruction, and restore the original one.
             * We currently ignore invalid bytes.
             */
            uint8_t bCurrent = 0;
            rc = PGMPhysSimpleReadGCPhys(pVM, &bCurrent, pBp->Pub.u.Int3.PhysAddr, sizeof(bCurrent));
            if (   RT_SUCCESS(rc)
                && bCurrent == 0xcc)
            {
                rc = PGMPhysSimpleWriteGCPhys(pVM, pBp->Pub.u.Int3.PhysAddr, &pBp->Pub.u.Int3.bOrg, sizeof(pBp->Pub.u.Int3.bOrg));
                if (RT_SUCCESS(rc))
                {
                    ASMAtomicDecU32(&pVM->dbgf.s.cEnabledInt3Breakpoints);
                    Log(("DBGF: Removed breakpoint at %RGv (Phys %RGp)\n", pBp->Pub.u.Int3.GCPtr, pBp->Pub.u.Int3.PhysAddr));
                }
            }

            if (RT_FAILURE(rc))
                dbgfR3BpSetEnabled(pBp, true /*fEnabled*/);

            break;
        }
        case DBGFBPTYPE_PORT_IO:
        case DBGFBPTYPE_MMIO:
            rc = VERR_NOT_IMPLEMENTED;
            break;
        default:
            AssertMsgFailedReturn(("Invalid breakpoint type %d\n", DBGF_BP_PUB_GET_TYPE(pBp->Pub.fFlagsAndType)),
                                  VERR_IPE_NOT_REACHED_DEFAULT_CASE);
    }

    return rc;
}


/**
 * Creates a new breakpoint owner returning a handle which can be used when setting breakpoints.
 *
 * @returns VBox status code.
 * @param   pUVM                The user mode VM handle.
 * @param   pfnBpHit            The R3 callback which is called when a breakpoint with the owner handle is hit.
 * @param   phBpOwner           Where to store the owner handle on success.
 */
VMMR3DECL(int) DBGFR3BpOwnerCreate(PUVM pUVM, PFNDBGFBPHIT pfnBpHit, PDBGFBPOWNER phBpOwner)
{
    /*
     * Validate the input.
     */
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    AssertPtrReturn(pfnBpHit, VERR_INVALID_PARAMETER);
    AssertPtrReturn(phBpOwner, VERR_INVALID_POINTER);

    return VERR_NOT_IMPLEMENTED;
}


/**
 * Destroys the owner identified by the given handle.
 *
 * @returns VBox status code.
 * @retval  VERR_DBGF_OWNER_BUSY if there are still breakpoints set with the given owner handle.
 * @param   pUVM                The user mode VM handle.
 * @param   hBpOwner            The breakpoint owner handle to destroy.
 */
VMMR3DECL(int) DBGFR3BpOwnerDestroy(PUVM pUVM, DBGFBPOWNER hBpOwner)
{
    /*
     * Validate the input.
     */
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(hBpOwner != NIL_DBGFBPOWNER, VERR_INVALID_HANDLE);

    return VERR_NOT_IMPLEMENTED;
}


/**
 * Sets a breakpoint (int 3 based).
 *
 * @returns VBox status code.
 * @param   pUVM        The user mode VM handle.
 * @param   idSrcCpu    The ID of the virtual CPU used for the
 *                      breakpoint address resolution.
 * @param   pAddress    The address of the breakpoint.
 * @param   iHitTrigger The hit count at which the breakpoint start triggering.
 *                      Use 0 (or 1) if it's gonna trigger at once.
 * @param   iHitDisable The hit count which disables the breakpoint.
 *                      Use ~(uint64_t) if it's never gonna be disabled.
 * @param   phBp        Where to store the breakpoint handle on success.
 *
 * @thread  Any thread.
 */
VMMR3DECL(int) DBGFR3BpSetInt3(PUVM pUVM, VMCPUID idSrcCpu, PCDBGFADDRESS pAddress,
                               uint64_t iHitTrigger, uint64_t iHitDisable, PDBGFBP phBp)
{
    return DBGFR3BpSetInt3Ex(pUVM, NIL_DBGFBPOWNER, NULL /*pvUser*/, idSrcCpu, pAddress,
                             iHitTrigger, iHitDisable, phBp);
}


/**
 * Sets a breakpoint (int 3 based) - extended version.
 *
 * @returns VBox status code.
 * @param   pUVM            The user mode VM handle.
 * @param   hOwner          The owner handle, use NIL_DBGFBPOWNER if no special owner attached.
 * @param   pvUser          Opaque user data to pass in the owner callback.
 * @param   idSrcCpu        The ID of the virtual CPU used for the
 *                          breakpoint address resolution.
 * @param   pAddress        The address of the breakpoint.
 * @param   iHitTrigger     The hit count at which the breakpoint start triggering.
 *                          Use 0 (or 1) if it's gonna trigger at once.
 * @param   iHitDisable     The hit count which disables the breakpoint.
 *                          Use ~(uint64_t) if it's never gonna be disabled.
 * @param   phBp            Where to store the breakpoint handle on success.
 *
 * @thread  Any thread.
 */
VMMR3DECL(int) DBGFR3BpSetInt3Ex(PUVM pUVM, DBGFBPOWNER hOwner, void *pvUser,
                                 VMCPUID idSrcCpu, PCDBGFADDRESS pAddress,
                                 uint64_t iHitTrigger, uint64_t iHitDisable, PDBGFBP phBp)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(hOwner != NIL_DBGFBPOWNER || pvUser == NULL, VERR_INVALID_PARAMETER);
    AssertReturn(DBGFR3AddrIsValid(pUVM, pAddress), VERR_INVALID_PARAMETER);
    AssertReturn(iHitTrigger <= iHitDisable, VERR_INVALID_PARAMETER);
    AssertPtrReturn(phBp, VERR_INVALID_POINTER);

    int rc = dbgfR3BpEnsureInit(pUVM);
    AssertRCReturn(rc, rc);

    DBGFBP hBp = NIL_DBGFBP;
    PDBGFBPINT pBp = NULL;
    rc = dbgfR3BpAlloc(pUVM, hOwner, pvUser, DBGFBPTYPE_INT3, iHitTrigger, iHitDisable, &hBp, &pBp);
    if (RT_SUCCESS(rc))
    {
        /*
         * Translate & save the breakpoint address into a guest-physical address.
         */
        rc = DBGFR3AddrToPhys(pUVM, idSrcCpu, pAddress, &pBp->Pub.u.Int3.PhysAddr);
        if (RT_SUCCESS(rc))
        {
            /*
             * The physical address from DBGFR3AddrToPhys() is the start of the page,
             * we need the exact byte offset into the page while writing to it in dbgfR3BpInt3Arm().
             */
            pBp->Pub.u.Int3.PhysAddr |= (pAddress->FlatPtr & X86_PAGE_OFFSET_MASK);
            pBp->Pub.u.Int3.GCPtr     = pAddress->FlatPtr;

            /* Add the breakpoint to the lookup tables. */
            rc = dbgfR3BpInt3Add(pUVM, hBp, pBp);
            if (RT_SUCCESS(rc))
            {
                /* Enable the breakpoint. */
                rc = dbgfR3BpArm(pUVM, hBp, pBp);
                if (RT_SUCCESS(rc))
                {
                    *phBp = hBp;
                    return VINF_SUCCESS;
                }

                int rc2 = dbgfR3BpInt3Remove(pUVM, hBp, pBp); AssertRC(rc2);
            }
        }

        dbgfR3BpFree(pUVM, hBp, pBp);
    }

    return rc;
}


/**
 * Sets a register breakpoint.
 *
 * @returns VBox status code.
 * @param   pUVM            The user mode VM handle.
 * @param   pAddress        The address of the breakpoint.
 * @param   iHitTrigger     The hit count at which the breakpoint start triggering.
 *                          Use 0 (or 1) if it's gonna trigger at once.
 * @param   iHitDisable     The hit count which disables the breakpoint.
 *                          Use ~(uint64_t) if it's never gonna be disabled.
 * @param   fType           The access type (one of the X86_DR7_RW_* defines).
 * @param   cb              The access size - 1,2,4 or 8 (the latter is AMD64 long mode only.
 *                          Must be 1 if fType is X86_DR7_RW_EO.
 * @param   phBp            Where to store the breakpoint handle.
 *
 * @thread  Any thread.
 */
VMMR3DECL(int) DBGFR3BpSetReg(PUVM pUVM, PCDBGFADDRESS pAddress, uint64_t iHitTrigger,
                              uint64_t iHitDisable, uint8_t fType, uint8_t cb, PDBGFBP phBp)
{
    return DBGFR3BpSetRegEx(pUVM, NIL_DBGFBPOWNER, NULL /*pvUser*/, pAddress,
                            iHitTrigger, iHitDisable, fType, cb, phBp);
}


/**
 * Sets a register breakpoint - extended version.
 *
 * @returns VBox status code.
 * @param   pUVM            The user mode VM handle.
 * @param   hOwner          The owner handle, use NIL_DBGFBPOWNER if no special owner attached.
 * @param   pvUser          Opaque user data to pass in the owner callback.
 * @param   pAddress        The address of the breakpoint.
 * @param   iHitTrigger     The hit count at which the breakpoint start triggering.
 *                          Use 0 (or 1) if it's gonna trigger at once.
 * @param   iHitDisable     The hit count which disables the breakpoint.
 *                          Use ~(uint64_t) if it's never gonna be disabled.
 * @param   fType           The access type (one of the X86_DR7_RW_* defines).
 * @param   cb              The access size - 1,2,4 or 8 (the latter is AMD64 long mode only.
 *                          Must be 1 if fType is X86_DR7_RW_EO.
 * @param   phBp            Where to store the breakpoint handle.
 *
 * @thread  Any thread.
 */
VMMR3DECL(int) DBGFR3BpSetRegEx(PUVM pUVM, DBGFBPOWNER hOwner, void *pvUser,
                                PCDBGFADDRESS pAddress, uint64_t iHitTrigger, uint64_t iHitDisable,
                                uint8_t fType, uint8_t cb, PDBGFBP phBp)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(hOwner != NIL_DBGFBPOWNER || pvUser == NULL, VERR_INVALID_PARAMETER);
    AssertReturn(DBGFR3AddrIsValid(pUVM, pAddress), VERR_INVALID_PARAMETER);
    AssertReturn(iHitTrigger <= iHitDisable, VERR_INVALID_PARAMETER);
    AssertReturn(cb > 0 && cb <= 8 && RT_IS_POWER_OF_TWO(cb), VERR_INVALID_PARAMETER);
    AssertPtrReturn(phBp, VERR_INVALID_POINTER);
    switch (fType)
    {
        case X86_DR7_RW_EO:
            if (cb == 1)
                break;
            AssertMsgFailedReturn(("fType=%#x cb=%d != 1\n", fType, cb), VERR_INVALID_PARAMETER);
        case X86_DR7_RW_IO:
        case X86_DR7_RW_RW:
        case X86_DR7_RW_WO:
            break;
        default:
            AssertMsgFailedReturn(("fType=%#x\n", fType), VERR_INVALID_PARAMETER);
    }

    int rc = dbgfR3BpEnsureInit(pUVM);
    AssertRCReturn(rc, rc);

    PDBGFBPINT pBp = NULL;
    DBGFBP hBp = dbgfR3BpGetByAddr(pUVM, DBGFBPTYPE_REG, pAddress->FlatPtr, &pBp);
    if (    hBp != NIL_DBGFBP
        &&  pBp->Pub.u.Reg.cb == cb
        &&  pBp->Pub.u.Reg.fType == fType)
    {
        rc = VINF_SUCCESS;
        if (!DBGF_BP_PUB_IS_ENABLED(pBp->Pub.fFlagsAndType))
            rc = dbgfR3BpArm(pUVM, hBp, pBp);
        if (RT_SUCCESS(rc))
        {
            rc = VINF_DBGF_BP_ALREADY_EXIST;
            if (phBp)
                *phBp = hBp;
        }
        return rc;
    }

    /* Allocate new breakpoint. */
    rc = dbgfR3BpAlloc(pUVM, hOwner, pvUser, DBGFBPTYPE_REG, iHitTrigger, iHitDisable, &hBp, &pBp);
    if (RT_SUCCESS(rc))
    {
        pBp->Pub.u.Reg.GCPtr = pAddress->FlatPtr;
        pBp->Pub.u.Reg.fType = fType;
        pBp->Pub.u.Reg.cb    = cb;
        pBp->Pub.u.Reg.iReg  = UINT8_MAX;
        ASMCompilerBarrier();

        /* Assign the proper hardware breakpoint. */
        rc = dbgfR3BpRegAssign(pUVM->pVM, hBp, pBp);
        if (RT_SUCCESS(rc))
        {
            /* Arm the breakpoint. */
            rc = dbgfR3BpArm(pUVM, hBp, pBp);
            if (RT_SUCCESS(rc))
            {
                if (phBp)
                    *phBp = hBp;
                return VINF_SUCCESS;
            }
            else
            {
                int rc2 = dbgfR3BpRegRemove(pUVM->pVM, hBp, pBp);
                AssertRC(rc2); RT_NOREF(rc2);
            }
        }

        dbgfR3BpFree(pUVM, hBp, pBp);
    }

    return rc;
}


/**
 * This is only kept for now to not mess with the debugger implementation at this point,
 * recompiler breakpoints are not supported anymore (IEM has some API but it isn't implemented
 * and should probably be merged with the DBGF breakpoints).
 */
VMMR3DECL(int) DBGFR3BpSetREM(PUVM pUVM, PCDBGFADDRESS pAddress, uint64_t iHitTrigger,
                              uint64_t iHitDisable, PDBGFBP phBp)
{
    RT_NOREF(pUVM, pAddress, iHitTrigger, iHitDisable, phBp);
    return VERR_NOT_SUPPORTED;
}


/**
 * Sets an I/O port breakpoint.
 *
 * @returns VBox status code.
 * @param   pUVM            The user mode VM handle.
 * @param   uPort           The first I/O port.
 * @param   cPorts          The number of I/O ports, see DBGFBPIOACCESS_XXX.
 * @param   fAccess         The access we want to break on.
 * @param   iHitTrigger     The hit count at which the breakpoint start
 *                          triggering. Use 0 (or 1) if it's gonna trigger at
 *                          once.
 * @param   iHitDisable     The hit count which disables the breakpoint.
 *                          Use ~(uint64_t) if it's never gonna be disabled.
 * @param   phBp            Where to store the breakpoint handle.
 *
 * @thread  Any thread.
 */
VMMR3DECL(int) DBGFR3BpSetPortIo(PUVM pUVM, RTIOPORT uPort, RTIOPORT cPorts, uint32_t fAccess,
                                 uint64_t iHitTrigger, uint64_t iHitDisable, PDBGFBP phBp)
{
    return DBGFR3BpSetPortIoEx(pUVM, NIL_DBGFBPOWNER, NULL /*pvUser*/, uPort, cPorts,
                               fAccess, iHitTrigger, iHitDisable, phBp);
}


/**
 * Sets an I/O port breakpoint - extended version.
 *
 * @returns VBox status code.
 * @param   pUVM            The user mode VM handle.
 * @param   hOwner          The owner handle, use NIL_DBGFBPOWNER if no special owner attached.
 * @param   pvUser          Opaque user data to pass in the owner callback.
 * @param   uPort           The first I/O port.
 * @param   cPorts          The number of I/O ports, see DBGFBPIOACCESS_XXX.
 * @param   fAccess         The access we want to break on.
 * @param   iHitTrigger     The hit count at which the breakpoint start
 *                          triggering. Use 0 (or 1) if it's gonna trigger at
 *                          once.
 * @param   iHitDisable     The hit count which disables the breakpoint.
 *                          Use ~(uint64_t) if it's never gonna be disabled.
 * @param   phBp            Where to store the breakpoint handle.
 *
 * @thread  Any thread.
 */
VMMR3DECL(int) DBGFR3BpSetPortIoEx(PUVM pUVM, DBGFBPOWNER hOwner, void *pvUser,
                                   RTIOPORT uPort, RTIOPORT cPorts, uint32_t fAccess,
                                   uint64_t iHitTrigger, uint64_t iHitDisable, PDBGFBP phBp)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(hOwner != NIL_DBGFBPOWNER || pvUser == NULL, VERR_INVALID_PARAMETER);
    AssertReturn(!(fAccess & ~DBGFBPIOACCESS_VALID_MASK_PORT_IO), VERR_INVALID_FLAGS);
    AssertReturn(fAccess, VERR_INVALID_FLAGS);
    AssertReturn(iHitTrigger <= iHitDisable, VERR_INVALID_PARAMETER);
    AssertPtrReturn(phBp, VERR_INVALID_POINTER);
    AssertReturn(cPorts > 0, VERR_OUT_OF_RANGE);
    AssertReturn((RTIOPORT)(uPort + cPorts) < uPort, VERR_OUT_OF_RANGE);

    int rc = dbgfR3BpEnsureInit(pUVM);
    AssertRCReturn(rc, rc);

    return VERR_NOT_IMPLEMENTED;
}


/**
 * Sets a memory mapped I/O breakpoint.
 *
 * @returns VBox status code.
 * @param   pUVM            The user mode VM handle.
 * @param   GCPhys          The first MMIO address.
 * @param   cb              The size of the MMIO range to break on.
 * @param   fAccess         The access we want to break on.
 * @param   iHitTrigger     The hit count at which the breakpoint start
 *                          triggering. Use 0 (or 1) if it's gonna trigger at
 *                          once.
 * @param   iHitDisable     The hit count which disables the breakpoint.
 *                          Use ~(uint64_t) if it's never gonna be disabled.
 * @param   phBp            Where to store the breakpoint handle.
 *
 * @thread  Any thread.
 */
VMMR3DECL(int) DBGFR3BpSetMmio(PUVM pUVM, RTGCPHYS GCPhys, uint32_t cb, uint32_t fAccess,
                               uint64_t iHitTrigger, uint64_t iHitDisable, PDBGFBP phBp)
{
    return DBGFR3BpSetMmioEx(pUVM, NIL_DBGFBPOWNER, NULL /*pvUser*/, GCPhys, cb, fAccess,
                             iHitTrigger, iHitDisable, phBp);
}


/**
 * Sets a memory mapped I/O breakpoint - extended version.
 *
 * @returns VBox status code.
 * @param   pUVM            The user mode VM handle.
 * @param   hOwner          The owner handle, use NIL_DBGFBPOWNER if no special owner attached.
 * @param   pvUser          Opaque user data to pass in the owner callback.
 * @param   GCPhys          The first MMIO address.
 * @param   cb              The size of the MMIO range to break on.
 * @param   fAccess         The access we want to break on.
 * @param   iHitTrigger     The hit count at which the breakpoint start
 *                          triggering. Use 0 (or 1) if it's gonna trigger at
 *                          once.
 * @param   iHitDisable     The hit count which disables the breakpoint.
 *                          Use ~(uint64_t) if it's never gonna be disabled.
 * @param   phBp            Where to store the breakpoint handle.
 *
 * @thread  Any thread.
 */
VMMR3DECL(int) DBGFR3BpSetMmioEx(PUVM pUVM, DBGFBPOWNER hOwner, void *pvUser,
                                 RTGCPHYS GCPhys, uint32_t cb, uint32_t fAccess,
                                 uint64_t iHitTrigger, uint64_t iHitDisable, PDBGFBP phBp)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(hOwner != NIL_DBGFBPOWNER || pvUser == NULL, VERR_INVALID_PARAMETER);
    AssertReturn(!(fAccess & ~DBGFBPIOACCESS_VALID_MASK_MMIO), VERR_INVALID_FLAGS);
    AssertReturn(fAccess, VERR_INVALID_FLAGS);
    AssertReturn(iHitTrigger <= iHitDisable, VERR_INVALID_PARAMETER);
    AssertPtrReturn(phBp, VERR_INVALID_POINTER);
    AssertReturn(cb, VERR_OUT_OF_RANGE);
    AssertReturn(GCPhys + cb < GCPhys, VERR_OUT_OF_RANGE);

    int rc = dbgfR3BpEnsureInit(pUVM);
    AssertRCReturn(rc, rc);

    return VERR_NOT_IMPLEMENTED;
}


/**
 * Clears a breakpoint.
 *
 * @returns VBox status code.
 * @param   pUVM        The user mode VM handle.
 * @param   hBp         The handle of the breakpoint which should be removed (cleared).
 *
 * @thread  Any thread.
 */
VMMR3DECL(int) DBGFR3BpClear(PUVM pUVM, DBGFBP hBp)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(hBp != NIL_DBGFBPOWNER, VERR_INVALID_HANDLE);

    PDBGFBPINT pBp = dbgfR3BpGetByHnd(pUVM, hBp);
    AssertPtrReturn(pBp, VERR_DBGF_BP_NOT_FOUND);

    /* Disarm the breakpoint when it is enabled. */
    if (DBGF_BP_PUB_IS_ENABLED(pBp->Pub.fFlagsAndType))
    {
        int rc = dbgfR3BpDisarm(pUVM, hBp, pBp);
        AssertRC(rc);
    }

    switch (DBGF_BP_PUB_GET_TYPE(pBp->Pub.fFlagsAndType))
    {
        case DBGFBPTYPE_REG:
        {
            int rc = dbgfR3BpRegRemove(pUVM->pVM, hBp, pBp);
            AssertRC(rc);
            break;
        }
        default:
            break;
    }

    dbgfR3BpFree(pUVM, hBp, pBp);
    return VINF_SUCCESS;
}


/**
 * Enables a breakpoint.
 *
 * @returns VBox status code.
 * @param   pUVM        The user mode VM handle.
 * @param   hBp         The handle of the breakpoint which should be enabled.
 *
 * @thread  Any thread.
 */
VMMR3DECL(int) DBGFR3BpEnable(PUVM pUVM, DBGFBP hBp)
{
    /*
     * Validate the input.
     */
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(hBp != NIL_DBGFBPOWNER, VERR_INVALID_HANDLE);

    PDBGFBPINT pBp = dbgfR3BpGetByHnd(pUVM, hBp);
    AssertPtrReturn(pBp, VERR_DBGF_BP_NOT_FOUND);

    int rc = VINF_SUCCESS;
    if (!DBGF_BP_PUB_IS_ENABLED(pBp->Pub.fFlagsAndType))
        rc = dbgfR3BpArm(pUVM, hBp, pBp);
    else
        rc = VINF_DBGF_BP_ALREADY_ENABLED;

    return rc;
}


/**
 * Disables a breakpoint.
 *
 * @returns VBox status code.
 * @param   pUVM        The user mode VM handle.
 * @param   hBp         The handle of the breakpoint which should be disabled.
 *
 * @thread  Any thread.
 */
VMMR3DECL(int) DBGFR3BpDisable(PUVM pUVM, DBGFBP hBp)
{
    /*
     * Validate the input.
     */
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(hBp != NIL_DBGFBPOWNER, VERR_INVALID_HANDLE);

    PDBGFBPINT pBp = dbgfR3BpGetByHnd(pUVM, hBp);
    AssertPtrReturn(pBp, VERR_DBGF_BP_NOT_FOUND);

    int rc = VINF_SUCCESS;
    if (DBGF_BP_PUB_IS_ENABLED(pBp->Pub.fFlagsAndType))
        rc = dbgfR3BpDisarm(pUVM, hBp, pBp);
    else
        rc = VINF_DBGF_BP_ALREADY_DISABLED;

    return rc;
}


/**
 * EMT worker for DBGFR3BpEnum().
 *
 * @returns VBox status code.
 * @param   pUVM        The user mode VM handle.
 * @param   pfnCallback The callback function.
 * @param   pvUser      The user argument to pass to the callback.
 *
 * @thread  EMT
 * @internal
 */
static DECLCALLBACK(int) dbgfR3BpEnum(PUVM pUVM, PFNDBGFBPENUM pfnCallback, void *pvUser)
{
    /*
     * Validate input.
     */
    PVM pVM = pUVM->pVM;
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertPtrReturn(pfnCallback, VERR_INVALID_POINTER);

    RT_NOREF(pvUser);

    return VERR_NOT_IMPLEMENTED;
}


/**
 * Enumerate the breakpoints.
 *
 * @returns VBox status code.
 * @param   pUVM        The user mode VM handle.
 * @param   pfnCallback The callback function.
 * @param   pvUser      The user argument to pass to the callback.
 *
 * @thread  Any thread but the callback will be called from EMT.
 */
VMMR3DECL(int) DBGFR3BpEnum(PUVM pUVM, PFNDBGFBPENUM pfnCallback, void *pvUser)
{
    /*
     * This must be done on EMT.
     */
    int rc = VMR3ReqPriorityCallWaitU(pUVM, 0 /*idDstCpu*/, (PFNRT)dbgfR3BpEnum, 3, pUVM, pfnCallback, pvUser);
    LogFlow(("DBGFR3BpEnum: returns %Rrc\n", rc));
    return rc;
}

