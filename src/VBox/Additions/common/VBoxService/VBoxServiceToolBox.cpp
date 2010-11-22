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

/** @todo r=bird: Don't use VBoxServiceError here, just use RTMsg**.  */
/** @todo r=bird: 'static' is a wonderful keyword, please use it as much as
 *        possible like I've said in the coding guidelines.  Not only does it
 *        help wrt to linking, but it also helps understanding what's
 *        internal and external interfaces in a source file! */


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


/** @todo r=bird: Again, function headers like this are uslesss and better left
 *        out. */
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
     /** @todo r=bird: Pass RTGETOPTINIT_FLAGS_OPTS_FIRST, our mkdir shall be
      *        like the GNU one wrt to option (dash-something) and argument
      *        order (dirs). */

     int rc = VINF_SUCCESS;
     bool fMakeParentDirs = false;
     bool fVerbose = false;

     char szDir[RTPATH_MAX];
     RTFMODE newMode = 0;
#ifdef RT_OS_WINDOWS
     RTFMODE fileMode = 0;
#else
     RTFMODE fileMode = S_IRWXU | S_IRWXG | S_IRWXO; /** @todo r=bird: We've got RTFS_ defines for these, they are x-platform.  Why 'file' when we're creating directories? */
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
                 /** @todo r=bird: you can make multiple directories just by
                  *  adding them to the mkdir command line:
                  *     "mkdir foo/ foo/bar foo/bar/wiz"
                  *  This will now only create foo/bar/wiz, which will fail
                  *  because the two previous steps weren't executed.  It
                  *  will also leak memory, but that's not important. (Also,
                  *  I don't get why we need to call RTPathAbs here as nobody
                  *  is going to change the current directory.) */
                 break;
             }
             /** @todo r=bird: Missing handling of the standard options 'V' and
              *        'h'. */

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
             VBoxServiceVerbose(0, "mkdir: Created directory '%s', mode 0x%RTfmode\n", szDir, fileMode); /** @todo r=bird: drop the 0x here, use %#RTfmode instead. */
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
     /** @todo r=bird: Missing options 'A', 'b', 'e', 'E', 'n', 's', 'T',
      *        'u', 'v' as found on 'man cat' on a linux system. They must
      *        not be implemented, just return an apologetic error message. */
     };


     int ch;
     RTGETOPTUNION ValueUnion;
     RTGETOPTSTATE GetState;
     RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, 0);

     int rc = VINF_SUCCESS;
     RTFILE hInput = NIL_RTFILE;

     RTFILE hOutput = NIL_RTFILE;
     uint32_t fFlags = RTFILE_O_CREATE_REPLACE /* Output file flags. */
                     | RTFILE_O_WRITE
                     | RTFILE_O_DENY_WRITE;

     while (   (ch = RTGetOpt(&GetState, &ValueUnion))
            && RT_SUCCESS(rc))
     {
         /* For options that require an argument, ValueUnion has received the value. */
         switch (ch)
         {
             /** @todo r=bird: You add a flag --no-content-indexed without a
              * short form (use a #define CAT_OPT_NO_CONTENT_INDEXED 1000 for
              * iShort). */

             case 'f':
                 /* Process flags; no fancy parsing here yet. */
                 if (RTStrIStr(ValueUnion.psz, "noindex"))
                     fFlags |= RTFILE_O_NOT_CONTENT_INDEXED;
                 else
                 {
                     VBoxServiceError("cat: Unknown flag set!\n");
                     rc = VERR_INVALID_PARAMETER;
                 }
                 break;

             case 'o':
                 rc = RTFileOpen(&hOutput, ValueUnion.psz, fFlags);
                 if (RT_FAILURE(rc))
                     VBoxServiceError("cat: Could not create output file \"%s\"! rc=%Rrc\n",
                                      ValueUnion.psz, rc);
                 break;

                 /** @todo r=bird: Again, there shall be no need for any -i
                  *        options since all non-options are input files. */

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

             /** @todo r=bird: Missing handling of the standard options 'V' and
              *        'h'. */

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
    /** @todo r=bird: The return type of this function is mixed; both RTEXITCODE
     *  and IPRT status code.  That doesn't cut it.  The RTEXITCODE part should
     *  be returned separately from the handled-or-unhandled bit.
     *
     *  Also, please change VBoxServiceToolboxCat and VBoxServiceToolboxMkDir to
     *  return RTEXITCODE and use RTMsg* like RTZipTarCmd (and later
     *  RTZipGzipCmd). */
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

