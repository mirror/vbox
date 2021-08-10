/* $Id$ */
/** @file
 * IPRT, TPM common definitions (this is actually a protocol and not a format).
 */

/*
 * Copyright (C) 2021 Oracle Corporation
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

#ifndef IPRT_INCLUDED_formats_tpm_h
#define IPRT_INCLUDED_formats_tpm_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/asm.h>
#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/assertcompile.h>
#include <iprt/string.h>

#pragma pack(1)
/**
 * TPM request header (everything big endian).
 */
typedef struct TPMREQHDR
{
    /** The tag for this request. */
    uint16_t            u16Tag;
    /** Size of the request in bytes. */
    uint32_t            cbReq;
    /** The request ordinal to execute. */
    uint32_t            u32Ordinal;
} TPMREQHDR;
AssertCompileSize(TPMREQHDR, 2 + 4 + 4);
/** Pointer to a TPM request header. */
typedef TPMREQHDR *PTPMREQHDR;
/** Pointer to a const TPM request header. */
typedef const TPMREQHDR *PCTPMREQHDR;


/** @name TPM request ordinals.
 * @{ */
/** Perform a full self test. */
#define TPM_ORD_SELFTESTFULL                UINT32_C(80)
/** Continue the selftest. */
#define TPM_ORD_CONTINUESELFTEST            UINT32_C(83)
/** Return the test result. */
#define TPM_ORD_GETTESTRESULT               UINT32_C(84)
/** @} */


/**
 * TPM response header (everything big endian).
 */
typedef struct TPMRESPHDR
{
    /** The tag for this request. */
    uint16_t            u16Tag;
    /** Size of the response in bytes. */
    uint32_t            cbResp;
    /** The error code for the response. */
    uint32_t            u32ErrCode;
} TPMRESPHDR;
AssertCompileSize(TPMRESPHDR, 2 + 4 + 4);
/** Pointer to a TPM response header. */
typedef TPMRESPHDR *PTPMRESPHDR;
/** Pointer to a const TPM response header. */
typedef const TPMRESPHDR *PCTPMRESPHDR;


/** @name TPM status codes.
 * @{ */
/** Request executed successfully. */
#define TPM_SUCCESS                         UINT32_C(0)
/** Authentication failed. */
#define TPM_AUTHFAIL                        UINT32_C(1)
/** An index is malformed. */
#define TPM_BADINDEX                        UINT32_C(2)
/** A request parameter is invalid. */
#define TPM_BAD_PARAMETER                   UINT32_C(3)
/** The TPM failed to execute the request. */
#define TPM_FAIL                            UINT32_C(9)
/** @todo Extend as need arises. */
/** @} */


/* Some inline helpers to account for the unaligned members of the request and response headers. */

/**
 * Returns the request tag of the given TPM request header.
 *
 * @returns TPM request tag in bytes.
 * @param   pTpmReqHdr          Pointer to the TPM request header.
 */
DECLINLINE(uint16_t) RTTpmReqGetTag(PCTPMREQHDR pTpmReqHdr)
{
    return RT_BE2H_U16(pTpmReqHdr->u16Tag);
}


/**
 * Returns the request size of the given TPM request header.
 *
 * @returns TPM request size in bytes.
 * @param   pTpmReqHdr          Pointer to the TPM request header.
 */
DECLINLINE(size_t) RTTpmReqGetSz(PCTPMREQHDR pTpmReqHdr)
{
    uint32_t cbReq;
    memcpy(&cbReq, &pTpmReqHdr->cbReq, sizeof(pTpmReqHdr->cbReq));
    return RT_BE2H_U32(cbReq);
}


/**
 * Returns the request ordinal of the given TPM request header.
 *
 * @returns TPM request ordinal in bytes.
 * @param   pTpmReqHdr          Pointer to the TPM request header.
 */
DECLINLINE(uint32_t) RTTpmReqGetOrdinal(PCTPMREQHDR pTpmReqHdr)
{
    uint32_t u32Ordinal;
    memcpy(&u32Ordinal, &pTpmReqHdr->u32Ordinal, sizeof(pTpmReqHdr->u32Ordinal));
    return RT_BE2H_U32(u32Ordinal);
}


/**
 * Returns the response tag of the given TPM response header.
 *
 * @returns TPM request tag in bytes.
 * @param   pTpmRespHdr         Pointer to the TPM response header.
 */
DECLINLINE(uint16_t) RTTpmRespGetTag(PCTPMRESPHDR pTpmRespHdr)
{
    return RT_BE2H_U16(pTpmRespHdr->u16Tag);
}


/**
 * Returns the request size of the given TPM request header.
 *
 * @returns TPM response size in bytes.
 * @param   pTpmRespHdr         Pointer to the TPM response header.
 */
DECLINLINE(size_t) RTTpmRespGetSz(PCTPMRESPHDR pTpmRespHdr)
{
    uint32_t cbResp;
    memcpy(&cbResp, &pTpmRespHdr->cbResp, sizeof(pTpmRespHdr->cbResp));
    return RT_BE2H_U32(cbResp);
}


/**
 * Returns the request ordinal of the given TPM request header.
 *
 * @returns TPM response error code.
 * @param   pTpmRespHdr         Pointer to the TPM response header.
 */
DECLINLINE(uint32_t) RTTpmRespGetErrCode(PCTPMRESPHDR pTpmRespHdr)
{
    uint32_t u32ErrCode;
    memcpy(&u32ErrCode, &pTpmRespHdr->u32ErrCode, sizeof(pTpmRespHdr->u32ErrCode));
    return RT_BE2H_U32(u32ErrCode);
}

#pragma pack()

#endif /* !IPRT_INCLUDED_formats_tpm_h */

