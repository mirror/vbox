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
    return GMMR3CheckSharedModules(pVM);
}

/**
 * Shared module check helper (called on the way out).
 *
 * @param   pVM         The VM handle.
 */
static DECLCALLBACK(void) pgmR3CheckSharedModulesHelper(PVM pVM)
{   
    /* We must stall other VCPUs as we'd otherwise have to send IPI flush commands for every single change we make. */
    int rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ONCE, pgmR3SharedModuleRegRendezvous, NULL);
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
    return VMR3ReqCallNoWait(pVM, VMMGetCpuId(pVM), (PFNRT)pgmR3CheckSharedModulesHelper, 1, pVM);
#else 
    return VERR_NOT_IMPLEMENTED;
#endif
}
