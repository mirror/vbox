/** @file
 * REM - The Recompiled Execution Manager.
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

#ifndef ___VBox_rem_h
#define ___VBox_rem_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/pgm.h>
#include <VBox/vmapi.h>


__BEGIN_DECLS

/** @defgroup grp_rem      The Recompiled Execution Manager API
 * @{
 */

/** No pending interrupt. */
#define REM_NO_PENDING_IRQ          (~(uint32_t)0)


#if defined(IN_RING0) || defined(IN_GC)

/**
 * Records a invlpg instruction for replaying upon REM entry.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_REM_FLUSHED_PAGES_OVERFLOW if a return to HC for flushing of
 *          recorded pages is required before the call can succeed.
 * @param   pVM         The VM handle.
 * @param   GCPtrPage   The address of the invalidated page.
 */
REMDECL(int) REMNotifyInvalidatePage(PVM pVM, RTGCPTR GCPtrPage);

/**
 * Notification about a successful PGMR3HandlerPhysicalRegister() call.
 *
 * @param   pVM             VM Handle.
 * @param   enmType         Handler type.
 * @param   GCPhys          Handler range address.
 * @param   cb              Size of the handler range.
 * @param   fHasHCHandler   Set if the handler have a HC callback function.
 */
REMDECL(void) REMNotifyHandlerPhysicalRegister(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhys, RTGCPHYS cb, bool fHasHCHandler);

/**
 * Notification about a successful PGMR3HandlerPhysicalDeregister() operation.
 *
 * @param   pVM             VM Handle.
 * @param   enmType         Handler type.
 * @param   GCPhys          Handler range address.
 * @param   cb              Size of the handler range.
 * @param   fHasHCHandler   Set if the handler have a HC callback function.
 * @param   fRestoreAsRAM   Whether the to restore it as normal RAM or as unassigned memory.
 */
REMDECL(void) REMNotifyHandlerPhysicalDeregister(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhys, RTGCPHYS cb, bool fHasHCHandler, bool fRestoreAsRAM);

/**
 * Notification about a successful PGMR3HandlerPhysicalModify() call.
 *
 * @param   pVM             VM Handle.
 * @param   enmType         Handler type.
 * @param   GCPhysOld       Old handler range address.
 * @param   GCPhysNew       New handler range address.
 * @param   cb              Size of the handler range.
 * @param   fHasHCHandler   Set if the handler have a HC callback function.
 * @param   fRestoreAsRAM   Whether the to restore it as normal RAM or as unassigned memory.
 */
REMDECL(void) REMNotifyHandlerPhysicalModify(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhysOld, RTGCPHYS GCPhysNew, RTGCPHYS cb, bool fHasHCHandler, bool fRestoreAsRAM);

#endif /* IN_RING0 || IN_GC */


#ifdef IN_RING3
/** @defgroup grp_rem_r3   REM Host Context Ring 3 API
 * @ingroup grp_rem
 * @{
 */

/**
 * Initializes the REM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
REMR3DECL(int) REMR3Init(PVM pVM);

/**
 * Terminates the REM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
REMR3DECL(int) REMR3Term(PVM pVM);

/**
 * The VM is being reset.
 *
 * For the REM component this means to call the cpu_reset() and
 * reinitialize some state variables.
 *
 * @param   pVM     VM handle.
 */
REMR3DECL(void) REMR3Reset(PVM pVM);

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
REMR3DECL(int) REMR3Run(PVM pVM);

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
REMR3DECL(int) REMR3EmulateInstruction(PVM pVM);

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
REMR3DECL(int) REMR3Step(PVM pVM);

/**
 * Set a breakpoint using the REM facilities.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   Address         The breakpoint address.
 * @thread  The emulation thread.
 */
REMR3DECL(int) REMR3BreakpointSet(PVM pVM, RTGCUINTPTR Address);

/**
 * Clears a breakpoint set by REMR3BreakpointSet().
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   Address         The breakpoint address.
 * @thread  The emulation thread.
 */
REMR3DECL(int) REMR3BreakpointClear(PVM pVM, RTGCUINTPTR Address);

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
REMR3DECL(int) REMR3State(PVM pVM);

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
REMR3DECL(int) REMR3StateBack(PVM pVM);

/**
 * Update the VMM state information if we're currently in REM.
 *
 * This method is used by the DBGF and PDMDevice when there is any uncertainty of whether
 * we're currently executing in REM and the VMM state is invalid. This method will of
 * course check that we're executing in REM before syncing any data over to the VMM.
 *
 * @param   pVM         The VM handle.
 */
REMR3DECL(void) REMR3StateUpdate(PVM pVM);

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
REMR3DECL(void) REMR3A20Set(PVM pVM, bool fEnable);

/**
 * Enables or disables singled stepped disassembly.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   fEnable     To enable set this flag, to disable clear it.
 */
REMR3DECL(int) REMR3DisasEnableStepping(PVM pVM, bool fEnable);

/**
 * Replays the recorded invalidated pages.
 * Called in response to VERR_REM_FLUSHED_PAGES_OVERFLOW from the RAW execution loop.
 *
 * @param   pVM         VM handle.
 */
REMR3DECL(void) REMR3ReplayInvalidatedPages(PVM pVM);

/**
 * Replays the recorded physical handler notifications.
 *
 * @param   pVM         VM handle.
 */
REMR3DECL(void) REMR3ReplayHandlerNotifications(PVM pVM);

/**
 * Notify REM about changed code page.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   pvCodePage  Code page address
 */
REMR3DECL(int) REMR3NotifyCodePageChanged(PVM pVM, RTGCPTR pvCodePage);

/**
 * Notification about a successful MMR3RamRegister() call.
 *
 * @param   pVM         VM handle.
 * @param   GCPhys      The physical address the RAM.
 * @param   cb          Size of the memory.
 * @param   fFlags      Flags of the MM_RAM_FLAGS_* defines.
 */
REMR3DECL(void) REMR3NotifyPhysRamRegister(PVM pVM, RTGCPHYS GCPhys, RTUINT cb, unsigned fFlags);

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
REMR3DECL(void) REMR3NotifyPhysRamChunkRegister(PVM pVM, RTGCPHYS GCPhys, RTUINT cb, RTHCUINTPTR pvRam, unsigned fFlags);
#endif

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
REMR3DECL(void) REMR3NotifyPhysRomRegister(PVM pVM, RTGCPHYS GCPhys, RTUINT cb, void *pvCopy, bool fShadow);

/**
 * Notification about a successful MMR3PhysRegister() call.
 *
 * @param   pVM         VM Handle.
 * @param   GCPhys      Start physical address.
 * @param   cb          The size of the range.
 */
REMR3DECL(void) REMR3NotifyPhysReserve(PVM pVM, RTGCPHYS GCPhys, RTUINT cb);

/**
 * Notification about a successful PGMR3HandlerPhysicalRegister() call.
 *
 * @param   pVM             VM Handle.
 * @param   enmType         Handler type.
 * @param   GCPhys          Handler range address.
 * @param   cb              Size of the handler range.
 * @param   fHasHCHandler   Set if the handler have a HC callback function.
 */
REMR3DECL(void) REMR3NotifyHandlerPhysicalRegister(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhys, RTGCPHYS cb, bool fHasHCHandler);

/**
 * Notification about a successful PGMR3HandlerPhysicalDeregister() operation.
 *
 * @param   pVM             VM Handle.
 * @param   enmType         Handler type.
 * @param   GCPhys          Handler range address.
 * @param   cb              Size of the handler range.
 * @param   fHasHCHandler   Set if the handler have a HC callback function.
 * @param   fRestoreAsRAM   Whether the to restore it as normal RAM or as unassigned memory.
 */
REMR3DECL(void) REMR3NotifyHandlerPhysicalDeregister(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhys, RTGCPHYS cb, bool fHasHCHandler, bool fRestoreAsRAM);

/**
 * Notification about a successful PGMR3HandlerPhysicalModify() call.
 *
 * @param   pVM             VM Handle.
 * @param   enmType         Handler type.
 * @param   GCPhysOld       Old handler range address.
 * @param   GCPhysNew       New handler range address.
 * @param   cb              Size of the handler range.
 * @param   fHasHCHandler   Set if the handler have a HC callback function.
 * @param   fRestoreAsRAM   Whether the to restore it as normal RAM or as unassigned memory.
 */
REMR3DECL(void) REMR3NotifyHandlerPhysicalModify(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhysOld, RTGCPHYS GCPhysNew, RTGCPHYS cb, bool fHasHCHandler, bool fRestoreAsRAM);

/**
 * Notification about a pending interrupt.
 *
 * @param   pVM             VM Handle.
 * @param   u8Interrupt     Interrupt
 * @thread  The emulation thread.
 */
REMR3DECL(void) REMR3NotifyPendingInterrupt(PVM pVM, uint8_t u8Interrupt);

/**
 * Notification about a pending interrupt.
 *
 * @returns Pending interrupt or REM_NO_PENDING_IRQ
 * @param   pVM             VM Handle.
 * @thread  The emulation thread.
 */
REMR3DECL(uint32_t) REMR3QueryPendingInterrupt(PVM pVM);

/**
 * Notification about the interrupt FF being set.
 *
 * @param   pVM             VM Handle.
 * @thread  The emulation thread.
 */
REMR3DECL(void) REMR3NotifyInterruptSet(PVM pVM);

/**
 * Notification about the interrupt FF being set.
 *
 * @param   pVM             VM Handle.
 * @thread  The emulation thread.
 */
REMR3DECL(void) REMR3NotifyInterruptClear(PVM pVM);

/**
 * Notification about pending timer(s).
 *
 * @param   pVM             VM Handle.
 * @thread  Any.
 */
REMR3DECL(void) REMR3NotifyTimerPending(PVM pVM);

/**
 * Notification about pending DMA transfers.
 *
 * @param   pVM             VM Handle.
 * @thread  Any.
 */
REMR3DECL(void) REMR3NotifyDmaPending(PVM pVM);

/**
 * Notification about pending timer(s).
 *
 * @param   pVM             VM Handle.
 * @thread  Any.
 */
REMR3DECL(void) REMR3NotifyQueuePending(PVM pVM);

/**
 * Notification about pending FF set by an external thread.
 *
 * @param   pVM             VM handle.
 * @thread  Any.
 */
REMR3DECL(void) REMR3NotifyFF(PVM pVM);

#ifdef VBOX_STRICT
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
REMDECL(bool) REMR3IsPageAccessHandled(PVM pVM, RTGCPHYS GCPhys);
#endif

/** @} */
#endif


/** @} */
__END_DECLS


#endif
