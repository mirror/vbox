/** @file
 * HWACCM - Intel/AMD VM Hardware Support Manager
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

#ifndef ___VBox_hwaccm_h
#define ___VBox_hwaccm_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/pgm.h>
#include <iprt/mp.h>


/** @defgroup grp_hwaccm      The VM Hardware Manager API
 * @{
 */

/**
 * HWACCM state
 */
typedef enum HWACCMSTATE
{
    /* Not yet set */
    HWACCMSTATE_UNINITIALIZED = 0,
    /* Enabled */
    HWACCMSTATE_ENABLED,
    /* Disabled */
    HWACCMSTATE_DISABLED,
    /** The usual 32-bit hack. */
    HWACCMSTATE_32BIT_HACK = 0x7fffffff
} HWACCMSTATE;

__BEGIN_DECLS

/**
 * Query HWACCM state (enabled/disabled)
 *
 * @returns 0 - disabled, 1 - enabled
 * @param   pVM         The VM to operate on.
 */
#define HWACCMIsEnabled(a)    (a->fHWACCMEnabled)

#ifdef IN_RING0
/** @defgroup grp_hwaccm_r0    The VM Hardware Manager API
 * @ingroup grp_hwaccm
 * @{
 */

/**
 * Does global Ring-0 HWACCM initialization.
 *
 * @returns VBox status code.
 */
HWACCMR0DECL(int) HWACCMR0Init();

/**
 * Does global Ring-0 HWACCM termination.
 *
 * @returns VBox status code.
 */
HWACCMR0DECL(int) HWACCMR0Term();

/**
 * Does Ring-0 per VM HWACCM initialization.
 *
 * This is mainly to check that the Host CPU mode is compatible
 * with VMX or SVM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) HWACCMR0InitVM(PVM pVM);

/**
 * Does Ring-0 per VM HWACCM termination.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) HWACCMR0TermVM(PVM pVM);

/**
 * Sets up HWACCM on all cpus.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM to operate on.
 * @param   enmNewHwAccmState   New hwaccm state
 *
 */
HWACCMR0DECL(int) HWACCMR0EnableAllCpus(PVM pVM, HWACCMSTATE enmNewHwAccmState);

/** @} */
#endif


#ifdef IN_RING3
/** @defgroup grp_hwaccm_r3    The VM Hardware Manager API
 * @ingroup grp_hwaccm
 * @{
 */

/**
 * Checks if internal events are pending
 *
 * @returns boolean
 * @param   pVM         The VM to operate on.
 */
HWACCMR3DECL(bool) HWACCMR3IsEventPending(PVM pVM);

/**
 * Initializes the HWACCM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR3DECL(int) HWACCMR3Init(PVM pVM);

/**
 * Initialize VT-x or AMD-V
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 */
HWACCMR3DECL(int) HWACCMR3InitFinalizeR0(PVM pVM);

/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * The HWACCM will update the addresses used by the switcher.
 *
 * @param   pVM     The VM.
 */
HWACCMR3DECL(void) HWACCMR3Relocate(PVM pVM);

/**
 * Terminates the VMXM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR3DECL(int) HWACCMR3Term(PVM pVM);

/**
 * VMXM reset callback.
 *
 * @param   pVM     The VM which is reset.
 */
HWACCMR3DECL(void) HWACCMR3Reset(PVM pVM);


/**
 * Checks if we can currently use hardware accelerated raw mode.
 *
 * @returns boolean
 * @param   pVM         The VM to operate on.
 * @param   pCtx        Partial VM execution context
 */
HWACCMR3DECL(bool) HWACCMR3CanExecuteGuest(PVM pVM, PCPUMCTX pCtx);


/**
 * Checks if we are currently using hardware accelerated raw mode.
 *
 * @returns boolean
 * @param   pVM         The VM to operate on.
 */
HWACCMR3DECL(bool) HWACCMR3IsActive(PVM pVM);

/**
 * Checks hardware accelerated raw mode is allowed.
 *
 * @returns boolean
 * @param   pVM         The VM to operate on.
 */
HWACCMR3DECL(bool) HWACCMR3IsAllowed(PVM pVM);

/**
 * Notification callback which is called whenever there is a chance that a CR3
 * value might have changed.
 * This is called by PGM.
 *
 * @param   pVM            The VM to operate on.
 * @param   enmShadowMode  New paging mode.
 */
HWACCMR3DECL(void) HWACCMR3PagingModeChanged(PVM pVM, PGMMODE enmShadowMode);

/** @} */
#endif

#ifdef IN_RING0
/** @addtogroup grp_hwaccm_r0
 * @{
 */

/**
 * Sets up a VT-x or AMD-V session
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) HWACCMR0SetupVM(PVM pVM);


/**
 * Runs guest code in a VMX/SVM VM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) HWACCMR0RunGuestCode(PVM pVM);

/**
 * Enters the VT-x or AMD-V session
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) HWACCMR0Enter(PVM pVM);


/**
 * Leaves the VT-x or AMD-V session
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) HWACCMR0Leave(PVM pVM);

/**
 * Invalidates a guest page
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   GCVirt      Page to invalidate
 */
HWACCMR0DECL(int) HWACCMR0InvalidatePage(PVM pVM, RTGCPTR GCVirt);

/**
 * Flushes the guest TLB
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) HWACCMR0FlushTLB(PVM pVM);

/** @} */
#endif


/** @} */
__END_DECLS


#endif

