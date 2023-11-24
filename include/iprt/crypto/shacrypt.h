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


/** Default number of rounds for SHA-crypt 256/512. */
#define RT_SHACRYPT_DEFAULT_ROUNDS 5000
/** Minimum salt length (in bytes) for SHA-crypt 256/512. */
#define RT_SHACRYPT_MIN_SALT_LEN   8
/** Maximum salt length (in bytes) for SHA-crypt 256/512. */
#define RT_SHACRYPT_MAX_SALT_LEN   16

RT_C_DECLS_BEGIN

RTDECL(int) RTShaCrypt256(const char *pszKey, const char *pszSalt, uint32_t cRounds, uint8_t pabHash[RTSHA256_HASH_SIZE]);
RTDECL(int) RTShaCrypt256ToString(uint8_t abHash[RTSHA256_HASH_SIZE], const char *pszSalt, uint32_t cRounds, char *pszString, size_t cbString);
RTDECL(int) RTShaCrypt512(const char *pszKey, const char *szSalt, uint32_t cRounds, uint8_t pabHash[RTSHA512_HASH_SIZE]);
RTDECL(int) RTShaCrypt512ToString(uint8_t abHash[RTSHA512_HASH_SIZE], const char *pszSalt, uint32_t cRounds, char *pszString, size_t cbString);

RT_C_DECLS_END

#endif /* !IPRT_INCLUDED_crypto_shacrypt_h */

