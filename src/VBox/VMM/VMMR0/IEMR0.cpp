/* $Id$ */
/** @file
 * IEM - Interpreted Execution Manager - Ring-0.
 */

/*
 * Copyright (C) 2011-2022 Oracle Corporation
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
#define LOG_GROUP   LOG_GROUP_IEM
#define VMCPU_INCL_CPUM_GST_CTX
#include <VBox/vmm/iem.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/pgm.h>
#include "IEMInternal.h"
#include <VBox/vmm/vmcc.h>
#include <VBox/log.h>
#include <iprt/errcore.h>



VMMR0_INT_DECL(int) IEMR0InitVM(PGVM pGVM)
{
    AssertCompile(sizeof(pGVM->iem.s) <= sizeof(pGVM->iem.padding));
    AssertCompile(sizeof(pGVM->aCpus[0].iem.s) <= sizeof(pGVM->aCpus[0].iem.padding));

#ifdef VBOX_WITH_NESTED_HWVIRT_VMX
    /*
     * Register the per-VM VMX APIC-access page handler type.
     */
    if (pGVM->cpum.ro.GuestFeatures.fVmx)
    {
        int rc = PGMR0HandlerPhysicalTypeSetUpContext(pGVM, PGMPHYSHANDLERKIND_ALL, 0 /*fFlags*/,
                                                      iemVmxApicAccessPageHandler, iemVmxApicAccessPagePfHandler,
                                                      "VMX APIC-access page", pGVM->iem.s.hVmxApicAccessPage);
        AssertLogRelRCReturn(rc, rc);
    }
#endif
    return VINF_SUCCESS;
}

