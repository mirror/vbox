/* $Id$ */
/** @file
 * IPRT - No-CRT - qsort().
 */

/*
 * Copyright (C) 2022 Oracle Corporation
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
#define IPRT_NO_CRT_FOR_3RD_PARTY
#include "internal/nocrt.h"
#include <iprt/nocrt/stdlib.h>
#include <iprt/sort.h>


#if !defined(RT_ARCH_AMD64) /* ASSUMING the regular calling convention */ \
 && !defined(RT_ARCH_X86)   /* ASSUMING __cdecl, won't work for __stdcall or __fastcall. */
# define NEED_COMPARE_WRAPPER
/** @callback_method_impl{FNRTSORTCMP} */
static int DECLCALLBACK(int) CompareWrapper(void const *pvElement1, void const *pvElement2, void *pvUser))
{
    int (*pfnCompare)(const void *pv1, const void *pv2) = (int (*)(const void *pv1, const void *pv2))(uintptr_t)pvUser;
    return pfnCompare(pvElement1, pvElement2);
}
#endif

#undef qsort
void RT_NOCRT(qsort)(void *pvBase, size_t cEntries, size_t cbEntry,
                     int (*pfnCompare)(const void *pv1, const void *pv2))
{
    /** @todo Implement and use RTSortQuick! */
#ifdef NEED_COMPARE_WRAPPER
    RTSortShell(pvBase, cEntries, cbEntry, CompareWrapper, (void *)(uintptr_t)pfnCompare);
#else
    RTSortShell(pvBase, cEntries, cbEntry, (PFNRTSORTCMP)pfnCompare, NULL);
#endif
}
RT_ALIAS_AND_EXPORT_NOCRT_SYMBOL(qsort);

