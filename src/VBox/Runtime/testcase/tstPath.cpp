/* $Id$ */
/** @file
 * InnoTek Portable Runtime Testcase - Test various path functions.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
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
     * RTPathProgram
     */
    char szPath[RTPATH_MAX];
    CHECK_RC(RTPathProgram(szPath, sizeof(szPath)));

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
        "relative_base/dir\\",          "\\from_root",
        "relative_base/dir/",           "relative_also",
#if defined (__OS2__) || defined (__WIN__)
        "C:\\temp",                     "..",
        "C:\\VirtualBox/Machines",      "..\\VirtualBox.xml",
        "C:\\MustDie",                  "\\from_root/dir/..",
        "C:\\temp",                     "D:\\data",
        NULL,                           "\\\\server\\../share", // -- GetFullPathName doesn't remove .. here
        "\\\\server\\share_as_base",    "/from_root",
        "\\\\just_server",              "/from_root",
        "\\\\server\\share_as_base",    "relative\\data",
        "base",                         "\\\\?\\UNC\\relative/edwef/..",
        // this is not (and I guess should not be) supported
        ///@todo "\\\\?\\UNC\\base",             "/from_root", - r=bird: negative tests shouldn't fail the testcase!
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

