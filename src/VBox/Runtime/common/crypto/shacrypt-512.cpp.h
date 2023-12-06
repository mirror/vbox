/* $Id$ */
/** @file
 * IPRT - Crypto - SHA-crypt, code template for SHA-512 core.
 *
 * This is almost identical to shacrypt-256.cpp.h, fixes generally applies to
 * both. Diff the files after updates!
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


RTDECL(int) RTCrShaCrypt512(const char *pszPhrase, const char *pszSalt, uint32_t cRounds, char *pszString, size_t cbString)
{
    uint8_t abHash[RTSHA512_HASH_SIZE];
    int rc = RTCrShaCrypt512Ex(pszPhrase, pszSalt, cRounds, abHash);
    if (RT_SUCCESS(rc))
        rc = RTCrShaCrypt512ToString(abHash, pszSalt, cRounds, pszString, cbString);
    return rc;
}



RTR3DECL(int) RTCrShaCrypt512Ex(const char *pszPhrase, const char *pszSalt, uint32_t cRounds, uint8_t pabHash[RTSHA512_HASH_SIZE])
{
    /*
     * Validate and adjust input.
     */
    AssertPtrReturn(pszPhrase, VERR_INVALID_POINTER);
    size_t const cchPhrase = strlen(pszPhrase);
    AssertReturn(cchPhrase, VERR_INVALID_PARAMETER);

    AssertPtrReturn(pszSalt, VERR_INVALID_POINTER);
    size_t cchSalt;
    pszSalt = rtCrShaCryptExtractSaltAndRounds(pszSalt, &cchSalt, &cRounds);
    AssertReturn(pszSalt != NULL, VERR_INVALID_PARAMETER);
    AssertReturn(cchSalt >= RT_SHACRYPT_SALT_MIN_LEN, VERR_BUFFER_UNDERFLOW);
    AssertReturn(cchSalt <= RT_SHACRYPT_SALT_MAX_LEN, VERR_TOO_MUCH_DATA);
    AssertReturn(cRounds >= RT_SHACRYPT_ROUNDS_MIN && cRounds <= RT_SHACRYPT_ROUNDS_MAX, VERR_OUT_OF_RANGE);

    /*
     * Get started...
     */
    RTSHA512CONTEXT CtxA;
    RTSha512Init(&CtxA);                                                        /* Step 1. */
    RTSha512Update(&CtxA, pszPhrase, cchPhrase);                                /* Step 2. */
    RTSha512Update(&CtxA, pszSalt, cchSalt);                                    /* Step 3. */

    RTSHA512CONTEXT CtxB;
    RTSha512Init(&CtxB);                                                        /* Step 4. */
    RTSha512Update(&CtxB, pszPhrase, cchPhrase);                                /* Step 5. */
    RTSha512Update(&CtxB, pszSalt, cchSalt);                                    /* Step 6. */
    RTSha512Update(&CtxB, pszPhrase, cchPhrase);                                /* Step 7. */
    uint8_t abDigest[RTSHA512_HASH_SIZE];
    RTSha512Final(&CtxB, abDigest);                                             /* Step 8. */

    size_t i = cchPhrase;
    for (; i > RTSHA512_HASH_SIZE; i -= RTSHA512_HASH_SIZE)                     /* Step 9. */
        RTSha512Update(&CtxA, abDigest, sizeof(abDigest));
    RTSha512Update(&CtxA, abDigest, i);                                         /* Step 10. */

    size_t iPhraseBit = cchPhrase;
    while (iPhraseBit)                                                          /* Step 11. */
    {
        if ((iPhraseBit & 1) != 0)
            RTSha512Update(&CtxA, abDigest, sizeof(abDigest));                  /* a) */
        else
            RTSha512Update(&CtxA, pszPhrase, cchPhrase);                        /* b) */
        iPhraseBit >>= 1;
    }

    RTSha512Final(&CtxA, abDigest);                                             /* Step 12. */

    RTSha512Init(&CtxB);                                                        /* Step 13. */
    for (i = 0; i < cchPhrase; i++)                                             /* Step 14. */
        RTSha512Update(&CtxB, pszPhrase, cchPhrase);

    uint8_t abDigestTemp[RTSHA512_HASH_SIZE];
    RTSha512Final(&CtxB, abDigestTemp);                                         /* Step 15. */

    /*
     * Byte sequence P (= password).
     */
    size_t const cbSeqP  = cchPhrase;
    uint8_t     *pabSeqP = (uint8_t *)RTMemTmpAllocZ(cbSeqP + 1);               /* +1 because the password may be empty */
    uint8_t     *pb       = pabSeqP;
    AssertPtrReturn(pabSeqP, VERR_NO_MEMORY);

    for (i = cbSeqP; i > RTSHA512_HASH_SIZE; i -= RTSHA512_HASH_SIZE)           /* Step 16. */
    {
        memcpy(pb, abDigestTemp, sizeof(abDigestTemp));                         /* a) */
        pb += RTSHA512_HASH_SIZE;
    }
    memcpy(pb, abDigestTemp, i);                                                /* b) */

    RTSha512Init(&CtxB);                                                        /* Step 17. */

    for (i = 0; i < 16 + (unsigned)abDigest[0]; i++)                            /* Step 18. */
        RTSha512Update(&CtxB, pszSalt, cchSalt);

    RTSha512Final(&CtxB, abDigestTemp);                                         /* Step 19. */

    /*
     * Byte sequence S (= salt).
     */
    /* Step 20. */
    size_t   const  cbSeqS  = cchSalt;
#if 0 /* Given that the salt has a fixed range (8 thru 16 bytes), and SHA-512
       * producing 64 bytes, we can safely skip the loop part here (a) and go
       * straight for step (b). Further, we can drop the whole memory allocation,
       * let alone duplication (it's all overwritten!), and use an uninitalized
       * stack buffer. */
    uint8_t * const pabSeqS = (uint8_t *)RTMemDup(pszSalt, cbSeqS + 1);
    AssertPtrReturn(pabSeqS, VERR_NO_MEMORY);

    pb = pabSeqS;
    for (i = cbSeqS; i > RTSHA512_HASH_SIZE; i -= RTSHA512_HASH_SIZE)
    {
        memcpy(pb, (void *)abDigestTemp, sizeof(abDigestTemp));                 /* a) */
        pb += RTSHA512_HASH_SIZE;
    }
    memcpy(pb, abDigestTemp, i);                                                /* b) */
#else
    AssertCompile(RT_SHACRYPT_SALT_MAX_LEN < RTSHA512_HASH_SIZE);
    uint8_t         abSeqS[RT_SHACRYPT_SALT_MAX_LEN + 2];
    uint8_t * const pabSeqS = abSeqS;
    memcpy(abSeqS, abDigestTemp, cbSeqS);                                       /* b) */
#endif

    /* Step 21. */
    for (uint32_t iRound = 0; iRound < cRounds; iRound++)
    {
        RTSHA512CONTEXT CtxC;
        RTSha512Init(&CtxC);                                                    /* a) */

        if ((iRound & 1) != 0)
            RTSha512Update(&CtxC, pabSeqP, cbSeqP);                             /* b) */
        else
            RTSha512Update(&CtxC, abDigest, sizeof(abDigest));                  /* c) */

        if (iRound % 3 != 0)                                                    /* d) */
            RTSha512Update(&CtxC, pabSeqS, cbSeqS);

        if (iRound % 7 != 0)
            RTSha512Update(&CtxC, pabSeqP, cbSeqP);                             /* e) */

        if ((iRound & 1) != 0)
            RTSha512Update(&CtxC, abDigest, sizeof(abDigest));                  /* f) */
        else
            RTSha512Update(&CtxC, pabSeqP, cbSeqP);                             /* g) */

        RTSha512Final(&CtxC, abDigest);                                         /* h) */
    }

    /*
     * Done.
     */
    memcpy(pabHash, abDigest, RTSHA512_HASH_SIZE);

    /*
     * Cleanup.
     */
    RTMemWipeThoroughly(abDigestTemp, RTSHA512_HASH_SIZE, 3);
    RTMemWipeThoroughly(pabSeqP, cbSeqP, 3);
    RTMemTmpFree(pabSeqP);
#if 0
    RTMemWipeThoroughly(pabSeqS, cbSeqS, 3);
    RTMemFree(pabSeqS);
#else
    RTMemWipeThoroughly(abSeqS, sizeof(abSeqS), 3);
#endif

    return VINF_SUCCESS;
}


RTR3DECL(int) RTCrShaCrypt512ToString(uint8_t const pabHash[RTSHA512_HASH_SIZE], const char *pszSalt, uint32_t cRounds,
                                      char *pszString, size_t cbString)
{
    /*
     * Validate and adjust input.
     */
    AssertPtrReturn(pszSalt, VERR_INVALID_POINTER);
    size_t cchSalt;
    pszSalt = rtCrShaCryptExtractSaltAndRounds(pszSalt, &cchSalt, &cRounds);
    AssertReturn(pszSalt != NULL, VERR_INVALID_PARAMETER);
    AssertReturn(cchSalt >= RT_SHACRYPT_SALT_MIN_LEN, VERR_BUFFER_UNDERFLOW);
    AssertReturn(cchSalt <= RT_SHACRYPT_SALT_MAX_LEN, VERR_TOO_MUCH_DATA);
    AssertReturn(cRounds >= RT_SHACRYPT_ROUNDS_MIN && cRounds <= RT_SHACRYPT_ROUNDS_MAX, VERR_OUT_OF_RANGE);

    AssertPtrReturn(pszString, VERR_INVALID_POINTER);

    /*
     * Calc the necessary buffer space and check that the caller supplied enough.
     */
    char    szRounds[64];
    ssize_t cchRounds = 0;
    if (cRounds != RT_SHACRYPT_ROUNDS_DEFAULT)
    {
        cchRounds = RTStrFormatU32(szRounds, sizeof(szRounds), cRounds, 10, 0, 0, 0);
        Assert(cchRounds > 0 && cchRounds <= 9);
    }

    size_t const cchNeeded = sizeof(RT_SHACRYPT_ID_STR_512) - 1
                           + (cRounds != RT_SHACRYPT_ROUNDS_DEFAULT ? cchRounds + sizeof("rounds=$") - 1 : 0)
                           + cchSalt + 1
                           + RTSHA512_HASH_SIZE * 4 / 3
                           + 1;
    AssertReturn(cbString > cchNeeded, VERR_BUFFER_OVERFLOW);

    /*
     * Do the formatting.
     */
    memcpy(pszString, RT_STR_TUPLE(RT_SHACRYPT_ID_STR_512));
    size_t off = sizeof(RT_SHACRYPT_ID_STR_512) - 1;

    if (cRounds != RT_SHACRYPT_ROUNDS_DEFAULT)
    {
        memcpy(&pszString[off], RT_STR_TUPLE("rounds="));
        off += sizeof("rounds=") - 1;

        memcpy(&pszString[off], szRounds, cchRounds);
        off += cchRounds;
        pszString[off++] = '$';
    }

    memcpy(&pszString[off], pszSalt, cchSalt);
    off += cchSalt;
    pszString[off++] = '$';

    BASE64_ENCODE(pszString, off, pabHash[ 0], pabHash[21], pabHash[42], 4);
    BASE64_ENCODE(pszString, off, pabHash[22], pabHash[43], pabHash[ 1], 4);
    BASE64_ENCODE(pszString, off, pabHash[44], pabHash[ 2], pabHash[23], 4);
    BASE64_ENCODE(pszString, off, pabHash[ 3], pabHash[24], pabHash[45], 4);
    BASE64_ENCODE(pszString, off, pabHash[25], pabHash[46], pabHash[ 4], 4);
    BASE64_ENCODE(pszString, off, pabHash[47], pabHash[ 5], pabHash[26], 4);
    BASE64_ENCODE(pszString, off, pabHash[ 6], pabHash[27], pabHash[48], 4);
    BASE64_ENCODE(pszString, off, pabHash[28], pabHash[49], pabHash[ 7], 4);
    BASE64_ENCODE(pszString, off, pabHash[50], pabHash[ 8], pabHash[29], 4);
    BASE64_ENCODE(pszString, off, pabHash[ 9], pabHash[30], pabHash[51], 4);
    BASE64_ENCODE(pszString, off, pabHash[31], pabHash[52], pabHash[10], 4);
    BASE64_ENCODE(pszString, off, pabHash[53], pabHash[11], pabHash[32], 4);
    BASE64_ENCODE(pszString, off, pabHash[12], pabHash[33], pabHash[54], 4);
    BASE64_ENCODE(pszString, off, pabHash[34], pabHash[55], pabHash[13], 4);
    BASE64_ENCODE(pszString, off, pabHash[56], pabHash[14], pabHash[35], 4);
    BASE64_ENCODE(pszString, off, pabHash[15], pabHash[36], pabHash[57], 4);
    BASE64_ENCODE(pszString, off, pabHash[37], pabHash[58], pabHash[16], 4);
    BASE64_ENCODE(pszString, off, pabHash[59], pabHash[17], pabHash[38], 4);
    BASE64_ENCODE(pszString, off, pabHash[18], pabHash[39], pabHash[60], 4);
    BASE64_ENCODE(pszString, off, pabHash[40], pabHash[61], pabHash[19], 4);
    BASE64_ENCODE(pszString, off, pabHash[62], pabHash[20], pabHash[41], 4);
    BASE64_ENCODE(pszString, off,           0,           0, pabHash[63], 2);

    pszString[off] = '\0';
    Assert(off < cbString);

    return VINF_SUCCESS;
}

