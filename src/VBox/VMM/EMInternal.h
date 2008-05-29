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
 * Converts a EM pointer into a VM pointer.
 * @returns Pointer to the VM structure the EM is part of.
 * @param   pEM   Pointer to EM instance data.
 */
#define EM2VM(pEM)  ( (PVM)((char*)pEM - pEM->offVM) )

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
    STAMPROFILE             StatGCEmulate;
    /** HC: Profiling of EMInterpretInstruction(). */
    STAMPROFILE             StatHCEmulate;

    /** @name Interpreter Instruction statistics.
     * @{
     */
    STAMCOUNTER             StatGCInterpretSucceeded;
    STAMCOUNTER             StatHCInterpretSucceeded;

    STAMCOUNTER             StatGCAnd;
    STAMCOUNTER             StatHCAnd;
    STAMCOUNTER             StatGCCpuId;
    STAMCOUNTER             StatHCCpuId;
    STAMCOUNTER             StatGCDec;
    STAMCOUNTER             StatHCDec;
    STAMCOUNTER             StatGCHlt;
    STAMCOUNTER             StatHCHlt;
    STAMCOUNTER             StatGCInc;
    STAMCOUNTER             StatHCInc;
    STAMCOUNTER             StatGCInvlPg;
    STAMCOUNTER             StatHCInvlPg;
    STAMCOUNTER             StatGCIret;
    STAMCOUNTER             StatHCIret;
    STAMCOUNTER             StatGCLLdt;
    STAMCOUNTER             StatHCLLdt;
    STAMCOUNTER             StatGCMov;
    STAMCOUNTER             StatHCMov;
    STAMCOUNTER             StatGCMovCRx;
    STAMCOUNTER             StatHCMovCRx;
    STAMCOUNTER             StatGCMovDRx;
    STAMCOUNTER             StatHCMovDRx;
    STAMCOUNTER             StatGCOr;
    STAMCOUNTER             StatHCOr;
    STAMCOUNTER             StatGCPop;
    STAMCOUNTER             StatHCPop;
    STAMCOUNTER             StatGCSti;
    STAMCOUNTER             StatHCSti;
    STAMCOUNTER             StatGCXchg;
    STAMCOUNTER             StatHCXchg;
    STAMCOUNTER             StatGCXor;
    STAMCOUNTER             StatHCXor;
    STAMCOUNTER             StatGCMonitor;
    STAMCOUNTER             StatHCMonitor;
    STAMCOUNTER             StatGCMWait;
    STAMCOUNTER             StatHCMWait;
    STAMCOUNTER             StatGCAdd;
    STAMCOUNTER             StatHCAdd;
    STAMCOUNTER             StatGCSub;
    STAMCOUNTER             StatHCSub;
    STAMCOUNTER             StatGCAdc;
    STAMCOUNTER             StatHCAdc;
    STAMCOUNTER             StatGCRdtsc;
    STAMCOUNTER             StatHCRdtsc;
    STAMCOUNTER             StatGCBtr;
    STAMCOUNTER             StatHCBtr;
    STAMCOUNTER             StatGCBts;
    STAMCOUNTER             StatHCBts;
    STAMCOUNTER             StatGCBtc;
    STAMCOUNTER             StatHCBtc;
    STAMCOUNTER             StatGCCmpXchg;
    STAMCOUNTER             StatHCCmpXchg;
    STAMCOUNTER             StatGCCmpXchg8b;
    STAMCOUNTER             StatHCCmpXchg8b;
    STAMCOUNTER             StatGCXAdd;
    STAMCOUNTER             StatHCXAdd;
    STAMCOUNTER             StatGCClts;
    STAMCOUNTER             StatHCClts;
    STAMCOUNTER             StatHCRdmsr;
    STAMCOUNTER             StatHCWrmsr;
    STAMCOUNTER             StatGCRdmsr;
    STAMCOUNTER             StatGCWrmsr;

    STAMCOUNTER             StatGCInterpretFailed;
    STAMCOUNTER             StatHCInterpretFailed;

    STAMCOUNTER             StatGCFailedAnd;
    STAMCOUNTER             StatHCFailedAnd;
    STAMCOUNTER             StatGCFailedCpuId;
    STAMCOUNTER             StatHCFailedCpuId;
    STAMCOUNTER             StatGCFailedDec;
    STAMCOUNTER             StatHCFailedDec;
    STAMCOUNTER             StatGCFailedHlt;
    STAMCOUNTER             StatHCFailedHlt;
    STAMCOUNTER             StatGCFailedInc;
    STAMCOUNTER             StatHCFailedInc;
    STAMCOUNTER             StatGCFailedInvlPg;
    STAMCOUNTER             StatHCFailedInvlPg;
    STAMCOUNTER             StatGCFailedIret;
    STAMCOUNTER             StatHCFailedIret;
    STAMCOUNTER             StatGCFailedLLdt;
    STAMCOUNTER             StatHCFailedLLdt;
    STAMCOUNTER             StatGCFailedMisc;
    STAMCOUNTER             StatHCFailedMisc;
    STAMCOUNTER             StatGCFailedMov;
    STAMCOUNTER             StatHCFailedMov;
    STAMCOUNTER             StatGCFailedMovCRx;
    STAMCOUNTER             StatHCFailedMovCRx;
    STAMCOUNTER             StatGCFailedMovDRx;
    STAMCOUNTER             StatHCFailedMovDRx;
    STAMCOUNTER             StatGCFailedOr;
    STAMCOUNTER             StatHCFailedOr;
    STAMCOUNTER             StatGCFailedPop;
    STAMCOUNTER             StatHCFailedPop;
    STAMCOUNTER             StatGCFailedSti;
    STAMCOUNTER             StatHCFailedSti;
    STAMCOUNTER             StatGCFailedXchg;
    STAMCOUNTER             StatHCFailedXchg;
    STAMCOUNTER             StatGCFailedXor;
    STAMCOUNTER             StatHCFailedXor;
    STAMCOUNTER             StatGCFailedMonitor;
    STAMCOUNTER             StatHCFailedMonitor;
    STAMCOUNTER             StatGCFailedMWait;
    STAMCOUNTER             StatHCFailedMWait;
    STAMCOUNTER             StatHCFailedRdmsr;
    STAMCOUNTER             StatHCFailedWrmsr;
    STAMCOUNTER             StatGCFailedRdmsr;
    STAMCOUNTER             StatGCFailedWrmsr;

    STAMCOUNTER             StatGCFailedAdd;
    STAMCOUNTER             StatHCFailedAdd;
    STAMCOUNTER             StatGCFailedAdc;
    STAMCOUNTER             StatHCFailedAdc;
    STAMCOUNTER             StatGCFailedBtr;
    STAMCOUNTER             StatHCFailedBtr;
    STAMCOUNTER             StatGCFailedBts;
    STAMCOUNTER             StatHCFailedBts;
    STAMCOUNTER             StatGCFailedBtc;
    STAMCOUNTER             StatHCFailedBtc;
    STAMCOUNTER             StatGCFailedCli;
    STAMCOUNTER             StatHCFailedCli;
    STAMCOUNTER             StatGCFailedCmpXchg;
    STAMCOUNTER             StatHCFailedCmpXchg;
    STAMCOUNTER             StatGCFailedCmpXchg8b;
    STAMCOUNTER             StatHCFailedCmpXchg8b;
    STAMCOUNTER             StatGCFailedXAdd;
    STAMCOUNTER             StatHCFailedXAdd;
    STAMCOUNTER             StatHCFailedMovNTPS;
    STAMCOUNTER             StatGCFailedMovNTPS;
    STAMCOUNTER             StatGCFailedStosWD;
    STAMCOUNTER             StatHCFailedStosWD;
    STAMCOUNTER             StatGCFailedSub;
    STAMCOUNTER             StatHCFailedSub;
    STAMCOUNTER             StatGCFailedWbInvd;
    STAMCOUNTER             StatHCFailedWbInvd;
    STAMCOUNTER             StatGCFailedRdtsc;
    STAMCOUNTER             StatHCFailedRdtsc;
    STAMCOUNTER             StatGCFailedClts;
    STAMCOUNTER             StatHCFailedClts;

    STAMCOUNTER             StatGCFailedUserMode;
    STAMCOUNTER             StatHCFailedUserMode;
    STAMCOUNTER             StatGCFailedPrefix;
    STAMCOUNTER             StatHCFailedPrefix;
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

} EMSTATS, *PEMSTATS;

/**
 * EM VM Instance data.
 * Changes to this must checked against the padding of the cfgm union in VM!
 */
typedef struct EM
{
    /** Offset to the VM structure.
     * See EM2VM(). */
    RTUINT                  offVM;

    /** Execution Manager State. */
    volatile EMSTATE        enmState;
    /** Force raw-mode execution.
     * This is used to prevent REM from trying to execute patch code.
     * The flag is cleared upon entering emR3RawExecute() and updated in certain return paths. */
    bool                    fForceRAW;

#ifdef DEBUG_TRACING_ENABLED
    /** @see DEBUG_TRACING_ENABLED */
    bool                    fTracing;
#endif

    /** Inhibit interrupts for this instruction. Valid only when VM_FF_INHIBIT_INTERRUPTS is set. */
    RTGCUINTPTR             GCPtrInhibitInterrupts;


    /** Pointer to the PATM status structure. (HC Ptr) */
    R3R0PTRTYPE(PPATMGCSTATE) pPatmGCState;

    /** Pointer to the guest CPUM state. (HC Ptr) */
    R3R0PTRTYPE(PCPUMCTX)     pCtx;

#if GC_ARCH_BITS == 64
    RTGCPTR                   aPadding1;
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

    /** HC: Profiling of emR3RawExecuteIOInstruction. */
    STAMPROFILE             StatIOEmu;
    /** HC: Profiling of emR3RawPrivileged. */
    STAMPROFILE             StatPrivEmu;
    /** HC: Profiling of emR3RawExecuteInstruction. */
    STAMPROFILE             StatMiscEmu;

    /** @todo r=bird: Are any of these actually used? */
    STAMCOUNTER             StatPatchTrap;
    STAMCOUNTER             StatPatchInt3;
    STAMCOUNTER             StatPatchIF0;
    STAMCOUNTER             StatPatchEmulate;
    STAMCOUNTER             StatPageOutOfSync;
    STAMCOUNTER             StatHwAccExecuteEntry;

    /** More statistics (HC). */
    R3R0PTRTYPE(PEMSTATS)   pStatsHC;
    /** More statistics (GC). */
    RCPTRTYPE(PEMSTATS)     pStatsGC;
#if HC_ARCH_BITS != GC_ARCH_BITS && GC_ARCH_BITS == 32
    RTGCPTR                 padding0;
#endif

    /** Tree for keeping track of cli occurances (debug only). */
    R3PTRTYPE(PAVLPVNODECORE) pCliStatTree;
    STAMCOUNTER             StatTotalClis;
#if 0
    /** 64-bit Visual C++ rounds the struct size up to 16 byte. */
    uint64_t                padding1;
#endif

} EM;
/** Pointer to EM VM instance data. */
typedef EM *PEM;



/** @} */

__END_DECLS

#endif

