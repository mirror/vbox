/* $Id$ */
/** @file
 * PGM - Page Monitor, Guest Context.
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
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PGM
#include <VBox/pgm.h>
#include <VBox/cpum.h>
#include <VBox/mm.h>
#include <VBox/em.h>
#include <VBox/selm.h>
#include <VBox/iom.h>
#include <VBox/trpm.h>
#include <VBox/rem.h>
#include "PGMInternal.h"
#include <VBox/vm.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <VBox/log.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/dis.h>
#include <VBox/disopcode.h>



/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/



#ifndef RT_ARCH_AMD64
/*
 * Shadow - 32-bit mode
 */
#define PGM_SHW_TYPE                PGM_TYPE_32BIT
#define PGM_SHW_NAME(name)          PGM_SHW_NAME_32BIT(name)
#include "PGMGCShw.h"

/* Guest - real mode */
#define PGM_GST_TYPE                PGM_TYPE_REAL
#define PGM_GST_NAME(name)          PGM_GST_NAME_REAL(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_32BIT_REAL(name)
#include "PGMGCGst.h"
#include "PGMGCBth.h"
#undef PGM_BTH_NAME
#undef PGM_GST_TYPE
#undef PGM_GST_NAME

/* Guest - protected mode */
#define PGM_GST_TYPE                PGM_TYPE_PROT
#define PGM_GST_NAME(name)          PGM_GST_NAME_PROT(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_32BIT_PROT(name)
#include "PGMGCGst.h"
#include "PGMGCBth.h"
#undef PGM_BTH_NAME
#undef PGM_GST_TYPE
#undef PGM_GST_NAME

/* Guest - 32-bit mode */
#define PGM_GST_TYPE                PGM_TYPE_32BIT
#define PGM_GST_NAME(name)          PGM_GST_NAME_32BIT(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_32BIT_32BIT(name)
#include "PGMGCGst.h"
#include "PGMGCBth.h"
#undef PGM_BTH_NAME
#undef PGM_GST_TYPE
#undef PGM_GST_NAME

#undef PGM_SHW_TYPE
#undef PGM_SHW_NAME
#endif /* !RT_ARCH_AMD64 */


/*
 * Shadow - PAE mode
 */
#define PGM_SHW_TYPE                PGM_TYPE_PAE
#define PGM_SHW_NAME(name)          PGM_SHW_NAME_PAE(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_REAL(name)
#include "PGMGCShw.h"

/* Guest - real mode */
#define PGM_GST_TYPE                PGM_TYPE_REAL
#define PGM_GST_NAME(name)          PGM_GST_NAME_REAL(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_REAL(name)
#include "PGMGCBth.h"
#undef PGM_BTH_NAME
#undef PGM_GST_TYPE
#undef PGM_GST_NAME

/* Guest - protected mode */
#define PGM_GST_TYPE                PGM_TYPE_PROT
#define PGM_GST_NAME(name)          PGM_GST_NAME_PROT(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_PROT(name)
#include "PGMGCBth.h"
#undef PGM_BTH_NAME
#undef PGM_GST_TYPE
#undef PGM_GST_NAME

/* Guest - 32-bit mode */
#define PGM_GST_TYPE                PGM_TYPE_32BIT
#define PGM_GST_NAME(name)          PGM_GST_NAME_32BIT(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_32BIT(name)
#include "PGMGCBth.h"
#undef PGM_BTH_NAME
#undef PGM_GST_TYPE
#undef PGM_GST_NAME

/* Guest - PAE mode */
#define PGM_GST_TYPE                PGM_TYPE_PAE
#define PGM_GST_NAME(name)          PGM_GST_NAME_PAE(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_PAE(name)
#include "PGMGCGst.h"
#include "PGMGCBth.h"
#undef PGM_BTH_NAME
#undef PGM_GST_TYPE
#undef PGM_GST_NAME

#undef PGM_SHW_TYPE
#undef PGM_SHW_NAME


/*
 * Shadow - AMD64 mode
 */
#define PGM_SHW_TYPE                PGM_TYPE_AMD64
#define PGM_SHW_NAME(name)          PGM_SHW_NAME_AMD64(name)
#include "PGMGCShw.h"

/* Guest - AMD64 mode */
#define PGM_GST_TYPE                PGM_TYPE_AMD64
#define PGM_GST_NAME(name)          PGM_GST_NAME_AMD64(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_AMD64_AMD64(name)
#include "PGMGCGst.h"
#include "PGMGCBth.h"
#undef PGM_BTH_NAME
#undef PGM_GST_TYPE
#undef PGM_GST_NAME

#undef PGM_SHW_TYPE
#undef PGM_SHW_NAME



/**
 * Emulation of the invlpg instruction.
 *
 * @returns VBox status code suitable for scheduling.
 * @param   pVM         VM handle.
 * @param   GCPtrPage   Page to invalidate.
 */
VMMRCDECL(int) PGMGCInvalidatePage(PVM pVM, RTGCPTR GCPtrPage)
{
    LogFlow(("PGMGCInvalidatePage: GCPtrPage=%VGv\n", GCPtrPage));

    STAM_PROFILE_START(&pVM->pgm.s.StatRCInvalidatePage, a);

    /*
     * Check for conflicts and pending CR3 monitoring updates.
     */
    if (!pVM->pgm.s.fMappingsFixed)
    {
        if (    pgmGetMapping(pVM, GCPtrPage)
            /** @todo &&  (PGMGstGetPDE(pVM, GCPtrPage) & X86_PDE_P) - FIX THIS NOW!!! */ )
        {
            LogFlow(("PGMGCInvalidatePage: Conflict!\n"));
            VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
            STAM_PROFILE_STOP(&pVM->pgm.s.StatRCInvalidatePage, a);
            return VINF_PGM_SYNC_CR3;
        }

        if (pVM->pgm.s.fSyncFlags & PGM_SYNC_MONITOR_CR3)
        {
            LogFlow(("PGMGCInvalidatePage: PGM_SYNC_MONITOR_CR3 -> reinterpret instruction in HC\n"));
            STAM_PROFILE_STOP(&pVM->pgm.s.StatRCInvalidatePage, a);
            /** @todo counter for these... */
            return VINF_EM_RAW_EMULATE_INSTR;
        }
    }

    /*
     * Notify the recompiler so it can record this instruction.
     * Failure happens when it's out of space. We'll return to HC in that case.
     */
    int rc = REMNotifyInvalidatePage(pVM, GCPtrPage);
    if (rc == VINF_SUCCESS)
        rc = PGM_BTH_PFN(InvalidatePage, pVM)(pVM, GCPtrPage);

    STAM_PROFILE_STOP(&pVM->pgm.s.StatRCInvalidatePage, a);
    return rc;
}
