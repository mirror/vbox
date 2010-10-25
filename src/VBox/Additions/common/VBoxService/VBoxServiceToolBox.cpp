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
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/stream.h>

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
            VBoxServiceError("Cat: Could not translate input file to native handle, rc=%Rrc\n", rc);
    }

    if (hOutput == NIL_RTFILE)
    {
        rc = RTFileFromNative(&hOutput, RTFILE_NATIVE_STDOUT);
        if (RT_FAILURE(rc))
            VBoxServiceError("Cat: Could not translate output file to native handle, rc=%Rrc\n", rc);
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
int VBoxServiceToolboxCat(int argc, char **argv)
{
     static const RTGETOPTDEF s_aOptions[] =
     {
         { "--input",     'i', RTGETOPT_REQ_STRING },
         { "--output",    'o', RTGETOPT_REQ_STRING }
     };

     int ch;
     RTGETOPTUNION ValueUnion;
     RTGETOPTSTATE GetState;
     RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, 0);

     int rc = VINF_SUCCESS;
     //bool fSeenInput = false;
     RTFILE hInput = NIL_RTFILE;
     RTFILE hOutput = NIL_RTFILE;

     while (   (ch = RTGetOpt(&GetState, &ValueUnion))
            && RT_SUCCESS(rc))
     {
         /* For options that require an argument, ValueUnion has received the value. */
         switch (ch)
         {
             case 'o':
                 rc = RTFileOpen(&hOutput, ValueUnion.psz,
                                 RTFILE_O_CREATE_REPLACE | RTFILE_O_READWRITE | RTFILE_O_DENY_WRITE);
                 if (RT_FAILURE(rc))
                     VBoxServiceError("Cat: Could not create output file \"%s\"! rc=%Rrc\n",
                                      ValueUnion.psz, rc);
                 break;

             case 'i':
             case VINF_GETOPT_NOT_OPTION:
             {
                 /*rc = VBoxServiceToolboxCatInput(ValueUnion.psz, hOutput);
                 if (RT_SUCCESS(rc))
                     fSeenInput = true;*/

                 rc = RTFileOpen(&hInput, ValueUnion.psz,
                                 RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
                 if (RT_FAILURE(rc))
                     VBoxServiceError("Cat: Could not open input file \"%s\"! rc=%Rrc\n",
                                      ValueUnion.psz, rc);
                 break;
             }

             default:
                 return RTGetOptPrintError(ch, &ValueUnion);
         }
     }

     if (RT_SUCCESS(rc) /*&& !fSeenInput*/)
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
    }

    if (rc != VERR_NOT_FOUND)
        rc = RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
    return rc;
}

