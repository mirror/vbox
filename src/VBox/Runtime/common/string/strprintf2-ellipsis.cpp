/* $Id$ */
/** @file
 * IPRT - String Formatters, alternative, ellipsis.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
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
#include <iprt/string.h>
#include "internal/iprt.h"


RTDECL(ssize_t) RTStrPrintf2(char *pszBuffer, size_t cchBuffer, const char *pszFormat, ...)
{
    va_list va;
    ssize_t cbRet;
    va_start(va, pszFormat);
    cbRet = RTStrPrintf2V(pszBuffer, cchBuffer, pszFormat, va);
    va_end(va);
    return cbRet;
}
RT_EXPORT_SYMBOL(RTStrPrintf2);


RTDECL(ssize_t) RTStrPrintf2Ex(PFNSTRFORMAT pfnFormat, void *pvArg, char *pszBuffer, size_t cchBuffer, const char *pszFormat, ...)
{
    va_list args;
    ssize_t cbRet;
    va_start(args, pszFormat);
    cbRet = RTStrPrintf2ExV(pfnFormat, pvArg, pszBuffer, cchBuffer, pszFormat, args);
    va_end(args);
    return cbRet;
}
RT_EXPORT_SYMBOL(RTStrPrintf2Ex);

