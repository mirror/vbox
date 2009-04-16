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
 * Gets the current virtual CPU ID.
 *
 * @returns The CPU ID.
 * @param   pVM         Pointer to the shared VM handle.
 * @thread  EMT
 */
VMMDECL(VMCPUID) VMMGetCpuId(PVM pVM)
{
#ifdef IN_RING3
    /* Only emulation thread(s) allowed to ask for CPU id */
    if (!VM_IS_EMT(pVM))
        return 0;
#endif

    /* Only emulation thread(s) allowed to ask for CPU id */
    VM_ASSERT_EMT(pVM);

#if defined(IN_RC)
    /* There is only one CPU if we're in GC. */
    return 0;

#elif defined(IN_RING3)
    return VMR3GetVMCPUId(pVM);

#else  /* IN_RING0 */
    return HWACCMGetVMCPUId(pVM);
#endif /* IN_RING0 */
}

/**
 * Returns the VMCPU of the current EMT thread.
 *
 * @returns The VMCPU pointer.
 * @param   pVM         The VM to operate on.
 */
VMMDECL(PVMCPU) VMMGetCpu(PVM pVM)
{
#ifdef IN_RING3
    /* Only emulation thread(s) allowed to ask for CPU id */
    if (!VM_IS_EMT(pVM))
        return &pVM->aCpus[0];
#endif
    /* Only emulation thread(s) allowed to ask for CPU id */
    VM_ASSERT_EMT(pVM);

#if defined(IN_RC)
    /* There is only one CPU if we're in GC. */
    return &pVM->aCpus[0];

#elif defined(IN_RING3)
    return &pVM->aCpus[VMR3GetVMCPUId(pVM)];

#else  /* IN_RING0 */
    return &pVM->aCpus[HWACCMGetVMCPUId(pVM)];
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
 * @returns The VMCPU pointer.
 * @param   pVM         The VM to operate on.
 */
VMMDECL(PVMCPU) VMMGetCpuEx(PVM pVM, RTCPUID idCpu)
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
