/* $Id$ */
/** @file
 * IPRT - Initialization & Termination, R0 Driver, Darwin.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-darwin-kernel.h"
#include <iprt/err.h>
#include <iprt/assert.h>
#include "internal/initterm.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Pointer to the lock group used by IPRT. */
lck_grp_t *g_pDarwinLockGroup = NULL;


int rtR0InitNative(void)
{
    /*
     * Create the lock group.
     */
    g_pDarwinLockGroup = lck_grp_alloc_init("IPRT", LCK_GRP_ATTR_NULL);
    AssertReturn(g_pDarwinLockGroup, VERR_NO_MEMORY);

    /*
     * Initialize the preemption hacks.
     */
    int rc = rtThreadPreemptDarwinInit();
    if (RT_FAILURE(rc))
        rtR0TermNative();

    return rc;
}


void rtR0TermNative(void)
{
    /*
     * Preemption hacks before the lock group.
     */
    rtThreadPreemptDarwinTerm();

    /*
     * Free the lock group.
     */
    if (g_pDarwinLockGroup)
    {
        lck_grp_free(g_pDarwinLockGroup);
        g_pDarwinLockGroup = NULL;
    }
}

