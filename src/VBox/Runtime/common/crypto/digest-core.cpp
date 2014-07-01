/* $Id$ */
/** @file
 * IPRT - Crypto - Cryptographic Hash / Message Digest API
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
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
#include <iprt/crypto/digest.h>

#include <iprt/asm.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/string.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Generic message digest instance.
 */
typedef struct RTCRDIGESTINT
{
    /** Magic value (RTCRDIGESTINT_MAGIC). */
    uint32_t            u32Magic;
    /** Reference counter. */
    uint32_t volatile   cRefs;
    /** Pointer to the message digest descriptor. */
    PCRTCRDIGESTDESC    pDesc;
    /** The offset into abState of the storage space .  At
     * least RTCRDIGESTDESC::cbHash bytes is available at that location. */
    uint32_t            offHash;
    /** State. */
    uint32_t            uState;
    /** The number of bytes consumed. */
    uint64_t            cbConsumed;
    /** Opaque data specific to the message digest algorithm, size given by
     * RTCRDIGESTDESC::cbState.  This is followed by space for the final hash at
     * offHash with size RTCRDIGESTDESC::cbHash. */
    uint8_t             abState[1];
} RTCRDIGESTINT;
/** Pointer to a message digest instance. */
typedef RTCRDIGESTINT *PRTCRDIGESTINT;

/** Magic value for RTCRDIGESTINT::u32Magic (Ralph C. Merkle). */
#define RTCRDIGESTINT_MAGIC         UINT32_C(0x19520202)

/** @name RTCRDIGESTINT::uState values.
 * @{ */
/** Ready for more data. */
#define RTCRDIGEST_STATE_READY      UINT32_C(1)
/** The hash has been finalized and can be found at offHash. */
#define RTCRDIGEST_STATE_FINAL      UINT32_C(2)
/** Busted state, can happen after re-init. */
#define RTCRDIGEST_STATE_BUSTED     UINT32_C(3)
/** @} */



RTDECL(int) RTCrDigestCreate(PRTCRDIGEST phDigest, PCRTCRDIGESTDESC pDesc, void *pvOpaque)
{
    AssertPtrReturn(phDigest, VERR_INVALID_POINTER);
    AssertPtrReturn(pDesc, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;
    uint32_t offHash = RT_ALIGN_32(pDesc->cbState, 8);
    PRTCRDIGESTINT pThis = (PRTCRDIGESTINT)RTMemAllocZ(RT_OFFSETOF(RTCRDIGESTINT, abState[offHash + pDesc->cbHash]));
    if (pThis)
    {
        pThis->u32Magic = RTCRDIGESTINT_MAGIC;
        pThis->cRefs    = 1;
        pThis->offHash  = offHash;
        pThis->pDesc    = pDesc;
        pThis->uState   = RTCRDIGEST_STATE_READY;
        if (pDesc->pfnInit)
            rc = pDesc->pfnInit(pThis->abState, pvOpaque, false /*fReInit*/);
        if (RT_SUCCESS(rc))
        {
            *phDigest = pThis;
            return VINF_SUCCESS;
        }
        pThis->u32Magic = 0;
        RTMemFree(pThis);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


RTDECL(int) RTCrDigestClone(PRTCRDIGEST phDigest, RTCRDIGEST hSrc)
{
    AssertPtrReturn(phDigest, VERR_INVALID_POINTER);
    AssertPtrReturn(hSrc, VERR_INVALID_HANDLE);
    AssertReturn(hSrc->u32Magic == RTCRDIGESTINT_MAGIC, VERR_INVALID_HANDLE);

    int rc = VINF_SUCCESS;
    uint32_t const offHash = hSrc->offHash;
    PRTCRDIGESTINT pThis = (PRTCRDIGESTINT)RTMemAllocZ(RT_OFFSETOF(RTCRDIGESTINT, abState[offHash + hSrc->pDesc->cbHash]));
    if (pThis)
    {
        pThis->u32Magic = RTCRDIGESTINT_MAGIC;
        pThis->cRefs    = 1;
        pThis->offHash  = offHash;
        pThis->pDesc    = hSrc->pDesc;
        if (hSrc->pDesc->pfnClone)
            rc = hSrc->pDesc->pfnClone(pThis->abState, hSrc->abState);
        else
            memcpy(pThis->abState, hSrc->abState, offHash);
        memcpy(&pThis->abState[offHash], &hSrc->abState[offHash], hSrc->pDesc->cbHash);
        pThis->uState = hSrc->uState;

        if (RT_SUCCESS(rc))
        {
            *phDigest = pThis;
            return VINF_SUCCESS;
        }
        pThis->u32Magic = 0;
        RTMemFree(pThis);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


RTDECL(int) RTCrDigestReset(RTCRDIGEST hDigest)
{
    PRTCRDIGESTINT pThis = hDigest;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTCRDIGESTINT_MAGIC, VERR_INVALID_HANDLE);

    pThis->cbConsumed = 0;
    pThis->uState = RTCRDIGEST_STATE_READY;

    int rc = VINF_SUCCESS;
    if (pThis->pDesc->pfnInit)
    {
        rc = pThis->pDesc->pfnInit(pThis->abState, NULL, true /*fReInit*/);
        if (RT_FAILURE(rc))
            pThis->uState = RTCRDIGEST_STATE_BUSTED;
        RT_BZERO(&pThis->abState[pThis->offHash], pThis->pDesc->cbHash);
    }
    else
        RT_BZERO(pThis->abState, pThis->offHash + pThis->pDesc->cbHash);
    return rc;
}


RTDECL(uint32_t) RTCrDigestRetain(RTCRDIGEST hDigest)
{
    PRTCRDIGESTINT pThis = hDigest;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTCRDIGESTINT_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    Assert(cRefs < 64);
    return cRefs;
}


RTDECL(uint32_t) RTCrDigestRelease(RTCRDIGEST hDigest)
{
    PRTCRDIGESTINT pThis = hDigest;
    if (pThis == NIL_RTCRDIGEST)
        return 0;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTCRDIGESTINT_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    if (!cRefs)
    {
        pThis->u32Magic = ~RTCRDIGESTINT_MAGIC;
        if (pThis->pDesc->pfnDelete)
            pThis->pDesc->pfnDelete(pThis->abState);
        RTMemFree(pThis);
    }
    Assert(cRefs < 64);
    return cRefs;
}


RTDECL(int) RTCrDigestUpdate(RTCRDIGEST hDigest, void const *pvData, size_t cbData)
{
    PRTCRDIGESTINT pThis = hDigest;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTCRDIGESTINT_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(pThis->uState == RTCRDIGEST_STATE_READY, VERR_INVALID_STATE);

    pThis->pDesc->pfnUpdate(pThis->abState, pvData, cbData);
    pThis->cbConsumed += cbData;
    return VINF_SUCCESS;
}


RTDECL(int) RTCrDigestFinal(RTCRDIGEST hDigest, void *pvHash, size_t cbHash)
{
    PRTCRDIGESTINT pThis = hDigest;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTCRDIGESTINT_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(pThis->uState == RTCRDIGEST_STATE_READY || pThis->uState == RTCRDIGEST_STATE_FINAL, VERR_INVALID_STATE);
    AssertPtrNullReturn(pvHash, VERR_INVALID_POINTER);

    /*
     * Make sure the hash calculation is final.
     */
    if (pThis->uState == RTCRDIGEST_STATE_READY)
    {
        pThis->pDesc->pfnFinal(pThis->abState, &pThis->abState[pThis->offHash]);
        pThis->uState = RTCRDIGEST_STATE_FINAL;
    }
    else
        AssertReturn(pThis->uState == RTCRDIGEST_STATE_FINAL, VERR_INVALID_STATE);

    /*
     * Copy out the hash if requested.
     */
    if (cbHash > 0)
    {
        uint32_t cbNeeded = pThis->pDesc->cbHash;
        if (pThis->pDesc->pfnGetHashSize)
            cbNeeded = pThis->pDesc->pfnGetHashSize(pThis->abState);
        Assert(cbNeeded > 0);

        if (cbNeeded == cbHash)
            memcpy(pvHash, &pThis->abState[pThis->offHash], cbNeeded);
        else if (cbNeeded > cbHash)
        {
            memcpy(pvHash, &pThis->abState[pThis->offHash], cbNeeded);
            memset((uint8_t *)pvHash + cbNeeded, 0, cbHash - cbNeeded);
            return VINF_BUFFER_UNDERFLOW;
        }
        else
        {
            memcpy(pvHash, &pThis->abState[pThis->offHash], cbHash);
            return VERR_BUFFER_OVERFLOW;
        }
    }

    return VINF_SUCCESS;
}


RTDECL(bool) RTCrDigestMatch(RTCRDIGEST hDigest, void const *pvHash, size_t cbHash)
{
    PRTCRDIGESTINT pThis = hDigest;

    int rc = RTCrDigestFinal(pThis, NULL, 0);
    AssertRCReturn(rc, false);

    AssertPtrReturn(pvHash, false);
    return pThis->pDesc->cbHash == cbHash
        && !memcmp(&pThis->abState[pThis->offHash], pvHash, cbHash);
}


RTDECL(uint8_t const *) RTCrDigestGetHash(RTCRDIGEST hDigest)
{
    PRTCRDIGESTINT pThis = hDigest;
    AssertPtrReturn(pThis, NULL);
    AssertReturn(pThis->u32Magic == RTCRDIGESTINT_MAGIC, NULL);

    int rc = RTCrDigestFinal(pThis, NULL, 0);
    AssertRCReturn(rc, NULL);

    return &pThis->abState[pThis->offHash];
}


RTDECL(uint32_t) RTCrDigestGetHashSize(RTCRDIGEST hDigest)
{
    PRTCRDIGESTINT pThis = hDigest;
    AssertPtrReturn(pThis, 0);
    AssertReturn(pThis->u32Magic == RTCRDIGESTINT_MAGIC, 0);
    if (pThis->pDesc->pfnGetHashSize)
    {
        uint32_t cbHash = pThis->pDesc->pfnGetHashSize(pThis->abState);
        Assert(cbHash <= pThis->pDesc->cbHash);
        return cbHash;
    }
    return pThis->pDesc->cbHash;
}


RTDECL(uint64_t) RTCrDigestGetConsumedSize(RTCRDIGEST hDigest)
{
    PRTCRDIGESTINT pThis = hDigest;
    AssertPtrReturn(pThis, 0);
    AssertReturn(pThis->u32Magic == RTCRDIGESTINT_MAGIC, 0);
    return pThis->cbConsumed;
}


RTDECL(bool) RTCrDigestIsFinalized(RTCRDIGEST hDigest)
{
    PRTCRDIGESTINT pThis = hDigest;
    AssertPtrReturn(pThis, false);
    AssertReturn(pThis->u32Magic == RTCRDIGESTINT_MAGIC, false);
    return pThis->uState == RTCRDIGEST_STATE_FINAL;
}


RTDECL(RTDIGESTTYPE) RTCrDigestGetType(RTCRDIGEST hDigest)
{
    PRTCRDIGESTINT pThis = hDigest;
    AssertPtrReturn(pThis, RTDIGESTTYPE_INVALID);
    AssertReturn(pThis->u32Magic == RTCRDIGESTINT_MAGIC, RTDIGESTTYPE_INVALID);

    RTDIGESTTYPE enmType = pThis->pDesc->enmType;
    if (pThis->pDesc->pfnGetDigestType)
        enmType = pThis->pDesc->pfnGetDigestType(pThis->abState);
    return enmType;
}

