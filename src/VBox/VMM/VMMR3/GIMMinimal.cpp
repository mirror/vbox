/* $Id$ */
/** @file
 * GIM - Guest Interface Manager, Minimal implementation.
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

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/asm-amd64-x86.h>

#include <VBox/vmm/cpum.h>
#include <VBox/vmm/vm.h>

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/

VMMR3_INT_DECL(int) GIMR3MinimalInit(PVM pVM, GIMOSID enmGuest)
{
    AssertReturn(pVM, VERR_INVALID_PARAMETER);
    AssertReturn(pVM->gim.s.enmProviderId == GIMPROVIDERID_MINIMAL, VERR_INTERNAL_ERROR_5);

    /*
     * Enable the Hypervisor Present.
     */
    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_HVP);

    if (GIMR3IsOSXGuest(enmGuest))
    {
        /*
         * Enable MWAIT Extensions for OS X Guests.
         */
        CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_MWAIT_EXTS);

        /*
         * Fake the CPU family/model so the guest works. This is partly because older
         * Mac releases really doesn't work on newer CPUs, and partly because OS X
         * expects more from systems with newer CPUs (MSRs, power features etc.)
         */
        if (CPUMGetGuestCpuVendor(pVM) == CPUMCPUVENDOR_INTEL)
        {
            uint32_t uMaxIntelFamilyModelStep = UINT32_MAX;
            switch (enmGuest)
            {
                case GIMOSID_OSX:
                case GIMOSID_OSX_64:
                    uMaxIntelFamilyModelStep = RT_MAKE_U32_FROM_U8(1, 23, 6, 7); /* Penryn / X5482. */
                    break;

                case GIMOSID_OSX_106:
                case GIMOSID_OSX_106_64:
                    uMaxIntelFamilyModelStep = RT_MAKE_U32_FROM_U8(1, 23, 6, 7); /* Penryn / X5482 */
                    break;

                case GIMOSID_OSX_107:
                case GIMOSID_OSX_107_64:
                    /** @todo Figure out what is required here. */
                    uMaxIntelFamilyModelStep = RT_MAKE_U32_FROM_U8(1, 23, 6, 7); /* Penryn / X5482 */
                    break;

                case GIMOSID_OSX_108:
                case GIMOSID_OSX_108_64:
                    /** @todo Figure out what is required here. */
                    uMaxIntelFamilyModelStep = RT_MAKE_U32_FROM_U8(1, 23, 6, 7); /* Penryn / X5482 */
                    break;

                case GIMOSID_OSX_109:
                case GIMOSID_OSX_109_64:
                    /** @todo Figure out what is required here. */
                    uMaxIntelFamilyModelStep = RT_MAKE_U32_FROM_U8(1, 23, 6, 7); /* Penryn / X5482 */
                    break;

                default:    /* shut up gcc */
                    break;
            }

            if (uMaxIntelFamilyModelStep != UINT32_MAX)
            {
                CPUMCPUIDLEAF Leaf;
                int rc = CPUMR3CpuIdGetLeaf(pVM, &Leaf, 1, 0 /* uSubLeaf */);
                if (RT_SUCCESS(rc))
                {
                    uint32_t uCurIntelFamilyModelStep = RT_MAKE_U32_FROM_U8(ASMGetCpuStepping(Leaf.uEax),
                                                                            ASMGetCpuModelIntel(Leaf.uEax),
                                                                            ASMGetCpuFamily(Leaf.uEax),
                                                                            0);
                    if (uMaxIntelFamilyModelStep < uCurIntelFamilyModelStep)
                    {
                        uint32_t uNew = Leaf.uEax & UINT32_C(0xf0003000);
                        uNew |=  RT_BYTE1(uMaxIntelFamilyModelStep) & 0xf;          /* stepping */
                        uNew |= (RT_BYTE2(uMaxIntelFamilyModelStep) & 0xf) << 4;    /* 4 low model bits */
                        uNew |= (RT_BYTE2(uMaxIntelFamilyModelStep) >> 4) << 16;    /* 4 high model bits */
                        uNew |= (RT_BYTE3(uMaxIntelFamilyModelStep) & 0xf) << 8;    /* 4 low family bits */
                        /* 8 high family bits, Intel's suggested calculation. */
                        if (RT_BYTE3(uMaxIntelFamilyModelStep) > 0xf)
                        {
                            uNew |= (  (RT_BYTE3(uMaxIntelFamilyModelStep)
                                     - (RT_BYTE3(uMaxIntelFamilyModelStep) & 0xf)) & 0xff ) << 20;
                        }

                        LogRel(("GIM: Minimal: CPUID(0).EAX %#x -> %#x (uMaxIntelFamilyModelStep=%#x, uCurIntelFamilyModelStep=%#x\n",
                                Leaf.uEax, uNew, uMaxIntelFamilyModelStep, uCurIntelFamilyModelStep));
                        Leaf.uEax = uNew;
                    }

                    rc = CPUMR3CpuIdInsert(pVM, &Leaf);
                    if (RT_FAILURE(rc))
                    {
                        LogRel(("GIM: Minimal: Failed to insert CPUID leaf for OS X guest. rc=%Rrc\n", rc));
                        return rc;
                    }
                }
                else
                {
                    LogRel(("GIM: Minimal: Failed to retreive std. CPUID leaf. rc=%Rrc\n", rc));
                    return rc;
                }
            }
        }
    }

    return VINF_SUCCESS;
}


VMMR3_INT_DECL(void) GIMR3MinimalRelocate(PVM pVM, RTGCINTPTR offDelta)
{
    NOREF(pVM); NOREF(offDelta);
}

