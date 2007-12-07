/* $Id$ */
/** @file
 * innotek Portable Runtime Testcase - Test various path functions.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#include <iprt/path.h>
#include <iprt/runtime.h>
#include <iprt/stream.h>
#include <iprt/err.h>
#include <iprt/param.h>


#define CHECK_RC(method) \
    do { \
        rc = method; \
        if (RT_FAILURE(rc)) \
        { \
            cErrors++; \
            RTPrintf("\ntstPath: FAILED calling " #method " at line %d: rc=%Vrc\n", __LINE__, rc); \
        } \
    } while (0)

int main()
{
    /*
     * Init RT.
     */
    int rc;
    int cErrors = 0;
    CHECK_RC(RTR3Init());
    if (RT_FAILURE(rc))
        return 1;

    /*
     * RTPathProgram, RTPathUserHome
     */
    char szPath[RTPATH_MAX];
    CHECK_RC(RTPathProgram(szPath, sizeof(szPath)));
    if (RT_SUCCESS(rc))
        RTPrintf("Program={%s}\n", szPath);
    CHECK_RC(RTPathUserHome(szPath, sizeof(szPath)));
    if (RT_SUCCESS(rc))
        RTPrintf("UserHome={%s}\n", szPath);
    
    /*
     * RTPathAbsEx
     */
    RTPrintf("tstPath: TESTING RTPathAbsEx()\n");
    static const char *aInput[] =
    {
        // NULL, NULL, -- assertion in RTStrUtf8ToUcs2
        NULL,                           "/absolute/..",
        NULL,                           "/absolute\\\\../..",
        NULL,                           "/absolute//../path",
        NULL,                           "/absolute/../../path",
        NULL,                           "relative/../dir\\.\\.\\.\\file.txt",
        NULL,                           "\\",
        "relative_base/dir\\",          "\\from_root",
        "relative_base/dir/",           "relative_also",
#if defined (RT_OS_OS2) || defined (RT_OS_WINDOWS)
        NULL,                           "C:\\",
        "C:\\",                         "..",
        "C:\\temp",                     "..",
        "C:\\VirtualBox/Machines",      "..\\VirtualBox.xml",
        "C:\\MustDie",                  "\\from_root/dir/..",
        "C:\\temp",                     "D:\\data",
        NULL,                           "\\\\server\\../share", // -- on Win32, GetFullPathName doesn't remove .. here
        /* the three below use cases should fail with VERR_INVALID_NAME */
        //NULL,                           "\\\\server",
        //NULL,                           "\\\\",
        //NULL,                           "\\\\\\something",
        "\\\\server\\share_as_base",    "/from_root",
        "\\\\just_server",              "/from_root",
        "\\\\server\\share_as_base",    "relative\\data",
        "base",                         "\\\\?\\UNC\\relative/edwef/..",
        "base",                         "\\\\?\\UNC\\relative/edwef/..",
        /* this is not (and I guess should not be) supported, should fail */
        ///@todo "\\\\?\\UNC\\base",             "/from_root",
#else
        "\\temp",                       "..",
        "\\VirtualBox/Machines",        "..\\VirtualBox.xml",
        "\\MustDie",                    "\\from_root/dir/..",
        "\\temp",                       "\\data",
#endif
        };

    for (unsigned i = 0; i < ELEMENTS(aInput); i += 2)
    {
        RTPrintf("tstPath: base={%s}, path={%s}, ", aInput[i], aInput[i + 1]);
        CHECK_RC(RTPathAbsEx(aInput[i], aInput[i + 1], szPath, sizeof(szPath)));
        if (RT_SUCCESS(rc))
            RTPrintf("abs={%s}\n", szPath);
    }

    /*
     * Summary.
     */
    if (!cErrors)
        RTPrintf("tstTimer: SUCCESS\n");
    else
        RTPrintf("tstTimer: FAILURE %d errors\n", cErrors);
    return !!cErrors;
}

