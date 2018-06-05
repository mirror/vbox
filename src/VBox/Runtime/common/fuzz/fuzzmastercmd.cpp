/* $Id$ */
/** @file
 * IPRT - Fuzzing framework API, master command.
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

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/buildconfig.h>
#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/getopt.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/stream.h>
#include <iprt/thread.h>



/**
 * Wrapper around RTErrInfoSetV / RTMsgErrorV.
 *
 * @returns @a rc
 * @param   pErrInfo            Extended error info.
 * @param   rc                  The return code.
 * @param   pszFormat           The message format.
 * @param   ...                 The message format arguments.
 */
static int rtFuzzCmdMasterErrorRc(PRTERRINFO pErrInfo, int rc, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    if (pErrInfo)
        RTErrInfoSetV(pErrInfo, rc, pszFormat, va);
    else
        RTMsgErrorV(pszFormat, va);
    va_end(va);
    return rc;
}


/**
 * Executes the
 */
static RTEXITCODE rtFuzzCmdMasterDoIt(const char *pszBinary, uint32_t cProcs, const char *pszInpSeedDir,
                                      size_t cbInputMax, const char * const *papszClientArgs, unsigned cClientArgs,
                                      bool fInputFile, const char *pszTmpDir)
{
    RTFUZZOBS hFuzzObs;

    int rc = RTFuzzObsCreate(&hFuzzObs);
    if (RT_SUCCESS(rc))
    {
        RTFUZZCTX hFuzzCtx;

        rc = RTFuzzObsQueryCtx(hFuzzObs, &hFuzzCtx);
        if (RT_SUCCESS(rc))
        {
            uint32_t fFlags = 0;

            if (fInputFile)
            {
                fFlags |= RTFUZZ_OBS_BINARY_F_INPUT_FILE;
                rc = RTFuzzObsSetTmpDirectory(hFuzzObs, pszTmpDir);
            }

            if (RT_SUCCESS(rc))
                rc = RTFuzzObsSetTestBinary(hFuzzObs, pszBinary, fFlags);
            if (RT_SUCCESS(rc))
            {
                rc = RTFuzzObsSetTestBinaryArgs(hFuzzObs, papszClientArgs, cClientArgs);
                if (RT_SUCCESS(rc))
                {
                    rc = RTFuzzCtxCfgSetInputSeedMaximum(hFuzzCtx, cbInputMax);
                    if (RT_SUCCESS(rc))
                    {
                        rc = RTFuzzCtxCorpusInputAddFromDirPath(hFuzzCtx, pszInpSeedDir);
                        if (RT_SUCCESS(rc))
                        {
                            rc = RTFuzzObsExecStart(hFuzzObs, cProcs);
                            if (RT_SUCCESS(rc))
                            {
                                RTThreadSleep(3600 * RT_MS_1SEC);
                                RTFuzzObsExecStop(hFuzzObs);
                            }
                            else
                                rc = rtFuzzCmdMasterErrorRc(NULL, rc, "Failed to start fuzzing observer: %Rrc\n", rc);
                        }
                        else
                            rc = rtFuzzCmdMasterErrorRc(NULL, rc, "Failed to load corpus seeds from \"%s\": %Rrc\n", pszInpSeedDir, rc);
                    }
                    else
                        rc = rtFuzzCmdMasterErrorRc(NULL, rc, "Failed to set maximum input size to %zu: %Rrc\n", cbInputMax, rc);
                }
                else
                    rc = rtFuzzCmdMasterErrorRc(NULL, rc, "Failed to set test program arguments: %Rrc\n", rc);
            }
            else
                rc = rtFuzzCmdMasterErrorRc(NULL, rc, "Failed to set the specified binary as test program: %Rrc\n", rc);

            RTFuzzCtxRelease(hFuzzCtx);
        }
        else
            rc = rtFuzzCmdMasterErrorRc(NULL, rc, "Error obtaining the fuzzing context from the observer: %Rrc\n", rc);

        RTFuzzObsDestroy(hFuzzObs);
    }
    else
        rc = rtFuzzCmdMasterErrorRc(NULL, rc, "Error creating observer instance: %Rrc\n", rc);

    return RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


RTR3DECL(RTEXITCODE) RTFuzzCmdMaster(unsigned cArgs, char **papszArgs)
{
    /*
     * Parse the command line.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--binary",                          'b', RTGETOPT_REQ_STRING  },
        { "--processes",                       'p', RTGETOPT_REQ_UINT32  },
        { "--input-as-file",                   'f', RTGETOPT_REQ_STRING  },
        { "--input-seed-file",                 'i', RTGETOPT_REQ_STRING  },
        { "--input-seed-dir",                  's', RTGETOPT_REQ_STRING  },
        { "--input-seed-size-max",             'm', RTGETOPT_REQ_UINT32  },
        { "--args",                            'a', RTGETOPT_REQ_STRING  },
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
        const char *pszBinary = NULL;
        uint32_t    cProcs = 0;
        const char *pszInpSeedDir = NULL;
        int         cClientArgs = 0;
        size_t      cbInputMax = 0;
        char      **papszClientArgs = NULL;
        bool        fInputFile = false;
        const char *pszTmpDir = NULL;

        /* Argument parsing loop. */
        bool fContinue = true;
        do
        {
            RTGETOPTUNION ValueUnion;
            int chOpt = RTGetOpt(&GetState, &ValueUnion);
            switch (chOpt)
            {
                case 0:
                    rcExit = rtFuzzCmdMasterDoIt(pszBinary, cProcs, pszInpSeedDir, cbInputMax,
                                                 papszClientArgs, cClientArgs, fInputFile, pszTmpDir);
                    fContinue = false;
                    break;

                case 'b':
                    pszBinary = ValueUnion.psz;
                    break;

                case 'p':
                    cProcs = ValueUnion.u32;
                    break;

                case 'f':
                    pszTmpDir = ValueUnion.psz;
                    fInputFile = true;
                    break;

                case 'a':
                    rc = RTGetOptArgvFromString(&papszClientArgs, &cClientArgs, ValueUnion.psz, 0, NULL);
                    break;

                case 's':
                    pszInpSeedDir = ValueUnion.psz;
                    break;

                case 'm':
                    cbInputMax = ValueUnion.u32;
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
    }
    else
        rcExit = RTMsgErrorExit(RTEXITCODE_SYNTAX, "RTGetOptInit: %Rrc", rc);
    return rcExit;
}

