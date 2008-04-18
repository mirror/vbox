/* $Id$ */
/** @file
 * innotek Portable Runtime - RTFileMove & RTDirMove test program.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/dir.h>
#include <iprt/err.h>
#include <iprt/runtime.h>
#include <iprt/stream.h>


/**
 * Checks if there is one of the typical help options in the argument list.
 */
static bool HasHelpOption(int argc, char **argv)
{
    for (int argi = 1; argi < argc; argi++)
        if (    argv[argi][0] == '-'
            &&  (   argv[argi][1] == 'h'
                 || argv[argi][1] == 'H'
                 || argv[argi][1] == '?'
                 || argv[argi][1] == '-')
            )
            return true;
    return false;
}


int main(int argc, char **argv)
{
    RTR3Init();

    /*
     * Arguments or any -? or --help?
     */
    if (argc <= 1 || HasHelpOption(argc, argv))
    {
        RTPrintf("usage: tstMove [-efdr] <src> <dst>\n"
                 "\n"
                 "  -f      File only.\n"
                 "  -d      Directory only.\n"
                 "  -m      Use move operation instead of rename. (implies -f)\n"
                 "  -r      Replace existing destination.\n"
                 );
        return 1;
    }

    /*
     * Parse args.
     */
    const char *pszNew = NULL;
    const char *pszOld = NULL;
    bool        fDir = false;
    bool        fFile = false;
    bool        fReplace = false;
    bool        fMoveFile = false;
    for (int argi = 1; argi < argc; argi++)
    {
        if (argv[argi][0] == '-')
        {
            const char *psz = &argv[argi][1];
            do
            {
                switch (*psz)
                {
                    case 'd':
                        fDir = true;
                        fMoveFile = false;
                        break;
                    case 'f':
                        fFile = true;
                        break;
                    case 'm':
                        fMoveFile = true;
                        fDir = false;
                        fFile = true;
                        break;
                    case 'r':
                        fReplace = true;
                        break;
                    default:
                        RTPrintf("tstRTFileMove: syntax error: Unknown option '%c' in '%s'!\n", *psz, argv[argi]);
                        return 1;
                }
            } while (*++psz);
        }
        else if (!pszOld)
            pszOld = argv[argi];
        else if (!pszNew)
            pszNew = argv[argi];
        else
        {
            RTPrintf("tstRTFileMove: syntax error: too many filenames!\n");
            return 1;
        }
    }
    if (!pszNew || !pszOld)
    {
        RTPrintf("tstRTFileMove: syntax error: too few filenames!\n");
        return 1;
    }

    /*
     * Do the operation.
     */
    int rc;
    if (!fDir && !fFile)
        rc = RTPathRename(pszOld, pszNew, fReplace ? RTPATHRENAME_FLAGS_REPLACE : 0);
    else if (fDir)
        rc = RTDirRename( pszOld, pszNew, fReplace ? RTPATHRENAME_FLAGS_REPLACE : 0);
    else if (!fMoveFile)
        rc = RTFileRename(pszOld, pszNew, fReplace ? RTPATHRENAME_FLAGS_REPLACE : 0);
    else
        rc = RTFileMove(  pszOld, pszNew, fReplace ? RTFILEMOVE_FLAGS_REPLACE : 0);

    RTPrintf("The API returned %Rrc\n", rc);
    return !RT_SUCCESS(rc);
}

