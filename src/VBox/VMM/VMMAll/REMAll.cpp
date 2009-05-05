/* $Id$ */
/** @file
 * REM - Recompiled Execution Monitor, all Contexts part.
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
*   Global Variables                                                           *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_REM
#include <VBox/rem.h>
#include <VBox/vmm.h>
#include "REMInternal.h"
#include <VBox/vm.h>
#include <VBox/err.h>
#include <VBox/log.h>

#include <iprt/assert.h>


#ifndef IN_RING3

/**
 * Records a invlpg instruction for replaying upon REM entry.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_REM_FLUSHED_PAGES_OVERFLOW if a return to HC for flushing of
 *          recorded pages is required before the call can succeed.
 * @param   pVM         The VM handle.
 * @param   GCPtrPage   The
 */
VMMDECL(int) REMNotifyInvalidatePage(PVM pVM, RTGCPTR GCPtrPage)
{
    if (pVM->rem.s.cInvalidatedPages < RT_ELEMENTS(pVM->rem.s.aGCPtrInvalidatedPages))
    {
        /*
         * We sync them back in REMR3State.
         */
        pVM->rem.s.aGCPtrInvalidatedPages[pVM->rem.s.cInvalidatedPages++] = GCPtrPage;
    }
    else
    {
        /* Tell the recompiler to flush its TLB. */
#ifndef DEBUG_bird /* temporary */
        Assert(pVM->cCPUs == 1); /* @todo SMP */
#endif
        CPUMSetChangedFlags(VMMGetCpu(pVM), CPUM_CHANGED_GLOBAL_TLB_FLUSH);
        pVM->rem.s.cInvalidatedPages = 0;
    }

    return VINF_SUCCESS;
}


/**
 * Flushes the handler notifications by calling the host.
 *
 * @param   pVM     The VM handle.
 */
static void remFlushHandlerNotifications(PVM pVM)
{
#ifdef IN_RC
    VMMGCCallHost(pVM, VMMCALLHOST_REM_REPLAY_HANDLER_NOTIFICATIONS, 0);
#elif defined(IN_RING0)
    /** @todo necessary? */
    VMMR0CallHost(pVM, VMMCALLHOST_REM_REPLAY_HANDLER_NOTIFICATIONS, 0);
#else
    AssertReleaseMsgFailed(("Ring 3 call????.\n"));
#endif
    Assert(pVM->rem.s.cHandlerNotifications == 0);
}


/**
 * Notification about a successful PGMR3HandlerPhysicalRegister() call.
 *
 * @param   pVM             VM Handle.
 * @param   enmType         Handler type.
 * @param   GCPhys          Handler range address.
 * @param   cb              Size of the handler range.
 * @param   fHasHCHandler   Set if the handler have a HC callback function.
 */
VMMDECL(void) REMNotifyHandlerPhysicalRegister(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhys, RTGCPHYS cb, bool fHasHCHandler)
{
    if (pVM->rem.s.cHandlerNotifications >= RT_ELEMENTS(pVM->rem.s.aHandlerNotifications))
        remFlushHandlerNotifications(pVM);
    PREMHANDLERNOTIFICATION pRec = &pVM->rem.s.aHandlerNotifications[pVM->rem.s.cHandlerNotifications++];
    pRec->enmKind = REMHANDLERNOTIFICATIONKIND_PHYSICAL_REGISTER;
    pRec->u.PhysicalRegister.enmType = enmType;
    pRec->u.PhysicalRegister.GCPhys = GCPhys;
    pRec->u.PhysicalRegister.cb = cb;
    pRec->u.PhysicalRegister.fHasHCHandler = fHasHCHandler;
    VM_FF_SET(pVM, VM_FF_REM_HANDLER_NOTIFY);
}


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
VMMDECL(void) REMNotifyHandlerPhysicalDeregister(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhys, RTGCPHYS cb, bool fHasHCHandler, bool fRestoreAsRAM)
{
    if (pVM->rem.s.cHandlerNotifications >= RT_ELEMENTS(pVM->rem.s.aHandlerNotifications))
        remFlushHandlerNotifications(pVM);
    PREMHANDLERNOTIFICATION pRec = &pVM->rem.s.aHandlerNotifications[pVM->rem.s.cHandlerNotifications++];
    pRec->enmKind = REMHANDLERNOTIFICATIONKIND_PHYSICAL_DEREGISTER;
    pRec->u.PhysicalDeregister.enmType = enmType;
    pRec->u.PhysicalDeregister.GCPhys = GCPhys;
    pRec->u.PhysicalDeregister.cb = cb;
    pRec->u.PhysicalDeregister.fHasHCHandler = fHasHCHandler;
    pRec->u.PhysicalDeregister.fRestoreAsRAM = fRestoreAsRAM;
    VM_FF_SET(pVM, VM_FF_REM_HANDLER_NOTIFY);
}


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
VMMDECL(void) REMNotifyHandlerPhysicalModify(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhysOld, RTGCPHYS GCPhysNew, RTGCPHYS cb, bool fHasHCHandler, bool fRestoreAsRAM)
{
    if (pVM->rem.s.cHandlerNotifications >= RT_ELEMENTS(pVM->rem.s.aHandlerNotifications))
        remFlushHandlerNotifications(pVM);
    PREMHANDLERNOTIFICATION pRec = &pVM->rem.s.aHandlerNotifications[pVM->rem.s.cHandlerNotifications++];
    pRec->enmKind = REMHANDLERNOTIFICATIONKIND_PHYSICAL_MODIFY;
    pRec->u.PhysicalModify.enmType = enmType;
    pRec->u.PhysicalModify.GCPhysOld = GCPhysOld;
    pRec->u.PhysicalModify.GCPhysNew = GCPhysNew;
    pRec->u.PhysicalModify.cb = cb;
    pRec->u.PhysicalModify.fHasHCHandler = fHasHCHandler;
    pRec->u.PhysicalModify.fRestoreAsRAM = fRestoreAsRAM;
    VM_FF_SET(pVM, VM_FF_REM_HANDLER_NOTIFY);
}

#endif /* !IN_RING3 */

/**
 * Make REM flush all translation block upon the next call to REMR3State().
 *
 * @param   pVM             Pointer to the shared VM structure.
 */
VMMDECL(void) REMFlushTBs(PVM pVM)
{
    LogFlow(("REMFlushTBs: fFlushTBs=%RTbool fInREM=%RTbool fInStateSync=%RTbool\n",
             pVM->rem.s.fFlushTBs, pVM->rem.s.fInREM, pVM->rem.s.fInStateSync));
    pVM->rem.s.fFlushTBs = true;
}

