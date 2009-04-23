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
#include <VBox/hwaccm.h>
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

static void patmCorrectFixup(PVM pVM, unsigned ulSSMVersion, PATM &patmInfo, PPATCHINFO pPatch, PRELOCREC pRec, int32_t offset, RTRCPTR *pFixup);

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
 * Callback function for RTAvloU32DoWithAll
 *
 * Counts the number of patches in the tree
 *
 * @returns VBox status code.
 * @param   pNode           Current node
 * @param   pcPatches       Pointer to patch counter
 */
static DECLCALLBACK(int) patmCountPatch(PAVLOU32NODECORE pNode, void *pcPatches)
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
    RTRCPTR            *pFixup = (RTRCPTR *)rec.pRelocPos;

    Assert(rec.pRelocPos);
    /* Convert pointer to an offset into patch memory. */
    PATM_SUBTRACT_PTR(rec.pRelocPos, pVM->patm.s.pPatchMemHC);

    if (rec.uType == FIXUP_ABSOLUTE)
    {
        /* Core.Key abused to store the fixup type. */
        if (*pFixup == pVM->pVMRC + RT_OFFSETOF(VM, aCpus[0].fLocalForcedActions))
            rec.Core.Key = (AVLPVKEY)PATM_FIXUP_CPU_FF_ACTION;
        else 
        if (*pFixup == CPUMR3GetGuestCpuIdDefRCPtr(pVM))
            rec.Core.Key = (AVLPVKEY)PATM_FIXUP_CPUID_DEFAULT;
        else 
        if (*pFixup == CPUMR3GetGuestCpuIdStdRCPtr(pVM))
            rec.Core.Key = (AVLPVKEY)PATM_FIXUP_CPUID_STANDARD;
        else 
        if (*pFixup == CPUMR3GetGuestCpuIdExtRCPtr(pVM))
            rec.Core.Key = (AVLPVKEY)PATM_FIXUP_CPUID_EXTENDED;
        else 
        if (*pFixup == CPUMR3GetGuestCpuIdCentaurRCPtr(pVM))
            rec.Core.Key = (AVLPVKEY)PATM_FIXUP_CPUID_CENTAUR;
    }

    /* Save the lookup record. */
    int rc = SSMR3PutMem(pSSM, &rec, sizeof(rec));
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}


/**
 * Callback function for RTAvloU32DoWithAll
 *
 * Saves the state of the patch that's being enumerated
 *
 * @returns VBox status code.
 * @param   pNode           Current node
 * @param   pVM1            VM Handle
 */
static DECLCALLBACK(int) patmSavePatchState(PAVLOU32NODECORE pNode, void *pVM1)
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
    RTAvloU32DoWithAll(&pVM->patm.s.PatchLookupTreeHC->PatchTree, true, patmCountPatch, &patmInfo.savedstate.cPatches);

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
    rc = RTAvloU32DoWithAll(&pVM->patm.s.PatchLookupTreeHC->PatchTree, true, patmSavePatchState, pVM);
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

    if (    u32Version != PATM_SSM_VERSION
        &&  u32Version != PATM_SSM_VERSION_FIXUP_HACK
        &&  u32Version != PATM_SSM_VERSION_VER16
#ifdef PATM_WITH_NEW_SSM
        &&  u32Version != PATM_SSM_VERSION_GETPUTMEM)
#else
       )
#endif
    {
        AssertMsgFailed(("patmR3Load: Invalid version u32Version=%d!\n", u32Version));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    pVM->patm.s.savedstate.pSSM = pSSM;

    /*
     * Restore PATM structure
     */
#ifdef PATM_WITH_NEW_SSM
    if (u32Version == PATM_SSM_VERSION_GETPUTMEM)
    {
#endif
        rc = SSMR3GetMem(pSSM, &patmInfo, sizeof(patmInfo));
        AssertRCReturn(rc, rc);
#ifdef PATM_WITH_NEW_SSM
    }
    else
    {
        memset(&patmInfo, 0, sizeof(patmInfo));

        AssertCompile(sizeof(patmInfo.pGCStateGC) == sizeof(RTRCPTR));
        rc = SSMR3GetRCPtr(pSSM, &patmInfo.pGCStateGC);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(patmInfo.pCPUMCtxGC) == sizeof(RTRCPTR));
        rc = SSMR3GetRCPtr(pSSM, &patmInfo.pCPUMCtxGC);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(patmInfo.pStatsGC) == sizeof(RTRCPTR));
        rc = SSMR3GetRCPtr(pSSM, &patmInfo.pStatsGC);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(patmInfo.pfnHelperCallGC) == sizeof(RTRCPTR));
        rc = SSMR3GetRCPtr(pSSM, &patmInfo.pfnHelperCallGC);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(patmInfo.pfnHelperRetGC) == sizeof(RTRCPTR));
        rc = SSMR3GetRCPtr(pSSM, &patmInfo.pfnHelperRetGC);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(patmInfo.pfnHelperJumpGC) == sizeof(RTRCPTR));
        rc = SSMR3GetRCPtr(pSSM, &patmInfo.pfnHelperJumpGC);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(patmInfo.pfnHelperIretGC) == sizeof(RTRCPTR));
        rc = SSMR3GetRCPtr(pSSM, &patmInfo.pfnHelperIretGC);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(patmInfo.pPatchMemGC) == sizeof(RTRCPTR));
        rc = SSMR3GetRCPtr(pSSM, &patmInfo.pPatchMemGC);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(patmInfo.cbPatchMem) == sizeof(uint32_t));
        rc = SSMR3GetU32(pSSM, &patmInfo.cbPatchMem);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(patmInfo.offPatchMem) == sizeof(uint32_t));
        rc = SSMR3GetU32(pSSM, &patmInfo.offPatchMem);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(patmInfo.deltaReloc) == sizeof(int32_t));
        rc = SSMR3GetS32(pSSM, &patmInfo.deltaReloc);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(patmInfo.uCurrentPatchIdx) == sizeof(uint32_t));
        rc = SSMR3GetS32(pSSM, &patmInfo.uCurrentPatchIdx);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(patmInfo.pPatchedInstrGCLowest) == sizeof(RTRCPTR));
        rc = SSMR3GetRCPtr(pSSM, &patmInfo.pPatchedInstrGCLowest);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(patmInfo.pPatchedInstrGCHighest) == sizeof(RTRCPTR));
        rc = SSMR3GetRCPtr(pSSM, &patmInfo.pPatchedInstrGCHighest);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(patmInfo.pfnSysEnterGC) == sizeof(RTRCPTR));
        rc = SSMR3GetRCPtr(pSSM, &patmInfo.pfnSysEnterGC);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(patmInfo.pfnSysEnterPatchGC) == sizeof(RTRCPTR));
        rc = SSMR3GetRCPtr(pSSM, &patmInfo.pfnSysEnterPatchGC);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(patmInfo.uSysEnterPatchIdx) == sizeof(uint32_t));
        rc = SSMR3GetU32(pSSM, &patmInfo.uSysEnterPatchIdx);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(patmInfo.ulCallDepth) == sizeof(uint32_t));
        rc = SSMR3GetU32(pSSM, &patmInfo.ulCallDepth);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(patmInfo.pGCStackGC) == sizeof(RTRCPTR));
        rc = SSMR3GetRCPtr(pSSM, &patmInfo.pGCStackGC);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(patmInfo.cPageRecords) == sizeof(uint32_t));
        rc = SSMR3GetU32(pSSM, &patmInfo.cPageRecords);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(patmInfo.fOutOfMemory) == sizeof(bool));
        rc = SSMR3GetBool(pSSM, &patmInfo.fOutOfMemory);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(patmInfo.savedstate.cPatches) == sizeof(uint32_t));
        rc = SSMR3GetU32(pSSM, &patmInfo.savedstate.cPatches);
        AssertRCReturn(rc, rc);

    }
#endif

    /* Relative calls are made to the helper functions. Therefor their relative location must not change! */
    /* Note: we reuse the saved global helpers and assume they are identical, which is kind of dangerous. */
    if (    (pVM->patm.s.pfnHelperCallGC - pVM->patm.s.pPatchMemGC) != (patmInfo.pfnHelperCallGC  - patmInfo.pPatchMemGC)
        ||  (pVM->patm.s.pfnHelperRetGC  - pVM->patm.s.pPatchMemGC) != (patmInfo.pfnHelperRetGC   - patmInfo.pPatchMemGC)
        ||  (pVM->patm.s.pfnHelperJumpGC - pVM->patm.s.pPatchMemGC) != (patmInfo.pfnHelperJumpGC  - patmInfo.pPatchMemGC)
        ||  (pVM->patm.s.pfnHelperIretGC - pVM->patm.s.pPatchMemGC) != (patmInfo.pfnHelperIretGC  - patmInfo.pPatchMemGC))
    {
        AssertMsgFailed(("Helper function ptrs don't match!!!\n"));
        return VERR_SSM_INVALID_STATE;
    }

    if (pVM->patm.s.cbPatchMem != patmInfo.cbPatchMem)
    {
        AssertMsgFailed(("Patch memory ptrs and/or sizes don't match!!!\n"));
        return VERR_SSM_INVALID_STATE;
    }
    pVM->patm.s.offPatchMem         = patmInfo.offPatchMem;
    pVM->patm.s.deltaReloc          = patmInfo.deltaReloc;
    pVM->patm.s.uCurrentPatchIdx    = patmInfo.uCurrentPatchIdx;
    pVM->patm.s.fOutOfMemory        = patmInfo.fOutOfMemory;

    /* Lowest and highest patched instruction */
    pVM->patm.s.pPatchedInstrGCLowest    = patmInfo.pPatchedInstrGCLowest;
    pVM->patm.s.pPatchedInstrGCHighest   = patmInfo.pPatchedInstrGCHighest;

    /* Sysenter handlers */
    pVM->patm.s.pfnSysEnterGC            = patmInfo.pfnSysEnterGC;
    pVM->patm.s.pfnSysEnterPatchGC       = patmInfo.pfnSysEnterPatchGC;
    pVM->patm.s.uSysEnterPatchIdx        = patmInfo.uSysEnterPatchIdx;

    Assert(patmInfo.ulCallDepth == 0 && pVM->patm.s.ulCallDepth == 0);

    Log(("pPatchMemGC %RRv vs old %RRv\n", pVM->patm.s.pPatchMemGC, patmInfo.pPatchMemGC));
    Log(("pGCStateGC  %RRv vs old %RRv\n", pVM->patm.s.pGCStateGC, patmInfo.pGCStateGC));
    Log(("pGCStackGC  %RRv vs old %RRv\n", pVM->patm.s.pGCStackGC, patmInfo.pGCStackGC));
    Log(("pCPUMCtxGC  %RRv vs old %RRv\n", pVM->patm.s.pCPUMCtxGC, patmInfo.pCPUMCtxGC));


    /** @note patch statistics are not restored. */

    /*
     * Restore patch memory contents
     */
    Log(("Restore patch memory: new %RRv old %RRv\n", pVM->patm.s.pPatchMemGC, patmInfo.pPatchMemGC));
    rc = SSMR3GetMem(pSSM, pVM->patm.s.pPatchMemHC, pVM->patm.s.cbPatchMem);
    AssertRCReturn(rc, rc);

    /*
     * Restore GC state memory
     */
#ifdef PATM_WITH_NEW_SSM
    if (u32Version == PATM_SSM_VERSION_GETPUTMEM)
    {
#endif
        rc = SSMR3GetMem(pSSM, pVM->patm.s.pGCStateHC, sizeof(PATMGCSTATE));
        AssertRCReturn(rc, rc);
#ifdef PATM_WITH_NEW_SSM
    }
    else
    {
        AssertCompile(sizeof(pVM->patm.s.pGCStateHC->uVMFlags) == sizeof(uint32_t));
        rc = SSMR3GetU32(pSSM, &pVM->patm.s.pGCStateHC->uVMFlags);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(pVM->patm.s.pGCStateHC->uPendingAction) == sizeof(uint32_t));
        rc = SSMR3GetU32(pSSM, &pVM->patm.s.pGCStateHC->uPendingAction);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(pVM->patm.s.pGCStateHC->uPatchCalls) == sizeof(uint32_t));
        rc = SSMR3GetU32(pSSM, &pVM->patm.s.pGCStateHC->uPatchCalls);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(pVM->patm.s.pGCStateHC->uScratch) == sizeof(uint32_t));
        rc = SSMR3GetU32(pSSM, &pVM->patm.s.pGCStateHC->uScratch);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(pVM->patm.s.pGCStateHC->uIretEFlags) == sizeof(uint32_t));
        rc = SSMR3GetU32(pSSM, &pVM->patm.s.pGCStateHC->uIretEFlags);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(pVM->patm.s.pGCStateHC->uIretCS) == sizeof(uint32_t));
        rc = SSMR3GetU32(pSSM, &pVM->patm.s.pGCStateHC->uIretCS);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(pVM->patm.s.pGCStateHC->uIretEIP) == sizeof(uint32_t));
        rc = SSMR3GetU32(pSSM, &pVM->patm.s.pGCStateHC->uIretEIP);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(pVM->patm.s.pGCStateHC->Psp) == sizeof(uint32_t));
        rc = SSMR3GetU32(pSSM, &pVM->patm.s.pGCStateHC->Psp);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(pVM->patm.s.pGCStateHC->fPIF) == sizeof(uint32_t));
        rc = SSMR3GetU32(pSSM, &pVM->patm.s.pGCStateHC->fPIF);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(pVM->patm.s.pGCStateHC->GCPtrInhibitInterrupts) == sizeof(RTRCPTR));
        rc = SSMR3GetRCPtr(pSSM, &pVM->patm.s.pGCStateHC->GCPtrInhibitInterrupts);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(pVM->patm.s.pGCStateHC->GCCallPatchTargetAddr) == sizeof(RTRCPTR));
        rc = SSMR3GetRCPtr(pSSM, &pVM->patm.s.pGCStateHC->GCCallPatchTargetAddr);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(pVM->patm.s.pGCStateHC->GCCallReturnAddr) == sizeof(RTRCPTR));
        rc = SSMR3GetRCPtr(pSSM, &pVM->patm.s.pGCStateHC->GCCallReturnAddr);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(pVM->patm.s.pGCStateHC->Restore.uEAX) == sizeof(uint32_t));
        rc = SSMR3GetU32(pSSM, &pVM->patm.s.pGCStateHC->Restore.uEAX);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(pVM->patm.s.pGCStateHC->Restore.uECX) == sizeof(uint32_t));
        rc = SSMR3GetU32(pSSM, &pVM->patm.s.pGCStateHC->Restore.uECX);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(pVM->patm.s.pGCStateHC->Restore.uEDI) == sizeof(uint32_t));
        rc = SSMR3GetU32(pSSM, &pVM->patm.s.pGCStateHC->Restore.uEDI);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(pVM->patm.s.pGCStateHC->Restore.eFlags) == sizeof(uint32_t));
        rc = SSMR3GetU32(pSSM, &pVM->patm.s.pGCStateHC->Restore.eFlags);
        AssertRCReturn(rc, rc);

        AssertCompile(sizeof(pVM->patm.s.pGCStateHC->Restore.uFlags) == sizeof(uint32_t));
        rc = SSMR3GetU32(pSSM, &pVM->patm.s.pGCStateHC->Restore.uFlags);
        AssertRCReturn(rc, rc);
    }
#endif

    /*
     * Restore PATM stack page
     */
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
        if (RT_FAILURE(rc))
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

        Log(("Restoring patch %RRv -> %RRv\n", pPatchRec->patch.pPrivInstrGC, patmInfo.pPatchMemGC + pPatchRec->patch.pPatchBlockOffset));
        bool ret = RTAvloU32Insert(&pVM->patm.s.PatchLookupTreeHC->PatchTree, &pPatchRec->Core);
        Assert(ret);
        if (pPatchRec->patch.uState != PATCH_REFUSED)
        {
            if (pPatchRec->patch.pPatchBlockOffset)
            {
                /* We actually generated code for this patch. */
                ret = RTAvloU32Insert(&pVM->patm.s.PatchLookupTreeHC->PatchTreeByPatchAddr, &pPatchRec->CoreOffset);
                AssertMsg(ret, ("Inserting patch %RRv offset %08RX32 failed!!\n", pPatchRec->patch.pPrivInstrGC, pPatchRec->CoreOffset.Key));
            }
        }
        /* Set to zero as we don't need it anymore. */
        pPatchRec->patch.pTempInfo = 0;

        pPatchRec->patch.pPrivInstrHC   = 0;
        /* The GC virtual ptr is fixed, but we must convert it manually again to HC. */
        int rc2 = rc = PGMPhysGCPtr2R3Ptr(VMMGetCpu0(pVM), pPatchRec->patch.pPrivInstrGC, (PRTR3PTR)&pPatchRec->patch.pPrivInstrHC);
        /* Can fail due to page or page table not present. */

        /*
         * Restore fixup records and correct HC pointers in fixup records
         */
        pPatchRec->patch.FixupTree = 0;
        pPatchRec->patch.nrFixups  = 0;    /* increased by patmPatchAddReloc32 */
        for (int i=0;i<patch.patch.nrFixups;i++)
        {
            RELOCREC rec;
            int32_t offset;
            RTRCPTR *pFixup;

            rc = SSMR3GetMem(pSSM, &rec, sizeof(rec));
            AssertRCReturn(rc, rc);

            /* rec.pRelocPos now contains the relative position inside the hypervisor area. */
            offset = (int32_t)(int64_t)rec.pRelocPos;
            /* Convert to HC pointer again. */
            PATM_ADD_PTR(rec.pRelocPos, pVM->patm.s.pPatchMemHC);
            pFixup = (RTRCPTR *)rec.pRelocPos;

            if (pPatchRec->patch.uState != PATCH_REFUSED)
            {
                if (    rec.uType == FIXUP_REL_JMPTOPATCH
                    &&  (pPatchRec->patch.flags & PATMFL_PATCHED_GUEST_CODE))
                {
                    Assert(pPatchRec->patch.cbPatchJump == SIZEOF_NEARJUMP32 || pPatchRec->patch.cbPatchJump == SIZEOF_NEAR_COND_JUMP32);
                    unsigned offset = (pPatchRec->patch.cbPatchJump == SIZEOF_NEARJUMP32) ? 1 : 2;

                    /** @todo This will fail & crash in patmCorrectFixup if the page isn't present
                     *        when we restore. Happens with my XP image here
                     *        (pPrivInstrGC=0x8069e051). */
                    AssertLogRelMsg(pPatchRec->patch.pPrivInstrHC, ("%RRv rc=%Rrc uState=%u\n", pPatchRec->patch.pPrivInstrGC, rc2, pPatchRec->patch.uState));
                    rec.pRelocPos = pPatchRec->patch.pPrivInstrHC + offset;
                    pFixup        = (RTRCPTR *)rec.pRelocPos;
                }

                patmCorrectFixup(pVM, u32Version, patmInfo, &pPatchRec->patch, &rec, offset, pFixup);
            }

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

#if 0 /* can fail def LOG_ENABLED */
        if (    pPatchRec->patch.uState != PATCH_REFUSED
            &&  !(pPatchRec->patch.flags & PATMFL_INT3_REPLACEMENT))
        {
            pPatchRec->patch.pTempInfo = (PPATCHINFOTEMP)MMR3HeapAllocZ(pVM, MM_TAG_PATM_PATCH, sizeof(PATCHINFOTEMP));
            Log(("Patch code ----------------------------------------------------------\n"));
            patmr3DisasmCodeStream(pVM, PATCHCODE_PTR_GC(&pPatchRec->patch), PATCHCODE_PTR_GC(&pPatchRec->patch), patmr3DisasmCallback, &pPatchRec->patch);
            Log(("Patch code ends -----------------------------------------------------\n"));
            MMR3HeapFree(pPatchRec->patch.pTempInfo);
            pPatchRec->patch.pTempInfo = NULL;
        }
#endif

    }

    /*
     * Correct absolute fixups in the global patch. (helper functions)
     * Bit of a mess. Uses the new patch record, but restored patch functions.
     */
    PRELOCREC pRec = 0;
    AVLPVKEY  key  = 0;

    Log(("Correct fixups in global helper functions\n"));
    while (true)
    {
        int32_t offset;
        RTRCPTR *pFixup;

        /* Get the record that's closest from above */
        pRec = (PRELOCREC)RTAvlPVGetBestFit(&pVM->patm.s.pGlobalPatchRec->patch.FixupTree, key, true);
        if (pRec == 0)
            break;

        key = (AVLPVKEY)(pRec->pRelocPos + 1);   /* search for the next record during the next round. */

        /* rec.pRelocPos now contains the relative position inside the hypervisor area. */
        offset = (int32_t)(pRec->pRelocPos - pVM->patm.s.pPatchMemHC);
        pFixup = (RTRCPTR *)pRec->pRelocPos;

        /* Correct fixups that refer to PATM structures in the hypervisor region (their addresses might have changed). */
        patmCorrectFixup(pVM, u32Version, patmInfo, &pVM->patm.s.pGlobalPatchRec->patch, pRec, offset, pFixup);
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

/**
 * Correct fixups to predefined hypervisor PATM regions. (their addresses might have changed)
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   ulSSMVersion    SSM version
 * @param   patmInfo        Saved PATM structure
 * @param   pPatch          Patch record
 * @param   pRec            Relocation record
 * @param   offset          Offset of referenced data/code
 * @param   pFixup          Fixup address
 */
static void patmCorrectFixup(PVM pVM, unsigned ulSSMVersion, PATM &patmInfo, PPATCHINFO pPatch, PRELOCREC pRec, int32_t offset, RTRCPTR *pFixup)
{
    int32_t delta = pVM->patm.s.pPatchMemGC - patmInfo.pPatchMemGC;

    switch (pRec->uType)
    {
    case FIXUP_ABSOLUTE:
    {
        if (pRec->pSource && !PATMIsPatchGCAddr(pVM, pRec->pSource))
            break;

        if (    *pFixup >= patmInfo.pGCStateGC
            &&  *pFixup <  patmInfo.pGCStateGC + sizeof(PATMGCSTATE))
        {
            LogFlow(("Changing absolute GCState at %RRv from %RRv to %RRv\n", patmInfo.pPatchMemGC + offset, *pFixup, (*pFixup - patmInfo.pGCStateGC) + pVM->patm.s.pGCStateGC));
            *pFixup = (*pFixup - patmInfo.pGCStateGC) + pVM->patm.s.pGCStateGC;
        }
        else
        if (    *pFixup >= patmInfo.pCPUMCtxGC
            &&  *pFixup <  patmInfo.pCPUMCtxGC + sizeof(CPUMCTX))
        {
            LogFlow(("Changing absolute CPUMCTX at %RRv from %RRv to %RRv\n", patmInfo.pPatchMemGC + offset, *pFixup, (*pFixup - patmInfo.pCPUMCtxGC) + pVM->patm.s.pCPUMCtxGC));

            /* The CPUMCTX structure has completely changed, so correct the offsets too. */
            if (ulSSMVersion == PATM_SSM_VERSION_VER16)
            {
                unsigned uCPUMOffset = *pFixup - patmInfo.pCPUMCtxGC;

                /* ''case RT_OFFSETOF()'' does not work as gcc refuses to use & as a constant expression.
                 * Defining RT_OFFSETOF as __builtin_offsetof for gcc would make this possible. But this
                 * function is not available in older gcc versions, at least not in gcc-3.3 */
                if (uCPUMOffset == (unsigned)RT_OFFSETOF(CPUMCTX_VER1_6, dr0))
                {
                    LogFlow(("Changing dr[0] offset from %x to %x\n", uCPUMOffset, RT_OFFSETOF(CPUMCTX, dr[0])));
                    *pFixup = pVM->patm.s.pCPUMCtxGC + RT_OFFSETOF(CPUMCTX, dr[0]);
                }
                else if (uCPUMOffset == (unsigned)RT_OFFSETOF(CPUMCTX_VER1_6, dr1))
                {
                    LogFlow(("Changing dr[1] offset from %x to %x\n", uCPUMOffset, RT_OFFSETOF(CPUMCTX, dr[1])));
                    *pFixup = pVM->patm.s.pCPUMCtxGC + RT_OFFSETOF(CPUMCTX, dr[1]);
                }
                else if (uCPUMOffset == (unsigned)RT_OFFSETOF(CPUMCTX_VER1_6, dr2))
                {
                    LogFlow(("Changing dr[2] offset from %x to %x\n", uCPUMOffset, RT_OFFSETOF(CPUMCTX, dr[2])));
                    *pFixup = pVM->patm.s.pCPUMCtxGC + RT_OFFSETOF(CPUMCTX, dr[2]);
                }
                else if (uCPUMOffset == (unsigned)RT_OFFSETOF(CPUMCTX_VER1_6, dr3))
                {
                    LogFlow(("Changing dr[3] offset from %x to %x\n", uCPUMOffset, RT_OFFSETOF(CPUMCTX, dr[3])));
                    *pFixup = pVM->patm.s.pCPUMCtxGC + RT_OFFSETOF(CPUMCTX, dr[3]);
                }
                else if (uCPUMOffset == (unsigned)RT_OFFSETOF(CPUMCTX_VER1_6, dr4))
                {
                    LogFlow(("Changing dr[4] offset from %x to %x\n", uCPUMOffset, RT_OFFSETOF(CPUMCTX, dr[4])));
                    *pFixup = pVM->patm.s.pCPUMCtxGC + RT_OFFSETOF(CPUMCTX, dr[4]);
                }
                else if (uCPUMOffset == (unsigned)RT_OFFSETOF(CPUMCTX_VER1_6, dr5))
                {
                    LogFlow(("Changing dr[5] offset from %x to %x\n", uCPUMOffset, RT_OFFSETOF(CPUMCTX, dr[5])));
                    *pFixup = pVM->patm.s.pCPUMCtxGC + RT_OFFSETOF(CPUMCTX, dr[5]);
                }
                else if (uCPUMOffset == (unsigned)RT_OFFSETOF(CPUMCTX_VER1_6, dr6))
                {
                    LogFlow(("Changing dr[6] offset from %x to %x\n", uCPUMOffset, RT_OFFSETOF(CPUMCTX, dr[6])));
                    *pFixup = pVM->patm.s.pCPUMCtxGC + RT_OFFSETOF(CPUMCTX, dr[6]);
                }
                else if (uCPUMOffset == (unsigned)RT_OFFSETOF(CPUMCTX_VER1_6, dr7))
                {
                    LogFlow(("Changing dr[7] offset from %x to %x\n", uCPUMOffset, RT_OFFSETOF(CPUMCTX, dr[7])));
                    *pFixup = pVM->patm.s.pCPUMCtxGC + RT_OFFSETOF(CPUMCTX, dr[7]);
                }
                else if (uCPUMOffset == (unsigned)RT_OFFSETOF(CPUMCTX_VER1_6, cr0))
                {
                    LogFlow(("Changing cr0 offset from %x to %x\n", uCPUMOffset, RT_OFFSETOF(CPUMCTX, cr0)));
                    *pFixup = pVM->patm.s.pCPUMCtxGC + RT_OFFSETOF(CPUMCTX, cr0);
                }
                else if (uCPUMOffset == (unsigned)RT_OFFSETOF(CPUMCTX_VER1_6, cr2))
                {
                    LogFlow(("Changing cr2 offset from %x to %x\n", uCPUMOffset, RT_OFFSETOF(CPUMCTX, cr2)));
                    *pFixup = pVM->patm.s.pCPUMCtxGC + RT_OFFSETOF(CPUMCTX, cr2);
                }
                else if (uCPUMOffset == (unsigned)RT_OFFSETOF(CPUMCTX_VER1_6, cr3))
                {
                    LogFlow(("Changing cr3 offset from %x to %x\n", uCPUMOffset, RT_OFFSETOF(CPUMCTX, cr3)));
                    *pFixup = pVM->patm.s.pCPUMCtxGC + RT_OFFSETOF(CPUMCTX, cr3);
                }
                else if (uCPUMOffset == (unsigned)RT_OFFSETOF(CPUMCTX_VER1_6, cr4))
                {
                    LogFlow(("Changing cr4 offset from %x to %x\n", uCPUMOffset, RT_OFFSETOF(CPUMCTX, cr4)));
                    *pFixup = pVM->patm.s.pCPUMCtxGC + RT_OFFSETOF(CPUMCTX, cr4);
                }
                else if (uCPUMOffset == (unsigned)RT_OFFSETOF(CPUMCTX_VER1_6, tr))
                {
                    LogFlow(("Changing tr offset from %x to %x\n", uCPUMOffset, RT_OFFSETOF(CPUMCTX, tr)));
                    *pFixup = pVM->patm.s.pCPUMCtxGC + RT_OFFSETOF(CPUMCTX, tr);
                }
                else if (uCPUMOffset == (unsigned)RT_OFFSETOF(CPUMCTX_VER1_6, ldtr))
                {
                    LogFlow(("Changing ldtr offset from %x to %x\n", uCPUMOffset, RT_OFFSETOF(CPUMCTX, ldtr)));
                    *pFixup = pVM->patm.s.pCPUMCtxGC + RT_OFFSETOF(CPUMCTX, ldtr);
                }
                else if (uCPUMOffset == (unsigned)RT_OFFSETOF(CPUMCTX_VER1_6, gdtr.pGdt))
                {
                    LogFlow(("Changing pGdt offset from %x to %x\n", uCPUMOffset, RT_OFFSETOF(CPUMCTX, gdtr.pGdt)));
                    *pFixup = pVM->patm.s.pCPUMCtxGC + RT_OFFSETOF(CPUMCTX, gdtr.pGdt);
                }
                else if (uCPUMOffset == (unsigned)RT_OFFSETOF(CPUMCTX_VER1_6, gdtr.cbGdt))
                {
                    LogFlow(("Changing cbGdt offset from %x to %x\n", uCPUMOffset, RT_OFFSETOF(CPUMCTX, gdtr.cbGdt)));
                    *pFixup = pVM->patm.s.pCPUMCtxGC + RT_OFFSETOF(CPUMCTX, gdtr.cbGdt);
                }
                else if (uCPUMOffset == (unsigned)RT_OFFSETOF(CPUMCTX_VER1_6, idtr.pIdt))
                {
                    LogFlow(("Changing pIdt offset from %x to %x\n", uCPUMOffset, RT_OFFSETOF(CPUMCTX, idtr.pIdt)));
                    *pFixup = pVM->patm.s.pCPUMCtxGC + RT_OFFSETOF(CPUMCTX, idtr.pIdt);
                }
                else if (uCPUMOffset == (unsigned)RT_OFFSETOF(CPUMCTX_VER1_6, idtr.cbIdt))
                {
                    LogFlow(("Changing cbIdt offset from %x to %x\n", uCPUMOffset, RT_OFFSETOF(CPUMCTX, idtr.cbIdt)));
                    *pFixup = pVM->patm.s.pCPUMCtxGC + RT_OFFSETOF(CPUMCTX, idtr.cbIdt);
                }
                else
                    AssertMsgFailed(("Unexpected CPUMCTX offset %x\n", uCPUMOffset));
            }
            else
                *pFixup = (*pFixup - patmInfo.pCPUMCtxGC) + pVM->patm.s.pCPUMCtxGC;
        }
        else
        if (    *pFixup >= patmInfo.pStatsGC
            &&  *pFixup <  patmInfo.pStatsGC + PATM_STAT_MEMSIZE)
        {
            LogFlow(("Changing absolute Stats at %RRv from %RRv to %RRv\n", patmInfo.pPatchMemGC + offset, *pFixup, (*pFixup - patmInfo.pStatsGC) + pVM->patm.s.pStatsGC));
            *pFixup = (*pFixup - patmInfo.pStatsGC) + pVM->patm.s.pStatsGC;
        }
        else
        if (    *pFixup >= patmInfo.pGCStackGC
            &&  *pFixup <  patmInfo.pGCStackGC + PATM_STACK_TOTAL_SIZE)
        {
            LogFlow(("Changing absolute Stack at %RRv from %RRv to %RRv\n", patmInfo.pPatchMemGC + offset, *pFixup, (*pFixup - patmInfo.pGCStackGC) + pVM->patm.s.pGCStackGC));
            *pFixup = (*pFixup - patmInfo.pGCStackGC) + pVM->patm.s.pGCStackGC;
        }
        else
        if (    *pFixup >= patmInfo.pPatchMemGC
            &&  *pFixup <  patmInfo.pPatchMemGC + patmInfo.cbPatchMem)
        {
            LogFlow(("Changing absolute PatchMem at %RRv from %RRv to %RRv\n", patmInfo.pPatchMemGC + offset, *pFixup, (*pFixup - patmInfo.pPatchMemGC) + pVM->patm.s.pPatchMemGC));
            *pFixup = (*pFixup - patmInfo.pPatchMemGC) + pVM->patm.s.pPatchMemGC;
        }
        else
        if (    ulSSMVersion <= PATM_SSM_VERSION_FIXUP_HACK
            &&  *pFixup >= pVM->pVMRC
            &&  *pFixup < pVM->pVMRC + 32)
        {
            LogFlow(("Changing fLocalForcedActions fixup from %x to %x\n", *pFixup, pVM->pVMRC + RT_OFFSETOF(VM, aCpus[0].fLocalForcedActions)));
            *pFixup = pVM->pVMRC + RT_OFFSETOF(VM, aCpus[0].fLocalForcedActions);
        }
        else
        if (    ulSSMVersion <= PATM_SSM_VERSION_FIXUP_HACK
            &&  *pFixup >= pVM->pVMRC
            &&  *pFixup < pVM->pVMRC + 8192)
        {
            static int cCpuidFixup = 0;
#ifdef LOG_ENABLED
            RTRCPTR oldFixup = *pFixup;
#endif
            /* very dirty assumptions about the cpuid patch and cpuid ordering. */
            switch(cCpuidFixup & 3)
            {
            case 0:
                *pFixup = CPUMR3GetGuestCpuIdDefRCPtr(pVM);
                break;
            case 1:
                *pFixup = CPUMR3GetGuestCpuIdStdRCPtr(pVM);
                break;
            case 2:
                *pFixup = CPUMR3GetGuestCpuIdExtRCPtr(pVM);
                break;
            case 3:
                *pFixup = CPUMR3GetGuestCpuIdCentaurRCPtr(pVM);
                break;
            }
            LogFlow(("Changing cpuid fixup %d from %x to %x\n", cCpuidFixup, oldFixup, *pFixup));
            cCpuidFixup++;
        }
        else
        if (ulSSMVersion >= PATM_SSM_VERSION)
        {
#ifdef LOG_ENABLED
            RTRCPTR oldFixup = *pFixup;
#endif
            /* Core.Key abused to store the type of fixup */
            switch ((uintptr_t)pRec->Core.Key)
            {
            case PATM_FIXUP_CPU_FF_ACTION:
                *pFixup = pVM->pVMRC + RT_OFFSETOF(VM, aCpus[0].fLocalForcedActions);
                LogFlow(("Changing cpu ff action fixup from %x to %x\n", oldFixup, *pFixup));
                break;
            case PATM_FIXUP_CPUID_DEFAULT:
                *pFixup = CPUMR3GetGuestCpuIdDefRCPtr(pVM);
                LogFlow(("Changing cpuid def fixup from %x to %x\n", oldFixup, *pFixup));
                break;
            case PATM_FIXUP_CPUID_STANDARD:
                *pFixup = CPUMR3GetGuestCpuIdStdRCPtr(pVM);
                LogFlow(("Changing cpuid std fixup from %x to %x\n", oldFixup, *pFixup));
                break;
            case PATM_FIXUP_CPUID_EXTENDED:
                *pFixup = CPUMR3GetGuestCpuIdExtRCPtr(pVM);
                LogFlow(("Changing cpuid ext fixup from %x to %x\n", oldFixup, *pFixup));
                break;
            case PATM_FIXUP_CPUID_CENTAUR:
                *pFixup = CPUMR3GetGuestCpuIdCentaurRCPtr(pVM);
                LogFlow(("Changing cpuid centaur fixup from %x to %x\n", oldFixup, *pFixup));
                break;
            default:
                AssertMsgFailed(("Unexpected fixup value %x\n", *pFixup));
                break;
            }
        }

#ifdef RT_OS_WINDOWS
        AssertCompile(RT_OFFSETOF(VM, fGlobalForcedActions) < 32);
#endif
        break;
    }

    case FIXUP_REL_JMPTOPATCH:
    {
        RTRCPTR pTarget = (RTRCPTR)((RTRCINTPTR)pRec->pDest + delta);

        if (    pPatch->uState == PATCH_ENABLED
            &&  (pPatch->flags & PATMFL_PATCHED_GUEST_CODE))
        {
            uint8_t    oldJump[SIZEOF_NEAR_COND_JUMP32];
            uint8_t    temp[SIZEOF_NEAR_COND_JUMP32];
            RTRCPTR    pJumpOffGC;
            RTRCINTPTR displ   = (RTRCINTPTR)pTarget - (RTRCINTPTR)pRec->pSource;
            RTRCINTPTR displOld= (RTRCINTPTR)pRec->pDest - (RTRCINTPTR)pRec->pSource;

            Log(("Relative fixup (g2p) %08X -> %08X at %08X (source=%08x, target=%08x)\n", *(int32_t*)pRec->pRelocPos, displ, pRec->pRelocPos, pRec->pSource, pRec->pDest));

            Assert(pRec->pSource - pPatch->cbPatchJump == pPatch->pPrivInstrGC);
#ifdef PATM_RESOLVE_CONFLICTS_WITH_JUMP_PATCHES
            if (pPatch->cbPatchJump == SIZEOF_NEAR_COND_JUMP32)
            {
                Assert(pPatch->flags & PATMFL_JUMP_CONFLICT);

                pJumpOffGC = pPatch->pPrivInstrGC + 2;    //two byte opcode
                oldJump[0] = pPatch->aPrivInstr[0];
                oldJump[1] = pPatch->aPrivInstr[1];
                *(RTRCUINTPTR *)&oldJump[2] = displOld;
            }
            else
#endif
            if (pPatch->cbPatchJump == SIZEOF_NEARJUMP32)
            {
                pJumpOffGC = pPatch->pPrivInstrGC + 1;    //one byte opcode
                oldJump[0] = 0xE9;
                *(RTRCUINTPTR *)&oldJump[1] = displOld;
            }
            else
            {
                AssertMsgFailed(("Invalid patch jump size %d\n", pPatch->cbPatchJump));
                break;
            }
            Assert(pPatch->cbPatchJump <= sizeof(temp));

            /*
             * Read old patch jump and compare it to the one we previously installed
             */
            int rc = PGMPhysSimpleReadGCPtr(VMMGetCpu0(pVM), temp, pPatch->pPrivInstrGC, pPatch->cbPatchJump);
            Assert(RT_SUCCESS(rc) || rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT);

            if (rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT)
            {
                RTRCPTR pPage = pPatch->pPrivInstrGC & PAGE_BASE_GC_MASK;

                rc = PGMR3HandlerVirtualRegister(pVM, PGMVIRTHANDLERTYPE_ALL, pPage, pPage + (PAGE_SIZE - 1) /* inclusive! */, 0, patmVirtPageHandler, "PATMGCMonitorPage", 0, "PATMMonitorPatchJump");
                Assert(RT_SUCCESS(rc) || rc == VERR_PGM_HANDLER_VIRTUAL_CONFLICT);
            }
            else
            if (memcmp(temp, oldJump, pPatch->cbPatchJump))
            {
                Log(("PATM: Patch jump was overwritten -> disabling patch!!\n"));
                /*
                 * Disable patch; this is not a good solution
                 */
                /* @todo hopefully it was completely overwritten (if the read was successful)!!!! */
                pPatch->uState = PATCH_DISABLED;
            }
            else
            if (RT_SUCCESS(rc))
            {
                rc = PGMPhysSimpleDirtyWriteGCPtr(VMMGetCpu0(pVM), pJumpOffGC, &displ, sizeof(displ));
                AssertRC(rc);
            }
            else
            {
                AssertMsgFailed(("Unexpected error %d from MMR3PhysReadGCVirt\n", rc));
            }
        }
        else
        {
            Log(("Skip the guest jump to patch code for this disabled patch %08X - %08X\n", pPatch->pPrivInstrHC, pRec->pRelocPos));
        }

        pRec->pDest = pTarget;
        break;
    }

    case FIXUP_REL_JMPTOGUEST:
    {
        RTRCPTR    pSource = (RTRCPTR)((RTRCINTPTR)pRec->pSource + delta);
        RTRCINTPTR displ   = (RTRCINTPTR)pRec->pDest - (RTRCINTPTR)pSource;

        Assert(!(pPatch->flags & PATMFL_GLOBAL_FUNCTIONS));
        Log(("Relative fixup (p2g) %08X -> %08X at %08X (source=%08x, target=%08x)\n", *(int32_t*)pRec->pRelocPos, displ, pRec->pRelocPos, pRec->pSource, pRec->pDest));
        *(RTRCUINTPTR *)pRec->pRelocPos = displ;
        pRec->pSource = pSource;
        break;

    }
}
}

