/* $Id$ */
/** @file
 * VBoxServiceToolbox - Internal (BusyBox-like) toolbox.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
#include <iprt/symlink.h>

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

/** Generic option indices for commands. */
enum
{
    VBOXSERVICETOOLBOXOPT_MACHINE_READABLE = 1000,
    VBOXSERVICETOOLBOXOPT_VERBOSE
};

/** Options indices for "vbox_cat". */
typedef enum VBOXSERVICETOOLBOXCATOPT
{
    VBOXSERVICETOOLBOXCATOPT_NO_CONTENT_INDEXED = 1000
} VBOXSERVICETOOLBOXCATOPT;

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
/** Pointer to a handler function. */
typedef RTEXITCODE (*PFNHANDLER)(int , char **);

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
 * Displays a common header for all help text to stdout.
 */
static void VBoxServiceToolboxShowUsageHeader(void)
{
    RTPrintf(VBOX_PRODUCT " Guest Toolbox Version "
             VBOX_VERSION_STRING "\n"
             "(C) " VBOX_C_YEAR " " VBOX_VENDOR "\n"
             "All rights reserved.\n"
             "\n");
    RTPrintf("Usage:\n\n");
}


/**
 * Displays a help text to stdout.
 */
static void VBoxServiceToolboxShowUsage(void)
{
    VBoxServiceToolboxShowUsageHeader();
    RTPrintf("  VBoxService [--use-toolbox] vbox_<command> <general options> <parameters>\n\n"
             "General options:\n\n"
             "  --machinereadable          produce all output in machine-readable form\n"
             "  -V                         print version number and exit\n"
             "\n"
             "Commands:\n\n"
             "  cat <general options>      <file>...\n"
             "  ls <general options>       [--dereference|-L] [-l] [-R]\n"
             "                             [--verbose|-v] [<file>...]\n"
             "  rm <general options>       [-r|-R] <file>...\n"
             "  mktemp <general options>   <template>\n"
             "  mkdir <general options>    [--mode|-m] [--parents|-p]\n"
             "                             [--verbose|-v] <directory>...\n"
             "  stat <general options>     [--file-system|-f]\n"
             "                             [--dereference|-L] [--terse|-t]\n"
             "                             [--verbose|-v] <file>...\n"
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
 * Initializes the parseable stream(s).
 *
 * @return  IPRT status code.
 */
static int VBoxServiceToolboxStrmInit(void)
{
    /* Set stdout's mode to binary. This is required for outputting all the machine-readable
     * data correctly. */
    int rc = RTStrmSetMode(g_pStdOut, 1 /* Binary mode */, -1 /* Current code set, not changed */);
    if (RT_FAILURE(rc))
        RTMsgError("Unable to set stdout to binary mode, rc=%Rrc\n", rc);

    return rc;
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
            RTMsgError("Could not translate input file to native handle, rc=%Rrc\n", rc);
    }

    if (hOutput == NIL_RTFILE)
    {
        rc = RTFileFromNative(&hOutput, RTFILE_NATIVE_STDOUT);
        if (RT_FAILURE(rc))
            RTMsgError("Could not translate output file to native handle, rc=%Rrc\n", rc);
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
                if (RT_FAILURE(rc))
                {
                    RTMsgError("Error while writing output, rc=%Rrc\n", rc);
                    break;
                }
            }
            else
            {
                if (rc == VERR_BROKEN_PIPE)
                    rc = VINF_SUCCESS;
                else if (RT_FAILURE(rc))
                    RTMsgError("Error while reading input, rc=%Rrc\n", rc);
                break;
            }
        }
    }
    return rc;
}


/** @todo Document options! */
static char g_paszCatHelp[] =
    "  VBoxService [--use-toolbox] vbox_cat <general options> <file>...\n\n"
    "Concatenate files, or standard input, to standard output.\n"
    "\n";


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
        { "--show-all",            'a',                                           RTGETOPT_REQ_NOTHING },
        { "--number-nonblank",     'b',                                           RTGETOPT_REQ_NOTHING},
        { NULL,                    'e',                                           RTGETOPT_REQ_NOTHING},
        { NULL,                    'E',                                           RTGETOPT_REQ_NOTHING},
        { "--flags",               'f',                                           RTGETOPT_REQ_STRING},
        { "--no-content-indexed",  VBOXSERVICETOOLBOXCATOPT_NO_CONTENT_INDEXED,   RTGETOPT_REQ_NOTHING},
        { "--number",              'n',                                           RTGETOPT_REQ_NOTHING},
        { "--output",              'o',                                           RTGETOPT_REQ_STRING},
        { "--squeeze-blank",       's',                                           RTGETOPT_REQ_NOTHING},
        { NULL,                    't',                                           RTGETOPT_REQ_NOTHING},
        { "--show-tabs",           'T',                                           RTGETOPT_REQ_NOTHING},
        { NULL,                    'u',                                           RTGETOPT_REQ_NOTHING},
        { "--show-noneprinting",   'v',                                           RTGETOPT_REQ_NOTHING}
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
    RTLISTANCHOR inputList;
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
                RTMsgError("Sorry, option '%s' is not implemented yet!\n",
                           ValueUnion.pDef->pszLong);
                rc = VERR_INVALID_PARAMETER;
                break;

            case 'h':
                VBoxServiceToolboxShowUsageHeader();
                RTPrintf("%s", g_paszCatHelp);
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

            case VBOXSERVICETOOLBOXCATOPT_NO_CONTENT_INDEXED:
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
                RTMsgError("Could not create output file '%s', rc=%Rrc\n",
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
                        RTMsgError("Could not open input file '%s': %s\n",
                                   pNodeIt->pszName, pMsg->pszMsgFull);
                    else
                        RTMsgError("Could not open input file '%s', rc=%Rrc\n", pNodeIt->pszName, rc);
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
        if (!(uOutputFlags & VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE))
            RTMsgError("Failed to retrieve absolute path of '%s', rc=%Rrc\n", pszDir, rc);
        return rc;
    }

    PRTDIR pDir;
    rc = RTDirOpen(&pDir, szPathAbs);
    if (RT_FAILURE(rc))
    {
        if (!(uOutputFlags & VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE))
            RTMsgError("Failed to open directory '%s', rc=%Rrc\n", szPathAbs, rc);
        return rc;
    }

    RTLISTANCHOR dirList;
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
        if (!(uOutputFlags & VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE))
            RTMsgError("Failed to close dir '%s', rc=%Rrc\n",
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


/** @todo Document options! */
static char g_paszLsHelp[] =
    "  VBoxService [--use-toolbox] vbox_ls <general options> [option]...\n"
    "                                      [<file>...]\n\n"
    "List information about files (the current directory by default).\n\n"
    "Options:\n\n"
    "  [--dereference|-L]\n"
    "  [-l][-R]\n"
    "  [--verbose|-v]\n"
    "  [<file>...]\n"
    "\n";


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
        { "--machinereadable", VBOXSERVICETOOLBOXOPT_MACHINE_READABLE,      RTGETOPT_REQ_NOTHING },
        { "--dereference",     'L',                                           RTGETOPT_REQ_NOTHING },
        { NULL,                'l',                                           RTGETOPT_REQ_NOTHING },
        { NULL,                'R',                                           RTGETOPT_REQ_NOTHING },
        { "--verbose",         VBOXSERVICETOOLBOXOPT_VERBOSE,               RTGETOPT_REQ_NOTHING}
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    int rc = RTGetOptInit(&GetState, argc, argv,
                          s_aOptions, RT_ELEMENTS(s_aOptions),
                          1 /*iFirst*/, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertRCReturn(rc, RTEXITCODE_INIT);

    bool     fVerbose     = false;
    uint32_t fFlags       = VBOXSERVICETOOLBOXLSFLAG_NONE;
    uint32_t fOutputFlags = VBOXSERVICETOOLBOXOUTPUTFLAG_NONE;

    /* Init file list. */
    RTLISTANCHOR fileList;
    RTListInit(&fileList);

    while (   (ch = RTGetOpt(&GetState, &ValueUnion))
              && RT_SUCCESS(rc))
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            case 'h':
                VBoxServiceToolboxShowUsageHeader();
                RTPrintf("%s", g_paszLsHelp);
                return RTEXITCODE_SUCCESS;

            case 'L': /* Dereference symlinks. */
                fFlags |= VBOXSERVICETOOLBOXLSFLAG_SYMLINKS;
                break;

            case 'l': /* Print long format. */
                fOutputFlags |= VBOXSERVICETOOLBOXOUTPUTFLAG_LONG;
                break;

            case VBOXSERVICETOOLBOXOPT_MACHINE_READABLE:
                fOutputFlags |= VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE;
                break;

            case 'R': /* Recursive processing. */
                fFlags |= VBOXSERVICETOOLBOXLSFLAG_RECURSIVE;
                break;

            case VBOXSERVICETOOLBOXOPT_VERBOSE:
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
                    RTMsgError("Adding current directory failed, rc=%Rrc\n", rc);
            }
            else
                RTMsgError("Getting current directory failed, rc=%Rrc\n", rc);
        }

        /* Print magic/version. */
        if (fOutputFlags & VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE)
        {
            rc = VBoxServiceToolboxStrmInit();
            if (RT_FAILURE(rc))
                RTMsgError("Error while initializing parseable streams, rc=%Rrc\n", rc);
            VBoxServiceToolboxPrintStrmHeader("vbt_ls", 1 /* Stream version */);
        }

        PVBOXSERVICETOOLBOXPATHENTRY pNodeIt;
        RTListForEach(&fileList, pNodeIt, VBOXSERVICETOOLBOXPATHENTRY, Node)
        {
            if (RTFileExists(pNodeIt->pszName))
            {
                RTFSOBJINFO objInfo;
                int rc2 = RTPathQueryInfoEx(pNodeIt->pszName, &objInfo,
                                            RTFSOBJATTRADD_UNIX, RTPATH_F_ON_LINK /* @todo Follow link? */);
                if (RT_FAILURE(rc2))
                {
                    if (!(fOutputFlags & VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE))
                        RTMsgError("Cannot access '%s': No such file or directory\n",
                                   pNodeIt->pszName);
                    rc = VERR_FILE_NOT_FOUND;
                    /* Do not break here -- process every element in the list
                     * and keep failing rc. */
                }
                else
                {
                    rc2 = VBoxServiceToolboxPrintFsInfo(pNodeIt->pszName,
                                                        strlen(pNodeIt->pszName) /* cbName */,
                                                        fOutputFlags,
                                                        &objInfo);
                    if (RT_FAILURE(rc2))
                        rc = rc2;
                }
            }
            else
            {
                int rc2 = VBoxServiceToolboxLsHandleDir(pNodeIt->pszName,
                                                        fFlags, fOutputFlags);
                if (RT_FAILURE(rc2))
                    rc = rc2;
            }
        }

        if (fOutputFlags & VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE) /* Output termination. */
            VBoxServiceToolboxPrintStrmTermination();
    }
    else if (fVerbose)
        RTMsgError("Failed with rc=%Rrc\n", rc);

    VBoxServiceToolboxPathBufDestroy(&fileList);
    return RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


static char g_paszRmHelp[] =
    "  VBoxService [--use-toolbox] vbox_rm <general options> <file>...\n\n"
    "Delete files and optionally directories if the '-R' or '-r' option is specified.\n"
    "If a file or directory cannot be deleted, an error message is printed if the\n"
    "'--machine-readable' option is not specified and the next file will be\n"
    "processed. The root directory is always ignored.\n\n"
    "Options:\n\n"
    "  [-R|-r]                    Recursively delete directories too.\n"
    "\n";


/**
 * Report the result of a vbox_rm operation - either errors to stderr (not
 * machine-readable) or everything to stdout as <name>\0<rc>\0 (machine-
 * readable format).  The message may optionally contain a '%s' for the file
 * name and an %Rrc for the result code in that order.  In future a "verbose"
 * flag may be added, without which nothing will be output in non-machine-
 * readable mode.  Sets prc if rc is a non-success code.
 */
static void toolboxRmReport(const char *pcszMessage, const char *pcszFile,
                            bool fActive, int rc, uint32_t fOutputFlags,
                            int *prc)
{
    if (!fActive)
        return;
    if (!(fOutputFlags & VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE))
    {
        if (RT_SUCCESS(rc))
            RTPrintf(pcszMessage, pcszFile, rc);
        else
            RTMsgError(pcszMessage, pcszFile, rc);
    }
    else
        RTPrintf("fname=%s%crc=%d%c", pcszFile, 0, rc, 0);
    if (prc && RT_FAILURE(rc))
        *prc = rc;
}


/**
 * Main function for tool "vbox_rm".
 *
 * @return  RTEXITCODE.
 * @param   argc                    Number of arguments.
 * @param   argv                    Pointer to argument array.
 */
static RTEXITCODE VBoxServiceToolboxRm(int argc, char **argv)
{
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--machinereadable", VBOXSERVICETOOLBOXOPT_MACHINE_READABLE,
          RTGETOPT_REQ_NOTHING },
        /* Be like POSIX, which has both 'r' and 'R'. */
        { NULL,                'r',
          RTGETOPT_REQ_NOTHING },
        { NULL,                'R',
          RTGETOPT_REQ_NOTHING },
    };

    enum
    {
        VBOXSERVICETOOLBOXRMFLAG_RECURSIVE = RT_BIT_32(0)
    };

    int ch, rc, rc2;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    rc = RTGetOptInit(&GetState, argc, argv, s_aOptions,
                      RT_ELEMENTS(s_aOptions), 1 /*iFirst*/,
                      RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertRCReturn(rc, RTEXITCODE_INIT);

    bool     fVerbose     = false;
    uint32_t fFlags       = 0;
    uint32_t fOutputFlags = 0;
    int cNonOptions       = 0;

    while (   (ch = RTGetOpt(&GetState, &ValueUnion))
              && RT_SUCCESS(rc))
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            case 'h':
                VBoxServiceToolboxShowUsageHeader();
                RTPrintf("%s", g_paszRmHelp);
                return RTEXITCODE_SUCCESS;

            case 'V':
                VBoxServiceToolboxShowVersion();
                return RTEXITCODE_SUCCESS;

            case VBOXSERVICETOOLBOXOPT_MACHINE_READABLE:
                fOutputFlags |= VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE;
                break;

            case 'r':
            case 'R': /* Allow directories too. */
                fFlags |= VBOXSERVICETOOLBOXRMFLAG_RECURSIVE;
                break;

            case VINF_GETOPT_NOT_OPTION:
                /* RTGetOpt will sort these to the end of the argv vector so
                 * that we will deal with them afterwards. */
                ++cNonOptions;
                break;

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }
    if (RT_SUCCESS(rc))
    {
        /* Print magic/version. */
        if (fOutputFlags & VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE)
        {
            rc = VBoxServiceToolboxStrmInit();
            if (RT_FAILURE(rc))
                RTMsgError("Error while initializing parseable streams, rc=%Rrc\n", rc);
            VBoxServiceToolboxPrintStrmHeader("vbt_rm", 1 /* Stream version */);
        }
    }

    /* We need at least one file. */
    if (RT_SUCCESS(rc) && cNonOptions == 0)
    {
        toolboxRmReport("No files or directories specified.\n", NULL, true, 0,
                        fOutputFlags, NULL);
        return RTEXITCODE_FAILURE;
    }
    if (RT_SUCCESS(rc))
    {
        for (int i = argc - cNonOptions; i < argc; ++i)
        {
            /* I'm sure this isn't the most effective way, but I hope it will
             * be readable and reliable code. */
            if (RTDirExists(argv[i]) && !RTSymlinkExists(argv[i]))
            {
                if (!(fFlags & VBOXSERVICETOOLBOXRMFLAG_RECURSIVE))
                    toolboxRmReport("Cannot remove directory '%s' as the '-R' option was not specified.\n",
                                    argv[i], true, VERR_INVALID_PARAMETER,
                                    fOutputFlags, &rc);
                else
                {
                    rc2 = RTDirRemoveRecursive(argv[i],
                                               RTDIRRMREC_F_CONTENT_AND_DIR);
                    toolboxRmReport("", argv[i], RT_SUCCESS(rc2), rc2,
                                    fOutputFlags, NULL);
                    toolboxRmReport("The following error occurred while removing directory '%s': %Rrc.\n",
                                    argv[i], RT_FAILURE(rc2), rc2,
                                    fOutputFlags, &rc);
                }
            }
            else if (RTPathExists(argv[i]) || RTSymlinkExists(argv[i]))
            {
                rc2 = RTFileDelete(argv[i]);
                toolboxRmReport("", argv[i], RT_SUCCESS(rc2), rc2,
                                fOutputFlags, NULL);
                toolboxRmReport("The following error occurred while removing file '%s': %Rrc.\n",
                                argv[i], RT_FAILURE(rc2), rc2, fOutputFlags,
                                &rc);
            }
            else
                toolboxRmReport("File '%s' does not exist.\n", argv[i],
                                true, VERR_FILE_NOT_FOUND, fOutputFlags, &rc);
        }

        if (fOutputFlags & VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE) /* Output termination. */
            VBoxServiceToolboxPrintStrmTermination();
    }
    return RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


static char g_paszMkTempHelp[] =
    "  VBoxService [--use-toolbox] vbox_mktemp <general options> <template>\n\n"
    "Create a temporary directory based on the template supplied. The last string\n"
    "of consecutive 'X' characters in the template will be replaced to form a unique\n"
    "name for the directory.\n"
    "\n";


/**
 * Report the result of a vbox_mktemp operation - either errors to stderr (not
 * machine-readable) or everything to stdout as <name>\0<rc>\0 (machine-
 * readable format).  The message may optionally contain a '%s' for the file
 * name and an %Rrc for the result code in that order.  In future a "verbose"
 * flag may be added, without which nothing will be output in non-machine-
 * readable mode.  Sets prc if rc is a non-success code.
 */
static void toolboxMkTempReport(const char *pcszMessage, const char *pcszFile,
                                bool fActive, int rc, uint32_t fOutputFlags,
                                int *prc)
{
    if (!fActive)
        return;
    if (!(fOutputFlags & VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE))
        if (RT_SUCCESS(rc))
            RTPrintf(pcszMessage, pcszFile, rc);
        else
            RTMsgError(pcszMessage, pcszFile, rc);
    else
        RTPrintf("name=%s%crc=%d%c", pcszFile, 0, rc, 0);
    if (prc && RT_FAILURE(rc))
        *prc = rc;
}


/**
 * Main function for tool "vbox_mktemp".
 *
 * @return  RTEXITCODE.
 * @param   argc                    Number of arguments.
 * @param   argv                    Pointer to argument array.
 */
static RTEXITCODE VBoxServiceToolboxMkTemp(int argc, char **argv)
{
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--machinereadable", VBOXSERVICETOOLBOXOPT_MACHINE_READABLE,
          RTGETOPT_REQ_NOTHING },
    };

    int ch, rc, rc2;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    rc = RTGetOptInit(&GetState, argc, argv, s_aOptions,
                      RT_ELEMENTS(s_aOptions), 1 /*iFirst*/,
                      RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertRCReturn(rc, RTEXITCODE_INIT);

    bool     fVerbose     = false;
    uint32_t fFlags       = 0;
    uint32_t fOutputFlags = 0;
    int cNonOptions       = 0;
    char *pszName;

    while (   (ch = RTGetOpt(&GetState, &ValueUnion))
              && RT_SUCCESS(rc))
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            case 'h':
                VBoxServiceToolboxShowUsageHeader();
                RTPrintf("%s", g_paszMkTempHelp);
                return RTEXITCODE_SUCCESS;

            case 'V':
                VBoxServiceToolboxShowVersion();
                return RTEXITCODE_SUCCESS;

            case VBOXSERVICETOOLBOXOPT_MACHINE_READABLE:
                fOutputFlags |= VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE;
                break;

            case VINF_GETOPT_NOT_OPTION:
                /* RTGetOpt will sort these to the end of the argv vector so
                 * that we will deal with them afterwards. */
                ++cNonOptions;
                break;

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }
    if (RT_SUCCESS(rc))
    {
        /* Print magic/version. */
        if (fOutputFlags & VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE)
        {
            rc = VBoxServiceToolboxStrmInit();
            if (RT_FAILURE(rc))
                RTMsgError("Error while initializing parseable streams, rc=%Rrc\n", rc);
            VBoxServiceToolboxPrintStrmHeader("vbt_mktemp", 1 /* Stream version */);
        }
    }

    /* We need exactly one template, containing at least one 'X'. */
    if (RT_SUCCESS(rc) && cNonOptions != 1)
    {
        toolboxMkTempReport("Please specify exactly one template.\n", "",
                            true, VERR_INVALID_PARAMETER, fOutputFlags, &rc);
        return RTEXITCODE_FAILURE;
    }
    pszName = argv[argc - 1];
    if (RT_SUCCESS(rc) && !strchr(pszName, 'X'))  /* IPRT asserts this. */
    {
        toolboxMkTempReport("Template '%s' should contain at least one 'X' character.\n",
                            pszName, true, VERR_INVALID_PARAMETER,
                            fOutputFlags, &rc);
        return RTEXITCODE_FAILURE;
    }

    if (RT_SUCCESS(rc))
    {
        rc = RTDirCreateTemp(pszName, 0700);
        toolboxMkTempReport("Created temporary directory '%s'.\n",
                            pszName, RT_SUCCESS(rc), rc, fOutputFlags, NULL);
        toolboxMkTempReport("The following error occurred while creating a temporary directory with template '%s': %Rrc.\n",
                            pszName, RT_FAILURE(rc), rc, fOutputFlags, NULL);
        if (fOutputFlags & VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE) /* Output termination. */
            VBoxServiceToolboxPrintStrmTermination();
    }
    return RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


/** @todo Document options! */
static char g_paszMkDirHelp[] =
    "  VBoxService [--use-toolbox] vbox_mkdir <general options> [options]\n"
    "                                         <directory>...\n\n"
    "Options:\n\n"
    "  [--mode=<mode>|-m <mode>]  The file mode to set (chmod) on the created\n"
    "                             directories.  Default: a=rwx & umask.\n"
    "  [--parents|-p]             Create parent directories as needed, no\n"
    "                             error if the directory already exists.\n"
    "  [--verbose|-v]             Display a message for each created directory.\n"
    "\n";


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
    int rc = RTGetOptInit(&GetState, argc, argv,
                          s_aOptions, RT_ELEMENTS(s_aOptions),
                          1 /*iFirst*/, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertRCReturn(rc, RTEXITCODE_INIT);

    bool    fMakeParentDirs = false;
    bool    fVerbose        = false;
    RTFMODE fDirMode        = RTFS_UNIX_IRWXU | RTFS_UNIX_IRWXG | RTFS_UNIX_IRWXO;
    int     cDirsCreated    = 0;

    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            case 'p':
                fMakeParentDirs = true;
#ifndef RT_OS_WINDOWS
                umask(0); /* RTDirCreate workaround */
#endif
                break;

            case 'm':
                rc = RTStrToUInt32Ex(ValueUnion.psz, NULL, 8 /* Base */, &fDirMode);
                if (RT_FAILURE(rc)) /* Only octet based values supported right now! */
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX,
                                          "Mode flag strings not implemented yet! Use octal numbers instead. (%s)\n",
                                          ValueUnion.psz);
#ifndef RT_OS_WINDOWS
                umask(0); /* RTDirCreate workaround */
#endif
                break;

            case 'v':
                fVerbose = true;
                break;

            case 'h':
                VBoxServiceToolboxShowUsageHeader();
                RTPrintf("%s", g_paszMkDirHelp);
                return RTEXITCODE_SUCCESS;

            case 'V':
                VBoxServiceToolboxShowVersion();
                return RTEXITCODE_SUCCESS;

            case VINF_GETOPT_NOT_OPTION:
                if (fMakeParentDirs)
                    /** @todo r=bird: If fVerbose is set, we should also show
                     * which directories that get created, parents as well as
                     * omitting existing final dirs. Annoying, but check any
                     * mkdir implementation (try "mkdir -pv asdf/1/2/3/4"
                     * twice). */
                    rc = RTDirCreateFullPath(ValueUnion.psz, fDirMode);
                else
                    rc = RTDirCreate(ValueUnion.psz, fDirMode, 0);
                if (RT_FAILURE(rc))
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Could not create directory '%s': %Rra\n",
                                          ValueUnion.psz, rc);
                if (fVerbose)
                    RTMsgInfo("Created directory '%s', mode %#RTfmode\n", ValueUnion.psz, fDirMode);
                cDirsCreated++;
                break;

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }
    AssertRC(rc);

    if (cDirsCreated == 0)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "No directory argument.");

    return RTEXITCODE_SUCCESS;
}


/** @todo Document options! */
static char g_paszStatHelp[] =
    "  VBoxService [--use-toolbox] vbox_stat <general options> [options] <file>...\n\n"
    "Display file or file system status.\n\n"
    "Options:\n\n"
    "  [--file-system|-f]\n"
    "  [--dereference|-L]\n"
    "  [--terse|-t]\n"
    "  [--verbose|-v]\n"
    "\n";


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
        { "--file-system",     'f',                                          RTGETOPT_REQ_NOTHING },
        { "--dereference",     'L',                                          RTGETOPT_REQ_NOTHING },
        { "--machinereadable", VBOXSERVICETOOLBOXOPT_MACHINE_READABLE,     RTGETOPT_REQ_NOTHING },
        { "--terse",           't',                                          RTGETOPT_REQ_NOTHING },
        { "--verbose",         'v',                                          RTGETOPT_REQ_NOTHING }
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
    RTLISTANCHOR fileList;
    RTListInit(&fileList);

    while (   (ch = RTGetOpt(&GetState, &ValueUnion))
              && RT_SUCCESS(rc))
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            case 'f':
            case 'L':
                RTMsgError("Sorry, option '%s' is not implemented yet!\n", ValueUnion.pDef->pszLong);
                rc = VERR_INVALID_PARAMETER;
                break;

            case VBOXSERVICETOOLBOXOPT_MACHINE_READABLE:
                fOutputFlags |= VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE;
                break;

            case 'v':   /** @todo r=bird: There is no verbose option for stat. */
                fVerbose = true;
                break;

            case 'h':
                VBoxServiceToolboxShowUsageHeader();
                RTPrintf("%s", g_paszStatHelp);
                return RTEXITCODE_SUCCESS;

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
        {
            rc = VBoxServiceToolboxStrmInit();
            if (RT_FAILURE(rc))
                RTMsgError("Error while initializing parseable streams, rc=%Rrc\n", rc);
            VBoxServiceToolboxPrintStrmHeader("vbt_stat", 1 /* Stream version */);
        }

        PVBOXSERVICETOOLBOXPATHENTRY pNodeIt;
        RTListForEach(&fileList, pNodeIt, VBOXSERVICETOOLBOXPATHENTRY, Node)
        {
            RTFSOBJINFO objInfo;
            int rc2 = RTPathQueryInfoEx(pNodeIt->pszName, &objInfo,
                                        RTFSOBJATTRADD_UNIX, RTPATH_F_ON_LINK /* @todo Follow link? */);
            if (RT_FAILURE(rc2))
            {
                if (!(fOutputFlags & VBOXSERVICETOOLBOXOUTPUTFLAG_PARSEABLE))
                    RTMsgError("Cannot stat for '%s': No such file or directory\n",
                               pNodeIt->pszName);
                rc = VERR_FILE_NOT_FOUND;
                /* Do not break here -- process every element in the list
                 * and keep failing rc. */
            }
            else
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
            RTMsgError("Missing operand\n");
    }
    else if (fVerbose)
        RTMsgError("Failed with rc=%Rrc\n", rc);

    VBoxServiceToolboxPathBufDestroy(&fileList);
    return RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}



/**
 * Looks up the handler for the tool give by @a pszTool.
 *
 * @returns Pointer to handler function.  NULL if not found.
 * @param   pszTool     The name of the tool.
 */
static PFNHANDLER vboxServiceToolboxLookUpHandler(const char *pszTool)
{
    static struct
    {
        const char *pszName;
        RTEXITCODE (*pfnHandler)(int argc, char **argv);
    }
    const s_aTools[] =
    {
        { "cat",    VBoxServiceToolboxCat    },
        { "ls",     VBoxServiceToolboxLs     },
        { "rm",     VBoxServiceToolboxRm     },
        { "mktemp", VBoxServiceToolboxMkTemp },
        { "mkdir",  VBoxServiceToolboxMkDir  },
        { "stat",   VBoxServiceToolboxStat   },
    };

    /* Skip optional 'vbox_' prefix. */
    if (   pszTool[0] == 'v'
        && pszTool[1] == 'b'
        && pszTool[2] == 'o'
        && pszTool[3] == 'x'
        && pszTool[4] == '_')
        pszTool += 5;

    /* Do a linear search, since we don't have that much stuff in the table. */
    for (unsigned i = 0; i < RT_ELEMENTS(s_aTools); i++)
        if (!strcmp(s_aTools[i].pszName, pszTool))
            return s_aTools[i].pfnHandler;

    return NULL;
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

    /*
     * Check if the file named in argv[0] is one of the toolbox programs.
     */
    AssertReturn(argc > 0, false);
    const char *pszTool    = RTPathFilename(argv[0]);
    PFNHANDLER  pfnHandler = vboxServiceToolboxLookUpHandler(pszTool);
    if (!pfnHandler)
    {
        /*
         * For debugging and testing purposes we also allow toolbox program access
         * when the first VBoxService argument is --use-toolbox.
         */
        if (argc < 3 || strcmp(argv[1], "--use-toolbox"))
            return false;
        argc -= 2;
        argv += 2;
        pszTool = argv[0];
        pfnHandler = vboxServiceToolboxLookUpHandler(pszTool);
        if (!pfnHandler)
        {
           *prcExit = RTEXITCODE_SUCCESS;
           if (!strcmp(pszTool, "-V"))
           {
               VBoxServiceToolboxShowVersion();
               return true;
           }
           if (   (strcmp(pszTool, "help")) && (strcmp(pszTool, "--help"))
               && (strcmp(pszTool, "-h")))
               *prcExit = RTEXITCODE_SYNTAX;
           VBoxServiceToolboxShowUsage();
           return true;
        }
    }

    /*
     * Invoke the handler.
     */
    RTMsgSetProgName("VBoxService/%s", pszTool);
    *prcExit = pfnHandler(argc, argv);

    return true;
}

