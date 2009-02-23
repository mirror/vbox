/* $Id$ */
/** @file
 * IPRT - RTProcIsRunningByName, Linux implementation.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
#define LOG_GROUP RTLOGGROUP_PROCESS
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/dir.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/param.h>
#include <iprt/assert.h>


RTR3DECL(bool) RTProcIsRunningByName(const char *pszName)
{
    /*
     * Quick validation.
     */
    if (!pszName)
        return false;

    bool fFoundIt = false;
    PRTDIR pDir;
    int rc = RTDirOpen(&pDir, "/proc");
    AssertMsgRCReturn(rc, ("RTDirOpen on /proc failed: rc=%Rrc\n", rc), false);
    if (RT_SUCCESS(rc))
    {
        RTDIRENTRY DirEntry;
        while (RT_SUCCESS(RTDirRead(pDir, &DirEntry, NULL)))
        {
            /*
             * Filter numeric directory entries only.
             */
            if (    DirEntry.enmType == RTDIRENTRYTYPE_DIRECTORY
                &&  RTStrToUInt32(DirEntry.szName) > 0)
            {
                /*
                 * The first line of the 'cmdline' file is the argv[0] passed to
                 * execv, which is what we're interested in... (Alternatively we
                 * could try readlink 'exe'. Check what happens to set-uid procs.)
                 */
                PRTSTREAM pStream;
                rc = RTStrmOpenF("r", &pStream, "/proc/%s/cmdline", &DirEntry.szName[0]);
                if (RT_SUCCESS(rc))
                {
                    char szLine[RTPATH_MAX];
                    rc = RTStrmGetLine(pStream, szLine, sizeof(szLine));
                    RTStrmClose(pStream);
                    if (    RT_SUCCESS(rc)
                        ||  rc == VERR_BUFFER_OVERFLOW)
                    {
                        /*
                         * We are interested on the file name part only.
                         */
                        char const *pszFilename = RTPathFilename(szLine);
                        if (RTStrCmp(pszFilename, pszName) == 0)
                        {
                            /* Found it! */
                            RTDirClose(pDir);
                            return true;
                        }
                    }
                }
            }
        }
        RTDirClose(pDir);
    }

    return false;
}

