/* $Id$ */
/** @file
 * IPRT - ASN.1, Default Allocator.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
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
#include "internal/iprt.h"
#include <iprt/asn1.h>

#include <iprt/mem.h>
#include <iprt/err.h>
#include <iprt/string.h>


/**
 * Aligns allocation sizes a little.
 *
 * @returns Aligned size.
 * @param   cb                  Requested size.
 */
static size_t rtAsn1DefaultAllocator_AlignSize(size_t cb)
{
    if (cb >= 64)
        return RT_ALIGN_Z(cb, 64);
    if (cb >= 32)
        return RT_ALIGN_Z(cb, 32);
    if (cb >= 16)
        return RT_ALIGN_Z(cb, 16);
    return cb;
}


/** @interface_method_impl{RTASN1ALLOCATORVTABLE, pfnFree} */
static DECLCALLBACK(void) rtAsn1DefaultAllocator_Free(PCRTASN1ALLOCATORVTABLE pThis, PRTASN1ALLOCATION pAllocation, void *pv)
{
    RT_NOREF_PV(pThis);
    RTMemFree(pv);
    pAllocation->cbAllocated = 0;
}


/** @interface_method_impl{RTASN1ALLOCATORVTABLE, pfnAlloc} */
static DECLCALLBACK(int)  rtAsn1DefaultAllocator_Alloc(PCRTASN1ALLOCATORVTABLE pThis, PRTASN1ALLOCATION pAllocation,
                                                       void **ppv, size_t cb)
{
    size_t cbAlloc = rtAsn1DefaultAllocator_AlignSize(cb);
    void *pv = RTMemAllocZ(cbAlloc);
    if (pv)
    {
        *ppv = pv;
        pAllocation->cbAllocated = (uint32_t)cbAlloc;
        return VINF_SUCCESS;
    }
    RT_NOREF_PV(pThis);
    return VERR_NO_MEMORY;
}


/** @interface_method_impl{RTASN1ALLOCATORVTABLE, pfnRealloc} */
static DECLCALLBACK(int)  rtAsn1DefaultAllocator_Realloc(PCRTASN1ALLOCATORVTABLE pThis, PRTASN1ALLOCATION pAllocation,
                                                         void *pvOld, void **ppvNew, size_t cbNew)
{
    Assert(pvOld);
    Assert(cbNew);
    size_t cbAlloc = rtAsn1DefaultAllocator_AlignSize(cbNew);
    void *pv = RTMemRealloc(pvOld, cbAlloc);
    if (pv)
    {
        *ppvNew = pv;
        pAllocation->cbAllocated = (uint32_t)cbAlloc;
        return VINF_SUCCESS;
    }
    RT_NOREF_PV(pThis);
    return VERR_NO_MEMORY;
}


/** The default ASN.1 allocator. */
RT_DECL_DATA_CONST(RTASN1ALLOCATORVTABLE const) g_RTAsn1DefaultAllocator =
{
    rtAsn1DefaultAllocator_Free,
    rtAsn1DefaultAllocator_Alloc,
    rtAsn1DefaultAllocator_Realloc
};

