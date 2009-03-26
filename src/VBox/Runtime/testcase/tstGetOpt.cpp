/* $Id$ */
/** @file
 * IPRT Testcase - RTGetOpt
 */

/*
 * Copyright (C) 2007-2009 Sun Microsystems, Inc.
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
#include <iprt/net.h>
#include <iprt/getopt.h>
#include <iprt/stream.h>
#include <iprt/initterm.h>
#include <iprt/string.h>
#include <iprt/err.h>


int main()
{
    int cErrors = 0;
    RTR3Init();
    RTPrintf("tstGetOpt: TESTING...\n");

    RTGETOPTSTATE GetState;
    RTGETOPTUNION Val;
#define CHECK(expr)  do { if (!(expr)) { RTPrintf("tstGetOpt: error line %d (iNext=%d): %s\n", __LINE__, GetState.iNext, #expr); cErrors++; } } while (0)
#define CHECK2(expr, fmt) \
    do { \
        if (!(expr)) { \
            RTPrintf("tstGetOpt: error line %d (iNext=%d): %s\n", __LINE__, GetState.iNext, #expr); \
            RTPrintf fmt; \
            cErrors++; \
         } \
    } while (0)

#define CHECK_pDef(paOpts, i) \
    CHECK2(Val.pDef == &(paOpts)[(i)], ("Got #%d (%p) expected #%d\n", (int)(Val.pDef - &(paOpts)[0]), Val.pDef, i));

#define CHECK_GETOPT(expr, chRet, iInc) \
    do { \
        const int iPrev = GetState.iNext; \
        const int rc = (expr); \
        CHECK2(rc == (chRet), ("got %d, expected %d\n", rc, (chRet))); \
        CHECK2(GetState.iNext == (iInc) + iPrev, ("iNext=%d expected %d\n", GetState.iNext, (iInc) + iPrev)); \
        GetState.iNext = (iInc) + iPrev; \
    } while (0)


    /*
     * The basics.
     */
    static const RTGETOPTDEF s_aOpts2[] =
    {
        { "--optwithstring",    's', RTGETOPT_REQ_STRING },
        { "--optwithint",       'i', RTGETOPT_REQ_INT32 },
        { "--verbose",          'v', RTGETOPT_REQ_NOTHING },
        { NULL,                 'q', RTGETOPT_REQ_NOTHING },
        { "--quiet",            384, RTGETOPT_REQ_NOTHING },
        { "-novalue",           385, RTGETOPT_REQ_NOTHING },
        { "-startvm",           386, RTGETOPT_REQ_STRING },
        { "nodash",             387, RTGETOPT_REQ_NOTHING },
        { "nodashval",          388, RTGETOPT_REQ_STRING },
        { "--gateway",          'g', RTGETOPT_REQ_IPV4ADDR },
        { "--mac",              'm', RTGETOPT_REQ_MACADDR },
    };

    const char *argv2[] =
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

        "-novalue",
        "-startvm",         "myvm",

        "nodash",
        "nodashval",        "string3",

        "filename1",
        "-q",
        "filename2",

        "-vqi999",

        "-g192.168.1.1",

        "-m08:0:27:00:ab:f3",
        "--mac:1:::::c",

        NULL
    };
    int argc2 = (int)RT_ELEMENTS(argv2) - 1;

    CHECK(RT_SUCCESS(RTGetOptInit(&GetState, argc2, (char **)argv2, &s_aOpts2[0], RT_ELEMENTS(s_aOpts2), 0, 0 /* fFlags */)));

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
    CHECK_pDef(s_aOpts2, 2);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'v', 1);
    CHECK_pDef(s_aOpts2, 2);

    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'q', 1);
    CHECK_pDef(s_aOpts2, 3);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 384, 1);
    CHECK_pDef(s_aOpts2, 4);

    /* -novalue / -startvm (single dash long options) */
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 385, 1);
    CHECK_pDef(s_aOpts2, 5);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 386, 2);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "myvm"));

    /* no-dash options */
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 387, 1);
    CHECK_pDef(s_aOpts2, 7);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 388, 2);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "string3"));

    /* non-option, option, non-option  */
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1);
    CHECK(Val.psz && !strcmp(Val.psz, "filename1"));
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'q', 1);
    CHECK_pDef(s_aOpts2, 3);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1);
    CHECK(Val.psz && !strcmp(Val.psz, "filename2"));

    /* compress short options */
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'v', 0);
    CHECK_pDef(s_aOpts2, 2);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'q', 0);
    CHECK_pDef(s_aOpts2, 3);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 1);
    CHECK(Val.i32 == 999);

    /* IPv4 */
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'g', 1);
    CHECK(Val.IPv4Addr.u == RT_H2N_U32_C(RT_BSWAP_U32_C(RT_MAKE_U32_FROM_U8(192,168,1,1))));

    /* Ethernet MAC address. */
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'm', 1);
    CHECK(   Val.MacAddr.au8[0] == 0x08
          && Val.MacAddr.au8[1] == 0x00
          && Val.MacAddr.au8[2] == 0x27
          && Val.MacAddr.au8[3] == 0x00
          && Val.MacAddr.au8[4] == 0xab
          && Val.MacAddr.au8[5] == 0xf3);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'm', 1);
    CHECK(   Val.MacAddr.au8[0] == 0x01
          && Val.MacAddr.au8[1] == 0x00
          && Val.MacAddr.au8[2] == 0x00
          && Val.MacAddr.au8[3] == 0x00
          && Val.MacAddr.au8[4] == 0x00
          && Val.MacAddr.au8[5] == 0x0c);

    /* the end */
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

