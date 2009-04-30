/* $Id$ */
/** @file
 * EM - Internal header file.
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

#ifndef ___EMInternal_h
#define ___EMInternal_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/em.h>
#include <VBox/stam.h>
#include <VBox/patm.h>
#include <VBox/dis.h>
#include <VBox/pdmcritsect.h>
#include <iprt/avl.h>
#include <setjmp.h>

__BEGIN_DECLS


/** @defgroup grp_em_int       Internal
 * @ingroup grp_em
 * @internal
 * @{
 */

/** The saved state version. */
#define EM_SAVED_STATE_VERSION                      2

/** Enable for tracing in raw mode.
 * @remark SvL: debugging help primarily for myself. */
#define DEBUG_TRACING_ENABLED

/**
 * Cli node structure
 */
typedef struct CLISTAT
{
    /** The key is the cli address. */
    AVLPVNODECORE           Core;
    /** Occurrences. */
    STAMCOUNTER             Counter;
} CLISTAT, *PCLISTAT;


/**
 * Excessive EM statistics.
 */
typedef struct EMSTATS
{
    /** GC: Profiling of EMInterpretInstruction(). */
    STAMPROFILE             StatRZEmulate;
    /** HC: Profiling of EMInterpretInstruction(). */
    STAMPROFILE             StatR3Emulate;

    /** @name Interpreter Instruction statistics.
     * @{
     */
    STAMCOUNTER             StatRZInterpretSucceeded;
    STAMCOUNTER             StatR3InterpretSucceeded;

    STAMCOUNTER             StatRZAnd;
    STAMCOUNTER             StatR3And;
    STAMCOUNTER             StatRZCpuId;
    STAMCOUNTER             StatR3CpuId;
    STAMCOUNTER             StatRZDec;
    STAMCOUNTER             StatR3Dec;
    STAMCOUNTER             StatRZHlt;
    STAMCOUNTER             StatR3Hlt;
    STAMCOUNTER             StatRZInc;
    STAMCOUNTER             StatR3Inc;
    STAMCOUNTER             StatRZInvlPg;
    STAMCOUNTER             StatR3InvlPg;
    STAMCOUNTER             StatRZIret;
    STAMCOUNTER             StatR3Iret;
    STAMCOUNTER             StatRZLLdt;
    STAMCOUNTER             StatR3LLdt;
    STAMCOUNTER             StatRZLIdt;
    STAMCOUNTER             StatR3LIdt;
    STAMCOUNTER             StatRZLGdt;
    STAMCOUNTER             StatR3LGdt;
    STAMCOUNTER             StatRZMov;
    STAMCOUNTER             StatR3Mov;
    STAMCOUNTER             StatRZMovCRx;
    STAMCOUNTER             StatR3MovCRx;
    STAMCOUNTER             StatRZMovDRx;
    STAMCOUNTER             StatR3MovDRx;
    STAMCOUNTER             StatRZOr;
    STAMCOUNTER             StatR3Or;
    STAMCOUNTER             StatRZPop;
    STAMCOUNTER             StatR3Pop;
    STAMCOUNTER             StatRZSti;
    STAMCOUNTER             StatR3Sti;
    STAMCOUNTER             StatRZXchg;
    STAMCOUNTER             StatR3Xchg;
    STAMCOUNTER             StatRZXor;
    STAMCOUNTER             StatR3Xor;
    STAMCOUNTER             StatRZMonitor;
    STAMCOUNTER             StatR3Monitor;
    STAMCOUNTER             StatRZMWait;
    STAMCOUNTER             StatR3MWait;
    STAMCOUNTER             StatRZAdd;
    STAMCOUNTER             StatR3Add;
    STAMCOUNTER             StatRZSub;
    STAMCOUNTER             StatR3Sub;
    STAMCOUNTER             StatRZAdc;
    STAMCOUNTER             StatR3Adc;
    STAMCOUNTER             StatRZRdtsc;
    STAMCOUNTER             StatR3Rdtsc;
    STAMCOUNTER             StatRZRdpmc;
    STAMCOUNTER             StatR3Rdpmc;
    STAMCOUNTER             StatRZBtr;
    STAMCOUNTER             StatR3Btr;
    STAMCOUNTER             StatRZBts;
    STAMCOUNTER             StatR3Bts;
    STAMCOUNTER             StatRZBtc;
    STAMCOUNTER             StatR3Btc;
    STAMCOUNTER             StatRZCmpXchg;
    STAMCOUNTER             StatR3CmpXchg;
    STAMCOUNTER             StatRZCmpXchg8b;
    STAMCOUNTER             StatR3CmpXchg8b;
    STAMCOUNTER             StatRZXAdd;
    STAMCOUNTER             StatR3XAdd;
    STAMCOUNTER             StatRZClts;
    STAMCOUNTER             StatR3Clts;
    STAMCOUNTER             StatRZStosWD;
    STAMCOUNTER             StatR3StosWD;
    STAMCOUNTER             StatR3Rdmsr;
    STAMCOUNTER             StatR3Wrmsr;
    STAMCOUNTER             StatRZRdmsr;
    STAMCOUNTER             StatRZWrmsr;
    STAMCOUNTER             StatRZWbInvd;
    STAMCOUNTER             StatR3WbInvd;
    STAMCOUNTER             StatRZLmsw;
    STAMCOUNTER             StatR3Lmsw;
    STAMCOUNTER             StatRZSmsw;
    STAMCOUNTER             StatR3Smsw;

    STAMCOUNTER             StatRZInterpretFailed;
    STAMCOUNTER             StatR3InterpretFailed;

    STAMCOUNTER             StatRZFailedAnd;
    STAMCOUNTER             StatR3FailedAnd;
    STAMCOUNTER             StatRZFailedCpuId;
    STAMCOUNTER             StatR3FailedCpuId;
    STAMCOUNTER             StatRZFailedDec;
    STAMCOUNTER             StatR3FailedDec;
    STAMCOUNTER             StatRZFailedHlt;
    STAMCOUNTER             StatR3FailedHlt;
    STAMCOUNTER             StatRZFailedInc;
    STAMCOUNTER             StatR3FailedInc;
    STAMCOUNTER             StatRZFailedInvlPg;
    STAMCOUNTER             StatR3FailedInvlPg;
    STAMCOUNTER             StatRZFailedIret;
    STAMCOUNTER             StatR3FailedIret;
    STAMCOUNTER             StatRZFailedLLdt;
    STAMCOUNTER             StatR3FailedLLdt;
    STAMCOUNTER             StatRZFailedLGdt;
    STAMCOUNTER             StatR3FailedLGdt;
    STAMCOUNTER             StatRZFailedLIdt;
    STAMCOUNTER             StatR3FailedLIdt;
    STAMCOUNTER             StatRZFailedMisc;
    STAMCOUNTER             StatR3FailedMisc;
    STAMCOUNTER             StatRZFailedMov;
    STAMCOUNTER             StatR3FailedMov;
    STAMCOUNTER             StatRZFailedMovCRx;
    STAMCOUNTER             StatR3FailedMovCRx;
    STAMCOUNTER             StatRZFailedMovDRx;
    STAMCOUNTER             StatR3FailedMovDRx;
    STAMCOUNTER             StatRZFailedOr;
    STAMCOUNTER             StatR3FailedOr;
    STAMCOUNTER             StatRZFailedPop;
    STAMCOUNTER             StatR3FailedPop;
    STAMCOUNTER             StatRZFailedSti;
    STAMCOUNTER             StatR3FailedSti;
    STAMCOUNTER             StatRZFailedXchg;
    STAMCOUNTER             StatR3FailedXchg;
    STAMCOUNTER             StatRZFailedXor;
    STAMCOUNTER             StatR3FailedXor;
    STAMCOUNTER             StatRZFailedMonitor;
    STAMCOUNTER             StatR3FailedMonitor;
    STAMCOUNTER             StatRZFailedMWait;
    STAMCOUNTER             StatR3FailedMWait;
    STAMCOUNTER             StatR3FailedRdmsr;
    STAMCOUNTER             StatR3FailedWrmsr;
    STAMCOUNTER             StatRZFailedRdmsr;
    STAMCOUNTER             StatRZFailedWrmsr;
    STAMCOUNTER             StatRZFailedLmsw;
    STAMCOUNTER             StatR3FailedLmsw;
    STAMCOUNTER             StatRZFailedSmsw;
    STAMCOUNTER             StatR3FailedSmsw;

    STAMCOUNTER             StatRZFailedAdd;
    STAMCOUNTER             StatR3FailedAdd;
    STAMCOUNTER             StatRZFailedAdc;
    STAMCOUNTER             StatR3FailedAdc;
    STAMCOUNTER             StatRZFailedBtr;
    STAMCOUNTER             StatR3FailedBtr;
    STAMCOUNTER             StatRZFailedBts;
    STAMCOUNTER             StatR3FailedBts;
    STAMCOUNTER             StatRZFailedBtc;
    STAMCOUNTER             StatR3FailedBtc;
    STAMCOUNTER             StatRZFailedCli;
    STAMCOUNTER             StatR3FailedCli;
    STAMCOUNTER             StatRZFailedCmpXchg;
    STAMCOUNTER             StatR3FailedCmpXchg;
    STAMCOUNTER             StatRZFailedCmpXchg8b;
    STAMCOUNTER             StatR3FailedCmpXchg8b;
    STAMCOUNTER             StatRZFailedXAdd;
    STAMCOUNTER             StatR3FailedXAdd;
    STAMCOUNTER             StatR3FailedMovNTPS;
    STAMCOUNTER             StatRZFailedMovNTPS;
    STAMCOUNTER             StatRZFailedStosWD;
    STAMCOUNTER             StatR3FailedStosWD;
    STAMCOUNTER             StatRZFailedSub;
    STAMCOUNTER             StatR3FailedSub;
    STAMCOUNTER             StatRZFailedWbInvd;
    STAMCOUNTER             StatR3FailedWbInvd;
    STAMCOUNTER             StatRZFailedRdtsc;
    STAMCOUNTER             StatR3FailedRdtsc;
    STAMCOUNTER             StatRZFailedRdpmc;
    STAMCOUNTER             StatR3FailedRdpmc;
    STAMCOUNTER             StatRZFailedClts;
    STAMCOUNTER             StatR3FailedClts;

    STAMCOUNTER             StatRZFailedUserMode;
    STAMCOUNTER             StatR3FailedUserMode;
    STAMCOUNTER             StatRZFailedPrefix;
    STAMCOUNTER             StatR3FailedPrefix;
    /** @} */

    /** @name Privileged Instructions Ending Up In HC.
     * @{ */
    STAMCOUNTER             StatCli;
    STAMCOUNTER             StatSti;
    STAMCOUNTER             StatIn;
    STAMCOUNTER             StatOut;
    STAMCOUNTER             StatInvlpg;
    STAMCOUNTER             StatHlt;
    STAMCOUNTER             StatMovReadCR[USE_REG_CR4 + 1];
    STAMCOUNTER             StatMovWriteCR[USE_REG_CR4 + 1];
    STAMCOUNTER             StatMovDRx;
    STAMCOUNTER             StatIret;
    STAMCOUNTER             StatMovLgdt;
    STAMCOUNTER             StatMovLldt;
    STAMCOUNTER             StatMovLidt;
    STAMCOUNTER             StatMisc;
    STAMCOUNTER             StatSysEnter;
    STAMCOUNTER             StatSysExit;
    STAMCOUNTER             StatSysCall;
    STAMCOUNTER             StatSysRet;
    /** @} */

} EMSTATS;
/** Pointer to the excessive EM statistics. */
typedef EMSTATS *PEMSTATS;


/**
 * Converts a EM pointer into a VM pointer.
 * @returns Pointer to the VM structure the EM is part of.
 * @param   pEM   Pointer to EM instance data.
 */
#define EM2VM(pEM)  ( (PVM)((char*)pEM - pEM->offVM) )

/**
 * EM VM Instance data.
 * Changes to this must checked against the padding of the cfgm union in VM!
 */
typedef struct EM
{
    /** Offset to the VM structure.
     * See EM2VM(). */
    RTUINT                  offVM;

    /** Id of the VCPU that last executed code in the recompiler. */
    VMCPUID                 idLastRemCpu;

    /** PGM critical section.
     * This protects recompiler usage
     */
    PDMCRITSECT             CritSectREM;
} EM;
/** Pointer to EM VM instance data. */
typedef EM *PEM;


/**
 * EM VMCPU Instance data.
 */
typedef struct EMCPU
{
    /** Offset to the VM structure.
     * See EMCPU2VM(). */
    RTUINT                  offVMCPU;

    /** Execution Manager State. */
    EMSTATE volatile        enmState;
    /** Force raw-mode execution.
     * This is used to prevent REM from trying to execute patch code.
     * The flag is cleared upon entering emR3RawExecute() and updated in certain return paths. */
    bool                    fForceRAW;

#ifdef DEBUG_TRACING_ENABLED
    /** @see DEBUG_TRACING_ENABLED */
    bool                    fTracing;
#endif

    uint8_t                 u8Padding[GC_ARCH_BITS == 64 ? 6 : 2];

    /** Inhibit interrupts for this instruction. Valid only when VM_FF_INHIBIT_INTERRUPTS is set. */
    RTGCUINTPTR             GCPtrInhibitInterrupts;


    /** Pointer to the PATM status structure. (R3 Ptr) */
    R3PTRTYPE(PPATMGCSTATE) pPatmGCState;

    /** Pointer to the guest CPUM state. (R3 Ptr) */
    R3PTRTYPE(PCPUMCTX)     pCtx;

#if GC_ARCH_BITS == 64
    RTGCPTR                 aPadding1;
#endif

    union
    {
        /** Padding used in the other rings.
         * This must be larger than jmp_buf on any supported platform. */
        char                achPaddingFatalLongJump[HC_ARCH_BITS == 32 ? 176 : 256];
#ifdef IN_RING3
        /** Long buffer jump for fatal VM errors.
         * It will jump to before the outer EM loop is entered. */
        jmp_buf             FatalLongJump;
#endif
    } u;

    /** @name Execution profiling.
     * @{ */
    STAMPROFILE             StatForcedActions;
    STAMPROFILE             StatHalted;
    STAMPROFILEADV          StatHwAccEntry;
    STAMPROFILE             StatHwAccExec;
    STAMPROFILE             StatREMEmu;
    STAMPROFILE             StatREMExec;
    STAMPROFILE             StatREMSync;
    STAMPROFILEADV          StatREMTotal;
    STAMPROFILE             StatRAWExec;
    STAMPROFILEADV          StatRAWEntry;
    STAMPROFILEADV          StatRAWTail;
    STAMPROFILEADV          StatRAWTotal;
    STAMPROFILEADV          StatTotal;
    /** @} */

    /** R3: Profiling of emR3RawExecuteIOInstruction. */
    STAMPROFILE             StatIOEmu;
    /** R3: Profiling of emR3RawPrivileged. */
    STAMPROFILE             StatPrivEmu;
    /** R3: Profiling of emR3RawExecuteInstruction. */
    STAMPROFILE             StatMiscEmu;
    /** R3: Number of time emR3HwAccExecute is called. */
    STAMCOUNTER             StatHwAccExecuteEntry;

    /** More statistics (R3). */
    R3PTRTYPE(PEMSTATS)     pStatsR3;
    /** More statistics (R0). */
    R0PTRTYPE(PEMSTATS)     pStatsR0;
    /** More statistics (RC). */
    RCPTRTYPE(PEMSTATS)     pStatsRC;
#if HC_ARCH_BITS == 64
    RTRCPTR                 padding0;
#endif

    /** Tree for keeping track of cli occurances (debug only). */
    R3PTRTYPE(PAVLPVNODECORE)   pCliStatTree;
    STAMCOUNTER             StatTotalClis;
#if 0
    /** 64-bit Visual C++ rounds the struct size up to 16 byte. */
    uint64_t                padding1;
#endif
} EMCPU;
/** Pointer to EM VM instance data. */
typedef EMCPU *PEMCPU;

/** @} */

__END_DECLS

#endif

