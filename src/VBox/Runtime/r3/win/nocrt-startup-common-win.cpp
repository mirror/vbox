/* $Id$ */
/** @file
 * IPRT - No-CRT - Common Windows startup code.
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
#include "internal/iprt.h"
#include "internal/process.h"

#include <iprt/nt/nt-and-windows.h>
#include <iprt/getopt.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/utf16.h>

#include "internal/compiler-vcc.h"


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
RT_C_DECLS_BEGIN
DECL_HIDDEN_DATA(char)      g_szrtProcExePath[RTPATH_MAX] = "Unknown.exe";
DECL_HIDDEN_DATA(size_t)    g_cchrtProcExePath = 11;
DECL_HIDDEN_DATA(size_t)    g_cchrtProcExeDir = 0;
DECL_HIDDEN_DATA(size_t)    g_offrtProcName = 0;
RT_C_DECLS_END



void rtVccWinInitProcExecPath(void)
{
    WCHAR wszPath[RTPATH_MAX];
    UINT cwcPath = GetModuleFileNameW(NULL, wszPath, RT_ELEMENTS(wszPath));
    if (cwcPath)
    {
        char *pszDst = g_szrtProcExePath;
        int rc = RTUtf16ToUtf8Ex(wszPath, cwcPath, &pszDst, sizeof(g_szrtProcExePath), &g_cchrtProcExePath);
        if (RT_SUCCESS(rc))
        {
            g_cchrtProcExeDir = g_offrtProcName = RTPathFilename(pszDst) - g_szrtProcExePath;
            while (   g_cchrtProcExeDir >= 2
                   && RTPATH_IS_SLASH(g_szrtProcExePath[g_cchrtProcExeDir - 1])
                   && g_szrtProcExePath[g_cchrtProcExeDir - 2] != ':')
                g_cchrtProcExeDir--;
        }
        else
            RTMsgError("initProcExecPath: RTUtf16ToUtf8Ex failed: %Rrc\n", rc);
    }
    else
        RTMsgError("initProcExecPath: GetModuleFileNameW failed: %Rhrc\n", GetLastError());
}

