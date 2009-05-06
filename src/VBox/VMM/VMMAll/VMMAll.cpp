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
#include <VBox/vmm.h>
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
    PVMCPU pVCpu = VMMGetCpu(pVM);
    Assert(pVCpu);

    return (RTRCPTR)pVCpu->vmm.s.pbEMTStackBottomRC;
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


#ifdef IN_RING3
/**
 * On VCPU worker for VMMSendSipi.
 *
 * @param   pVM         The VM to operate on.
 * @param   idCpu       Virtual CPU to perform SIPI on
 * @param   uVector     SIPI vector
 */
DECLCALLBACK(int) vmmR3SendSipi(PVM pVM, VMCPUID idCpu, uint32_t uVector)
{
    PVMCPU pVCpu = VMMGetCpuById(pVM, idCpu);
    VMCPU_ASSERT_EMT(pVCpu);

    /** @todo what are we supposed to do if the processor is already running? */
    CPUMSetGuestCS(pVCpu, uVector * 0x100);
    CPUMSetGuestEIP(pVCpu, 0);

# if 1 /* If we keep the EMSTATE_WAIT_SIPI method, then move this to EM.cpp. */
    return VINF_EM_RESCHEDULE;
# else /* And if we go the VMCPU::enmState way it can stay here. */
    VMCPU_ASSERT_STATE(pVCpu, VMCPUSTATE_STOPPED);
    VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED);
    return VINF_SUCCESS;
# endif
}
#endif /* IN_RING3 */


/**
 * Sends SIPI to the virtual CPU by setting CS:EIP into vector-dependent state
 * and unhalting processor
 *
 * @param   pVM         The VM to operate on.
 * @param   idCpu       Virtual CPU to perform SIPI on
 * @param   iVector     SIPI vector
 */
VMMDECL(void) VMMSendSipi(PVM pVM, VMCPUID idCpu, int iVector) /** @todo why is iVector signed? */
{
    AssertReturnVoid(idCpu < pVM->cCPUs);

#ifdef IN_RING3
    PVMREQ pReq;
    int rc = VMR3ReqCallU(pVM->pUVM, idCpu, &pReq, RT_INDEFINITE_WAIT, 0,
                          (PFNRT)vmmR3SendSipi, 3, pVM, idCpu, (uint32_t)iVector);
    AssertRC(rc);
    VMR3ReqFree(pReq);
#else
    AssertMsgFailed(("has to be done in ring-3, fix the code.\n"));
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

