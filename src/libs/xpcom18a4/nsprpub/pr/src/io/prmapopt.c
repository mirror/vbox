/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * This file defines _PR_MapOptionName().  The purpose of putting
 * _PR_MapOptionName() in a separate file is to work around a Winsock
 * header file problem on Windows NT.
 *
 * On Windows NT, if we define _WIN32_WINNT to be 0x0400 (in order
 * to use Service Pack 3 extensions), windows.h includes winsock2.h
 * (instead of winsock.h), which doesn't define many socket options
 * defined in winsock.h.
 *
 * We need the socket options defined in winsock.h.  So this file
 * includes winsock.h, with _WIN32_WINNT undefined.
 */

#if defined(WINNT) || defined(__MINGW32__)
#include <winsock.h>
#endif

/* MinGW doesn't define these in its winsock.h. */
#ifdef __MINGW32__
#ifndef IP_TTL
#define IP_TTL 7
#endif
#ifndef IP_TOS
#define IP_TOS 8
#endif
#endif

#include "primpl.h"

#if defined(NEXTSTEP)
/* NEXTSTEP is special: this must come before netinet/tcp.h. */
#include <netinet/in_systm.h>  /* n_short, n_long, n_time */
#endif

#if defined(XP_UNIX) || defined(OS2) || (defined(XP_BEOS) && defined(BONE_VERSION))
#include <netinet/tcp.h>  /* TCP_NODELAY, TCP_MAXSEG */
#endif

/*
 *********************************************************************
 *********************************************************************
 **
 ** Make sure that the following is at the end of this file,
 ** because we will be playing with macro redefines.
 **
 *********************************************************************
 *********************************************************************
 */

#if defined(VMS)
/*
** Sad but true. The DEC C header files define the following socket options
** differently to what UCX is expecting. The values that UCX expects are
** defined in SYS$LIBRARY:UCX$INETDEF.H. We redefine them here to the values
** that UCX expects. Note that UCX V4.x will only accept these values while
** UCX V5.x will accept either. So in theory this hack can be removed once
** UCX V5 is the minimum.
*/
#undef IP_MULTICAST_IF
#undef IP_MULTICAST_TTL
#undef IP_MULTICAST_LOOP
#undef IP_ADD_MEMBERSHIP
#undef IP_DROP_MEMBERSHIP
#include <ucx$inetdef.h>
#define IP_MULTICAST_IF    UCX$C_IP_MULTICAST_IF
#define IP_MULTICAST_TTL   UCX$C_IP_MULTICAST_TTL
#define IP_MULTICAST_LOOP  UCX$C_IP_MULTICAST_LOOP
#define IP_ADD_MEMBERSHIP  UCX$C_IP_ADD_MEMBERSHIP
#define IP_DROP_MEMBERSHIP UCX$C_IP_DROP_MEMBERSHIP
#endif

/*
 * Not every platform has all the socket options we want to
 * support.  Some older operating systems such as SunOS 4.1.3
 * don't have the IP multicast socket options.  Win32 doesn't
 * have TCP_MAXSEG.
 *
 * To deal with this problem, we define the missing socket
 * options as _PR_NO_SUCH_SOCKOPT.  _PR_MapOptionName() fails with
 * PR_OPERATION_NOT_SUPPORTED_ERROR if a socket option not
 * available on the platform is requested.
 */

/*
 * Sanity check.  SO_LINGER and TCP_NODELAY should be available
 * on all platforms.  Just to make sure we have included the
 * appropriate header files.  Then any undefined socket options
 * are really missing.
 */

#if !defined(SO_LINGER)
#error "SO_LINGER is not defined"
#endif

/*
 * Some platforms, such as NCR 2.03, don't have TCP_NODELAY defined
 * in <netinet/tcp.h>
 */
#if !defined(NCR)
#if !defined(TCP_NODELAY)
#error "TCP_NODELAY is not defined"
#endif
#endif

/*
 * Make sure the value of _PR_NO_SUCH_SOCKOPT is not
 * a valid socket option.
 */
#define _PR_NO_SUCH_SOCKOPT -1

#ifndef SO_KEEPALIVE
#define SO_KEEPALIVE        _PR_NO_SUCH_SOCKOPT
#endif

#ifndef SO_SNDBUF
#define SO_SNDBUF           _PR_NO_SUCH_SOCKOPT
#endif

#ifndef SO_RCVBUF
#define SO_RCVBUF           _PR_NO_SUCH_SOCKOPT
#endif

#ifndef IP_MULTICAST_IF                 /* set/get IP multicast interface   */
#define IP_MULTICAST_IF     _PR_NO_SUCH_SOCKOPT
#endif

#ifndef IP_MULTICAST_TTL                /* set/get IP multicast timetolive  */
#define IP_MULTICAST_TTL    _PR_NO_SUCH_SOCKOPT
#endif

#ifndef IP_MULTICAST_LOOP               /* set/get IP multicast loopback    */
#define IP_MULTICAST_LOOP   _PR_NO_SUCH_SOCKOPT
#endif

#ifndef IP_ADD_MEMBERSHIP               /* add  an IP group membership      */
#define IP_ADD_MEMBERSHIP   _PR_NO_SUCH_SOCKOPT
#endif

#ifndef IP_DROP_MEMBERSHIP              /* drop an IP group membership      */
#define IP_DROP_MEMBERSHIP  _PR_NO_SUCH_SOCKOPT
#endif

#ifndef IP_TTL                          /* set/get IP Time To Live          */
#define IP_TTL              _PR_NO_SUCH_SOCKOPT
#endif

#ifndef IP_TOS                          /* set/get IP Type Of Service       */
#define IP_TOS              _PR_NO_SUCH_SOCKOPT
#endif

#ifndef TCP_NODELAY                     /* don't delay to coalesce data     */
#define TCP_NODELAY         _PR_NO_SUCH_SOCKOPT
#endif

#ifndef TCP_MAXSEG                      /* maxumum segment size for tcp     */
#define TCP_MAXSEG          _PR_NO_SUCH_SOCKOPT
#endif

#ifndef SO_BROADCAST                 /* enable broadcast on udp sockets */
#define SO_BROADCAST        _PR_NO_SUCH_SOCKOPT
#endif

PRStatus _PR_MapOptionName(
    PRSockOption optname, PRInt32 *level, PRInt32 *name)
{
    static PRInt32 socketOptions[PR_SockOpt_Last] =
    {
        0, SO_LINGER, SO_REUSEADDR, SO_KEEPALIVE, SO_RCVBUF, SO_SNDBUF,
        IP_TTL, IP_TOS, IP_ADD_MEMBERSHIP, IP_DROP_MEMBERSHIP,
        IP_MULTICAST_IF, IP_MULTICAST_TTL, IP_MULTICAST_LOOP,
        TCP_NODELAY, TCP_MAXSEG, SO_BROADCAST
    };
    static PRInt32 socketLevels[PR_SockOpt_Last] =
    {
        0, SOL_SOCKET, SOL_SOCKET, SOL_SOCKET, SOL_SOCKET, SOL_SOCKET,
        IPPROTO_IP, IPPROTO_IP, IPPROTO_IP, IPPROTO_IP,
        IPPROTO_IP, IPPROTO_IP, IPPROTO_IP,
        IPPROTO_TCP, IPPROTO_TCP, SOL_SOCKET
    };

    if ((optname < PR_SockOpt_Linger)
    || (optname >= PR_SockOpt_Last))
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }

    if (socketOptions[optname] == _PR_NO_SUCH_SOCKOPT)
    {
        PR_SetError(PR_OPERATION_NOT_SUPPORTED_ERROR, 0);
        return PR_FAILURE;
    }
    *name = socketOptions[optname];
    *level = socketLevels[optname];
    return PR_SUCCESS;
}  /* _PR_MapOptionName */
