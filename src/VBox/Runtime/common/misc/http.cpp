/* $Id$ */
/** @file
 * IPRT - HTTP communication API.
 */

/*
 * Copyright (C) 2012-2013 Oracle Corporation
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
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/env.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/file.h>
#include <iprt/stream.h>

#include <curl/curl.h>
#include <openssl/ssl.h>
#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct RTHTTPINTERNAL
{
    /** Magic value. */
    uint32_t            u32Magic;
    /** cURL handle. */
    CURL                *pCurl;
    /** The last response code. */
    long                lLastResp;
    /** custom headers */
    struct curl_slist   *pHeaders;
    /** CA certificate for HTTPS authentication check. */
    char                *pcszCAFile;
    /** Abort the current HTTP request if true. */
    bool                fAbort;
    /** The location field for 301 responses. */
    char                *pszRedirLocation;
} RTHTTPINTERNAL;
typedef RTHTTPINTERNAL *PRTHTTPINTERNAL;

typedef struct RTHTTPMEMCHUNK
{
    uint8_t *pu8Mem;
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
        return VERR_HTTP_INIT_FAILED;

    CURL *pCurl = curl_easy_init();
    if (!pCurl)
        return VERR_HTTP_INIT_FAILED;

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

    if (pHttpInt->pHeaders)
        curl_slist_free_all(pHttpInt->pHeaders);

    if (pHttpInt->pcszCAFile)
        RTStrFree(pHttpInt->pcszCAFile);

    if (pHttpInt->pszRedirLocation)
        RTStrFree(pHttpInt->pszRedirLocation);

    RTMemFree(pHttpInt);

    curl_global_cleanup();
}

static DECLCALLBACK(size_t) rtHttpWriteData(void *pvBuf, size_t cb, size_t n, void *pvUser)
{
    PRTHTTPMEMCHUNK pMem = (PRTHTTPMEMCHUNK)pvUser;
    size_t cbAll = cb * n;

    pMem->pu8Mem = (uint8_t*)RTMemRealloc(pMem->pu8Mem, pMem->cb + cbAll + 1);
    if (pMem->pu8Mem)
    {
        memcpy(&pMem->pu8Mem[pMem->cb], pvBuf, cbAll);
        pMem->cb += cbAll;
        pMem->pu8Mem[pMem->cb] = '\0';
    }
    return cbAll;
}

static DECLCALLBACK(int) rtHttpProgress(void *pData, double DlTotal, double DlNow,
                                        double UlTotal, double UlNow)
{
    PRTHTTPINTERNAL pHttpInt = (PRTHTTPINTERNAL)pData;
    AssertReturn(pHttpInt->u32Magic == RTHTTP_MAGIC, 1);

    return pHttpInt->fAbort ? 1 : 0;
}

RTR3DECL(int) RTHttpAbort(RTHTTP hHttp)
{
    PRTHTTPINTERNAL pHttpInt = hHttp;
    RTHTTP_VALID_RETURN(pHttpInt);

    pHttpInt->fAbort = true;

    return VINF_SUCCESS;
}

RTR3DECL(int) RTHttpGetRedirLocation(RTHTTP hHttp, char **ppszRedirLocation)
{
    PRTHTTPINTERNAL pHttpInt = hHttp;
    RTHTTP_VALID_RETURN(pHttpInt);

    if (!pHttpInt->pszRedirLocation)
        return VERR_HTTP_NOT_FOUND;

    *ppszRedirLocation = RTStrDup(pHttpInt->pszRedirLocation);
    return VINF_SUCCESS;
}

RTR3DECL(int) RTHttpUseSystemProxySettings(RTHTTP hHttp)
{
    PRTHTTPINTERNAL pHttpInt = hHttp;
    RTHTTP_VALID_RETURN(pHttpInt);

    /*
     * Very limited right now, just enought to make it work for ourselves.
     */
    char szProxy[_1K];
    int rc = RTEnvGetEx(RTENV_DEFAULT, "http_proxy", szProxy, sizeof(szProxy), NULL);
    if (RT_SUCCESS(rc))
    {
        int rcCurl;
        if (!strncmp(szProxy, RT_STR_TUPLE("http://")))
        {
            rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_PROXY, &szProxy[sizeof("http://") - 1]);
            if (CURL_FAILED(rcCurl))
                return VERR_INVALID_PARAMETER;
            rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_PROXYPORT, 80);
            if (CURL_FAILED(rcCurl))
                return VERR_INVALID_PARAMETER;
        }
        else
        {
            rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_PROXY, &szProxy[sizeof("http://") - 1]);
            if (CURL_FAILED(rcCurl))
                return VERR_INVALID_PARAMETER;
        }
    }
    else if (rc == VERR_ENV_VAR_NOT_FOUND)
        rc = VINF_SUCCESS;

    return rc;
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

RTR3DECL(int) RTHttpSetHeaders(RTHTTP hHttp, size_t cHeaders, const char * const *papszHeaders)
{
    PRTHTTPINTERNAL pHttpInt = hHttp;
    RTHTTP_VALID_RETURN(pHttpInt);

    if (!cHeaders)
    {
        if (pHttpInt->pHeaders)
            curl_slist_free_all(pHttpInt->pHeaders);
        pHttpInt->pHeaders = 0;
        return VINF_SUCCESS;
    }

    struct curl_slist *pHeaders = NULL;
    for (size_t i = 0; i < cHeaders; i++)
        pHeaders = curl_slist_append(pHeaders, papszHeaders[i]);

    pHttpInt->pHeaders = pHeaders;
    int rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_HTTPHEADER, pHeaders);
    if (CURL_FAILED(rcCurl))
        return VERR_INVALID_PARAMETER;

    return VINF_SUCCESS;
}

RTR3DECL(int) RTHttpCertDigest(RTHTTP hHttp, char *pcszCert, size_t cbCert,
                               uint8_t **pabSha1,   size_t *pcbSha1,
                               uint8_t **pabSha512, size_t *pcbSha512)
{
    int rc = VINF_SUCCESS;

    BIO *cert = BIO_new_mem_buf(pcszCert, (int)cbCert);
    if (cert)
    {
        X509 *crt = NULL;
        if (PEM_read_bio_X509(cert, &crt, NULL, NULL))
        {
            unsigned cb;
            unsigned char md[EVP_MAX_MD_SIZE];

            int rc1 = X509_digest(crt, EVP_sha1(), md, &cb);
            if (rc1 > 0)
            {
                *pabSha1 = (uint8_t*)RTMemAlloc(cb);
                if (*pabSha1)
                {
                    memcpy(*pabSha1, md, cb);
                    *pcbSha1 = cb;

                    rc1 = X509_digest(crt, EVP_sha512(), md, &cb);
                    if (rc1 > 0)
                    {
                        *pabSha512 = (uint8_t*)RTMemAlloc(cb);
                        if (*pabSha512)
                        {
                            memcpy(*pabSha512, md, cb);
                            *pcbSha512 = cb;
                        }
                        else
                            rc = VERR_NO_MEMORY;
                    }
                    else
                        rc = VERR_HTTP_CACERT_WRONG_FORMAT;

                    if (RT_FAILURE(rc))
                        RTMemFree(*pabSha1);
                }
                else
                    rc = VERR_NO_MEMORY;
            }
            else
                rc = VERR_HTTP_CACERT_WRONG_FORMAT;
            X509_free(crt);
        }
        else
            rc = VERR_HTTP_CACERT_WRONG_FORMAT;
        BIO_free(cert);
    }
    else
        rc = VERR_INTERNAL_ERROR;

    return rc;
}

RTR3DECL(int) RTHttpSetCAFile(RTHTTP hHttp, const char *pcszCAFile)
{
    PRTHTTPINTERNAL pHttpInt = hHttp;
    RTHTTP_VALID_RETURN(pHttpInt);

    if (pHttpInt->pcszCAFile)
        RTStrFree(pHttpInt->pcszCAFile);
    pHttpInt->pcszCAFile = RTStrDup(pcszCAFile);
    if (!pHttpInt->pcszCAFile)
        return VERR_NO_MEMORY;

    return VINF_SUCCESS;
}


/**
 * Figures out the IPRT status code for a GET.
 *
 * @returns IPRT status code.
 * @param   pHttpInt            HTTP instance.
 * @param   rcCurl              What curl returned.
 */
static int rtHttpGetCalcStatus(PRTHTTPINTERNAL pHttpInt, int rcCurl)
{
    int rc = VERR_INTERNAL_ERROR;

    if (pHttpInt->pszRedirLocation)
    {
        RTStrFree(pHttpInt->pszRedirLocation);
        pHttpInt->pszRedirLocation = NULL;
    }
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
            case 301:
            {
                const char *pszRedirect;
                curl_easy_getinfo(pHttpInt->pCurl, CURLINFO_REDIRECT_URL, &pszRedirect);
                size_t cb = strlen(pszRedirect);
                if (cb > 0 && cb < 2048)
                    pHttpInt->pszRedirLocation = RTStrDup(pszRedirect);
                rc = VERR_HTTP_REDIRECTED;
                break;
            }
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
            case CURLE_COULDNT_CONNECT:
                rc = VERR_HTTP_COULDNT_CONNECT;
                break;
            case CURLE_SSL_CONNECT_ERROR:
                rc = VERR_HTTP_SSL_CONNECT_ERROR;
                break;
            case CURLE_SSL_CACERT:
                /* The peer certificate cannot be authenticated with the CA certificates
                 * set by RTHttpSetCAFile(). We need other or additional CA certificates. */
                rc = VERR_HTTP_CACERT_CANNOT_AUTHENTICATE;
                break;
            case CURLE_SSL_CACERT_BADFILE:
                /* CAcert file (see RTHttpSetCAFile()) has wrong format */
                rc = VERR_HTTP_CACERT_WRONG_FORMAT;
                break;
            case CURLE_ABORTED_BY_CALLBACK:
                /* forcefully aborted */
                rc = VERR_HTTP_ABORTED;
                break;
            default:
                break;
        }
    }

    return rc;
}

RTR3DECL(int) rtHttpGet(RTHTTP hHttp, const char *pcszUrl, uint8_t **ppvResponse, size_t *pcb)
{
    PRTHTTPINTERNAL pHttpInt = hHttp;
    RTHTTP_VALID_RETURN(pHttpInt);

    pHttpInt->fAbort = false;

    int rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_URL, pcszUrl);
    if (CURL_FAILED(rcCurl))
        return VERR_INVALID_PARAMETER;

#if 0
    rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_VERBOSE, 1);
    if (CURL_FAILED(rcCurl))
        return VERR_INVALID_PARAMETER;
#endif

    const char *pcszCAFile = "/etc/ssl/certs/ca-certificates.crt";
    if (pHttpInt->pcszCAFile)
        pcszCAFile = pHttpInt->pcszCAFile;
    if (RTFileExists(pcszCAFile))
    {
        rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_CAINFO, pcszCAFile);
        if (CURL_FAILED(rcCurl))
            return VERR_INTERNAL_ERROR;
    }

    RTHTTPMEMCHUNK chunk = { NULL, 0 };
    rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_WRITEFUNCTION, &rtHttpWriteData);
    if (CURL_FAILED(rcCurl))
        return VERR_INTERNAL_ERROR;
    rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_WRITEDATA, (void*)&chunk);
    if (CURL_FAILED(rcCurl))
        return VERR_INTERNAL_ERROR;
    rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_PROGRESSFUNCTION, &rtHttpProgress);
    if (CURL_FAILED(rcCurl))
        return VERR_INTERNAL_ERROR;
    rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_PROGRESSDATA, (void*)pHttpInt);
    if (CURL_FAILED(rcCurl))
        return VERR_INTERNAL_ERROR;
    rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_NOPROGRESS, (long)0);
    if (CURL_FAILED(rcCurl))
        return VERR_INTERNAL_ERROR;
    rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_SSLVERSION, (long)CURL_SSLVERSION_TLSv1);
    if (CURL_FAILED(rcCurl))
        return VERR_INVALID_PARAMETER;

    rcCurl = curl_easy_perform(pHttpInt->pCurl);
    int rc = rtHttpGetCalcStatus(pHttpInt, rcCurl);
    *ppvResponse = chunk.pu8Mem;
    *pcb = chunk.cb;

    return rc;
}


RTR3DECL(int) RTHttpGetText(RTHTTP hHttp, const char *pcszUrl, char **ppszResponse)
{
    uint8_t *pv;
    size_t cb;
    int rc = rtHttpGet(hHttp, pcszUrl, &pv, &cb);
    *ppszResponse = (char*)pv;
    return rc;
}


RTR3DECL(int) RTHttpGetBinary(RTHTTP hHttp, const char *pcszUrl, void **ppvResponse, size_t *pcb)
{
    return rtHttpGet(hHttp, pcszUrl, (uint8_t**)ppvResponse, pcb);
}


static size_t rtHttpWriteDataToFile(void *pvBuf, size_t cb, size_t n, void *pvUser)
{
    size_t cbAll = cb * n;
    RTFILE hFile = (RTFILE)(intptr_t)pvUser;

    size_t cbWritten = 0;
    int rc = RTFileWrite(hFile, pvBuf, cbAll, &cbWritten);
    if (RT_SUCCESS(rc))
        return cbWritten;
    return 0;
}


RTR3DECL(int) RTHttpGetFile(RTHTTP hHttp, const char *pszUrl, const char *pszDstFile)
{
    PRTHTTPINTERNAL pHttpInt = hHttp;
    RTHTTP_VALID_RETURN(pHttpInt);

    /*
     * Set up the request.
     */
    pHttpInt->fAbort = false;

    int rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_URL, pszUrl);
    if (CURL_FAILED(rcCurl))
        return VERR_INVALID_PARAMETER;

#if 0
    rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_VERBOSE, 1);
    if (CURL_FAILED(rcCurl))
        return VERR_INVALID_PARAMETER;
#endif

    const char *pcszCAFile = "/etc/ssl/certs/ca-certificates.crt";
    if (pHttpInt->pcszCAFile)
        pcszCAFile = pHttpInt->pcszCAFile;
    if (RTFileExists(pcszCAFile))
    {
        rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_CAINFO, pcszCAFile);
        if (CURL_FAILED(rcCurl))
            return VERR_INTERNAL_ERROR;
    }

    rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_WRITEFUNCTION, &rtHttpWriteDataToFile);
    if (CURL_FAILED(rcCurl))
        return VERR_INTERNAL_ERROR;
    rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_PROGRESSFUNCTION, &rtHttpProgress);
    if (CURL_FAILED(rcCurl))
        return VERR_INTERNAL_ERROR;
    rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_PROGRESSDATA, (void*)pHttpInt);
    if (CURL_FAILED(rcCurl))
        return VERR_INTERNAL_ERROR;
    rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_NOPROGRESS, (long)0);
    if (CURL_FAILED(rcCurl))
        return VERR_INTERNAL_ERROR;

    /*
     * Open the output file.
     */
    RTFILE hFile;
    int rc = RTFileOpen(&hFile, pszDstFile, RTFILE_O_WRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_READWRITE);
    if (RT_SUCCESS(rc))
    {
        rcCurl = curl_easy_setopt(pHttpInt->pCurl, CURLOPT_WRITEDATA, (void *)(uintptr_t)hFile);
        if (!CURL_FAILED(rcCurl))
        {
            /*
             * Perform the request.
             */
            rcCurl = curl_easy_perform(pHttpInt->pCurl);
            rc = rtHttpGetCalcStatus(pHttpInt, rcCurl);
        }
        else
            rc = VERR_INTERNAL_ERROR;

        int rc2 = RTFileClose(hFile);
        if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
            rc = rc2;
    }

    return rc;
}

