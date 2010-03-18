/* $Id$ */
/** @file
 * IPRT - Internal Header for RTSocket.
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___internal_socket_h
#define ___internal_socket_h

#include <iprt/cdefs.h>
#include <iprt/types.h>


RT_C_DECLS_BEGIN

int rtSocketResolverError(void);
int rtSocketCreateForNative(RTSOCKETINT **ppSocket,
#ifdef RT_OS_WINDOWS
                            SOCKET hNative
#else
                            int hNative
#endif
                            );
int rtSocketCreate(PRTSOCKET phSocket, int iDomain, int iType, int iProtocol);
int rtSocketBind(RTSOCKET hSocket, const struct sockaddr *pAddr, int cbAddr);
int rtSocketListen(RTSOCKET hSocket, int cMaxPending);
int rtSocketAccept(RTSOCKET hSocket, PRTSOCKET phClient, struct sockaddr *pAddr, size_t *pcbAddr);
int rtSocketConnect(RTSOCKET hSocket, const struct sockaddr *pAddr, int cbAddr);
int rtSocketSetOpt(RTSOCKET hSocket, int iLevel, int iOption, void const *pvValue, int cbValue);

RT_C_DECLS_END

#endif

