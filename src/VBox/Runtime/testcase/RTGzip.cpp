/* $Id$ */
/** @file
 * IPRT - GZIP Testcase Utility.
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

#include <iprt/getopt.h>
#include <iprt/init.h>
#include <iprt/message.h>
#include <iprt/string.h>
#include <iprt/vfs.h>


static bool isStdHandleATty(int fd)
{
    /** @todo IPRT is missing this */
    return false;
}

int main(int argc, char **argv)
{
    int rc = RTR3Init();
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    /*
     * Parse the command line.
     */
    static const RTGETOPTDEF s_aOptions[]
    {
        { "--ascii",        'a', RTGETOPT_REQ_NOTHING },
        { "--stdout",       'c', RTGETOPT_REQ_NOTHING },
        { "--to-stdout",    'c', RTGETOPT_REQ_NOTHING },
        { "--decompress",   'd', RTGETOPT_REQ_NOTHING },
        { "--uncompress",   'd', RTGETOPT_REQ_NOTHING },
        { "--force",        'f', RTGETOPT_REQ_NOTHING },
        { "--list",         'l', RTGETOPT_REQ_NOTHING },
        { "--no-name",      'n', RTGETOPT_REQ_NOTHING },
        { "--name",         'N', RTGETOPT_REQ_NOTHING },
        { "--quiet",        'q', RTGETOPT_REQ_NOTHING },
        { "--recursive",    'r', RTGETOPT_REQ_NOTHING },
        { "--suffix",       'S', RTGETOPT_REQ_STRING  },
        { "--test",         't', RTGETOPT_REQ_NOTHING },
        { "--verbose",      'v', RTGETOPT_REQ_NOTHING },
        { "--fast",         '1', RTGETOPT_REQ_NOTHING },
        { "-1",             '1', RTGETOPT_REQ_NOTHING },
        { "-2",             '2', RTGETOPT_REQ_NOTHING },
        { "-3",             '3', RTGETOPT_REQ_NOTHING },
        { "-4",             '4', RTGETOPT_REQ_NOTHING },
        { "-5",             '5', RTGETOPT_REQ_NOTHING },
        { "-6",             '6', RTGETOPT_REQ_NOTHING },
        { "-7",             '7', RTGETOPT_REQ_NOTHING },
        { "-8",             '8', RTGETOPT_REQ_NOTHING },
        { "-9",             '9', RTGETOPT_REQ_NOTHING },
        { "--best",         '9', RTGETOPT_REQ_NOTHING }
    };

    bool        fAscii      = false;
    bool        fStdOut     = false;
    bool        fDecompress = false;
    bool        fForce      = false;
    bool        fList       = false;
    bool        fName       = true;
    bool        fQuiet      = false;
    bool        fRecursive  = false;
    const char *pszSuff     = ".gz";
    bool        fTest       = false;
    unsigned    uLevel      = 6;

    RTEXITCODE  rcExit      = RTEXITCODE_SUCCESS;
    unsigned    cProcessed  = 0;

    RTGETOPTSTAT GetState;
    int rc = RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1,
                          RTGETOPTINIT_FLAGS_OPTS_FIRST);
    for (;;)
    {
        RTGETOPTUNION ValueUnion;
        rc = RTGetOpt(&GetState, &ValueUnion);
        switch (rc)
        {
            case 0:
            {
                /*
                 * If we've processed any files we're done.  Otherwise take
                 * input from stdin and write the output to stdout.
                 */
                if (cProcessed)
                    return rcExit;

                RTVFSIOSTREAM hVfsIn;
                RTVFSIOSTREAM hVfsOut;

                if (!fForce && isStdHandleATty(fDecompress ? 0 : 1))
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX,
                                          "Yeah, right. I'm not %s any compressed data %s the terminal without --force.\n",
                                          fDecompress ? "reading" : "writing",
                                          fDecompress ? "from"    : "to");
                return rcExit;
            }

            case VINF_GETOPT_NOT_OPTION:
            {
                RTVFSIOSTREAM hVfsSrc;
                rc = RTVfsOpenIoStreamFromSpec(ValueUnion.psz, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE, &hVfsSrc);

                cProcessed++;
                if (fDecompress)
                {
                }
                else
                {
                    if (!*pszSuff && !fStdOut)
                        return
                    {
                    }
                }
                break;
            }

            case 'a':   fAscii      = true;  break;
            case 'c':   fStdOut     = true;  break;
            case 'd':   fDecompress = true;  break;
            case 'f':   fForce      = true;  break;
            case 'l':   fList       = true;  break;
            case 'n':   fName       = false; break;
            case 'N':   fName       = true;  break;
            case 'q':   fQuiet      = true;  break;
            case 'r':   fRecursive  = true;  break;
            case 'S':   pszSuff     = ValueUnion.psz; break;
            case 't':   fTest       = true;  break;
            case 'v':   fQuiet      = false; break;
            case '1':   uLevel      = 1;     break;
            case '2':   uLevel      = 2;     break;
            case '3':   uLevel      = 3;     break;
            case '4':   uLevel      = 4;     break;
            case '5':   uLevel      = 5;     break;
            case '6':   uLevel      = 6;     break;
            case '7':   uLevel      = 7;     break;
            case '8':   uLevel      = 8;     break;
            case '9':   uLevel      = 9;     break;

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }
}


