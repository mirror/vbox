/* $Id$ */
/** @file
 * IPRT - Utility for manipulating EFI signature databases.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
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
#include <iprt/assert.h>
#include <iprt/buildconfig.h>
#include <iprt/errcore.h>
#include <iprt/efi.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/vfs.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

/**
 * Display the version of the cache program.
 *
 * @returns exit code.
 */
static RTEXITCODE rtEfiSigDbVersion(void)
{
    RTPrintf("%sr%d\n", RTBldCfgVersion(), RTBldCfgRevision());
    return RTEXITCODE_SUCCESS;
}


/**
 * Shows the usage of the program.
 *
 * @returns Exit code.
 * @param   pszArg0             Program name.
 * @param   pszCommand          Command selector, NULL if all.
 */
static RTEXITCODE rtEfiSigDbUsage(const char *pszArg0, const char *pszCommand)
{
    if (!pszCommand || !strcmp(pszCommand, "list"))
        RTPrintf("Usage: %s list <signature database path>\n"
                 , RTPathFilename(pszArg0));

    return RTEXITCODE_SUCCESS;
}


/**
 * Signature database enumeration callback.
 */
static DECLCALLBACK(int) rtEfiSgDbEnum(RTEFISIGDB hEfiSigDb, RTEFISIGTYPE enmSigType, PCRTUUID pUuidOwner,
                                       const void *pvSig, size_t cbSig, void *pvUser)
{
    RT_NOREF(hEfiSigDb, pvUser);

    uint32_t *pidxSig = (uint32_t *)pvUser;

    RTPrintf("%02u: %s\n", (*pidxSig)++, RTEfiSigDbTypeStringify(enmSigType));
    RTPrintf("    Owner: %RTuuid\n", pUuidOwner);
    RTPrintf("    Signature:\n"
             "%.*Rhxd\n\n", cbSig, pvSig);
    return VINF_SUCCESS;
}


/**
 * Handles the 'list' command.
 *
 * @returns Program exit code.
 * @param   pszArg0             The program name.
 * @param   cArgs               The number of arguments to the 'add' command.
 * @param   papszArgs           The argument vector, starting after 'add'.
 */
static RTEXITCODE rtEfiSgDbCmdList(const char *pszArg0, int cArgs, char **papszArgs)
{
    RT_NOREF(pszArg0);

    /*
     * Parse arguments.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--input",    'i', RTGETOPT_REQ_STRING }
    };

    int             rc       = VINF_SUCCESS;
    RTEXITCODE      rcExit   = RTEXITCODE_SUCCESS;
    const char     *pszInput = NULL;

    RTGETOPTUNION   ValueUnion;
    RTGETOPTSTATE   GetState;
    RTGetOptInit(&GetState, cArgs, papszArgs, s_aOptions, RT_ELEMENTS(s_aOptions), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    while ((rc = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (rc)
        {
            case 'i':
                pszInput = ValueUnion.psz;
                break;
            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    if (!pszInput)
    {
        RTPrintf("An input path must be given\n");
        return RTEXITCODE_FAILURE;
    }

    RTVFSFILE hVfsFile = NIL_RTVFSFILE;
    if (RTVfsChainIsSpec(pszInput))
    {
        RTVFSOBJ hVfsObj;
        rc = RTVfsChainOpenObj(pszInput, RTFILE_O_READ | RTFILE_O_DENY_NONE | RTFILE_O_OPEN,
                               RTVFSOBJ_F_OPEN_ANY | RTVFSOBJ_F_CREATE_NOTHING | RTPATH_F_ON_LINK,
                               &hVfsObj, NULL, NULL);
        if (   RT_SUCCESS(rc)
            && RTVfsObjGetType(hVfsObj) == RTVFSOBJTYPE_FILE)
        {
            hVfsFile = RTVfsObjToFile(hVfsObj);
            RTVfsObjRelease(hVfsObj);
        }
        else
        {
            RTPrintf("'%s' doesn't point to a file\n", pszInput);
            return RTEXITCODE_FAILURE;
        }
    }
    else
        rc = RTVfsFileOpenNormal(pszInput, RTFILE_O_READ | RTFILE_O_DENY_NONE | RTFILE_O_OPEN,
                                 &hVfsFile);

    RTEFISIGDB hEfiSigDb;
    rc = RTEfiSigDbCreate(&hEfiSigDb);
    if (RT_SUCCESS(rc))
    {
        uint32_t idxSig = 0;

        rc = RTEfiSigDbAddFromExistingDb(hEfiSigDb, hVfsFile);
        if (RT_SUCCESS(rc))
            RTEfiSigDbEnum(hEfiSigDb, rtEfiSgDbEnum, &idxSig);

        RTEfiSigDbDestroy(hEfiSigDb);
    }

    RTVfsFileRelease(hVfsFile);
    return rcExit;
}


int main(int argc, char **argv)
{
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    /*
     * Switch on the command.
     */
    RTEXITCODE rcExit = RTEXITCODE_SYNTAX;
    if (argc < 2)
        rtEfiSigDbUsage(argv[0], NULL);
    else if (!strcmp(argv[1], "list"))
        rcExit = rtEfiSgDbCmdList(argv[0], argc - 2, argv + 2);
    else if (   !strcmp(argv[1], "-h")
             || !strcmp(argv[1], "-?")
             || !strcmp(argv[1], "--help"))
        rcExit = rtEfiSigDbUsage(argv[0], NULL);
    else if (   !strcmp(argv[1], "-V")
             || !strcmp(argv[1], "--version"))
        rcExit = rtEfiSigDbVersion();
    else
        RTMsgError("Unknown command: '%s'", argv[1]);

    return rcExit;
}

