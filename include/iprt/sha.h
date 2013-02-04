/** @file
 * IPRT - SHA1 digest creation
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
 */

#ifndef ___iprt_sha_h
#define ___iprt_sha_h

#include <iprt/types.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_sha   RTSha - SHA Family of Hash Functions
 * @ingroup grp_rt
 * @{
 */

/** The size of a SHA-1 hash. */
#define RTSHA1_HASH_SIZE    20
/** The length of a SHA-1 digest string. The terminator is not included. */
#define RTSHA1_DIGEST_LEN   40

/**
 * SHA-1 context.
 */
typedef union RTSHA1CONTEXT
{
    uint8_t abPadding[ARCH_BITS == 32 ? 96 : 128];
#ifdef RT_SHA1_PRIVATE_CONTEXT
    SHA_CTX Private;
#endif
} RTSHA1CONTEXT;
/** Pointer to an SHA-1 context. */
typedef RTSHA1CONTEXT *PRTSHA1CONTEXT;

/**
 * Compute the SHA-1 hash of the data.
 *
 * @param   pvBuf       Pointer to the data.
 * @param   cbBuf       The amount of data (in bytes).
 * @param   pabDigest   Where to store the hash. (What is passed is a pointer to
 *                      the caller's buffer.)
 */
RTDECL(void) RTSha1(const void *pvBuf, size_t cbBuf, uint8_t pabDigest[RTSHA1_HASH_SIZE]);

/**
 * Initializes the SHA-1 context.
 *
 * @param   pCtx        Pointer to the SHA-1 context.
 */
RTDECL(void) RTSha1Init(PRTSHA1CONTEXT pCtx);

/**
 * Feed data into the SHA-1 computation.
 *
 * @param   pCtx        Pointer to the SHA-1 context.
 * @param   pvBuf       Pointer to the data.
 * @param   cbBuf       The length of the data (in bytes).
 */
RTDECL(void) RTSha1Update(PRTSHA1CONTEXT pCtx, const void *pvBuf, size_t cbBuf);

/**
 * Compute the SHA-1 hash of the data.
 *
 * @param   pCtx        Pointer to the SHA-1 context.
 * @param   pabDigest   Where to store the hash. (What is passed is a pointer to
 *                      the caller's buffer.)
 */
RTDECL(void) RTSha1Final(PRTSHA1CONTEXT pCtx, uint8_t pabDigest[RTSHA1_HASH_SIZE]);

/**
 * Converts a SHA-1 hash to a digest string.
 *
 * @returns IPRT status code.
 *
 * @param   pabDigest   The binary digest returned by RTSha1Final or RTSha1.
 * @param   pszDigest   Where to return the stringified digest.
 * @param   cchDigest   The size of the output buffer. Should be at least
 *                      RTSHA1_DIGEST_LEN + 1 bytes.
 */
RTDECL(int) RTSha1ToString(uint8_t const pabDigest[RTSHA1_HASH_SIZE], char *pszDigest, size_t cchDigest);

/**
 * Converts a SHA-1 hash to a digest string.
 *
 * @returns IPRT status code.
 *
 * @param   pszDigest   The stringified digest. Leading and trailing spaces are
 *                      ignored.
 * @param   pabDigest   Where to store the hash. (What is passed is a pointer to
 *                      the caller's buffer.)
 */
RTDECL(int) RTSha1FromString(char const *pszDigest, uint8_t pabDigest[RTSHA1_HASH_SIZE]);

/**
 * Creates a SHA1 digest for the given memory buffer.
 *
 * @returns iprt status code.
 *
 * @param   pvBuf                 Memory buffer to create a SHA1 digest for.
 * @param   cbBuf                 The amount of data (in bytes).
 * @param   ppszDigest            On success the SHA1 digest.
 * @param   pfnProgressCallback   optional callback for the progress indication
 * @param   pvUser                user defined pointer for the callback
 */
RTR3DECL(int) RTSha1Digest(void* pvBuf, size_t cbBuf, char **ppszDigest, PFNRTPROGRESS pfnProgressCallback, void *pvUser);

/**
 * Creates a SHA1 digest for the given file.
 *
 * @returns iprt status code.
 *
 * @param   pszFile               Filename to create a SHA1 digest for.
 * @param   ppszDigest            On success the SHA1 digest.
 * @param   pfnProgressCallback   optional callback for the progress indication
 * @param   pvUser                user defined pointer for the callback
 */
RTR3DECL(int) RTSha1DigestFromFile(const char *pszFile, char **ppszDigest, PFNRTPROGRESS pfnProgressCallback, void *pvUser);


/** The size of a SHA-256 hash. */
#define RTSHA256_HASH_SIZE      32
/** The length of a SHA-256 digest string. The terminator is not included. */
#define RTSHA256_DIGEST_LEN     64

/**
 * SHA-256 context.
 */
typedef union RTSHA256CONTEXT
{
    uint8_t abPadding[ARCH_BITS == 32 ? 112 : 160];
#ifdef RT_SHA256_PRIVATE_CONTEXT
    SHA256_CTX Private;
#endif
} RTSHA256CONTEXT;
/** Pointer to an SHA-256 context. */
typedef RTSHA256CONTEXT *PRTSHA256CONTEXT;

/**
 * Compute the SHA-256 hash of the data.
 *
 * @param   pvBuf       Pointer to the data.
 * @param   cbBuf       The amount of data (in bytes).
 * @param   pabDigest   Where to store the hash. (What is passed is a pointer to
 *                      the caller's buffer.)
 */
RTDECL(void) RTSha256(const void *pvBuf, size_t cbBuf, uint8_t pabDigest[RTSHA256_HASH_SIZE]);

/**
 * Initializes the SHA-256 context.
 *
 * @param   pCtx        Pointer to the SHA-256 context.
 */
RTDECL(void) RTSha256Init(PRTSHA256CONTEXT pCtx);

/**
 * Feed data into the SHA-256 computation.
 *
 * @param   pCtx        Pointer to the SHA-256 context.
 * @param   pvBuf       Pointer to the data.
 * @param   cbBuf       The length of the data (in bytes).
 */
RTDECL(void) RTSha256Update(PRTSHA256CONTEXT pCtx, const void *pvBuf, size_t cbBuf);

/**
 * Compute the SHA-256 hash of the data.
 *
 * @param   pCtx        Pointer to the SHA-256 context.
 * @param   pabDigest   Where to store the hash. (What is passed is a pointer to
 *                      the caller's buffer.)
 */
RTDECL(void) RTSha256Final(PRTSHA256CONTEXT pCtx, uint8_t pabDigest[RTSHA256_HASH_SIZE]);

/**
 * Converts a SHA-256 hash to a digest string.
 *
 * @returns IPRT status code.
 *
 * @param   pabDigest   The binary digest returned by RTSha256Final or RTSha256.
 * @param   pszDigest   Where to return the stringified digest.
 * @param   cchDigest   The size of the output buffer. Should be at least
 *                      RTSHA256_DIGEST_LEN + 1 bytes.
 */
RTDECL(int) RTSha256ToString(uint8_t const pabDigest[RTSHA256_HASH_SIZE], char *pszDigest, size_t cchDigest);

/**
 * Converts a SHA-256 hash to a digest string.
 *
 * @returns IPRT status code.
 *
 * @param   pszDigest   The stringified digest. Leading and trailing spaces are
 *                      ignored.
 * @param   pabDigest   Where to store the hash. (What is passed is a pointer to
 *                      the caller's buffer.)
 */
RTDECL(int) RTSha256FromString(char const *pszDigest, uint8_t pabDigest[RTSHA256_HASH_SIZE]);



/** The size of a SHA-512 hash. */
#define RTSHA512_HASH_SIZE      64
/** The length of a SHA-512 digest string. The terminator is not included. */
#define RTSHA512_DIGEST_LEN     128

/**
 * SHA-512 context.
 */
typedef union RTSHA512CONTEXT
{
    uint8_t abPadding[ARCH_BITS == 32 ? 216 : 256];
#ifdef RT_SHA512_PRIVATE_CONTEXT
    SHA512_CTX Private;
#endif
} RTSHA512CONTEXT;
/** Pointer to an SHA-512 context. */
typedef RTSHA512CONTEXT *PRTSHA512CONTEXT;

/**
 * Compute the SHA-512 hash of the data.
 *
 * @param   pvBuf       Pointer to the data.
 * @param   cbBuf       The amount of data (in bytes).
 * @param   pabDigest   Where to store the hash. (What is passed is a pointer to
 *                      the caller's buffer.)
 */
RTDECL(void) RTSha512(const void *pvBuf, size_t cbBuf, uint8_t pabDigest[RTSHA512_HASH_SIZE]);

/**
 * Initializes the SHA-512 context.
 *
 * @param   pCtx        Pointer to the SHA-512 context.
 */
RTDECL(void) RTSha512Init(PRTSHA512CONTEXT pCtx);

/**
 * Feed data into the SHA-512 computation.
 *
 * @param   pCtx        Pointer to the SHA-512 context.
 * @param   pvBuf       Pointer to the data.
 * @param   cbBuf       The length of the data (in bytes).
 */
RTDECL(void) RTSha512Update(PRTSHA512CONTEXT pCtx, const void *pvBuf, size_t cbBuf);

/**
 * Compute the SHA-512 hash of the data.
 *
 * @param   pCtx        Pointer to the SHA-512 context.
 * @param   pabDigest   Where to store the hash. (What is passed is a pointer to
 *                      the caller's buffer.)
 */
RTDECL(void) RTSha512Final(PRTSHA512CONTEXT pCtx, uint8_t pabDigest[RTSHA512_HASH_SIZE]);

/**
 * Converts a SHA-512 hash to a digest string.
 *
 * @returns IPRT status code.
 *
 * @param   pabDigest   The binary digest returned by RTSha512Final or RTSha512.
 * @param   pszDigest   Where to return the stringified digest.
 * @param   cchDigest   The size of the output buffer. Should be at least
 *                      RTSHA512_DIGEST_LEN + 1 bytes.
 */
RTDECL(int) RTSha512ToString(uint8_t const pabDigest[RTSHA512_HASH_SIZE], char *pszDigest, size_t cchDigest);

/**
 * Converts a SHA-512 hash to a digest string.
 *
 * @returns IPRT status code.
 *
 * @param   pszDigest   The stringified digest. Leading and trailing spaces are
 *                      ignored.
 * @param   pabDigest   Where to store the hash. (What is passed is a pointer to
 *                      the caller's buffer.)
 */
RTDECL(int) RTSha512FromString(char const *pszDigest, uint8_t pabDigest[RTSHA512_HASH_SIZE]);

/** @} */

RT_C_DECLS_END

#endif /* ___iprt_sha1_h */

