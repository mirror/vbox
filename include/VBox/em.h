/** @file
 * EM - Execution Monitor.
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_em_h
#define ___VBox_em_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/trpm.h>
#include <VBox/cpum.h>
#include <VBox/dis.h>

__BEGIN_DECLS

/** @defgroup grp_em        The Execution Monitor API
 * @{
 */

/** Enable to allow V86 code to run in raw mode. */
#define VBOX_RAW_V86

/**
 * The Execution Manager State.
 */
typedef enum EMSTATE
{
    /** Not yet started. */
    EMSTATE_NONE = 1,
    /** Raw-mode execution. */
    EMSTATE_RAW,
    /** Hardware accelerated raw-mode execution. */
    EMSTATE_HWACC,
    /** Recompiled mode execution. */
    EMSTATE_REM,
    /** Execution is halted. (waiting for interrupt) */
    EMSTATE_HALTED,
    /** Execution is suspended. */
    EMSTATE_SUSPENDED,
    /** The VM is terminating. */
    EMSTATE_TERMINATING,
    /** Guest debug event from raw-mode is being processed. */
    EMSTATE_DEBUG_GUEST_RAW,
    /** Guest debug event from hardware accelerated mode is being processed. */
    EMSTATE_DEBUG_GUEST_HWACC,
    /** Guest debug event from recompiled-mode is being processed. */
    EMSTATE_DEBUG_GUEST_REM,
    /** Hypervisor debug event being processed. */
    EMSTATE_DEBUG_HYPER,
    /** The VM has encountered a fatal error. (And everyone is panicing....) */
    EMSTATE_GURU_MEDITATION,
    /** Just a hack to ensure that we get a 32-bit integer. */
    EMSTATE_MAKE_32BIT_HACK = 0x7fffffff
} EMSTATE;


/**
 * Get the current execution manager status.
 *
 * @returns Current status.
 */
EMDECL(EMSTATE) EMGetState(PVM pVM);

/**
 * Checks if raw ring-3 execute mode is enabled.
 *
 * @returns true if enabled.
 * @returns false if disabled.
 * @param   pVM         The VM to operate on.
 */
#define EMIsRawRing3Enabled(pVM) ((pVM)->fRawR3Enabled)

/**
 * Checks if raw ring-0 execute mode is enabled.
 *
 * @returns true if enabled.
 * @returns false if disabled.
 * @param   pVM         The VM to operate on.
 */
#define EMIsRawRing0Enabled(pVM) ((pVM)->fRawR0Enabled)

/**
 * Sets the PC for which interrupts should be inhibited.
 *
 * @param   pVM         The VM handle.
 * @param   PC          The PC.
 */
EMDECL(void) EMSetInhibitInterruptsPC(PVM pVM, RTGCUINTPTR PC);

/**
 * Gets the PC for which interrupts should be inhibited.
 *
 * There are a few instructions which inhibits or delays interrupts
 * for the instruction following them. These instructions are:
 *      - STI
 *      - MOV SS, r/m16
 *      - POP SS
 *
 * @returns The PC for which interrupts should be inhibited.
 * @param   pVM         VM handle.
 *
 */
EMDECL(RTGCUINTPTR) EMGetInhibitInterruptsPC(PVM pVM);

/**
 * Disassembles one instruction.
 *
 * @param   pVM             The VM handle.
 * @param   pCtxCore        The context core (used for both the mode and instruction).
 * @param   pCpu            Where to return the parsed instruction info.
 * @param   pcbInstr        Where to return the instruction size. (optional)
 */
EMDECL(int) EMInterpretDisasOne(PVM pVM, PCCPUMCTXCORE pCtxCore, PDISCPUSTATE pCpu, unsigned *pcbInstr);

/**
 * Disassembles one instruction.
 *
 * This is used by internally by the interpreter and by trap/access handlers.
 *
 * @param   pVM             The VM handle.
 * @param   GCPtrInstr      The flat address of the instruction.
 * @param   pCtxCore        The context core (used to determin the cpu mode).
 * @param   pCpu            Where to return the parsed instruction info.
 * @param   pcbInstr        Where to return the instruction size. (optional)
 */
EMDECL(int) EMInterpretDisasOneEx(PVM pVM, RTGCUINTPTR GCPtrInstr, PCCPUMCTXCORE pCtxCore,
                                  PDISCPUSTATE pCpu, unsigned *pcbInstr);

/**
 * Interprets the current instruction.
 *
 * @returns VBox status code.
 * @retval  VINF_*                  Scheduling instructions.
 * @retval  VERR_EM_INTERPRETER     Something we can't cope with.
 * @retval  VERR_*                  Fatal errors.
 *
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 *                      Updates the EIP if an instruction was executed successfully.
 * @param   pvFault     The fault address (CR2).
 * @param   pcbSize     Size of the write (if applicable).
 *
 * @remark  Invalid opcode exceptions have a higher priority than GP (see Intel
 *          Architecture System Developers Manual, Vol 3, 5.5) so we don't need
 *          to worry about e.g. invalid modrm combinations (!)
 */
EMDECL(int) EMInterpretInstruction(PVM pVM, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize);

/**
 * Interprets the current instruction using the supplied DISCPUSTATE structure.
 *
 * EIP is *NOT* updated!
 *
 * @returns VBox status code.
 * @retval  VINF_*                  Scheduling instructions. When these are returned, it
 *                                  starts to get a bit tricky to know whether code was
 *                                  executed or not... We'll address this when it becomes a problem.
 * @retval  VERR_EM_INTERPRETER     Something we can't cope with.
 * @retval  VERR_*                  Fatal errors.
 *
 * @param   pVM         The VM handle.
 * @param   pCpu        The disassembler cpu state for the instruction to be interpreted.
 * @param   pRegFrame   The register frame. EIP is *NOT* changed!
 * @param   pvFault     The fault address (CR2).
 * @param   pcbSize     Size of the write (if applicable).
 *
 * @remark  Invalid opcode exceptions have a higher priority than GP (see Intel
 *          Architecture System Developers Manual, Vol 3, 5.5) so we don't need
 *          to worry about e.g. invalid modrm combinations (!)
 */
EMDECL(int) EMInterpretInstructionCPU(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbSize);

/**
 * Interpret CPUID given the parameters in the CPU context
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 *
 */
EMDECL(int) EMInterpretCpuId(PVM pVM, PCPUMCTXCORE pRegFrame);

/**
 * Interpret RDTSC
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 *
 */
EMDECL(int) EMInterpretRdtsc(PVM pVM, PCPUMCTXCORE pRegFrame);

/**
 * Interpret INVLPG
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 * @param   pAddrGC     Operand address
 *
 */
EMDECL(int) EMInterpretInvlpg(PVM pVM, PCPUMCTXCORE pRegFrame, RTGCPTR pAddrGC);

/**
 * Interpret IRET (currently only to V86 code)
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 *
 */
EMDECL(int) EMInterpretIret(PVM pVM, PCPUMCTXCORE pRegFrame);

/**
 * Interpret DRx write
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 * @param   DestRegDRx  DRx register index (USE_REG_DR*)
 * @param   SrcRegGen   General purpose register index (USE_REG_E**))
 *
 */
EMDECL(int) EMInterpretDRxWrite(PVM pVM, PCPUMCTXCORE pRegFrame, uint32_t DestRegDrx, uint32_t SrcRegGen);

/**
 * Interpret DRx read
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 * @param   DestRegGen  General purpose register index (USE_REG_E**))
 * @param   SrcRegDRx   DRx register index (USE_REG_DR*)
 *
 */
EMDECL(int) EMInterpretDRxRead(PVM pVM, PCPUMCTXCORE pRegFrame, uint32_t DestRegGen, uint32_t SrcRegDrx);

/**
 * Interpret CRx write
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 * @param   DestRegCRx  DRx register index (USE_REG_CR*)
 * @param   SrcRegGen   General purpose register index (USE_REG_E**))
 *
 */
EMDECL(int) EMInterpretCRxWrite(PVM pVM, PCPUMCTXCORE pRegFrame, uint32_t DestRegCrx, uint32_t SrcRegGen);

/**
 * Interpret CRx read
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 * @param   DestRegGen  General purpose register index (USE_REG_E**))
 * @param   SrcRegCRx   CRx register index (USE_REG_CR*)
 *
 */
EMDECL(int) EMInterpretCRxRead(PVM pVM, PCPUMCTXCORE pRegFrame, uint32_t DestRegGen, uint32_t SrcRegCrx);

/**
 * Interpret LMSW
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   u16Data     LMSW source data.
 */
EMDECL(int) EMInterpretLMSW(PVM pVM, uint16_t u16Data);

/**
 * Interpret CLTS
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 *
 */
EMDECL(int) EMInterpretCLTS(PVM pVM);

/**
 * Interpret a port I/O instruction.
 *
 * @returns VBox status code suitable for scheduling.
 * @param   pVM         The VM handle.
 * @param   pCtxCore    The context core. This will be updated on successful return.
 * @param   pCpu        The instruction to interpret.
 * @param   cbOp        The size of the instruction.
 * @remark  This may raise exceptions.
 */
EMDECL(int) EMInterpretPortIO(PVM pVM, PCPUMCTXCORE pCtxCore, PDISCPUSTATE pCpu, uint32_t cbOp);

EMDECL(uint32_t) EMEmulateCmp(uint32_t u32Param1, uint32_t u32Param2, size_t cb);
EMDECL(uint32_t) EMEmulateAnd(uint32_t *pu32Param1, uint32_t u32Param2, size_t cb);
EMDECL(uint32_t) EMEmulateInc(uint32_t *pu32Param1, size_t cb);
EMDECL(uint32_t) EMEmulateDec(uint32_t *pu32Param1, size_t cb);
EMDECL(uint32_t) EMEmulateOr(uint32_t *pu32Param1, uint32_t u32Param2, size_t cb);
EMDECL(int)      EMEmulateLockOr(RTGCPTR GCPtrParam1, RTGCUINTREG Param2, size_t cbSize, uint32_t *pf);
EMDECL(uint32_t) EMEmulateXor(uint32_t *pu32Param1, uint32_t u32Param2, size_t cb);
EMDECL(uint32_t) EMEmulateAdd(uint32_t *pu32Param1, uint32_t u32Param2, size_t cb);
EMDECL(uint32_t) EMEmulateSub(uint32_t *pu32Param1, uint32_t u32Param2, size_t cb);
EMDECL(uint32_t) EMEmulateAdcWithCarrySet(uint32_t *pu32Param1, uint32_t u32Param2, size_t cb);
EMDECL(uint32_t) EMEmulateBtr(uint32_t *pu32Param1, uint32_t u32Param2);
EMDECL(int)      EMEmulateLockBtr(RTGCPTR GCPtrParam1, RTGCUINTREG Param2, uint32_t *pf);
EMDECL(uint32_t) EMEmulateBts(uint32_t *pu32Param1, uint32_t u32Param2);
EMDECL(uint32_t) EMEmulateBtc(uint32_t *pu32Param1, uint32_t u32Param2);

#ifdef IN_RING3
/** @defgroup grp_em_r3     The EM Host Context Ring-3 API
 * @ingroup grp_em
 * @{
 */

/**
 * Initializes the EM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
EMR3DECL(int) EMR3Init(PVM pVM);

/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * @param   pVM     The VM.
 */
EMR3DECL(void) EMR3Relocate(PVM pVM);

/**
 * Reset notification.
 *
 * @param   pVM
 */
EMR3DECL(void) EMR3Reset(PVM pVM);

/**
 * Terminates the EM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
EMR3DECL(int) EMR3Term(PVM pVM);


/**
 * Command argument for EMR3RawSetMode().
 *
 * It's possible to extend this interface to change several
 * execution modes at once should the need arise.
 */
typedef enum EMRAWMODE
{
    /** No raw execution. */
    EMRAW_NONE = 0,
    /** Enable Only ring-3 raw execution. */
    EMRAW_RING3_ENABLE,
    /** Only ring-3 raw execution. */
    EMRAW_RING3_DISABLE,
    /** Enable raw ring-0 execution. */
    EMRAW_RING0_ENABLE,
    /** Disable raw ring-0 execution. */
    EMRAW_RING0_DISABLE,
    EMRAW_END
} EMRAWMODE;

/**
 * Enables or disables a set of raw-mode execution modes.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VINF_RESCHEDULE if a rescheduling might be required.
 * @returns VERR_INVALID_PARAMETER on an invalid enmMode value.
 *
 * @param   pVM         The VM to operate on.
 * @param   enmMode     The execution mode change.
 * @thread  The emulation thread.
 */
EMR3DECL(int) EMR3RawSetMode(PVM pVM, EMRAWMODE enmMode);

/**
 * Raise a fatal error.
 *
 * Safely terminate the VM with full state report and stuff. This function
 * will naturally never return.
 *
 * @param   pVM         VM handle.
 * @param   rc          VBox status code.
 */
EMR3DECL(DECLNORETURN(void)) EMR3FatalError(PVM pVM, int rc);

/**
 * Execute VM
 *
 * This function is the main loop of the VM. The emulation thread
 * calls this function when the VM has been successfully constructed
 * and we're ready for executing the VM.
 *
 * Returning from this function means that the VM is turned off or
 * suspended (state already saved) and deconstruction in next in line.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
EMR3DECL(int) EMR3ExecuteVM(PVM pVM);

/**
 * Check for pending raw actions
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
EMR3DECL(int) EMR3CheckRawForcedActions(PVM pVM);

/**
 * Interpret instructions.
 * This works directly on the Guest CPUM context.
 * The interpretation will try execute at least one instruction. It will
 * stop when a we're better off in a raw or recompiler mode.
 *
 * @returns Todo - status describing what to do next?
 * @param   pVM         The VM to operate on.
 */
EMR3DECL(int) EMR3Interpret(PVM pVM);

/** @} */
#endif


#ifdef IN_GC
/** @defgroup grp_em_gc     The EM Guest Context API
 * @ingroup grp_em
 * @{
 */

/**
 * Decide what to do with a trap.
 *
 * @returns Next VMM state.
 * @returns Might not return at all?
 * @param   pVM         The VM to operate on.
 * @param   uTrap       The trap number.
 * @param   pRegFrame   Register frame to operate on.
 */
EMGCDECL(int) EMGCTrap(PVM pVM, unsigned uTrap, PCPUMCTXCORE pRegFrame);

EMGCDECL(uint32_t) EMGCEmulateLockCmpXchg(RTGCPTR pu32Param1, uint32_t *pu32Param2, uint32_t u32Param3, size_t cbSize, uint32_t *pEflags);
EMGCDECL(uint32_t) EMGCEmulateCmpXchg(RTGCPTR pu32Param1, uint32_t *pu32Param2, uint32_t u32Param3, size_t cbSize, uint32_t *pEflags);

/** @} */
#endif

/** @} */

__END_DECLS

#endif

