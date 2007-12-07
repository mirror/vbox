/** @file
 * TRPM - The Trap Monitor.
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

#ifndef ___VBox_trpm_h
#define ___VBox_trpm_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/cpum.h>


__BEGIN_DECLS
/** @defgroup grp_trpm The Trap Monitor API
 * @{
 */

/**
 * Trap: error code present or not
 */
typedef enum
{
    TRPM_TRAP_HAS_ERRORCODE = 0,
    TRPM_TRAP_NO_ERRORCODE,
    /** The usual 32-bit paranoia. */
    TRPM_TRAP_32BIT_HACK = 0x7fffffff
} TRPMERRORCODE;

/**
 * TRPM event type
 */
/** Note: must match trpm.mac! */
typedef enum
{
    TRPM_TRAP         = 0,
    TRPM_HARDWARE_INT = 1,
    TRPM_SOFTWARE_INT = 2,
    /** The usual 32-bit paranoia. */
    TRPM_32BIT_HACK   = 0x7fffffff
} TRPMEVENT;
/** Pointer to a TRPM event type. */
typedef TRPMEVENT *PTRPMEVENT;
/** Pointer to a const TRPM event type. */
typedef TRPMEVENT const *PCTRPMEVENT;

/**
 * Invalid trap handler for trampoline calls
 */
#define TRPM_INVALID_HANDLER        0

/**
 * Query info about the current active trap/interrupt.
 * If no trap is active active an error code is returned.
 *
 * @returns VBox status code.
 * @param   pVM                     The virtual machine.
 * @param   pu8TrapNo               Where to store the trap number.
 * @param   pEnmType                Where to store the trap type.
 */
TRPMDECL(int)  TRPMQueryTrap(PVM pVM, uint8_t *pu8TrapNo, PTRPMEVENT pEnmType);

/**
 * Gets the trap number for the current trap.
 *
 * The caller is responsible for making sure there is an active trap which
 * takes an error code when making this request.
 *
 * @returns The current trap number.
 * @param   pVM         VM handle.
 */
TRPMDECL(uint8_t)  TRPMGetTrapNo(PVM pVM);

/**
 * Gets the error code for the current trap.
 *
 * The caller is responsible for making sure there is an active trap which
 * takes an error code when making this request.
 *
 * @returns Error code.
 * @param   pVM         VM handle.
 */
TRPMDECL(RTGCUINT)  TRPMGetErrorCode(PVM pVM);

/**
 * Gets the fault address for the current trap.
 *
 * The caller is responsible for making sure there is an active trap 0x0e when
 * making this request.
 *
 * @returns Fault address associated with the trap.
 * @param   pVM         VM handle.
 */
TRPMDECL(RTGCUINTPTR) TRPMGetFaultAddress(PVM pVM);

/**
 * Clears the current active trap/exception/interrupt.
 *
 * The caller is responsible for making sure there is an active trap
 * when making this request.
 *
 * @returns VBox status code.
 * @param   pVM         The virtual machine handle.
 */
TRPMDECL(int) TRPMResetTrap(PVM pVM);

/**
 * Assert trap/exception/interrupt.
 *
 * The caller is responsible for making sure there is no active trap
 * when making this request.
 *
 * @returns VBox status code.
 * @param   pVM                 The virtual machine.
 * @param   u8TrapNo            The trap vector to assert.
 * @param   enmType             Trap type.
 */
TRPMDECL(int)  TRPMAssertTrap(PVM pVM, uint8_t u8TrapNo, TRPMEVENT enmType);

/**
 * Sets the error code of the current trap.
 * (This function is for use in trap handlers and such.)
 *
 * The caller is responsible for making sure there is an active trap
 * which takes an errorcode when making this request.
 *
 * @param   pVM         The virtual machine.
 * @param   uErrorCode  The new error code.
 */
TRPMDECL(void)  TRPMSetErrorCode(PVM pVM, RTGCUINT uErrorCode);

/**
 * Sets the error code of the current trap.
 * (This function is for use in trap handlers and such.)
 *
 * The caller is responsible for making sure there is an active trap 0e
 * when making this request.
 *
 * @param   pVM         The virtual machine.
 * @param   uCR2        The new fault address (cr2 register).
 */
TRPMDECL(void)  TRPMSetFaultAddress(PVM pVM, RTGCUINTPTR uCR2);

/**
 * Checks if the current active trap/interrupt/exception/fault/whatever is a software
 * interrupt or not.
 *
 * The caller is responsible for making sure there is an active trap
 * when making this request.
 *
 * @returns true if software interrupt, false if not.
 *
 * @param   pVM         VM handle.
 */
TRPMDECL(bool) TRPMIsSoftwareInterrupt(PVM pVM);

/**
 * Check if there is an active trap.
 *
 * @returns true if trap active, false if not.
 * @param   pVM         The virtual machine.
 */
TRPMDECL(bool)  TRPMHasTrap(PVM pVM);

/**
 * Query all info about the current active trap/interrupt.
 * If no trap is active active an error code is returned.
 *
 * @returns VBox status code.
 * @param   pVM                     The virtual machine.
 * @param   pu8TrapNo               Where to store the trap number.
 * @param   pEnmType                Where to store the trap type.
 * @param   puErrorCode             Where to store the error code associated with some traps.
 *                                  ~0U is stored if the trap have no error code.
 * @param   puCR2                   Where to store the CR2 associated with a trap 0E.
 */
TRPMDECL(int)  TRPMQueryTrapAll(PVM pVM, uint8_t *pu8TrapNo, PTRPMEVENT pEnmType, PRTGCUINT puErrorCode, PRTGCUINTPTR puCR2);


/**
 * Save the active trap.
 *
 * This routine useful when doing try/catch in the hypervisor.
 * Any function which uses temporary trap handlers should
 * probably also use this facility to save the original trap.
 *
 * @param   pVM     VM handle.
 */
TRPMDECL(void) TRPMSaveTrap(PVM pVM);

/**
 * Restore a saved trap.
 *
 * Multiple restores of a saved trap is possible.
 *
 * @param   pVM     VM handle.
 */
TRPMDECL(void) TRPMRestoreTrap(PVM pVM);

/**
 * Forward trap or interrupt to the guest's handler
 *
 *
 * @returns VBox status code.
 *  or does not return at all (when the trap is actually forwarded)
 *
 * @param   pVM         The VM to operate on.
 * @param   pRegFrame   Pointer to the register frame for the trap.
 * @param   iGate       Trap or interrupt gate number
 * @param   opsize      Instruction size (only relevant for software interrupts)
 * @param   enmError    TRPM_TRAP_HAS_ERRORCODE or TRPM_TRAP_NO_ERRORCODE.
 * @param   enmType     TRPM event type
 * @internal
 */
TRPMDECL(int) TRPMForwardTrap(PVM pVM, PCPUMCTXCORE pRegFrame, uint32_t iGate, uint32_t opsize, TRPMERRORCODE enmError, TRPMEVENT enmType);

/**
 * Raises a cpu exception which doesn't take an error code.
 *
 * This function may or may not dispatch the exception before returning.
 *
 * @returns VBox status code fit for scheduling.
 * @retval  VINF_EM_RAW_GUEST_TRAP if the exception was left pending.
 * @retval  VINF_TRPM_XCPT_DISPATCHED if the exception was raised and dispatched for raw-mode execution.
 * @retval  VINF_EM_RESCHEDULE_REM if the exception was dispatched and cannot be executed in raw-mode.
 *
 * @param   pVM         The VM handle.
 * @param   pCtxCore    The CPU context core.
 * @param   enmXcpt     The exception.
 */
TRPMDECL(int) TRPMRaiseXcpt(PVM pVM, PCPUMCTXCORE pCtxCore, X86XCPT enmXcpt);

/**
 * Raises a cpu exception with an errorcode.
 *
 * This function may or may not dispatch the exception before returning.
 *
 * @returns VBox status code fit for scheduling.
 * @retval  VINF_EM_RAW_GUEST_TRAP if the exception was left pending.
 * @retval  VINF_TRPM_XCPT_DISPATCHED if the exception was raised and dispatched for raw-mode execution.
 * @retval  VINF_EM_RESCHEDULE_REM if the exception was dispatched and cannot be executed in raw-mode.
 *
 * @param   pVM         The VM handle.
 * @param   pCtxCore    The CPU context core.
 * @param   enmXcpt     The exception.
 * @param   uErr        The error code.
 */
TRPMDECL(int) TRPMRaiseXcptErr(PVM pVM, PCPUMCTXCORE pCtxCore, X86XCPT enmXcpt, uint32_t uErr);

/**
 * Raises a cpu exception with an errorcode and CR2.
 *
 * This function may or may not dispatch the exception before returning.
 *
 * @returns VBox status code fit for scheduling.
 * @retval  VINF_EM_RAW_GUEST_TRAP if the exception was left pending.
 * @retval  VINF_TRPM_XCPT_DISPATCHED if the exception was raised and dispatched for raw-mode execution.
 * @retval  VINF_EM_RESCHEDULE_REM if the exception was dispatched and cannot be executed in raw-mode.
 *
 * @param   pVM         The VM handle.
 * @param   pCtxCore    The CPU context core.
 * @param   enmXcpt     The exception.
 * @param   uErr        The error code.
 * @param   uCR2        The CR2 value.
 */
TRPMDECL(int) TRPMRaiseXcptErrCR2(PVM pVM, PCPUMCTXCORE pCtxCore, X86XCPT enmXcpt, uint32_t uErr, RTGCUINTPTR uCR2);


#ifdef IN_RING3
/** @defgroup grp_trpm_r3   TRPM Host Context Ring 3 API
 * @ingroup grp_trpm
 * @{
 */

/**
 * Initializes the SELM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
TRPMR3DECL(int) TRPMR3Init(PVM pVM);

/**
 * Applies relocations to data and code managed by this component.
 *
 * This function will be called at init and whenever the VMM need
 * to relocate itself inside the GC.
 *
 * @param   pVM         The VM handle.
 * @param   offDelta    Relocation delta relative to old location.
 */
TRPMR3DECL(void) TRPMR3Relocate(PVM pVM, RTGCINTPTR offDelta);

/**
 * The VM is being reset.
 *
 * For the TRPM component this means that any IDT write monitors
 * needs to be removed, any pending trap cleared, and the IDT reset.
 *
 * @param   pVM     VM handle.
 */
TRPMR3DECL(void) TRPMR3Reset(PVM pVM);

/**
 * Set interrupt gate handler
 * Used for setting up interrupt gates used for kernel calls.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   iTrap       Interrupt number.
 */
TRPMR3DECL(int) TRPMR3EnableGuestTrapHandler(PVM pVM, unsigned iTrap);

/**
 * Set guest trap/interrupt gate handler
 * Used for setting up trap gates used for kernel calls.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   iTrap       Interrupt/trap number.
 * @parapm  pHandler    GC handler pointer
 */
TRPMR3DECL(int) TRPMR3SetGuestTrapHandler(PVM pVM, unsigned iTrap, RTGCPTR pHandler);

/**
 * Get guest trap/interrupt gate handler
 *
 * @returns Guest trap handler address or TRPM_INVALID_HANDLER if none installed
 * @param   pVM         The VM to operate on.
 * @param   iTrap       Interrupt/trap number.
 */
TRPMR3DECL(RTGCPTR) TRPMR3GetGuestTrapHandler(PVM pVM, unsigned iTrap);

/**
 * Disable IDT monitoring and syncing
 *
 * @param   pVM         The VM to operate on.
 */
TRPMR3DECL(void) TRPMR3DisableMonitoring(PVM pVM);

/**
 * Check if gate handlers were updated
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
TRPMR3DECL(int) TRPMR3SyncIDT(PVM pVM);

/**
 * Check if address is a gate handler (interrupt/trap/task/anything).
 *
 * @returns True is gate handler, false if not.
 *
 * @param   pVM         VM handle.
 * @param   GCPtr       GC address to check.
 */
TRPMR3DECL(bool) TRPMR3IsGateHandler(PVM pVM, RTGCPTR GCPtr);

/**
 * Check if address is a gate handler (interrupt or trap).
 *
 * @returns gate nr or ~0 is not found
 *
 * @param   pVM         VM handle.
 * @param   GCPtr       GC address to check.
 */
TRPMR3DECL(uint32_t) TRPMR3QueryGateByHandler(PVM pVM, RTGCPTR GCPtr);

/**
 * Initializes the SELM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
TRPMR3DECL(int) TRPMR3Term(PVM pVM);


/**
 * Inject event (such as external irq or trap)
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   enmEvent    Trpm event type
 */
TRPMR3DECL(int) TRPMR3InjectEvent(PVM pVM, TRPMEVENT enmEvent);

/** @} */
#endif


#ifdef IN_GC
/** @defgroup grp_trpm_gc    The TRPM Guest Context API
 * @ingroup grp_trpm
 * @{
 */

/**
 * Guest Context temporary trap handler
 *
 * @returns VBox status code (appropriate for GC return).
 *          In this context VBOX_SUCCESS means to restart the instruction.
 * @param   pVM         VM handle.
 * @param   pRegFrame   Trap register frame.
 */
typedef DECLCALLBACK(int) FNTRPMGCTRAPHANDLER(PVM pVM, PCPUMCTXCORE pRegFrame);
/** Pointer to a TRPMGCTRAPHANDLER() function. */
typedef FNTRPMGCTRAPHANDLER *PFNTRPMGCTRAPHANDLER;

/**
 * Arms a temporary trap handler for traps in Hypervisor code.
 *
 * The operation is similar to a System V signal handler. I.e. when the handler
 * is called it is first set to default action. So, if you need to handler more
 * than one trap, you must reinstall the handler.
 *
 * To uninstall the temporary handler, call this function with pfnHandler set to NULL.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   iTrap       Trap number to install handler [0..255].
 * @param   pfnHandler  Pointer to the handler. Use NULL for uninstalling the handler.
 */
TRPMGCDECL(int) TRPMGCSetTempHandler(PVM pVM, unsigned iTrap, PFNTRPMGCTRAPHANDLER pfnHandler);

/**
 * Return to host context from a hypervisor trap handler.
 * It will also reset any traps that are pending.
 *
 * This function will *never* return.
 *
 * @param   pVM     The VM handle.
 * @param   rc      The return code for host context.
 */
TRPMGCDECL(void) TRPMGCHyperReturnToHost(PVM pVM, int rc);

/** @} */
#endif


#ifdef IN_RING0
/** @defgroup grp_trpm_r0   TRPM Host Context Ring 0 API
 * @ingroup grp_trpm
 * @{
 */

/**
 * Dispatches an interrupt that arrived while we were in the guest context.
 *
 * @param   pVM     The VM handle.
 * @remark  Must be called with interrupts disabled.
 */
TRPMR0DECL(void) TRPMR0DispatchHostInterrupt(PVM pVM);

/**
 * Changes the VMMR0Entry() call frame and stack used by the IDT patch code
 * so that we'll dispatch an interrupt rather than returning directly to Ring-3
 * when VMMR0Entry() returns.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pvRet       Pointer to the return address of VMMR0Entry() on the stack.
 */
TRPMR0DECL(void) TRPMR0SetupInterruptDispatcherFrame(PVM pVM, void *pvRet);

/** @} */
#endif

/** @} */
__END_DECLS

#endif
