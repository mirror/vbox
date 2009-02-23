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
#include <iprt/string.h>
#include <iprt/process.h>
#include <iprt/dir.h>
#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/assert.h>

#include <unistd.h>


RTR3DECL(bool) RTProcIsRunningByName(const char *pszName)
{
    /*
     * Quick validation.
     */
    if (!pszName)
        return false;

    PRTDIR pDir;
    int rc = RTDirOpen(&pDir, "/proc");
    AssertMsgRCReturn(rc, ("RTDirOpen on /proc failed: rc=%Rrc\n", rc), false);
    if (RT_SUCCESS(rc))
    {
        RTDIRENTRY DirEntry;
        while(RT_SUCCESS(RTDirRead(pDir, &DirEntry, NULL)))
        {
            /*
             * Filter numeric directory entries only.
             */
            if (DirEntry.enmType == RTDIRENTRYTYPE_DIRECTORY &&
                RTStrToUInt32(DirEntry.szName) > 0)
            {
                /*
                 * Build the path to the proc exec file of this process.
                 */
                char *pszPath;
                RTStrAPrintf(&pszPath, "/proc/%s/exe", DirEntry.szName);
                /*
                 * Read the file name of the link pointing at.
                 */
                char szExe[RTPATH_MAX];
                size_t cchExe = sizeof(szExe)-1;
                int cchLink = readlink(pszPath, szExe, cchExe);
                if (cchLink > 0 && (size_t)cchLink <= cchExe)
                {
                    szExe[cchLink] = '\0';
                    /*
                     * We are interested on the file name part only.
                     */
                    char *pszFilename = RTPathFilename(szExe);
                    if (RTStrCmp(pszFilename, pszName) == 0)
                    {
                        /*
                         * Clean up
                         */
                        RTStrFree(pszPath);
                        RTDirClose(pDir);
                        return true;
                    }
                }
                RTStrFree(pszPath);
            }
        }
        RTDirClose(pDir);
    }

    return false;
}

