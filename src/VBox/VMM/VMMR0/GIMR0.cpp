/* $Id$ */
/** @file
 * Guest Interface Manager (GIM) - Host Context Ring-0.
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
#include "GIMHvInternal.h"

#include <iprt/err.h>
#include <VBox/vmm/vm.h>


/**
 * Does ring-0 per-VM GIM initialization.
 *
 * @returns VBox status code.
 * @param   pVM     Pointer to the VM.
 */
VMMR0_INT_DECL(int) GIMR0InitVM(PVM pVM)
{
    if (!GIMIsEnabled(pVM))
        return VINF_SUCCESS;

    switch (pVM->gim.s.enmProviderId)
    {
        case GIMPROVIDERID_HYPERV:
            return GIMR0HvInitVM(pVM);

        default:
            break;
    }
    return VINF_SUCCESS;
}


/**
 * Does ring-0 per-VM GIM termination.
 *
 * @returns VBox status code.
 * @param   pVM     Pointer to the VM.
 */
VMMR0_INT_DECL(int) GIMR0TermVM(PVM pVM)
{
    if (!GIMIsEnabled(pVM))
        return VINF_SUCCESS;

    switch (pVM->gim.s.enmProviderId)
    {
        case GIMPROVIDERID_HYPERV:
            return GIMR0HvTermVM(pVM);

        default:
            break;
    }
    return VINF_SUCCESS;
}

