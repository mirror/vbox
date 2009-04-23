/* $Id$ */
/** @file
 * PATM - Dynamic Guest OS Patching Manager
 *
 * NOTE: Never ever reuse patch memory!!
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
#include <VBox/cpumdis.h>
#include <VBox/iom.h>
#include <VBox/sup.h>
#include <VBox/mm.h>
#include <VBox/ssm.h>
#include <VBox/pdm.h>
#include <VBox/trpm.h>
#include <VBox/cfgm.h>
#include <VBox/param.h>
#include <VBox/selm.h>
#include <iprt/avl.h>
#include "PATMInternal.h"
#include "PATMPatch.h"
#include <VBox/vm.h>
#include <VBox/csam.h>

#include <VBox/dbg.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <VBox/dis.h>
#include <VBox/disopcode.h>

#include <iprt/string.h>
#include "PATMA.h"

//#define PATM_REMOVE_PATCH_ON_TOO_MANY_TRAPS
//#define PATM_DISABLE_ALL

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/

static int          patmDisableUnusablePatch(PVM pVM, RTRCPTR pInstrGC, RTRCPTR pConflictAddr, PPATCHINFO pPatch);
static int          patmActivateInt3Patch(PVM pVM, PPATCHINFO pPatch);
static int          patmDeactivateInt3Patch(PVM pVM, PPATCHINFO pPatch);

#ifdef LOG_ENABLED // keep gcc quiet
static bool         patmIsCommonIDTHandlerPatch(PVM pVM, RTRCPTR pInstrGC);
#endif
#ifdef VBOX_WITH_STATISTICS
static const char  *PATMPatchType(PVM pVM, PPATCHINFO pPatch);
static void         patmResetStat(PVM pVM, void *pvSample);
static void         patmPrintStat(PVM pVM, void *pvSample, char *pszBuf, size_t cchBuf);
#endif

#define             patmPatchHCPtr2PatchGCPtr(pVM, pHC)      (pVM->patm.s.pPatchMemGC + (pHC - pVM->patm.s.pPatchMemHC))
#define             patmPatchGCPtr2PatchHCPtr(pVM, pGC)      (pVM->patm.s.pPatchMemHC + (pGC - pVM->patm.s.pPatchMemGC))

static int               patmReinit(PVM pVM);
static DECLCALLBACK(int) RelocatePatches(PAVLOU32NODECORE pNode, void *pParam);

#ifdef VBOX_WITH_DEBUGGER
static DECLCALLBACK(int) DisableAllPatches(PAVLOU32NODECORE pNode, void *pVM);
static DECLCALLBACK(int) patmr3CmdOn(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) patmr3CmdOff(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);

/** Command descriptors. */
static const DBGCCMD    g_aCmds[] =
{
    /* pszCmd,      cArgsMin, cArgsMax, paArgDesc,          cArgDescs,                  pResultDesc,        fFlags,     pfnHandler          pszSyntax,          ....pszDescription */
    { "patmon",     0,        0,        NULL,               0,                          NULL,               0,          patmr3CmdOn,        "",                     "Enable patching." },
    { "patmoff",    0,        0,        NULL,               0,                          NULL,               0,          patmr3CmdOff,       "",                     "Disable patching." },
};
#endif

/* Don't want to break saved states, so put it here as a global variable. */
static unsigned int cIDTHandlersDisabled = 0;

/**
 * Initializes the PATM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) PATMR3Init(PVM pVM)
{
    int rc;

    Log(("PATMR3Init: Patch record size %d\n", sizeof(PATCHINFO)));

    /* These values can't change as they are hardcoded in patch code (old saved states!) */
    AssertCompile(VM_FF_TIMER   == VMCPU_FF_TIMER);
    AssertCompile(VM_FF_REQUEST == VMCPU_FF_REQUEST);
    AssertCompile(VMCPU_FF_INTERRUPT_APIC == RT_BIT_32(0));
    AssertCompile(VMCPU_FF_INTERRUPT_PIC == RT_BIT_32(1));

    AssertReleaseMsg(PATMInterruptFlag == (VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC | VMCPU_FF_TIMER | VMCPU_FF_REQUEST),
                     ("Interrupt flags out of sync!! PATMInterruptFlag=%#x expected %#x. broken assembler?\n", PATMInterruptFlag, VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC | VMCPU_FF_TIMER | VMCPU_FF_REQUEST));

    /* Allocate patch memory and GC patch state memory. */
    pVM->patm.s.cbPatchMem = PATCH_MEMORY_SIZE;
    /* Add another page in case the generated code is much larger than expected. */
    /** @todo bad safety precaution */
    rc = MMR3HyperAllocOnceNoRel(pVM, PATCH_MEMORY_SIZE + PAGE_SIZE + PATM_STACK_TOTAL_SIZE + PAGE_SIZE + PATM_STAT_MEMSIZE, PAGE_SIZE, MM_TAG_PATM, (void **)&pVM->patm.s.pPatchMemHC);
    if (RT_FAILURE(rc))
    {
        Log(("MMR3HyperAlloc failed with %Rrc\n", rc));
        return rc;
    }
    pVM->patm.s.pPatchMemGC = MMHyperR3ToRC(pVM, pVM->patm.s.pPatchMemHC);

    /* PATM stack page for call instruction execution. (2 parts: one for our private stack and one to store the original return address */
    pVM->patm.s.pGCStackHC  = (RTRCPTR *)(pVM->patm.s.pPatchMemHC + PATCH_MEMORY_SIZE + PAGE_SIZE);
    pVM->patm.s.pGCStackGC  = MMHyperR3ToRC(pVM, pVM->patm.s.pGCStackHC);

    /*
     * Hypervisor memory for GC status data (read/write)
     *
     * Note1: This is non-critical data; if trashed by the guest, then it will only cause problems for itself
     * Note2: This doesn't really belong here, but we need access to it for relocation purposes
     *
     */
    Assert(sizeof(PATMGCSTATE) < PAGE_SIZE);    /** @note hardcoded dependencies on this exist. */
    pVM->patm.s.pGCStateHC  = (PPATMGCSTATE)((uint8_t *)pVM->patm.s.pGCStackHC + PATM_STACK_TOTAL_SIZE);
    pVM->patm.s.pGCStateGC  = MMHyperR3ToRC(pVM, pVM->patm.s.pGCStateHC);

    /* Hypervisor memory for patch statistics */
    pVM->patm.s.pStatsHC  = (PSTAMRATIOU32)((uint8_t *)pVM->patm.s.pGCStateHC + PAGE_SIZE);
    pVM->patm.s.pStatsGC  = MMHyperR3ToRC(pVM, pVM->patm.s.pStatsHC);

    /* Memory for patch lookup trees. */
    rc = MMHyperAlloc(pVM, sizeof(*pVM->patm.s.PatchLookupTreeHC), 0, MM_TAG_PATM, (void **)&pVM->patm.s.PatchLookupTreeHC);
    AssertRCReturn(rc, rc);
    pVM->patm.s.PatchLookupTreeGC = MMHyperR3ToRC(pVM, pVM->patm.s.PatchLookupTreeHC);

#ifdef RT_ARCH_AMD64 /* see patmReinit(). */
    /* Check CFGM option. */
    rc = CFGMR3QueryBool(CFGMR3GetRoot(pVM), "PATMEnabled", &pVM->fPATMEnabled);
    if (RT_FAILURE(rc))
# ifdef PATM_DISABLE_ALL
        pVM->fPATMEnabled = false;
# else
        pVM->fPATMEnabled = true;
# endif
#endif

    rc = patmReinit(pVM);
    AssertRC(rc);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Register save and load state notificators.
     */
    rc = SSMR3RegisterInternal(pVM, "PATM", 0, PATM_SSM_VERSION, sizeof(pVM->patm.s) + PATCH_MEMORY_SIZE  + PAGE_SIZE + PATM_STACK_TOTAL_SIZE + PAGE_SIZE,
                               NULL, patmr3Save, NULL,
                               NULL, patmr3Load, NULL);
    if (RT_FAILURE(rc))
    {
        AssertRC(rc);
        return rc;
    }

#ifdef VBOX_WITH_DEBUGGER
    /*
     * Debugger commands.
     */
    static bool fRegisteredCmds = false;
    if (!fRegisteredCmds)
    {
        int rc = DBGCRegisterCommands(&g_aCmds[0], RT_ELEMENTS(g_aCmds));
        if (RT_SUCCESS(rc))
            fRegisteredCmds = true;
    }
#endif

#ifdef VBOX_WITH_STATISTICS
    STAM_REG(pVM, &pVM->patm.s.StatNrOpcodeRead,  STAMTYPE_COUNTER, "/PATM/OpcodeBytesRead",      STAMUNIT_OCCURENCES,     "The number of opcode bytes read by the recompiler.");
    STAM_REG(pVM, &pVM->patm.s.StatPATMMemoryUsed,STAMTYPE_COUNTER, "/PATM/MemoryUsed",           STAMUNIT_OCCURENCES,     "The amount of hypervisor heap used for patches.");
    STAM_REG(pVM, &pVM->patm.s.StatDisabled,      STAMTYPE_COUNTER, "/PATM/Patch/Disabled",        STAMUNIT_OCCURENCES,     "Number of times patches were disabled.");
    STAM_REG(pVM, &pVM->patm.s.StatEnabled,       STAMTYPE_COUNTER, "/PATM/Patch/Enabled",         STAMUNIT_OCCURENCES,     "Number of times patches were enabled.");
    STAM_REG(pVM, &pVM->patm.s.StatDirty,         STAMTYPE_COUNTER, "/PATM/Patch/Dirty",           STAMUNIT_OCCURENCES,     "Number of times patches were marked dirty.");
    STAM_REG(pVM, &pVM->patm.s.StatUnusable,      STAMTYPE_COUNTER, "/PATM/Patch/Unusable",        STAMUNIT_OCCURENCES,     "Number of unusable patches (conflicts).");
    STAM_REG(pVM, &pVM->patm.s.StatInstalled,     STAMTYPE_COUNTER, "/PATM/Patch/Installed",       STAMUNIT_OCCURENCES,     "Number of installed patches.");
    STAM_REG(pVM, &pVM->patm.s.StatInt3Callable,  STAMTYPE_COUNTER, "/PATM/Patch/Int3Callable",    STAMUNIT_OCCURENCES,     "Number of cli patches turned into int3 patches.");

    STAM_REG(pVM, &pVM->patm.s.StatInt3BlockRun,  STAMTYPE_COUNTER, "/PATM/Patch/Run/Int3",        STAMUNIT_OCCURENCES,     "Number of times an int3 block patch was executed.");
    STAMR3RegisterF(pVM, &pVM->patm.s.pGCStateHC->uPatchCalls, STAMTYPE_U32, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, NULL, "/PATM/Patch/Run/Normal");

    STAM_REG(pVM, &pVM->patm.s.StatInstalledFunctionPatches,   STAMTYPE_COUNTER, "/PATM/Patch/Installed/Function",       STAMUNIT_OCCURENCES,     "Number of installed function duplication patches.");
    STAM_REG(pVM, &pVM->patm.s.StatInstalledTrampoline,   STAMTYPE_COUNTER, "/PATM/Patch/Installed/Trampoline",          STAMUNIT_OCCURENCES,     "Number of installed trampoline patches.");
    STAM_REG(pVM, &pVM->patm.s.StatInstalledJump, STAMTYPE_COUNTER, "/PATM/Patch/Installed/Jump",  STAMUNIT_OCCURENCES,     "Number of installed jump patches.");

    STAM_REG(pVM, &pVM->patm.s.StatOverwritten,   STAMTYPE_COUNTER, "/PATM/Patch/Overwritten",     STAMUNIT_OCCURENCES,     "Number of overwritten patches.");
    STAM_REG(pVM, &pVM->patm.s.StatFixedConflicts,STAMTYPE_COUNTER, "/PATM/Patch/ConflictFixed",   STAMUNIT_OCCURENCES,     "Number of fixed conflicts.");
    STAM_REG(pVM, &pVM->patm.s.StatFlushed,       STAMTYPE_COUNTER, "/PATM/Patch/Flushed",         STAMUNIT_OCCURENCES,     "Number of flushes of pages with patch jumps.");
    STAM_REG(pVM, &pVM->patm.s.StatMonitored,     STAMTYPE_COUNTER, "/PATM/Patch/Monitored",       STAMUNIT_OCCURENCES,     "Number of patches in monitored patch pages.");
    STAM_REG(pVM, &pVM->patm.s.StatPageBoundaryCrossed, STAMTYPE_COUNTER, "/PATM/Patch/BoundaryCross", STAMUNIT_OCCURENCES,     "Number of refused patches due to patch jump crossing page boundary.");

    STAM_REG(pVM, &pVM->patm.s.StatHandleTrap,    STAMTYPE_PROFILE, "/PATM/HandleTrap",           STAMUNIT_TICKS_PER_CALL, "Profiling of PATMR3HandleTrap");
    STAM_REG(pVM, &pVM->patm.s.StatPushTrap,      STAMTYPE_COUNTER, "/PATM/HandleTrap/PushWP",    STAMUNIT_OCCURENCES,     "Number of traps due to monitored stack pages.");

    STAM_REG(pVM, &pVM->patm.s.StatSwitchBack,    STAMTYPE_COUNTER, "/PATM/SwitchBack",           STAMUNIT_OCCURENCES,     "Switch back to original guest code when IF=1 & executing PATM instructions");
    STAM_REG(pVM, &pVM->patm.s.StatSwitchBackFail,STAMTYPE_COUNTER, "/PATM/SwitchBackFail",       STAMUNIT_OCCURENCES,     "Failed switch back to original guest code when IF=1 & executing PATM instructions");

    STAM_REG(pVM, &pVM->patm.s.StatDuplicateREQFailed,  STAMTYPE_COUNTER,      "/PATM/Function/DupREQ/Failed",     STAMUNIT_OCCURENCES,    "Nr of failed PATMR3DuplicateFunctionRequest calls");
    STAM_REG(pVM, &pVM->patm.s.StatDuplicateREQSuccess, STAMTYPE_COUNTER,      "/PATM/Function/DupREQ/Success",    STAMUNIT_OCCURENCES,    "Nr of successful PATMR3DuplicateFunctionRequest calls");
    STAM_REG(pVM, &pVM->patm.s.StatDuplicateUseExisting,STAMTYPE_COUNTER,      "/PATM/Function/DupREQ/UseExist",   STAMUNIT_OCCURENCES,    "Nr of successful PATMR3DuplicateFunctionRequest calls when using an existing patch");

    STAM_REG(pVM, &pVM->patm.s.StatFunctionLookupInsert,  STAMTYPE_COUNTER,    "/PATM/Function/Lookup/Insert",     STAMUNIT_OCCURENCES,    "Nr of successful function address insertions");
    STAM_REG(pVM, &pVM->patm.s.StatFunctionLookupReplace, STAMTYPE_COUNTER,    "/PATM/Function/Lookup/Replace",    STAMUNIT_OCCURENCES,    "Nr of successful function address replacements");
    STAM_REG(pVM, &pVM->patm.s.StatU32FunctionMaxSlotsUsed, STAMTYPE_U32_RESET,"/PATM/Function/Lookup/MaxSlots",   STAMUNIT_OCCURENCES,    "Maximum nr of lookup slots used in all call patches");

    STAM_REG(pVM, &pVM->patm.s.StatFunctionFound,     STAMTYPE_COUNTER, "/PATM/Function/Found",     STAMUNIT_OCCURENCES,     "Nr of successful function patch lookups in GC");
    STAM_REG(pVM, &pVM->patm.s.StatFunctionNotFound,  STAMTYPE_COUNTER, "/PATM/Function/NotFound",  STAMUNIT_OCCURENCES,     "Nr of failed function patch lookups in GC");

    STAM_REG(pVM, &pVM->patm.s.StatPatchWrite,        STAMTYPE_PROFILE, "/PATM/Write/Handle",       STAMUNIT_TICKS_PER_CALL, "Profiling of PATMR3PatchWrite");
    STAM_REG(pVM, &pVM->patm.s.StatPatchWriteDetect,  STAMTYPE_PROFILE, "/PATM/Write/Detect",       STAMUNIT_TICKS_PER_CALL, "Profiling of PATMIsWriteToPatchPage");
    STAM_REG(pVM, &pVM->patm.s.StatPatchWriteInterpreted, STAMTYPE_COUNTER, "/PATM/Write/Interpreted/Success", STAMUNIT_OCCURENCES,  "Nr of interpreted patch writes.");
    STAM_REG(pVM, &pVM->patm.s.StatPatchWriteInterpretedFailed, STAMTYPE_COUNTER, "/PATM/Write/Interpreted/Failed", STAMUNIT_OCCURENCES,  "Nr of failed interpreted patch writes.");

    STAM_REG(pVM, &pVM->patm.s.StatPatchRefreshSuccess, STAMTYPE_COUNTER, "/PATM/Refresh/Success",    STAMUNIT_OCCURENCES, "Successful patch refreshes");
    STAM_REG(pVM, &pVM->patm.s.StatPatchRefreshFailed,  STAMTYPE_COUNTER, "/PATM/Refresh/Failure",    STAMUNIT_OCCURENCES, "Failed patch refreshes");

    STAM_REG(pVM, &pVM->patm.s.StatPatchPageInserted, STAMTYPE_COUNTER, "/PATM/Page/Inserted",      STAMUNIT_OCCURENCES,     "Nr of inserted guest pages that were patched");
    STAM_REG(pVM, &pVM->patm.s.StatPatchPageRemoved,  STAMTYPE_COUNTER, "/PATM/Page/Removed",       STAMUNIT_OCCURENCES,     "Nr of removed guest pages that were patched");

    STAM_REG(pVM, &pVM->patm.s.StatInstrDirty,        STAMTYPE_COUNTER, "/PATM/Instr/Dirty/Detected",  STAMUNIT_OCCURENCES,     "Number of times instructions were marked dirty.");
    STAM_REG(pVM, &pVM->patm.s.StatInstrDirtyGood,    STAMTYPE_COUNTER, "/PATM/Instr/Dirty/Corrected", STAMUNIT_OCCURENCES,     "Number of times instructions were marked dirty and corrected later on.");
    STAM_REG(pVM, &pVM->patm.s.StatInstrDirtyBad,     STAMTYPE_COUNTER, "/PATM/Instr/Dirty/Failed",    STAMUNIT_OCCURENCES,     "Number of times instructions were marked dirty and we were not able to correct them.");

    STAM_REG(pVM, &pVM->patm.s.StatSysEnter,          STAMTYPE_COUNTER, "/PATM/Emul/SysEnter",         STAMUNIT_OCCURENCES,     "Number of times sysenter was emulated.");
    STAM_REG(pVM, &pVM->patm.s.StatSysExit,           STAMTYPE_COUNTER, "/PATM/Emul/SysExit" ,         STAMUNIT_OCCURENCES,     "Number of times sysexit was emulated.");
    STAM_REG(pVM, &pVM->patm.s.StatEmulIret,          STAMTYPE_COUNTER, "/PATM/Emul/Iret/Success",     STAMUNIT_OCCURENCES,     "Number of times iret was emulated.");
    STAM_REG(pVM, &pVM->patm.s.StatEmulIretFailed,    STAMTYPE_COUNTER, "/PATM/Emul/Iret/Failed",      STAMUNIT_OCCURENCES,     "Number of times iret was emulated.");

    STAM_REG(pVM, &pVM->patm.s.StatGenRet,            STAMTYPE_COUNTER, "/PATM/Gen/Ret" ,         STAMUNIT_OCCURENCES,     "Number of generated ret instructions.");
    STAM_REG(pVM, &pVM->patm.s.StatGenRetReused,      STAMTYPE_COUNTER, "/PATM/Gen/RetReused" ,   STAMUNIT_OCCURENCES,     "Number of reused ret instructions.");
    STAM_REG(pVM, &pVM->patm.s.StatGenCall,           STAMTYPE_COUNTER, "/PATM/Gen/Call",         STAMUNIT_OCCURENCES,     "Number of generated call instructions.");
    STAM_REG(pVM, &pVM->patm.s.StatGenJump,           STAMTYPE_COUNTER, "/PATM/Gen/Jmp" ,         STAMUNIT_OCCURENCES,     "Number of generated indirect jump instructions.");
    STAM_REG(pVM, &pVM->patm.s.StatGenPopf,           STAMTYPE_COUNTER, "/PATM/Gen/Popf" ,        STAMUNIT_OCCURENCES,     "Number of generated popf instructions.");

    STAM_REG(pVM, &pVM->patm.s.StatCheckPendingIRQ,   STAMTYPE_COUNTER, "/PATM/GC/CheckIRQ" ,        STAMUNIT_OCCURENCES,     "Number of traps that ask to check for pending irqs.");
#endif /* VBOX_WITH_STATISTICS */

    Log(("PATMCallRecord.size           %d\n", PATMCallRecord.size));
    Log(("PATMCallIndirectRecord.size   %d\n", PATMCallIndirectRecord.size));
    Log(("PATMRetRecord.size            %d\n", PATMRetRecord.size));
    Log(("PATMJumpIndirectRecord.size   %d\n", PATMJumpIndirectRecord.size));
    Log(("PATMPopf32Record.size         %d\n", PATMPopf32Record.size));
    Log(("PATMIretRecord.size           %d\n", PATMIretRecord.size));
    Log(("PATMStiRecord.size            %d\n", PATMStiRecord.size));
    Log(("PATMCheckIFRecord.size        %d\n", PATMCheckIFRecord.size));

    return rc;
}

/**
 * Finalizes HMA page attributes.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 */
VMMR3DECL(int) PATMR3InitFinalize(PVM pVM)
{
    /* The GC state, stack and statistics must be read/write for the guest (supervisor only of course). */
    int rc = PGMMapSetPage(pVM, pVM->patm.s.pGCStateGC, PAGE_SIZE, X86_PTE_P | X86_PTE_A | X86_PTE_D | X86_PTE_RW);
    if (RT_FAILURE(rc))
        Log(("PATMR3InitFinalize: PGMMapSetPage failed with %Rrc!!\n", rc));

    rc = PGMMapSetPage(pVM, pVM->patm.s.pGCStackGC, PATM_STACK_TOTAL_SIZE, X86_PTE_P | X86_PTE_A | X86_PTE_D | X86_PTE_RW);
    if (RT_FAILURE(rc))
        Log(("PATMR3InitFinalize: PGMMapSetPage failed with %Rrc!!\n", rc));

    rc = PGMMapSetPage(pVM, pVM->patm.s.pStatsGC, PATM_STAT_MEMSIZE, X86_PTE_P | X86_PTE_A | X86_PTE_D | X86_PTE_RW);
    if (RT_FAILURE(rc))
        Log(("PATMR3InitFinalize: PGMMapSetPage failed with %Rrc!!\n", rc));

    return rc;
}

/**
 * (Re)initializes PATM
 *
 * @param   pVM     The VM.
 */
static int patmReinit(PVM pVM)
{
    int rc;

    /*
     * Assert alignment and sizes.
     */
    AssertRelease(!(RT_OFFSETOF(VM, patm.s) & 31));
    AssertRelease(sizeof(pVM->patm.s) <= sizeof(pVM->patm.padding));

    /*
     * Setup any fixed pointers and offsets.
     */
    pVM->patm.s.offVM = RT_OFFSETOF(VM, patm);

#ifndef RT_ARCH_AMD64 /* would be nice if this was changed everywhere. was driving me crazy on AMD64. */
#ifndef PATM_DISABLE_ALL
    pVM->fPATMEnabled = true;
#endif
#endif

    Assert(pVM->patm.s.pGCStateHC);
    memset(pVM->patm.s.pGCStateHC, 0, PAGE_SIZE);
    AssertReleaseMsg(pVM->patm.s.pGCStateGC, ("Impossible! MMHyperHC2GC(%p) failed!\n", pVM->patm.s.pGCStateGC));

    Log(("Patch memory allocated at %p - %RRv\n", pVM->patm.s.pPatchMemHC, pVM->patm.s.pPatchMemGC));
    pVM->patm.s.pGCStateHC->uVMFlags = X86_EFL_IF;

    Assert(pVM->patm.s.pGCStackHC);
    memset(pVM->patm.s.pGCStackHC, 0, PAGE_SIZE);
    AssertReleaseMsg(pVM->patm.s.pGCStackGC, ("Impossible! MMHyperHC2GC(%p) failed!\n", pVM->patm.s.pGCStackGC));
    pVM->patm.s.pGCStateHC->Psp = PATM_STACK_SIZE;
    pVM->patm.s.pGCStateHC->fPIF = 1;   /* PATM Interrupt Flag */

    Assert(pVM->patm.s.pStatsHC);
    memset(pVM->patm.s.pStatsHC, 0, PATM_STAT_MEMSIZE);
    AssertReleaseMsg(pVM->patm.s.pStatsGC, ("Impossible! MMHyperHC2GC(%p) failed!\n", pVM->patm.s.pStatsGC));

    Assert(pVM->patm.s.pPatchMemHC);
    Assert(pVM->patm.s.pPatchMemGC = MMHyperR3ToRC(pVM, pVM->patm.s.pPatchMemHC));
    memset(pVM->patm.s.pPatchMemHC, 0, PATCH_MEMORY_SIZE);
    AssertReleaseMsg(pVM->patm.s.pPatchMemGC, ("Impossible! MMHyperHC2GC(%p) failed!\n", pVM->patm.s.pPatchMemHC));

    /* Needed for future patching of sldt/sgdt/sidt/str etc. */
    pVM->patm.s.pCPUMCtxGC = VM_RC_ADDR(pVM, CPUMQueryGuestCtxPtr(VMMGetCpu0(pVM)));

    Assert(pVM->patm.s.PatchLookupTreeHC);
    Assert(pVM->patm.s.PatchLookupTreeGC == MMHyperR3ToRC(pVM, pVM->patm.s.PatchLookupTreeHC));

    /*
     * (Re)Initialize PATM structure
     */
    Assert(!pVM->patm.s.PatchLookupTreeHC->PatchTree);
    Assert(!pVM->patm.s.PatchLookupTreeHC->PatchTreeByPatchAddr);
    Assert(!pVM->patm.s.PatchLookupTreeHC->PatchTreeByPage);
    pVM->patm.s.offPatchMem          = 16;  /* don't start with zero here */
    pVM->patm.s.uCurrentPatchIdx     = 1;   /* Index zero is a dummy */
    pVM->patm.s.pvFaultMonitor       = 0;
    pVM->patm.s.deltaReloc           = 0;

    /* Lowest and highest patched instruction */
    pVM->patm.s.pPatchedInstrGCLowest     = ~0;
    pVM->patm.s.pPatchedInstrGCHighest    = 0;

    pVM->patm.s.PatchLookupTreeHC->PatchTree            = 0;
    pVM->patm.s.PatchLookupTreeHC->PatchTreeByPatchAddr = 0;
    pVM->patm.s.PatchLookupTreeHC->PatchTreeByPage      = 0;

    pVM->patm.s.pfnSysEnterPatchGC = 0;
    pVM->patm.s.pfnSysEnterGC = 0;

    pVM->patm.s.fOutOfMemory = false;

    pVM->patm.s.pfnHelperCallGC = 0;

    /* Generate all global functions to be used by future patches. */
    /* We generate a fake patch in order to use the existing code for relocation. */
    rc = MMHyperAlloc(pVM, sizeof(PATMPATCHREC), 0, MM_TAG_PATM_PATCH, (void **)&pVM->patm.s.pGlobalPatchRec);
    if (RT_FAILURE(rc))
    {
        Log(("Out of memory!!!!\n"));
        return VERR_NO_MEMORY;
    }
    pVM->patm.s.pGlobalPatchRec->patch.flags             = PATMFL_GLOBAL_FUNCTIONS;
    pVM->patm.s.pGlobalPatchRec->patch.uState            = PATCH_ENABLED;
    pVM->patm.s.pGlobalPatchRec->patch.pPatchBlockOffset = pVM->patm.s.offPatchMem;

    rc = patmPatchGenGlobalFunctions(pVM, &pVM->patm.s.pGlobalPatchRec->patch);
    AssertRC(rc);

    /* Update free pointer in patch memory. */
    pVM->patm.s.offPatchMem += pVM->patm.s.pGlobalPatchRec->patch.uCurPatchOffset;
    /* Round to next 8 byte boundary. */
    pVM->patm.s.offPatchMem = RT_ALIGN_32(pVM->patm.s.offPatchMem, 8);
    return rc;
}


/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * The PATM will update the addresses used by the switcher.
 *
 * @param   pVM     The VM.
 */
VMMR3DECL(void) PATMR3Relocate(PVM pVM)
{
    RTRCPTR     GCPtrNew = MMHyperR3ToRC(pVM, pVM->patm.s.pGCStateHC);
    RTRCINTPTR  delta = GCPtrNew - pVM->patm.s.pGCStateGC;

    Log(("PATMR3Relocate from %RRv to %RRv - delta %08X\n", pVM->patm.s.pGCStateGC, GCPtrNew, delta));
    if (delta)
    {
        PCPUMCTX pCtx;

        /* Update CPUMCTX guest context pointer. */
        pVM->patm.s.pCPUMCtxGC   += delta;

        pVM->patm.s.deltaReloc = delta;

        RTAvloU32DoWithAll(&pVM->patm.s.PatchLookupTreeHC->PatchTree, true, RelocatePatches, (void *)pVM);

        pCtx = CPUMQueryGuestCtxPtr(VMMGetCpu0(pVM));

        /* If we are running patch code right now, then also adjust EIP. */
        if (PATMIsPatchGCAddr(pVM, pCtx->eip))
            pCtx->eip += delta;

        pVM->patm.s.pGCStateGC  = GCPtrNew;
        pVM->patm.s.pPatchMemGC = MMHyperR3ToRC(pVM, pVM->patm.s.pPatchMemHC);

        pVM->patm.s.pGCStackGC  = MMHyperR3ToRC(pVM, pVM->patm.s.pGCStackHC);

        pVM->patm.s.pStatsGC    = MMHyperR3ToRC(pVM, pVM->patm.s.pStatsHC);

        pVM->patm.s.PatchLookupTreeGC = MMHyperR3ToRC(pVM, pVM->patm.s.PatchLookupTreeHC);

        if (pVM->patm.s.pfnSysEnterPatchGC)
            pVM->patm.s.pfnSysEnterPatchGC += delta;

        /* Deal with the global patch functions. */
        pVM->patm.s.pfnHelperCallGC += delta;
        pVM->patm.s.pfnHelperRetGC  += delta;
        pVM->patm.s.pfnHelperIretGC += delta;
        pVM->patm.s.pfnHelperJumpGC += delta;

        RelocatePatches(&pVM->patm.s.pGlobalPatchRec->Core, (void *)pVM);
    }
}


/**
 * Terminates the PATM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) PATMR3Term(PVM pVM)
{
    /* Memory was all allocated from the two MM heaps and requires no freeing. */
    return VINF_SUCCESS;
}


/**
 * PATM reset callback.
 *
 * @returns VBox status code.
 * @param   pVM     The VM which is reset.
 */
VMMR3DECL(int) PATMR3Reset(PVM pVM)
{
    Log(("PATMR3Reset\n"));

    /* Free all patches. */
    while (true)
    {
        PPATMPATCHREC pPatchRec = (PPATMPATCHREC)RTAvloU32RemoveBestFit(&pVM->patm.s.PatchLookupTreeHC->PatchTree, 0, true);
        if (pPatchRec)
        {
            PATMRemovePatch(pVM, pPatchRec, true);
        }
        else
            break;
    }
    Assert(!pVM->patm.s.PatchLookupTreeHC->PatchTreeByPage);
    Assert(!pVM->patm.s.PatchLookupTreeHC->PatchTree);
    pVM->patm.s.PatchLookupTreeHC->PatchTreeByPatchAddr = 0;
    pVM->patm.s.PatchLookupTreeHC->PatchTreeByPage      = 0;

    int rc = patmReinit(pVM);
    if (RT_SUCCESS(rc))
        rc = PATMR3InitFinalize(pVM); /* paranoia */

    return rc;
}

/**
 * Read callback for disassembly function; supports reading bytes that cross a page boundary
 *
 * @returns VBox status code.
 * @param   pSrc        GC source pointer
 * @param   pDest       HC destination pointer
 * @param   size        Number of bytes to read
 * @param   pvUserdata  Callback specific user data (pCpu)
 *
 */
int patmReadBytes(RTUINTPTR pSrc, uint8_t *pDest, unsigned size, void *pvUserdata)
{
    DISCPUSTATE  *pCpu     = (DISCPUSTATE *)pvUserdata;
    PATMDISASM   *pDisInfo = (PATMDISASM *)pCpu->apvUserData[0];
    int           orgsize  = size;

    Assert(size);
    if (size == 0)
        return VERR_INVALID_PARAMETER;

    /*
     * Trap/interrupt handler typically call common code on entry. Which might already have patches inserted.
     * As we currently don't support calling patch code from patch code, we'll let it read the original opcode bytes instead.
     */
    /** @todo could change in the future! */
    if (pDisInfo->fReadFlags & PATMREAD_ORGCODE)
    {
        for (int i=0;i<orgsize;i++)
        {
            int rc = PATMR3QueryOpcode(pDisInfo->pVM, (RTRCPTR)pSrc, pDest);
            if (RT_SUCCESS(rc))
            {
                pSrc++;
                pDest++;
                size--;
            }
            else break;
        }
        if (size == 0)
            return VINF_SUCCESS;
#ifdef VBOX_STRICT
        if (    !(pDisInfo->pPatchInfo->flags & (PATMFL_DUPLICATE_FUNCTION|PATMFL_IDTHANDLER))
            &&  !(pDisInfo->fReadFlags & PATMREAD_NOCHECK))
        {
            Assert(PATMR3IsInsidePatchJump(pDisInfo->pVM, pSrc, NULL) == false);
            Assert(PATMR3IsInsidePatchJump(pDisInfo->pVM, pSrc+size-1, NULL) == false);
        }
#endif
    }


    if (PAGE_ADDRESS(pDisInfo->pInstrGC) != PAGE_ADDRESS(pSrc + size - 1) && !PATMIsPatchGCAddr(pDisInfo->pVM, pSrc))
    {
        return PGMPhysSimpleReadGCPtr(&pDisInfo->pVM->aCpus[0], pDest, pSrc, size);
    }
    else
    {
        uint8_t *pInstrHC = pDisInfo->pInstrHC;

        Assert(pInstrHC);

        /* pInstrHC is the base address; adjust according to the GC pointer. */
        pInstrHC = pInstrHC + (pSrc - pDisInfo->pInstrGC);

        memcpy(pDest, (void *)pInstrHC, size);
    }

    return VINF_SUCCESS;
}

/**
 * Callback function for RTAvloU32DoWithAll
 *
 * Updates all fixups in the patches
 *
 * @returns VBox status code.
 * @param   pNode       Current node
 * @param   pParam      The VM to operate on.
 */
static DECLCALLBACK(int) RelocatePatches(PAVLOU32NODECORE pNode, void *pParam)
{
    PPATMPATCHREC   pPatch = (PPATMPATCHREC)pNode;
    PVM             pVM = (PVM)pParam;
    RTRCINTPTR      delta;
#ifdef LOG_ENABLED
    DISCPUSTATE     cpu;
    char            szOutput[256];
    uint32_t        opsize;
    bool            disret;
#endif
    int             rc;

    /* Nothing to do if the patch is not active. */
    if (pPatch->patch.uState == PATCH_REFUSED)
        return 0;

#ifdef LOG_ENABLED
    if (pPatch->patch.flags & PATMFL_PATCHED_GUEST_CODE)
    {
        /** @note pPrivInstrHC is probably not valid anymore */
        rc = PGMPhysGCPtr2R3Ptr(VMMGetCpu0(pVM), pPatch->patch.pPrivInstrGC, (PRTR3PTR)&pPatch->patch.pPrivInstrHC);
        if (rc == VINF_SUCCESS)
        {
            cpu.mode = (pPatch->patch.flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;
            disret = PATMR3DISInstr(pVM, &pPatch->patch, &cpu, pPatch->patch.pPrivInstrGC, pPatch->patch.pPrivInstrHC, &opsize, szOutput, PATMREAD_RAWCODE);
            Log(("Org patch jump: %s", szOutput));
        }
    }
#endif

    Log(("Nr of fixups %d\n", pPatch->patch.nrFixups));
    delta = (RTRCINTPTR)pVM->patm.s.deltaReloc;

    /*
     * Apply fixups
     */
    PRELOCREC pRec = 0;
    AVLPVKEY  key  = 0;

    while (true)
    {
        /* Get the record that's closest from above */
        pRec = (PRELOCREC)RTAvlPVGetBestFit(&pPatch->patch.FixupTree, key, true);
        if (pRec == 0)
            break;

        key = (AVLPVKEY)(pRec->pRelocPos + 1);   /* search for the next record during the next round. */

        switch (pRec->uType)
        {
        case FIXUP_ABSOLUTE:
            Log(("Absolute fixup at %RRv %RHv -> %RHv at %RRv\n", pRec->pSource, *(RTRCUINTPTR *)pRec->pRelocPos, *(RTRCINTPTR*)pRec->pRelocPos + delta, pRec->pRelocPos));
            if (!pRec->pSource || PATMIsPatchGCAddr(pVM, pRec->pSource))
            {
                *(RTRCUINTPTR *)pRec->pRelocPos += delta;
            }
            else
            {
                uint8_t curInstr[15];
                uint8_t oldInstr[15];
                Assert(pRec->pSource && pPatch->patch.cbPrivInstr <= 15);

                Assert(!(pPatch->patch.flags & PATMFL_GLOBAL_FUNCTIONS));

                memcpy(oldInstr, pPatch->patch.aPrivInstr, pPatch->patch.cbPrivInstr);
                *(RTRCPTR *)&oldInstr[pPatch->patch.cbPrivInstr - sizeof(RTRCPTR)] = pRec->pDest;

                rc = PGMPhysSimpleReadGCPtr(VMMGetCpu0(pVM), curInstr, pPatch->patch.pPrivInstrGC, pPatch->patch.cbPrivInstr);
                Assert(RT_SUCCESS(rc) || rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT);

                pRec->pDest = (RTRCPTR)((RTRCUINTPTR)pRec->pDest + delta);

                if (rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT)
                {
                    RTRCPTR pPage = pPatch->patch.pPrivInstrGC & PAGE_BASE_GC_MASK;

                    Log(("PATM: Patch page not present -> check later!\n"));
                    rc = PGMR3HandlerVirtualRegister(pVM, PGMVIRTHANDLERTYPE_ALL, pPage, pPage + (PAGE_SIZE - 1) /* inclusive! */, 0, patmVirtPageHandler, "PATMGCMonitorPage", 0, "PATMMonitorPatchJump");
                    Assert(RT_SUCCESS(rc) || rc == VERR_PGM_HANDLER_VIRTUAL_CONFLICT);
                }
                else
                if (memcmp(curInstr, oldInstr, pPatch->patch.cbPrivInstr))
                {
                    Log(("PATM: Patch was overwritten -> disabling patch!!\n"));
                    /*
                     * Disable patch; this is not a good solution
                     */
                     /* @todo hopefully it was completely overwritten (if the read was successful)!!!! */
                    pPatch->patch.uState = PATCH_DISABLED;
                }
                else
                if (RT_SUCCESS(rc))
                {
                    *(RTRCPTR *)&curInstr[pPatch->patch.cbPrivInstr - sizeof(RTRCPTR)] = pRec->pDest;
                    rc = PGMPhysSimpleDirtyWriteGCPtr(VMMGetCpu0(pVM), pRec->pSource, curInstr, pPatch->patch.cbPrivInstr);
                    AssertRC(rc);
                }
            }
            break;

        case FIXUP_REL_JMPTOPATCH:
        {
            RTRCPTR pTarget = (RTRCPTR)((RTRCINTPTR)pRec->pDest + delta);

            if (    pPatch->patch.uState == PATCH_ENABLED
                &&  (pPatch->patch.flags & PATMFL_PATCHED_GUEST_CODE))
            {
                uint8_t    oldJump[SIZEOF_NEAR_COND_JUMP32];
                uint8_t    temp[SIZEOF_NEAR_COND_JUMP32];
                RTRCPTR    pJumpOffGC;
                RTRCINTPTR displ   = (RTRCINTPTR)pTarget - (RTRCINTPTR)pRec->pSource;
                RTRCINTPTR displOld= (RTRCINTPTR)pRec->pDest - (RTRCINTPTR)pRec->pSource;

                Log(("Relative fixup (g2p) %08X -> %08X at %08X (source=%08x, target=%08x)\n", *(int32_t*)pRec->pRelocPos, displ, pRec->pRelocPos, pRec->pSource, pRec->pDest));

                Assert(pRec->pSource - pPatch->patch.cbPatchJump == pPatch->patch.pPrivInstrGC);
#ifdef PATM_RESOLVE_CONFLICTS_WITH_JUMP_PATCHES
                if (pPatch->patch.cbPatchJump == SIZEOF_NEAR_COND_JUMP32)
                {
                    Assert(pPatch->patch.flags & PATMFL_JUMP_CONFLICT);

                    pJumpOffGC = pPatch->patch.pPrivInstrGC + 2;    //two byte opcode
                    oldJump[0] = pPatch->patch.aPrivInstr[0];
                    oldJump[1] = pPatch->patch.aPrivInstr[1];
                    *(RTRCUINTPTR *)&oldJump[2] = displOld;
                }
                else
#endif
                if (pPatch->patch.cbPatchJump == SIZEOF_NEARJUMP32)
                {
                    pJumpOffGC = pPatch->patch.pPrivInstrGC + 1;    //one byte opcode
                    oldJump[0] = 0xE9;
                    *(RTRCUINTPTR *)&oldJump[1] = displOld;
                }
                else
                {
                    AssertMsgFailed(("Invalid patch jump size %d\n", pPatch->patch.cbPatchJump));
                    continue;   //this should never happen!!
                }
                Assert(pPatch->patch.cbPatchJump <= sizeof(temp));

                /*
                 * Read old patch jump and compare it to the one we previously installed
                 */
                rc = PGMPhysSimpleReadGCPtr(VMMGetCpu0(pVM), temp, pPatch->patch.pPrivInstrGC, pPatch->patch.cbPatchJump);
                Assert(RT_SUCCESS(rc) || rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT);

                if (rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT)
                {
                    RTRCPTR pPage = pPatch->patch.pPrivInstrGC & PAGE_BASE_GC_MASK;

                    rc = PGMR3HandlerVirtualRegister(pVM, PGMVIRTHANDLERTYPE_ALL, pPage, pPage + (PAGE_SIZE - 1) /* inclusive! */, 0, patmVirtPageHandler, "PATMGCMonitorPage", 0, "PATMMonitorPatchJump");
                    Assert(RT_SUCCESS(rc) || rc == VERR_PGM_HANDLER_VIRTUAL_CONFLICT);
                }
                else
                if (memcmp(temp, oldJump, pPatch->patch.cbPatchJump))
                {
                    Log(("PATM: Patch jump was overwritten -> disabling patch!!\n"));
                    /*
                     * Disable patch; this is not a good solution
                     */
                     /* @todo hopefully it was completely overwritten (if the read was successful)!!!! */
                    pPatch->patch.uState = PATCH_DISABLED;
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
                Log(("Skip the guest jump to patch code for this disabled patch %08X - %08X\n", pPatch->patch.pPrivInstrHC, pRec->pRelocPos));
            }

            pRec->pDest = pTarget;
            break;
        }

        case FIXUP_REL_JMPTOGUEST:
        {
            RTRCPTR    pSource = (RTRCPTR)((RTRCINTPTR)pRec->pSource + delta);
            RTRCINTPTR displ   = (RTRCINTPTR)pRec->pDest - (RTRCINTPTR)pSource;

            Assert(!(pPatch->patch.flags & PATMFL_GLOBAL_FUNCTIONS));
            Log(("Relative fixup (p2g) %08X -> %08X at %08X (source=%08x, target=%08x)\n", *(int32_t*)pRec->pRelocPos, displ, pRec->pRelocPos, pRec->pSource, pRec->pDest));
            *(RTRCUINTPTR *)pRec->pRelocPos = displ;
            pRec->pSource = pSource;
            break;
        }

        default:
            AssertMsg(0, ("Invalid fixup type!!\n"));
            return VERR_INVALID_PARAMETER;
        }
    }

#ifdef LOG_ENABLED
    if (pPatch->patch.flags & PATMFL_PATCHED_GUEST_CODE)
    {
        /** @note pPrivInstrHC is probably not valid anymore */
        rc = PGMPhysGCPtr2R3Ptr(VMMGetCpu0(pVM), pPatch->patch.pPrivInstrGC, (PRTR3PTR)&pPatch->patch.pPrivInstrHC);
        if (rc == VINF_SUCCESS)
        {
            cpu.mode = (pPatch->patch.flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;
            disret = PATMR3DISInstr(pVM, &pPatch->patch, &cpu, pPatch->patch.pPrivInstrGC, pPatch->patch.pPrivInstrHC, &opsize, szOutput, PATMREAD_RAWCODE);
            Log(("Rel patch jump: %s", szOutput));
        }
    }
#endif
    return 0;
}

/**
 * #PF Handler callback for virtual access handler ranges.
 *
 * Important to realize that a physical page in a range can have aliases, and
 * for ALL and WRITE handlers these will also trigger.
 *
 * @returns VINF_SUCCESS if the handler have carried out the operation.
 * @returns VINF_PGM_HANDLER_DO_DEFAULT if the caller should carry out the access operation.
 * @param   pVM             VM Handle.
 * @param   GCPtr           The virtual address the guest is writing to. (not correct if it's an alias!)
 * @param   pvPtr           The HC mapping of that address.
 * @param   pvBuf           What the guest is reading/writing.
 * @param   cbBuf           How much it's reading/writing.
 * @param   enmAccessType   The access type.
 * @param   pvUser          User argument.
 */
DECLCALLBACK(int) patmVirtPageHandler(PVM pVM, RTGCPTR GCPtr, void *pvPtr, void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType, void *pvUser)
{
    Assert(enmAccessType == PGMACCESSTYPE_WRITE);
    /** @todo could be the wrong virtual address (alias) */
    pVM->patm.s.pvFaultMonitor = GCPtr;
    PATMR3HandleMonitoredPage(pVM);
    return VINF_PGM_HANDLER_DO_DEFAULT;
}


#ifdef VBOX_WITH_DEBUGGER
/**
 * Callback function for RTAvloU32DoWithAll
 *
 * Enables the patch that's being enumerated
 *
 * @returns 0 (continue enumeration).
 * @param   pNode       Current node
 * @param   pVM         The VM to operate on.
 */
static DECLCALLBACK(int) EnableAllPatches(PAVLOU32NODECORE pNode, void *pVM)
{
    PPATMPATCHREC pPatch = (PPATMPATCHREC)pNode;

    PATMR3EnablePatch((PVM)pVM, (RTRCPTR)pPatch->Core.Key);
    return 0;
}
#endif /* VBOX_WITH_DEBUGGER */


#ifdef VBOX_WITH_DEBUGGER
/**
 * Callback function for RTAvloU32DoWithAll
 *
 * Disables the patch that's being enumerated
 *
 * @returns 0 (continue enumeration).
 * @param   pNode       Current node
 * @param   pVM         The VM to operate on.
 */
static DECLCALLBACK(int) DisableAllPatches(PAVLOU32NODECORE pNode, void *pVM)
{
    PPATMPATCHREC pPatch = (PPATMPATCHREC)pNode;

    PATMR3DisablePatch((PVM)pVM, (RTRCPTR)pPatch->Core.Key);
    return 0;
}
#endif

/**
 * Returns the host context pointer and size of the patch memory block
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pcb         Size of the patch memory block
 */
VMMR3DECL(void *) PATMR3QueryPatchMemHC(PVM pVM, uint32_t *pcb)
{
    if (pcb)
    {
        *pcb = pVM->patm.s.cbPatchMem;
    }
    return pVM->patm.s.pPatchMemHC;
}


/**
 * Returns the guest context pointer and size of the patch memory block
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pcb         Size of the patch memory block
 */
VMMR3DECL(RTRCPTR) PATMR3QueryPatchMemGC(PVM pVM, uint32_t *pcb)
{
    if (pcb)
    {
        *pcb = pVM->patm.s.cbPatchMem;
    }
    return pVM->patm.s.pPatchMemGC;
}


/**
 * Returns the host context pointer of the GC context structure
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(PPATMGCSTATE) PATMR3QueryGCStateHC(PVM pVM)
{
    return pVM->patm.s.pGCStateHC;
}


/**
 * Checks whether the HC address is part of our patch region
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pAddrGC     Guest context address
 */
VMMR3DECL(bool) PATMR3IsPatchHCAddr(PVM pVM, R3PTRTYPE(uint8_t *) pAddrHC)
{
    return (pAddrHC >= pVM->patm.s.pPatchMemHC && pAddrHC < pVM->patm.s.pPatchMemHC + pVM->patm.s.cbPatchMem) ? true : false;
}


/**
 * Allows or disallow patching of privileged instructions executed by the guest OS
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   fAllowPatching Allow/disallow patching
 */
VMMR3DECL(int) PATMR3AllowPatching(PVM pVM, uint32_t fAllowPatching)
{
    pVM->fPATMEnabled = (fAllowPatching) ? true : false;
    return VINF_SUCCESS;
}

/**
 * Convert a GC patch block pointer to a HC patch pointer
 *
 * @returns HC pointer or NULL if it's not a GC patch pointer
 * @param   pVM         The VM to operate on.
 * @param   pAddrGC     GC pointer
 */
VMMR3DECL(R3PTRTYPE(void *)) PATMR3GCPtrToHCPtr(PVM pVM, RTRCPTR pAddrGC)
{
    if (pVM->patm.s.pPatchMemGC <= pAddrGC && pVM->patm.s.pPatchMemGC + pVM->patm.s.cbPatchMem > pAddrGC)
    {
        return pVM->patm.s.pPatchMemHC + (pAddrGC - pVM->patm.s.pPatchMemGC);
    }
    return NULL;
}

/**
 * Query PATM state (enabled/disabled)
 *
 * @returns 0 - disabled, 1 - enabled
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) PATMR3IsEnabled(PVM pVM)
{
    return pVM->fPATMEnabled;
}


/**
 * Convert guest context address to host context pointer
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch block structure pointer
 * @param   pGCPtr      Guest context pointer
 *
 * @returns             Host context pointer or NULL in case of an error
 *
 */
R3PTRTYPE(uint8_t *) PATMGCVirtToHCVirt(PVM pVM, PPATCHINFO pPatch, RCPTRTYPE(uint8_t *) pGCPtr)
{
    int rc;
    R3PTRTYPE(uint8_t *) pHCPtr;
    uint32_t offset;

    if (PATMIsPatchGCAddr(pVM, pGCPtr))
    {
        return PATCHCODE_PTR_HC(pPatch) + (pGCPtr - PATCHCODE_PTR_GC(pPatch));
    }

    offset = pGCPtr & PAGE_OFFSET_MASK;
    if (pPatch->cacheRec.pGuestLoc == (pGCPtr & PAGE_BASE_GC_MASK))
    {
        return pPatch->cacheRec.pPatchLocStartHC + offset;
    }

    rc = PGMPhysGCPtr2R3Ptr(VMMGetCpu0(pVM), pGCPtr, (void **)&pHCPtr);
    if (rc != VINF_SUCCESS)
    {
        AssertMsg(rc == VINF_SUCCESS || rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT, ("MMR3PhysGCVirt2HCVirtEx failed for %08X\n", pGCPtr));
        return NULL;
    }
////invalid?    Assert(sizeof(R3PTRTYPE(uint8_t*)) == sizeof(uint32_t));

    pPatch->cacheRec.pPatchLocStartHC = (R3PTRTYPE(uint8_t*))((RTHCUINTPTR)pHCPtr & PAGE_BASE_HC_MASK);
    pPatch->cacheRec.pGuestLoc        = pGCPtr & PAGE_BASE_GC_MASK;
    return pHCPtr;
}


/* Calculates and fills in all branch targets
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Current patch block pointer
 *
 */
static int patmr3SetBranchTargets(PVM pVM, PPATCHINFO pPatch)
{
    int32_t displ;

    PJUMPREC pRec = 0;
    int      nrJumpRecs = 0;

    /*
     * Set all branch targets inside the patch block.
     * We remove all jump records as they are no longer needed afterwards.
     */
    while (true)
    {
        RCPTRTYPE(uint8_t *) pInstrGC;
        RCPTRTYPE(uint8_t *) pBranchTargetGC = 0;

        pRec = (PJUMPREC)RTAvlPVRemoveBestFit(&pPatch->JumpTree, 0, true);
        if (pRec == 0)
            break;

        nrJumpRecs++;

        /* HC in patch block to GC in patch block. */
        pInstrGC = patmPatchHCPtr2PatchGCPtr(pVM, pRec->pJumpHC);

        if (pRec->opcode == OP_CALL)
        {
            /* Special case: call function replacement patch from this patch block.
             */
            PPATMPATCHREC pFunctionRec = PATMQueryFunctionPatch(pVM, pRec->pTargetGC);
            if (!pFunctionRec)
            {
                int rc;

                if (PATMR3HasBeenPatched(pVM, pRec->pTargetGC) == false)
                    rc = PATMR3InstallPatch(pVM, pRec->pTargetGC, PATMFL_CODE32 | PATMFL_DUPLICATE_FUNCTION);
                else
                    rc = VERR_PATCHING_REFUSED;    /* exists as a normal patch; can't use it */

                if (RT_FAILURE(rc))
                {
                    uint8_t *pPatchHC;
                    RTRCPTR  pPatchGC;
                    RTRCPTR  pOrgInstrGC;

                    pOrgInstrGC = PATMR3PatchToGCPtr(pVM, pInstrGC, 0);
                    Assert(pOrgInstrGC);

                    /* Failure for some reason -> mark exit point with int 3. */
                    Log(("Failed to install function replacement patch (at %x) for reason %Rrc\n", pOrgInstrGC, rc));

                    pPatchGC = patmGuestGCPtrToPatchGCPtr(pVM, pPatch, pOrgInstrGC);
                    Assert(pPatchGC);

                    pPatchHC = pVM->patm.s.pPatchMemHC + (pPatchGC - pVM->patm.s.pPatchMemGC);

                    /* Set a breakpoint at the very beginning of the recompiled instruction */
                    *pPatchHC = 0xCC;

                    continue;
                }
            }
            else
            {
                Log(("Patch block %RRv called as function\n", pFunctionRec->patch.pPrivInstrGC));
                pFunctionRec->patch.flags |= PATMFL_CODE_REFERENCED;
            }

            pBranchTargetGC = PATMR3QueryPatchGCPtr(pVM, pRec->pTargetGC);
        }
        else
        {
            pBranchTargetGC = patmGuestGCPtrToPatchGCPtr(pVM, pPatch, pRec->pTargetGC);
        }

        if (pBranchTargetGC == 0)
        {
            AssertMsgFailed(("patmr3SetBranchTargets: patmGuestGCPtrToPatchGCPtr failed for %08X\n", pRec->pTargetGC));
            return VERR_PATCHING_REFUSED;
        }
        /* Our jumps *always* have a dword displacement (to make things easier). */
        Assert(sizeof(uint32_t) == sizeof(RTRCPTR));
        displ =  pBranchTargetGC - (pInstrGC + pRec->offDispl + sizeof(RTRCPTR));
        *(RTRCPTR *)(pRec->pJumpHC + pRec->offDispl) = displ;
        Log(("Set branch target %d to %08X : %08x - (%08x + %d + %d)\n", nrJumpRecs, displ, pBranchTargetGC, pInstrGC, pRec->offDispl, sizeof(RTRCPTR)));
    }
    Assert(nrJumpRecs == pPatch->nrJumpRecs);
    Assert(pPatch->JumpTree == 0);
    return VINF_SUCCESS;
}

/* Add an illegal instruction record
 *
 * @param   pVM             The VM to operate on.
 * @param   pPatch          Patch structure ptr
 * @param   pInstrGC        Guest context pointer to privileged instruction
 *
 */
static void patmAddIllegalInstrRecord(PVM pVM, PPATCHINFO pPatch, RTRCPTR pInstrGC)
{
    PAVLPVNODECORE pRec;

    pRec = (PAVLPVNODECORE)MMR3HeapAllocZ(pVM, MM_TAG_PATM_PATCH, sizeof(*pRec));
    Assert(pRec);
    pRec->Key = (AVLPVKEY)pInstrGC;

    bool ret = RTAvlPVInsert(&pPatch->pTempInfo->IllegalInstrTree, pRec);
    Assert(ret); NOREF(ret);
    pPatch->pTempInfo->nrIllegalInstr++;
}

static bool patmIsIllegalInstr(PPATCHINFO pPatch, RTRCPTR pInstrGC)
{
    PAVLPVNODECORE pRec;

    pRec = RTAvlPVGet(&pPatch->pTempInfo->IllegalInstrTree, (AVLPVKEY)pInstrGC);
    if (pRec)
        return true;
    return false;
}

/**
 * Add a patch to guest lookup record
 *
 * @param   pVM             The VM to operate on.
 * @param   pPatch          Patch structure ptr
 * @param   pPatchInstrHC   Guest context pointer to patch block
 * @param   pInstrGC        Guest context pointer to privileged instruction
 * @param   enmType         Lookup type
 * @param   fDirty          Dirty flag
 *
 */
 /** @note Be extremely careful with this function. Make absolutely sure the guest address is correct! (to avoid executing instructions twice!) */
void patmr3AddP2GLookupRecord(PVM pVM, PPATCHINFO pPatch, uint8_t *pPatchInstrHC, RTRCPTR pInstrGC, PATM_LOOKUP_TYPE enmType, bool fDirty)
{
    bool ret;
    PRECPATCHTOGUEST pPatchToGuestRec;
    PRECGUESTTOPATCH pGuestToPatchRec;
    uint32_t PatchOffset = pPatchInstrHC - pVM->patm.s.pPatchMemHC;  /* Offset in memory reserved for PATM. */

    if (enmType == PATM_LOOKUP_PATCH2GUEST)
    {
        pPatchToGuestRec = (PRECPATCHTOGUEST)RTAvlU32Get(&pPatch->Patch2GuestAddrTree, PatchOffset);
        if (pPatchToGuestRec && pPatchToGuestRec->Core.Key == PatchOffset)
            return; /* already there */

        Assert(!pPatchToGuestRec);
    }
#ifdef VBOX_STRICT
    else
    {
        pPatchToGuestRec = (PRECPATCHTOGUEST)RTAvlU32Get(&pPatch->Patch2GuestAddrTree, PatchOffset);
        Assert(!pPatchToGuestRec);
    }
#endif

    pPatchToGuestRec = (PRECPATCHTOGUEST)MMR3HeapAllocZ(pVM, MM_TAG_PATM_PATCH, sizeof(RECPATCHTOGUEST) + sizeof(RECGUESTTOPATCH));
    Assert(pPatchToGuestRec);
    pPatchToGuestRec->Core.Key    = PatchOffset;
    pPatchToGuestRec->pOrgInstrGC = pInstrGC;
    pPatchToGuestRec->enmType     = enmType;
    pPatchToGuestRec->fDirty     = fDirty;

    ret = RTAvlU32Insert(&pPatch->Patch2GuestAddrTree, &pPatchToGuestRec->Core);
    Assert(ret);

    /* GC to patch address */
    if (enmType == PATM_LOOKUP_BOTHDIR)
    {
        pGuestToPatchRec = (PRECGUESTTOPATCH)RTAvlU32Get(&pPatch->Guest2PatchAddrTree, pInstrGC);
        if (!pGuestToPatchRec)
        {
            pGuestToPatchRec = (PRECGUESTTOPATCH)(pPatchToGuestRec+1);
            pGuestToPatchRec->Core.Key    = pInstrGC;
            pGuestToPatchRec->PatchOffset = PatchOffset;

            ret = RTAvlU32Insert(&pPatch->Guest2PatchAddrTree, &pGuestToPatchRec->Core);
            Assert(ret);
        }
    }

    pPatch->nrPatch2GuestRecs++;
}


/**
 * Removes a patch to guest lookup record
 *
 * @param   pVM             The VM to operate on.
 * @param   pPatch          Patch structure ptr
 * @param   pPatchInstrGC   Guest context pointer to patch block
 */
void patmr3RemoveP2GLookupRecord(PVM pVM, PPATCHINFO pPatch, RTRCPTR pPatchInstrGC)
{
    PAVLU32NODECORE     pNode;
    PAVLU32NODECORE     pNode2;
    PRECPATCHTOGUEST    pPatchToGuestRec;
    uint32_t            PatchOffset = pPatchInstrGC - pVM->patm.s.pPatchMemGC;  /* Offset in memory reserved for PATM. */

    pPatchToGuestRec = (PRECPATCHTOGUEST)RTAvlU32Get(&pPatch->Patch2GuestAddrTree, PatchOffset);
    Assert(pPatchToGuestRec);
    if (pPatchToGuestRec)
    {
        if (pPatchToGuestRec->enmType == PATM_LOOKUP_BOTHDIR)
        {
            PRECGUESTTOPATCH pGuestToPatchRec = (PRECGUESTTOPATCH)(pPatchToGuestRec+1);

            Assert(pGuestToPatchRec->Core.Key);
            pNode2 = RTAvlU32Remove(&pPatch->Guest2PatchAddrTree, pGuestToPatchRec->Core.Key);
            Assert(pNode2);
        }
        pNode = RTAvlU32Remove(&pPatch->Patch2GuestAddrTree, pPatchToGuestRec->Core.Key);
        Assert(pNode);

        MMR3HeapFree(pPatchToGuestRec);
        pPatch->nrPatch2GuestRecs--;
    }
}


/**
 * RTAvlPVDestroy callback.
 */
static DECLCALLBACK(int) patmEmptyTreePVCallback(PAVLPVNODECORE pNode, void *)
{
    MMR3HeapFree(pNode);
    return 0;
}

/**
 * Empty the specified tree (PV tree, MMR3 heap)
 *
 * @param   pVM             The VM to operate on.
 * @param   ppTree          Tree to empty
 */
void patmEmptyTree(PVM pVM, PAVLPVNODECORE *ppTree)
{
    RTAvlPVDestroy(ppTree, patmEmptyTreePVCallback, NULL);
}


/**
 * RTAvlU32Destroy callback.
 */
static DECLCALLBACK(int) patmEmptyTreeU32Callback(PAVLU32NODECORE pNode, void *)
{
    MMR3HeapFree(pNode);
    return 0;
}

/**
 * Empty the specified tree (U32 tree, MMR3 heap)
 *
 * @param   pVM             The VM to operate on.
 * @param   ppTree          Tree to empty
 */
void patmEmptyTreeU32(PVM pVM, PPAVLU32NODECORE ppTree)
{
    RTAvlU32Destroy(ppTree, patmEmptyTreeU32Callback, NULL);
}


/**
 * Analyses the instructions following the cli for compliance with our heuristics for cli & pushf
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pCpu        CPU disassembly state
 * @param   pInstrGC    Guest context pointer to privileged instruction
 * @param   pCurInstrGC Guest context pointer to the current instruction
 * @param   pUserData   User pointer (callback specific)
 *
 */
static int patmAnalyseBlockCallback(PVM pVM, DISCPUSTATE *pCpu, RCPTRTYPE(uint8_t *) pInstrGC, RCPTRTYPE(uint8_t *) pCurInstrGC, void *pUserData)
{
    PPATCHINFO pPatch = (PPATCHINFO)pUserData;
    bool       fIllegalInstr = false;

    //Preliminary heuristics:
    //- no call instructions without a fixed displacement between cli and sti/popf
    //- no jumps in the instructions following cli (4+ bytes; enough for the replacement jump (5 bytes))
    //- no nested pushf/cli
    //- sti/popf should be the (eventual) target of all branches
    //- no near or far returns; no int xx, no into
    //
    // Note: Later on we can impose less stricter guidelines if the need arises

    /* Bail out if the patch gets too big. */
    if (pPatch->cbPatchBlockSize >= MAX_PATCH_SIZE)
    {
        Log(("Code block too big (%x) for patch at %RRv!!\n", pPatch->cbPatchBlockSize, pCurInstrGC));
        fIllegalInstr = true;
        patmAddIllegalInstrRecord(pVM, pPatch, pCurInstrGC);
    }
    else
    {
        /* No unconditinal jumps or calls without fixed displacements. */
        if (    (pCpu->pCurInstr->optype & OPTYPE_CONTROLFLOW)
             && (pCpu->pCurInstr->opcode == OP_JMP || pCpu->pCurInstr->opcode == OP_CALL)
           )
        {
            Assert(pCpu->param1.size <= 4 || pCpu->param1.size == 6);
            if (    pCpu->param1.size == 6 /* far call/jmp */
                ||  (pCpu->pCurInstr->opcode == OP_CALL && !(pPatch->flags & PATMFL_SUPPORT_CALLS))
                ||  (OP_PARM_VTYPE(pCpu->pCurInstr->param1) != OP_PARM_J && !(pPatch->flags & PATMFL_SUPPORT_INDIRECT_CALLS))
               )
            {
                fIllegalInstr = true;
                patmAddIllegalInstrRecord(pVM, pPatch, pCurInstrGC);
            }
        }

        /* An unconditional (short) jump right after a cli is a potential problem; we will overwrite whichever function comes afterwards */
        if (pPatch->opcode == OP_CLI && pCpu->pCurInstr->opcode == OP_JMP)
        {
            if (pCurInstrGC > pPatch->pPrivInstrGC && pCurInstrGC + pCpu->opsize < pPatch->pPrivInstrGC + SIZEOF_NEARJUMP32) /* hardcoded patch jump size; cbPatchJump is still zero */
            {
                Log(("Dangerous unconditional jump ends in our generated patch jump!! (%x vs %x)\n", pCurInstrGC, pPatch->pPrivInstrGC));
                /* We turn this one into a int 3 callable patch. */
                pPatch->flags |= PATMFL_INT3_REPLACEMENT_BLOCK;
            }
        }
        else
        /* no nested pushfs just yet; nested cli is allowed for cli patches though. */
        if (pPatch->opcode == OP_PUSHF)
        {
            if (pCurInstrGC != pInstrGC && pCpu->pCurInstr->opcode == OP_PUSHF)
            {
                fIllegalInstr = true;
                patmAddIllegalInstrRecord(pVM, pPatch, pCurInstrGC);
            }
        }

        // no far returns
        if (pCpu->pCurInstr->opcode == OP_RETF)
        {
            pPatch->pTempInfo->nrRetInstr++;
            fIllegalInstr = true;
            patmAddIllegalInstrRecord(pVM, pPatch, pCurInstrGC);
        }
        else
        // no int xx or into either
        if (pCpu->pCurInstr->opcode == OP_INT3 || pCpu->pCurInstr->opcode == OP_INT || pCpu->pCurInstr->opcode == OP_INTO)
        {
            fIllegalInstr = true;
            patmAddIllegalInstrRecord(pVM, pPatch, pCurInstrGC);
        }
    }

    pPatch->cbPatchBlockSize += pCpu->opsize;

    /* Illegal instruction -> end of analysis phase for this code block */
    if (fIllegalInstr || patmIsIllegalInstr(pPatch, pCurInstrGC))
        return VINF_SUCCESS;

    /* Check for exit points. */
    switch (pCpu->pCurInstr->opcode)
    {
    case OP_SYSEXIT:
        return VINF_SUCCESS; /* duplicate it; will fault or emulated in GC. */

    case OP_SYSENTER:
    case OP_ILLUD2:
        //This appears to be some kind of kernel panic in Linux 2.4; no point to analyse more
        Log(("Illegal opcode (0xf 0xb) -> return here\n"));
        return VINF_SUCCESS;

    case OP_STI:
    case OP_POPF:
        Assert(!(pPatch->flags & (PATMFL_DUPLICATE_FUNCTION)));
        /* If out exit point lies within the generated patch jump, then we have to refuse!! */
        if (pCurInstrGC > pPatch->pPrivInstrGC && pCurInstrGC < pPatch->pPrivInstrGC + SIZEOF_NEARJUMP32) /* hardcoded patch jump size; cbPatchJump is still zero */
        {
            Log(("Exit point within patch jump itself!! (%x vs %x)\n", pCurInstrGC, pPatch->pPrivInstrGC));
            return VERR_PATCHING_REFUSED;
        }
        if (pPatch->opcode == OP_PUSHF)
        {
            if (pCpu->pCurInstr->opcode == OP_POPF)
            {
                if (pPatch->cbPatchBlockSize >= SIZEOF_NEARJUMP32)
                    return VINF_SUCCESS;

                /* Or else we need to duplicate more instructions, because we can't jump back yet! */
                Log(("WARNING: End of block reached, but we need to duplicate some extra instruction to avoid a conflict with the patch jump\n"));
                pPatch->flags |= PATMFL_CHECK_SIZE;
            }
            break;  //sti doesn't mark the end of a pushf block; only popf does
        }
        //else no break
    case OP_RETN: /* exit point for function replacement */
        return VINF_SUCCESS;

    case OP_IRET:
        return VINF_SUCCESS;    /* exitpoint */

    case OP_CPUID:
    case OP_CALL:
    case OP_JMP:
        break;

    default:
        if (pCpu->pCurInstr->optype & (OPTYPE_PRIVILEGED_NOTRAP))
        {
            patmAddIllegalInstrRecord(pVM, pPatch, pCurInstrGC);
            return VINF_SUCCESS;    /* exit point */
        }
        break;
    }

    // If single instruction patch, we've copied enough instructions *and* the current instruction is not a relative jump
    if ((pPatch->flags & PATMFL_CHECK_SIZE) && pPatch->cbPatchBlockSize > SIZEOF_NEARJUMP32 && !(pCpu->pCurInstr->optype & OPTYPE_RELATIVE_CONTROLFLOW))
    {
        // The end marker for this kind of patch is any instruction at a location outside our patch jump
        Log(("End of block at %RRv size %d\n", pCurInstrGC, pCpu->opsize));
        return VINF_SUCCESS;
    }

    return VWRN_CONTINUE_ANALYSIS;
}

/**
 * Analyses the instructions inside a function for compliance
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pCpu        CPU disassembly state
 * @param   pInstrGC    Guest context pointer to privileged instruction
 * @param   pCurInstrGC Guest context pointer to the current instruction
 * @param   pUserData   User pointer (callback specific)
 *
 */
static int patmAnalyseFunctionCallback(PVM pVM, DISCPUSTATE *pCpu, RCPTRTYPE(uint8_t *) pInstrGC, RCPTRTYPE(uint8_t *) pCurInstrGC, void *pUserData)
{
    PPATCHINFO pPatch = (PPATCHINFO)pUserData;
    bool       fIllegalInstr = false;

    //Preliminary heuristics:
    //- no call instructions
    //- ret ends a block

    Assert(pPatch->flags & (PATMFL_DUPLICATE_FUNCTION));

    // bail out if the patch gets too big
    if (pPatch->cbPatchBlockSize >= MAX_PATCH_SIZE)
    {
        Log(("Code block too big (%x) for function patch at %RRv!!\n", pPatch->cbPatchBlockSize, pCurInstrGC));
        fIllegalInstr = true;
        patmAddIllegalInstrRecord(pVM, pPatch, pCurInstrGC);
    }
    else
    {
        // no unconditinal jumps or calls without fixed displacements
        if (    (pCpu->pCurInstr->optype & OPTYPE_CONTROLFLOW)
             && (pCpu->pCurInstr->opcode == OP_JMP || pCpu->pCurInstr->opcode == OP_CALL)
           )
        {
            Assert(pCpu->param1.size <= 4 || pCpu->param1.size == 6);
            if (    pCpu->param1.size == 6 /* far call/jmp */
                ||  (pCpu->pCurInstr->opcode == OP_CALL && !(pPatch->flags & PATMFL_SUPPORT_CALLS))
                ||  (OP_PARM_VTYPE(pCpu->pCurInstr->param1) != OP_PARM_J && !(pPatch->flags & PATMFL_SUPPORT_INDIRECT_CALLS))
               )
            {
                fIllegalInstr = true;
                patmAddIllegalInstrRecord(pVM, pPatch, pCurInstrGC);
            }
        }
        else /* no far returns */
        if (pCpu->pCurInstr->opcode == OP_RETF)
        {
            patmAddIllegalInstrRecord(pVM, pPatch, pCurInstrGC);
            fIllegalInstr = true;
        }
        else /* no int xx or into either */
        if (pCpu->pCurInstr->opcode == OP_INT3 || pCpu->pCurInstr->opcode == OP_INT || pCpu->pCurInstr->opcode == OP_INTO)
        {
            patmAddIllegalInstrRecord(pVM, pPatch, pCurInstrGC);
            fIllegalInstr = true;
        }

    #if 0
        ///@todo we can handle certain in/out and privileged instructions in the guest context
        if (pCpu->pCurInstr->optype & OPTYPE_PRIVILEGED && pCpu->pCurInstr->opcode != OP_STI)
        {
            Log(("Illegal instructions for function patch!!\n"));
            return VERR_PATCHING_REFUSED;
        }
    #endif
    }

    pPatch->cbPatchBlockSize += pCpu->opsize;

    /* Illegal instruction -> end of analysis phase for this code block */
    if (fIllegalInstr || patmIsIllegalInstr(pPatch, pCurInstrGC))
    {
        return VINF_SUCCESS;
    }

    // Check for exit points
    switch (pCpu->pCurInstr->opcode)
    {
    case OP_ILLUD2:
        //This appears to be some kind of kernel panic in Linux 2.4; no point to analyse more
        Log(("Illegal opcode (0xf 0xb) -> return here\n"));
        return VINF_SUCCESS;

    case OP_IRET:
    case OP_SYSEXIT: /* will fault or emulated in GC */
    case OP_RETN:
        return VINF_SUCCESS;

    case OP_POPF:
    case OP_STI:
        return VWRN_CONTINUE_ANALYSIS;
    default:
        if (pCpu->pCurInstr->optype & (OPTYPE_PRIVILEGED_NOTRAP))
        {
            patmAddIllegalInstrRecord(pVM, pPatch, pCurInstrGC);
            return VINF_SUCCESS;    /* exit point */
        }
        return VWRN_CONTINUE_ANALYSIS;
    }

    return VWRN_CONTINUE_ANALYSIS;
}

/**
 * Recompiles the instructions in a code block
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pCpu        CPU disassembly state
 * @param   pInstrGC    Guest context pointer to privileged instruction
 * @param   pCurInstrGC Guest context pointer to the current instruction
 * @param   pUserData   User pointer (callback specific)
 *
 */
static int patmRecompileCallback(PVM pVM, DISCPUSTATE *pCpu, RCPTRTYPE(uint8_t *) pInstrGC, RCPTRTYPE(uint8_t *) pCurInstrGC, void *pUserData)
{
    PPATCHINFO pPatch = (PPATCHINFO)pUserData;
    int rc = VINF_SUCCESS;
    bool fInhibitIRQInstr = false;  /* did the instruction cause PATMFL_INHIBITIRQS to be set? */

    LogFlow(("patmRecompileCallback %RRv %RRv\n", pInstrGC, pCurInstrGC));

    if (    patmGuestGCPtrToPatchGCPtr(pVM, pPatch, pCurInstrGC) != 0
        &&  !(pPatch->flags & PATMFL_RECOMPILE_NEXT)) /* do not do this when the next instruction *must* be executed! */
    {
        /*
         * Been there, done that; so insert a jump (we don't want to duplicate code)
         * no need to record this instruction as it's glue code that never crashes (it had better not!)
         */
        Log(("patmRecompileCallback: jump to code we've recompiled before %RRv!\n", pCurInstrGC));
        return patmPatchGenRelJump(pVM, pPatch, pCurInstrGC, OP_JMP, !!(pCpu->prefix & PREFIX_OPSIZE));
    }

    if (pPatch->flags & (PATMFL_DUPLICATE_FUNCTION))
    {
        rc = patmAnalyseFunctionCallback(pVM, pCpu, pInstrGC, pCurInstrGC, pUserData);
    }
    else
        rc = patmAnalyseBlockCallback(pVM, pCpu, pInstrGC, pCurInstrGC, pUserData);

    if (RT_FAILURE(rc))
        return rc;

    /** @note Never do a direct return unless a failure is encountered! */

    /* Clear recompilation of next instruction flag; we are doing that right here. */
    if (pPatch->flags & PATMFL_RECOMPILE_NEXT)
        pPatch->flags &= ~PATMFL_RECOMPILE_NEXT;

    /* Add lookup record for patch to guest address translation */
    patmr3AddP2GLookupRecord(pVM, pPatch, PATCHCODE_PTR_HC(pPatch) + pPatch->uCurPatchOffset, pCurInstrGC, PATM_LOOKUP_BOTHDIR);

    /* Update lowest and highest instruction address for this patch */
    if (pCurInstrGC < pPatch->pInstrGCLowest)
        pPatch->pInstrGCLowest = pCurInstrGC;
    else
    if (pCurInstrGC > pPatch->pInstrGCHighest)
        pPatch->pInstrGCHighest = pCurInstrGC + pCpu->opsize;

    /* Illegal instruction -> end of recompile phase for this code block. */
    if (patmIsIllegalInstr(pPatch, pCurInstrGC))
    {
        Log(("Illegal instruction at %RRv -> mark with int 3\n", pCurInstrGC));
        rc = patmPatchGenIllegalInstr(pVM, pPatch);
        goto end;
    }

    /* For our first attempt, we'll handle only simple relative jumps (immediate offset coded in instruction).
     * Indirect calls are handled below.
     */
    if (   (pCpu->pCurInstr->optype & OPTYPE_CONTROLFLOW)
        && (pCpu->pCurInstr->opcode != OP_CALL || (pPatch->flags & PATMFL_SUPPORT_CALLS))
        && (OP_PARM_VTYPE(pCpu->pCurInstr->param1) == OP_PARM_J))
    {
        RCPTRTYPE(uint8_t *) pTargetGC = PATMResolveBranch(pCpu, pCurInstrGC);
        if (pTargetGC == 0)
        {
            Log(("We don't support far jumps here!! (%08X)\n", pCpu->param1.flags));
            return VERR_PATCHING_REFUSED;
        }

        if (pCpu->pCurInstr->opcode == OP_CALL)
        {
            Assert(!PATMIsPatchGCAddr(pVM, pTargetGC));
            rc = patmPatchGenCall(pVM, pPatch, pCpu, pCurInstrGC, pTargetGC, false);
            if (RT_FAILURE(rc))
                goto end;
        }
        else
            rc = patmPatchGenRelJump(pVM, pPatch, pTargetGC, pCpu->pCurInstr->opcode, !!(pCpu->prefix & PREFIX_OPSIZE));

        if (RT_SUCCESS(rc))
            rc = VWRN_CONTINUE_RECOMPILE;

        goto end;
    }

    switch (pCpu->pCurInstr->opcode)
    {
    case OP_CLI:
    {
        /* If a cli is found while duplicating instructions for another patch, then it's of vital importance to continue
         * until we've found the proper exit point(s).
         */
        if (    pCurInstrGC != pInstrGC
            && !(pPatch->flags & (PATMFL_DUPLICATE_FUNCTION))
           )
        {
            Log(("cli instruction found in other instruction patch block; force it to continue & find an exit point\n"));
            pPatch->flags &= ~(PATMFL_CHECK_SIZE | PATMFL_SINGLE_INSTRUCTION);
        }
        /* Set by irq inhibition; no longer valid now. */
        pPatch->flags &= ~PATMFL_GENERATE_JUMPTOGUEST;

        rc = patmPatchGenCli(pVM, pPatch);
        if (RT_SUCCESS(rc))
            rc = VWRN_CONTINUE_RECOMPILE;
        break;
    }

    case OP_MOV:
        if (pCpu->pCurInstr->optype & OPTYPE_POTENTIALLY_DANGEROUS)
        {
            /* mov ss, src? */
            if (    (pCpu->param1.flags & USE_REG_SEG)
                &&  (pCpu->param1.base.reg_seg == DIS_SELREG_SS))
            {
                Log(("Force recompilation of next instruction for OP_MOV at %RRv\n", pCurInstrGC));
                pPatch->flags |= PATMFL_RECOMPILE_NEXT;
                /** @todo this could cause a fault (ring 0 selector being loaded in ring 1) */
            }
#if 0 /* necessary for Haiku */
            else
            if (    (pCpu->param2.flags & USE_REG_SEG)
                &&  (pCpu->param2.base.reg_seg == USE_REG_SS)
                &&  (pCpu->param1.flags & (USE_REG_GEN32|USE_REG_GEN16)))     /** @todo memory operand must in theory be handled too */
            {
                /* mov GPR, ss */
                rc = patmPatchGenMovFromSS(pVM, pPatch, pCpu, pCurInstrGC);
                if (RT_SUCCESS(rc))
                    rc = VWRN_CONTINUE_RECOMPILE;
                break;
            }
#endif
        }
        goto duplicate_instr;

    case OP_POP:
        if (pCpu->pCurInstr->param1 == OP_PARM_REG_SS)
        {
            Assert(pCpu->pCurInstr->optype & OPTYPE_INHIBIT_IRQS);

            Log(("Force recompilation of next instruction for OP_MOV at %RRv\n", pCurInstrGC));
            pPatch->flags |= PATMFL_RECOMPILE_NEXT;
        }
        goto duplicate_instr;

    case OP_STI:
    {
        RTRCPTR pNextInstrGC = 0;   /* by default no inhibit irq */

        /** In a sequence of instructions that inhibit irqs, only the first one actually inhibits irqs. */
        if (!(pPatch->flags & PATMFL_INHIBIT_IRQS))
        {
            pPatch->flags   |= PATMFL_INHIBIT_IRQS | PATMFL_GENERATE_JUMPTOGUEST;
            fInhibitIRQInstr = true;
            pNextInstrGC     = pCurInstrGC + pCpu->opsize;
            Log(("Inhibit irqs for instruction OP_STI at %RRv\n", pCurInstrGC));
        }
        rc = patmPatchGenSti(pVM, pPatch, pCurInstrGC, pNextInstrGC);

        if (RT_SUCCESS(rc))
        {
            DISCPUSTATE cpu = *pCpu;
            unsigned    opsize;
            int         disret;
            RCPTRTYPE(uint8_t *) pNextInstrGC, pReturnInstrGC;
            R3PTRTYPE(uint8_t *) pNextInstrHC;

            pPatch->flags |= PATMFL_FOUND_PATCHEND;

            pNextInstrGC = pCurInstrGC + pCpu->opsize;
            pNextInstrHC = PATMGCVirtToHCVirt(pVM, pPatch, pNextInstrGC);
            if (pNextInstrHC == NULL)
            {
                AssertFailed();
                return VERR_PATCHING_REFUSED;
            }

            // Disassemble the next instruction
            disret = PATMR3DISInstr(pVM, pPatch, &cpu, pNextInstrGC, pNextInstrHC, &opsize, NULL);
            if (disret == false)
            {
                AssertMsgFailed(("STI: Disassembly failed (probably page not present) -> return to caller\n"));
                return VERR_PATCHING_REFUSED;
            }
            pReturnInstrGC = pNextInstrGC + opsize;

            if (   (pPatch->flags & (PATMFL_DUPLICATE_FUNCTION))
                ||  pReturnInstrGC <= pInstrGC
                ||  pReturnInstrGC - pInstrGC >= SIZEOF_NEARJUMP32
               )
            {
                /* Not an exit point for function duplication patches */
                if (    (pPatch->flags & PATMFL_DUPLICATE_FUNCTION)
                    &&  RT_SUCCESS(rc))
                {
                    pPatch->flags &= ~PATMFL_GENERATE_JUMPTOGUEST;  /* Don't generate a jump back */
                    rc = VWRN_CONTINUE_RECOMPILE;
                }
                else
                    rc = VINF_SUCCESS;  //exit point
            }
            else {
                Log(("PATM: sti occurred too soon; refusing patch!\n"));
                rc = VERR_PATCHING_REFUSED; //not allowed!!
            }
        }
        break;
    }

    case OP_POPF:
    {
        bool fGenerateJmpBack = (pCurInstrGC + pCpu->opsize - pInstrGC >= SIZEOF_NEARJUMP32);

        /* Not an exit point for IDT handler or function replacement patches */
        /* Note: keep IOPL in mind when changing any of this!! (see comments in PATMA.asm, PATMPopf32Replacement) */
        if (pPatch->flags & (PATMFL_IDTHANDLER|PATMFL_DUPLICATE_FUNCTION))
            fGenerateJmpBack = false;

        rc = patmPatchGenPopf(pVM, pPatch, pCurInstrGC + pCpu->opsize, !!(pCpu->prefix & PREFIX_OPSIZE), fGenerateJmpBack);
        if (RT_SUCCESS(rc))
        {
            if (fGenerateJmpBack == false)
            {
                /* Not an exit point for IDT handler or function replacement patches */
                rc = VWRN_CONTINUE_RECOMPILE;
            }
            else
            {
                pPatch->flags |= PATMFL_FOUND_PATCHEND;
                rc = VINF_SUCCESS;  /* exit point! */
            }
        }
        break;
    }

    case OP_PUSHF:
        rc = patmPatchGenPushf(pVM, pPatch, !!(pCpu->prefix & PREFIX_OPSIZE));
        if (RT_SUCCESS(rc))
            rc = VWRN_CONTINUE_RECOMPILE;
        break;

    case OP_PUSH:
        if (pCpu->pCurInstr->param1 == OP_PARM_REG_CS)
        {
            rc = patmPatchGenPushCS(pVM, pPatch);
            if (RT_SUCCESS(rc))
                rc = VWRN_CONTINUE_RECOMPILE;
            break;
        }
        goto duplicate_instr;

    case OP_IRET:
        Log(("IRET at %RRv\n", pCurInstrGC));
        rc = patmPatchGenIret(pVM, pPatch, pCurInstrGC, !!(pCpu->prefix & PREFIX_OPSIZE));
        if (RT_SUCCESS(rc))
        {
            pPatch->flags |= PATMFL_FOUND_PATCHEND;
            rc = VINF_SUCCESS;  /* exit point by definition */
        }
        break;

    case OP_ILLUD2:
        /* This appears to be some kind of kernel panic in Linux 2.4; no point to continue */
        rc = patmPatchGenIllegalInstr(pVM, pPatch);
        if (RT_SUCCESS(rc))
            rc = VINF_SUCCESS;  /* exit point by definition */
        Log(("Illegal opcode (0xf 0xb)\n"));
        break;

    case OP_CPUID:
        rc = patmPatchGenCpuid(pVM, pPatch, pCurInstrGC);
        if (RT_SUCCESS(rc))
            rc = VWRN_CONTINUE_RECOMPILE;
        break;

    case OP_STR:
    case OP_SLDT:
        rc = patmPatchGenSldtStr(pVM, pPatch, pCpu, pCurInstrGC);
        if (RT_SUCCESS(rc))
            rc = VWRN_CONTINUE_RECOMPILE;
        break;

    case OP_SGDT:
    case OP_SIDT:
        rc = patmPatchGenSxDT(pVM, pPatch, pCpu, pCurInstrGC);
        if (RT_SUCCESS(rc))
            rc = VWRN_CONTINUE_RECOMPILE;
        break;

    case OP_RETN:
        /* retn is an exit point for function patches */
        rc = patmPatchGenRet(pVM, pPatch, pCpu, pCurInstrGC);
        if (RT_SUCCESS(rc))
            rc = VINF_SUCCESS;  /* exit point by definition */
        break;

    case OP_SYSEXIT:
        /* Duplicate it, so it can be emulated in GC (or fault). */
        rc = patmPatchGenDuplicate(pVM, pPatch, pCpu, pCurInstrGC);
        if (RT_SUCCESS(rc))
            rc = VINF_SUCCESS;  /* exit point by definition */
        break;

    case OP_CALL:
        Assert(pPatch->flags & PATMFL_SUPPORT_INDIRECT_CALLS);
        /* In interrupt gate handlers it's possible to encounter jumps or calls when IF has been enabled again.
         * In that case we'll jump to the original instruction and continue from there. Otherwise an int 3 is executed.
         */
        Assert(pCpu->param1.size == 4 || pCpu->param1.size == 6);
        if (pPatch->flags & PATMFL_SUPPORT_INDIRECT_CALLS && pCpu->param1.size == 4 /* no far calls! */)
        {
            rc = patmPatchGenCall(pVM, pPatch, pCpu, pCurInstrGC, (RTRCPTR)0xDEADBEEF, true);
            if (RT_SUCCESS(rc))
            {
                rc = VWRN_CONTINUE_RECOMPILE;
            }
            break;
        }
        goto gen_illegal_instr;

    case OP_JMP:
        Assert(pPatch->flags & PATMFL_SUPPORT_INDIRECT_CALLS);
        /* In interrupt gate handlers it's possible to encounter jumps or calls when IF has been enabled again.
         * In that case we'll jump to the original instruction and continue from there. Otherwise an int 3 is executed.
         */
        Assert(pCpu->param1.size == 4 || pCpu->param1.size == 6);
        if (pPatch->flags & PATMFL_SUPPORT_INDIRECT_CALLS && pCpu->param1.size == 4 /* no far jumps! */)
        {
            rc = patmPatchGenJump(pVM, pPatch, pCpu, pCurInstrGC);
            if (RT_SUCCESS(rc))
                rc = VINF_SUCCESS;  /* end of branch */
            break;
        }
        goto gen_illegal_instr;

    case OP_INT3:
    case OP_INT:
    case OP_INTO:
        goto gen_illegal_instr;

    case OP_MOV_DR:
        /** @note: currently we let DRx writes cause a trap d; our trap handler will decide to interpret it or not. */
        if (pCpu->pCurInstr->param2 == OP_PARM_Dd)
        {
            rc = patmPatchGenMovDebug(pVM, pPatch, pCpu);
            if (RT_SUCCESS(rc))
                rc = VWRN_CONTINUE_RECOMPILE;
            break;
        }
        goto duplicate_instr;

    case OP_MOV_CR:
        /** @note: currently we let CRx writes cause a trap d; our trap handler will decide to interpret it or not. */
        if (pCpu->pCurInstr->param2 == OP_PARM_Cd)
        {
            rc = patmPatchGenMovControl(pVM, pPatch, pCpu);
            if (RT_SUCCESS(rc))
                rc = VWRN_CONTINUE_RECOMPILE;
            break;
        }
        goto duplicate_instr;

    default:
        if (pCpu->pCurInstr->optype & (OPTYPE_CONTROLFLOW | OPTYPE_PRIVILEGED_NOTRAP))
        {
gen_illegal_instr:
            rc = patmPatchGenIllegalInstr(pVM, pPatch);
            if (RT_SUCCESS(rc))
                rc = VINF_SUCCESS;  /* exit point by definition */
        }
        else
        {
duplicate_instr:
            Log(("patmPatchGenDuplicate\n"));
            rc = patmPatchGenDuplicate(pVM, pPatch, pCpu, pCurInstrGC);
            if (RT_SUCCESS(rc))
                rc = VWRN_CONTINUE_RECOMPILE;
        }
        break;
    }

end:

    if (    !fInhibitIRQInstr
        && (pPatch->flags & PATMFL_INHIBIT_IRQS))
    {
        int     rc2;
        RTRCPTR pNextInstrGC = pCurInstrGC + pCpu->opsize;

        pPatch->flags &= ~PATMFL_INHIBIT_IRQS;
        Log(("Clear inhibit IRQ flag at %RRv\n", pCurInstrGC));
        if (pPatch->flags & PATMFL_GENERATE_JUMPTOGUEST)
        {
            Log(("patmRecompileCallback: generate jump back to guest (%RRv) after fused instruction\n", pNextInstrGC));

            rc2 = patmPatchGenJumpToGuest(pVM, pPatch, pNextInstrGC, true /* clear inhibit irq flag */);
            pPatch->flags &= ~PATMFL_GENERATE_JUMPTOGUEST;
            rc = VINF_SUCCESS; /* end of the line */
        }
        else
        {
            rc2 = patmPatchGenClearInhibitIRQ(pVM, pPatch, pNextInstrGC);
        }
        if (RT_FAILURE(rc2))
            rc = rc2;
    }

    if (RT_SUCCESS(rc))
    {
        // If single instruction patch, we've copied enough instructions *and* the current instruction is not a relative jump
        if (    (pPatch->flags & PATMFL_CHECK_SIZE)
             &&  pCurInstrGC + pCpu->opsize - pInstrGC >= SIZEOF_NEARJUMP32
             &&  !(pCpu->pCurInstr->optype & OPTYPE_RELATIVE_CONTROLFLOW)
             &&  !(pPatch->flags & PATMFL_RECOMPILE_NEXT) /* do not do this when the next instruction *must* be executed! */
           )
        {
            RTRCPTR pNextInstrGC = pCurInstrGC + pCpu->opsize;

            // The end marker for this kind of patch is any instruction at a location outside our patch jump
            Log(("patmRecompileCallback: end found for single instruction patch at %RRv opsize %d\n", pNextInstrGC, pCpu->opsize));

            rc = patmPatchGenJumpToGuest(pVM, pPatch, pNextInstrGC);
            AssertRC(rc);
        }
    }
    return rc;
}


#ifdef LOG_ENABLED

/* Add a disasm jump record (temporary for prevent duplicate analysis)
 *
 * @param   pVM             The VM to operate on.
 * @param   pPatch          Patch structure ptr
 * @param   pInstrGC        Guest context pointer to privileged instruction
 *
 */
static void patmPatchAddDisasmJump(PVM pVM, PPATCHINFO pPatch, RTRCPTR pInstrGC)
{
    PAVLPVNODECORE pRec;

    pRec = (PAVLPVNODECORE)MMR3HeapAllocZ(pVM, MM_TAG_PATM_PATCH, sizeof(*pRec));
    Assert(pRec);
    pRec->Key = (AVLPVKEY)pInstrGC;

    int ret = RTAvlPVInsert(&pPatch->pTempInfo->DisasmJumpTree, pRec);
    Assert(ret);
}

/**
 * Checks if jump target has been analysed before.
 *
 * @returns VBox status code.
 * @param   pPatch      Patch struct
 * @param   pInstrGC    Jump target
 *
 */
static bool patmIsKnownDisasmJump(PPATCHINFO pPatch, RTRCPTR pInstrGC)
{
    PAVLPVNODECORE pRec;

    pRec = RTAvlPVGet(&pPatch->pTempInfo->DisasmJumpTree, (AVLPVKEY)pInstrGC);
    if (pRec)
        return true;
    return false;
}

/**
 * For proper disassembly of the final patch block
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pCpu        CPU disassembly state
 * @param   pInstrGC    Guest context pointer to privileged instruction
 * @param   pCurInstrGC Guest context pointer to the current instruction
 * @param   pUserData   User pointer (callback specific)
 *
 */
int patmr3DisasmCallback(PVM pVM, DISCPUSTATE *pCpu, RCPTRTYPE(uint8_t *) pInstrGC, RCPTRTYPE(uint8_t *) pCurInstrGC, void *pUserData)
{
    PPATCHINFO pPatch = (PPATCHINFO)pUserData;

    if (pCpu->pCurInstr->opcode == OP_INT3)
    {
        /* Could be an int3 inserted in a call patch. Check to be sure */
        DISCPUSTATE cpu;
        uint8_t    *pOrgJumpHC;
        RTRCPTR     pOrgJumpGC;
        uint32_t    dummy;

        cpu.mode = (pPatch->flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;
        pOrgJumpGC = patmPatchGCPtr2GuestGCPtr(pVM, pPatch, pCurInstrGC);
        pOrgJumpHC = PATMGCVirtToHCVirt(pVM, pPatch, pOrgJumpGC);

        bool disret = PATMR3DISInstr(pVM, pPatch, &cpu, pOrgJumpGC, pOrgJumpHC, &dummy, NULL);
        if (!disret || cpu.pCurInstr->opcode != OP_CALL || cpu.param1.size != 4 /* only near calls */)
            return VINF_SUCCESS;

        return VWRN_CONTINUE_ANALYSIS;
    }

    if (    pCpu->pCurInstr->opcode == OP_ILLUD2
        &&  PATMIsPatchGCAddr(pVM, pCurInstrGC))
    {
        /* the indirect call patch contains an 0xF/0xB illegal instr to call for assistance; check for this and continue */
        return VWRN_CONTINUE_ANALYSIS;
    }

    if (   (pCpu->pCurInstr->opcode == OP_CALL && !(pPatch->flags & PATMFL_SUPPORT_CALLS))
        || pCpu->pCurInstr->opcode == OP_INT
        || pCpu->pCurInstr->opcode == OP_IRET
        || pCpu->pCurInstr->opcode == OP_RETN
        || pCpu->pCurInstr->opcode == OP_RETF
       )
    {
        return VINF_SUCCESS;
    }

    if (pCpu->pCurInstr->opcode == OP_ILLUD2)
        return VINF_SUCCESS;

    return VWRN_CONTINUE_ANALYSIS;
}


/**
 * Disassembles the code stream until the callback function detects a failure or decides everything is acceptable
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context pointer to the initial privileged instruction
 * @param   pCurInstrGC Guest context pointer to the current instruction
 * @param   pfnPATMR3Disasm Callback for testing the disassembled instruction
 * @param   pUserData   User pointer (callback specific)
 *
 */
int patmr3DisasmCode(PVM pVM, RCPTRTYPE(uint8_t *) pInstrGC, RCPTRTYPE(uint8_t *) pCurInstrGC, PFN_PATMR3ANALYSE pfnPATMR3Disasm, void *pUserData)
{
    DISCPUSTATE cpu;
    PPATCHINFO pPatch = (PPATCHINFO)pUserData;
    int rc = VWRN_CONTINUE_ANALYSIS;
    uint32_t opsize, delta;
    R3PTRTYPE(uint8_t *) pCurInstrHC = 0;
    bool disret;
    char szOutput[256];

    Assert(pCurInstrHC != PATCHCODE_PTR_HC(pPatch) || pPatch->pTempInfo->DisasmJumpTree == 0);

    /* We need this to determine branch targets (and for disassembling). */
    delta = pVM->patm.s.pPatchMemGC - (uintptr_t)pVM->patm.s.pPatchMemHC;

    while(rc == VWRN_CONTINUE_ANALYSIS)
    {
        cpu.mode = (pPatch->flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;

        pCurInstrHC = PATMGCVirtToHCVirt(pVM, pPatch, pCurInstrGC);
        if (pCurInstrHC == NULL)
        {
            rc = VERR_PATCHING_REFUSED;
            goto end;
        }

        disret = PATMR3DISInstr(pVM, pPatch, &cpu, pCurInstrGC, pCurInstrHC, &opsize, szOutput, PATMREAD_RAWCODE);
        if (PATMIsPatchGCAddr(pVM, pCurInstrGC))
        {
            RTRCPTR pOrgInstrGC = patmPatchGCPtr2GuestGCPtr(pVM, pPatch, pCurInstrGC);

            if (pOrgInstrGC != pPatch->pTempInfo->pLastDisasmInstrGC)
                Log(("DIS %RRv<-%s", pOrgInstrGC, szOutput));
            else
                Log(("DIS           %s", szOutput));

            pPatch->pTempInfo->pLastDisasmInstrGC = pOrgInstrGC;
            if (patmIsIllegalInstr(pPatch, pOrgInstrGC))
            {
                rc = VINF_SUCCESS;
                goto end;
            }
        }
        else
            Log(("DIS: %s", szOutput));

        if (disret == false)
        {
            Log(("Disassembly failed (probably page not present) -> return to caller\n"));
            rc = VINF_SUCCESS;
            goto end;
        }

        rc = pfnPATMR3Disasm(pVM, &cpu, pInstrGC, pCurInstrGC, pUserData);
        if (rc != VWRN_CONTINUE_ANALYSIS) {
            break; //done!
        }

        /* For our first attempt, we'll handle only simple relative jumps and calls (immediate offset coded in instruction) */
        if (   (cpu.pCurInstr->optype & OPTYPE_CONTROLFLOW)
            && (OP_PARM_VTYPE(cpu.pCurInstr->param1) == OP_PARM_J)
            &&  cpu.pCurInstr->opcode != OP_CALL /* complete functions are replaced; don't bother here. */
           )
        {
            RTRCPTR pTargetGC = PATMResolveBranch(&cpu, pCurInstrGC);
            RTRCPTR pOrgTargetGC;

            if (pTargetGC == 0)
            {
                Log(("We don't support far jumps here!! (%08X)\n", cpu.param1.flags));
                rc = VERR_PATCHING_REFUSED;
                break;
            }

            if (!PATMIsPatchGCAddr(pVM, pTargetGC))
            {
                //jump back to guest code
                rc = VINF_SUCCESS;
                goto end;
            }
            pOrgTargetGC = PATMR3PatchToGCPtr(pVM, pTargetGC, 0);

            if (patmIsCommonIDTHandlerPatch(pVM, pOrgTargetGC))
            {
                rc = VINF_SUCCESS;
                goto end;
            }

            if (patmIsKnownDisasmJump(pPatch, pTargetGC) == false)
            {
                /* New jump, let's check it. */
                patmPatchAddDisasmJump(pVM, pPatch, pTargetGC);

                if (cpu.pCurInstr->opcode == OP_CALL)  pPatch->pTempInfo->nrCalls++;
                rc = patmr3DisasmCode(pVM, pInstrGC, pTargetGC, pfnPATMR3Disasm, pUserData);
                if (cpu.pCurInstr->opcode == OP_CALL)  pPatch->pTempInfo->nrCalls--;

                if (rc != VINF_SUCCESS) {
                    break; //done!
                }
            }
            if (cpu.pCurInstr->opcode == OP_JMP)
            {
                /* Unconditional jump; return to caller. */
                rc = VINF_SUCCESS;
                goto end;
            }

            rc = VWRN_CONTINUE_ANALYSIS;
        }
        pCurInstrGC += opsize;
    }
end:
    return rc;
}

/**
 * Disassembles the code stream until the callback function detects a failure or decides everything is acceptable
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context pointer to the initial privileged instruction
 * @param   pCurInstrGC Guest context pointer to the current instruction
 * @param   pfnPATMR3Disasm Callback for testing the disassembled instruction
 * @param   pUserData   User pointer (callback specific)
 *
 */
int patmr3DisasmCodeStream(PVM pVM, RCPTRTYPE(uint8_t *) pInstrGC, RCPTRTYPE(uint8_t *) pCurInstrGC, PFN_PATMR3ANALYSE pfnPATMR3Disasm, void *pUserData)
{
    PPATCHINFO pPatch = (PPATCHINFO)pUserData;

    int rc = patmr3DisasmCode(pVM, pInstrGC, pCurInstrGC, pfnPATMR3Disasm, pUserData);
    /* Free all disasm jump records. */
    patmEmptyTree(pVM, &pPatch->pTempInfo->DisasmJumpTree);
    return rc;
}

#endif /* LOG_ENABLED */

/**
 * Detects it the specified address falls within a 5 byte jump generated for an active patch.
 * If so, this patch is permanently disabled.
 *
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context pointer to instruction
 * @param   pConflictGC Guest context pointer to check
 *
 * @note also checks for patch hints to make sure they can never be enabled if a conflict is present.
 *
 */
VMMR3DECL(int) PATMR3DetectConflict(PVM pVM, RTRCPTR pInstrGC, RTRCPTR pConflictGC)
{
    PPATCHINFO pTargetPatch = PATMFindActivePatchByEntrypoint(pVM, pConflictGC, true /* include patch hints */);
    if (pTargetPatch)
    {
        return patmDisableUnusablePatch(pVM, pInstrGC, pConflictGC, pTargetPatch);
    }
    return VERR_PATCH_NO_CONFLICT;
}

/**
 * Recompile the code stream until the callback function detects a failure or decides everything is acceptable
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context pointer to privileged instruction
 * @param   pCurInstrGC Guest context pointer to the current instruction
 * @param   pfnPATMR3Recompile Callback for testing the disassembled instruction
 * @param   pUserData   User pointer (callback specific)
 *
 */
static int patmRecompileCodeStream(PVM pVM, RCPTRTYPE(uint8_t *) pInstrGC, RCPTRTYPE(uint8_t *) pCurInstrGC, PFN_PATMR3ANALYSE pfnPATMR3Recompile, void *pUserData)
{
    DISCPUSTATE cpu;
    PPATCHINFO pPatch = (PPATCHINFO)pUserData;
    int rc = VWRN_CONTINUE_ANALYSIS;
    uint32_t opsize;
    R3PTRTYPE(uint8_t *) pCurInstrHC = 0;
    bool disret;
#ifdef LOG_ENABLED
    char szOutput[256];
#endif

    while (rc == VWRN_CONTINUE_RECOMPILE)
    {
        cpu.mode = (pPatch->flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;

        ////Log(("patmRecompileCodeStream %RRv %RRv\n", pInstrGC, pCurInstrGC));

        pCurInstrHC = PATMGCVirtToHCVirt(pVM, pPatch, pCurInstrGC);
        if (pCurInstrHC == NULL)
        {
            rc = VERR_PATCHING_REFUSED;   /* fatal in this case */
            goto end;
        }
#ifdef LOG_ENABLED
        disret = PATMR3DISInstr(pVM, pPatch, &cpu, pCurInstrGC, pCurInstrHC, &opsize, szOutput);
        Log(("Recompile: %s", szOutput));
#else
        disret = PATMR3DISInstr(pVM, pPatch, &cpu, pCurInstrGC, pCurInstrHC, &opsize, NULL);
#endif
        if (disret == false)
        {
            Log(("Disassembly failed (probably page not present) -> return to caller\n"));

            /* Add lookup record for patch to guest address translation */
            patmr3AddP2GLookupRecord(pVM, pPatch, PATCHCODE_PTR_HC(pPatch) + pPatch->uCurPatchOffset, pCurInstrGC, PATM_LOOKUP_BOTHDIR);
            patmPatchGenIllegalInstr(pVM, pPatch);
            rc = VINF_SUCCESS;   /* Note: don't fail here; we might refuse an important patch!! */
            goto end;
        }

        rc = pfnPATMR3Recompile(pVM, &cpu, pInstrGC, pCurInstrGC, pUserData);
        if (rc != VWRN_CONTINUE_RECOMPILE)
        {
            /* If irqs are inhibited because of the current instruction, then we must make sure the next one is executed! */
            if (    rc == VINF_SUCCESS
                && (pPatch->flags & PATMFL_INHIBIT_IRQS))
            {
                DISCPUSTATE cpunext;
                uint32_t    opsizenext;
                uint8_t *pNextInstrHC;
                RTRCPTR  pNextInstrGC = pCurInstrGC + opsize;

                Log(("patmRecompileCodeStream: irqs inhibited by instruction %RRv\n", pNextInstrGC));

                /* Certain instructions (e.g. sti) force the next instruction to be executed before any interrupts can occur.
                 * Recompile the next instruction as well
                 */
                pNextInstrHC = PATMGCVirtToHCVirt(pVM, pPatch, pNextInstrGC);
                if (pNextInstrHC == NULL)
                {
                    rc = VERR_PATCHING_REFUSED;   /* fatal in this case */
                    goto end;
                }
                cpunext.mode = (pPatch->flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;
                disret = PATMR3DISInstr(pVM, pPatch, &cpunext, pNextInstrGC, pNextInstrHC, &opsizenext, NULL);
                if (disret == false)
                {
                    rc = VERR_PATCHING_REFUSED;   /* fatal in this case */
                    goto end;
                }
                switch(cpunext.pCurInstr->opcode)
                {
                case OP_IRET:       /* inhibit cleared in generated code */
                case OP_SYSEXIT:    /* faults; inhibit should be cleared in HC handling */
                case OP_HLT:
                    break; /* recompile these */

                default:
                    if (cpunext.pCurInstr->optype & OPTYPE_CONTROLFLOW)
                    {
                        Log(("Unexpected control flow instruction after inhibit irq instruction\n"));

                        rc = patmPatchGenJumpToGuest(pVM, pPatch, pNextInstrGC, true /* clear inhibit irq flag */);
                        AssertRC(rc);
                        pPatch->flags &= ~PATMFL_INHIBIT_IRQS;
                        goto end;       /** @todo should be ok to ignore instruction fusing in this case */
                    }
                    break;
                }

                /** @note after a cli we must continue to a proper exit point */
                if (cpunext.pCurInstr->opcode != OP_CLI)
                {
                    rc = pfnPATMR3Recompile(pVM, &cpunext, pInstrGC, pNextInstrGC, pUserData);
                    if (RT_SUCCESS(rc))
                    {
                        rc = VINF_SUCCESS;
                        goto end;
                    }
                    break;
                }
                else
                    rc = VWRN_CONTINUE_RECOMPILE;
            }
            else
                break; /* done! */
        }

        /** @todo continue with the instructions following the jump and then recompile the jump target code */


        /* For our first attempt, we'll handle only simple relative jumps and calls (immediate offset coded in instruction). */
        if (   (cpu.pCurInstr->optype & OPTYPE_CONTROLFLOW)
            && (OP_PARM_VTYPE(cpu.pCurInstr->param1) == OP_PARM_J)
            &&  cpu.pCurInstr->opcode != OP_CALL /* complete functions are replaced; don't bother here. */
           )
        {
            RCPTRTYPE(uint8_t *) addr = PATMResolveBranch(&cpu, pCurInstrGC);
            if (addr == 0)
            {
                Log(("We don't support far jumps here!! (%08X)\n", cpu.param1.flags));
                rc = VERR_PATCHING_REFUSED;
                break;
            }

            Log(("Jump encountered target %RRv\n", addr));

            /* We don't check if the branch target lies in a valid page as we've already done that in the analysis phase. */
            if (!(cpu.pCurInstr->optype & OPTYPE_UNCOND_CONTROLFLOW))
            {
                Log(("patmRecompileCodeStream continue passed conditional jump\n"));
                /* First we need to finish this linear code stream until the next exit point. */
                rc = patmRecompileCodeStream(pVM, pInstrGC, pCurInstrGC+opsize, pfnPATMR3Recompile, pUserData);
                if (RT_FAILURE(rc))
                {
                    Log(("patmRecompileCodeStream fatal error %d\n", rc));
                    break; //fatal error
                }
            }

            if (patmGuestGCPtrToPatchGCPtr(pVM, pPatch, addr) == 0)
            {
                /* New code; let's recompile it. */
                Log(("patmRecompileCodeStream continue with jump\n"));

                 /*
                  * If we are jumping to an existing patch (or within 5 bytes of the entrypoint), then we must temporarily disable
                  * this patch so we can continue our analysis
                  *
                  * We rely on CSAM to detect and resolve conflicts
                  */
                PPATCHINFO pTargetPatch = PATMFindActivePatchByEntrypoint(pVM, addr);
                if(pTargetPatch)
                {
                    Log(("Found active patch at target %RRv (%RRv) -> temporarily disabling it!!\n", addr, pTargetPatch->pPrivInstrGC));
                    PATMR3DisablePatch(pVM, pTargetPatch->pPrivInstrGC);
                }

                if (cpu.pCurInstr->opcode == OP_CALL)  pPatch->pTempInfo->nrCalls++;
                rc = patmRecompileCodeStream(pVM, pInstrGC, addr, pfnPATMR3Recompile, pUserData);
                if (cpu.pCurInstr->opcode == OP_CALL)  pPatch->pTempInfo->nrCalls--;

                if(pTargetPatch)
                {
                    PATMR3EnablePatch(pVM, pTargetPatch->pPrivInstrGC);
                }

                if (RT_FAILURE(rc))
                {
                    Log(("patmRecompileCodeStream fatal error %d\n", rc));
                    break; //done!
                }
            }
            /* Always return to caller here; we're done! */
            rc = VINF_SUCCESS;
            goto end;
        }
        else
        if (cpu.pCurInstr->optype & OPTYPE_UNCOND_CONTROLFLOW)
        {
            rc = VINF_SUCCESS;
            goto end;
        }
        pCurInstrGC += opsize;
    }
end:
    Assert(!(pPatch->flags & PATMFL_RECOMPILE_NEXT));
    return rc;
}


/**
 * Generate the jump from guest to patch code
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch record
 */
static int patmGenJumpToPatch(PVM pVM, PPATCHINFO pPatch, bool fAddFixup = true)
{
    uint8_t  temp[8];
    uint8_t *pPB;
    int      rc;

    Assert(pPatch->cbPatchJump <= sizeof(temp));
    Assert(!(pPatch->flags & PATMFL_PATCHED_GUEST_CODE));

    pPB = pPatch->pPrivInstrHC;

#ifdef PATM_RESOLVE_CONFLICTS_WITH_JUMP_PATCHES
    if (pPatch->flags & PATMFL_JUMP_CONFLICT)
    {
        Assert(pPatch->pPatchJumpDestGC);

        if (pPatch->cbPatchJump == SIZEOF_NEARJUMP32)
        {
            // jmp [PatchCode]
            if (fAddFixup)
            {
                if (patmPatchAddReloc32(pVM, pPatch, &pPB[1], FIXUP_REL_JMPTOPATCH, pPatch->pPrivInstrGC + pPatch->cbPatchJump, pPatch->pPatchJumpDestGC) != VINF_SUCCESS)
                {
                    Log(("Relocation failed for the jump in the guest code!!\n"));
                    return VERR_PATCHING_REFUSED;
                }
            }

            temp[0] = pPatch->aPrivInstr[0];  //jump opcode copied from original instruction
            *(uint32_t *)&temp[1] = (uint32_t)pPatch->pPatchJumpDestGC - ((uint32_t)pPatch->pPrivInstrGC + pPatch->cbPatchJump);    //return address
        }
        else
        if (pPatch->cbPatchJump == SIZEOF_NEAR_COND_JUMP32)
        {
            // jmp [PatchCode]
            if (fAddFixup)
            {
                if (patmPatchAddReloc32(pVM, pPatch, &pPB[2], FIXUP_REL_JMPTOPATCH, pPatch->pPrivInstrGC + pPatch->cbPatchJump, pPatch->pPatchJumpDestGC) != VINF_SUCCESS)
                {
                    Log(("Relocation failed for the jump in the guest code!!\n"));
                    return VERR_PATCHING_REFUSED;
                }
            }

            temp[0] = pPatch->aPrivInstr[0];  //jump opcode copied from original instruction
            temp[1] = pPatch->aPrivInstr[1];  //jump opcode copied from original instruction
            *(uint32_t *)&temp[2] = (uint32_t)pPatch->pPatchJumpDestGC - ((uint32_t)pPatch->pPrivInstrGC + pPatch->cbPatchJump);    //return address
        }
        else
        {
            Assert(0);
            return VERR_PATCHING_REFUSED;
        }
    }
    else
#endif
    {
        Assert(pPatch->cbPatchJump == SIZEOF_NEARJUMP32);

        // jmp [PatchCode]
        if (fAddFixup)
        {
            if (patmPatchAddReloc32(pVM, pPatch, &pPB[1], FIXUP_REL_JMPTOPATCH, pPatch->pPrivInstrGC + SIZEOF_NEARJUMP32, PATCHCODE_PTR_GC(pPatch)) != VINF_SUCCESS)
            {
                Log(("Relocation failed for the jump in the guest code!!\n"));
                return VERR_PATCHING_REFUSED;
            }
        }
        temp[0] = 0xE9;  //jmp
        *(uint32_t *)&temp[1] = (RTRCUINTPTR)PATCHCODE_PTR_GC(pPatch) - ((RTRCUINTPTR)pPatch->pPrivInstrGC + SIZEOF_NEARJUMP32);    //return address
    }
    rc = PGMPhysSimpleDirtyWriteGCPtr(VMMGetCpu0(pVM), pPatch->pPrivInstrGC, temp, pPatch->cbPatchJump);
    AssertRC(rc);

    if (rc == VINF_SUCCESS)
        pPatch->flags |= PATMFL_PATCHED_GUEST_CODE;

    return rc;
}

/**
 * Remove the jump from guest to patch code
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch record
 */
static int patmRemoveJumpToPatch(PVM pVM, PPATCHINFO pPatch)
{
#ifdef DEBUG
    DISCPUSTATE cpu;
    char szOutput[256];
    uint32_t opsize, i = 0;
    bool disret;

    while(i < pPatch->cbPrivInstr)
    {
        cpu.mode = (pPatch->flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;
        disret = PATMR3DISInstr(pVM, pPatch, &cpu, pPatch->pPrivInstrGC + i, &pPatch->pPrivInstrHC[i], &opsize, szOutput);
        if (disret == false)
            break;

        Log(("Org patch jump: %s", szOutput));
        Assert(opsize);
        i += opsize;
    }
#endif

    /* Restore original code (privileged instruction + following instructions that were overwritten because of the 5/6 byte jmp). */
    int rc = PGMPhysSimpleDirtyWriteGCPtr(VMMGetCpu0(pVM), pPatch->pPrivInstrGC, pPatch->aPrivInstr, pPatch->cbPatchJump);
#ifdef DEBUG
    if (rc == VINF_SUCCESS)
    {
        DISCPUSTATE cpu;
        char szOutput[256];
        uint32_t opsize, i = 0;
        bool disret;

        while(i < pPatch->cbPrivInstr)
        {
            cpu.mode = (pPatch->flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;
            disret = PATMR3DISInstr(pVM, pPatch, &cpu, pPatch->pPrivInstrGC + i, &pPatch->pPrivInstrHC[i], &opsize, szOutput);
            if (disret == false)
                break;

            Log(("Org instr: %s", szOutput));
            Assert(opsize);
            i += opsize;
        }
    }
#endif
    pPatch->flags &= ~PATMFL_PATCHED_GUEST_CODE;
    return rc;
}

/**
 * Generate the call from guest to patch code
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch record
 */
static int patmGenCallToPatch(PVM pVM, PPATCHINFO pPatch, RTRCPTR pTargetGC, bool fAddFixup = true)
{
    uint8_t  temp[8];
    uint8_t *pPB;
    int      rc;

    Assert(pPatch->cbPatchJump <= sizeof(temp));

    pPB = pPatch->pPrivInstrHC;

    Assert(pPatch->cbPatchJump == SIZEOF_NEARJUMP32);

    // jmp [PatchCode]
    if (fAddFixup)
    {
        if (patmPatchAddReloc32(pVM, pPatch, &pPB[1], FIXUP_REL_JMPTOPATCH, pPatch->pPrivInstrGC + SIZEOF_NEARJUMP32, pTargetGC) != VINF_SUCCESS)
        {
            Log(("Relocation failed for the jump in the guest code!!\n"));
            return VERR_PATCHING_REFUSED;
        }
    }

    Assert(pPatch->aPrivInstr[0] == 0xE8 || pPatch->aPrivInstr[0] == 0xE9); /* call or jmp */
    temp[0] = pPatch->aPrivInstr[0];
    *(uint32_t *)&temp[1] = (uint32_t)pTargetGC - ((uint32_t)pPatch->pPrivInstrGC + SIZEOF_NEARJUMP32);    //return address

    rc = PGMPhysSimpleDirtyWriteGCPtr(VMMGetCpu0(pVM), pPatch->pPrivInstrGC, temp, pPatch->cbPatchJump);
    AssertRC(rc);

    return rc;
}


/**
 * Patch cli/sti pushf/popf instruction block at specified location
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context point to privileged instruction
 * @param   pInstrHC    Host context point to privileged instruction
 * @param   uOpcode     Instruction opcode
 * @param   uOpSize     Size of starting instruction
 * @param   pPatchRec   Patch record
 *
 * @note    returns failure if patching is not allowed or possible
 *
 */
VMMR3DECL(int) PATMR3PatchBlock(PVM pVM, RTRCPTR pInstrGC, R3PTRTYPE(uint8_t *) pInstrHC,
                                 uint32_t uOpcode, uint32_t uOpSize, PPATMPATCHREC pPatchRec)
{
    PPATCHINFO pPatch = &pPatchRec->patch;
    int rc = VERR_PATCHING_REFUSED;
    DISCPUSTATE cpu;
    uint32_t orgOffsetPatchMem = ~0;
    RTRCPTR pInstrStart;
#ifdef LOG_ENABLED
    uint32_t opsize;
    char szOutput[256];
    bool disret;
#endif

    /* Save original offset (in case of failures later on) */
    /** @todo use the hypervisor heap (that has quite a few consequences for save/restore though) */
    orgOffsetPatchMem = pVM->patm.s.offPatchMem;

    Assert(!(pPatch->flags & (PATMFL_GUEST_SPECIFIC|PATMFL_USER_MODE|PATMFL_TRAPHANDLER)));
    switch (uOpcode)
    {
    case OP_MOV:
        break;

    case OP_CLI:
    case OP_PUSHF:
        /* We can 'call' a cli or pushf patch. It will either return to the original guest code when IF is set again, or fault. */
        /** @note special precautions are taken when disabling and enabling such patches. */
        pPatch->flags |= PATMFL_CALLABLE_AS_FUNCTION;
        break;

    default:
        if (!(pPatch->flags & PATMFL_IDTHANDLER))
        {
            AssertMsg(0, ("PATMR3PatchBlock: Invalid opcode %x\n", uOpcode));
            return VERR_INVALID_PARAMETER;
        }
    }

    if (!(pPatch->flags & (PATMFL_IDTHANDLER|PATMFL_IDTHANDLER_WITHOUT_ENTRYPOINT|PATMFL_SYSENTER|PATMFL_INT3_REPLACEMENT_BLOCK)))
        pPatch->flags |= PATMFL_MUST_INSTALL_PATCHJMP;

    /* If we're going to insert a patch jump, then the jump itself is not allowed to cross a page boundary. */
    if (     (pPatch->flags & PATMFL_MUST_INSTALL_PATCHJMP)
        &&   PAGE_ADDRESS(pInstrGC) != PAGE_ADDRESS(pInstrGC + SIZEOF_NEARJUMP32)
       )
    {
        STAM_COUNTER_INC(&pVM->patm.s.StatPageBoundaryCrossed);
#ifdef DEBUG_sandervl
////        AssertMsgFailed(("Patch jump would cross page boundary -> refuse!!\n"));
#endif
        rc = VERR_PATCHING_REFUSED;
        goto failure;
    }

    pPatch->nrPatch2GuestRecs = 0;
    pInstrStart = pInstrGC;

#ifdef PATM_ENABLE_CALL
    pPatch->flags |= PATMFL_SUPPORT_CALLS | PATMFL_SUPPORT_INDIRECT_CALLS;
#endif

    pPatch->pPatchBlockOffset = pVM->patm.s.offPatchMem;
    pPatch->uCurPatchOffset = 0;

    cpu.mode = (pPatch->flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;

    if ((pPatch->flags & (PATMFL_IDTHANDLER|PATMFL_IDTHANDLER_WITHOUT_ENTRYPOINT|PATMFL_SYSENTER)) == PATMFL_IDTHANDLER)
    {
        Assert(pPatch->flags & PATMFL_INTHANDLER);

        /* Install fake cli patch (to clear the virtual IF and check int xx parameters) */
        rc = patmPatchGenIntEntry(pVM, pPatch, pInstrGC);
        if (RT_FAILURE(rc))
            goto failure;
    }

    /***************************************************************************************************************************/
    /** @note We can't insert *any* code before a sysenter handler; some linux guests have an invalid stack at this point!!!!! */
    /***************************************************************************************************************************/
#ifdef VBOX_WITH_STATISTICS
    if (!(pPatch->flags & PATMFL_SYSENTER))
    {
        rc = patmPatchGenStats(pVM, pPatch, pInstrGC);
        if (RT_FAILURE(rc))
            goto failure;
    }
#endif

    rc = patmRecompileCodeStream(pVM, pInstrGC, pInstrGC, patmRecompileCallback, pPatch);
    if (rc != VINF_SUCCESS)
    {
        Log(("PATMR3PatchCli: patmRecompileCodeStream failed with %d\n", rc));
        goto failure;
    }

    /* Calculated during analysis. */
    if (pPatch->cbPatchBlockSize < SIZEOF_NEARJUMP32)
    {
        /* Most likely cause: we encountered an illegal instruction very early on. */
        /** @todo could turn it into an int3 callable patch. */
        Log(("PATMR3PatchBlock: patch block too small -> refuse\n"));
        rc = VERR_PATCHING_REFUSED;
        goto failure;
    }

    /* size of patch block */
    pPatch->cbPatchBlockSize = pPatch->uCurPatchOffset;


    /* Update free pointer in patch memory. */
    pVM->patm.s.offPatchMem += pPatch->cbPatchBlockSize;
    /* Round to next 8 byte boundary. */
    pVM->patm.s.offPatchMem = RT_ALIGN_32(pVM->patm.s.offPatchMem, 8);

    /*
     * Insert into patch to guest lookup tree
     */
    LogFlow(("Insert %RRv patch offset %RRv\n", pPatchRec->patch.pPrivInstrGC, pPatch->pPatchBlockOffset));
    pPatchRec->CoreOffset.Key = pPatch->pPatchBlockOffset;
    rc = RTAvloU32Insert(&pVM->patm.s.PatchLookupTreeHC->PatchTreeByPatchAddr, &pPatchRec->CoreOffset);
    AssertMsg(rc, ("RTAvlULInsert failed for %x\n", pPatchRec->CoreOffset.Key));
    if (!rc)
    {
        rc = VERR_PATCHING_REFUSED;
        goto failure;
    }

    /* Note that patmr3SetBranchTargets can install additional patches!! */
    rc = patmr3SetBranchTargets(pVM, pPatch);
    if (rc != VINF_SUCCESS)
    {
        Log(("PATMR3PatchCli: patmr3SetBranchTargets failed with %d\n", rc));
        goto failure;
    }

#ifdef LOG_ENABLED
    Log(("Patch code ----------------------------------------------------------\n"));
    patmr3DisasmCodeStream(pVM, PATCHCODE_PTR_GC(pPatch), PATCHCODE_PTR_GC(pPatch), patmr3DisasmCallback, pPatch);
    Log(("Patch code ends -----------------------------------------------------\n"));
#endif

    /* make a copy of the guest code bytes that will be overwritten */
    pPatch->cbPatchJump = SIZEOF_NEARJUMP32;

    rc = PGMPhysSimpleReadGCPtr(VMMGetCpu0(pVM), pPatch->aPrivInstr, pPatch->pPrivInstrGC, pPatch->cbPatchJump);
    AssertRC(rc);

    if (pPatch->flags & PATMFL_INT3_REPLACEMENT_BLOCK)
    {
        /*uint8_t ASMInt3 = 0xCC; - unused */

        Log(("PATMR3PatchBlock %RRv -> int 3 callable patch.\n", pPatch->pPrivInstrGC));
        /* Replace first opcode byte with 'int 3'. */
        rc = patmActivateInt3Patch(pVM, pPatch);
        if (RT_FAILURE(rc))
            goto failure;

        /* normal patch can be turned into an int3 patch -> clear patch jump installation flag. */
        pPatch->flags &= ~PATMFL_MUST_INSTALL_PATCHJMP;

        pPatch->flags &= ~PATMFL_INSTR_HINT;
        STAM_COUNTER_INC(&pVM->patm.s.StatInt3Callable);
    }
    else
    if (pPatch->flags & PATMFL_MUST_INSTALL_PATCHJMP)
    {
        Assert(!(pPatch->flags & (PATMFL_IDTHANDLER|PATMFL_IDTHANDLER_WITHOUT_ENTRYPOINT|PATMFL_SYSENTER|PATMFL_INT3_REPLACEMENT_BLOCK)));
        /* now insert a jump in the guest code */
        rc = patmGenJumpToPatch(pVM, pPatch, true);
        AssertRC(rc);
        if (RT_FAILURE(rc))
            goto failure;

    }

#ifdef LOG_ENABLED
    cpu.mode = (pPatch->flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;
    disret = PATMR3DISInstr(pVM, pPatch, &cpu, pPatch->pPrivInstrGC, pPatch->pPrivInstrHC, &opsize, szOutput, PATMREAD_RAWCODE);
    Log(("%s patch: %s", patmGetInstructionString(pPatch->opcode, pPatch->flags), szOutput));
#endif

    patmEmptyTree(pVM, &pPatch->pTempInfo->IllegalInstrTree);
    pPatch->pTempInfo->nrIllegalInstr = 0;

    Log(("Successfully installed %s patch at %RRv\n", patmGetInstructionString(pPatch->opcode, pPatch->flags), pInstrGC));

    pPatch->uState = PATCH_ENABLED;
    return VINF_SUCCESS;

failure:
    if (pPatchRec->CoreOffset.Key)
        RTAvloU32Remove(&pVM->patm.s.PatchLookupTreeHC->PatchTreeByPatchAddr, pPatchRec->CoreOffset.Key);

    patmEmptyTree(pVM, &pPatch->FixupTree);
    pPatch->nrFixups = 0;

    patmEmptyTree(pVM, &pPatch->JumpTree);
    pPatch->nrJumpRecs = 0;

    patmEmptyTree(pVM, &pPatch->pTempInfo->IllegalInstrTree);
    pPatch->pTempInfo->nrIllegalInstr = 0;

    /* Turn this cli patch into a dummy. */
    pPatch->uState = PATCH_REFUSED;
    pPatch->pPatchBlockOffset = 0;

    // Give back the patch memory we no longer need
    Assert(orgOffsetPatchMem != (uint32_t)~0);
    pVM->patm.s.offPatchMem = orgOffsetPatchMem;

    return rc;
}

/**
 * Patch IDT handler
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context point to privileged instruction
 * @param   pInstrHC    Host context point to privileged instruction
 * @param   uOpSize     Size of starting instruction
 * @param   pPatchRec   Patch record
 *
 * @note    returns failure if patching is not allowed or possible
 *
 */
static int patmIdtHandler(PVM pVM, RTRCPTR pInstrGC, R3PTRTYPE(uint8_t *) pInstrHC,
                          uint32_t uOpSize, PPATMPATCHREC pPatchRec)
{
    PPATCHINFO pPatch = &pPatchRec->patch;
    bool disret;
    DISCPUSTATE cpuPush, cpuJmp;
    uint32_t opsize;
    RTRCPTR  pCurInstrGC = pInstrGC;
    uint8_t *pCurInstrHC = pInstrHC;
    uint32_t orgOffsetPatchMem = ~0;

    /*
     * In Linux it's often the case that many interrupt handlers push a predefined value onto the stack
     * and then jump to a common entrypoint. In order not to waste a lot of memory, we will check for this
     * condition here and only patch the common entypoint once.
     */
    cpuPush.mode = (pPatch->flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;
    disret = PATMR3DISInstr(pVM, pPatch, &cpuPush, pCurInstrGC, pCurInstrHC, &opsize, NULL);
    Assert(disret);
    if (disret && cpuPush.pCurInstr->opcode == OP_PUSH)
    {
        RTRCPTR  pJmpInstrGC;
        int      rc;

        pCurInstrGC += opsize;
        pCurInstrHC = PATMGCVirtToHCVirt(pVM, pPatch, pCurInstrGC);

        cpuJmp.mode = (pPatch->flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;
        disret = PATMR3DISInstr(pVM, pPatch, &cpuJmp, pCurInstrGC, pCurInstrHC, &opsize, NULL);
        if (   disret
            && cpuJmp.pCurInstr->opcode == OP_JMP
            && (pJmpInstrGC = PATMResolveBranch(&cpuJmp, pCurInstrGC))
           )
        {
            PPATMPATCHREC pJmpPatch = (PPATMPATCHREC)RTAvloU32Get(&pVM->patm.s.PatchLookupTreeHC->PatchTree, pJmpInstrGC);
            if (pJmpPatch == 0)
            {
                /* Patch it first! */
                rc = PATMR3InstallPatch(pVM, pJmpInstrGC, pPatch->flags | PATMFL_IDTHANDLER_WITHOUT_ENTRYPOINT);
                if (rc != VINF_SUCCESS)
                    goto failure;
                pJmpPatch = (PPATMPATCHREC)RTAvloU32Get(&pVM->patm.s.PatchLookupTreeHC->PatchTree, pJmpInstrGC);
                Assert(pJmpPatch);
            }
            if (pJmpPatch->patch.uState != PATCH_ENABLED)
                goto failure;

            /* save original offset (in case of failures later on) */
            orgOffsetPatchMem = pVM->patm.s.offPatchMem;

            pPatch->pPatchBlockOffset = pVM->patm.s.offPatchMem;
            pPatch->uCurPatchOffset   = 0;
            pPatch->nrPatch2GuestRecs = 0;

#ifdef VBOX_WITH_STATISTICS
            rc = patmPatchGenStats(pVM, pPatch, pInstrGC);
            if (RT_FAILURE(rc))
                goto failure;
#endif

            /* Install fake cli patch (to clear the virtual IF) */
            rc = patmPatchGenIntEntry(pVM, pPatch, pInstrGC);
            if (RT_FAILURE(rc))
                goto failure;

            /* Add lookup record for patch to guest address translation (for the push) */
            patmr3AddP2GLookupRecord(pVM, pPatch, PATCHCODE_PTR_HC(pPatch) + pPatch->uCurPatchOffset, pInstrGC, PATM_LOOKUP_BOTHDIR);

            /* Duplicate push. */
            rc = patmPatchGenDuplicate(pVM, pPatch, &cpuPush, pInstrGC);
            if (RT_FAILURE(rc))
                goto failure;

            /* Generate jump to common entrypoint. */
            rc = patmPatchGenPatchJump(pVM, pPatch, pCurInstrGC, PATCHCODE_PTR_GC(&pJmpPatch->patch));
            if (RT_FAILURE(rc))
                goto failure;

            /* size of patch block */
            pPatch->cbPatchBlockSize = pPatch->uCurPatchOffset;

            /* Update free pointer in patch memory. */
            pVM->patm.s.offPatchMem += pPatch->cbPatchBlockSize;
            /* Round to next 8 byte boundary */
            pVM->patm.s.offPatchMem = RT_ALIGN_32(pVM->patm.s.offPatchMem, 8);

            /* There's no jump from guest to patch code. */
            pPatch->cbPatchJump = 0;


#ifdef LOG_ENABLED
            Log(("Patch code ----------------------------------------------------------\n"));
            patmr3DisasmCodeStream(pVM, PATCHCODE_PTR_GC(pPatch), PATCHCODE_PTR_GC(pPatch), patmr3DisasmCallback, pPatch);
            Log(("Patch code ends -----------------------------------------------------\n"));
#endif
            Log(("Successfully installed IDT handler patch at %RRv\n", pInstrGC));

            /*
             * Insert into patch to guest lookup tree
             */
            LogFlow(("Insert %RRv patch offset %RRv\n", pPatchRec->patch.pPrivInstrGC, pPatch->pPatchBlockOffset));
            pPatchRec->CoreOffset.Key = pPatch->pPatchBlockOffset;
            rc = RTAvloU32Insert(&pVM->patm.s.PatchLookupTreeHC->PatchTreeByPatchAddr, &pPatchRec->CoreOffset);
            AssertMsg(rc, ("RTAvlULInsert failed for %x\n", pPatchRec->CoreOffset.Key));

            pPatch->uState = PATCH_ENABLED;

            return VINF_SUCCESS;
        }
    }
failure:
    /* Give back the patch memory we no longer need */
    if (orgOffsetPatchMem != (uint32_t)~0)
        pVM->patm.s.offPatchMem = orgOffsetPatchMem;

    return PATMR3PatchBlock(pVM, pInstrGC, pInstrHC, OP_CLI, uOpSize, pPatchRec);
}

/**
 * Install a trampoline to call a guest trap handler directly
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context point to privileged instruction
 * @param   pPatchRec   Patch record
 *
 */
static int patmInstallTrapTrampoline(PVM pVM, RTRCPTR pInstrGC, PPATMPATCHREC pPatchRec)
{
    PPATCHINFO pPatch = &pPatchRec->patch;
    int rc = VERR_PATCHING_REFUSED;
    uint32_t orgOffsetPatchMem = ~0;
#ifdef LOG_ENABLED
    bool disret;
    DISCPUSTATE cpu;
    uint32_t opsize;
    char szOutput[256];
#endif

    // save original offset (in case of failures later on)
    orgOffsetPatchMem = pVM->patm.s.offPatchMem;

    pPatch->pPatchBlockOffset = pVM->patm.s.offPatchMem;
    pPatch->uCurPatchOffset   = 0;
    pPatch->nrPatch2GuestRecs = 0;

#ifdef VBOX_WITH_STATISTICS
    rc = patmPatchGenStats(pVM, pPatch, pInstrGC);
    if (RT_FAILURE(rc))
        goto failure;
#endif

    rc = patmPatchGenTrapEntry(pVM, pPatch, pInstrGC);
    if (RT_FAILURE(rc))
        goto failure;

    /* size of patch block */
    pPatch->cbPatchBlockSize = pPatch->uCurPatchOffset;

    /* Update free pointer in patch memory. */
    pVM->patm.s.offPatchMem += pPatch->cbPatchBlockSize;
    /* Round to next 8 byte boundary */
    pVM->patm.s.offPatchMem = RT_ALIGN_32(pVM->patm.s.offPatchMem, 8);

    /* There's no jump from guest to patch code. */
    pPatch->cbPatchJump = 0;

#ifdef LOG_ENABLED
    Log(("Patch code ----------------------------------------------------------\n"));
    patmr3DisasmCodeStream(pVM, PATCHCODE_PTR_GC(pPatch), PATCHCODE_PTR_GC(pPatch), patmr3DisasmCallback, pPatch);
    Log(("Patch code ends -----------------------------------------------------\n"));
#endif

#ifdef LOG_ENABLED
    cpu.mode = (pPatch->flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;
    disret = PATMR3DISInstr(pVM, pPatch, &cpu, pPatch->pPrivInstrGC, pPatch->pPrivInstrHC, &opsize, szOutput);
    Log(("TRAP handler patch: %s", szOutput));
#endif
    Log(("Successfully installed Trap Trampoline patch at %RRv\n", pInstrGC));

    /*
     * Insert into patch to guest lookup tree
     */
    LogFlow(("Insert %RRv patch offset %RRv\n", pPatchRec->patch.pPrivInstrGC, pPatch->pPatchBlockOffset));
    pPatchRec->CoreOffset.Key = pPatch->pPatchBlockOffset;
    rc = RTAvloU32Insert(&pVM->patm.s.PatchLookupTreeHC->PatchTreeByPatchAddr, &pPatchRec->CoreOffset);
    AssertMsg(rc, ("RTAvlULInsert failed for %x\n", pPatchRec->CoreOffset.Key));

    pPatch->uState = PATCH_ENABLED;
    return VINF_SUCCESS;

failure:
    AssertMsgFailed(("Failed to install trap handler trampoline!!\n"));

    /* Turn this cli patch into a dummy. */
    pPatch->uState = PATCH_REFUSED;
    pPatch->pPatchBlockOffset = 0;

    /* Give back the patch memory we no longer need */
    Assert(orgOffsetPatchMem != (uint32_t)~0);
    pVM->patm.s.offPatchMem = orgOffsetPatchMem;

    return rc;
}


#ifdef LOG_ENABLED
/**
 * Check if the instruction is patched as a common idt handler
 *
 * @returns true or false
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context point to the instruction
 *
 */
static bool patmIsCommonIDTHandlerPatch(PVM pVM, RTRCPTR pInstrGC)
{
    PPATMPATCHREC pRec;

    pRec = (PPATMPATCHREC)RTAvloU32Get(&pVM->patm.s.PatchLookupTreeHC->PatchTree, pInstrGC);
    if (pRec && pRec->patch.flags & PATMFL_IDTHANDLER_WITHOUT_ENTRYPOINT)
        return true;
    return false;
}
#endif //DEBUG


/**
 * Duplicates a complete function
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context point to privileged instruction
 * @param   pPatchRec   Patch record
 *
 */
static int patmDuplicateFunction(PVM pVM, RTRCPTR pInstrGC, PPATMPATCHREC pPatchRec)
{
    PPATCHINFO pPatch = &pPatchRec->patch;
    int rc = VERR_PATCHING_REFUSED;
    DISCPUSTATE cpu;
    uint32_t orgOffsetPatchMem = ~0;

    Log(("patmDuplicateFunction %RRv\n", pInstrGC));
    /* Save original offset (in case of failures later on). */
    orgOffsetPatchMem = pVM->patm.s.offPatchMem;

    /* We will not go on indefinitely with call instruction handling. */
    if (pVM->patm.s.ulCallDepth > PATM_MAX_CALL_DEPTH)
    {
        Log(("patmDuplicateFunction: maximum callback depth reached!!\n"));
        return VERR_PATCHING_REFUSED;
    }

    pVM->patm.s.ulCallDepth++;

#ifdef PATM_ENABLE_CALL
    pPatch->flags |= PATMFL_SUPPORT_CALLS | PATMFL_SUPPORT_INDIRECT_CALLS;
#endif

    Assert(pPatch->flags & (PATMFL_DUPLICATE_FUNCTION));

    pPatch->nrPatch2GuestRecs = 0;
    pPatch->pPatchBlockOffset = pVM->patm.s.offPatchMem;
    pPatch->uCurPatchOffset   = 0;

    cpu.mode = (pPatch->flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;

    /** @note Set the PATM interrupt flag here; it was cleared before the patched call. (!!!) */
    rc = patmPatchGenSetPIF(pVM, pPatch, pInstrGC);
    if (RT_FAILURE(rc))
        goto failure;

#ifdef VBOX_WITH_STATISTICS
    rc = patmPatchGenStats(pVM, pPatch, pInstrGC);
    if (RT_FAILURE(rc))
        goto failure;
#endif
    rc = patmRecompileCodeStream(pVM, pInstrGC, pInstrGC, patmRecompileCallback, pPatch);
    if (rc != VINF_SUCCESS)
    {
        Log(("PATMR3PatchCli: patmRecompileCodeStream failed with %d\n", rc));
        goto failure;
    }

    //size of patch block
    pPatch->cbPatchBlockSize = pPatch->uCurPatchOffset;

    //update free pointer in patch memory
    pVM->patm.s.offPatchMem += pPatch->cbPatchBlockSize;
    /* Round to next 8 byte boundary. */
    pVM->patm.s.offPatchMem = RT_ALIGN_32(pVM->patm.s.offPatchMem, 8);

    pPatch->uState = PATCH_ENABLED;

    /*
     * Insert into patch to guest lookup tree
     */
    LogFlow(("Insert %RRv patch offset %RRv\n", pPatchRec->patch.pPrivInstrGC, pPatch->pPatchBlockOffset));
    pPatchRec->CoreOffset.Key = pPatch->pPatchBlockOffset;
    rc = RTAvloU32Insert(&pVM->patm.s.PatchLookupTreeHC->PatchTreeByPatchAddr, &pPatchRec->CoreOffset);
    AssertMsg(rc, ("RTAvloU32Insert failed for %x\n", pPatchRec->CoreOffset.Key));
    if (!rc)
    {
        rc = VERR_PATCHING_REFUSED;
        goto failure;
    }

    /* Note that patmr3SetBranchTargets can install additional patches!! */
    rc = patmr3SetBranchTargets(pVM, pPatch);
    if (rc != VINF_SUCCESS)
    {
        Log(("PATMR3PatchCli: patmr3SetBranchTargets failed with %d\n", rc));
        goto failure;
    }

#ifdef LOG_ENABLED
    Log(("Patch code ----------------------------------------------------------\n"));
    patmr3DisasmCodeStream(pVM, PATCHCODE_PTR_GC(pPatch), PATCHCODE_PTR_GC(pPatch), patmr3DisasmCallback, pPatch);
    Log(("Patch code ends -----------------------------------------------------\n"));
#endif

    Log(("Successfully installed function duplication patch at %RRv\n", pInstrGC));

    patmEmptyTree(pVM, &pPatch->pTempInfo->IllegalInstrTree);
    pPatch->pTempInfo->nrIllegalInstr = 0;

    pVM->patm.s.ulCallDepth--;
    STAM_COUNTER_INC(&pVM->patm.s.StatInstalledFunctionPatches);
    return VINF_SUCCESS;

failure:
    if (pPatchRec->CoreOffset.Key)
        RTAvloU32Remove(&pVM->patm.s.PatchLookupTreeHC->PatchTreeByPatchAddr, pPatchRec->CoreOffset.Key);

    patmEmptyTree(pVM, &pPatch->FixupTree);
    pPatch->nrFixups = 0;

    patmEmptyTree(pVM, &pPatch->JumpTree);
    pPatch->nrJumpRecs = 0;

    patmEmptyTree(pVM, &pPatch->pTempInfo->IllegalInstrTree);
    pPatch->pTempInfo->nrIllegalInstr = 0;

    /* Turn this cli patch into a dummy. */
    pPatch->uState = PATCH_REFUSED;
    pPatch->pPatchBlockOffset = 0;

    // Give back the patch memory we no longer need
    Assert(orgOffsetPatchMem != (uint32_t)~0);
    pVM->patm.s.offPatchMem = orgOffsetPatchMem;

    pVM->patm.s.ulCallDepth--;
    Log(("patmDupicateFunction %RRv failed!!\n", pInstrGC));
    return rc;
}

/**
 * Creates trampoline code to jump inside an existing patch
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context point to privileged instruction
 * @param   pPatchRec   Patch record
 *
 */
static int patmCreateTrampoline(PVM pVM, RTRCPTR pInstrGC, PPATMPATCHREC pPatchRec)
{
    PPATCHINFO  pPatch = &pPatchRec->patch;
    RTRCPTR     pPage, pPatchTargetGC = 0;
    uint32_t    orgOffsetPatchMem = ~0;
    int         rc = VERR_PATCHING_REFUSED;

    Log(("patmCreateTrampoline %RRv\n", pInstrGC));
    /* Save original offset (in case of failures later on). */
    orgOffsetPatchMem = pVM->patm.s.offPatchMem;

    /* First we check if the duplicate function target lies in some existing function patch already. Will save some space. */
    /** @todo we already checked this before */
    pPage = pInstrGC & PAGE_BASE_GC_MASK;

    PPATMPATCHPAGE pPatchPage = (PPATMPATCHPAGE)RTAvloU32Get(&pVM->patm.s.PatchLookupTreeHC->PatchTreeByPage, (RTRCPTR)pPage);
    if (pPatchPage)
    {
        uint32_t i;

        for (i=0;i<pPatchPage->cCount;i++)
        {
            if (pPatchPage->aPatch[i])
            {
                PPATCHINFO pPatch = pPatchPage->aPatch[i];

                if (    (pPatch->flags & PATMFL_DUPLICATE_FUNCTION)
                    &&  pPatch->uState == PATCH_ENABLED)
                {
                    pPatchTargetGC = patmGuestGCPtrToPatchGCPtr(pVM, pPatch, pInstrGC);
                    if (pPatchTargetGC)
                    {
                        uint32_t         offsetPatch      = pPatchTargetGC - pVM->patm.s.pPatchMemGC;
                        PRECPATCHTOGUEST pPatchToGuestRec = (PRECPATCHTOGUEST)RTAvlU32GetBestFit(&pPatch->Patch2GuestAddrTree, offsetPatch, false);
                        Assert(pPatchToGuestRec);

                        pPatchToGuestRec->fJumpTarget = true;
                        Assert(pPatchTargetGC != pPatch->pPrivInstrGC);
                        Log(("patmCreateTrampoline: generating jump to code inside patch at %RRv\n", pPatch->pPrivInstrGC));
                        pPatch->flags |= PATMFL_EXTERNAL_JUMP_INSIDE;
                        break;
                    }
                }
            }
        }
    }
    AssertReturn(pPatchPage && pPatchTargetGC, VERR_PATCHING_REFUSED);

    pPatch->nrPatch2GuestRecs = 0;
    pPatch->pPatchBlockOffset = pVM->patm.s.offPatchMem;
    pPatch->uCurPatchOffset   = 0;

    /** @note Set the PATM interrupt flag here; it was cleared before the patched call. (!!!) */
    rc = patmPatchGenSetPIF(pVM, pPatch, pInstrGC);
    if (RT_FAILURE(rc))
        goto failure;

#ifdef VBOX_WITH_STATISTICS
    rc = patmPatchGenStats(pVM, pPatch, pInstrGC);
    if (RT_FAILURE(rc))
        goto failure;
#endif

    rc = patmPatchGenPatchJump(pVM, pPatch, pInstrGC, pPatchTargetGC);
    if (RT_FAILURE(rc))
        goto failure;

    /*
     * Insert into patch to guest lookup tree
     */
    LogFlow(("Insert %RRv patch offset %RRv\n", pPatchRec->patch.pPrivInstrGC, pPatch->pPatchBlockOffset));
    pPatchRec->CoreOffset.Key = pPatch->pPatchBlockOffset;
    rc = RTAvloU32Insert(&pVM->patm.s.PatchLookupTreeHC->PatchTreeByPatchAddr, &pPatchRec->CoreOffset);
    AssertMsg(rc, ("RTAvloU32Insert failed for %x\n", pPatchRec->CoreOffset.Key));
    if (!rc)
    {
        rc = VERR_PATCHING_REFUSED;
        goto failure;
    }

    /* size of patch block */
    pPatch->cbPatchBlockSize = pPatch->uCurPatchOffset;

    /* Update free pointer in patch memory. */
    pVM->patm.s.offPatchMem += pPatch->cbPatchBlockSize;
    /* Round to next 8 byte boundary */
    pVM->patm.s.offPatchMem = RT_ALIGN_32(pVM->patm.s.offPatchMem, 8);

    /* There's no jump from guest to patch code. */
    pPatch->cbPatchJump = 0;

    /* Enable the patch. */
    pPatch->uState = PATCH_ENABLED;
    /* We allow this patch to be called as a function. */
    pPatch->flags |= PATMFL_CALLABLE_AS_FUNCTION;
    STAM_COUNTER_INC(&pVM->patm.s.StatInstalledTrampoline);
    return VINF_SUCCESS;

failure:
    if (pPatchRec->CoreOffset.Key)
        RTAvloU32Remove(&pVM->patm.s.PatchLookupTreeHC->PatchTreeByPatchAddr, pPatchRec->CoreOffset.Key);

    patmEmptyTree(pVM, &pPatch->FixupTree);
    pPatch->nrFixups = 0;

    patmEmptyTree(pVM, &pPatch->JumpTree);
    pPatch->nrJumpRecs = 0;

    patmEmptyTree(pVM, &pPatch->pTempInfo->IllegalInstrTree);
    pPatch->pTempInfo->nrIllegalInstr = 0;

    /* Turn this cli patch into a dummy. */
    pPatch->uState = PATCH_REFUSED;
    pPatch->pPatchBlockOffset = 0;

    // Give back the patch memory we no longer need
    Assert(orgOffsetPatchMem != (uint32_t)~0);
    pVM->patm.s.offPatchMem = orgOffsetPatchMem;

    return rc;
}


/**
 * Patch branch target function for call/jump at specified location.
 * (in responds to a VINF_PATM_DUPLICATE_FUNCTION GC exit reason)
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pCtx        Guest context
 *
 */
VMMR3DECL(int) PATMR3DuplicateFunctionRequest(PVM pVM, PCPUMCTX pCtx)
{
    RTRCPTR     pBranchTarget, pPage;
    int         rc;
    RTRCPTR     pPatchTargetGC = 0;

    pBranchTarget = pCtx->edx;
    pBranchTarget = SELMToFlat(pVM, DIS_SELREG_CS, CPUMCTX2CORE(pCtx), pBranchTarget);

    /* First we check if the duplicate function target lies in some existing function patch already. Will save some space. */
    pPage = pBranchTarget & PAGE_BASE_GC_MASK;

    PPATMPATCHPAGE pPatchPage = (PPATMPATCHPAGE)RTAvloU32Get(&pVM->patm.s.PatchLookupTreeHC->PatchTreeByPage, (RTRCPTR)pPage);
    if (pPatchPage)
    {
        uint32_t i;

        for (i=0;i<pPatchPage->cCount;i++)
        {
            if (pPatchPage->aPatch[i])
            {
                PPATCHINFO pPatch = pPatchPage->aPatch[i];

                if (    (pPatch->flags & PATMFL_DUPLICATE_FUNCTION)
                    &&  pPatch->uState == PATCH_ENABLED)
                {
                    pPatchTargetGC = patmGuestGCPtrToPatchGCPtr(pVM, pPatch, pBranchTarget);
                    if (pPatchTargetGC)
                    {
                        STAM_COUNTER_INC(&pVM->patm.s.StatDuplicateUseExisting);
                        break;
                    }
                }
            }
        }
    }

    if (pPatchTargetGC)
    {
        /* Create a trampoline that also sets PATM_INTERRUPTFLAG. */
        rc = PATMR3InstallPatch(pVM, pBranchTarget, PATMFL_CODE32 | PATMFL_TRAMPOLINE);
    }
    else
    {
        rc = PATMR3InstallPatch(pVM, pBranchTarget, PATMFL_CODE32 | PATMFL_DUPLICATE_FUNCTION);
    }

    if (rc == VINF_SUCCESS)
    {
        pPatchTargetGC = PATMR3QueryPatchGCPtr(pVM, pBranchTarget);
        Assert(pPatchTargetGC);
    }

    if (pPatchTargetGC)
    {
        pCtx->eax = pPatchTargetGC;
        pCtx->eax = pCtx->eax - (RTRCUINTPTR)pVM->patm.s.pPatchMemGC;   /* make it relative */
    }
    else
    {
        /* We add a dummy entry into the lookup cache so we won't get bombarded with the same requests over and over again. */
        pCtx->eax = 0;
        STAM_COUNTER_INC(&pVM->patm.s.StatDuplicateREQFailed);
    }
    Assert(PATMIsPatchGCAddr(pVM, pCtx->edi));
    rc = PATMAddBranchToLookupCache(pVM, pCtx->edi, pBranchTarget, pCtx->eax);
    AssertRC(rc);

    pCtx->eip += PATM_ILLEGAL_INSTR_SIZE;
    STAM_COUNTER_INC(&pVM->patm.s.StatDuplicateREQSuccess);
    return VINF_SUCCESS;
}

/**
 * Replaces a function call by a call to an existing function duplicate (or jmp -> jmp)
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pCpu        Disassembly CPU structure ptr
 * @param   pInstrGC    Guest context point to privileged instruction
 * @param   pPatch      Patch record
 *
 */
static int patmReplaceFunctionCall(PVM pVM, DISCPUSTATE *pCpu, RTRCPTR pInstrGC, PPATCHINFO pPatch)
{
    int           rc = VERR_PATCHING_REFUSED;
    DISCPUSTATE   cpu;
    RTRCPTR       pTargetGC;
    PPATMPATCHREC pPatchFunction;
    uint32_t      opsize;
    bool          disret;
#ifdef LOG_ENABLED
    char          szOutput[256];
#endif

    Assert(pPatch->flags & PATMFL_REPLACE_FUNCTION_CALL);
    Assert((pCpu->pCurInstr->opcode == OP_CALL || pCpu->pCurInstr->opcode == OP_JMP) && pCpu->opsize == SIZEOF_NEARJUMP32);

    if ((pCpu->pCurInstr->opcode != OP_CALL && pCpu->pCurInstr->opcode != OP_JMP) || pCpu->opsize != SIZEOF_NEARJUMP32)
    {
        rc = VERR_PATCHING_REFUSED;
        goto failure;
    }

    pTargetGC = PATMResolveBranch(pCpu, pInstrGC);
    if (pTargetGC == 0)
    {
        Log(("We don't support far jumps here!! (%08X)\n", pCpu->param1.flags));
        rc = VERR_PATCHING_REFUSED;
        goto failure;
    }

    pPatchFunction = (PPATMPATCHREC)RTAvloU32Get(&pVM->patm.s.PatchLookupTreeHC->PatchTree, pTargetGC);
    if (pPatchFunction == NULL)
    {
        for(;;)
        {
            /* It could be an indirect call (call -> jmp dest).
             * Note that it's dangerous to assume the jump will never change...
             */
            uint8_t *pTmpInstrHC;

            pTmpInstrHC = PATMGCVirtToHCVirt(pVM, pPatch, pTargetGC);
            Assert(pTmpInstrHC);
            if (pTmpInstrHC == 0)
                break;

            cpu.mode = (pPatch->flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;
            disret = PATMR3DISInstr(pVM, pPatch, &cpu, pTargetGC, pTmpInstrHC, &opsize, NULL);
            if (disret == false || cpu.pCurInstr->opcode != OP_JMP)
                break;

            pTargetGC = PATMResolveBranch(&cpu, pTargetGC);
            if (pTargetGC == 0)
            {
                break;
            }

            pPatchFunction = (PPATMPATCHREC)RTAvloU32Get(&pVM->patm.s.PatchLookupTreeHC->PatchTree, pTargetGC);
            break;
        }
        if (pPatchFunction == 0)
        {
            AssertMsgFailed(("Unable to find duplicate function %RRv\n", pTargetGC));
            rc = VERR_PATCHING_REFUSED;
            goto failure;
        }
    }

    // make a copy of the guest code bytes that will be overwritten
    pPatch->cbPatchJump = SIZEOF_NEARJUMP32;

    rc = PGMPhysSimpleReadGCPtr(VMMGetCpu0(pVM), pPatch->aPrivInstr, pPatch->pPrivInstrGC, pPatch->cbPatchJump);
    AssertRC(rc);

    /* Now replace the original call in the guest code */
    rc = patmGenCallToPatch(pVM, pPatch, PATCHCODE_PTR_GC(&pPatchFunction->patch), true);
    AssertRC(rc);
    if (RT_FAILURE(rc))
        goto failure;

    /* Lowest and highest address for write monitoring. */
    pPatch->pInstrGCLowest  = pInstrGC;
    pPatch->pInstrGCHighest = pInstrGC + pCpu->opsize;

#ifdef LOG_ENABLED
    cpu.mode = (pPatch->flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;
    disret = PATMR3DISInstr(pVM, pPatch, &cpu, pPatch->pPrivInstrGC, pPatch->pPrivInstrHC, &opsize, szOutput);
    Log(("Call patch: %s", szOutput));
#endif

    Log(("Successfully installed function replacement patch at %RRv\n", pInstrGC));

    pPatch->uState = PATCH_ENABLED;
    return VINF_SUCCESS;

failure:
    /* Turn this patch into a dummy. */
    pPatch->uState = PATCH_REFUSED;

    return rc;
}

/**
 * Replace the address in an MMIO instruction with the cached version.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context point to privileged instruction
 * @param   pCpu        Disassembly CPU structure ptr
 * @param   pPatch      Patch record
 *
 * @note    returns failure if patching is not allowed or possible
 *
 */
static int patmPatchMMIOInstr(PVM pVM, RTRCPTR pInstrGC, DISCPUSTATE *pCpu, PPATCHINFO pPatch)
{
    uint8_t *pPB;
    int      rc = VERR_PATCHING_REFUSED;
#ifdef LOG_ENABLED
    DISCPUSTATE   cpu;
    uint32_t      opsize;
    bool          disret;
    char          szOutput[256];
#endif

    Assert(pVM->patm.s.mmio.pCachedData);
    if (!pVM->patm.s.mmio.pCachedData)
        goto failure;

    if (pCpu->param2.flags != USE_DISPLACEMENT32)
        goto failure;

    pPB = pPatch->pPrivInstrHC;

    /* Add relocation record for cached data access. */
    if (patmPatchAddReloc32(pVM, pPatch, &pPB[pCpu->opsize - sizeof(RTRCPTR)], FIXUP_ABSOLUTE, pPatch->pPrivInstrGC, pVM->patm.s.mmio.pCachedData) != VINF_SUCCESS)
    {
        Log(("Relocation failed for cached mmio address!!\n"));
        return VERR_PATCHING_REFUSED;
    }
#ifdef LOG_ENABLED
    cpu.mode = (pPatch->flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;
    disret = PATMR3DISInstr(pVM, pPatch, &cpu, pPatch->pPrivInstrGC, pPatch->pPrivInstrHC, &opsize, szOutput);
    Log(("MMIO patch old instruction: %s", szOutput));
#endif

    /* Save original instruction. */
    rc = PGMPhysSimpleReadGCPtr(VMMGetCpu0(pVM), pPatch->aPrivInstr, pPatch->pPrivInstrGC, pPatch->cbPrivInstr);
    AssertRC(rc);

    pPatch->cbPatchJump = pPatch->cbPrivInstr;  /* bit of a misnomer in this case; size of replacement instruction. */

    /* Replace address with that of the cached item. */
    rc = PGMPhysSimpleDirtyWriteGCPtr(VMMGetCpu0(pVM), pInstrGC + pCpu->opsize - sizeof(RTRCPTR), &pVM->patm.s.mmio.pCachedData, sizeof(RTRCPTR));
    AssertRC(rc);
    if (RT_FAILURE(rc))
    {
        goto failure;
    }

#ifdef LOG_ENABLED
    cpu.mode = (pPatch->flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;
    disret = PATMR3DISInstr(pVM, pPatch, &cpu, pPatch->pPrivInstrGC, pPatch->pPrivInstrHC, &opsize, szOutput);
    Log(("MMIO patch: %s", szOutput));
#endif
    pVM->patm.s.mmio.pCachedData = 0;
    pVM->patm.s.mmio.GCPhys = 0;
    pPatch->uState = PATCH_ENABLED;
    return VINF_SUCCESS;

failure:
    /* Turn this patch into a dummy. */
    pPatch->uState = PATCH_REFUSED;

    return rc;
}


/**
 * Replace the address in an MMIO instruction with the cached version. (instruction is part of an existing patch)
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context point to privileged instruction
 * @param   pPatch      Patch record
 *
 * @note    returns failure if patching is not allowed or possible
 *
 */
static int patmPatchPATMMMIOInstr(PVM pVM, RTRCPTR pInstrGC, PPATCHINFO pPatch)
{
    DISCPUSTATE   cpu;
    uint32_t      opsize;
    bool          disret;
    uint8_t      *pInstrHC;
#ifdef LOG_ENABLED
    char          szOutput[256];
#endif

    AssertReturn(pVM->patm.s.mmio.pCachedData, VERR_INVALID_PARAMETER);

    /* Convert GC to HC address. */
    pInstrHC = patmPatchGCPtr2PatchHCPtr(pVM, pInstrGC);
    AssertReturn(pInstrHC, VERR_PATCHING_REFUSED);

    /* Disassemble mmio instruction. */
    cpu.mode = pPatch->uOpMode;
    disret = PATMR3DISInstr(pVM, pPatch, &cpu, pInstrGC, pInstrHC, &opsize, NULL);
    if (disret == false)
    {
        Log(("Disassembly failed (probably page not present) -> return to caller\n"));
        return VERR_PATCHING_REFUSED;
    }

    AssertMsg(opsize <= MAX_INSTR_SIZE, ("privileged instruction too big %d!!\n", opsize));
    if (opsize > MAX_INSTR_SIZE)
        return VERR_PATCHING_REFUSED;
    if (cpu.param2.flags != USE_DISPLACEMENT32)
        return VERR_PATCHING_REFUSED;

    /* Add relocation record for cached data access. */
    if (patmPatchAddReloc32(pVM, pPatch, &pInstrHC[cpu.opsize - sizeof(RTRCPTR)], FIXUP_ABSOLUTE) != VINF_SUCCESS)
    {
        Log(("Relocation failed for cached mmio address!!\n"));
        return VERR_PATCHING_REFUSED;
    }
    /* Replace address with that of the cached item. */
    *(RTRCPTR *)&pInstrHC[cpu.opsize - sizeof(RTRCPTR)] = pVM->patm.s.mmio.pCachedData;

    /* Lowest and highest address for write monitoring. */
    pPatch->pInstrGCLowest  = pInstrGC;
    pPatch->pInstrGCHighest = pInstrGC + cpu.opsize;

#ifdef LOG_ENABLED
    cpu.mode = (pPatch->flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;
    disret = PATMR3DISInstr(pVM, pPatch, &cpu, pInstrGC, pInstrHC, &opsize, szOutput);
    Log(("MMIO patch: %s", szOutput));
#endif

    pVM->patm.s.mmio.pCachedData = 0;
    pVM->patm.s.mmio.GCPhys = 0;
    return VINF_SUCCESS;
}

/**
 * Activates an int3 patch
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch record
 */
static int patmActivateInt3Patch(PVM pVM, PPATCHINFO pPatch)
{
    uint8_t ASMInt3 = 0xCC;
    int     rc;

    Assert(pPatch->flags & (PATMFL_INT3_REPLACEMENT|PATMFL_INT3_REPLACEMENT_BLOCK));
    Assert(pPatch->uState != PATCH_ENABLED);

    /* Replace first opcode byte with 'int 3'. */
    rc = PGMPhysSimpleDirtyWriteGCPtr(VMMGetCpu0(pVM), pPatch->pPrivInstrGC, &ASMInt3, sizeof(ASMInt3));
    AssertRC(rc);

    pPatch->cbPatchJump = sizeof(ASMInt3);

    return rc;
}

/**
 * Deactivates an int3 patch
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch record
 */
static int patmDeactivateInt3Patch(PVM pVM, PPATCHINFO pPatch)
{
    uint8_t ASMInt3 = 0xCC;
    int     rc;

    Assert(pPatch->flags & (PATMFL_INT3_REPLACEMENT|PATMFL_INT3_REPLACEMENT_BLOCK));
    Assert(pPatch->uState == PATCH_ENABLED || pPatch->uState == PATCH_DIRTY);

    /* Restore first opcode byte. */
    rc = PGMPhysSimpleDirtyWriteGCPtr(VMMGetCpu0(pVM), pPatch->pPrivInstrGC, pPatch->aPrivInstr, sizeof(ASMInt3));
    AssertRC(rc);
    return rc;
}

/**
 * Replace an instruction with a breakpoint (0xCC), that is handled dynamically in the guest context.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context point to privileged instruction
 * @param   pInstrHC    Host context point to privileged instruction
 * @param   pCpu        Disassembly CPU structure ptr
 * @param   pPatch      Patch record
 *
 * @note    returns failure if patching is not allowed or possible
 *
 */
VMMR3DECL(int) PATMR3PatchInstrInt3(PVM pVM, RTRCPTR pInstrGC, R3PTRTYPE(uint8_t *) pInstrHC, DISCPUSTATE *pCpu, PPATCHINFO pPatch)
{
    uint8_t ASMInt3 = 0xCC;
    int rc;

    /** @note Do not use patch memory here! It might called during patch installation too. */

#ifdef LOG_ENABLED
    DISCPUSTATE   cpu;
    char          szOutput[256];
    uint32_t      opsize;

    cpu.mode = (pPatch->flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;
    PATMR3DISInstr(pVM, pPatch, &cpu, pInstrGC, pInstrHC, &opsize, szOutput);
    Log(("PATMR3PatchInstrInt3: %s", szOutput));
#endif

    /* Save the original instruction. */
    rc = PGMPhysSimpleReadGCPtr(VMMGetCpu0(pVM), pPatch->aPrivInstr, pPatch->pPrivInstrGC, pPatch->cbPrivInstr);
    AssertRC(rc);
    pPatch->cbPatchJump = sizeof(ASMInt3);  /* bit of a misnomer in this case; size of replacement instruction. */

    pPatch->flags |= PATMFL_INT3_REPLACEMENT;

    /* Replace first opcode byte with 'int 3'. */
    rc = patmActivateInt3Patch(pVM, pPatch);
    if (RT_FAILURE(rc))
        goto failure;

    /* Lowest and highest address for write monitoring. */
    pPatch->pInstrGCLowest  = pInstrGC;
    pPatch->pInstrGCHighest = pInstrGC + pCpu->opsize;

    pPatch->uState = PATCH_ENABLED;
    return VINF_SUCCESS;

failure:
    /* Turn this patch into a dummy. */
    return VERR_PATCHING_REFUSED;
}

#ifdef PATM_RESOLVE_CONFLICTS_WITH_JUMP_PATCHES
/**
 * Patch a jump instruction at specified location
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context point to privileged instruction
 * @param   pInstrHC    Host context point to privileged instruction
 * @param   pCpu        Disassembly CPU structure ptr
 * @param   pPatchRec   Patch record
 *
 * @note    returns failure if patching is not allowed or possible
 *
 */
int patmPatchJump(PVM pVM, RTRCPTR pInstrGC, R3PTRTYPE(uint8_t *) pInstrHC, DISCPUSTATE *pCpu, PPATMPATCHREC pPatchRec)
{
    PPATCHINFO pPatch = &pPatchRec->patch;
    int rc = VERR_PATCHING_REFUSED;
#ifdef LOG_ENABLED
    bool disret;
    DISCPUSTATE cpu;
    uint32_t opsize;
    char szOutput[256];
#endif

    pPatch->pPatchBlockOffset = 0;  /* doesn't use patch memory */
    pPatch->uCurPatchOffset   = 0;
    pPatch->cbPatchBlockSize  = 0;
    pPatch->flags            |= PATMFL_SINGLE_INSTRUCTION;

    /*
     * Instruction replacements such as these should never be interrupted. I've added code to EM.cpp to
     * make sure this never happens. (unless a trap is triggered (intentionally or not))
     */
    switch (pCpu->pCurInstr->opcode)
    {
    case OP_JO:
    case OP_JNO:
    case OP_JC:
    case OP_JNC:
    case OP_JE:
    case OP_JNE:
    case OP_JBE:
    case OP_JNBE:
    case OP_JS:
    case OP_JNS:
    case OP_JP:
    case OP_JNP:
    case OP_JL:
    case OP_JNL:
    case OP_JLE:
    case OP_JNLE:
    case OP_JMP:
        Assert(pPatch->flags & PATMFL_JUMP_CONFLICT);
        Assert(pCpu->param1.flags & USE_IMMEDIATE32_REL);
        if (!(pCpu->param1.flags & USE_IMMEDIATE32_REL))
            goto failure;

        Assert(pCpu->opsize == SIZEOF_NEARJUMP32 || pCpu->opsize == SIZEOF_NEAR_COND_JUMP32);
        if (pCpu->opsize != SIZEOF_NEARJUMP32 && pCpu->opsize != SIZEOF_NEAR_COND_JUMP32)
            goto failure;

        if (PAGE_ADDRESS(pInstrGC) != PAGE_ADDRESS(pInstrGC + pCpu->opsize))
        {
            STAM_COUNTER_INC(&pVM->patm.s.StatPageBoundaryCrossed);
            AssertMsgFailed(("Patch jump would cross page boundary -> refuse!!\n"));
            rc = VERR_PATCHING_REFUSED;
            goto failure;
        }

        break;

    default:
        goto failure;
    }

    // make a copy of the guest code bytes that will be overwritten
    Assert(pCpu->opsize <= sizeof(pPatch->aPrivInstr));
    Assert(pCpu->opsize >= SIZEOF_NEARJUMP32);
    pPatch->cbPatchJump = pCpu->opsize;

    rc = PGMPhysSimpleReadGCPtr(VMMGetCpu0(pVM), pPatch->aPrivInstr, pPatch->pPrivInstrGC, pPatch->cbPatchJump);
    AssertRC(rc);

    /* Now insert a jump in the guest code. */
    /*
     * A conflict jump patch needs to be treated differently; we'll just replace the relative jump address with one that
     * references the target instruction in the conflict patch.
     */
    RTRCPTR pJmpDest = PATMR3GuestGCPtrToPatchGCPtr(pVM, pInstrGC + pCpu->opsize + (int32_t)pCpu->param1.parval);

    AssertMsg(pJmpDest, ("PATMR3GuestGCPtrToPatchGCPtr failed for %RRv\n", pInstrGC + pCpu->opsize + (int32_t)pCpu->param1.parval));
    pPatch->pPatchJumpDestGC = pJmpDest;

    rc = patmGenJumpToPatch(pVM, pPatch, true);
    AssertRC(rc);
    if (RT_FAILURE(rc))
        goto failure;

    pPatch->flags |= PATMFL_MUST_INSTALL_PATCHJMP;

#ifdef LOG_ENABLED
    cpu.mode = (pPatch->flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;
    disret = PATMR3DISInstr(pVM, pPatch, &cpu, pPatch->pPrivInstrGC, pPatch->pPrivInstrHC, &opsize, szOutput);
    Log(("%s patch: %s", patmGetInstructionString(pPatch->opcode, pPatch->flags), szOutput));
#endif

    Log(("Successfully installed %s patch at %RRv\n", patmGetInstructionString(pPatch->opcode, pPatch->flags), pInstrGC));

    STAM_COUNTER_INC(&pVM->patm.s.StatInstalledJump);

    /* Lowest and highest address for write monitoring. */
    pPatch->pInstrGCLowest  = pInstrGC;
    pPatch->pInstrGCHighest = pInstrGC + pPatch->cbPatchJump;

    pPatch->uState = PATCH_ENABLED;
    return VINF_SUCCESS;

failure:
    /* Turn this cli patch into a dummy. */
    pPatch->uState = PATCH_REFUSED;

    return rc;
}
#endif /* PATM_RESOLVE_CONFLICTS_WITH_JUMP_PATCHES */


/**
 * Gives hint to PATM about supervisor guest instructions
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstr      Guest context point to privileged instruction
 * @param   flags       Patch flags
 */
VMMR3DECL(int) PATMR3AddHint(PVM pVM, RTRCPTR pInstrGC, uint32_t flags)
{
    Assert(pInstrGC);
    Assert(flags == PATMFL_CODE32);

    Log(("PATMR3AddHint %RRv\n", pInstrGC));
    return PATMR3InstallPatch(pVM, pInstrGC, PATMFL_CODE32 | PATMFL_INSTR_HINT);
}

/**
 * Patch privileged instruction at specified location
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstr      Guest context point to privileged instruction (0:32 flat address)
 * @param   flags       Patch flags
 *
 * @note    returns failure if patching is not allowed or possible
 */
VMMR3DECL(int) PATMR3InstallPatch(PVM pVM, RTRCPTR pInstrGC, uint64_t flags)
{
    DISCPUSTATE cpu;
    R3PTRTYPE(uint8_t *) pInstrHC;
    uint32_t opsize;
    PPATMPATCHREC pPatchRec;
    PCPUMCTX pCtx = 0;
    bool disret;
    int rc;
    PVMCPU pVCpu = VMMGetCpu0(pVM);

    if (!pVM || pInstrGC == 0 || (flags & ~(PATMFL_CODE32|PATMFL_IDTHANDLER|PATMFL_INTHANDLER|PATMFL_SYSENTER|PATMFL_TRAPHANDLER|PATMFL_DUPLICATE_FUNCTION|PATMFL_REPLACE_FUNCTION_CALL|PATMFL_GUEST_SPECIFIC|PATMFL_INT3_REPLACEMENT|PATMFL_TRAPHANDLER_WITH_ERRORCODE|PATMFL_IDTHANDLER_WITHOUT_ENTRYPOINT|PATMFL_MMIO_ACCESS|PATMFL_TRAMPOLINE|PATMFL_INSTR_HINT|PATMFL_JUMP_CONFLICT)))
    {
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }

    if (PATMIsEnabled(pVM) == false)
        return VERR_PATCHING_REFUSED;

    /* Test for patch conflict only with patches that actually change guest code. */
    if (!(flags & (PATMFL_GUEST_SPECIFIC|PATMFL_IDTHANDLER|PATMFL_INTHANDLER|PATMFL_TRAMPOLINE)))
    {
        PPATCHINFO pConflictPatch = PATMFindActivePatchByEntrypoint(pVM, pInstrGC);
        AssertReleaseMsg(pConflictPatch == 0, ("Unable to patch overwritten instruction at %RRv (%RRv)\n", pInstrGC, pConflictPatch->pPrivInstrGC));
        if (pConflictPatch != 0)
            return VERR_PATCHING_REFUSED;
    }

    if (!(flags & PATMFL_CODE32))
    {
        /** @todo Only 32 bits code right now */
        AssertMsgFailed(("PATMR3InstallPatch: We don't support 16 bits code at this moment!!\n"));
        return VERR_NOT_IMPLEMENTED;
    }

    /* We ran out of patch memory; don't bother anymore. */
    if (pVM->patm.s.fOutOfMemory == true)
        return VERR_PATCHING_REFUSED;

    /* Make sure the code selector is wide open; otherwise refuse. */
    pCtx = CPUMQueryGuestCtxPtr(pVCpu);
    if (CPUMGetGuestCPL(pVCpu, CPUMCTX2CORE(pCtx)) == 0)
    {
        RTRCPTR pInstrGCFlat = SELMToFlat(pVM, DIS_SELREG_CS, CPUMCTX2CORE(pCtx), pInstrGC);
        if (pInstrGCFlat != pInstrGC)
        {
            Log(("PATMR3InstallPatch: code selector not wide open: %04x:%RRv != %RRv eflags=%08x\n", pCtx->cs, pInstrGCFlat, pInstrGC, pCtx->eflags.u32));
            return VERR_PATCHING_REFUSED;
        }
    }

    /** @note the OpenBSD specific check will break if we allow additional patches to be installed (int 3)) */
    if (!(flags & PATMFL_GUEST_SPECIFIC))
    {
        /* New code. Make sure CSAM has a go at it first. */
        CSAMR3CheckCode(pVM, pInstrGC);
    }

    /** @note obsolete */
    if (    PATMIsPatchGCAddr(pVM, pInstrGC)
        && (flags & PATMFL_MMIO_ACCESS))
    {
        RTRCUINTPTR offset;
        void         *pvPatchCoreOffset;

        /* Find the patch record. */
        offset = pInstrGC - pVM->patm.s.pPatchMemGC;
        pvPatchCoreOffset = RTAvloU32GetBestFit(&pVM->patm.s.PatchLookupTreeHC->PatchTreeByPatchAddr, offset, false);
        if (pvPatchCoreOffset == NULL)
        {
            AssertMsgFailed(("PATMR3InstallPatch: patch not found at address %RRv!!\n", pInstrGC));
            return VERR_PATCH_NOT_FOUND;    //fatal error
        }
        pPatchRec = PATM_PATCHREC_FROM_COREOFFSET(pvPatchCoreOffset);

        return patmPatchPATMMMIOInstr(pVM, pInstrGC, &pPatchRec->patch);
    }

    AssertReturn(!PATMIsPatchGCAddr(pVM, pInstrGC), VERR_PATCHING_REFUSED);

    pPatchRec = (PPATMPATCHREC)RTAvloU32Get(&pVM->patm.s.PatchLookupTreeHC->PatchTree, pInstrGC);
    if (pPatchRec)
    {
        Assert(!(flags & PATMFL_TRAMPOLINE));

        /* Hints about existing patches are ignored. */
        if (flags & PATMFL_INSTR_HINT)
            return VERR_PATCHING_REFUSED;

        if (pPatchRec->patch.uState == PATCH_DISABLE_PENDING)
        {
            Log(("PATMR3InstallPatch: disable operation is pending for patch at %RRv\n", pPatchRec->patch.pPrivInstrGC));
            PATMR3DisablePatch(pVM, pPatchRec->patch.pPrivInstrGC);
            Assert(pPatchRec->patch.uState == PATCH_DISABLED);
        }

        if (pPatchRec->patch.uState == PATCH_DISABLED)
        {
            /* A patch, for which we previously received a hint, will be enabled and turned into a normal patch. */
            if (pPatchRec->patch.flags & PATMFL_INSTR_HINT)
            {
                Log(("Enabling HINTED patch %RRv\n", pInstrGC));
                pPatchRec->patch.flags &= ~PATMFL_INSTR_HINT;
            }
            else
                Log(("Enabling patch %RRv again\n", pInstrGC));

            /** @todo we shouldn't disable and enable patches too often (it's relatively cheap, but pointless if it always happens) */
            rc = PATMR3EnablePatch(pVM, pInstrGC);
            if (RT_SUCCESS(rc))
                return VWRN_PATCH_ENABLED;

            return rc;
        }
        if (    pPatchRec->patch.uState == PATCH_ENABLED
            ||  pPatchRec->patch.uState == PATCH_DIRTY)
        {
            /*
             * The patch might have been overwritten.
             */
            STAM_COUNTER_INC(&pVM->patm.s.StatOverwritten);
            if (pPatchRec->patch.uState != PATCH_REFUSED && pPatchRec->patch.uState != PATCH_UNUSABLE)
            {
                /* Patch must have been overwritten; remove it and pretend nothing happened. */
                Log(("Patch an existing patched instruction?!? (%RRv)\n", pInstrGC));
                if (pPatchRec->patch.flags & (PATMFL_DUPLICATE_FUNCTION|PATMFL_IDTHANDLER|PATMFL_MMIO_ACCESS|PATMFL_INT3_REPLACEMENT|PATMFL_INT3_REPLACEMENT_BLOCK))
                {
                    if (flags & PATMFL_IDTHANDLER)
                        pPatchRec->patch.flags |= (flags & (PATMFL_IDTHANDLER|PATMFL_TRAPHANDLER|PATMFL_INTHANDLER)); /* update the type */

                    return VERR_PATM_ALREADY_PATCHED;    /* already done once */
                }
            }
            rc = PATMR3RemovePatch(pVM, pInstrGC);
            if (RT_FAILURE(rc))
                return VERR_PATCHING_REFUSED;
        }
        else
        {
            AssertMsg(pPatchRec->patch.uState == PATCH_REFUSED || pPatchRec->patch.uState == PATCH_UNUSABLE, ("Patch an existing patched instruction?!? (%RRv, state=%d)\n", pInstrGC, pPatchRec->patch.uState));
            /* already tried it once! */
            return VERR_PATCHING_REFUSED;
        }
    }

    rc = MMHyperAlloc(pVM, sizeof(PATMPATCHREC), 0, MM_TAG_PATM_PATCH, (void **)&pPatchRec);
    if (RT_FAILURE(rc))
    {
        Log(("Out of memory!!!!\n"));
        return VERR_NO_MEMORY;
    }
    pPatchRec->Core.Key = pInstrGC;
    pPatchRec->patch.uState = PATCH_REFUSED;   //default
    rc = RTAvloU32Insert(&pVM->patm.s.PatchLookupTreeHC->PatchTree, &pPatchRec->Core);
    Assert(rc);

    RTGCPHYS GCPhys;
    rc = PGMGstGetPage(pVCpu, pInstrGC, NULL, &GCPhys);
    if (rc != VINF_SUCCESS)
    {
        Log(("PGMGstGetPage failed with %Rrc\n", rc));
        return rc;
    }
    /* Disallow patching instructions inside ROM code; complete function duplication is allowed though. */
    if (    !(flags & (PATMFL_DUPLICATE_FUNCTION|PATMFL_TRAMPOLINE))
        &&  !PGMPhysIsGCPhysNormal(pVM, GCPhys))
    {
        Log(("Code at %RGv (phys %RGp) is in a ROM, MMIO or invalid page - refused\n", pInstrGC, GCPhys));
        return VERR_PATCHING_REFUSED;
    }
    GCPhys = GCPhys + (pInstrGC & PAGE_OFFSET_MASK);
    rc = PGMPhysGCPhys2R3Ptr(pVM, GCPhys, MAX_INSTR_SIZE, (void **)&pInstrHC);
    AssertRCReturn(rc, rc);

    pPatchRec->patch.pPrivInstrHC = pInstrHC;
    pPatchRec->patch.pPrivInstrGC = pInstrGC;
    pPatchRec->patch.flags   = flags;
    pPatchRec->patch.uOpMode = (flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;

    pPatchRec->patch.pInstrGCLowest  = pInstrGC;
    pPatchRec->patch.pInstrGCHighest = pInstrGC;

    if (!(pPatchRec->patch.flags & (PATMFL_DUPLICATE_FUNCTION | PATMFL_IDTHANDLER | PATMFL_SYSENTER | PATMFL_TRAMPOLINE)))
    {
        /*
         * Close proximity to an unusable patch is a possible hint that this patch would turn out to be dangerous too!
         */
        PPATMPATCHREC pPatchNear = (PPATMPATCHREC)RTAvloU32GetBestFit(&pVM->patm.s.PatchLookupTreeHC->PatchTree, (pInstrGC + SIZEOF_NEARJUMP32 - 1), false);
        if (pPatchNear)
        {
            if (pPatchNear->patch.uState == PATCH_UNUSABLE && pInstrGC < pPatchNear->patch.pPrivInstrGC && pInstrGC + SIZEOF_NEARJUMP32 > pPatchNear->patch.pPrivInstrGC)
            {
                Log(("Dangerous patch; would overwrite the ususable patch at %RRv\n", pPatchNear->patch.pPrivInstrGC));

                pPatchRec->patch.uState = PATCH_UNUSABLE;
                /*
                 * Leave the new patch active as it's marked unusable; to prevent us from checking it over and over again
                 */
                return VERR_PATCHING_REFUSED;
            }
        }
    }

    pPatchRec->patch.pTempInfo = (PPATCHINFOTEMP)MMR3HeapAllocZ(pVM, MM_TAG_PATM_PATCH, sizeof(PATCHINFOTEMP));
    if (pPatchRec->patch.pTempInfo == 0)
    {
        Log(("Out of memory!!!!\n"));
        return VERR_NO_MEMORY;
    }

    cpu.mode = pPatchRec->patch.uOpMode;
    disret = PATMR3DISInstr(pVM, &pPatchRec->patch, &cpu, pInstrGC, pInstrHC, &opsize, NULL);
    if (disret == false)
    {
        Log(("Disassembly failed (probably page not present) -> return to caller\n"));
        return VERR_PATCHING_REFUSED;
    }

    AssertMsg(opsize <= MAX_INSTR_SIZE, ("privileged instruction too big %d!!\n", opsize));
    if (opsize > MAX_INSTR_SIZE)
    {
        return VERR_PATCHING_REFUSED;
    }

    pPatchRec->patch.cbPrivInstr = opsize;
    pPatchRec->patch.opcode      = cpu.pCurInstr->opcode;

    /* Restricted hinting for now. */
    Assert(!(flags & PATMFL_INSTR_HINT) || cpu.pCurInstr->opcode == OP_CLI);

    /* Allocate statistics slot */
    if (pVM->patm.s.uCurrentPatchIdx < PATM_STAT_MAX_COUNTERS)
    {
        pPatchRec->patch.uPatchIdx = pVM->patm.s.uCurrentPatchIdx++;
    }
    else
    {
        Log(("WARNING: Patch index wrap around!!\n"));
        pPatchRec->patch.uPatchIdx = PATM_STAT_INDEX_DUMMY;
    }

    if (pPatchRec->patch.flags & PATMFL_TRAPHANDLER)
    {
        rc = patmInstallTrapTrampoline(pVM, pInstrGC, pPatchRec);
    }
    else
    if (pPatchRec->patch.flags & (PATMFL_DUPLICATE_FUNCTION ))
    {
        rc = patmDuplicateFunction(pVM, pInstrGC, pPatchRec);
    }
    else
    if (pPatchRec->patch.flags & PATMFL_TRAMPOLINE)
    {
        rc = patmCreateTrampoline(pVM, pInstrGC, pPatchRec);
    }
    else
    if (pPatchRec->patch.flags & PATMFL_REPLACE_FUNCTION_CALL)
    {
        rc = patmReplaceFunctionCall(pVM, &cpu, pInstrGC, &pPatchRec->patch);
    }
    else
    if (pPatchRec->patch.flags & PATMFL_INT3_REPLACEMENT)
    {
        rc = PATMR3PatchInstrInt3(pVM, pInstrGC, pInstrHC, &cpu, &pPatchRec->patch);
    }
    else
    if (pPatchRec->patch.flags & PATMFL_MMIO_ACCESS)
    {
        rc = patmPatchMMIOInstr(pVM, pInstrGC, &cpu, &pPatchRec->patch);
    }
    else
    if (pPatchRec->patch.flags & (PATMFL_IDTHANDLER|PATMFL_SYSENTER))
    {
        if (pPatchRec->patch.flags & PATMFL_SYSENTER)
            pPatchRec->patch.flags |= PATMFL_IDTHANDLER;    /* we treat a sysenter handler as an IDT handler */

        rc = patmIdtHandler(pVM, pInstrGC, pInstrHC, opsize, pPatchRec);
#ifdef VBOX_WITH_STATISTICS
        if (    rc == VINF_SUCCESS
            &&  (pPatchRec->patch.flags & PATMFL_SYSENTER))
        {
            pVM->patm.s.uSysEnterPatchIdx = pPatchRec->patch.uPatchIdx;
        }
#endif
    }
    else
    if (pPatchRec->patch.flags & PATMFL_GUEST_SPECIFIC)
    {
        switch (cpu.pCurInstr->opcode)
        {
        case OP_SYSENTER:
        case OP_PUSH:
            rc = PATMInstallGuestSpecificPatch(pVM, &cpu, pInstrGC, pInstrHC, pPatchRec);
            if (rc == VINF_SUCCESS)
            {
                if (rc == VINF_SUCCESS)
                    Log(("PATMR3InstallPatch GUEST: %s %RRv code32=%d\n", patmGetInstructionString(pPatchRec->patch.opcode, pPatchRec->patch.flags), pInstrGC, (flags & PATMFL_CODE32) ? 1 : 0));
                return rc;
            }
            break;

        default:
            rc = VERR_NOT_IMPLEMENTED;
            break;
        }
    }
    else
    {
        switch (cpu.pCurInstr->opcode)
        {
        case OP_SYSENTER:
            rc = PATMInstallGuestSpecificPatch(pVM, &cpu, pInstrGC, pInstrHC, pPatchRec);
            if (rc == VINF_SUCCESS)
            {
                Log(("PATMR3InstallPatch GUEST: %s %RRv code32=%d\n", patmGetInstructionString(pPatchRec->patch.opcode, pPatchRec->patch.flags), pInstrGC, (flags & PATMFL_CODE32) ? 1 : 0));
                return VINF_SUCCESS;
            }
            break;

#ifdef PATM_RESOLVE_CONFLICTS_WITH_JUMP_PATCHES
        case OP_JO:
        case OP_JNO:
        case OP_JC:
        case OP_JNC:
        case OP_JE:
        case OP_JNE:
        case OP_JBE:
        case OP_JNBE:
        case OP_JS:
        case OP_JNS:
        case OP_JP:
        case OP_JNP:
        case OP_JL:
        case OP_JNL:
        case OP_JLE:
        case OP_JNLE:
        case OP_JECXZ:
        case OP_LOOP:
        case OP_LOOPNE:
        case OP_LOOPE:
        case OP_JMP:
            if (pPatchRec->patch.flags & PATMFL_JUMP_CONFLICT)
            {
                rc = patmPatchJump(pVM, pInstrGC, pInstrHC, &cpu, pPatchRec);
                break;
            }
            return VERR_NOT_IMPLEMENTED;
#endif

        case OP_PUSHF:
        case OP_CLI:
            Log(("PATMR3InstallPatch %s %RRv code32=%d\n", patmGetInstructionString(pPatchRec->patch.opcode, pPatchRec->patch.flags), pInstrGC, (flags & PATMFL_CODE32) ? 1 : 0));
            rc = PATMR3PatchBlock(pVM, pInstrGC, pInstrHC, cpu.pCurInstr->opcode, opsize, pPatchRec);
            break;

        case OP_STR:
        case OP_SGDT:
        case OP_SLDT:
        case OP_SIDT:
        case OP_CPUID:
        case OP_LSL:
        case OP_LAR:
        case OP_SMSW:
        case OP_VERW:
        case OP_VERR:
        case OP_IRET:
            rc = PATMR3PatchInstrInt3(pVM, pInstrGC, pInstrHC, &cpu, &pPatchRec->patch);
            break;

        default:
            return VERR_NOT_IMPLEMENTED;
        }
    }

    if (rc != VINF_SUCCESS)
    {
        if (pPatchRec && pPatchRec->patch.nrPatch2GuestRecs)
        {
            patmEmptyTreeU32(pVM, &pPatchRec->patch.Patch2GuestAddrTree);
            pPatchRec->patch.nrPatch2GuestRecs = 0;
        }
        pVM->patm.s.uCurrentPatchIdx--;
    }
    else
    {
        rc = patmInsertPatchPages(pVM, &pPatchRec->patch);
        AssertRCReturn(rc, rc);

        /* Keep track upper and lower boundaries of patched instructions */
        if (pPatchRec->patch.pInstrGCLowest < pVM->patm.s.pPatchedInstrGCLowest)
            pVM->patm.s.pPatchedInstrGCLowest = pPatchRec->patch.pInstrGCLowest;
        if (pPatchRec->patch.pInstrGCHighest > pVM->patm.s.pPatchedInstrGCHighest)
            pVM->patm.s.pPatchedInstrGCHighest = pPatchRec->patch.pInstrGCHighest;

        Log(("Patch  lowest %RRv highest %RRv\n", pPatchRec->patch.pInstrGCLowest, pPatchRec->patch.pInstrGCHighest));
        Log(("Global lowest %RRv highest %RRv\n", pVM->patm.s.pPatchedInstrGCLowest, pVM->patm.s.pPatchedInstrGCHighest));

        STAM_COUNTER_ADD(&pVM->patm.s.StatInstalled, 1);
        STAM_COUNTER_ADD(&pVM->patm.s.StatPATMMemoryUsed, pPatchRec->patch.cbPatchBlockSize);

        rc = VINF_SUCCESS;

        /* Patch hints are not enabled by default. Only when the are actually encountered. */
        if (pPatchRec->patch.flags & PATMFL_INSTR_HINT)
        {
            rc = PATMR3DisablePatch(pVM, pInstrGC);
            AssertRCReturn(rc, rc);
        }

#ifdef VBOX_WITH_STATISTICS
        /* Register statistics counter */
        if (PATM_STAT_INDEX_IS_VALID(pPatchRec->patch.uPatchIdx))
        {
            STAMR3RegisterCallback(pVM, &pPatchRec->patch, STAMVISIBILITY_NOT_GUI, STAMUNIT_GOOD_BAD, patmResetStat, patmPrintStat, "Patch statistics",
                                   "/PATM/Stats/Patch/0x%RRv", pPatchRec->patch.pPrivInstrGC);
#ifndef DEBUG_sandervl
            /* Full breakdown for the GUI. */
            STAMR3RegisterF(pVM, &pVM->patm.s.pStatsHC[pPatchRec->patch.uPatchIdx], STAMTYPE_RATIO_U32, STAMVISIBILITY_ALWAYS, STAMUNIT_GOOD_BAD, PATMPatchType(pVM, &pPatchRec->patch),
                            "/PATM/Stats/PatchBD/0x%RRv", pPatchRec->patch.pPrivInstrGC);
            STAMR3RegisterF(pVM, &pPatchRec->patch.cbPatchBlockSize,STAMTYPE_U32, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,      NULL, "/PATM/Stats/PatchBD/0x%RRv/cbPatchBlockSize", pPatchRec->patch.pPrivInstrGC);
            STAMR3RegisterF(pVM, &pPatchRec->patch.cbPatchJump,     STAMTYPE_U32, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,      NULL, "/PATM/Stats/PatchBD/0x%RRv/cbPatchJump", pPatchRec->patch.pPrivInstrGC);
            STAMR3RegisterF(pVM, &pPatchRec->patch.cbPrivInstr,     STAMTYPE_U32, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,      NULL, "/PATM/Stats/PatchBD/0x%RRv/cbPrivInstr", pPatchRec->patch.pPrivInstrGC);
            STAMR3RegisterF(pVM, &pPatchRec->patch.cCodeWrites,     STAMTYPE_U32, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, NULL, "/PATM/Stats/PatchBD/0x%RRv/cCodeWrites", pPatchRec->patch.pPrivInstrGC);
            STAMR3RegisterF(pVM, &pPatchRec->patch.cInvalidWrites,  STAMTYPE_U32, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, NULL, "/PATM/Stats/PatchBD/0x%RRv/cInvalidWrites", pPatchRec->patch.pPrivInstrGC);
            STAMR3RegisterF(pVM, &pPatchRec->patch.cTraps,          STAMTYPE_U32, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, NULL, "/PATM/Stats/PatchBD/0x%RRv/cTraps", pPatchRec->patch.pPrivInstrGC);
            STAMR3RegisterF(pVM, &pPatchRec->patch.flags,           STAMTYPE_X32, STAMVISIBILITY_ALWAYS, STAMUNIT_NONE,       NULL, "/PATM/Stats/PatchBD/0x%RRv/flags", pPatchRec->patch.pPrivInstrGC);
            STAMR3RegisterF(pVM, &pPatchRec->patch.nrJumpRecs,      STAMTYPE_U32, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, NULL, "/PATM/Stats/PatchBD/0x%RRv/nrJumpRecs", pPatchRec->patch.pPrivInstrGC);
            STAMR3RegisterF(pVM, &pPatchRec->patch.nrFixups,        STAMTYPE_U32, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, NULL, "/PATM/Stats/PatchBD/0x%RRv/nrFixups", pPatchRec->patch.pPrivInstrGC);
            STAMR3RegisterF(pVM, &pPatchRec->patch.opcode,          STAMTYPE_U32, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, NULL, "/PATM/Stats/PatchBD/0x%RRv/opcode", pPatchRec->patch.pPrivInstrGC);
            STAMR3RegisterF(pVM, &pPatchRec->patch.uOldState,       STAMTYPE_U32, STAMVISIBILITY_ALWAYS, STAMUNIT_NONE,       NULL, "/PATM/Stats/PatchBD/0x%RRv/uOldState", pPatchRec->patch.pPrivInstrGC);
            STAMR3RegisterF(pVM, &pPatchRec->patch.uOpMode,         STAMTYPE_U32, STAMVISIBILITY_ALWAYS, STAMUNIT_NONE,       NULL, "/PATM/Stats/PatchBD/0x%RRv/uOpMode", pPatchRec->patch.pPrivInstrGC);
            /// @todo change the state to be a callback so we can get a state mnemonic instead.
            STAMR3RegisterF(pVM, &pPatchRec->patch.uState,          STAMTYPE_U32, STAMVISIBILITY_ALWAYS, STAMUNIT_NONE,       NULL, "/PATM/Stats/PatchBD/0x%RRv/uState", pPatchRec->patch.pPrivInstrGC);
#endif
        }
#endif
    }
    return rc;
}

/**
 * Query instruction size
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch record
 * @param   pInstrGC    Instruction address
 */
static uint32_t patmGetInstrSize(PVM pVM, PPATCHINFO pPatch, RTRCPTR pInstrGC)
{
    uint8_t *pInstrHC;

    int rc = PGMPhysGCPtr2R3Ptr(VMMGetCpu0(pVM), pInstrGC, (PRTR3PTR)&pInstrHC);
    if (rc == VINF_SUCCESS)
    {
        DISCPUSTATE cpu;
        bool        disret;
        uint32_t    opsize;

        cpu.mode = (pPatch->flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;
        disret = PATMR3DISInstr(pVM, pPatch, &cpu, pInstrGC, pInstrHC, &opsize, NULL, PATMREAD_ORGCODE | PATMREAD_NOCHECK);
        if (disret)
            return opsize;
    }
    return 0;
}

/**
 * Add patch to page record
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPage       Page address
 * @param   pPatch      Patch record
 */
int patmAddPatchToPage(PVM pVM, RTRCUINTPTR pPage, PPATCHINFO pPatch)
{
    PPATMPATCHPAGE pPatchPage;
    int            rc;

    Log(("patmAddPatchToPage: insert patch %RHv to page %RRv\n", pPatch, pPage));

    pPatchPage = (PPATMPATCHPAGE)RTAvloU32Get(&pVM->patm.s.PatchLookupTreeHC->PatchTreeByPage, pPage);
    if (pPatchPage)
    {
        Assert(pPatchPage->cCount <= pPatchPage->cMaxPatches);
        if (pPatchPage->cCount == pPatchPage->cMaxPatches)
        {
            uint32_t    cMaxPatchesOld = pPatchPage->cMaxPatches;
            PPATCHINFO *paPatchOld     = pPatchPage->aPatch;

            pPatchPage->cMaxPatches += PATMPATCHPAGE_PREALLOC_INCREMENT;
            rc = MMHyperAlloc(pVM, sizeof(PPATCHINFO)*pPatchPage->cMaxPatches, 0, MM_TAG_PATM_PATCH, (void **)&pPatchPage->aPatch);
            if (RT_FAILURE(rc))
            {
                Log(("Out of memory!!!!\n"));
                return VERR_NO_MEMORY;
            }
            memcpy(pPatchPage->aPatch, paPatchOld, cMaxPatchesOld*sizeof(PPATCHINFO));
            MMHyperFree(pVM, paPatchOld);
        }
        pPatchPage->aPatch[pPatchPage->cCount] = pPatch;
        pPatchPage->cCount++;
    }
    else
    {
        rc = MMHyperAlloc(pVM, sizeof(PATMPATCHPAGE), 0, MM_TAG_PATM_PATCH, (void **)&pPatchPage);
        if (RT_FAILURE(rc))
        {
            Log(("Out of memory!!!!\n"));
            return VERR_NO_MEMORY;
        }
        pPatchPage->Core.Key    = pPage;
        pPatchPage->cCount      = 1;
        pPatchPage->cMaxPatches = PATMPATCHPAGE_PREALLOC_INCREMENT;

        rc = MMHyperAlloc(pVM, sizeof(PPATCHINFO)*PATMPATCHPAGE_PREALLOC_INCREMENT, 0, MM_TAG_PATM_PATCH, (void **)&pPatchPage->aPatch);
        if (RT_FAILURE(rc))
        {
            Log(("Out of memory!!!!\n"));
            MMHyperFree(pVM, pPatchPage);
            return VERR_NO_MEMORY;
        }
        pPatchPage->aPatch[0] = pPatch;

        rc = RTAvloU32Insert(&pVM->patm.s.PatchLookupTreeHC->PatchTreeByPage, &pPatchPage->Core);
        Assert(rc);
        pVM->patm.s.cPageRecords++;

        STAM_COUNTER_INC(&pVM->patm.s.StatPatchPageInserted);
    }
    CSAMR3MonitorPage(pVM, pPage, CSAM_TAG_PATM);

    /* Get the closest guest instruction (from below) */
    PRECGUESTTOPATCH pGuestToPatchRec = (PRECGUESTTOPATCH)RTAvlU32GetBestFit(&pPatch->Guest2PatchAddrTree, pPage, true);
    Assert(pGuestToPatchRec);
    if (pGuestToPatchRec)
    {
        LogFlow(("patmAddPatchToPage: lowest patch page address %RRv current lowest %RRv\n", pGuestToPatchRec->Core.Key, pPatchPage->pLowestAddrGC));
        if (    pPatchPage->pLowestAddrGC == 0
            ||  pPatchPage->pLowestAddrGC > (RTRCPTR)pGuestToPatchRec->Core.Key)
        {
            RTRCUINTPTR offset;

            pPatchPage->pLowestAddrGC = (RTRCPTR)pGuestToPatchRec->Core.Key;

            offset = pPatchPage->pLowestAddrGC & PAGE_OFFSET_MASK;
            /* If we're too close to the page boundary, then make sure an instruction from the previous page doesn't cross the boundary itself. */
            if (offset && offset < MAX_INSTR_SIZE)
            {
                /* Get the closest guest instruction (from above) */
                pGuestToPatchRec = (PRECGUESTTOPATCH)RTAvlU32GetBestFit(&pPatch->Guest2PatchAddrTree, pPage-1, false);

                if (pGuestToPatchRec)
                {
                    uint32_t size = patmGetInstrSize(pVM, pPatch, (RTRCPTR)pGuestToPatchRec->Core.Key);
                    if ((RTRCUINTPTR)pGuestToPatchRec->Core.Key + size  > pPage)
                    {
                        pPatchPage->pLowestAddrGC = pPage;
                        LogFlow(("patmAddPatchToPage: new lowest %RRv\n", pPatchPage->pLowestAddrGC));
                    }
                }
            }
        }
    }

    /* Get the closest guest instruction (from above) */
    pGuestToPatchRec = (PRECGUESTTOPATCH)RTAvlU32GetBestFit(&pPatch->Guest2PatchAddrTree, pPage+PAGE_SIZE-1, false);
    Assert(pGuestToPatchRec);
    if (pGuestToPatchRec)
    {
        LogFlow(("patmAddPatchToPage: highest patch page address %RRv current lowest %RRv\n", pGuestToPatchRec->Core.Key, pPatchPage->pHighestAddrGC));
        if (    pPatchPage->pHighestAddrGC == 0
            ||  pPatchPage->pHighestAddrGC <= (RTRCPTR)pGuestToPatchRec->Core.Key)
        {
            pPatchPage->pHighestAddrGC = (RTRCPTR)pGuestToPatchRec->Core.Key;
            /* Increase by instruction size. */
            uint32_t size = patmGetInstrSize(pVM, pPatch, pPatchPage->pHighestAddrGC);
////            Assert(size);
            pPatchPage->pHighestAddrGC += size;
            LogFlow(("patmAddPatchToPage: new highest %RRv\n", pPatchPage->pHighestAddrGC));
        }
    }

    return VINF_SUCCESS;
}

/**
 * Remove patch from page record
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPage       Page address
 * @param   pPatch      Patch record
 */
int patmRemovePatchFromPage(PVM pVM, RTRCUINTPTR pPage, PPATCHINFO pPatch)
{
    PPATMPATCHPAGE pPatchPage;
    int            rc;

    pPatchPage = (PPATMPATCHPAGE)RTAvloU32Get(&pVM->patm.s.PatchLookupTreeHC->PatchTreeByPage, pPage);
    Assert(pPatchPage);

    if (!pPatchPage)
        return VERR_INVALID_PARAMETER;

    Assert(pPatchPage->cCount <= pPatchPage->cMaxPatches);

    Log(("patmRemovePatchPage: remove patch %RHv from page %RRv\n", pPatch, pPage));
    if (pPatchPage->cCount > 1)
    {
        uint32_t i;

        /* Used by multiple patches */
        for (i=0;i<pPatchPage->cCount;i++)
        {
            if (pPatchPage->aPatch[i] == pPatch)
            {
                pPatchPage->aPatch[i] = 0;
                break;
            }
        }
        /* close the gap between the remaining pointers. */
        if (i < pPatchPage->cCount - 1)
        {
            memcpy(&pPatchPage->aPatch[i], &pPatchPage->aPatch[i+1], sizeof(PPATCHINFO)*(pPatchPage->cCount - (i+1)));
        }
        AssertMsg(i < pPatchPage->cCount, ("Unable to find patch %RHv in page %RRv\n", pPatch, pPage));

        pPatchPage->cCount--;
    }
    else
    {
        PPATMPATCHPAGE pPatchNode;

        Log(("patmRemovePatchFromPage %RRv\n", pPage));

        STAM_COUNTER_INC(&pVM->patm.s.StatPatchPageRemoved);
        pPatchNode = (PPATMPATCHPAGE)RTAvloU32Remove(&pVM->patm.s.PatchLookupTreeHC->PatchTreeByPage, pPage);
        Assert(pPatchNode && pPatchNode == pPatchPage);

        Assert(pPatchPage->aPatch);
        rc = MMHyperFree(pVM, pPatchPage->aPatch);
        AssertRC(rc);
        rc = MMHyperFree(pVM, pPatchPage);
        AssertRC(rc);
        pVM->patm.s.cPageRecords--;
    }
    return VINF_SUCCESS;
}

/**
 * Insert page records for all guest pages that contain instructions that were recompiled for this patch
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch record
 */
int patmInsertPatchPages(PVM pVM, PPATCHINFO pPatch)
{
    int           rc;
    RTRCUINTPTR pPatchPageStart, pPatchPageEnd, pPage;

    /* Insert the pages that contain patched instructions into a lookup tree for detecting self-modifying code. */
    pPatchPageStart = (RTRCUINTPTR)pPatch->pInstrGCLowest  & PAGE_BASE_GC_MASK;
    pPatchPageEnd   = (RTRCUINTPTR)pPatch->pInstrGCHighest & PAGE_BASE_GC_MASK;

    /** @todo optimize better (large gaps between current and next used page) */
    for(pPage = pPatchPageStart; pPage <= pPatchPageEnd; pPage += PAGE_SIZE)
    {
        /* Get the closest guest instruction (from above) */
        PRECGUESTTOPATCH pGuestToPatchRec = (PRECGUESTTOPATCH)RTAvlU32GetBestFit(&pPatch->Guest2PatchAddrTree, pPage, true);
        if (    pGuestToPatchRec
            &&  PAGE_ADDRESS(pGuestToPatchRec->Core.Key) == PAGE_ADDRESS(pPage)
           )
        {
            /* Code in page really patched -> add record */
            rc = patmAddPatchToPage(pVM, pPage, pPatch);
            AssertRC(rc);
        }
    }
    pPatch->flags |= PATMFL_CODE_MONITORED;
    return VINF_SUCCESS;
}

/**
 * Remove page records for all guest pages that contain instructions that were recompiled for this patch
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch record
 */
int patmRemovePatchPages(PVM pVM, PPATCHINFO pPatch)
{
    int         rc;
    RTRCUINTPTR pPatchPageStart, pPatchPageEnd, pPage;

    /* Insert the pages that contain patched instructions into a lookup tree for detecting self-modifying code. */
    pPatchPageStart = (RTRCUINTPTR)pPatch->pInstrGCLowest  & PAGE_BASE_GC_MASK;
    pPatchPageEnd   = (RTRCUINTPTR)pPatch->pInstrGCHighest & PAGE_BASE_GC_MASK;

    for(pPage = pPatchPageStart; pPage <= pPatchPageEnd; pPage += PAGE_SIZE)
    {
        /* Get the closest guest instruction (from above) */
        PRECGUESTTOPATCH pGuestToPatchRec = (PRECGUESTTOPATCH)RTAvlU32GetBestFit(&pPatch->Guest2PatchAddrTree, pPage, true);
        if (    pGuestToPatchRec
            &&  PAGE_ADDRESS(pGuestToPatchRec->Core.Key) == PAGE_ADDRESS(pPage) /** @todo bird: PAGE_ADDRESS is for the current context really. check out these. */
           )
        {
            /* Code in page really patched -> remove record */
            rc = patmRemovePatchFromPage(pVM, pPage, pPatch);
            AssertRC(rc);
        }
    }
    pPatch->flags &= ~PATMFL_CODE_MONITORED;
    return VINF_SUCCESS;
}

/**
 * Notifies PATM about a (potential) write to code that has been patched.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   GCPtr       GC pointer to write address
 * @param   cbWrite     Nr of bytes to write
 *
 */
VMMR3DECL(int) PATMR3PatchWrite(PVM pVM, RTRCPTR GCPtr, uint32_t cbWrite)
{
    RTRCUINTPTR          pWritePageStart, pWritePageEnd, pPage;

    Log(("PATMR3PatchWrite %RRv %x\n", GCPtr, cbWrite));

    Assert(VM_IS_EMT(pVM));

    /* Quick boundary check */
    if (    GCPtr < pVM->patm.s.pPatchedInstrGCLowest
        ||  GCPtr > pVM->patm.s.pPatchedInstrGCHighest
       )
       return VINF_SUCCESS;

    STAM_PROFILE_ADV_START(&pVM->patm.s.StatPatchWrite, a);

    pWritePageStart =  (RTRCUINTPTR)GCPtr & PAGE_BASE_GC_MASK;
    pWritePageEnd   = ((RTRCUINTPTR)GCPtr + cbWrite - 1) & PAGE_BASE_GC_MASK;

    for (pPage = pWritePageStart; pPage <= pWritePageEnd; pPage += PAGE_SIZE)
    {
loop_start:
        PPATMPATCHPAGE pPatchPage = (PPATMPATCHPAGE)RTAvloU32Get(&pVM->patm.s.PatchLookupTreeHC->PatchTreeByPage, (RTRCPTR)pPage);
        if (pPatchPage)
        {
            uint32_t i;
            bool fValidPatchWrite = false;

            /* Quick check to see if the write is in the patched part of the page */
            if (    pPatchPage->pLowestAddrGC  > (RTRCPTR)((RTRCUINTPTR)GCPtr + cbWrite - 1)
                ||  pPatchPage->pHighestAddrGC < GCPtr)
            {
                break;
            }

            for (i=0;i<pPatchPage->cCount;i++)
            {
                if (pPatchPage->aPatch[i])
                {
                    PPATCHINFO pPatch = pPatchPage->aPatch[i];
                    RTRCPTR pPatchInstrGC;
                    //unused: bool    fForceBreak = false;

                    Assert(pPatchPage->aPatch[i]->flags & PATMFL_CODE_MONITORED);
                    /** @todo inefficient and includes redundant checks for multiple pages. */
                    for (uint32_t j=0; j<cbWrite; j++)
                    {
                        RTRCPTR pGuestPtrGC = (RTRCPTR)((RTRCUINTPTR)GCPtr + j);

                        if (    pPatch->cbPatchJump
                            &&  pGuestPtrGC >= pPatch->pPrivInstrGC
                            &&  pGuestPtrGC < pPatch->pPrivInstrGC + pPatch->cbPatchJump)
                        {
                            /* The guest is about to overwrite the 5 byte jump to patch code. Remove the patch. */
                            Log(("PATMR3PatchWrite: overwriting jump to patch code -> remove patch.\n"));
                            int rc = PATMR3RemovePatch(pVM, pPatch->pPrivInstrGC);
                            if (rc == VINF_SUCCESS)
                                /** @note jump back to the start as the pPatchPage has been deleted or changed */
                                goto loop_start;

                            continue;
                        }

                        /* Find the closest instruction from below; the above quick check ensured that we are indeed in patched code */
                        pPatchInstrGC = patmGuestGCPtrToPatchGCPtr(pVM, pPatch, pGuestPtrGC);
                        if (!pPatchInstrGC)
                        {
                            RTRCPTR  pClosestInstrGC;
                            uint32_t size;

                            pPatchInstrGC   = patmGuestGCPtrToClosestPatchGCPtr(pVM, pPatch, pGuestPtrGC);
                            if (pPatchInstrGC)
                            {
                                pClosestInstrGC = patmPatchGCPtr2GuestGCPtr(pVM, pPatch, pPatchInstrGC);
                                Assert(pClosestInstrGC <= pGuestPtrGC);
                                size            = patmGetInstrSize(pVM, pPatch, pClosestInstrGC);
                                /* Check if this is not a write into a gap between two patches */
                                if (pClosestInstrGC + size - 1 < pGuestPtrGC)
                                    pPatchInstrGC = 0;
                            }
                        }
                        if (pPatchInstrGC)
                        {
                            uint32_t PatchOffset = pPatchInstrGC - pVM->patm.s.pPatchMemGC;  /* Offset in memory reserved for PATM. */

                            fValidPatchWrite = true;

                            PRECPATCHTOGUEST pPatchToGuestRec = (PRECPATCHTOGUEST)RTAvlU32Get(&pPatch->Patch2GuestAddrTree, PatchOffset);
                            Assert(pPatchToGuestRec);
                            if (pPatchToGuestRec && !pPatchToGuestRec->fDirty)
                            {
                                Log(("PATMR3PatchWrite: Found patched instruction %RRv -> %RRv\n", pGuestPtrGC, pPatchInstrGC));

                                if (++pPatch->cCodeWrites > PATM_MAX_CODE_WRITES)
                                {
                                    LogRel(("PATM: Disable block at %RRv - write %RRv-%RRv\n", pPatch->pPrivInstrGC, pGuestPtrGC, pGuestPtrGC+cbWrite));

                                    PATMR3MarkDirtyPatch(pVM, pPatch);

                                    /** @note jump back to the start as the pPatchPage has been deleted or changed */
                                    goto loop_start;
                                }
                                else
                                {
                                    /* Replace the patch instruction with a breakpoint; when it's hit, then we'll attempt to recompile the instruction again. */
                                    uint8_t *pInstrHC = patmPatchGCPtr2PatchHCPtr(pVM, pPatchInstrGC);

                                    pPatchToGuestRec->u8DirtyOpcode = *pInstrHC;
                                    pPatchToGuestRec->fDirty   = true;

                                    *pInstrHC = 0xCC;

                                    STAM_COUNTER_INC(&pVM->patm.s.StatInstrDirty);
                                }
                            }
                            /* else already marked dirty */
                        }
                    }
                }
            } /* for each patch */

            if (fValidPatchWrite == false)
            {
                /* Write to a part of the page that either:
                 * - doesn't contain any code (shared code/data); rather unlikely
                 * - old code page that's no longer in active use.
                 */
invalid_write_loop_start:
                pPatchPage = (PPATMPATCHPAGE)RTAvloU32Get(&pVM->patm.s.PatchLookupTreeHC->PatchTreeByPage, (RTRCPTR)pPage);

                if (pPatchPage)
                {
                    for (i=0;i<pPatchPage->cCount;i++)
                    {
                        PPATCHINFO pPatch = pPatchPage->aPatch[i];

                        if (pPatch->cInvalidWrites > PATM_MAX_INVALID_WRITES)
                        {
                            /** @note possibly dangerous assumption that all future writes will be harmless. */
                            if (pPatch->flags & PATMFL_IDTHANDLER)
                            {
                                LogRel(("PATM: Stop monitoring IDT handler pages at %RRv - invalid write %RRv-%RRv (this is not a fatal error)\n", pPatch->pPrivInstrGC, GCPtr, GCPtr+cbWrite));

                                Assert(pPatch->flags & PATMFL_CODE_MONITORED);
                                int rc = patmRemovePatchPages(pVM, pPatch);
                                AssertRC(rc);
                            }
                            else
                            {
                                LogRel(("PATM: Disable block at %RRv - invalid write %RRv-%RRv \n", pPatch->pPrivInstrGC, GCPtr, GCPtr+cbWrite));
                                PATMR3MarkDirtyPatch(pVM, pPatch);
                            }
                            /** @note jump back to the start as the pPatchPage has been deleted or changed */
                            goto invalid_write_loop_start;
                        }
                    } /* for */
                }
            }
        }
    }
    STAM_PROFILE_ADV_STOP(&pVM->patm.s.StatPatchWrite, a);
    return VINF_SUCCESS;

}

/**
 * Disable all patches in a flushed page
 *
 * @returns VBox status code
 * @param   pVM         The VM to operate on.
 * @param   addr        GC address of the page to flush
 */
/** @note Currently only called by CSAMR3FlushPage; optimization to avoid having to double check if the physical address has changed
 */
VMMR3DECL(int) PATMR3FlushPage(PVM pVM, RTRCPTR addr)
{
    addr &= PAGE_BASE_GC_MASK;

    PPATMPATCHPAGE pPatchPage = (PPATMPATCHPAGE)RTAvloU32Get(&pVM->patm.s.PatchLookupTreeHC->PatchTreeByPage, addr);
    if (pPatchPage)
    {
        int i;

        /* From top to bottom as the array is modified by PATMR3MarkDirtyPatch. */
        for (i=(int)pPatchPage->cCount-1;i>=0;i--)
        {
            if (pPatchPage->aPatch[i])
            {
                PPATCHINFO pPatch = pPatchPage->aPatch[i];

                Log(("PATMR3FlushPage %RRv remove patch at %RRv\n", addr, pPatch->pPrivInstrGC));
                PATMR3MarkDirtyPatch(pVM, pPatch);
            }
        }
        STAM_COUNTER_INC(&pVM->patm.s.StatFlushed);
    }
    return VINF_SUCCESS;
}

/**
 * Checks if the instructions at the specified address has been patched already.
 *
 * @returns boolean, patched or not
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context pointer to instruction
 */
VMMR3DECL(bool) PATMR3HasBeenPatched(PVM pVM, RTRCPTR pInstrGC)
{
    PPATMPATCHREC pPatchRec;
    pPatchRec = (PPATMPATCHREC)RTAvloU32Get(&pVM->patm.s.PatchLookupTreeHC->PatchTree, pInstrGC);
    if (pPatchRec && pPatchRec->patch.uState == PATCH_ENABLED)
        return true;
    return false;
}

/**
 * Query the opcode of the original code that was overwritten by the 5 bytes patch jump
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    GC address of instr
 * @param   pByte       opcode byte pointer (OUT)
 *
 */
VMMR3DECL(int) PATMR3QueryOpcode(PVM pVM, RTRCPTR pInstrGC, uint8_t *pByte)
{
    PPATMPATCHREC pPatchRec;

    /** @todo this will not work for aliased pages! (never has, but so far not a problem for us) */

    /* Shortcut. */
    if (   !PATMIsEnabled(pVM)
        ||  pInstrGC < pVM->patm.s.pPatchedInstrGCLowest
        ||  pInstrGC > pVM->patm.s.pPatchedInstrGCHighest)
    {
        return VERR_PATCH_NOT_FOUND;
    }

    pPatchRec = (PPATMPATCHREC)RTAvloU32GetBestFit(&pVM->patm.s.PatchLookupTreeHC->PatchTree, pInstrGC, false);
    // if the patch is enabled and the pointer lies within 5 bytes of this priv instr ptr, then we've got a hit!
    if (    pPatchRec
        &&  pPatchRec->patch.uState == PATCH_ENABLED
        &&  pInstrGC >= pPatchRec->patch.pPrivInstrGC
        &&  pInstrGC < pPatchRec->patch.pPrivInstrGC + pPatchRec->patch.cbPatchJump)
    {
        RTRCPTR offset = pInstrGC - pPatchRec->patch.pPrivInstrGC;
        *pByte = pPatchRec->patch.aPrivInstr[offset];

        if (pPatchRec->patch.cbPatchJump == 1)
        {
            Log(("PATMR3QueryOpcode: returning opcode %2X for instruction at %RRv\n", *pByte, pInstrGC));
        }
        STAM_COUNTER_ADD(&pVM->patm.s.StatNrOpcodeRead, 1);
        return VINF_SUCCESS;
    }
    return VERR_PATCH_NOT_FOUND;
}

/**
 * Disable patch for privileged instruction at specified location
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstr      Guest context point to privileged instruction
 *
 * @note    returns failure if patching is not allowed or possible
 *
 */
VMMR3DECL(int) PATMR3DisablePatch(PVM pVM, RTRCPTR pInstrGC)
{
    PPATMPATCHREC pPatchRec;
    PPATCHINFO    pPatch;

    Log(("PATMR3DisablePatch: %RRv\n", pInstrGC));
    pPatchRec = (PPATMPATCHREC)RTAvloU32Get(&pVM->patm.s.PatchLookupTreeHC->PatchTree, pInstrGC);
    if (pPatchRec)
    {
        int rc = VINF_SUCCESS;

        pPatch = &pPatchRec->patch;

        /* Already disabled? */
        if (pPatch->uState == PATCH_DISABLED)
            return VINF_SUCCESS;

        /* Clear the IDT entries for the patch we're disabling. */
        /** @note very important as we clear IF in the patch itself */
        /** @todo this needs to be changed */
        if (pPatch->flags & PATMFL_IDTHANDLER)
        {
            uint32_t iGate;

            iGate = TRPMR3QueryGateByHandler(pVM, PATCHCODE_PTR_GC(pPatch));
            if (iGate != (uint32_t)~0)
            {
                TRPMR3SetGuestTrapHandler(pVM, iGate, TRPM_INVALID_HANDLER);
                if (++cIDTHandlersDisabled < 256)
                    LogRel(("PATM: Disabling IDT %x patch handler %RRv\n", iGate, pInstrGC));
            }
        }

        /* Mark the entry with a breakpoint in case somebody else calls it later on (cli patch used as a function, function, trampoline or idt patches) */
        if (    pPatch->pPatchBlockOffset
            &&  pPatch->uState == PATCH_ENABLED)
        {
            Log(("Invalidate patch at %RRv (HC=%RRv)\n", PATCHCODE_PTR_GC(pPatch), PATCHCODE_PTR_HC(pPatch)));
            pPatch->bDirtyOpcode   = *PATCHCODE_PTR_HC(pPatch);
            *PATCHCODE_PTR_HC(pPatch) = 0xCC;
        }

        /* IDT or function patches haven't changed any guest code.  */
        if (pPatch->flags & PATMFL_PATCHED_GUEST_CODE)
        {
            Assert(pPatch->flags & PATMFL_MUST_INSTALL_PATCHJMP);
            Assert(!(pPatch->flags & (PATMFL_DUPLICATE_FUNCTION|PATMFL_IDTHANDLER|PATMFL_TRAMPOLINE|PATMFL_INT3_REPLACEMENT|PATMFL_INT3_REPLACEMENT_BLOCK)));

            if (pPatch->uState != PATCH_REFUSED)
            {
                AssertMsg(pPatch->pPrivInstrHC, ("Invalid HC pointer?!? (%RRv)\n", pInstrGC));
                Assert(pPatch->cbPatchJump);

                /** pPrivInstrHC is probably not valid anymore */
                rc = PGMPhysGCPtr2R3Ptr(VMMGetCpu0(pVM), pPatchRec->patch.pPrivInstrGC, (PRTR3PTR)&pPatchRec->patch.pPrivInstrHC);
                if (rc == VINF_SUCCESS)
                {
                    uint8_t temp[16];

                    Assert(pPatch->cbPatchJump < sizeof(temp));

                    /* Let's first check if the guest code is still the same. */
                    rc = PGMPhysSimpleReadGCPtr(VMMGetCpu0(pVM), temp, pPatch->pPrivInstrGC, pPatch->cbPatchJump);
                    Assert(rc == VINF_SUCCESS || rc == VERR_PAGE_TABLE_NOT_PRESENT || rc == VERR_PAGE_NOT_PRESENT);
                    if (rc == VINF_SUCCESS)
                    {
                        RTRCINTPTR displ = (RTRCUINTPTR)PATCHCODE_PTR_GC(pPatch) - ((RTRCUINTPTR)pPatch->pPrivInstrGC + SIZEOF_NEARJUMP32);

                        if (    temp[0] != 0xE9 /* jmp opcode */
                            ||  *(RTRCINTPTR *)(&temp[1]) != displ
                           )
                        {
                            Log(("PATMR3DisablePatch: Can't disable a patch who's guest code has changed!!\n"));
                            STAM_COUNTER_INC(&pVM->patm.s.StatOverwritten);
                            /* Remove it completely */
                            pPatch->uState = PATCH_DISABLED;    /* don't call PATMR3DisablePatch again */
                            rc = PATMR3RemovePatch(pVM, pInstrGC);
                            AssertRC(rc);
                            return VWRN_PATCH_REMOVED;
                        }
                    }
                    patmRemoveJumpToPatch(pVM, pPatch);

                }
                else
                {
                    Log(("PATMR3DisablePatch: unable to disable patch -> mark PATCH_DISABLE_PENDING\n"));
                    pPatch->uState = PATCH_DISABLE_PENDING;
                }
            }
            else
            {
                AssertMsgFailed(("Patch was refused!\n"));
                return VERR_PATCH_ALREADY_DISABLED;
            }
        }
        else
        if (pPatch->flags & (PATMFL_INT3_REPLACEMENT|PATMFL_INT3_REPLACEMENT_BLOCK))
        {
            uint8_t temp[16];

            Assert(pPatch->cbPatchJump < sizeof(temp));

            /* Let's first check if the guest code is still the same. */
            rc = PGMPhysSimpleReadGCPtr(VMMGetCpu0(pVM), temp, pPatch->pPrivInstrGC, pPatch->cbPatchJump);
            Assert(rc == VINF_SUCCESS || rc == VERR_PAGE_TABLE_NOT_PRESENT || rc == VERR_PAGE_NOT_PRESENT);
            if (rc == VINF_SUCCESS)
            {
                if (temp[0] != 0xCC)
                {
                    Log(("PATMR3DisablePatch: Can't disable a patch who's guest code has changed!!\n"));
                    STAM_COUNTER_INC(&pVM->patm.s.StatOverwritten);
                    /* Remove it completely */
                    pPatch->uState = PATCH_DISABLED;    /* don't call PATMR3DisablePatch again */
                    rc = PATMR3RemovePatch(pVM, pInstrGC);
                    AssertRC(rc);
                    return VWRN_PATCH_REMOVED;
                }
                patmDeactivateInt3Patch(pVM, pPatch);
            }
        }

        if (rc == VINF_SUCCESS)
        {
            /* Save old state and mark this one as disabled (so it can be enabled later on). */
            if (pPatch->uState == PATCH_DISABLE_PENDING)
            {
                /* Just to be safe, let's make sure this one can never be reused; the patch might be marked dirty already (int3 at start) */
                pPatch->uState    = PATCH_UNUSABLE;
            }
            else
            if (pPatch->uState != PATCH_DIRTY)
            {
                pPatch->uOldState = pPatch->uState;
                pPatch->uState    = PATCH_DISABLED;
            }
            STAM_COUNTER_ADD(&pVM->patm.s.StatDisabled, 1);
        }

        Log(("PATMR3DisablePatch: disabled patch at %RRv\n", pInstrGC));
        return VINF_SUCCESS;
    }
    Log(("Patch not found!\n"));
    return VERR_PATCH_NOT_FOUND;
}

/**
 * Permanently disable patch for privileged instruction at specified location
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstr      Guest context instruction pointer
 * @param   pConflictAddr  Guest context pointer which conflicts with specified patch
 * @param   pConflictPatch Conflicting patch
 *
 */
static int patmDisableUnusablePatch(PVM pVM, RTRCPTR pInstrGC, RTRCPTR pConflictAddr, PPATCHINFO pConflictPatch)
{
#ifdef PATM_RESOLVE_CONFLICTS_WITH_JUMP_PATCHES
    PATCHINFO            patch = {0};
    DISCPUSTATE          cpu;
    R3PTRTYPE(uint8_t *) pInstrHC;
    uint32_t             opsize;
    bool                 disret;
    int                  rc;

    pInstrHC = PATMGCVirtToHCVirt(pVM, &patch, pInstrGC);
    cpu.mode = (pConflictPatch->flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;
    disret = PATMR3DISInstr(pVM, &patch, &cpu, pInstrGC, pInstrHC, &opsize, NULL);
    /*
     * If it's a 5 byte relative jump, then we can work around the problem by replacing the 32 bits relative offset
     * with one that jumps right into the conflict patch.
     * Otherwise we must disable the conflicting patch to avoid serious problems.
     */
    if (    disret == true
        && (pConflictPatch->flags & PATMFL_CODE32)
        && (cpu.pCurInstr->opcode == OP_JMP || (cpu.pCurInstr->optype & OPTYPE_COND_CONTROLFLOW))
        && (cpu.param1.flags & USE_IMMEDIATE32_REL))
    {
        /* Hint patches must be enabled first. */
        if (pConflictPatch->flags & PATMFL_INSTR_HINT)
        {
            Log(("Enabling HINTED patch %RRv\n", pConflictPatch->pPrivInstrGC));
            pConflictPatch->flags &= ~PATMFL_INSTR_HINT;
            rc = PATMR3EnablePatch(pVM, pConflictPatch->pPrivInstrGC);
            Assert(rc == VINF_SUCCESS || rc == VERR_PATCH_NOT_FOUND);
            /* Enabling might fail if the patched code has changed in the meantime. */
            if (rc != VINF_SUCCESS)
                return rc;
        }

        rc = PATMR3InstallPatch(pVM, pInstrGC, PATMFL_CODE32 | PATMFL_JUMP_CONFLICT);
        if (RT_SUCCESS(rc))
        {
            Log(("PATM -> CONFLICT: Installed JMP patch for patch conflict at %RRv\n", pInstrGC));
            STAM_COUNTER_INC(&pVM->patm.s.StatFixedConflicts);
            return VINF_SUCCESS;
        }
    }
#endif

    if (pConflictPatch->opcode == OP_CLI)
    {
        /* Turn it into an int3 patch; our GC trap handler will call the generated code manually. */
        Log(("PATM -> CONFLICT: Found active patch at instruction %RRv with target %RRv -> turn into int 3 patch!!\n", pInstrGC, pConflictPatch->pPrivInstrGC));
        int rc =  PATMR3DisablePatch(pVM, pConflictPatch->pPrivInstrGC);
        if (rc == VWRN_PATCH_REMOVED)
            return VINF_SUCCESS;
        if (RT_SUCCESS(rc))
        {
            pConflictPatch->flags &= ~(PATMFL_MUST_INSTALL_PATCHJMP|PATMFL_INSTR_HINT);
            pConflictPatch->flags |= PATMFL_INT3_REPLACEMENT_BLOCK;
            rc = PATMR3EnablePatch(pVM, pConflictPatch->pPrivInstrGC);
            if (rc == VERR_PATCH_NOT_FOUND)
                return VINF_SUCCESS;    /* removed already */

            AssertRC(rc);
            if (RT_SUCCESS(rc))
            {
                STAM_COUNTER_INC(&pVM->patm.s.StatInt3Callable);
                return VINF_SUCCESS;
            }
        }
        /* else turned into unusable patch (see below) */
    }
    else
    {
        Log(("PATM -> CONFLICT: Found active patch at instruction %RRv with target %RRv -> DISABLING it!!\n", pInstrGC, pConflictPatch->pPrivInstrGC));
        int rc = PATMR3DisablePatch(pVM, pConflictPatch->pPrivInstrGC);
        if (rc == VWRN_PATCH_REMOVED)
            return VINF_SUCCESS;
    }

    /* No need to monitor the code anymore. */
    if (pConflictPatch->flags & PATMFL_CODE_MONITORED)
    {
        int rc = patmRemovePatchPages(pVM, pConflictPatch);
        AssertRC(rc);
    }
    pConflictPatch->uState = PATCH_UNUSABLE;
    STAM_COUNTER_INC(&pVM->patm.s.StatUnusable);
    return VERR_PATCH_DISABLED;
}

/**
 * Enable patch for privileged instruction at specified location
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstr      Guest context point to privileged instruction
 *
 * @note    returns failure if patching is not allowed or possible
 *
 */
VMMR3DECL(int) PATMR3EnablePatch(PVM pVM, RTRCPTR pInstrGC)
{
    PPATMPATCHREC pPatchRec;
    PPATCHINFO    pPatch;

    Log(("PATMR3EnablePatch %RRv\n", pInstrGC));
    pPatchRec = (PPATMPATCHREC)RTAvloU32Get(&pVM->patm.s.PatchLookupTreeHC->PatchTree, pInstrGC);
    if (pPatchRec)
    {
        int rc = VINF_SUCCESS;

        pPatch = &pPatchRec->patch;

        if (pPatch->uState == PATCH_DISABLED)
        {
            if (pPatch->flags & PATMFL_MUST_INSTALL_PATCHJMP)
            {
                Assert(!(pPatch->flags & PATMFL_PATCHED_GUEST_CODE));
                /** @todo -> pPrivInstrHC is probably not valid anymore */
                rc = PGMPhysGCPtr2R3Ptr(VMMGetCpu0(pVM), pPatchRec->patch.pPrivInstrGC, (PRTR3PTR)&pPatchRec->patch.pPrivInstrHC);
                if (rc == VINF_SUCCESS)
                {
#ifdef DEBUG
                    DISCPUSTATE cpu;
                    char szOutput[256];
                    uint32_t opsize, i = 0;
#endif
                    uint8_t temp[16];

                    Assert(pPatch->cbPatchJump < sizeof(temp));

                    // let's first check if the guest code is still the same
                    int rc = PGMPhysSimpleReadGCPtr(VMMGetCpu0(pVM), temp, pPatch->pPrivInstrGC, pPatch->cbPatchJump);
                    AssertRC(rc);

                    if (memcmp(temp, pPatch->aPrivInstr, pPatch->cbPatchJump))
                    {
                        Log(("PATMR3EnablePatch: Can't enable a patch who's guest code has changed!!\n"));
                        STAM_COUNTER_INC(&pVM->patm.s.StatOverwritten);
                        /* Remove it completely */
                        rc = PATMR3RemovePatch(pVM, pInstrGC);
                        AssertRC(rc);
                        return VERR_PATCH_NOT_FOUND;
                    }

                    rc = patmGenJumpToPatch(pVM, pPatch, false);
                    AssertRC(rc);
                    if (RT_FAILURE(rc))
                        return rc;

#ifdef DEBUG
                    bool disret;
                    i = 0;
                    while(i < pPatch->cbPatchJump)
                    {
                        cpu.mode = (pPatch->flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;
                        disret = PATMR3DISInstr(pVM, pPatch, &cpu, pPatch->pPrivInstrGC + i, &pPatch->pPrivInstrHC[i], &opsize, szOutput);
                        Log(("Renewed patch instr: %s", szOutput));
                        i += opsize;
                    }
#endif
                }
            }
            else
            if (pPatch->flags & (PATMFL_INT3_REPLACEMENT|PATMFL_INT3_REPLACEMENT_BLOCK))
            {
                uint8_t temp[16];

                Assert(pPatch->cbPatchJump < sizeof(temp));

                /* Let's first check if the guest code is still the same. */
                int rc = PGMPhysSimpleReadGCPtr(VMMGetCpu0(pVM), temp, pPatch->pPrivInstrGC, pPatch->cbPatchJump);
                AssertRC(rc);

                if (memcmp(temp, pPatch->aPrivInstr, pPatch->cbPatchJump))
                {
                    Log(("PATMR3EnablePatch: Can't enable a patch who's guest code has changed!!\n"));
                    STAM_COUNTER_INC(&pVM->patm.s.StatOverwritten);
                    rc = PATMR3RemovePatch(pVM, pInstrGC);
                    AssertRC(rc);
                    return VERR_PATCH_NOT_FOUND;
                }

                rc = patmActivateInt3Patch(pVM, pPatch);
                if (RT_FAILURE(rc))
                    return rc;
            }

            pPatch->uState = pPatch->uOldState; //restore state

            /* Restore the entry breakpoint with the original opcode (see PATMR3DisablePatch). */
            if (pPatch->pPatchBlockOffset)
            {
                *PATCHCODE_PTR_HC(pPatch) = pPatch->bDirtyOpcode;
            }

            STAM_COUNTER_ADD(&pVM->patm.s.StatEnabled, 1);
        }
        else
            Log(("PATMR3EnablePatch: Unable to enable patch %RRv with state %d\n", pInstrGC, pPatch->uState));

        return rc;
    }
    return VERR_PATCH_NOT_FOUND;
}

/**
 * Remove patch for privileged instruction at specified location
 *
 * @returns VBox status code.
 * @param   pVM             The VM to operate on.
 * @param   pPatchRec       Patch record
 * @param   fForceRemove    Remove *all* patches
 */
int PATMRemovePatch(PVM pVM, PPATMPATCHREC pPatchRec, bool fForceRemove)
{
    PPATCHINFO    pPatch;

    pPatch = &pPatchRec->patch;

    /* Strictly forbidden to remove such patches. There can be dependencies!! */
    if (!fForceRemove && (pPatch->flags & (PATMFL_DUPLICATE_FUNCTION|PATMFL_CODE_REFERENCED)))
    {
        Log(("PATMRemovePatch %RRv REFUSED!\n", pPatch->pPrivInstrGC));
        return VERR_ACCESS_DENIED;
    }
    Log(("PATMRemovePatch %RRv\n", pPatch->pPrivInstrGC));

    /** @note NEVER EVER REUSE PATCH MEMORY */
    /** @note PATMR3DisablePatch put a breakpoint (0xCC) at the entry of this patch */

    if (pPatchRec->patch.pPatchBlockOffset)
    {
        PAVLOU32NODECORE pNode;

        pNode = RTAvloU32Remove(&pVM->patm.s.PatchLookupTreeHC->PatchTreeByPatchAddr, pPatchRec->patch.pPatchBlockOffset);
        Assert(pNode);
    }

    if (pPatchRec->patch.flags & PATMFL_CODE_MONITORED)
    {
        int rc = patmRemovePatchPages(pVM, &pPatchRec->patch);
        AssertRC(rc);
    }

#ifdef VBOX_WITH_STATISTICS
    if (PATM_STAT_INDEX_IS_VALID(pPatchRec->patch.uPatchIdx))
    {
        STAMR3Deregister(pVM, &pPatchRec->patch);
#ifndef DEBUG_sandervl
        STAMR3Deregister(pVM, &pVM->patm.s.pStatsHC[pPatchRec->patch.uPatchIdx]);
        STAMR3Deregister(pVM, &pPatchRec->patch.cbPatchBlockSize);
        STAMR3Deregister(pVM, &pPatchRec->patch.cbPatchJump);
        STAMR3Deregister(pVM, &pPatchRec->patch.cbPrivInstr);
        STAMR3Deregister(pVM, &pPatchRec->patch.cCodeWrites);
        STAMR3Deregister(pVM, &pPatchRec->patch.cInvalidWrites);
        STAMR3Deregister(pVM, &pPatchRec->patch.cTraps);
        STAMR3Deregister(pVM, &pPatchRec->patch.flags);
        STAMR3Deregister(pVM, &pPatchRec->patch.nrJumpRecs);
        STAMR3Deregister(pVM, &pPatchRec->patch.nrFixups);
        STAMR3Deregister(pVM, &pPatchRec->patch.opcode);
        STAMR3Deregister(pVM, &pPatchRec->patch.uState);
        STAMR3Deregister(pVM, &pPatchRec->patch.uOldState);
        STAMR3Deregister(pVM, &pPatchRec->patch.uOpMode);
#endif
    }
#endif

    /** @note no need to free Guest2PatchAddrTree as those records share memory with Patch2GuestAddrTree records. */
    patmEmptyTreeU32(pVM, &pPatch->Patch2GuestAddrTree);
    pPatch->nrPatch2GuestRecs = 0;
    Assert(pPatch->Patch2GuestAddrTree == 0);

    patmEmptyTree(pVM, &pPatch->FixupTree);
    pPatch->nrFixups = 0;
    Assert(pPatch->FixupTree == 0);

    if (pPatchRec->patch.pTempInfo)
        MMR3HeapFree(pPatchRec->patch.pTempInfo);

    /** @note might fail, because it has already been removed (e.g. during reset). */
    RTAvloU32Remove(&pVM->patm.s.PatchLookupTreeHC->PatchTree, pPatchRec->Core.Key);

    /* Free the patch record */
    MMHyperFree(pVM, pPatchRec);
    return VINF_SUCCESS;
}

/**
 * Attempt to refresh the patch by recompiling its entire code block
 *
 * @returns VBox status code.
 * @param   pVM             The VM to operate on.
 * @param   pPatchRec       Patch record
 */
int patmR3RefreshPatch(PVM pVM, PPATMPATCHREC pPatchRec)
{
    PPATCHINFO  pPatch;
    int         rc;
    RTRCPTR     pInstrGC = pPatchRec->patch.pPrivInstrGC;

    Log(("patmR3RefreshPatch: attempt to refresh patch at %RRv\n", pInstrGC));

    pPatch = &pPatchRec->patch;
    AssertReturn(pPatch->flags & (PATMFL_DUPLICATE_FUNCTION|PATMFL_IDTHANDLER|PATMFL_TRAPHANDLER), VERR_PATCHING_REFUSED);
    if (pPatch->flags & PATMFL_EXTERNAL_JUMP_INSIDE)
    {
        Log(("patmR3RefreshPatch: refused because external jumps to this patch exist\n"));
        return VERR_PATCHING_REFUSED;
    }

    /** Note: quite ugly to enable/disable/remove/insert old and new patches, but there's no easy way around it. */

    rc = PATMR3DisablePatch(pVM, pInstrGC);
    AssertRC(rc);

    /** Kick it out of the lookup tree to make sure PATMR3InstallPatch doesn't fail (hack alert) */
    RTAvloU32Remove(&pVM->patm.s.PatchLookupTreeHC->PatchTree, pPatchRec->Core.Key);
#ifdef VBOX_WITH_STATISTICS
    if (PATM_STAT_INDEX_IS_VALID(pPatchRec->patch.uPatchIdx))
    {
        STAMR3Deregister(pVM, &pPatchRec->patch);
#ifndef DEBUG_sandervl
        STAMR3Deregister(pVM, &pVM->patm.s.pStatsHC[pPatchRec->patch.uPatchIdx]);
        STAMR3Deregister(pVM, &pPatchRec->patch.cbPatchBlockSize);
        STAMR3Deregister(pVM, &pPatchRec->patch.cbPatchJump);
        STAMR3Deregister(pVM, &pPatchRec->patch.cbPrivInstr);
        STAMR3Deregister(pVM, &pPatchRec->patch.cCodeWrites);
        STAMR3Deregister(pVM, &pPatchRec->patch.cInvalidWrites);
        STAMR3Deregister(pVM, &pPatchRec->patch.cTraps);
        STAMR3Deregister(pVM, &pPatchRec->patch.flags);
        STAMR3Deregister(pVM, &pPatchRec->patch.nrJumpRecs);
        STAMR3Deregister(pVM, &pPatchRec->patch.nrFixups);
        STAMR3Deregister(pVM, &pPatchRec->patch.opcode);
        STAMR3Deregister(pVM, &pPatchRec->patch.uState);
        STAMR3Deregister(pVM, &pPatchRec->patch.uOldState);
        STAMR3Deregister(pVM, &pPatchRec->patch.uOpMode);
#endif
    }
#endif

    /** Note: We don't attempt to reuse patch memory here as it's quite common that the new code block requires more memory. */

    /* Attempt to install a new patch. */
    rc = PATMR3InstallPatch(pVM, pInstrGC, pPatch->flags & (PATMFL_CODE32|PATMFL_IDTHANDLER|PATMFL_INTHANDLER|PATMFL_TRAPHANDLER|PATMFL_DUPLICATE_FUNCTION|PATMFL_TRAPHANDLER_WITH_ERRORCODE|PATMFL_IDTHANDLER_WITHOUT_ENTRYPOINT));
    if (RT_SUCCESS(rc))
    {
        RTRCPTR         pPatchTargetGC;
        PPATMPATCHREC   pNewPatchRec;

        /* Determine target address in new patch */
        pPatchTargetGC = PATMR3QueryPatchGCPtr(pVM, pInstrGC);
        Assert(pPatchTargetGC);
        if (!pPatchTargetGC)
        {
            rc = VERR_PATCHING_REFUSED;
            goto failure;
        }

        /* Reset offset into patch memory to put the next code blocks right at the beginning. */
        pPatch->uCurPatchOffset   = 0;

        /* insert jump to new patch in old patch block */
        rc = patmPatchGenPatchJump(pVM, pPatch, pInstrGC, pPatchTargetGC, false /* no lookup record */);
        if (RT_FAILURE(rc))
            goto failure;

        pNewPatchRec = (PPATMPATCHREC)RTAvloU32Get(&pVM->patm.s.PatchLookupTreeHC->PatchTree, pInstrGC);
        Assert(pNewPatchRec); /* can't fail */

        /* Remove old patch (only do that when everything is finished) */
        int rc2 = PATMRemovePatch(pVM, pPatchRec, true /* force removal */);
        AssertRC(rc2);

        /* Put the new patch back into the tree, because removing the old one kicked this one out. (hack alert) */
        RTAvloU32Insert(&pVM->patm.s.PatchLookupTreeHC->PatchTree, &pNewPatchRec->Core);

        LogRel(("PATM: patmR3RefreshPatch: succeeded to refresh patch at %RRv \n", pInstrGC));
        STAM_COUNTER_INC(&pVM->patm.s.StatPatchRefreshSuccess);

        /* Used by another patch, so don't remove it! */
        pNewPatchRec->patch.flags |= PATMFL_CODE_REFERENCED;
    }

failure:
    if (RT_FAILURE(rc))
    {
        LogRel(("PATM: patmR3RefreshPatch: failed to refresh patch at %RRv. Reactiving old one. \n", pInstrGC));

        /* Remove the new inactive patch */
        rc = PATMR3RemovePatch(pVM, pInstrGC);
        AssertRC(rc);

        /* Put the old patch back into the tree (or else it won't be saved) (hack alert) */
        RTAvloU32Insert(&pVM->patm.s.PatchLookupTreeHC->PatchTree, &pPatchRec->Core);

        /* Enable again in case the dirty instruction is near the end and there are safe code paths. */
        int rc2 = PATMR3EnablePatch(pVM, pInstrGC);
        AssertRC(rc2);

        STAM_COUNTER_INC(&pVM->patm.s.StatPatchRefreshFailed);
    }
    return rc;
}

/**
 * Find patch for privileged instruction at specified location
 *
 * @returns Patch structure pointer if found; else NULL
 * @param   pVM           The VM to operate on.
 * @param   pInstr        Guest context point to instruction that might lie within 5 bytes of an existing patch jump
 * @param   fIncludeHints Include hinted patches or not
 *
 */
PPATCHINFO PATMFindActivePatchByEntrypoint(PVM pVM, RTRCPTR pInstrGC, bool fIncludeHints)
{
    PPATMPATCHREC pPatchRec = (PPATMPATCHREC)RTAvloU32GetBestFit(&pVM->patm.s.PatchLookupTreeHC->PatchTree, pInstrGC, false);
    /* if the patch is enabled, the pointer is not indentical to the privileged patch ptr and it lies within 5 bytes of this priv instr ptr, then we've got a hit! */
    if (pPatchRec)
    {
        if (    pPatchRec->patch.uState == PATCH_ENABLED
            && (pPatchRec->patch.flags & PATMFL_PATCHED_GUEST_CODE)
            &&  pInstrGC > pPatchRec->patch.pPrivInstrGC
            &&  pInstrGC < pPatchRec->patch.pPrivInstrGC + pPatchRec->patch.cbPatchJump)
        {
            Log(("Found active patch at %RRv (org %RRv)\n", pInstrGC, pPatchRec->patch.pPrivInstrGC));
            return &pPatchRec->patch;
        }
        else
        if (    fIncludeHints
            &&  pPatchRec->patch.uState == PATCH_DISABLED
            &&  (pPatchRec->patch.flags & PATMFL_INSTR_HINT)
            &&  pInstrGC > pPatchRec->patch.pPrivInstrGC
            &&  pInstrGC < pPatchRec->patch.pPrivInstrGC + pPatchRec->patch.cbPatchJump)
        {
            Log(("Found HINT patch at %RRv (org %RRv)\n", pInstrGC, pPatchRec->patch.pPrivInstrGC));
            return &pPatchRec->patch;
        }
    }
    return NULL;
}

/**
 * Checks whether the GC address is inside a generated patch jump
 *
 * @returns true -> yes, false -> no
 * @param   pVM         The VM to operate on.
 * @param   pAddr       Guest context address
 * @param   pPatchAddr  Guest context patch address (if true)
 */
VMMR3DECL(bool) PATMR3IsInsidePatchJump(PVM pVM, RTRCPTR pAddr, PRTGCPTR32 pPatchAddr)
{
    RTRCPTR addr;
    PPATCHINFO pPatch;

    if (PATMIsEnabled(pVM) == false)
        return false;

    if (pPatchAddr == NULL)
        pPatchAddr = &addr;

    *pPatchAddr = 0;

    pPatch = PATMFindActivePatchByEntrypoint(pVM, pAddr);
    if (pPatch)
    {
        *pPatchAddr = pPatch->pPrivInstrGC;
    }
    return *pPatchAddr == 0 ? false : true;
}

/**
 * Remove patch for privileged instruction at specified location
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstr      Guest context point to privileged instruction
 *
 * @note    returns failure if patching is not allowed or possible
 *
 */
VMMR3DECL(int) PATMR3RemovePatch(PVM pVM, RTRCPTR pInstrGC)
{
    PPATMPATCHREC pPatchRec;

    pPatchRec = (PPATMPATCHREC)RTAvloU32Get(&pVM->patm.s.PatchLookupTreeHC->PatchTree, pInstrGC);
    if (pPatchRec)
    {
        int rc = PATMR3DisablePatch(pVM, pInstrGC);
        if (rc == VWRN_PATCH_REMOVED)
            return VINF_SUCCESS;
        return PATMRemovePatch(pVM, pPatchRec, false);
    }
    AssertFailed();
    return VERR_PATCH_NOT_FOUND;
}

/**
 * Mark patch as dirty
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch record
 *
 * @note    returns failure if patching is not allowed or possible
 *
 */
VMMR3DECL(int) PATMR3MarkDirtyPatch(PVM pVM, PPATCHINFO pPatch)
{
    if (pPatch->pPatchBlockOffset)
    {
        Log(("Invalidate patch at %RRv (HC=%RRv)\n", PATCHCODE_PTR_GC(pPatch), PATCHCODE_PTR_HC(pPatch)));
        pPatch->bDirtyOpcode   = *PATCHCODE_PTR_HC(pPatch);
        *PATCHCODE_PTR_HC(pPatch) = 0xCC;
    }

    STAM_COUNTER_INC(&pVM->patm.s.StatDirty);
    /* Put back the replaced instruction. */
    int rc = PATMR3DisablePatch(pVM, pPatch->pPrivInstrGC);
    if (rc == VWRN_PATCH_REMOVED)
        return VINF_SUCCESS;

    /** @note we don't restore patch pages for patches that are not enabled! */
    /** @note be careful when changing this behaviour!! */

    /* The patch pages are no longer marked for self-modifying code detection */
    if (pPatch->flags & PATMFL_CODE_MONITORED)
    {
        int rc = patmRemovePatchPages(pVM, pPatch);
        AssertRCReturn(rc, rc);
    }
    pPatch->uState = PATCH_DIRTY;

    /* Paranoia; make sure this patch is not somewhere in the callchain, so prevent ret instructions from succeeding. */
    CTXSUFF(pVM->patm.s.pGCState)->Psp = PATM_STACK_SIZE;

    return VINF_SUCCESS;
}

/**
 * Query the corresponding GC instruction pointer from a pointer inside the patch block itself
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch block structure pointer
 * @param   pPatchGC    GC address in patch block
 */
RTRCPTR patmPatchGCPtr2GuestGCPtr(PVM pVM, PPATCHINFO pPatch, RCPTRTYPE(uint8_t *) pPatchGC)
{
    Assert(pPatch->Patch2GuestAddrTree);
    /* Get the closest record from below. */
    PRECPATCHTOGUEST pPatchToGuestRec = (PRECPATCHTOGUEST)RTAvlU32GetBestFit(&pPatch->Patch2GuestAddrTree, pPatchGC - pVM->patm.s.pPatchMemGC, false);
    if (pPatchToGuestRec)
        return pPatchToGuestRec->pOrgInstrGC;

    return 0;
}

/* Converts Guest code GC ptr to Patch code GC ptr (if found)
 *
 * @returns corresponding GC pointer in patch block
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Current patch block pointer
 * @param   pInstrGC    Guest context pointer to privileged instruction
 *
 */
RTRCPTR patmGuestGCPtrToPatchGCPtr(PVM pVM, PPATCHINFO pPatch, RCPTRTYPE(uint8_t*) pInstrGC)
{
    if (pPatch->Guest2PatchAddrTree)
    {
        PRECGUESTTOPATCH pGuestToPatchRec = (PRECGUESTTOPATCH)RTAvlU32Get(&pPatch->Guest2PatchAddrTree, pInstrGC);
        if (pGuestToPatchRec)
            return pVM->patm.s.pPatchMemGC + pGuestToPatchRec->PatchOffset;
    }

    return 0;
}

/* Converts Guest code GC ptr to Patch code GC ptr (or nearest from below if no identical match)
 *
 * @returns corresponding GC pointer in patch block
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Current patch block pointer
 * @param   pInstrGC    Guest context pointer to privileged instruction
 *
 */
RTRCPTR patmGuestGCPtrToClosestPatchGCPtr(PVM pVM, PPATCHINFO pPatch, RCPTRTYPE(uint8_t*) pInstrGC)
{
        PRECGUESTTOPATCH pGuestToPatchRec = (PRECGUESTTOPATCH)RTAvlU32GetBestFit(&pPatch->Guest2PatchAddrTree, pInstrGC, false);
        if (pGuestToPatchRec)
            return pVM->patm.s.pPatchMemGC + pGuestToPatchRec->PatchOffset;

    return 0;
}

/* Converts Guest code GC ptr to Patch code GC ptr (if found)
 *
 * @returns corresponding GC pointer in patch block
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context pointer to privileged instruction
 *
 */
VMMR3DECL(RTRCPTR) PATMR3GuestGCPtrToPatchGCPtr(PVM pVM, RCPTRTYPE(uint8_t*) pInstrGC)
{
    PPATMPATCHREC pPatchRec = (PPATMPATCHREC)RTAvloU32GetBestFit(&pVM->patm.s.PatchLookupTreeHC->PatchTree, pInstrGC, false);
    if (pPatchRec && pPatchRec->patch.uState == PATCH_ENABLED && pInstrGC >= pPatchRec->patch.pPrivInstrGC)
    {
        return patmGuestGCPtrToPatchGCPtr(pVM, &pPatchRec->patch, pInstrGC);
    }
    return 0;
}

/**
 * Query the corresponding GC instruction pointer from a pointer inside the patch block itself
 *
 * @returns original GC instruction pointer or 0 if not found
 * @param   pVM         The VM to operate on.
 * @param   pPatchGC    GC address in patch block
 * @param   pEnmState   State of the translated address (out)
 *
 */
VMMR3DECL(RTRCPTR) PATMR3PatchToGCPtr(PVM pVM, RTRCPTR pPatchGC, PATMTRANSSTATE *pEnmState)
{
    PPATMPATCHREC pPatchRec;
    void         *pvPatchCoreOffset;
    RTRCPTR       pPrivInstrGC;

    Assert(PATMIsPatchGCAddr(pVM, pPatchGC));
    pvPatchCoreOffset = RTAvloU32GetBestFit(&pVM->patm.s.PatchLookupTreeHC->PatchTreeByPatchAddr, pPatchGC - pVM->patm.s.pPatchMemGC, false);
    if (pvPatchCoreOffset == 0)
    {
        Log(("PATMR3PatchToGCPtr failed for %RRv offset %x\n", pPatchGC, pPatchGC - pVM->patm.s.pPatchMemGC));
        return 0;
    }
    pPatchRec = PATM_PATCHREC_FROM_COREOFFSET(pvPatchCoreOffset);
    pPrivInstrGC = patmPatchGCPtr2GuestGCPtr(pVM, &pPatchRec->patch, pPatchGC);
    if (pEnmState)
    {
        AssertMsg(pPrivInstrGC && (     pPatchRec->patch.uState == PATCH_ENABLED
                                    ||  pPatchRec->patch.uState == PATCH_DIRTY
                                    ||  pPatchRec->patch.uState == PATCH_DISABLE_PENDING
                                    ||  pPatchRec->patch.uState == PATCH_UNUSABLE),
                  ("pPrivInstrGC=%RRv uState=%d\n", pPrivInstrGC, pPatchRec->patch.uState));

        if (    !pPrivInstrGC
            ||   pPatchRec->patch.uState == PATCH_UNUSABLE
            ||   pPatchRec->patch.uState == PATCH_REFUSED)
        {
            pPrivInstrGC = 0;
            *pEnmState = PATMTRANS_FAILED;
        }
        else
        if (pVM->patm.s.pGCStateHC->GCPtrInhibitInterrupts == pPrivInstrGC)
        {
            *pEnmState = PATMTRANS_INHIBITIRQ;
        }
        else
        if (    pPatchRec->patch.uState == PATCH_ENABLED
            && !(pPatchRec->patch.flags & (PATMFL_DUPLICATE_FUNCTION|PATMFL_IDTHANDLER|PATMFL_TRAMPOLINE))
            &&  pPrivInstrGC > pPatchRec->patch.pPrivInstrGC
            &&  pPrivInstrGC < pPatchRec->patch.pPrivInstrGC + pPatchRec->patch.cbPatchJump)
        {
            *pEnmState = PATMTRANS_OVERWRITTEN;
        }
        else
        if (PATMFindActivePatchByEntrypoint(pVM, pPrivInstrGC))
        {
            *pEnmState = PATMTRANS_OVERWRITTEN;
        }
        else
        if (pPrivInstrGC == pPatchRec->patch.pPrivInstrGC)
        {
            *pEnmState = PATMTRANS_PATCHSTART;
        }
        else
            *pEnmState = PATMTRANS_SAFE;
    }
    return pPrivInstrGC;
}

/**
 * Returns the GC pointer of the patch for the specified GC address
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pAddrGC     Guest context address
 */
VMMR3DECL(RTRCPTR) PATMR3QueryPatchGCPtr(PVM pVM, RTRCPTR pAddrGC)
{
    PPATMPATCHREC pPatchRec;

    // Find the patch record
    pPatchRec = (PPATMPATCHREC)RTAvloU32Get(&pVM->patm.s.PatchLookupTreeHC->PatchTree, pAddrGC);
    /** @todo we should only use patches that are enabled! always did this, but it's incorrect! */
    if (pPatchRec && (pPatchRec->patch.uState == PATCH_ENABLED || pPatchRec->patch.uState == PATCH_DIRTY))
        return PATCHCODE_PTR_GC(&pPatchRec->patch);

    return 0;
}

/**
 * Attempt to recover dirty instructions
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pCtx        CPU context
 * @param   pPatch      Patch record
 * @param   pPatchToGuestRec    Patch to guest address record
 * @param   pEip        GC pointer of trapping instruction
 */
static int patmR3HandleDirtyInstr(PVM pVM, PCPUMCTX pCtx, PPATMPATCHREC pPatch, PRECPATCHTOGUEST pPatchToGuestRec, RTRCPTR pEip)
{
    DISCPUSTATE  CpuOld, CpuNew;
    uint8_t     *pPatchInstrHC, *pCurPatchInstrHC;
    int          rc;
    RTRCPTR      pCurInstrGC, pCurPatchInstrGC;
    uint32_t     cbDirty;
    PRECPATCHTOGUEST pRec;
    PVMCPU       pVCpu = VMMGetCpu0(pVM);

    Log(("patmR3HandleDirtyInstr: dirty instruction at %RRv (%RRv)\n", pEip, pPatchToGuestRec->pOrgInstrGC));

    pRec             = pPatchToGuestRec;
    pCurInstrGC      = pPatchToGuestRec->pOrgInstrGC;
    pCurPatchInstrGC = pEip;
    cbDirty          = 0;
    pPatchInstrHC    = patmPatchGCPtr2PatchHCPtr(pVM, pCurPatchInstrGC);

    /* Find all adjacent dirty instructions */
    while (true)
    {
        if (pRec->fJumpTarget)
        {
            LogRel(("PATM: patmR3HandleDirtyInstr: dirty instruction at %RRv (%RRv) ignored, because instruction in function was reused as target of jump\n", pEip, pPatchToGuestRec->pOrgInstrGC));
            pRec->fDirty = false;
            return VERR_PATCHING_REFUSED;
        }

        /* Restore original instruction opcode byte so we can check if the write was indeed safe. */
        pCurPatchInstrHC  = patmPatchGCPtr2PatchHCPtr(pVM, pCurPatchInstrGC);
        *pCurPatchInstrHC = pRec->u8DirtyOpcode;

        /* Only harmless instructions are acceptable. */
        rc = CPUMR3DisasmInstrCPU(pVM, pVCpu, pCtx, pCurPatchInstrGC, &CpuOld, 0);
        if (    RT_FAILURE(rc)
            ||  !(CpuOld.pCurInstr->optype & OPTYPE_HARMLESS))
        {
            if (RT_SUCCESS(rc))
                cbDirty += CpuOld.opsize;
            else
            if (!cbDirty)
                cbDirty = 1;
            break;
        }

#ifdef DEBUG
        char szBuf[256];
        szBuf[0] = '\0';
        DBGFR3DisasInstr(pVM, pVCpu, pCtx->cs, pCurPatchInstrGC, szBuf, sizeof(szBuf));
        Log(("DIRTY: %s\n", szBuf));
#endif
        /* Mark as clean; if we fail we'll let it always fault. */
        pRec->fDirty      = false;

        /** Remove old lookup record. */
        patmr3RemoveP2GLookupRecord(pVM, &pPatch->patch, pCurPatchInstrGC);

        pCurPatchInstrGC += CpuOld.opsize;
        cbDirty          += CpuOld.opsize;

        /* Let's see if there's another dirty instruction right after. */
        pRec = (PRECPATCHTOGUEST)RTAvlU32GetBestFit(&pPatch->patch.Patch2GuestAddrTree, pCurPatchInstrGC - pVM->patm.s.pPatchMemGC, true);
        if (!pRec || !pRec->fDirty)
            break;  /* no more dirty instructions */

        /* In case of complex instructions the next guest instruction could be quite far off. */
        pCurPatchInstrGC = pRec->Core.Key + pVM->patm.s.pPatchMemGC;
    }

    if (    RT_SUCCESS(rc)
        &&  (CpuOld.pCurInstr->optype & OPTYPE_HARMLESS)
       )
    {
        uint32_t cbLeft;

        pCurPatchInstrHC = pPatchInstrHC;
        pCurPatchInstrGC = pEip;
        cbLeft           = cbDirty;

        while (cbLeft && RT_SUCCESS(rc))
        {
            bool fValidInstr;

            rc = CPUMR3DisasmInstrCPU(pVM, pVCpu, pCtx, pCurInstrGC, &CpuNew, 0);

            fValidInstr = !!(CpuNew.pCurInstr->optype & OPTYPE_HARMLESS);
            if (    !fValidInstr
                &&  (CpuNew.pCurInstr->optype & OPTYPE_RELATIVE_CONTROLFLOW)
               )
            {
                RTRCPTR pTargetGC = PATMResolveBranch(&CpuNew, pCurInstrGC);

                if (    pTargetGC >= pPatchToGuestRec->pOrgInstrGC
                    &&  pTargetGC <= pPatchToGuestRec->pOrgInstrGC + cbDirty
                   )
                {
                    /* A relative jump to an instruction inside or to the end of the dirty block is acceptable. */
                    fValidInstr = true;
                }
            }

            /* If the instruction is completely harmless (which implies a 1:1 patch copy). */
            if (    rc == VINF_SUCCESS
                &&  CpuNew.opsize <= cbLeft /* must still fit */
                &&  fValidInstr
               )
            {
#ifdef DEBUG
                char szBuf[256];
                szBuf[0] = '\0';
                DBGFR3DisasInstr(pVM, pVCpu, pCtx->cs, pCurInstrGC, szBuf, sizeof(szBuf));
                Log(("NEW:   %s\n", szBuf));
#endif

                /* Copy the new instruction. */
                rc = PGMPhysSimpleReadGCPtr(VMMGetCpu0(pVM), pCurPatchInstrHC, pCurInstrGC, CpuNew.opsize);
                AssertRC(rc);

                /* Add a new lookup record for the duplicated instruction. */
                patmr3AddP2GLookupRecord(pVM, &pPatch->patch, pCurPatchInstrHC, pCurInstrGC, PATM_LOOKUP_BOTHDIR);
            }
            else
            {
#ifdef DEBUG
                char szBuf[256];
                szBuf[0] = '\0';
                DBGFR3DisasInstr(pVM, pVCpu, pCtx->cs, pCurInstrGC, szBuf, sizeof(szBuf));
                Log(("NEW:   %s (FAILED)\n", szBuf));
#endif
                /* Restore the old lookup record for the duplicated instruction. */
                patmr3AddP2GLookupRecord(pVM, &pPatch->patch, pCurPatchInstrHC, pCurInstrGC, PATM_LOOKUP_BOTHDIR);

                /** @todo in theory we need to restore the lookup records for the remaining dirty instructions too! */
                rc = VERR_PATCHING_REFUSED;
                break;
            }
            pCurInstrGC      += CpuNew.opsize;
            pCurPatchInstrHC += CpuNew.opsize;
            pCurPatchInstrGC += CpuNew.opsize;
            cbLeft           -= CpuNew.opsize;
        }
    }
    else
        rc = VERR_PATCHING_REFUSED;

    if (RT_SUCCESS(rc))
    {
        STAM_COUNTER_INC(&pVM->patm.s.StatInstrDirtyGood);
    }
    else
    {
        STAM_COUNTER_INC(&pVM->patm.s.StatInstrDirtyBad);
        Assert(cbDirty);

        /* Mark the whole instruction stream with breakpoints. */
        if (cbDirty)
            memset(pPatchInstrHC, 0xCC, cbDirty);

        if (    pVM->patm.s.fOutOfMemory == false
            &&  (pPatch->patch.flags & (PATMFL_DUPLICATE_FUNCTION|PATMFL_IDTHANDLER|PATMFL_TRAPHANDLER)))
        {
            rc = patmR3RefreshPatch(pVM, pPatch);
            if (RT_FAILURE(rc))
            {
                LogRel(("PATM: Failed to refresh dirty patch at %RRv. Disabling it.\n", pPatch->patch.pPrivInstrGC));
            }
            /* Even if we succeed, we must go back to the original instruction as the patched one could be invalid. */
            rc = VERR_PATCHING_REFUSED;
        }
    }
    return rc;
}

/**
 * Handle trap inside patch code
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pCtx        CPU context
 * @param   pEip        GC pointer of trapping instruction
 * @param   ppNewEip    GC pointer to new instruction
 */
VMMR3DECL(int) PATMR3HandleTrap(PVM pVM, PCPUMCTX pCtx, RTRCPTR pEip, RTGCPTR *ppNewEip)
{
    PPATMPATCHREC    pPatch = 0;
    void            *pvPatchCoreOffset;
    RTRCUINTPTR      offset;
    RTRCPTR          pNewEip;
    int              rc ;
    PRECPATCHTOGUEST pPatchToGuestRec = 0;
    PVMCPU           pVCpu = VMMGetCpu0(pVM);

    Assert(pVM->cCPUs == 1);

    pNewEip   = 0;
    *ppNewEip = 0;

    STAM_PROFILE_ADV_START(&pVM->patm.s.StatHandleTrap, a);

    /* Find the patch record. */
    /** @note there might not be a patch to guest translation record (global function) */
    offset = pEip - pVM->patm.s.pPatchMemGC;
    pvPatchCoreOffset = RTAvloU32GetBestFit(&pVM->patm.s.PatchLookupTreeHC->PatchTreeByPatchAddr, offset, false);
    if (pvPatchCoreOffset)
    {
        pPatch = PATM_PATCHREC_FROM_COREOFFSET(pvPatchCoreOffset);

        Assert(offset >= pPatch->patch.pPatchBlockOffset && offset < pPatch->patch.pPatchBlockOffset + pPatch->patch.cbPatchBlockSize);

        if (pPatch->patch.uState == PATCH_DIRTY)
        {
            Log(("PATMR3HandleTrap: trap in dirty patch at %RRv\n", pEip));
            if (pPatch->patch.flags & (PATMFL_DUPLICATE_FUNCTION|PATMFL_CODE_REFERENCED))
            {
                /* Function duplication patches set fPIF to 1 on entry */
                pVM->patm.s.pGCStateHC->fPIF = 1;
            }
        }
        else
        if (pPatch->patch.uState == PATCH_DISABLED)
        {
            Log(("PATMR3HandleTrap: trap in disabled patch at %RRv\n", pEip));
            if (pPatch->patch.flags & (PATMFL_DUPLICATE_FUNCTION|PATMFL_CODE_REFERENCED))
            {
                /* Function duplication patches set fPIF to 1 on entry */
                pVM->patm.s.pGCStateHC->fPIF = 1;
            }
        }
        else
        if (pPatch->patch.uState == PATCH_DISABLE_PENDING)
        {
            RTRCPTR pPrivInstrGC = pPatch->patch.pPrivInstrGC;

            Log(("PATMR3HandleTrap: disable operation is pending for patch at %RRv\n", pPatch->patch.pPrivInstrGC));
            rc = PATMR3DisablePatch(pVM, pPatch->patch.pPrivInstrGC);
            AssertReleaseMsg(rc != VWRN_PATCH_REMOVED, ("PATMR3DisablePatch removed patch at %RRv\n", pPrivInstrGC));
            AssertMsg(pPatch->patch.uState == PATCH_DISABLED || pPatch->patch.uState == PATCH_UNUSABLE, ("Unexpected failure to disable patch state=%d rc=%Rrc\n", pPatch->patch.uState, rc));
        }

        pPatchToGuestRec = (PRECPATCHTOGUEST)RTAvlU32GetBestFit(&pPatch->patch.Patch2GuestAddrTree, offset, false);
        AssertReleaseMsg(pPatchToGuestRec, ("PATMR3HandleTrap: Unable to find corresponding guest address for %RRv (offset %x)\n", pEip, offset));

        pNewEip = pPatchToGuestRec->pOrgInstrGC;
        pPatch->patch.cTraps++;
        PATM_STAT_FAULT_INC(&pPatch->patch);
    }
    else
        AssertReleaseMsg(pVM->patm.s.pGCStateHC->fPIF == 0, ("PATMR3HandleTrap: Unable to find translation record for %RRv (PIF=0)\n", pEip));

    /* Check if we were interrupted in PATM generated instruction code. */
    if (pVM->patm.s.pGCStateHC->fPIF == 0)
    {
        DISCPUSTATE Cpu;
        rc = CPUMR3DisasmInstrCPU(pVM, pVCpu, pCtx, pEip, &Cpu, "PIF Trap: ");
        AssertRC(rc);

        if (    rc == VINF_SUCCESS
            &&  (   Cpu.pCurInstr->opcode == OP_PUSHF
                 || Cpu.pCurInstr->opcode == OP_PUSH
                 || Cpu.pCurInstr->opcode == OP_CALL)
           )
        {
            uint64_t fFlags;

            STAM_COUNTER_INC(&pVM->patm.s.StatPushTrap);

            if (Cpu.pCurInstr->opcode == OP_PUSH)
            {
                rc = PGMShwGetPage(pVCpu, pCtx->esp, &fFlags, NULL);
                if (    rc == VINF_SUCCESS
                    &&  ((fFlags & (X86_PTE_P|X86_PTE_RW)) == (X86_PTE_P|X86_PTE_RW)) )
                {
                    /* The stack address is fine, so the push argument is a pointer -> emulate this instruction */

                    /* Reset the PATM stack. */
                    CTXSUFF(pVM->patm.s.pGCState)->Psp = PATM_STACK_SIZE;

                    pVM->patm.s.pGCStateHC->fPIF = 1;

                    Log(("Faulting push -> go back to the original instruction\n"));

                    /* continue at the original instruction */
                    *ppNewEip = pNewEip - SELMToFlat(pVM, DIS_SELREG_CS, CPUMCTX2CORE(pCtx), 0);
                    STAM_PROFILE_ADV_STOP(&pVM->patm.s.StatHandleTrap, a);
                    return VINF_SUCCESS;
                }
            }

            /* Typical pushf (most patches)/push (call patch) trap because of a monitored page. */
            rc = PGMShwModifyPage(pVCpu, pCtx->esp, 1, X86_PTE_RW, ~(uint64_t)X86_PTE_RW);
            AssertMsgRC(rc, ("PGMShwModifyPage -> rc=%Rrc\n", rc));
            if (rc == VINF_SUCCESS)
            {

                /* The guest page *must* be present. */
                rc = PGMGstGetPage(pVCpu, pCtx->esp, &fFlags, NULL);
                if (rc == VINF_SUCCESS && (fFlags & X86_PTE_P))
                {
                    STAM_PROFILE_ADV_STOP(&pVM->patm.s.StatHandleTrap, a);
                    return VINF_PATCH_CONTINUE;
                }
            }
        }
        else
        if (pPatch->patch.pPrivInstrGC == pNewEip)
        {
            /* Invalidated patch or first instruction overwritten.
             * We can ignore the fPIF state in this case.
             */
            /* Reset the PATM stack. */
            CTXSUFF(pVM->patm.s.pGCState)->Psp = PATM_STACK_SIZE;

            Log(("Call to invalidated patch -> go back to the original instruction\n"));

            pVM->patm.s.pGCStateHC->fPIF = 1;

            /* continue at the original instruction */
            *ppNewEip = pNewEip - SELMToFlat(pVM, DIS_SELREG_CS, CPUMCTX2CORE(pCtx), 0);
            STAM_PROFILE_ADV_STOP(&pVM->patm.s.StatHandleTrap, a);
            return VINF_SUCCESS;
        }

        char szBuf[256];
        szBuf[0] = '\0';
        DBGFR3DisasInstr(pVM, pVCpu, pCtx->cs, pEip, szBuf, sizeof(szBuf));

        /* Very bad. We crashed in emitted code. Probably stack? */
        if (pPatch)
        {
            AssertReleaseMsg(pVM->patm.s.pGCStateHC->fPIF == 1,
                            ("Crash in patch code %RRv (%RRv) esp=%RX32\nPatch state=%x flags=%x fDirty=%d\n%s\n", pEip, pNewEip, CPUMGetGuestESP(pVCpu), pPatch->patch.uState, pPatch->patch.flags, pPatchToGuestRec->fDirty, szBuf));
        }
        else
            AssertReleaseMsg(pVM->patm.s.pGCStateHC->fPIF == 1,
                            ("Crash in patch code %RRv (%RRv) esp=%RX32\n%s\n", pEip, pNewEip, CPUMGetGuestESP(pVCpu), szBuf));
        EMR3FatalError(pVCpu, VERR_INTERNAL_ERROR);
    }

    /* From here on, we must have a valid patch to guest translation. */
    if (pvPatchCoreOffset == 0)
    {
        STAM_PROFILE_ADV_STOP(&pVM->patm.s.StatHandleTrap, a);
        AssertMsgFailed(("PATMR3HandleTrap: patch not found at address %RRv!!\n", pEip));
        return VERR_PATCH_NOT_FOUND;    //fatal error
    }

    /* Take care of dirty/changed instructions. */
    if (pPatchToGuestRec->fDirty)
    {
        Assert(pPatchToGuestRec->Core.Key == offset);
        Assert(pVM->patm.s.pGCStateHC->fPIF == 1);

        rc = patmR3HandleDirtyInstr(pVM, pCtx, pPatch, pPatchToGuestRec, pEip);
        if (RT_SUCCESS(rc))
        {
            /* Retry the current instruction. */
            pNewEip = pEip;
            rc = VINF_PATCH_CONTINUE;   /* Continue at current patch instruction. */
        }
        else
        {
            /* Reset the PATM stack. */
            CTXSUFF(pVM->patm.s.pGCState)->Psp = PATM_STACK_SIZE;

            rc = VINF_SUCCESS;  /* Continue at original instruction. */
        }

        *ppNewEip = pNewEip - SELMToFlat(pVM, DIS_SELREG_CS, CPUMCTX2CORE(pCtx), 0);
        STAM_PROFILE_ADV_STOP(&pVM->patm.s.StatHandleTrap, a);
        return rc;
    }

#ifdef VBOX_STRICT
    if (pPatch->patch.flags & PATMFL_DUPLICATE_FUNCTION)
    {
        DISCPUSTATE cpu;
        bool        disret;
        uint32_t    opsize;

        cpu.mode = (pPatch->patch.flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;
        disret = PATMR3DISInstr(pVM, &pPatch->patch, &cpu, pNewEip, PATMGCVirtToHCVirt(pVM, &pPatch->patch, pNewEip), &opsize, NULL, PATMREAD_RAWCODE);
        if (disret && cpu.pCurInstr->opcode == OP_RETN)
        {
            RTRCPTR retaddr;
            PCPUMCTX pCtx;

            pCtx = CPUMQueryGuestCtxPtr(pVCpu);

            rc = PGMPhysSimpleReadGCPtr(pVCpu,  &retaddr, pCtx->esp, sizeof(retaddr));
            AssertRC(rc);

            Log(("Return failed at %RRv (%RRv)\n", pEip, pNewEip));
            Log(("Expected return address %RRv found address %RRv Psp=%x\n", pVM->patm.s.pGCStackHC[(pVM->patm.s.pGCStateHC->Psp+PATM_STACK_SIZE)/sizeof(RTRCPTR)], retaddr, pVM->patm.s.pGCStateHC->Psp));
        }
    }
#endif

    /* Return original address, correct by subtracting the CS base address. */
    *ppNewEip = pNewEip - SELMToFlat(pVM, DIS_SELREG_CS, CPUMCTX2CORE(pCtx), 0);

    /* Reset the PATM stack. */
    CTXSUFF(pVM->patm.s.pGCState)->Psp = PATM_STACK_SIZE;

    if (pVM->patm.s.pGCStateHC->GCPtrInhibitInterrupts == pNewEip)
    {
        /* Must be a faulting instruction after sti; currently only sysexit, hlt or iret */
        Log(("PATMR3HandleTrap %RRv -> inhibit irqs set!\n", pEip));
#ifdef VBOX_STRICT
        DISCPUSTATE cpu;
        bool        disret;
        uint32_t    opsize;

        cpu.mode = (pPatch->patch.flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;
        disret = PATMR3DISInstr(pVM, &pPatch->patch, &cpu, pNewEip, PATMGCVirtToHCVirt(pVM, &pPatch->patch, pNewEip), &opsize, NULL, PATMREAD_ORGCODE);

        if (disret && (cpu.pCurInstr->opcode == OP_SYSEXIT || cpu.pCurInstr->opcode == OP_HLT || cpu.pCurInstr->opcode == OP_INT3))
        {
            cpu.mode = (pPatch->patch.flags & PATMFL_CODE32) ? CPUMODE_32BIT : CPUMODE_16BIT;
            disret = PATMR3DISInstr(pVM, &pPatch->patch, &cpu, pNewEip, PATMGCVirtToHCVirt(pVM, &pPatch->patch, pNewEip), &opsize, NULL, PATMREAD_RAWCODE);

            Assert(cpu.pCurInstr->opcode == OP_SYSEXIT || cpu.pCurInstr->opcode == OP_HLT || cpu.pCurInstr->opcode == OP_IRET);
        }
#endif
        EMSetInhibitInterruptsPC(pVCpu, pNewEip);
        pVM->patm.s.pGCStateHC->GCPtrInhibitInterrupts = 0;
    }

    Log2(("pPatchBlockGC %RRv - pEip %RRv corresponding GC address %RRv\n", PATCHCODE_PTR_GC(&pPatch->patch), pEip, pNewEip));
#ifdef LOG_ENABLED
    CPUMR3DisasmInstr(pVM, pVCpu, pCtx, pNewEip, "PATCHRET: ");
#endif
    if (pNewEip >= pPatch->patch.pPrivInstrGC && pNewEip < pPatch->patch.pPrivInstrGC + pPatch->patch.cbPatchJump)
    {
        /* We can't jump back to code that we've overwritten with a 5 byte jump! */
        Log(("Disabling patch at location %RRv due to trap too close to the privileged instruction \n", pPatch->patch.pPrivInstrGC));
        PATMR3DisablePatch(pVM, pPatch->patch.pPrivInstrGC);
        STAM_PROFILE_ADV_STOP(&pVM->patm.s.StatHandleTrap, a);
        return VERR_PATCH_DISABLED;
    }

#ifdef PATM_REMOVE_PATCH_ON_TOO_MANY_TRAPS
    /** @todo compare to nr of successful runs. add some aging algorithm and determine the best time to disable the patch */
    if (pPatch->patch.cTraps > MAX_PATCH_TRAPS)
    {
        Log(("Disabling patch at location %RRv due to too many traps inside patch code\n", pPatch->patch.pPrivInstrGC));
        //we are only wasting time, back out the patch
        PATMR3DisablePatch(pVM, pPatch->patch.pPrivInstrGC);
        pTrapRec->pNextPatchInstr = 0;
        STAM_PROFILE_ADV_STOP(&pVM->patm.s.StatHandleTrap, a);
        return VERR_PATCH_DISABLED;
    }
#endif

    STAM_PROFILE_ADV_STOP(&pVM->patm.s.StatHandleTrap, a);
    return VINF_SUCCESS;
}


/**
 * Handle page-fault in monitored page
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) PATMR3HandleMonitoredPage(PVM pVM)
{
    RTRCPTR addr = pVM->patm.s.pvFaultMonitor;

    addr &= PAGE_BASE_GC_MASK;

    int rc = PGMHandlerVirtualDeregister(pVM, addr);
    AssertRC(rc); NOREF(rc);

    PPATMPATCHREC pPatchRec = (PPATMPATCHREC)RTAvloU32GetBestFit(&pVM->patm.s.PatchLookupTreeHC->PatchTree, addr, false);
    if (pPatchRec && pPatchRec->patch.uState == PATCH_ENABLED && PAGE_ADDRESS(pPatchRec->patch.pPrivInstrGC) == PAGE_ADDRESS(addr))
    {
        STAM_COUNTER_INC(&pVM->patm.s.StatMonitored);
        Log(("Renewing patch at %RRv\n", pPatchRec->patch.pPrivInstrGC));
        rc = PATMR3DisablePatch(pVM, pPatchRec->patch.pPrivInstrGC);
        if (rc == VWRN_PATCH_REMOVED)
            return VINF_SUCCESS;

        PATMR3EnablePatch(pVM, pPatchRec->patch.pPrivInstrGC);

        if (addr == pPatchRec->patch.pPrivInstrGC)
            addr++;
    }

    for(;;)
    {
        pPatchRec = (PPATMPATCHREC)RTAvloU32GetBestFit(&pVM->patm.s.PatchLookupTreeHC->PatchTree, addr, true);

        if (!pPatchRec || PAGE_ADDRESS(pPatchRec->patch.pPrivInstrGC) != PAGE_ADDRESS(addr))
            break;

        if (pPatchRec && pPatchRec->patch.uState == PATCH_ENABLED)
        {
            STAM_COUNTER_INC(&pVM->patm.s.StatMonitored);
            Log(("Renewing patch at %RRv\n", pPatchRec->patch.pPrivInstrGC));
            PATMR3DisablePatch(pVM, pPatchRec->patch.pPrivInstrGC);
            PATMR3EnablePatch(pVM, pPatchRec->patch.pPrivInstrGC);
        }
        addr = pPatchRec->patch.pPrivInstrGC + 1;
    }

    pVM->patm.s.pvFaultMonitor = 0;
    return VINF_SUCCESS;
}


#ifdef VBOX_WITH_STATISTICS

static const char *PATMPatchType(PVM pVM, PPATCHINFO pPatch)
{
    if (pPatch->flags & PATMFL_SYSENTER)
    {
        return "SYSENT";
    }
    else
    if (pPatch->flags & (PATMFL_TRAPHANDLER|PATMFL_INTHANDLER))
    {
        static char szTrap[16];
        uint32_t iGate;

        iGate = TRPMR3QueryGateByHandler(pVM, PATCHCODE_PTR_GC(pPatch));
        if (iGate < 256)
            RTStrPrintf(szTrap, sizeof(szTrap), (pPatch->flags & PATMFL_INTHANDLER) ? "INT-%2X" : "TRAP-%2X", iGate);
        else
            RTStrPrintf(szTrap, sizeof(szTrap), (pPatch->flags & PATMFL_INTHANDLER) ? "INT-??" : "TRAP-??");
        return szTrap;
    }
    else
    if (pPatch->flags & (PATMFL_DUPLICATE_FUNCTION))
        return "DUPFUNC";
    else
    if (pPatch->flags & PATMFL_REPLACE_FUNCTION_CALL)
        return "FUNCCALL";
    else
    if (pPatch->flags & PATMFL_TRAMPOLINE)
        return "TRAMP";
    else
        return patmGetInstructionString(pPatch->opcode, pPatch->flags);
}

static const char *PATMPatchState(PVM pVM, PPATCHINFO pPatch)
{
    switch(pPatch->uState)
    {
    case PATCH_ENABLED:
        return "ENA";
    case PATCH_DISABLED:
        return "DIS";
    case PATCH_DIRTY:
        return "DIR";
    case PATCH_UNUSABLE:
        return "UNU";
    case PATCH_REFUSED:
        return "REF";
    case PATCH_DISABLE_PENDING:
        return "DIP";
    default:
        AssertFailed();
        return "   ";
    }
}

/**
 * Resets the sample.
 * @param   pVM         The VM handle.
 * @param   pvSample    The sample registered using STAMR3RegisterCallback.
 */
static void patmResetStat(PVM pVM, void *pvSample)
{
    PPATCHINFO pPatch = (PPATCHINFO)pvSample;
    Assert(pPatch);

    pVM->patm.s.pStatsHC[pPatch->uPatchIdx].u32A = 0;
    pVM->patm.s.pStatsHC[pPatch->uPatchIdx].u32B = 0;
}

/**
 * Prints the sample into the buffer.
 *
 * @param   pVM         The VM handle.
 * @param   pvSample    The sample registered using STAMR3RegisterCallback.
 * @param   pszBuf      The buffer to print into.
 * @param   cchBuf      The size of the buffer.
 */
static void patmPrintStat(PVM pVM, void *pvSample, char *pszBuf, size_t cchBuf)
{
    PPATCHINFO pPatch = (PPATCHINFO)pvSample;
    Assert(pPatch);

    Assert(pPatch->uState != PATCH_REFUSED);
    Assert(!(pPatch->flags & (PATMFL_REPLACE_FUNCTION_CALL|PATMFL_MMIO_ACCESS)));

    RTStrPrintf(pszBuf, cchBuf, "size %04x ->%3s %8s - %08d - %08d",
                pPatch->cbPatchBlockSize, PATMPatchState(pVM, pPatch), PATMPatchType(pVM, pPatch),
                pVM->patm.s.pStatsHC[pPatch->uPatchIdx].u32A, pVM->patm.s.pStatsHC[pPatch->uPatchIdx].u32B);
}

/**
 * Returns the GC address of the corresponding patch statistics counter
 *
 * @returns Stat address
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch structure
 */
RTRCPTR patmPatchQueryStatAddress(PVM pVM, PPATCHINFO pPatch)
{
    Assert(pPatch->uPatchIdx != PATM_STAT_INDEX_NONE);
    return pVM->patm.s.pStatsGC + sizeof(STAMRATIOU32) * pPatch->uPatchIdx + RT_OFFSETOF(STAMRATIOU32, u32A);
}

#endif /* VBOX_WITH_STATISTICS */

#ifdef VBOX_WITH_DEBUGGER
/**
 * The '.patmoff' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) patmr3CmdOff(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    /*
     * Validate input.
     */
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: The command requires VM to be selected.\n");

    RTAvloU32DoWithAll(&pVM->patm.s.PatchLookupTreeHC->PatchTree, true, DisableAllPatches, pVM);
    PATMR3AllowPatching(pVM, false);
    return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Patching disabled\n");
}

/**
 * The '.patmon' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) patmr3CmdOn(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    /*
     * Validate input.
     */
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: The command requires VM to be selected.\n");

    PATMR3AllowPatching(pVM, true);
    RTAvloU32DoWithAll(&pVM->patm.s.PatchLookupTreeHC->PatchTree, true, EnableAllPatches, pVM);
    return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Patching enabled\n");
}
#endif
