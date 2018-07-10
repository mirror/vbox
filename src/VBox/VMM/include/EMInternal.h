/* $Id$ */
/** @file
 * EM - Internal header file.
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

#ifndef ___EMInternal_h
#define ___EMInternal_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/stam.h>
#include <VBox/vmm/patm.h>
#include <VBox/dis.h>
#include <VBox/vmm/pdmcritsect.h>
#include <iprt/avl.h>
#include <setjmp.h>

RT_C_DECLS_BEGIN


/** @defgroup grp_em_int       Internal
 * @ingroup grp_em
 * @internal
 * @{
 */

/** The saved state version. */
#define EM_SAVED_STATE_VERSION                          5
#define EM_SAVED_STATE_VERSION_PRE_IEM                  4
#define EM_SAVED_STATE_VERSION_PRE_MWAIT                3
#define EM_SAVED_STATE_VERSION_PRE_SMP                  2


/** @name MWait state flags.
 * @{
 */
/** MWait activated. */
#define EMMWAIT_FLAG_ACTIVE             RT_BIT(0)
/** MWait will continue when an interrupt is pending even when IF=0. */
#define EMMWAIT_FLAG_BREAKIRQIF0        RT_BIT(1)
/** Monitor instruction was executed previously. */
#define EMMWAIT_FLAG_MONITOR_ACTIVE     RT_BIT(2)
/** @} */

/** EM time slice in ms; used for capping execution time. */
#define EM_TIME_SLICE                   100

/**
 * Cli node structure
 */
typedef struct CLISTAT
{
    /** The key is the cli address. */
    AVLGCPTRNODECORE        Core;
#if HC_ARCH_BITS == 32 && !defined(RT_OS_WINDOWS)
    /** Padding. */
    uint32_t                u32Padding;
#endif
    /** Occurrences. */
    STAMCOUNTER             Counter;
} CLISTAT, *PCLISTAT;
#ifdef IN_RING3
AssertCompileMemberAlignment(CLISTAT, Counter, 8);
#endif


/**
 * Excessive (used to be) EM statistics.
 */
typedef struct EMSTATS
{
#if 1 /* rawmode only? */
    /** @name Privileged Instructions Ending Up In HC.
     * @{ */
    STAMCOUNTER             StatIoRestarted;
    STAMCOUNTER             StatIoIem;
    STAMCOUNTER             StatCli;
    STAMCOUNTER             StatSti;
    STAMCOUNTER             StatInvlpg;
    STAMCOUNTER             StatHlt;
    STAMCOUNTER             StatMovReadCR[DISCREG_CR4 + 1];
    STAMCOUNTER             StatMovWriteCR[DISCREG_CR4 + 1];
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
#endif
} EMSTATS;
/** Pointer to the excessive EM statistics. */
typedef EMSTATS *PEMSTATS;


/**
 * Exit history entry.
 *
 * @remarks We could perhaps trim this down a little bit by assuming uFlatPC
 *          only needs 48 bits (currently true but will change) and stuffing
 *          the flags+type in the available 16 bits made available.  The
 *          timestamp could likewise be shortened to accomodate the index, or
 *          we might skip the index entirely.  However, since we will have to
 *          deal with 56-bit wide PC address before long, there's not point.
 *
 *          On the upside, there are unused bits in both uFlagsAndType and the
 *          idxSlot fields if needed for anything.
 */
typedef struct EMEXITENTRY
{
    /** The flat PC (CS:EIP/RIP) address of the exit.
     * UINT64_MAX if not available.  */
    uint64_t        uFlatPC;
    /** The EMEXIT_MAKE_FLAGS_AND_TYPE */
    uint32_t        uFlagsAndType;
    /** The index into the exit slot hash table.
     * UINT32_MAX if too many collisions and not entered into it. */
    uint32_t        idxSlot;
    /** The TSC timestamp of the exit.
     * This is 0 if not timestamped. */
    uint64_t        uTimestamp;
} EMEXITENTRY;
/** Pointer to an exit history entry. */
typedef EMEXITENTRY *PEMEXITENTRY;
/** Pointer to a const exit history entry. */
typedef EMEXITENTRY const *PCEMEXITENTRY;


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

    /** Whether IEM executes everything. */
    bool                    fIemExecutesAll;
    /** Whether a triple fault triggers a guru. */
    bool                    fGuruOnTripleFault;
    /** Alignment padding. */
    bool                    afPadding[6];

    /** Id of the VCPU that last executed code in the recompiler. */
    VMCPUID                 idLastRemCpu;

#ifdef VBOX_WITH_REM
    /** REM critical section.
     * This protects recompiler usage
     */
    PDMCRITSECT             CritSectREM;
#endif
} EM;
/** Pointer to EM VM instance data. */
typedef EM *PEM;


/**
 * EM VMCPU Instance data.
 */
typedef struct EMCPU
{
    /** Execution Manager State. */
    EMSTATE volatile        enmState;

    /** The state prior to the suspending of the VM. */
    EMSTATE                 enmPrevState;

    /** Force raw-mode execution.
     * This is used to prevent REM from trying to execute patch code.
     * The flag is cleared upon entering emR3RawExecute() and updated in certain return paths. */
    bool                    fForceRAW;

    /** Set if hypercall instruction VMMCALL (AMD) & VMCALL (Intel) are enabled.
     * GIM sets this and the execution managers queries it.  Not saved, as GIM
     * takes care of that bit too.  */
    bool                    fHypercallEnabled;

    /** Explicit padding. */
    uint8_t                 abPadding[2];

    /** The number of instructions we've executed in IEM since switching to the
     *  EMSTATE_IEM_THEN_REM state. */
    uint32_t                cIemThenRemInstructions;

    /** Inhibit interrupts for this instruction. Valid only when VM_FF_INHIBIT_INTERRUPTS is set. */
    RTGCUINTPTR             GCPtrInhibitInterrupts;

#ifdef VBOX_WITH_RAW_MODE
    /** Pointer to the PATM status structure. (R3 Ptr) */
    R3PTRTYPE(PPATMGCSTATE) pPatmGCState;
#else
    RTR3PTR                 R3PtrPaddingNoRaw;
#endif
    RTR3PTR                 R3PtrNullPadding; /**< Used to be pCtx. */
#if GC_ARCH_BITS == 64
    RTGCPTR                 aPadding1;
#endif

    /** Start of the current time slice in ms. */
    uint64_t                u64TimeSliceStart;
    /** Start of the current time slice in thread execution time (ms). */
    uint64_t                u64TimeSliceStartExec;
    /** Current time slice value. */
    uint64_t                u64TimeSliceExec;

    /** Pending ring-3 I/O port access (VINF_EM_PENDING_R3_IOPORT_READ / VINF_EM_PENDING_R3_IOPORT_WRITE). */
    struct
    {
        RTIOPORT            uPort;          /**< The I/O port number.*/
        uint8_t             cbValue;        /**< The value size in bytes.  Zero when not pending. */
        uint8_t             cbInstr;        /**< The instruction length. */
        uint32_t            uValue;         /**< The value to write. */
    } PendingIoPortAccess;

    /** MWait halt state. */
    struct
    {
        uint32_t            fWait;          /**< Type of mwait; see EMMWAIT_FLAG_*. */
        uint32_t            u32Padding;
        RTGCPTR             uMWaitRAX;      /**< MWAIT hints. */
        RTGCPTR             uMWaitRCX;      /**< MWAIT extensions. */
        RTGCPTR             uMonitorRAX;    /**< Monitored address. */
        RTGCPTR             uMonitorRCX;    /**< Monitor extension. */
        RTGCPTR             uMonitorRDX;    /**< Monitor hint. */
    } MWait;

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

    /** For saving stack space, the disassembler state is allocated here instead of
     * on the stack. */
    DISCPUSTATE             DisState;

    /** @name Execution profiling.
     * @{ */
    STAMPROFILE             StatForcedActions;
    STAMPROFILE             StatHalted;
    STAMPROFILEADV          StatCapped;
    STAMPROFILEADV          StatHMEntry;
    STAMPROFILE             StatHMExec;
    STAMPROFILE             StatIEMEmu;
    STAMPROFILE             StatIEMThenREM;
    STAMPROFILEADV          StatNEMEntry;
    STAMPROFILE             StatNEMExec;
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
    /** R3: Number of times emR3HmExecute is called. */
    STAMCOUNTER             StatHMExecuteCalled;
    /** R3: Number of times emR3NEMExecute is called. */
    STAMCOUNTER             StatNEMExecuteCalled;

    /** More statistics (R3). */
    R3PTRTYPE(PEMSTATS)     pStatsR3;
    /** More statistics (R0). */
    R0PTRTYPE(PEMSTATS)     pStatsR0;
    /** More statistics (RC). */
    RCPTRTYPE(PEMSTATS)     pStatsRC;
#if HC_ARCH_BITS == 64
    RTRCPTR                 padding0;
#endif

    /** Tree for keeping track of cli occurrences (debug only). */
    R3PTRTYPE(PAVLGCPTRNODECORE) pCliStatTree;
    STAMCOUNTER             StatTotalClis;
#if HC_ARCH_BITS == 32
    uint64_t                padding1;
#endif

    /** Exit history table (6KB). */
    EMEXITENTRY             aExitHistory[256];
    /** Where to store the next exit history entry.
     * Since aExitHistory is 256 items longs, we'll just increment this and
     * mask it when using it.  That help the readers detect whether we've
     * wrapped around or not.  */
    uint64_t                iNextExit;

    /** Index into aExitRecords set by EMHistoryExec when returning to ring-3.
     * This is UINT16_MAX if not armed.  */
    uint16_t volatile       idxContinueExitRec;
    /** Whether exit optimizations are enabled or not (in general). */
    bool                    fExitOptimizationEnabled : 1;
    /** Whether exit optimizations are enabled for ring-0 (in general). */
    bool                    fExitOptimizationEnabledR0 : 1;
    /** Whether exit optimizations are enabled for ring-0 when preemption is disabled. */
    bool                    fExitOptimizationEnabledR0PreemptDisabled : 1;
    /** Explicit padding. */
    bool                    fPadding2;
    /** Max number of instructions to execute. */
    uint16_t                cHistoryExecMaxInstructions;
    /** Min number of instructions to execute while probing. */
    uint16_t                cHistoryProbeMinInstructions;
    /** Max number of instructions to execute without an exit before giving up probe. */
    uint16_t                cHistoryProbeMaxInstructionsWithoutExit;
    uint16_t                uPadding3;
    /** Number of exit records in use. */
    uint32_t                cExitRecordUsed;
    /** Profiling the EMHistoryExec when executing (not probing). */
    STAMPROFILE             StatHistoryExec;
    /** Number of saved exits. */
    STAMCOUNTER             StatHistoryExecSavedExits;
    /** Number of instructions executed by EMHistoryExec. */
    STAMCOUNTER             StatHistoryExecInstructions;
    uint64_t                uPadding4;
    /** Number of instructions executed by EMHistoryExec when probing. */
    STAMCOUNTER             StatHistoryProbeInstructions;
    /** Number of times probing resulted in EMEXITACTION_NORMAL_PROBED. */
    STAMCOUNTER             StatHistoryProbedNormal;
    /** Number of times probing resulted in EMEXITACTION_EXEC_WITH_MAX. */
    STAMCOUNTER             StatHistoryProbedExecWithMax;
    /** Number of times probing resulted in ring-3 continuation. */
    STAMCOUNTER             StatHistoryProbedToRing3;
    /** Profiling the EMHistoryExec when probing.*/
    STAMPROFILE             StatHistoryProbe;
    /** Hit statistics for each lookup step. */
    STAMCOUNTER             aStatHistoryRecHits[16];
    /** Type change statistics for each lookup step. */
    STAMCOUNTER             aStatHistoryRecTypeChanged[16];
    /** Replacement statistics for each lookup step. */
    STAMCOUNTER             aStatHistoryRecReplaced[16];
    /** New record statistics for each lookup step. */
    STAMCOUNTER             aStatHistoryRecNew[16];

    /** Exit records (32KB). (Aligned on 32 byte boundrary.) */
    EMEXITREC               aExitRecords[1024];
} EMCPU;
/** Pointer to EM VM instance data. */
typedef EMCPU *PEMCPU;

/** @} */

int             emR3InitDbg(PVM pVM);

int             emR3HmExecute(PVM pVM, PVMCPU pVCpu, bool *pfFFDone);
VBOXSTRICTRC    emR3NemExecute(PVM pVM, PVMCPU pVCpu, bool *pfFFDone);
int             emR3RawExecute(PVM pVM, PVMCPU pVCpu, bool *pfFFDone);

EMSTATE         emR3Reschedule(PVM pVM, PVMCPU pVCpu);
int             emR3ForcedActions(PVM pVM, PVMCPU pVCpu, int rc);
VBOXSTRICTRC    emR3HighPriorityPostForcedActions(PVM pVM, PVMCPU pVCpu, VBOXSTRICTRC rc);

int             emR3RawResumeHyper(PVM pVM, PVMCPU pVCpu);
int             emR3RawStep(PVM pVM, PVMCPU pVCpu);

VBOXSTRICTRC    emR3NemSingleInstruction(PVM pVM, PVMCPU pVCpu, uint32_t fFlags);

int             emR3SingleStepExecRem(PVM pVM, PVMCPU pVCpu, uint32_t cIterations);

bool            emR3IsExecutionAllowed(PVM pVM, PVMCPU pVCpu);

VBOXSTRICTRC    emR3ExecutePendingIoPortWrite(PVM pVM, PVMCPU pVCpu);
VBOXSTRICTRC    emR3ExecutePendingIoPortRead(PVM pVM, PVMCPU pVCpu);

RT_C_DECLS_END

#endif

