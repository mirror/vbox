/* $Id$ */
/** @file
 * VBoxServiceToolbox - Internal (BusyBox-like) toolbox.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <stdio.h>

#include <iprt/assert.h>
#include <iprt/buildconfig.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/stream.h>

#ifndef RT_OS_WINDOWS
# include <sys/stat.h> /* need umask */
#endif

#include <VBox/VBoxGuestLib.h>
#include <VBox/version.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Options indices for "vbox_cat". */
#define CAT_OPT_NO_CONTENT_INDEXED              1000
/** Options indices for "vbox_ls". */
#define LS_OPT_MACHINE_READABLE                 1000
/** Options indices for "vbox_stat". */
#define STAT_OPT_MACHINE_READABLE               1000
/** Flags for "vbox_ls". */
typedef enum VBOXSERVICETOOLBOXLSFLAG
{
    VBOXSERVICETOOLBOXLSFLAG_NONE =             0x0,
    VBOXSERVICETOOLBOXLSFLAG_RECURSIVE =        0x1,
    VBOXSERVICETOOLBOXLSFLAG_SYMLINKS =         0x2
} VBOXSERVICETOOLBOXLSFLAG;

/** Flags for fs object output. */
typedef enum VBOXSERVICETOOLBOXOUTPUTFLAG
{
    VBOXSERVICETOOLBOXOUTPUTFLAG_NONE =         0x0,
    VBOXSERVICETOOLBOXOUTPUTFLAG_LONG =         0x1,
    VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE =    0x2
} VBOXSERVICETOOLBOXOUTPUTFLAG;


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * An file/directory entry. Used to cache
 * file names/paths for later processing.
 */
typedef struct VBOXSERVICETOOLBOXPATHENTRY
{
    /** Our node. */
    RTLISTNODE  Node;
    /** Name of the entry. */
    char       *pszName;
} VBOXSERVICETOOLBOXPATHENTRY, *PVBOXSERVICETOOLBOXPATHENTRY;

typedef struct VBOXSERVICETOOLBOXDIRENTRY
{
    /** Our node. */
    RTLISTNODE   Node;
    /** The actual entry. */
    RTDIRENTRYEX dirEntry;
} VBOXSERVICETOOLBOXDIRENTRY, *PVBOXSERVICETOOLBOXDIRENTRY;


/**
 * Displays a help text to stdout.
 */
static void VBoxServiceToolboxShowUsage(void)
{
    RTPrintf("Toolbox Usage:\n"
             "cat [FILE] - Concatenate FILE(s), or standard input, to standard output.\n"
             "\n"
             /** @todo Document options! */
             "ls [OPTION]... FILE... - List information about the FILEs (the current directory by default).\n"
             "\n"
             /** @todo Document options! */
             "mkdir [OPTION]... DIRECTORY... - Create the DIRECTORY(ies), if they do not already exist.\n"
             "\n"
             /** @todo Document options! */
             "stat [OPTION]... FILE... - Display file or file system status.\n"
             "\n"
             /** @todo Document options! */
             "\n");
}


/**
 * Displays the program's version number.
 */
static void VBoxServiceToolboxShowVersion(void)
{
    RTPrintf("%sr%d\n", VBOX_VERSION_STRING, RTBldCfgRevision());
}


/**
 * Displays an error message because of syntax error.
 *
 * @return  VERR_INVALID_PARAMETER
 * @param   pszFormat
 */
static int VBoxServiceToolboxErrorSyntax(const char *pszFormat, ...)
{
    va_list args;

    va_start(args, pszFormat);
    RTPrintf("\n"
             "Syntax error: %N\n", pszFormat, &args);
    va_end(args);
    return VERR_INVALID_PARAMETER;
}

/**
 * Prints a parseable stream header which contains the actual tool
 * which was called/used along with its stream version.
 *
 * @param   pszToolName             Name of the tool being used, e.g. "vbt_ls".
 * @param   uVersion                Stream version name. Handy for distinguishing
 *                                  different stream versions later.
 */
static void VBoxServiceToolboxPrintStrmHeader(const char *pszToolName, uint32_t uVersion)
{
    AssertPtrReturnVoid(pszToolName);
    RTPrintf("hdr_id=%s%chdr_ver=%u%c", pszToolName, 0, uVersion, 0);
}

/**
 * Prints a standardized termination sequence indicating that the
 * parseable stream just ended.
 *
 */
static void VBoxServiceToolboxPrintStrmTermination()
{
    RTPrintf("%c%c%c%c", 0, 0, 0, 0);
}

/**
 * Destroys a path buffer list.
 *
 * @return  IPRT status code.
 * @param   pList                   Pointer to list to destroy.
 */
static void VBoxServiceToolboxPathBufDestroy(PRTLISTNODE pList)
{
    AssertPtr(pList);
    /** @todo use RTListForEachSafe */
    PVBOXSERVICETOOLBOXPATHENTRY pNode = RTListGetFirst(pList, VBOXSERVICETOOLBOXPATHENTRY, Node);
    while (pNode)
    {
        PVBOXSERVICETOOLBOXPATHENTRY pNext = RTListNodeIsLast(pList, &pNode->Node)
                                           ? NULL
                                           : RTListNodeGetNext(&pNode->Node, VBOXSERVICETOOLBOXPATHENTRY, Node);
        RTListNodeRemove(&pNode->Node);

        RTStrFree(pNode->pszName);

        RTMemFree(pNode);
        pNode = pNext;
    }
}


/**
 * Adds a path entry (file/directory/whatever) to a given path buffer list.
 *
 * @return  IPRT status code.
 * @param   pList                   Pointer to list to add entry to.
 * @param   pszName                 Name of entry to add.
 */
static int VBoxServiceToolboxPathBufAddPathEntry(PRTLISTNODE pList, const char *pszName)
{
    AssertPtrReturn(pList, VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;
    PVBOXSERVICETOOLBOXPATHENTRY pNode = (PVBOXSERVICETOOLBOXPATHENTRY)RTMemAlloc(sizeof(VBOXSERVICETOOLBOXPATHENTRY));
    if (pNode)
    {
        pNode->pszName = RTStrDup(pszName);
        AssertPtr(pNode->pszName);

        /*rc =*/ RTListAppend(pList, &pNode->Node);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


/**
 * Performs the actual output operation of "vbox_cat".
 *
 * @return  IPRT status code.
 * @param   hInput                  Handle of input file (if any) to use;
 *                                  else stdin will be used.
 * @param   hOutput                 Handle of output file (if any) to use;
 *                                  else stdout will be used.
 */
static int VBoxServiceToolboxCatOutput(RTFILE hInput, RTFILE hOutput)
{
    int rc = VINF_SUCCESS;
    if (hInput == NIL_RTFILE)
    {
        rc = RTFileFromNative(&hInput, RTFILE_NATIVE_STDIN);
        if (RT_FAILURE(rc))
            RTMsgError("cat: Could not translate input file to native handle, rc=%Rrc\n", rc);
    }

    if (hOutput == NIL_RTFILE)
    {
        rc = RTFileFromNative(&hOutput, RTFILE_NATIVE_STDOUT);
        if (RT_FAILURE(rc))
            RTMsgError("cat: Could not translate output file to native handle, rc=%Rrc\n", rc);
    }

    if (RT_SUCCESS(rc))
    {
        uint8_t abBuf[_64K];
        size_t cbRead;
        for (;;)
        {
            rc = RTFileRead(hInput, abBuf, sizeof(abBuf), &cbRead);
            if (RT_SUCCESS(rc) && cbRead > 0)
            {
                rc = RTFileWrite(hOutput, abBuf, cbRead, NULL /* Try to write all at once! */);
                cbRead = 0;
            }
            else
            {
                if (rc == VERR_BROKEN_PIPE)
                    rc = VINF_SUCCESS;
                else if (RT_FAILURE(rc))
                    RTMsgError("cat: Error while reading input, rc=%Rrc\n", rc);
                break;
            }
        }
    }
    return rc;
}


/**
 * Main function for tool "vbox_cat".
 *
 * @return  RTEXITCODE.
 * @param   argc                    Number of arguments.
 * @param   argv                    Pointer to argument array.
 */
static RTEXITCODE VBoxServiceToolboxCat(int argc, char **argv)
{
    static const RTGETOPTDEF s_aOptions[] =
    {
        /* Sorted by short ops. */
        { "--show-all",            'a',                         RTGETOPT_REQ_NOTHING },
        { "--number-nonblank",     'b',                         RTGETOPT_REQ_NOTHING},
        { NULL,                    'e',                         RTGETOPT_REQ_NOTHING},
        { NULL,                    'E',                         RTGETOPT_REQ_NOTHING},
        { "--flags",               'f',                         RTGETOPT_REQ_STRING},
        { "--no-content-indexed",  CAT_OPT_NO_CONTENT_INDEXED,  RTGETOPT_REQ_NOTHING},
        { "--number",              'n',                         RTGETOPT_REQ_NOTHING},
        { "--output",              'o',                         RTGETOPT_REQ_STRING},
        { "--squeeze-blank",       's',                         RTGETOPT_REQ_NOTHING},
        { NULL,                    't',                         RTGETOPT_REQ_NOTHING},
        { "--show-tabs",           'T',                         RTGETOPT_REQ_NOTHING},
        { NULL,                    'u',                         RTGETOPT_REQ_NOTHING},
        { "--show-noneprinting",   'v',                         RTGETOPT_REQ_NOTHING}
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;

    RTGetOptInit(&GetState, argc, argv,
                 s_aOptions, RT_ELEMENTS(s_aOptions),
                 1 /*iFirst*/, 0 /*fFlags*/);

    int rc = VINF_SUCCESS;
    bool fUsageOK = true;

    char szOutput[RTPATH_MAX] = { 0 };
    RTFILE hOutput = NIL_RTFILE;
    uint32_t fFlags = RTFILE_O_CREATE_REPLACE /* Output file flags. */
                      | RTFILE_O_WRITE
                      | RTFILE_O_DENY_WRITE;

    /* Init directory list. */
    RTLISTNODE inputList;
    RTListInit(&inputList);

    while (   (ch = RTGetOpt(&GetState, &ValueUnion))
              && RT_SUCCESS(rc))
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            case 'a':
            case 'b':
            case 'e':
            case 'E':
            case 'n':
            case 's':
            case 't':
            case 'T':
            case 'v':
                RTMsgError("cat: Sorry, option '%s' is not implemented yet!\n",
                           ValueUnion.pDef->pszLong);
                rc = VERR_INVALID_PARAMETER;
                break;

            case 'h':
                VBoxServiceToolboxShowUsage();
                return RTEXITCODE_SUCCESS;

            case 'o':
                if (!RTStrPrintf(szOutput, sizeof(szOutput), ValueUnion.psz))
                    rc = VERR_NO_MEMORY;
                break;

            case 'u':
                /* Ignored. */
                break;

            case 'V':
                VBoxServiceToolboxShowVersion();
                return RTEXITCODE_SUCCESS;

            case CAT_OPT_NO_CONTENT_INDEXED:
                fFlags |= RTFILE_O_NOT_CONTENT_INDEXED;
                break;

            case VINF_GETOPT_NOT_OPTION:
                {
                    /* Add file(s) to buffer. This enables processing multiple paths
                     * at once.
                     *
                     * Since the non-options (RTGETOPTINIT_FLAGS_OPTS_FIRST) come last when
                     * processing this loop it's safe to immediately exit on syntax errors
                     * or showing the help text (see above). */
                    rc = VBoxServiceToolboxPathBufAddPathEntry(&inputList, ValueUnion.psz);
                    break;
                }

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }

    if (RT_SUCCESS(rc))
    {
        if (strlen(szOutput))
        {
            rc = RTFileOpen(&hOutput, szOutput, fFlags);
            if (RT_FAILURE(rc))
                RTMsgError("cat: Could not create output file '%s', rc=%Rrc\n",
                           szOutput, rc);
        }

        if (RT_SUCCESS(rc))
        {
            /* Process each input file. */
            PVBOXSERVICETOOLBOXPATHENTRY pNodeIt;
            RTFILE hInput = NIL_RTFILE;
            RTListForEach(&inputList, pNodeIt, VBOXSERVICETOOLBOXPATHENTRY, Node)
            {
                rc = RTFileOpen(&hInput, pNodeIt->pszName,
                                RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
                if (RT_SUCCESS(rc))
                {
                    rc = VBoxServiceToolboxCatOutput(hInput, hOutput);
                    RTFileClose(hInput);
                }
                else
                {
                    PCRTSTATUSMSG pMsg = RTErrGet(rc);
                    if (pMsg)
                        RTMsgError("cat: Could not open input file '%s': %s\n",
                                   pNodeIt->pszName, pMsg->pszMsgFull);
                    else
                        RTMsgError("cat: Could not open input file '%s', rc=%Rrc\n", pNodeIt->pszName, rc);
                }

                if (RT_FAILURE(rc))
                    break;
            }

            /* If not input files were defined, process stdin. */
            if (RTListNodeIsFirst(&inputList, &inputList))
                rc = VBoxServiceToolboxCatOutput(hInput, hOutput);
        }
    }

    if (hOutput != NIL_RTFILE)
        RTFileClose(hOutput);
    VBoxServiceToolboxPathBufDestroy(&inputList);

    return RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

/**
 * Prints information (based on given flags) of a file system object (file/directory/...)
 * to stdout.
 *
 * @return  IPRT status code.
 * @param   pszName                     Object name.
 * @param   cbName                      Size of object name.
 * @param   uOutputFlags                Output / handling flags of type VBOXSERVICETOOLBOXOUTPUTFLAG.
 * @param   pObjInfo                    Pointer to object information.
 */
static int VBoxServiceToolboxPrintFsInfo(const char *pszName, uint16_t cbName,
                                         uint32_t uOutputFlags,
                                         PRTFSOBJINFO pObjInfo)
{
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertReturn(cbName, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pObjInfo, VERR_INVALID_POINTER);

    RTFMODE fMode = pObjInfo->Attr.fMode;
    char chFileType;
    switch (fMode & RTFS_TYPE_MASK)
    {
        case RTFS_TYPE_FIFO:        chFileType = 'f'; break;
        case RTFS_TYPE_DEV_CHAR:    chFileType = 'c'; break;
        case RTFS_TYPE_DIRECTORY:   chFileType = 'd'; break;
        case RTFS_TYPE_DEV_BLOCK:   chFileType = 'b'; break;
        case RTFS_TYPE_FILE:        chFileType = '-'; break;
        case RTFS_TYPE_SYMLINK:     chFileType = 'l'; break;
        case RTFS_TYPE_SOCKET:      chFileType = 's'; break;
        case RTFS_TYPE_WHITEOUT:    chFileType = 'w'; break;
        default:                    chFileType = '?'; break;
    }
    /** @todo sticy bits++ */

    if (!(uOutputFlags & VBOXSERVICETOOLBOXOUTPUTFLAG_LONG))
    {
        if (uOutputFlags & VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE)
        {
            /** @todo Skip node_id if not present/available! */
            RTPrintf("ftype=%c%cnode_id=%RU64%cname_len=%RU16%cname=%s%c",
                     chFileType, 0, (uint64_t)pObjInfo->Attr.u.Unix.INodeId, 0,
                     cbName, 0, pszName, 0);
        }
        else
            RTPrintf("%c %#18llx %3d %s\n",
                     chFileType, (uint64_t)pObjInfo->Attr.u.Unix.INodeId, cbName, pszName);

        if (uOutputFlags & VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE) /* End of data block. */
            RTPrintf("%c%c", 0, 0);
    }
    else
    {
        if (uOutputFlags & VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE)
        {
            RTPrintf("ftype=%c%c", chFileType, 0);
            RTPrintf("owner_mask=%c%c%c%c",
                     fMode & RTFS_UNIX_IRUSR ? 'r' : '-',
                     fMode & RTFS_UNIX_IWUSR ? 'w' : '-',
                     fMode & RTFS_UNIX_IXUSR ? 'x' : '-', 0);
            RTPrintf("group_mask=%c%c%c%c",
                     fMode & RTFS_UNIX_IRGRP ? 'r' : '-',
                     fMode & RTFS_UNIX_IWGRP ? 'w' : '-',
                     fMode & RTFS_UNIX_IXGRP ? 'x' : '-', 0);
            RTPrintf("other_mask=%c%c%c%c",
                     fMode & RTFS_UNIX_IROTH ? 'r' : '-',
                     fMode & RTFS_UNIX_IWOTH ? 'w' : '-',
                     fMode & RTFS_UNIX_IXOTH ? 'x' : '-', 0);
            RTPrintf("dos_mask=%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
                     fMode & RTFS_DOS_READONLY          ? 'R' : '-',
                     fMode & RTFS_DOS_HIDDEN            ? 'H' : '-',
                     fMode & RTFS_DOS_SYSTEM            ? 'S' : '-',
                     fMode & RTFS_DOS_DIRECTORY         ? 'D' : '-',
                     fMode & RTFS_DOS_ARCHIVED          ? 'A' : '-',
                     fMode & RTFS_DOS_NT_DEVICE         ? 'd' : '-',
                     fMode & RTFS_DOS_NT_NORMAL         ? 'N' : '-',
                     fMode & RTFS_DOS_NT_TEMPORARY      ? 'T' : '-',
                     fMode & RTFS_DOS_NT_SPARSE_FILE    ? 'P' : '-',
                     fMode & RTFS_DOS_NT_REPARSE_POINT  ? 'J' : '-',
                     fMode & RTFS_DOS_NT_COMPRESSED     ? 'C' : '-',
                     fMode & RTFS_DOS_NT_OFFLINE        ? 'O' : '-',
                     fMode & RTFS_DOS_NT_NOT_CONTENT_INDEXED ? 'I' : '-',
                     fMode & RTFS_DOS_NT_ENCRYPTED      ? 'E' : '-', 0);

            char szTimeBirth[256];
            RTTimeSpecToString(&pObjInfo->BirthTime, szTimeBirth, sizeof(szTimeBirth));
            char szTimeChange[256];
            RTTimeSpecToString(&pObjInfo->ChangeTime, szTimeChange, sizeof(szTimeChange));
            char szTimeModification[256];
            RTTimeSpecToString(&pObjInfo->ModificationTime, szTimeModification, sizeof(szTimeModification));
            char szTimeAccess[256];
            RTTimeSpecToString(&pObjInfo->AccessTime, szTimeAccess, sizeof(szTimeAccess));

            RTPrintf("hlinks=%RU32%cuid=%RU32%cgid=%RU32%cst_size=%RI64%calloc=%RI64%c"
                     "st_birthtime=%s%cst_ctime=%s%cst_mtime=%s%cst_atime=%s%c",
                     pObjInfo->Attr.u.Unix.cHardlinks, 0,
                     pObjInfo->Attr.u.Unix.uid, 0,
                     pObjInfo->Attr.u.Unix.gid, 0,
                     pObjInfo->cbObject, 0,
                     pObjInfo->cbAllocated, 0,
                     szTimeBirth, 0,
                     szTimeChange, 0,
                     szTimeModification, 0,
                     szTimeAccess, 0);
            RTPrintf("cname_len=%RU16%cname=%s%c",
                     cbName, 0, pszName, 0);

            /* End of data block. */
            RTPrintf("%c%c", 0, 0);
        }
        else
        {
            RTPrintf("%c", chFileType);
            RTPrintf("%c%c%c",
                     fMode & RTFS_UNIX_IRUSR ? 'r' : '-',
                     fMode & RTFS_UNIX_IWUSR ? 'w' : '-',
                     fMode & RTFS_UNIX_IXUSR ? 'x' : '-');
            RTPrintf("%c%c%c",
                     fMode & RTFS_UNIX_IRGRP ? 'r' : '-',
                     fMode & RTFS_UNIX_IWGRP ? 'w' : '-',
                     fMode & RTFS_UNIX_IXGRP ? 'x' : '-');
            RTPrintf("%c%c%c",
                     fMode & RTFS_UNIX_IROTH ? 'r' : '-',
                     fMode & RTFS_UNIX_IWOTH ? 'w' : '-',
                     fMode & RTFS_UNIX_IXOTH ? 'x' : '-');
            RTPrintf(" %c%c%c%c%c%c%c%c%c%c%c%c%c%c",
                     fMode & RTFS_DOS_READONLY          ? 'R' : '-',
                     fMode & RTFS_DOS_HIDDEN            ? 'H' : '-',
                     fMode & RTFS_DOS_SYSTEM            ? 'S' : '-',
                     fMode & RTFS_DOS_DIRECTORY         ? 'D' : '-',
                     fMode & RTFS_DOS_ARCHIVED          ? 'A' : '-',
                     fMode & RTFS_DOS_NT_DEVICE         ? 'd' : '-',
                     fMode & RTFS_DOS_NT_NORMAL         ? 'N' : '-',
                     fMode & RTFS_DOS_NT_TEMPORARY      ? 'T' : '-',
                     fMode & RTFS_DOS_NT_SPARSE_FILE    ? 'P' : '-',
                     fMode & RTFS_DOS_NT_REPARSE_POINT  ? 'J' : '-',
                     fMode & RTFS_DOS_NT_COMPRESSED     ? 'C' : '-',
                     fMode & RTFS_DOS_NT_OFFLINE        ? 'O' : '-',
                     fMode & RTFS_DOS_NT_NOT_CONTENT_INDEXED ? 'I' : '-',
                     fMode & RTFS_DOS_NT_ENCRYPTED      ? 'E' : '-');
            RTPrintf(" %d %4d %4d %10lld %10lld %#llx %#llx %#llx %#llx",
                     pObjInfo->Attr.u.Unix.cHardlinks,
                     pObjInfo->Attr.u.Unix.uid,
                     pObjInfo->Attr.u.Unix.gid,
                     pObjInfo->cbObject,
                     pObjInfo->cbAllocated,
                     pObjInfo->BirthTime,
                     pObjInfo->ChangeTime,
                     pObjInfo->ModificationTime,
                     pObjInfo->AccessTime);
            RTPrintf(" %2d %s\n", cbName, pszName);
        }
    }

    return VINF_SUCCESS;
}


/**
 * Helper routine for ls tool doing the actual parsing and output of
 * a specified directory.
 *
 * @return  IPRT status code.
 * @param   pszDir                  Directory (path) to ouptut.
 * @param   uFlags                  Flags of type VBOXSERVICETOOLBOXLSFLAG.
 * @param   uOutputFlags            Flags of type  VBOXSERVICETOOLBOXOUTPUTFLAG.
 */
static int VBoxServiceToolboxLsHandleDir(const char *pszDir,
                                         uint32_t uFlags, uint32_t uOutputFlags)
{
    AssertPtrReturn(pszDir, VERR_INVALID_PARAMETER);

    if (uFlags & VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE)
        RTPrintf("dname=%s%c", pszDir, 0);
    else if (uFlags & VBOXSERVICETOOLBOXLSFLAG_RECURSIVE)
        RTPrintf("%s:\n", pszDir);

    char szPathAbs[RTPATH_MAX + 1];
    int rc = RTPathAbs(pszDir, szPathAbs, sizeof(szPathAbs));
    if (RT_FAILURE(rc))
    {
        RTMsgError("ls: Failed to retrieve absolute path of '%s', rc=%Rrc\n", pszDir, rc);
        return rc;
    }

    PRTDIR pDir;
    rc = RTDirOpen(&pDir, szPathAbs);
    if (RT_FAILURE(rc))
    {
        RTMsgError("ls: Failed to open directory '%s', rc=%Rrc\n", szPathAbs, rc);
        return rc;
    }

    RTLISTNODE dirList;
    RTListInit(&dirList);

    /* To prevent races we need to read in the directory entries once
     * and process them afterwards: First loop is displaying the current
     * directory's content and second loop is diving deeper into
     * sub directories (if wanted). */
    for (;RT_SUCCESS(rc);)
    {
        RTDIRENTRYEX DirEntry;
        rc = RTDirReadEx(pDir, &DirEntry, NULL, RTFSOBJATTRADD_UNIX, RTPATH_F_ON_LINK);
        if (RT_SUCCESS(rc))
        {
            PVBOXSERVICETOOLBOXDIRENTRY pNode = (PVBOXSERVICETOOLBOXDIRENTRY)RTMemAlloc(sizeof(VBOXSERVICETOOLBOXDIRENTRY));
            if (pNode)
            {
                memcpy(&pNode->dirEntry, &DirEntry, sizeof(RTDIRENTRYEX));
                /*rc =*/ RTListAppend(&dirList, &pNode->Node);
            }
            else
                rc = VERR_NO_MEMORY;
        }
    }

    if (rc == VERR_NO_MORE_FILES)
        rc = VINF_SUCCESS;

    int rc2 = RTDirClose(pDir);
    if (RT_FAILURE(rc2))
    {
        RTMsgError("ls: Failed to close dir '%s', rc=%Rrc\n",
                   pszDir, rc2);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICETOOLBOXDIRENTRY pNodeIt;
        RTListForEach(&dirList, pNodeIt, VBOXSERVICETOOLBOXDIRENTRY, Node)
        {
            rc = VBoxServiceToolboxPrintFsInfo(pNodeIt->dirEntry.szName, pNodeIt->dirEntry.cbName,
                                               uOutputFlags,
                                               &pNodeIt->dirEntry.Info);
            if (RT_FAILURE(rc))
                break;
        }

        /* If everything went fine we do the second run (if needed) ... */
        if (   RT_SUCCESS(rc)
            && (uFlags & VBOXSERVICETOOLBOXLSFLAG_RECURSIVE))
        {
            /* Process all sub-directories. */
            RTListForEach(&dirList, pNodeIt, VBOXSERVICETOOLBOXDIRENTRY, Node)
            {
                RTFMODE fMode = pNodeIt->dirEntry.Info.Attr.fMode;
                switch (fMode & RTFS_TYPE_MASK)
                {
                    case RTFS_TYPE_SYMLINK:
                        if (!(uFlags & VBOXSERVICETOOLBOXLSFLAG_SYMLINKS))
                            break;
                        /* Fall through is intentional. */
                    case RTFS_TYPE_DIRECTORY:
                        {
                            const char *pszName = pNodeIt->dirEntry.szName;
                            if (   !RTStrICmp(pszName, ".")
                                || !RTStrICmp(pszName, ".."))
                            {
                                /* Skip dot directories. */
                                continue;
                            }

                            char szPath[RTPATH_MAX];
                            rc = RTPathJoin(szPath, sizeof(szPath),
                                            pszDir, pNodeIt->dirEntry.szName);
                            if (RT_SUCCESS(rc))
                                rc = VBoxServiceToolboxLsHandleDir(szPath,
                                                                   uFlags, uOutputFlags);
                        }
                        break;

                    default: /* Ignore the rest. */
                        break;
                }
                if (RT_FAILURE(rc))
                    break;
            }
        }
    }

    /* Clean up the mess. */
    PVBOXSERVICETOOLBOXDIRENTRY pNode, pSafe;
    RTListForEachSafe(&dirList, pNode, pSafe, VBOXSERVICETOOLBOXDIRENTRY, Node)
    {
        RTListNodeRemove(&pNode->Node);
        RTMemFree(pNode);
    }
    return rc;
}


/**
 * Main function for tool "vbox_ls".
 *
 * @return  RTEXITCODE.
 * @param   argc                    Number of arguments.
 * @param   argv                    Pointer to argument array.
 */
static RTEXITCODE VBoxServiceToolboxLs(int argc, char **argv)
{
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--machinereadable", LS_OPT_MACHINE_READABLE, RTGETOPT_REQ_NOTHING },
        { "--dereference",     'L',                     RTGETOPT_REQ_NOTHING },
        { NULL,                'l',                     RTGETOPT_REQ_NOTHING },
        { NULL,                'R',                     RTGETOPT_REQ_NOTHING },
        { "--verbose",         'v',                     RTGETOPT_REQ_NOTHING}
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv,
                 s_aOptions, RT_ELEMENTS(s_aOptions),
                 1 /*iFirst*/, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    int rc = VINF_SUCCESS;
    bool fVerbose = false;
    uint32_t fFlags = VBOXSERVICETOOLBOXLSFLAG_NONE;
    uint32_t fOutputFlags = VBOXSERVICETOOLBOXOUTPUTFLAG_NONE;

    /* Init file list. */
    RTLISTNODE fileList;
    RTListInit(&fileList);

    while (   (ch = RTGetOpt(&GetState, &ValueUnion))
              && RT_SUCCESS(rc))
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            case 'h':
                VBoxServiceToolboxShowUsage();
                return RTEXITCODE_SUCCESS;

            case 'L': /* Dereference symlinks. */
                fFlags |= VBOXSERVICETOOLBOXLSFLAG_SYMLINKS;
                break;

            case 'l': /* Print long format. */
                fOutputFlags |= VBOXSERVICETOOLBOXOUTPUTFLAG_LONG;
                break;

            case LS_OPT_MACHINE_READABLE:
                fOutputFlags |= VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE;
                break;

            case 'R': /* Recursive processing. */
                fFlags |= VBOXSERVICETOOLBOXLSFLAG_RECURSIVE;
                break;

            case 'v':
                fVerbose = true;
                break;

            case 'V':
                VBoxServiceToolboxShowVersion();
                return RTEXITCODE_SUCCESS;

            case VINF_GETOPT_NOT_OPTION:
                /* Add file(s) to buffer. This enables processing multiple files
                 * at once.
                 *
                 * Since the non-options (RTGETOPTINIT_FLAGS_OPTS_FIRST) come last when
                 * processing this loop it's safe to immediately exit on syntax errors
                 * or showing the help text (see above). */
                rc = VBoxServiceToolboxPathBufAddPathEntry(&fileList, ValueUnion.psz);
                /** @todo r=bird: Nit: creating a list here is not really
                 *        necessary since you've got one in argv that's
                 *        accessible via RTGetOpt. */
                break;

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }

    if (RT_SUCCESS(rc))
    {
        /* If not files given add current directory to list. */
        if (RTListIsEmpty(&fileList))
        {
            char szDirCur[RTPATH_MAX + 1];
            rc = RTPathGetCurrent(szDirCur, sizeof(szDirCur));
            if (RT_SUCCESS(rc))
            {
                rc = VBoxServiceToolboxPathBufAddPathEntry(&fileList, szDirCur);
                if (RT_FAILURE(rc))
                    RTMsgError("ls: Adding current directory failed, rc=%Rrc\n", rc);
            }
            else
                RTMsgError("ls: Getting current directory failed, rc=%Rrc\n", rc);
        }

        /* Print magic/version. */
        if (fOutputFlags & VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE)
            VBoxServiceToolboxPrintStrmHeader("vbt_ls", 1 /* Stream version */);

        PVBOXSERVICETOOLBOXPATHENTRY pNodeIt;
        RTListForEach(&fileList, pNodeIt, VBOXSERVICETOOLBOXPATHENTRY, Node)
        {
            /** @todo r=bird: Call RTPathQueryInfoEx directly (RTFileExists
             *        does it under the hood) and skip the RTFileOpen stuff!
             *        /bin/ls can list full details for files you cannot open.
             *
             * Just try the following:
             * @code
                      $ echo > testfile
                      $ chmod 600 testfile
                      $ sudo chown root:root testfile
                      $ ls -la testfile
             * @endcode
             */

            if (RTFileExists(pNodeIt->pszName))
            {
                RTFILE file;
                rc = RTFileOpen(&file, pNodeIt->pszName, RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_READ);
                if (RT_SUCCESS(rc))
                {
                    RTFSOBJINFO objInfo;
                    rc = RTFileQueryInfo(file, &objInfo, RTFSOBJATTRADD_UNIX);
                    if (RT_SUCCESS(rc))
                    {
                        rc = VBoxServiceToolboxPrintFsInfo(pNodeIt->pszName, strlen(pNodeIt->pszName),
                                                           fOutputFlags, &objInfo);
                    }
                    else
                        RTMsgError("ls: Unable to query information for '%s', rc=%Rrc\n",
                                   pNodeIt->pszName, rc);
                    RTFileClose(file);
                }
                else
                    RTMsgError("ls: Failed opening '%s', rc=%Rrc\n",
                               pNodeIt->pszName, rc);
            }
            else
                rc = VBoxServiceToolboxLsHandleDir(pNodeIt->pszName,
                                                   fFlags, fOutputFlags);
            if (RT_FAILURE(rc))
                RTMsgError("ls: Failed while enumerating '%s', rc=%Rrc\n",
                           pNodeIt->pszName, rc);
        }

        if (fOutputFlags & VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE) /* Output termination. */
            VBoxServiceToolboxPrintStrmTermination();
    }
    else if (fVerbose)
        RTMsgError("ls: Failed with rc=%Rrc\n", rc);

    VBoxServiceToolboxPathBufDestroy(&fileList);
    return RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


/**
 * Main function for tool "vbox_mkdir".
 *
 * @return  RTEXITCODE.
 * @param   argc                    Number of arguments.
 * @param   argv                    Pointer to argument array.
 */
static RTEXITCODE VBoxServiceToolboxMkDir(int argc, char **argv)
{
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--mode",     'm', RTGETOPT_REQ_STRING },
        { "--parents",  'p', RTGETOPT_REQ_NOTHING},
        { "--verbose",  'v', RTGETOPT_REQ_NOTHING}
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv,
                 s_aOptions, RT_ELEMENTS(s_aOptions),
                 1 /*iFirst*/, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    int rc = VINF_SUCCESS;
    bool fMakeParentDirs = false;
    bool fVerbose = false;

    RTFMODE newMode = 0;
    RTFMODE dirMode = RTFS_UNIX_IRWXU | RTFS_UNIX_IRWXG | RTFS_UNIX_IRWXO;

    /* Init directory list. */
    RTLISTNODE dirList;
    RTListInit(&dirList);

    while (   (ch = RTGetOpt(&GetState, &ValueUnion))
              && RT_SUCCESS(rc))
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            case 'h':
                VBoxServiceToolboxShowUsage();
                return RTEXITCODE_SUCCESS;

            case 'p':
                fMakeParentDirs = true;
                break;

            case 'm':
                rc = RTStrToUInt32Ex(ValueUnion.psz, NULL, 8 /* Base */, &newMode);
                if (RT_FAILURE(rc)) /* Only octet based values supported right now! */
                {
                    RTMsgError("mkdir: Mode flag strings not implemented yet! Use octal numbers instead.\n");
                    return RTEXITCODE_SYNTAX;
                }
                break;

            case 'v':
                fVerbose = true;
                break;

            case 'V':
                VBoxServiceToolboxShowVersion();
                return RTEXITCODE_SUCCESS;

            case VINF_GETOPT_NOT_OPTION:
               /* Add path(s) to buffer. This enables processing multiple paths
                * at once.
                *
                * Since the non-options (RTGETOPTINIT_FLAGS_OPTS_FIRST) come last when
                * processing this loop it's safe to immediately exit on syntax errors
                * or showing the help text (see above). */
               rc = VBoxServiceToolboxPathBufAddPathEntry(&dirList, ValueUnion.psz);
               break;


            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }

    if (RT_SUCCESS(rc))
    {
        if (fMakeParentDirs || newMode)
        {
#ifndef RT_OS_WINDOWS
            mode_t umaskMode = umask(0); /* Get current umask. */
            if (newMode)
                dirMode = newMode;
#endif
        }

        PVBOXSERVICETOOLBOXPATHENTRY pNodeIt;
        RTListForEach(&dirList, pNodeIt, VBOXSERVICETOOLBOXPATHENTRY, Node)
        {
            rc = fMakeParentDirs
               ? RTDirCreateFullPath(pNodeIt->pszName, dirMode)
               : RTDirCreate(pNodeIt->pszName, dirMode);

            if (RT_SUCCESS(rc) && fVerbose)
                RTMsgError("mkdir: Created directory 's', mode %#RTfmode\n", pNodeIt->pszName, dirMode);
            else if (RT_FAILURE(rc)) /** @todo Add a switch with more helpful error texts! */
            {
                RTMsgError("mkdir: Could not create directory '%s': %Rra\n", pNodeIt->pszName, rc);
                break;
            }
        }
    }
    else if (fVerbose)
        RTMsgError("mkdir: Failed with rc=%Rrc\n", rc);

    VBoxServiceToolboxPathBufDestroy(&dirList);
    return RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


/**
 * Main function for tool "vbox_stat".
 *
 * @return  RTEXITCODE.
 * @param   argc                    Number of arguments.
 * @param   argv                    Pointer to argument array.
 */
static RTEXITCODE VBoxServiceToolboxStat(int argc, char **argv)
{
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--file-system",     'f',                       RTGETOPT_REQ_NOTHING },
        { "--dereference",     'L',                       RTGETOPT_REQ_NOTHING },
        { "--machinereadable", STAT_OPT_MACHINE_READABLE, RTGETOPT_REQ_NOTHING },
        { "--terse",           't',                       RTGETOPT_REQ_NOTHING },
        { "--verbose",         'v',                       RTGETOPT_REQ_NOTHING }
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv,
                 s_aOptions, RT_ELEMENTS(s_aOptions),
                 1 /*iFirst*/, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    int rc = VINF_SUCCESS;
    bool fVerbose = false;
    uint32_t fOutputFlags = VBOXSERVICETOOLBOXOUTPUTFLAG_LONG; /* Use long mode by default. */

    /* Init file list. */
    RTLISTNODE fileList;
    RTListInit(&fileList);

    while (   (ch = RTGetOpt(&GetState, &ValueUnion))
              && RT_SUCCESS(rc))
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            case 'h':
                VBoxServiceToolboxShowUsage();
                return RTEXITCODE_SUCCESS;

            case 'f':
            case 'L':
                RTMsgError("stat: Sorry, option '%s' is not implemented yet!\n",
                           ValueUnion.pDef->pszLong);
                rc = VERR_INVALID_PARAMETER;
                break;

            case LS_OPT_MACHINE_READABLE:
                fOutputFlags |= VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE;
                break;

            case 'v':
                fVerbose = true;
                break;

            case 'V':
                VBoxServiceToolboxShowVersion();
                return RTEXITCODE_SUCCESS;

            case VINF_GETOPT_NOT_OPTION:
                {
                    /* Add file(s) to buffer. This enables processing multiple files
                     * at once.
                     *
                     * Since the non-options (RTGETOPTINIT_FLAGS_OPTS_FIRST) come last when
                     * processing this loop it's safe to immediately exit on syntax errors
                     * or showing the help text (see above). */
                    rc = VBoxServiceToolboxPathBufAddPathEntry(&fileList, ValueUnion.psz);
                    break;
                }

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }

    if (RT_SUCCESS(rc))
    {
        if (fOutputFlags & VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE) /* Output termination. */
            VBoxServiceToolboxPrintStrmHeader("vbt_stat", 1 /* Stream version */);

        PVBOXSERVICETOOLBOXPATHENTRY pNodeIt;
        RTListForEach(&fileList, pNodeIt, VBOXSERVICETOOLBOXPATHENTRY, Node)
        {
            /** @todo r=bird: Use RTPathQueryInfoEx!  The tool is called
             *        'stat' because it's just a wrapper for 'stat()',
             *        right?  Our equvialent to the 'stat()' call is
             *        RTPathQueryInfoEx().  Same permission issues as above
             *        with 'ls'. */
            /* Only check for file existence for now. */
            RTFSOBJINFO objInfo;
            int rc2 = VINF_SUCCESS; /* Temporary rc to keep original rc. */
            if (RTFileExists(pNodeIt->pszName))
            {
                RTFILE file;
                rc2 = RTFileOpen(&file, pNodeIt->pszName, RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_READ);
                if (RT_SUCCESS(rc2))
                {
                    rc2 = RTFileQueryInfo(file, &objInfo, RTFSOBJATTRADD_UNIX);
                    if (RT_FAILURE(rc2))
                        RTMsgError("stat: Unable to query information for '%s', rc=%Rrc\n",
                                   pNodeIt->pszName, rc2);
                    RTFileClose(file);
                }
                else
                    RTMsgError("stat: Failed opening '%s', rc=%Rrc\n",
                               pNodeIt->pszName, rc2);

                if (RT_FAILURE(rc2))
                    rc = rc2;
            }
            else if (RTDirExists(pNodeIt->pszName))
            {
                PRTDIR pDir;
                rc2 = RTDirOpen(&pDir, pNodeIt->pszName);
                if (RT_SUCCESS(rc2))
                {
                    rc2 = RTDirQueryInfo(pDir, &objInfo, RTFSOBJATTRADD_UNIX);
                    RTDirClose(pDir);
                }
            }
            else
            {
                RTMsgError("stat: Cannot stat for '%s': No such file or directory\n",
                           pNodeIt->pszName);
                rc = VERR_FILE_NOT_FOUND;
                /* Do not break here -- process every element in the list
                 * and keep failing rc. */
            }

            if (RT_SUCCESS(rc2))
            {
                rc2 = VBoxServiceToolboxPrintFsInfo(pNodeIt->pszName,
                                                    strlen(pNodeIt->pszName) /* cbName */,
                                                    fOutputFlags,
                                                    &objInfo);
                if (RT_FAILURE(rc2))
                    rc = rc2;
            }
        }

        if (fOutputFlags & VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE) /* Output termination. */
            VBoxServiceToolboxPrintStrmTermination();

        /* At this point the overall result (success/failure) should be in rc. */

        if (RTListIsEmpty(&fileList))
            RTMsgError("stat: Missing operand\n");
    }
    else if (fVerbose)
        RTMsgError("stat: Failed with rc=%Rrc\n", rc);

    VBoxServiceToolboxPathBufDestroy(&fileList);
    return RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


/**
 * Entry point for internal toolbox.
 *
 * @return  True if an internal tool was handled, false if not.
 * @param   argc                    Number of arguments.
 * @param   argv                    Pointer to argument array.
 * @param   prcExit                 Where to store the exit code when an
 *                                  internal toolbox command was handled.
 */
bool VBoxServiceToolboxMain(int argc, char **argv, RTEXITCODE *prcExit)
{
    static struct
    {
        const char *pszName;
        RTEXITCODE (*pfnHandler)(int argc, char **argv);
    }
    const s_aTools[] =
    {
        { "cat",    VBoxServiceToolboxCat   },
        { "ls",     VBoxServiceToolboxLs    },
        { "mkdir",  VBoxServiceToolboxMkDir },
        { "stat",   VBoxServiceToolboxStat  },
    };

    /*
     * Check if the file named in argv[0] is one of the toolbox programs.
     */
    AssertReturn(argc > 0, false);
    const char *pszTool = RTPathFilename(argv[0]);
    /* Skip optional 'vbox_' prefix. */
    if (   pszTool[0] == 'v'
        && pszTool[1] == 'b'
        && pszTool[2] == 'o'
        && pszTool[3] == 'x'
        && pszTool[4] == '_')
        pszTool += 5;

    for (unsigned i = 0; i < RT_ELEMENTS(s_aTools); i++)
        if (!strcmp(s_aTools[i].pszName, pszTool))
        {
            *prcExit = s_aTools[i].pfnHandler(argc, argv);
            return true;
        }

    /*
     * For debugging and testing purposes we also allow toolbox program access
     * when the first VBoxService argument is --use-toolbox.
     */
    if (argc >= 3 && !strcmp(argv[1], "--use-toolbox"))
    {
        pszTool = argv[2];
        for (unsigned i = 0; i < RT_ELEMENTS(s_aTools); i++)
            if (!strcmp(s_aTools[i].pszName, pszTool))
            {
                *prcExit = s_aTools[i].pfnHandler(argc - 2, argv + 2);
                return true;
            }

       *prcExit = RTMsgErrorExit(RTEXITCODE_SYNTAX, "Toolbox program '%s' does not exist", pszTool);
       return true;
    }

    return false;
}

