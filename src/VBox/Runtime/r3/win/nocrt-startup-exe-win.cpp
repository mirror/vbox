/* $Id$ */
/** @file
 * IPRT - No-CRT - Windows EXE startup code.
 *
 * @note Does not run static constructors and destructors!
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

#ifdef IPRT_NO_CRT
# include <iprt/asm.h>
# include <iprt/nocrt/stdlib.h>
#endif

#include "internal/compiler-vcc.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
#ifdef IPRT_NO_CRT
typedef struct RTNOCRTATEXITCHUNK
{
    PFNRTNOCRTATEXITCALLBACK apfnCallbacks[256];
} RTNOCRTATEXITCHUNK;
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
RT_C_DECLS_BEGIN
DECL_HIDDEN_DATA(char)      g_szrtProcExePath[RTPATH_MAX] = "Unknown.exe";
DECL_HIDDEN_DATA(size_t)    g_cchrtProcExePath = 11;
DECL_HIDDEN_DATA(size_t)    g_cchrtProcExeDir = 0;
DECL_HIDDEN_DATA(size_t)    g_offrtProcName = 0;
RT_C_DECLS_END

#ifdef IPRT_NO_CRT
/** The first atexit() registration chunk. */
static RTNOCRTATEXITCHUNK   g_aAtExitPrealloc;
/** Array of atexit() callback chunk pointers. */
static RTNOCRTATEXITCHUNK  *g_apAtExit[8192 / 256] = { &g_aAtExitPrealloc, };
/** Chunk and callback index in one. */
static volatile uint32_t    g_idxNextAtExit        = 0;
#endif


/*********************************************************************************************************************************
*   External Symbols                                                                                                             *
*********************************************************************************************************************************/
extern DECLHIDDEN(void) InitStdHandles(PRTL_USER_PROCESS_PARAMETERS pParams); /* nocrt-streams-win.cpp */ /** @todo put in header */

extern int main(int argc, char **argv, char **envp);    /* in program */


#ifdef IPRT_NO_CRT
extern "C"
int rtnocrt_atexit(PFNRTNOCRTATEXITCALLBACK pfnCallback) RT_NOEXCEPT
{
    AssertPtr(pfnCallback);

    /*
     * Allocate a table index.
     */
    uint32_t idx = ASMAtomicIncU32(&g_idxNextAtExit) - 1;
    AssertReturnStmt(idx < RT_ELEMENTS(g_apAtExit) * RT_ELEMENTS(g_apAtExit[0]->apfnCallbacks),
                     ASMAtomicDecU32(&g_idxNextAtExit), -1);

    /*
     * Make sure the table chunk is there.
     */
    uint32_t            idxChunk = idx / RT_ELEMENTS(g_apAtExit[0]->apfnCallbacks);
    RTNOCRTATEXITCHUNK *pChunk   = ASMAtomicReadPtrT(&g_apAtExit[idxChunk], RTNOCRTATEXITCHUNK *);
    if (!pChunk)
    {
        pChunk = (RTNOCRTATEXITCHUNK *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*pChunk));
        AssertReturn(pChunk, -1); /* don't try decrement, someone could be racing us... */

        if (!ASMAtomicCmpXchgPtr(&g_apAtExit[idxChunk], pChunk, NULL))
        {
            HeapFree(GetProcessHeap(), 0, pChunk);

            pChunk = ASMAtomicReadPtrT(&g_apAtExit[idxChunk], RTNOCRTATEXITCHUNK *);
            Assert(pChunk);
        }
    }

    /*
     * Add our callback.
     */
    pChunk->apfnCallbacks[idxChunk % RT_ELEMENTS(pChunk->apfnCallbacks)] = pfnCallback;
    return 0;
}
#endif


static int rtTerminateProcess(int32_t rcExit, bool fDoAtExit)
{
#ifdef IPRT_NO_CRT
    /*
     * Run atexit callback in reverse order.
     */
    if (fDoAtExit)
    {
        uint32_t idxAtExit = ASMAtomicReadU32(&g_idxNextAtExit);
        if (idxAtExit-- > 0)
        {
            uint32_t idxChunk    = idxAtExit / RT_ELEMENTS(g_apAtExit[0]->apfnCallbacks);
            uint32_t idxCallback = idxAtExit % RT_ELEMENTS(g_apAtExit[0]->apfnCallbacks);
            for (;;)
            {
                RTNOCRTATEXITCHUNK *pChunk = ASMAtomicReadPtrT(&g_apAtExit[idxChunk], RTNOCRTATEXITCHUNK *);
                if (pChunk)
                {
                    do
                    {
                        PFNRTNOCRTATEXITCALLBACK pfnCallback = pChunk->apfnCallbacks[idxCallback];
                        if (pfnCallback) /* Can be NULL see registration code */
                            pfnCallback();
                    } while (idxCallback-- > 0);
                }
                if (idxChunk == 0)
                    break;
                idxChunk--;
                idxCallback = RT_ELEMENTS(g_apAtExit[0]->apfnCallbacks) - 1;
            }
        }

        rtVccInitializersRunTerm();
    }
#else
    RT_NOREF(fDoAtExit);
#endif

    /*
     * Terminate.
     */
    for (;;)
        NtTerminateProcess(NtCurrentProcess(), rcExit);
}


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
            RTMsgError("initProcExecPath: RTUtf16ToUtf8Ex failed: %Rrc\n", rc);
    }
    else
        RTMsgError("initProcExecPath: GetModuleFileNameW failed: %Rhrc\n", GetLastError());
}


DECLASM(void) CustomMainEntrypoint(PPEB pPeb)
{
    /*
     * Initialize stuff.
     */
    InitStdHandles(pPeb->ProcessParameters);
    initProcExecPath();

    RTEXITCODE rcExit;
#ifdef IPRT_NO_CRT
    AssertCompile(sizeof(rcExit) == sizeof(int));
    rcExit = (RTEXITCODE)rtVccInitializersRunInit();
    if (rcExit == RTEXITCODE_SUCCESS)
#endif
    {
        /*
         * Get and convert the command line to argc/argv format.
         */
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
                    AssertCompile(sizeof(rcExit) == sizeof(int));
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
        rtTerminateProcess(rcExit, true /*fDoAtExit*/);
    }
#ifdef IPRT_NO_CRT
    else
    {
        RTMsgError("A C static initializor failed (%d)\n", rcExit);
        rtTerminateProcess(rcExit, false /*fDoAtExit*/);
    }
#endif
}

