/* $Id$ */
/** @file
 * Advanced Programmable Interrupt Controller (APIC) - Host Context Ring-0.
 */

/*
 * Copyright (C) 2016 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DEV_APIC
#include "APICInternal.h"
#include <iprt/mem.h>
#include <iprt/memobj.h>
#include <iprt/process.h>
#include <VBox/vmm/vm.h>


/**
 * Does ring-0 per-VM APIC initialization.
 *
 * @returns VBox status code.
 * @param   pVM     The cross context VM structure.
 */
VMMR0_INT_DECL(int) APICR0InitVM(PVM pVM)
{
    /* Paranoia. */
    PAPIC pApic = VM_TO_APIC(pVM);
    Assert(pApic->hMapObjApicPibR0 == NIL_RTR0MEMOBJ);
    Assert(pApic->hMapObjApicPibR0 == NIL_RTR0MEMOBJ);

    /* Init. non-zero members. */
    pApic->HCPhysApicPib = NIL_RTHCPHYS;

    /*
     * Allocate and map the pending-interrupt bitmap (PIB).
     *
     * We allocate all the VCPUs' PIBs contiguously in order to save space
     * as physically contiguous allocations are rounded to a multiple of page size.
     */
    size_t const cbApicPib = RT_ALIGN_Z(pVM->cCpus * sizeof(APICPIB), PAGE_SIZE);
    int rc = RTR0MemObjAllocCont(&pApic->hMemObjApicPibR0, cbApicPib, false /* fExecutable */);
    if (RT_SUCCESS(rc))
    {
        pApic->cbApicPib      = cbApicPib;
        pApic->pvApicPibR0    = RTR0MemObjAddress(pApic->hMemObjApicPibR0);
        pApic->HCPhysApicPib  = RTR0MemObjGetPagePhysAddr(pApic->hMemObjApicPibR0, 0 /* iPage */);
        ASMMemZero32(pApic->pvApicPibR0, cbApicPib);

        rc = RTR0MemObjMapUser(&pApic->hMapObjApicPibR0, pApic->hMemObjApicPibR0, (RTR3PTR)-1, 0 /* uAlignment */,
                               RTMEM_PROT_READ | RTMEM_PROT_WRITE, RTR0ProcHandleSelf());
        if (RT_SUCCESS(rc))
            pApic->pvApicPibR3 = RTR0MemObjAddressR3(pApic->hMapObjApicPibR0);
        else
            return rc;
    }
    else
        return rc;

    /*
     * Allocate and map the virtual-APIC page.
     */
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PVMCPU       pVCpu      = &pVM->aCpus[idCpu];
        PAPICCPU     pApicCpu   = VMCPU_TO_APICCPU(pVCpu);
        size_t const cbApicPage = PAGE_SIZE;

        /* Paranoia. */
        Assert(pApicCpu->hMapObjApicPageR0 == NIL_RTR0MEMOBJ);
        Assert(pApicCpu->hMapObjApicPageR0 == NIL_RTR0MEMOBJ);

        /* Init. non-zero members. */
        pApicCpu->HCPhysApicPage = NIL_RTHCPHYS;

        rc = RTR0MemObjAllocCont(&pApicCpu->hMemObjApicPageR0, cbApicPage, false /* fExecutable */);
        if (RT_SUCCESS(rc))
        {
            pApicCpu->cbApicPage     = cbApicPage;
            pApicCpu->pvApicPageR0   = RTR0MemObjAddress(pApicCpu->hMemObjApicPageR0);
            pApicCpu->HCPhysApicPage = RTR0MemObjGetPagePhysAddr(pApicCpu->hMemObjApicPageR0, 0 /* iPage */);
            ASMMemZero32(pApicCpu->pvApicPageR0, cbApicPage);

            rc = RTR0MemObjMapUser(&pApicCpu->hMapObjApicPageR0, pApicCpu->hMemObjApicPageR0, (RTR3PTR)-1, 0 /* uAlignment */,
                                   RTMEM_PROT_READ | RTMEM_PROT_WRITE, RTR0ProcHandleSelf());
            if (RT_SUCCESS(rc))
            {
                pApicCpu->pvApicPageR3 = RTR0MemObjAddressR3(pApicCpu->hMapObjApicPageR0);

                /*
                 * Associate the per-VCPU PIB pointers to the per-VM PIB allocation.
                 */
                const size_t offApicPib = idCpu * sizeof(APICPIB);
                pApicCpu->HCPhysApicPib = pApic->HCPhysApicPib + offApicPib;
                pApicCpu->pvApicPibR0   = (RTR0PTR)((uint8_t *)pApic->pvApicPibR0 + offApicPib);
                pApicCpu->pvApicPibR3   = (RTR3PTR)((uint8_t *)pApic->pvApicPibR3 + offApicPib);
            }
            else
                return rc;
        }
        else
            return rc;
    }

    return VINF_SUCCESS;
}


/**
 * Does ring-0 per-VM APIC termination.
 *
 * @returns VBox status code.
 * @param   pVM     The cross context VM structure.
 */
VMMR0_INT_DECL(int) APICR0TermVM(PVM pVM)
{
    PAPIC pApic = VM_TO_APIC(pVM);
    if (pApic->hMapObjApicPibR0 != NIL_RTR0MEMOBJ)
    {
        RTR0MemObjFree(pApic->hMapObjApicPibR0, true /* fFreeMappings */);
        pApic->hMapObjApicPibR0 = NIL_RTR0MEMOBJ;
        pApic->pvApicPibR3 = NULL;
    }
    if (pApic->hMemObjApicPibR0 != NIL_RTR0MEMOBJ)
    {
        RTR0MemObjFree(pApic->hMemObjApicPibR0, true /* fFreeMappings */);
        pApic->hMemObjApicPibR0 = NIL_RTR0MEMOBJ;
        pApic->pvApicPibR0 = NULL;
        pApic->HCPhysApicPib = NIL_RTHCPHYS;
    }

    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PVMCPU   pVCpu    = &pVM->aCpus[idCpu];
        PAPICCPU pApicCpu = &pVCpu->apic.s;

        if (pApicCpu->hMapObjApicPageR0 != NIL_RTR0MEMOBJ)
        {
            RTR0MemObjFree(pApicCpu->hMapObjApicPageR0, true /* fFreeMappings */);
            pApicCpu->hMapObjApicPageR0 = NIL_RTR0MEMOBJ;
            pApicCpu->pvApicPageR3 = NULL;
        }
        if (pApicCpu->hMemObjApicPageR0 != NIL_RTR0MEMOBJ)
        {
            RTR0MemObjFree(pApicCpu->hMemObjApicPageR0, true /* fFreeMappings */);
            pApicCpu->hMemObjApicPageR0 = NIL_RTR0MEMOBJ;
            pApicCpu->pvApicPageR0 = NULL;
            pApicCpu->HCPhysApicPage = NIL_RTHCPHYS;
        }
    }

    return VINF_SUCCESS;
}

