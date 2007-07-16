/** @file
 * innotek Portable Runtime - Message-Digest algorithm 5.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef ___iprt_md5_h
#define ___iprt_md5_h

#include <iprt/types.h>

/** @defgroup grp_rt_md5    RTMd5 - Message-Digest algorithm 5
 * @ingroup grp_rt
 * @{
 */

/** Size of a MD5 hash. */
#define RTMD5HASHSIZE 16

/**
 * MD5 hash algorithm context.
 */
typedef struct RTMD5CONTEXT
{
    uint32_t buf[4];
    uint32_t bits[2];
    uint32_t in[16];
} RTMD5CONTEXT;

/** Pointer to MD5 hash algorithm context. */
typedef RTMD5CONTEXT *PRTMD5CONTEXT;

__BEGIN_DECLS

/**
 * Initialize MD5 context.
 *
 * @param   pCtx        Pointer to the MD5 context to initialize.
 */
RTDECL(void) RTMd5Init(PRTMD5CONTEXT pCtx);

/**
 * Feed data into the MD5 computation.
 *
 * @param   pCtx        Pointer to the MD5 context.
 * @param   pvBuf       Pointer to data.
 * @param   cbBuf       Length of data (in bytes).
 */
RTDECL(void) RTMd5Update(PRTMD5CONTEXT pCtx, const void *pvBuf, size_t cbBuf);

/**
 * Compute the MD5 hash of the data.
 *
 * @param   pabDigest  Where to store the hash.
 *                     (What's passed is a pointer to the caller's buffer.)
 * @param   pCtx       Pointer to the MD5 context.
 */
RTDECL(void) RTMd5Final(uint8_t pabDigest[RTMD5HASHSIZE], PRTMD5CONTEXT pCtx);

__END_DECLS

/** @} */

#endif /* !__iprt_md5_h__ */
