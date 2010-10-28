/* $Id$ */
/** @file
 * IPRT - Initialization & Termination, R0 Driver, Solaris.
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
#include "the-solaris-kernel.h"
#include "internal/iprt.h"

#include <iprt/err.h>
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
# include <iprt/asm-amd64-x86.h>
#endif
#include "internal/initterm.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Indicates that the spl routines (and therefore a bunch of other ones too)
 * will set EFLAGS::IF and break code that disables interrupts.  */
bool g_frtSolarisSplSetsEIF = false;

/** timeout_generic address. */
PFNSOL_timeout_generic      g_pfnrtR0Sol_timeout_generic   = NULL;
/** untimeout_generic address. */
PFNSOL_untimeout_generic    g_pfnrtR0Sol_untimeout_generic = NULL;
/** cyclic_reprogram address. */
PFNSOL_cyclic_reprogram     g_pfnrtR0Sol_cyclic_reprogram  = NULL;


int rtR0InitNative(void)
{
    /*
     * Initialize vbi (keeping it separate for now)
     */
    int rc = vbi_init();
    if (!rc)
    {
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
        /*
         * Detect whether spl*() is preserving the interrupt flag or not.
         * This is a problem on S10.
         */
        RTCCUINTREG uOldFlags = ASMIntDisableFlags();
        int iOld = splr(DISP_LEVEL);
        if (ASMIntAreEnabled())
            g_frtSolarisSplSetsEIF = true;
        splx(iOld);
        if (ASMIntAreEnabled())
            g_frtSolarisSplSetsEIF = true;
        ASMSetFlags(uOldFlags);
#else
        /* PORTME: See if the amd64/x86 problem applies to this architecture. */
#endif

        /*
         * Dynamically resolve new symbols we want to use.
         */
        g_pfnrtR0Sol_timeout_generic    = (PFNSOL_timeout_generic  )kobj_getsymvalue("timeout_generic",   1);
        g_pfnrtR0Sol_untimeout_generic  = (PFNSOL_untimeout_generic)kobj_getsymvalue("untimeout_generic", 1);
        if ((g_pfnrtR0Sol_timeout_generic == NULL) != (g_pfnrtR0Sol_untimeout_generic == NULL))
        {
            static const char *s_apszFn[2] = { "timeout_generic", "untimeout_generic" };
            bool iMissingFn = g_pfnrtR0Sol_timeout_generic == NULL;
            cmn_err(CE_NOTE, "rtR0InitNative: Weird! Found %s but not %s!\n", s_apszFn[!iMissingFn], s_apszFn[iMissingFn]);
            g_pfnrtR0Sol_timeout_generic   = NULL;
            g_pfnrtR0Sol_untimeout_generic = NULL;
        }

        g_pfnrtR0Sol_cyclic_reprogram   = (PFNSOL_cyclic_reprogram )kobj_getsymvalue("cyclic_reprogram",  1);


        return VINF_SUCCESS;
    }
    cmn_err(CE_NOTE, "vbi_init failed. rc=%d\n", rc);
    return VERR_GENERAL_FAILURE;
}


void rtR0TermNative(void)
{
}

