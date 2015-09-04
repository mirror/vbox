/* $Id$ */
/** @file
 * IPRT - HTTP communication API.
 */

/*
 * Copyright (C) 2012-2015 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/http.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/crypto/store.h>
#include <iprt/env.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>

#include "internal/magics.h"

#include <curl/curl.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Internal HTTP client instance.
 */
typedef struct RTHTTPINTERNAL
{
    /** Magic value. */
    uint32_t            u32Magic;
    /** cURL handle. */
    CURL               *pCurl;
    /** The last response code. */
    long                lLastResp;
    /** Custom headers/ */
    struct curl_slist  *pHeaders;
    /** CA certificate file for HTTPS authentication. */
    char               *pszCaFile;
    /** Whether to delete the CA on destruction. */
    bool                fDeleteCaFile;
    /** Abort the current HTTP request if true. */
    bool volatile       fAbort;
    /** Set if someone is preforming an HTTP operation. */
    bool volatile       fBusy;
    /** The location field for 301 responses. */
    char               *pszRedirLocation;

    /** Output callback data. */
    union
    {
        /** For file destination.  */
        RTFILE          hFile;
        /** For memory destination. */
        struct
        {
            /** The current size (sans terminator char). */
            size_t      cb;
            /** The currently allocated size. */
            size_t      cbAllocated;
            /** Pointer to the buffer. */
            uint8_t    *pb;
        } Mem;
    } Output;
    /** Output callback status. */
    int                 rcOutput;
    /** Download size hint set by the progress callback. */
    uint64_t            cbDownloadHint;
} RTHTTPINTERNAL;
/** Pointer to an internal HTTP client instance. */
typedef RTHTTPINTERNAL *PRTHTTPINTERNAL;


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** @def RTHTTP_MAX_MEM_DOWNLOAD
 * The max size we are allowed to download to a memory buffer.
 *
 * @remarks The minus 1 is for the trailing zero terminator we always add.
 */
#if ARCH_BITS == 64
# define RTHTTP_MAX_MEM_DOWNLOAD_SIZE       (UINT32_C(64)*_1M - 1)
#else
# define RTHTTP_MAX_MEM_DOWNLOAD_SIZE       (UINT32_C(32)*_1M - 1)
#endif

/** Checks whether a cURL return code indicates success. */
#define CURL_SUCCESS(rcCurl)    RT_LIKELY(rcCurl == CURLE_OK)
/** Checks whether a cURL return code indicates failure. */
#define CURL_FAILURE(rcCurl)    RT_UNLIKELY(rcCurl != CURLE_OK)

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


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static void rtHttpUnsetCaFile(PRTHTTPINTERNAL pThis);


RTR3DECL(int) RTHttpCreate(PRTHTTP phHttp)
{
    AssertPtrReturn(phHttp, VERR_INVALID_PARAMETER);

    /** @todo r=bird: rainy day: curl_global_init is not thread safe, only a
     *        problem if multiple threads get here at the same time. */
    int rc = VERR_HTTP_INIT_FAILED;
    CURLcode rcCurl = curl_global_init(CURL_GLOBAL_ALL);
    if (!CURL_FAILURE(rcCurl))
    {
        CURL *pCurl = curl_easy_init();
        if (pCurl)
        {
            PRTHTTPINTERNAL pThis = (PRTHTTPINTERNAL)RTMemAllocZ(sizeof(RTHTTPINTERNAL));
            if (pThis)
            {
                pThis->u32Magic = RTHTTP_MAGIC;
                pThis->pCurl    = pCurl;

                *phHttp = (RTHTTP)pThis;

                return VINF_SUCCESS;
            }
            rc = VERR_NO_MEMORY;
        }
        else
            rc = VERR_HTTP_INIT_FAILED;
    }
    curl_global_cleanup();
    return rc;
}


RTR3DECL(void) RTHttpDestroy(RTHTTP hHttp)
{
    if (hHttp == NIL_RTHTTP)
        return;

    PRTHTTPINTERNAL pThis = hHttp;
    RTHTTP_VALID_RETURN_VOID(pThis);

    Assert(!pThis->fBusy);

    pThis->u32Magic = RTHTTP_MAGIC_DEAD;

    curl_easy_cleanup(pThis->pCurl);
    pThis->pCurl = NULL;

    if (pThis->pHeaders)
        curl_slist_free_all(pThis->pHeaders);

    rtHttpUnsetCaFile(pThis);
    Assert(!pThis->pszCaFile);

    if (pThis->pszRedirLocation)
        RTStrFree(pThis->pszRedirLocation);

    RTMemFree(pThis);

    curl_global_cleanup();
}


RTR3DECL(int) RTHttpAbort(RTHTTP hHttp)
{
    PRTHTTPINTERNAL pThis = hHttp;
    RTHTTP_VALID_RETURN(pThis);

    pThis->fAbort = true;

    return VINF_SUCCESS;
}


RTR3DECL(int) RTHttpGetRedirLocation(RTHTTP hHttp, char **ppszRedirLocation)
{
    PRTHTTPINTERNAL pThis = hHttp;
    RTHTTP_VALID_RETURN(pThis);
    Assert(!pThis->fBusy);

    if (!pThis->pszRedirLocation)
        return VERR_HTTP_NOT_FOUND;

    return RTStrDupEx(ppszRedirLocation, pThis->pszRedirLocation);
}


RTR3DECL(int) RTHttpUseSystemProxySettings(RTHTTP hHttp)
{
    PRTHTTPINTERNAL pThis = hHttp;
    RTHTTP_VALID_RETURN(pThis);

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
            rcCurl = curl_easy_setopt(pThis->pCurl, CURLOPT_PROXY, &szProxy[sizeof("http://") - 1]);
            if (CURL_FAILURE(rcCurl))
                return VERR_INVALID_PARAMETER;
            rcCurl = curl_easy_setopt(pThis->pCurl, CURLOPT_PROXYPORT, 80);
            if (CURL_FAILURE(rcCurl))
                return VERR_INVALID_PARAMETER;
        }
        else
        {
            rcCurl = curl_easy_setopt(pThis->pCurl, CURLOPT_PROXY, &szProxy[sizeof("http://") - 1]);
            if (CURL_FAILURE(rcCurl))
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
    PRTHTTPINTERNAL pThis = hHttp;
    RTHTTP_VALID_RETURN(pThis);
    AssertPtrReturn(pcszProxy, VERR_INVALID_PARAMETER);

    int rcCurl = curl_easy_setopt(pThis->pCurl, CURLOPT_PROXY, pcszProxy);
    if (CURL_FAILURE(rcCurl))
        return VERR_INVALID_PARAMETER;

    if (uPort != 0)
    {
        rcCurl = curl_easy_setopt(pThis->pCurl, CURLOPT_PROXYPORT, (long)uPort);
        if (CURL_FAILURE(rcCurl))
            return VERR_INVALID_PARAMETER;
    }

    if (pcszProxyUser && pcszProxyPwd)
    {
        rcCurl = curl_easy_setopt(pThis->pCurl, CURLOPT_PROXYUSERNAME, pcszProxyUser);
        if (CURL_FAILURE(rcCurl))
            return VERR_INVALID_PARAMETER;

        rcCurl = curl_easy_setopt(pThis->pCurl, CURLOPT_PROXYPASSWORD, pcszProxyPwd);
        if (CURL_FAILURE(rcCurl))
            return VERR_INVALID_PARAMETER;
    }

    return VINF_SUCCESS;
}


RTR3DECL(int) RTHttpSetHeaders(RTHTTP hHttp, size_t cHeaders, const char * const *papszHeaders)
{
    PRTHTTPINTERNAL pThis = hHttp;
    RTHTTP_VALID_RETURN(pThis);

    if (!cHeaders)
    {
        if (pThis->pHeaders)
            curl_slist_free_all(pThis->pHeaders);
        pThis->pHeaders = 0;
        return VINF_SUCCESS;
    }

    struct curl_slist *pHeaders = NULL;
    for (size_t i = 0; i < cHeaders; i++)
        pHeaders = curl_slist_append(pHeaders, papszHeaders[i]);

    pThis->pHeaders = pHeaders;
    int rcCurl = curl_easy_setopt(pThis->pCurl, CURLOPT_HTTPHEADER, pHeaders);
    if (CURL_FAILURE(rcCurl))
        return VERR_INVALID_PARAMETER;

    return VINF_SUCCESS;
}


/**
 * Set the CA file to NULL, deleting any temporary file if necessary.
 *
 * @param   pThis           The HTTP/HTTPS client instance.
 */
static void rtHttpUnsetCaFile(PRTHTTPINTERNAL pThis)
{
    if (pThis->pszCaFile)
    {
        if (pThis->fDeleteCaFile)
        {
            int rc2 = RTFileDelete(pThis->pszCaFile);
            AssertMsg(RT_SUCCESS(rc2) || !RTFileExists(pThis->pszCaFile), ("rc=%Rrc '%s'\n", rc2, pThis->pszCaFile));
        }
        RTStrFree(pThis->pszCaFile);
        pThis->pszCaFile = NULL;
    }
}


RTR3DECL(int) RTHttpSetCAFile(RTHTTP hHttp, const char *pszCaFile)
{
    PRTHTTPINTERNAL pThis = hHttp;
    RTHTTP_VALID_RETURN(pThis);

    rtHttpUnsetCaFile(pThis);

    pThis->fDeleteCaFile = false;
    if (pszCaFile)
        return RTStrDupEx(&pThis->pszCaFile, pszCaFile);
    return VINF_SUCCESS;
}


RTR3DECL(int) RTHttpUseTemporaryCaFile(RTHTTP hHttp, PRTERRINFO pErrInfo)
{
    PRTHTTPINTERNAL pThis = hHttp;
    RTHTTP_VALID_RETURN(pThis);

    /*
     * Create a temporary file.
     */
    int rc = VERR_NO_STR_MEMORY;
    char *pszCaFile = RTStrAlloc(RTPATH_MAX);
    if (pszCaFile)
    {
        RTFILE hFile;
        rc = RTFileOpenTemp(&hFile, pszCaFile, RTPATH_MAX,
                            RTFILE_O_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_NONE | (0600 << RTFILE_O_CREATE_MODE_SHIFT));
        if (RT_SUCCESS(rc))
        {
            /*
             * Gather certificates into a temporary store and export them to the temporary file.
             */
            RTCRSTORE hStore;
            rc = RTCrStoreCreateInMem(&hStore, 256);
            if (RT_SUCCESS(rc))
            {
                rc = RTHttpGatherCaCertsInStore(hStore, 0 /*fFlags*/, pErrInfo);
                if (RT_SUCCESS(rc))
                    /** @todo Consider adding an API for exporting to a RTFILE... */
                    rc = RTCrStoreCertExportAsPem(hStore, 0 /*fFlags*/, pszCaFile);
                RTCrStoreRelease(hStore);
            }
            RTFileClose(hFile);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Set the CA file for the instance.
                 */
                rtHttpUnsetCaFile(pThis);

                pThis->fDeleteCaFile = true;
                pThis->pszCaFile = pszCaFile;
                return VINF_SUCCESS;
            }

            int rc2 = RTFileDelete(pszCaFile);
            AssertRC(rc2);
        }
        else
            RTErrInfoAddF(pErrInfo, rc, "Error creating temorary file: %Rrc", rc);

        RTStrFree(pszCaFile);
    }
    return rc;
}


RTR3DECL(int) RTHttpGatherCaCertsInStore(RTCRSTORE hStore, uint32_t fFlags, PRTERRINFO pErrInfo)
{
    uint32_t const cBefore = RTCrStoreCertCount(hStore);
    AssertReturn(cBefore != UINT32_MAX, VERR_INVALID_HANDLE);

    /*
     * Add the user store, quitely ignoring any errors.
     */
    RTCRSTORE hSrcStore;
    int rcUser = RTCrStoreCreateSnapshotById(&hSrcStore, RTCRSTOREID_USER_TRUSTED_CAS_AND_CERTIFICATES, pErrInfo);
    if (RT_SUCCESS(rcUser))
    {
        rcUser = RTCrStoreCertAddFromStore(hStore, RTCRCERTCTX_F_ADD_IF_NOT_FOUND | RTCRCERTCTX_F_ADD_CONTINUE_ON_ERROR,
                                           hSrcStore);
        RTCrStoreRelease(hSrcStore);
    }

    /*
     * Ditto for the system store.
     */
    int rcSystem = RTCrStoreCreateSnapshotById(&hSrcStore, RTCRSTOREID_SYSTEM_TRUSTED_CAS_AND_CERTIFICATES, pErrInfo);
    if (RT_SUCCESS(rcSystem))
    {
        rcSystem = RTCrStoreCertAddFromStore(hStore, RTCRCERTCTX_F_ADD_IF_NOT_FOUND | RTCRCERTCTX_F_ADD_CONTINUE_ON_ERROR,
                                             hSrcStore);
        RTCrStoreRelease(hSrcStore);
    }

    /*
     * If the number of certificates increased, we consider it a success.
     */
    if (RTCrStoreCertCount(hStore) > cBefore)
    {
        if (RT_FAILURE(rcSystem))
            return -rcSystem;
        if (RT_FAILURE(rcUser))
            return -rcUser;
        return rcSystem != VINF_SUCCESS ? rcSystem : rcUser;
    }

    if (RT_FAILURE(rcSystem))
        return rcSystem;
    if (RT_FAILURE(rcUser))
        return rcUser;
    return VERR_NOT_FOUND;
}


RTR3DECL(int) RTHttpGatherCaCertsInFile(const char *pszCaFile, uint32_t fFlags, PRTERRINFO pErrInfo)
{
    RTCRSTORE hStore;
    int rc = RTCrStoreCreateInMem(&hStore, 256);
    if (RT_SUCCESS(rc))
    {
        rc = RTHttpGatherCaCertsInStore(hStore, fFlags, pErrInfo);
        if (RT_SUCCESS(rc))
            rc = RTCrStoreCertExportAsPem(hStore, 0 /*fFlags*/, pszCaFile);
        RTCrStoreRelease(hStore);
    }
    return rc;
}



/**
 * Figures out the IPRT status code for a GET.
 *
 * @returns IPRT status code.
 * @param   pThis           The HTTP/HTTPS client instance.
 * @param   rcCurl          What curl returned.
 */
static int rtHttpGetCalcStatus(PRTHTTPINTERNAL pThis, int rcCurl)
{
    int rc = VERR_INTERNAL_ERROR;

    if (pThis->pszRedirLocation)
    {
        RTStrFree(pThis->pszRedirLocation);
        pThis->pszRedirLocation = NULL;
    }
    if (rcCurl == CURLE_OK)
    {
        curl_easy_getinfo(pThis->pCurl, CURLINFO_RESPONSE_CODE, &pThis->lLastResp);
        switch (pThis->lLastResp)
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
                curl_easy_getinfo(pThis->pCurl, CURLINFO_REDIRECT_URL, &pszRedirect);
                size_t cb = strlen(pszRedirect);
                if (cb > 0 && cb < 2048)
                    pThis->pszRedirLocation = RTStrDup(pszRedirect);
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
            case CURLE_COULDNT_RESOLVE_PROXY:
                rc = VERR_HTTP_PROXY_NOT_FOUND;
                break;
            case CURLE_WRITE_ERROR:
                rc = RT_FAILURE_NP(pThis->rcOutput) ? pThis->rcOutput : VERR_WRITE_ERROR;
                break;
            //case CURLE_READ_ERROR

            default:
                break;
        }
    }

    return rc;
}


/**
 * cURL callback for reporting progress, we use it for checking for abort.
 */
static int rtHttpProgress(void *pData, double rdTotalDownload, double rdDownloaded, double rdTotalUpload, double rdUploaded)
{
    PRTHTTPINTERNAL pThis = (PRTHTTPINTERNAL)pData;
    AssertReturn(pThis->u32Magic == RTHTTP_MAGIC, 1);

    pThis->cbDownloadHint = (uint64_t)rdTotalDownload;

    return pThis->fAbort ? 1 : 0;
}


/**
 * Whether we're likely to need SSL to handle the give URL.
 *
 * @returns true if we need, false if we probably don't.
 * @param   pszUrl              The URL.
 */
static bool rtHttpNeedSsl(const char *pszUrl)
{
    return RTStrNICmp(pszUrl, RT_STR_TUPLE("https:")) == 0;
}


/**
 * Applies recoded settings to the cURL instance before doing work.
 *
 * @returns IPRT status code.
 * @param   pThis           The HTTP/HTTPS client instance.
 * @param   pszUrl          The URL.
 */
static int rtHttpApplySettings(PRTHTTPINTERNAL pThis, const char *pszUrl)
{
    /*
     * The URL.
     */
    int rcCurl = curl_easy_setopt(pThis->pCurl, CURLOPT_URL, pszUrl);
    if (CURL_FAILURE(rcCurl))
        return VERR_INVALID_PARAMETER;

    /*
     * Setup SSL.  Can be a bit of work.
     */
    rcCurl = curl_easy_setopt(pThis->pCurl, CURLOPT_SSLVERSION, (long)CURL_SSLVERSION_TLSv1);
    if (CURL_FAILURE(rcCurl))
        return VERR_INVALID_PARAMETER;

    const char *pszCaFile = pThis->pszCaFile;
    if (   !pszCaFile
        && rtHttpNeedSsl(pszUrl))
    {
        int rc = RTHttpUseTemporaryCaFile(pThis, NULL);
        if (RT_SUCCESS(rc))
            pszCaFile = pThis->pszCaFile;
        else
            return rc; /* Non-portable alternative: pszCaFile = "/etc/ssl/certs/ca-certificates.crt"; */
    }
    if (pszCaFile)
    {
        rcCurl = curl_easy_setopt(pThis->pCurl, CURLOPT_CAINFO, pszCaFile);
        if (CURL_FAILURE(rcCurl))
            return VERR_INTERNAL_ERROR;
    }

    /*
     * Progress/abort.
     */
    rcCurl = curl_easy_setopt(pThis->pCurl, CURLOPT_PROGRESSFUNCTION, &rtHttpProgress);
    if (CURL_FAILURE(rcCurl))
        return VERR_INTERNAL_ERROR;
    rcCurl = curl_easy_setopt(pThis->pCurl, CURLOPT_PROGRESSDATA, (void *)pThis);
    if (CURL_FAILURE(rcCurl))
        return VERR_INTERNAL_ERROR;
    rcCurl = curl_easy_setopt(pThis->pCurl, CURLOPT_NOPROGRESS, (long)0);
    if (CURL_FAILURE(rcCurl))
        return VERR_INTERNAL_ERROR;

    return VINF_SUCCESS;
}


/**
 * cURL callback for writing data.
 */
static size_t rtHttpWriteData(void *pvBuf, size_t cbUnit, size_t cUnits, void *pvUser)
{
    PRTHTTPINTERNAL pThis = (PRTHTTPINTERNAL)pvUser;

    /*
     * Do max size and overflow checks.
     */
    size_t const cbToAppend = cbUnit * cUnits;
    size_t const cbCurSize  = pThis->Output.Mem.cb;
    size_t const cbNewSize  = cbCurSize + cbToAppend;
    if (   cbToAppend < RTHTTP_MAX_MEM_DOWNLOAD_SIZE
        && cbNewSize  < RTHTTP_MAX_MEM_DOWNLOAD_SIZE)
    {
        if (cbNewSize + 1 <= pThis->Output.Mem.cbAllocated)
        {
            memcpy(&pThis->Output.Mem.pb[cbCurSize], pvBuf, cbToAppend);
            pThis->Output.Mem.cb = cbNewSize;
            pThis->Output.Mem.pb[cbNewSize] = '\0';
            return cbToAppend;
        }

        /*
         * We need to reallocate the output buffer.
         */
        /** @todo this could do with a better strategy wrt growth. */
        size_t cbAlloc = RT_ALIGN_Z(cbNewSize + 1, 64);
        if (   cbAlloc <= pThis->cbDownloadHint
            && pThis->cbDownloadHint < RTHTTP_MAX_MEM_DOWNLOAD_SIZE)
            cbAlloc = RT_ALIGN_Z(pThis->cbDownloadHint + 1, 64);

        uint8_t *pbNew = (uint8_t *)RTMemRealloc(pThis->Output.Mem.pb, cbAlloc);
        if (pbNew)
        {
            memcpy(&pbNew[cbCurSize], pvBuf, cbToAppend);
            pbNew[cbNewSize] = '\0';

            pThis->Output.Mem.cbAllocated = cbAlloc;
            pThis->Output.Mem.pb = pbNew;
            pThis->Output.Mem.cb = cbNewSize;
            return cbToAppend;
        }

        pThis->rcOutput = VERR_NO_MEMORY;
    }
    else
        pThis->rcOutput = VERR_TOO_MUCH_DATA;

    /*
     * Failure - abort.
     */
    RTMemFree(pThis->Output.Mem.pb);
    pThis->Output.Mem.pb = NULL;
    pThis->Output.Mem.cb = RTHTTP_MAX_MEM_DOWNLOAD_SIZE;
    pThis->fAbort        = true;
    return 0;
}


/**
 * Internal worker that performs a HTTP GET.
 *
 * @returns IPRT status code.
 * @param   hHttp               The HTTP/HTTPS client instance.
 * @param   pszUrl              The URL.
 * @param   ppvResponse         Where to return the pointer to the allocated
 *                              response data (RTMemFree).  There will always be
 *                              an zero terminator char after the response, that
 *                              is not part of the size returned via @a pcb.
 * @param   pcb                 The size of the response data.
 *
 * @remarks We ASSUME the API user doesn't do concurrent GETs in different
 *          threads, because that will probably blow up!
 */
static int rtHttpGetToMem(RTHTTP hHttp, const char *pszUrl, uint8_t **ppvResponse, size_t *pcb)
{
    PRTHTTPINTERNAL pThis = hHttp;
    RTHTTP_VALID_RETURN(pThis);

    /*
     * Reset the return values in case of more "GUI programming" on the client
     * side (i.e. a programming style not bothering checking return codes).
     */
    *ppvResponse = NULL;
    *pcb         = 0;

    /*
     * Set the busy flag (paranoia).
     */
    bool fBusy = ASMAtomicXchgBool(&pThis->fBusy, true);
    AssertReturn(!fBusy, VERR_WRONG_ORDER);

    /*
     * Reset the state and apply settings.
     */
    pThis->fAbort = false;
    pThis->rcOutput = VINF_SUCCESS;
    pThis->cbDownloadHint = 0;

    int rc = rtHttpApplySettings(hHttp, pszUrl);
    if (RT_SUCCESS(rc))
    {
        RT_ZERO(pThis->Output.Mem);
        int rcCurl = curl_easy_setopt(pThis->pCurl, CURLOPT_WRITEFUNCTION, &rtHttpWriteData);
        if (!CURL_FAILURE(rcCurl))
            rcCurl = curl_easy_setopt(pThis->pCurl, CURLOPT_WRITEDATA, (void *)pThis);
        if (!CURL_FAILURE(rcCurl))
        {
            /*
             * Perform the HTTP operation.
             */
            rcCurl = curl_easy_perform(pThis->pCurl);
            rc = rtHttpGetCalcStatus(pThis, rcCurl);
            if (RT_SUCCESS(rc))
                rc = pThis->rcOutput;
            if (RT_SUCCESS(rc))
            {
                *ppvResponse = pThis->Output.Mem.pb;
                *pcb         = pThis->Output.Mem.cb;
            }
            else if (pThis->Output.Mem.pb)
                RTMemFree(pThis->Output.Mem.pb);
            RT_ZERO(pThis->Output.Mem);
        }
        else
            rc = VERR_INTERNAL_ERROR_3;
    }

    ASMAtomicWriteBool(&pThis->fBusy, false);
    return rc;
}


RTR3DECL(int) RTHttpGetText(RTHTTP hHttp, const char *pszUrl, char **ppszNotUtf8)
{
    uint8_t *pv;
    size_t   cb;
    int rc = rtHttpGetToMem(hHttp, pszUrl, &pv, &cb);
    if (RT_SUCCESS(rc))
    {
        if (pv) /* paranoia */
            *ppszNotUtf8 = (char *)pv;
        else
            *ppszNotUtf8 = (char *)RTMemDup("", 1);
    }
    else
        *ppszNotUtf8 = NULL;
    return rc;
}


RTR3DECL(void) RTHttpFreeResponseText(char *pszNotUtf8)
{
    RTMemFree(pszNotUtf8);
}


RTR3DECL(int) RTHttpGetBinary(RTHTTP hHttp, const char *pszUrl, void **ppvResponse, size_t *pcb)
{
    return rtHttpGetToMem(hHttp, pszUrl, (uint8_t **)ppvResponse, pcb);
}


RTR3DECL(void) RTHttpFreeResponse(void *pvResponse)
{
    RTMemFree(pvResponse);
}


/**
 * cURL callback for writing data to a file.
 */
static size_t rtHttpWriteDataToFile(void *pvBuf, size_t cbUnit, size_t cUnits, void *pvUser)
{
    PRTHTTPINTERNAL pThis = (PRTHTTPINTERNAL)pvUser;
    size_t cbWritten = 0;
    int rc = RTFileWrite(pThis->Output.hFile, pvBuf, cbUnit * cUnits, &cbWritten);
    if (RT_SUCCESS(rc))
        return cbWritten;
    pThis->rcOutput = rc;
    return 0;
}


RTR3DECL(int) RTHttpGetFile(RTHTTP hHttp, const char *pszUrl, const char *pszDstFile)
{
    PRTHTTPINTERNAL pThis = hHttp;
    RTHTTP_VALID_RETURN(pThis);

    /*
     * Set the busy flag (paranoia).
     */
    bool fBusy = ASMAtomicXchgBool(&pThis->fBusy, true);
    AssertReturn(!fBusy, VERR_WRONG_ORDER);

    /*
     * Reset the state and apply settings.
     */
    pThis->fAbort = false;
    pThis->rcOutput = VINF_SUCCESS;
    pThis->cbDownloadHint = 0;

    int rc = rtHttpApplySettings(hHttp, pszUrl);
    if (RT_SUCCESS(rc))
    {
        pThis->Output.hFile = NIL_RTFILE;
        int rcCurl = curl_easy_setopt(pThis->pCurl, CURLOPT_WRITEFUNCTION, &rtHttpWriteDataToFile);
        if (!CURL_FAILURE(rcCurl))
            rcCurl = curl_easy_setopt(pThis->pCurl, CURLOPT_WRITEDATA, (void *)pThis);
        if (!CURL_FAILURE(rcCurl))
        {
            /*
             * Open the output file.
             */
            rc = RTFileOpen(&pThis->Output.hFile, pszDstFile, RTFILE_O_CREATE_REPLACE | RTFILE_O_WRITE | RTFILE_O_DENY_READWRITE);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Perform the HTTP operation.
                 */
                rcCurl = curl_easy_perform(pThis->pCurl);
                rc = rtHttpGetCalcStatus(pThis, rcCurl);
                if (RT_SUCCESS(rc))
                     rc = pThis->rcOutput;

                int rc2 = RTFileClose(pThis->Output.hFile);
                if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                    rc = rc2;
            }
            pThis->Output.hFile = NIL_RTFILE;
        }
        else
            rc = VERR_INTERNAL_ERROR_3;
    }

    ASMAtomicWriteBool(&pThis->fBusy, false);
    return rc;
}

