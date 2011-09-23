/* $Id$ */
/** @file
 * DBGF - Debugger Facility, CPU State Accessors.
 */

/*
 * Copyright (C) 2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGF
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/cpum.h>
#include "DBGFInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/param.h>
#include <iprt/assert.h>


/**
 * Wrapper around CPUMGetGuestMode.
 *
 * @returns VINF_SUCCESS.
 * @param   pVM                 The VM handle.
 * @param   idCpu               The current CPU ID.
 * @param   penmMode            Where to return the mode.
 */
static DECLCALLBACK(int) dbgfR3CpuGetMode(PVM pVM, VMCPUID idCpu, CPUMMODE *penmMode)
{
    Assert(idCpu == VMMGetCpuId(pVM));
    PVMCPU pVCpu = VMMGetCpuById(pVM, idCpu);
    *penmMode = CPUMGetGuestMode(pVCpu);
    return VINF_SUCCESS;
}


/**
 * Get the current CPU mode.
 *
 * @returns The CPU mode on success, CPUMMODE_INVALID on failure.
 * @param   pVM                 The VM handle.
 * @param   idCpu               The target CPU ID.
 */
VMMR3DECL(CPUMMODE) DBGFR3CpuGetMode(PVM pVM, VMCPUID idCpu)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, CPUMMODE_INVALID);
    AssertReturn(idCpu < pVM->cCpus, CPUMMODE_INVALID);

    CPUMMODE enmMode;
    int rc = VMR3ReqPriorityCallWait(pVM, idCpu, (PFNRT)dbgfR3CpuGetMode, 3, pVM, idCpu, &enmMode);
    if (RT_FAILURE(rc))
        return CPUMMODE_INVALID;
    return enmMode;
}

