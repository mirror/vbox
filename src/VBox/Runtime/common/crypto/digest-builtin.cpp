/* $Id$ */
/** @file
 * IPRT - Crypto - Cryptographic Hash / Message Digest API, Built-in providers.
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

#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/md2.h>
#include <iprt/md5.h>
#include <iprt/sha.h>
#include <iprt/crypto/pkix.h>

#ifdef IPRT_WITH_OPENSSL
# include "internal/iprt-openssl.h"
# include <openssl/evp.h>
#endif

/*
 * MD2
 */

/** @impl_interface_method{RTCRDIGESTDESC::pfnUpdate} */
static DECLCALLBACK(void) rtCrDigestMd2_Update(void *pvState, const void *pvData, size_t cbData)
{
    RTMd2Update((PRTMD2CONTEXT)pvState, pvData, cbData);
}

/** @impl_interface_method{RTCRDIGESTDESC::pfnFinal} */
static DECLCALLBACK(void) rtCrDigestMd2_Final(void *pvState, uint8_t *pbHash)
{
    RTMd2Final((PRTMD2CONTEXT)pvState, pbHash);
}

/** @impl_interface_method{RTCRDIGESTDESC::pfnInit} */
static DECLCALLBACK(int) rtCrDigestMd2_Init(void *pvState, void *pvOpaque, bool fReInit)
{
    AssertReturn(pvOpaque == NULL, VERR_INVALID_PARAMETER);
    RTMd2Init((PRTMD2CONTEXT)pvState);
    return VINF_SUCCESS;
}

/** MD2 alias ODIs. */
static const char * const g_apszMd2Aliases[] =
{
    RTCR_PKCS1_MD2_WITH_RSA_OID,
    "1.3.14.3.2.24" /* OIW md2WithRSASignature */,
    NULL
};

/** MD2 descriptor. */
static RTCRDIGESTDESC const g_rtCrDigestMd2Desc =
{
    "md2",
    "1.2.840.113549.2.2",
    g_apszMd2Aliases,
    RTDIGESTTYPE_MD2,
    RTMD2_HASH_SIZE,
    sizeof(RTMD2CONTEXT),
    0,
    rtCrDigestMd2_Update,
    rtCrDigestMd2_Final,
    rtCrDigestMd2_Init,
    NULL,
    NULL,
    NULL,
    NULL,
};


/*
 * MD5
 */

/** @impl_interface_method{RTCRDIGESTDESC::pfnUpdate} */
static DECLCALLBACK(void) rtCrDigestMd5_Update(void *pvState, const void *pvData, size_t cbData)
{
    RTMd5Update((PRTMD5CONTEXT)pvState, pvData, cbData);
}

/** @impl_interface_method{RTCRDIGESTDESC::pfnFinal} */
static DECLCALLBACK(void) rtCrDigestMd5_Final(void *pvState, uint8_t *pbHash)
{
    RTMd5Final(pbHash, (PRTMD5CONTEXT)pvState);
}

/** @impl_interface_method{RTCRDIGESTDESC::pfnInit} */
static DECLCALLBACK(int) rtCrDigestMd5_Init(void *pvState, void *pvOpaque, bool fReInit)
{
    AssertReturn(pvOpaque == NULL, VERR_INVALID_PARAMETER);
    RTMd5Init((PRTMD5CONTEXT)pvState);
    return VINF_SUCCESS;
}

/** MD5 alias ODIs. */
static const char * const g_apszMd5Aliases[] =
{
    RTCR_PKCS1_MD5_WITH_RSA_OID,
    "1.3.14.3.2.25" /* OIW md5WithRSASignature */,
    NULL
};

/** MD5 descriptor. */
static RTCRDIGESTDESC const g_rtCrDigestMd5Desc =
{
    "md5",
    "1.2.840.113549.2.5",
    g_apszMd5Aliases,
    RTDIGESTTYPE_MD5,
    RTMD5_HASH_SIZE,
    sizeof(RTMD5CONTEXT),
    0,
    rtCrDigestMd5_Update,
    rtCrDigestMd5_Final,
    rtCrDigestMd5_Init,
    NULL,
    NULL,
    NULL,
    NULL,
};


/*
 * SHA-1
 */

/** @impl_interface_method{RTCRDIGESTDESC::pfnUpdate} */
static DECLCALLBACK(void) rtCrDigestSha1_Update(void *pvState, const void *pvData, size_t cbData)
{
    RTSha1Update((PRTSHA1CONTEXT)pvState, pvData, cbData);
}

/** @impl_interface_method{RTCRDIGESTDESC::pfnFinal} */
static DECLCALLBACK(void) rtCrDigestSha1_Final(void *pvState, uint8_t *pbHash)
{
    RTSha1Final((PRTSHA1CONTEXT)pvState, pbHash);
}

/** @impl_interface_method{RTCRDIGESTDESC::pfnInit} */
static DECLCALLBACK(int) rtCrDigestSha1_Init(void *pvState, void *pvOpaque, bool fReInit)
{
    AssertReturn(pvOpaque == NULL, VERR_INVALID_PARAMETER);
    RTSha1Init((PRTSHA1CONTEXT)pvState);
    return VINF_SUCCESS;
}

/** SHA-1 alias ODIs. */
static const char * const g_apszSha1Aliases[] =
{
    RTCR_PKCS1_SHA1_WITH_RSA_OID,
    "1.3.14.3.2.29" /* OIW sha1WithRSASignature */,
    NULL
};

/** SHA-1 descriptor. */
static RTCRDIGESTDESC const g_rtCrDigestSha1Desc =
{
    "sha-1",
    "1.3.14.3.2.26",
    g_apszSha1Aliases,
    RTDIGESTTYPE_SHA1,
    RTSHA1_HASH_SIZE,
    sizeof(RTSHA1CONTEXT),
    0,
    rtCrDigestSha1_Update,
    rtCrDigestSha1_Final,
    rtCrDigestSha1_Init,
    NULL,
    NULL,
    NULL,
    NULL,
};


/*
 * SHA-256
 */

/** @impl_interface_method{RTCRDIGESTDESC::pfnUpdate} */
static DECLCALLBACK(void) rtCrDigestSha256_Update(void *pvState, const void *pvData, size_t cbData)
{
    RTSha256Update((PRTSHA256CONTEXT)pvState, pvData, cbData);
}

/** @impl_interface_method{RTCRDIGESTDESC::pfnFinal} */
static DECLCALLBACK(void) rtCrDigestSha256_Final(void *pvState, uint8_t *pbHash)
{
    RTSha256Final((PRTSHA256CONTEXT)pvState, pbHash);
}

/** @impl_interface_method{RTCRDIGESTDESC::pfnInit} */
static DECLCALLBACK(int) rtCrDigestSha256_Init(void *pvState, void *pvOpaque, bool fReInit)
{
    AssertReturn(pvOpaque == NULL, VERR_INVALID_PARAMETER);
    RTSha256Init((PRTSHA256CONTEXT)pvState);
    return VINF_SUCCESS;
}

/** SHA-256 alias ODIs. */
static const char * const g_apszSha256Aliases[] =
{
    RTCR_PKCS1_SHA256_WITH_RSA_OID,
    NULL
};

/** SHA-256 descriptor. */
static RTCRDIGESTDESC const g_rtCrDigestSha256Desc =
{
    "sha-256",
    "2.16.840.1.101.3.4.2.1",
    g_apszSha256Aliases,
    RTDIGESTTYPE_SHA256,
    RTSHA256_HASH_SIZE,
    sizeof(RTSHA256CONTEXT),
    0,
    rtCrDigestSha256_Update,
    rtCrDigestSha256_Final,
    rtCrDigestSha256_Init,
    NULL,
    NULL,
    NULL,
    NULL,
};


/*
 * SHA-512
 */

/** @impl_interface_method{RTCRDIGESTDESC::pfnUpdate} */
static DECLCALLBACK(void) rtCrDigestSha512_Update(void *pvState, const void *pvData, size_t cbData)
{
    RTSha512Update((PRTSHA512CONTEXT)pvState, pvData, cbData);
}

/** @impl_interface_method{RTCRDIGESTDESC::pfnFinal} */
static DECLCALLBACK(void) rtCrDigestSha512_Final(void *pvState, uint8_t *pbHash)
{
    RTSha512Final((PRTSHA512CONTEXT)pvState, pbHash);
}

/** @impl_interface_method{RTCRDIGESTDESC::pfnInit} */
static DECLCALLBACK(int) rtCrDigestSha512_Init(void *pvState, void *pvOpaque, bool fReInit)
{
    AssertReturn(pvOpaque == NULL, VERR_INVALID_PARAMETER);
    RTSha512Init((PRTSHA512CONTEXT)pvState);
    return VINF_SUCCESS;
}

/** SHA-512 alias ODIs. */
static const char * const g_apszSha512Aliases[] =
{
    RTCR_PKCS1_SHA512_WITH_RSA_OID,
    NULL
};

/** SHA-512 descriptor. */
static RTCRDIGESTDESC const g_rtCrDigestSha512Desc =
{
    "sha-512",
    "2.16.840.1.101.3.4.2.3",
    g_apszSha512Aliases,
    RTDIGESTTYPE_SHA512,
    RTSHA512_HASH_SIZE,
    sizeof(RTSHA512CONTEXT),
    0,
    rtCrDigestSha512_Update,
    rtCrDigestSha512_Final,
    rtCrDigestSha512_Init,
    NULL,
    NULL,
    NULL,
    NULL,
};


/**
 * Array of built in message digest vtables.
 */
static PCRTCRDIGESTDESC const g_apDigestOps[] =
{
    &g_rtCrDigestMd5Desc,
    &g_rtCrDigestSha1Desc,
    &g_rtCrDigestSha256Desc,
    &g_rtCrDigestSha512Desc,
};


#ifdef IPRT_WITH_OPENSSL
/*
 * OpenSSL EVP.
 */

/** @impl_interface_method{RTCRDIGESTDESC::pfnUpdate} */
static DECLCALLBACK(void) rtCrDigestOsslEvp_Update(void *pvState, const void *pvData, size_t cbData)
{
    EVP_DigestUpdate((EVP_MD_CTX *)pvState, pvData, cbData);
}


/** @impl_interface_method{RTCRDIGESTDESC::pfnFinal} */
static DECLCALLBACK(void) rtCrDigestOsslEvp_Final(void *pvState, uint8_t *pbHash)
{
    unsigned int cbHash = EVP_MAX_MD_SIZE;
    EVP_DigestFinal((EVP_MD_CTX *)pvState, (unsigned char *)pbHash, &cbHash);
}

/** @impl_interface_method{RTCRDIGESTDESC::pfnInit} */
static DECLCALLBACK(int) rtCrDigestOsslEvp_Init(void *pvState, void *pvOpaque, bool fReInit)
{
    EVP_MD_CTX   *pThis    = (EVP_MD_CTX *)pvState;
    EVP_MD const *pEvpType = (EVP_MD const *)pvOpaque;

    if (fReInit)
    {
        pEvpType = EVP_MD_CTX_md(pThis);
        EVP_MD_CTX_cleanup(pThis);
    }

    AssertPtrReturn(pEvpType, VERR_INVALID_PARAMETER);
    Assert(pEvpType->md_size);
    if (EVP_DigestInit(pThis, pEvpType))
        return VINF_SUCCESS;
    return VERR_CR_DIGEST_OSSL_DIGEST_INIT_ERROR;
}


/** @impl_interface_method{RTCRDIGESTDESC::pfn} */
static DECLCALLBACK(void) rtCrDigestOsslEvp_Delete(void *pvState)
{
    EVP_MD_CTX *pThis = (EVP_MD_CTX *)pvState;
    EVP_MD_CTX_cleanup(pThis);
}


/** @impl_interface_method{RTCRDIGESTDESC::pfnClone} */
static DECLCALLBACK(int) rtCrDigestOsslEvp_Clone(void *pvState, void const *pvSrcState)
{
    EVP_MD_CTX        *pThis = (EVP_MD_CTX *)pvState;
    EVP_MD_CTX const  *pSrc  = (EVP_MD_CTX const *)pvSrcState;

    if (EVP_MD_CTX_copy(pThis, pSrc))
        return VINF_SUCCESS;
    return VERR_CR_DIGEST_OSSL_DIGEST_CTX_COPY_ERROR;
}


/** @impl_interface_method{RTCRDIGESTDESC::pfnGetHashSize} */
static DECLCALLBACK(uint32_t) rtCrDigestOsslEvp_GetHashSize(void *pvState)
{
    EVP_MD_CTX *pThis = (EVP_MD_CTX *)pvState;
    return EVP_MD_size(EVP_MD_CTX_md(pThis));
}


/** @impl_interface_method{RTCRDIGESTDESC::pfnGetHashSize} */
static DECLCALLBACK(RTDIGESTTYPE) rtCrDigestOsslEvp_GetDigestType(void *pvState)
{
    EVP_MD_CTX *pThis = (EVP_MD_CTX *)pvState;
    /** @todo figure which digest algorithm it is! */
    return RTDIGESTTYPE_UNKNOWN;
}


/** Descriptor for the OpenSSL EVP base message digest provider. */
static RTCRDIGESTDESC const g_rtCrDigestOpenSslDesc =
{
    "OpenSSL EVP",
    NULL,
    NULL,
    RTDIGESTTYPE_UNKNOWN,
    EVP_MAX_MD_SIZE,
    sizeof(EVP_MD_CTX),
    0,
    rtCrDigestOsslEvp_Update,
    rtCrDigestOsslEvp_Final,
    rtCrDigestOsslEvp_Init,
    rtCrDigestOsslEvp_Delete,
    rtCrDigestOsslEvp_Clone,
    rtCrDigestOsslEvp_GetHashSize,
    rtCrDigestOsslEvp_GetDigestType
};

#endif /* IPRT_WITH_OPENSSL */


PCRTCRDIGESTDESC RTCrDigestFindByObjIdString(const char *pszObjId, void **ppvOpaque)
{
    if (ppvOpaque)
        *ppvOpaque = NULL;

    /*
     * Primary OIDs.
     */
    uint32_t i = RT_ELEMENTS(g_apDigestOps);
    while (i-- > 0)
        if (strcmp(g_apDigestOps[i]->pszObjId, pszObjId) == 0)
            return g_apDigestOps[i];

    /*
     * Alias OIDs.
     */
    i = RT_ELEMENTS(g_apDigestOps);
    while (i-- > 0)
    {
        const char * const *ppszAliases = g_apDigestOps[i]->papszObjIdAliases;
        if (ppszAliases)
            for (; *ppszAliases; ppszAliases++)
                if (strcmp(*ppszAliases, pszObjId) == 0)
                    return g_apDigestOps[i];
    }

#ifdef IPRT_WITH_OPENSSL
    /*
     * Try EVP and see if it knows the algorithm.
     */
    if (ppvOpaque)
    {
        rtCrOpenSslInit();
        int iAlgoNid = OBJ_txt2nid(pszObjId);
        if (iAlgoNid != NID_undef)
        {
            const char *pszAlogSn = OBJ_nid2sn(iAlgoNid);
            const EVP_MD *pEvpMdType = EVP_get_digestbyname(pszAlogSn);
            if (pEvpMdType)
            {
                /*
                 * Return the OpenSSL provider descriptor and the EVP_MD address.
                 */
                Assert(pEvpMdType->md_size);
                *ppvOpaque = (void *)pEvpMdType;
                return &g_rtCrDigestOpenSslDesc;
            }
        }
    }
#endif
    return NULL;
}


PCRTCRDIGESTDESC RTCrDigestFindByObjId(PCRTASN1OBJID pObjId, void **ppvOpaque)
{
    return RTCrDigestFindByObjIdString(pObjId->szObjId, ppvOpaque);
}


RTDECL(int) RTCrDigestCreateByObjIdString(PRTCRDIGEST phDigest, const char *pszObjId)
{
    void *pvOpaque;
    PCRTCRDIGESTDESC pDesc = RTCrDigestFindByObjIdString(pszObjId, &pvOpaque);
    if (pDesc)
        return RTCrDigestCreate(phDigest, pDesc, pvOpaque);
    return VERR_NOT_FOUND;
}


RTDECL(int) RTCrDigestCreateByObjId(PRTCRDIGEST phDigest, PCRTASN1OBJID pObjId)
{
    void *pvOpaque;
    PCRTCRDIGESTDESC pDesc = RTCrDigestFindByObjId(pObjId, &pvOpaque);
    if (pDesc)
        return RTCrDigestCreate(phDigest, pDesc, pvOpaque);
    return VERR_NOT_FOUND;
}

