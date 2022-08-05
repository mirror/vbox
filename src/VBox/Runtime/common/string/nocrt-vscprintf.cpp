/* $Id$ */
/** @file
 * IPRT - No-CRT Strings, vscprintf().
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
#include "internal/iprt.h"
#include <iprt/nocrt/stdio.h>
#include <iprt/nocrt/limits.h>
#include <iprt/string.h>


/**
 * @callback_method_impl{FNRTSTROUTPUT, Dummy that only return cbChars.}
 */
static DECLCALLBACK(size_t) rtStrDummyOutputter(void *pvArg, const char *pachChars, size_t cbChars)
{
    RT_NOREF(pvArg, pachChars);
    return cbChars;
}


#undef vscprintf
int RT_NOCRT(vscprintf)(const char *pszFormat, va_list va)
{
    size_t cchOutput = RTStrFormatV(rtStrDummyOutputter, NULL, NULL, NULL, pszFormat, va);
    return cchOutput < (size_t)INT_MAX ? (int)cchOutput : INT_MAX - 1;
}
RT_ALIAS_AND_EXPORT_NOCRT_SYMBOL(vscprintf);

