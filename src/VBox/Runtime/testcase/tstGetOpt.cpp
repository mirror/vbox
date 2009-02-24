/* $Id$ */
/** @file
 * IPRT Testcase - RTGetOpt
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
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


#include <iprt/getopt.h>
#include <iprt/stream.h>
#include <iprt/initterm.h>
#include <iprt/string.h>
#include <iprt/err.h>


int main()
{
    int cErrors = 0;
    RTR3Init();

    RTGETOPTSTATE GetState;
    RTGETOPTUNION Val;
#define CHECK(expr)  do { if (!(expr)) { RTPrintf("tstGetOpt: error line %d (iNext=%d): %s\n", __LINE__, GetState.iNext, #expr); cErrors++; } } while (0)

#define CHECK_GETOPT(expr, chRet, iInc) \
    do { \
        const int iPrev = GetState.iNext; \
        CHECK((expr) == (chRet)); \
        CHECK(GetState.iNext == (iInc) + iPrev); \
        GetState.iNext = (iInc) + iPrev; \
    } while (0)

    /*
     * Simple.
     */
    static const RTGETOPTDEF s_aOpts2[] =
    {
        { "--optwithstring",    's', RTGETOPT_REQ_STRING },
        { "--optwithint",       'i', RTGETOPT_REQ_INT32 },
        { "--verbose",          'v', RTGETOPT_REQ_NOTHING },
        { NULL,                 'q', RTGETOPT_REQ_NOTHING },
        { "--quiet",            384, RTGETOPT_REQ_NOTHING },
        { "-quiet2",            385, RTGETOPT_REQ_NOTHING },
    };

    char *argv2[] =
    {
        "-s",               "string1",
        "--optwithstring",  "string2",
        "-i",               "-42",
        "-i:-42",
        "-i=-42",
        "-i:",              "-42",
        "-i=",              "-42",
        "--optwithint",     "42",
        "--optwithint:42",
        "--optwithint=42",
        "--optwithint:",    "42",
        "--optwithint=",    "42",
        "-v",
        "--verbose",
        "-q",
        "--quiet",
        "-quiet2",
        /* "filename1", */
        /* "filename2", */
        NULL
    };
    int argc2 = (int)RT_ELEMENTS(argv2) - 1;

    CHECK(RT_SUCCESS(RTGetOptInit(&GetState, argc2, argv2, &s_aOpts2[0], RT_ELEMENTS(s_aOpts2), 0, 0 /* fFlags */)));

    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 's', 2);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "string1"));
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 's', 2);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "string2"));

    /* -i */
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 2);
    CHECK(Val.i32 == -42);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 1);
    CHECK(Val.i32 == -42);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 1);
    CHECK(Val.i32 == -42);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 2);
    CHECK(Val.i32 == -42);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 2);
    CHECK(Val.i32 == -42);

    /* --optwithint */
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 2);
    CHECK(Val.i32 == 42);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 1);
    CHECK(Val.i32 == 42);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 1);
    CHECK(Val.i32 == 42);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 2);
    CHECK(Val.i32 == 42);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 2);
    CHECK(Val.i32 == 42);

    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'v', 1);
    CHECK(Val.pDef == &s_aOpts2[2]);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'v', 1);
    CHECK(Val.pDef == &s_aOpts2[2]);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'q', 1);
    CHECK(Val.pDef == &s_aOpts2[3]);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 384, 1);
    CHECK(Val.pDef == &s_aOpts2[4]);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 385, 1);
    CHECK(Val.pDef == &s_aOpts2[5]);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 0, 0);
    CHECK(Val.pDef == NULL);
    CHECK(argc2 == GetState.iNext);


    /*
     * Summary.
     */
    if (!cErrors)
        RTPrintf("tstGetOpt: SUCCESS\n");
    else
        RTPrintf("tstGetOpt: FAILURE - %d errors\n", cErrors);

    return !!cErrors;
}
