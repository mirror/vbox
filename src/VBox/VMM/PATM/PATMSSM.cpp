/* $Id$ */
/** @file
 * PATMSSM - Dynamic Guest OS Patching Manager; Save and load state
 *
 * NOTE: CSAM assumes patch memory is never reused!!
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PATM
#include <VBox/patm.h>
#include <VBox/stam.h>
#include <VBox/pgm.h>
#include <VBox/cpum.h>
#include <VBox/iom.h>
#include <VBox/sup.h>
#include <VBox/mm.h>
#include <VBox/ssm.h>
#include <VBox/pdm.h>
#include <VBox/trpm.h>
#include <VBox/param.h>
#include <iprt/avl.h>
#include "PATMInternal.h"
#include "PATMPatch.h"
#include "PATMA.h"
#include <VBox/vm.h>
#include <VBox/csam.h>

#include <VBox/dbg.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <VBox/dis.h>
#include <VBox/disopcode.h>

#define PATM_SUBTRACT_PTR(a, b) *(uintptr_t *)&(a) = (uintptr_t)(a) - (uintptr_t)(b)
#define PATM_ADD_PTR(a, b)      *(uintptr_t *)&(a) = (uintptr_t)(a) + (uintptr_t)(b)

#ifdef VBOX_STRICT
/**
 * Callback function for RTAvlPVDoWithAll
 *
 * Counts the number of patches in the tree
 *
 * @returns VBox status code.
 * @param   pNode           Current node
 * @param   pcPatches       Pointer to patch counter (uint32_t)
 */
static DECLCALLBACK(int) patmCountLeafPV(PAVLPVNODECORE pNode, void *pcPatches)
{
    *(uint32_t *)pcPatches = *(uint32_t *)pcPatches + 1;
    return VINF_SUCCESS;
}

/**
 * Callback function for RTAvlU32DoWithAll
 *
 * Counts the number of patches in the tree
 *
 * @returns VBox status code.
 * @param   pNode           Current node
 * @param   pcPatches       Pointer to patch counter (uint32_t)
 */
static DECLCALLBACK(int) patmCountLeaf(PAVLU32NODECORE pNode, void *pcPatches)
{
    *(uint32_t *)pcPatches = *(uint32_t *)pcPatches + 1;
    return VINF_SUCCESS;
}
#endif /* VBOX_STRICT */

/**
 * Callback function for RTAvloGCPtrDoWithAll
 *
 * Counts the number of patches in the tree
 *
 * @returns VBox status code.
 * @param   pNode           Current node
 * @param   pcPatches       Pointer to patch counter
 */
static DECLCALLBACK(int) patmCountPatch(PAVLOGCPTRNODECORE pNode, void *pcPatches)
{
    *(uint32_t *)pcPatches = *(uint32_t *)pcPatches + 1;
    return VINF_SUCCESS;
}

/**
 * Callback function for RTAvlU32DoWithAll
 *
 * Saves all patch to guest lookup records.
 *
 * @returns VBox status code.
 * @param   pNode           Current node
 * @param   pVM1            VM Handle
 */
static DECLCALLBACK(int) patmSaveP2GLookupRecords(PAVLU32NODECORE pNode, void *pVM1)
{
    PVM                 pVM    = (PVM)pVM1;
    PSSMHANDLE          pSSM   = pVM->patm.s.savedstate.pSSM;
    PRECPATCHTOGUEST    pPatchToGuestRec = (PRECPATCHTOGUEST)pNode;

    /* Save the lookup record. */
    int rc = SSMR3PutMem(pSSM, pPatchToGuestRec, sizeof(RECPATCHTOGUEST));
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}

/**
 * Callback function for RTAvlPVDoWithAll
 *
 * Saves all patch to guest lookup records.
 *
 * @returns VBox status code.
 * @param   pNode           Current node
 * @param   pVM1            VM Handle
 */
static DECLCALLBACK(int) patmSaveFixupRecords(PAVLPVNODECORE pNode, void *pVM1)
{
    PVM                 pVM  = (PVM)pVM1;
    PSSMHANDLE          pSSM = pVM->patm.s.savedstate.pSSM;
    RELOCREC            rec  = *(PRELOCREC)pNode;

    Assert(rec.pRelocPos);
    PATM_SUBTRACT_PTR(rec.pRelocPos, pVM->patm.s.pPatchMemHC);

    /* Save the lookup record. */
    int rc = SSMR3PutMem(pSSM, &rec, sizeof(rec));
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}


/**
 * Callback function for RTAvloGCPtrDoWithAll
 *
 * Saves the state of the patch that's being enumerated
 *
 * @returns VBox status code.
 * @param   pNode           Current node
 * @param   pVM1            VM Handle
 */
static DECLCALLBACK(int) patmSavePatchState(PAVLOGCPTRNODECORE pNode, void *pVM1)
{
    PVM           pVM    = (PVM)pVM1;
    PPATMPATCHREC pPatch = (PPATMPATCHREC)pNode;
    PATMPATCHREC  patch  = *pPatch;
    PSSMHANDLE    pSSM   = pVM->patm.s.savedstate.pSSM;
    int           rc;

    Assert(!(pPatch->patch.flags & PATMFL_GLOBAL_FUNCTIONS));

    /*
     * Reset HC pointers that need to be recalculated when loading the state
     */
    AssertMsg(patch.patch.uState == PATCH_REFUSED || (patch.patch.pPatchBlockOffset || (patch.patch.flags & (PATMFL_SYSENTER_XP|PATMFL_INT3_REPLACEMENT))),
              ("State = %x pPrivInstrHC=%08x pPatchBlockHC=%08x flags=%x\n", patch.patch.uState, patch.patch.pPrivInstrHC, PATCHCODE_PTR_HC(&patch.patch), patch.patch.flags));
    Assert(pPatch->patch.JumpTree == 0);
    Assert(!pPatch->patch.pTempInfo || pPatch->patch.pTempInfo->DisasmJumpTree == 0);
    Assert(!pPatch->patch.pTempInfo || pPatch->patch.pTempInfo->IllegalInstrTree == 0);

    memset(&patch.patch.cacheRec, 0, sizeof(patch.patch.cacheRec));

    /* Save the patch record itself */
    rc = SSMR3PutMem(pSSM, &patch, sizeof(patch));
    AssertRCReturn(rc, rc);

    /*
     * Reset HC pointers in fixup records and save them.
     */
#ifdef VBOX_STRICT
    uint32_t nrFixupRecs = 0;
    RTAvlPVDoWithAll(&pPatch->patch.FixupTree, true, patmCountLeafPV, &nrFixupRecs);
    AssertMsg((int32_t)nrFixupRecs == pPatch->patch.nrFixups, ("Fixup inconsistency! counted %d vs %d\n", nrFixupRecs, pPatch->patch.nrFixups));
#endif
    RTAvlPVDoWithAll(&pPatch->patch.FixupTree, true, patmSaveFixupRecords, pVM);

#ifdef VBOX_STRICT
    uint32_t nrLookupRecords = 0;
    RTAvlU32DoWithAll(&pPatch->patch.Patch2GuestAddrTree, true, patmCountLeaf, &nrLookupRecords);
    Assert(nrLookupRecords == pPatch->patch.nrPatch2GuestRecs);
#endif

    RTAvlU32DoWithAll(&pPatch->patch.Patch2GuestAddrTree, true, patmSaveP2GLookupRecords, pVM);
    return VINF_SUCCESS;
}

/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 */
DECLCALLBACK(int) patmr3Save(PVM pVM, PSSMHANDLE pSSM)
{
    PATM patmInfo = pVM->patm.s;
    int  rc;

    pVM->patm.s.savedstate.pSSM = pSSM;

    /*
     * Reset HC pointers that need to be recalculated when loading the state
     */
    patmInfo.pPatchMemHC = NULL;
    patmInfo.pGCStateHC  = 0;
    patmInfo.pvFaultMonitor = 0;

    Assert(patmInfo.ulCallDepth == 0);

    /*
     * Count the number of patches in the tree (feeling lazy)
     */
    patmInfo.savedstate.cPatches = 0;
    RTAvloGCPtrDoWithAll(&pVM->patm.s.PatchLookupTreeHC->PatchTree, true, patmCountPatch, &patmInfo.savedstate.cPatches);

    /*
     * Save PATM structure
     */
    rc = SSMR3PutMem(pSSM, &patmInfo, sizeof(patmInfo));
    AssertRCReturn(rc, rc);

    /*
     * Save patch memory contents
     */
    rc = SSMR3PutMem(pSSM, pVM->patm.s.pPatchMemHC, pVM->patm.s.cbPatchMem);
    AssertRCReturn(rc, rc);

    /*
     * Save GC state memory
     */
    rc = SSMR3PutMem(pSSM, pVM->patm.s.pGCStateHC, sizeof(PATMGCSTATE));
    AssertRCReturn(rc, rc);

    /*
     * Save PATM stack page
     */
    rc = SSMR3PutMem(pSSM, pVM->patm.s.pGCStackHC, PATM_STACK_TOTAL_SIZE);
    AssertRCReturn(rc, rc);

    /*
     * Save all patches
     */
    rc = RTAvloGCPtrDoWithAll(&pVM->patm.s.PatchLookupTreeHC->PatchTree, true, patmSavePatchState, pVM);
    AssertRCReturn(rc, rc);

    /** @note patch statistics are not saved. */

    return VINF_SUCCESS;
}

/**
 * Execute state load operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 * @param   u32Version      Data layout version.
 */
DECLCALLBACK(int) patmr3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version)
{
    PATM patmInfo;
    int  rc;

    if (u32Version != PATM_SSM_VERSION)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    pVM->patm.s.savedstate.pSSM = pSSM;

    /*
     * Restore PATM structure
     */
    rc = SSMR3GetMem(pSSM, &patmInfo, sizeof(patmInfo));
    AssertRCReturn(rc, rc);

    /** @todo this restriction could be removed as we relocate when loading the saved state,.. */
    if (    pVM->patm.s.pGCStateGC != patmInfo.pGCStateGC
        ||  pVM->patm.s.pCPUMCtxGC != patmInfo.pCPUMCtxGC
        ||  pVM->patm.s.pStatsGC   != patmInfo.pStatsGC)
    {
        if (SSMR3HandleGetAfter(pSSM) == SSMAFTER_DEBUG_IT) /* hack for x86 / amd64 mix. */
            return VINF_SUCCESS;
        AssertMsgFailed(("GC state, stat or cpum ptrs don't match!!!\n"));
        return VERR_SSM_INVALID_STATE;
    }

    /* Relative calls are made to the helper functions. Therefor their location must not change! */
    if (    pVM->patm.s.pfnHelperCallGC != patmInfo.pfnHelperCallGC
        ||  pVM->patm.s.pfnHelperRetGC  != patmInfo.pfnHelperRetGC
        ||  pVM->patm.s.pfnHelperJumpGC != patmInfo.pfnHelperJumpGC
        ||  pVM->patm.s.pfnHelperIretGC != patmInfo.pfnHelperIretGC)
    {
        AssertMsgFailed(("Helper function ptrs don't match!!!\n"));
        return VERR_SSM_INVALID_STATE;
    }

    if (    pVM->patm.s.pPatchMemGC != patmInfo.pPatchMemGC
        ||  pVM->patm.s.cbPatchMem != patmInfo.cbPatchMem)
    {
        AssertMsgFailed(("Patch memory ptrs and/or sizes don't match!!!\n"));
        return VERR_SSM_INVALID_STATE;
    }
    pVM->patm.s.offPatchMem         = patmInfo.offPatchMem;
    pVM->patm.s.deltaReloc          = patmInfo.deltaReloc;
    pVM->patm.s.uCurrentPatchIdx    = patmInfo.uCurrentPatchIdx;

    /* Lowest and highest patched instruction */
    pVM->patm.s.pPatchedInstrGCLowest    = patmInfo.pPatchedInstrGCLowest;
    pVM->patm.s.pPatchedInstrGCHighest   = patmInfo.pPatchedInstrGCHighest;

    /* Sysenter handlers */
    pVM->patm.s.pfnSysEnterGC            = patmInfo.pfnSysEnterGC;
    pVM->patm.s.pfnSysEnterPatchGC       = patmInfo.pfnSysEnterPatchGC;
    pVM->patm.s.uSysEnterPatchIdx        = patmInfo.uSysEnterPatchIdx;

    Assert(patmInfo.ulCallDepth == 0 && pVM->patm.s.ulCallDepth == 0);

    /** @note patch statistics are not restored. */

    /*
     * Restore patch memory contents
     */
    rc = SSMR3GetMem(pSSM, pVM->patm.s.pPatchMemHC, pVM->patm.s.cbPatchMem);
    AssertRCReturn(rc, rc);

    /*
     * Restore GC state memory
     */
    if (pVM->patm.s.pGCStateGC != patmInfo.pGCStateGC)
    {
        AssertMsgFailed(("GC patch state ptrs don't match!!!\n"));
        return VERR_SSM_INVALID_STATE;
    }
    rc = SSMR3GetMem(pSSM, pVM->patm.s.pGCStateHC, sizeof(PATMGCSTATE));
    AssertRCReturn(rc, rc);

    /*
     * Restore PATM stack page
     */
    if (pVM->patm.s.pGCStackGC != patmInfo.pGCStackGC)
    {
        AssertMsgFailed(("GC patch stack ptrs don't match!!!\n"));
        return VERR_SSM_INVALID_STATE;
    }
    rc = SSMR3GetMem(pSSM, pVM->patm.s.pGCStackHC, PATM_STACK_TOTAL_SIZE);
    AssertRCReturn(rc, rc);

    /*
     * Load all patches
     */
    for (uint32_t i=0;i<patmInfo.savedstate.cPatches;i++)
    {
        PATMPATCHREC patch, *pPatchRec;

        rc = SSMR3GetMem(pSSM, &patch, sizeof(patch));
        AssertRCReturn(rc, rc);

        Assert(!(patch.patch.flags & PATMFL_GLOBAL_FUNCTIONS));

        rc = MMHyperAlloc(pVM, sizeof(PATMPATCHREC), 0, MM_TAG_PATM_PATCH, (void **)&pPatchRec);
        if (VBOX_FAILURE(rc))
        {
            AssertMsgFailed(("Out of memory!!!!\n"));
            return VERR_NO_MEMORY;
        }
        /*
         * Only restore the patch part of the tree record; not the internal data (except the key of course)
         */
        pPatchRec->patch             = patch.patch;
        pPatchRec->Core.Key          = patch.Core.Key;
        pPatchRec->CoreOffset.Key    = patch.CoreOffset.Key;

        bool ret = RTAvloGCPtrInsert(&pVM->patm.s.PatchLookupTreeHC->PatchTree, &pPatchRec->Core);
        Assert(ret);
        if (pPatchRec->patch.uState != PATCH_REFUSED)
        {
            if (pPatchRec->patch.pPatchBlockOffset)
            {
                /* We actually generated code for this patch. */
                ret = RTAvloGCPtrInsert(&pVM->patm.s.PatchLookupTreeHC->PatchTreeByPatchAddr, &pPatchRec->CoreOffset);
                AssertMsg(ret, ("Inserting patch %VGv offset %VGv failed!!\n", pPatchRec->patch.pPrivInstrGC, pPatchRec->CoreOffset.Key));
            }
        }
        /* Set to zero as we don't need it anymore. */
        pPatchRec->patch.pTempInfo = 0;

        pPatchRec->patch.pPrivInstrHC   = 0;
        /* The GC virtual ptr is fixed, but we must convert it manually again to HC. */
        rc = PGMPhysGCPtr2HCPtr(pVM, pPatchRec->patch.pPrivInstrGC, (PRTHCPTR)&pPatchRec->patch.pPrivInstrHC);
        /* Can fail due to page or page table not present. */

        /*
         * Restore fixup records and correct HC pointers in fixup records
         */
        pPatchRec->patch.FixupTree = 0;
        pPatchRec->patch.nrFixups  = 0;    /* increased by patmPatchAddReloc32 */
        for (int i=0;i<patch.patch.nrFixups;i++)
        {
            RELOCREC rec;

            rc = SSMR3GetMem(pSSM, &rec, sizeof(rec));
            AssertRCReturn(rc, rc);
            PATM_ADD_PTR(rec.pRelocPos, pVM->patm.s.pPatchMemHC);

            rc = patmPatchAddReloc32(pVM, &pPatchRec->patch, rec.pRelocPos, rec.uType, rec.pSource, rec.pDest);
            AssertRCReturn(rc, rc);
        }

        /* And all patch to guest lookup records */
        Assert(pPatchRec->patch.nrPatch2GuestRecs || pPatchRec->patch.uState == PATCH_REFUSED || (pPatchRec->patch.flags & (PATMFL_SYSENTER_XP | PATMFL_IDTHANDLER | PATMFL_TRAPHANDLER | PATMFL_INT3_REPLACEMENT)));

        pPatchRec->patch.Patch2GuestAddrTree = 0;
        pPatchRec->patch.Guest2PatchAddrTree = 0;
        if (pPatchRec->patch.nrPatch2GuestRecs)
        {
            RECPATCHTOGUEST rec;
            uint32_t        nrPatch2GuestRecs = pPatchRec->patch.nrPatch2GuestRecs;

            pPatchRec->patch.nrPatch2GuestRecs = 0;    /* incremented by patmr3AddP2GLookupRecord */
            for (uint32_t i=0;i<nrPatch2GuestRecs;i++)
            {
                rc = SSMR3GetMem(pSSM, &rec, sizeof(rec));
                AssertRCReturn(rc, rc);

                patmr3AddP2GLookupRecord(pVM, &pPatchRec->patch, (uintptr_t)rec.Core.Key + pVM->patm.s.pPatchMemHC, rec.pOrgInstrGC, rec.enmType, rec.fDirty);
            }
            Assert(pPatchRec->patch.Patch2GuestAddrTree);
        }

        if (pPatchRec->patch.flags & PATMFL_CODE_MONITORED)
        {
            /* Insert the guest page lookup records (for detection self-modifying code) */
            rc = patmInsertPatchPages(pVM, &pPatchRec->patch);
            AssertRCReturn(rc, rc);
        }
    }

#ifdef VBOX_WITH_STATISTICS
    /*
     * Restore relevant old statistics
     */
    pVM->patm.s.StatDisabled  = patmInfo.StatDisabled;
    pVM->patm.s.StatUnusable  = patmInfo.StatUnusable;
    pVM->patm.s.StatEnabled   = patmInfo.StatEnabled;
    pVM->patm.s.StatInstalled = patmInfo.StatInstalled;
#endif
    return VINF_SUCCESS;
}


