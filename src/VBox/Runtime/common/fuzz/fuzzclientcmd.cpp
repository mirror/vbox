/* $Id$ */
/** @file
 * IPRT - Fuzzing framework API, fuzzed client command.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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
#include <iprt/fuzz.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/buildconfig.h>
#include <iprt/getopt.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/types.h>
#include <iprt/vfs.h>


/**
 * Fuzzing client command state.
 */
typedef struct RTFUZZCMDCLIENT
{
    /** Our own fuzzing context containing all the data. */
    RTFUZZCTX               hFuzzCtx;
    /** Consumption callback. */
    PFNFUZZCLIENTCONSUME    pfnConsume;
    /** Opaque user data to pass to the consumption callback. */
    void                    *pvUser;
    /** Standard input VFS handle. */
    RTVFSIOSTREAM           hVfsStdIn;
    /** Standard output VFS handle. */
    RTVFSIOSTREAM           hVfsStdOut;
} RTFUZZCMDCLIENT;
/** Pointer to a fuzzing client command state. */
typedef RTFUZZCMDCLIENT *PRTFUZZCMDCLIENT;


/**
 * The fuzzing client mainloop.
 *
 * @returns IPRT status code.
 * @param   pThis               The fuzzing client command state.
 */
static int rtFuzzCmdClientMainloop(PRTFUZZCMDCLIENT pThis)
{
    int rc = VINF_SUCCESS;
    bool fShutdown = false;

    while (   !fShutdown
           && RT_SUCCESS(rc))
    {
        RTFUZZINPUT hFuzzInput;

        rc = RTFuzzCtxInputGenerate(pThis->hFuzzCtx, &hFuzzInput);
        if (RT_SUCCESS(rc))
        {
            void *pv = NULL;
            size_t cb = 0;
            rc = RTFuzzInputQueryData(hFuzzInput, &pv, &cb);
            if (RT_SUCCESS(rc))
            {
                char bResp = '.';
                int rc2 = pThis->pfnConsume(pv, cb, pThis->pvUser);
                if (RT_SUCCESS(rc2))
                {
                    rc = RTFuzzInputAddToCtxCorpus(hFuzzInput);
                    bResp = 'A';
                }

                if (RT_SUCCESS(rc))
                    rc = RTVfsIoStrmWrite(pThis->hVfsStdOut, &bResp, 1, true /*fBlocking*/, NULL);
            }

            RTFuzzInputRelease(hFuzzInput);
        }
    }

    return rc;
}


/**
 * Run the fuzzing client.
 *
 * @returns Process exit status.
 * @param   pThis               The fuzzing client command state.
 */
static RTEXITCODE rtFuzzCmdClientRun(PRTFUZZCMDCLIENT pThis)
{
    int rc = RTVfsIoStrmFromStdHandle(RTHANDLESTD_INPUT, 0, true /*fLeaveOpen*/, &pThis->hVfsStdIn);
    if (RT_SUCCESS(rc))
    {
        rc = RTVfsIoStrmFromStdHandle(RTHANDLESTD_OUTPUT, 0, true /*fLeaveOpen*/, &pThis->hVfsStdOut);
        if (RT_SUCCESS(rc))
        {
            /* Read the initial input fuzzer state from the standard input. */
            uint32_t cbFuzzCtxState;
            rc = RTVfsIoStrmRead(pThis->hVfsStdIn, &cbFuzzCtxState, sizeof(cbFuzzCtxState), true /*fBlocking*/, NULL);
            if (RT_SUCCESS(rc))
            {
                void *pvFuzzCtxState = RTMemAllocZ(cbFuzzCtxState);
                if (RT_LIKELY(pvFuzzCtxState))
                {
                    rc = RTVfsIoStrmRead(pThis->hVfsStdIn, pvFuzzCtxState, cbFuzzCtxState, true /*fBlocking*/, NULL);
                    if (RT_SUCCESS(rc))
                    {
                        rc = RTFuzzCtxCreateFromState(&pThis->hFuzzCtx, pvFuzzCtxState, cbFuzzCtxState);
                        if (RT_SUCCESS(rc))
                            rc = rtFuzzCmdClientMainloop(pThis);
                    }

                    RTMemFree(pvFuzzCtxState);
                }
                else
                    rc = VERR_NO_MEMORY;
            }
        }
    }

    if (RT_SUCCESS(rc))
        return RTEXITCODE_SUCCESS;

    return RTEXITCODE_FAILURE;
}


RTR3DECL(RTEXITCODE) RTFuzzCmdFuzzingClient(unsigned cArgs, char **papszArgs, PFNFUZZCLIENTCONSUME pfnConsume, void *pvUser)
{
    /*
     * Parse the command line.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--help",                            'h', RTGETOPT_REQ_NOTHING },
        { "--version",                         'V', RTGETOPT_REQ_NOTHING },
    };

    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    RTGETOPTSTATE GetState;
    int rc = RTGetOptInit(&GetState, cArgs, papszArgs, s_aOptions, RT_ELEMENTS(s_aOptions), 1,
                          RTGETOPTINIT_FLAGS_OPTS_FIRST);
    if (RT_SUCCESS(rc))
    {
        /* Option variables:  */
        RTFUZZCMDCLIENT This;

        This.pfnConsume = pfnConsume;
        This.pvUser     = pvUser;

        /* Argument parsing loop. */
        bool fContinue = true;
        do
        {
            RTGETOPTUNION ValueUnion;
            int chOpt = RTGetOpt(&GetState, &ValueUnion);
            switch (chOpt)
            {
                case 0:
                    fContinue = false;
                    break;

                case 'h':
                    RTPrintf("Usage: to be written\nOption dump:\n");
                    for (unsigned i = 0; i < RT_ELEMENTS(s_aOptions); i++)
                        RTPrintf(" -%c,%s\n", s_aOptions[i].iShort, s_aOptions[i].pszLong);
                    fContinue = false;
                    break;

                case 'V':
                    RTPrintf("%sr%d\n", RTBldCfgVersion(), RTBldCfgRevision());
                    fContinue = false;
                    break;

                default:
                    rcExit = RTGetOptPrintError(chOpt, &ValueUnion);
                    fContinue = false;
                    break;
            }
        } while (fContinue);

        if (rcExit == RTEXITCODE_SUCCESS)
            rcExit = rtFuzzCmdClientRun(&This);
    }
    else
        rcExit = RTMsgErrorExit(RTEXITCODE_SYNTAX, "RTGetOptInit: %Rrc", rc);
    return rcExit;
}

