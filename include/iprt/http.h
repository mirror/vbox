/* $Id$ */
/** @file
 * IPRT - Simple HTTP/HTTPS Client API.
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
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

#ifndef ___iprt_http_h
#define ___iprt_http_h

#include <iprt/types.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_http   RTHttp - Simple HTTP/HTTPS Client API
 * @ingroup grp_rt
 * @{
 */

/** @todo the following three definitions may move the iprt/types.h later. */
/** HTTP/HTTPS client handle. */
typedef R3PTRTYPE(struct RTHTTPINTERNAL *)      RTHTTP;
/** Pointer to a HTTP/HTTPS client handle. */
typedef RTHTTP                                 *PRTHTTP;
/** Nil HTTP/HTTPS client handle. */
#define NIL_RTHTTP                              ((RTHTTP)0)
/** Callback function to be called during RTHttpGet*(). Register it using RTHttpSetDownloadProgressCallback(). */
typedef DECLCALLBACK(void) FNRTHTTPDOWNLDPROGRCALLBACK(RTHTTP hHttp, void *pvUser, uint64_t cbDownloadTotal, uint64_t cbDownloaded);
typedef FNRTHTTPDOWNLDPROGRCALLBACK *PFNRTHTTPDOWNLDPROGRCALLBACK;



/**
 * Creates a HTTP client instance.
 *
 * @returns IPRT status code.
 * @param   phHttp      Where to store the HTTP handle.
 */
RTR3DECL(int) RTHttpCreate(PRTHTTP phHttp);

/**
 * Resets a HTTP client instance.
 *
 * @returns IPRT status code.
 * @param   hHttp      Handle to the HTTP interface.
 */
RTR3DECL(int) RTHttpReset(RTHTTP hHttp);

/**
 * Destroys a HTTP client instance.
 *
 * @returns IPRT status code.
 * @param   hHttp       Handle to the HTTP interface.
 */
RTR3DECL(int) RTHttpDestroy(RTHTTP hHttp);


/**
 * Retrieve the redir location for 301 responses.
 *
 * @param   hHttp               Handle to the HTTP interface.
 * @param   ppszRedirLocation   Where to store the string.  To be freed with
 *                              RTStrFree().
 */
RTR3DECL(int) RTHttpGetRedirLocation(RTHTTP hHttp, char **ppszRedirLocation);

/**
 * Perform a simple blocking HTTP GET request.
 *
 * This is a just a convenient wrapper around RTHttpGetBinary that returns a
 * different type and sheds a parameter.
 *
 * @returns iprt status code.
 *
 * @param   hHttp           The HTTP client handle.
 * @param   pszUrl          URL.
 * @param   ppszNotUtf8     Where to return the pointer to the HTTP response.
 *                          The string is of course zero terminated.  Use
 *                          RTHttpFreeReponseText to free.
 *
 * @remarks BIG FAT WARNING!
 *
 *          This function does not guarantee the that returned string is valid UTF-8 or
 *          any other kind of text encoding!
 *
 *          The caller must determine and validate the string encoding _before_
 *          passing it along to functions that expect UTF-8!
 *
 *          Also, this function does not guarantee that the returned string
 *          doesn't have embedded zeros and provides the caller no way of
 *          finding out!  If you are worried about the response from the HTTPD
 *          containing embedded zero's, use RTHttpGetBinary instead.
 */
RTR3DECL(int) RTHttpGetText(RTHTTP hHttp, const char *pszUrl, char **ppszNotUtf8);

/**
 * Perform a simple blocking HTTP HEAD request.
 *
 * This is a just a convenient wrapper around RTHttpGetBinary that returns a
 * different type and sheds a parameter.
 *
 * @returns iprt status code.
 *
 * @param   hHttp           The HTTP client handle.
 * @param   pszUrl          URL.
 * @param   ppszNotUtf8     Where to return the pointer to the HTTP response.
 *                          The string is of course zero terminated.  Use
 *                          RTHttpFreeReponseText to free.
 *
 * @remarks BIG FAT WARNING!
 *
 *          This function does not guarantee the that returned string is valid UTF-8 or
 *          any other kind of text encoding!
 *
 *          The caller must determine and validate the string encoding _before_
 *          passing it along to functions that expect UTF-8!
 *
 *          Also, this function does not guarantee that the returned string
 *          doesn't have embedded zeros and provides the caller no way of
 *          finding out!  If you are worried about the response from the HTTPD
 *          containing embedded zero's, use RTHttpGetHeaderBinary instead.
 */
RTR3DECL(int) RTHttpGetHeaderText(RTHTTP hHttp, const char *pszUrl, char **ppszNotUtf8);

/**
 * Frees memory returned by RTHttpGetText.
 *
 * @param   pszNotUtf8      What RTHttpGetText returned.
 */
RTR3DECL(void) RTHttpFreeResponseText(char *pszNotUtf8);

/**
 * Perform a simple blocking HTTP GET request.
 *
 * @returns iprt status code.
 *
 * @param   hHttp           The HTTP client handle.
 * @param   pszUrl          The URL.
 * @param   ppvResponse     Where to store the HTTP response data.  Use
 *                          RTHttpFreeResponse to free.
 * @param   pcb             Size of the returned buffer.
 */
RTR3DECL(int) RTHttpGetBinary(RTHTTP hHttp, const char *pszUrl, void **ppvResponse, size_t *pcb);

/**
 * Perform a simple blocking HTTP HEAD request.
 *
 * @returns iprt status code.
 *
 * @param   hHttp           The HTTP client handle.
 * @param   pszUrl          The URL.
 * @param   ppvResponse     Where to store the HTTP response data.  Use
 *                          RTHttpFreeResponse to free.
 * @param   pcb             Size of the returned buffer.
 */
RTR3DECL(int) RTHttpGetHeaderBinary(RTHTTP hHttp, const char *pszUrl, void **ppvResponse, size_t *pcb);

/**
 * Frees memory returned by RTHttpGetBinary.
 *
 * @param   pvResponse      What RTHttpGetBinary returned.
 */
RTR3DECL(void) RTHttpFreeResponse(void *pvResponse);

/**
 * Perform a simple blocking HTTP request, writing the output to a file.
 *
 * @returns iprt status code.
 *
 * @param   hHttp           The HTTP client handle.
 * @param   pszUrl          The URL.
 * @param   pszDstFile      The destination file name.
 */
RTR3DECL(int) RTHttpGetFile(RTHTTP hHttp, const char *pszUrl, const char *pszDstFile);

/** HTTP methods. */
typedef enum RTHTTPMETHOD
{
    RTHTTPMETHOD_INVALID = 0,
    RTHTTPMETHOD_GET,
    RTHTTPMETHOD_PUT,
    RTHTTPMETHOD_POST,
    RTHTTPMETHOD_PATCH,
    RTHTTPMETHOD_DELETE,
    RTHTTPMETHOD_HEAD,
    RTHTTPMETHOD_OPTIONS,
    RTHTTPMETHOD_TRACE,
    RTHTTPMETHOD_END,
    RTHTTPMETHOD_32BIT_HACK = 0x7fffffff
} RTHTTPMETHOD;

/**
 * Returns the name of the HTTP method.
 * @returns Read only string.
 * @param   enmMethod       The HTTP method to name.
 */
RTR3DECL(const char *) RTHttpMethodName(RTHTTPMETHOD enmMethod);

/**
 * Performs generic blocking HTTP request, optionally returning the body and headers.
 *
 * @returns IPRT status code.
 * @param   hHttp           The HTTP client handle.
 * @param   pszUrl          The URL.
 * @param   enmMethod       The HTTP method for the request.
 * @param   pvReqBody       Pointer to the request body. NULL if none.
 * @param   cbReqBody       Size of the request body. Zero if none.
 * @param   puHttpStatus    Where to return the HTTP status code. Optional.
 * @param   ppvHeaders      Where to return the headers. Optional.
 * @param   pcbHeaders      Where to return the header size.
 * @param   ppvBody         Where to return the body.  Optional.
 * @param   pcbBody         Where to return the body size.
 */
RTR3DECL(int) RTHttpPerform(RTHTTP hHttp, const char *pszUrl, RTHTTPMETHOD enmMethod, void const *pvReqBody, size_t cbReqBody,
                            uint32_t *puHttpStatus, void **ppvHeaders, size_t *pcbHeaders, void **ppvBody, size_t *pcbBody);


/**
 * Abort a pending HTTP request. A blocking RTHttpGet() call will return with
 * VERR_HTTP_ABORTED. It may take some time (current cURL implementation needs
 * up to 1 second) before the request is aborted.
 *
 * @returns iprt status code.
 *
 * @param   hHttp           The HTTP client handle.
 */
RTR3DECL(int) RTHttpAbort(RTHTTP hHttp);

/**
 * Tells the HTTP interface to use the system proxy configuration.
 *
 * @returns iprt status code.
 * @param   hHttp           The HTTP client handle.
 */
RTR3DECL(int) RTHttpUseSystemProxySettings(RTHTTP hHttp);

/**
 * Specify proxy settings.
 *
 * @returns iprt status code.
 *
 * @param   hHttp           The HTTP client handle.
 * @param   pszProxyUrl     URL of the proxy server.
 * @param   uPort           port number of the proxy, use 0 for not specifying a port.
 * @param   pszProxyUser    Username, pass NULL for no authentication.
 * @param   pszProxyPwd     Password, pass NULL for no authentication.
 *
 * @todo    This API does not allow specifying the type of proxy server... We're
 *          currently assuming it's a HTTP proxy.
 */
RTR3DECL(int) RTHttpSetProxy(RTHTTP hHttp, const char *pszProxyUrl, uint32_t uPort,
                             const char *pszProxyUser, const char *pszProxyPwd);

/**
 * Set follow redirects (3xx)
 *
 * @returns iprt status code.
 *
 * @param   hHttp           The HTTP client handle.
 * @param   cMaxRedirects   Max number of redirects to follow.  Zero if no
 *                          redirects should be followed but instead returned
 *                          to caller.
 */
RTR3DECL(int) RTHttpSetFollowRedirects(RTHTTP hHttp, uint32_t cMaxRedirects);

/**
 * Set custom raw headers.
 *
 * @returns iprt status code.
 *
 * @param   hHttp           The HTTP client handle.
 * @param   cHeaders        Number of custom headers.
 * @param   papszHeaders    Array of headers in form "foo: bar".
 */
RTR3DECL(int) RTHttpSetHeaders(RTHTTP hHttp, size_t cHeaders, const char * const *papszHeaders);

/** @name RTHTTPADDHDR_F_XXX - Flags for RTHttpAddRawHeader and RTHttpAddHeader
 * @{ */
#define RTHTTPADDHDR_F_BACK     UINT32_C(0) /**< Append the header. */
#define RTHTTPADDHDR_F_FRONT    UINT32_C(1) /**< Prepend the header. */
/** @} */

/**
 * Adds a raw header.
 *
 * @returns IPRT status code.
 * @param   hHttp           The HTTP client handle.
 * @param   pszHeader       Header string on the form "foo: bar".
 * @param   fFlags          RTHTTPADDHDR_F_FRONT or RTHTTPADDHDR_F_BACK.
 */
RTR3DECL(int) RTHttpAddRawHeader(RTHTTP hHttp, const char *pszHeader, uint32_t fFlags);

/**
 * Adds a header field and value.
 *
 * @returns IPRT status code.
 * @param   hHttp           The HTTP client handle.
 * @param   pszField        The header field name.
 * @param   pszValue        The header field value.
 * @param   fFlags          Only RTHTTPADDHDR_F_FRONT or RTHTTPADDHDR_F_BACK,
 *                          may be extended with encoding controlling flags if
 *                          needed later.
 */
RTR3DECL(int) RTHttpAddHeader(RTHTTP hHttp, const char *pszField, const char *pszValue, uint32_t fFlags);

/**
 * Gets a header previously added using RTHttpSetHeaders, RTHttpAppendRawHeader
 * or RTHttpAppendHeader.
 *
 * @returns Pointer to the header value on if found, otherwise NULL.
 * @param   hHttp           The HTTP client handle.
 * @param   pszField        The field name (no colon).
 * @param   cchField        The length of the field name or RTSTR_MAX.
 */
RTR3DECL(const char *) RTHttpGetHeader(RTHTTP hHttp, const char *pszField, size_t cchField);

/**
 * Sign all headers present according to pending "Signing HTTP Messages" RFC.
 *
 * Currently hardcoded RSA-SHA-256 algorithm choice.
 *
 * @returns IPRT status code.
 * @param   hHttp           The HTTP client handle.
 * @param   enmMethod       The HTTP method that will be used for the request.
 * @param   pszUrl          The target URL for the request.
 * @param   hKey            The RSA key to use when signing.
 * @param   pszKeyId        The key ID string corresponding to @a hKey.
 * @param   fFlags          Reserved for future, MBZ.
 *
 * @note    Caller is responsible for making all desired fields are present before
 *          making the call.
 *
 * @remarks Latest RFC draft at the time of writing:
 *          https://tools.ietf.org/html/draft-cavage-http-signatures-10
 */
RTR3DECL(int) RTHttpSignHeaders(RTHTTP hHttp, RTHTTPMETHOD enmMethod, const char *pszUrl,
                                RTCRKEY hKey, const char *pszKeyId, uint32_t fFlags);

/**
 * Tells the HTTP client instance to gather system CA certificates into a
 * temporary file and use it for HTTPS connections.
 *
 * This will be called automatically if a 'https' URL is presented and
 * RTHttpSetCaFile hasn't been called yet.
 *
 * @returns IPRT status code.
 * @param   hHttp           The HTTP client handle.
 * @param   pErrInfo        Where to store additional error/warning information.
 *                          Optional.
 */
RTR3DECL(int) RTHttpUseTemporaryCaFile(RTHTTP hHttp, PRTERRINFO pErrInfo);

/**
 * Set a custom certification authority file, containing root certificates.
 *
 * @returns iprt status code.
 *
 * @param   hHttp           The HTTP client handle.
 * @param   pszCAFile       File name containing root certificates.
 *
 * @remarks For portable HTTPS support, use RTHttpGatherCaCertsInFile and pass
 */
RTR3DECL(int) RTHttpSetCAFile(RTHTTP hHttp, const char *pszCAFile);

/**
 * Gathers certificates into a cryptographic (certificate) store
 *
 * This is a just a combination of RTHttpGatherCaCertsInStore and
 * RTCrStoreCertExportAsPem.
 *
 * @returns IPRT status code.
 * @param   hStore          The certificate store to gather the certificates
 *                          in.
 * @param   fFlags          RTHTTPGATHERCACERT_F_XXX.
 * @param   pErrInfo        Where to store additional error/warning information.
 *                          Optional.
 */
RTR3DECL(int) RTHttpGatherCaCertsInStore(RTCRSTORE hStore, uint32_t fFlags, PRTERRINFO pErrInfo);

/**
 * Gathers certificates into a file that can be used with RTHttpSetCAFile.
 *
 * This is a just a combination of RTHttpGatherCaCertsInStore and
 * RTCrStoreCertExportAsPem.
 *
 * @returns IPRT status code.
 * @param   pszCaFile       The output file.
 * @param   fFlags          RTHTTPGATHERCACERT_F_XXX.
 * @param   pErrInfo        Where to store additional error/warning information.
 *                          Optional.
 */
RTR3DECL(int) RTHttpGatherCaCertsInFile(const char *pszCaFile, uint32_t fFlags, PRTERRINFO pErrInfo);

/**
 * Set a callback function which is called during RTHttpGet*()
 *
 * @returns IPRT status code.
 * @param   hHttp           The HTTP client handle.
 * @param   pfnDownloadProgress Progress function to be called. Set it to
 *                          NULL to disable the callback.
 * @param   pvUser          Convenience pointer for the callback function.
 */
RTR3DECL(int) RTHttpSetDownloadProgressCallback(RTHTTP hHttp, PFNRTHTTPDOWNLDPROGRCALLBACK pfnDownloadProgress, void *pvUser);


/** @name thin wrappers for setting one or a few related curl options
 * @remarks Subject to change.
 * @{ */
typedef size_t FNRTHTTPREADCALLBACK(void *pbDst, size_t cbItem, size_t cItems, void *pvUser);
typedef FNRTHTTPREADCALLBACK *PFNRTHTTPREADCALLBACK;

#define RT_HTTP_READCALLBACK_ABORT 0x10000000 /* CURL_READFUNC_ABORT */

RTR3DECL(int) RTHttpSetReadCallback(RTHTTP hHttp, PFNRTHTTPREADCALLBACK pfnRead, void *pvUser);


typedef size_t FNRTHTTPWRITECALLBACK(char *pbSrc, size_t cbItem, size_t cItems, void *pvUser);
typedef FNRTHTTPWRITECALLBACK *PFNRTHTTPWRITECALLBACK;

RTR3DECL(int) RTHttpSetWriteCallback(RTHTTP hHttp, PFNRTHTTPWRITECALLBACK pfnWrite, void *pvUser);
RTR3DECL(int) RTHttpSetWriteHeaderCallback(RTHTTP hHttp, PFNRTHTTPWRITECALLBACK pfnWrite, void *pvUser);


RTR3DECL(int) RTHttpRawSetUrl(RTHTTP hHttp, const char *pszUrl);

RTR3DECL(int) RTHttpRawSetGet(RTHTTP hHttp);
RTR3DECL(int) RTHttpRawSetHead(RTHTTP hHttp);
RTR3DECL(int) RTHttpRawSetPost(RTHTTP hHttp);
RTR3DECL(int) RTHttpRawSetPut(RTHTTP hHttp);
RTR3DECL(int) RTHttpRawSetDelete(RTHTTP hHttp);
RTR3DECL(int) RTHttpRawSetCustomRequest(RTHTTP hHttp, const char *pszVerb);

RTR3DECL(int) RTHttpRawSetPostFields(RTHTTP hHttp, const void *pv, size_t cb);
RTR3DECL(int) RTHttpRawSetInfileSize(RTHTTP hHttp, RTFOFF cb);

RTR3DECL(int) RTHttpRawSetVerbose(RTHTTP hHttp, bool fValue);
RTR3DECL(int) RTHttpRawSetTimeout(RTHTTP hHttp, long sec);

RTR3DECL(int) RTHttpRawPerform(RTHTTP hHttp);

RTR3DECL(int) RTHttpRawGetResponseCode(RTHTTP hHttp, long *plCode);
/** @} */

/** @} */

RT_C_DECLS_END

#endif

