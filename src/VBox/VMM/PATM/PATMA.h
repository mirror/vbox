/* $Id$ */
/** @file
 * PATM macros & definitions (identical to PATMA.mac!!)
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___PATMA_H
#define ___PATMA_H


#define PATM_VMFLAGS                            0xF1ABCD00
#ifdef VBOX_WITH_STATISTICS
#define PATM_ALLPATCHCALLS                      0xF1ABCD01
#define PATM_PERPATCHCALLS                      0xF1ABCD02
#endif
#define PATM_JUMPDELTA                          0xF1ABCD03
#ifdef VBOX_WITH_STATISTICS
#define PATM_IRETEFLAGS                         0xF1ABCD04
#define PATM_IRETCS                             0xF1ABCD05
#define PATM_IRETEIP                            0xF1ABCD06
#endif
#define PATM_FIXUP                              0xF1ABCD07
#define PATM_PENDINGACTION                      0xF1ABCD08
#define PATM_CPUID_STD_PTR                      0xF1ABCD09
#define PATM_CPUID_EXT_PTR                      0xF1ABCD0a
#define PATM_CPUID_DEF_PTR                      0xF1ABCD0b
#define PATM_STACKBASE                          0xF1ABCD0c    /** Stack to store our private patch return addesses */
#define PATM_STACKBASE_GUEST                    0xF1ABCD0d    /** Stack to store guest return addresses */
#define PATM_STACKPTR                           0xF1ABCD0e
#define PATM_PATCHBASE                          0xF1ABCD0f
#define PATM_INTERRUPTFLAG                      0xF1ABCD10
#define PATM_INHIBITIRQADDR                     0xF1ABCD11
#define PATM_VM_FORCEDACTIONS                   0xF1ABCD12
#define PATM_TEMP_EAX                           0xF1ABCD13      /** Location for original EAX register */
#define PATM_TEMP_ECX                           0xF1ABCD14      /** Location for original ECX register */
#define PATM_TEMP_EDI                           0xF1ABCD15      /** Location for original EDI register */
#define PATM_TEMP_EFLAGS                        0xF1ABCD16      /** Location for original eflags */
#define PATM_TEMP_RESTORE_FLAGS                 0xF1ABCD17      /** Which registers to restore */
#define PATM_CALL_PATCH_TARGET_ADDR             0xF1ABCD18
#define PATM_CALL_RETURN_ADDR                   0xF1ABCD19
#define PATM_CPUID_CENTAUR_PTR                  0xF1ABCD1a

/* Anything larger doesn't require a fixup */
#define PATM_NO_FIXUP                           0xF1ABCE00
#define PATM_CPUID_STD_MAX                      0xF1ABCE00
#define PATM_CPUID_EXT_MAX                      0xF1ABCE01
#define PATM_RETURNADDR                         0xF1ABCE02
#define PATM_PATCHNEXTBLOCK                     0xF1ABCE03
#define PATM_CALLTARGET                         0xF1ABCE04    /** relative call target */
#define PATM_NEXTINSTRADDR                      0xF1ABCE05    /** absolute guest address of the next instruction */
#define PATM_CURINSTRADDR                       0xF1ABCE06    /** absolute guest address of the current instruction */
#define PATM_LOOKUP_AND_CALL_FUNCTION           0xF1ABCE07    /** Relative address of global PATM lookup and call function. */
#define PATM_RETURN_FUNCTION                    0xF1ABCE08    /** Relative address of global PATM return function. */
#define PATM_LOOKUP_AND_JUMP_FUNCTION           0xF1ABCE09    /** Relative address of global PATM lookup and jump function. */
#define PATM_IRET_FUNCTION                      0xF1ABCE0A    /** Relative address of global PATM iret function. */
#define PATM_CPUID_CENTAUR_MAX                  0xF1ABCE0B

// everything except IOPL, NT, IF, VM, VIF, VIP and RF
#define PATM_FLAGS_MASK                         (X86_EFL_CF|X86_EFL_PF|X86_EFL_AF|X86_EFL_ZF|X86_EFL_SF|X86_EFL_TF|X86_EFL_DF|X86_EFL_OF|X86_EFL_AC|X86_EFL_ID)

// currently only IF & IOPL
#define PATM_VIRTUAL_FLAGS_MASK                 (X86_EFL_IF|X86_EFL_IOPL)

/* PATM stack size (identical in PATMA.mac!!) */
#define PATM_STACK_SIZE                         (PAGE_SIZE)
#define PATM_STACK_TOTAL_SIZE                   (2*PATM_STACK_SIZE)
#define PATM_MAX_STACK                          (PATM_STACK_SIZE/sizeof(RTGCPTR))

/* Patch Manager pending actions (in GCSTATE). */
#define PATM_ACTION_LOOKUP_ADDRESS              1
#define PATM_ACTION_DISPATCH_PENDING_IRQ        2
#define PATM_ACTION_PENDING_IRQ_AFTER_IRET      3
#define PATM_ACTION_DO_V86_IRET                 4
#define PATM_ACTION_LOG_IF1                     5
#define PATM_ACTION_LOG_CLI                     6
#define PATM_ACTION_LOG_STI                     7
#define PATM_ACTION_LOG_POPF_IF1                8
#define PATM_ACTION_LOG_POPF_IF0                9
#define PATM_ACTION_LOG_PUSHF                   10
#define PATM_ACTION_LOG_IRET                    11
#define PATM_ACTION_LOG_RET                     12
#define PATM_ACTION_LOG_CALL                    13
#define PATM_ACTION_LOG_GATE_ENTRY              14

/* Magic dword found in ecx for patm pending actions. */
#define PATM_ACTION_MAGIC                       0xABCD4321

/** PATM_TEMP_RESTORE_FLAGS */
#define PATM_RESTORE_EAX                        RT_BIT(0)
#define PATM_RESTORE_ECX                        RT_BIT(1)
#define PATM_RESTORE_EDI                        RT_BIT(2)

typedef struct
{
    uint8_t *pFunction;
    uint32_t offJump;
    uint32_t offRelJump;	//used only by loop/loopz/loopnz
    uint32_t offSizeOverride;   //size override byte position
    uint32_t size;
    uint32_t nrRelocs;
    uint32_t uReloc[1];
} PATCHASMRECORD, *PPATCHASMRECORD;

/* For indirect calls/jump (identical in PATMA.h & PATMA.mac!) */
/** @note MUST BE A POWER OF TWO! */
/** @note direct calls have only one lookup slot (PATCHDIRECTJUMPTABLE_SIZE) */
/** @note Some statistics reveal that:
 *   - call: Windows XP boot  -> max 16, 127 replacements
 *   - call: Knoppix 3.7 boot -> max 9
 *   - ret:  Knoppix 5.0.1 boot -> max 16, 80000 replacements (3+ million hits)
 */
#define PATM_MAX_JUMPTABLE_ENTRIES        16
typedef struct
{
    uint16_t     nrSlots;
    uint16_t     ulInsertPos;
    uint32_t     cAddresses;
    struct
    {
        RTGCPTR      pInstrGC;
        RTGCUINTPTR  pRelPatchGC; /* relative to patch base */
    } Slot[1];
} PATCHJUMPTABLE, *PPATCHJUMPTABLE;


__BEGIN_DECLS

extern PATCHASMRECORD PATMCliRecord;
extern PATCHASMRECORD PATMStiRecord;
extern PATCHASMRECORD PATMPopf32Record;
extern PATCHASMRECORD PATMPopf16Record;
extern PATCHASMRECORD PATMPopf16Record_NoExit;
extern PATCHASMRECORD PATMPopf32Record_NoExit;
extern PATCHASMRECORD PATMPushf32Record;
extern PATCHASMRECORD PATMPushf16Record;
extern PATCHASMRECORD PATMIretRecord;
extern PATCHASMRECORD PATMCpuidRecord;
extern PATCHASMRECORD PATMLoopRecord;
extern PATCHASMRECORD PATMLoopZRecord;
extern PATCHASMRECORD PATMLoopNZRecord;
extern PATCHASMRECORD PATMJEcxRecord;
extern PATCHASMRECORD PATMIntEntryRecord;
extern PATCHASMRECORD PATMIntEntryRecordErrorCode;
extern PATCHASMRECORD PATMTrapEntryRecord;
extern PATCHASMRECORD PATMTrapEntryRecordErrorCode;
extern PATCHASMRECORD PATMPushCSRecord;

extern PATCHASMRECORD PATMCheckIFRecord;
extern PATCHASMRECORD PATMJumpToGuest_IF1Record;

extern PATCHASMRECORD PATMCallRecord;
extern PATCHASMRECORD PATMCallIndirectRecord;
extern PATCHASMRECORD PATMRetRecord;
extern PATCHASMRECORD PATMJumpIndirectRecord;

extern PATCHASMRECORD PATMLookupAndCallRecord;
extern PATCHASMRECORD PATMRetFunctionRecord;
extern PATCHASMRECORD PATMLookupAndJumpRecord;
extern PATCHASMRECORD PATMIretFunctionRecord;

extern PATCHASMRECORD PATMStatsRecord;

extern PATCHASMRECORD PATMSetPIFRecord;
extern PATCHASMRECORD PATMClearPIFRecord;

extern PATCHASMRECORD PATMSetInhibitIRQRecord;
extern PATCHASMRECORD PATMClearInhibitIRQFaultIF0Record;
extern PATCHASMRECORD PATMClearInhibitIRQContIF0Record;

extern PATCHASMRECORD PATMMovFromSSRecord;

extern uint32_t       PATMInterruptFlag;

__END_DECLS

#endif
