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

#include "primpl.h"

#include <string.h>

void _PR_InitNet(void)
{
    PR_ASSERT("_PR_InitNet called!");
}  /* _PR_InitNet */

void _PR_CleanupNet(void)
{
    PR_ASSERT("_PR_CleanupNet called!");
}  /* _PR_CleanupNet */

PR_IMPLEMENT(PRStatus) PR_GetHostByName(
    const char *name, char *buf, PRIntn bufsize, PRHostEnt *hp)
{
    PR_ASSERT("PR_GetHostByName called!");
    return 0;
}  /* PR_GetHostByName */

PR_IMPLEMENT(PRStatus) PR_GetIPNodeByName(
    const char *name, PRUint16 af, PRIntn flags,
    char *buf, PRIntn bufsize, PRHostEnt *hp)
{
    PR_ASSERT("PR_GetIPNodeByName called!");
    return 0;
}  /* PR_GetIPNodeByName */

PR_IMPLEMENT(PRStatus) PR_GetHostByAddr(
    const PRNetAddr *hostaddr, char *buf, PRIntn bufsize, PRHostEnt *hostentry)
{
    PR_ASSERT("PR_GetHostByAddr called!");
    return 0;
}  /* PR_GetHostByAddr */

PR_IMPLEMENT(PRStatus) PR_GetProtoByName(
    const char* name, char* buffer, PRInt32 buflen, PRProtoEnt* result)
{
    PR_ASSERT("PR_GetProtoByName called!");
    return 0;
}  /* PR_GetProtoByName */

PR_IMPLEMENT(PRStatus) PR_GetProtoByNumber(
    PRInt32 number, char* buffer, PRInt32 buflen, PRProtoEnt* result)
{
    PR_ASSERT("PR_GetProtoByNumber called!");
    return 0;
}  /* PR_GetProtoByNumber */

PRUintn _PR_NetAddrSize(const PRNetAddr* addr)
{
    PR_ASSERT("_PR_NetAddrSize called!");
    return 0;
}  /* _PR_NetAddrSize */

PR_IMPLEMENT(PRIntn) PR_EnumerateHostEnt(
    PRIntn enumIndex, const PRHostEnt *hostEnt, PRUint16 port, PRNetAddr *address)
{
    PR_ASSERT("PR_EnumerateHostEnt called!");
    return 0;
}  /* PR_EnumerateHostEnt */

PR_IMPLEMENT(PRStatus) PR_InitializeNetAddr(
    PRNetAddrValue val, PRUint16 port, PRNetAddr *addr)
{
    PR_ASSERT("PR_InitializeNetAddr called!");
    return 0;
}  /* PR_InitializeNetAddr */

PR_IMPLEMENT(PRStatus) PR_SetNetAddr(
    PRNetAddrValue val, PRUint16 af, PRUint16 port, PRNetAddr *addr)
{
    PR_ASSERT("PR_SetNetAddr called!");
    return 0;
}  /* PR_SetNetAddr */

PR_IMPLEMENT(PRBool)
PR_IsNetAddrType(const PRNetAddr *addr, PRNetAddrValue val)
{
    PR_ASSERT("PR_IsNetAddrType called!");
    return PR_FALSE;
}  /* PR_IsNetAddrType */

PR_IMPLEMENT(PRStatus) PR_StringToNetAddr(const char *string, PRNetAddr *addr)
{
    PR_ASSERT("PR_StringToNetAddr called!");
    return 0;
}  /* PR_StringToNetAddr */

PR_IMPLEMENT(PRStatus) PR_NetAddrToString(
    const PRNetAddr *addr, char *string, PRUint32 size)
{
    PR_ASSERT("PR_NetAddrToString called!");
    return 0;
}  /* PR_NetAddrToString */

/*
 * Convert an IPv4 addr to an (IPv4-mapped) IPv6 addr
 */
PR_IMPLEMENT(void) PR_ConvertIPv4AddrToIPv6(PRUint32 v4addr, PRIPv6Addr *v6addr)
{
    PRUint8 *dstp;
    dstp = v6addr->pr_s6_addr;
    memset(dstp, 0, 10);
    memset(dstp + 10, 0xff, 2);
    memcpy(dstp + 12,(char *) &v4addr, 4);
}

PR_IMPLEMENT(PRUint16) PR_ntohs(PRUint16 n) { return ntohs(n); }
PR_IMPLEMENT(PRUint32) PR_ntohl(PRUint32 n) { return ntohl(n); }
PR_IMPLEMENT(PRUint16) PR_htons(PRUint16 n) { return htons(n); }
PR_IMPLEMENT(PRUint32) PR_htonl(PRUint32 n) { return htonl(n); }
PR_IMPLEMENT(PRUint64) PR_ntohll(PRUint64 n)
{
#ifdef IS_BIG_ENDIAN
    return n;
#else
    PRUint64 tmp;
    PRUint32 hi, lo;
    LL_L2UI(lo, n);
    LL_SHR(tmp, n, 32);
    LL_L2UI(hi, tmp);
    hi = PR_ntohl(hi);
    lo = PR_ntohl(lo);
    LL_UI2L(n, lo);
    LL_SHL(n, n, 32);
    LL_UI2L(tmp, hi);
    LL_ADD(n, n, tmp);
    return n;
#endif
}  /* ntohll */

PR_IMPLEMENT(PRUint64) PR_htonll(PRUint64 n)
{
#ifdef IS_BIG_ENDIAN
    return n;
#else
    PRUint64 tmp;
    PRUint32 hi, lo;
    LL_L2UI(lo, n);
    LL_SHR(tmp, n, 32);
    LL_L2UI(hi, tmp);
    hi = htonl(hi);
    lo = htonl(lo);
    LL_UI2L(n, lo);
    LL_SHL(n, n, 32);
    LL_UI2L(tmp, hi);
    LL_ADD(n, n, tmp);
    return n;
#endif
}  /* htonll */

PR_IMPLEMENT(PRAddrInfo *) PR_GetAddrInfoByName(const char  *hostname,
                                                PRUint16     af,
                                                PRIntn       flags)
{
    PR_ASSERT("PR_AddrInfoByName called!");
    return NULL;
}  /* PR_AddrInfoByName */

PR_IMPLEMENT(void) PR_FreeAddrInfo(PRAddrInfo *ai)
{
    PR_ASSERT("PR_FreeAddrInfo called!");
}  /* PR_FreeAddrInfo */

PR_IMPLEMENT(void *) PR_EnumerateAddrInfo(void             *iterPtr,
                                          const PRAddrInfo *base,
                                          PRUint16          port,
                                          PRNetAddr        *result)
{
    PR_ASSERT("PR_EnumerateAddrInfo called!");
    return NULL;
}  /* PR_EnumerateAddrInfo */

PR_IMPLEMENT(const char *) PR_GetCanonNameFromAddrInfo(const PRAddrInfo *ai)
{
    PR_ASSERT("PR_GetCanonNameFromAddrInfo called!");
    return NULL;
}  /* PR_GetCanonNameFromAddrInfo */
