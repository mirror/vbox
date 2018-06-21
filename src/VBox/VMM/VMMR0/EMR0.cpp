/* $Id$ */
/** @file
 * EM - Host Context Ring 0.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_EM
#include <VBox/vmm/em.h>
#include "EMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/vmm/gvm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/thread.h>



/**
 * Adjusts EM configuration options.
 *
 * @returns VBox status code.
 * @param   pGVM            The ring-0 VM structure.
 * @param   pVM             The cross context VM structure.
 */
VMMR0_INT_DECL(int) EMR0InitVM(PGVM pGVM, PVM pVM)
{
    /*
     * Override ring-0 exit optimizations settings.
     */
    bool fEnabledR0                = pVM->aCpus[0].em.s.fExitOptimizationEnabled
                                  && pVM->aCpus[0].em.s.fExitOptimizationEnabledR0
                                  && (RTThreadPreemptIsPossible() || RTThreadPreemptIsPendingTrusty());
    bool fEnabledR0PreemptDisabled = fEnabledR0
                                  && pVM->aCpus[0].em.s.fExitOptimizationEnabledR0PreemptDisabled
                                  && RTThreadPreemptIsPendingTrusty();
    for (VMCPUID i = 0; i < pGVM->cCpus; i++)
    {
        pVM->aCpus[i].em.s.fExitOptimizationEnabledR0                = fEnabledR0;
        pVM->aCpus[i].em.s.fExitOptimizationEnabledR0PreemptDisabled = fEnabledR0PreemptDisabled;
    }

    return VINF_SUCCESS;
}

