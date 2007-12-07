/* $Id$ */
/** @file
 * VMM All Contexts.
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
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_VMM
#include <VBox/vmm.h>
#include "VMMInternal.h"
#include <VBox/vm.h>
#include <VBox/param.h>


/**
 * Gets the bottom of the hypervisor stack - GC Ptr.
 * I.e. the returned address is not actually writable.
 *
 * @returns bottom of the stack.
 * @param   pVM         The VM handle.
 */
RTGCPTR VMMGetStackGC(PVM pVM)
{
    return pVM->vmm.s.pbGCStackBottom;
}


/**
 * Gets the bottom of the hypervisor stack - HC Ptr.
 * I.e. the returned address is not actually writable.
 *
 * @returns bottom of the stack.
 * @param   pVM         The VM handle.
 */
RTHCPTR VMMGetHCStack(PVM pVM)
{
    return pVM->vmm.s.pbHCStack + VMM_STACK_SIZE;
}
