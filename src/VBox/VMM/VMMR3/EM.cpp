/* $Id$ */
/** @file
 * EM - Execution Monitor / Manager.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/** @page pg_em         EM - The Execution Monitor / Manager
 *
 * The Execution Monitor/Manager is responsible for running the VM, scheduling
 * the right kind of execution (Raw-mode, Hardware Assisted, Recompiled or
 * Interpreted), and keeping the CPU states in sync. The function
 * EMR3ExecuteVM() is the 'main-loop' of the VM, while each of the execution
 * modes has different inner loops (emR3RawExecute, emR3HmExecute, and
 * emR3RemExecute).
 *
 * The interpreted execution is only used to avoid switching between
 * raw-mode/hm and the recompiler when fielding virtualization traps/faults.
 * The interpretation is thus implemented as part of EM.
 *
 * @see grp_em
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_EM
#define VMCPU_INCL_CPUM_GST_CTX /* for CPUM_IMPORT_GUEST_STATE_RET */
#include <VBox/vmm/em.h>
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/patm.h>
#include <VBox/vmm/csam.h>
#include <VBox/vmm/selm.h>
#include <VBox/vmm/trpm.h>
#include <VBox/vmm/iem.h>
#include <VBox/vmm/nem.h>
#include <VBox/vmm/iom.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/pgm.h>
#ifdef VBOX_WITH_REM
# include <VBox/vmm/rem.h>
#endif
#include <VBox/vmm/apic.h>
#include <VBox/vmm/tm.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/ssm.h>
#include <VBox/vmm/pdmapi.h>
#include <VBox/vmm/pdmcritsect.h>
#include <VBox/vmm/pdmqueue.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/patm.h>
#include "EMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/vmm/uvm.h>
#include <VBox/vmm/cpumdis.h>
#include <VBox/dis.h>
#include <VBox/disopcode.h>
#include "VMMTracing.h"

#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/thread.h>


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static DECLCALLBACK(int) emR3Save(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int) emR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass);
#if defined(LOG_ENABLED) || defined(VBOX_STRICT)
static const char *emR3GetStateName(EMSTATE enmState);
#endif
static VBOXSTRICTRC emR3Debug(PVM pVM, PVMCPU pVCpu, VBOXSTRICTRC rc);
#if defined(VBOX_WITH_REM) || defined(DEBUG)
static int emR3RemStep(PVM pVM, PVMCPU pVCpu);
#endif
static int emR3RemExecute(PVM pVM, PVMCPU pVCpu, bool *pfFFDone);


/**
 * Initializes the EM.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
VMMR3_INT_DECL(int) EMR3Init(PVM pVM)
{
    LogFlow(("EMR3Init\n"));
    /*
     * Assert alignment and sizes.
     */
    AssertCompileMemberAlignment(VM, em.s, 32);
    AssertCompile(sizeof(pVM->em.s) <= sizeof(pVM->em.padding));
    AssertCompile(sizeof(pVM->aCpus[0].em.s.u.FatalLongJump) <= sizeof(pVM->aCpus[0].em.s.u.achPaddingFatalLongJump));

    /*
     * Init the structure.
     */
    pVM->em.s.offVM = RT_UOFFSETOF(VM, em.s);
    PCFGMNODE pCfgRoot = CFGMR3GetRoot(pVM);
    PCFGMNODE pCfgEM = CFGMR3GetChild(pCfgRoot, "EM");

    bool fEnabled;
    int rc = CFGMR3QueryBoolDef(pCfgRoot, "RawR3Enabled", &fEnabled, true);
    AssertLogRelRCReturn(rc, rc);
    pVM->fRecompileUser       = !fEnabled;

    rc = CFGMR3QueryBoolDef(pCfgRoot, "RawR0Enabled", &fEnabled, true);
    AssertLogRelRCReturn(rc, rc);
    pVM->fRecompileSupervisor = !fEnabled;

#ifdef VBOX_WITH_RAW_RING1
    rc = CFGMR3QueryBoolDef(pCfgRoot, "RawR1Enabled", &pVM->fRawRing1Enabled, false);
    AssertLogRelRCReturn(rc, rc);
#else
    pVM->fRawRing1Enabled = false;      /* Disabled by default. */
#endif

    rc = CFGMR3QueryBoolDef(pCfgEM, "IemExecutesAll", &pVM->em.s.fIemExecutesAll, false);
    AssertLogRelRCReturn(rc, rc);

    rc = CFGMR3QueryBoolDef(pCfgEM, "TripleFaultReset", &fEnabled, false);
    AssertLogRelRCReturn(rc, rc);
    pVM->em.s.fGuruOnTripleFault = !fEnabled;
    if (!pVM->em.s.fGuruOnTripleFault && pVM->cCpus > 1)
    {
        LogRel(("EM: Overriding /EM/TripleFaultReset, must be false on SMP.\n"));
        pVM->em.s.fGuruOnTripleFault = true;
    }

    LogRel(("EMR3Init: fRecompileUser=%RTbool fRecompileSupervisor=%RTbool fRawRing1Enabled=%RTbool fIemExecutesAll=%RTbool fGuruOnTripleFault=%RTbool\n",
            pVM->fRecompileUser, pVM->fRecompileSupervisor, pVM->fRawRing1Enabled, pVM->em.s.fIemExecutesAll, pVM->em.s.fGuruOnTripleFault));

    /** @cfgm{/EM/ExitOptimizationEnabled, bool, true}
     * Whether to try correlate exit history in any context, detect hot spots and
     * try optimize these using IEM if there are other exits close by.  This
     * overrides the context specific settings. */
    bool fExitOptimizationEnabled = true;
    rc = CFGMR3QueryBoolDef(pCfgEM, "ExitOptimizationEnabled", &fExitOptimizationEnabled, true);
    AssertLogRelRCReturn(rc, rc);

    /** @cfgm{/EM/ExitOptimizationEnabledR0, bool, true}
     * Whether to optimize exits in ring-0.  Setting this to false will also disable
     * the /EM/ExitOptimizationEnabledR0PreemptDisabled setting.  Depending on preemption
     * capabilities of the host kernel, this optimization may be unavailable. */
    bool fExitOptimizationEnabledR0 = true;
    rc = CFGMR3QueryBoolDef(pCfgEM, "ExitOptimizationEnabledR0", &fExitOptimizationEnabledR0, true);
    AssertLogRelRCReturn(rc, rc);
    fExitOptimizationEnabledR0 &= fExitOptimizationEnabled;

    /** @cfgm{/EM/ExitOptimizationEnabledR0PreemptDisabled, bool, false}
     * Whether to optimize exits in ring-0 when preemption is disable (or preemption
     * hooks are in effect). */
    /** @todo change the default to true here */
    bool fExitOptimizationEnabledR0PreemptDisabled = true;
    rc = CFGMR3QueryBoolDef(pCfgEM, "ExitOptimizationEnabledR0PreemptDisabled", &fExitOptimizationEnabledR0PreemptDisabled, false);
    AssertLogRelRCReturn(rc, rc);
    fExitOptimizationEnabledR0PreemptDisabled &= fExitOptimizationEnabledR0;

    /** @cfgm{/EM/HistoryExecMaxInstructions, integer, 16, 65535, 8192}
     * Maximum number of instruction to let EMHistoryExec execute in one go. */
    uint16_t cHistoryExecMaxInstructions = 8192;
    rc = CFGMR3QueryU16Def(pCfgEM, "HistoryExecMaxInstructions", &cHistoryExecMaxInstructions, cHistoryExecMaxInstructions);
    AssertLogRelRCReturn(rc, rc);
    if (cHistoryExecMaxInstructions < 16)
        return VMSetError(pVM, VERR_OUT_OF_RANGE, RT_SRC_POS, "/EM/HistoryExecMaxInstructions value is too small, min 16");

    /** @cfgm{/EM/HistoryProbeMaxInstructionsWithoutExit, integer, 2, 65535, 24 for HM, 32 for NEM}
     * Maximum number of instruction between exits during probing. */
    uint16_t cHistoryProbeMaxInstructionsWithoutExit = 24;
#ifdef RT_OS_WINDOWS
    if (VM_IS_NEM_ENABLED(pVM))
        cHistoryProbeMaxInstructionsWithoutExit = 32;
#endif
    rc = CFGMR3QueryU16Def(pCfgEM, "HistoryProbeMaxInstructionsWithoutExit", &cHistoryProbeMaxInstructionsWithoutExit,
                           cHistoryProbeMaxInstructionsWithoutExit);
    AssertLogRelRCReturn(rc, rc);
    if (cHistoryProbeMaxInstructionsWithoutExit < 2)
        return VMSetError(pVM, VERR_OUT_OF_RANGE, RT_SRC_POS,
                          "/EM/HistoryProbeMaxInstructionsWithoutExit value is too small, min 16");

    /** @cfgm{/EM/HistoryProbMinInstructions, integer, 0, 65535, depends}
     * The default is (/EM/HistoryProbeMaxInstructionsWithoutExit + 1) * 3. */
    uint16_t cHistoryProbeMinInstructions = cHistoryProbeMaxInstructionsWithoutExit < 0x5554
                                          ? (cHistoryProbeMaxInstructionsWithoutExit + 1) * 3 : 0xffff;
    rc = CFGMR3QueryU16Def(pCfgEM, "HistoryProbMinInstructions", &cHistoryProbeMinInstructions,
                           cHistoryProbeMinInstructions);
    AssertLogRelRCReturn(rc, rc);

    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        pVM->aCpus[i].em.s.fExitOptimizationEnabled                  = fExitOptimizationEnabled;
        pVM->aCpus[i].em.s.fExitOptimizationEnabledR0                = fExitOptimizationEnabledR0;
        pVM->aCpus[i].em.s.fExitOptimizationEnabledR0PreemptDisabled = fExitOptimizationEnabledR0PreemptDisabled;

        pVM->aCpus[i].em.s.cHistoryExecMaxInstructions               = cHistoryExecMaxInstructions;
        pVM->aCpus[i].em.s.cHistoryProbeMinInstructions              = cHistoryProbeMinInstructions;
        pVM->aCpus[i].em.s.cHistoryProbeMaxInstructionsWithoutExit   = cHistoryProbeMaxInstructionsWithoutExit;
    }

#ifdef VBOX_WITH_REM
    /*
     * Initialize the REM critical section.
     */
    AssertCompileMemberAlignment(EM, CritSectREM, sizeof(uintptr_t));
    rc = PDMR3CritSectInit(pVM, &pVM->em.s.CritSectREM, RT_SRC_POS, "EM-REM");
    AssertRCReturn(rc, rc);
#endif

    /*
     * Saved state.
     */
    rc = SSMR3RegisterInternal(pVM, "em", 0, EM_SAVED_STATE_VERSION, 16,
                               NULL, NULL, NULL,
                               NULL, emR3Save, NULL,
                               NULL, emR3Load, NULL);
    if (RT_FAILURE(rc))
        return rc;

    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        pVCpu->em.s.enmState            = i == 0 ? EMSTATE_NONE : EMSTATE_WAIT_SIPI;
        pVCpu->em.s.enmPrevState        = EMSTATE_NONE;
        pVCpu->em.s.fForceRAW           = false;
        pVCpu->em.s.u64TimeSliceStart   = 0; /* paranoia */
        pVCpu->em.s.idxContinueExitRec  = UINT16_MAX;

#ifdef VBOX_WITH_RAW_MODE
        if (VM_IS_RAW_MODE_ENABLED(pVM))
        {
            pVCpu->em.s.pPatmGCState = PATMR3QueryGCStateHC(pVM);
            AssertMsg(pVCpu->em.s.pPatmGCState, ("PATMR3QueryGCStateHC failed!\n"));
        }
#endif

# define EM_REG_COUNTER(a, b, c) \
        rc = STAMR3RegisterF(pVM, a, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, c, b, i); \
        AssertRC(rc);

# define EM_REG_COUNTER_USED(a, b, c) \
        rc = STAMR3RegisterF(pVM, a, STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES, c, b, i); \
        AssertRC(rc);

# define EM_REG_PROFILE(a, b, c) \
        rc = STAMR3RegisterF(pVM, a, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, c, b, i); \
        AssertRC(rc);

# define EM_REG_PROFILE_ADV(a, b, c) \
        rc = STAMR3RegisterF(pVM, a, STAMTYPE_PROFILE_ADV, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, c, b, i); \
        AssertRC(rc);

        /*
         * Statistics.
         */
#ifdef VBOX_WITH_STATISTICS
        PEMSTATS pStats;
        rc = MMHyperAlloc(pVM, sizeof(*pStats), 0, MM_TAG_EM, (void **)&pStats);
        if (RT_FAILURE(rc))
            return rc;

        pVCpu->em.s.pStatsR3 = pStats;
        pVCpu->em.s.pStatsR0 = MMHyperR3ToR0(pVM, pStats);
        pVCpu->em.s.pStatsRC = MMHyperR3ToRC(pVM, pStats);

# if 1 /* rawmode only? */
        EM_REG_COUNTER_USED(&pStats->StatIoRestarted,            "/EM/CPU%d/R3/PrivInst/IoRestarted",        "I/O instructions restarted in ring-3.");
        EM_REG_COUNTER_USED(&pStats->StatIoIem,                  "/EM/CPU%d/R3/PrivInst/IoIem",              "I/O instructions end to IEM in ring-3.");
        EM_REG_COUNTER_USED(&pStats->StatCli,                    "/EM/CPU%d/R3/PrivInst/Cli",                "Number of cli instructions.");
        EM_REG_COUNTER_USED(&pStats->StatSti,                    "/EM/CPU%d/R3/PrivInst/Sti",                "Number of sli instructions.");
        EM_REG_COUNTER_USED(&pStats->StatHlt,                    "/EM/CPU%d/R3/PrivInst/Hlt",                "Number of hlt instructions not handled in GC because of PATM.");
        EM_REG_COUNTER_USED(&pStats->StatInvlpg,                 "/EM/CPU%d/R3/PrivInst/Invlpg",             "Number of invlpg instructions.");
        EM_REG_COUNTER_USED(&pStats->StatMisc,                   "/EM/CPU%d/R3/PrivInst/Misc",               "Number of misc. instructions.");
        EM_REG_COUNTER_USED(&pStats->StatMovWriteCR[0],          "/EM/CPU%d/R3/PrivInst/Mov CR0, X",         "Number of mov CR0 write instructions.");
        EM_REG_COUNTER_USED(&pStats->StatMovWriteCR[1],          "/EM/CPU%d/R3/PrivInst/Mov CR1, X",         "Number of mov CR1 write instructions.");
        EM_REG_COUNTER_USED(&pStats->StatMovWriteCR[2],          "/EM/CPU%d/R3/PrivInst/Mov CR2, X",         "Number of mov CR2 write instructions.");
        EM_REG_COUNTER_USED(&pStats->StatMovWriteCR[3],          "/EM/CPU%d/R3/PrivInst/Mov CR3, X",         "Number of mov CR3 write instructions.");
        EM_REG_COUNTER_USED(&pStats->StatMovWriteCR[4],          "/EM/CPU%d/R3/PrivInst/Mov CR4, X",         "Number of mov CR4 write instructions.");
        EM_REG_COUNTER_USED(&pStats->StatMovReadCR[0],           "/EM/CPU%d/R3/PrivInst/Mov X, CR0",         "Number of mov CR0 read instructions.");
        EM_REG_COUNTER_USED(&pStats->StatMovReadCR[1],           "/EM/CPU%d/R3/PrivInst/Mov X, CR1",         "Number of mov CR1 read instructions.");
        EM_REG_COUNTER_USED(&pStats->StatMovReadCR[2],           "/EM/CPU%d/R3/PrivInst/Mov X, CR2",         "Number of mov CR2 read instructions.");
        EM_REG_COUNTER_USED(&pStats->StatMovReadCR[3],           "/EM/CPU%d/R3/PrivInst/Mov X, CR3",         "Number of mov CR3 read instructions.");
        EM_REG_COUNTER_USED(&pStats->StatMovReadCR[4],           "/EM/CPU%d/R3/PrivInst/Mov X, CR4",         "Number of mov CR4 read instructions.");
        EM_REG_COUNTER_USED(&pStats->StatMovDRx,                 "/EM/CPU%d/R3/PrivInst/MovDRx",             "Number of mov DRx instructions.");
        EM_REG_COUNTER_USED(&pStats->StatIret,                   "/EM/CPU%d/R3/PrivInst/Iret",               "Number of iret instructions.");
        EM_REG_COUNTER_USED(&pStats->StatMovLgdt,                "/EM/CPU%d/R3/PrivInst/Lgdt",               "Number of lgdt instructions.");
        EM_REG_COUNTER_USED(&pStats->StatMovLidt,                "/EM/CPU%d/R3/PrivInst/Lidt",               "Number of lidt instructions.");
        EM_REG_COUNTER_USED(&pStats->StatMovLldt,                "/EM/CPU%d/R3/PrivInst/Lldt",               "Number of lldt instructions.");
        EM_REG_COUNTER_USED(&pStats->StatSysEnter,               "/EM/CPU%d/R3/PrivInst/Sysenter",           "Number of sysenter instructions.");
        EM_REG_COUNTER_USED(&pStats->StatSysExit,                "/EM/CPU%d/R3/PrivInst/Sysexit",            "Number of sysexit instructions.");
        EM_REG_COUNTER_USED(&pStats->StatSysCall,                "/EM/CPU%d/R3/PrivInst/Syscall",            "Number of syscall instructions.");
        EM_REG_COUNTER_USED(&pStats->StatSysRet,                 "/EM/CPU%d/R3/PrivInst/Sysret",             "Number of sysret instructions.");
        EM_REG_COUNTER(&pVCpu->em.s.StatTotalClis,               "/EM/CPU%d/Cli/Total",                      "Total number of cli instructions executed.");
#endif
        pVCpu->em.s.pCliStatTree = 0;

        /* these should be considered for release statistics. */
        EM_REG_COUNTER(&pVCpu->em.s.StatIOEmu,              "/PROF/CPU%d/EM/Emulation/IO",      "Profiling of emR3RawExecuteIOInstruction.");
        EM_REG_COUNTER(&pVCpu->em.s.StatPrivEmu,            "/PROF/CPU%d/EM/Emulation/Priv",    "Profiling of emR3RawPrivileged.");
        EM_REG_PROFILE(&pVCpu->em.s.StatHMEntry,            "/PROF/CPU%d/EM/HMEnter",           "Profiling Hardware Accelerated Mode entry overhead.");
        EM_REG_PROFILE(&pVCpu->em.s.StatHMExec,             "/PROF/CPU%d/EM/HMExec",            "Profiling Hardware Accelerated Mode execution.");
        EM_REG_COUNTER(&pVCpu->em.s.StatHMExecuteCalled,    "/PROF/CPU%d/EM/HMExecuteCalled",   "Number of times enmR3HMExecute is called.");
        EM_REG_PROFILE(&pVCpu->em.s.StatIEMEmu,             "/PROF/CPU%d/EM/IEMEmuSingle",      "Profiling single instruction IEM execution.");
        EM_REG_PROFILE(&pVCpu->em.s.StatIEMThenREM,         "/PROF/CPU%d/EM/IEMThenRem",        "Profiling IEM-then-REM instruction execution (by IEM).");
        EM_REG_PROFILE(&pVCpu->em.s.StatNEMEntry,           "/PROF/CPU%d/EM/NEMEnter",          "Profiling NEM entry overhead.");
#endif /* VBOX_WITH_STATISTICS */
        EM_REG_PROFILE(&pVCpu->em.s.StatNEMExec,            "/PROF/CPU%d/EM/NEMExec",           "Profiling NEM execution.");
        EM_REG_COUNTER(&pVCpu->em.s.StatNEMExecuteCalled,   "/PROF/CPU%d/EM/NEMExecuteCalled",  "Number of times enmR3NEMExecute is called.");
#ifdef VBOX_WITH_STATISTICS
        EM_REG_PROFILE(&pVCpu->em.s.StatREMEmu,             "/PROF/CPU%d/EM/REMEmuSingle",      "Profiling single instruction REM execution.");
        EM_REG_PROFILE(&pVCpu->em.s.StatREMExec,            "/PROF/CPU%d/EM/REMExec",           "Profiling REM execution.");
        EM_REG_PROFILE(&pVCpu->em.s.StatREMSync,            "/PROF/CPU%d/EM/REMSync",           "Profiling REM context syncing.");
        EM_REG_PROFILE(&pVCpu->em.s.StatRAWEntry,           "/PROF/CPU%d/EM/RAWEnter",          "Profiling Raw Mode entry overhead.");
        EM_REG_PROFILE(&pVCpu->em.s.StatRAWExec,            "/PROF/CPU%d/EM/RAWExec",           "Profiling Raw Mode execution.");
        EM_REG_PROFILE(&pVCpu->em.s.StatRAWTail,            "/PROF/CPU%d/EM/RAWTail",           "Profiling Raw Mode tail overhead.");
#endif /* VBOX_WITH_STATISTICS */

        EM_REG_COUNTER(&pVCpu->em.s.StatForcedActions,      "/PROF/CPU%d/EM/ForcedActions",     "Profiling forced action execution.");
        EM_REG_COUNTER(&pVCpu->em.s.StatHalted,             "/PROF/CPU%d/EM/Halted",            "Profiling halted state (VMR3WaitHalted).");
        EM_REG_PROFILE_ADV(&pVCpu->em.s.StatCapped,         "/PROF/CPU%d/EM/Capped",            "Profiling capped state (sleep).");
        EM_REG_COUNTER(&pVCpu->em.s.StatREMTotal,           "/PROF/CPU%d/EM/REMTotal",          "Profiling emR3RemExecute (excluding FFs).");
        EM_REG_COUNTER(&pVCpu->em.s.StatRAWTotal,           "/PROF/CPU%d/EM/RAWTotal",          "Profiling emR3RawExecute (excluding FFs).");

        EM_REG_PROFILE_ADV(&pVCpu->em.s.StatTotal,          "/PROF/CPU%d/EM/Total",             "Profiling EMR3ExecuteVM.");

        rc = STAMR3RegisterF(pVM, &pVCpu->em.s.iNextExit, STAMTYPE_U64, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,
                             "Number of recorded exits.", "/PROF/CPU%u/EM/RecordedExits", i);
        AssertRC(rc);

        /* History record statistics */
        rc = STAMR3RegisterF(pVM, &pVCpu->em.s.cExitRecordUsed, STAMTYPE_U32, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,
                             "Number of used hash table entries.", "/EM/CPU%u/ExitHashing/Used", i);
        AssertRC(rc);

        for (uint32_t iStep = 0; iStep < RT_ELEMENTS(pVCpu->em.s.aStatHistoryRecHits); iStep++)
        {
            rc = STAMR3RegisterF(pVM, &pVCpu->em.s.aStatHistoryRecHits[iStep], STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES,
                                 "Number of hits at this step.",             "/EM/CPU%u/ExitHashing/Step%02u-Hits", i, iStep);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pVCpu->em.s.aStatHistoryRecTypeChanged[iStep], STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES,
                                 "Number of type changes at this step.",     "/EM/CPU%u/ExitHashing/Step%02u-TypeChanges", i, iStep);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pVCpu->em.s.aStatHistoryRecTypeChanged[iStep], STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES,
                                 "Number of replacments at this step.",     "/EM/CPU%u/ExitHashing/Step%02u-Replacments", i, iStep);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pVCpu->em.s.aStatHistoryRecNew[iStep], STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES,
                                 "Number of new inserts at this step.",     "/EM/CPU%u/ExitHashing/Step%02u-NewInserts", i, iStep);
            AssertRC(rc);
        }

        EM_REG_PROFILE(&pVCpu->em.s.StatHistoryExec,              "/EM/CPU%d/ExitOpt/Exec",              "Profiling normal EMHistoryExec operation.");
        EM_REG_COUNTER(&pVCpu->em.s.StatHistoryExecSavedExits,    "/EM/CPU%d/ExitOpt/ExecSavedExit",     "Net number of saved exits.");
        EM_REG_COUNTER(&pVCpu->em.s.StatHistoryExecInstructions,  "/EM/CPU%d/ExitOpt/ExecInstructions",  "Number of instructions executed during normal operation.");
        EM_REG_PROFILE(&pVCpu->em.s.StatHistoryProbe,             "/EM/CPU%d/ExitOpt/Probe",             "Profiling EMHistoryExec when probing.");
        EM_REG_COUNTER(&pVCpu->em.s.StatHistoryProbeInstructions, "/EM/CPU%d/ExitOpt/ProbeInstructions", "Number of instructions executed during probing.");
        EM_REG_COUNTER(&pVCpu->em.s.StatHistoryProbedNormal,      "/EM/CPU%d/ExitOpt/ProbedNormal",      "Number of EMEXITACTION_NORMAL_PROBED results.");
        EM_REG_COUNTER(&pVCpu->em.s.StatHistoryProbedExecWithMax, "/EM/CPU%d/ExitOpt/ProbedExecWithMax", "Number of EMEXITACTION_EXEC_WITH_MAX results.");
        EM_REG_COUNTER(&pVCpu->em.s.StatHistoryProbedToRing3,     "/EM/CPU%d/ExitOpt/ProbedToRing3",     "Number of ring-3 probe continuations.");
    }

    emR3InitDbg(pVM);
    return VINF_SUCCESS;
}


/**
 * Called when a VM initialization stage is completed.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   enmWhat         The initialization state that was completed.
 */
VMMR3_INT_DECL(int) EMR3InitCompleted(PVM pVM, VMINITCOMPLETED enmWhat)
{
    if (enmWhat == VMINITCOMPLETED_RING0)
        LogRel(("EM: Exit history optimizations: enabled=%RTbool enabled-r0=%RTbool enabled-r0-no-preemption=%RTbool\n",
                pVM->aCpus[0].em.s.fExitOptimizationEnabled, pVM->aCpus[0].em.s.fExitOptimizationEnabledR0,
                pVM->aCpus[0].em.s.fExitOptimizationEnabledR0PreemptDisabled));
    return VINF_SUCCESS;
}


/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * @param   pVM     The cross context VM structure.
 */
VMMR3_INT_DECL(void) EMR3Relocate(PVM pVM)
{
    LogFlow(("EMR3Relocate\n"));
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];
        if (pVCpu->em.s.pStatsR3)
            pVCpu->em.s.pStatsRC = MMHyperR3ToRC(pVM, pVCpu->em.s.pStatsR3);
    }
}


/**
 * Reset the EM state for a CPU.
 *
 * Called by EMR3Reset and hot plugging.
 *
 * @param   pVCpu   The cross context virtual CPU structure.
 */
VMMR3_INT_DECL(void) EMR3ResetCpu(PVMCPU pVCpu)
{
    /* Reset scheduling state. */
    pVCpu->em.s.fForceRAW = false;
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_UNHALT);

    /* VMR3ResetFF may return VINF_EM_RESET or VINF_EM_SUSPEND, so transition
       out of the HALTED state here so that enmPrevState doesn't end up as
       HALTED when EMR3Execute returns. */
    if (pVCpu->em.s.enmState == EMSTATE_HALTED)
    {
        Log(("EMR3ResetCpu: Cpu#%u %s -> %s\n", pVCpu->idCpu, emR3GetStateName(pVCpu->em.s.enmState), pVCpu->idCpu == 0 ? "EMSTATE_NONE" : "EMSTATE_WAIT_SIPI"));
        pVCpu->em.s.enmState = pVCpu->idCpu == 0 ? EMSTATE_NONE : EMSTATE_WAIT_SIPI;
    }
}


/**
 * Reset notification.
 *
 * @param   pVM         The cross context VM structure.
 */
VMMR3_INT_DECL(void) EMR3Reset(PVM pVM)
{
    Log(("EMR3Reset: \n"));
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
        EMR3ResetCpu(&pVM->aCpus[i]);
}


/**
 * Terminates the EM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
VMMR3_INT_DECL(int) EMR3Term(PVM pVM)
{
    AssertMsg(pVM->em.s.offVM, ("bad init order!\n"));

#ifdef VBOX_WITH_REM
    PDMR3CritSectDelete(&pVM->em.s.CritSectREM);
#else
    RT_NOREF(pVM);
#endif
    return VINF_SUCCESS;
}


/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pSSM            SSM operation handle.
 */
static DECLCALLBACK(int) emR3Save(PVM pVM, PSSMHANDLE pSSM)
{
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        SSMR3PutBool(pSSM, pVCpu->em.s.fForceRAW);

        Assert(pVCpu->em.s.enmState     == EMSTATE_SUSPENDED);
        Assert(pVCpu->em.s.enmPrevState != EMSTATE_SUSPENDED);
        SSMR3PutU32(pSSM, pVCpu->em.s.enmPrevState);

        /* Save mwait state. */
        SSMR3PutU32(pSSM, pVCpu->em.s.MWait.fWait);
        SSMR3PutGCPtr(pSSM, pVCpu->em.s.MWait.uMWaitRAX);
        SSMR3PutGCPtr(pSSM, pVCpu->em.s.MWait.uMWaitRCX);
        SSMR3PutGCPtr(pSSM, pVCpu->em.s.MWait.uMonitorRAX);
        SSMR3PutGCPtr(pSSM, pVCpu->em.s.MWait.uMonitorRCX);
        int rc = SSMR3PutGCPtr(pSSM, pVCpu->em.s.MWait.uMonitorRDX);
        AssertRCReturn(rc, rc);
    }
    return VINF_SUCCESS;
}


/**
 * Execute state load operation.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pSSM            SSM operation handle.
 * @param   uVersion        Data layout version.
 * @param   uPass           The data pass.
 */
static DECLCALLBACK(int) emR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    /*
     * Validate version.
     */
    if (    uVersion > EM_SAVED_STATE_VERSION
        ||  uVersion < EM_SAVED_STATE_VERSION_PRE_SMP)
    {
        AssertMsgFailed(("emR3Load: Invalid version uVersion=%d (current %d)!\n", uVersion, EM_SAVED_STATE_VERSION));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }
    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    /*
     * Load the saved state.
     */
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        int rc = SSMR3GetBool(pSSM, &pVCpu->em.s.fForceRAW);
        if (RT_FAILURE(rc))
            pVCpu->em.s.fForceRAW = false;
        AssertRCReturn(rc, rc);

        if (uVersion > EM_SAVED_STATE_VERSION_PRE_SMP)
        {
            AssertCompile(sizeof(pVCpu->em.s.enmPrevState) == sizeof(uint32_t));
            rc = SSMR3GetU32(pSSM, (uint32_t *)&pVCpu->em.s.enmPrevState);
            AssertRCReturn(rc, rc);
            Assert(pVCpu->em.s.enmPrevState != EMSTATE_SUSPENDED);

            pVCpu->em.s.enmState = EMSTATE_SUSPENDED;
        }
        if (uVersion > EM_SAVED_STATE_VERSION_PRE_MWAIT)
        {
            /* Load mwait state. */
            rc = SSMR3GetU32(pSSM, &pVCpu->em.s.MWait.fWait);
            AssertRCReturn(rc, rc);
            rc = SSMR3GetGCPtr(pSSM, &pVCpu->em.s.MWait.uMWaitRAX);
            AssertRCReturn(rc, rc);
            rc = SSMR3GetGCPtr(pSSM, &pVCpu->em.s.MWait.uMWaitRCX);
            AssertRCReturn(rc, rc);
            rc = SSMR3GetGCPtr(pSSM, &pVCpu->em.s.MWait.uMonitorRAX);
            AssertRCReturn(rc, rc);
            rc = SSMR3GetGCPtr(pSSM, &pVCpu->em.s.MWait.uMonitorRCX);
            AssertRCReturn(rc, rc);
            rc = SSMR3GetGCPtr(pSSM, &pVCpu->em.s.MWait.uMonitorRDX);
            AssertRCReturn(rc, rc);
        }

        Assert(!pVCpu->em.s.pCliStatTree);
    }
    return VINF_SUCCESS;
}


/**
 * Argument packet for emR3SetExecutionPolicy.
 */
struct EMR3SETEXECPOLICYARGS
{
    EMEXECPOLICY    enmPolicy;
    bool            fEnforce;
};


/**
 * @callback_method_impl{FNVMMEMTRENDEZVOUS, Rendezvous callback for EMR3SetExecutionPolicy.}
 */
static DECLCALLBACK(VBOXSTRICTRC) emR3SetExecutionPolicy(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    /*
     * Only the first CPU changes the variables.
     */
    if (pVCpu->idCpu == 0)
    {
        struct EMR3SETEXECPOLICYARGS *pArgs = (struct EMR3SETEXECPOLICYARGS *)pvUser;
        switch (pArgs->enmPolicy)
        {
            case EMEXECPOLICY_RECOMPILE_RING0:
                pVM->fRecompileSupervisor = pArgs->fEnforce;
                break;
            case EMEXECPOLICY_RECOMPILE_RING3:
                pVM->fRecompileUser       = pArgs->fEnforce;
                break;
            case EMEXECPOLICY_IEM_ALL:
                pVM->em.s.fIemExecutesAll = pArgs->fEnforce;
                break;
            default:
                AssertFailedReturn(VERR_INVALID_PARAMETER);
        }
        LogRel(("emR3SetExecutionPolicy: fRecompileUser=%RTbool fRecompileSupervisor=%RTbool fIemExecutesAll=%RTbool\n",
                pVM->fRecompileUser, pVM->fRecompileSupervisor, pVM->em.s.fIemExecutesAll));
    }

    /*
     * Force rescheduling if in RAW, HM, NEM, IEM, or REM.
     */
    return    pVCpu->em.s.enmState == EMSTATE_RAW
           || pVCpu->em.s.enmState == EMSTATE_HM
           || pVCpu->em.s.enmState == EMSTATE_NEM
           || pVCpu->em.s.enmState == EMSTATE_IEM
           || pVCpu->em.s.enmState == EMSTATE_REM
           || pVCpu->em.s.enmState == EMSTATE_IEM_THEN_REM
         ? VINF_EM_RESCHEDULE
         : VINF_SUCCESS;
}


/**
 * Changes an execution scheduling policy parameter.
 *
 * This is used to enable or disable raw-mode / hardware-virtualization
 * execution of user and supervisor code.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VINF_RESCHEDULE if a rescheduling might be required.
 * @returns VERR_INVALID_PARAMETER on an invalid enmMode value.
 *
 * @param   pUVM            The user mode VM handle.
 * @param   enmPolicy       The scheduling policy to change.
 * @param   fEnforce        Whether to enforce the policy or not.
 */
VMMR3DECL(int) EMR3SetExecutionPolicy(PUVM pUVM, EMEXECPOLICY enmPolicy, bool fEnforce)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    VM_ASSERT_VALID_EXT_RETURN(pUVM->pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(enmPolicy > EMEXECPOLICY_INVALID && enmPolicy < EMEXECPOLICY_END, VERR_INVALID_PARAMETER);

    struct EMR3SETEXECPOLICYARGS Args = { enmPolicy, fEnforce };
    return VMMR3EmtRendezvous(pUVM->pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_DESCENDING, emR3SetExecutionPolicy, &Args);
}


/**
 * Queries an execution scheduling policy parameter.
 *
 * @returns VBox status code
 * @param   pUVM            The user mode VM handle.
 * @param   enmPolicy       The scheduling policy to query.
 * @param   pfEnforced      Where to return the current value.
 */
VMMR3DECL(int) EMR3QueryExecutionPolicy(PUVM pUVM, EMEXECPOLICY enmPolicy, bool *pfEnforced)
{
    AssertReturn(enmPolicy > EMEXECPOLICY_INVALID && enmPolicy < EMEXECPOLICY_END, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pfEnforced, VERR_INVALID_POINTER);
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    PVM pVM = pUVM->pVM;
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);

    /* No need to bother EMTs with a query. */
    switch (enmPolicy)
    {
        case EMEXECPOLICY_RECOMPILE_RING0:
            *pfEnforced = pVM->fRecompileSupervisor;
            break;
        case EMEXECPOLICY_RECOMPILE_RING3:
            *pfEnforced = pVM->fRecompileUser;
            break;
        case EMEXECPOLICY_IEM_ALL:
            *pfEnforced = pVM->em.s.fIemExecutesAll;
            break;
        default:
            AssertFailedReturn(VERR_INTERNAL_ERROR_2);
    }

    return VINF_SUCCESS;
}


/**
 * Queries the main execution engine of the VM.
 *
 * @returns VBox status code
 * @param   pUVM                    The user mode VM handle.
 * @param   pbMainExecutionEngine   Where to return the result, VM_EXEC_ENGINE_XXX.
 */
VMMR3DECL(int) EMR3QueryMainExecutionEngine(PUVM pUVM, uint8_t *pbMainExecutionEngine)
{
    AssertPtrReturn(pbMainExecutionEngine, VERR_INVALID_POINTER);
    *pbMainExecutionEngine = VM_EXEC_ENGINE_NOT_SET;

    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    PVM pVM = pUVM->pVM;
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);

    *pbMainExecutionEngine = pVM->bMainExecutionEngine;
    return VINF_SUCCESS;
}


/**
 * Raise a fatal error.
 *
 * Safely terminate the VM with full state report and stuff. This function
 * will naturally never return.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   rc          VBox status code.
 */
VMMR3DECL(void) EMR3FatalError(PVMCPU pVCpu, int rc)
{
    pVCpu->em.s.enmState = EMSTATE_GURU_MEDITATION;
    longjmp(pVCpu->em.s.u.FatalLongJump, rc);
}


#if defined(LOG_ENABLED) || defined(VBOX_STRICT)
/**
 * Gets the EM state name.
 *
 * @returns pointer to read only state name,
 * @param   enmState    The state.
 */
static const char *emR3GetStateName(EMSTATE enmState)
{
    switch (enmState)
    {
        case EMSTATE_NONE:              return "EMSTATE_NONE";
        case EMSTATE_RAW:               return "EMSTATE_RAW";
        case EMSTATE_HM:                return "EMSTATE_HM";
        case EMSTATE_IEM:               return "EMSTATE_IEM";
        case EMSTATE_REM:               return "EMSTATE_REM";
        case EMSTATE_HALTED:            return "EMSTATE_HALTED";
        case EMSTATE_WAIT_SIPI:         return "EMSTATE_WAIT_SIPI";
        case EMSTATE_SUSPENDED:         return "EMSTATE_SUSPENDED";
        case EMSTATE_TERMINATING:       return "EMSTATE_TERMINATING";
        case EMSTATE_DEBUG_GUEST_RAW:   return "EMSTATE_DEBUG_GUEST_RAW";
        case EMSTATE_DEBUG_GUEST_HM:    return "EMSTATE_DEBUG_GUEST_HM";
        case EMSTATE_DEBUG_GUEST_IEM:   return "EMSTATE_DEBUG_GUEST_IEM";
        case EMSTATE_DEBUG_GUEST_REM:   return "EMSTATE_DEBUG_GUEST_REM";
        case EMSTATE_DEBUG_HYPER:       return "EMSTATE_DEBUG_HYPER";
        case EMSTATE_GURU_MEDITATION:   return "EMSTATE_GURU_MEDITATION";
        case EMSTATE_IEM_THEN_REM:      return "EMSTATE_IEM_THEN_REM";
        case EMSTATE_NEM:               return "EMSTATE_NEM";
        case EMSTATE_DEBUG_GUEST_NEM:   return "EMSTATE_DEBUG_GUEST_NEM";
        default:                        return "Unknown!";
    }
}
#endif /* LOG_ENABLED || VBOX_STRICT */


/**
 * Handle pending ring-3 I/O port write.
 *
 * This is in response to a VINF_EM_PENDING_R3_IOPORT_WRITE status code returned
 * by EMRZSetPendingIoPortWrite() in ring-0 or raw-mode context.
 *
 * @returns Strict VBox status code.
 * @param   pVM     The cross context VM structure.
 * @param   pVCpu   The cross context virtual CPU structure.
 */
VBOXSTRICTRC emR3ExecutePendingIoPortWrite(PVM pVM, PVMCPU pVCpu)
{
    CPUM_ASSERT_NOT_EXTRN(pVCpu, CPUMCTX_EXTRN_RIP | CPUMCTX_EXTRN_RFLAGS);

    /* Get and clear the pending data. */
    RTIOPORT const uPort   = pVCpu->em.s.PendingIoPortAccess.uPort;
    uint32_t const uValue  = pVCpu->em.s.PendingIoPortAccess.uValue;
    uint8_t  const cbValue = pVCpu->em.s.PendingIoPortAccess.cbValue;
    uint8_t  const cbInstr = pVCpu->em.s.PendingIoPortAccess.cbInstr;
    pVCpu->em.s.PendingIoPortAccess.cbValue = 0;

    /* Assert sanity. */
    switch (cbValue)
    {
        case 1:     Assert(!(cbValue & UINT32_C(0xffffff00))); break;
        case 2:     Assert(!(cbValue & UINT32_C(0xffff0000))); break;
        case 4:     break;
        default:    AssertMsgFailedReturn(("cbValue=%#x\n", cbValue), VERR_EM_INTERNAL_ERROR);
    }
    AssertReturn(cbInstr <= 15 && cbInstr >= 1, VERR_EM_INTERNAL_ERROR);

    /* Do the work.*/
    VBOXSTRICTRC rcStrict = IOMIOPortWrite(pVM, pVCpu, uPort, uValue, cbValue);
    LogFlow(("EM/OUT: %#x, %#x LB %u -> %Rrc\n", uPort, uValue, cbValue, VBOXSTRICTRC_VAL(rcStrict) ));
    if (IOM_SUCCESS(rcStrict))
    {
        pVCpu->cpum.GstCtx.rip += cbInstr;
        pVCpu->cpum.GstCtx.rflags.Bits.u1RF = 0;
    }
    return rcStrict;
}


/**
 * Handle pending ring-3 I/O port write.
 *
 * This is in response to a VINF_EM_PENDING_R3_IOPORT_WRITE status code returned
 * by EMRZSetPendingIoPortRead() in ring-0 or raw-mode context.
 *
 * @returns Strict VBox status code.
 * @param   pVM     The cross context VM structure.
 * @param   pVCpu   The cross context virtual CPU structure.
 */
VBOXSTRICTRC emR3ExecutePendingIoPortRead(PVM pVM, PVMCPU pVCpu)
{
    CPUM_ASSERT_NOT_EXTRN(pVCpu, CPUMCTX_EXTRN_RIP | CPUMCTX_EXTRN_RFLAGS | CPUMCTX_EXTRN_RAX);

    /* Get and clear the pending data. */
    RTIOPORT const uPort   = pVCpu->em.s.PendingIoPortAccess.uPort;
    uint8_t  const cbValue = pVCpu->em.s.PendingIoPortAccess.cbValue;
    uint8_t  const cbInstr = pVCpu->em.s.PendingIoPortAccess.cbInstr;
    pVCpu->em.s.PendingIoPortAccess.cbValue = 0;

    /* Assert sanity. */
    switch (cbValue)
    {
        case 1:     break;
        case 2:     break;
        case 4:     break;
        default:    AssertMsgFailedReturn(("cbValue=%#x\n", cbValue), VERR_EM_INTERNAL_ERROR);
    }
    AssertReturn(pVCpu->em.s.PendingIoPortAccess.uValue == UINT32_C(0x52454144) /* READ*/, VERR_EM_INTERNAL_ERROR);
    AssertReturn(cbInstr <= 15 && cbInstr >= 1, VERR_EM_INTERNAL_ERROR);

    /* Do the work.*/
    uint32_t uValue = 0;
    VBOXSTRICTRC rcStrict = IOMIOPortRead(pVM, pVCpu, uPort, &uValue, cbValue);
    LogFlow(("EM/IN: %#x LB %u -> %Rrc, %#x\n", uPort, cbValue, VBOXSTRICTRC_VAL(rcStrict), uValue ));
    if (IOM_SUCCESS(rcStrict))
    {
        if (cbValue == 4)
            pVCpu->cpum.GstCtx.rax = uValue;
        else if (cbValue == 2)
            pVCpu->cpum.GstCtx.ax = (uint16_t)uValue;
        else
            pVCpu->cpum.GstCtx.al = (uint8_t)uValue;
        pVCpu->cpum.GstCtx.rip += cbInstr;
        pVCpu->cpum.GstCtx.rflags.Bits.u1RF = 0;
    }
    return rcStrict;
}


/**
 * Debug loop.
 *
 * @returns VBox status code for EM.
 * @param   pVM     The cross context VM structure.
 * @param   pVCpu   The cross context virtual CPU structure.
 * @param   rc      Current EM VBox status code.
 */
static VBOXSTRICTRC emR3Debug(PVM pVM, PVMCPU pVCpu, VBOXSTRICTRC rc)
{
    for (;;)
    {
        Log(("emR3Debug: rc=%Rrc\n", VBOXSTRICTRC_VAL(rc)));
        const VBOXSTRICTRC rcLast = rc;

        /*
         * Debug related RC.
         */
        switch (VBOXSTRICTRC_VAL(rc))
        {
            /*
             * Single step an instruction.
             */
            case VINF_EM_DBG_STEP:
                if (   pVCpu->em.s.enmState == EMSTATE_DEBUG_GUEST_RAW
                    || pVCpu->em.s.enmState == EMSTATE_DEBUG_HYPER
                    || pVCpu->em.s.fForceRAW /* paranoia */)
#ifdef VBOX_WITH_RAW_MODE
                    rc = emR3RawStep(pVM, pVCpu);
#else
                    AssertLogRelMsgFailedStmt(("Bad EM state."), VERR_EM_INTERNAL_ERROR);
#endif
                else if (pVCpu->em.s.enmState == EMSTATE_DEBUG_GUEST_HM)
                    rc = EMR3HmSingleInstruction(pVM, pVCpu, 0 /*fFlags*/);
                else if (pVCpu->em.s.enmState == EMSTATE_DEBUG_GUEST_NEM)
                    rc = VBOXSTRICTRC_TODO(emR3NemSingleInstruction(pVM, pVCpu, 0 /*fFlags*/));
#ifdef VBOX_WITH_REM
                else if (pVCpu->em.s.enmState == EMSTATE_DEBUG_GUEST_REM)
                    rc = emR3RemStep(pVM, pVCpu);
#endif
                else
                {
                    rc = IEMExecOne(pVCpu); /** @todo add dedicated interface... */
                    if (rc == VINF_SUCCESS || rc == VINF_EM_RESCHEDULE)
                        rc = VINF_EM_DBG_STEPPED;
                }
                break;

            /*
             * Simple events: stepped, breakpoint, stop/assertion.
             */
            case VINF_EM_DBG_STEPPED:
                rc = DBGFR3Event(pVM, DBGFEVENT_STEPPED);
                break;

            case VINF_EM_DBG_BREAKPOINT:
                rc = DBGFR3EventBreakpoint(pVM, DBGFEVENT_BREAKPOINT);
                break;

            case VINF_EM_DBG_STOP:
                rc = DBGFR3EventSrc(pVM, DBGFEVENT_DEV_STOP, NULL, 0, NULL, NULL);
                break;

            case VINF_EM_DBG_EVENT:
                rc = DBGFR3EventHandlePending(pVM, pVCpu);
                break;

            case VINF_EM_DBG_HYPER_STEPPED:
                rc = DBGFR3Event(pVM, DBGFEVENT_STEPPED_HYPER);
                break;

            case VINF_EM_DBG_HYPER_BREAKPOINT:
                rc = DBGFR3EventBreakpoint(pVM, DBGFEVENT_BREAKPOINT_HYPER);
                break;

            case VINF_EM_DBG_HYPER_ASSERTION:
                RTPrintf("\nVINF_EM_DBG_HYPER_ASSERTION:\n%s%s\n", VMMR3GetRZAssertMsg1(pVM), VMMR3GetRZAssertMsg2(pVM));
                RTLogFlush(NULL);
                rc = DBGFR3EventAssertion(pVM, DBGFEVENT_ASSERTION_HYPER, VMMR3GetRZAssertMsg1(pVM), VMMR3GetRZAssertMsg2(pVM));
                break;

            /*
             * Guru meditation.
             */
            case VERR_VMM_RING0_ASSERTION: /** @todo Make a guru meditation event! */
                rc = DBGFR3EventSrc(pVM, DBGFEVENT_FATAL_ERROR, "VERR_VMM_RING0_ASSERTION", 0, NULL, NULL);
                break;
            case VERR_REM_TOO_MANY_TRAPS: /** @todo Make a guru meditation event! */
                rc = DBGFR3EventSrc(pVM, DBGFEVENT_DEV_STOP, "VERR_REM_TOO_MANY_TRAPS", 0, NULL, NULL);
                break;
            case VINF_EM_TRIPLE_FAULT:    /** @todo Make a guru meditation event! */
                rc = DBGFR3EventSrc(pVM, DBGFEVENT_DEV_STOP, "VINF_EM_TRIPLE_FAULT", 0, NULL, NULL);
                break;

            default: /** @todo don't use default for guru, but make special errors code! */
            {
                LogRel(("emR3Debug: rc=%Rrc\n", VBOXSTRICTRC_VAL(rc)));
                rc = DBGFR3Event(pVM, DBGFEVENT_FATAL_ERROR);
                break;
            }
        }

        /*
         * Process the result.
         */
        switch (VBOXSTRICTRC_VAL(rc))
        {
            /*
             * Continue the debugging loop.
             */
            case VINF_EM_DBG_STEP:
            case VINF_EM_DBG_STOP:
            case VINF_EM_DBG_EVENT:
            case VINF_EM_DBG_STEPPED:
            case VINF_EM_DBG_BREAKPOINT:
            case VINF_EM_DBG_HYPER_STEPPED:
            case VINF_EM_DBG_HYPER_BREAKPOINT:
            case VINF_EM_DBG_HYPER_ASSERTION:
                break;

            /*
             * Resuming execution (in some form) has to be done here if we got
             * a hypervisor debug event.
             */
            case VINF_SUCCESS:
            case VINF_EM_RESUME:
            case VINF_EM_SUSPEND:
            case VINF_EM_RESCHEDULE:
            case VINF_EM_RESCHEDULE_RAW:
            case VINF_EM_RESCHEDULE_REM:
            case VINF_EM_HALT:
                if (pVCpu->em.s.enmState == EMSTATE_DEBUG_HYPER)
                {
#ifdef VBOX_WITH_RAW_MODE
                    rc = emR3RawResumeHyper(pVM, pVCpu);
                    if (rc != VINF_SUCCESS && RT_SUCCESS(rc))
                        continue;
#else
                    AssertLogRelMsgFailedReturn(("Not implemented\n"), VERR_EM_INTERNAL_ERROR);
#endif
                }
                if (rc == VINF_SUCCESS)
                    rc = VINF_EM_RESCHEDULE;
                return rc;

            /*
             * The debugger isn't attached.
             * We'll simply turn the thing off since that's the easiest thing to do.
             */
            case VERR_DBGF_NOT_ATTACHED:
                switch (VBOXSTRICTRC_VAL(rcLast))
                {
                    case VINF_EM_DBG_HYPER_STEPPED:
                    case VINF_EM_DBG_HYPER_BREAKPOINT:
                    case VINF_EM_DBG_HYPER_ASSERTION:
                    case VERR_TRPM_PANIC:
                    case VERR_TRPM_DONT_PANIC:
                    case VERR_VMM_RING0_ASSERTION:
                    case VERR_VMM_HYPER_CR3_MISMATCH:
                    case VERR_VMM_RING3_CALL_DISABLED:
                        return rcLast;
                }
                return VINF_EM_OFF;

            /*
             * Status codes terminating the VM in one or another sense.
             */
            case VINF_EM_TERMINATE:
            case VINF_EM_OFF:
            case VINF_EM_RESET:
            case VINF_EM_NO_MEMORY:
            case VINF_EM_RAW_STALE_SELECTOR:
            case VINF_EM_RAW_IRET_TRAP:
            case VERR_TRPM_PANIC:
            case VERR_TRPM_DONT_PANIC:
            case VERR_IEM_INSTR_NOT_IMPLEMENTED:
            case VERR_IEM_ASPECT_NOT_IMPLEMENTED:
            case VERR_VMM_RING0_ASSERTION:
            case VERR_VMM_HYPER_CR3_MISMATCH:
            case VERR_VMM_RING3_CALL_DISABLED:
            case VERR_INTERNAL_ERROR:
            case VERR_INTERNAL_ERROR_2:
            case VERR_INTERNAL_ERROR_3:
            case VERR_INTERNAL_ERROR_4:
            case VERR_INTERNAL_ERROR_5:
            case VERR_IPE_UNEXPECTED_STATUS:
            case VERR_IPE_UNEXPECTED_INFO_STATUS:
            case VERR_IPE_UNEXPECTED_ERROR_STATUS:
                return rc;

            /*
             * The rest is unexpected, and will keep us here.
             */
            default:
                AssertMsgFailed(("Unexpected rc %Rrc!\n", VBOXSTRICTRC_VAL(rc)));
                break;
        }
    } /* debug for ever */
}


#if defined(VBOX_WITH_REM) || defined(DEBUG)
/**
 * Steps recompiled code.
 *
 * @returns VBox status code. The most important ones are: VINF_EM_STEP_EVENT,
 *          VINF_EM_RESCHEDULE, VINF_EM_SUSPEND, VINF_EM_RESET and VINF_EM_TERMINATE.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
static int emR3RemStep(PVM pVM, PVMCPU pVCpu)
{
    Log3(("emR3RemStep: cs:eip=%04x:%08x\n", CPUMGetGuestCS(pVCpu),  CPUMGetGuestEIP(pVCpu)));

# ifdef VBOX_WITH_REM
    EMRemLock(pVM);

    /*
     * Switch to REM, step instruction, switch back.
     */
    int rc = REMR3State(pVM, pVCpu);
    if (RT_SUCCESS(rc))
    {
        rc = REMR3Step(pVM, pVCpu);
        REMR3StateBack(pVM, pVCpu);
    }
    EMRemUnlock(pVM);

# else
    int rc = VBOXSTRICTRC_TODO(IEMExecOne(pVCpu)); NOREF(pVM);
# endif

    Log3(("emR3RemStep: returns %Rrc cs:eip=%04x:%08x\n", rc, CPUMGetGuestCS(pVCpu),  CPUMGetGuestEIP(pVCpu)));
    return rc;
}
#endif /* VBOX_WITH_REM || DEBUG */


#ifdef VBOX_WITH_REM
/**
 * emR3RemExecute helper that syncs the state back from REM and leave the REM
 * critical section.
 *
 * @returns false - new fInREMState value.
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
DECLINLINE(bool) emR3RemExecuteSyncBack(PVM pVM, PVMCPU pVCpu)
{
    STAM_PROFILE_START(&pVCpu->em.s.StatREMSync, a);
    REMR3StateBack(pVM, pVCpu);
    STAM_PROFILE_STOP(&pVCpu->em.s.StatREMSync, a);

    EMRemUnlock(pVM);
    return false;
}
#endif


/**
 * Executes recompiled code.
 *
 * This function contains the recompiler version of the inner
 * execution loop (the outer loop being in EMR3ExecuteVM()).
 *
 * @returns VBox status code. The most important ones are: VINF_EM_RESCHEDULE,
 *          VINF_EM_SUSPEND, VINF_EM_RESET and VINF_EM_TERMINATE.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pfFFDone    Where to store an indicator telling whether or not
 *                      FFs were done before returning.
 *
 */
static int emR3RemExecute(PVM pVM, PVMCPU pVCpu, bool *pfFFDone)
{
#ifdef LOG_ENABLED
    uint32_t cpl = CPUMGetGuestCPL(pVCpu);

    if (pVCpu->cpum.GstCtx.eflags.Bits.u1VM)
        Log(("EMV86: %04X:%08X IF=%d\n", pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.eip, pVCpu->cpum.GstCtx.eflags.Bits.u1IF));
    else
        Log(("EMR%d: %04X:%08X ESP=%08X IF=%d CR0=%x eflags=%x\n", cpl, pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.eip, pVCpu->cpum.GstCtx.esp, pVCpu->cpum.GstCtx.eflags.Bits.u1IF, (uint32_t)pVCpu->cpum.GstCtx.cr0, pVCpu->cpum.GstCtx.eflags.u));
#endif
    STAM_REL_PROFILE_ADV_START(&pVCpu->em.s.StatREMTotal, a);

#if defined(VBOX_STRICT) && defined(DEBUG_bird)
    AssertMsg(   VMCPU_FF_IS_ANY_SET(pVCpu, VMCPU_FF_PGM_SYNC_CR3 | VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL)
              || !MMHyperIsInsideArea(pVM, CPUMGetGuestEIP(pVCpu)),  /** @todo @bugref{1419} - get flat address. */
              ("cs:eip=%RX16:%RX32\n", CPUMGetGuestCS(pVCpu), CPUMGetGuestEIP(pVCpu)));
#endif

    /*
     * Spin till we get a forced action which returns anything but VINF_SUCCESS
     * or the REM suggests raw-mode execution.
     */
    *pfFFDone = false;
#ifdef VBOX_WITH_REM
    bool    fInREMState = false;
#else
    uint32_t cLoops     = 0;
#endif
    int     rc          = VINF_SUCCESS;
    for (;;)
    {
#ifdef VBOX_WITH_REM
        /*
         * Lock REM and update the state if not already in sync.
         *
         * Note! Big lock, but you are not supposed to own any lock when
         *       coming in here.
         */
        if (!fInREMState)
        {
            EMRemLock(pVM);
            STAM_PROFILE_START(&pVCpu->em.s.StatREMSync, b);

            /* Flush the recompiler translation blocks if the VCPU has changed,
               also force a full CPU state resync. */
            if (pVM->em.s.idLastRemCpu != pVCpu->idCpu)
            {
                REMFlushTBs(pVM);
                CPUMSetChangedFlags(pVCpu, CPUM_CHANGED_ALL);
            }
            pVM->em.s.idLastRemCpu = pVCpu->idCpu;

            rc = REMR3State(pVM, pVCpu);

            STAM_PROFILE_STOP(&pVCpu->em.s.StatREMSync, b);
            if (RT_FAILURE(rc))
                break;
            fInREMState = true;

            /*
             * We might have missed the raising of VMREQ, TIMER and some other
             * important FFs while we were busy switching the state. So, check again.
             */
            if (    VM_FF_IS_PENDING(pVM, VM_FF_REQUEST | VM_FF_PDM_QUEUES | VM_FF_DBGF | VM_FF_CHECK_VM_STATE | VM_FF_RESET)
                ||  VMCPU_FF_IS_ANY_SET(pVCpu, VMCPU_FF_TIMER | VMCPU_FF_REQUEST))
            {
                LogFlow(("emR3RemExecute: Skipping run, because FF is set. %#x\n", pVM->fGlobalForcedActions));
                goto l_REMDoForcedActions;
            }
        }
#endif

        /*
         * Execute REM.
         */
        if (RT_LIKELY(emR3IsExecutionAllowed(pVM, pVCpu)))
        {
            STAM_PROFILE_START(&pVCpu->em.s.StatREMExec, c);
#ifdef VBOX_WITH_REM
            rc = REMR3Run(pVM, pVCpu);
#else
            rc = VBOXSTRICTRC_TODO(IEMExecLots(pVCpu, NULL /*pcInstructions*/));
#endif
            STAM_PROFILE_STOP(&pVCpu->em.s.StatREMExec, c);
        }
        else
        {
            /* Give up this time slice; virtual time continues */
            STAM_REL_PROFILE_ADV_START(&pVCpu->em.s.StatCapped, u);
            RTThreadSleep(5);
            STAM_REL_PROFILE_ADV_STOP(&pVCpu->em.s.StatCapped, u);
            rc = VINF_SUCCESS;
        }

        /*
         * Deal with high priority post execution FFs before doing anything
         * else.  Sync back the state and leave the lock to be on the safe side.
         */
        if (    VM_FF_IS_PENDING(pVM, VM_FF_HIGH_PRIORITY_POST_MASK)
            ||  VMCPU_FF_IS_ANY_SET(pVCpu, VMCPU_FF_HIGH_PRIORITY_POST_MASK))
        {
#ifdef VBOX_WITH_REM
            fInREMState = emR3RemExecuteSyncBack(pVM, pVCpu);
#endif
            rc = VBOXSTRICTRC_TODO(emR3HighPriorityPostForcedActions(pVM, pVCpu, rc));
        }

        /*
         * Process the returned status code.
         */
        if (rc != VINF_SUCCESS)
        {
            if (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST)
                break;
            if (rc != VINF_REM_INTERRUPED_FF)
            {
#ifndef VBOX_WITH_REM
                /* Try dodge unimplemented IEM trouble by reschduling. */
                if (   rc == VERR_IEM_ASPECT_NOT_IMPLEMENTED
                    || rc == VERR_IEM_INSTR_NOT_IMPLEMENTED)
                {
                    EMSTATE enmNewState = emR3Reschedule(pVM, pVCpu);
                    if (enmNewState != EMSTATE_REM && enmNewState != EMSTATE_IEM_THEN_REM)
                    {
                        rc = VINF_EM_RESCHEDULE;
                        break;
                    }
                }
#endif

                /*
                 * Anything which is not known to us means an internal error
                 * and the termination of the VM!
                 */
                AssertMsg(rc == VERR_REM_TOO_MANY_TRAPS, ("Unknown GC return code: %Rra\n", rc));
                break;
            }
        }


        /*
         * Check and execute forced actions.
         *
         * Sync back the VM state and leave the lock  before calling any of
         * these, you never know what's going to happen here.
         */
#ifdef VBOX_HIGH_RES_TIMERS_HACK
        TMTimerPollVoid(pVM, pVCpu);
#endif
        AssertCompile(VMCPU_FF_ALL_REM_MASK & VMCPU_FF_TIMER);
        if (    VM_FF_IS_PENDING(pVM, VM_FF_ALL_REM_MASK)
            ||  VMCPU_FF_IS_ANY_SET(pVCpu,
                                     VMCPU_FF_ALL_REM_MASK
                                   & VM_WHEN_RAW_MODE(~(VMCPU_FF_CSAM_PENDING_ACTION | VMCPU_FF_CSAM_SCAN_PAGE), UINT32_MAX)) )
        {
#ifdef VBOX_WITH_REM
l_REMDoForcedActions:
            if (fInREMState)
                fInREMState = emR3RemExecuteSyncBack(pVM, pVCpu);
#endif
            STAM_REL_PROFILE_ADV_SUSPEND(&pVCpu->em.s.StatREMTotal, a);
            rc = emR3ForcedActions(pVM, pVCpu, rc);
            VBOXVMM_EM_FF_ALL_RET(pVCpu, rc);
            STAM_REL_PROFILE_ADV_RESUME(&pVCpu->em.s.StatREMTotal, a);
            if (    rc != VINF_SUCCESS
                &&  rc != VINF_EM_RESCHEDULE_REM)
            {
                *pfFFDone = true;
                break;
            }
        }

#ifndef VBOX_WITH_REM
        /*
         * Have to check if we can get back to fast execution mode every so often.
         */
        if (!(++cLoops & 7))
        {
            EMSTATE enmCheck = emR3Reschedule(pVM, pVCpu);
            if (   enmCheck != EMSTATE_REM
                && enmCheck != EMSTATE_IEM_THEN_REM)
                return VINF_EM_RESCHEDULE;
        }
#endif

    } /* The Inner Loop, recompiled execution mode version. */


#ifdef VBOX_WITH_REM
    /*
     * Returning. Sync back the VM state if required.
     */
    if (fInREMState)
        fInREMState = emR3RemExecuteSyncBack(pVM, pVCpu);
#endif

    STAM_REL_PROFILE_ADV_STOP(&pVCpu->em.s.StatREMTotal, a);
    return rc;
}


#ifdef DEBUG

int emR3SingleStepExecRem(PVM pVM, PVMCPU pVCpu, uint32_t cIterations)
{
    EMSTATE  enmOldState = pVCpu->em.s.enmState;

    pVCpu->em.s.enmState = EMSTATE_DEBUG_GUEST_REM;

    Log(("Single step BEGIN:\n"));
    for (uint32_t i = 0; i < cIterations; i++)
    {
        DBGFR3PrgStep(pVCpu);
        DBGFR3_DISAS_INSTR_CUR_LOG(pVCpu, "RSS");
        emR3RemStep(pVM, pVCpu);
        if (emR3Reschedule(pVM, pVCpu) != EMSTATE_REM)
            break;
    }
    Log(("Single step END:\n"));
    CPUMSetGuestEFlags(pVCpu, CPUMGetGuestEFlags(pVCpu) & ~X86_EFL_TF);
    pVCpu->em.s.enmState = enmOldState;
    return VINF_EM_RESCHEDULE;
}

#endif /* DEBUG */


/**
 * Try execute the problematic code in IEM first, then fall back on REM if there
 * is too much of it or if IEM doesn't implement something.
 *
 * @returns Strict VBox status code from IEMExecLots.
 * @param   pVM        The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   pfFFDone    Force flags done indicator.
 *
 * @thread  EMT(pVCpu)
 */
static VBOXSTRICTRC emR3ExecuteIemThenRem(PVM pVM, PVMCPU pVCpu, bool *pfFFDone)
{
    LogFlow(("emR3ExecuteIemThenRem: %04x:%RGv\n", CPUMGetGuestCS(pVCpu), CPUMGetGuestRIP(pVCpu)));
    *pfFFDone = false;

    /*
     * Execute in IEM for a while.
     */
    while (pVCpu->em.s.cIemThenRemInstructions < 1024)
    {
        uint32_t     cInstructions;
        VBOXSTRICTRC rcStrict = IEMExecLots(pVCpu, &cInstructions);
        pVCpu->em.s.cIemThenRemInstructions += cInstructions;
        if (rcStrict != VINF_SUCCESS)
        {
            if (   rcStrict == VERR_IEM_ASPECT_NOT_IMPLEMENTED
                || rcStrict == VERR_IEM_INSTR_NOT_IMPLEMENTED)
                break;

            Log(("emR3ExecuteIemThenRem: returns %Rrc after %u instructions\n",
                 VBOXSTRICTRC_VAL(rcStrict), pVCpu->em.s.cIemThenRemInstructions));
            return rcStrict;
        }

        EMSTATE enmNewState = emR3Reschedule(pVM, pVCpu);
        if (enmNewState != EMSTATE_REM && enmNewState != EMSTATE_IEM_THEN_REM)
        {
            LogFlow(("emR3ExecuteIemThenRem: -> %d (%s) after %u instructions\n",
                     enmNewState, emR3GetStateName(enmNewState), pVCpu->em.s.cIemThenRemInstructions));
            pVCpu->em.s.enmPrevState = pVCpu->em.s.enmState;
            pVCpu->em.s.enmState     = enmNewState;
            return VINF_SUCCESS;
        }

        /*
         * Check for pending actions.
         */
        if (   VM_FF_IS_PENDING(pVM, VM_FF_ALL_REM_MASK)
            || VMCPU_FF_IS_ANY_SET(pVCpu, VMCPU_FF_ALL_REM_MASK & ~VMCPU_FF_UNHALT))
            return VINF_SUCCESS;
    }

    /*
     * Switch to REM.
     */
    Log(("emR3ExecuteIemThenRem: -> EMSTATE_REM (after %u instructions)\n", pVCpu->em.s.cIemThenRemInstructions));
    pVCpu->em.s.enmState = EMSTATE_REM;
    return VINF_SUCCESS;
}


/**
 * Decides whether to execute RAW, HWACC or REM.
 *
 * @returns new EM state
 * @param   pVM     The cross context VM structure.
 * @param   pVCpu   The cross context virtual CPU structure.
 */
EMSTATE emR3Reschedule(PVM pVM, PVMCPU pVCpu)
{
    /*
     * When forcing raw-mode execution, things are simple.
     */
    if (pVCpu->em.s.fForceRAW)
        return EMSTATE_RAW;

    /*
     * We stay in the wait for SIPI state unless explicitly told otherwise.
     */
    if (pVCpu->em.s.enmState == EMSTATE_WAIT_SIPI)
        return EMSTATE_WAIT_SIPI;

    /*
     * Execute everything in IEM?
     */
    if (pVM->em.s.fIemExecutesAll)
        return EMSTATE_IEM;

    /* !!! THIS MUST BE IN SYNC WITH remR3CanExecuteRaw !!! */
    /* !!! THIS MUST BE IN SYNC WITH remR3CanExecuteRaw !!! */
    /* !!! THIS MUST BE IN SYNC WITH remR3CanExecuteRaw !!! */

    X86EFLAGS EFlags = pVCpu->cpum.GstCtx.eflags;
    if (!VM_IS_RAW_MODE_ENABLED(pVM))
    {
        if (EMIsHwVirtExecutionEnabled(pVM))
        {
            if (VM_IS_HM_ENABLED(pVM))
            {
                if (HMCanExecuteGuest(pVCpu, &pVCpu->cpum.GstCtx))
                    return EMSTATE_HM;
            }
            else if (NEMR3CanExecuteGuest(pVM, pVCpu))
                return EMSTATE_NEM;

            /*
             * Note! Raw mode and hw accelerated mode are incompatible. The latter
             *       turns off monitoring features essential for raw mode!
             */
            return EMSTATE_IEM_THEN_REM;
        }
    }

    /*
     * Standard raw-mode:
     *
     * Here we only support 16 & 32 bits protected mode ring 3 code that has no IO privileges
     * or 32 bits protected mode ring 0 code
     *
     * The tests are ordered by the likelihood of being true during normal execution.
     */
    if (EFlags.u32 & (X86_EFL_TF /* | HF_INHIBIT_IRQ_MASK*/))
    {
        Log2(("raw mode refused: EFlags=%#x\n", EFlags.u32));
        return EMSTATE_REM;
    }

# ifndef VBOX_RAW_V86
    if (EFlags.u32 & X86_EFL_VM) {
        Log2(("raw mode refused: VM_MASK\n"));
        return EMSTATE_REM;
    }
# endif

    /** @todo check up the X86_CR0_AM flag in respect to raw mode!!! We're probably not emulating it right! */
    uint32_t u32CR0 = pVCpu->cpum.GstCtx.cr0;
    if ((u32CR0 & (X86_CR0_PG | X86_CR0_PE)) != (X86_CR0_PG | X86_CR0_PE))
    {
        //Log2(("raw mode refused: %s%s%s\n", (u32CR0 & X86_CR0_PG) ? "" : " !PG", (u32CR0 & X86_CR0_PE) ? "" : " !PE", (u32CR0 & X86_CR0_AM) ? "" : " !AM"));
        return EMSTATE_REM;
    }

    if (pVCpu->cpum.GstCtx.cr4 & X86_CR4_PAE)
    {
        uint32_t u32Dummy, u32Features;

        CPUMGetGuestCpuId(pVCpu, 1, 0, &u32Dummy, &u32Dummy, &u32Dummy, &u32Features);
        if (!(u32Features & X86_CPUID_FEATURE_EDX_PAE))
            return EMSTATE_REM;
    }

    unsigned uSS = pVCpu->cpum.GstCtx.ss.Sel;
    if (    pVCpu->cpum.GstCtx.eflags.Bits.u1VM
        ||  (uSS & X86_SEL_RPL) == 3)
    {
        if (!EMIsRawRing3Enabled(pVM))
            return EMSTATE_REM;

        if (!(EFlags.u32 & X86_EFL_IF))
        {
            Log2(("raw mode refused: IF (RawR3)\n"));
            return EMSTATE_REM;
        }

        if (!(u32CR0 & X86_CR0_WP) && EMIsRawRing0Enabled(pVM))
        {
            Log2(("raw mode refused: CR0.WP + RawR0\n"));
            return EMSTATE_REM;
        }
    }
    else
    {
        if (!EMIsRawRing0Enabled(pVM))
            return EMSTATE_REM;

        if (EMIsRawRing1Enabled(pVM))
        {
            /* Only ring 0 and 1 supervisor code. */
            if ((uSS & X86_SEL_RPL) == 2)   /* ring 1 code is moved into ring 2, so we can't support ring-2 in that case. */
            {
                Log2(("raw r0 mode refused: CPL %d\n", uSS & X86_SEL_RPL));
                return EMSTATE_REM;
            }
        }
        /* Only ring 0 supervisor code. */
        else if ((uSS & X86_SEL_RPL) != 0)
        {
            Log2(("raw r0 mode refused: CPL %d\n", uSS & X86_SEL_RPL));
            return EMSTATE_REM;
        }

        // Let's start with pure 32 bits ring 0 code first
        /** @todo What's pure 32-bit mode? flat? */
        if (    !(pVCpu->cpum.GstCtx.ss.Attr.n.u1DefBig)
            ||  !(pVCpu->cpum.GstCtx.cs.Attr.n.u1DefBig))
        {
            Log2(("raw r0 mode refused: SS/CS not 32bit\n"));
            return EMSTATE_REM;
        }

        /* Write protection must be turned on, or else the guest can overwrite our hypervisor code and data. */
        if (!(u32CR0 & X86_CR0_WP))
        {
            Log2(("raw r0 mode refused: CR0.WP=0!\n"));
            return EMSTATE_REM;
        }

# ifdef VBOX_WITH_RAW_MODE
        if (PATMShouldUseRawMode(pVM, (RTGCPTR)pVCpu->cpum.GstCtx.eip))
        {
            Log2(("raw r0 mode forced: patch code\n"));
#  ifdef VBOX_WITH_SAFE_STR
            Assert(pVCpu->cpum.GstCtx.tr.Sel);
#  endif
            return EMSTATE_RAW;
        }
# endif /* VBOX_WITH_RAW_MODE */

# if !defined(VBOX_ALLOW_IF0) && !defined(VBOX_RUN_INTERRUPT_GATE_HANDLERS)
        if (!(EFlags.u32 & X86_EFL_IF))
        {
            ////Log2(("R0: IF=0 VIF=%d %08X\n", eip, pVMeflags));
            //Log2(("RR0: Interrupts turned off; fall back to emulation\n"));
            return EMSTATE_REM;
        }
# endif

# ifndef VBOX_WITH_RAW_RING1
        /** @todo still necessary??? */
        if (EFlags.Bits.u2IOPL != 0)
        {
            Log2(("raw r0 mode refused: IOPL %d\n", EFlags.Bits.u2IOPL));
            return EMSTATE_REM;
        }
# endif
    }

    /*
     * Stale hidden selectors means raw-mode is unsafe (being very careful).
     */
    if (pVCpu->cpum.GstCtx.cs.fFlags & CPUMSELREG_FLAGS_STALE)
    {
        Log2(("raw mode refused: stale CS\n"));
        return EMSTATE_REM;
    }
    if (pVCpu->cpum.GstCtx.ss.fFlags & CPUMSELREG_FLAGS_STALE)
    {
        Log2(("raw mode refused: stale SS\n"));
        return EMSTATE_REM;
    }
    if (pVCpu->cpum.GstCtx.ds.fFlags & CPUMSELREG_FLAGS_STALE)
    {
        Log2(("raw mode refused: stale DS\n"));
        return EMSTATE_REM;
    }
    if (pVCpu->cpum.GstCtx.es.fFlags & CPUMSELREG_FLAGS_STALE)
    {
        Log2(("raw mode refused: stale ES\n"));
        return EMSTATE_REM;
    }
    if (pVCpu->cpum.GstCtx.fs.fFlags & CPUMSELREG_FLAGS_STALE)
    {
        Log2(("raw mode refused: stale FS\n"));
        return EMSTATE_REM;
    }
    if (pVCpu->cpum.GstCtx.gs.fFlags & CPUMSELREG_FLAGS_STALE)
    {
        Log2(("raw mode refused: stale GS\n"));
        return EMSTATE_REM;
    }

# ifdef VBOX_WITH_SAFE_STR
    if (pVCpu->cpum.GstCtx.tr.Sel == 0)
    {
        Log(("Raw mode refused -> TR=0\n"));
        return EMSTATE_REM;
    }
# endif

    /*Assert(PGMPhysIsA20Enabled(pVCpu));*/
    return EMSTATE_RAW;
}


/**
 * Executes all high priority post execution force actions.
 *
 * @returns Strict VBox status code.  Typically @a rc, but may be upgraded to
 *          fatal error status code.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   rc          The current strict VBox status code rc.
 */
VBOXSTRICTRC emR3HighPriorityPostForcedActions(PVM pVM, PVMCPU pVCpu, VBOXSTRICTRC rc)
{
    VBOXVMM_EM_FF_HIGH(pVCpu, pVM->fGlobalForcedActions, pVCpu->fLocalForcedActions, VBOXSTRICTRC_VAL(rc));

    if (VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_PDM_CRITSECT))
        PDMCritSectBothFF(pVCpu);

    /* Update CR3 (Nested Paging case for HM). */
    if (VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_HM_UPDATE_CR3))
    {
        CPUM_IMPORT_EXTRN_RCSTRICT(pVCpu, CPUMCTX_EXTRN_CR0 | CPUMCTX_EXTRN_CR3 | CPUMCTX_EXTRN_CR4 | CPUMCTX_EXTRN_EFER, rc);
        int rc2 = PGMUpdateCR3(pVCpu, CPUMGetGuestCR3(pVCpu));
        if (RT_FAILURE(rc2))
            return rc2;
        Assert(!VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_HM_UPDATE_CR3));
    }

    /* Update PAE PDPEs. This must be done *after* PGMUpdateCR3() and used only by the Nested Paging case for HM. */
    if (VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_HM_UPDATE_PAE_PDPES))
    {
        CPUM_IMPORT_EXTRN_RCSTRICT(pVCpu, CPUMCTX_EXTRN_CR0 | CPUMCTX_EXTRN_CR3 | CPUMCTX_EXTRN_CR4 | CPUMCTX_EXTRN_EFER, rc);
        if (CPUMIsGuestInPAEMode(pVCpu))
        {
            PX86PDPE pPdpes = HMGetPaePdpes(pVCpu);
            AssertPtr(pPdpes);

            PGMGstUpdatePaePdpes(pVCpu, pPdpes);
            Assert(!VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_HM_UPDATE_PAE_PDPES));
        }
        else
            VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_HM_UPDATE_PAE_PDPES);
    }

    /* IEM has pending work (typically memory write after INS instruction). */
    if (VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_IEM))
        rc = IEMR3ProcessForceFlag(pVM, pVCpu, rc);

    /* IOM has pending work (comitting an I/O or MMIO write). */
    if (VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_IOM))
    {
        rc = IOMR3ProcessForceFlag(pVM, pVCpu, rc);
        if (pVCpu->em.s.idxContinueExitRec >= RT_ELEMENTS(pVCpu->em.s.aExitRecords))
        { /* half likely, or at least it's a line shorter. */ }
        else if (rc == VINF_SUCCESS)
            rc = VINF_EM_RESUME_R3_HISTORY_EXEC;
        else
            pVCpu->em.s.idxContinueExitRec = UINT16_MAX;
    }

#ifdef VBOX_WITH_RAW_MODE
    if (VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_CSAM_PENDING_ACTION))
        CSAMR3DoPendingAction(pVM, pVCpu);
#endif

    if (VM_FF_IS_SET(pVM, VM_FF_PGM_NO_MEMORY))
    {
        if (    rc > VINF_EM_NO_MEMORY
            &&  rc <= VINF_EM_LAST)
            rc = VINF_EM_NO_MEMORY;
    }

    return rc;
}

#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
/**
 * Helper for emR3ForcedActions() for injecting interrupts into the
 * nested-guest.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pfResched   Where to store whether a reschedule is required.
 * @param   pfInject    Where to store whether an interrupt was injected (and if
 *                      a wake up is pending).
 */
static int emR3NstGstInjectIntr(PVMCPU pVCpu, bool *pfResched, bool *pfInject)
{
    *pfResched = false;
    *pfInject  = false;
    if (CPUMIsGuestInSvmNestedHwVirtMode(&pVCpu->cpum.GstCtx))
    {
        PVM pVM  = pVCpu->CTX_SUFF(pVM);
        Assert(pVCpu->cpum.GstCtx.hwvirt.fGif);
        bool fVirtualGif = CPUMGetSvmNstGstVGif(&pVCpu->cpum.GstCtx);
#ifdef VBOX_WITH_RAW_MODE
        fVirtualGif     &= !PATMIsPatchGCAddr(pVM, pVCpu->cpum.GstCtx.eip);
#endif
        if (fVirtualGif)
        {
            if (CPUMCanSvmNstGstTakePhysIntr(pVCpu, &pVCpu->cpum.GstCtx))
            {
                Assert(pVCpu->em.s.enmState != EMSTATE_WAIT_SIPI);
                if (VMCPU_FF_IS_ANY_SET(pVCpu, VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC))
                {
                    if (CPUMIsGuestSvmCtrlInterceptSet(pVCpu, &pVCpu->cpum.GstCtx, SVM_CTRL_INTERCEPT_INTR))
                    {
                        CPUM_IMPORT_EXTRN_RET(pVCpu, IEM_CPUMCTX_EXTRN_SVM_VMEXIT_MASK);
                        VBOXSTRICTRC rcStrict = IEMExecSvmVmexit(pVCpu, SVM_EXIT_INTR, 0, 0);
                        if (RT_SUCCESS(rcStrict))
                        {
                            /** @todo r=ramshankar: Do we need to signal a wakeup here? If a nested-guest
                             *        doesn't intercept HLT but intercepts INTR? */
                            *pfResched = true;
                            Assert(rcStrict != VINF_PGM_CHANGE_MODE);
                            if (rcStrict == VINF_SVM_VMEXIT)
                                return VINF_SUCCESS;
                            return VBOXSTRICTRC_VAL(rcStrict);
                        }

                        AssertMsgFailed(("INTR #VMEXIT failed! rc=%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
                        return VINF_EM_TRIPLE_FAULT;
                    }

                    /* Note: it's important to make sure the return code from TRPMR3InjectEvent isn't ignored! */
                    /** @todo this really isn't nice, should properly handle this */
                    CPUM_IMPORT_EXTRN_RET(pVCpu, IEM_CPUMCTX_EXTRN_XCPT_MASK);
                    int rc = TRPMR3InjectEvent(pVM, pVCpu, TRPM_HARDWARE_INT);
                    Assert(rc != VINF_PGM_CHANGE_MODE);
                    if (rc == VINF_SVM_VMEXIT)
                        rc = VINF_SUCCESS;
                    if (pVM->em.s.fIemExecutesAll && (   rc == VINF_EM_RESCHEDULE_REM
                                                      || rc == VINF_EM_RESCHEDULE_HM
                                                      || rc == VINF_EM_RESCHEDULE_RAW))
                    {
                        rc = VINF_EM_RESCHEDULE;
                    }

                    *pfResched = true;
                    *pfInject  = true;
                    return rc;
                }
            }

            if (   VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INTERRUPT_NESTED_GUEST)
                && CPUMCanSvmNstGstTakeVirtIntr(pVCpu, &pVCpu->cpum.GstCtx))
            {
                if (CPUMIsGuestSvmCtrlInterceptSet(pVCpu, &pVCpu->cpum.GstCtx, SVM_CTRL_INTERCEPT_VINTR))
                {
                    CPUM_IMPORT_EXTRN_RET(pVCpu, IEM_CPUMCTX_EXTRN_SVM_VMEXIT_MASK);
                    VBOXSTRICTRC rcStrict = IEMExecSvmVmexit(pVCpu, SVM_EXIT_VINTR, 0, 0);
                    if (RT_SUCCESS(rcStrict))
                    {
                        /** @todo r=ramshankar: Do we need to signal a wakeup here? If a nested-guest
                         *        doesn't intercept HLT but intercepts VINTR? */
                        *pfResched = true;
                        Assert(rcStrict != VINF_PGM_CHANGE_MODE);
                        if (rcStrict == VINF_SVM_VMEXIT)
                            return VINF_SUCCESS;
                        return VBOXSTRICTRC_VAL(rcStrict);
                    }

                    AssertMsgFailed(("VINTR #VMEXIT failed! rc=%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
                    return VINF_EM_TRIPLE_FAULT;
                }

                VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_NESTED_GUEST);
                uint8_t const uNstGstVector = CPUMGetSvmNstGstInterrupt(&pVCpu->cpum.GstCtx);
                AssertMsg(uNstGstVector > 0 && uNstGstVector <= X86_XCPT_LAST, ("Invalid VINTR vector %#x\n", uNstGstVector));
                TRPMAssertTrap(pVCpu, uNstGstVector, TRPM_HARDWARE_INT);
                Log(("EM: Asserting nested-guest virt. hardware intr: %#x\n", uNstGstVector));

                *pfResched = true;
                *pfInject  = true;
                return VINF_EM_RESCHEDULE;
            }
        }
        return VINF_SUCCESS;
    }

    if (CPUMIsGuestInVmxNonRootMode(&pVCpu->cpum.GstCtx))
    { /** @todo Nested VMX. */ }

    /* Shouldn't really get here. */
    AssertMsgFailed(("Unrecognized nested hwvirt. arch!\n"));
    return VERR_EM_INTERNAL_ERROR;
}
#endif

/**
 * Executes all pending forced actions.
 *
 * Forced actions can cause execution delays and execution
 * rescheduling. The first we deal with using action priority, so
 * that for instance pending timers aren't scheduled and ran until
 * right before execution. The rescheduling we deal with using
 * return codes. The same goes for VM termination, only in that case
 * we exit everything.
 *
 * @returns VBox status code of equal or greater importance/severity than rc.
 *          The most important ones are: VINF_EM_RESCHEDULE,
 *          VINF_EM_SUSPEND, VINF_EM_RESET and VINF_EM_TERMINATE.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   rc          The current rc.
 *
 */
int emR3ForcedActions(PVM pVM, PVMCPU pVCpu, int rc)
{
    STAM_REL_PROFILE_START(&pVCpu->em.s.StatForcedActions, a);
#ifdef VBOX_STRICT
    int rcIrq = VINF_SUCCESS;
#endif
    int rc2;
#define UPDATE_RC() \
        do { \
            AssertMsg(rc2 <= 0 || (rc2 >= VINF_EM_FIRST && rc2 <= VINF_EM_LAST), ("Invalid FF return code: %Rra\n", rc2)); \
            if (rc2 == VINF_SUCCESS || rc < VINF_SUCCESS) \
                break; \
            if (!rc || rc2 < rc) \
                rc = rc2; \
        } while (0)
    VBOXVMM_EM_FF_ALL(pVCpu, pVM->fGlobalForcedActions, pVCpu->fLocalForcedActions, rc);

    /*
     * Post execution chunk first.
     */
    if (    VM_FF_IS_PENDING(pVM, VM_FF_NORMAL_PRIORITY_POST_MASK)
        ||  (VMCPU_FF_NORMAL_PRIORITY_POST_MASK && VMCPU_FF_IS_ANY_SET(pVCpu, VMCPU_FF_NORMAL_PRIORITY_POST_MASK)) )
    {
        /*
         * EMT Rendezvous (must be serviced before termination).
         */
        if (VM_FF_IS_SET(pVM, VM_FF_EMT_RENDEZVOUS))
        {
            CPUM_IMPORT_EXTRN_RCSTRICT(pVCpu, ~CPUMCTX_EXTRN_KEEPER_MASK, rc);
            rc2 = VMMR3EmtRendezvousFF(pVM, pVCpu);
            UPDATE_RC();
            /** @todo HACK ALERT! The following test is to make sure EM+TM
             * thinks the VM is stopped/reset before the next VM state change
             * is made. We need a better solution for this, or at least make it
             * possible to do: (rc >= VINF_EM_FIRST && rc <=
             * VINF_EM_SUSPEND). */
            if (RT_UNLIKELY(rc == VINF_EM_SUSPEND || rc == VINF_EM_RESET || rc == VINF_EM_OFF))
            {
                Log2(("emR3ForcedActions: returns %Rrc\n", rc));
                STAM_REL_PROFILE_STOP(&pVCpu->em.s.StatForcedActions, a);
                return rc;
            }
        }

        /*
         * State change request (cleared by vmR3SetStateLocked).
         */
        if (VM_FF_IS_SET(pVM, VM_FF_CHECK_VM_STATE))
        {
            VMSTATE enmState = VMR3GetState(pVM);
            switch (enmState)
            {
                case VMSTATE_FATAL_ERROR:
                case VMSTATE_FATAL_ERROR_LS:
                case VMSTATE_GURU_MEDITATION:
                case VMSTATE_GURU_MEDITATION_LS:
                    Log2(("emR3ForcedActions: %s -> VINF_EM_SUSPEND\n", VMGetStateName(enmState) ));
                    STAM_REL_PROFILE_STOP(&pVCpu->em.s.StatForcedActions, a);
                    return VINF_EM_SUSPEND;

                case VMSTATE_DESTROYING:
                    Log2(("emR3ForcedActions: %s -> VINF_EM_TERMINATE\n", VMGetStateName(enmState) ));
                    STAM_REL_PROFILE_STOP(&pVCpu->em.s.StatForcedActions, a);
                    return VINF_EM_TERMINATE;

                default:
                    AssertMsgFailed(("%s\n", VMGetStateName(enmState)));
            }
        }

        /*
         * Debugger Facility polling.
         */
        if (   VM_FF_IS_SET(pVM, VM_FF_DBGF)
            || VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_DBGF) )
        {
            CPUM_IMPORT_EXTRN_RCSTRICT(pVCpu, ~CPUMCTX_EXTRN_KEEPER_MASK, rc);
            rc2 = DBGFR3VMMForcedAction(pVM, pVCpu);
            UPDATE_RC();
        }

        /*
         * Postponed reset request.
         */
        if (VM_FF_TEST_AND_CLEAR(pVM, VM_FF_RESET))
        {
            CPUM_IMPORT_EXTRN_RCSTRICT(pVCpu, ~CPUMCTX_EXTRN_KEEPER_MASK, rc);
            rc2 = VBOXSTRICTRC_TODO(VMR3ResetFF(pVM));
            UPDATE_RC();
        }

#ifdef VBOX_WITH_RAW_MODE
        /*
         * CSAM page scanning.
         */
        if (    !VM_FF_IS_SET(pVM, VM_FF_PGM_NO_MEMORY)
            &&  VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_CSAM_SCAN_PAGE))
        {
            /** @todo check for 16 or 32 bits code! (D bit in the code selector) */
            Log(("Forced action VMCPU_FF_CSAM_SCAN_PAGE\n"));
            CPUM_IMPORT_EXTRN_RCSTRICT(pVCpu, ~CPUMCTX_EXTRN_KEEPER_MASK, rc);
            CSAMR3CheckCodeEx(pVM, &pVCpu->cpum.GstCtx, pVCpu->cpum.GstCtx.eip);
            VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_CSAM_SCAN_PAGE);
        }
#endif

        /*
         * Out of memory? Putting this after CSAM as it may in theory cause us to run out of memory.
         */
        if (VM_FF_IS_SET(pVM, VM_FF_PGM_NO_MEMORY))
        {
            rc2 = PGMR3PhysAllocateHandyPages(pVM);
            UPDATE_RC();
            if (rc == VINF_EM_NO_MEMORY)
                return rc;
        }

        /* check that we got them all  */
        AssertCompile(VM_FF_NORMAL_PRIORITY_POST_MASK == (VM_FF_CHECK_VM_STATE | VM_FF_DBGF | VM_FF_RESET | VM_FF_PGM_NO_MEMORY | VM_FF_EMT_RENDEZVOUS));
        AssertCompile(VMCPU_FF_NORMAL_PRIORITY_POST_MASK == (VM_WHEN_RAW_MODE(VMCPU_FF_CSAM_SCAN_PAGE, 0) | VMCPU_FF_DBGF));
    }

    /*
     * Normal priority then.
     * (Executed in no particular order.)
     */
    if (VM_FF_IS_PENDING_EXCEPT(pVM, VM_FF_NORMAL_PRIORITY_MASK, VM_FF_PGM_NO_MEMORY))
    {
        /*
         * PDM Queues are pending.
         */
        if (VM_FF_IS_PENDING_EXCEPT(pVM, VM_FF_PDM_QUEUES, VM_FF_PGM_NO_MEMORY))
            PDMR3QueueFlushAll(pVM);

        /*
         * PDM DMA transfers are pending.
         */
        if (VM_FF_IS_PENDING_EXCEPT(pVM, VM_FF_PDM_DMA, VM_FF_PGM_NO_MEMORY))
            PDMR3DmaRun(pVM);

        /*
         * EMT Rendezvous (make sure they are handled before the requests).
         */
        if (VM_FF_IS_SET(pVM, VM_FF_EMT_RENDEZVOUS))
        {
            CPUM_IMPORT_EXTRN_RCSTRICT(pVCpu, ~CPUMCTX_EXTRN_KEEPER_MASK, rc);
            rc2 = VMMR3EmtRendezvousFF(pVM, pVCpu);
            UPDATE_RC();
            /** @todo HACK ALERT! The following test is to make sure EM+TM
             * thinks the VM is stopped/reset before the next VM state change
             * is made. We need a better solution for this, or at least make it
             * possible to do: (rc >= VINF_EM_FIRST && rc <=
             * VINF_EM_SUSPEND). */
            if (RT_UNLIKELY(rc == VINF_EM_SUSPEND || rc == VINF_EM_RESET || rc == VINF_EM_OFF))
            {
                Log2(("emR3ForcedActions: returns %Rrc\n", rc));
                STAM_REL_PROFILE_STOP(&pVCpu->em.s.StatForcedActions, a);
                return rc;
            }
        }

        /*
         * Requests from other threads.
         */
        if (VM_FF_IS_PENDING_EXCEPT(pVM, VM_FF_REQUEST, VM_FF_PGM_NO_MEMORY))
        {
            CPUM_IMPORT_EXTRN_RCSTRICT(pVCpu, ~CPUMCTX_EXTRN_KEEPER_MASK, rc);
            rc2 = VMR3ReqProcessU(pVM->pUVM, VMCPUID_ANY, false /*fPriorityOnly*/);
            if (rc2 == VINF_EM_OFF || rc2 == VINF_EM_TERMINATE) /** @todo this shouldn't be necessary */
            {
                Log2(("emR3ForcedActions: returns %Rrc\n", rc2));
                STAM_REL_PROFILE_STOP(&pVCpu->em.s.StatForcedActions, a);
                return rc2;
            }
            UPDATE_RC();
            /** @todo HACK ALERT! The following test is to make sure EM+TM
             * thinks the VM is stopped/reset before the next VM state change
             * is made. We need a better solution for this, or at least make it
             * possible to do: (rc >= VINF_EM_FIRST && rc <=
             * VINF_EM_SUSPEND). */
            if (RT_UNLIKELY(rc == VINF_EM_SUSPEND || rc == VINF_EM_RESET || rc == VINF_EM_OFF))
            {
                Log2(("emR3ForcedActions: returns %Rrc\n", rc));
                STAM_REL_PROFILE_STOP(&pVCpu->em.s.StatForcedActions, a);
                return rc;
            }
        }

#ifdef VBOX_WITH_REM
        /* Replay the handler notification changes. */
        if (VM_FF_IS_PENDING_EXCEPT(pVM, VM_FF_REM_HANDLER_NOTIFY, VM_FF_PGM_NO_MEMORY))
        {
            /* Try not to cause deadlocks. */
            if (    pVM->cCpus == 1
                ||  (   !PGMIsLockOwner(pVM)
                     && !IOMIsLockWriteOwner(pVM))
               )
            {
                EMRemLock(pVM);
                REMR3ReplayHandlerNotifications(pVM);
                EMRemUnlock(pVM);
            }
        }
#endif

        /* check that we got them all  */
        AssertCompile(VM_FF_NORMAL_PRIORITY_MASK == (VM_FF_REQUEST | VM_FF_PDM_QUEUES | VM_FF_PDM_DMA | VM_FF_REM_HANDLER_NOTIFY | VM_FF_EMT_RENDEZVOUS));
    }

    /*
     * Normal priority then. (per-VCPU)
     * (Executed in no particular order.)
     */
    if (    !VM_FF_IS_SET(pVM, VM_FF_PGM_NO_MEMORY)
        &&  VMCPU_FF_IS_ANY_SET(pVCpu, VMCPU_FF_NORMAL_PRIORITY_MASK))
    {
        /*
         * Requests from other threads.
         */
        if (VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_REQUEST))
        {
            CPUM_IMPORT_EXTRN_RCSTRICT(pVCpu, ~CPUMCTX_EXTRN_KEEPER_MASK, rc);
            rc2 = VMR3ReqProcessU(pVM->pUVM, pVCpu->idCpu, false /*fPriorityOnly*/);
            if (rc2 == VINF_EM_OFF || rc2 == VINF_EM_TERMINATE || rc2 == VINF_EM_RESET)
            {
                Log2(("emR3ForcedActions: returns %Rrc\n", rc2));
                STAM_REL_PROFILE_STOP(&pVCpu->em.s.StatForcedActions, a);
                return rc2;
            }
            UPDATE_RC();
            /** @todo HACK ALERT! The following test is to make sure EM+TM
             * thinks the VM is stopped/reset before the next VM state change
             * is made. We need a better solution for this, or at least make it
             * possible to do: (rc >= VINF_EM_FIRST && rc <=
             * VINF_EM_SUSPEND). */
            if (RT_UNLIKELY(rc == VINF_EM_SUSPEND || rc == VINF_EM_RESET || rc == VINF_EM_OFF))
            {
                Log2(("emR3ForcedActions: returns %Rrc\n", rc));
                STAM_REL_PROFILE_STOP(&pVCpu->em.s.StatForcedActions, a);
                return rc;
            }
        }

        /* check that we got them all  */
        Assert(!(VMCPU_FF_NORMAL_PRIORITY_MASK & ~VMCPU_FF_REQUEST));
    }

    /*
     * High priority pre execution chunk last.
     * (Executed in ascending priority order.)
     */
    if (    VM_FF_IS_PENDING(pVM, VM_FF_HIGH_PRIORITY_PRE_MASK)
        ||  VMCPU_FF_IS_ANY_SET(pVCpu, VMCPU_FF_HIGH_PRIORITY_PRE_MASK))
    {
        /*
         * Timers before interrupts.
         */
        if (   VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_TIMER)
            && !VM_FF_IS_SET(pVM, VM_FF_PGM_NO_MEMORY))
            TMR3TimerQueuesDo(pVM);

        /*
         * Pick up asynchronously posted interrupts into the APIC.
         */
        if (VMCPU_FF_TEST_AND_CLEAR(pVCpu, VMCPU_FF_UPDATE_APIC))
            APICUpdatePendingInterrupts(pVCpu);

        /*
         * The instruction following an emulated STI should *always* be executed!
         *
         * Note! We intentionally don't clear VM_FF_INHIBIT_INTERRUPTS here if
         *       the eip is the same as the inhibited instr address.  Before we
         *       are able to execute this instruction in raw mode (iret to
         *       guest code) an external interrupt might force a world switch
         *       again.  Possibly allowing a guest interrupt to be dispatched
         *       in the process.  This could break the guest.  Sounds very
         *       unlikely, but such timing sensitive problem are not as rare as
         *       you might think.
         */
        if (   VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS)
            && !VM_FF_IS_SET(pVM, VM_FF_PGM_NO_MEMORY))
        {
            CPUM_ASSERT_NOT_EXTRN(pVCpu, CPUMCTX_EXTRN_RIP);
            if (CPUMGetGuestRIP(pVCpu) != EMGetInhibitInterruptsPC(pVCpu))
            {
                Log(("Clearing VMCPU_FF_INHIBIT_INTERRUPTS at %RGv - successor %RGv\n", (RTGCPTR)CPUMGetGuestRIP(pVCpu), EMGetInhibitInterruptsPC(pVCpu)));
                VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);
            }
            else
                Log(("Leaving VMCPU_FF_INHIBIT_INTERRUPTS set at %RGv\n", (RTGCPTR)CPUMGetGuestRIP(pVCpu)));
        }

        /*
         * Interrupts.
         */
        bool fWakeupPending = false;
        if (   !VM_FF_IS_SET(pVM, VM_FF_PGM_NO_MEMORY)
            && (!rc || rc >= VINF_EM_RESCHEDULE_HM))
        {
            if (   !VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS)
                && !TRPMHasTrap(pVCpu)) /* an interrupt could already be scheduled for dispatching in the recompiler. */
            {
                Assert(!HMR3IsEventPending(pVCpu));
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
                if (CPUMIsGuestInSvmNestedHwVirtMode(&pVCpu->cpum.GstCtx))
                {
                    bool fResched, fInject;
                    rc2 = emR3NstGstInjectIntr(pVCpu, &fResched, &fInject);
                    if (fInject)
                    {
                        fWakeupPending = true;
# ifdef VBOX_STRICT
                        rcIrq = rc2;
# endif
                    }
                    if (fResched)
                        UPDATE_RC();
                }
                else
#endif
                {
                    CPUM_ASSERT_NOT_EXTRN(pVCpu, CPUMCTX_EXTRN_RFLAGS);
                    if (   VMCPU_FF_IS_ANY_SET(pVCpu, VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC)
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
                        && pVCpu->cpum.GstCtx.hwvirt.fGif
#endif
#ifdef VBOX_WITH_RAW_MODE
                        && !PATMIsPatchGCAddr(pVM, pVCpu->cpum.GstCtx.eip)
#endif
                        && pVCpu->cpum.GstCtx.eflags.Bits.u1IF)
                    {
                        Assert(pVCpu->em.s.enmState != EMSTATE_WAIT_SIPI);
                        /* Note: it's important to make sure the return code from TRPMR3InjectEvent isn't ignored! */
                        /** @todo this really isn't nice, should properly handle this */
                        CPUM_IMPORT_EXTRN_RET(pVCpu, IEM_CPUMCTX_EXTRN_XCPT_MASK);
                        rc2 = TRPMR3InjectEvent(pVM, pVCpu, TRPM_HARDWARE_INT);
                        Log(("EM: TRPMR3InjectEvent -> %d\n", rc2));
                        if (pVM->em.s.fIemExecutesAll && (   rc2 == VINF_EM_RESCHEDULE_REM
                                                          || rc2 == VINF_EM_RESCHEDULE_HM
                                                          || rc2 == VINF_EM_RESCHEDULE_RAW))
                        {
                            rc2 = VINF_EM_RESCHEDULE;
                        }
#ifdef VBOX_STRICT
                        rcIrq = rc2;
#endif
                        UPDATE_RC();
                        /* Reschedule required: We must not miss the wakeup below! */
                        fWakeupPending = true;
                    }
                }
            }
        }

        /*
         * Allocate handy pages.
         */
        if (VM_FF_IS_PENDING_EXCEPT(pVM, VM_FF_PGM_NEED_HANDY_PAGES, VM_FF_PGM_NO_MEMORY))
        {
            rc2 = PGMR3PhysAllocateHandyPages(pVM);
            UPDATE_RC();
        }

        /*
         * Debugger Facility request.
         */
        if (   (   VM_FF_IS_SET(pVM, VM_FF_DBGF)
                || VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_DBGF) )
            && !VM_FF_IS_SET(pVM, VM_FF_PGM_NO_MEMORY) )
        {
            CPUM_IMPORT_EXTRN_RCSTRICT(pVCpu, ~CPUMCTX_EXTRN_KEEPER_MASK, rc);
            rc2 = DBGFR3VMMForcedAction(pVM, pVCpu);
            UPDATE_RC();
        }

        /*
         * EMT Rendezvous (must be serviced before termination).
         */
        if (   !fWakeupPending /* don't miss the wakeup from EMSTATE_HALTED! */
            && VM_FF_IS_SET(pVM, VM_FF_EMT_RENDEZVOUS))
        {
            CPUM_IMPORT_EXTRN_RCSTRICT(pVCpu, ~CPUMCTX_EXTRN_KEEPER_MASK, rc);
            rc2 = VMMR3EmtRendezvousFF(pVM, pVCpu);
            UPDATE_RC();
            /** @todo HACK ALERT! The following test is to make sure EM+TM thinks the VM is
             * stopped/reset before the next VM state change is made. We need a better
             * solution for this, or at least make it possible to do: (rc >= VINF_EM_FIRST
             * && rc >= VINF_EM_SUSPEND). */
            if (RT_UNLIKELY(rc == VINF_EM_SUSPEND || rc == VINF_EM_RESET || rc == VINF_EM_OFF))
            {
                Log2(("emR3ForcedActions: returns %Rrc\n", rc));
                STAM_REL_PROFILE_STOP(&pVCpu->em.s.StatForcedActions, a);
                return rc;
            }
        }

        /*
         * State change request (cleared by vmR3SetStateLocked).
         */
        if (   !fWakeupPending /* don't miss the wakeup from EMSTATE_HALTED! */
            && VM_FF_IS_SET(pVM, VM_FF_CHECK_VM_STATE))
        {
            VMSTATE enmState = VMR3GetState(pVM);
            switch (enmState)
            {
                case VMSTATE_FATAL_ERROR:
                case VMSTATE_FATAL_ERROR_LS:
                case VMSTATE_GURU_MEDITATION:
                case VMSTATE_GURU_MEDITATION_LS:
                    Log2(("emR3ForcedActions: %s -> VINF_EM_SUSPEND\n", VMGetStateName(enmState) ));
                    STAM_REL_PROFILE_STOP(&pVCpu->em.s.StatForcedActions, a);
                    return VINF_EM_SUSPEND;

                case VMSTATE_DESTROYING:
                    Log2(("emR3ForcedActions: %s -> VINF_EM_TERMINATE\n", VMGetStateName(enmState) ));
                    STAM_REL_PROFILE_STOP(&pVCpu->em.s.StatForcedActions, a);
                    return VINF_EM_TERMINATE;

                default:
                    AssertMsgFailed(("%s\n", VMGetStateName(enmState)));
            }
        }

        /*
         * Out of memory? Since most of our fellow high priority actions may cause us
         * to run out of memory, we're employing VM_FF_IS_PENDING_EXCEPT and putting this
         * at the end rather than the start. Also, VM_FF_TERMINATE has higher priority
         * than us since we can terminate without allocating more memory.
         */
        if (VM_FF_IS_SET(pVM, VM_FF_PGM_NO_MEMORY))
        {
            rc2 = PGMR3PhysAllocateHandyPages(pVM);
            UPDATE_RC();
            if (rc == VINF_EM_NO_MEMORY)
                return rc;
        }

        /*
         * If the virtual sync clock is still stopped, make TM restart it.
         */
        if (VM_FF_IS_SET(pVM, VM_FF_TM_VIRTUAL_SYNC))
            TMR3VirtualSyncFF(pVM, pVCpu);

#ifdef DEBUG
        /*
         * Debug, pause the VM.
         */
        if (VM_FF_IS_SET(pVM, VM_FF_DEBUG_SUSPEND))
        {
            VM_FF_CLEAR(pVM, VM_FF_DEBUG_SUSPEND);
            Log(("emR3ForcedActions: returns VINF_EM_SUSPEND\n"));
            return VINF_EM_SUSPEND;
        }
#endif

        /* check that we got them all  */
        AssertCompile(VM_FF_HIGH_PRIORITY_PRE_MASK == (VM_FF_TM_VIRTUAL_SYNC | VM_FF_DBGF | VM_FF_CHECK_VM_STATE | VM_FF_DEBUG_SUSPEND | VM_FF_PGM_NEED_HANDY_PAGES | VM_FF_PGM_NO_MEMORY | VM_FF_EMT_RENDEZVOUS));
        AssertCompile(VMCPU_FF_HIGH_PRIORITY_PRE_MASK == (VMCPU_FF_TIMER | VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_UPDATE_APIC | VMCPU_FF_INTERRUPT_PIC | VMCPU_FF_PGM_SYNC_CR3 | VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL | VMCPU_FF_INHIBIT_INTERRUPTS | VMCPU_FF_DBGF | VM_WHEN_RAW_MODE(VMCPU_FF_SELM_SYNC_TSS | VMCPU_FF_TRPM_SYNC_IDT | VMCPU_FF_SELM_SYNC_GDT | VMCPU_FF_SELM_SYNC_LDT, 0)));
    }

#undef UPDATE_RC
    Log2(("emR3ForcedActions: returns %Rrc\n", rc));
    STAM_REL_PROFILE_STOP(&pVCpu->em.s.StatForcedActions, a);
    Assert(rcIrq == VINF_SUCCESS || rcIrq == rc);
    return rc;
}


/**
 * Check if the preset execution time cap restricts guest execution scheduling.
 *
 * @returns true if allowed, false otherwise
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
bool emR3IsExecutionAllowed(PVM pVM, PVMCPU pVCpu)
{
    uint64_t u64UserTime, u64KernelTime;

    if (    pVM->uCpuExecutionCap != 100
        &&  RT_SUCCESS(RTThreadGetExecutionTimeMilli(&u64KernelTime, &u64UserTime)))
    {
        uint64_t u64TimeNow = RTTimeMilliTS();
        if (pVCpu->em.s.u64TimeSliceStart + EM_TIME_SLICE < u64TimeNow)
        {
            /* New time slice. */
            pVCpu->em.s.u64TimeSliceStart     = u64TimeNow;
            pVCpu->em.s.u64TimeSliceStartExec = u64KernelTime + u64UserTime;
            pVCpu->em.s.u64TimeSliceExec      = 0;
        }
        pVCpu->em.s.u64TimeSliceExec = u64KernelTime + u64UserTime - pVCpu->em.s.u64TimeSliceStartExec;

        Log2(("emR3IsExecutionAllowed: start=%RX64 startexec=%RX64 exec=%RX64 (cap=%x)\n", pVCpu->em.s.u64TimeSliceStart, pVCpu->em.s.u64TimeSliceStartExec, pVCpu->em.s.u64TimeSliceExec, (EM_TIME_SLICE * pVM->uCpuExecutionCap) / 100));
        if (pVCpu->em.s.u64TimeSliceExec >= (EM_TIME_SLICE * pVM->uCpuExecutionCap) / 100)
            return false;
    }
    return true;
}


/**
 * Execute VM.
 *
 * This function is the main loop of the VM. The emulation thread
 * calls this function when the VM has been successfully constructed
 * and we're ready for executing the VM.
 *
 * Returning from this function means that the VM is turned off or
 * suspended (state already saved) and deconstruction is next in line.
 *
 * All interaction from other thread are done using forced actions
 * and signaling of the wait object.
 *
 * @returns VBox status code, informational status codes may indicate failure.
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
VMMR3_INT_DECL(int) EMR3ExecuteVM(PVM pVM, PVMCPU pVCpu)
{
    Log(("EMR3ExecuteVM: pVM=%p enmVMState=%d (%s)  enmState=%d (%s) enmPrevState=%d (%s) fForceRAW=%RTbool\n",
         pVM,
         pVM->enmVMState,          VMR3GetStateName(pVM->enmVMState),
         pVCpu->em.s.enmState,     emR3GetStateName(pVCpu->em.s.enmState),
         pVCpu->em.s.enmPrevState, emR3GetStateName(pVCpu->em.s.enmPrevState),
         pVCpu->em.s.fForceRAW));
    VM_ASSERT_EMT(pVM);
    AssertMsg(   pVCpu->em.s.enmState == EMSTATE_NONE
              || pVCpu->em.s.enmState == EMSTATE_WAIT_SIPI
              || pVCpu->em.s.enmState == EMSTATE_SUSPENDED,
              ("%s\n", emR3GetStateName(pVCpu->em.s.enmState)));

    int rc = setjmp(pVCpu->em.s.u.FatalLongJump);
    if (rc == 0)
    {
        /*
         * Start the virtual time.
         */
        TMR3NotifyResume(pVM, pVCpu);

        /*
         * The Outer Main Loop.
         */
        bool fFFDone = false;

        /* Reschedule right away to start in the right state. */
        rc = VINF_SUCCESS;

        /* If resuming after a pause or a state load, restore the previous
           state or else we'll start executing code. Else, just reschedule. */
        if (    pVCpu->em.s.enmState == EMSTATE_SUSPENDED
            &&  (   pVCpu->em.s.enmPrevState == EMSTATE_WAIT_SIPI
                 || pVCpu->em.s.enmPrevState == EMSTATE_HALTED))
            pVCpu->em.s.enmState = pVCpu->em.s.enmPrevState;
        else
            pVCpu->em.s.enmState = emR3Reschedule(pVM, pVCpu);
        pVCpu->em.s.cIemThenRemInstructions = 0;
        Log(("EMR3ExecuteVM: enmState=%s\n", emR3GetStateName(pVCpu->em.s.enmState)));

        STAM_REL_PROFILE_ADV_START(&pVCpu->em.s.StatTotal, x);
        for (;;)
        {
            /*
             * Before we can schedule anything (we're here because
             * scheduling is required) we must service any pending
             * forced actions to avoid any pending action causing
             * immediate rescheduling upon entering an inner loop
             *
             * Do forced actions.
             */
            if (   !fFFDone
                && RT_SUCCESS(rc)
                && rc != VINF_EM_TERMINATE
                && rc != VINF_EM_OFF
                && (   VM_FF_IS_PENDING(pVM, VM_FF_ALL_REM_MASK)
                    || VMCPU_FF_IS_ANY_SET(pVCpu, VMCPU_FF_ALL_REM_MASK & ~VMCPU_FF_UNHALT)))
            {
                rc = emR3ForcedActions(pVM, pVCpu, rc);
                VBOXVMM_EM_FF_ALL_RET(pVCpu, rc);
                if (   (   rc == VINF_EM_RESCHEDULE_REM
                        || rc == VINF_EM_RESCHEDULE_HM)
                    && pVCpu->em.s.fForceRAW)
                    rc = VINF_EM_RESCHEDULE_RAW;
            }
            else if (fFFDone)
                fFFDone = false;

            /*
             * Now what to do?
             */
            Log2(("EMR3ExecuteVM: rc=%Rrc\n", rc));
            EMSTATE const enmOldState = pVCpu->em.s.enmState;
            switch (rc)
            {
                /*
                 * Keep doing what we're currently doing.
                 */
                case VINF_SUCCESS:
                    break;

                /*
                 * Reschedule - to raw-mode execution.
                 */
/** @todo r=bird: consider merging VINF_EM_RESCHEDULE_RAW with VINF_EM_RESCHEDULE_HM, they serve the same purpose here at least. */
                case VINF_EM_RESCHEDULE_RAW:
                    Assert(!pVM->em.s.fIemExecutesAll || pVCpu->em.s.enmState != EMSTATE_IEM);
                    if (VM_IS_RAW_MODE_ENABLED(pVM))
                    {
                        Log2(("EMR3ExecuteVM: VINF_EM_RESCHEDULE_RAW: %d -> %d (EMSTATE_RAW)\n", enmOldState, EMSTATE_RAW));
                        pVCpu->em.s.enmState = EMSTATE_RAW;
                    }
                    else
                    {
                        AssertLogRelFailed();
                        pVCpu->em.s.enmState = EMSTATE_NONE;
                    }
                    break;

                /*
                 * Reschedule - to HM or NEM.
                 */
                case VINF_EM_RESCHEDULE_HM:
                    Assert(!pVM->em.s.fIemExecutesAll || pVCpu->em.s.enmState != EMSTATE_IEM);
                    Assert(!pVCpu->em.s.fForceRAW);
                    if (VM_IS_HM_ENABLED(pVM))
                    {
                        Log2(("EMR3ExecuteVM: VINF_EM_RESCHEDULE_HM: %d -> %d (EMSTATE_HM)\n", enmOldState, EMSTATE_HM));
                        pVCpu->em.s.enmState = EMSTATE_HM;
                    }
                    else if (VM_IS_NEM_ENABLED(pVM))
                    {
                        Log2(("EMR3ExecuteVM: VINF_EM_RESCHEDULE_HM: %d -> %d (EMSTATE_NEM)\n", enmOldState, EMSTATE_NEM));
                        pVCpu->em.s.enmState = EMSTATE_NEM;
                    }
                    else
                    {
                        AssertLogRelFailed();
                        pVCpu->em.s.enmState = EMSTATE_NONE;
                    }
                    break;

                /*
                 * Reschedule - to recompiled execution.
                 */
                case VINF_EM_RESCHEDULE_REM:
                    Assert(!pVM->em.s.fIemExecutesAll || pVCpu->em.s.enmState != EMSTATE_IEM);
                    if (!VM_IS_RAW_MODE_ENABLED(pVM))
                    {
                        Log2(("EMR3ExecuteVM: VINF_EM_RESCHEDULE_REM: %d -> %d (EMSTATE_IEM_THEN_REM)\n",
                              enmOldState, EMSTATE_IEM_THEN_REM));
                        if (pVCpu->em.s.enmState != EMSTATE_IEM_THEN_REM)
                        {
                            pVCpu->em.s.enmState = EMSTATE_IEM_THEN_REM;
                            pVCpu->em.s.cIemThenRemInstructions = 0;
                        }
                    }
                    else
                    {
                        Log2(("EMR3ExecuteVM: VINF_EM_RESCHEDULE_REM: %d -> %d (EMSTATE_REM)\n", enmOldState, EMSTATE_REM));
                        pVCpu->em.s.enmState = EMSTATE_REM;
                    }
                    break;

                /*
                 * Resume.
                 */
                case VINF_EM_RESUME:
                    Log2(("EMR3ExecuteVM: VINF_EM_RESUME: %d -> VINF_EM_RESCHEDULE\n", enmOldState));
                    /* Don't reschedule in the halted or wait for SIPI case. */
                    if (    pVCpu->em.s.enmPrevState == EMSTATE_WAIT_SIPI
                        ||  pVCpu->em.s.enmPrevState == EMSTATE_HALTED)
                    {
                        pVCpu->em.s.enmState = pVCpu->em.s.enmPrevState;
                        break;
                    }
                    /* fall through and get scheduled. */
                    RT_FALL_THRU();

                /*
                 * Reschedule.
                 */
                case VINF_EM_RESCHEDULE:
                {
                    EMSTATE enmState = emR3Reschedule(pVM, pVCpu);
                    Log2(("EMR3ExecuteVM: VINF_EM_RESCHEDULE: %d -> %d (%s)\n", enmOldState, enmState, emR3GetStateName(enmState)));
                    if (pVCpu->em.s.enmState != enmState && enmState == EMSTATE_IEM_THEN_REM)
                        pVCpu->em.s.cIemThenRemInstructions = 0;
                    pVCpu->em.s.enmState = enmState;
                    break;
                }

                /*
                 * Halted.
                 */
                case VINF_EM_HALT:
                    Log2(("EMR3ExecuteVM: VINF_EM_HALT: %d -> %d\n", enmOldState, EMSTATE_HALTED));
                    pVCpu->em.s.enmState = EMSTATE_HALTED;
                    break;

                /*
                 * Switch to the wait for SIPI state (application processor only)
                 */
                case VINF_EM_WAIT_SIPI:
                    Assert(pVCpu->idCpu != 0);
                    Log2(("EMR3ExecuteVM: VINF_EM_WAIT_SIPI: %d -> %d\n", enmOldState, EMSTATE_WAIT_SIPI));
                    pVCpu->em.s.enmState = EMSTATE_WAIT_SIPI;
                    break;


                /*
                 * Suspend.
                 */
                case VINF_EM_SUSPEND:
                    Log2(("EMR3ExecuteVM: VINF_EM_SUSPEND: %d -> %d\n", enmOldState, EMSTATE_SUSPENDED));
                    Assert(enmOldState != EMSTATE_SUSPENDED);
                    pVCpu->em.s.enmPrevState = enmOldState;
                    pVCpu->em.s.enmState     = EMSTATE_SUSPENDED;
                    break;

                /*
                 * Reset.
                 * We might end up doing a double reset for now, we'll have to clean up the mess later.
                 */
                case VINF_EM_RESET:
                {
                    if (pVCpu->idCpu == 0)
                    {
                        EMSTATE enmState = emR3Reschedule(pVM, pVCpu);
                        Log2(("EMR3ExecuteVM: VINF_EM_RESET: %d -> %d (%s)\n", enmOldState, enmState, emR3GetStateName(enmState)));
                        if (pVCpu->em.s.enmState != enmState && enmState == EMSTATE_IEM_THEN_REM)
                            pVCpu->em.s.cIemThenRemInstructions = 0;
                        pVCpu->em.s.enmState = enmState;
                    }
                    else
                    {
                        /* All other VCPUs go into the wait for SIPI state. */
                        pVCpu->em.s.enmState = EMSTATE_WAIT_SIPI;
                    }
                    break;
                }

                /*
                 * Power Off.
                 */
                case VINF_EM_OFF:
                    pVCpu->em.s.enmState = EMSTATE_TERMINATING;
                    Log2(("EMR3ExecuteVM: returns VINF_EM_OFF (%d -> %d)\n", enmOldState, EMSTATE_TERMINATING));
                    TMR3NotifySuspend(pVM, pVCpu);
                    STAM_REL_PROFILE_ADV_STOP(&pVCpu->em.s.StatTotal, x);
                    return rc;

                /*
                 * Terminate the VM.
                 */
                case VINF_EM_TERMINATE:
                    pVCpu->em.s.enmState = EMSTATE_TERMINATING;
                    Log(("EMR3ExecuteVM returns VINF_EM_TERMINATE (%d -> %d)\n", enmOldState, EMSTATE_TERMINATING));
                    if (pVM->enmVMState < VMSTATE_DESTROYING) /* ugly */
                        TMR3NotifySuspend(pVM, pVCpu);
                    STAM_REL_PROFILE_ADV_STOP(&pVCpu->em.s.StatTotal, x);
                    return rc;


                /*
                 * Out of memory, suspend the VM and stuff.
                 */
                case VINF_EM_NO_MEMORY:
                    Log2(("EMR3ExecuteVM: VINF_EM_NO_MEMORY: %d -> %d\n", enmOldState, EMSTATE_SUSPENDED));
                    Assert(enmOldState != EMSTATE_SUSPENDED);
                    pVCpu->em.s.enmPrevState = enmOldState;
                    pVCpu->em.s.enmState = EMSTATE_SUSPENDED;
                    TMR3NotifySuspend(pVM, pVCpu);
                    STAM_REL_PROFILE_ADV_STOP(&pVCpu->em.s.StatTotal, x);

                    rc = VMSetRuntimeError(pVM, VMSETRTERR_FLAGS_SUSPEND, "HostMemoryLow",
                                           N_("Unable to allocate and lock memory. The virtual machine will be paused. Please close applications to free up memory or close the VM"));
                    if (rc != VINF_EM_SUSPEND)
                    {
                        if (RT_SUCCESS_NP(rc))
                        {
                            AssertLogRelMsgFailed(("%Rrc\n", rc));
                            rc = VERR_EM_INTERNAL_ERROR;
                        }
                        pVCpu->em.s.enmState = EMSTATE_GURU_MEDITATION;
                    }
                    return rc;

                /*
                 * Guest debug events.
                 */
                case VINF_EM_DBG_STEPPED:
                case VINF_EM_DBG_STOP:
                case VINF_EM_DBG_EVENT:
                case VINF_EM_DBG_BREAKPOINT:
                case VINF_EM_DBG_STEP:
                    if (enmOldState == EMSTATE_RAW)
                    {
                        Log2(("EMR3ExecuteVM: %Rrc: %d -> %d\n", rc, enmOldState, EMSTATE_DEBUG_GUEST_RAW));
                        pVCpu->em.s.enmState = EMSTATE_DEBUG_GUEST_RAW;
                    }
                    else if (enmOldState == EMSTATE_HM)
                    {
                        Log2(("EMR3ExecuteVM: %Rrc: %d -> %d\n", rc, enmOldState, EMSTATE_DEBUG_GUEST_HM));
                        pVCpu->em.s.enmState = EMSTATE_DEBUG_GUEST_HM;
                    }
                    else if (enmOldState == EMSTATE_NEM)
                    {
                        Log2(("EMR3ExecuteVM: %Rrc: %d -> %d\n", rc, enmOldState, EMSTATE_DEBUG_GUEST_NEM));
                        pVCpu->em.s.enmState = EMSTATE_DEBUG_GUEST_NEM;
                    }
                    else if (enmOldState == EMSTATE_REM)
                    {
                        Log2(("EMR3ExecuteVM: %Rrc: %d -> %d\n", rc, enmOldState, EMSTATE_DEBUG_GUEST_REM));
                        pVCpu->em.s.enmState = EMSTATE_DEBUG_GUEST_REM;
                    }
                    else
                    {
                        Log2(("EMR3ExecuteVM: %Rrc: %d -> %d\n", rc, enmOldState, EMSTATE_DEBUG_GUEST_IEM));
                        pVCpu->em.s.enmState = EMSTATE_DEBUG_GUEST_IEM;
                    }
                    break;

                /*
                 * Hypervisor debug events.
                 */
                case VINF_EM_DBG_HYPER_STEPPED:
                case VINF_EM_DBG_HYPER_BREAKPOINT:
                case VINF_EM_DBG_HYPER_ASSERTION:
                    Log2(("EMR3ExecuteVM: %Rrc: %d -> %d\n", rc, enmOldState, EMSTATE_DEBUG_HYPER));
                    pVCpu->em.s.enmState = EMSTATE_DEBUG_HYPER;
                    break;

                /*
                 * Triple fault.
                 */
                case VINF_EM_TRIPLE_FAULT:
                    if (!pVM->em.s.fGuruOnTripleFault)
                    {
                        Log(("EMR3ExecuteVM: VINF_EM_TRIPLE_FAULT: CPU reset...\n"));
                        rc = VBOXSTRICTRC_TODO(VMR3ResetTripleFault(pVM));
                        Log2(("EMR3ExecuteVM: VINF_EM_TRIPLE_FAULT: %d -> %d (rc=%Rrc)\n", enmOldState, pVCpu->em.s.enmState, rc));
                        continue;
                    }
                    /* Else fall through and trigger a guru. */
                    RT_FALL_THRU();

                case VERR_VMM_RING0_ASSERTION:
                    Log(("EMR3ExecuteVM: %Rrc: %d -> %d (EMSTATE_GURU_MEDITATION)\n", rc, enmOldState, EMSTATE_GURU_MEDITATION));
                    pVCpu->em.s.enmState = EMSTATE_GURU_MEDITATION;
                    break;

                /*
                 * Any error code showing up here other than the ones we
                 * know and process above are considered to be FATAL.
                 *
                 * Unknown warnings and informational status codes are also
                 * included in this.
                 */
                default:
                    if (RT_SUCCESS_NP(rc))
                    {
                        AssertMsgFailed(("Unexpected warning or informational status code %Rra!\n", rc));
                        rc = VERR_EM_INTERNAL_ERROR;
                    }
                    Log(("EMR3ExecuteVM: %Rrc: %d -> %d (EMSTATE_GURU_MEDITATION)\n", rc, enmOldState, EMSTATE_GURU_MEDITATION));
                    pVCpu->em.s.enmState = EMSTATE_GURU_MEDITATION;
                    break;
            }

            /*
             * Act on state transition.
             */
            EMSTATE const enmNewState = pVCpu->em.s.enmState;
            if (enmOldState != enmNewState)
            {
                VBOXVMM_EM_STATE_CHANGED(pVCpu, enmOldState, enmNewState, rc);

                /* Clear MWait flags and the unhalt FF. */
                if (   enmOldState == EMSTATE_HALTED
                    && (   (pVCpu->em.s.MWait.fWait & EMMWAIT_FLAG_ACTIVE)
                        || VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_UNHALT))
                    && (   enmNewState == EMSTATE_RAW
                        || enmNewState == EMSTATE_HM
                        || enmNewState == EMSTATE_NEM
                        || enmNewState == EMSTATE_REM
                        || enmNewState == EMSTATE_IEM_THEN_REM
                        || enmNewState == EMSTATE_DEBUG_GUEST_RAW
                        || enmNewState == EMSTATE_DEBUG_GUEST_HM
                        || enmNewState == EMSTATE_DEBUG_GUEST_NEM
                        || enmNewState == EMSTATE_DEBUG_GUEST_IEM
                        || enmNewState == EMSTATE_DEBUG_GUEST_REM) )
                {
                    if (pVCpu->em.s.MWait.fWait & EMMWAIT_FLAG_ACTIVE)
                    {
                        LogFlow(("EMR3ExecuteVM: Clearing MWAIT\n"));
                        pVCpu->em.s.MWait.fWait &= ~(EMMWAIT_FLAG_ACTIVE | EMMWAIT_FLAG_BREAKIRQIF0);
                    }
                    if (VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_UNHALT))
                    {
                        LogFlow(("EMR3ExecuteVM: Clearing UNHALT\n"));
                        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_UNHALT);
                    }
                }
            }
            else
                VBOXVMM_EM_STATE_UNCHANGED(pVCpu, enmNewState, rc);

            STAM_PROFILE_ADV_STOP(&pVCpu->em.s.StatTotal, x); /* (skip this in release) */
            STAM_PROFILE_ADV_START(&pVCpu->em.s.StatTotal, x);

            /*
             * Act on the new state.
             */
            switch (enmNewState)
            {
                /*
                 * Execute raw.
                 */
                case EMSTATE_RAW:
#ifdef VBOX_WITH_RAW_MODE
                    rc = emR3RawExecute(pVM, pVCpu, &fFFDone);
#else
                    AssertLogRelMsgFailed(("%Rrc\n", rc));
                    rc = VERR_EM_INTERNAL_ERROR;
#endif
                    break;

                /*
                 * Execute hardware accelerated raw.
                 */
                case EMSTATE_HM:
                    rc = emR3HmExecute(pVM, pVCpu, &fFFDone);
                    break;

                /*
                 * Execute hardware accelerated raw.
                 */
                case EMSTATE_NEM:
                    rc = VBOXSTRICTRC_TODO(emR3NemExecute(pVM, pVCpu, &fFFDone));
                    break;

                /*
                 * Execute recompiled.
                 */
                case EMSTATE_REM:
                    rc = emR3RemExecute(pVM, pVCpu, &fFFDone);
                    Log2(("EMR3ExecuteVM: emR3RemExecute -> %Rrc\n", rc));
                    break;

                /*
                 * Execute in the interpreter.
                 */
                case EMSTATE_IEM:
                {
#if 0 /* For testing purposes. */
                    STAM_PROFILE_START(&pVCpu->em.s.StatHmExec, x1);
                    rc = VBOXSTRICTRC_TODO(EMR3HmSingleInstruction(pVM, pVCpu, EM_ONE_INS_FLAGS_RIP_CHANGE));
                    STAM_PROFILE_STOP(&pVCpu->em.s.StatHmExec, x1);
                    if (rc == VINF_EM_DBG_STEPPED || rc == VINF_EM_RESCHEDULE_HM || rc == VINF_EM_RESCHEDULE_REM || rc == VINF_EM_RESCHEDULE_RAW)
                        rc = VINF_SUCCESS;
                    else if (rc == VERR_EM_CANNOT_EXEC_GUEST)
#endif
                        rc = VBOXSTRICTRC_TODO(IEMExecLots(pVCpu, NULL /*pcInstructions*/));
                    if (pVM->em.s.fIemExecutesAll)
                    {
                        Assert(rc != VINF_EM_RESCHEDULE_REM);
                        Assert(rc != VINF_EM_RESCHEDULE_RAW);
                        Assert(rc != VINF_EM_RESCHEDULE_HM);
                    }
                    fFFDone = false;
                    break;
                }

                /*
                 * Execute in IEM, hoping we can quickly switch aback to HM
                 * or RAW execution.  If our hopes fail, we go to REM.
                 */
                case EMSTATE_IEM_THEN_REM:
                {
                    STAM_PROFILE_START(&pVCpu->em.s.StatIEMThenREM, pIemThenRem);
                    rc = VBOXSTRICTRC_TODO(emR3ExecuteIemThenRem(pVM, pVCpu, &fFFDone));
                    STAM_PROFILE_STOP(&pVCpu->em.s.StatIEMThenREM, pIemThenRem);
                    break;
                }

                /*
                 * Application processor execution halted until SIPI.
                 */
                case EMSTATE_WAIT_SIPI:
                    /* no break */
                /*
                 * hlt - execution halted until interrupt.
                 */
                case EMSTATE_HALTED:
                {
                    STAM_REL_PROFILE_START(&pVCpu->em.s.StatHalted, y);
                    /* If HM (or someone else) store a pending interrupt in
                       TRPM, it must be dispatched ASAP without any halting.
                       Anything pending in TRPM has been accepted and the CPU
                       should already be the right state to receive it. */
                    if (TRPMHasTrap(pVCpu))
                        rc = VINF_EM_RESCHEDULE;
                    /* MWAIT has a special extension where it's woken up when
                       an interrupt is pending even when IF=0. */
                    else if (   (pVCpu->em.s.MWait.fWait & (EMMWAIT_FLAG_ACTIVE | EMMWAIT_FLAG_BREAKIRQIF0))
                             ==                            (EMMWAIT_FLAG_ACTIVE | EMMWAIT_FLAG_BREAKIRQIF0))
                    {
                        rc = VMR3WaitHalted(pVM, pVCpu, false /*fIgnoreInterrupts*/);
                        if (rc == VINF_SUCCESS)
                        {
                            if (VMCPU_FF_TEST_AND_CLEAR(pVCpu, VMCPU_FF_UPDATE_APIC))
                                APICUpdatePendingInterrupts(pVCpu);

                            if (VMCPU_FF_IS_ANY_SET(pVCpu, VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC
                                                         | VMCPU_FF_INTERRUPT_NMI  | VMCPU_FF_INTERRUPT_SMI | VMCPU_FF_UNHALT))
                            {
                                Log(("EMR3ExecuteVM: Triggering reschedule on pending IRQ after MWAIT\n"));
                                rc = VINF_EM_RESCHEDULE;
                            }
                        }
                    }
                    else
                    {
                        rc = VMR3WaitHalted(pVM, pVCpu, !(CPUMGetGuestEFlags(pVCpu) & X86_EFL_IF));
                        /* We're only interested in NMI/SMIs here which have their own FFs, so we don't need to
                           check VMCPU_FF_UPDATE_APIC here. */
                        if (   rc == VINF_SUCCESS
                            && VMCPU_FF_IS_ANY_SET(pVCpu, VMCPU_FF_INTERRUPT_NMI | VMCPU_FF_INTERRUPT_SMI | VMCPU_FF_UNHALT))
                        {
                            Log(("EMR3ExecuteVM: Triggering reschedule on pending NMI/SMI/UNHALT after HLT\n"));
                            rc = VINF_EM_RESCHEDULE;
                        }
                    }

                    STAM_REL_PROFILE_STOP(&pVCpu->em.s.StatHalted, y);
                    break;
                }

                /*
                 * Suspended - return to VM.cpp.
                 */
                case EMSTATE_SUSPENDED:
                    TMR3NotifySuspend(pVM, pVCpu);
                    STAM_REL_PROFILE_ADV_STOP(&pVCpu->em.s.StatTotal, x);
                    Log(("EMR3ExecuteVM: actually returns %Rrc (state %s / %s)\n", rc, emR3GetStateName(pVCpu->em.s.enmState), emR3GetStateName(enmOldState)));
                    return VINF_EM_SUSPEND;

                /*
                 * Debugging in the guest.
                 */
                case EMSTATE_DEBUG_GUEST_RAW:
                case EMSTATE_DEBUG_GUEST_HM:
                case EMSTATE_DEBUG_GUEST_NEM:
                case EMSTATE_DEBUG_GUEST_IEM:
                case EMSTATE_DEBUG_GUEST_REM:
                    TMR3NotifySuspend(pVM, pVCpu);
                    rc = VBOXSTRICTRC_TODO(emR3Debug(pVM, pVCpu, rc));
                    TMR3NotifyResume(pVM, pVCpu);
                    Log2(("EMR3ExecuteVM: emR3Debug -> %Rrc (state %d)\n", rc, pVCpu->em.s.enmState));
                    break;

                /*
                 * Debugging in the hypervisor.
                 */
                case EMSTATE_DEBUG_HYPER:
                {
                    TMR3NotifySuspend(pVM, pVCpu);
                    STAM_REL_PROFILE_ADV_STOP(&pVCpu->em.s.StatTotal, x);

                    rc = VBOXSTRICTRC_TODO(emR3Debug(pVM, pVCpu, rc));
                    Log2(("EMR3ExecuteVM: emR3Debug -> %Rrc (state %d)\n", rc, pVCpu->em.s.enmState));
                    if (rc != VINF_SUCCESS)
                    {
                        if (rc == VINF_EM_OFF || rc == VINF_EM_TERMINATE)
                            pVCpu->em.s.enmState = EMSTATE_TERMINATING;
                        else
                        {
                            /* switch to guru meditation mode */
                            pVCpu->em.s.enmState = EMSTATE_GURU_MEDITATION;
                            VMR3SetGuruMeditation(pVM); /* This notifies the other EMTs. */
                            VMMR3FatalDump(pVM, pVCpu, rc);
                        }
                        Log(("EMR3ExecuteVM: actually returns %Rrc (state %s / %s)\n", rc, emR3GetStateName(pVCpu->em.s.enmState), emR3GetStateName(enmOldState)));
                        return rc;
                    }

                    STAM_REL_PROFILE_ADV_START(&pVCpu->em.s.StatTotal, x);
                    TMR3NotifyResume(pVM, pVCpu);
                    break;
                }

                /*
                 * Guru meditation takes place in the debugger.
                 */
                case EMSTATE_GURU_MEDITATION:
                {
                    TMR3NotifySuspend(pVM, pVCpu);
                    VMR3SetGuruMeditation(pVM); /* This notifies the other EMTs. */
                    VMMR3FatalDump(pVM, pVCpu, rc);
                    emR3Debug(pVM, pVCpu, rc);
                    STAM_REL_PROFILE_ADV_STOP(&pVCpu->em.s.StatTotal, x);
                    Log(("EMR3ExecuteVM: actually returns %Rrc (state %s / %s)\n", rc, emR3GetStateName(pVCpu->em.s.enmState), emR3GetStateName(enmOldState)));
                    return rc;
                }

                /*
                 * The states we don't expect here.
                 */
                case EMSTATE_NONE:
                case EMSTATE_TERMINATING:
                default:
                    AssertMsgFailed(("EMR3ExecuteVM: Invalid state %d!\n", pVCpu->em.s.enmState));
                    pVCpu->em.s.enmState = EMSTATE_GURU_MEDITATION;
                    TMR3NotifySuspend(pVM, pVCpu);
                    STAM_REL_PROFILE_ADV_STOP(&pVCpu->em.s.StatTotal, x);
                    Log(("EMR3ExecuteVM: actually returns %Rrc (state %s / %s)\n", rc, emR3GetStateName(pVCpu->em.s.enmState), emR3GetStateName(enmOldState)));
                    return VERR_EM_INTERNAL_ERROR;
            }
        } /* The Outer Main Loop */
    }
    else
    {
        /*
         * Fatal error.
         */
        Log(("EMR3ExecuteVM: returns %Rrc because of longjmp / fatal error; (state %s / %s)\n", rc, emR3GetStateName(pVCpu->em.s.enmState), emR3GetStateName(pVCpu->em.s.enmPrevState)));
        TMR3NotifySuspend(pVM, pVCpu);
        VMR3SetGuruMeditation(pVM); /* This notifies the other EMTs. */
        VMMR3FatalDump(pVM, pVCpu, rc);
        emR3Debug(pVM, pVCpu, rc);
        STAM_REL_PROFILE_ADV_STOP(&pVCpu->em.s.StatTotal, x);
        /** @todo change the VM state! */
        return rc;
    }

    /* not reached */
}

/**
 * Notify EM of a state change (used by FTM)
 *
 * @param   pVM             The cross context VM structure.
 */
VMMR3_INT_DECL(int) EMR3NotifySuspend(PVM pVM)
{
    PVMCPU pVCpu = VMMGetCpu(pVM);

    TMR3NotifySuspend(pVM, pVCpu);  /* Stop the virtual time. */
    pVCpu->em.s.enmPrevState = pVCpu->em.s.enmState;
    pVCpu->em.s.enmState     = EMSTATE_SUSPENDED;
    return VINF_SUCCESS;
}

/**
 * Notify EM of a state change (used by FTM)
 *
 * @param   pVM             The cross context VM structure.
 */
VMMR3_INT_DECL(int) EMR3NotifyResume(PVM pVM)
{
    PVMCPU pVCpu = VMMGetCpu(pVM);
    EMSTATE enmCurState = pVCpu->em.s.enmState;

    TMR3NotifyResume(pVM, pVCpu);  /* Resume the virtual time. */
    pVCpu->em.s.enmState     = pVCpu->em.s.enmPrevState;
    pVCpu->em.s.enmPrevState = enmCurState;
    return VINF_SUCCESS;
}
