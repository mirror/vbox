/* $Id$ */
/** @file
 * CPUM - Guest Context Code.
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
#define LOG_GROUP LOG_GROUP_CPUM
#include <VBox/cpum.h>
#include <VBox/vmm.h>
#include <VBox/trpm.h>
#include "CPUMInternal.h"
#include <VBox/vm.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <VBox/log.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS /* addressed from asm (not called so no DECLASM). */
DECLCALLBACK(int) cpumGCHandleNPAndGP(PVM pVM, PCPUMCTXCORE pRegFrame, uintptr_t uUser);
__END_DECLS


/**
 * Deal with traps occuring during segment loading and IRET
 * when resuming guest context.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 * @param   uUser       User argument. In this case a combination of the
 *                      CPUM_HANDLER_* \#defines.
 */
DECLCALLBACK(int) cpumGCHandleNPAndGP(PVM pVM, PCPUMCTXCORE pRegFrame, uintptr_t uUser)
{
    Log(("********************************************************\n"));
    Log(("cpumGCHandleNPAndGP: eip=%RX32 uUser=%#x\n", pRegFrame->eip, uUser));
    Log(("********************************************************\n"));

    /*
     * Update the guest cpu state.
     */
    if (uUser & CPUM_HANDLER_CTXCORE_IN_EBP)
    {
        PCPUMCTXCORE  pGstCtxCore = CPUMCTX2CORE(&pVM->cpum.s.Guest);
        PCCPUMCTXCORE pGstCtxCoreSrc = (PCPUMCTXCORE)pRegFrame->ebp;
        *pGstCtxCore = *pGstCtxCoreSrc;
    }

    /*
     * Take action based on what's happended.
     */
    switch (uUser & CPUM_HANDLER_TYPEMASK)
    {
        case CPUM_HANDLER_GS:
        //    if (!pVM->cpum.s.Guest.ldtr)
        //    {
        //        pRegFrame->gs = 0;
        //        pRegFrame->eip += 6; /* mov gs, [edx + CPUM.Guest.gs] */
        //        return VINF_SUCCESS;
        //    }
        case CPUM_HANDLER_DS:
        case CPUM_HANDLER_ES:
        case CPUM_HANDLER_FS:
            TRPMGCHyperReturnToHost(pVM, VINF_EM_RAW_STALE_SELECTOR);
            break;

        /* Make sure we restore the guest context from the interrupt stack frame. */
        case CPUM_HANDLER_IRET:
        {
            PCPUMCTXCORE  pGstCtxCore = CPUMCTX2CORE(&pVM->cpum.s.Guest);
            uint32_t     *pEsp = (uint32_t *)pRegFrame->esp;

            /* Sync general purpose registers */
            *pGstCtxCore = *pRegFrame;

            pGstCtxCore->eip        = *pEsp++;
            pGstCtxCore->cs         = (RTSEL)*pEsp++;
            pGstCtxCore->eflags.u32 = *pEsp++;
            pGstCtxCore->esp        = *pEsp++;
            pGstCtxCore->ss         = (RTSEL)*pEsp++;
            if (pGstCtxCore->eflags.Bits.u1VM)
            {
                pGstCtxCore->es     = (RTSEL)*pEsp++;
                pGstCtxCore->ds     = (RTSEL)*pEsp++;
                pGstCtxCore->fs     = (RTSEL)*pEsp++;
                pGstCtxCore->gs     = (RTSEL)*pEsp++;
            }

            TRPMGCHyperReturnToHost(pVM, VINF_EM_RAW_IRET_TRAP);
            break;
        }
    }
    return VERR_TRPM_DONT_PANIC;
}

