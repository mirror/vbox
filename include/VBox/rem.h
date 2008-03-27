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
REMDECL(int)  REMNotifyInvalidatePage(PVM pVM, RTGCPTR GCPtrPage);
REMDECL(void) REMNotifyHandlerPhysicalRegister(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhys, RTGCPHYS cb, bool fHasHCHandler);
REMDECL(void) REMNotifyHandlerPhysicalDeregister(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhys, RTGCPHYS cb, bool fHasHCHandler, bool fRestoreAsRAM);
REMDECL(void) REMNotifyHandlerPhysicalModify(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhysOld, RTGCPHYS GCPhysNew, RTGCPHYS cb, bool fHasHCHandler, bool fRestoreAsRAM);
#endif /* IN_RING0 || IN_GC */


#ifdef IN_RING3
/** @defgroup grp_rem_r3   REM Host Context Ring 3 API
 * @ingroup grp_rem
 * @{
 */
REMR3DECL(int)  REMR3Init(PVM pVM);
REMR3DECL(int)  REMR3Term(PVM pVM);
REMR3DECL(void) REMR3Reset(PVM pVM);
REMR3DECL(int)  REMR3Run(PVM pVM);
REMR3DECL(int)  REMR3EmulateInstruction(PVM pVM);
REMR3DECL(int)  REMR3Step(PVM pVM);
REMR3DECL(int)  REMR3BreakpointSet(PVM pVM, RTGCUINTPTR Address);
REMR3DECL(int)  REMR3BreakpointClear(PVM pVM, RTGCUINTPTR Address);
REMR3DECL(int)  REMR3State(PVM pVM);
REMR3DECL(int)  REMR3StateBack(PVM pVM);
REMR3DECL(void) REMR3StateUpdate(PVM pVM);
REMR3DECL(void) REMR3A20Set(PVM pVM, bool fEnable);
REMR3DECL(int)  REMR3DisasEnableStepping(PVM pVM, bool fEnable);
REMR3DECL(void) REMR3ReplayInvalidatedPages(PVM pVM);
REMR3DECL(void) REMR3ReplayHandlerNotifications(PVM pVM);
REMR3DECL(int)  REMR3NotifyCodePageChanged(PVM pVM, RTGCPTR pvCodePage);
REMR3DECL(void) REMR3NotifyPhysRamRegister(PVM pVM, RTGCPHYS GCPhys, RTUINT cb, unsigned fFlags);
#ifndef VBOX_WITH_NEW_PHYS_CODE
REMR3DECL(void) REMR3NotifyPhysRamChunkRegister(PVM pVM, RTGCPHYS GCPhys, RTUINT cb, RTHCUINTPTR pvRam, unsigned fFlags);
#endif
REMR3DECL(void) REMR3NotifyPhysRomRegister(PVM pVM, RTGCPHYS GCPhys, RTUINT cb, void *pvCopy, bool fShadow);
REMR3DECL(void) REMR3NotifyPhysReserve(PVM pVM, RTGCPHYS GCPhys, RTUINT cb);
REMR3DECL(void) REMR3NotifyHandlerPhysicalRegister(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhys, RTGCPHYS cb, bool fHasHCHandler);
REMR3DECL(void) REMR3NotifyHandlerPhysicalDeregister(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhys, RTGCPHYS cb, bool fHasHCHandler, bool fRestoreAsRAM);
REMR3DECL(void) REMR3NotifyHandlerPhysicalModify(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhysOld, RTGCPHYS GCPhysNew, RTGCPHYS cb, bool fHasHCHandler, bool fRestoreAsRAM);
REMR3DECL(void) REMR3NotifyPendingInterrupt(PVM pVM, uint8_t u8Interrupt);
REMR3DECL(uint32_t) REMR3QueryPendingInterrupt(PVM pVM);
REMR3DECL(void) REMR3NotifyInterruptSet(PVM pVM);
REMR3DECL(void) REMR3NotifyInterruptClear(PVM pVM);
REMR3DECL(void) REMR3NotifyTimerPending(PVM pVM);
REMR3DECL(void) REMR3NotifyDmaPending(PVM pVM);
REMR3DECL(void) REMR3NotifyQueuePending(PVM pVM);
REMR3DECL(void) REMR3NotifyFF(PVM pVM);
#ifdef VBOX_STRICT
REMR3DECL(bool) REMR3IsPageAccessHandled(PVM pVM, RTGCPHYS GCPhys);
#endif
/** @} */
#endif /* IN_RING3 */


/** @} */
__END_DECLS


#endif

