/* $Id$ */
/** @file
 * VMM All Contexts.
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
#define LOG_GROUP LOG_GROUP_VMM
#include <VBox/vmm.h>
#include "VMMInternal.h"
#include <VBox/vm.h>
#include <VBox/param.h>
#include <VBox/hwaccm.h>


/**
 * Gets the bottom of the hypervisor stack - RC Ptr.
 *
 * (The returned address is not actually writable, only after it's decremented
 * by a push/ret/whatever does it become writable.)
 *
 * @returns bottom of the stack.
 * @param   pVM         The VM handle.
 */
VMMDECL(RTRCPTR) VMMGetStackRC(PVM pVM)
{
    return (RTRCPTR)pVM->vmm.s.pbEMTStackBottomRC;
}


/**
 * Gets the ID virtual of the virtual CPU assoicated with the calling thread.
 *
 * @returns The CPU ID. NIL_VMCPUID if the thread isn't an EMT.
 *
 * @param   pVM         Pointer to the shared VM handle.
 */
VMMDECL(VMCPUID) VMMGetCpuId(PVM pVM)
{
#if defined(IN_RING3)
    return VMR3GetVMCPUId(pVM);

#elif defined(IN_RING0)
    if (pVM->cCPUs == 1)
        return 0;
    return HWACCMR0GetVMCPUId(pVM);

#else /* RC: Always EMT(0) */
    return 0;
#endif
}


/**
 * Returns the VMCPU of the calling EMT.
 *
 * @returns The VMCPU pointer. NULL if not an EMT.
 *
 * @param   pVM         The VM to operate on.
 */
VMMDECL(PVMCPU) VMMGetCpu(PVM pVM)
{
#ifdef IN_RING3
    VMCPUID idCpu = VMR3GetVMCPUId(pVM);
    if (idCpu == NIL_VMCPUID)
        return NULL;
    Assert(idCpu < pVM->cCPUs);
    return &pVM->aCpus[VMR3GetVMCPUId(pVM)];

#elif defined(IN_RING0)
    if (pVM->cCPUs == 1)
        return &pVM->aCpus[0];
    return HWACCMR0GetVMCPU(pVM);

#else /* RC: Always EMT(0) */
    return &pVM->aCpus[0];
#endif /* IN_RING0 */
}


/**
 * Returns the VMCPU of the first EMT thread.
 *
 * @returns The VMCPU pointer.
 * @param   pVM         The VM to operate on.
 */
VMMDECL(PVMCPU) VMMGetCpu0(PVM pVM)
{
    Assert(pVM->cCPUs == 1);
    return &pVM->aCpus[0];
}


/**
 * Returns the VMCPU of the specified virtual CPU.
 *
 * @returns The VMCPU pointer. NULL if idCpu is invalid.
 *
 * @param   pVM         The VM to operate on.
 * @param   idCpu       The ID of the virtual CPU.
 */
VMMDECL(PVMCPU) VMMGetCpuById(PVM pVM, RTCPUID idCpu)
{
    AssertReturn(idCpu < pVM->cCPUs, NULL);
    return &pVM->aCpus[idCpu];
}


/**
 * Gets the VBOX_SVN_REV.
 *
 * This is just to avoid having to compile a bunch of big files
 * and requires less Makefile mess.
 *
 * @returns VBOX_SVN_REV.
 */
VMMDECL(uint32_t) VMMGetSvnRev(void)
{
    return VBOX_SVN_REV;
}


/**
 * Queries the current switcher
 *
 * @returns active switcher
 * @param   pVM             VM handle.
 */
VMMDECL(VMMSWITCHER) VMMGetSwitcher(PVM pVM)
{
    return pVM->vmm.s.enmSwitcher;
}

