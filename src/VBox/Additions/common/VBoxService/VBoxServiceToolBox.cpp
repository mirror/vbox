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
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/stream.h>

#ifndef RT_OS_WINDOWS
#include <sys/stat.h>
#endif

#include <VBox/VBoxGuestLib.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"


/**
 * Displays a help text to stdout.
 */
void VBoxServiceToolboxShowUsage(void)
{
    RTPrintf("Toolbox Usage:\n"
             "cat [FILE] - Concatenate FILE(s), or standard input, to standard output\n"
             "\n");
}


/**
 *
 *
 * @return  int
 *
 * @param   pszFormat
 */
int VBoxServiceToolboxErrorSyntax(const char *pszFormat, ...)
{
    va_list args;

    va_start(args, pszFormat);
    RTPrintf("\n"
             "Syntax error: %N\n", pszFormat, &args);
    va_end(args);
    VBoxServiceToolboxShowUsage();
    return VERR_INVALID_PARAMETER;
}


/**
 *
 *
 * @return  int
 *
 * @param   hInput
 * @param   hOutput
 */
int VBoxServiceToolboxCatOutput(RTFILE hInput, RTFILE hOutput)
{
    int rc = VINF_SUCCESS;
    if (hInput == NIL_RTFILE)
    {
        rc = RTFileFromNative(&hInput, RTFILE_NATIVE_STDIN);
        if (RT_FAILURE(rc))
            VBoxServiceError("cat: Could not translate input file to native handle, rc=%Rrc\n", rc);
    }

    if (hOutput == NIL_RTFILE)
    {
        rc = RTFileFromNative(&hOutput, RTFILE_NATIVE_STDOUT);
        if (RT_FAILURE(rc))
            VBoxServiceError("cat: Could not translate output file to native handle, rc=%Rrc\n", rc);
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
 *
 *
 * @return  int
 *
 * @param   argc
 * @param   argv
 */
int VBoxServiceToolboxMkDir(int argc, char **argv)
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
     RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, 0);

     int rc = VINF_SUCCESS;
     bool fMakeParentDirs = false;
     bool fVerbose = false;

     char szDir[RTPATH_MAX];
     RTFMODE newMode = 0;
#ifdef RT_OS_WINDOWS
     RTFMODE fileMode = 0;
#else
     RTFMODE fileMode = S_IRWXU | S_IRWXG | S_IRWXO;
#endif

     while (   (ch = RTGetOpt(&GetState, &ValueUnion))
            && RT_SUCCESS(rc))
     {
         /* For options that require an argument, ValueUnion has received the value. */
         switch (ch)
         {
             case 'p':
                 fMakeParentDirs = true;
                 break;

             case 'm':
                 rc = RTStrToUInt32Ex(ValueUnion.psz, NULL, 8 /* Base */, &newMode);
                 if (RT_FAILURE(rc)) /* Only octet based values supported right now! */
                     VBoxServiceVerbose(0, "mkdir: Mode flag strings not implemented yet!\n");
                 break;

             case 'v':
                 fVerbose = true;
                 break;

             case VINF_GETOPT_NOT_OPTION:
             {
                 rc = RTPathAbs(ValueUnion.psz, szDir, sizeof(szDir));
                 if (RT_FAILURE(rc))
                     VBoxServiceError("mkdir: Could not build absolute directory!\n");
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
                fileMode = newMode;
#endif
         }

         rc = fMakeParentDirs ?
                RTDirCreateFullPath(szDir, fileMode)
              : RTDirCreate(szDir, fileMode);

         if (RT_SUCCESS(rc) && fVerbose)
             VBoxServiceVerbose(0, "mkdir: Created directory '%s', mode 0x%RTfmode\n", szDir, fileMode);
         else if (RT_FAILURE(rc)) /** @todo Add a switch with more helpful error texts! */
         {
             PCRTSTATUSMSG pMsg = RTErrGet(rc);
             if (pMsg)
                 VBoxServiceError("mkdir: Could not create directory: %s\n", pMsg->pszMsgFull);
             else
                 VBoxServiceError("mkdir: Could not create directory, rc=%Rrc\n", rc);
         }

     }
     else if (fVerbose)
         VBoxServiceError("mkdir: Failed with rc=%Rrc\n", rc);
     return rc;
}


/**
 *
 *
 * @return  int
 *
 * @param   argc
 * @param   argv
 */
int VBoxServiceToolboxCat(int argc, char **argv)
{
     static const RTGETOPTDEF s_aOptions[] =
     {
         { "--input",     'i', RTGETOPT_REQ_STRING },
         { "--output",    'o', RTGETOPT_REQ_STRING },
         { "--flags",     'f', RTGETOPT_REQ_STRING }
     };

     int ch;
     RTGETOPTUNION ValueUnion;
     RTGETOPTSTATE GetState;
     RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, 0);

     int rc = VINF_SUCCESS;
     RTFILE hInput = NIL_RTFILE;

     RTFILE hOutput = NIL_RTFILE;
     uint32_t ofFlags =   RTFILE_O_CREATE_REPLACE /* Output file flags. */
                        | RTFILE_O_WRITE
                        | RTFILE_O_DENY_WRITE;

     while (   (ch = RTGetOpt(&GetState, &ValueUnion))
            && RT_SUCCESS(rc))
     {
         /* For options that require an argument, ValueUnion has received the value. */
         switch (ch)
         {
             case 'f':
                 /* Process flags; no fancy parsing here yet. */
                 if (RTStrIStr(ValueUnion.psz, "noindex"))
                     ofFlags |= RTFILE_O_NOT_CONTENT_INDEXED;
                 else
                 {
                     VBoxServiceError("cat: Unknown flag set!\n");
                     rc = VERR_INVALID_PARAMETER;
                 }
                 break;

             case 'o':
                 rc = RTFileOpen(&hOutput, ValueUnion.psz, ofFlags);
                 if (RT_FAILURE(rc))
                     VBoxServiceError("cat: Could not create output file \"%s\"! rc=%Rrc\n",
                                      ValueUnion.psz, rc);
                 break;

             case 'i':
             case VINF_GETOPT_NOT_OPTION:
             {
                 rc = RTFileOpen(&hInput, ValueUnion.psz,
                                 RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
                 if (RT_FAILURE(rc))
                     VBoxServiceError("cat: Could not open input file \"%s\"! rc=%Rrc\n",
                                      ValueUnion.psz, rc);
                 break;
             }

             default:
                 return RTGetOptPrintError(ch, &ValueUnion);
         }
     }

     if (RT_SUCCESS(rc))
         rc  = VBoxServiceToolboxCatOutput(hInput, hOutput);

     if (hInput != NIL_RTFILE)
         RTFileClose(hInput);
     if (hOutput != NIL_RTFILE)
         RTFileClose(hOutput);
     return rc;
}


/**
 * Main routine for toolbox command line handling.
 *
 * @return  int
 *
 * @param   argc
 * @param   argv
 */
int VBoxServiceToolboxMain(int argc, char **argv)
{
    int rc = VERR_NOT_FOUND;
    if (argc > 0) /* Do we have at least a main command? */
    {
        if (   !strcmp(argv[0], "cat")
            || !strcmp(argv[0], "vbox_cat"))
        {
            rc = VBoxServiceToolboxCat(argc, argv);
        }
        else if (   !strcmp(argv[0], "mkdir")
                 || !strcmp(argv[0], "vbox_mkdir"))
        {
            rc = VBoxServiceToolboxMkDir(argc, argv);
        }
    }

    if (rc != VERR_NOT_FOUND)
        rc = RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
    return rc;
}

