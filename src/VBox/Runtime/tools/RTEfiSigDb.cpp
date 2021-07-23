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
#include <iprt/uuid.h>
#include <iprt/vfs.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Signature type identifier to internal type mapping. */
struct
{
    const char   *pszId;
    RTEFISIGTYPE enmType;
} g_aId2SigType[] =
{
    { "sha256",  RTEFISIGTYPE_SHA256  },
    { "rsa2048", RTEFISIGTYPE_RSA2048 },
    { "x509",    RTEFISIGTYPE_X509    }
};


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

    if (!pszCommand || !strcmp(pszCommand, "add"))
        RTPrintf("Usage: %s add <signature database path> <x509|sha256|rsa2048> <owner uuid> <signature path> ...\n"
                 , RTPathFilename(pszArg0));

    return RTEXITCODE_SUCCESS;
}


static RTEFISIGTYPE rtEfiSigDbGetTypeById(const char *pszId)
{
    for (uint32_t i = 0; i < RT_ELEMENTS(g_aId2SigType); i++)
        if (!strcmp(pszId, g_aId2SigType[i].pszId))
            return g_aId2SigType[i].enmType;

    return RTEFISIGTYPE_INVALID;
}


/**
 * Opens the specified signature database, returning an VFS file handle on success.
 *
 * @returns IPRT status code.
 * @param   pszPath             Path to the signature database.
 * @param   phVfsFile           Where to return the VFS file handle on success.
 */
static int rtEfiSigDbOpen(const char *pszPath, PRTVFSFILE phVfsFile)
{
    int rc;

    if (RTVfsChainIsSpec(pszPath))
    {
        RTVFSOBJ hVfsObj;
        rc = RTVfsChainOpenObj(pszPath, RTFILE_O_READWRITE | RTFILE_O_DENY_NONE | RTFILE_O_OPEN,
                               RTVFSOBJ_F_OPEN_ANY | RTVFSOBJ_F_CREATE_NOTHING | RTPATH_F_ON_LINK,
                               &hVfsObj, NULL, NULL);
        if (   RT_SUCCESS(rc)
            && RTVfsObjGetType(hVfsObj) == RTVFSOBJTYPE_FILE)
        {
            *phVfsFile = RTVfsObjToFile(hVfsObj);
            RTVfsObjRelease(hVfsObj);
        }
        else
        {
            RTPrintf("'%s' doesn't point to a file\n", pszPath);
            rc = VERR_INVALID_PARAMETER;
        }
    }
    else
        rc = RTVfsFileOpenNormal(pszPath, RTFILE_O_READWRITE | RTFILE_O_DENY_NONE | RTFILE_O_OPEN,
                                 phVfsFile);

    return rc;
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

    if (!cArgs)
    {
        RTPrintf("An input path must be given\n");
        return RTEXITCODE_FAILURE;
    }

    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    RTVFSFILE hVfsFile = NIL_RTVFSFILE;
    int rc = rtEfiSigDbOpen(papszArgs[0], &hVfsFile);
    if (RT_SUCCESS(rc))
    {
        RTEFISIGDB hEfiSigDb;
        rc = RTEfiSigDbCreate(&hEfiSigDb);
        if (RT_SUCCESS(rc))
        {
            uint32_t idxSig = 0;

            rc = RTEfiSigDbAddFromExistingDb(hEfiSigDb, hVfsFile);
            if (RT_SUCCESS(rc))
                RTEfiSigDbEnum(hEfiSigDb, rtEfiSgDbEnum, &idxSig);
            else
            {
                RTPrintf("Loading the signature database failed with %Rrc\n", rc);
                rcExit = RTEXITCODE_FAILURE;
            }

            RTEfiSigDbDestroy(hEfiSigDb);
        }
        else
        {
            RTPrintf("Creating the signature database failed with %Rrc\n", rc);
            rcExit = RTEXITCODE_FAILURE;
        }

        RTVfsFileRelease(hVfsFile);
    }
    else
        rcExit = RTEXITCODE_FAILURE;

    return rcExit;
}


/**
 * Handles the 'add' command.
 *
 * @returns Program exit code.
 * @param   pszArg0             The program name.
 * @param   cArgs               The number of arguments to the 'add' command.
 * @param   papszArgs           The argument vector, starting after 'add'.
 */
static RTEXITCODE rtEfiSgDbCmdAdd(const char *pszArg0, int cArgs, char **papszArgs)
{
    RT_NOREF(pszArg0);

    if (!cArgs)
    {
        RTPrintf("The signature database path is missing\n");
        return RTEXITCODE_FAILURE;
    }

    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    RTVFSFILE hVfsFile = NIL_RTVFSFILE;
    int rc = rtEfiSigDbOpen(papszArgs[0], &hVfsFile);
    if (RT_SUCCESS(rc))
    {
        RTEFISIGDB hEfiSigDb;
        rc = RTEfiSigDbCreate(&hEfiSigDb);
        if (RT_SUCCESS(rc))
        {
            uint64_t cbSigDb = 0;
            rc = RTVfsFileQuerySize(hVfsFile, &cbSigDb);
            if (   RT_SUCCESS(rc)
                && cbSigDb)
                rc = RTEfiSigDbAddFromExistingDb(hEfiSigDb, hVfsFile);
            if (RT_SUCCESS(rc))
            {
                cArgs--;
                papszArgs++;

                while (cArgs >= 3)
                {
                    RTEFISIGTYPE enmSigType    = rtEfiSigDbGetTypeById(papszArgs[0]);
                    const char *pszUuidOwner   = papszArgs[1];
                    const char *pszSigDataPath = papszArgs[2];

                    if (enmSigType == RTEFISIGTYPE_INVALID)
                    {
                        RTPrintf("Signature type '%s' is not known\n", papszArgs[0]);
                        break;
                    }

                    RTUUID UuidOwner;
                    rc = RTUuidFromStr(&UuidOwner, pszUuidOwner);
                    if (RT_FAILURE(rc))
                    {
                        RTPrintf("UUID '%s' is malformed\n", pszUuidOwner);
                        break;
                    }

                    RTVFSFILE hVfsFileSigData = NIL_RTVFSFILE;
                    rc = rtEfiSigDbOpen(pszSigDataPath, &hVfsFileSigData);
                    if (RT_FAILURE(rc))
                    {
                        RTPrintf("Opening '%s' failed with %Rrc\n", pszSigDataPath, rc);
                        break;
                    }

                    rc = RTEfiSigDbAddSignatureFromFile(hEfiSigDb, enmSigType, &UuidOwner, hVfsFileSigData);
                    RTVfsFileRelease(hVfsFileSigData);
                    if (RT_FAILURE(rc))
                    {
                        RTPrintf("Adding signature data from '%s' failed with %Rrc\n", pszSigDataPath, rc);
                        break;
                    }
                    papszArgs += 3;
                    cArgs     -= 3;
                }

                if (RT_SUCCESS(rc))
                {
                    if (!cArgs)
                    {
                        rc = RTVfsFileSeek(hVfsFile, 0 /*offSeek*/, RTFILE_SEEK_BEGIN, NULL /*poffActual*/);
                        AssertRC(rc);

                        rc = RTEfiSigDbWriteToFile(hEfiSigDb, hVfsFile);
                        if (RT_FAILURE(rc))
                        {
                            RTPrintf("Writing the updated signature database failed with %Rrc\n", rc);
                            rcExit = RTEXITCODE_FAILURE;
                        }
                    }
                    else
                    {
                        RTPrintf("Incomplete list of entries to add given\n");
                        rcExit = RTEXITCODE_FAILURE;
                    }
                }
            }
            else
            {
                RTPrintf("Loading the signature database failed with %Rrc\n", rc);
                rcExit = RTEXITCODE_FAILURE;
            }

            RTEfiSigDbDestroy(hEfiSigDb);
        }
        else
        {
            RTPrintf("Creating the signature database failed with %Rrc\n", rc);
            rcExit = RTEXITCODE_FAILURE;
        }

        RTVfsFileRelease(hVfsFile);
    }
    else
        rcExit = RTEXITCODE_FAILURE;

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
    else if (!strcmp(argv[1], "add"))
        rcExit = rtEfiSgDbCmdAdd(argv[0], argc - 2, argv + 2);
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

