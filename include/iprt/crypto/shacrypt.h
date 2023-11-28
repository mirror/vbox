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

#ifndef IPRT_INCLUDED_crypto_shacrypt_h
#define IPRT_INCLUDED_crypto_shacrypt_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/sha.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_rt_crshacrypt   RTCrShaCrypt - SHAcrypt functions
 * @ingroup grp_rt
 * @{
 */

/** Default number of rounds for SHA-crypt 256/512. */
#define RT_SHACRYPT_DEFAULT_ROUNDS 5000
/** Minimum salt length (in bytes) for SHA-crypt 256/512. */
#define RT_SHACRYPT_MIN_SALT_LEN   8
/** Maximum salt length (in bytes) for SHA-crypt 256/512. */
#define RT_SHACRYPT_MAX_SALT_LEN   16


/**
 * Creates a randomized salt for the RTCrShaCryptXXX functions.
 *
 * @returns IPRT status code.
 * @param   szSalt      Where to store the generated salt.
 * @param   cchSalt     Number of characters the generated salt should use.
 *                      Must be >= RT_SHACRYPT_MIN_SALT_LEN and <= RT_SHACRYPT_MAX_SALT_LEN.
 */
RTR3DECL(int) RTCrShaCryptGenerateSalt(char szSalt[RT_SHACRYPT_MAX_SALT_LEN + 1], size_t cchSalt);

/**
 * Creates a randomized salt for the RTCrShaCryptXXX functions, weak version. Can be overriden in local modules.
 *
 * @returns IPRT status code.
 * @param   szSalt      Where to store the generated salt.
 * @param   cchSalt     Number of characters the generated salt should use.
 *                      Must be >= RT_SHACRYPT_MIN_SALT_LEN and <= RT_SHACRYPT_MAX_SALT_LEN.
 */
#if defined(IN_RING3) && (defined(IN_RT_STATIC) || defined(IPRT_NO_CRT))
RTR3DECL(int) RTCrShaCryptGenerateSaltWeak(char szSalt[RT_SHACRYPT_MAX_SALT_LEN + 1], size_t cchSalt);
#else
# undef  RTCrShaCryptGenerateSaltWeak /* in case of mangling */
# define RTCrShaCryptGenerateSaltWeak RTCrShaCryptGenerateSalt
#endif

/**
 * Calculates a SHAcrypt (SHA-256) digest.
 *
 * @returns IPRT status code.
 * @param   pszKey              Key (password) to use.
 * @param   pszSalt             Salt to use.
 *                              Must be >= RT_SHACRYPT_MIN_SALT_LEN and <= RT_SHACRYPT_MAX_SALT_LEN.
 * @param   cRounds             Number of rounds to use.  @sa RT_SHACRYPT_DEFAULT_ROUNDS
 * @param   abHash              Where to return the hash on success.
 *
 * @note    This implements SHA-crypt.txt Version: 0.6 2016-8-31.
 */
RTR3DECL(int) RTCrShaCrypt256(const char *pszKey, const char *pszSalt, uint32_t cRounds, uint8_t abHash[RTSHA256_HASH_SIZE]);


/**
 * Returns a SHAcrypt (SHA-256) digest as a printable scheme.
 *
 * @returns IPRT status code.
 * @param   abHash              SHAcrypt (SHA-256) digest to return printable scheme for.
 * @param   pszSalt             Salt to use. Must match the salt used when generating \a pabHash via RTSha256Crypt().
 * @param   cRounds             Number of rounds used for generating \a pabHash.
 * @param   pszString           Where to store the printable string on success.
 * @param   cchString           Size of \a pszString.
 *                              Should be at least RTSHA256_DIGEST_LEN + 1 bytes.
 *
 * @note    This implements step 22 of SHA-crypt.txt Version: 0.6 2016-8-31.
 */
RTR3DECL(int) RTCrShaCrypt256ToString(uint8_t abHash[RTSHA256_HASH_SIZE], const char *pszSalt, uint32_t cRounds, char *pszString, size_t cchString);


/**
 * Calculates a SHAcrypt (SHA-512) digest.
 *
 * @returns IPRT status code.
 * @param   pszKey              Key (password) to use.
 * @param   pszSalt             Salt to use.
 *                              Must be >= RT_SHACRYPT_MIN_SALT_LEN and <= RT_SHACRYPT_MAX_SALT_LEN.
 * @param   cRounds             Number of rounds to use. @sa RT_SHACRYPT_DEFAULT_ROUNDS
 * @param   abHash              Where to return the hash on success.
 *
 * @note    This implements SHA-crypt.txt Version: 0.6 2016-8-31.
 */
RTR3DECL(int) RTCrShaCrypt512(const char *pszKey, const char *pszSalt, uint32_t cRounds, uint8_t abHash[RTSHA512_HASH_SIZE]);


/**
 * Returns a SHAcrypt (SHA-512) digest as a printable scheme.
 *
 * @returns IPRT status code.
 * @param   abHash              SHAcrypt (SHA-512) digest to return printable scheme for.
 * @param   pszSalt             Salt to use. Must match the salt used when generating \a pabHash via RTSha512Crypt().
 * @param   cRounds             Number of rounds used for generating \a pabHash.
 * @param   pszString           Where to store the printable string on success.
 * @param   cchString           Size of \a pszString.
 *                              Should be at least RTSHA512_DIGEST_LEN + 1 bytes.
 *
 * @note    This implements step 22 of SHA-crypt.txt Version: 0.6 2016-8-31.
 */
RTR3DECL(int) RTCrShaCrypt512ToString(uint8_t abHash[RTSHA512_HASH_SIZE], const char *pszSalt, uint32_t cRounds, char *pszString, size_t cchString);

/** @} */

RT_C_DECLS_END

#endif /* !IPRT_INCLUDED_crypto_shacrypt_h */

