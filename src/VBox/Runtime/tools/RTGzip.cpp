/* $Id$ */
/** @file
 * IPRT - GZIP Utility.
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

#include <iprt/buildconfig.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/param.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/vfs.h>
#include <iprt/zip.h>


static bool isStdHandleATty(int fd)
{
    /** @todo IPRT is missing this */
    return false;
}


/**
 * Pushes data from the input to the output I/O streams.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE.
 * @param   hVfsIn              The input I/O stream.
 * @param   hVfsOut             The input I/O stream.
 */
static RTEXITCODE gzipPush(RTVFSIOSTREAM hVfsIn, RTVFSIOSTREAM hVfsOut)
{
    for (;;)
    {
        uint8_t abBuf[_64K];
        size_t  cbRead;
        int rc = RTVfsIoStrmRead(hVfsIn, abBuf, sizeof(abBuf), true /*fBlocking*/, &cbRead);
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTVfsIoStrmRead failed: %Rrc", rc);
        if (rc == VINF_EOF && cbRead == 0)
            return RTEXITCODE_SUCCESS;

        rc = RTVfsIoStrmWrite(hVfsOut, abBuf, cbRead, true /*fBlocking*/, NULL /*cbWritten*/);
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTVfsIoStrmWrite failed: %Rrc", rc);
    }
}

static RTEXITCODE gzipCompress(RTVFSIOSTREAM hVfsIn, RTVFSIOSTREAM hVfsOut)
{
    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Compression is not yet implemented, sorry.");
}


static RTEXITCODE gzipCompressFile(const char *pszFile, bool fStdOut, bool fForce, PRTVFSIOSTREAM phVfsStdOut)
{
    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Compression is not yet implemented, sorry.");
}


static RTEXITCODE gzipDecompress(RTVFSIOSTREAM hVfsIn, RTVFSIOSTREAM hVfsOut)
{
    RTEXITCODE      rcExit;
    RTVFSIOSTREAM   hVfsGunzip;
    int rc = RTZipGzipDecompressIoStream(hVfsIn, 0 /*fFlags*/, &hVfsGunzip);
    if (RT_SUCCESS(rc))
    {
        rcExit = gzipPush(hVfsGunzip, hVfsOut);
        RTVfsIoStrmRelease(hVfsGunzip);
    }
    else
        rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "RTZipGzipDecompressIoStream failed: %Rrc", rc);
    return rcExit;
}


/**
 * Handles a file on the command line.
 *
 * @returns exit code.
 * @param   pszFile             The file to handle.
 * @param   fStdOut             Whether to output to standard output or not.
 * @param   fForce              Whether to output to or input from terminals.
 * @param   phVfsStdOut         Pointer to the standard out handle.
 *                              (input/output)
 */
static RTEXITCODE gzipDecompressFile(const char *pszFile, bool fStdOut, bool fForce, PRTVFSIOSTREAM phVfsStdOut)
{
    /*
     * Open the specified input file.
     */
    const char *pszError;
    RTVFSIOSTREAM hVfsIn;
    int rc = RTVfsChainOpenIoStream(pszFile, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE, &hVfsIn, &pszError);
    if (RT_FAILURE(rc))
    {
        if (pszError && *pszError)
            return RTMsgErrorExit(RTEXITCODE_FAILURE,
                                  "RTVfsChainOpenIoStream failed with rc=%Rrc:\n"
                                  "    '%s'\n",
                                  "     %*s^\n",
                                  rc, pszFile, pszError - pszFile, "");
        return RTMsgErrorExit(RTEXITCODE_FAILURE,
                              "RTVfsChainOpenIoStream failed with rc=%Rrc: '%s'",
                              rc, pszFile);
    }

    /*
     * Output the output file.
     */
    RTVFSIOSTREAM hVfsOut;
    char szFinal[RTPATH_MAX];
    if (fStdOut)
    {
        if (*phVfsStdOut == NIL_RTVFSIOSTREAM)
        {
            rc = RTVfsIoStrmFromStdHandle(RTHANDLESTD_OUTPUT, 0 /*fOpen*/, true /*fLeaveOpen*/, phVfsStdOut);
            if (RT_FAILURE(rc))
                return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to set up standard out: %Rrc", rc);
        }
        hVfsOut = *phVfsStdOut;
        szFinal[0] = '\0';
    }
    else
    {
        rc = RTStrCopy(szFinal, sizeof(szFinal), pszFile);
        /** @todo remove the extension?  Or are we supposed
         *        to get the org name from the gzip stream? */
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Decompressing to file is not implemented");
    }

    /*
     * Do the decompressing, then flush and close the output stream (unless
     * it is stdout).
     */
    RTEXITCODE rcExit = gzipDecompress(hVfsIn, hVfsOut);
    RTVfsIoStrmRelease(hVfsIn);
    rc = RTVfsIoStrmFlush(hVfsOut);
    if (RT_FAILURE(rc))
        rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to flush the output file: %Rrc", rc);
    RTVfsIoStrmRelease(hVfsOut);

    /*
     * Remove the input file, if that's the desire of the caller, or
     * remove the output file on decompression failure.
     */
    if (!fStdOut)
    {
        if (rcExit == RTEXITCODE_SUCCESS)
        {
            rc = RTFileDelete(pszFile);
            if (RT_FAILURE(rc))
                rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "RTFileDelete failed with %rc: '%s'", rc, pszFile);
        }
        else
        {
            /* should we do this? */
            rc = RTFileDelete(szFinal);
            if (RT_FAILURE(rc))
                RTMsgError("RTFileDelete failed with %rc: '%s'", rc, pszFile);
        }
    }

    return rcExit;
}


static RTEXITCODE gzipTestFile(const char *pszFile, bool fForce)
{
    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Testiong has not been implemented");
}


static RTEXITCODE gzipListFile(const char *pszFile, bool fForce)
{
    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Listing has not been implemented");
}


int main(int argc, char **argv)
{
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    /*
     * Parse the command line.
     */
    static const RTGETOPTDEF s_aOptions[] =
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
    RTVFSIOSTREAM hVfsStdOut= NIL_RTVFSIOSTREAM;

    RTGETOPTSTATE GetState;
    rc = RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1,
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
                if (cProcessed > 0)
                    return rcExit;
#if 0
                rc = RTVfsFileFromRTFile(1,
                                         RTFILE_O_WRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE,
                                         true /*fLeaveOpen*/,
                                         &hVfsOut);


                if (!fForce && isStdHandleATty(fDecompress ? 0 : 1))
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX,
                                          "Yeah, right. I'm not %s any compressed data %s the terminal without --force.\n",
                                          fDecompress ? "reading" : "writing",
                                          fDecompress ? "from"    : "to");
#else
                rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "reading from standard input has not yet been implemented");
#endif
                return rcExit;
            }

            case VINF_GETOPT_NOT_OPTION:
            {
                if (!*pszSuff && !fStdOut)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "The --suffix option specified an empty string");
                if (!fStdOut && RTVfsChainIsSpec(ValueUnion.psz))
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Must use standard out with VFS chain specifications");

                RTEXITCODE rcExit2;
                if (fList)
                    rcExit2 = gzipListFile(ValueUnion.psz, fForce);
                else if (fTest)
                    rcExit2 = gzipTestFile(ValueUnion.psz, fForce);
                else if (fDecompress)
                    rcExit2 = gzipDecompressFile(ValueUnion.psz, fStdOut, fForce, &hVfsStdOut);
                else
                    rcExit2 = gzipCompressFile(ValueUnion.psz, fStdOut, fForce, &hVfsStdOut);
                if (rcExit2 != RTEXITCODE_SUCCESS)
                    rcExit = rcExit2;

                cProcessed++;
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

            case 'h':
                RTPrintf("Usage: to be written\nOption dump:\n");
                for (unsigned i = 0; i < RT_ELEMENTS(s_aOptions); i++)
                    RTPrintf(" -%c,%s\n", s_aOptions[i].iShort, s_aOptions[i].pszLong);
                return RTEXITCODE_SUCCESS;

            case 'V':
                RTPrintf("%sr%d\n", RTBldCfgVersion(), RTBldCfgRevision());
                return RTEXITCODE_SUCCESS;

            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }
}

