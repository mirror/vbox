/* $Id$ */
/** @file
 * Header file for HTTP server implementation.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
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

#ifndef IPRT_INCLUDED_http_server_h
#define IPRT_INCLUDED_http_server_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/http-common.h>
#include <iprt/types.h>
#include <iprt/fs.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_http       RTHttp - HTTP server.
 * @ingroup grp_rt
 * @{
 */

/** @defgroup grp_rt_httpserver  RTHttpServer - HTTP server implementation.
 * @{
 */

/** @todo the following three definitions may move the iprt/types.h later. */
/** HTTP server handle. */
typedef R3PTRTYPE(struct RTHTTPSERVERINTERNAL *) RTHTTPSERVER;
/** Pointer to a HTTP server handle. */
typedef RTHTTPSERVER                            *PRTHTTPSERVER;
/** Nil HTTP client handle. */
#define NIL_RTHTTPSERVER                         ((RTHTTPSERVER)0)

/**
 * Structure for maintaining a HTTP client request.
 */
typedef struct RTHTTPSERVERREQ
{
    char            *pszUrl;
    RTHTTPMETHOD     enmMethod;
    RTHTTPHEADERLIST hHdrLst;
    void            *pvBody;
    size_t           cbBodyAlloc;
    size_t           cbBodyUsed;
} RTHTTPSERVERREQ;
/** Pointer to a HTTP client request. */
typedef RTHTTPSERVERREQ *PRTHTTPSERVERREQ;

/**
 * Structure for maintaining a HTTP server response.
 */
typedef struct RTHTTPSERVERRESP
{
    RTHTTPSTATUS     enmSts;
    RTHTTPHEADERLIST hHdrLst;
    void            *pvBody;
    size_t           cbBodyAlloc;
    size_t           cbBodyUsed;
} RTHTTPSERVERRESP;
/** Pointer to a HTTP server response. */
typedef RTHTTPSERVERRESP *PRTHTTPSERVERRESP;

/**
 * Structure for maintaining a HTTP server client state.
 *
 * Note: The HTTP protocol itself is stateless, but we want to have to possibility to store
 *       some state stuff here nevertheless.
 */
typedef struct RTHTTPSERVERCLIENTSTATE
{
    uint32_t         fUnused;
} RTHTTPSERVERCLIENTSTATE;
/** Pointer to a FTP server client state. */
typedef RTHTTPSERVERCLIENTSTATE *PRTHTTPSERVERCLIENTSTATE;

/**
 * Structure for storing HTTP server callback data.
 */
typedef struct RTHTTPCALLBACKDATA
{
    /** Pointer to the client state. */
    PRTHTTPSERVERCLIENTSTATE pClient;
    /** Saved user pointer. */
    void                    *pvUser;
    /** Size (in bytes) of data at user pointer. */
    size_t                   cbUser;
} RTHTTPCALLBACKDATA;
/** Pointer to HTTP server callback data. */
typedef RTHTTPCALLBACKDATA *PRTHTTPCALLBACKDATA;

/**
 * Function callback table for the HTTP server implementation.
 *
 * All callbacks are optional and therefore can be NULL.
 */
typedef struct RTHTTPSERVERCALLBACKS
{
    DECLCALLBACKMEMBER(int, pfnOpen,(PRTHTTPCALLBACKDATA pData, const char *pszUrl, uint64_t *pidObj));
    DECLCALLBACKMEMBER(int, pfnRead,(PRTHTTPCALLBACKDATA pData, uint64_t idObj, void *pvBuf, size_t cbBuf, size_t *pcbRead));
    DECLCALLBACKMEMBER(int, pfnClose,(PRTHTTPCALLBACKDATA pData, uint64_t idObj));
    DECLCALLBACKMEMBER(int, pfnQueryInfo,(PRTHTTPCALLBACKDATA pData, const char *pszUrl, PRTFSOBJINFO pObjInfo));
    DECLCALLBACKMEMBER(int, pfnOnGetRequest,(PRTHTTPCALLBACKDATA pData, PRTHTTPSERVERREQ pReq));
    DECLCALLBACKMEMBER(int, pfnOnHeadRequest,(PRTHTTPCALLBACKDATA pData, PRTHTTPSERVERREQ pReq));
} RTHTTPSERVERCALLBACKS;
/** Pointer to a HTTP server callback data table. */
typedef RTHTTPSERVERCALLBACKS *PRTHTTPSERVERCALLBACKS;

/** Maximum length (in bytes) a client request can have. */
#define RTHTTPSERVER_MAX_REQ_LEN        _8K

/**
 * Creates a HTTP server instance.
 *
 * @returns IPRT status code.
 * @param   phHttpServer        Where to store the HTTP server handle.
 * @param   pcszAddress         The address for creating a listening socket.
 *                              If NULL or empty string the server is bound to all interfaces.
 * @param   uPort               The port for creating a listening socket.
 * @param   pCallbacks          Callback table to use.
 * @param   pvUser              Pointer to user-specific data. Optional.
 * @param   cbUser              Size of user-specific data. Optional.
 */
RTR3DECL(int) RTHttpServerCreate(PRTHTTPSERVER phHttpServer, const char *pcszAddress, uint16_t uPort,
                                 PRTHTTPSERVERCALLBACKS pCallbacks, void *pvUser, size_t cbUser);

/**
 * Destroys a HTTP server instance.
 *
 * @returns IPRT status code.
 * @param   hHttpServer          Handle to the HTTP server handle.
 */
RTR3DECL(int) RTHttpServerDestroy(RTHTTPSERVER hHttpServer);

/** @} */

/** @} */
RT_C_DECLS_END

#endif /* !IPRT_INCLUDED_http_server_h */

