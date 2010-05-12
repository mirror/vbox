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
#define LOG_GROUP LOG_GROUP_PGM_PHYS
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


#ifdef VBOX_WITH_PAGE_SHARING
/**
 * Rendezvous callback that will be called once.
 *
 * @returns VBox strict status code.
 * @param   pVM                 VM handle.
 * @param   pVCpu               The VMCPU handle for the calling EMT.
 * @param   pvUser              PGMMREGISTERSHAREDMODULEREQ
 */
static DECLCALLBACK(VBOXSTRICTRC) pgmR3SharedModuleRegRendezvous(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    PGMMREGISTERSHAREDMODULEREQ pReq = (PGMMREGISTERSHAREDMODULEREQ)pvUser;

    return VMMR3CallR0(pVM, VMMR0_DO_PGM_CHECK_SHARED_MODULE, 0, &pReq->Hdr);
}

/**
 * Shared module registration helper (called on the way out).
 *
 * @param   pVM         The VM handle.
 * @param   pReq        Registration request info
 */
static DECLCALLBACK(void) pgmR3SharedModuleRegisterHelper(PVM pVM, PGMMREGISTERSHAREDMODULEREQ pReq)
{
    int rc;
    
    rc = GMMR3RegisterSharedModule(pVM, pReq);
    Assert(rc == VINF_SUCCESS || rc == VINF_PGM_SHARED_MODULE_COLLISION || rc == VINF_PGM_SHARED_MODULE_ALREADY_REGISTERED);
    if (rc == VINF_PGM_SHARED_MODULE_ALREADY_REGISTERED)
    {
        PVMCPU   pVCpu = VMMGetCpu(pVM);
        unsigned cFlushedPages = 0;

        /** todo count copy-on-write actions in the trap handler so we don't have to check everything all the time! */

        /* Count the number of shared pages that were changed (copy-on-write). */
        for (unsigned i = 0; i < pReq->cRegions; i++)
        {
            Assert((pReq->aRegions[i].cbRegion & 0xfff) == 0);
            Assert((pReq->aRegions[i].GCRegionAddr & 0xfff) == 0);

            RTGCPTR GCRegion  = pReq->aRegions[i].GCRegionAddr;
            uint32_t cbRegion = pReq->aRegions[i].cbRegion & ~0xfff;

            while (cbRegion)
            {
                RTGCPHYS GCPhys;
                uint64_t fFlags;

                rc = PGMGstGetPage(pVCpu, GCRegion, &fFlags, &GCPhys);
                if (    rc == VINF_SUCCESS
                    &&  !(fFlags & X86_PTE_RW))
                {
                    PPGMPAGE pPage = pgmPhysGetPage(&pVM->pgm.s, GCPhys);
                    if (    pPage
                        &&  !PGM_PAGE_IS_SHARED(pPage))
                    {
                        cFlushedPages++;
                    }
                }

                GCRegion += PAGE_SIZE;
                cbRegion -= PAGE_SIZE;
            }
        }

        if (cFlushedPages > 32)
            rc = VINF_SUCCESS;  /* force recheck below */
    }
    /* Full (re)check needed? */
    if (rc == VINF_SUCCESS)
    {
        pReq->Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
        pReq->Hdr.cbReq = RT_OFFSETOF(GMMREGISTERSHAREDMODULEREQ, aRegions[pReq->cRegions]);

        /* We must stall other VCPUs as we'd otherwise have to send IPI flush commands for every single change we make. */
        rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ONCE, pgmR3SharedModuleRegRendezvous, pReq);
        AssertRC(rc);
    }
    RTMemFree(pReq);
    return;
}
#endif

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

    /* Queue the actual registration as we are under the IOM lock right now. Perform this operation on the way out. */
    return VMR3ReqCallNoWait(pVM, VMMGetCpuId(pVM), (PFNRT)pgmR3SharedModuleRegisterHelper, 2, pVM, pReq);
#else
    return VERR_NOT_IMPLEMENTED;
#endif
}


#ifdef VBOX_WITH_PAGE_SHARING
/**
 * Shared module unregistration helper (called on the way out).
 *
 * @param   pVM         The VM handle.
 * @param   pReq        Unregistration request info
 */
static DECLCALLBACK(void) pgmR3SharedModuleUnregisterHelper(PVM pVM, PGMMREGISTERSHAREDMODULEREQ pReq)
{
    int rc;
    
    rc = GMMR3UnregisterSharedModule(pVM, pReq);
    RTMemFree(pReq);
    return;
}
#endif

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
        return VERR_BUFFER_OVERFLOW;
    }
    /* Queue the actual registration as we are under the IOM lock right now. Perform this operation on the way out. */
    return VMR3ReqCallNoWait(pVM, VMMGetCpuId(pVM), (PFNRT)pgmR3SharedModuleUnregisterHelper, 2, pVM, pReq);
#else 
    return VERR_NOT_IMPLEMENTED;
#endif
}
