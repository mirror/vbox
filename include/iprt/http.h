/* $Id$ */
/** @file
 * IPRT - Simple HTTP Communication API.
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

#ifndef ___iprt_http_h
#define ___iprt_http_h

#include <iprt/types.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_http   RTHttp - Simple HTTP API
 * @ingroup grp_rt
 * @{
 */

/** @todo the following three definitions may move the iprt/types.h later. */
/** RTHTTP interface handle. */
typedef R3PTRTYPE(struct RTHTTPINTERNAL *)      RTHTTP;
/** Pointer to a RTHTTP interface handle. */
typedef RTHTTP                                  *PRTHTTP;
/** Nil RTHTTP interface handle. */
#define NIL_RTHTTP                              ((RTHTTP)0)


/**
 * Creates a HTTP interface handle.
 *
 * @returns iprt status code.
 *
 * @param   phHttp      Where to store the HTTP handle.
 */
RTR3DECL(int) RTHttpCreate(PRTHTTP phHttp);

/**
 * Destroys a HTTP interface handle.
 *
 * @param   hHttp       Handle to the HTTP interface.
 */
RTR3DECL(void) RTHttpDestroy(RTHTTP hHttp);

/**
 * Perform a simple blocking HTTP request.
 *
 * @returns iprt status code.
 *
 * @param    hHttp         HTTP interface handle.
 * @param    pcszUrl       URL.
 * @param    ppszResponse  HTTP response. It is guaranteed that this string is
 *                         '\0'-terminated.
 */
RTR3DECL(int) RTHttpGet(RTHTTP hHttp, const char *pcszUrl, char **ppszResponse);

/**
 * Perform a simple blocking HTTP request, writing the output to a file.
 *
 * @returns iprt status code.
 *
 * @param   hHttp           HTTP interface handle.
 * @param   pszUrl          URL.
 * @param   pszDstFile      The destination file name.
 */
RTR3DECL(int) RTHttpGetFile(RTHTTP hHttp, const char *pszUrl, const char *pszDstFile);

/**
 * Abort a pending HTTP request. A blocking RTHttpGet() call will return with
 * VERR_HTTP_ABORTED. It may take some time (current cURL implementation needs
 * up to 1 second) before the request is aborted.
 *
 * @returns iprt status code.
 *
 * @param    hHttp         HTTP interface handle.
 */
RTR3DECL(int) RTHttpAbort(RTHTTP hHttp);

/**
 * Tells the HTTP interface to use the system proxy configuration.
 *
 * @returns iprt status code.
 * @param   hHttp           HTTP interface handle.
 */
RTR3DECL(int) RTHttpUseSystemProxySettings(RTHTTP hHttp);

/**
 * Specify proxy settings.
 *
 * @returns iprt status code.
 *
 * @param    hHttp         HTTP interface handle.
 * @param    pcszProxy     URL of the proxy
 * @param    uPort         port number of the proxy, use 0 for not specifying a port.
 * @param    pcszUser      username, pass NULL for no authentication
 * @param    pcszPwd       password, pass NULL for no authentication
 */
RTR3DECL(int) RTHttpSetProxy(RTHTTP hHttp, const char *pcszProxyUrl, uint32_t uPort,
                             const char *pcszProxyUser, const char *pcszProxyPwd);

/**
 * Set custom headers.
 *
 * @returns iprt status code.
 *
 * @param    hHttp         HTTP interface handle.
 * @param    cHeaders      number of custom headers.
 * @param    pcszHeaders   array of headers in form "foo: bar".
 */
RTR3DECL(int) RTHttpSetHeaders(RTHTTP hHttp, size_t cHeaders, const char * const *papszHeaders);

/**
 * Set a custom certification authority file, containing root certificates.
 *
 * @returns iprt status code.
 *
 * @param    hHttp         HTTP interface handle.
 * @param    pcszCAFile    File name containing root certificates.
 */
RTR3DECL(int) RTHttpSetCAFile(RTHTTP hHttp, const char *pcszCAFile);


/**
 * Determine the digest (fingerprint) of a certificate. Allocate memory for
 * storing the SHA1 fingerprint as well as the SHA512 fingerprint. This
 * memory has to be freed by RTMemFree().
 *
 * @todo Move this function to somewhere else.
 *
 * @returns iprt status code.
 *
 * @param    hHttp         HTTP interface handle (ignored).
 * @param    pcszCert      The certificate in PEM format.
 * @param    cbCert        Size of the certificate.
 * @param    pabSha1       Where to store the pointer to the SHA1 fingerprint.
 * @param    pcbSha1       Where to store the size of the SHA1 fingerprint.
 * @param    pabSha512     Where to store the pointer to the SHA512 fingerprint.
 * @param    pcbSha512     Where to store the size of the SHA512 fingerprint.
 */
RTR3DECL(int) RTHttpCertDigest(RTHTTP hHttp, char *pcszCert, size_t cbCert,
                               uint8_t **pabSha1, size_t *pcbSha1,
                               uint8_t **pabSha512, size_t *pcbSha512);


/** @} */

RT_C_DECLS_END

#endif

