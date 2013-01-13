/* $Id$ */
/** @file
 * IPRT - TAR Command.
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/zip.h>

#include <iprt/asm.h>
#include <iprt/buildconfig.h>
#include <iprt/ctype.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/param.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/vfs.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define RTZIPTARCMD_OPT_DELETE      1000
#define RTZIPTARCMD_OPT_OWNER       1001
#define RTZIPTARCMD_OPT_GROUP       1002
#define RTZIPTARCMD_OPT_UTC         1003
#define RTZIPTARCMD_OPT_PREFIX      1004


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * IPRT TAR option structure.
 */
typedef struct RTZIPTARCMDOPS
{
    /** The operation (Acdrtux or RTZIPTARCMD_OPT_DELETE). */
    int             iOperation;
    /** The long operation option name. */
    const char     *pszOperation;

    /** The directory to change into when packing and unpacking. */
    const char     *pszDirectory;
    /** The tar file name. */
    const char     *pszFile;
    /** Whether we're verbose or quiet. */
    bool            fVerbose;
    /** Whether to preserve permissions when restoring. */
    bool            fPreservePermissions;
    /** The compressor/decompressor method to employ (0, z or j). */
    char            chZipper;

    /** The owner to set. */
    const char     *pszOwner;
    /** The owner ID to set when unpacking if pszOwner is not NULL. */
    RTUID           uidOwner;
    /** The group to set. */
    const char     *pszGroup;
    /** The group ID to set when unpacking if pszGroup is not NULL. */
    RTGID           gidGroup;
    /** Display the modification times in UTC instead of local time. */
    bool            fDisplayUtc;

    /** What to prefix all names with when creating, adding, whatever. */
    const char     *pszPrefix;

    /** The number of files(, directories or whatever) specified. */
    uint32_t        cFiles;
    /** Array of files(, directories or whatever).
     * Terminated by a NULL entry. */
    const char * const *papszFiles;
} RTZIPTARCMDOPS;
/** Pointer to the IPRT tar options. */
typedef RTZIPTARCMDOPS *PRTZIPTARCMDOPS;


/**
 * Checks if @a pszName is a member of @a papszNames, optionally returning the
 * index.
 *
 * @returns true if the name is in the list, otherwise false.
 * @param   pszName             The name to find.
 * @param   papszNames          The array of names.
 * @param   piName              Where to optionally return the array index.
 */
static bool rtZipTarCmdIsNameInArray(const char *pszName, const char * const *papszNames, uint32_t *piName)
{
    for (uint32_t iName = 0; papszNames[iName]; iName)
        if (!strcmp(papszNames[iName], pszName))
        {
            if (piName)
                *piName = iName;
            return true;
        }
    return false;
}


/**
 * Opens the input archive specified by the options.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE + printed message.
 * @param   pOpts           The options.
 * @param   phVfsFss        Where to return the TAR filesystem stream handle.
 */
static RTEXITCODE rtZipTarCmdOpenInputArchive(PRTZIPTARCMDOPS pOpts, PRTVFSFSSTREAM phVfsFss)
{
    int rc;

    /*
     * Open the input file.
     */
    RTVFSIOSTREAM   hVfsIos;
    if (   pOpts->pszFile
        && strcmp(pOpts->pszFile, "-") != 0)
    {
        const char *pszError;
        rc = RTVfsChainOpenIoStream(pOpts->pszFile,
                                    RTFILE_O_READ | RTFILE_O_DENY_WRITE | RTFILE_O_OPEN,
                                    &hVfsIos,
                                    &pszError);
        if (RT_FAILURE(rc))
        {
            if (pszError && *pszError)
                return RTMsgErrorExit(RTEXITCODE_FAILURE,
                                      "RTVfsChainOpenIoStream failed with rc=%Rrc:\n"
                                      "    '%s'\n",
                                      "     %*s^\n",
                                      rc, pOpts->pszFile, pszError - pOpts->pszFile, "");
            return RTMsgErrorExit(RTEXITCODE_FAILURE,
                                  "Failed with %Rrc opening the input archive '%s'", rc, pOpts->pszFile);
        }
    }
    else
    {
        rc = RTVfsIoStrmFromStdHandle(RTHANDLESTD_INPUT,
                                      RTFILE_O_READ | RTFILE_O_DENY_WRITE | RTFILE_O_OPEN,
                                      true /*fLeaveOpen*/,
                                      &hVfsIos);
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to prepare standard in for reading: %Rrc", rc);
    }

    /*
     * Pass it thru a decompressor?
     */
    RTVFSIOSTREAM hVfsIosDecomp = NIL_RTVFSIOSTREAM;
    switch (pOpts->chZipper)
    {
        /* no */
        case '\0':
            rc = VINF_SUCCESS;
            break;

        /* gunzip */
        case 'z':
            rc = RTZipGzipDecompressIoStream(hVfsIos, 0 /*fFlags*/, &hVfsIosDecomp);
            if (RT_FAILURE(rc))
                RTMsgError("Failed to open gzip decompressor: %Rrc", rc);
            break;

        /* bunzip2 */
        case 'j':
            rc = VERR_NOT_SUPPORTED;
            RTMsgError("bzip2 is not supported by this build");
            break;

        /* bug */
        default:
            rc = VERR_INTERNAL_ERROR_2;
            RTMsgError("unknown decompression method '%c'",  pOpts->chZipper);
            break;
    }
    if (RT_FAILURE(rc))
    {
        RTVfsIoStrmRelease(hVfsIos);
        return RTEXITCODE_FAILURE;
    }

    if (hVfsIosDecomp != NIL_RTVFSIOSTREAM)
    {
        RTVfsIoStrmRelease(hVfsIos);
        hVfsIos = hVfsIosDecomp;
        hVfsIosDecomp = NIL_RTVFSIOSTREAM;
    }

    /*
     * Open the tar filesystem stream.
     */
    rc = RTZipTarFsStreamFromIoStream(hVfsIos, 0/*fFlags*/, phVfsFss);
    RTVfsIoStrmRelease(hVfsIos);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to open tar filesystem stream: %Rrc", rc);

    return RTEXITCODE_SUCCESS;
}


/**
 * Display a tar entry in the verbose form.
 *
 * @returns rcExit or RTEXITCODE_FAILURE.
 * @param   rcExit              The current exit code.
 * @param   hVfsObj             The tar object to display
 * @param   pszName             The name.
 * @param   pOpts               The tar options.
 */
static RTEXITCODE rtZipTarCmdDisplayEntryVerbose(RTEXITCODE rcExit, RTVFSOBJ hVfsObj, const char *pszName,
                                                 PRTZIPTARCMDOPS pOpts)
{
    /*
     * Query all the information.
     */
    RTFSOBJINFO UnixInfo;
    int rc = RTVfsObjQueryInfo(hVfsObj, &UnixInfo, RTFSOBJATTRADD_UNIX);
    if (RT_FAILURE(rc))
    {
        rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "RTVfsObjQueryInfo returned %Rrc on '%s'", rc, pszName);
        RT_ZERO(UnixInfo);
    }

    RTFSOBJINFO Owner;
    rc = RTVfsObjQueryInfo(hVfsObj, &Owner, RTFSOBJATTRADD_UNIX_OWNER);
    if (RT_FAILURE(rc))
    {
        rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE,
                                "RTVfsObjQueryInfo(,,UNIX_OWNER) returned %Rrc on '%s'",
                                rc, pszName);
        RT_ZERO(Owner);
    }

    RTFSOBJINFO Group;
    rc = RTVfsObjQueryInfo(hVfsObj, &Group, RTFSOBJATTRADD_UNIX_GROUP);
    if (RT_FAILURE(rc))
    {
        rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE,
                                "RTVfsObjQueryInfo(,,UNIX_OWNER) returned %Rrc on '%s'",
                                rc, pszName);
        RT_ZERO(Group);
    }

    const char *pszLinkType = NULL;
    char szTarget[RTPATH_MAX];
    szTarget[0] = '\0';
    RTVFSSYMLINK hVfsSymlink = RTVfsObjToSymlink(hVfsObj);
    if (hVfsSymlink != NIL_RTVFSSYMLINK)
    {
        rc = RTVfsSymlinkRead(hVfsSymlink, szTarget, sizeof(szTarget));
        if (RT_FAILURE(rc))
            rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "RTVfsSymlinkRead returned %Rrc on '%s'", rc, pszName);
        RTVfsSymlinkRelease(hVfsSymlink);
        pszLinkType = RTFS_IS_SYMLINK(UnixInfo.Attr.fMode) ? "->" : "link to";
    }
    else if (RTFS_IS_SYMLINK(UnixInfo.Attr.fMode))
        rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to get symlink object for '%s'", pszName);

    /*
     * Translate the mode mask.
     */
    char szMode[16];
    switch (UnixInfo.Attr.fMode & RTFS_TYPE_MASK)
    {
        case RTFS_TYPE_FIFO:        szMode[0] = 'f'; break;
        case RTFS_TYPE_DEV_CHAR:    szMode[0] = 'c'; break;
        case RTFS_TYPE_DIRECTORY:   szMode[0] = 'd'; break;
        case RTFS_TYPE_DEV_BLOCK:   szMode[0] = 'b'; break;
        case RTFS_TYPE_FILE:        szMode[0] = '-'; break;
        case RTFS_TYPE_SYMLINK:     szMode[0] = 'l'; break;
        case RTFS_TYPE_SOCKET:      szMode[0] = 's'; break;
        case RTFS_TYPE_WHITEOUT:    szMode[0] = 'w'; break;
        default:                    szMode[0] = '?'; break;
    }
    if (pszLinkType && szMode[0] != 's')
        szMode[0] = 'h';

    szMode[1] = UnixInfo.Attr.fMode & RTFS_UNIX_IRUSR ? 'r' : '-';
    szMode[2] = UnixInfo.Attr.fMode & RTFS_UNIX_IWUSR ? 'w' : '-';
    szMode[3] = UnixInfo.Attr.fMode & RTFS_UNIX_IXUSR ? 'x' : '-';

    szMode[4] = UnixInfo.Attr.fMode & RTFS_UNIX_IRGRP ? 'r' : '-';
    szMode[5] = UnixInfo.Attr.fMode & RTFS_UNIX_IWGRP ? 'w' : '-';
    szMode[6] = UnixInfo.Attr.fMode & RTFS_UNIX_IXGRP ? 'x' : '-';

    szMode[7] = UnixInfo.Attr.fMode & RTFS_UNIX_IROTH ? 'r' : '-';
    szMode[8] = UnixInfo.Attr.fMode & RTFS_UNIX_IWOTH ? 'w' : '-';
    szMode[9] = UnixInfo.Attr.fMode & RTFS_UNIX_IXOTH ? 'x' : '-';
    szMode[10] = '\0';

    /** @todo sticky and set-uid/gid bits. */

    /*
     * Make sure we've got valid owner and group strings.
     */
    if (!Owner.Attr.u.UnixGroup.szName[0])
        RTStrPrintf(Owner.Attr.u.UnixOwner.szName, sizeof(Owner.Attr.u.UnixOwner.szName),
                    "%u", UnixInfo.Attr.u.Unix.uid);

    if (!Group.Attr.u.UnixOwner.szName[0])
        RTStrPrintf(Group.Attr.u.UnixGroup.szName, sizeof(Group.Attr.u.UnixGroup.szName),
                    "%u", UnixInfo.Attr.u.Unix.gid);

    /*
     * Format the modification time.
     */
    char       szModTime[32];
    RTTIME     ModTime;
    PRTTIME    pTime;
    if (!pOpts->fDisplayUtc)
        pTime = RTTimeLocalExplode(&ModTime, &UnixInfo.ModificationTime);
    else
        pTime = RTTimeExplode(&ModTime, &UnixInfo.ModificationTime);
    if (!pTime)
        RT_ZERO(ModTime);
    RTStrPrintf(szModTime, sizeof(szModTime), "%04d-%02u-%02u %02u:%02u",
                ModTime.i32Year, ModTime.u8Month, ModTime.u8MonthDay, ModTime.u8Hour, ModTime.u8Minute);

    /*
     * Format the size and figure how much space is needed between the
     * user/group and the size.
     */
    char   szSize[64];
    size_t cchSize;
    switch (UnixInfo.Attr.fMode & RTFS_TYPE_MASK)
    {
        case RTFS_TYPE_DEV_CHAR:
        case RTFS_TYPE_DEV_BLOCK:
            cchSize = RTStrPrintf(szSize, sizeof(szSize), "%u,%u",
                                  RTDEV_MAJOR(UnixInfo.Attr.u.Unix.Device), RTDEV_MINOR(UnixInfo.Attr.u.Unix.Device));
            break;
        default:
            cchSize = RTStrPrintf(szSize, sizeof(szSize), "%RU64", UnixInfo.cbObject);
            break;
    }

    size_t cchUserGroup = strlen(Owner.Attr.u.UnixOwner.szName)
                        + 1
                        + strlen(Group.Attr.u.UnixGroup.szName);
    ssize_t cchPad = cchUserGroup + cchSize + 1 < 19
                   ? 19 - (cchUserGroup + cchSize + 1)
                   : 0;

    /*
     * Go to press.
     */
    if (pszLinkType)
        RTPrintf("%s %s/%s%*s %s %s %s %s %s\n",
                 szMode,
                 Owner.Attr.u.UnixOwner.szName, Group.Attr.u.UnixGroup.szName,
                 cchPad, "",
                 szSize,
                 szModTime,
                 pszName,
                 pszLinkType,
                 szTarget);
    else
        RTPrintf("%s %s/%s%*s %s %s %s\n",
                 szMode,
                 Owner.Attr.u.UnixOwner.szName, Group.Attr.u.UnixGroup.szName,
                 cchPad, "",
                 szSize,
                 szModTime,
                 pszName);

    return rcExit;
}

/**
 * Implements the -t/--list operation.
 *
 * @returns The appropriate exit code.
 * @param   pOpts               The tar options.
 */
static RTEXITCODE rtZipTarCmdList(PRTZIPTARCMDOPS pOpts)
{
    /*
     * Allocate a bitmap to go with the file list.  This will be used to
     * indicate which files we've processed and which not.
     */
    uint32_t *pbmFound = NULL;
    if (pOpts->cFiles)
    {
        pbmFound = (uint32_t *)RTMemAllocZ(((pOpts->cFiles + 31) / 32) * sizeof(uint32_t));
        if (!pbmFound)
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate the found-file-bitmap");
    }


    /*
     * Open the input archive.
     */
    RTVFSFSSTREAM hVfsFssIn;
    RTEXITCODE rcExit = rtZipTarCmdOpenInputArchive(pOpts, &hVfsFssIn);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        /*
         * Process the stream.
         */
        for (;;)
        {
            /*
             * Retrive the next object.
             */
            char       *pszName;
            RTVFSOBJ    hVfsObj;
            int rc = RTVfsFsStrmNext(hVfsFssIn, &pszName, NULL, &hVfsObj);
            if (RT_FAILURE(rc))
            {
                if (rc != VERR_EOF)
                    rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "RTVfsFsStrmNext returned %Rrc", rc);
                break;
            }

            /*
             * Should we display this entry?
             */
            uint32_t    iFile = UINT32_MAX;
            if (   !pOpts->cFiles
                || rtZipTarCmdIsNameInArray(pszName, pOpts->papszFiles, &iFile) )
            {
                if (pbmFound)
                    ASMBitSet(pbmFound, iFile);

                if (!pOpts->fVerbose)
                    RTPrintf("%s\n", pszName);
                else
                    rcExit = rtZipTarCmdDisplayEntryVerbose(rcExit, hVfsObj, pszName, pOpts);
            }

            /*
             * Release the current object and string.
             */
            RTVfsObjRelease(hVfsObj);
            RTStrFree(pszName);
        }

        /*
         * Complain about any files we didn't find.
         */
        for (uint32_t iFile = 0; iFile < pOpts->cFiles; iFile++)
            if (!ASMBitTest(pbmFound, iFile))
            {
                RTMsgError("%s: Was not found in the archive", pOpts->papszFiles[iFile]);
                rcExit = RTEXITCODE_FAILURE;
            }
    }
    RTMemFree(pbmFound);
    return rcExit;
}



RTDECL(RTEXITCODE) RTZipTarCmd(unsigned cArgs, char **papszArgs)
{
    /*
     * Parse the command line.
     *
     * N.B. This is less flexible that your regular tar program in that it
     *      requires the operation to be specified as an option.  On the other
     *      hand, you can specify it where ever you like in the command line.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        /* operations */
        { "--concatenate",          'A', RTGETOPT_REQ_NOTHING },
        { "--catenate",             'A', RTGETOPT_REQ_NOTHING },
        { "--create",               'c', RTGETOPT_REQ_NOTHING },
        { "--diff",                 'd', RTGETOPT_REQ_NOTHING },
        { "--compare",              'd', RTGETOPT_REQ_NOTHING },
        { "--append",               'r', RTGETOPT_REQ_NOTHING },
        { "--list",                 't', RTGETOPT_REQ_NOTHING },
        { "--update",               'u', RTGETOPT_REQ_NOTHING },
        { "--extract",              'x', RTGETOPT_REQ_NOTHING },
        { "--get",                  'x', RTGETOPT_REQ_NOTHING },
        { "--delete",       RTZIPTARCMD_OPT_DELETE, RTGETOPT_REQ_NOTHING },

        /* basic options */
        { "--directory",            'C', RTGETOPT_REQ_STRING },
        { "--file",                 'f', RTGETOPT_REQ_STRING },
        { "--verbose",              'v', RTGETOPT_REQ_NOTHING },
        { "--preserve-permissions", 'p', RTGETOPT_REQ_NOTHING },
        { "--bzip2",                'j', RTGETOPT_REQ_NOTHING },
        { "--gzip",                 'z', RTGETOPT_REQ_NOTHING },
        { "--gunzip",               'z', RTGETOPT_REQ_NOTHING },
        { "--ungzip",               'z', RTGETOPT_REQ_NOTHING },

        /* other options. */
        { "--owner",                RTZIPTARCMD_OPT_OWNER, RTGETOPT_REQ_STRING },
        { "--group",                RTZIPTARCMD_OPT_GROUP, RTGETOPT_REQ_STRING },
        { "--utc",                  RTZIPTARCMD_OPT_UTC,  RTGETOPT_REQ_NOTHING },

        /* IPRT extensions */
        { "--prefix",               RTZIPTARCMD_OPT_PREFIX, RTGETOPT_REQ_STRING },
    };

    RTGETOPTSTATE GetState;
    int rc = RTGetOptInit(&GetState, cArgs, papszArgs, s_aOptions, RT_ELEMENTS(s_aOptions), 1,
                          RTGETOPTINIT_FLAGS_OPTS_FIRST);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTGetOpt failed: %Rrc", rc);

    RTZIPTARCMDOPS Opts;
    RT_ZERO(Opts); /* nice defaults :-) */

    RTGETOPTUNION   ValueUnion;
    while (   (rc = RTGetOpt(&GetState, &ValueUnion)) != 0
           && rc != VINF_GETOPT_NOT_OPTION)
    {
        switch (rc)
        {
            /* operations */
            case 'A':
            case 'c':
            case 'd':
            case 'r':
            case 't':
            case 'u':
            case 'x':
            case RTZIPTARCMD_OPT_DELETE:
                if (Opts.iOperation)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Conflicting tar operation (%s already set, now %s)",
                                          Opts.pszOperation, ValueUnion.pDef->pszLong);
                Opts.iOperation   = rc;
                Opts.pszOperation = ValueUnion.pDef->pszLong;
                break;

            /* basic options */
            case 'C':
                if (Opts.pszDirectory)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "You may only specify -C/--directory once");
                Opts.pszDirectory = ValueUnion.psz;
                break;

            case 'f':
                if (Opts.pszFile)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "You may only specify -f/--file once");
                Opts.pszFile = ValueUnion.psz;
                break;

            case 'v':
                Opts.fVerbose = true;
                break;

            case 'p':
                Opts.fPreservePermissions = true;
                break;

            case 'j':
            case 'z':
                if (Opts.chZipper)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "You may only specify one compressor / decompressor");
                Opts.chZipper = rc;
                break;

            case RTZIPTARCMD_OPT_OWNER:
                if (Opts.pszOwner)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "You may only specify --owner once");
                Opts.pszOwner = ValueUnion.psz;
                break;

            case RTZIPTARCMD_OPT_GROUP:
                if (Opts.pszGroup)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "You may only specify --group once");
                Opts.pszGroup = ValueUnion.psz;
                break;

            case RTZIPTARCMD_OPT_UTC:
                Opts.fDisplayUtc = true;
                break;

            /* iprt extensions */
            case RTZIPTARCMD_OPT_PREFIX:
                if (Opts.pszPrefix)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "You may only specify --prefix once");
                Opts.pszPrefix = ValueUnion.psz;
                break;

            case 'h':
                RTPrintf("Usage: to be written\nOption dump:\n");
                for (unsigned i = 0; i < RT_ELEMENTS(s_aOptions); i++)
                    if (RT_C_IS_PRINT(s_aOptions[i].iShort))
                        RTPrintf(" -%c,%s\n", s_aOptions[i].iShort, s_aOptions[i].pszLong);
                    else
                        RTPrintf(" %s\n", s_aOptions[i].pszLong);
                return RTEXITCODE_SUCCESS;

            case 'V':
                RTPrintf("%sr%d\n", RTBldCfgVersion(), RTBldCfgRevision());
                return RTEXITCODE_SUCCESS;

            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    if (rc == VINF_GETOPT_NOT_OPTION)
    {
        /* this is kind of ugly. */
        Assert((unsigned)GetState.iNext - 1 <= cArgs);
        Opts.papszFiles = (const char * const *)&papszArgs[GetState.iNext - 1];
        Opts.cFiles     = cArgs - GetState.iNext + 1;
    }

    /*
     * Post proceess the options.
     */
    if (Opts.iOperation == 0)
    {
        Opts.iOperation   = 't';
        Opts.pszOperation = "--list";
    }

    if (   Opts.iOperation == 'x'
        && Opts.pszOwner)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "The use of --owner with %s has not implemented yet", Opts.pszOperation);

    if (   Opts.iOperation == 'x'
        && Opts.pszGroup)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "The use of --group with %s has not implemented yet", Opts.pszOperation);

    /*
     * Do the job.
     */
    switch (Opts.iOperation)
    {
        case 't':
            return rtZipTarCmdList(&Opts);

        case 'A':
        case 'c':
        case 'd':
        case 'r':
        case 'u':
        case 'x':
        case RTZIPTARCMD_OPT_DELETE:
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "The operation %s is not implemented yet", Opts.pszOperation);

        default:
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Internal error");
    }
}

