/* $Id$ */
/** @file
 * IEM - Interpreted Execution Manager.
 */

/*
 * Copyright (C) 2011-2015 Oracle Corporation
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
#include <VBox/vmm/iem.h>
#include <VBox/vmm/cpum.h>
#include "IEMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/err.h>

#include <iprt/asm-amd64-x86.h>
#include <iprt/assert.h>



/**
 * Initializes the interpreted execution manager.
 *
 * This must be called after CPUM as we're quering information from CPUM about
 * the guest and host CPUs.
 *
 * @returns VBox status code.
 * @param   pVM                The cross context VM structure.
 */
VMMR3DECL(int)      IEMR3Init(PVM pVM)
{
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PVMCPU pVCpu = &pVM->aCpus[idCpu];
        pVCpu->iem.s.offVM    = -RT_OFFSETOF(VM, aCpus[idCpu].iem.s);
        pVCpu->iem.s.offVMCpu = -RT_OFFSETOF(VMCPU, iem.s);
        pVCpu->iem.s.pCtxR3   = CPUMQueryGuestCtxPtr(pVCpu);
        pVCpu->iem.s.pCtxR0   = VM_R0_ADDR(pVM, pVCpu->iem.s.pCtxR3);
        pVCpu->iem.s.pCtxRC   = VM_RC_ADDR(pVM, pVCpu->iem.s.pCtxR3);

        STAMR3RegisterF(pVM, &pVCpu->iem.s.cInstructions,             STAMTYPE_U32,       STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Instructions interpreted",          "/IEM/CPU%u/cInstructions", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.cPotentialExits,           STAMTYPE_U32,       STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Potential exits",                   "/IEM/CPU%u/cPotentialExits", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.cRetAspectNotImplemented,  STAMTYPE_U32_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "VERR_IEM_ASPECT_NOT_IMPLEMENTED",   "/IEM/CPU%u/cRetAspectNotImplemented", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.cRetInstrNotImplemented,   STAMTYPE_U32_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "VERR_IEM_INSTR_NOT_IMPLEMENTED",    "/IEM/CPU%u/cRetInstrNotImplemented", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.cRetInfStatuses,           STAMTYPE_U32_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Informational statuses returned",   "/IEM/CPU%u/cRetInfStatuses", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.cRetErrStatuses,           STAMTYPE_U32_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Error statuses returned",           "/IEM/CPU%u/cRetErrStatuses", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.cbWritten,                 STAMTYPE_U32,       STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,
                        "Approx bytes written",              "/IEM/CPU%u/cbWritten", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.cPendingCommit,            STAMTYPE_U32,       STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,
                        "Times RC/R0 had to postpone instruction committing to ring-3", "/IEM/CPU%u/cPendingCommit", idCpu);

        /*
         * Host and guest CPU information.
         */
#if IEM_CFG_TARGET_CPU == IEMTARGETCPU_DYNAMIC
        pVCpu->iem.s.uTargetCpu = IEMTARGETCPU_CURRENT;
#endif
        if (idCpu == 0)
        {
            pVCpu->iem.s.enmCpuVendor             = CPUMGetGuestCpuVendor(pVM);
            pVCpu->iem.s.enmHostCpuVendor         = CPUMGetHostCpuVendor(pVM);
        }
        else
        {
            pVCpu->iem.s.enmCpuVendor             = pVM->aCpus[0].iem.s.enmCpuVendor;
            pVCpu->iem.s.enmHostCpuVendor         = pVM->aCpus[0].iem.s.enmHostCpuVendor;
        }

        /*
         * Mark all buffers free.
         */
        uint32_t iMemMap = RT_ELEMENTS(pVCpu->iem.s.aMemMappings);
        while (iMemMap-- > 0)
            pVCpu->iem.s.aMemMappings[iMemMap].fAccess = IEM_ACCESS_INVALID;
    }
    return VINF_SUCCESS;
}


VMMR3DECL(int)      IEMR3Term(PVM pVM)
{
    NOREF(pVM);
    return VINF_SUCCESS;
}


VMMR3DECL(void)     IEMR3Relocate(PVM pVM)
{
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
        pVM->aCpus[idCpu].iem.s.pCtxRC = VM_RC_ADDR(pVM, pVM->aCpus[idCpu].iem.s.pCtxR3);
}

