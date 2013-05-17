/* $Id$ */
/** @file
 * PATM - Dynamic Guest OS Patching Manager, Debugger Related Parts.
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_PATM
#include <VBox/vmm/patm.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/hm.h>
#include "PATMInternal.h"
#include "PATMA.h"
#include <VBox/vmm/vm.h>
#include <VBox/err.h>
#include <VBox/log.h>

#include <iprt/assert.h>
#include <iprt/dbg.h>
#include <iprt/string.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Adds a structure member to a debug (pseudo) module as a symbol. */
#define ADD_MEMBER(a_hDbgMod, a_Struct, a_Member, a_pszName) \
        do { \
            rc = RTDbgModSymbolAdd(hDbgMod, a_pszName, 0 /*iSeg*/, RT_OFFSETOF(a_Struct, a_Member),  \
                                   RT_SIZEOFMEMB(a_Struct, a_Member), 0 /*fFlags*/, NULL /*piOrdinal*/); \
            AssertRC(rc); \
        } while (0)

/** Adds a structure member to a debug (pseudo) module as a symbol. */
#define ADD_FUNC(a_hDbgMod, a_BaseRCPtr, a_FuncRCPtr, a_cbFunc, a_pszName) \
        do { \
            int rcAddFunc = RTDbgModSymbolAdd(hDbgMod, a_pszName, 0 /*iSeg*/, \
                                              (RTRCUINTPTR)a_FuncRCPtr - (RTRCUINTPTR)(a_BaseRCPtr),  \
                                              a_cbFunc, 0 /*fFlags*/, NULL /*piOrdinal*/); \
            AssertRC(rcAddFunc); \
        } while (0)



/**
 * Called by PATMR3Init.
 *
 * @param   pVM                 The cross context VM structure.
 */
void patmR3DbgInit(PVM pVM)
{
    pVM->patm.s.hDbgModPatchMem = NIL_RTDBGMOD;
}


/**
 * Called by PATMR3Term.
 *
 * @param   pVM                 The cross context VM structure.
 */
void patmR3DbgTerm(PVM pVM)
{
    if (pVM->patm.s.hDbgModPatchMem != NIL_RTDBGMOD)
    {
        RTDbgModRelease(pVM->patm.s.hDbgModPatchMem);
        pVM->patm.s.hDbgModPatchMem = NIL_RTDBGMOD;
    }
}


/**
 * Called by when the patch memory is reinitialized.
 *
 * @param   pVM                 The cross context VM structure.
 */
void patmR3DbgReset(PVM pVM)
{
    if (pVM->patm.s.hDbgModPatchMem != NIL_RTDBGMOD)
    {
        RTDbgModRemoveAll(pVM->patm.s.hDbgModPatchMem, true);
    }
}


/**
 * Called when a new patch is added or when first populating the address space.
 *
 * @param   pVM                 The cross context VM structure.
 * @param   pPatchRec           The patch record.
 */
void patmR3DbgAddPatch(PVM pVM, PPATMPATCHREC pPatchRec)
{
    if (   pVM->patm.s.hDbgModPatchMem != NIL_RTDBGMOD
        && pPatchRec->patch.pPatchBlockOffset > 0
        && !(pPatchRec->patch.flags & PATMFL_GLOBAL_FUNCTIONS))
    {
        /** @todo find a cheap way of checking whether we've already added the patch.
         *        Using a flag would be nice, except I don't want to consider saved
         *        state considerations right now (I don't recall if we're still
         *        depending on structure layout there or not). */
        int  rc;
        char szName[256];

#define ADD_SZ(a_sz)   \
        do { \
            if (cbLeft >= sizeof(a_sz)) \
            { \
                memcpy(pszName, a_sz, sizeof(a_sz)); \
                pszName += sizeof(a_sz); \
                cbLeft -= sizeof(a_sz);\
            }\
        } while (0)

        /* Start the name off with the address of the guest code. */
        size_t cch = RTStrPrintf(szName, sizeof(szName), "Patch_%#08x", pPatchRec->patch.pPrivInstrGC);
        char   *pszName = &szName[cch];
        size_t  cbLeft  = sizeof(szName) - cch;

        /* Append flags. */
        uint64_t fFlags  = pPatchRec->patch.flags;
        if (fFlags & PATMFL_INTHANDLER)
            ADD_SZ("_IntHandler");
        if (fFlags & PATMFL_SYSENTER)
            ADD_SZ("_SysEnter");
        if (fFlags & PATMFL_GUEST_SPECIFIC)
            ADD_SZ("_GuestSpecific");
        if (fFlags & PATMFL_USER_MODE)
            ADD_SZ("_UserMode");
        if (fFlags & PATMFL_IDTHANDLER)
            ADD_SZ("_IdtHandler");
        if (fFlags & PATMFL_TRAPHANDLER)
            ADD_SZ("_TrapHandler");
        if (fFlags & PATMFL_DUPLICATE_FUNCTION)
            ADD_SZ("_DupFunc");
        if (fFlags & PATMFL_REPLACE_FUNCTION_CALL)
            ADD_SZ("_ReplFunc");
        if (fFlags & PATMFL_TRAPHANDLER_WITH_ERRORCODE)
            ADD_SZ("_TrapHandlerErrCd");
        if (fFlags & PATMFL_MMIO_ACCESS)
            ADD_SZ("_MmioAccess");
        if (fFlags & PATMFL_SYSENTER_XP)
            ADD_SZ("_SysEnterXP");
        if (fFlags & PATMFL_INT3_REPLACEMENT)
            ADD_SZ("_Int3Replacement");
        if (fFlags & PATMFL_SUPPORT_CALLS)
            ADD_SZ("_SupportCalls");
        if (fFlags & PATMFL_SUPPORT_INDIRECT_CALLS)
            ADD_SZ("_SupportIndirectCalls");
        if (fFlags & PATMFL_IDTHANDLER_WITHOUT_ENTRYPOINT)
            ADD_SZ("_IdtHandlerWE");
        if (fFlags & PATMFL_INHIBIT_IRQS)
            ADD_SZ("_InhibitIrqs");
        if (fFlags & PATMFL_RECOMPILE_NEXT)
            ADD_SZ("_RecompileNext");
        if (fFlags & PATMFL_CALLABLE_AS_FUNCTION)
            ADD_SZ("_Callable");
        if (fFlags & PATMFL_TRAMPOLINE)
            ADD_SZ("_Trampoline");
        if (fFlags & PATMFL_PATCHED_GUEST_CODE)
            ADD_SZ("_PatchedGuestCode");
        if (fFlags & PATMFL_MUST_INSTALL_PATCHJMP)
            ADD_SZ("_MustInstallPatchJmp");
        if (fFlags & PATMFL_INT3_REPLACEMENT_BLOCK)
            ADD_SZ("_Int3ReplacementBlock");
        if (fFlags & PATMFL_EXTERNAL_JUMP_INSIDE)
            ADD_SZ("_ExtJmp");
        if (fFlags & PATMFL_CODE_REFERENCED)
            ADD_SZ("_CodeRefed");

        /* If we have a symbol near the guest address, append that. */
        if (cbLeft > 8)
        {
            DBGFSYMBOL Symbol;
            RTGCINTPTR offDisp;

            rc = DBGFR3SymbolByAddr(pVM, pPatchRec->patch.pPrivInstrGC, &offDisp, &Symbol);
            if (RT_SUCCESS(rc))
            {
                ADD_SZ("__");
                RTStrCopy(pszName, cbLeft, Symbol.szName);
            }
        }

        /* Add it (may fail due to enable/disable patches). */
        RTDbgModSymbolAdd(pVM->patm.s.hDbgModPatchMem, szName, 0 /*iSeg*/,
                          pPatchRec->patch.pPatchBlockOffset,
                          pPatchRec->patch.cbPatchBlockSize,
                          0 /*fFlags*/, NULL /*piOrdinal*/);

    }
}


/**
 * Enumeration callback used by patmR3DbgAddPatches
 *
 * @returns 0 (continue enum)
 * @param   pNode               The patch record node.
 * @param   pvUser              The cross context VM structure.
 */
static DECLCALLBACK(int) patmR3DbgAddPatchCallback(PAVLOU32NODECORE pNode, void *pvUser)
{
    patmR3DbgAddPatch((PVM)pvUser, (PPATMPATCHREC)pNode);
    return 0;
}


/**
 * Populates an empty "patches" (hDbgModPatchMem) module with patch symbols.
 *
 * @param   pVM                 The cross context VM structure.
 * @param   hDbgMod             The debug module handle.
 */
static void patmR3DbgAddPatches(PVM pVM, RTDBGMOD hDbgMod)
{
    /*
     * Global functions.
     */
    ADD_FUNC(hDbgMod, pVM->patm.s.pPatchMemGC, pVM->patm.s.pfnHelperCallGC, PATMLookupAndCallRecord.size, "PATMLookupAndCall");
    ADD_FUNC(hDbgMod, pVM->patm.s.pPatchMemGC, pVM->patm.s.pfnHelperRetGC,  PATMRetFunctionRecord.size,   "PATMRetFunction");
    ADD_FUNC(hDbgMod, pVM->patm.s.pPatchMemGC, pVM->patm.s.pfnHelperJumpGC, PATMLookupAndJumpRecord.size, "PATMLookupAndJump");
    ADD_FUNC(hDbgMod, pVM->patm.s.pPatchMemGC, pVM->patm.s.pfnHelperIretGC, PATMIretFunctionRecord.size,  "PATMIretFunction");

    /*
     * The patches.
     */
    RTAvloU32DoWithAll(&pVM->patm.s.PatchLookupTreeHC->PatchTree, true /*fFromLeft*/, patmR3DbgAddPatchCallback, pVM);
}


/**
 * Populate DBGF_AS_RC with PATM symbols.
 *
 * Called by dbgfR3AsLazyPopulate when DBGF_AS_RC or DBGF_AS_RC_AND_GC_GLOBAL is
 * accessed for the first time.
 *
 * @param   pVM                 The cross context VM structure.
 * @param   hDbgAs              The DBGF_AS_RC address space handle.
 */
VMMR3_INT_DECL(void) PATMR3DbgPopulateAddrSpace(PVM pVM, RTDBGAS hDbgAs)
{
    AssertReturnVoid(!HMIsEnabled(pVM));

    /*
     * Add a fake debug module for the PATMGCSTATE structure.
     */
    RTDBGMOD hDbgMod;
    int rc = RTDbgModCreate(&hDbgMod, "patmgcstate", sizeof(PATMGCSTATE), 0 /*fFlags*/);
    if (RT_SUCCESS(rc))
    {
        ADD_MEMBER(hDbgMod, PATMGCSTATE, uVMFlags,                  "uVMFlags");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, uPendingAction,            "uPendingAction");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, uPatchCalls,               "uPatchCalls");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, uScratch,                  "uScratch");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, uIretEFlags,               "uIretEFlags");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, uIretCS,                   "uIretCS");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, uIretEIP,                  "uIretEIP");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, Psp,                       "Psp");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, fPIF,                      "fPIF");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, GCPtrInhibitInterrupts,    "GCPtrInhibitInterrupts");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, GCCallPatchTargetAddr,     "GCCallPatchTargetAddr");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, GCCallReturnAddr,          "GCCallReturnAddr");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, Restore.uEAX,              "Restore.uEAX");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, Restore.uECX,              "Restore.uECX");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, Restore.uEDI,              "Restore.uEDI");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, Restore.eFlags,            "Restore.eFlags");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, Restore.uFlags,            "Restore.uFlags");

        rc = RTDbgAsModuleLink(hDbgAs, hDbgMod, pVM->patm.s.pGCStateGC, 0 /*fFlags/*/);
        AssertLogRelRC(rc);
        RTDbgModRelease(hDbgMod);
    }

    /*
     * Add a fake debug module for the patches.
     */
    rc = RTDbgModCreate(&hDbgMod, "patches", pVM->patm.s.cbPatchMem, 0 /*fFlags*/);
    if (RT_SUCCESS(rc))
    {
        pVM->patm.s.hDbgModPatchMem = hDbgMod;
        patmR3DbgAddPatches(pVM, hDbgMod);

        rc = RTDbgAsModuleLink(hDbgAs, hDbgMod, pVM->patm.s.pPatchMemGC, 0 /*fFlags/*/);
        AssertLogRelRC(rc);
    }
}


