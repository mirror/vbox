/* $Id$ */
/** @file
 * IPRT Testcase - Base64.
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
#include <iprt/base64.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/err.h>
#include <iprt/initterm.h>


int main()
{
    int cErrors =0;
    RTR3Init();

    /*
     * Test 0.
     */
    static const char s_szText0[] = "Hey";
    static const char s_szEnc0[]  = "SGV5";
    size_t cchOut0 = 0;
    char szOut0[sizeof(s_szText0)];
    int rc = RTBase64Decode(&s_szEnc0[0], szOut0, sizeof(szOut0), &cchOut0, NULL);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstBase64: FAILURE - RTBase64Decode s_szEnc0 -> %Rrc\n", rc);
        cErrors++;
    }
    else if (cchOut0 != sizeof(s_szText0) - 1)
    {
        RTPrintf("tstBase64: FAILURE - RTBase64Decode returned %zu bytes, expected %zu.\n",
                 cchOut0, sizeof(s_szText0) - 1);
        cErrors++;
    }
    else if (memcmp(szOut0, s_szText0, cchOut0))
    {
        RTPrintf("tstBase64: FAILURE - RTBase64Decode returned:\n%.*s\nexpected:\n%s\n",
                 (int)cchOut0, szOut0, s_szText0);
        cErrors++;
    }

    /*
     * Test 1.
     */
    static const char s_szText1[] = "Call me Ishmael.";
    static const char s_szEnc1[]  = "Q2FsbCBtZSBJc2htYWVsLg==";
    size_t cchOut1 = 0;
    char szOut1[sizeof(s_szText1)];
    rc = RTBase64Decode(&s_szEnc1[0], szOut1, sizeof(szOut1), &cchOut1, NULL);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstBase64: FAILURE - RTBase64Decode s_szEnc1 -> %Rrc\n", rc);
        cErrors++;
    }
    else if (cchOut1 != sizeof(s_szText1) - 1)
    {
        RTPrintf("tstBase64: FAILURE - RTBase64Decode returned %zu bytes, expected %zu.\n",
                 cchOut1, sizeof(s_szText1) - 1);
        cErrors++;
    }
    else if (memcmp(szOut1, s_szText1, cchOut1))
    {
        RTPrintf("tstBase64: FAILURE - RTBase64Decode returned:\n%.*s\nexpected:\n%s\n",
                 (int)cchOut1, szOut1, s_szText1);
        cErrors++;
    }

    /*
     * Test 2.
     */
    static const char s_szText2[] =
        "Man is distinguished, not only by his reason, but by this singular passion "
        "from other animals, which is a lust of the mind, that by a perseverance of "
        "delight in the continued and indefatigable generation of knowledge, exceeds "
        "the short vehemence of any carnal pleasure."; /* Thomas Hobbes's Leviathan */

    static const char s_szEnc2[] =
        " TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBieSB0aGlz\n"
        " IHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpcyBhIGx1c3Qgb2Yg\n\r"
        " dGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpbiB0aGUgY29udGlu\n"
        " dWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZGdlLCBleGNlZWRzIHRo\n\r"
        " ZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3VyZS4=\n \n";

    size_t cchOut2 = 0;
    char szOut2[sizeof(s_szText2)];
    rc = RTBase64Decode(&s_szEnc2[0], szOut2, sizeof(szOut2), &cchOut2, NULL);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstBase64: FAILURE - RTBase64Decode s_szEnc2 -> %Rrc\n", rc);
        cErrors++;
    }
    else if (cchOut2 != sizeof(s_szText2) - 1)
    {
        RTPrintf("tstBase64: FAILURE - RTBase64Decode returned %zu bytes, expected %zu.\n",
                 cchOut2, sizeof(s_szText2) - 1);
        cErrors++;
    }
    else if (memcmp(szOut2, s_szText2, cchOut2))
    {
        RTPrintf("tstBase64: FAILURE - RTBase64Decode returned:\n%.*s\nexpected:\n%s\n",
                 (int)cchOut2, szOut2, s_szText2);
        cErrors++;
    }

    /*
     * Summary.
     */
    if (!cErrors)
        RTPrintf("tstBase64: SUCCESS\n");
    else
        RTPrintf("tstBase64: FAILURE - %d errors\n", cErrors);
    return !!cErrors;
}

