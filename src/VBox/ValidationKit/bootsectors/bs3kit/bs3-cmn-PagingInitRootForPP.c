/* $Id$ */
/** @file
 * BS3Kit - Bs3PagingInitRootForPP
 */

/*
 * Copyright (C) 2007-2015 Oracle Corporation
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

/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "bs3kit-template-header.h"
#include "bs3-cmn-paging.h"


BS3_DECL(int) Bs3PagingInitRootForPP(void)
{
    X86PD BS3_FAR *pPgDir;

    BS3_ASSERT(g_PhysPagingRootPP == UINT32_MAX);


    /*
     * By default we do a identity mapping of the entire address space
     * using 4 GB pages.  So, we only really need one page directory,
     * that's all.
     *
     * ASSUMES page size extension available, i.e. pentium+.
     */
    pPgDir = (X86PD BS3_FAR *)Bs3MemAlloc(BS3MEMKIND_TILED, _4K);
    if (pPgDir)
    {
        BS3_XPTR_AUTO(X86PD, XptrPgDir);
        unsigned i;

        if (g_uBs3CpuDetected)
        {
        }
        for (i = 0; i < RT_ELEMENTS(pPgDir->a); i++)
        {
            pPgDir->a[i].u = (uint32_t)i << X86_PD_SHIFT;
            pPgDir->a[i].u |= X86_PDE4M_P | X86_PDE4M_RW | X86_PDE4M_US | X86_PDE4M_PS | X86_PDE4M_A | X86_PDE4M_D;
        }

        BS3_XPTR_SET(X86PD, XptrPgDir, pPgDir);
        g_PhysPagingRootPP = BS3_XPTR_GET_FLAT(X86PD, XptrPgDir);
        return VINF_SUCCESS;
    }

    BS3_ASSERT(false);
    return VERR_NO_MEMORY;
}

