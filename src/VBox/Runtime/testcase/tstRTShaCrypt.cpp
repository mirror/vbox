/* $Id$ */
/** @file
 * IPRT Testcase - SHA-crypt 256 / 512.
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/crypto/shacrypt.h>
#include <iprt/errcore.h>
#include <iprt/initterm.h>
#include <iprt/rand.h>
#include <iprt/sha.h>
#include <iprt/string.h>
#include <iprt/test.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static RTTEST g_hTest;


/*********************************************************************************************************************************
*   Test data                                                                                                                    *
*********************************************************************************************************************************/

/** Digest type. */
typedef enum TST_DIGESTTYPE
{
    TST_DIGESTTYPE_RANDOM = 0,
    TST_DIGESTTYPE_SHA256,
    TST_DIGESTTYPE_SHA512,
    TST_DIGESTTYPE_LAST
} TST_DIGESTTYPE;

static struct
{
    /** Cleartext password. */
    const char     *pszPassword;
    /** Salt to use. If NULL, a random salt will be used. */
    const char     *pszSalt;
    /** Number of rounds to use. If set to UINT32_MAX, random rounds will be used. */
    uint32_t        cRounds;
    /** Digest type to use. If set to 0, a random digest type will be used. */
    TST_DIGESTTYPE enmType;
    /** Overall test outcome. */
    int            rc;
    /** Expected result as a string. Can be NULL to skip testing this. */
    const char    *pszResultStr;
} g_aTests[] =
{
    /*
     * Invalid stuff.
     */
    {   /* No salt */
        /* pszPassword */   "changeme",
        /* pszSalt */       "",
        /* cRounds */       RT_SHACRYPT_DEFAULT_ROUNDS,
        /* enmType */       TST_DIGESTTYPE_RANDOM,
        /* rc */            VERR_INVALID_PARAMETER,
        /* pszResultStr */  ""
    },
    {   /* Salt too short */
        /* pszPassword */   "changeme",
        /* pszSalt */       "1234",
        /* cRounds */       RT_SHACRYPT_DEFAULT_ROUNDS,
        /* enmType */       TST_DIGESTTYPE_RANDOM,
        /* rc */            VERR_INVALID_PARAMETER,
        /* pszResultStr */  ""
    },
    {   /* Salt too long */
        /* pszPassword */   "changeme",
        /* pszSalt */       "12341234123412341234123412341234",
        /* cRounds */       RT_SHACRYPT_DEFAULT_ROUNDS,
        /* enmType */       TST_DIGESTTYPE_RANDOM,
        /* rc */            VERR_INVALID_PARAMETER,
        /* pszResultStr */  ""
    },
    {   /* Invalid rounds */
        /* pszPassword */   "changeme",
        /* pszSalt */       "12341234123412341234123412341234",
        /* cRounds */       0,
        /* enmType */       TST_DIGESTTYPE_RANDOM,
        /* rc */            VERR_INVALID_PARAMETER,
        /* pszResultStr */  ""
    },
    /*
     * Valid stuff.
     */
    {   /* Expected string */
        /* pszPassword */   "changeme",
        /* pszSalt */       "foo12345",
        /* cRounds */       RT_SHACRYPT_DEFAULT_ROUNDS,
        /* enmType */       TST_DIGESTTYPE_SHA256,
        /* rc */            VINF_SUCCESS,
        /* pszResultStr */  "$5$foo12345$KnOIYJmTgZ744xCqNLl1I9qF.Xq47vHTH.yVStiAMZD"
    },
    {   /* Expected string */
        /* pszPassword */   "changeme",
        /* pszSalt */       "foo12345",
        /* cRounds */       RT_SHACRYPT_DEFAULT_ROUNDS,
        /* enmType */       TST_DIGESTTYPE_SHA512,
        /* rc */            VINF_SUCCESS,
        /* pszResultStr */  "$6$foo12345$cb11CtCP6YgoZr8SyNoD2TAdOY4OmTzA6kfDgju5JrNVzgeCBU1ALbJHVlEuSImPKAoSnT53N7k7BqzjYRRPk/"
    },
    {   /* Expected string in Korean */
        /* pszPassword */   "이것은 테스트입니다", /* "This is a test" */
        /* pszSalt */       "foo12345",
        /* cRounds */       RT_SHACRYPT_DEFAULT_ROUNDS,
        /* enmType */       TST_DIGESTTYPE_SHA256,
        /* rc */            VINF_SUCCESS,
        /* pszResultStr */  "$5$foo12345$7fumMsJKgCGipks2nNPi185ANXwfTf9Ilz70J4wKqe1"
    },
    {   /* Expected string in Korean */
        /* pszPassword */   "이것은 테스트입니다", /* "This is a test" */
        /* pszSalt */       "foo12345",
        /* cRounds */       RT_SHACRYPT_DEFAULT_ROUNDS,
        /* enmType */       TST_DIGESTTYPE_SHA512,
        /* rc */            VINF_SUCCESS,
        /* pszResultStr */  "$6$foo12345$IWlIz4tyl39ETRpKlQ.R42tdeB2Ax9gz9sazAynilHDFm0zXUdsrm4nXzdlSd5jJhvwV7EPSc./2pBNoL1PIw1"
    },
    {   /* Custom rounds */
        /* pszPassword */   "changeme",
        /* pszSalt */       "foo12345",
        /* cRounds */       42,
        /* enmType */       TST_DIGESTTYPE_RANDOM,
        /* rc */            VINF_SUCCESS,
        /* pszResultStr */  NULL
    },
    {   /* Random salt + rounds */
        /* pszPassword */   "changeme",
        /* pszSalt */       NULL,
        /* cRounds */       UINT32_MAX,
        /* enmType */       TST_DIGESTTYPE_RANDOM,
        /* rc */            VINF_SUCCESS,
        /* pszResultStr */  NULL
    },
    {   /* Random salt */
        /* pszPassword */   "changeme",
        /* pszSalt */       NULL,
        /* cRounds */       RT_SHACRYPT_DEFAULT_ROUNDS,
        /* enmType */       TST_DIGESTTYPE_RANDOM,
        /* rc */            VINF_SUCCESS,
        /* pszResultStr */  NULL
    }
};


int main()
{
    /*
     * Init.
     */
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTShaCrypt", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);
    g_hTest = hTest;

    bool const fAssertMayPanic = RTAssertMayPanic();
    RTAssertSetMayPanic(false); /* To test invalid stuff. */
    bool const fAssertQuiet = RTAssertAreQuiet();
    RTAssertSetQuiet(true);     /* Ditto. */

    char    szSalt[RT_SHACRYPT_MAX_SALT_LEN + 1];
    uint8_t abDigest[RTSHA512_HASH_SIZE];

    for (uint32_t i = 0; i < RT_ELEMENTS(g_aTests); i++)
    {
        const char *pszSalt;
        if (g_aTests[i].pszSalt)
            pszSalt = g_aTests[i].pszSalt;
        else
        {
            rc = RTCrShaCryptGenerateSalt(szSalt, RT_SHACRYPT_MAX_SALT_LEN);
            RTTEST_CHECK_RC_OK(hTest, rc);
            pszSalt = szSalt;
        }

        uint32_t cRounds;
        if (g_aTests[i].cRounds == UINT32_MAX)
            cRounds = RTRandU32Ex(1, _512K /* Save a bit of time on the testboxes */);
        else
            cRounds = g_aTests[i].cRounds;

        TST_DIGESTTYPE enmType;
        if (g_aTests[i].enmType == TST_DIGESTTYPE_RANDOM)
            enmType = (TST_DIGESTTYPE)RTRandU32Ex(1, TST_DIGESTTYPE_LAST - 1);
        else
            enmType = g_aTests[i].enmType;

        switch (enmType)
        {
            case TST_DIGESTTYPE_SHA256:
                rc = RTCrShaCrypt256(g_aTests[i].pszPassword, pszSalt, cRounds, abDigest);
                break;

            case TST_DIGESTTYPE_SHA512:
                rc = RTCrShaCrypt512(g_aTests[i].pszPassword, pszSalt, cRounds, abDigest);
                break;

            default:
                AssertFailed();
                break;
        }

        if (   RT_SUCCESS(rc)
            && g_aTests[i].pszResultStr)
        {
            char szResult[RTSHA512_DIGEST_LEN + 1];

            switch (enmType)
            {
                case TST_DIGESTTYPE_SHA256:
                    rc = RTCrShaCrypt256ToString(abDigest, pszSalt, cRounds, szResult, sizeof(szResult));
                    break;

                case TST_DIGESTTYPE_SHA512:
                    rc = RTCrShaCrypt512ToString(abDigest, pszSalt, cRounds, szResult, sizeof(szResult));
                    break;

                default:
                    AssertFailed();
                    break;
            }

            if (RT_SUCCESS(rc))
            {
                if (RTStrCmp(szResult, g_aTests[i].pszResultStr))
                    RTTestIFailed("#%u: Returns '%s', expected '%s'", i, szResult, g_aTests[i].pszResultStr);

                /* Now do the same, but hand-in the result string as the salt.
                 *
                 * This approach is used by many *crypt implementations -- it allows feeding the user-provided password and the
                 * crypted password from "the password file" to the function. If it returns the same crypted password then the
                 * user-provided password must be the correct one.
                 */
                switch (enmType)
                {
                    case TST_DIGESTTYPE_SHA256:
                    {
                        rc = RTCrShaCrypt256(g_aTests[i].pszPassword, g_aTests[i].pszResultStr, cRounds, abDigest);
                        if (RT_SUCCESS(rc))
                            rc = RTCrShaCrypt256ToString(abDigest, pszSalt, cRounds, szResult, sizeof(szResult));
                        break;
                    }

                    case TST_DIGESTTYPE_SHA512:
                    {
                        rc = RTCrShaCrypt512(g_aTests[i].pszPassword, g_aTests[i].pszResultStr, cRounds, abDigest);
                        if (RT_SUCCESS(rc))
                            rc = RTCrShaCrypt512ToString(abDigest, pszSalt, cRounds, szResult, sizeof(szResult));
                        break;
                    }

                    default:
                        AssertFailed();
                        break;
                }

                if (RTStrCmp(szResult, g_aTests[i].pszResultStr))
                    RTTestIFailed("#%u (result as salt): Returns '%s', expected '%s'",
                                  i, szResult, g_aTests[i].pszResultStr);
            }
        }

        if (rc != g_aTests[i].rc)
            RTTestIFailed("#%u: Returned %Rrc, expected %Rrc", i, rc, g_aTests[i].rc);
    }

    RTAssertSetMayPanic(fAssertMayPanic);
    RTAssertSetQuiet(fAssertQuiet);

    /*
     * Done.
     */
    return RTTestSummaryAndDestroy(hTest);
}

