
/* $Id$ */
/** @file
 * IPRT - HTTP communication API.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
#include <iprt/http.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/string.h>

#include <curl/curl.h>
#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct RTHTTPINTERNAL
{
    uint32_t u32Magic;
    CURL *pCurl;
    long lLastResp;
} RTHTTPINTERNAL;
typedef RTHTTPINTERNAL *PRTHTTPINTERNAL;

typedef struct RTHTTPMEMCHUNK
{
    char *pszMem;
    size_t cb;
} RTHTTPMEMCHUNK;
typedef RTHTTPMEMCHUNK *PRTHTTPMEMCHUNK;

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define CURL_FAILED(rcCurl) (RT_UNLIKELY(rcCurl != CURLE_OK))

/** Validates a handle and returns VERR_INVALID_HANDLE if not valid. */
#define RTHTTP_VALID_RETURN_RC(hHttp, rcCurl) \
    do { \
        AssertPtrReturn((hHttp), (rcCurl)); \
        AssertReturn((hHttp)->u32Magic == RTHTTP_MAGIC, (rcCurl)); \
    } while (0)

/** Validates a handle and returns VERR_INVALID_HANDLE if not valid. */
#define RTHTTP_VALID_RETURN(hHTTP) RTHTTP_VALID_RETURN_RC((hHttp), VERR_INVALID_HANDLE)

/** Validates a handle and returns (void) if not valid. */
#define RTHTTP_VALID_RETURN_VOID(hHttp) \
    do { \
        AssertPtrReturnVoid(hHttp); \
        AssertReturnVoid((hHttp)->u32Magic == RTHTTP_MAGIC); \
    } while (0)


RTR3DECL(int) RTHttpCreate(PRTHTTP phHttp)
{
    AssertPtrReturn(phHttp, VERR_INVALID_PARAMETER);

    CURLcode rcCurl = curl_global_init(CURL_GLOBAL_ALL);
    if (CURL_FAILED(rcCurl))
        return VERR_INTERNAL_ERROR;

    CURL* pCurl = curl_easy_init();
    if (!pCurl)
        return VERR_INTERNAL_ERROR;

    PRTHTTPINTERNAL pHttpInt = (PRTHTTPINTERNAL)RTMemAllocZ(sizeof(RTHTTPINTERNAL));
    if (!pHttpInt)
        return VERR_NO_MEMORY;

    pHttpInt->u32Magic = RTHTTP_MAGIC;
    pHttpInt->pCurl = pCurl;

    *phHttp = (RTHTTP)pHttpInt;

    return VINF_SUCCESS;
}

RTR3DECL(void) RTHttpDestroy(RTHTTP hHttp)
{
    if (!hHttp)
        return;

    PRTHTTPINTERNAL pHttpInt = hHttp;
    RTHTTP_VALID_RETURN_VOID(pHttpInt);

    pHttpInt->u32Magic = RTHTTP_MAGIC_DEAD;

    curl_easy_cleanup(pHttpInt->pCurl);

    RTMemFree(pHttpInt);

    curl_global_cleanup();
}

static size_t rtHttpWriteData(void *pvBuf, size_t cb, size_t n, void *pvUser)
{
    PRTHTTPMEMCHUNK pMem = (PRTHTTPMEMCHUNK)pvUser;
    size_t cbAll = cb * n;

    pMem->pszMem = (char*)RTMemRealloc(pMem->pszMem, pMem->cb + cbAll + 1);
    if (pMem->pszMem)
    {
        memcpy(&pMem->pszMem[pMem->cb], pvBuf, cbAll);
        pMem->cb += cbAll;
        pMem->pszMem[pMem->cb] = '\0';
    }
    return cbAll;
}

RTR3DECL(int) RTHttpSetProxy(RTHTTP hHttp, const char *pcszProxy, uint32_t uPort,
                             const char *pcszProxyUser, const char *pcszProxyPwd)
{
    PRTHTTPINTERNAL pHttpInt = hHttp;
    RTHTTP_VALID_RETURN(pHttpInt);
    AssertPtrReturn(pcszProxy, VERR_INVALID_PARAMETER);

    int rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_PROXY, pcszProxy);
    if (CURL_FAILED(rcCurl))
        return VERR_INVALID_PARAMETER;

    if (uPort != 0)
    {
        rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_PROXYPORT, (long)uPort);
        if (CURL_FAILED(rcCurl))
            return VERR_INVALID_PARAMETER;
    }

    if (pcszProxyUser && pcszProxyPwd)
    {
        rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_PROXYUSERNAME, pcszProxyUser);
        if (CURL_FAILED(rcCurl))
            return VERR_INVALID_PARAMETER;

        rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_PROXYPASSWORD, pcszProxyPwd);
        if (CURL_FAILED(rcCurl))
            return VERR_INVALID_PARAMETER;
    }

    return VINF_SUCCESS;
}

RTR3DECL(int) RTHttpGet(RTHTTP hHttp, const char *pcszUrl, char **ppszResponse)
{
    PRTHTTPINTERNAL pHttpInt = hHttp;
    RTHTTP_VALID_RETURN(pHttpInt);

    int rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_URL, pcszUrl);
    if (CURL_FAILED(rcCurl))
        return VERR_INVALID_PARAMETER;

    /* XXX */
    rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt");
    if (CURL_FAILED(rcCurl))
        return VERR_INTERNAL_ERROR;

    RTHTTPMEMCHUNK chunk = { NULL, 0 };
    rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_WRITEFUNCTION, &rtHttpWriteData);
    if (CURL_FAILED(rcCurl))
        return VERR_INTERNAL_ERROR;
    rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_WRITEDATA, (void*)&chunk);
    if (CURL_FAILED(rcCurl))
        return VERR_INTERNAL_ERROR;

    rcCurl = curl_easy_perform(pHttpInt->pCurl);
    int rc = VERR_INTERNAL_ERROR;
    if (rcCurl == CURLE_OK)
    {
        curl_easy_getinfo(pHttpInt->pCurl, CURLINFO_RESPONSE_CODE, &pHttpInt->lLastResp);
        switch (pHttpInt->lLastResp)
        {
            case 200:
                /* OK, request was fulfilled */
            case 204:
                /* empty response */
                rc = VINF_SUCCESS;
                break;
            case 400:
                /* bad request */
                rc = VERR_HTTP_BAD_REQUEST;
                break;
            case 403:
                /* forbidden, authorization will not help */
                rc = VERR_HTTP_ACCESS_DENIED;
                break;
            case 404:
                /* URL not found */
                rc = VERR_HTTP_NOT_FOUND;
                break;
        }
    }
    else
    {
        switch (rcCurl)
        {
            case CURLE_URL_MALFORMAT:
            case CURLE_COULDNT_RESOLVE_HOST:
                rc = VERR_HTTP_NOT_FOUND; 
                break;
            default:
                break;
        }
    }

    *ppszResponse = chunk.pszMem;

    return rc;
}
