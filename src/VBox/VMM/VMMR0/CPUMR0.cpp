/* $Id$ */
/** @file
 * CPUM - Host Context Ring 0.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_CPUM
#include <VBox/cpum.h>
#include "CPUMInternal.h"
#include <VBox/vm.h>
#include <VBox/x86.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm.h>




/**
 * Does Ring-0 CPUM initialization.
 *
 * This is mainly to check that the Host CPU mode is compatible
 * with VBox.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
CPUMR0DECL(int) CPUMR0Init(PVM pVM)
{
    LogFlow(("CPUMR0Init: %p\n", pVM));

    /*
     * Check CR0 & CR4 flags.
     */
    uint32_t    u32CR0 = ASMGetCR0();
    if ((u32CR0 & (X86_CR0_PE | X86_CR0_PG)) != (X86_CR0_PE | X86_CR0_PG)) /* a bit paranoid perhaps.. */
    {
        Log(("CPUMR0Init: PE or PG not set. cr0=%#x\n", u32CR0));
        return VERR_UNSUPPORTED_CPU_MODE;
    }

    /*
     * Check for sysenter if it's used.
     */
    if (ASMHasCpuId())
    {
        uint32_t u32CpuVersion;
        uint32_t u32Dummy;
        uint32_t u32Features;
        ASMCpuId(1, &u32CpuVersion, &u32Dummy, &u32Dummy, &u32Features);
        uint32_t u32Family   = u32CpuVersion >> 8;
        uint32_t u32Model    = (u32CpuVersion >> 4) & 0xF;
        uint32_t u32Stepping = u32CpuVersion & 0xF;

        /*
         * Intel docs claim you should test both the flag and family, model & stepping.
         * Some Pentium Pro cpus have the SEP cpuid flag set, but don't support it.
         */
        if (    (u32Features & X86_CPUID_FEATURE_EDX_SEP)
            && !(u32Family == 6 && u32Model < 3 && u32Stepping < 3))
        {
            /*
             * Read the MSR and see if it's in use or not.
             */
            uint32_t    u32 = ASMRdMsr_Low(MSR_IA32_SYSENTER_CS);
            if (u32)
            {
                pVM->cpum.s.fUseFlags |= CPUM_USE_SYSENTER;
                Log(("CPUMR0Init: host uses sysenter cs=%08x%08x\n", ASMRdMsr_High(MSR_IA32_SYSENTER_CS), u32));
            }
        }

        /** @todo check for AMD and syscall!!!!!! */
    }


    /*
     * Check if debug registers are armed.
     */
    uint32_t    u32DR7 = ASMGetDR7();
    if (u32DR7 & X86_DR7_ENABLED_MASK)
    {
        pVM->cpum.s.fUseFlags |= CPUM_USE_DEBUG_REGS_HOST;
        Log(("CPUMR0Init: host uses debug registers (dr7=%x)\n", u32DR7));
    }

    return VINF_SUCCESS;
}


