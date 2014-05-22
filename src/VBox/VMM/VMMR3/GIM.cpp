/* $Id$ */
/** @file
 * GIM - Guest Interface Manager.
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
 * ease the guest in running under a recognized, virtualized environment. This
 * is also referred to as paravirtualization interfaces.
 *
 * The idea behind this is primarily for making guests more accurate and
 * efficient when operating in a virtualized environment.
 *
 * For instance, a guest when interfaced to VirtualBox through a GIM provider
 * may rely on the provider (and VirtualBox ultimately) for providing the
 * correct TSC frequency and may therefore not have to caliberate the TSC
 * itself, resulting in higher accuracy and saving several CPU cycles.
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

#include <iprt/err.h>
#include <iprt/string.h>

/* Include all GIM providers. */
#include "GIMMinimalInternal.h"
#include "GIMHvInternal.h"

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) gimR3Save(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int) gimR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass);


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
    int rc;
#if 0
    rc = SSMR3RegisterInternal(pVM, "GIM", 0, GIM_SSM_VERSION, sizeof(GIM),
                                    NULL, NULL, NULL,
                                    NULL, gimR3Save, NULL,
                                    NULL, gimR3Load, NULL);
    if (RT_FAILURE(rc))
        return rc;
#endif

    /*
     * Read configuration.
     */
    PCFGMNODE pCfgNode = CFGMR3GetChild(CFGMR3GetRoot(pVM), "GIM/");

    /** @cfgm{GIM/Provider, string}
     * The name of the GIM provider. The default is "none". */
    char szProvider[64];
    rc = CFGMR3QueryStringDef(pCfgNode, "Provider", szProvider, sizeof(szProvider), "None");
    AssertLogRelRCReturn(rc, rc);

    /** @cfgm{GIM/Version, uint32_t}
     * The interface version. The default is 0, which means "provide the most
     * up-to-date implementation". */
    uint32_t uVersion;
    rc = CFGMR3QueryU32Def(pCfgNode, "Version", &uVersion, 0 /* default */);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Setup the GIM provider for this VM.
     */
    LogRel(("GIM: Using provider \"%s\" version %u\n", szProvider, uVersion));
    if (!RTStrCmp(szProvider, "None"))
    {
        Assert(!pVM->gim.s.fEnabled);
        pVM->gim.s.enmProviderId = GIMPROVIDERID_NONE;
    }
    else
    {
        pVM->gim.s.fEnabled = true;
        pVM->gim.s.u32Version = uVersion;
        if (!RTStrCmp(szProvider, "Minimal"))
        {
            pVM->gim.s.enmProviderId = GIMPROVIDERID_MINIMAL;
            rc = GIMR3MinimalInit(pVM);
        }
        else if (!RTStrCmp(szProvider, "HyperV"))
        {
            pVM->gim.s.enmProviderId = GIMPROVIDERID_HYPERV;
            rc = GIMR3HvInit(pVM);
        }
        /** @todo KVM and others. */
        else
        {
            LogRel(("GIM: Provider \"%s\" unknown.\n", szProvider));
            rc = VERR_NOT_SUPPORTED;
        }
    }

    return rc;
}


/**
 * Applies relocations to data and code managed by this component. This function
 * will be called at init and whenever the VMM need to relocate itself inside
 * the GC.
 *
 * @param   pVM         Pointer to the VM.
 * @param   offDelta    Relocation delta relative to old location.
 */
VMM_INT_DECL(void) GIMR3Relocate(PVM pVM, RTGCINTPTR offDelta)
{
    LogFlow(("GIMR3Relocate\n"));

    if (   !pVM->gim.s.fEnabled
        || HMIsEnabled(pVM))
    {
        return;
    }

    switch (pVM->gim.s.enmProviderId)
    {
        case GIMPROVIDERID_MINIMAL:
        {
            GIMR3MinimalRelocate(pVM, offDelta);
            break;
        }

        case GIMPROVIDERID_HYPERV:
        {
            GIMR3HvRelocate(pVM, offDelta);
            break;
        }

        case GIMPROVIDERID_KVM:            /** @todo KVM. */
        default:
        {
            AssertMsgFailed(("Invalid provider Id %#x\n", pVM->gim.s.enmProviderId));
            break;
        }
    }
}


/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the VM.
 * @param   pSSM            SSM operation handle.
 */
DECLCALLBACK(int) gimR3Save(PVM pVM, PSSMHANDLE pSSM)
{
    /** @todo save state. */
    return VINF_SUCCESS;
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
DECLCALLBACK(int) gimR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    /** @todo load state. */
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
    return VINF_SUCCESS;
}

