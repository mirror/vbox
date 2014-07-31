/* $Id$ */
/** @file
 * GIM - Guest Interface Manager, Minimal implementation.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_GIM
#include "GIMInternal.h"

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/asm-amd64-x86.h>

#include <VBox/vmm/cpum.h>
#include <VBox/vmm/vm.h>

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/

VMMR3_INT_DECL(int) GIMR3MinimalInit(PVM pVM)
{
    AssertReturn(pVM, VERR_INVALID_PARAMETER);
    AssertReturn(pVM->gim.s.enmProviderId == GIMPROVIDERID_MINIMAL, VERR_INTERNAL_ERROR_5);

    /*
     * Enable the Hypervisor Present.
     */
    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_HVP);

    return VINF_SUCCESS;
}


VMMR3_INT_DECL(void) GIMR3MinimalRelocate(PVM pVM, RTGCINTPTR offDelta)
{
    NOREF(pVM); NOREF(offDelta);
}

