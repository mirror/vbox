/* $Id$ */
/** @file
 * innotek Portable Runtime Testcase - RTGetOpt
 */

/*
 * Copyright (C) 2007 innotek GmbH
 *
 * innotek GmbH confidential
 * All rights reserved
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

    int i;
    RTOPTIONUNION Val;
#define CHECK(expr)  do { if (!(expr)) { RTPrintf("tstGetOpt: error line %d (i=%d): %s\n", __LINE__, i, #expr); cErrors++; } } while (0)

#define CHECK_GETOPT(expr, chRet, iNext) \
    do { \
        CHECK((expr) == (chRet)); \
        CHECK(i == (iNext)); \
        i = (iNext); \
    } while (0)

    /*
     * Simple.
     */
    static const RTOPTIONDEF s_aOpts2[] = 
    {
        { "--optwithstring",    's', RTGETOPT_REQ_STRING },
        { "--optwithint",       'i', RTGETOPT_REQ_INT32 },
        { "--verbose",          'v', RTGETOPT_REQ_NOTHING },
        { NULL,                 'q', RTGETOPT_REQ_NOTHING },
        { "--quiet",            384, RTGETOPT_REQ_NOTHING },
    };

    char *argv2[] = 
    {
        "-s",               "string1",
        "--optwithstring",  "string2",
        "-i",               "-42",
        "--optwithint",     "42",
        "-v",
        "--verbose",
        "-q",
        "--quiet",
        /* "filename1", */
        /* "filename2", */
        NULL
    };
    int argc2 = (int)RT_ELEMENTS(argv2) - 1;

    i = 0;
    CHECK_GETOPT(RTGetOpt(argc2, argv2, &s_aOpts2[0], RT_ELEMENTS(s_aOpts2), &i, &Val), 's', 2);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "string1"));
    CHECK_GETOPT(RTGetOpt(argc2, argv2, &s_aOpts2[0], RT_ELEMENTS(s_aOpts2), &i, &Val), 's', 4);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "string2"));
    CHECK_GETOPT(RTGetOpt(argc2, argv2, &s_aOpts2[0], RT_ELEMENTS(s_aOpts2), &i, &Val), 'i', 6);
    CHECK(Val.i32 == -42);
    CHECK_GETOPT(RTGetOpt(argc2, argv2, &s_aOpts2[0], RT_ELEMENTS(s_aOpts2), &i, &Val), 'i', 8);
    CHECK(Val.i32 == 42);
    CHECK_GETOPT(RTGetOpt(argc2, argv2, &s_aOpts2[0], RT_ELEMENTS(s_aOpts2), &i, &Val), 'v', 9);
    CHECK(Val.pDef == &s_aOpts2[2]);
    CHECK_GETOPT(RTGetOpt(argc2, argv2, &s_aOpts2[0], RT_ELEMENTS(s_aOpts2), &i, &Val), 'v', 10);
    CHECK(Val.pDef == &s_aOpts2[2]);
    CHECK_GETOPT(RTGetOpt(argc2, argv2, &s_aOpts2[0], RT_ELEMENTS(s_aOpts2), &i, &Val), 'q', 11);
    CHECK(Val.pDef == &s_aOpts2[3]);
    CHECK_GETOPT(RTGetOpt(argc2, argv2, &s_aOpts2[0], RT_ELEMENTS(s_aOpts2), &i, &Val), 384, 12);
    CHECK(Val.pDef == &s_aOpts2[4]);
    CHECK_GETOPT(RTGetOpt(argc2, argv2, &s_aOpts2[0], RT_ELEMENTS(s_aOpts2), &i, &Val), 0, 12);
    CHECK(Val.pDef == NULL);
    CHECK(argc2 == i);


    /*
     * Summary.
     */
    if (!cErrors)
        RTPrintf("tstGetOpt: SUCCESS\n");
    else
        RTPrintf("tstGetOpt: FAILURE - %d errors\n", cErrors);

    return !!cErrors;
}
