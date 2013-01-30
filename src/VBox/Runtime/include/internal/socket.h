/* $Id$ */
/** @file
 * IPRT - Internal Header for RTSocket.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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

#ifndef ___internal_socket_h
#define ___internal_socket_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/net.h>
/* Currently requires a bunch of socket headers. */


/** Native socket handle type. */
#ifdef RT_OS_WINDOWS
# define RTSOCKETNATIVE         SOCKET
#else
# define RTSOCKETNATIVE         int
#endif

/** NIL value for native socket handles. */
#ifdef RT_OS_WINDOWS
# define NIL_RTSOCKETNATIVE     INVALID_SOCKET
#else
# define NIL_RTSOCKETNATIVE     (-1)
#endif


RT_C_DECLS_BEGIN

#ifndef IPRT_INTERNAL_SOCKET_POLLING_ONLY
int rtSocketResolverError(void);
int rtSocketCreateForNative(RTSOCKETINT **ppSocket, RTSOCKETNATIVE hNative);
int rtSocketCreate(PRTSOCKET phSocket, int iDomain, int iType, int iProtocol);
int rtSocketBind(RTSOCKET hSocket, PCRTNETADDR pAddr);
int rtSocketListen(RTSOCKET hSocket, int cMaxPending);
int rtSocketAccept(RTSOCKET hSocket, PRTSOCKET phClient, struct sockaddr *pAddr, size_t *pcbAddr);
int rtSocketConnect(RTSOCKET hSocket, PCRTNETADDR pAddr);
int rtSocketSetOpt(RTSOCKET hSocket, int iLevel, int iOption, void const *pvValue, int cbValue);
#endif /* IPRT_INTERNAL_SOCKET_POLLING_ONLY */

int         rtSocketPollGetHandle(RTSOCKET hSocket, uint32_t fEvents, PRTHCINTPTR phNative);
#ifdef RT_OS_WINDOWS
uint32_t    rtSocketPollStart(RTSOCKET hSocket, RTPOLLSET hPollSet, uint32_t fEvents, bool fFinalEntry, bool fNoWait);
uint32_t    rtSocketPollDone(RTSOCKET hSocket, uint32_t fEvents, bool fFinalEntry, bool fHarvestEvents);
#endif /* RT_OS_WINDOWS */

RT_C_DECLS_END

#endif

