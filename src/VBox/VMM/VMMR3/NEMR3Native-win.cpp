/* $Id$ */
/** @file
 * NEM - Native execution manager, native ring-3 Windows backend.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_NEM
#include <VBox/vmm/nem.h>
#include "NEMInternal.h"
#include <VBox/vmm/vm.h>



int nemR3NativeInit(PVM pVM, bool fFallback, bool fForced)
{
    NOREF(pVM); NOREF(fFallback); NOREF(fForced);
    return VINF_SUCCESS;
}


int nemR3NativeInitCompleted(PVM pVM, VMINITCOMPLETED enmWhat)
{
    NOREF(pVM); NOREF(enmWhat);
    return VINF_SUCCESS;
}


int nemR3NativeTerm(PVM pVM)
{
    NOREF(pVM);
    return VINF_SUCCESS;
}


void nemR3NativeReset(PVM pVM)
{
    NOREF(pVM);
}


void nemR3NativeResetCpu(PVMCPU pVCpu)
{
    NOREF(pVCpu);
}

