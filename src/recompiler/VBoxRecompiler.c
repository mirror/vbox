/* $Id$ */
/** @file
 * VBox Recompiler - QEMU.
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
#define LOG_GROUP LOG_GROUP_REM
#include "vl.h"
#include "exec-all.h"

#include <VBox/rem.h>
#include <VBox/vmapi.h>
#include <VBox/tm.h>
#include <VBox/ssm.h>
#include <VBox/em.h>
#include <VBox/trpm.h>
#include <VBox/iom.h>
#include <VBox/mm.h>
#include <VBox/pgm.h>
#include <VBox/pdm.h>
#include <VBox/dbgf.h>
#include <VBox/dbg.h>
#include <VBox/hwaccm.h>
#include <VBox/patm.h>
#include <VBox/csam.h>
#include "REMInternal.h"
#include <VBox/vm.h>
#include <VBox/param.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/semaphore.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/thread.h>
#include <iprt/string.h>

/* Don't wanna include everything. */
extern void cpu_x86_update_cr3(CPUX86State *env, target_ulong new_cr3);
extern void cpu_x86_update_cr0(CPUX86State *env, uint32_t new_cr0);
extern void cpu_x86_update_cr4(CPUX86State *env, uint32_t new_cr4);
extern void tlb_flush_page(CPUX86State *env, target_ulong addr);
extern void tlb_flush(CPUState *env, int flush_global);
extern void sync_seg(CPUX86State *env1, int seg_reg, int selector);
extern void sync_ldtr(CPUX86State *env1, int selector);
extern int  sync_tr(CPUX86State *env1, int selector);

#ifdef VBOX_STRICT
unsigned long get_phys_page_offset(target_ulong addr);
#endif


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/

/** Copy 80-bit fpu register at pSrc to pDst.
 * This is probably faster than *calling* memcpy.
 */
#define REM_COPY_FPU_REG(pDst, pSrc) \
    do { *(PX86FPUMMX)(pDst) = *(const X86FPUMMX *)(pSrc); } while (0)


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) remR3Save(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int) remR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version);
static void     remR3StateUpdate(PVM pVM);

static uint32_t remR3MMIOReadU8(void *pvVM, target_phys_addr_t GCPhys);
static uint32_t remR3MMIOReadU16(void *pvVM, target_phys_addr_t GCPhys);
static uint32_t remR3MMIOReadU32(void *pvVM, target_phys_addr_t GCPhys);
static void     remR3MMIOWriteU8(void *pvVM, target_phys_addr_t GCPhys, uint32_t u32);
static void     remR3MMIOWriteU16(void *pvVM, target_phys_addr_t GCPhys, uint32_t u32);
static void     remR3MMIOWriteU32(void *pvVM, target_phys_addr_t GCPhys, uint32_t u32);

static uint32_t remR3HandlerReadU8(void *pvVM, target_phys_addr_t GCPhys);
static uint32_t remR3HandlerReadU16(void *pvVM, target_phys_addr_t GCPhys);
static uint32_t remR3HandlerReadU32(void *pvVM, target_phys_addr_t GCPhys);
static void     remR3HandlerWriteU8(void *pvVM, target_phys_addr_t GCPhys, uint32_t u32);
static void     remR3HandlerWriteU16(void *pvVM, target_phys_addr_t GCPhys, uint32_t u32);
static void     remR3HandlerWriteU32(void *pvVM, target_phys_addr_t GCPhys, uint32_t u32);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/

/** @todo Move stats to REM::s some rainy day we have nothing do to. */
#ifdef VBOX_WITH_STATISTICS
static STAMPROFILEADV gStatExecuteSingleInstr;
static STAMPROFILEADV gStatCompilationQEmu;
static STAMPROFILEADV gStatRunCodeQEmu;
static STAMPROFILEADV gStatTotalTimeQEmu;
static STAMPROFILEADV gStatTimers;
static STAMPROFILEADV gStatTBLookup;
static STAMPROFILEADV gStatIRQ;
static STAMPROFILEADV gStatRawCheck;
static STAMPROFILEADV gStatMemRead;
static STAMPROFILEADV gStatMemWrite;
static STAMPROFILE    gStatGCPhys2HCVirt;
static STAMPROFILE    gStatHCVirt2GCPhys;
static STAMCOUNTER    gStatCpuGetTSC;
static STAMCOUNTER    gStatRefuseTFInhibit;
static STAMCOUNTER    gStatRefuseVM86;
static STAMCOUNTER    gStatRefusePaging;
static STAMCOUNTER    gStatRefusePAE;
static STAMCOUNTER    gStatRefuseIOPLNot0;
static STAMCOUNTER    gStatRefuseIF0;
static STAMCOUNTER    gStatRefuseCode16;
static STAMCOUNTER    gStatRefuseWP0;
static STAMCOUNTER    gStatRefuseRing1or2;
static STAMCOUNTER    gStatRefuseCanExecute;
static STAMCOUNTER    gStatREMGDTChange;
static STAMCOUNTER    gStatREMIDTChange;
static STAMCOUNTER    gStatREMLDTRChange;
static STAMCOUNTER    gStatREMTRChange;
static STAMCOUNTER    gStatSelOutOfSync[6];
static STAMCOUNTER    gStatSelOutOfSyncStateBack[6];
#endif

/*
 * Global stuff.
 */

/** MMIO read callbacks. */
CPUReadMemoryFunc  *g_apfnMMIORead[3] =
{
    remR3MMIOReadU8,
    remR3MMIOReadU16,
    remR3MMIOReadU32
};

/** MMIO write callbacks. */
CPUWriteMemoryFunc *g_apfnMMIOWrite[3] =
{
    remR3MMIOWriteU8,
    remR3MMIOWriteU16,
    remR3MMIOWriteU32
};

/** Handler read callbacks. */
CPUReadMemoryFunc  *g_apfnHandlerRead[3] =
{
    remR3HandlerReadU8,
    remR3HandlerReadU16,
    remR3HandlerReadU32
};

/** Handler write callbacks. */
CPUWriteMemoryFunc *g_apfnHandlerWrite[3] =
{
    remR3HandlerWriteU8,
    remR3HandlerWriteU16,
    remR3HandlerWriteU32
};


#if defined(VBOX_WITH_DEBUGGER) && !(defined(RT_OS_WINDWS) && defined(RT_ARCH_AMD64))
/*
 * Debugger commands.
 */
static DECLCALLBACK(int) remR3CmdDisasEnableStepping(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);

/** '.remstep' arguments. */
static const DBGCVARDESC    g_aArgRemStep[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  0,           ~0,         DBGCVAR_CAT_NUMBER,     0,                              "on/off",       "Boolean value/mnemonic indicating the new state." },
};

/** Command descriptors. */
static const DBGCCMD    g_aCmds[] =
{
    {
        .pszCmd ="remstep",
        .cArgsMin = 0,
        .cArgsMax = 1,
        .paArgDescs = &g_aArgRemStep[0],
        .cArgDescs = ELEMENTS(g_aArgRemStep),
        .pResultDesc = NULL,
        .fFlags = 0,
        .pfnHandler = remR3CmdDisasEnableStepping,
        .pszSyntax = "[on/off]",
        .pszDescription = "Enable or disable the single stepping with logged disassembly. "
                          "If no arguments show the current state."
    }
};
#endif


/* Instantiate the structure signatures. */
#define REM_STRUCT_OP 0
#include "Sun/structs.h"



/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void remAbort(int rc, const char *pszTip);
extern int testmath(void);

/* Put them here to avoid unused variable warning. */
AssertCompile(RT_SIZEOFMEMB(VM, rem.padding) >= RT_SIZEOFMEMB(VM, rem.s));
#if !defined(IPRT_NO_CRT) && (defined(RT_OS_LINUX) || defined(RT_OS_DARWIN) || defined(RT_OS_WINDOWS))
//AssertCompileMemberSize(REM, Env, REM_ENV_SIZE);
/* Why did this have to be identical?? */
AssertCompile(RT_SIZEOFMEMB(REM, Env) <= REM_ENV_SIZE);
#else
AssertCompile(RT_SIZEOFMEMB(REM, Env) <= REM_ENV_SIZE);
#endif


/**
 * Initializes the REM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
REMR3DECL(int) REMR3Init(PVM pVM)
{
    uint32_t u32Dummy;
    unsigned i;

    /*
     * Assert sanity.
     */
    AssertReleaseMsg(sizeof(pVM->rem.padding) >= sizeof(pVM->rem.s), ("%#x >= %#x; sizeof(Env)=%#x\n", sizeof(pVM->rem.padding), sizeof(pVM->rem.s), sizeof(pVM->rem.s.Env)));
    AssertReleaseMsg(sizeof(pVM->rem.s.Env) <= REM_ENV_SIZE, ("%#x == %#x\n", sizeof(pVM->rem.s.Env), REM_ENV_SIZE));
    AssertReleaseMsg(!(RT_OFFSETOF(VM, rem) & 31), ("off=%#x\n", RT_OFFSETOF(VM, rem)));
#if defined(DEBUG) && !defined(RT_OS_SOLARIS) /// @todo fix the solaris math stuff.
    Assert(!testmath());
#endif
    ASSERT_STRUCT_TABLE(Misc);
    ASSERT_STRUCT_TABLE(TLB);
    ASSERT_STRUCT_TABLE(SegmentCache);
    ASSERT_STRUCT_TABLE(XMMReg);
    ASSERT_STRUCT_TABLE(MMXReg);
    ASSERT_STRUCT_TABLE(float_status);
    ASSERT_STRUCT_TABLE(float32u);
    ASSERT_STRUCT_TABLE(float64u);
    ASSERT_STRUCT_TABLE(floatx80u);
    ASSERT_STRUCT_TABLE(CPUState);

    /*
     * Init some internal data members.
     */
    pVM->rem.s.offVM = RT_OFFSETOF(VM, rem.s);
    pVM->rem.s.Env.pVM = pVM;
#ifdef CPU_RAW_MODE_INIT
    pVM->rem.s.state |= CPU_RAW_MODE_INIT;
#endif

    /* ctx. */
    int rc = CPUMQueryGuestCtxPtr(pVM, &pVM->rem.s.pCtx);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to obtain guest ctx pointer. rc=%Vrc\n", rc));
        return rc;
    }
    AssertMsg(MMR3PhysGetRamSize(pVM) == 0, ("Init order have changed! REM depends on notification about ALL physical memory registrations\n"));

    /* ignore all notifications */
    pVM->rem.s.fIgnoreAll = true;

    /*
     * Init the recompiler.
     */
    if (!cpu_x86_init(&pVM->rem.s.Env))
    {
        AssertMsgFailed(("cpu_x86_init failed - impossible!\n"));
        return VERR_GENERAL_FAILURE;
    }
    CPUMGetGuestCpuId(pVM,          1, &u32Dummy, &u32Dummy, &pVM->rem.s.Env.cpuid_ext_features, &pVM->rem.s.Env.cpuid_features);
    CPUMGetGuestCpuId(pVM, 0x80000001, &u32Dummy, &u32Dummy, &pVM->rem.s.Env.cpuid_ext3_features, &pVM->rem.s.Env.cpuid_ext2_features);

    /* allocate code buffer for single instruction emulation. */
    pVM->rem.s.Env.cbCodeBuffer = 4096;
    pVM->rem.s.Env.pvCodeBuffer = RTMemExecAlloc(pVM->rem.s.Env.cbCodeBuffer);
    AssertMsgReturn(pVM->rem.s.Env.pvCodeBuffer, ("Failed to allocate code buffer!\n"), VERR_NO_MEMORY);

    /* finally, set the cpu_single_env global. */
    cpu_single_env = &pVM->rem.s.Env;

    /* Nothing is pending by default */
    pVM->rem.s.u32PendingInterrupt = REM_NO_PENDING_IRQ;

    /*
     * Register ram types.
     */
    pVM->rem.s.iMMIOMemType    = cpu_register_io_memory(-1, g_apfnMMIORead, g_apfnMMIOWrite, pVM);
    AssertReleaseMsg(pVM->rem.s.iMMIOMemType >= 0, ("pVM->rem.s.iMMIOMemType=%d\n", pVM->rem.s.iMMIOMemType));
    pVM->rem.s.iHandlerMemType = cpu_register_io_memory(-1, g_apfnHandlerRead, g_apfnHandlerWrite, pVM);
    AssertReleaseMsg(pVM->rem.s.iHandlerMemType >= 0, ("pVM->rem.s.iHandlerMemType=%d\n", pVM->rem.s.iHandlerMemType));
    Log2(("REM: iMMIOMemType=%d iHandlerMemType=%d\n", pVM->rem.s.iMMIOMemType, pVM->rem.s.iHandlerMemType));

    /* stop ignoring. */
    pVM->rem.s.fIgnoreAll = false;

    /*
     * Register the saved state data unit.
     */
    rc = SSMR3RegisterInternal(pVM, "rem", 1, REM_SAVED_STATE_VERSION, sizeof(uint32_t) * 10,
                               NULL, remR3Save, NULL,
                               NULL, remR3Load, NULL);
    if (VBOX_FAILURE(rc))
        return rc;

#if defined(VBOX_WITH_DEBUGGER) && !(defined(RT_OS_WINDOWS) && defined(RT_ARCH_AMD64))
    /*
     * Debugger commands.
     */
    static bool fRegisteredCmds = false;
    if (!fRegisteredCmds)
    {
        int rc = DBGCRegisterCommands(&g_aCmds[0], ELEMENTS(g_aCmds));
        if (VBOX_SUCCESS(rc))
            fRegisteredCmds = true;
    }
#endif

#ifdef VBOX_WITH_STATISTICS
    /*
     * Statistics.
     */
    STAM_REG(pVM, &gStatExecuteSingleInstr, STAMTYPE_PROFILE, "/PROF/REM/SingleInstr",STAMUNIT_TICKS_PER_CALL, "Profiling single instruction emulation.");
    STAM_REG(pVM, &gStatCompilationQEmu,    STAMTYPE_PROFILE, "/PROF/REM/Compile",    STAMUNIT_TICKS_PER_CALL, "Profiling QEmu compilation.");
    STAM_REG(pVM, &gStatRunCodeQEmu,        STAMTYPE_PROFILE, "/PROF/REM/Runcode",    STAMUNIT_TICKS_PER_CALL, "Profiling QEmu code execution.");
    STAM_REG(pVM, &gStatTotalTimeQEmu,      STAMTYPE_PROFILE, "/PROF/REM/Emulate",    STAMUNIT_TICKS_PER_CALL, "Profiling code emulation.");
    STAM_REG(pVM, &gStatTimers,             STAMTYPE_PROFILE, "/PROF/REM/Timers",     STAMUNIT_TICKS_PER_CALL, "Profiling timer scheduling.");
    STAM_REG(pVM, &gStatTBLookup,           STAMTYPE_PROFILE, "/PROF/REM/TBLookup",   STAMUNIT_TICKS_PER_CALL, "Profiling timer scheduling.");
    STAM_REG(pVM, &gStatIRQ,                STAMTYPE_PROFILE, "/PROF/REM/IRQ",        STAMUNIT_TICKS_PER_CALL, "Profiling timer scheduling.");
    STAM_REG(pVM, &gStatRawCheck,           STAMTYPE_PROFILE, "/PROF/REM/RawCheck",   STAMUNIT_TICKS_PER_CALL, "Profiling timer scheduling.");
    STAM_REG(pVM, &gStatMemRead,            STAMTYPE_PROFILE, "/PROF/REM/MemRead",    STAMUNIT_TICKS_PER_CALL, "Profiling memory access.");
    STAM_REG(pVM, &gStatMemWrite,           STAMTYPE_PROFILE, "/PROF/REM/MemWrite",   STAMUNIT_TICKS_PER_CALL, "Profiling memory access.");
    STAM_REG(pVM, &gStatHCVirt2GCPhys,      STAMTYPE_PROFILE, "/PROF/REM/HCVirt2GCPhys", STAMUNIT_TICKS_PER_CALL, "Profiling memory convertion.");
    STAM_REG(pVM, &gStatGCPhys2HCVirt,      STAMTYPE_PROFILE, "/PROF/REM/GCPhys2HCVirt", STAMUNIT_TICKS_PER_CALL, "Profiling memory convertion.");

    STAM_REG(pVM, &gStatCpuGetTSC,          STAMTYPE_COUNTER, "/REM/CpuGetTSC",         STAMUNIT_OCCURENCES,     "cpu_get_tsc calls");

    STAM_REG(pVM, &gStatRefuseTFInhibit,    STAMTYPE_COUNTER, "/REM/Refuse/TFInibit", STAMUNIT_OCCURENCES,     "Raw mode refused because of TF or irq inhibit");
    STAM_REG(pVM, &gStatRefuseVM86,         STAMTYPE_COUNTER, "/REM/Refuse/VM86",     STAMUNIT_OCCURENCES,     "Raw mode refused because of VM86");
    STAM_REG(pVM, &gStatRefusePaging,       STAMTYPE_COUNTER, "/REM/Refuse/Paging",   STAMUNIT_OCCURENCES,     "Raw mode refused because of disabled paging/pm");
    STAM_REG(pVM, &gStatRefusePAE,          STAMTYPE_COUNTER, "/REM/Refuse/PAE",      STAMUNIT_OCCURENCES,     "Raw mode refused because of PAE");
    STAM_REG(pVM, &gStatRefuseIOPLNot0,     STAMTYPE_COUNTER, "/REM/Refuse/IOPLNot0", STAMUNIT_OCCURENCES,     "Raw mode refused because of IOPL != 0");
    STAM_REG(pVM, &gStatRefuseIF0,          STAMTYPE_COUNTER, "/REM/Refuse/IF0",      STAMUNIT_OCCURENCES,     "Raw mode refused because of IF=0");
    STAM_REG(pVM, &gStatRefuseCode16,       STAMTYPE_COUNTER, "/REM/Refuse/Code16",   STAMUNIT_OCCURENCES,     "Raw mode refused because of 16 bit code");
    STAM_REG(pVM, &gStatRefuseWP0,          STAMTYPE_COUNTER, "/REM/Refuse/WP0",      STAMUNIT_OCCURENCES,     "Raw mode refused because of WP=0");
    STAM_REG(pVM, &gStatRefuseRing1or2,     STAMTYPE_COUNTER, "/REM/Refuse/Ring1or2", STAMUNIT_OCCURENCES,     "Raw mode refused because of ring 1/2 execution");
    STAM_REG(pVM, &gStatRefuseCanExecute,   STAMTYPE_COUNTER, "/REM/Refuse/CanExecuteRaw", STAMUNIT_OCCURENCES,     "Raw mode refused because of cCanExecuteRaw");

    STAM_REG(pVM, &gStatREMGDTChange,       STAMTYPE_COUNTER, "/REM/Change/GDTBase",   STAMUNIT_OCCURENCES,     "GDT base changes");
    STAM_REG(pVM, &gStatREMLDTRChange,      STAMTYPE_COUNTER, "/REM/Change/LDTR",      STAMUNIT_OCCURENCES,     "LDTR changes");
    STAM_REG(pVM, &gStatREMIDTChange,       STAMTYPE_COUNTER, "/REM/Change/IDTBase",   STAMUNIT_OCCURENCES,     "IDT base changes");
    STAM_REG(pVM, &gStatREMTRChange,        STAMTYPE_COUNTER, "/REM/Change/TR",        STAMUNIT_OCCURENCES,     "TR selector changes");

    STAM_REG(pVM, &gStatSelOutOfSync[0],    STAMTYPE_COUNTER, "/REM/State/SelOutOfSync/ES",        STAMUNIT_OCCURENCES,     "ES out of sync");
    STAM_REG(pVM, &gStatSelOutOfSync[1],    STAMTYPE_COUNTER, "/REM/State/SelOutOfSync/CS",        STAMUNIT_OCCURENCES,     "CS out of sync");
    STAM_REG(pVM, &gStatSelOutOfSync[2],    STAMTYPE_COUNTER, "/REM/State/SelOutOfSync/SS",        STAMUNIT_OCCURENCES,     "SS out of sync");
    STAM_REG(pVM, &gStatSelOutOfSync[3],    STAMTYPE_COUNTER, "/REM/State/SelOutOfSync/DS",        STAMUNIT_OCCURENCES,     "DS out of sync");
    STAM_REG(pVM, &gStatSelOutOfSync[4],    STAMTYPE_COUNTER, "/REM/State/SelOutOfSync/FS",        STAMUNIT_OCCURENCES,     "FS out of sync");
    STAM_REG(pVM, &gStatSelOutOfSync[5],    STAMTYPE_COUNTER, "/REM/State/SelOutOfSync/GS",        STAMUNIT_OCCURENCES,     "GS out of sync");

    STAM_REG(pVM, &gStatSelOutOfSyncStateBack[0],    STAMTYPE_COUNTER, "/REM/StateBack/SelOutOfSync/ES",        STAMUNIT_OCCURENCES,     "ES out of sync");
    STAM_REG(pVM, &gStatSelOutOfSyncStateBack[1],    STAMTYPE_COUNTER, "/REM/StateBack/SelOutOfSync/CS",        STAMUNIT_OCCURENCES,     "CS out of sync");
    STAM_REG(pVM, &gStatSelOutOfSyncStateBack[2],    STAMTYPE_COUNTER, "/REM/StateBack/SelOutOfSync/SS",        STAMUNIT_OCCURENCES,     "SS out of sync");
    STAM_REG(pVM, &gStatSelOutOfSyncStateBack[3],    STAMTYPE_COUNTER, "/REM/StateBack/SelOutOfSync/DS",        STAMUNIT_OCCURENCES,     "DS out of sync");
    STAM_REG(pVM, &gStatSelOutOfSyncStateBack[4],    STAMTYPE_COUNTER, "/REM/StateBack/SelOutOfSync/FS",        STAMUNIT_OCCURENCES,     "FS out of sync");
    STAM_REG(pVM, &gStatSelOutOfSyncStateBack[5],    STAMTYPE_COUNTER, "/REM/StateBack/SelOutOfSync/GS",        STAMUNIT_OCCURENCES,     "GS out of sync");


#endif

#ifdef DEBUG_ALL_LOGGING
    loglevel = ~0;
#endif

    return rc;
}


/**
 * Terminates the REM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
REMR3DECL(int) REMR3Term(PVM pVM)
{
    return VINF_SUCCESS;
}


/**
 * The VM is being reset.
 *
 * For the REM component this means to call the cpu_reset() and
 * reinitialize some state variables.
 *
 * @param   pVM     VM handle.
 */
REMR3DECL(void) REMR3Reset(PVM pVM)
{
    /*
     * Reset the REM cpu.
     */
    pVM->rem.s.fIgnoreAll = true;
    cpu_reset(&pVM->rem.s.Env);
    pVM->rem.s.cInvalidatedPages = 0;
    pVM->rem.s.fIgnoreAll = false;

    /* Clear raw ring 0 init state */
    pVM->rem.s.Env.state &= ~CPU_RAW_RING0;
}


/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 */
static DECLCALLBACK(int) remR3Save(PVM pVM, PSSMHANDLE pSSM)
{
    LogFlow(("remR3Save:\n"));

    /*
     * Save the required CPU Env bits.
     * (Not much because we're never in REM when doing the save.)
     */
    PREM pRem = &pVM->rem.s;
    Assert(!pRem->fInREM);
    SSMR3PutU32(pSSM,   pRem->Env.hflags);
    SSMR3PutMem(pSSM,   &pRem->Env, RT_OFFSETOF(CPUState, jmp_env));
    SSMR3PutU32(pSSM,   ~0);            /* separator */

    /* Remember if we've entered raw mode (vital for ring 1 checks in e.g. iret emulation). */
    SSMR3PutU32(pSSM, !!(pRem->Env.state & CPU_RAW_RING0));

    /*
     * Save the REM stuff.
     */
    SSMR3PutUInt(pSSM,  pRem->cInvalidatedPages);
    unsigned i;
    for (i = 0; i < pRem->cInvalidatedPages; i++)
        SSMR3PutGCPtr(pSSM, pRem->aGCPtrInvalidatedPages[i]);

    SSMR3PutUInt(pSSM, pVM->rem.s.u32PendingInterrupt);

    return SSMR3PutU32(pSSM, ~0);       /* terminator */
}


/**
 * Execute state load operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 * @param   u32Version      Data layout version.
 */
static DECLCALLBACK(int) remR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version)
{
    uint32_t u32Dummy;
    uint32_t fRawRing0 = false;
    LogFlow(("remR3Load:\n"));

    /*
     * Validate version.
     */
    if (u32Version != REM_SAVED_STATE_VERSION)
    {
        Log(("remR3Load: Invalid version u32Version=%d!\n", u32Version));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    /*
     * Do a reset to be on the safe side...
     */
    REMR3Reset(pVM);

    /*
     * Ignore all ignorable notifications.
     * (Not doing this will cause serious trouble.)
     */
    pVM->rem.s.fIgnoreAll = true;

    /*
     * Load the required CPU Env bits.
     * (Not much because we're never in REM when doing the save.)
     */
    PREM pRem = &pVM->rem.s;
    Assert(!pRem->fInREM);
    SSMR3GetU32(pSSM,   &pRem->Env.hflags);
    SSMR3GetMem(pSSM,   &pRem->Env, RT_OFFSETOF(CPUState, jmp_env));
    uint32_t u32Sep;
    int rc = SSMR3GetU32(pSSM, &u32Sep);            /* separator */
    if (VBOX_FAILURE(rc))
        return rc;
    if (u32Sep != ~0)
    {
        AssertMsgFailed(("u32Sep=%#x\n", u32Sep));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }

    /* Remember if we've entered raw mode (vital for ring 1 checks in e.g. iret emulation). */
    SSMR3GetUInt(pSSM, &fRawRing0);
    if (fRawRing0)
        pRem->Env.state |= CPU_RAW_RING0;

    /*
     * Load the REM stuff.
     */
    rc = SSMR3GetUInt(pSSM, &pRem->cInvalidatedPages);
    if (VBOX_FAILURE(rc))
        return rc;
    if (pRem->cInvalidatedPages > ELEMENTS(pRem->aGCPtrInvalidatedPages))
    {
        AssertMsgFailed(("cInvalidatedPages=%#x\n", pRem->cInvalidatedPages));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }
    unsigned i;
    for (i = 0; i < pRem->cInvalidatedPages; i++)
        SSMR3GetGCPtr(pSSM, &pRem->aGCPtrInvalidatedPages[i]);

    rc = SSMR3GetUInt(pSSM, &pVM->rem.s.u32PendingInterrupt);
    if (VBOX_FAILURE(rc))
        return rc;

    /* check the terminator. */
    rc = SSMR3GetU32(pSSM, &u32Sep);
    if (VBOX_FAILURE(rc))
        return rc;
    if (u32Sep != ~0)
    {
        AssertMsgFailed(("u32Sep=%#x (term)\n", u32Sep));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }

    /*
     * Get the CPUID features.
     */
    CPUMGetGuestCpuId(pVM,          1, &u32Dummy, &u32Dummy, &pVM->rem.s.Env.cpuid_ext_features, &pVM->rem.s.Env.cpuid_features);
    CPUMGetGuestCpuId(pVM, 0x80000001, &u32Dummy, &u32Dummy, &u32Dummy, &pVM->rem.s.Env.cpuid_ext2_features);

    /*
     * Sync the Load Flush the TLB
     */
    tlb_flush(&pRem->Env, 1);

#if 0 /** @todo r=bird: this doesn't make sense. WHY? */
    /*
     * Clear all lazy flags (only FPU sync for now).
     */
    CPUMGetAndClearFPUUsedREM(pVM);
#endif

    /*
     * Stop ignoring ignornable notifications.
     */
    pVM->rem.s.fIgnoreAll = false;

    return VINF_SUCCESS;
}



#undef LOG_GROUP
#define LOG_GROUP LOG_GROUP_REM_RUN

/**
 * Single steps an instruction in recompiled mode.
 *
 * Before calling this function the REM state needs to be in sync with
 * the VM. Call REMR3State() to perform the sync. It's only necessary
 * (and permitted) to sync at the first call to REMR3Step()/REMR3Run()
 * and after calling REMR3StateBack().
 *
 * @returns VBox status code.
 *
 * @param   pVM         VM Handle.
 */
REMR3DECL(int) REMR3Step(PVM pVM)
{
    /*
     * Lock the REM - we don't wanna have anyone interrupting us
     * while stepping - and enabled single stepping. We also ignore
     * pending interrupts and suchlike.
     */
    int interrupt_request = pVM->rem.s.Env.interrupt_request;
    Assert(!(interrupt_request & ~(CPU_INTERRUPT_HARD | CPU_INTERRUPT_EXIT | CPU_INTERRUPT_EXITTB | CPU_INTERRUPT_TIMER  | CPU_INTERRUPT_EXTERNAL_HARD | CPU_INTERRUPT_EXTERNAL_EXIT | CPU_INTERRUPT_EXTERNAL_TIMER)));
    pVM->rem.s.Env.interrupt_request = 0;
    cpu_single_step(&pVM->rem.s.Env, 1);

    /*
     * If we're standing at a breakpoint, that have to be disabled before we start stepping.
     */
    RTGCPTR     GCPtrPC = pVM->rem.s.Env.eip + pVM->rem.s.Env.segs[R_CS].base;
    bool        fBp = !cpu_breakpoint_remove(&pVM->rem.s.Env, GCPtrPC);

    /*
     * Execute and handle the return code.
     * We execute without enabling the cpu tick, so on success we'll
     * just flip it on and off to make sure it moves
     */
    int rc = cpu_exec(&pVM->rem.s.Env);
    if (rc == EXCP_DEBUG)
    {
        TMCpuTickResume(pVM);
        TMCpuTickPause(pVM);
        TMVirtualResume(pVM);
        TMVirtualPause(pVM);
        rc = VINF_EM_DBG_STEPPED;
    }
    else
    {
        AssertMsgFailed(("Damn, this shouldn't happen! cpu_exec returned %d while singlestepping\n", rc));
        switch (rc)
        {
            case EXCP_INTERRUPT:    rc = VINF_SUCCESS; break;
            case EXCP_HLT:
            case EXCP_HALTED:       rc = VINF_EM_HALT; break;
            case EXCP_RC:
                rc = pVM->rem.s.rc;
                pVM->rem.s.rc = VERR_INTERNAL_ERROR;
                break;
            default:
                AssertReleaseMsgFailed(("This really shouldn't happen, rc=%d!\n", rc));
                rc = VERR_INTERNAL_ERROR;
                break;
        }
    }

    /*
     * Restore the stuff we changed to prevent interruption.
     * Unlock the REM.
     */
    if (fBp)
    {
        int rc2 = cpu_breakpoint_insert(&pVM->rem.s.Env, GCPtrPC);
        Assert(rc2 == 0); NOREF(rc2);
    }
    cpu_single_step(&pVM->rem.s.Env, 0);
    pVM->rem.s.Env.interrupt_request = interrupt_request;

    return rc;
}


/**
 * Set a breakpoint using the REM facilities.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   Address         The breakpoint address.
 * @thread  The emulation thread.
 */
REMR3DECL(int) REMR3BreakpointSet(PVM pVM, RTGCUINTPTR Address)
{
    VM_ASSERT_EMT(pVM);
    if (!cpu_breakpoint_insert(&pVM->rem.s.Env, Address))
    {
        LogFlow(("REMR3BreakpointSet: Address=%VGv\n", Address));
        return VINF_SUCCESS;
    }
    LogFlow(("REMR3BreakpointSet: Address=%VGv - failed!\n", Address));
    return VERR_REM_NO_MORE_BP_SLOTS;
}


/**
 * Clears a breakpoint set by REMR3BreakpointSet().
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   Address         The breakpoint address.
 * @thread  The emulation thread.
 */
REMR3DECL(int) REMR3BreakpointClear(PVM pVM, RTGCUINTPTR Address)
{
    VM_ASSERT_EMT(pVM);
    if (!cpu_breakpoint_remove(&pVM->rem.s.Env, Address))
    {
        LogFlow(("REMR3BreakpointClear: Address=%VGv\n", Address));
        return VINF_SUCCESS;
    }
    LogFlow(("REMR3BreakpointClear: Address=%VGv - not found!\n", Address));
    return VERR_REM_BP_NOT_FOUND;
}


/**
 * Emulate an instruction.
 *
 * This function executes one instruction without letting anyone
 * interrupt it. This is intended for being called while being in
 * raw mode and thus will take care of all the state syncing between
 * REM and the rest.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 */
REMR3DECL(int) REMR3EmulateInstruction(PVM pVM)
{
    Log2(("REMR3EmulateInstruction: (cs:eip=%04x:%08x)\n", CPUMGetGuestCS(pVM), CPUMGetGuestEIP(pVM)));

    /*
     * Sync the state and enable single instruction / single stepping.
     */
    int rc = REMR3State(pVM);
    if (VBOX_SUCCESS(rc))
    {
        int interrupt_request = pVM->rem.s.Env.interrupt_request;
        Assert(!(interrupt_request & ~(CPU_INTERRUPT_HARD | CPU_INTERRUPT_EXIT | CPU_INTERRUPT_EXITTB | CPU_INTERRUPT_TIMER | CPU_INTERRUPT_EXTERNAL_HARD | CPU_INTERRUPT_EXTERNAL_EXIT | CPU_INTERRUPT_EXTERNAL_TIMER)));
        Assert(!pVM->rem.s.Env.singlestep_enabled);
#if 1

        /*
         * Now we set the execute single instruction flag and enter the cpu_exec loop.
         */
        pVM->rem.s.Env.interrupt_request = CPU_INTERRUPT_SINGLE_INSTR;
        rc = cpu_exec(&pVM->rem.s.Env);
        switch (rc)
        {
            /*
             * Executed without anything out of the way happening.
             */
            case EXCP_SINGLE_INSTR:
                rc = VINF_EM_RESCHEDULE;
                Log2(("REMR3EmulateInstruction: cpu_exec -> EXCP_SINGLE_INSTR\n"));
                break;

            /*
             * If we take a trap or start servicing a pending interrupt, we might end up here.
             * (Timer thread or some other thread wishing EMT's attention.)
             */
            case EXCP_INTERRUPT:
                Log2(("REMR3EmulateInstruction: cpu_exec -> EXCP_INTERRUPT\n"));
                rc = VINF_EM_RESCHEDULE;
                break;

            /*
             * Single step, we assume!
             * If there was a breakpoint there we're fucked now.
             */
            case EXCP_DEBUG:
            {
                /* breakpoint or single step? */
                RTGCPTR     GCPtrPC = pVM->rem.s.Env.eip + pVM->rem.s.Env.segs[R_CS].base;
                int         iBP;
                rc = VINF_EM_DBG_STEPPED;
                for (iBP = 0; iBP < pVM->rem.s.Env.nb_breakpoints; iBP++)
                    if (pVM->rem.s.Env.breakpoints[iBP] == GCPtrPC)
                    {
                        rc = VINF_EM_DBG_BREAKPOINT;
                        break;
                    }
                Log2(("REMR3EmulateInstruction: cpu_exec -> EXCP_DEBUG rc=%Vrc iBP=%d GCPtrPC=%VGv\n", rc, iBP, GCPtrPC));
                break;
            }

            /*
             * hlt instruction.
             */
            case EXCP_HLT:
                Log2(("REMR3EmulateInstruction: cpu_exec -> EXCP_HLT\n"));
                rc = VINF_EM_HALT;
                break;

            /*
             * The VM has halted.
             */
            case EXCP_HALTED:
                Log2(("REMR3EmulateInstruction: cpu_exec -> EXCP_HALTED\n"));
                rc = VINF_EM_HALT;
                break;

            /*
             * Switch to RAW-mode.
             */
            case EXCP_EXECUTE_RAW:
                Log2(("REMR3EmulateInstruction: cpu_exec -> EXCP_EXECUTE_RAW\n"));
                rc = VINF_EM_RESCHEDULE_RAW;
                break;

            /*
             * Switch to hardware accelerated RAW-mode.
             */
            case EXCP_EXECUTE_HWACC:
                Log2(("REMR3EmulateInstruction: cpu_exec -> EXCP_EXECUTE_HWACC\n"));
                rc = VINF_EM_RESCHEDULE_HWACC;
                break;

            /*
             * An EM RC was raised (VMR3Reset/Suspend/PowerOff/some-fatal-error).
             */
            case EXCP_RC:
                Log2(("REMR3EmulateInstruction: cpu_exec -> EXCP_RC\n"));
                rc = pVM->rem.s.rc;
                pVM->rem.s.rc = VERR_INTERNAL_ERROR;
                break;

            /*
             * Figure out the rest when they arrive....
             */
            default:
                AssertMsgFailed(("rc=%d\n", rc));
                Log2(("REMR3EmulateInstruction: cpu_exec -> %d\n", rc));
                rc = VINF_EM_RESCHEDULE;
                break;
        }

        /*
         * Switch back the state.
         */
#else
        pVM->rem.s.Env.interrupt_request = 0;
        cpu_single_step(&pVM->rem.s.Env, 1);

        /*
         * Execute and handle the return code.
         * We execute without enabling the cpu tick, so on success we'll
         * just flip it on and off to make sure it moves.
         *
         * (We do not use emulate_single_instr() because that doesn't enter the
         * right way in will cause serious trouble if a longjmp was attempted.)
         */
# ifdef DEBUG_bird
        remR3DisasInstr(&pVM->rem.s.Env, 1, "REMR3EmulateInstruction");
# endif
        int cTimesMax = 16384;
        uint32_t eip = pVM->rem.s.Env.eip;
        do
        {
            rc = cpu_exec(&pVM->rem.s.Env);

        } while (   eip == pVM->rem.s.Env.eip
                 && (rc == EXCP_DEBUG || rc == EXCP_EXECUTE_RAW)
                 && --cTimesMax > 0);
        switch (rc)
        {
            /*
             * Single step, we assume!
             * If there was a breakpoint there we're fucked now.
             */
            case EXCP_DEBUG:
            {
                Log2(("REMR3EmulateInstruction: cpu_exec -> EXCP_DEBUG\n"));
                rc = VINF_EM_RESCHEDULE;
                break;
            }

            /*
             * We cannot be interrupted!
             */
            case EXCP_INTERRUPT:
                AssertMsgFailed(("Shouldn't happen! Everything was locked!\n"));
                rc = VERR_INTERNAL_ERROR;
                break;

            /*
             * hlt instruction.
             */
            case EXCP_HLT:
                Log2(("REMR3EmulateInstruction: cpu_exec -> EXCP_HLT\n"));
                rc = VINF_EM_HALT;
                break;

            /*
             * The VM has halted.
             */
            case EXCP_HALTED:
                Log2(("REMR3EmulateInstruction: cpu_exec -> EXCP_HALTED\n"));
                rc = VINF_EM_HALT;
                break;

            /*
             * Switch to RAW-mode.
             */
            case EXCP_EXECUTE_RAW:
                Log2(("REMR3EmulateInstruction: cpu_exec -> EXCP_EXECUTE_RAW\n"));
                rc = VINF_EM_RESCHEDULE_RAW;
                break;

            /*
             * Switch to hardware accelerated RAW-mode.
             */
            case EXCP_EXECUTE_HWACC:
                Log2(("REMR3EmulateInstruction: cpu_exec -> EXCP_EXECUTE_HWACC\n"));
                rc = VINF_EM_RESCHEDULE_HWACC;
                break;

            /*
             * An EM RC was raised (VMR3Reset/Suspend/PowerOff/some-fatal-error).
             */
            case EXCP_RC:
                Log2(("REMR3EmulateInstruction: cpu_exec -> EXCP_RC rc=%Vrc\n", pVM->rem.s.rc));
                rc = pVM->rem.s.rc;
                pVM->rem.s.rc = VERR_INTERNAL_ERROR;
                break;

            /*
             * Figure out the rest when they arrive....
             */
            default:
                AssertMsgFailed(("rc=%d\n", rc));
                Log2(("REMR3EmulateInstruction: cpu_exec -> %d\n", rc));
                rc = VINF_SUCCESS;
                break;
        }

        /*
         * Switch back the state.
         */
        cpu_single_step(&pVM->rem.s.Env, 0);
#endif
        pVM->rem.s.Env.interrupt_request = interrupt_request;
        int rc2 = REMR3StateBack(pVM);
        AssertRC(rc2);
    }

    Log2(("REMR3EmulateInstruction: returns %Vrc (cs:eip=%04x:%08x)\n",
          rc, pVM->rem.s.Env.segs[R_CS].selector, pVM->rem.s.Env.eip));
    return rc;
}


/**
 * Runs code in recompiled mode.
 *
 * Before calling this function the REM state needs to be in sync with
 * the VM. Call REMR3State() to perform the sync. It's only necessary
 * (and permitted) to sync at the first call to REMR3Step()/REMR3Run()
 * and after calling REMR3StateBack().
 *
 * @returns VBox status code.
 *
 * @param   pVM         VM Handle.
 */
REMR3DECL(int) REMR3Run(PVM pVM)
{
    Log2(("REMR3Run: (cs:eip=%04x:%08x)\n", pVM->rem.s.Env.segs[R_CS].selector, pVM->rem.s.Env.eip));
    Assert(pVM->rem.s.fInREM);

    int rc = cpu_exec(&pVM->rem.s.Env);
    switch (rc)
    {
        /*
         * This happens when the execution was interrupted
         * by an external event, like pending timers.
         */
        case EXCP_INTERRUPT:
            Log2(("REMR3Run: cpu_exec -> EXCP_INTERRUPT\n"));
            rc = VINF_SUCCESS;
            break;

        /*
         * hlt instruction.
         */
        case EXCP_HLT:
            Log2(("REMR3Run: cpu_exec -> EXCP_HLT\n"));
            rc = VINF_EM_HALT;
            break;

        /*
         * The VM has halted.
         */
        case EXCP_HALTED:
            Log2(("REMR3Run: cpu_exec -> EXCP_HALTED\n"));
            rc = VINF_EM_HALT;
            break;

        /*
         * Breakpoint/single step.
         */
        case EXCP_DEBUG:
        {
#if 0//def DEBUG_bird
            static int iBP = 0;
            printf("howdy, breakpoint! iBP=%d\n", iBP);
            switch (iBP)
            {
                case 0:
                    cpu_breakpoint_remove(&pVM->rem.s.Env, pVM->rem.s.Env.eip + pVM->rem.s.Env.segs[R_CS].base);
                    pVM->rem.s.Env.state |= CPU_EMULATE_SINGLE_STEP;
                    //pVM->rem.s.Env.interrupt_request = 0;
                    //pVM->rem.s.Env.exception_index = -1;
                    //g_fInterruptDisabled = 1;
                    rc = VINF_SUCCESS;
                    asm("int3");
                    break;
                default:
                    asm("int3");
                    break;
            }
            iBP++;
#else
            /* breakpoint or single step? */
            RTGCPTR     GCPtrPC = pVM->rem.s.Env.eip + pVM->rem.s.Env.segs[R_CS].base;
            int         iBP;
            rc = VINF_EM_DBG_STEPPED;
            for (iBP = 0; iBP < pVM->rem.s.Env.nb_breakpoints; iBP++)
                if (pVM->rem.s.Env.breakpoints[iBP] == GCPtrPC)
                {
                    rc = VINF_EM_DBG_BREAKPOINT;
                    break;
                }
            Log2(("REMR3Run: cpu_exec -> EXCP_DEBUG rc=%Vrc iBP=%d GCPtrPC=%VGv\n", rc, iBP, GCPtrPC));
#endif
            break;
        }

        /*
         * Switch to RAW-mode.
         */
        case EXCP_EXECUTE_RAW:
            Log2(("REMR3Run: cpu_exec -> EXCP_EXECUTE_RAW\n"));
            rc = VINF_EM_RESCHEDULE_RAW;
            break;

        /*
         * Switch to hardware accelerated RAW-mode.
         */
        case EXCP_EXECUTE_HWACC:
            Log2(("REMR3Run: cpu_exec -> EXCP_EXECUTE_HWACC\n"));
            rc = VINF_EM_RESCHEDULE_HWACC;
            break;

        /*
         * An EM RC was raised (VMR3Reset/Suspend/PowerOff/some-fatal-error).
         */
        case EXCP_RC:
            Log2(("REMR3Run: cpu_exec -> EXCP_RC rc=%Vrc\n", pVM->rem.s.rc));
            rc = pVM->rem.s.rc;
            pVM->rem.s.rc = VERR_INTERNAL_ERROR;
            break;

        /*
         * Figure out the rest when they arrive....
         */
        default:
            AssertMsgFailed(("rc=%d\n", rc));
            Log2(("REMR3Run: cpu_exec -> %d\n", rc));
            rc = VINF_SUCCESS;
            break;
    }

    Log2(("REMR3Run: returns %Vrc (cs:eip=%04x:%08x)\n", rc, pVM->rem.s.Env.segs[R_CS].selector, pVM->rem.s.Env.eip));
    return rc;
}


/**
 * Check if the cpu state is suitable for Raw execution.
 *
 * @returns boolean
 * @param   env         The CPU env struct.
 * @param   eip         The EIP to check this for (might differ from env->eip).
 * @param   fFlags      hflags OR'ed with IOPL, TF and VM from eflags.
 * @param   piException Stores EXCP_EXECUTE_RAW/HWACC in case raw mode is supported in this context
 *
 * @remark  This function must be kept in perfect sync with the scheduler in EM.cpp!
 */
bool remR3CanExecuteRaw(CPUState *env, RTGCPTR eip, unsigned fFlags, int *piException)
{
    /* !!! THIS MUST BE IN SYNC WITH emR3Reschedule !!! */
    /* !!! THIS MUST BE IN SYNC WITH emR3Reschedule !!! */
    /* !!! THIS MUST BE IN SYNC WITH emR3Reschedule !!! */

    /* Update counter. */
    env->pVM->rem.s.cCanExecuteRaw++;

    if (HWACCMIsEnabled(env->pVM))
    {
        env->state |= CPU_RAW_HWACC;

        /*
         * Create partial context for HWACCMR3CanExecuteGuest
         */
        CPUMCTX Ctx;
        Ctx.cr0            = env->cr[0];
        Ctx.cr3            = env->cr[3];
        Ctx.cr4            = env->cr[4];

        Ctx.tr             = env->tr.selector;
        Ctx.trHid.u64Base  = env->tr.base;
        Ctx.trHid.u32Limit = env->tr.limit;
        Ctx.trHid.Attr.u   = (env->tr.flags >> 8) & 0xF0FF;

        Ctx.idtr.cbIdt     = env->idt.limit;
        Ctx.idtr.pIdt      = env->idt.base;

        Ctx.eflags.u32     = env->eflags;

        Ctx.cs             = env->segs[R_CS].selector;
        Ctx.csHid.u64Base  = env->segs[R_CS].base;
        Ctx.csHid.u32Limit = env->segs[R_CS].limit;
        Ctx.csHid.Attr.u   = (env->segs[R_CS].flags >> 8) & 0xF0FF;

        Ctx.ss             = env->segs[R_SS].selector;
        Ctx.ssHid.u64Base  = env->segs[R_SS].base;
        Ctx.ssHid.u32Limit = env->segs[R_SS].limit;
        Ctx.ssHid.Attr.u   = (env->segs[R_SS].flags >> 8) & 0xF0FF;

        /* Hardware accelerated raw-mode:
         *
         * Typically only 32-bits protected mode, with paging enabled, code is allowed here.
         */
        if (HWACCMR3CanExecuteGuest(env->pVM, &Ctx) == true)
        {
            *piException = EXCP_EXECUTE_HWACC;
            return true;
        }
        return false;
    }

    /*
     * Here we only support 16 & 32 bits protected mode ring 3 code that has no IO privileges
     * or 32 bits protected mode ring 0 code
     *
     * The tests are ordered by the likelyhood of being true during normal execution.
     */
    if (fFlags & (HF_TF_MASK | HF_INHIBIT_IRQ_MASK))
    {
        STAM_COUNTER_INC(&gStatRefuseTFInhibit);
        Log2(("raw mode refused: fFlags=%#x\n", fFlags));
        return false;
    }

#ifndef VBOX_RAW_V86
    if (fFlags & VM_MASK) {
        STAM_COUNTER_INC(&gStatRefuseVM86);
        Log2(("raw mode refused: VM_MASK\n"));
        return false;
    }
#endif

    if (env->state & CPU_EMULATE_SINGLE_INSTR)
    {
#ifndef DEBUG_bird
        Log2(("raw mode refused: CPU_EMULATE_SINGLE_INSTR\n"));
#endif
        return false;
    }

    if (env->singlestep_enabled)
    {
        //Log2(("raw mode refused: Single step\n"));
        return false;
    }

    if (env->nb_breakpoints > 0)
    {
        //Log2(("raw mode refused: Breakpoints\n"));
        return false;
    }

    uint32_t u32CR0 = env->cr[0];
    if ((u32CR0 & (X86_CR0_PG | X86_CR0_PE)) != (X86_CR0_PG | X86_CR0_PE))
    {
        STAM_COUNTER_INC(&gStatRefusePaging);
        //Log2(("raw mode refused: %s%s%s\n", (u32CR0 & X86_CR0_PG) ? "" : " !PG", (u32CR0 & X86_CR0_PE) ? "" : " !PE", (u32CR0 & X86_CR0_AM) ? "" : " !AM"));
        return false;
    }

    if (env->cr[4] & CR4_PAE_MASK)
    {
        if (!(env->cpuid_features & X86_CPUID_FEATURE_EDX_PAE))
        {
            STAM_COUNTER_INC(&gStatRefusePAE);
            return false;
        }
    }

    if (((fFlags >> HF_CPL_SHIFT) & 3) == 3)
    {
        if (!EMIsRawRing3Enabled(env->pVM))
            return false;

        if (!(env->eflags & IF_MASK))
        {
            STAM_COUNTER_INC(&gStatRefuseIF0);
            Log2(("raw mode refused: IF (RawR3)\n"));
            return false;
        }

        if (!(u32CR0 & CR0_WP_MASK) && EMIsRawRing0Enabled(env->pVM))
        {
            STAM_COUNTER_INC(&gStatRefuseWP0);
            Log2(("raw mode refused: CR0.WP + RawR0\n"));
            return false;
        }
    }
    else
    {
        if (!EMIsRawRing0Enabled(env->pVM))
            return false;

        // Let's start with pure 32 bits ring 0 code first
        if ((fFlags & (HF_SS32_MASK | HF_CS32_MASK)) != (HF_SS32_MASK | HF_CS32_MASK))
        {
            STAM_COUNTER_INC(&gStatRefuseCode16);
            Log2(("raw r0 mode refused: HF_[S|C]S32_MASK fFlags=%#x\n", fFlags));
            return false;
        }

        // Only R0
        if (((fFlags >> HF_CPL_SHIFT) & 3) != 0)
        {
            STAM_COUNTER_INC(&gStatRefuseRing1or2);
            Log2(("raw r0 mode refused: CPL %d\n", ((fFlags >> HF_CPL_SHIFT) & 3) ));
            return false;
        }

        if (!(u32CR0 & CR0_WP_MASK))
        {
            STAM_COUNTER_INC(&gStatRefuseWP0);
            Log2(("raw r0 mode refused: CR0.WP=0!\n"));
            return false;
        }

        if (PATMIsPatchGCAddr(env->pVM, eip))
        {
            Log2(("raw r0 mode forced: patch code\n"));
            *piException = EXCP_EXECUTE_RAW;
            return true;
        }

#if !defined(VBOX_ALLOW_IF0) && !defined(VBOX_RUN_INTERRUPT_GATE_HANDLERS)
        if (!(env->eflags & IF_MASK))
        {
            STAM_COUNTER_INC(&gStatRefuseIF0);
            ////Log2(("R0: IF=0 VIF=%d %08X\n", eip, *env->pVMeflags));
            //Log2(("RR0: Interrupts turned off; fall back to emulation\n"));
            return false;
        }
#endif

        env->state |= CPU_RAW_RING0;
    }

    /*
     * Don't reschedule the first time we're called, because there might be
     * special reasons why we're here that is not covered by the above checks.
     */
    if (env->pVM->rem.s.cCanExecuteRaw == 1)
    {
        Log2(("raw mode refused: first scheduling\n"));
        STAM_COUNTER_INC(&gStatRefuseCanExecute);
        return false;
    }

    Assert(PGMPhysIsA20Enabled(env->pVM));
    *piException = EXCP_EXECUTE_RAW;
    return true;
}


/**
 * Fetches a code byte.
 *
 * @returns Success indicator (bool) for ease of use.
 * @param   env         The CPU environment structure.
 * @param   GCPtrInstr  Where to fetch code.
 * @param   pu8Byte     Where to store the byte on success
 */
bool remR3GetOpcode(CPUState *env, RTGCPTR GCPtrInstr, uint8_t *pu8Byte)
{
    int rc = PATMR3QueryOpcode(env->pVM, GCPtrInstr, pu8Byte);
    if (VBOX_SUCCESS(rc))
        return true;
    return false;
}


/**
 * Flush (or invalidate if you like) page table/dir entry.
 *
 * (invlpg instruction; tlb_flush_page)
 *
 * @param   env         Pointer to cpu environment.
 * @param   GCPtr       The virtual address which page table/dir entry should be invalidated.
 */
void remR3FlushPage(CPUState *env, RTGCPTR GCPtr)
{
    PVM pVM = env->pVM;

    /*
     * When we're replaying invlpg instructions or restoring a saved
     * state we disable this path.
     */
    if (pVM->rem.s.fIgnoreInvlPg || pVM->rem.s.fIgnoreAll)
        return;
    Log(("remR3FlushPage: GCPtr=%VGv\n", GCPtr));
    Assert(pVM->rem.s.fInREM || pVM->rem.s.fInStateSync);

    //RAWEx_ProfileStop(env, STATS_QEMU_TOTAL);

    /*
     * Update the control registers before calling PGMFlushPage.
     */
    PCPUMCTX pCtx = (PCPUMCTX)pVM->rem.s.pCtx;
    pCtx->cr0 = env->cr[0];
    pCtx->cr3 = env->cr[3];
    pCtx->cr4 = env->cr[4];

    /*
     * Let PGM do the rest.
     */
    int rc = PGMInvalidatePage(pVM, GCPtr);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("remR3FlushPage %VGv failed with %d!!\n", GCPtr, rc));
        VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
    }
    //RAWEx_ProfileStart(env, STATS_QEMU_TOTAL);
}


/**
 * Called from tlb_protect_code in order to write monitor a code page.
 *
 * @param   env             Pointer to the CPU environment.
 * @param   GCPtr           Code page to monitor
 */
void remR3ProtectCode(CPUState *env, RTGCPTR GCPtr)
{
    Assert(env->pVM->rem.s.fInREM);
    if (     (env->cr[0] & X86_CR0_PG)                      /* paging must be enabled */
        &&  !(env->state & CPU_EMULATE_SINGLE_INSTR)        /* ignore during single instruction execution */
        &&   (((env->hflags >> HF_CPL_SHIFT) & 3) == 0)     /* supervisor mode only */
        &&  !(env->eflags & VM_MASK)                        /* no V86 mode */
        &&  !HWACCMIsEnabled(env->pVM))
        CSAMR3MonitorPage(env->pVM, GCPtr, CSAM_TAG_REM);
}

/**
 * Called from tlb_unprotect_code in order to clear write monitoring for a code page.
 *
 * @param   env             Pointer to the CPU environment.
 * @param   GCPtr           Code page to monitor
 */
void remR3UnprotectCode(CPUState *env, RTGCPTR GCPtr)
{
    Assert(env->pVM->rem.s.fInREM);
    if (     (env->cr[0] & X86_CR0_PG)                      /* paging must be enabled */
        &&  !(env->state & CPU_EMULATE_SINGLE_INSTR)        /* ignore during single instruction execution */
        &&   (((env->hflags >> HF_CPL_SHIFT) & 3) == 0)     /* supervisor mode only */
        &&  !(env->eflags & VM_MASK)                        /* no V86 mode */
        &&  !HWACCMIsEnabled(env->pVM))
        CSAMR3UnmonitorPage(env->pVM, GCPtr, CSAM_TAG_REM);
}


/**
 * Called when the CPU is initialized, any of the CRx registers are changed or
 * when the A20 line is modified.
 *
 * @param   env             Pointer to the CPU environment.
 * @param   fGlobal         Set if the flush is global.
 */
void remR3FlushTLB(CPUState *env, bool fGlobal)
{
    PVM pVM = env->pVM;

    /*
     * When we're replaying invlpg instructions or restoring a saved
     * state we disable this path.
     */
    if (pVM->rem.s.fIgnoreCR3Load || pVM->rem.s.fIgnoreAll)
        return;
    Assert(pVM->rem.s.fInREM);

    /*
     * The caller doesn't check cr4, so we have to do that for ourselves.
     */
    if (!fGlobal && !(env->cr[4] & X86_CR4_PGE))
        fGlobal = true;
    Log(("remR3FlushTLB: CR0=%VGp CR3=%VGp CR4=%VGp %s\n", env->cr[0], env->cr[3], env->cr[4], fGlobal ? " global" : ""));

    /*
     * Update the control registers before calling PGMR3FlushTLB.
     */
    PCPUMCTX pCtx = (PCPUMCTX)pVM->rem.s.pCtx;
    pCtx->cr0 = env->cr[0];
    pCtx->cr3 = env->cr[3];
    pCtx->cr4 = env->cr[4];

    /*
     * Let PGM do the rest.
     */
    PGMFlushTLB(pVM, env->cr[3], fGlobal);
}


/**
 * Called when any of the cr0, cr4 or efer registers is updated.
 *
 * @param   env     Pointer to the CPU environment.
 */
void remR3ChangeCpuMode(CPUState *env)
{
    int rc;
    PVM pVM = env->pVM;

    /*
     * When we're replaying loads or restoring a saved
     * state this path is disabled.
     */
    if (pVM->rem.s.fIgnoreCpuMode || pVM->rem.s.fIgnoreAll)
        return;
    Assert(pVM->rem.s.fInREM);

    /*
     * Update the control registers before calling PGMChangeMode()
     * as it may need to map whatever cr3 is pointing to.
     */
    PCPUMCTX pCtx = (PCPUMCTX)pVM->rem.s.pCtx;
    pCtx->cr0 = env->cr[0];
    pCtx->cr3 = env->cr[3];
    pCtx->cr4 = env->cr[4];

#ifdef TARGET_X86_64
    rc = PGMChangeMode(pVM, env->cr[0], env->cr[4], env->efer);
    if (rc != VINF_SUCCESS)
        cpu_abort(env, "PGMChangeMode(, %RX64, %RX64, %RX64) -> %Vrc\n", env->cr[0], env->cr[4], env->efer, rc);
#else
    rc = PGMChangeMode(pVM, env->cr[0], env->cr[4], 0);
    if (rc != VINF_SUCCESS)
        cpu_abort(env, "PGMChangeMode(, %RX64, %RX64, %RX64) -> %Vrc\n", env->cr[0], env->cr[4], 0LL, rc);
#endif
}


/**
 * Called from compiled code to run dma.
 *
 * @param   env             Pointer to the CPU environment.
 */
void remR3DmaRun(CPUState *env)
{
    remR3ProfileStop(STATS_QEMU_RUN_EMULATED_CODE);
    PDMR3DmaRun(env->pVM);
    remR3ProfileStart(STATS_QEMU_RUN_EMULATED_CODE);
}


/**
 * Called from compiled code to schedule pending timers in VMM
 *
 * @param   env             Pointer to the CPU environment.
 */
void remR3TimersRun(CPUState *env)
{
    remR3ProfileStop(STATS_QEMU_RUN_EMULATED_CODE);
    remR3ProfileStart(STATS_QEMU_RUN_TIMERS);
    TMR3TimerQueuesDo(env->pVM);
    remR3ProfileStop(STATS_QEMU_RUN_TIMERS);
    remR3ProfileStart(STATS_QEMU_RUN_EMULATED_CODE);
}


/**
 * Record trap occurance
 *
 * @returns VBox status code
 * @param   env             Pointer to the CPU environment.
 * @param   uTrap           Trap nr
 * @param   uErrorCode      Error code
 * @param   pvNextEIP       Next EIP
 */
int remR3NotifyTrap(CPUState *env, uint32_t uTrap, uint32_t uErrorCode, uint32_t pvNextEIP)
{
    PVM pVM = env->pVM;
#ifdef VBOX_WITH_STATISTICS
    static STAMCOUNTER s_aStatTrap[255];
    static bool        s_aRegisters[RT_ELEMENTS(s_aStatTrap)];
#endif

#ifdef VBOX_WITH_STATISTICS
    if (uTrap < 255)
    {
        if (!s_aRegisters[uTrap])
        {
            s_aRegisters[uTrap] = true;
            char szStatName[64];
            RTStrPrintf(szStatName, sizeof(szStatName), "/REM/Trap/0x%02X", uTrap);
            STAM_REG(env->pVM, &s_aStatTrap[uTrap], STAMTYPE_COUNTER, szStatName, STAMUNIT_OCCURENCES, "Trap stats.");
        }
        STAM_COUNTER_INC(&s_aStatTrap[uTrap]);
    }
#endif
    Log(("remR3NotifyTrap: uTrap=%x error=%x next_eip=%VGv eip=%VGv cr2=%08x\n", uTrap, uErrorCode, pvNextEIP, env->eip, env->cr[2]));
    if(   uTrap < 0x20
       && (env->cr[0] & X86_CR0_PE)
       && !(env->eflags & X86_EFL_VM))
    {
#ifdef DEBUG
        remR3DisasInstr(env, 1, "remR3NotifyTrap: ");
#endif
        if(pVM->rem.s.uPendingException == uTrap && ++pVM->rem.s.cPendingExceptions > 512)
        {
            LogRel(("VERR_REM_TOO_MANY_TRAPS -> uTrap=%x error=%x next_eip=%VGv eip=%VGv cr2=%08x\n", uTrap, uErrorCode, pvNextEIP, env->eip, env->cr[2]));
            remR3RaiseRC(env->pVM, VERR_REM_TOO_MANY_TRAPS);
            return VERR_REM_TOO_MANY_TRAPS;
        }
        if(pVM->rem.s.uPendingException != uTrap || pVM->rem.s.uPendingExcptEIP != env->eip || pVM->rem.s.uPendingExcptCR2 != env->cr[2])
            pVM->rem.s.cPendingExceptions = 1;
        pVM->rem.s.uPendingException = uTrap;
        pVM->rem.s.uPendingExcptEIP  = env->eip;
        pVM->rem.s.uPendingExcptCR2  = env->cr[2];
    }
    else
    {
        pVM->rem.s.cPendingExceptions = 0;
        pVM->rem.s.uPendingException = uTrap;
        pVM->rem.s.uPendingExcptEIP  = env->eip;
        pVM->rem.s.uPendingExcptCR2  = env->cr[2];
    }
    return VINF_SUCCESS;
}


/*
 * Clear current active trap
 *
 * @param   pVM         VM Handle.
 */
void remR3TrapClear(PVM pVM)
{
    pVM->rem.s.cPendingExceptions = 0;
    pVM->rem.s.uPendingException  = 0;
    pVM->rem.s.uPendingExcptEIP   = 0;
    pVM->rem.s.uPendingExcptCR2   = 0;
}


/*
 * Record previous call instruction addresses
 *
 * @param   env             Pointer to the CPU environment.
 */
void remR3RecordCall(CPUState *env)
{
    CSAMR3RecordCallAddress(env->pVM, env->eip);
}


/**
 * Syncs the internal REM state with the VM.
 *
 * This must be called before REMR3Run() is invoked whenever when the REM
 * state is not up to date. Calling it several times in a row is not
 * permitted.
 *
 * @returns VBox status code.
 *
 * @param   pVM         VM Handle.
 *
 * @remark  The caller has to check for important FFs before calling REMR3Run. REMR3State will
 *          no do this since the majority of the callers don't want any unnecessary of events
 *          pending that would immediatly interrupt execution.
 */
REMR3DECL(int) REMR3State(PVM pVM)
{
    Log2(("REMR3State:\n"));
    STAM_PROFILE_START(&pVM->rem.s.StatsState, a);
    register const CPUMCTX *pCtx = pVM->rem.s.pCtx;
    register unsigned       fFlags;
    bool                    fHiddenSelRegsValid = CPUMAreHiddenSelRegsValid(pVM);

    Assert(!pVM->rem.s.fInREM);
    pVM->rem.s.fInStateSync = true;

    /*
     * Copy the registers which require no special handling.
     */
#ifdef TARGET_X86_64
    /* Note that the high dwords of 64 bits registers are undefined in 32 bits mode and are undefined after a mode change. */
    Assert(R_EAX == 0);
    pVM->rem.s.Env.regs[R_EAX]  = pCtx->rax;
    Assert(R_ECX == 1);
    pVM->rem.s.Env.regs[R_ECX]  = pCtx->rcx;
    Assert(R_EDX == 2);
    pVM->rem.s.Env.regs[R_EDX]  = pCtx->rdx;
    Assert(R_EBX == 3);
    pVM->rem.s.Env.regs[R_EBX]  = pCtx->rbx;
    Assert(R_ESP == 4);
    pVM->rem.s.Env.regs[R_ESP]  = pCtx->rsp;
    Assert(R_EBP == 5);
    pVM->rem.s.Env.regs[R_EBP]  = pCtx->rbp;
    Assert(R_ESI == 6);
    pVM->rem.s.Env.regs[R_ESI]  = pCtx->rsi;
    Assert(R_EDI == 7);
    pVM->rem.s.Env.regs[R_EDI]  = pCtx->rdi;
    pVM->rem.s.Env.regs[8]      = pCtx->r8;
    pVM->rem.s.Env.regs[9]      = pCtx->r9;
    pVM->rem.s.Env.regs[10]     = pCtx->r10;
    pVM->rem.s.Env.regs[11]     = pCtx->r11;
    pVM->rem.s.Env.regs[12]     = pCtx->r12;
    pVM->rem.s.Env.regs[13]     = pCtx->r13;
    pVM->rem.s.Env.regs[14]     = pCtx->r14;
    pVM->rem.s.Env.regs[15]     = pCtx->r15;

    pVM->rem.s.Env.eip          = pCtx->rip;

    pVM->rem.s.Env.eflags       = pCtx->rflags.u64;
#else
    Assert(R_EAX == 0);
    pVM->rem.s.Env.regs[R_EAX]  = pCtx->eax;
    Assert(R_ECX == 1);
    pVM->rem.s.Env.regs[R_ECX]  = pCtx->ecx;
    Assert(R_EDX == 2);
    pVM->rem.s.Env.regs[R_EDX]  = pCtx->edx;
    Assert(R_EBX == 3);
    pVM->rem.s.Env.regs[R_EBX]  = pCtx->ebx;
    Assert(R_ESP == 4);
    pVM->rem.s.Env.regs[R_ESP]  = pCtx->esp;
    Assert(R_EBP == 5);
    pVM->rem.s.Env.regs[R_EBP]  = pCtx->ebp;
    Assert(R_ESI == 6);
    pVM->rem.s.Env.regs[R_ESI]  = pCtx->esi;
    Assert(R_EDI == 7);
    pVM->rem.s.Env.regs[R_EDI]  = pCtx->edi;
    pVM->rem.s.Env.eip          = pCtx->eip;

    pVM->rem.s.Env.eflags       = pCtx->eflags.u32;
#endif

    pVM->rem.s.Env.cr[2]        = pCtx->cr2;

    /** @todo we could probably benefit from using a CPUM_CHANGED_DRx flag too! */
    pVM->rem.s.Env.dr[0]        = pCtx->dr0;
    pVM->rem.s.Env.dr[1]        = pCtx->dr1;
    pVM->rem.s.Env.dr[2]        = pCtx->dr2;
    pVM->rem.s.Env.dr[3]        = pCtx->dr3;
    pVM->rem.s.Env.dr[4]        = pCtx->dr4;
    pVM->rem.s.Env.dr[5]        = pCtx->dr5;
    pVM->rem.s.Env.dr[6]        = pCtx->dr6;
    pVM->rem.s.Env.dr[7]        = pCtx->dr7;

    /*
     * Clear the halted hidden flag (the interrupt waking up the CPU can
     * have been dispatched in raw mode).
     */
    pVM->rem.s.Env.hflags      &= ~HF_HALTED_MASK;

    /*
     * Replay invlpg?
     */
    if (pVM->rem.s.cInvalidatedPages)
    {
        pVM->rem.s.fIgnoreInvlPg = true;
        RTUINT i;
        for (i = 0; i < pVM->rem.s.cInvalidatedPages; i++)
        {
            Log2(("REMR3State: invlpg %VGv\n", pVM->rem.s.aGCPtrInvalidatedPages[i]));
            tlb_flush_page(&pVM->rem.s.Env, pVM->rem.s.aGCPtrInvalidatedPages[i]);
        }
        pVM->rem.s.fIgnoreInvlPg = false;
        pVM->rem.s.cInvalidatedPages = 0;
    }

    /* Update MSRs; before CRx registers! */
    pVM->rem.s.Env.efer         = pCtx->msrEFER;
    pVM->rem.s.Env.star         = pCtx->msrSTAR;
    pVM->rem.s.Env.pat          = pCtx->msrPAT;
#ifdef TARGET_X86_64
    pVM->rem.s.Env.lstar        = pCtx->msrLSTAR;
    pVM->rem.s.Env.cstar        = pCtx->msrCSTAR;
    pVM->rem.s.Env.fmask        = pCtx->msrSFMASK;
    pVM->rem.s.Env.kernelgsbase = pCtx->msrKERNELGSBASE;
#endif


    /*
     * Registers which are rarely changed and require special handling / order when changed.
     */
    fFlags = CPUMGetAndClearChangedFlagsREM(pVM);
    if (fFlags & (  CPUM_CHANGED_CR4  | CPUM_CHANGED_CR3  | CPUM_CHANGED_CR0
                  | CPUM_CHANGED_GDTR | CPUM_CHANGED_IDTR | CPUM_CHANGED_LDTR | CPUM_CHANGED_TR
                  | CPUM_CHANGED_FPU_REM | CPUM_CHANGED_SYSENTER_MSR | CPUM_CHANGED_CPUID))
    {
        if (fFlags & CPUM_CHANGED_FPU_REM)
            save_raw_fp_state(&pVM->rem.s.Env, (uint8_t *)&pCtx->fpu); /* 'save' is an excellent name. */

        if (fFlags & CPUM_CHANGED_GLOBAL_TLB_FLUSH)
        {
            pVM->rem.s.fIgnoreCR3Load = true;
            tlb_flush(&pVM->rem.s.Env, true);
            pVM->rem.s.fIgnoreCR3Load = false;
        }

        if (fFlags & CPUM_CHANGED_CR4)
        {
            pVM->rem.s.fIgnoreCR3Load = true;
            pVM->rem.s.fIgnoreCpuMode = true;
            cpu_x86_update_cr4(&pVM->rem.s.Env, pCtx->cr4);
            pVM->rem.s.fIgnoreCpuMode = false;
            pVM->rem.s.fIgnoreCR3Load = false;
        }

        if (fFlags & CPUM_CHANGED_CR0)
        {
            pVM->rem.s.fIgnoreCR3Load = true;
            pVM->rem.s.fIgnoreCpuMode = true;
            cpu_x86_update_cr0(&pVM->rem.s.Env, pCtx->cr0);
            pVM->rem.s.fIgnoreCpuMode = false;
            pVM->rem.s.fIgnoreCR3Load = false;
        }

        if (fFlags & CPUM_CHANGED_CR3)
        {
            pVM->rem.s.fIgnoreCR3Load = true;
            cpu_x86_update_cr3(&pVM->rem.s.Env, pCtx->cr3);
            pVM->rem.s.fIgnoreCR3Load = false;
        }

        if (fFlags & CPUM_CHANGED_GDTR)
        {
            pVM->rem.s.Env.gdt.base     = pCtx->gdtr.pGdt;
            pVM->rem.s.Env.gdt.limit    = pCtx->gdtr.cbGdt;
        }

        if (fFlags & CPUM_CHANGED_IDTR)
        {
            pVM->rem.s.Env.idt.base     = pCtx->idtr.pIdt;
            pVM->rem.s.Env.idt.limit    = pCtx->idtr.cbIdt;
        }

        if (fFlags & CPUM_CHANGED_SYSENTER_MSR)
        {
            pVM->rem.s.Env.sysenter_cs  = pCtx->SysEnter.cs;
            pVM->rem.s.Env.sysenter_eip = pCtx->SysEnter.eip;
            pVM->rem.s.Env.sysenter_esp = pCtx->SysEnter.esp;
        }

        if (fFlags & CPUM_CHANGED_LDTR)
        {
            if (fHiddenSelRegsValid)
            {
                pVM->rem.s.Env.ldt.selector = pCtx->ldtr;
                pVM->rem.s.Env.ldt.base     = pCtx->ldtrHid.u64Base;
                pVM->rem.s.Env.ldt.limit    = pCtx->ldtrHid.u32Limit;
                pVM->rem.s.Env.ldt.flags    = (pCtx->ldtrHid.Attr.u << 8) & 0xFFFFFF;;
            }
            else
                sync_ldtr(&pVM->rem.s.Env, pCtx->ldtr);
        }

        if (fFlags & CPUM_CHANGED_TR)
        {
            if (fHiddenSelRegsValid)
            {
                pVM->rem.s.Env.tr.selector = pCtx->tr;
                pVM->rem.s.Env.tr.base     = pCtx->trHid.u64Base;
                pVM->rem.s.Env.tr.limit    = pCtx->trHid.u32Limit;
                pVM->rem.s.Env.tr.flags    = (pCtx->trHid.Attr.u << 8) & 0xFFFFFF;;
            }
            else
                sync_tr(&pVM->rem.s.Env, pCtx->tr);

            /** @note do_interrupt will fault if the busy flag is still set.... */
            pVM->rem.s.Env.tr.flags &= ~DESC_TSS_BUSY_MASK;
        }

        if (fFlags & CPUM_CHANGED_CPUID)
        {
            uint32_t u32Dummy;

            /*
            * Get the CPUID features.
            */
            CPUMGetGuestCpuId(pVM,          1, &u32Dummy, &u32Dummy, &pVM->rem.s.Env.cpuid_ext_features, &pVM->rem.s.Env.cpuid_features);
            CPUMGetGuestCpuId(pVM, 0x80000001, &u32Dummy, &u32Dummy, &u32Dummy, &pVM->rem.s.Env.cpuid_ext2_features);
        }
    }

    /*
     * Update selector registers.
     * This must be done *after* we've synced gdt, ldt and crX registers
     * since we're reading the GDT/LDT om sync_seg. This will happen with
     * saved state which takes a quick dip into rawmode for instance.
     */
    /*
     * Stack; Note first check this one as the CPL might have changed. The
     * wrong CPL can cause QEmu to raise an exception in sync_seg!!
     */

    if (fHiddenSelRegsValid)
    {
        /* The hidden selector registers are valid in the CPU context. */
        /** @note QEmu saves the 2nd dword of the descriptor; we should convert the attribute word back! */

        /* Set current CPL */
        cpu_x86_set_cpl(&pVM->rem.s.Env, CPUMGetGuestCPL(pVM, CPUMCTX2CORE(pCtx)));

        cpu_x86_load_seg_cache(&pVM->rem.s.Env, R_CS, pCtx->cs, pCtx->csHid.u64Base, pCtx->csHid.u32Limit, (pCtx->csHid.Attr.u << 8) & 0xFFFFFF);
        cpu_x86_load_seg_cache(&pVM->rem.s.Env, R_SS, pCtx->ss, pCtx->ssHid.u64Base, pCtx->ssHid.u32Limit, (pCtx->ssHid.Attr.u << 8) & 0xFFFFFF);
        cpu_x86_load_seg_cache(&pVM->rem.s.Env, R_DS, pCtx->ds, pCtx->dsHid.u64Base, pCtx->dsHid.u32Limit, (pCtx->dsHid.Attr.u << 8) & 0xFFFFFF);
        cpu_x86_load_seg_cache(&pVM->rem.s.Env, R_ES, pCtx->es, pCtx->esHid.u64Base, pCtx->esHid.u32Limit, (pCtx->esHid.Attr.u << 8) & 0xFFFFFF);

        /* FS & GS base addresses need to be loaded from the MSRs if in 64 bits mode. */
        if (CPUMIsGuestIn64BitCode(pVM, CPUMCTX2CORE(pCtx)))
        {
            /* Note that the base values in the hidden fs & gs registers are cut to 32 bits and can't be used in this case. */
            cpu_x86_load_seg_cache(&pVM->rem.s.Env, R_FS, pCtx->fs, pCtx->msrFSBASE, pCtx->fsHid.u32Limit, (pCtx->fsHid.Attr.u << 8) & 0xFFFFFF);
            cpu_x86_load_seg_cache(&pVM->rem.s.Env, R_GS, pCtx->gs, pCtx->msrGSBASE, pCtx->gsHid.u32Limit, (pCtx->gsHid.Attr.u << 8) & 0xFFFFFF);
        }
        else
        {
            cpu_x86_load_seg_cache(&pVM->rem.s.Env, R_FS, pCtx->fs, pCtx->fsHid.u64Base, pCtx->fsHid.u32Limit, (pCtx->fsHid.Attr.u << 8) & 0xFFFFFF);
            cpu_x86_load_seg_cache(&pVM->rem.s.Env, R_GS, pCtx->gs, pCtx->gsHid.u64Base, pCtx->gsHid.u32Limit, (pCtx->gsHid.Attr.u << 8) & 0xFFFFFF);
        }
    }
    else
    {
        /* In 'normal' raw mode we don't have access to the hidden selector registers. */
        if (pVM->rem.s.Env.segs[R_SS].selector != (uint16_t)pCtx->ss)
        {
            Log2(("REMR3State: SS changed from %04x to %04x!\n", pVM->rem.s.Env.segs[R_SS].selector, pCtx->ss));

            cpu_x86_set_cpl(&pVM->rem.s.Env, (pCtx->eflags.Bits.u1VM) ? 3 : (pCtx->ss & 3));
            sync_seg(&pVM->rem.s.Env, R_SS, pCtx->ss);
#ifdef VBOX_WITH_STATISTICS
            if (pVM->rem.s.Env.segs[R_SS].newselector)
            {
                STAM_COUNTER_INC(&gStatSelOutOfSync[R_SS]);
            }
#endif
        }
        else
            pVM->rem.s.Env.segs[R_SS].newselector = 0;

        if (pVM->rem.s.Env.segs[R_ES].selector != pCtx->es)
        {
            Log2(("REMR3State: ES changed from %04x to %04x!\n", pVM->rem.s.Env.segs[R_ES].selector, pCtx->es));
            sync_seg(&pVM->rem.s.Env, R_ES, pCtx->es);
#ifdef VBOX_WITH_STATISTICS
            if (pVM->rem.s.Env.segs[R_ES].newselector)
            {
                STAM_COUNTER_INC(&gStatSelOutOfSync[R_ES]);
            }
#endif
        }
        else
            pVM->rem.s.Env.segs[R_ES].newselector = 0;

        if (pVM->rem.s.Env.segs[R_CS].selector != pCtx->cs)
        {
            Log2(("REMR3State: CS changed from %04x to %04x!\n", pVM->rem.s.Env.segs[R_CS].selector, pCtx->cs));
            sync_seg(&pVM->rem.s.Env, R_CS, pCtx->cs);
#ifdef VBOX_WITH_STATISTICS
            if (pVM->rem.s.Env.segs[R_CS].newselector)
            {
                STAM_COUNTER_INC(&gStatSelOutOfSync[R_CS]);
            }
#endif
        }
        else
            pVM->rem.s.Env.segs[R_CS].newselector = 0;

        if (pVM->rem.s.Env.segs[R_DS].selector != pCtx->ds)
        {
            Log2(("REMR3State: DS changed from %04x to %04x!\n", pVM->rem.s.Env.segs[R_DS].selector, pCtx->ds));
            sync_seg(&pVM->rem.s.Env, R_DS, pCtx->ds);
#ifdef VBOX_WITH_STATISTICS
            if (pVM->rem.s.Env.segs[R_DS].newselector)
            {
                STAM_COUNTER_INC(&gStatSelOutOfSync[R_DS]);
            }
#endif
        }
        else
            pVM->rem.s.Env.segs[R_DS].newselector = 0;

    /** @todo need to find a way to communicate potential GDT/LDT changes and thread switches. The selector might
     * be the same but not the base/limit. */
        if (pVM->rem.s.Env.segs[R_FS].selector != pCtx->fs)
        {
            Log2(("REMR3State: FS changed from %04x to %04x!\n", pVM->rem.s.Env.segs[R_FS].selector, pCtx->fs));
            sync_seg(&pVM->rem.s.Env, R_FS, pCtx->fs);
#ifdef VBOX_WITH_STATISTICS
            if (pVM->rem.s.Env.segs[R_FS].newselector)
            {
                STAM_COUNTER_INC(&gStatSelOutOfSync[R_FS]);
            }
#endif
        }
        else
            pVM->rem.s.Env.segs[R_FS].newselector = 0;

        if (pVM->rem.s.Env.segs[R_GS].selector != pCtx->gs)
        {
            Log2(("REMR3State: GS changed from %04x to %04x!\n", pVM->rem.s.Env.segs[R_GS].selector, pCtx->gs));
            sync_seg(&pVM->rem.s.Env, R_GS, pCtx->gs);
#ifdef VBOX_WITH_STATISTICS
            if (pVM->rem.s.Env.segs[R_GS].newselector)
            {
                STAM_COUNTER_INC(&gStatSelOutOfSync[R_GS]);
            }
#endif
        }
        else
            pVM->rem.s.Env.segs[R_GS].newselector = 0;
    }

    /*
     * Check for traps.
     */
    pVM->rem.s.Env.exception_index = -1; /** @todo this won't work :/ */
    TRPMEVENT   enmType;
    uint8_t     u8TrapNo;
    int rc = TRPMQueryTrap(pVM, &u8TrapNo, &enmType);
    if (VBOX_SUCCESS(rc))
    {
#ifdef DEBUG
        if (u8TrapNo == 0x80)
        {
            remR3DumpLnxSyscall(pVM);
            remR3DumpOBsdSyscall(pVM);
        }
#endif

        pVM->rem.s.Env.exception_index = u8TrapNo;
        if (enmType != TRPM_SOFTWARE_INT)
        {
            pVM->rem.s.Env.exception_is_int     = 0;
            pVM->rem.s.Env.exception_next_eip   = pVM->rem.s.Env.eip;
        }
        else
        {
            /*
             * The there are two 1 byte opcodes and one 2 byte opcode for software interrupts.
             * We ASSUME that there are no prefixes and sets the default to 2 byte, and checks
             * for int03 and into.
             */
            pVM->rem.s.Env.exception_is_int     = 1;
            pVM->rem.s.Env.exception_next_eip   = pCtx->eip + 2;
            /* int 3 may be generated by one-byte 0xcc */
            if (u8TrapNo == 3)
            {
                if (read_byte(&pVM->rem.s.Env, pVM->rem.s.Env.segs[R_CS].base + pCtx->eip) == 0xcc)
                    pVM->rem.s.Env.exception_next_eip = pCtx->eip + 1;
            }
            /* int 4 may be generated by one-byte 0xce */
            else if (u8TrapNo == 4)
            {
                if (read_byte(&pVM->rem.s.Env, pVM->rem.s.Env.segs[R_CS].base + pCtx->eip) == 0xce)
                    pVM->rem.s.Env.exception_next_eip = pCtx->eip + 1;
            }
        }

        /* get error code and cr2 if needed. */
        switch (u8TrapNo)
        {
            case 0x0e:
                pVM->rem.s.Env.cr[2] = TRPMGetFaultAddress(pVM);
                /* fallthru */
            case 0x0a: case 0x0b: case 0x0c: case 0x0d:
                pVM->rem.s.Env.error_code = TRPMGetErrorCode(pVM);
                break;

            case 0x11: case 0x08:
            default:
                pVM->rem.s.Env.error_code = 0;
                break;
        }

        /*
         * We can now reset the active trap since the recompiler is gonna have a go at it.
         */
        rc = TRPMResetTrap(pVM);
        AssertRC(rc);
        Log2(("REMR3State: trap=%02x errcd=%VGv cr2=%VGv nexteip=%VGv%s\n", pVM->rem.s.Env.exception_index, pVM->rem.s.Env.error_code,
              pVM->rem.s.Env.cr[2], pVM->rem.s.Env.exception_next_eip, pVM->rem.s.Env.exception_is_int ? " software" : ""));
    }

    /*
     * Clear old interrupt request flags; Check for pending hardware interrupts.
     * (See @remark for why we don't check for other FFs.)
     */
    pVM->rem.s.Env.interrupt_request &= ~(CPU_INTERRUPT_HARD | CPU_INTERRUPT_EXIT | CPU_INTERRUPT_EXITTB | CPU_INTERRUPT_TIMER);
    if (    pVM->rem.s.u32PendingInterrupt != REM_NO_PENDING_IRQ
        ||  VM_FF_ISPENDING(pVM, VM_FF_INTERRUPT_APIC | VM_FF_INTERRUPT_PIC))
        pVM->rem.s.Env.interrupt_request |= CPU_INTERRUPT_HARD;

    /*
     * We're now in REM mode.
     */
    pVM->rem.s.fInREM = true;
    pVM->rem.s.fInStateSync = false;
    pVM->rem.s.cCanExecuteRaw = 0;
    STAM_PROFILE_STOP(&pVM->rem.s.StatsState, a);
    Log2(("REMR3State: returns VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}


/**
 * Syncs back changes in the REM state to the the VM state.
 *
 * This must be called after invoking REMR3Run().
 * Calling it several times in a row is not permitted.
 *
 * @returns VBox status code.
 *
 * @param   pVM         VM Handle.
 */
REMR3DECL(int) REMR3StateBack(PVM pVM)
{
    Log2(("REMR3StateBack:\n"));
    Assert(pVM->rem.s.fInREM);
    STAM_PROFILE_START(&pVM->rem.s.StatsStateBack, a);
    register PCPUMCTX pCtx = pVM->rem.s.pCtx;

    /*
     * Copy back the registers.
     * This is done in the order they are declared in the CPUMCTX structure.
     */

    /** @todo FOP */
    /** @todo FPUIP */
    /** @todo CS */
    /** @todo FPUDP */
    /** @todo DS */
    /** @todo Fix MXCSR support in QEMU so we don't overwrite MXCSR with 0 when we shouldn't! */
    pCtx->fpu.MXCSR         = 0;
    pCtx->fpu.MXCSR_MASK    = 0;

    /** @todo check if FPU/XMM was actually used in the recompiler */
    restore_raw_fp_state(&pVM->rem.s.Env, (uint8_t *)&pCtx->fpu);
////    dprintf2(("FPU state CW=%04X TT=%04X SW=%04X (%04X)\n", env->fpuc, env->fpstt, env->fpus, pVMCtx->fpu.FSW));

#ifdef TARGET_X86_64
    /* Note that the high dwords of 64 bits registers are undefined in 32 bits mode and are undefined after a mode change. */
    pCtx->rdi           = pVM->rem.s.Env.regs[R_EDI];
    pCtx->rsi           = pVM->rem.s.Env.regs[R_ESI];
    pCtx->rbp           = pVM->rem.s.Env.regs[R_EBP];
    pCtx->rax           = pVM->rem.s.Env.regs[R_EAX];
    pCtx->rbx           = pVM->rem.s.Env.regs[R_EBX];
    pCtx->rdx           = pVM->rem.s.Env.regs[R_EDX];
    pCtx->rcx           = pVM->rem.s.Env.regs[R_ECX];
    pCtx->r8            = pVM->rem.s.Env.regs[8];
    pCtx->r9            = pVM->rem.s.Env.regs[9];
    pCtx->r10           = pVM->rem.s.Env.regs[10];
    pCtx->r11           = pVM->rem.s.Env.regs[11];
    pCtx->r12           = pVM->rem.s.Env.regs[12];
    pCtx->r13           = pVM->rem.s.Env.regs[13];
    pCtx->r14           = pVM->rem.s.Env.regs[14];
    pCtx->r15           = pVM->rem.s.Env.regs[15];

    pCtx->rsp           = pVM->rem.s.Env.regs[R_ESP];

#else
    pCtx->edi           = pVM->rem.s.Env.regs[R_EDI];
    pCtx->esi           = pVM->rem.s.Env.regs[R_ESI];
    pCtx->ebp           = pVM->rem.s.Env.regs[R_EBP];
    pCtx->eax           = pVM->rem.s.Env.regs[R_EAX];
    pCtx->ebx           = pVM->rem.s.Env.regs[R_EBX];
    pCtx->edx           = pVM->rem.s.Env.regs[R_EDX];
    pCtx->ecx           = pVM->rem.s.Env.regs[R_ECX];

    pCtx->esp           = pVM->rem.s.Env.regs[R_ESP];
#endif

    pCtx->ss            = pVM->rem.s.Env.segs[R_SS].selector;

#ifdef VBOX_WITH_STATISTICS
    if (pVM->rem.s.Env.segs[R_SS].newselector)
    {
        STAM_COUNTER_INC(&gStatSelOutOfSyncStateBack[R_SS]);
    }
    if (pVM->rem.s.Env.segs[R_GS].newselector)
    {
        STAM_COUNTER_INC(&gStatSelOutOfSyncStateBack[R_GS]);
    }
    if (pVM->rem.s.Env.segs[R_FS].newselector)
    {
        STAM_COUNTER_INC(&gStatSelOutOfSyncStateBack[R_FS]);
    }
    if (pVM->rem.s.Env.segs[R_ES].newselector)
    {
        STAM_COUNTER_INC(&gStatSelOutOfSyncStateBack[R_ES]);
    }
    if (pVM->rem.s.Env.segs[R_DS].newselector)
    {
        STAM_COUNTER_INC(&gStatSelOutOfSyncStateBack[R_DS]);
    }
    if (pVM->rem.s.Env.segs[R_CS].newselector)
    {
        STAM_COUNTER_INC(&gStatSelOutOfSyncStateBack[R_CS]);
    }
#endif
    pCtx->gs            = pVM->rem.s.Env.segs[R_GS].selector;
    pCtx->fs            = pVM->rem.s.Env.segs[R_FS].selector;
    pCtx->es            = pVM->rem.s.Env.segs[R_ES].selector;
    pCtx->ds            = pVM->rem.s.Env.segs[R_DS].selector;
    pCtx->cs            = pVM->rem.s.Env.segs[R_CS].selector;

#ifdef TARGET_X86_64
    pCtx->rip           = pVM->rem.s.Env.eip;
    pCtx->rflags.u64    = pVM->rem.s.Env.eflags;
#else
    pCtx->eip           = pVM->rem.s.Env.eip;
    pCtx->eflags.u32    = pVM->rem.s.Env.eflags;
#endif

    pCtx->cr0           = pVM->rem.s.Env.cr[0];
    pCtx->cr2           = pVM->rem.s.Env.cr[2];
    pCtx->cr3           = pVM->rem.s.Env.cr[3];
    pCtx->cr4           = pVM->rem.s.Env.cr[4];

    pCtx->dr0           = pVM->rem.s.Env.dr[0];
    pCtx->dr1           = pVM->rem.s.Env.dr[1];
    pCtx->dr2           = pVM->rem.s.Env.dr[2];
    pCtx->dr3           = pVM->rem.s.Env.dr[3];
    pCtx->dr4           = pVM->rem.s.Env.dr[4];
    pCtx->dr5           = pVM->rem.s.Env.dr[5];
    pCtx->dr6           = pVM->rem.s.Env.dr[6];
    pCtx->dr7           = pVM->rem.s.Env.dr[7];

    pCtx->gdtr.cbGdt    = pVM->rem.s.Env.gdt.limit;
    if (pCtx->gdtr.pGdt != pVM->rem.s.Env.gdt.base)
    {
        pCtx->gdtr.pGdt = pVM->rem.s.Env.gdt.base;
        STAM_COUNTER_INC(&gStatREMGDTChange);
        VM_FF_SET(pVM, VM_FF_SELM_SYNC_GDT);
    }

    pCtx->idtr.cbIdt    = pVM->rem.s.Env.idt.limit;
    if (pCtx->idtr.pIdt != pVM->rem.s.Env.idt.base)
    {
        pCtx->idtr.pIdt = pVM->rem.s.Env.idt.base;
        STAM_COUNTER_INC(&gStatREMIDTChange);
        VM_FF_SET(pVM, VM_FF_TRPM_SYNC_IDT);
    }

    if (pCtx->ldtr != pVM->rem.s.Env.ldt.selector)
    {
        pCtx->ldtr      = pVM->rem.s.Env.ldt.selector;
        STAM_COUNTER_INC(&gStatREMLDTRChange);
        VM_FF_SET(pVM, VM_FF_SELM_SYNC_LDT);
    }
    if (pCtx->tr != pVM->rem.s.Env.tr.selector)
    {
        pCtx->tr        = pVM->rem.s.Env.tr.selector;
        STAM_COUNTER_INC(&gStatREMTRChange);
        VM_FF_SET(pVM, VM_FF_SELM_SYNC_TSS);
    }

    /** @todo These values could still be out of sync! */
    pCtx->csHid.u64Base    = pVM->rem.s.Env.segs[R_CS].base;
    pCtx->csHid.u32Limit   = pVM->rem.s.Env.segs[R_CS].limit;
    /** @note QEmu saves the 2nd dword of the descriptor; we should store the attribute word only! */
    pCtx->csHid.Attr.u     = (pVM->rem.s.Env.segs[R_CS].flags >> 8) & 0xF0FF;

    pCtx->dsHid.u64Base    = pVM->rem.s.Env.segs[R_DS].base;
    pCtx->dsHid.u32Limit   = pVM->rem.s.Env.segs[R_DS].limit;
    pCtx->dsHid.Attr.u     = (pVM->rem.s.Env.segs[R_DS].flags >> 8) & 0xF0FF;

    pCtx->esHid.u64Base    = pVM->rem.s.Env.segs[R_ES].base;
    pCtx->esHid.u32Limit   = pVM->rem.s.Env.segs[R_ES].limit;
    pCtx->esHid.Attr.u     = (pVM->rem.s.Env.segs[R_ES].flags >> 8) & 0xF0FF;

    pCtx->fsHid.u64Base    = pVM->rem.s.Env.segs[R_FS].base;
    pCtx->fsHid.u32Limit   = pVM->rem.s.Env.segs[R_FS].limit;
    pCtx->fsHid.Attr.u     = (pVM->rem.s.Env.segs[R_FS].flags >> 8) & 0xF0FF;

    pCtx->gsHid.u64Base    = pVM->rem.s.Env.segs[R_GS].base;
    pCtx->gsHid.u32Limit   = pVM->rem.s.Env.segs[R_GS].limit;
    pCtx->gsHid.Attr.u     = (pVM->rem.s.Env.segs[R_GS].flags >> 8) & 0xF0FF;

    pCtx->ssHid.u64Base    = pVM->rem.s.Env.segs[R_SS].base;
    pCtx->ssHid.u32Limit   = pVM->rem.s.Env.segs[R_SS].limit;
    pCtx->ssHid.Attr.u     = (pVM->rem.s.Env.segs[R_SS].flags >> 8) & 0xF0FF;

    pCtx->ldtrHid.u64Base  = pVM->rem.s.Env.ldt.base;
    pCtx->ldtrHid.u32Limit = pVM->rem.s.Env.ldt.limit;
    pCtx->ldtrHid.Attr.u   = (pVM->rem.s.Env.ldt.flags >> 8) & 0xF0FF;

    pCtx->trHid.u64Base    = pVM->rem.s.Env.tr.base;
    pCtx->trHid.u32Limit   = pVM->rem.s.Env.tr.limit;
    pCtx->trHid.Attr.u     = (pVM->rem.s.Env.tr.flags >> 8) & 0xF0FF;

    /* Sysenter MSR */
    pCtx->SysEnter.cs      = pVM->rem.s.Env.sysenter_cs;
    pCtx->SysEnter.eip     = pVM->rem.s.Env.sysenter_eip;
    pCtx->SysEnter.esp     = pVM->rem.s.Env.sysenter_esp;

    /* System MSRs. */
    pCtx->msrEFER          = pVM->rem.s.Env.efer;
    pCtx->msrSTAR          = pVM->rem.s.Env.star;
    pCtx->msrPAT           = pVM->rem.s.Env.pat;
#ifdef TARGET_X86_64
    pCtx->msrLSTAR         = pVM->rem.s.Env.lstar;
    pCtx->msrCSTAR         = pVM->rem.s.Env.cstar;
    pCtx->msrSFMASK        = pVM->rem.s.Env.fmask;
    pCtx->msrFSBASE        = pVM->rem.s.Env.segs[R_FS].base;
    pCtx->msrGSBASE        = pVM->rem.s.Env.segs[R_GS].base;
    pCtx->msrKERNELGSBASE  = pVM->rem.s.Env.kernelgsbase;
#endif

    remR3TrapClear(pVM);

    /*
     * Check for traps.
     */
    if (    pVM->rem.s.Env.exception_index >= 0
        &&  pVM->rem.s.Env.exception_index < 256)
    {
        Log(("REMR3StateBack: Pending trap %x %d\n", pVM->rem.s.Env.exception_index, pVM->rem.s.Env.exception_is_int));
        int rc = TRPMAssertTrap(pVM, pVM->rem.s.Env.exception_index, (pVM->rem.s.Env.exception_is_int) ? TRPM_SOFTWARE_INT : TRPM_HARDWARE_INT);
        AssertRC(rc);
        switch (pVM->rem.s.Env.exception_index)
        {
            case 0x0e:
                TRPMSetFaultAddress(pVM, pCtx->cr2);
                /* fallthru */
            case 0x0a: case 0x0b: case 0x0c: case 0x0d:
            case 0x11: case 0x08: /* 0 */
                TRPMSetErrorCode(pVM, pVM->rem.s.Env.error_code);
                break;
        }

    }

    /*
     * We're not longer in REM mode.
     */
    pVM->rem.s.fInREM   = false;
    STAM_PROFILE_STOP(&pVM->rem.s.StatsStateBack, a);
    Log2(("REMR3StateBack: returns VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}


/**
 * This is called by the disassembler when it wants to update the cpu state
 * before for instance doing a register dump.
 */
static void remR3StateUpdate(PVM pVM)
{
    Assert(pVM->rem.s.fInREM);
    register PCPUMCTX pCtx = pVM->rem.s.pCtx;

    /*
     * Copy back the registers.
     * This is done in the order they are declared in the CPUMCTX structure.
     */

    /** @todo FOP */
    /** @todo FPUIP */
    /** @todo CS */
    /** @todo FPUDP */
    /** @todo DS */
    /** @todo Fix MXCSR support in QEMU so we don't overwrite MXCSR with 0 when we shouldn't! */
    pCtx->fpu.MXCSR         = 0;
    pCtx->fpu.MXCSR_MASK    = 0;

    /** @todo check if FPU/XMM was actually used in the recompiler */
    restore_raw_fp_state(&pVM->rem.s.Env, (uint8_t *)&pCtx->fpu);
////    dprintf2(("FPU state CW=%04X TT=%04X SW=%04X (%04X)\n", env->fpuc, env->fpstt, env->fpus, pVMCtx->fpu.FSW));

#ifdef TARGET_X86_64
    pCtx->rdi           = pVM->rem.s.Env.regs[R_EDI];
    pCtx->rsi           = pVM->rem.s.Env.regs[R_ESI];
    pCtx->rbp           = pVM->rem.s.Env.regs[R_EBP];
    pCtx->rax           = pVM->rem.s.Env.regs[R_EAX];
    pCtx->rbx           = pVM->rem.s.Env.regs[R_EBX];
    pCtx->rdx           = pVM->rem.s.Env.regs[R_EDX];
    pCtx->rcx           = pVM->rem.s.Env.regs[R_ECX];
    pCtx->r8            = pVM->rem.s.Env.regs[8];
    pCtx->r9            = pVM->rem.s.Env.regs[9];
    pCtx->r10           = pVM->rem.s.Env.regs[10];
    pCtx->r11           = pVM->rem.s.Env.regs[11];
    pCtx->r12           = pVM->rem.s.Env.regs[12];
    pCtx->r13           = pVM->rem.s.Env.regs[13];
    pCtx->r14           = pVM->rem.s.Env.regs[14];
    pCtx->r15           = pVM->rem.s.Env.regs[15];

    pCtx->rsp           = pVM->rem.s.Env.regs[R_ESP];
#else
    pCtx->edi           = pVM->rem.s.Env.regs[R_EDI];
    pCtx->esi           = pVM->rem.s.Env.regs[R_ESI];
    pCtx->ebp           = pVM->rem.s.Env.regs[R_EBP];
    pCtx->eax           = pVM->rem.s.Env.regs[R_EAX];
    pCtx->ebx           = pVM->rem.s.Env.regs[R_EBX];
    pCtx->edx           = pVM->rem.s.Env.regs[R_EDX];
    pCtx->ecx           = pVM->rem.s.Env.regs[R_ECX];

    pCtx->esp           = pVM->rem.s.Env.regs[R_ESP];
#endif

    pCtx->ss            = pVM->rem.s.Env.segs[R_SS].selector;

    pCtx->gs            = pVM->rem.s.Env.segs[R_GS].selector;
    pCtx->fs            = pVM->rem.s.Env.segs[R_FS].selector;
    pCtx->es            = pVM->rem.s.Env.segs[R_ES].selector;
    pCtx->ds            = pVM->rem.s.Env.segs[R_DS].selector;
    pCtx->cs            = pVM->rem.s.Env.segs[R_CS].selector;

#ifdef TARGET_X86_64
    pCtx->rip           = pVM->rem.s.Env.eip;
    pCtx->rflags.u64    = pVM->rem.s.Env.eflags;
#else
    pCtx->eip           = pVM->rem.s.Env.eip;
    pCtx->eflags.u32    = pVM->rem.s.Env.eflags;
#endif

    pCtx->cr0           = pVM->rem.s.Env.cr[0];
    pCtx->cr2           = pVM->rem.s.Env.cr[2];
    pCtx->cr3           = pVM->rem.s.Env.cr[3];
    pCtx->cr4           = pVM->rem.s.Env.cr[4];

    pCtx->dr0           = pVM->rem.s.Env.dr[0];
    pCtx->dr1           = pVM->rem.s.Env.dr[1];
    pCtx->dr2           = pVM->rem.s.Env.dr[2];
    pCtx->dr3           = pVM->rem.s.Env.dr[3];
    pCtx->dr4           = pVM->rem.s.Env.dr[4];
    pCtx->dr5           = pVM->rem.s.Env.dr[5];
    pCtx->dr6           = pVM->rem.s.Env.dr[6];
    pCtx->dr7           = pVM->rem.s.Env.dr[7];

    pCtx->gdtr.cbGdt    = pVM->rem.s.Env.gdt.limit;
    if (pCtx->gdtr.pGdt != (uint32_t)pVM->rem.s.Env.gdt.base)
    {
        pCtx->gdtr.pGdt     = (uint32_t)pVM->rem.s.Env.gdt.base;
        STAM_COUNTER_INC(&gStatREMGDTChange);
        VM_FF_SET(pVM, VM_FF_SELM_SYNC_GDT);
    }

    pCtx->idtr.cbIdt    = pVM->rem.s.Env.idt.limit;
    if (pCtx->idtr.pIdt != (uint32_t)pVM->rem.s.Env.idt.base)
    {
        pCtx->idtr.pIdt     = (uint32_t)pVM->rem.s.Env.idt.base;
        STAM_COUNTER_INC(&gStatREMIDTChange);
        VM_FF_SET(pVM, VM_FF_TRPM_SYNC_IDT);
    }

    if (pCtx->ldtr != pVM->rem.s.Env.ldt.selector)
    {
        pCtx->ldtr      = pVM->rem.s.Env.ldt.selector;
        STAM_COUNTER_INC(&gStatREMLDTRChange);
        VM_FF_SET(pVM, VM_FF_SELM_SYNC_LDT);
    }
    if (pCtx->tr != pVM->rem.s.Env.tr.selector)
    {
        pCtx->tr        = pVM->rem.s.Env.tr.selector;
        STAM_COUNTER_INC(&gStatREMTRChange);
        VM_FF_SET(pVM, VM_FF_SELM_SYNC_TSS);
    }

    /** @todo These values could still be out of sync! */
    pCtx->csHid.u64Base    = pVM->rem.s.Env.segs[R_CS].base;
    pCtx->csHid.u32Limit   = pVM->rem.s.Env.segs[R_CS].limit;
    /** @note QEmu saves the 2nd dword of the descriptor; we should store the attribute word only! */
    pCtx->csHid.Attr.u     = (pVM->rem.s.Env.segs[R_CS].flags >> 8) & 0xFFFF;

    pCtx->dsHid.u64Base    = pVM->rem.s.Env.segs[R_DS].base;
    pCtx->dsHid.u32Limit   = pVM->rem.s.Env.segs[R_DS].limit;
    pCtx->dsHid.Attr.u     = (pVM->rem.s.Env.segs[R_DS].flags >> 8) & 0xFFFF;

    pCtx->esHid.u64Base    = pVM->rem.s.Env.segs[R_ES].base;
    pCtx->esHid.u32Limit   = pVM->rem.s.Env.segs[R_ES].limit;
    pCtx->esHid.Attr.u     = (pVM->rem.s.Env.segs[R_ES].flags >> 8) & 0xFFFF;

    pCtx->fsHid.u64Base    = pVM->rem.s.Env.segs[R_FS].base;
    pCtx->fsHid.u32Limit   = pVM->rem.s.Env.segs[R_FS].limit;
    pCtx->fsHid.Attr.u     = (pVM->rem.s.Env.segs[R_FS].flags >> 8) & 0xFFFF;

    pCtx->gsHid.u64Base    = pVM->rem.s.Env.segs[R_GS].base;
    pCtx->gsHid.u32Limit   = pVM->rem.s.Env.segs[R_GS].limit;
    pCtx->gsHid.Attr.u     = (pVM->rem.s.Env.segs[R_GS].flags >> 8) & 0xFFFF;

    pCtx->ssHid.u64Base    = pVM->rem.s.Env.segs[R_SS].base;
    pCtx->ssHid.u32Limit   = pVM->rem.s.Env.segs[R_SS].limit;
    pCtx->ssHid.Attr.u     = (pVM->rem.s.Env.segs[R_SS].flags >> 8) & 0xFFFF;

    pCtx->ldtrHid.u64Base  = pVM->rem.s.Env.ldt.base;
    pCtx->ldtrHid.u32Limit = pVM->rem.s.Env.ldt.limit;
    pCtx->ldtrHid.Attr.u   = (pVM->rem.s.Env.ldt.flags >> 8) & 0xFFFF;

    pCtx->trHid.u64Base    = pVM->rem.s.Env.tr.base;
    pCtx->trHid.u32Limit   = pVM->rem.s.Env.tr.limit;
    pCtx->trHid.Attr.u     = (pVM->rem.s.Env.tr.flags >> 8) & 0xFFFF;

    /* Sysenter MSR */
    pCtx->SysEnter.cs      = pVM->rem.s.Env.sysenter_cs;
    pCtx->SysEnter.eip     = pVM->rem.s.Env.sysenter_eip;
    pCtx->SysEnter.esp     = pVM->rem.s.Env.sysenter_esp;

    /* System MSRs. */
    pCtx->msrEFER          = pVM->rem.s.Env.efer;
    pCtx->msrSTAR          = pVM->rem.s.Env.star;
    pCtx->msrPAT           = pVM->rem.s.Env.pat;
#ifdef TARGET_X86_64
    pCtx->msrLSTAR         = pVM->rem.s.Env.lstar;
    pCtx->msrCSTAR         = pVM->rem.s.Env.cstar;
    pCtx->msrSFMASK        = pVM->rem.s.Env.fmask;
    pCtx->msrFSBASE        = pVM->rem.s.Env.segs[R_FS].base;
    pCtx->msrGSBASE        = pVM->rem.s.Env.segs[R_GS].base;
    pCtx->msrKERNELGSBASE  = pVM->rem.s.Env.kernelgsbase;
#endif

}


/**
 * Update the VMM state information if we're currently in REM.
 *
 * This method is used by the DBGF and PDMDevice when there is any uncertainty of whether
 * we're currently executing in REM and the VMM state is invalid. This method will of
 * course check that we're executing in REM before syncing any data over to the VMM.
 *
 * @param   pVM         The VM handle.
 */
REMR3DECL(void) REMR3StateUpdate(PVM pVM)
{
    if (pVM->rem.s.fInREM)
        remR3StateUpdate(pVM);
}


#undef LOG_GROUP
#define LOG_GROUP LOG_GROUP_REM


/**
 * Notify the recompiler about Address Gate 20 state change.
 *
 * This notification is required since A20 gate changes are
 * initialized from a device driver and the VM might just as
 * well be in REM mode as in RAW mode.
 *
 * @param   pVM         VM handle.
 * @param   fEnable     True if the gate should be enabled.
 *                      False if the gate should be disabled.
 */
REMR3DECL(void) REMR3A20Set(PVM pVM, bool fEnable)
{
    LogFlow(("REMR3A20Set: fEnable=%d\n", fEnable));
    VM_ASSERT_EMT(pVM);

    bool fSaved = pVM->rem.s.fIgnoreAll; /* just in case. */
    pVM->rem.s.fIgnoreAll = fSaved || !pVM->rem.s.fInREM;

    cpu_x86_set_a20(&pVM->rem.s.Env, fEnable);

    pVM->rem.s.fIgnoreAll = fSaved;
}


/**
 * Replays the invalidated recorded pages.
 * Called in response to VERR_REM_FLUSHED_PAGES_OVERFLOW from the RAW execution loop.
 *
 * @param   pVM         VM handle.
 */
REMR3DECL(void) REMR3ReplayInvalidatedPages(PVM pVM)
{
    VM_ASSERT_EMT(pVM);

    /*
     * Sync the required registers.
     */
    pVM->rem.s.Env.cr[0] = pVM->rem.s.pCtx->cr0;
    pVM->rem.s.Env.cr[2] = pVM->rem.s.pCtx->cr2;
    pVM->rem.s.Env.cr[3] = pVM->rem.s.pCtx->cr3;
    pVM->rem.s.Env.cr[4] = pVM->rem.s.pCtx->cr4;

    /*
     * Replay the flushes.
     */
    pVM->rem.s.fIgnoreInvlPg = true;
    RTUINT i;
    for (i = 0; i < pVM->rem.s.cInvalidatedPages; i++)
    {
        Log2(("REMR3ReplayInvalidatedPages: invlpg %VGv\n", pVM->rem.s.aGCPtrInvalidatedPages[i]));
        tlb_flush_page(&pVM->rem.s.Env, pVM->rem.s.aGCPtrInvalidatedPages[i]);
    }
    pVM->rem.s.fIgnoreInvlPg = false;
    pVM->rem.s.cInvalidatedPages = 0;
}


/**
 * Replays the invalidated recorded pages.
 * Called in response to VERR_REM_FLUSHED_PAGES_OVERFLOW from the RAW execution loop.
 *
 * @param   pVM         VM handle.
 */
REMR3DECL(void) REMR3ReplayHandlerNotifications(PVM pVM)
{
    LogFlow(("REMR3ReplayInvalidatedPages:\n"));
    VM_ASSERT_EMT(pVM);

    /*
     * Replay the flushes.
     */
    RTUINT i;
    const RTUINT c = pVM->rem.s.cHandlerNotifications;
    pVM->rem.s.cHandlerNotifications = 0;
    for (i = 0; i < c; i++)
    {
        PREMHANDLERNOTIFICATION pRec = &pVM->rem.s.aHandlerNotifications[i];
        switch (pRec->enmKind)
        {
            case REMHANDLERNOTIFICATIONKIND_PHYSICAL_REGISTER:
                REMR3NotifyHandlerPhysicalRegister(pVM,
                                                   pRec->u.PhysicalRegister.enmType,
                                                   pRec->u.PhysicalRegister.GCPhys,
                                                   pRec->u.PhysicalRegister.cb,
                                                   pRec->u.PhysicalRegister.fHasHCHandler);
                break;

            case REMHANDLERNOTIFICATIONKIND_PHYSICAL_DEREGISTER:
                REMR3NotifyHandlerPhysicalDeregister(pVM,
                                                     pRec->u.PhysicalDeregister.enmType,
                                                     pRec->u.PhysicalDeregister.GCPhys,
                                                     pRec->u.PhysicalDeregister.cb,
                                                     pRec->u.PhysicalDeregister.fHasHCHandler,
                                                     pRec->u.PhysicalDeregister.fRestoreAsRAM);
                break;

            case REMHANDLERNOTIFICATIONKIND_PHYSICAL_MODIFY:
                REMR3NotifyHandlerPhysicalModify(pVM,
                                                 pRec->u.PhysicalModify.enmType,
                                                 pRec->u.PhysicalModify.GCPhysOld,
                                                 pRec->u.PhysicalModify.GCPhysNew,
                                                 pRec->u.PhysicalModify.cb,
                                                 pRec->u.PhysicalModify.fHasHCHandler,
                                                 pRec->u.PhysicalModify.fRestoreAsRAM);
                break;

            default:
                AssertReleaseMsgFailed(("enmKind=%d\n", pRec->enmKind));
                break;
        }
    }
}


/**
 * Notify REM about changed code page.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   pvCodePage  Code page address
 */
REMR3DECL(int) REMR3NotifyCodePageChanged(PVM pVM, RTGCPTR pvCodePage)
{
    int      rc;
    RTGCPHYS PhysGC;
    uint64_t flags;

    VM_ASSERT_EMT(pVM);

    /*
     * Get the physical page address.
     */
    rc = PGMGstGetPage(pVM, pvCodePage, &flags, &PhysGC);
    if (rc == VINF_SUCCESS)
    {
        /*
         * Sync the required registers and flush the whole page.
         * (Easier to do the whole page than notifying it about each physical
         * byte that was changed.
         */
        pVM->rem.s.Env.cr[0] = pVM->rem.s.pCtx->cr0;
        pVM->rem.s.Env.cr[2] = pVM->rem.s.pCtx->cr2;
        pVM->rem.s.Env.cr[3] = pVM->rem.s.pCtx->cr3;
        pVM->rem.s.Env.cr[4] = pVM->rem.s.pCtx->cr4;

        tb_invalidate_phys_page_range(PhysGC, PhysGC + PAGE_SIZE - 1, 0);
    }
    return VINF_SUCCESS;
}


/**
 * Notification about a successful MMR3PhysRegister() call.
 *
 * @param   pVM         VM handle.
 * @param   GCPhys      The physical address the RAM.
 * @param   cb          Size of the memory.
 * @param   fFlags      Flags of the MM_RAM_FLAGS_* defines.
 */
REMR3DECL(void) REMR3NotifyPhysRamRegister(PVM pVM, RTGCPHYS GCPhys, RTUINT cb, unsigned fFlags)
{
    Log(("REMR3NotifyPhysRamRegister: GCPhys=%VGp cb=%d fFlags=%d\n", GCPhys, cb, fFlags));
    VM_ASSERT_EMT(pVM);

    /*
     * Validate input - we trust the caller.
     */
    Assert(RT_ALIGN_T(GCPhys, PAGE_SIZE, RTGCPHYS) == GCPhys);
    Assert(cb);
    Assert(RT_ALIGN_Z(cb, PAGE_SIZE) == cb);

    /*
     * Base ram?
     */
    if (!GCPhys)
    {
        phys_ram_size = cb;
        phys_ram_dirty_size = cb >> PAGE_SHIFT;
#ifndef VBOX_STRICT
        phys_ram_dirty = MMR3HeapAlloc(pVM, MM_TAG_REM, phys_ram_dirty_size);
        AssertReleaseMsg(phys_ram_dirty, ("failed to allocate %d bytes of dirty bytes\n", phys_ram_dirty_size));
#else /* VBOX_STRICT: allocate a full map and make the out of bounds pages invalid. */
        phys_ram_dirty = RTMemPageAlloc(_4G >> PAGE_SHIFT);
        AssertReleaseMsg(phys_ram_dirty, ("failed to allocate %d bytes of dirty bytes\n", _4G >> PAGE_SHIFT));
        uint32_t cbBitmap = RT_ALIGN_32(phys_ram_dirty_size, PAGE_SIZE);
        int rc = RTMemProtect(phys_ram_dirty + cbBitmap, (_4G >> PAGE_SHIFT) - cbBitmap, RTMEM_PROT_NONE);
        AssertRC(rc);
        phys_ram_dirty += cbBitmap - phys_ram_dirty_size;
#endif
        memset(phys_ram_dirty, 0xff, phys_ram_dirty_size);
    }

    /*
     * Register the ram.
     */
    Assert(!pVM->rem.s.fIgnoreAll);
    pVM->rem.s.fIgnoreAll = true;

#ifdef VBOX_WITH_NEW_PHYS_CODE
    if (fFlags & MM_RAM_FLAGS_RESERVED)
        cpu_register_physical_memory(GCPhys, cb, IO_MEM_UNASSIGNED);
    else
        cpu_register_physical_memory(GCPhys, cb, GCPhys);
#else
    if (!GCPhys)
        cpu_register_physical_memory(GCPhys, cb, GCPhys | IO_MEM_RAM_MISSING);
    else
    {
        if (fFlags & MM_RAM_FLAGS_RESERVED)
            cpu_register_physical_memory(GCPhys, cb, IO_MEM_UNASSIGNED);
        else
            cpu_register_physical_memory(GCPhys, cb, GCPhys);
    }
#endif
    Assert(pVM->rem.s.fIgnoreAll);
    pVM->rem.s.fIgnoreAll = false;
}

#ifndef VBOX_WITH_NEW_PHYS_CODE

/**
 * Notification about a successful PGMR3PhysRegisterChunk() call.
 *
 * @param   pVM         VM handle.
 * @param   GCPhys      The physical address the RAM.
 * @param   cb          Size of the memory.
 * @param   pvRam       The HC address of the RAM.
 * @param   fFlags      Flags of the MM_RAM_FLAGS_* defines.
 */
REMR3DECL(void) REMR3NotifyPhysRamChunkRegister(PVM pVM, RTGCPHYS GCPhys, RTUINT cb, RTHCUINTPTR pvRam, unsigned fFlags)
{
    Log(("REMR3NotifyPhysRamChunkRegister: GCPhys=%VGp cb=%d pvRam=%p fFlags=%d\n", GCPhys, cb, pvRam, fFlags));
    VM_ASSERT_EMT(pVM);

    /*
     * Validate input - we trust the caller.
     */
    Assert(pvRam);
    Assert(RT_ALIGN(pvRam, PAGE_SIZE) == pvRam);
    Assert(RT_ALIGN_T(GCPhys, PAGE_SIZE, RTGCPHYS) == GCPhys);
    Assert(cb == PGM_DYNAMIC_CHUNK_SIZE);
    Assert(fFlags == 0 /* normal RAM */);
    Assert(!pVM->rem.s.fIgnoreAll);
    pVM->rem.s.fIgnoreAll = true;

    cpu_register_physical_memory(GCPhys, cb, GCPhys);

    Assert(pVM->rem.s.fIgnoreAll);
    pVM->rem.s.fIgnoreAll = false;
}


/**
 *  Grows dynamically allocated guest RAM.
 *  Will raise a fatal error if the operation fails.
 *
 * @param   physaddr    The physical address.
 */
void remR3GrowDynRange(unsigned long physaddr)
{
    int rc;
    PVM pVM = cpu_single_env->pVM;

    LogFlow(("remR3GrowDynRange %VGp\n", physaddr));
    const RTGCPHYS GCPhys = physaddr;
    rc = PGM3PhysGrowRange(pVM, &GCPhys);
    if (VBOX_SUCCESS(rc))
        return;

    LogRel(("\nUnable to allocate guest RAM chunk at %VGp\n", physaddr));
    cpu_abort(cpu_single_env, "Unable to allocate guest RAM chunk at %VGp\n", physaddr);
    AssertFatalFailed();
}

#endif /* !VBOX_WITH_NEW_PHYS_CODE */

/**
 * Notification about a successful MMR3PhysRomRegister() call.
 *
 * @param   pVM         VM handle.
 * @param   GCPhys      The physical address of the ROM.
 * @param   cb          The size of the ROM.
 * @param   pvCopy      Pointer to the ROM copy.
 * @param   fShadow     Whether it's currently writable shadow ROM or normal readonly ROM.
 *                      This function will be called when ever the protection of the
 *                      shadow ROM changes (at reset and end of POST).
 */
REMR3DECL(void) REMR3NotifyPhysRomRegister(PVM pVM, RTGCPHYS GCPhys, RTUINT cb, void *pvCopy, bool fShadow)
{
    Log(("REMR3NotifyPhysRomRegister: GCPhys=%VGp cb=%d pvCopy=%p fShadow=%RTbool\n", GCPhys, cb, pvCopy, fShadow));
    VM_ASSERT_EMT(pVM);

    /*
     * Validate input - we trust the caller.
     */
    Assert(RT_ALIGN_T(GCPhys, PAGE_SIZE, RTGCPHYS) == GCPhys);
    Assert(cb);
    Assert(RT_ALIGN_Z(cb, PAGE_SIZE) == cb);
    Assert(pvCopy);
    Assert(RT_ALIGN_P(pvCopy, PAGE_SIZE) == pvCopy);

    /*
     * Register the rom.
     */
    Assert(!pVM->rem.s.fIgnoreAll);
    pVM->rem.s.fIgnoreAll = true;

    cpu_register_physical_memory(GCPhys, cb, GCPhys | (fShadow ? 0 : IO_MEM_ROM));

    Log2(("%.64Vhxd\n", (char *)pvCopy + cb - 64));

    Assert(pVM->rem.s.fIgnoreAll);
    pVM->rem.s.fIgnoreAll = false;
}


/**
 * Notification about a successful memory deregistration or reservation.
 *
 * @param   pVM         VM Handle.
 * @param   GCPhys      Start physical address.
 * @param   cb          The size of the range.
 * @todo    Rename to REMR3NotifyPhysRamDeregister (for MMIO2) as we won't
 *          reserve any memory soon.
 */
REMR3DECL(void) REMR3NotifyPhysReserve(PVM pVM, RTGCPHYS GCPhys, RTUINT cb)
{
    Log(("REMR3NotifyPhysReserve: GCPhys=%VGp cb=%d\n", GCPhys, cb));
    VM_ASSERT_EMT(pVM);

    /*
     * Validate input - we trust the caller.
     */
    Assert(RT_ALIGN_T(GCPhys, PAGE_SIZE, RTGCPHYS) == GCPhys);
    Assert(cb);
    Assert(RT_ALIGN_Z(cb, PAGE_SIZE) == cb);

    /*
     * Unassigning the memory.
     */
    Assert(!pVM->rem.s.fIgnoreAll);
    pVM->rem.s.fIgnoreAll = true;

    cpu_register_physical_memory(GCPhys, cb, IO_MEM_UNASSIGNED);

    Assert(pVM->rem.s.fIgnoreAll);
    pVM->rem.s.fIgnoreAll = false;
}


/**
 * Notification about a successful PGMR3HandlerPhysicalRegister() call.
 *
 * @param   pVM             VM Handle.
 * @param   enmType         Handler type.
 * @param   GCPhys          Handler range address.
 * @param   cb              Size of the handler range.
 * @param   fHasHCHandler   Set if the handler has a HC callback function.
 *
 * @remark  MMR3PhysRomRegister assumes that this function will not apply the
 *          Handler memory type to memory which has no HC handler.
 */
REMR3DECL(void) REMR3NotifyHandlerPhysicalRegister(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhys, RTGCPHYS cb, bool fHasHCHandler)
{
    Log(("REMR3NotifyHandlerPhysicalRegister: enmType=%d GCPhys=%VGp cb=%d fHasHCHandler=%d\n",
          enmType, GCPhys, cb, fHasHCHandler));
    VM_ASSERT_EMT(pVM);
    Assert(RT_ALIGN_T(GCPhys, PAGE_SIZE, RTGCPHYS) == GCPhys);
    Assert(RT_ALIGN_T(cb, PAGE_SIZE, RTGCPHYS) == cb);

    if (pVM->rem.s.cHandlerNotifications)
        REMR3ReplayHandlerNotifications(pVM);

    Assert(!pVM->rem.s.fIgnoreAll);
    pVM->rem.s.fIgnoreAll = true;

    if (enmType == PGMPHYSHANDLERTYPE_MMIO)
        cpu_register_physical_memory(GCPhys, cb, pVM->rem.s.iMMIOMemType);
    else if (fHasHCHandler)
        cpu_register_physical_memory(GCPhys, cb, pVM->rem.s.iHandlerMemType);

    Assert(pVM->rem.s.fIgnoreAll);
    pVM->rem.s.fIgnoreAll = false;
}


/**
 * Notification about a successful PGMR3HandlerPhysicalDeregister() operation.
 *
 * @param   pVM             VM Handle.
 * @param   enmType         Handler type.
 * @param   GCPhys          Handler range address.
 * @param   cb              Size of the handler range.
 * @param   fHasHCHandler   Set if the handler has a HC callback function.
 * @param   fRestoreAsRAM   Whether the to restore it as normal RAM or as unassigned memory.
 */
REMR3DECL(void) REMR3NotifyHandlerPhysicalDeregister(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhys, RTGCPHYS cb, bool fHasHCHandler, bool fRestoreAsRAM)
{
    Log(("REMR3NotifyHandlerPhysicalDeregister: enmType=%d GCPhys=%VGp cb=%VGp fHasHCHandler=%RTbool fRestoreAsRAM=%RTbool RAM=%08x\n",
          enmType, GCPhys, cb, fHasHCHandler, fRestoreAsRAM, MMR3PhysGetRamSize(pVM)));
    VM_ASSERT_EMT(pVM);

    if (pVM->rem.s.cHandlerNotifications)
        REMR3ReplayHandlerNotifications(pVM);

    Assert(!pVM->rem.s.fIgnoreAll);
    pVM->rem.s.fIgnoreAll = true;

/** @todo this isn't right, MMIO can (in theory) be restored as RAM. */
    if (enmType == PGMPHYSHANDLERTYPE_MMIO)
        cpu_register_physical_memory(GCPhys, cb, IO_MEM_UNASSIGNED);
    else if (fHasHCHandler)
    {
        if (!fRestoreAsRAM)
        {
            Assert(GCPhys > MMR3PhysGetRamSize(pVM));
            cpu_register_physical_memory(GCPhys, cb, IO_MEM_UNASSIGNED);
        }
        else
        {
            Assert(RT_ALIGN_T(GCPhys, PAGE_SIZE, RTGCPHYS) == GCPhys);
            Assert(RT_ALIGN_T(cb, PAGE_SIZE, RTGCPHYS) == cb);
            cpu_register_physical_memory(GCPhys, cb, GCPhys);
        }
    }

    Assert(pVM->rem.s.fIgnoreAll);
    pVM->rem.s.fIgnoreAll = false;
}


/**
 * Notification about a successful PGMR3HandlerPhysicalModify() call.
 *
 * @param   pVM             VM Handle.
 * @param   enmType         Handler type.
 * @param   GCPhysOld       Old handler range address.
 * @param   GCPhysNew       New handler range address.
 * @param   cb              Size of the handler range.
 * @param   fHasHCHandler   Set if the handler has a HC callback function.
 * @param   fRestoreAsRAM   Whether the to restore it as normal RAM or as unassigned memory.
 */
REMR3DECL(void) REMR3NotifyHandlerPhysicalModify(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhysOld, RTGCPHYS GCPhysNew, RTGCPHYS cb, bool fHasHCHandler, bool fRestoreAsRAM)
{
    Log(("REMR3NotifyHandlerPhysicalModify: enmType=%d GCPhysOld=%VGp GCPhysNew=%VGp cb=%d fHasHCHandler=%RTbool fRestoreAsRAM=%RTbool\n",
          enmType, GCPhysOld, GCPhysNew, cb, fHasHCHandler, fRestoreAsRAM));
    VM_ASSERT_EMT(pVM);
    AssertReleaseMsg(enmType != PGMPHYSHANDLERTYPE_MMIO, ("enmType=%d\n", enmType));

    if (pVM->rem.s.cHandlerNotifications)
        REMR3ReplayHandlerNotifications(pVM);

    if (fHasHCHandler)
    {
        Assert(!pVM->rem.s.fIgnoreAll);
        pVM->rem.s.fIgnoreAll = true;

        /*
         * Reset the old page.
         */
        if (!fRestoreAsRAM)
            cpu_register_physical_memory(GCPhysOld, cb, IO_MEM_UNASSIGNED);
        else
        {
            /* This is not perfect, but it'll do for PD monitoring... */
            Assert(cb == PAGE_SIZE);
            Assert(RT_ALIGN_T(GCPhysOld, PAGE_SIZE, RTGCPHYS) == GCPhysOld);
            cpu_register_physical_memory(GCPhysOld, cb, GCPhysOld);
        }

        /*
         * Update the new page.
         */
        Assert(RT_ALIGN_T(GCPhysNew, PAGE_SIZE, RTGCPHYS) == GCPhysNew);
        Assert(RT_ALIGN_T(cb, PAGE_SIZE, RTGCPHYS) == cb);
        cpu_register_physical_memory(GCPhysNew, cb, pVM->rem.s.iHandlerMemType);

        Assert(pVM->rem.s.fIgnoreAll);
        pVM->rem.s.fIgnoreAll = false;
    }
}


/**
 * Checks if we're handling access to this page or not.
 *
 * @returns true if we're trapping access.
 * @returns false if we aren't.
 * @param   pVM         The VM handle.
 * @param   GCPhys      The physical address.
 *
 * @remark  This function will only work correctly in VBOX_STRICT builds!
 */
REMR3DECL(bool) REMR3IsPageAccessHandled(PVM pVM, RTGCPHYS GCPhys)
{
#ifdef VBOX_STRICT
    if (pVM->rem.s.cHandlerNotifications)
        REMR3ReplayHandlerNotifications(pVM);

    unsigned long off = get_phys_page_offset(GCPhys);
    return (off & PAGE_OFFSET_MASK) == pVM->rem.s.iHandlerMemType
        || (off & PAGE_OFFSET_MASK) == pVM->rem.s.iMMIOMemType
        || (off & PAGE_OFFSET_MASK) == IO_MEM_ROM;
#else
    return false;
#endif
}


/**
 * Deals with a rare case in get_phys_addr_code where the code
 * is being monitored.
 *
 * It could also be an MMIO page, in which case we will raise a fatal error.
 *
 * @returns The physical address corresponding to addr.
 * @param   env         The cpu environment.
 * @param   addr        The virtual address.
 * @param   pTLBEntry   The TLB entry.
 */
target_ulong remR3PhysGetPhysicalAddressCode(CPUState *env, target_ulong addr, CPUTLBEntry *pTLBEntry)
{
    PVM pVM = env->pVM;
    if ((pTLBEntry->addr_code & ~TARGET_PAGE_MASK) == pVM->rem.s.iHandlerMemType)
    {
        target_ulong ret = pTLBEntry->addend + addr;
        AssertMsg2("remR3PhysGetPhysicalAddressCode: addr=%VGv addr_code=%VGv addend=%VGp ret=%VGp\n",
                   (RTGCPTR)addr, (RTGCPTR)pTLBEntry->addr_code, (RTGCPHYS)pTLBEntry->addend, ret);
        return ret;
    }
    LogRel(("\nTrying to execute code with memory type addr_code=%VGv addend=%VGp at %VGv! (iHandlerMemType=%#x iMMIOMemType=%#x)\n"
            "*** handlers\n",
            (RTGCPTR)pTLBEntry->addr_code, (RTGCPHYS)pTLBEntry->addend, (RTGCPTR)addr, pVM->rem.s.iHandlerMemType, pVM->rem.s.iMMIOMemType));
    DBGFR3Info(pVM, "handlers", NULL, DBGFR3InfoLogRelHlp());
    LogRel(("*** mmio\n"));
    DBGFR3Info(pVM, "mmio", NULL, DBGFR3InfoLogRelHlp());
    LogRel(("*** phys\n"));
    DBGFR3Info(pVM, "phys", NULL, DBGFR3InfoLogRelHlp());
    cpu_abort(env, "Trying to execute code with memory type addr_code=%VGv addend=%VGp at %VGv. (iHandlerMemType=%#x iMMIOMemType=%#x)\n",
              (RTGCPTR)pTLBEntry->addr_code, (RTGCPHYS)pTLBEntry->addend, (RTGCPTR)addr, pVM->rem.s.iHandlerMemType, pVM->rem.s.iMMIOMemType);
    AssertFatalFailed();
}


/** Validate the physical address passed to the read functions.
 * Useful for finding non-guest-ram reads/writes.  */
#if 1 /* disable if it becomes bothersome... */
# define VBOX_CHECK_ADDR(GCPhys) AssertMsg(PGMPhysIsGCPhysValid(cpu_single_env->pVM, (GCPhys)), ("%VGp\n", (GCPhys)))
#else
# define VBOX_CHECK_ADDR(GCPhys) do { } while (0)
#endif

/**
 * Read guest RAM and ROM.
 *
 * @param   SrcGCPhys       The source address (guest physical).
 * @param   pvDst           The destination address.
 * @param   cb              Number of bytes
 */
void remR3PhysRead(RTGCPHYS SrcGCPhys, void *pvDst, unsigned cb)
{
    STAM_PROFILE_ADV_START(&gStatMemRead, a);
    VBOX_CHECK_ADDR(SrcGCPhys);
    PGMPhysRead(cpu_single_env->pVM, SrcGCPhys, pvDst, cb);
    STAM_PROFILE_ADV_STOP(&gStatMemRead, a);
}


/**
 * Read guest RAM and ROM, unsigned 8-bit.
 *
 * @param   SrcGCPhys       The source address (guest physical).
 */
uint8_t remR3PhysReadU8(RTGCPHYS SrcGCPhys)
{
    uint8_t val;
    STAM_PROFILE_ADV_START(&gStatMemRead, a);
    VBOX_CHECK_ADDR(SrcGCPhys);
    val = PGMR3PhysReadU8(cpu_single_env->pVM, SrcGCPhys);
    STAM_PROFILE_ADV_STOP(&gStatMemRead, a);
    return val;
}


/**
 * Read guest RAM and ROM, signed 8-bit.
 *
 * @param   SrcGCPhys       The source address (guest physical).
 */
int8_t remR3PhysReadS8(RTGCPHYS SrcGCPhys)
{
    int8_t val;
    STAM_PROFILE_ADV_START(&gStatMemRead, a);
    VBOX_CHECK_ADDR(SrcGCPhys);
    val = PGMR3PhysReadU8(cpu_single_env->pVM, SrcGCPhys);
    STAM_PROFILE_ADV_STOP(&gStatMemRead, a);
    return val;
}


/**
 * Read guest RAM and ROM, unsigned 16-bit.
 *
 * @param   SrcGCPhys       The source address (guest physical).
 */
uint16_t remR3PhysReadU16(RTGCPHYS SrcGCPhys)
{
    uint16_t val;
    STAM_PROFILE_ADV_START(&gStatMemRead, a);
    VBOX_CHECK_ADDR(SrcGCPhys);
    val = PGMR3PhysReadU16(cpu_single_env->pVM, SrcGCPhys);
    STAM_PROFILE_ADV_STOP(&gStatMemRead, a);
    return val;
}


/**
 * Read guest RAM and ROM, signed 16-bit.
 *
 * @param   SrcGCPhys       The source address (guest physical).
 */
int16_t remR3PhysReadS16(RTGCPHYS SrcGCPhys)
{
    uint16_t val;
    STAM_PROFILE_ADV_START(&gStatMemRead, a);
    VBOX_CHECK_ADDR(SrcGCPhys);
    val = PGMR3PhysReadU16(cpu_single_env->pVM, SrcGCPhys);
    STAM_PROFILE_ADV_STOP(&gStatMemRead, a);
    return val;
}


/**
 * Read guest RAM and ROM, unsigned 32-bit.
 *
 * @param   SrcGCPhys       The source address (guest physical).
 */
uint32_t remR3PhysReadU32(RTGCPHYS SrcGCPhys)
{
    uint32_t val;
    STAM_PROFILE_ADV_START(&gStatMemRead, a);
    VBOX_CHECK_ADDR(SrcGCPhys);
    val = PGMR3PhysReadU32(cpu_single_env->pVM, SrcGCPhys);
    STAM_PROFILE_ADV_STOP(&gStatMemRead, a);
    return val;
}


/**
 * Read guest RAM and ROM, signed 32-bit.
 *
 * @param   SrcGCPhys       The source address (guest physical).
 */
int32_t remR3PhysReadS32(RTGCPHYS SrcGCPhys)
{
    int32_t val;
    STAM_PROFILE_ADV_START(&gStatMemRead, a);
    VBOX_CHECK_ADDR(SrcGCPhys);
    val = PGMR3PhysReadU32(cpu_single_env->pVM, SrcGCPhys);
    STAM_PROFILE_ADV_STOP(&gStatMemRead, a);
    return val;
}


/**
 * Read guest RAM and ROM, unsigned 64-bit.
 *
 * @param   SrcGCPhys       The source address (guest physical).
 */
uint64_t remR3PhysReadU64(RTGCPHYS SrcGCPhys)
{
    uint64_t val;
    STAM_PROFILE_ADV_START(&gStatMemRead, a);
    VBOX_CHECK_ADDR(SrcGCPhys);
    val = PGMR3PhysReadU64(cpu_single_env->pVM, SrcGCPhys);
    STAM_PROFILE_ADV_STOP(&gStatMemRead, a);
    return val;
}


/**
 * Write guest RAM.
 *
 * @param   DstGCPhys       The destination address (guest physical).
 * @param   pvSrc           The source address.
 * @param   cb              Number of bytes to write
 */
void remR3PhysWrite(RTGCPHYS DstGCPhys, const void *pvSrc, unsigned cb)
{
    STAM_PROFILE_ADV_START(&gStatMemWrite, a);
    VBOX_CHECK_ADDR(DstGCPhys);
    PGMPhysWrite(cpu_single_env->pVM, DstGCPhys, pvSrc, cb);
    STAM_PROFILE_ADV_STOP(&gStatMemWrite, a);
}


/**
 * Write guest RAM, unsigned 8-bit.
 *
 * @param   DstGCPhys       The destination address (guest physical).
 * @param   val             Value
 */
void remR3PhysWriteU8(RTGCPHYS DstGCPhys, uint8_t val)
{
    STAM_PROFILE_ADV_START(&gStatMemWrite, a);
    VBOX_CHECK_ADDR(DstGCPhys);
    PGMR3PhysWriteU8(cpu_single_env->pVM, DstGCPhys, val);
    STAM_PROFILE_ADV_STOP(&gStatMemWrite, a);
}


/**
 * Write guest RAM, unsigned 8-bit.
 *
 * @param   DstGCPhys       The destination address (guest physical).
 * @param   val             Value
 */
void remR3PhysWriteU16(RTGCPHYS DstGCPhys, uint16_t val)
{
    STAM_PROFILE_ADV_START(&gStatMemWrite, a);
    VBOX_CHECK_ADDR(DstGCPhys);
    PGMR3PhysWriteU16(cpu_single_env->pVM, DstGCPhys, val);
    STAM_PROFILE_ADV_STOP(&gStatMemWrite, a);
}


/**
 * Write guest RAM, unsigned 32-bit.
 *
 * @param   DstGCPhys       The destination address (guest physical).
 * @param   val             Value
 */
void remR3PhysWriteU32(RTGCPHYS DstGCPhys, uint32_t val)
{
    STAM_PROFILE_ADV_START(&gStatMemWrite, a);
    VBOX_CHECK_ADDR(DstGCPhys);
    PGMR3PhysWriteU32(cpu_single_env->pVM, DstGCPhys, val);
    STAM_PROFILE_ADV_STOP(&gStatMemWrite, a);
}


/**
 * Write guest RAM, unsigned 64-bit.
 *
 * @param   DstGCPhys       The destination address (guest physical).
 * @param   val             Value
 */
void remR3PhysWriteU64(RTGCPHYS DstGCPhys, uint64_t val)
{
    STAM_PROFILE_ADV_START(&gStatMemWrite, a);
    VBOX_CHECK_ADDR(DstGCPhys);
    PGMR3PhysWriteU64(cpu_single_env->pVM, DstGCPhys, val);
    STAM_PROFILE_ADV_STOP(&gStatMemWrite, a);
}

#undef LOG_GROUP
#define LOG_GROUP LOG_GROUP_REM_MMIO

/** Read MMIO memory. */
static uint32_t remR3MMIOReadU8(void *pvVM, target_phys_addr_t GCPhys)
{
    uint32_t u32 = 0;
    int rc = IOMMMIORead((PVM)pvVM, GCPhys, &u32, 1);
    AssertMsg(rc == VINF_SUCCESS, ("rc=%Vrc\n", rc)); NOREF(rc);
    Log2(("remR3MMIOReadU8: GCPhys=%VGp -> %02x\n", GCPhys, u32));
    return u32;
}

/** Read MMIO memory. */
static uint32_t remR3MMIOReadU16(void *pvVM, target_phys_addr_t GCPhys)
{
    uint32_t u32 = 0;
    int rc = IOMMMIORead((PVM)pvVM, GCPhys, &u32, 2);
    AssertMsg(rc == VINF_SUCCESS, ("rc=%Vrc\n", rc)); NOREF(rc);
    Log2(("remR3MMIOReadU16: GCPhys=%VGp -> %04x\n", GCPhys, u32));
    return u32;
}

/** Read MMIO memory. */
static uint32_t remR3MMIOReadU32(void *pvVM, target_phys_addr_t GCPhys)
{
    uint32_t u32 = 0;
    int rc = IOMMMIORead((PVM)pvVM, GCPhys, &u32, 4);
    AssertMsg(rc == VINF_SUCCESS, ("rc=%Vrc\n", rc)); NOREF(rc);
    Log2(("remR3MMIOReadU32: GCPhys=%VGp -> %08x\n", GCPhys, u32));
    return u32;
}

/** Write to MMIO memory. */
static void     remR3MMIOWriteU8(void *pvVM, target_phys_addr_t GCPhys, uint32_t u32)
{
    Log2(("remR3MMIOWriteU8: GCPhys=%VGp u32=%#x\n", GCPhys, u32));
    int rc = IOMMMIOWrite((PVM)pvVM, GCPhys, u32, 1);
    AssertMsg(rc == VINF_SUCCESS, ("rc=%Vrc\n", rc)); NOREF(rc);
}

/** Write to MMIO memory. */
static void     remR3MMIOWriteU16(void *pvVM, target_phys_addr_t GCPhys, uint32_t u32)
{
    Log2(("remR3MMIOWriteU16: GCPhys=%VGp u32=%#x\n", GCPhys, u32));
    int rc = IOMMMIOWrite((PVM)pvVM, GCPhys, u32, 2);
    AssertMsg(rc == VINF_SUCCESS, ("rc=%Vrc\n", rc)); NOREF(rc);
}

/** Write to MMIO memory. */
static void     remR3MMIOWriteU32(void *pvVM, target_phys_addr_t GCPhys, uint32_t u32)
{
    Log2(("remR3MMIOWriteU32: GCPhys=%VGp u32=%#x\n", GCPhys, u32));
    int rc = IOMMMIOWrite((PVM)pvVM, GCPhys, u32, 4);
    AssertMsg(rc == VINF_SUCCESS, ("rc=%Vrc\n", rc)); NOREF(rc);
}


#undef LOG_GROUP
#define LOG_GROUP LOG_GROUP_REM_HANDLER

/*  !!!WARNING!!! This is extremely hackish right now, we assume it's only for LFB access!  !!!WARNING!!!  */

static uint32_t remR3HandlerReadU8(void *pvVM, target_phys_addr_t GCPhys)
{
    Log2(("remR3HandlerReadU8: GCPhys=%VGp\n", GCPhys));
    uint8_t u8;
    PGMPhysRead((PVM)pvVM, GCPhys, &u8, sizeof(u8));
    return u8;
}

static uint32_t remR3HandlerReadU16(void *pvVM, target_phys_addr_t GCPhys)
{
    Log2(("remR3HandlerReadU16: GCPhys=%VGp\n", GCPhys));
    uint16_t u16;
    PGMPhysRead((PVM)pvVM, GCPhys, &u16, sizeof(u16));
    return u16;
}

static uint32_t remR3HandlerReadU32(void *pvVM, target_phys_addr_t GCPhys)
{
    Log2(("remR3HandlerReadU32: GCPhys=%VGp\n", GCPhys));
    uint32_t u32;
    PGMPhysRead((PVM)pvVM, GCPhys, &u32, sizeof(u32));
    return u32;
}

static void     remR3HandlerWriteU8(void *pvVM, target_phys_addr_t GCPhys, uint32_t u32)
{
    Log2(("remR3HandlerWriteU8: GCPhys=%VGp u32=%#x\n", GCPhys, u32));
    PGMPhysWrite((PVM)pvVM, GCPhys, &u32, sizeof(uint8_t));
}

static void     remR3HandlerWriteU16(void *pvVM, target_phys_addr_t GCPhys, uint32_t u32)
{
    Log2(("remR3HandlerWriteU16: GCPhys=%VGp u32=%#x\n", GCPhys, u32));
    PGMPhysWrite((PVM)pvVM, GCPhys, &u32, sizeof(uint16_t));
}

static void     remR3HandlerWriteU32(void *pvVM, target_phys_addr_t GCPhys, uint32_t u32)
{
    Log2(("remR3HandlerWriteU32: GCPhys=%VGp u32=%#x\n", GCPhys, u32));
    PGMPhysWrite((PVM)pvVM, GCPhys, &u32, sizeof(uint32_t));
}

/* -+- disassembly -+- */

#undef LOG_GROUP
#define LOG_GROUP LOG_GROUP_REM_DISAS


/**
 * Enables or disables singled stepped disassembly.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   fEnable     To enable set this flag, to disable clear it.
 */
static DECLCALLBACK(int) remR3DisasEnableStepping(PVM pVM, bool fEnable)
{
    LogFlow(("remR3DisasEnableStepping: fEnable=%d\n", fEnable));
    VM_ASSERT_EMT(pVM);

    if (fEnable)
        pVM->rem.s.Env.state |= CPU_EMULATE_SINGLE_STEP;
    else
        pVM->rem.s.Env.state &= ~CPU_EMULATE_SINGLE_STEP;
    return VINF_SUCCESS;
}


/**
 * Enables or disables singled stepped disassembly.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   fEnable     To enable set this flag, to disable clear it.
 */
REMR3DECL(int) REMR3DisasEnableStepping(PVM pVM, bool fEnable)
{
    PVMREQ  pReq;
    int     rc;

    LogFlow(("REMR3DisasEnableStepping: fEnable=%d\n", fEnable));
    if (VM_IS_EMT(pVM))
        return remR3DisasEnableStepping(pVM, fEnable);

    rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)remR3DisasEnableStepping, 2, pVM, fEnable);
    AssertRC(rc);
    if (VBOX_SUCCESS(rc))
        rc = pReq->iStatus;
    VMR3ReqFree(pReq);
    return rc;
}


#if defined(VBOX_WITH_DEBUGGER) && !(defined(RT_OS_WINDOWS) && defined(RT_ARCH_AMD64))
/**
 * External Debugger Command: .remstep [on|off|1|0]
 */
static DECLCALLBACK(int) remR3CmdDisasEnableStepping(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    bool fEnable;
    int rc;

    /* print status */
    if (cArgs == 0)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "DisasStepping is %s\n",
                                  pVM->rem.s.Env.state & CPU_EMULATE_SINGLE_STEP ? "enabled" : "disabled");

    /* convert the argument and change the mode. */
    rc = pCmdHlp->pfnVarToBool(pCmdHlp, &paArgs[0], &fEnable);
    if (VBOX_FAILURE(rc))
        return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "boolean conversion failed!\n");
    rc = REMR3DisasEnableStepping(pVM, fEnable);
    if (VBOX_FAILURE(rc))
        return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "REMR3DisasEnableStepping failed!\n");
    return rc;
}
#endif


/**
 * Disassembles n instructions and prints them to the log.
 *
 * @returns Success indicator.
 * @param   env         Pointer to the recompiler CPU structure.
 * @param   f32BitCode  Indicates that whether or not the code should
 *                      be disassembled as 16 or 32 bit. If -1 the CS
 *                      selector will be inspected.
 * @param   nrInstructions  Nr of instructions to disassemble
 * @param   pszPrefix
 * @remark  not currently used for anything but ad-hoc debugging.
 */
bool remR3DisasBlock(CPUState *env, int f32BitCode, int nrInstructions, char *pszPrefix)
{
    int i;

    /*
     * Determin 16/32 bit mode.
     */
    if (f32BitCode == -1)
        f32BitCode = !!(env->segs[R_CS].flags & X86_DESC_DB); /** @todo is this right?!!?!?!?!? */

    /*
     * Convert cs:eip to host context address.
     * We don't care to much about cross page correctness presently.
     */
    RTGCPTR    GCPtrPC = env->segs[R_CS].base + env->eip;
    void      *pvPC;
    if (f32BitCode && (env->cr[0] & (X86_CR0_PE | X86_CR0_PG)) == (X86_CR0_PE | X86_CR0_PG))
    {
        Assert(PGMGetGuestMode(env->pVM) < PGMMODE_AMD64);

        /* convert eip to physical address. */
        int rc = PGMPhysGCPtr2HCPtrByGstCR3(env->pVM,
                                            GCPtrPC,
                                            env->cr[3],
                                            env->cr[4] & (X86_CR4_PSE | X86_CR4_PAE), /** @todo add longmode flag */
                                            &pvPC);
        if (VBOX_FAILURE(rc))
        {
            if (!PATMIsPatchGCAddr(env->pVM, GCPtrPC))
                return false;
            pvPC = (char *)PATMR3QueryPatchMemHC(env->pVM, NULL)
                + (GCPtrPC - PATMR3QueryPatchMemGC(env->pVM, NULL));
        }
    }
    else
    {
        /* physical address */
        int rc = PGMPhysGCPhys2HCPtr(env->pVM, (RTGCPHYS)GCPtrPC, nrInstructions * 16, &pvPC);
        if (VBOX_FAILURE(rc))
            return false;
    }

    /*
     * Disassemble.
     */
    RTINTPTR        off = env->eip - (RTGCUINTPTR)pvPC;
    DISCPUSTATE     Cpu;
    Cpu.mode = f32BitCode ? CPUMODE_32BIT : CPUMODE_16BIT;
    Cpu.pfnReadBytes = NULL;            /** @todo make cs:eip reader for the disassembler. */
    //Cpu.dwUserData[0] = (uintptr_t)pVM;
    //Cpu.dwUserData[1] = (uintptr_t)pvPC;
    //Cpu.dwUserData[2] = GCPtrPC;

    for (i=0;i<nrInstructions;i++)
    {
        char szOutput[256];
        uint32_t    cbOp;
        if (RT_FAILURE(DISInstr(&Cpu, (uintptr_t)pvPC, off, &cbOp, &szOutput[0])))
            return false;
        if (pszPrefix)
            Log(("%s: %s", pszPrefix, szOutput));
        else
            Log(("%s", szOutput));

        pvPC += cbOp;
    }
    return true;
}


/** @todo need to test the new code, using the old code in the mean while. */
#define USE_OLD_DUMP_AND_DISASSEMBLY

/**
 * Disassembles one instruction and prints it to the log.
 *
 * @returns Success indicator.
 * @param   env         Pointer to the recompiler CPU structure.
 * @param   f32BitCode  Indicates that whether or not the code should
 *                      be disassembled as 16 or 32 bit. If -1 the CS
 *                      selector will be inspected.
 * @param   pszPrefix
 */
bool remR3DisasInstr(CPUState *env, int f32BitCode, char *pszPrefix)
{
#ifdef USE_OLD_DUMP_AND_DISASSEMBLY
    PVM pVM = env->pVM;

    /*
     * Determin 16/32 bit mode.
     */
    if (f32BitCode == -1)
        f32BitCode = !!(env->segs[R_CS].flags & X86_DESC_DB); /** @todo is this right?!!?!?!?!? */

    /*
     * Log registers
     */
    if (LogIs2Enabled())
    {
        remR3StateUpdate(pVM);
        DBGFR3InfoLog(pVM, "cpumguest", pszPrefix);
    }

    /*
     * Convert cs:eip to host context address.
     * We don't care to much about cross page correctness presently.
     */
    RTGCPTR    GCPtrPC = env->segs[R_CS].base + env->eip;
    void      *pvPC;
    if ((env->cr[0] & (X86_CR0_PE | X86_CR0_PG)) == (X86_CR0_PE | X86_CR0_PG))
    {
        /* convert eip to physical address. */
        int rc = PGMPhysGCPtr2HCPtrByGstCR3(pVM,
                                            GCPtrPC,
                                            env->cr[3],
                                            env->cr[4] & (X86_CR4_PSE | X86_CR4_PAE),
                                            &pvPC);
        if (VBOX_FAILURE(rc))
        {
            if (!PATMIsPatchGCAddr(pVM, GCPtrPC))
                return false;
            pvPC = (char *)PATMR3QueryPatchMemHC(pVM, NULL)
                + (GCPtrPC - PATMR3QueryPatchMemGC(pVM, NULL));
        }
    }
    else
    {

        /* physical address */
        int rc = PGMPhysGCPhys2HCPtr(pVM, (RTGCPHYS)GCPtrPC, 16, &pvPC);
        if (VBOX_FAILURE(rc))
            return false;
    }

    /*
     * Disassemble.
     */
    RTINTPTR        off = env->eip - (RTGCUINTPTR)pvPC;
    DISCPUSTATE     Cpu;
    Cpu.mode = f32BitCode ? CPUMODE_32BIT : CPUMODE_16BIT;
    Cpu.pfnReadBytes = NULL;            /** @todo make cs:eip reader for the disassembler. */
    //Cpu.dwUserData[0] = (uintptr_t)pVM;
    //Cpu.dwUserData[1] = (uintptr_t)pvPC;
    //Cpu.dwUserData[2] = GCPtrPC;
    char szOutput[256];
    uint32_t    cbOp;
    if (RT_FAILURE(DISInstr(&Cpu, (uintptr_t)pvPC, off, &cbOp, &szOutput[0])))
        return false;

    if (!f32BitCode)
    {
        if (pszPrefix)
            Log(("%s: %04X:%s", pszPrefix, env->segs[R_CS].selector, szOutput));
        else
            Log(("%04X:%s", env->segs[R_CS].selector, szOutput));
    }
    else
    {
        if (pszPrefix)
            Log(("%s: %s", pszPrefix, szOutput));
        else
            Log(("%s", szOutput));
    }
    return true;

#else /* !USE_OLD_DUMP_AND_DISASSEMBLY */
    PVM pVM = env->pVM;
    const bool fLog = LogIsEnabled();
    const bool fLog2 = LogIs2Enabled();
    int rc = VINF_SUCCESS;

    /*
     * Don't bother if there ain't any log output to do.
     */
    if (!fLog && !fLog2)
        return true;

    /*
     * Update the state so DBGF reads the correct register values.
     */
    remR3StateUpdate(pVM);

    /*
     * Log registers if requested.
     */
    if (!fLog2)
        DBGFR3InfoLog(pVM, "cpumguest", pszPrefix);

    /*
     * Disassemble to log.
     */
    if (fLog)
        rc = DBGFR3DisasInstrCurrentLogInternal(pVM, pszPrefix);

    return VBOX_SUCCESS(rc);
#endif
}


/**
 * Disassemble recompiled code.
 *
 * @param   phFileIgnored   Ignored, logfile usually.
 * @param   pvCode          Pointer to the code block.
 * @param   cb              Size of the code block.
 */
void disas(FILE *phFileIgnored, void *pvCode, unsigned long cb)
{
    if (LogIs2Enabled())
    {
        unsigned        off = 0;
        char            szOutput[256];
        DISCPUSTATE     Cpu;

        memset(&Cpu, 0, sizeof(Cpu));
#ifdef RT_ARCH_X86
        Cpu.mode = CPUMODE_32BIT;
#else
        Cpu.mode = CPUMODE_64BIT;
#endif

        RTLogPrintf("Recompiled Code: %p %#lx (%ld) bytes\n", pvCode, cb, cb);
        while (off < cb)
        {
            uint32_t cbInstr;
            if (RT_SUCCESS(DISInstr(&Cpu, (uintptr_t)pvCode + off, 0, &cbInstr, szOutput)))
                RTLogPrintf("%s", szOutput);
            else
            {
                RTLogPrintf("disas error\n");
                cbInstr = 1;
#ifdef RT_ARCH_AMD64 /** @todo remove when DISInstr starts supporing 64-bit code. */
                break;
#endif
            }
            off += cbInstr;
        }
    }
    NOREF(phFileIgnored);
}


/**
 * Disassemble guest code.
 *
 * @param   phFileIgnored   Ignored, logfile usually.
 * @param   uCode           The guest address of the code to disassemble. (flat?)
 * @param   cb              Number of bytes to disassemble.
 * @param   fFlags          Flags, probably something which tells if this is 16, 32 or 64 bit code.
 */
void target_disas(FILE *phFileIgnored, target_ulong uCode, target_ulong cb, int fFlags)
{
    if (LogIs2Enabled())
    {
        PVM pVM = cpu_single_env->pVM;

        /*
         * Update the state so DBGF reads the correct register values (flags).
         */
        remR3StateUpdate(pVM);

        /*
         * Do the disassembling.
         */
        RTLogPrintf("Guest Code: PC=%VGp #VGp (%VGp) bytes fFlags=%d\n", uCode, cb, cb, fFlags);
        RTSEL cs = cpu_single_env->segs[R_CS].selector;
        RTGCUINTPTR eip = uCode - cpu_single_env->segs[R_CS].base;
        for (;;)
        {
            char        szBuf[256];
            uint32_t    cbInstr;
            int rc = DBGFR3DisasInstrEx(pVM,
                                        cs,
                                        eip,
                                        0,
                                        szBuf, sizeof(szBuf),
                                        &cbInstr);
            if (VBOX_SUCCESS(rc))
                RTLogPrintf("%VGp %s\n", uCode, szBuf);
            else
            {
                RTLogPrintf("%VGp %04x:%VGp: %s\n", uCode, cs, eip, szBuf);
                cbInstr = 1;
            }

            /* next */
            if (cb <= cbInstr)
                break;
            cb -= cbInstr;
            uCode += cbInstr;
            eip += cbInstr;
        }
    }
    NOREF(phFileIgnored);
}


/**
 * Looks up a guest symbol.
 *
 * @returns Pointer to symbol name. This is a static buffer.
 * @param   orig_addr   The address in question.
 */
const char *lookup_symbol(target_ulong orig_addr)
{
    RTGCINTPTR off = 0;
    DBGFSYMBOL Sym;
    PVM pVM = cpu_single_env->pVM;
    int rc = DBGFR3SymbolByAddr(pVM, orig_addr, &off, &Sym);
    if (VBOX_SUCCESS(rc))
    {
        static char szSym[sizeof(Sym.szName) + 48];
        if (!off)
            RTStrPrintf(szSym,  sizeof(szSym), "%s\n", Sym.szName);
        else if (off > 0)
            RTStrPrintf(szSym,  sizeof(szSym), "%s+%x\n", Sym.szName,  off);
        else
            RTStrPrintf(szSym,  sizeof(szSym), "%s-%x\n", Sym.szName,  -off);
        return szSym;
    }
    return "<N/A>";
}


#undef LOG_GROUP
#define LOG_GROUP LOG_GROUP_REM


/* -+- FF notifications -+- */


/**
 * Notification about a pending interrupt.
 *
 * @param   pVM             VM Handle.
 * @param   u8Interrupt     Interrupt
 * @thread  The emulation thread.
 */
REMR3DECL(void) REMR3NotifyPendingInterrupt(PVM pVM, uint8_t u8Interrupt)
{
    Assert(pVM->rem.s.u32PendingInterrupt == REM_NO_PENDING_IRQ);
    pVM->rem.s.u32PendingInterrupt = u8Interrupt;
}

/**
 * Notification about a pending interrupt.
 *
 * @returns Pending interrupt or REM_NO_PENDING_IRQ
 * @param   pVM             VM Handle.
 * @thread  The emulation thread.
 */
REMR3DECL(uint32_t) REMR3QueryPendingInterrupt(PVM pVM)
{
    return pVM->rem.s.u32PendingInterrupt;
}

/**
 * Notification about the interrupt FF being set.
 *
 * @param   pVM             VM Handle.
 * @thread  The emulation thread.
 */
REMR3DECL(void) REMR3NotifyInterruptSet(PVM pVM)
{
    LogFlow(("REMR3NotifyInterruptSet: fInRem=%d interrupts %s\n", pVM->rem.s.fInREM,
             (pVM->rem.s.Env.eflags & IF_MASK) && !(pVM->rem.s.Env.hflags & HF_INHIBIT_IRQ_MASK) ? "enabled" : "disabled"));
    if (pVM->rem.s.fInREM)
    {
        if (VM_IS_EMT(pVM))
            cpu_interrupt(cpu_single_env, CPU_INTERRUPT_HARD);
        else
            ASMAtomicOrS32(&cpu_single_env->interrupt_request, CPU_INTERRUPT_EXTERNAL_HARD);
    }
}


/**
 * Notification about the interrupt FF being set.
 *
 * @param   pVM             VM Handle.
 * @thread  Any.
 */
REMR3DECL(void) REMR3NotifyInterruptClear(PVM pVM)
{
    LogFlow(("REMR3NotifyInterruptClear:\n"));
    if (pVM->rem.s.fInREM)
        cpu_reset_interrupt(cpu_single_env, CPU_INTERRUPT_HARD);
}


/**
 * Notification about pending timer(s).
 *
 * @param   pVM             VM Handle.
 * @thread  Any.
 */
REMR3DECL(void) REMR3NotifyTimerPending(PVM pVM)
{
#ifndef DEBUG_bird
    LogFlow(("REMR3NotifyTimerPending: fInRem=%d\n", pVM->rem.s.fInREM));
#endif
    if (pVM->rem.s.fInREM)
    {
        if (VM_IS_EMT(pVM))
            cpu_interrupt(cpu_single_env, CPU_INTERRUPT_EXIT);
        else
            ASMAtomicOrS32(&cpu_single_env->interrupt_request, CPU_INTERRUPT_EXTERNAL_TIMER);
    }
}


/**
 * Notification about pending DMA transfers.
 *
 * @param   pVM             VM Handle.
 * @thread  Any.
 */
REMR3DECL(void) REMR3NotifyDmaPending(PVM pVM)
{
    LogFlow(("REMR3NotifyDmaPending: fInRem=%d\n", pVM->rem.s.fInREM));
    if (pVM->rem.s.fInREM)
    {
        if (VM_IS_EMT(pVM))
            cpu_interrupt(cpu_single_env, CPU_INTERRUPT_EXIT);
        else
            ASMAtomicOrS32(&cpu_single_env->interrupt_request, CPU_INTERRUPT_EXTERNAL_DMA);
    }
}


/**
 * Notification about pending timer(s).
 *
 * @param   pVM             VM Handle.
 * @thread  Any.
 */
REMR3DECL(void) REMR3NotifyQueuePending(PVM pVM)
{
    LogFlow(("REMR3NotifyQueuePending: fInRem=%d\n", pVM->rem.s.fInREM));
    if (pVM->rem.s.fInREM)
    {
        if (VM_IS_EMT(pVM))
            cpu_interrupt(cpu_single_env, CPU_INTERRUPT_EXIT);
        else
            ASMAtomicOrS32(&cpu_single_env->interrupt_request, CPU_INTERRUPT_EXTERNAL_EXIT);
    }
}


/**
 * Notification about pending FF set by an external thread.
 *
 * @param   pVM             VM handle.
 * @thread  Any.
 */
REMR3DECL(void) REMR3NotifyFF(PVM pVM)
{
    LogFlow(("REMR3NotifyFF: fInRem=%d\n", pVM->rem.s.fInREM));
    if (pVM->rem.s.fInREM)
    {
        if (VM_IS_EMT(pVM))
            cpu_interrupt(cpu_single_env, CPU_INTERRUPT_EXIT);
        else
            ASMAtomicOrS32(&cpu_single_env->interrupt_request, CPU_INTERRUPT_EXTERNAL_EXIT);
    }
}


#ifdef VBOX_WITH_STATISTICS
void remR3ProfileStart(int statcode)
{
    STAMPROFILEADV *pStat;
    switch(statcode)
    {
    case STATS_EMULATE_SINGLE_INSTR:
        pStat = &gStatExecuteSingleInstr;
        break;
    case STATS_QEMU_COMPILATION:
        pStat = &gStatCompilationQEmu;
        break;
    case STATS_QEMU_RUN_EMULATED_CODE:
        pStat = &gStatRunCodeQEmu;
        break;
    case STATS_QEMU_TOTAL:
        pStat = &gStatTotalTimeQEmu;
        break;
    case STATS_QEMU_RUN_TIMERS:
        pStat = &gStatTimers;
        break;
    case STATS_TLB_LOOKUP:
        pStat= &gStatTBLookup;
        break;
    case STATS_IRQ_HANDLING:
        pStat= &gStatIRQ;
        break;
    case STATS_RAW_CHECK:
        pStat = &gStatRawCheck;
        break;

    default:
        AssertMsgFailed(("unknown stat %d\n", statcode));
        return;
    }
    STAM_PROFILE_ADV_START(pStat, a);
}


void remR3ProfileStop(int statcode)
{
    STAMPROFILEADV *pStat;
    switch(statcode)
    {
    case STATS_EMULATE_SINGLE_INSTR:
        pStat = &gStatExecuteSingleInstr;
        break;
    case STATS_QEMU_COMPILATION:
        pStat = &gStatCompilationQEmu;
        break;
    case STATS_QEMU_RUN_EMULATED_CODE:
        pStat = &gStatRunCodeQEmu;
        break;
    case STATS_QEMU_TOTAL:
        pStat = &gStatTotalTimeQEmu;
        break;
    case STATS_QEMU_RUN_TIMERS:
        pStat = &gStatTimers;
        break;
    case STATS_TLB_LOOKUP:
        pStat= &gStatTBLookup;
        break;
    case STATS_IRQ_HANDLING:
        pStat= &gStatIRQ;
        break;
    case STATS_RAW_CHECK:
        pStat = &gStatRawCheck;
        break;
    default:
        AssertMsgFailed(("unknown stat %d\n", statcode));
        return;
    }
    STAM_PROFILE_ADV_STOP(pStat, a);
}
#endif

/**
 * Raise an RC, force rem exit.
 *
 * @param   pVM     VM handle.
 * @param   rc      The rc.
 */
void remR3RaiseRC(PVM pVM, int rc)
{
    Log(("remR3RaiseRC: rc=%Vrc\n", rc));
    Assert(pVM->rem.s.fInREM);
    VM_ASSERT_EMT(pVM);
    pVM->rem.s.rc = rc;
    cpu_interrupt(&pVM->rem.s.Env, CPU_INTERRUPT_RC);
}


/* -+- timers -+- */

uint64_t cpu_get_tsc(CPUX86State *env)
{
    STAM_COUNTER_INC(&gStatCpuGetTSC);
    return TMCpuTickGet(env->pVM);
}


/* -+- interrupts -+- */

void cpu_set_ferr(CPUX86State *env)
{
    int rc = PDMIsaSetIrq(env->pVM, 13, 1);
    LogFlow(("cpu_set_ferr: rc=%d\n", rc)); NOREF(rc);
}

int cpu_get_pic_interrupt(CPUState *env)
{
    uint8_t u8Interrupt;
    int     rc;

    /* When we fail to forward interrupts directly in raw mode, we fall back to the recompiler.
     * In that case we can't call PDMGetInterrupt anymore, because it has already cleared the interrupt
     * with the (a)pic.
     */
    /** @note We assume we will go directly to the recompiler to handle the pending interrupt! */
    /** @todo r=bird: In the long run we should just do the interrupt handling in EM/CPUM/TRPM/somewhere and
     * if we cannot execute the interrupt handler in raw-mode just reschedule to REM. Once that is done we
     * remove this kludge. */
    if (env->pVM->rem.s.u32PendingInterrupt != REM_NO_PENDING_IRQ)
    {
        rc = VINF_SUCCESS;
        Assert(env->pVM->rem.s.u32PendingInterrupt >= 0 && env->pVM->rem.s.u32PendingInterrupt <= 255);
        u8Interrupt = env->pVM->rem.s.u32PendingInterrupt;
        env->pVM->rem.s.u32PendingInterrupt = REM_NO_PENDING_IRQ;
    }
    else
        rc = PDMGetInterrupt(env->pVM, &u8Interrupt);

    LogFlow(("cpu_get_pic_interrupt: u8Interrupt=%d rc=%Vrc\n", u8Interrupt, rc));
    if (VBOX_SUCCESS(rc))
    {
        if (VM_FF_ISPENDING(env->pVM, VM_FF_INTERRUPT_APIC | VM_FF_INTERRUPT_PIC))
            env->interrupt_request |= CPU_INTERRUPT_HARD;
        return u8Interrupt;
    }
    return -1;
}


/* -+- local apic -+- */

void cpu_set_apic_base(CPUX86State *env, uint64_t val)
{
    int rc = PDMApicSetBase(env->pVM, val);
    LogFlow(("cpu_set_apic_base: val=%#llx rc=%Vrc\n", val, rc)); NOREF(rc);
}

uint64_t cpu_get_apic_base(CPUX86State *env)
{
    uint64_t u64;
    int rc = PDMApicGetBase(env->pVM, &u64);
    if (VBOX_SUCCESS(rc))
    {
        LogFlow(("cpu_get_apic_base: returns %#llx \n", u64));
        return u64;
    }
    LogFlow(("cpu_get_apic_base: returns 0 (rc=%Vrc)\n", rc));
    return 0;
}

void cpu_set_apic_tpr(CPUX86State *env, uint8_t val)
{
    int rc = PDMApicSetTPR(env->pVM, val);
    LogFlow(("cpu_set_apic_tpr: val=%#x rc=%Vrc\n", val, rc)); NOREF(rc);
}

uint8_t cpu_get_apic_tpr(CPUX86State *env)
{
    uint8_t u8;
    int rc = PDMApicGetTPR(env->pVM, &u8);
    if (VBOX_SUCCESS(rc))
    {
        LogFlow(("cpu_get_apic_tpr: returns %#x\n", u8));
        return u8;
    }
    LogFlow(("cpu_get_apic_tpr: returns 0 (rc=%Vrc)\n", rc));
    return 0;
}


/* -+- I/O Ports -+- */

#undef LOG_GROUP
#define LOG_GROUP LOG_GROUP_REM_IOPORT

void cpu_outb(CPUState *env, int addr, int val)
{
    if (addr != 0x80 && addr != 0x70 && addr != 0x61)
        Log2(("cpu_outb: addr=%#06x val=%#x\n", addr, val));

    int rc = IOMIOPortWrite(env->pVM, (RTIOPORT)addr, val, 1);
    if (RT_LIKELY(rc == VINF_SUCCESS))
        return;
    if (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST)
    {
        Log(("cpu_outb: addr=%#06x val=%#x -> %Vrc\n", addr, val, rc));
        remR3RaiseRC(env->pVM, rc);
        return;
    }
    remAbort(rc, __FUNCTION__);
}

void cpu_outw(CPUState *env, int addr, int val)
{
    //Log2(("cpu_outw: addr=%#06x val=%#x\n", addr, val));
    int rc = IOMIOPortWrite(env->pVM, (RTIOPORT)addr, val, 2);
    if (RT_LIKELY(rc == VINF_SUCCESS))
        return;
    if (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST)
    {
        Log(("cpu_outw: addr=%#06x val=%#x -> %Vrc\n", addr, val, rc));
        remR3RaiseRC(env->pVM, rc);
        return;
    }
    remAbort(rc, __FUNCTION__);
}

void cpu_outl(CPUState *env, int addr, int val)
{
    Log2(("cpu_outl: addr=%#06x val=%#x\n", addr, val));
    int rc = IOMIOPortWrite(env->pVM, (RTIOPORT)addr, val, 4);
    if (RT_LIKELY(rc == VINF_SUCCESS))
        return;
    if (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST)
    {
        Log(("cpu_outl: addr=%#06x val=%#x -> %Vrc\n", addr, val, rc));
        remR3RaiseRC(env->pVM, rc);
        return;
    }
    remAbort(rc, __FUNCTION__);
}

int cpu_inb(CPUState *env, int addr)
{
    uint32_t u32 = 0;
    int rc = IOMIOPortRead(env->pVM, (RTIOPORT)addr, &u32, 1);
    if (RT_LIKELY(rc == VINF_SUCCESS))
    {
        if (/*addr != 0x61 && */addr != 0x71)
            Log2(("cpu_inb: addr=%#06x -> %#x\n", addr, u32));
        return (int)u32;
    }
    if (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST)
    {
        Log(("cpu_inb: addr=%#06x -> %#x rc=%Vrc\n", addr, u32, rc));
        remR3RaiseRC(env->pVM, rc);
        return (int)u32;
    }
    remAbort(rc, __FUNCTION__);
    return 0xff;
}

int cpu_inw(CPUState *env, int addr)
{
    uint32_t u32 = 0;
    int rc = IOMIOPortRead(env->pVM, (RTIOPORT)addr, &u32, 2);
    if (RT_LIKELY(rc == VINF_SUCCESS))
    {
        Log2(("cpu_inw: addr=%#06x -> %#x\n", addr, u32));
        return (int)u32;
    }
    if (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST)
    {
        Log(("cpu_inw: addr=%#06x -> %#x rc=%Vrc\n", addr, u32, rc));
        remR3RaiseRC(env->pVM, rc);
        return (int)u32;
    }
    remAbort(rc, __FUNCTION__);
    return 0xffff;
}

int cpu_inl(CPUState *env, int addr)
{
    uint32_t u32 = 0;
    int rc = IOMIOPortRead(env->pVM, (RTIOPORT)addr, &u32, 4);
    if (RT_LIKELY(rc == VINF_SUCCESS))
    {
//if (addr==0x01f0 && u32 == 0x6b6d)
//    loglevel = ~0;
        Log2(("cpu_inl: addr=%#06x -> %#x\n", addr, u32));
        return (int)u32;
    }
    if (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST)
    {
        Log(("cpu_inl: addr=%#06x -> %#x rc=%Vrc\n", addr, u32, rc));
        remR3RaiseRC(env->pVM, rc);
        return (int)u32;
    }
    remAbort(rc, __FUNCTION__);
    return 0xffffffff;
}

#undef LOG_GROUP
#define LOG_GROUP LOG_GROUP_REM


/* -+- helpers and misc other interfaces -+- */

/**
 * Perform the CPUID instruction.
 *
 * ASMCpuId cannot be invoked from some source files where this is used because of global
 * register allocations.
 *
 * @param   env         Pointer to the recompiler CPU structure.
 * @param   uOperator   CPUID operation (eax).
 * @param   pvEAX       Where to store eax.
 * @param   pvEBX       Where to store ebx.
 * @param   pvECX       Where to store ecx.
 * @param   pvEDX       Where to store edx.
 */
void remR3CpuId(CPUState *env, unsigned uOperator, void *pvEAX, void *pvEBX, void *pvECX, void *pvEDX)
{
    CPUMGetGuestCpuId(env->pVM, uOperator, (uint32_t *)pvEAX, (uint32_t *)pvEBX, (uint32_t *)pvECX, (uint32_t *)pvEDX);
}


#if 0 /* not used */
/**
 * Interface for qemu hardware to report back fatal errors.
 */
void hw_error(const char *pszFormat, ...)
{
    /*
     * Bitch about it.
     */
    /** @todo Add support for nested arg lists in the LogPrintfV routine! I've code for
     * this in my Odin32 tree at home! */
    va_list args;
    va_start(args, pszFormat);
    RTLogPrintf("fatal error in virtual hardware:");
    RTLogPrintfV(pszFormat, args);
    va_end(args);
    AssertReleaseMsgFailed(("fatal error in virtual hardware: %s\n", pszFormat));

    /*
     * If we're in REM context we'll sync back the state before 'jumping' to
     * the EMs failure handling.
     */
    PVM pVM = cpu_single_env->pVM;
    if (pVM->rem.s.fInREM)
        REMR3StateBack(pVM);
    EMR3FatalError(pVM, VERR_REM_VIRTUAL_HARDWARE_ERROR);
    AssertMsgFailed(("EMR3FatalError returned!\n"));
}
#endif

/**
 * Interface for the qemu cpu to report unhandled situation
 * raising a fatal VM error.
 */
void cpu_abort(CPUState *env, const char *pszFormat, ...)
{
    /*
     * Bitch about it.
     */
    RTLogFlags(NULL, "nodisabled nobuffered");
    va_list args;
    va_start(args, pszFormat);
    RTLogPrintf("fatal error in recompiler cpu: %N\n", pszFormat, &args);
    va_end(args);
    va_start(args, pszFormat);
    AssertReleaseMsgFailed(("fatal error in recompiler cpu: %N\n", pszFormat, &args));
    va_end(args);

    /*
     * If we're in REM context we'll sync back the state before 'jumping' to
     * the EMs failure handling.
     */
    PVM pVM = cpu_single_env->pVM;
    if (pVM->rem.s.fInREM)
        REMR3StateBack(pVM);
    EMR3FatalError(pVM, VERR_REM_VIRTUAL_CPU_ERROR);
    AssertMsgFailed(("EMR3FatalError returned!\n"));
}


/**
 * Aborts the VM.
 *
 * @param   rc      VBox error code.
 * @param   pszTip  Hint about why/when this happend.
 */
static void remAbort(int rc, const char *pszTip)
{
    /*
     * Bitch about it.
     */
    RTLogPrintf("internal REM fatal error: rc=%Vrc %s\n", rc, pszTip);
    AssertReleaseMsgFailed(("internal REM fatal error: rc=%Vrc %s\n", rc, pszTip));

    /*
     * Jump back to where we entered the recompiler.
     */
    PVM pVM = cpu_single_env->pVM;
    if (pVM->rem.s.fInREM)
        REMR3StateBack(pVM);
    EMR3FatalError(pVM, rc);
    AssertMsgFailed(("EMR3FatalError returned!\n"));
}


/**
 * Dumps a linux system call.
 * @param   pVM     VM handle.
 */
void remR3DumpLnxSyscall(PVM pVM)
{
    static const char *apsz[] =
    {
    	"sys_restart_syscall",	/* 0 - old "setup()" system call, used for restarting */
    	"sys_exit",
    	"sys_fork",
    	"sys_read",
    	"sys_write",
    	"sys_open",		/* 5 */
    	"sys_close",
    	"sys_waitpid",
    	"sys_creat",
    	"sys_link",
    	"sys_unlink",	/* 10 */
    	"sys_execve",
    	"sys_chdir",
    	"sys_time",
    	"sys_mknod",
    	"sys_chmod",		/* 15 */
    	"sys_lchown16",
    	"sys_ni_syscall",	/* old break syscall holder */
    	"sys_stat",
    	"sys_lseek",
    	"sys_getpid",	/* 20 */
    	"sys_mount",
    	"sys_oldumount",
    	"sys_setuid16",
    	"sys_getuid16",
    	"sys_stime",		/* 25 */
    	"sys_ptrace",
    	"sys_alarm",
    	"sys_fstat",
    	"sys_pause",
    	"sys_utime",		/* 30 */
    	"sys_ni_syscall",	/* old stty syscall holder */
    	"sys_ni_syscall",	/* old gtty syscall holder */
    	"sys_access",
    	"sys_nice",
    	"sys_ni_syscall",	/* 35 - old ftime syscall holder */
    	"sys_sync",
    	"sys_kill",
    	"sys_rename",
    	"sys_mkdir",
    	"sys_rmdir",		/* 40 */
    	"sys_dup",
    	"sys_pipe",
    	"sys_times",
    	"sys_ni_syscall",	/* old prof syscall holder */
    	"sys_brk",		/* 45 */
    	"sys_setgid16",
    	"sys_getgid16",
    	"sys_signal",
    	"sys_geteuid16",
    	"sys_getegid16",	/* 50 */
    	"sys_acct",
    	"sys_umount",	/* recycled never used phys() */
    	"sys_ni_syscall",	/* old lock syscall holder */
    	"sys_ioctl",
    	"sys_fcntl",		/* 55 */
    	"sys_ni_syscall",	/* old mpx syscall holder */
    	"sys_setpgid",
    	"sys_ni_syscall",	/* old ulimit syscall holder */
    	"sys_olduname",
    	"sys_umask",		/* 60 */
    	"sys_chroot",
    	"sys_ustat",
    	"sys_dup2",
    	"sys_getppid",
    	"sys_getpgrp",	/* 65 */
    	"sys_setsid",
    	"sys_sigaction",
    	"sys_sgetmask",
    	"sys_ssetmask",
    	"sys_setreuid16",	/* 70 */
    	"sys_setregid16",
    	"sys_sigsuspend",
    	"sys_sigpending",
    	"sys_sethostname",
    	"sys_setrlimit",	/* 75 */
    	"sys_old_getrlimit",
    	"sys_getrusage",
    	"sys_gettimeofday",
    	"sys_settimeofday",
    	"sys_getgroups16",	/* 80 */
    	"sys_setgroups16",
    	"old_select",
    	"sys_symlink",
    	"sys_lstat",
    	"sys_readlink",	/* 85 */
    	"sys_uselib",
    	"sys_swapon",
    	"sys_reboot",
    	"old_readdir",
    	"old_mmap",		/* 90 */
    	"sys_munmap",
    	"sys_truncate",
    	"sys_ftruncate",
    	"sys_fchmod",
    	"sys_fchown16",	/* 95 */
    	"sys_getpriority",
    	"sys_setpriority",
    	"sys_ni_syscall",	/* old profil syscall holder */
    	"sys_statfs",
    	"sys_fstatfs",	/* 100 */
    	"sys_ioperm",
    	"sys_socketcall",
    	"sys_syslog",
    	"sys_setitimer",
    	"sys_getitimer",	/* 105 */
    	"sys_newstat",
    	"sys_newlstat",
    	"sys_newfstat",
    	"sys_uname",
    	"sys_iopl",		/* 110 */
    	"sys_vhangup",
    	"sys_ni_syscall",	/* old "idle" system call */
    	"sys_vm86old",
    	"sys_wait4",
    	"sys_swapoff",	/* 115 */
    	"sys_sysinfo",
    	"sys_ipc",
    	"sys_fsync",
    	"sys_sigreturn",
    	"sys_clone",		/* 120 */
    	"sys_setdomainname",
    	"sys_newuname",
    	"sys_modify_ldt",
    	"sys_adjtimex",
    	"sys_mprotect",	/* 125 */
    	"sys_sigprocmask",
    	"sys_ni_syscall",	/* old "create_module" */
    	"sys_init_module",
    	"sys_delete_module",
    	"sys_ni_syscall",	/* 130:	old "get_kernel_syms" */
    	"sys_quotactl",
    	"sys_getpgid",
    	"sys_fchdir",
    	"sys_bdflush",
    	"sys_sysfs",		/* 135 */
    	"sys_personality",
    	"sys_ni_syscall",	/* reserved for afs_syscall */
    	"sys_setfsuid16",
    	"sys_setfsgid16",
    	"sys_llseek",	/* 140 */
    	"sys_getdents",
    	"sys_select",
    	"sys_flock",
    	"sys_msync",
    	"sys_readv",		/* 145 */
    	"sys_writev",
    	"sys_getsid",
    	"sys_fdatasync",
    	"sys_sysctl",
    	"sys_mlock",		/* 150 */
    	"sys_munlock",
    	"sys_mlockall",
    	"sys_munlockall",
    	"sys_sched_setparam",
    	"sys_sched_getparam",   /* 155 */
    	"sys_sched_setscheduler",
    	"sys_sched_getscheduler",
    	"sys_sched_yield",
    	"sys_sched_get_priority_max",
    	"sys_sched_get_priority_min",  /* 160 */
    	"sys_sched_rr_get_interval",
    	"sys_nanosleep",
    	"sys_mremap",
    	"sys_setresuid16",
    	"sys_getresuid16",	/* 165 */
    	"sys_vm86",
    	"sys_ni_syscall",	/* Old sys_query_module */
    	"sys_poll",
    	"sys_nfsservctl",
    	"sys_setresgid16",	/* 170 */
    	"sys_getresgid16",
    	"sys_prctl",
    	"sys_rt_sigreturn",
    	"sys_rt_sigaction",
    	"sys_rt_sigprocmask",	/* 175 */
    	"sys_rt_sigpending",
    	"sys_rt_sigtimedwait",
    	"sys_rt_sigqueueinfo",
    	"sys_rt_sigsuspend",
    	"sys_pread64",	/* 180 */
    	"sys_pwrite64",
    	"sys_chown16",
    	"sys_getcwd",
    	"sys_capget",
    	"sys_capset",	/* 185 */
    	"sys_sigaltstack",
    	"sys_sendfile",
    	"sys_ni_syscall",	/* reserved for streams1 */
    	"sys_ni_syscall",	/* reserved for streams2 */
    	"sys_vfork",		/* 190 */
    	"sys_getrlimit",
    	"sys_mmap2",
    	"sys_truncate64",
    	"sys_ftruncate64",
    	"sys_stat64",	/* 195 */
    	"sys_lstat64",
    	"sys_fstat64",
    	"sys_lchown",
    	"sys_getuid",
    	"sys_getgid",	/* 200 */
    	"sys_geteuid",
    	"sys_getegid",
    	"sys_setreuid",
    	"sys_setregid",
    	"sys_getgroups",	/* 205 */
    	"sys_setgroups",
    	"sys_fchown",
    	"sys_setresuid",
    	"sys_getresuid",
    	"sys_setresgid",	/* 210 */
    	"sys_getresgid",
    	"sys_chown",
    	"sys_setuid",
    	"sys_setgid",
    	"sys_setfsuid",	/* 215 */
    	"sys_setfsgid",
    	"sys_pivot_root",
    	"sys_mincore",
    	"sys_madvise",
    	"sys_getdents64",	/* 220 */
    	"sys_fcntl64",
    	"sys_ni_syscall",	/* reserved for TUX */
    	"sys_ni_syscall",
    	"sys_gettid",
    	"sys_readahead",	/* 225 */
    	"sys_setxattr",
    	"sys_lsetxattr",
    	"sys_fsetxattr",
    	"sys_getxattr",
    	"sys_lgetxattr",	/* 230 */
    	"sys_fgetxattr",
    	"sys_listxattr",
    	"sys_llistxattr",
    	"sys_flistxattr",
    	"sys_removexattr",	/* 235 */
    	"sys_lremovexattr",
    	"sys_fremovexattr",
    	"sys_tkill",
    	"sys_sendfile64",
    	"sys_futex",		/* 240 */
    	"sys_sched_setaffinity",
    	"sys_sched_getaffinity",
    	"sys_set_thread_area",
    	"sys_get_thread_area",
    	"sys_io_setup",	/* 245 */
    	"sys_io_destroy",
    	"sys_io_getevents",
    	"sys_io_submit",
    	"sys_io_cancel",
    	"sys_fadvise64",	/* 250 */
    	"sys_ni_syscall",
    	"sys_exit_group",
    	"sys_lookup_dcookie",
    	"sys_epoll_create",
    	"sys_epoll_ctl",	/* 255 */
    	"sys_epoll_wait",
     	"sys_remap_file_pages",
     	"sys_set_tid_address",
     	"sys_timer_create",
     	"sys_timer_settime",		/* 260 */
     	"sys_timer_gettime",
     	"sys_timer_getoverrun",
     	"sys_timer_delete",
     	"sys_clock_settime",
     	"sys_clock_gettime",		/* 265 */
     	"sys_clock_getres",
     	"sys_clock_nanosleep",
    	"sys_statfs64",
    	"sys_fstatfs64",
    	"sys_tgkill",	/* 270 */
    	"sys_utimes",
     	"sys_fadvise64_64",
    	"sys_ni_syscall"	/* sys_vserver */
    };

    uint32_t    uEAX = CPUMGetGuestEAX(pVM);
    switch (uEAX)
    {
        default:
            if (uEAX < ELEMENTS(apsz))
                Log(("REM: linux syscall %3d: %s (eip=%VGv ebx=%08x ecx=%08x edx=%08x esi=%08x edi=%08x ebp=%08x)\n",
                     uEAX, apsz[uEAX], CPUMGetGuestEIP(pVM), CPUMGetGuestEBX(pVM), CPUMGetGuestECX(pVM),
                     CPUMGetGuestEDX(pVM), CPUMGetGuestESI(pVM), CPUMGetGuestEDI(pVM), CPUMGetGuestEBP(pVM)));
            else
                Log(("eip=%08x: linux syscall %d (#%x) unknown\n", CPUMGetGuestEIP(pVM), uEAX, uEAX));
            break;

    }
}


/**
 * Dumps an OpenBSD system call.
 * @param   pVM     VM handle.
 */
void remR3DumpOBsdSyscall(PVM pVM)
{
    static const char *apsz[] =
    {
        "SYS_syscall",		//0
        "SYS_exit",		//1
        "SYS_fork",		//2
        "SYS_read",		//3
        "SYS_write",		//4
        "SYS_open",		//5
        "SYS_close",		//6
        "SYS_wait4",		//7
        "SYS_8",
        "SYS_link",		//9
        "SYS_unlink",		//10
        "SYS_11",
        "SYS_chdir",		//12
        "SYS_fchdir",		//13
        "SYS_mknod",		//14
        "SYS_chmod",		//15
        "SYS_chown",		//16
        "SYS_break",		//17
        "SYS_18",
        "SYS_19",
        "SYS_getpid",		//20
        "SYS_mount",		//21
        "SYS_unmount",		//22
        "SYS_setuid",		//23
        "SYS_getuid",		//24
        "SYS_geteuid",		//25
        "SYS_ptrace",		//26
        "SYS_recvmsg",		//27
        "SYS_sendmsg",		//28
        "SYS_recvfrom",		//29
        "SYS_accept",		//30
        "SYS_getpeername",	//31
        "SYS_getsockname",	//32
        "SYS_access",		//33
        "SYS_chflags",		//34
        "SYS_fchflags",		//35
        "SYS_sync",		//36
        "SYS_kill",		//37
        "SYS_38",
        "SYS_getppid",		//39
        "SYS_40",
        "SYS_dup",		//41
        "SYS_opipe",		//42
        "SYS_getegid",		//43
        "SYS_profil",		//44
        "SYS_ktrace",		//45
        "SYS_sigaction",	//46
        "SYS_getgid",		//47
        "SYS_sigprocmask",	//48
        "SYS_getlogin",		//49
        "SYS_setlogin",		//50
        "SYS_acct",		//51
        "SYS_sigpending",	//52
        "SYS_osigaltstack",	//53
        "SYS_ioctl",		//54
        "SYS_reboot",		//55
        "SYS_revoke",		//56
        "SYS_symlink",		//57
        "SYS_readlink",		//58
        "SYS_execve",		//59
        "SYS_umask",		//60
        "SYS_chroot",		//61
        "SYS_62",
        "SYS_63",
        "SYS_64",
        "SYS_65",
        "SYS_vfork",		//66
        "SYS_67",
        "SYS_68",
        "SYS_sbrk",		//69
        "SYS_sstk",		//70
        "SYS_61",
        "SYS_vadvise",		//72
        "SYS_munmap",		//73
        "SYS_mprotect",		//74
        "SYS_madvise",		//75
        "SYS_76",
        "SYS_77",
        "SYS_mincore",		//78
        "SYS_getgroups",	//79
        "SYS_setgroups",	//80
        "SYS_getpgrp",		//81
        "SYS_setpgid",		//82
        "SYS_setitimer",	//83
        "SYS_84",
        "SYS_85",
        "SYS_getitimer",	//86
        "SYS_87",
        "SYS_88",
        "SYS_89",
        "SYS_dup2",		//90
        "SYS_91",
        "SYS_fcntl",		//92
        "SYS_select",		//93
        "SYS_94",
        "SYS_fsync",		//95
        "SYS_setpriority",	//96
        "SYS_socket",		//97
        "SYS_connect",		//98
        "SYS_99",
        "SYS_getpriority",	//100
        "SYS_101",
        "SYS_102",
        "SYS_sigreturn",	//103
        "SYS_bind",		//104
        "SYS_setsockopt",	//105
        "SYS_listen",		//106
        "SYS_107",
        "SYS_108",
        "SYS_109",
        "SYS_110",
        "SYS_sigsuspend",	//111
        "SYS_112",
        "SYS_113",
        "SYS_114",
        "SYS_115",
        "SYS_gettimeofday",	//116
        "SYS_getrusage",	//117
        "SYS_getsockopt",	//118
        "SYS_119",
        "SYS_readv",		//120
        "SYS_writev",		//121
        "SYS_settimeofday",	//122
        "SYS_fchown",		//123
        "SYS_fchmod",		//124
        "SYS_125",
        "SYS_setreuid",		//126
        "SYS_setregid",		//127
        "SYS_rename",		//128
        "SYS_129",
        "SYS_130",
        "SYS_flock",		//131
        "SYS_mkfifo",		//132
        "SYS_sendto",		//133
        "SYS_shutdown",		//134
        "SYS_socketpair",	//135
        "SYS_mkdir",		//136
        "SYS_rmdir",		//137
        "SYS_utimes",		//138
        "SYS_139",
        "SYS_adjtime",		//140
        "SYS_141",
        "SYS_142",
        "SYS_143",
        "SYS_144",
        "SYS_145",
        "SYS_146",
        "SYS_setsid",		//147
        "SYS_quotactl",		//148
        "SYS_149",
        "SYS_150",
        "SYS_151",
        "SYS_152",
        "SYS_153",
        "SYS_154",
        "SYS_nfssvc",		//155
        "SYS_156",
        "SYS_157",
        "SYS_158",
        "SYS_159",
        "SYS_160",
        "SYS_getfh",		//161
        "SYS_162",
        "SYS_163",
        "SYS_164",
        "SYS_sysarch",		//165
        "SYS_166",
        "SYS_167",
        "SYS_168",
        "SYS_169",
        "SYS_170",
        "SYS_171",
        "SYS_172",
        "SYS_pread",		//173
        "SYS_pwrite",		//174
        "SYS_175",
        "SYS_176",
        "SYS_177",
        "SYS_178",
        "SYS_179",
        "SYS_180",
        "SYS_setgid",		//181
        "SYS_setegid",		//182
        "SYS_seteuid",		//183
        "SYS_lfs_bmapv",	//184
        "SYS_lfs_markv",	//185
        "SYS_lfs_segclean",	//186
        "SYS_lfs_segwait",	//187
        "SYS_188",
        "SYS_189",
        "SYS_190",
        "SYS_pathconf",		//191
        "SYS_fpathconf",	//192
        "SYS_swapctl",		//193
        "SYS_getrlimit",	//194
        "SYS_setrlimit",	//195
        "SYS_getdirentries",	//196
        "SYS_mmap",		//197
        "SYS___syscall",        //198
        "SYS_lseek",		//199
        "SYS_truncate",		//200
        "SYS_ftruncate",	//201
        "SYS___sysctl",         //202
        "SYS_mlock",		//203
        "SYS_munlock",		//204
        "SYS_205",
        "SYS_futimes",		//206
        "SYS_getpgid",		//207
        "SYS_xfspioctl",	//208
        "SYS_209",
        "SYS_210",
        "SYS_211",
        "SYS_212",
        "SYS_213",
        "SYS_214",
        "SYS_215",
        "SYS_216",
        "SYS_217",
        "SYS_218",
        "SYS_219",
        "SYS_220",
        "SYS_semget",		//221
        "SYS_222",
        "SYS_223",
        "SYS_224",
        "SYS_msgget",		//225
        "SYS_msgsnd",		//226
        "SYS_msgrcv",		//227
        "SYS_shmat",		//228
        "SYS_229",
        "SYS_shmdt",		//230
        "SYS_231",
        "SYS_clock_gettime",	//232
        "SYS_clock_settime",	//233
        "SYS_clock_getres",	//234
        "SYS_235",
        "SYS_236",
        "SYS_237",
        "SYS_238",
        "SYS_239",
        "SYS_nanosleep",	//240
        "SYS_241",
        "SYS_242",
        "SYS_243",
        "SYS_244",
        "SYS_245",
        "SYS_246",
        "SYS_247",
        "SYS_248",
        "SYS_249",
        "SYS_minherit",		//250
        "SYS_rfork",		//251
        "SYS_poll",		//252
        "SYS_issetugid",	//253
        "SYS_lchown",		//254
        "SYS_getsid",		//255
        "SYS_msync",		//256
        "SYS_257",
        "SYS_258",
        "SYS_259",
        "SYS_getfsstat",	//260
        "SYS_statfs",		//261
        "SYS_fstatfs",		//262
        "SYS_pipe",		//263
        "SYS_fhopen",		//264
        "SYS_265",
        "SYS_fhstatfs",		//266
        "SYS_preadv",		//267
        "SYS_pwritev",		//268
        "SYS_kqueue",		//269
        "SYS_kevent",		//270
        "SYS_mlockall",		//271
        "SYS_munlockall",	//272
        "SYS_getpeereid",	//273
        "SYS_274",
        "SYS_275",
        "SYS_276",
        "SYS_277",
        "SYS_278",
        "SYS_279",
        "SYS_280",
        "SYS_getresuid",	//281
        "SYS_setresuid",	//282
        "SYS_getresgid",	//283
        "SYS_setresgid",	//284
        "SYS_285",
        "SYS_mquery",		//286
        "SYS_closefrom",	//287
        "SYS_sigaltstack",	//288
        "SYS_shmget",		//289
        "SYS_semop",		//290
        "SYS_stat",		//291
        "SYS_fstat",		//292
        "SYS_lstat",		//293
        "SYS_fhstat",		//294
        "SYS___semctl",         //295
        "SYS_shmctl",		//296
        "SYS_msgctl",		//297
        "SYS_MAXSYSCALL",	//298
        //299
        //300
    };
    uint32_t    uEAX;
    if (!LogIsEnabled())
        return;
    uEAX = CPUMGetGuestEAX(pVM);
    switch (uEAX)
    {
        default:
            if (uEAX < ELEMENTS(apsz))
            {
                uint32_t au32Args[8] = {0};
                PGMPhysReadGCPtr(pVM, au32Args, CPUMGetGuestESP(pVM), sizeof(au32Args));
                RTLogPrintf("REM: OpenBSD syscall %3d: %s (eip=%08x %08x %08x %08x %08x %08x %08x %08x %08x)\n",
                            uEAX, apsz[uEAX], CPUMGetGuestEIP(pVM), au32Args[0], au32Args[1], au32Args[2], au32Args[3],
                            au32Args[4], au32Args[5], au32Args[6], au32Args[7]);
            }
            else
                RTLogPrintf("eip=%08x: OpenBSD syscall %d (#%x) unknown!!\n", CPUMGetGuestEIP(pVM), uEAX, uEAX);
            break;
    }
}


#if defined(IPRT_NO_CRT) && defined(RT_OS_WINDOWS) && defined(RT_ARCH_X86)
/**
 * The Dll main entry point (stub).
 */
bool __stdcall _DllMainCRTStartup(void *hModule, uint32_t dwReason, void *pvReserved)
{
    return true;
}

void *memcpy(void *dst, const void *src, size_t size)
{
    uint8_t*pbDst = dst, *pbSrc = src;
    while (size-- > 0)
        *pbDst++ = *pbSrc++;
    return dst;
}

#endif

