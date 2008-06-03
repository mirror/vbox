/** @file
 * PATM - Dynamic Guest OS Patching Manager
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_patm_h
#define ___VBox_patm_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/dis.h>


__BEGIN_DECLS

/** @defgroup grp_patm      The Patch Manager API
 * @{
 */
#define MAX_PATCHES          512

/**
 * Flags for specifying the type of patch to install with PATMR3InstallPatch
 * @{
 */
#define PATMFL_CODE32                RT_BIT_64(0)
#define PATMFL_INTHANDLER            RT_BIT_64(1)
#define PATMFL_SYSENTER              RT_BIT_64(2)
#define PATMFL_GUEST_SPECIFIC        RT_BIT_64(3)
#define PATMFL_USER_MODE             RT_BIT_64(4)
#define PATMFL_IDTHANDLER            RT_BIT_64(5)
#define PATMFL_TRAPHANDLER           RT_BIT_64(6)
#define PATMFL_DUPLICATE_FUNCTION    RT_BIT_64(7)
#define PATMFL_REPLACE_FUNCTION_CALL RT_BIT_64(8)
#define PATMFL_TRAPHANDLER_WITH_ERRORCODE   RT_BIT_64(9)
#define PATMFL_INTHANDLER_WITH_ERRORCODE    (PATMFL_TRAPHANDLER_WITH_ERRORCODE)
#define PATMFL_MMIO_ACCESS           RT_BIT_64(10)
/* no more room -> change PATMInternal.h if more is needed!! */

/*
 * Flags above 1024 are reserved for internal use!
 */
/** @} */

/** Enable to activate sysenter emulation in GC. */
/* #define PATM_EMULATE_SYSENTER */

/**
 * Maximum number of cached VGA writes
 */
#define MAX_VGA_WRITE_CACHE    64

typedef struct PATMGCSTATE
{
    // Virtual Flags register (IF + more later on)
    uint32_t  uVMFlags;

    /* Pending PATM actions (internal use only) */
    uint32_t  uPendingAction;

    // Records the number of times all patches are called (indicating how many exceptions we managed to avoid)
    uint32_t  uPatchCalls;
    // Scratchpad dword
    uint32_t  uScratch;
    // Debugging info
    uint32_t  uIretEFlags, uIretCS, uIretEIP;

    /* PATM stack pointer */
    uint32_t  Psp;

    /* PATM interrupt flag */
    uint32_t  fPIF;
    /* PATM inhibit irq address (used by sti) */
    RTRCPTR   GCPtrInhibitInterrupts;

    /* Scratch room for call patch */
    RTRCPTR   GCCallPatchTargetAddr;
    RTRCPTR   GCCallReturnAddr;

    /* Temporary storage for guest registers. */
    struct
    {
        uint32_t    uEAX;
        uint32_t    uECX;
        uint32_t    uEDI;
        uint32_t    eFlags;
        uint32_t    uFlags;
    } Restore;

} PATMGCSTATE, *PPATMGCSTATE;

typedef struct PATMTRAPREC
{
    // pointer to original guest code instruction (for emulation)
    RTRCPTR pNewEIP;
    // pointer to the next guest code instruction
    RTRCPTR pNextInstr;
    //pointer to the corresponding next instruction in the patch block
    RTRCPTR pNextPatchInstr;
} PATMTRAPREC, *PPATMTRAPREC;


/**
 * Translation state (currently patch to GC ptr)
 */
typedef enum
{
  PATMTRANS_FAILED,
  PATMTRANS_SAFE,          /* Safe translation */
  PATMTRANS_PATCHSTART,    /* Instruction starts a patch block */
  PATMTRANS_OVERWRITTEN,   /* Instruction overwritten by patchjump */
  PATMTRANS_INHIBITIRQ     /* Instruction must be executed due to instruction fusing */
} PATMTRANSSTATE;

/**
 * Load virtualized flags.
 *
 * This function is called from CPUMRawEnter(). It doesn't have to update the
 * IF and IOPL eflags bits, the caller will enforce those to set and 0 repectively.
 *
 * @param   pVM         VM handle.
 * @param   pCtxCore    The cpu context core.
 * @see     pg_raw
 */
PATMDECL(void) PATMRawEnter(PVM pVM, PCPUMCTXCORE pCtxCore);

/**
 * Restores virtualized flags.
 *
 * This function is called from CPUMRawLeave(). It will update the eflags register.
 *
 * @param   pVM         VM handle.
 * @param   pCtxCore    The cpu context core.
 * @param   rawRC       Raw mode return code
 * @see     @ref pg_raw
 */
PATMDECL(void) PATMRawLeave(PVM pVM, PCPUMCTXCORE pCtxCore, int rawRC);

/**
 * Get the EFLAGS.
 * This is a worker for CPUMRawGetEFlags().
 *
 * @returns The eflags.
 * @param   pVM         The VM handle.
 * @param   pCtxCore    The context core.
 */
PATMDECL(uint32_t) PATMRawGetEFlags(PVM pVM, PCCPUMCTXCORE pCtxCore);

/**
 * Updates the EFLAGS.
 * This is a worker for CPUMRawSetEFlags().
 *
 * @param   pVM         The VM handle.
 * @param   pCtxCore    The context core.
 * @param   efl         The new EFLAGS value.
 */
PATMDECL(void) PATMRawSetEFlags(PVM pVM, PCPUMCTXCORE pCtxCore, uint32_t efl);

/**
 * Returns the guest context pointer of the GC context structure
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
PATMDECL(RCPTRTYPE(PPATMGCSTATE)) PATMQueryGCState(PVM pVM);

/**
 * Checks whether the GC address is part of our patch region
 *
 * @returns true -> yes, false -> no
 * @param   pVM         The VM to operate on.
 * @param   pAddr       Guest context address
 */
PATMDECL(bool) PATMIsPatchGCAddr(PVM pVM, RTRCPTR pAddr);

/**
 * Check if we must use raw mode (patch code being executed or marked safe for IF=0)
 *
 * @param   pVM         VM handle.
 * @param   pAddrGC     Guest context address
 */
PATMDECL(bool) PATMShouldUseRawMode(PVM pVM, RTRCPTR pAddrGC);

/**
 * Query PATM state (enabled/disabled)
 *
 * @returns 0 - disabled, 1 - enabled
 * @param   pVM         The VM to operate on.
 */
#define PATMIsEnabled(pVM)    (pVM->fPATMEnabled)

/**
 * Set parameters for pending MMIO patch operation
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance.
 * @param   GCPhys          MMIO physical address
 * @param   pCachedData     GC pointer to cached data
 */
PATMDECL(int) PATMSetMMIOPatchInfo(PVM pVM, RTGCPHYS GCPhys, RTRCPTR pCachedData);


/**
 * Adds branch pair to the lookup cache of the particular branch instruction
 *
 * @returns VBox status
 * @param   pVM                 The VM to operate on.
 * @param   pJumpTableGC        Pointer to branch instruction lookup cache
 * @param   pBranchTarget       Original branch target
 * @param   pRelBranchPatch     Relative duplicated function address
 */
PATMDECL(int) PATMAddBranchToLookupCache(PVM pVM, RTRCPTR pJumpTableGC, RTRCPTR pBranchTarget, RTRCUINTPTR pRelBranchPatch);


/**
 * Checks if the int 3 was caused by a patched instruction
 *
 * @returns VBox status
 *
 * @param   pVM         The VM handle.
 * @param   pCtxCore    The relevant core context.
 */
PATMDECL(int) PATMHandleInt3PatchTrap(PVM pVM, PCPUMCTXCORE pRegFrame);

/**
 * Checks if the int 3 was caused by a patched instruction
 *
 * @returns VBox status
 *
 * @param   pVM         The VM handle.
 * @param   pInstrGC    Instruction pointer
 * @param   pOpcode     Original instruction opcode (out, optional)
 * @param   pSize       Original instruction size (out, optional)
 */
PATMDECL(bool) PATMIsInt3Patch(PVM pVM, RTRCPTR pInstrGC, uint32_t *pOpcode, uint32_t *pSize);


/**
 * Checks if the interrupt flag is enabled or not.
 *
 * @returns true if it's enabled.
 * @returns false if it's diabled.
 *
 * @param   pVM         The VM handle.
 */
PATMDECL(bool) PATMAreInterruptsEnabled(PVM pVM);

/**
 * Checks if the interrupt flag is enabled or not.
 *
 * @returns true if it's enabled.
 * @returns false if it's diabled.
 *
 * @param   pVM         The VM handle.
 * @param   pCtxCore    CPU context
 */
PATMDECL(bool) PATMAreInterruptsEnabledByCtxCore(PVM pVM, PCPUMCTXCORE pCtxCore);

#ifdef PATM_EMULATE_SYSENTER
/**
 * Emulate sysenter, sysexit and syscall instructions
 *
 * @returns VBox status
 *
 * @param   pVM         The VM handle.
 * @param   pCtxCore    The relevant core context.
 * @param   pCpu        Disassembly context
 */
PATMDECL(int) PATMSysCall(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu);
#endif

#ifdef IN_GC
/** @defgroup grp_patm_gc    The Patch Manager API
 * @ingroup grp_patm
 * @{
 */

/**
 * Checks if the write is located on a page with was patched before.
 * (if so, then we are not allowed to turn on r/w)
 *
 * @returns VBox status
 * @param   pVM         The VM to operate on.
 * @param   pRegFrame   CPU context
 * @param   GCPtr       GC pointer to write address
 * @param   cbWrite     Nr of bytes to write
 *
 */
PATMGCDECL(int) PATMGCHandleWriteToPatchPage(PVM pVM, PCPUMCTXCORE pRegFrame, RTRCPTR GCPtr, uint32_t cbWrite);

/**
 * Checks if the illegal instruction was caused by a patched instruction
 *
 * @returns VBox status
 *
 * @param   pVM         The VM handle.
 * @param   pCtxCore    The relevant core context.
 */
PATMDECL(int) PATMGCHandleIllegalInstrTrap(PVM pVM, PCPUMCTXCORE pRegFrame);

/** @} */

#endif

#ifdef IN_RING3
/** @defgroup grp_patm_r3    The Patch Manager API
 * @ingroup grp_patm
 * @{
 */

/**
 * Query PATM state (enabled/disabled)
 *
 * @returns 0 - disabled, 1 - enabled
 * @param   pVM         The VM to operate on.
 */
PATMR3DECL(int) PATMR3IsEnabled(PVM pVM);

/**
 * Initializes the PATM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
PATMR3DECL(int) PATMR3Init(PVM pVM);

/**
 * Finalizes HMA page attributes.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 */
PATMR3DECL(int) PATMR3InitFinalize(PVM pVM);

/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * The PATM will update the addresses used by the switcher.
 *
 * @param   pVM     The VM.
 */
PATMR3DECL(void) PATMR3Relocate(PVM pVM);

/**
 * Terminates the PATM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
PATMR3DECL(int) PATMR3Term(PVM pVM);

/**
 * PATM reset callback.
 *
 * @returns VBox status code.
 * @param   pVM     The VM which is reset.
 */
PATMR3DECL(int) PATMR3Reset(PVM pVM);

/**
 * Returns the host context pointer and size of the patch memory block
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pcb         Size of the patch memory block
 */
PATMR3DECL(void *) PATMR3QueryPatchMemHC(PVM pVM, uint32_t *pcb);

/**
 * Returns the guest context pointer and size of the patch memory block
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pcb         Size of the patch memory block
 */
PATMR3DECL(RTRCPTR) PATMR3QueryPatchMemGC(PVM pVM, uint32_t *pcb);

/**
 * Checks whether the GC address is inside a generated patch jump
 *
 * @returns true -> yes, false -> no
 * @param   pVM         The VM to operate on.
 * @param   pAddr       Guest context address
 * @param   pPatchAddr  Guest context patch address (if true)
 */
PATMR3DECL(bool) PATMR3IsInsidePatchJump(PVM pVM, RTRCPTR pAddr, PRTGCPTR32 pPatchAddr);


/**
 * Returns the GC pointer of the patch for the specified GC address
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pAddrGC     Guest context address
 */
PATMR3DECL(RTRCPTR) PATMR3QueryPatchGCPtr(PVM pVM, RTRCPTR pAddrGC);

/**
 * Checks whether the HC address is part of our patch region
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pAddrGC     Guest context address
 */
PATMR3DECL(bool) PATMR3IsPatchHCAddr(PVM pVM, R3PTRTYPE(uint8_t *) pAddrHC);

/**
 * Convert a GC patch block pointer to a HC patch pointer
 *
 * @returns HC pointer or NULL if it's not a GC patch pointer
 * @param   pVM         The VM to operate on.
 * @param   pAddrGC     GC pointer
 */
PATMR3DECL(R3PTRTYPE(void *)) PATMR3GCPtrToHCPtr(PVM pVM, RTRCPTR pAddrGC);


/**
 * Returns the host context pointer and size of the GC context structure
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
PATMR3DECL(PPATMGCSTATE) PATMR3QueryGCStateHC(PVM pVM);

/**
 * Handle trap inside patch code
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pCtx        CPU context
 * @param   pEip        GC pointer of trapping instruction
 * @param   pNewEip     GC pointer to new instruction
 */
PATMR3DECL(int) PATMR3HandleTrap(PVM pVM, PCPUMCTX pCtx, RTRCPTR pEip, RTGCPTR *ppNewEip);

/**
 * Handle page-fault in monitored page
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
PATMR3DECL(int) PATMR3HandleMonitoredPage(PVM pVM);

/**
 * Notifies PATM about a (potential) write to code that has been patched.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   GCPtr       GC pointer to write address
 * @param   cbWrite     Nr of bytes to write
 *
 */
PATMR3DECL(int) PATMR3PatchWrite(PVM pVM, RTRCPTR GCPtr, uint32_t cbWrite);

/**
 * Notify PATM of a page flush
 *
 * @returns VBox status code
 * @param   pVM         The VM to operate on.
 * @param   addr        GC address of the page to flush
 */
PATMR3DECL(int) PATMR3FlushPage(PVM pVM, RTRCPTR addr);

/**
 * Allows or disallow patching of privileged instructions executed by the guest OS
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   fAllowPatching Allow/disallow patching
 */
PATMR3DECL(int) PATMR3AllowPatching(PVM pVM, uint32_t fAllowPatching);

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
PATMR3DECL(int) PATMR3InstallPatch(PVM pVM, RTRCPTR pInstrGC, uint64_t flags);

/**
 * Gives hint to PATM about supervisor guest instructions
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstr      Guest context point to privileged instruction
 * @param   flags       Patch flags
 */
PATMR3DECL(int) PATMR3AddHint(PVM pVM, RTRCPTR pInstrGC, uint32_t flags);

/**
 * Patch branch target function for call/jump at specified location.
 * (in responds to a VINF_PATM_DUPLICATE_FUNCTION GC exit reason)
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pCtx        Guest context
 *
 */
PATMR3DECL(int) PATMR3DuplicateFunctionRequest(PVM pVM, PCPUMCTX pCtx);

/**
 * Query the corresponding GC instruction pointer from a pointer inside the patch block itself
 *
 * @returns original GC instruction pointer or 0 if not found
 * @param   pVM         The VM to operate on.
 * @param   pPatchGC    GC address in patch block
 * @param   pEnmState   State of the translated address (out)
 *
 */
PATMR3DECL(RTRCPTR) PATMR3PatchToGCPtr(PVM pVM, RTRCPTR pPatchGC, PATMTRANSSTATE *pEnmState);

/**
 * Converts Guest code GC ptr to Patch code GC ptr (if found)
 *
 * @returns corresponding GC pointer in patch block
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context pointer to privileged instruction
 *
 */
PATMR3DECL(RTRCPTR) PATMR3GuestGCPtrToPatchGCPtr(PVM pVM, RCPTRTYPE(uint8_t*) pInstrGC);

/**
 * Query the opcode of the original code that was overwritten by the 5 bytes patch jump
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    GC address of instr
 * @param   pByte       opcode byte pointer (OUT)
 * @returns VBOX error code
 *
 */
PATMR3DECL(int) PATMR3QueryOpcode(PVM pVM, RTRCPTR pInstrGC, uint8_t *pByte);

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
PATMR3DECL(int) PATMR3DisablePatch(PVM pVM, RTRCPTR pInstrGC);


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
PATMR3DECL(int) PATMR3EnablePatch(PVM pVM, RTRCPTR pInstrGC);


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
PATMR3DECL(int) PATMR3RemovePatch(PVM pVM, RTRCPTR pInstrGC);


/**
 * Detects it the specified address falls within a 5 byte jump generated for an active patch.
 * If so, this patch is permanently disabled.
 *
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context pointer to instruction
 * @param   pConflictGC Guest context pointer to check
 */
PATMR3DECL(int) PATMR3DetectConflict(PVM pVM, RTRCPTR pInstrGC, RTRCPTR pConflictGC);


/**
 * Checks if the instructions at the specified address has been patched already.
 *
 * @returns boolean, patched or not
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context pointer to instruction
 */
PATMR3DECL(bool) PATMR3HasBeenPatched(PVM pVM, RTRCPTR pInstrGC);


/**
 * Install Linux 2.6 spinlock patch
 *
 * @returns VBox status code.
 * @param   pVM                         The VM to operate on
 * @param   pCallAcquireSpinlockGC      GC pointer of call instruction
 * @param   cbAcquireSpinlockCall       Instruction size
 *
 */
PATMR3DECL(int) PATMInstallSpinlockPatch(PVM pVM, RTRCPTR pCallAcquireSpinlockGC, uint32_t cbAcquireSpinlockCall);


/**
 * Check if supplied call target is the Linux 2.6 spinlock acquire function
 *
 * @returns boolean
 * @param   pVM         The VM to operate on
 * @param   pCallAcquireSpinlockGC      Call target GC address
 *
 */
PATMR3DECL(bool) PATMIsSpinlockAcquire(PVM pVM, RTRCPTR pCallTargetGC);

/**
 * Check if supplied call target is the Linux 2.6 spinlock release function
 *
 * @returns boolean
 * @param   pVM             The VM to operate on
 * @param   pCallTargetGC   Call target GC address
 *
 */
PATMR3DECL(bool) PATMIsSpinlockRelease(PVM pVM, RTRCPTR pCallTargetGC);

/**
 * Check if supplied call target is the Linux 2.6 spinlock release function (patched equivalent)
 *
 * @returns boolean
 * @param   pVM             The VM to operate on
 * @param   pCallTargetGC   Call target GC address
 *
 */
PATMR3DECL(bool) PATMIsSpinlockReleasePatch(PVM pVM, RTRCPTR pCallTargetGC);

/** @} */
#endif


/** @} */
__END_DECLS


#endif
