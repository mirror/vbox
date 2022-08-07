/* $Id$ */
/** @file
 * IPRT - No-CRT - fputs().
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
#include <iprt/nocrt/stdio.h>
#include <iprt/nocrt/errno.h>
#include <iprt/nocrt/limits.h>
#include <iprt/nocrt/string.h>
#include <iprt/errcore.h>
#include <iprt/stream.h>


#undef fputs
int RT_NOCRT(fputs)(const char *psz, FILE *pFile)
{
    /* We're using RTStrmPutStr here for the benefit of console output optimization. */
    size_t cch = strlen(psz);
    int rc = RTStrmPutStr(pFile, psz);
    if (RT_SUCCESS(rc))
        return (int)RT_MIN(cch, (size_t)INT_MAX);
    errno = RTErrConvertToErrno(rc);
    return RT_NOCRT_EOF;
}
RT_ALIAS_AND_EXPORT_NOCRT_SYMBOL(fputs);

