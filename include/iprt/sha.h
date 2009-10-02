/** @file
 * IPRT - SHA1 digest creation
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

#ifndef ___iprt_sha_h
#define ___iprt_sha_h

#include <iprt/types.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_sha   RTSha - SHA Family of Hash Functions
 * @ingroup grp_rt
 * @{
 */

/** The size of a SHA-1 hash. */
#define RTSHA1_HASH_SIZE  20

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
 * Creates a SHA1 digest for the given file.
 *
 * @returns iprt status code.
 *
 * @param   pszFile      Filename to create a SHA1 digest for.
 * @param   ppszDigest   On success the SHA1 digest.
 */
RTR3DECL(int) RTSha1Digest(const char *pszFile, char **ppszDigest);


/** The size of a SHA-256 hash. */
#define RTSHA256_HASH_SIZE  32

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


/** The size of a SHA-512 hash. */
#define RTSHA512_HASH_SIZE  64

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

/** @} */

RT_C_DECLS_END

#endif /* ___iprt_sha1_h */

