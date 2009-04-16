/* $Id$ */
/** @file
 * CPUM - CPU Monitor(/Manager) - Stack manipulation.
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
#include <VBox/cpum.h>
#include "CPUMInternal.h"
#include <VBox/vm.h>
#include <VBox/mm.h>

/** Disable stack frame pointer generation here. */
#if defined(_MSC_VER) && !defined(DEBUG)
# pragma optimize("y", off)
#endif


VMMDECL(void) CPUMPushHyper(PVMCPU pVCpu, uint32_t u32)
{
    /* ASSUME always on flat stack within hypervisor memory for now */
    pVCpu->cpum.s.Hyper.esp -= sizeof(u32);
    *(uint32_t *)MMHyperRCToR3(pVCpu->CTXALLSUFF(pVM), (RTRCPTR)pVCpu->cpum.s.Hyper.esp) = u32;
}

