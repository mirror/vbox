/* $Id$ */
/** @file
 * IPRT - Simple HTTP Communication API.
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
 * @param    ppszResponse  HTTP response.
 */
RTR3DECL(int) RTHttpGet(RTHTTP hHttp, const char *pcszUrl, char **ppszResponse);

/** @} */

RT_C_DECLS_END

#endif

