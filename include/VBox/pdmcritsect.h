/** @file
 * PDM - Pluggable Device Manager, Critical Sections.
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

#ifndef ___VBox_pdmcritsect_h
#define ___VBox_pdmcritsect_h

#include <VBox/types.h>
#include <iprt/critsect.h>

__BEGIN_DECLS

/** @defgroup grp_pdm_critsect      The PDM Critical Section API
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
    uint8_t padding[HC_ARCH_BITS == 64 ? 0xb8 : 0xa8];
#ifdef PDMCRITSECTINT_DECLARED
    /** The internal structure (not normally visible). */
    struct PDMCRITSECTINT s;
#endif
} PDMCRITSECT;
/** Pointer to a PDM critical section. */
typedef PDMCRITSECT *PPDMCRITSECT;
/** Pointer to a const PDM critical section. */
typedef const PDMCRITSECT *PCPDMCRITSECT;

VMMR3DECL(int)  PDMR3CritSectInit(PVM pVM, PPDMCRITSECT pCritSect, const char *pszName);
VMMDECL(int)    PDMCritSectEnter(PPDMCRITSECT pCritSect, int rcBusy);
VMMR3DECL(int)  PDMR3CritSectEnterEx(PPDMCRITSECT pCritSect, bool fCallHost);
VMMDECL(void)   PDMCritSectLeave(PPDMCRITSECT pCritSect);
VMMDECL(bool)   PDMCritSectIsOwner(PCPDMCRITSECT pCritSect);
VMMDECL(bool)   PDMCritSectIsInitialized(PCPDMCRITSECT pCritSect);
VMMR3DECL(int)  PDMR3CritSectTryEnter(PPDMCRITSECT pCritSect);
VMMR3DECL(int)  PDMR3CritSectScheduleExitEvent(PPDMCRITSECT pCritSect, RTSEMEVENT EventToSignal);
VMMR3DECL(int)  PDMR3CritSectDelete(PPDMCRITSECT pCritSect);
VMMDECL(int)    PDMR3CritSectTerm(PVM pVM);
VMMR3DECL(void) PDMR3CritSectFF(PVM pVM);
VMMR3DECL(uint32_t) PDMR3CritSectCountOwned(PVM pVM, char *pszNames, size_t cbNames);

/** @} */

__END_DECLS

#endif

