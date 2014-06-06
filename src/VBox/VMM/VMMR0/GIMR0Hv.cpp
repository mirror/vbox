/* $Id$ */
/** @file
 * Guest Interface Manager (GIM), Hyper-V - Host Context Ring-0.
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
#include <iprt/asm.h>
#include <iprt/memobj.h>
#include <VBox/vmm/gim.h>
#include <VBox/vmm/vm.h>


/**
 * Allocates and maps one physically contiguous page. The allocated page is
 * zero'd out.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Pointer to the ring-0 memory object.
 * @param   ppVirt          Where to store the virtual address of the
 *                          allocation.
 * @param   pPhys           Where to store the physical address of the
 *                          allocation.
 */
static int gimR0HvPageAllocZ(PRTR0MEMOBJ pMemObj, PRTR0PTR ppVirt, PRTHCPHYS pHCPhys)
{
    AssertPtr(pMemObj);
    AssertPtr(ppVirt);
    AssertPtr(pHCPhys);

    int rc = RTR0MemObjAllocCont(pMemObj, PAGE_SIZE, false /* fExecutable */);
    if (RT_FAILURE(rc))
        return rc;
    *ppVirt  = RTR0MemObjAddress(*pMemObj);
    *pHCPhys = RTR0MemObjGetPagePhysAddr(*pMemObj, 0 /* iPage */);
    ASMMemZero32(*ppVirt, PAGE_SIZE);
    return VINF_SUCCESS;
}


/**
 * Frees and unmaps an allocated physical page.
 *
 * @param   pMemObj         Pointer to the ring-0 memory object.
 * @param   ppVirt          Where to re-initialize the virtual address of
 *                          allocation as 0.
 * @param   pHCPhys         Where to re-initialize the physical address of the
 *                          allocation as 0.
 */
static void gimR0HvPageFree(PRTR0MEMOBJ pMemObj, PRTR0PTR ppVirt, PRTHCPHYS pHCPhys)
{
    AssertPtr(pMemObj);
    AssertPtr(ppVirt);
    AssertPtr(pHCPhys);
    if (*pMemObj != NIL_RTR0MEMOBJ)
    {
        int rc = RTR0MemObjFree(*pMemObj, true /* fFreeMappings */);
        AssertRC(rc);
        *pMemObj = NIL_RTR0MEMOBJ;
        *ppVirt  = 0;
        *pHCPhys = 0;
    }
}


/**
 * Does ring-0 per-VM GIM Hyper-V initialization.
 *
 * @returns VBox status code.
 * @param   pVM     Pointer to the VM.
 */
VMMR0_INT_DECL(int) GIMR0HvInitVM(PVM pVM)
{
#if 0
    AssertPtr(pVM);
    Assert(GIMIsEnabled(pVM));

    PGIMHV pHv = &pVM->gim.s.u.Hv;
    Assert(pHv->hMemObjTscPage == NIL_RTR0MEMOBJ);

    /*
     * Allocate the TSC page.
     */
    int rc = gimR0HvPageAllocZ(&pHv->hMemObjTscPage, &pHv->pvTscPageR0, &pHv->HCPhysTscPage);
    if (RT_FAILURE(rc))
        goto cleanup;
#endif

    return VINF_SUCCESS;

#if 0
cleanup:
    gimR0HvPageFree(&pHv->hMemObjTscPage, &pHv->pvTscPageR0, &pHv->HCPhysTscPage);
    return rc;
#endif
}


/**
 * Does ring-0 per-VM GIM Hyper-V termination.
 *
 * @returns VBox status code.
 * @param   pVM     Pointer to the VM.
 */
VMMR0_INT_DECL(int) GIMR0HvTermVM(PVM pVM)
{
    AssertPtr(pVM);
    Assert(GIMIsEnabled(pVM));
#if 0
    PGIMHV pHv = &pVM->gim.s.u.Hv;
    gimR0HvPageFree(&pHv->hMemObjTscPage, &pHv->pvTscPageR0, &pHv->HCPhysTscPage);
#endif
    return VINF_SUCCESS;
}

