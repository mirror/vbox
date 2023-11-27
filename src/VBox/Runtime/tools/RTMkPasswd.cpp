/* $Id$ */
/** @file
 * IPRT - Makes passwords.
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

#include <iprt/buildconfig.h>
#include <iprt/crypto/shacrypt.h>
#include <iprt/errcore.h>
#include <iprt/initterm.h>
#include <iprt/getopt.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/rand.h>
#include <iprt/sha.h>
#include <iprt/stream.h>
#include <iprt/string.h>


/** Method type. */
typedef enum RTMKPASSWORD_METHODTYPE
{
    RTMKPASSWORD_METHODTYPE_SHA256,
    RTMKPASSWORD_METHODTYPE_SHA512
} RTMKPASSWORD_METHODTYPE;


int main(int argc, char **argv)
{
    RTR3InitExe(argc, &argv, 0);

    /*
     * Process options.
     */
    static const RTGETOPTDEF aOpts[] =
    {
        { "--help",             'h',               RTGETOPT_REQ_NOTHING },
        { "--salt",             'S',               RTGETOPT_REQ_STRING },
        { "--rounds",           'R',               RTGETOPT_REQ_UINT32 },
        { "--method",           'm',               RTGETOPT_REQ_STRING },
        { "--version",          'V',               RTGETOPT_REQ_NOTHING }
    };

    RTGETOPTSTATE GetState;
    int rc = RTGetOptInit(&GetState, argc, argv, aOpts, RT_ELEMENTS(aOpts), 1 /*idxFirst*/, 0 /*fFlags - must not sort! */);
    AssertRCReturn(rc, RTEXITCODE_INIT);

    const char *pszKey                = NULL;
          char   szSalt[RT_SHACRYPT_MAX_SALT_LEN + 1];
    const char *pszSalt               = NULL;
    uint32_t    cRounds               = RT_SHACRYPT_DEFAULT_ROUNDS;
    RTMKPASSWORD_METHODTYPE enmMethod = RTMKPASSWORD_METHODTYPE_SHA512; /* Go with strongest by default. */

    int           ch;
    RTGETOPTUNION ValueUnion;
    while ((ch = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (ch)
        {
            case 'S':
            {
                if (!pszSalt)
                {
                    pszSalt = ValueUnion.psz;
                }
                else
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Salt already specified!\n");
                break;
            }

            case 'R':
            {
                cRounds = ValueUnion.u32;
                if (cRounds < RT_SHACRYPT_DEFAULT_ROUNDS)
                    RTMsgWarning("Using less rounds than the default (%zu) isn't a good idea!!\n",
                                 RT_SHACRYPT_DEFAULT_ROUNDS);
                break;
            }

            case 'm':
            {
                const char *pszMethod = ValueUnion.psz;
                if (!RTStrICmp(pszMethod, "sha256"))
                    enmMethod = RTMKPASSWORD_METHODTYPE_SHA256;
                else if (!RTStrICmp(pszMethod, "sha512"))
                    enmMethod = RTMKPASSWORD_METHODTYPE_SHA512;
                else if (!RTStrICmp(pszMethod, "help"))
                {
                    RTPrintf("Supported methods: sha256, sha512\n");
                }
                else
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Sorry, method '%s' not implemented yet!\n", pszMethod);
                break;
            }

            case 'h':
            {
                RTPrintf("Usage: %s [options] password\n"
                         "\n"
                         "Options:\n"
                         "  -S <salt>, --salt <salt>\n"
                         "      Salt to use.\n"
                         "  -R, --rounds\n"
                         "      Number of rounds to use.\n"
                         "  -m <type>, --method <type>\n"
                         "      Method to use for creation. sha512 is the default.\n"
                         "      If <type> is 'help', then the list of all available methods are printed.\n"
                         "  -h, --help\n"
                         "      This help screen.\n"
                         , argv[0]);
                return RTEXITCODE_SUCCESS;
            }

            case 'V':
                RTPrintf("%sr%d\n", RTBldCfgVersion(), RTBldCfgRevision());
                return RTEXITCODE_SUCCESS;

            case VINF_GETOPT_NOT_OPTION:
            {
                if (!pszKey)
                    pszKey = ValueUnion.psz;
                else
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Password already specified!\n");
                break;
            }

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }

    if (!pszKey)
         return RTMsgErrorExit(RTEXITCODE_SYNTAX, "No password specified!\n");
    if (!pszSalt)
    {
        int vrc2 = RTCrShaCryptGenerateSalt(szSalt, RT_SHACRYPT_MAX_SALT_LEN);
        AssertRCReturn(vrc2, RTEXITCODE_FAILURE);
        pszSalt = szSalt;
    }
    else if (strlen(pszSalt) < RT_SHACRYPT_MIN_SALT_LEN)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Salt is too short (must be at least %zu characters)!\n",
                              RT_SHACRYPT_MIN_SALT_LEN);
    else if (strlen(pszSalt) > RT_SHACRYPT_MAX_SALT_LEN)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Salt is too long (must be less or equal than %zu characters)!\n",
                              RT_SHACRYPT_MAX_SALT_LEN);

    uint8_t abDigest[RTSHA512_HASH_SIZE];
    char    szResult[RTSHA512_DIGEST_LEN + 1];

    switch (enmMethod)
    {
        case RTMKPASSWORD_METHODTYPE_SHA256:
        {
            rc = RTCrShaCrypt256(pszKey, pszSalt, cRounds, abDigest);
            if (RT_SUCCESS(rc))
                rc = RTCrShaCrypt256ToString(abDigest, pszSalt, cRounds, szResult, sizeof(szResult));
            break;
        }

        case RTMKPASSWORD_METHODTYPE_SHA512:
        {
            rc = RTCrShaCrypt512(pszKey, pszSalt, cRounds, abDigest);
            if (RT_SUCCESS(rc))
                rc = RTCrShaCrypt512ToString(abDigest, pszSalt, cRounds, szResult, sizeof(szResult));
            break;
        }

        default:
            AssertFailed();
            break;
    }

   if (RT_SUCCESS(rc))
   {
        RTPrintf("%s\n", szResult);
   }
   else
        RTMsgError("Failed with %Rrc\n", rc);

    return RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

