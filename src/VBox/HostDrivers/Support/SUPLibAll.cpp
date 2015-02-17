/* $Id$ */
/** @file
 * VirtualBox Support Library - All Contexts Code.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/sup.h>
#ifdef IN_RC
# include <VBox/vmm/vm.h>
# include <VBox/vmm/vmm.h>
#endif
#ifdef IN_RING0
# include <iprt/mp.h>
#endif
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
# include <iprt/asm-amd64-x86.h>
#endif


#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)

/**
 * The slow case for SUPReadTsc where we need to apply deltas.
 *
 * Must only be called when deltas are applicable, so please do not call it
 * directly.
 *
 * @returns TSC with delta applied.
 *
 * @remarks May be called with interrupts disabled in ring-0!  This is why the
 *          ring-0 code doesn't attempt to figure the delta.
 *
 * @internal
 */
SUPDECL(uint64_t) SUPReadTscWithDelta(void)
{
    PSUPGLOBALINFOPAGE  pGip = g_pSUPGlobalInfoPage;
    uint64_t            uTsc;
    uint16_t            iGipCpu;
    AssertCompile(RT_IS_POWER_OF_TWO(RTCPUSET_MAX_CPUS));
    AssertCompile(RT_ELEMENTS(pGip->aiCpuFromCpuSetIdx) >= RTCPUSET_MAX_CPUS);
    Assert(GIP_ARE_TSC_DELTAS_APPLICABLE(pGip));

    /*
     * Read the TSC and get the corresponding aCPUs index.
     */
    if (pGip->fGetGipCpu & SUPGIPGETCPU_RDTSCP_MASK_MAX_SET_CPUS)
    {
        /* RDTSCP gives us all we need, no loops/cli. */
        uint32_t iCpuSet;
        uTsc      = ASMReadTscWithAux(&iCpuSet);
        iCpuSet  &= RTCPUSET_MAX_CPUS - 1;
        iGipCpu   = pGip->aiCpuFromCpuSetIdx[iCpuSet];
    }
# ifndef IN_RING0
    else if (pGip->fGetGipCpu & SUPGIPGETCPU_IDTR_LIMIT_MASK_MAX_SET_CPUS)
    {
        /* Storing the IDTR is normally very quick, but we need to loop. */
        uint32_t cTries = 0;
        for (;;)
        {
            uint16_t cbLim = ASMGetIdtrLimit();
            uTsc = ASMReadTSC();
            if (RT_LIKELY(ASMGetIdtrLimit() == cbLim))
            {
                uint16_t iCpuSet = cbLim - 256 * (ARCH_BITS == 64 ? 16 : 8);
                iCpuSet &= RTCPUSET_MAX_CPUS - 1;
                iGipCpu  = pGip->aiCpuFromCpuSetIdx[iCpuSet];
                break;
            }
            if (cTries >= 16)
            {
                iGipCpu = UINT16_MAX;
                break;
            }
            cTries++;
        }
    }
# endif /* !IN_RING0 */
    else
    {
# ifdef IN_RING3
        /* Ring-3: Get APIC ID via the slow CPUID instruction, requires looping. */
        uint32_t cTries = 0;
        for (;;)
        {
            uint8_t idApic = ASMGetApicId();
            uTsc = ASMReadTSC();
            if (RT_LIKELY(ASMGetApicId() == idApic))
            {
                iGipCpu = pGip->aiCpuFromApicId[idApic];
                break;
            }
            if (cTries >= 16)
            {
                iGipCpu = UINT16_MAX;
                break;
            }
            cTries++;
        }

# elif defined(IN_RING0)
        /* Ring-0: Use use RTMpCpuId(), no loops. */
        RTCCUINTREG uFlags  = ASMIntDisableFlags();
        int         iCpuSet = RTMpCpuIdToSetIndex(RTMpCpuId());
        if (RT_LIKELY((unsigned)iCpuSet < RT_ELEMENTS(pGip->aiCpuFromCpuSetIdx)))
            iGipCpu = pGip->aiCpuFromCpuSetIdx[iCpuSet];
        else
            iGipCpu = UINT16_MAX;
        uTsc = ASMReadTSC();
        ASMSetFlags(uFlags);

# elif defined(IN_RC)
        /* Raw-mode context: We can get the host CPU set index via VMCPU, no loops. */
        RTCCUINTREG uFlags  = ASMIntDisableFlags(); /* Are already disable, but play safe. */
        uint32_t    iCpuSet = VMMGetCpu(&g_VM)->iHostCpuSet;
        if (RT_LIKELY(iCpuSet < RT_ELEMENTS(pGip->aiCpuFromCpuSetIdx)))
            iGipCpu = pGip->aiCpuFromCpuSetIdx[iCpuSet];
        else
            iGipCpu = UINT16_MAX;
        uTsc = ASMReadTSC();
        ASMSetFlags(uFlags);
# else
#  error "IN_RING3, IN_RC or IN_RING0 must be defined!"
# endif
    }

    /*
     * If the delta is valid, apply it.
     */
    if (RT_LIKELY(iGipCpu < pGip->cCpus))
    {
        int64_t iTscDelta = pGip->aCPUs[iGipCpu].i64TSCDelta;
        if (RT_LIKELY(iTscDelta != INT64_MAX))
            return uTsc + iTscDelta;

# ifdef IN_RING3
        /*
         * The delta needs calculating, call supdrv to get the TSC.
         */
        int rc = SUPR3ReadTsc(&uTsc, NULL);
        if (RT_SUCCESS(rc))
            return uTsc;
        AssertMsgFailed(("SUPR3ReadTsc -> %Rrc\n", rc));
        uTsc = ASMReadTSC();
# endif /* IN_RING3 */
    }

    /*
     * This shouldn't happen, especially not in ring-3 and raw-mode context.
     * But if it does, return something that's half useful.
     */
    AssertMsgFailed(("iGipCpu=%d (%#x) cCpus=%d fGetGipCpu=%#x\n", iGipCpu, iGipCpu, pGip->cCpus, pGip->fGetGipCpu));
    return uTsc;
}

#endif /* RT_ARCH_AMD64 || RT_ARCH_X86 */

