/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Initialization & Termination, R0 Driver, Darwin.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-darwin-kernel.h"
#include "r0drv/initterm-r0drv.h"
#include <iprt/err.h>
#include <iprt/assert.h>


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

    return VINF_SUCCESS;
}


void rtR0TermNative(void)
{
    /*
     * Free the lock group.
     */
    if (g_pDarwinLockGroup)
    {
        lck_grp_free(g_pDarwinLockGroup);
        g_pDarwinLockGroup = NULL;
    }
}

