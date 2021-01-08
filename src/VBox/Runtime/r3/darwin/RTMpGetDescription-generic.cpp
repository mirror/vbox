/* $Id$ */
/** @file
 * IPRT - Multiprocessor, RTMpGetDescription for darwin/arm.
 */

/*
 * Copyright (C) 2009-2021 Oracle Corporation
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
#include <iprt/mp.h>
#include "internal/iprt.h"
#include <iprt/err.h>
#include <iprt/string.h>

#include <sys/sysctl.h>


RTDECL(int) RTMpGetDescription(RTCPUID idCpu, char *pszBuf, size_t cbBuf)
{
    /*
     * Check that the specified cpu is valid & online.
     */
    if (idCpu != NIL_RTCPUID && !RTMpIsCpuOnline(idCpu))
        return RTMpIsCpuPossible(idCpu)
             ? VERR_CPU_OFFLINE
             : VERR_CPU_NOT_FOUND;

    /*
     * Just use the sysctl machdep.cpu.brand_string value for now.
     */
    /** @todo on arm this is very boring, so we could use the cpufamily and
     *        cpusubfamily instead... */
    char szBrand[128] = {0};
    size_t cb = sizeof(szBrand);
    int rc = sysctlbyname("machdep.cpu.brand_string", &szBrand, &cb, NULL, 0);
    if (rc == -1)
        szBrand[0] = '\0';

    char *pszStripped = RTStrStrip(szBrand);
    if (*pszStripped)
        return RTStrCopy(pszBuf, cbBuf, pszStripped);
    return RTStrCopy(pszBuf, cbBuf, "Unknown");
}
RT_EXPORT_SYMBOL(RTMpGetDescription);

