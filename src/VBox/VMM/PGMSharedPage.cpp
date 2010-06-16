/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor, Shared page handling
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_PGM_SHARED
#include <VBox/pgm.h>
#include <VBox/stam.h>
#include "PGMInternal.h"
#include "PGMInline.h"
#include <VBox/vm.h>
#include <VBox/sup.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/mem.h>


/**
 * Registers a new shared module for the VM
 *
 * @returns VBox status code.
 * @param   pVM                 VM handle
 * @param   enmGuestOS          Guest OS type
 * @param   pszModuleName       Module name
 * @param   pszVersion          Module version
 * @param   GCBaseAddr          Module base address
 * @param   cbModule            Module size
 * @param   cRegions            Number of shared region descriptors
 * @param   pRegions            Shared region(s)
 */
VMMR3DECL(int) PGMR3SharedModuleRegister(PVM pVM, VBOXOSFAMILY enmGuestOS, char *pszModuleName, char *pszVersion, RTGCPTR GCBaseAddr, uint32_t cbModule,
                                         unsigned cRegions, VMMDEVSHAREDREGIONDESC *pRegions)
{
#ifdef VBOX_WITH_PAGE_SHARING
    PGMMREGISTERSHAREDMODULEREQ pReq;

    Log(("PGMR3SharedModuleRegister family=%d name=%s version=%s base=%RGv size=%x cRegions=%d\n", enmGuestOS, pszModuleName, pszVersion, GCBaseAddr, cbModule, cRegions));

    /* Sanity check. */
    AssertReturn(cRegions < VMMDEVSHAREDREGIONDESC_MAX, VERR_INVALID_PARAMETER);

    pReq = (PGMMREGISTERSHAREDMODULEREQ)RTMemAllocZ(RT_OFFSETOF(GMMREGISTERSHAREDMODULEREQ, aRegions[cRegions]));
    AssertReturn(pReq, VERR_NO_MEMORY);

    pReq->enmGuestOS    = enmGuestOS;
    pReq->GCBaseAddr    = GCBaseAddr;
    pReq->cbModule      = cbModule;
    pReq->cRegions      = cRegions;
    for (unsigned i = 0; i < cRegions; i++)
        pReq->aRegions[i] = pRegions[i];

    if (    RTStrCopy(pReq->szName, sizeof(pReq->szName), pszModuleName) != VINF_SUCCESS
        ||  RTStrCopy(pReq->szVersion, sizeof(pReq->szVersion), pszVersion) != VINF_SUCCESS)
    {
        RTMemFree(pReq);
        return VERR_BUFFER_OVERFLOW;
    }

    int rc = GMMR3RegisterSharedModule(pVM, pReq);
    RTMemFree(pReq);
    Assert(rc == VINF_SUCCESS || rc == VINF_PGM_SHARED_MODULE_COLLISION || rc == VINF_PGM_SHARED_MODULE_ALREADY_REGISTERED);
    if (RT_FAILURE(rc))
        return rc;
    return VINF_SUCCESS;
#else
    return VERR_NOT_IMPLEMENTED;
#endif
}

/**
 * Unregisters a shared module for the VM
 *
 * @returns VBox status code.
 * @param   pVM                 VM handle
 * @param   pszModuleName       Module name
 * @param   pszVersion          Module version
 * @param   GCBaseAddr          Module base address
 * @param   cbModule            Module size
 */
VMMR3DECL(int) PGMR3SharedModuleUnregister(PVM pVM, char *pszModuleName, char *pszVersion, RTGCPTR GCBaseAddr, uint32_t cbModule)
{
#ifdef VBOX_WITH_PAGE_SHARING
    PGMMUNREGISTERSHAREDMODULEREQ pReq;

    Log(("PGMR3SharedModuleUnregister name=%s version=%s base=%RGv size=%x\n", pszModuleName, pszVersion, GCBaseAddr, cbModule));

    pReq = (PGMMUNREGISTERSHAREDMODULEREQ)RTMemAllocZ(sizeof(*pReq));
    AssertReturn(pReq, VERR_NO_MEMORY);

    pReq->GCBaseAddr    = GCBaseAddr;
    pReq->cbModule      = cbModule;

    if (    RTStrCopy(pReq->szName, sizeof(pReq->szName), pszModuleName) != VINF_SUCCESS
        ||  RTStrCopy(pReq->szVersion, sizeof(pReq->szVersion), pszVersion) != VINF_SUCCESS)
    {
        RTMemFree(pReq);
        return VERR_BUFFER_OVERFLOW;
    }
    int rc = GMMR3UnregisterSharedModule(pVM, pReq);
    RTMemFree(pReq);
    return rc;
#else
    return VERR_NOT_IMPLEMENTED;
#endif
}

#ifdef VBOX_WITH_PAGE_SHARING
/**
 * Rendezvous callback that will be called once.
 *
 * @returns VBox strict status code.
 * @param   pVM                 VM handle.
 * @param   pVCpu               The VMCPU handle for the calling EMT.
 * @param   pvUser              Not used;
 */
static DECLCALLBACK(VBOXSTRICTRC) pgmR3SharedModuleRegRendezvous(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    VMCPUID idCpu = *(VMCPUID *)pvUser;

    /* Execute on the VCPU that issued the original request to make sure we're in the right cr3 context. */
    if (pVCpu->idCpu != idCpu)
    {
        Assert(pVM->cCpus > 1);
        return VINF_SUCCESS;
    }

    /* Flush all pending handy page operations before changing any shared page assignments. */
    int rc = PGMR3PhysAllocateHandyPages(pVM);
    AssertRC(rc);

    /* Lock it here as we can't deal with busy locks in this ring-0 path. */
    pgmLock(pVM);
    rc = GMMR3CheckSharedModules(pVM);
    pgmUnlock(pVM);

    return rc;
}

/**
 * Shared module check helper (called on the way out).
 *
 * @param   pVM         The VM handle.
 * @param   VMCPUID     VCPU id
 */
static DECLCALLBACK(void) pgmR3CheckSharedModulesHelper(PVM pVM, VMCPUID idCpu)
{
    /* We must stall other VCPUs as we'd otherwise have to send IPI flush commands for every single change we make. */
    int rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ONE_BY_ONE, pgmR3SharedModuleRegRendezvous, &idCpu);
    AssertRC(rc);
}
#endif

/**
 * Check all registered modules for changes.
 *
 * @returns VBox status code.
 * @param   pVM                 VM handle
 */
VMMR3DECL(int) PGMR3SharedModuleCheckAll(PVM pVM)
{
#ifdef VBOX_WITH_PAGE_SHARING
    /* Queue the actual registration as we are under the IOM lock right now. Perform this operation on the way out. */
    return VMR3ReqCallNoWait(pVM, VMCPUID_ANY_QUEUE, (PFNRT)pgmR3CheckSharedModulesHelper, 2, pVM, VMMGetCpuId(pVM));
#else
    return VERR_NOT_IMPLEMENTED;
#endif
}

/**
 * Query the state of a page in a shared module
 *
 * @returns VBox status code.
 * @param   pVM                 VM handle
 * @param   GCPtrPage           Page address
 * @param   pfShared            Shared status (out)
 * @param   puPageFlags         Page flags (out)
 */
VMMR3DECL(int) PGMR3SharedModuleGetPageState(PVM pVM, RTGCPTR GCPtrPage, bool *pfShared, uint64_t *puPageFlags)
{
#if defined(VBOX_WITH_PAGE_SHARING) && defined(DEBUG)
    /* Debug only API for the page fusion testcase. */
    RTGCPHYS GCPhys;
    uint64_t fFlags;

    pgmLock(pVM);

    int rc = PGMGstGetPage(VMMGetCpu(pVM), GCPtrPage, &fFlags, &GCPhys);
    switch (rc)
    {
    case VINF_SUCCESS:
    {
        PPGMPAGE pPage = pgmPhysGetPage(&pVM->pgm.s, GCPhys);
        if (pPage)
        {
            *pfShared    = PGM_PAGE_IS_SHARED(pPage);
            *puPageFlags = fFlags;
        }
        else
            rc = VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
        break;
    }
    case VERR_PAGE_NOT_PRESENT:
    case VERR_PAGE_TABLE_NOT_PRESENT:
    case VERR_PAGE_MAP_LEVEL4_NOT_PRESENT:
    case VERR_PAGE_DIRECTORY_PTR_NOT_PRESENT:
        *pfShared = false;
        *puPageFlags = 0;
        rc = VINF_SUCCESS;
        break;

    default:
        break;
    }

    pgmUnlock(pVM);
    return rc;
#else
    return VERR_NOT_IMPLEMENTED;
#endif
}
