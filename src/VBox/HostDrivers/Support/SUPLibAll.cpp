/* $Id$ */
/** @file
 * VirtualBox Support Library - All Contexts Code.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
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
    PSUPGLOBALINFOPAGE pGip = g_pSUPGlobalInfoPage;
    Assert(GIP_ARE_TSC_DELTAS_APPLICABLE(pGip));

    /** @todo Check out the rdtscp optimization, ASMGetApicId is very expensive. */

# ifdef IN_RING3
    /*
     * Read the TSC and delta.
     */
    uint32_t cTries = 0;
    for (;;)
    {
        uint8_t  idApic    = ASMGetApicId();
        uint64_t uTsc      = ASMReadTSC();
        int64_t  iTscDelta = pGip->aCPUs[pGip->aiCpuFromApicId[idApic]].i64TSCDelta;
        if (RT_LIKELY(   idApic == ASMGetApicId()
                      || cTries >= 10))
        {
            /*
             * If the delta is valid, apply it.
             */
            if (RT_LIKELY(iTscDelta != INT64_MAX))
                return uTsc - iTscDelta;

            /*
             * The delta needs calculating, call supdrv to get the TSC.
             */
            int rc = SUPR3ReadTsc(&uTsc, NULL);
            if (RT_SUCCESS(rc))
                return uTsc;
            AssertMsgFailed(("SUPR3ReadTsc -> %Rrc\n", rc));

            /* That didn't work, just return something half useful... */
            return ASMReadTSC();
        }
        cTries++;
    }
# else
    /*
     * Read the TSC and delta.
     */
    RTCCUINTREG uFlags    = ASMIntDisableFlags();
#  ifdef IN_RING0
    int         iCpuSet   = RTMpCpuIdToSetIndex(RTMpCpuId());
    uint16_t    iGipCpu   = (unsigned)iCpuSet < RT_ELEMENTS(pGip->aiCpuFromCpuSetIdx)
                          ? pGip->aiCpuFromCpuSetIdx[iCpuSet] : UINT16_MAX;
#  else
    uint16_t    idApic    = ASMGetApicId();  /** @todo this could probably be eliminated in RC if we really wanted to... */
    uint16_t    iGipCpu   = (unsigned)idApic < RT_ELEMENTS(pGip->aiCpuFromApicId) /* for the future */
                          ? pGip->aiCpuFromApicId[idApic] : UINT16_MAX;
#  endif
    int64_t     iTscDelta = (unsigned)iGipCpu < pGip->cCpus ? pGip->aCPUs[iGipCpu].i64TSCDelta : INT64_MAX;
    uint64_t    uTsc      = ASMReadTSC();
    ASMSetFlags(uFlags);

    /*
     * If the delta is valid, apply it, otherwise ignore it (really shouldn't
     * happen in these contexts!).
     */
    if (RT_LIKELY(iTscDelta != INT64_MAX))
        return uTsc - iTscDelta;
#  ifdef IN_RING0
    AssertMsgFailed(("iCpuSet=%d (%#x) iGipCpu=%#x\n", iCpuSet, iCpuSet, iGipCpu));
#  else
    AssertMsgFailed(("idApic=%#x iGipCpu=%#x\n", idApic, iGipCpu));
#  endif
    return uTsc;
# endif
}

#endif /* RT_ARCH_AMD64 || RT_ARCH_X86 */

