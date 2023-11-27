/* $Id$ */
/** @file
 * IPRT - Crypto - SHA-crypt.
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
#include <iprt/types.h>
#include <iprt/mem.h>
#include <iprt/rand.h>
#include <iprt/sha.h>
#include <iprt/string.h>



RTR3DECL(int) RTCrShaCryptGenerateSalt(char szSalt[RT_SHACRYPT_MAX_SALT_LEN + 1], size_t cchSalt)
{
    AssertMsgReturn(cchSalt >= RT_SHACRYPT_MIN_SALT_LEN && cchSalt <= RT_SHACRYPT_MAX_SALT_LEN, ("len=%zu\n", cchSalt),
                    VERR_INVALID_PARAMETER);

    static const char aRange[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890./";
    for (size_t i = 0; i < cchSalt; i++)
        szSalt[i] = aRange[RTRandU32Ex(0, sizeof(aRange) - 2)];

    szSalt[RT_SHACRYPT_MAX_SALT_LEN] = '\0';
    return VINF_SUCCESS;
}


RTR3DECL(int) RTCrShaCrypt256(const char *pszKey, const char *pszSalt, uint32_t cRounds, uint8_t abHash[RTSHA256_HASH_SIZE])
{
    AssertPtrReturn(pszKey,   VERR_INVALID_POINTER);
    AssertPtrReturn(pszSalt,  VERR_INVALID_POINTER);
    AssertReturn   (cRounds, VERR_INVALID_PARAMETER);

    size_t const cbKey     = strlen(pszKey);
    AssertReturn(cbKey, VERR_INVALID_PARAMETER);
    size_t const cbSalt    = strlen(pszSalt);
    AssertMsgReturn(cbSalt >= RT_SHACRYPT_MIN_SALT_LEN && cbSalt <= RT_SHACRYPT_MAX_SALT_LEN, ("len=%zu\n", cbSalt),
                    VERR_INVALID_PARAMETER);

    uint8_t abDigest[RTSHA256_HASH_SIZE];
    uint8_t abDigestTemp[RTSHA256_HASH_SIZE];

    RTSHA256CONTEXT Ctx;
    RTSha256Init(&Ctx);                                                         /* Step 1. */
    RTSha256Update(&Ctx, pszKey, cbKey);                                        /* Step 2. */
    RTSha256Update(&Ctx, pszSalt, cbSalt);                                      /* Step 3. */

    RTSHA256CONTEXT CtxAlt;
    RTSha256Init(&CtxAlt);                                                      /* Step 4. */
    RTSha256Update(&CtxAlt, pszKey, cbKey);                                     /* Step 5. */
    RTSha256Update(&CtxAlt, pszSalt, cbSalt);                                   /* Step 6. */
    RTSha256Update(&CtxAlt, pszKey, cbKey);                                     /* Step 7. */
    RTSha256Final(&CtxAlt, abDigest);                                           /* Step 8. */

    size_t i = cbKey;
    for (; i > RTSHA256_HASH_SIZE; i -= RTSHA256_HASH_SIZE)                     /* Step 9. */
        RTSha256Update(&Ctx, abDigest, sizeof(abDigest));
    RTSha256Update(&Ctx, abDigest, i);                                          /* Step 10. */

    size_t keyBits = cbKey;
    while (keyBits)                                                             /* Step 11. */
    {
        if ((keyBits & 1) != 0)
            RTSha256Update(&Ctx, abDigest, sizeof(abDigest));                   /* a) */
        else
            RTSha256Update(&Ctx, pszKey, cbKey);                                /* b) */
        keyBits >>= 1;
    }

    RTSha256Final(&Ctx, abDigest);                                              /* Step 12. */

    RTSha256Init(&CtxAlt);                                                      /* Step 13. */
    for (i = 0; i < cbKey; i++)                                                 /* Step 14. */
        RTSha256Update(&CtxAlt, pszKey, cbKey);
    RTSha256Final(&CtxAlt, abDigestTemp);                                       /* Step 15. */

    /*
     * Byte sequence P (= password).
     */
    size_t const cbSeqP  = cbKey;
    uint8_t     *pabSeqP = (uint8_t *)RTMemDup(pszKey, cbSeqP);
    uint8_t     *p       = pabSeqP;
    AssertPtrReturn(pabSeqP, VERR_NO_MEMORY);

    for (i = cbSeqP; i > RTSHA256_HASH_SIZE; i -= RTSHA256_HASH_SIZE)           /* Step 16. */
    {
        memcpy(p, (void *)abDigestTemp, sizeof(abDigestTemp));                  /* a) */
        p += RTSHA256_HASH_SIZE;
    }
    memcpy(p, abDigestTemp, i);                                                 /* b) */

    RTSha256Init(&CtxAlt);                                                      /* Step 17. */

    for (i = 0; i < 16 + (unsigned)abDigest[0]; i++)                            /* Step 18. */
        RTSha256Update(&CtxAlt, pszSalt, cbSalt);

    RTSha256Final(&CtxAlt, abDigestTemp);                                       /* Step 19. */

    /*
     * Byte sequence S (= salt).
     */
    size_t   const cbSeqS  = cbSalt;
    uint8_t       *pabSeqS = (uint8_t *)RTMemDup(pszSalt, cbSeqS);
                   p       = pabSeqS;
    AssertPtrReturn(pabSeqS, VERR_NO_MEMORY);

    for (i = cbSeqS; i > RTSHA256_HASH_SIZE; i -= RTSHA256_HASH_SIZE)           /* Step 20. */
    {
        memcpy(p, (void *)abDigestTemp, sizeof(abDigestTemp));                  /* a) */
        p += RTSHA256_HASH_SIZE;
    }
    memcpy(p, abDigestTemp, i);                                                 /* b) */

    /* Step 21. */
    for (uint32_t r = 0; r < cRounds; r++)
    {
        RTSHA256CONTEXT CtxC;
        RTSha256Init(&CtxC);                                                    /* a) */

        if ((r & 1) != 0)
            RTSha256Update(&CtxC, pabSeqP, cbSeqP);                             /* b) */
        else
            RTSha256Update(&CtxC, abDigest, sizeof(abDigest));                  /* c) */

        if (r % 3 != 0)                                                         /* d) */
            RTSha256Update(&CtxC, pabSeqS, cbSeqS);

        if (r % 7 != 0)
            RTSha256Update(&CtxC, pabSeqP, cbSeqP);                             /* e) */

        if ((r & 1) != 0)
            RTSha256Update(&CtxC, abDigest, sizeof(abDigest));                  /* f) */
        else
            RTSha256Update(&CtxC, pabSeqP, cbSeqP);                             /* g) */

        RTSha256Final(&CtxC, abDigest);                                         /* h) */
    }

    memcpy(abHash, abDigest, RTSHA256_HASH_SIZE);

    RTMemWipeThoroughly(abDigestTemp, RTSHA256_HASH_SIZE, 3);
    RTMemWipeThoroughly(pabSeqP, cbSeqP, 3);
    RTMemWipeThoroughly(pabSeqP, cbSeqP, 3);
    RTMemFree(pabSeqP);
    RTMemWipeThoroughly(pabSeqS, cbSeqS, 3);
    RTMemFree(pabSeqS);

    return VINF_SUCCESS;
}


RTR3DECL(int) RTCrShaCrypt256ToString(uint8_t abHash[RTSHA256_HASH_SIZE], const char *pszSalt, uint32_t cRounds,
                                      char *pszString, size_t cchString)
{
    AssertPtrReturn(pszSalt,   VERR_INVALID_POINTER);
    AssertReturn   (cRounds,   VERR_INVALID_PARAMETER);
    AssertReturn   (cchString >= RTSHA256_DIGEST_LEN + 1, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszString, VERR_INVALID_POINTER);

    char  *psz = pszString;
    size_t cch = cchString;

    *psz = '\0';

    size_t cchPrefix;
    if (cRounds == RT_SHACRYPT_DEFAULT_ROUNDS)
        cchPrefix = RTStrPrintf2(psz, cchString, "$5$%s$", pszSalt);
    else
        cchPrefix = RTStrPrintf2(psz, cchString, "$5$rounds=%RU32$%s$", cRounds, pszSalt);
    AssertReturn(cchPrefix > 0, VERR_BUFFER_OVERFLOW);
    AssertReturn(cch >= cchPrefix, VERR_BUFFER_OVERFLOW);
    cch -= cchPrefix;
    psz += cchPrefix;

    /* Make sure that there is enough room to store the base64-encoded hash. */
    AssertReturn(cch >= ((RTSHA256_HASH_SIZE / 3) * 4) + 1, VERR_BUFFER_OVERFLOW);

    static const char acBase64[64 + 1] =
        "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

#define BASE64_ENCODE(a_Val2, a_Val1, a_Val0, a_Cnt) \
    do { \
        unsigned int w = ((a_Val2) << 16) | ((a_Val1) << 8) | (a_Val0); \
        int n = (a_Cnt); \
        while (n-- > 0 && cch > 0) \
        { \
            *psz++ = acBase64[w & 0x3f]; \
            --cch; \
            w >>= 6; \
        } \
    } while (0)

    BASE64_ENCODE(abHash[0],  abHash[10], abHash[20], 4);
    BASE64_ENCODE(abHash[21], abHash[1],  abHash[11], 4);
    BASE64_ENCODE(abHash[12], abHash[22], abHash[2], 4);
    BASE64_ENCODE(abHash[3],  abHash[13], abHash[23], 4);
    BASE64_ENCODE(abHash[24], abHash[4],  abHash[14], 4);
    BASE64_ENCODE(abHash[15], abHash[25], abHash[5], 4);
    BASE64_ENCODE(abHash[6],  abHash[16], abHash[26], 4);
    BASE64_ENCODE(abHash[27], abHash[7],  abHash[17], 4);
    BASE64_ENCODE(abHash[18], abHash[28], abHash[8], 4);
    BASE64_ENCODE(abHash[9],  abHash[19], abHash[29], 4);
    BASE64_ENCODE(0,          abHash[31], abHash[30], 3);

#undef BASE64_ENCODE

    if (cch)
        *psz = '\0';

    return VINF_SUCCESS;
}


RTR3DECL(int) RTCrShaCrypt512(const char *pszKey, const char *pszSalt, uint32_t cRounds, uint8_t abHash[RTSHA512_HASH_SIZE])
{
    AssertPtrReturn(pszKey,   VERR_INVALID_POINTER);
    AssertPtrReturn(pszSalt,  VERR_INVALID_POINTER);
    AssertReturn   (cRounds, VERR_INVALID_PARAMETER);

    size_t const cbKey     = strlen(pszKey);
    AssertReturn(cbKey, VERR_INVALID_PARAMETER);
    size_t const cbSalt    = strlen(pszSalt);
    AssertMsgReturn(cbSalt >= RT_SHACRYPT_MIN_SALT_LEN && cbSalt <= RT_SHACRYPT_MAX_SALT_LEN, ("len=%zu\n", cbSalt),
                    VERR_INVALID_PARAMETER);

    uint8_t abDigest[RTSHA512_HASH_SIZE];
    uint8_t abDigestTemp[RTSHA512_HASH_SIZE];

    RTSHA512CONTEXT Ctx;
    RTSha512Init(&Ctx);                                                         /* Step 1. */
    RTSha512Update(&Ctx, pszKey, cbKey);                                        /* Step 2. */
    RTSha512Update(&Ctx, pszSalt, cbSalt);                                      /* Step 3. */

    RTSHA512CONTEXT CtxAlt;
    RTSha512Init(&CtxAlt);                                                      /* Step 4. */
    RTSha512Update(&CtxAlt, pszKey, cbKey);                                     /* Step 5. */
    RTSha512Update(&CtxAlt, pszSalt, cbSalt);                                   /* Step 6. */
    RTSha512Update(&CtxAlt, pszKey, cbKey);                                     /* Step 7. */
    RTSha512Final(&CtxAlt, abDigest);                                           /* Step 8. */

    size_t i = cbKey;
    for (; i > RTSHA512_HASH_SIZE; i -= RTSHA512_HASH_SIZE)                     /* Step 9. */
        RTSha512Update(&Ctx, abDigest, sizeof(abDigest));
    RTSha512Update(&Ctx, abDigest, i);                                          /* Step 10. */

    size_t keyBits = cbKey;
    while (keyBits)                                                             /* Step 11. */
    {
        if ((keyBits & 1) != 0)
            RTSha512Update(&Ctx, abDigest, sizeof(abDigest));                   /* a) */
        else
            RTSha512Update(&Ctx, pszKey, cbKey);                                /* b) */
        keyBits >>= 1;
    }

    RTSha512Final(&Ctx, abDigest);                                              /* Step 12. */

    RTSha512Init(&CtxAlt);                                                      /* Step 13. */
    for (i = 0; i < cbKey; i++)                                                 /* Step 14. */
        RTSha512Update(&CtxAlt, pszKey, cbKey);
    RTSha512Final(&CtxAlt, abDigestTemp);                                       /* Step 15. */

    /*
     * Byte sequence P (= password).
     */
    size_t const cbSeqP  = cbKey;
    uint8_t     *pabSeqP = (uint8_t *)RTMemDup(pszKey, cbSeqP);
    uint8_t     *p       = pabSeqP;
    AssertPtrReturn(pabSeqP, VERR_NO_MEMORY);

    for (i = cbSeqP; i > RTSHA512_HASH_SIZE; i -= RTSHA512_HASH_SIZE)           /* Step 16. */
    {
        memcpy(p, (void *)abDigestTemp, sizeof(abDigestTemp));                  /* a) */
        p += RTSHA512_HASH_SIZE;
    }
    memcpy(p, abDigestTemp, i);                                                 /* b) */

    RTSha512Init(&CtxAlt);                                                      /* Step 17. */

    for (i = 0; i < 16 + (unsigned)abDigest[0]; i++)                            /* Step 18. */
        RTSha512Update(&CtxAlt, pszSalt, cbSalt);

    RTSha512Final(&CtxAlt, abDigestTemp);                                       /* Step 19. */

    /*
     * Byte sequence S (= salt).
     */
    size_t   const cbSeqS  = cbSalt;
    uint8_t       *pabSeqS = (uint8_t *)RTMemDup(pszSalt, cbSeqS);
                   p       = pabSeqS;
    AssertPtrReturn(pabSeqS, VERR_NO_MEMORY);

    for (i = cbSeqS; i > RTSHA512_HASH_SIZE; i -= RTSHA512_HASH_SIZE)           /* Step 20. */
    {
        memcpy(p, (void *)abDigestTemp, sizeof(abDigestTemp));                  /* a) */
        p += RTSHA512_HASH_SIZE;
    }
    memcpy(p, abDigestTemp, i);                                                 /* b) */

    /* Step 21. */
    for (uint32_t r = 0; r < cRounds; r++)
    {
        RTSHA512CONTEXT CtxC;
        RTSha512Init(&CtxC);                                                    /* a) */

        if ((r & 1) != 0)
            RTSha512Update(&CtxC, pabSeqP, cbSeqP);                             /* b) */
        else
            RTSha512Update(&CtxC, abDigest, sizeof(abDigest));                  /* c) */

        if (r % 3 != 0)                                                         /* d) */
            RTSha512Update(&CtxC, pabSeqS, cbSeqS);

        if (r % 7 != 0)
            RTSha512Update(&CtxC, pabSeqP, cbSeqP);                             /* e) */

        if ((r & 1) != 0)
            RTSha512Update(&CtxC, abDigest, sizeof(abDigest));                  /* f) */
        else
            RTSha512Update(&CtxC, pabSeqP, cbSeqP);                             /* g) */

        RTSha512Final(&CtxC, abDigest);                                         /* h) */
    }

    memcpy(abHash, abDigest, RTSHA512_HASH_SIZE);

    RTMemWipeThoroughly(abDigestTemp, RTSHA512_HASH_SIZE, 3);
    RTMemWipeThoroughly(pabSeqP, cbSeqP, 3);
    RTMemWipeThoroughly(pabSeqP, cbSeqP, 3);
    RTMemFree(pabSeqP);
    RTMemWipeThoroughly(pabSeqS, cbSeqS, 3);
    RTMemFree(pabSeqS);

    return VINF_SUCCESS;
}


RTR3DECL(int) RTCrShaCrypt512ToString(uint8_t abHash[RTSHA512_HASH_SIZE], const char *pszSalt, uint32_t cRounds,
                                      char *pszString, size_t cchString)
{
    AssertPtrReturn(pszSalt,   VERR_INVALID_POINTER);
    AssertReturn   (cRounds,   VERR_INVALID_PARAMETER);
    AssertReturn   (cchString >= RTSHA512_DIGEST_LEN + 1, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszString, VERR_INVALID_POINTER);

    char  *psz = pszString;
    size_t cch = cchString;

    size_t cchPrefix;
    if (cRounds == RT_SHACRYPT_DEFAULT_ROUNDS)
        cchPrefix = RTStrPrintf2(psz, cchString, "$6$%s$", pszSalt);
    else
        cchPrefix = RTStrPrintf2(psz, cchString, "$6$rounds=%RU32$%s$", cRounds, pszSalt);
    AssertReturn(cchPrefix > 0, VERR_BUFFER_OVERFLOW);
    AssertReturn(cch >= cchPrefix, VERR_BUFFER_OVERFLOW);
    cch -= cchPrefix;
    psz += cchPrefix;

    /* Make sure that there is enough room to store the base64-encoded hash. */
    AssertReturn(cch >= ((RTSHA512_HASH_SIZE / 3) * 4) + 1, VERR_BUFFER_OVERFLOW);

    static const char acBase64[64 + 1] =
        "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

#define BASE64_ENCODE(a_Val2, a_Val1, a_Val0, a_Cnt) \
    do { \
        unsigned int w = ((a_Val2) << 16) | ((a_Val1) << 8) | (a_Val0); \
        int n = (a_Cnt); \
        while (n-- > 0 && cch > 0) \
        { \
            *psz++ = acBase64[w & 0x3f]; \
            --cch; \
            w >>= 6; \
        } \
    } while (0)

    BASE64_ENCODE(abHash[0],  abHash[21], abHash[42], 4);
    BASE64_ENCODE(abHash[22], abHash[43], abHash[1], 4);
    BASE64_ENCODE(abHash[44], abHash[2],  abHash[23], 4);
    BASE64_ENCODE(abHash[3],  abHash[24], abHash[45], 4);
    BASE64_ENCODE(abHash[25], abHash[46], abHash[4], 4);
    BASE64_ENCODE(abHash[47], abHash[5],  abHash[26], 4);
    BASE64_ENCODE(abHash[6],  abHash[27], abHash[48], 4);
    BASE64_ENCODE(abHash[28], abHash[49], abHash[7], 4);
    BASE64_ENCODE(abHash[50], abHash[8],  abHash[29], 4);
    BASE64_ENCODE(abHash[9],  abHash[30], abHash[51], 4);
    BASE64_ENCODE(abHash[31], abHash[52], abHash[10], 4);
    BASE64_ENCODE(abHash[53], abHash[11], abHash[32], 4);
    BASE64_ENCODE(abHash[12], abHash[33], abHash[54], 4);
    BASE64_ENCODE(abHash[34], abHash[55], abHash[13], 4);
    BASE64_ENCODE(abHash[56], abHash[14], abHash[35], 4);
    BASE64_ENCODE(abHash[15], abHash[36], abHash[57], 4);
    BASE64_ENCODE(abHash[37], abHash[58], abHash[16], 4);
    BASE64_ENCODE(abHash[59], abHash[17], abHash[38], 4);
    BASE64_ENCODE(abHash[18], abHash[39], abHash[60], 4);
    BASE64_ENCODE(abHash[40], abHash[61], abHash[19], 4);
    BASE64_ENCODE(abHash[62], abHash[20], abHash[41], 4);
    BASE64_ENCODE(0, 0, abHash[63], 2);

#undef BASE64_ENCODE

    if (cch)
        *psz = '\0';

    return VINF_SUCCESS;
}

