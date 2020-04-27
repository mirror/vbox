/* $Id$ */
/** @file
 * IPRT - No-CRT windows EXE startup code.
 *
 * @note Does not run static constructors and destructors!
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
RT_C_DECLS_BEGIN
DECLHIDDEN(char)             g_szrtProcExePath[RTPATH_MAX] = "Unknown.exe";
DECLHIDDEN(size_t)           g_cchrtProcExePath = 11;
DECLHIDDEN(size_t)           g_cchrtProcExeDir = 0;
DECLHIDDEN(size_t)           g_offrtProcName = 0;
RT_C_DECLS_END


/*********************************************************************************************************************************
*   External Symbols                                                                                                             *
*********************************************************************************************************************************/
extern DECLHIDDEN(void) InitStdHandles(PRTL_USER_PROCESS_PARAMETERS pParams); /* nocrt-streams-win.cpp */ /** @todo put in header */

extern int main(int argc, char **argv, char **envp);    /* in program */



DECL_NO_INLINE(static, void) initProcExecPath(void)
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
            RTMsgErrorExitFailure("initProcExecPath: RTUtf16ToUtf8Ex failed: %Rrc\n", rc);
    }
    else
        RTMsgErrorExitFailure("initProcExecPath: GetModuleFileNameW failed: %Rhrc\n", GetLastError());
}


void CustomMainEntrypoint(PPEB pPeb)
{
    /*
     * Initialize stuff.
     */
    InitStdHandles(pPeb->ProcessParameters);
    initProcExecPath();

    /*
     * Get and convert the command line to argc/argv format.
     */
    RTEXITCODE rcExit;
    UNICODE_STRING const *pCmdLine = pPeb->ProcessParameters ? &pPeb->ProcessParameters->CommandLine : NULL;
    if (pCmdLine)
    {
        char *pszCmdLine = NULL;
        int rc = RTUtf16ToUtf8Ex(pCmdLine->Buffer, pCmdLine->Length / sizeof(WCHAR), &pszCmdLine, 0, NULL);
        if (RT_SUCCESS(rc))
        {
            char **papszArgv;
            int    cArgs = 0;
            rc = RTGetOptArgvFromString(&papszArgv, &cArgs, pszCmdLine,
                                        RTGETOPTARGV_CNV_MODIFY_INPUT | RTGETOPTARGV_CNV_QUOTE_MS_CRT,  NULL);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Call the main function.
                 */
                rcExit = (RTEXITCODE)main(cArgs, papszArgv, NULL /*envp*/);
            }
            else
                rcExit = RTMsgErrorExitFailure("Error parsing command line: %Rrc\n", rc);
        }
        else
            rcExit = RTMsgErrorExitFailure("Failed to convert command line to UTF-8: %Rrc\n", rc);
    }
    else
        rcExit = RTMsgErrorExitFailure("No command line\n");

    for (;;)
        NtTerminateProcess(NtCurrentProcess(), rcExit);
}

