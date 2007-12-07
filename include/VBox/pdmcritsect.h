/** @file
 * PDM - Pluggable Device Manager, Critical Sections.
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

#ifndef ___VBox_pdmcritsect_h
#define ___VBox_pdmcritsect_h

#include <VBox/types.h>
#include <iprt/critsect.h>

__BEGIN_DECLS

/** @defgroup grp_pdm_critsect      The PDM Critical Section
 * @ingroup grp_pdm
 * @{
 */

/**
 * A PDM critical section.
 * Initialize using PDMDRVHLP::pfnCritSectInit().
 */
typedef union PDMCRITSECT
{
    /** Padding. */
    uint8_t padding[HC_ARCH_BITS == 64 ? 0xb8 : 0x80];
#ifdef PDMCRITSECTINT_DECLARED
    /** The internal structure (not normally visible). */
    struct PDMCRITSECTINT s;
#endif
} PDMCRITSECT;
/** Pointer to a PDM critical section. */
typedef PDMCRITSECT *PPDMCRITSECT;
/** Pointer to a const PDM critical section. */
typedef const PDMCRITSECT *PCPDMCRITSECT;

/**
 * Initializes a PDM critical section for internal use.
 *
 * The PDM critical sections are derived from the IPRT critical sections, but
 * works in GC as well.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pDevIns         Device instance.
 * @param   pCritSect       Pointer to the critical section.
 * @param   pszName         The name of the critical section (for statistics).
 */
PDMR3DECL(int) PDMR3CritSectInit(PVM pVM, PPDMCRITSECT pCritSect, const char *pszName);

/**
 * Leaves a critical section entered with PDMCritSectEnter().
 *
 * @returns VINF_SUCCESS if entered successfully.
 * @returns rcBusy when encountering a busy critical section in GC/R0.
 * @returns VERR_SEM_DESTROYED if the critical section is dead.
 *
 * @param   pCritSect           The PDM critical section to enter.
 * @param   rcBusy              The status code to return when we're in GC or R0
 *                              and the section is busy.
 */
PDMDECL(int) PDMCritSectEnter(PPDMCRITSECT pCritSect, int rcBusy);

/**
 * Leaves a critical section entered with PDMCritSectEnter().
 *
 * @param   pCritSect           The PDM critical section to leave.
 */
PDMDECL(void) PDMCritSectLeave(PPDMCRITSECT pCritSect);

/**
 * Checks the caller is the owner of the critical section.
 *
 * @returns true if owner.
 * @returns false if not owner.
 * @param   pCritSect   The critical section.
 */
PDMDECL(bool) PDMCritSectIsOwner(PCPDMCRITSECT pCritSect);

/**
 * Checks if a critical section is initialized or not.
 *
 * @returns true if initialized.
 * @returns false if not initialized.
 * @param   pCritSect   The critical section.
 */
PDMDECL(bool) PDMCritSectIsInitialized(PCPDMCRITSECT pCritSect);

/**
 * Try enter a critical section.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_SEM_BUSY if the critsect was owned.
 * @returns VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @returns VERR_SEM_DESTROYED if RTCritSectDelete was called while waiting.
 * @param   pCritSect   The critical section.
 */
PDMR3DECL(int) PDMR3CritSectTryEnter(PPDMCRITSECT pCritSect);

/**
 * Schedule a event semaphore for signalling upon critsect exit.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_TOO_MANY_SEMAPHORES if an event was already scheduled.
 * @returns VERR_NOT_OWNER if we're not the critsect owner.
 * @returns VERR_SEM_DESTROYED if RTCritSectDelete was called while waiting.
 * @param   pCritSect       The critical section.
 * @param   EventToSignal     The semapore that should be signalled.
 */
PDMR3DECL(int) PDMR3CritSectScheduleExitEvent(PPDMCRITSECT pCritSect, RTSEMEVENT EventToSignal);

/**
 * Deletes the critical section.
 *
 * @returns VBox status code.
 * @param   pCritSect           The PDM critical section to destroy.
 */
PDMR3DECL(int) PDMR3CritSectDelete(PPDMCRITSECT pCritSect);

/**
 * Deletes all remaining critical sections.
 *
 * This is called at the end of the termination process.
 *
 * @returns VBox status.
 *          First error code, rest is lost.
 * @param   pVM         The VM handle.
 * @remark  Don't confuse this with PDMR3CritSectDelete.
 */
PDMDECL(int) PDMR3CritSectTerm(PVM pVM);

/**
 * Process the critical sections queued for ring-3 'leave'.
 *
 * @param   pVM         The VM handle.
 */
PDMR3DECL(void) PDMR3CritSectFF(PVM pVM);

/** @} */

__END_DECLS

#endif

