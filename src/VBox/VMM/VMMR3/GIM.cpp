/* $Id$ */
/** @file
 * GIM - Guest Interface Manager.
 */

/*
 * Copyright (C) 2014-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/** @page pg_gim        GIM - The Guest Interface Manager
 *
 * The Guest Interface Manager abstracts an interface provider through which
 * guests may interact with the hypervisor.
 *
 * @see grp_gim
 *
 *
 * @section sec_gim_provider   Providers
 *
 * A GIM provider implements a particular hypervisor interface such as Microsoft
 * Hyper-V, Linux KVM and so on. It hooks into various components in the VMM to
 * ease the guest in running under a recognized, virtualized environment.
 *
 * The GIM provider configured for the VM needs to be recognized by the guest OS
 * in order to make use of features supported by the interface. Since it
 * requires co-operation from the guest OS, a GIM provider may also referred to
 * as a paravirtualization interface.
 *
 * One of the goals of having a paravirtualized interface is for enabling guests
 * to be more accurate and efficient when operating in a virtualized
 * environment. For instance, a guest OS which interfaces to VirtualBox through
 * a GIM provider may rely on the provider for supplying the correct TSC
 * frequency of the host processor. The guest can then avoid caliberating the
 * TSC itself, resulting in higher accuracy and better performance.
 *
 * At most, only one GIM provider can be active for a running VM and cannot be
 * changed during the lifetime of the VM.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_GIM
#include <VBox/log.h>
#include "GIMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/ssm.h>
#include <VBox/vmm/pdmdev.h>

#include <iprt/err.h>
#include <iprt/string.h>

/* Include all GIM providers. */
#include "GIMMinimalInternal.h"
#include "GIMHvInternal.h"
#include "GIMKvmInternal.h"

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) gimR3Save(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int) gimR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t uSSMVersion, uint32_t uPass);


/**
 * Initializes the GIM.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
VMMR3_INT_DECL(int) GIMR3Init(PVM pVM)
{
    LogFlow(("GIMR3Init\n"));

    /*
     * Assert alignment and sizes.
     */
    AssertCompile(sizeof(pVM->gim.s) <= sizeof(pVM->gim.padding));

    /*
     * Register the saved state data unit.
     */
    int rc = SSMR3RegisterInternal(pVM, "GIM", 0 /* uInstance */, GIM_SAVED_STATE_VERSION, sizeof(GIM),
                                   NULL /* pfnLivePrep */, NULL /* pfnLiveExec */, NULL /* pfnLiveVote*/,
                                   NULL /* pfnSavePrep */, gimR3Save,              NULL /* pfnSaveDone */,
                                   NULL /* pfnLoadPrep */, gimR3Load,              NULL /* pfnLoadDone */);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Read configuration.
     */
    PCFGMNODE pCfgNode = CFGMR3GetChild(CFGMR3GetRoot(pVM), "GIM/");

    /** @cfgm{/GIM/Provider, string}
     * The name of the GIM provider. The default is "none". */
    char szProvider[64];
    rc = CFGMR3QueryStringDef(pCfgNode, "Provider", szProvider, sizeof(szProvider), "None");
    AssertLogRelRCReturn(rc, rc);

    /** @cfgm{/GIM/Version, uint32_t}
     * The interface version. The default is 0, which means "provide the most
     * up-to-date implementation". */
    uint32_t uVersion;
    rc = CFGMR3QueryU32Def(pCfgNode, "Version", &uVersion, 0 /* default */);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Setup the GIM provider for this VM.
     */
    LogRel(("GIM: Using provider \"%s\" (Implementation version: %u)\n", szProvider, uVersion));
    if (!RTStrCmp(szProvider, "None"))
        pVM->gim.s.enmProviderId = GIMPROVIDERID_NONE;
    else
    {
        pVM->gim.s.u32Version = uVersion;
        /** @todo r=bird: Because u32Version is saved, it should be translated to the
         *        'most up-to-date implementation' version number when 0. Otherwise,
         *        we'll have abiguities when loading the state of older VMs. */
        if (!RTStrCmp(szProvider, "Minimal"))
        {
            pVM->gim.s.enmProviderId = GIMPROVIDERID_MINIMAL;
            rc = gimR3MinimalInit(pVM);
        }
        else if (!RTStrCmp(szProvider, "HyperV"))
        {
            pVM->gim.s.enmProviderId = GIMPROVIDERID_HYPERV;
            rc = gimR3HvInit(pVM);
        }
        else if (!RTStrCmp(szProvider, "KVM"))
        {
            pVM->gim.s.enmProviderId = GIMPROVIDERID_KVM;
            rc = gimR3KvmInit(pVM);
        }
        /** @todo KVM and others. */
        else
            rc = VMR3SetError(pVM->pUVM, VERR_GIM_INVALID_PROVIDER, RT_SRC_POS, "Provider \"%s\" unknown.", szProvider);
    }
    return rc;
}


/**
 * Initializes the remaining bits of the GIM provider.
 *
 * This is called after initializing HM and most other VMM components.
 *
 * @returns VBox status code.
 * @param   pVM                 Pointer to the VM.
 * @param   enmWhat             What has been completed.
 * @thread  EMT(0)
 */
VMMR3_INT_DECL(int) GIMR3InitCompleted(PVM pVM)
{
    switch (pVM->gim.s.enmProviderId)
    {
        case GIMPROVIDERID_MINIMAL:
            return gimR3MinimalInitCompleted(pVM);

        case GIMPROVIDERID_HYPERV:
            return gimR3HvInitCompleted(pVM);

        case GIMPROVIDERID_KVM:
            return gimR3KvmInitCompleted(pVM);

        default:
            break;
    }

    if (!TMR3CpuTickIsFixedRateMonotonic(pVM, true /* fWithParavirtEnabled */))
        LogRel(("GIM: Warning!!! Host TSC is unstable. The guest may behave unpredictably with a paravirtualized clock.\n"));

    return VINF_SUCCESS;
}


/**
 * Applies relocations to data and code managed by this component.
 *
 * This function will be called at init and whenever the VMM need to relocate
 * itself inside the GC.
 *
 * @param   pVM         Pointer to the VM.
 * @param   offDelta    Relocation delta relative to old location.
 */
VMM_INT_DECL(void) GIMR3Relocate(PVM pVM, RTGCINTPTR offDelta)
{
    LogFlow(("GIMR3Relocate\n"));

    if (   pVM->gim.s.enmProviderId == GIMPROVIDERID_NONE
        || HMIsEnabled(pVM))
        return;

    switch (pVM->gim.s.enmProviderId)
    {
        case GIMPROVIDERID_MINIMAL:
        {
            gimR3MinimalRelocate(pVM, offDelta);
            break;
        }

        case GIMPROVIDERID_HYPERV:
        {
            gimR3HvRelocate(pVM, offDelta);
            break;
        }

        case GIMPROVIDERID_KVM:
        {
            gimR3KvmRelocate(pVM, offDelta);
            break;
        }

        default:
        {
            AssertMsgFailed(("Invalid provider Id %#x\n", pVM->gim.s.enmProviderId));
            break;
        }
    }
}


/**
 * Executes state-save operation.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the VM.
 * @param   pSSM            SSM operation handle.
 */
DECLCALLBACK(int) gimR3Save(PVM pVM, PSSMHANDLE pSSM)
{
    AssertReturn(pVM,  VERR_INVALID_PARAMETER);
    AssertReturn(pSSM, VERR_SSM_INVALID_STATE);

    /** @todo Save per-CPU data. */
    int rc = VINF_SUCCESS;
#if 0
    SSMR3PutU32(pSSM, pVM->cCpus);
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        rc = SSMR3PutXYZ(pSSM, pVM->aCpus[i].gim.s.XYZ);
    }
#endif

    /*
     * Save per-VM data.
     */
    SSMR3PutU32(pSSM, pVM->gim.s.enmProviderId);
    SSMR3PutU32(pSSM, pVM->gim.s.u32Version);

    /*
     * Save provider-specific data.
     */
    switch (pVM->gim.s.enmProviderId)
    {
        case GIMPROVIDERID_HYPERV:
            rc = gimR3HvSave(pVM, pSSM);
            AssertRCReturn(rc, rc);
            break;

        case GIMPROVIDERID_KVM:
            rc = gimR3KvmSave(pVM, pSSM);
            AssertRCReturn(rc, rc);
            break;

        default:
            break;
    }

    return rc;
}


/**
 * Execute state load operation.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the VM.
 * @param   pSSM            SSM operation handle.
 * @param   uVersion        Data layout version.
 * @param   uPass           The data pass.
 */
DECLCALLBACK(int) gimR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t uSSMVersion, uint32_t uPass)
{
    if (uPass != SSM_PASS_FINAL)
        return VINF_SUCCESS;
    if (uSSMVersion != GIM_SAVED_STATE_VERSION)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    /** @todo Load per-CPU data. */
    int rc;
#if 0
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        rc = SSMR3PutXYZ(pSSM, pVM->aCpus[i].gim.s.XYZ);
    }
#endif

    /*
     * Load per-VM data.
     */
    uint32_t uProviderId;
    uint32_t uProviderVersion;

    rc = SSMR3GetU32(pSSM, &uProviderId);           AssertRCReturn(rc, rc);
    rc = SSMR3GetU32(pSSM, &uProviderVersion);      AssertRCReturn(rc, rc);

    if ((GIMPROVIDERID)uProviderId != pVM->gim.s.enmProviderId)
        return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Saved GIM provider %u differs from the configured one (%u)."),
                                uProviderId, pVM->gim.s.enmProviderId);
#if 0 /** @todo r=bird: Figure out what you mean to do here with the version. */
    if (uProviderVersion != pVM->gim.s.u32Version)
        return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Saved GIM provider version %u differs from the configured one (%u)."),
                                uProviderVersion, pVM->gim.s.u32Version);
#else
    pVM->gim.s.u32Version = uProviderVersion;
#endif

    /*
     * Load provider-specific data.
     */
    switch (pVM->gim.s.enmProviderId)
    {
        case GIMPROVIDERID_HYPERV:
            rc = gimR3HvLoad(pVM, pSSM, uSSMVersion);
            AssertRCReturn(rc, rc);
            break;

        case GIMPROVIDERID_KVM:
            rc = gimR3KvmLoad(pVM, pSSM, uSSMVersion);
            AssertRCReturn(rc, rc);
            break;

        default:
            break;
    }

    return VINF_SUCCESS;
}


/**
 * Terminates the GIM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM itself is, at this point, powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
VMMR3_INT_DECL(int) GIMR3Term(PVM pVM)
{
    switch (pVM->gim.s.enmProviderId)
    {
        case GIMPROVIDERID_HYPERV:
            return gimR3HvTerm(pVM);

        case GIMPROVIDERID_KVM:
            return gimR3KvmTerm(pVM);

        default:
            break;
    }
    return VINF_SUCCESS;
}


/**
 * The VM is being reset.
 *
 * For the GIM component this means unmapping and unregistering MMIO2 regions
 * and other provider-specific resets.
 *
 * @returns VBox status code.
 * @param   pVM     Pointer to the VM.
 */
VMMR3_INT_DECL(void) GIMR3Reset(PVM pVM)
{
    switch (pVM->gim.s.enmProviderId)
    {
        case GIMPROVIDERID_HYPERV:
            return gimR3HvReset(pVM);

        case GIMPROVIDERID_KVM:
            return gimR3KvmReset(pVM);

        default:
            break;
    }
}


/**
 * Registers the GIM device with VMM.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pDevIns     Pointer to the GIM device instance.
 */
VMMR3DECL(void) GIMR3GimDeviceRegister(PVM pVM, PPDMDEVINS pDevIns)
{
    pVM->gim.s.pDevInsR3 = pDevIns;
}


/**
 * Returns the array of MMIO2 regions that are expected to be registered and
 * later mapped into the guest-physical address space for the GIM provider
 * configured for the VM.
 *
 * @returns Pointer to an array of GIM MMIO2 regions, may return NULL.
 * @param   pVM         Pointer to the VM.
 * @param   pcRegions   Where to store the number of items in the array.
 *
 * @remarks The caller does not own and therefore must -NOT- try to free the
 *          returned pointer.
 */
VMMR3DECL(PGIMMMIO2REGION) GIMR3GetMmio2Regions(PVM pVM, uint32_t *pcRegions)
{
    Assert(pVM);
    Assert(pcRegions);

    *pcRegions = 0;
    switch (pVM->gim.s.enmProviderId)
    {
        case GIMPROVIDERID_HYPERV:
            return gimR3HvGetMmio2Regions(pVM, pcRegions);

        default:
            break;
    }

    return NULL;
}


/**
 * Unmaps a registered MMIO2 region in the guest address space and removes any
 * access handlers for it.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pRegion     Pointer to the GIM MMIO2 region.
 */
VMMR3_INT_DECL(int) GIMR3Mmio2Unmap(PVM pVM, PGIMMMIO2REGION pRegion)
{
    AssertPtr(pVM);
    AssertPtr(pRegion);

    PPDMDEVINS pDevIns = pVM->gim.s.pDevInsR3;
    AssertPtr(pDevIns);
    if (pRegion->fMapped)
    {
        int rc = PGMHandlerPhysicalDeregister(pVM, pRegion->GCPhysPage);
        AssertRC(rc);

        rc = PDMDevHlpMMIO2Unmap(pDevIns, pRegion->iRegion, pRegion->GCPhysPage);
        if (RT_SUCCESS(rc))
        {
            pRegion->fMapped    = false;
            pRegion->GCPhysPage = NIL_RTGCPHYS;
        }
    }
    return VINF_SUCCESS;
}


/**
 * Write access handler for a mapped MMIO2 region. At present, this handler
 * simply ignores writes.
 *
 * In the future we might want to let the GIM provider decide what the handler
 * should do (like throwing #GP faults).
 *
 * @returns VBox status code.
 * @param pVM               Pointer to the VM.
 * @param GCPhys            The guest-physical address of the region.
 * @param pvPhys            Pointer to the region in the guest address space.
 * @param pvBuf             Pointer to the data being read/written.
 * @param cbBuf             The size of the buffer in @a pvBuf.
 * @param enmAccessType     The type of access.
 * @param pvUser            User argument (NULL, not used).
 */
static DECLCALLBACK(int) gimR3Mmio2WriteHandler(PVM pVM, RTGCPHYS GCPhys, void *pvPhys, void *pvBuf, size_t cbBuf,
                                                PGMACCESSTYPE enmAccessType, void *pvUser)
{
    /*
     * Ignore writes to the mapped MMIO2 page.
     */
    Assert(enmAccessType == PGMACCESSTYPE_WRITE);
    return VINF_SUCCESS;        /** @todo Hyper-V says we should #GP(0) fault for writes to the Hypercall and TSC page. */
}


/**
 * Maps a registered MMIO2 region in the guest address space. The region will be
 * made read-only and writes from the guest will be ignored.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the VM.
 * @param   pRegion         Pointer to the GIM MMIO2 region.
 * @param   GCPhysRegion    Where in the guest address space to map the region.
 */
VMMR3_INT_DECL(int) GIMR3Mmio2Map(PVM pVM, PGIMMMIO2REGION pRegion, RTGCPHYS GCPhysRegion)
{
    PPDMDEVINS pDevIns = pVM->gim.s.pDevInsR3;
    AssertPtr(pDevIns);

    /* The guest-physical address must be page-aligned. */
    if (GCPhysRegion & PAGE_OFFSET_MASK)
    {
        LogFunc(("%s: %#RGp not paging aligned\n", pRegion->szDescription, GCPhysRegion));
        return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
    }

    /* Allow only normal pages to be overlaid using our MMIO2 pages (disallow MMIO, ROM, reserved pages). */
    /** @todo Hyper-V doesn't seem to be very strict about this, may be relax
     *        later if some guest really requires it. */
    if (!PGMPhysIsGCPhysNormal(pVM, GCPhysRegion))
    {
        LogFunc(("%s: %#RGp is not normal memory\n", pRegion->szDescription, GCPhysRegion));
        return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
    }

    if (!pRegion->fRegistered)
    {
        LogFunc(("%s: Region has not been registered.\n"));
        return VERR_GIM_IPE_1;
    }

    /*
     * Map the MMIO2 region over the specified guest-physical address.
     */
    int rc = PDMDevHlpMMIO2Map(pDevIns, pRegion->iRegion, GCPhysRegion);
    if (RT_SUCCESS(rc))
    {
        /*
         * Install access-handlers for the mapped page to prevent (ignore) writes to it from the guest.
         */
        rc = PGMR3HandlerPhysicalRegister(pVM,
                                          PGMPHYSHANDLERTYPE_PHYSICAL_WRITE,
                                          GCPhysRegion, GCPhysRegion + (pRegion->cbRegion - 1),
                                          gimR3Mmio2WriteHandler,  NULL /* pvUserR3 */,
                                          NULL /* pszModR0 */, NULL /* pszHandlerR0 */, NIL_RTR0PTR /* pvUserR0 */,
                                          NULL /* pszModRC */, NULL /* pszHandlerRC */, NIL_RTRCPTR /* pvUserRC */,
                                          pRegion->szDescription);
        if (RT_SUCCESS(rc))
        {
            pRegion->fMapped    = true;
            pRegion->GCPhysPage = GCPhysRegion;
            return rc;
        }

        PDMDevHlpMMIO2Unmap(pDevIns, pRegion->iRegion, GCPhysRegion);
    }

    return rc;
}

#if 0
/**
 * Registers the physical handler for the registered and mapped MMIO2 region.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pRegion     Pointer to the GIM MMIO2 region.
 */
VMMR3_INT_DECL(int) GIMR3Mmio2HandlerPhysicalRegister(PVM pVM, PGIMMMIO2REGION pRegion)
{
    AssertPtr(pRegion);
    AssertReturn(pRegion->fRegistered, VERR_GIM_IPE_2);
    AssertReturn(pRegion->fMapped, VERR_GIM_IPE_3);

    return PGMR3HandlerPhysicalRegister(pVM,
                                        PGMPHYSHANDLERTYPE_PHYSICAL_WRITE,
                                        pRegion->GCPhysPage, pRegion->GCPhysPage + (pRegion->cbRegion - 1),
                                        gimR3Mmio2WriteHandler,  NULL /* pvUserR3 */,
                                        NULL /* pszModR0 */, NULL /* pszHandlerR0 */, NIL_RTR0PTR /* pvUserR0 */,
                                        NULL /* pszModRC */, NULL /* pszHandlerRC */, NIL_RTRCPTR /* pvUserRC */,
                                        pRegion->szDescription);
}


/**
 * Deregisters the physical handler for the MMIO2 region.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pRegion     Pointer to the GIM MMIO2 region.
 */
VMMR3_INT_DECL(int) GIMR3Mmio2HandlerPhysicalDeregister(PVM pVM, PGIMMMIO2REGION pRegion)
{
    return PGMHandlerPhysicalDeregister(pVM, pRegion->GCPhysPage);
}
#endif

