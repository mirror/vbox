/* $Id$ */
/** @file
 * IPRT - Error reporting to standard error.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
#include "internal/iprt.h"
#include <iprt/message.h>

#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include "internal/process.h"


RTDECL(int)  RTMsgError(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    int rc = RTMsgErrorV(pszFormat, va);
    va_end(va);
    return rc;
}


RTDECL(int)  RTMsgErrorV(const char *pszFormat, va_list va)
{
    if (   !*pszFormat
        || !strcmp(pszFormat, "\n"))
        RTStrmPrintf(g_pStdErr, "\n");
    else
    {
        char *pszMsg;
        ssize_t cch = RTStrAPrintfV(&pszMsg, pszFormat, va);
        if (cch >= 0)
        {
            /* print it line by line. */
            char *psz = pszMsg;
            do
            {
                char *pszEnd = strchr(psz, '\n');
                if (!pszEnd)
                {
                    RTStrmPrintf(g_pStdErr, "%s: error: %s\n", &g_szrtProcExePath[g_offrtProcName], psz);
                    break;
                }
                if (pszEnd == psz)
                    RTStrmPrintf(g_pStdErr, "\n");
                else
                {
                    *pszEnd = '\0';
                    RTStrmPrintf(g_pStdErr, "%s: error: %s\n", &g_szrtProcExePath[g_offrtProcName], psz);
                }
                psz = pszEnd + 1;
            } while (*psz);
            RTStrFree(pszMsg);
        }
        else
        {
            /* Simple fallback for handling out-of-memory conditions. */
            RTStrmPrintf(g_pStdErr, "%s: error: ", &g_szrtProcExePath[g_offrtProcName]);
            RTStrmPrintfV(g_pStdErr, pszFormat, va);
            if (!strchr(pszFormat, '\n'))
                RTStrmPrintf(g_pStdErr, "\n");
        }
    }

    return VINF_SUCCESS;
}

