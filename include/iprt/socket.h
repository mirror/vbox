/** @file
 * IPRT - Network Sockets.
 */

/*
 * Copyright (C) 2006-2010 Sun Microsystems, Inc.
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

#ifndef ___iprt_socket_h
#define ___iprt_socket_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/thread.h>
#include <iprt/net.h>

#ifdef IN_RING0
# error "There are no RTSocket APIs available Ring-0 Host Context!"
#endif


RT_C_DECLS_BEGIN

/** @defgroup grp_rt_tcp    RTSocket - Network Sockets
 * @ingroup grp_rt
 * @{
 */

/**
 * Retains a reference to the socket handle.
 *
 * @returns New reference count, UINT32_MAX on invalid handle (asserted).
 *
 * @param   hSocket         The socket handle.
 */
RTDECL(uint32_t) RTSocketRetain(RTSOCKET hSocket);

/**
 * Release a reference to the socket handle.
 *
 * When the reference count reaches zero, the socket handle is shut down and
 * destroyed.  This will not be graceful shutdown, use the protocol specific
 * close method if this is desired.
 *
 * @returns New reference count, UINT32_MAX on invalid handle (asserted).
 *
 * @param   hSocket         The socket handle.  The NIL handle is quietly
 *                          ignored and 0 is returned.
 */
RTDECL(uint32_t) RTSocketRelease(RTSOCKET hSocket);

/**
 * Shuts down the socket, close it and then release one handle reference.
 *
 * This is slightly different from RTSocketRelease which will first do the
 * shutting down and closing when the reference count reaches zero.
 *
 * @returns IPRT status code.
 * @param   hSocket         The socket handle.  NIL is ignored.
 *
 * @remarks This will not perform a graceful shutdown of the socket, it will
 *          just destroy it.  Use the protocol specific close method if this is
 *          desired.
 */
RTDECL(int) RTSocketClose(RTSOCKET hSocket);

/**
 * Creates an IPRT socket handle from a native one.
 *
 * Do NOT use the native handle after passing it to this function, IPRT owns it
 * and might even have closed upon a successful return.
 *
 * @returns IPRT status code.
 * @param   phSocket        Where to store the IPRT socket handle.
 * @param   uNative         The native handle.
 */
RTDECL(int) RTSocketFromNative(PRTSOCKET phSocket, RTHCINTPTR uNative);

/**
 * Gets the native socket handle.
 *
 * @returns The native socket handle or RTHCUINTPTR_MAX if not invalid.
 * @param   hSocket             The socket handle.
 */
RTDECL(RTHCUINTPTR) RTSocketToNative(RTSOCKET hSocket);

/**
 * Helper that ensures the correct inheritability of a socket.
 *
 * We're currently ignoring failures.
 *
 * @returns IPRT status code
 * @param   hSocket         The socket handle.
 * @param   fInheritable    The desired inheritability state.
 */
RTDECL(int) RTSocketSetInheritance(RTSOCKET hSocket, bool fInheritable);

/**
 * Receive data from a socket.
 *
 * @returns IPRT status code.
 * @param   hSocket         The socket handle.
 * @param   pvBuffer        Where to put the data we read.
 * @param   cbBuffer        Read buffer size.
 * @param   pcbRead         Number of bytes read. If NULL the entire buffer
 *                          will be filled upon successful return. If not NULL a
 *                          partial read can be done successfully.
 */
RTDECL(int) RTSocketRead(RTSOCKET hSocket, void *pvBuffer, size_t cbBuffer, size_t *pcbRead);

/**
 * Send data to a socket.
 *
 * @returns IPRT status code.
 * @retval  VERR_INTERRUPTED if interrupted before anything was written.
 *
 * @param   hSocket         The socket handle.
 * @param   pvBuffer        Buffer to write data to socket.
 * @param   cbBuffer        How much to write.
 */
RTDECL(int) RTSocketWrite(RTSOCKET hSocket, const void *pvBuffer, size_t cbBuffer);

/**
 * Checks if the socket is ready for reading (for I/O multiplexing).
 *
 * @returns IPRT status code.
 * @param   hSocket         The socket handle.
 * @param   cMillies        Number of milliseconds to wait for the socket.  Use
 *                          RT_INDEFINITE_WAIT to wait for ever.
 */
RTDECL(int) RTSocketSelectOne(RTSOCKET hSocket, RTMSINTERVAL cMillies);

/**
 * Shuts down one or both directions of communciation.
 *
 * @returns IPRT status code.
 * @param   hSocket             The socket handle.
 * @param   fRead               Whether to shutdown our read direction.
 * @param   fWrite              Whether to shutdown our write direction.
 */
RTDECL(int) RTSocketShutdown(RTSOCKET hSocket, bool fRead, bool fWrite);

/**
 * Gets the address of the local side.
 *
 * @returns IPRT status code.
 * @param   Sock            Socket descriptor.
 * @param   pAddr           Where to store the local address on success.
 */
RTDECL(int) RTSocketGetLocalAddress(RTSOCKET hSocket, PRTNETADDR pAddr);

/**
 * Gets the address of the other party.
 *
 * @returns IPRT status code.
 * @param   Sock            Socket descriptor.
 * @param   pAddr           Where to store the peer address on success.
 */
RTDECL(int) RTSocketGetPeerAddress(RTSOCKET hSocket, PRTNETADDR pAddr);

/** @} */
RT_C_DECLS_END

#endif

