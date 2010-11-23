/* $Id$ */
/** @file
 * VBoxServiceToolBox - Internal (BusyBox-like) toolbox.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
#include <sys/stat.h>
#endif

#include <VBox/VBoxGuestLib.h>
#include <VBox/version.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"


#define CAT_OPT_NO_CONTENT_INDEXED              1000

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


/**
 * Displays a help text to stdout.
 */
static void VBoxServiceToolboxShowUsage(void)
{
    RTPrintf("Toolbox Usage:\n"
             "cat [FILE] - Concatenate FILE(s), or standard input, to standard output\n"
             "\n"
             "mkdir - Make directories\n"
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
 * Destroys a path buffer list.
 *
 * @return  IPRT status code.
 * @param   pList                   Pointer to list to destroy.
 */
static void VBoxServiceToolboxPathBufDestroy(PRTLISTNODE pList)
{
    AssertPtr(pList);
    PVBOXSERVICETOOLBOXPATHENTRY pNode = RTListNodeGetFirst(pList, VBOXSERVICETOOLBOXPATHENTRY, Node);
    while (pNode)
    {
        PVBOXSERVICETOOLBOXPATHENTRY pNext = RTListNodeIsLast(pList, &pNode->Node)
                                                              ? NULL :
                                                                RTListNodeGetNext(&pNode->Node,
                                                                                  VBOXSERVICETOOLBOXPATHENTRY, Node);
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
            if (RT_SUCCESS(rc) && cbRead)
            {
                rc = RTFileWrite(hOutput, abBuf, cbRead, NULL /* Try to write all at once! */);
                cbRead = 0;
            }
            else
            {
                if (   cbRead == 0
                    && rc     == VERR_BROKEN_PIPE)
                {
                    rc = VINF_SUCCESS;
                }
                break;
            }
        }
    }
    return rc;
}


/**
 * Main function for tool "vbox_mkdir".
 *
 * @return  RTEXITCODE.
 * @param   argc                    Number of arguments.
 * @param   argv                    Pointer to argument array.
 */
static int VBoxServiceToolboxMkDir(int argc, char **argv)
{
     static const RTGETOPTDEF s_aOptions[] =
     {
         { "--mode",     'm', RTGETOPT_REQ_STRING },
         { "--parents",  'p', RTGETOPT_REQ_NOTHING },
         { "--verbose",  'v', RTGETOPT_REQ_NOTHING }
     };

     int ch;
     RTGETOPTUNION ValueUnion;
     RTGETOPTSTATE GetState;
     RTGetOptInit(&GetState, argc, argv,
                  s_aOptions, RT_ELEMENTS(s_aOptions),
                  1 /* Index of argv to start with. */, RTGETOPTINIT_FLAGS_OPTS_FIRST);

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
             {
                 /* Add path(s) to buffer. This enables processing multiple paths
                  * at once.
                  *
                  * Since the non-options (RTGETOPTINIT_FLAGS_OPTS_FIRST) come last when
                  * processing this loop it's safe to immediately exit on syntax errors
                  * or showing the help text (see above). */
                 rc = VBoxServiceToolboxPathBufAddPathEntry(&dirList, ValueUnion.psz);
                 break;
             }

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
             rc = fMakeParentDirs ?
                    RTDirCreateFullPath(pNodeIt->pszName, dirMode)
                  : RTDirCreate(pNodeIt->pszName, dirMode);

             if (RT_SUCCESS(rc) && fVerbose)
                 RTMsgError("mkdir: Created directory 's', mode %#RTfmode\n", pNodeIt->pszName, dirMode);
             else if (RT_FAILURE(rc)) /** @todo Add a switch with more helpful error texts! */
             {
                 PCRTSTATUSMSG pMsg = RTErrGet(rc);
                 if (pMsg)
                     RTMsgError("mkdir: Could not create directory '%s': %s\n",
                                pNodeIt->pszName, pMsg->pszMsgFull);
                 else
                     RTMsgError("mkdir: Could not create directory '%s', rc=%Rrc\n", pNodeIt->pszName, rc);
             }
         }
     }
     else if (fVerbose)
         RTMsgError("mkdir: Failed with rc=%Rrc\n", rc);

     VBoxServiceToolboxPathBufDestroy(&dirList);
     return RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


/**
 * Main function for tool "vbox_cat".
 *
 * @return  RTEXITCODE.
 * @param   argc                    Number of arguments.
 * @param   argv                    Pointer to argument array.
 */
static int VBoxServiceToolboxCat(int argc, char **argv)
{
     static const RTGETOPTDEF s_aOptions[] =
     {
         /* Sorted by short ops. */
         { "--show-all",            'a',                         RTGETOPT_REQ_NOTHING },
         { "--number-nonblank",     'b',                         RTGETOPT_REQ_NOTHING },
         { NULL,                    'e',                         RTGETOPT_REQ_NOTHING },
         { NULL,                    'E',                         RTGETOPT_REQ_NOTHING },
         { "--flags",               'f',                         RTGETOPT_REQ_STRING  },
         { "--no-content-indexed",  CAT_OPT_NO_CONTENT_INDEXED,  RTGETOPT_REQ_NOTHING },
         { "--number",              'n',                         RTGETOPT_REQ_NOTHING },
         { "--output",              'o',                         RTGETOPT_REQ_STRING  },
         { "--squeeze-blank",       's',                         RTGETOPT_REQ_NOTHING },
         { NULL,                    't',                         RTGETOPT_REQ_NOTHING },
         { "--show-tabs",           'T',                         RTGETOPT_REQ_NOTHING },
         { NULL,                    'u',                         RTGETOPT_REQ_NOTHING },
         { "--show-noneprinting",   'v',                         RTGETOPT_REQ_NOTHING }
     };

     int ch;
     RTGETOPTUNION ValueUnion;
     RTGETOPTSTATE GetState;
     RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, 0);

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
                 RTMsgError("cat: Could not create output file '%s'! rc=%Rrc\n",
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
 * Entry point for internal toolbox.
 *
 * @return  True if an internal tool was handled, false if not.
 * @param   argc                    Number of arguments.
 * @param   argv                    Pointer to argument array.
 * @param   piExitCode              Pointer to receive exit code when internal command
 *                                  was handled.
 */
bool VBoxServiceToolboxMain(int argc, char **argv, int *piExitCode)
{
    if (argc > 0) /* Do we have at least a main command? */
    {
        if (   !strcmp(argv[0], "cat")
            || !strcmp(argv[0], "vbox_cat"))
        {
            *piExitCode = VBoxServiceToolboxCat(argc, argv);
            return true;
        }
        else if (   !strcmp(argv[0], "mkdir")
                 || !strcmp(argv[0], "vbox_mkdir"))
        {
            *piExitCode = VBoxServiceToolboxMkDir(argc, argv);
            return true;
        }
    }
    return false;
}

