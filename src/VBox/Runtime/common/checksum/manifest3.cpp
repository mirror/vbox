/* $Id$ */
/** @file
 * IPRT - Manifest, the bits with the most dependencies.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "internal/iprt.h"
#include <iprt/manifest.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/md5.h>
#include <iprt/mem.h>
#include <iprt/sha.h>
#include <iprt/string.h>
#include <iprt/vfs.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Hashes data.
 *
 * Used when hashing a file, stream or similar.
 */
typedef struct RTMANIFESTHASHES
{
    /** The desired attribute types.
     * Only the hashes indicated by this will be calculated. */
    uint32_t        fAttrs;
    /** The size. */
    RTFOFF          cbStream;

    /** The MD5 context.  */
    RTMD5CONTEXT    Md5Ctx;
    /** The SHA-1 context.  */
    RTSHA1CONTEXT   Sha1Ctx;
    /** The SHA-256 context.  */
    RTSHA256CONTEXT Sha256Ctx;
    /** The SHA-512 context.  */
    RTSHA512CONTEXT Sha512Ctx;

    /** The MD5 digest. */
    uint8_t         abMd5Digest[RTMD5_HASH_SIZE];
    /** The SHA-1 digest. */
    uint8_t         abSha1Digest[RTSHA1_HASH_SIZE];
    /** The SHA-256 digest. */
    uint8_t         abSha256Digest[RTSHA256_HASH_SIZE];
    /** The SHA-512 digest. */
    uint8_t         abSha512Digest[RTSHA512_HASH_SIZE];
} RTMANIFESTHASHES;
/** Pointer to a the hashes for a stream. */
typedef RTMANIFESTHASHES *PRTMANIFESTHASHES;


/**
 * Creates a hashes structure.
 *
 * @returns Pointer to a hashes structure.
 * @param   fAttrs              The desired hashes, RTMANIFEST_ATTR_XXX.
 */
static PRTMANIFESTHASHES rtManifestHashesCreate(uint32_t fAttrs)
{
    PRTMANIFESTHASHES pHashes = (PRTMANIFESTHASHES)RTMemTmpAllocZ(sizeof(*pHashes));
    if (pHashes)
    {
        pHashes->fAttrs     = fAttrs;
        /*pHashes->cbStream   = 0;*/
        if (fAttrs & RTMANIFEST_ATTR_MD5)
            RTMd5Init(&pHashes->Md5Ctx);
        if (fAttrs & RTMANIFEST_ATTR_SHA1)
            RTSha1Init(&pHashes->Sha1Ctx);
        if (fAttrs & RTMANIFEST_ATTR_SHA256)
            RTSha256Init(&pHashes->Sha256Ctx);
        if (fAttrs & RTMANIFEST_ATTR_SHA512)
            RTSha512Init(&pHashes->Sha512Ctx);
    }
    return pHashes;
}


/**
 * Updates the hashes with a block of data.
 *
 * @param   pHashes             The hashes structure.
 * @param   pvBuf               The data block.
 * @param   cbBuf               The size of the data block.
 */
static void rtManifestHashesUpdate(PRTMANIFESTHASHES pHashes, void const *pvBuf, size_t cbBuf)
{
    pHashes->cbStream += cbBuf;
    if (pHashes->fAttrs & RTMANIFEST_ATTR_MD5)
        RTMd5Update(&pHashes->Md5Ctx, pvBuf, cbBuf);
    if (pHashes->fAttrs & RTMANIFEST_ATTR_SHA1)
        RTSha1Update(&pHashes->Sha1Ctx, pvBuf, cbBuf);
    if (pHashes->fAttrs & RTMANIFEST_ATTR_SHA256)
        RTSha256Update(&pHashes->Sha256Ctx, pvBuf, cbBuf);
    if (pHashes->fAttrs & RTMANIFEST_ATTR_SHA512)
        RTSha512Update(&pHashes->Sha512Ctx, pvBuf, cbBuf);
}


/**
 * Finalizes all the hashes.
 *
 * @param   pHashes             The hashes structure.
 */
static void rtManifestHashesFinal(PRTMANIFESTHASHES pHashes)
{
    if (pHashes->fAttrs & RTMANIFEST_ATTR_MD5)
        RTMd5Final(pHashes->abMd5Digest, &pHashes->Md5Ctx);
    if (pHashes->fAttrs & RTMANIFEST_ATTR_SHA1)
        RTSha1Final(&pHashes->Sha1Ctx, pHashes->abSha1Digest);
    if (pHashes->fAttrs & RTMANIFEST_ATTR_SHA256)
        RTSha256Final(&pHashes->Sha256Ctx, pHashes->abSha256Digest);
    if (pHashes->fAttrs & RTMANIFEST_ATTR_SHA512)
        RTSha512Final(&pHashes->Sha512Ctx, pHashes->abSha512Digest);
}


/**
 * Adds the hashes to a manifest entry.
 *
 * @returns IPRT status code.
 * @param   pHashes             The hashes structure.
 * @param   hManifest           The manifest to add them to.
 * @param   pszEntry            The entry name.
 */
static int rtManifestHashesSetAttrs(PRTMANIFESTHASHES pHashes, RTMANIFEST hManifest, const char *pszEntry)
{
    char szValue[RTSHA512_DIGEST_LEN + 8];
    int rc = VINF_SUCCESS;
    int rc2;

    if (pHashes->fAttrs & RTMANIFEST_ATTR_SIZE)
    {
        RTStrPrintf(szValue, sizeof(szValue), "%RU64", (uint64_t)pHashes->cbStream);
        rc2 = RTManifestEntrySetAttr(hManifest, pszEntry, "SIZE", szValue, RTMANIFEST_ATTR_SIZE);
        if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
            rc = rc2;
    }

    if (pHashes->fAttrs & RTMANIFEST_ATTR_MD5)
    {
        rc2 = RTMd5ToString(pHashes->abMd5Digest, szValue, sizeof(szValue));
        if (RT_SUCCESS(rc2))
            rc2 = RTManifestEntrySetAttr(hManifest, pszEntry, "MD5", szValue, RTMANIFEST_ATTR_MD5);
        if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
            rc = rc2;
    }

    if (pHashes->fAttrs & RTMANIFEST_ATTR_SHA1)
    {
        rc2 = RTSha1ToString(pHashes->abSha1Digest, szValue, sizeof(szValue));
        if (RT_SUCCESS(rc2))
            rc2 = RTManifestEntrySetAttr(hManifest, pszEntry, "SHA1", szValue, RTMANIFEST_ATTR_SHA1);
        if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
            rc = rc2;
    }

    if (pHashes->fAttrs & RTMANIFEST_ATTR_SHA256)
    {
        rc2 = RTSha256ToString(pHashes->abSha256Digest, szValue, sizeof(szValue));
        if (RT_SUCCESS(rc2))
            rc2 = RTManifestEntrySetAttr(hManifest, pszEntry, "SHA256", szValue, RTMANIFEST_ATTR_SHA256);
        if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
            rc = rc2;
    }

    if (pHashes->fAttrs & RTMANIFEST_ATTR_SHA512)
    {
        rc2 = RTSha512ToString(pHashes->abSha512Digest, szValue, sizeof(szValue));
        if (RT_SUCCESS(rc2))
            rc2 = RTManifestEntrySetAttr(hManifest, pszEntry, "SHA512", szValue, RTMANIFEST_ATTR_SHA512);
        if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


/**
 * Destroys the hashes.
 *
 * @param   pHashes             The hashes structure.  NULL is ignored.
 */
static void rtManifestHashesDestroy(PRTMANIFESTHASHES pHashes)
{
    RTMemTmpFree(pHashes);
}


/**
 * Adds an entry for a file with the specified set of attributes.
 *
 * @returns IPRT status code.
 *
 * @param   hManifest           The manifest handle.
 * @param   hVfsIos             The I/O stream handle of the entry.  This will
 *                              be processed to its end on successful return.
 *                              (Must be positioned at the start to get
 *                              the expected results.)
 * @param   pszEntry            The entry name.
 * @param   fAttrs              The attributes to create for this stream.
 */
RTDECL(int) RTManifestEntryAddIoStream(RTMANIFEST hManifest, RTVFSIOSTREAM hVfsIos, const char *pszEntry, uint32_t fAttrs)
{
    /*
     * Note! This is a convenicence function, so just use the available public
     *       methods to get the job done.
     */
    AssertReturn(fAttrs < RTMANIFEST_ATTR_END, VERR_INVALID_PARAMETER);
    AssertPtr(pszEntry);

    /*
     * Allocate and initialize the hash contexts, hash digests and I/O buffer.
     */
    PRTMANIFESTHASHES pHashes = rtManifestHashesCreate(fAttrs);
    if (!pHashes)
        return VERR_NO_TMP_MEMORY;

    int         rc;
    size_t      cbBuf = _1M;
    void       *pvBuf = RTMemTmpAlloc(cbBuf);
    if (RT_UNLIKELY(!pvBuf))
    {
        cbBuf = _4K;
        pvBuf = RTMemTmpAlloc(cbBuf);
    }
    if (RT_LIKELY(pvBuf))
    {
        /*
         * Process the stream data.
         */
        for (;;)
        {
            size_t cbRead;
            rc = RTVfsIoStrmRead(hVfsIos, pvBuf, cbBuf, true /*fBlocking*/, &cbRead);
            if (   (rc == VINF_EOF && cbRead == 0)
                || RT_FAILURE(rc))
                break;
            rtManifestHashesUpdate(pHashes, pvBuf, cbBuf);
        }
        if (RT_SUCCESS(rc))
        {
            /*
             * Add the entry with the finalized hashes.
             */
            rtManifestHashesFinal(pHashes);
            rc = RTManifestEntryAdd(hManifest, pszEntry);
            if (RT_SUCCESS(rc))
                rc = rtManifestHashesSetAttrs(pHashes, hManifest, pszEntry);
        }
    }
    else
    {
        rtManifestHashesDestroy(pHashes);
        rc = VERR_NO_TMP_MEMORY;
    }
    return rc;
}



