/* $Id$ */
/** @file
 * IPRT - IPRT String Formatter, ring-0 addresses.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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
#define LOG_GROUP RTLOGGROUP_STRING
#include "the-linux-kernel.h"
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 38)
# include <linux/capability.h>
# include <linux/security.h>
#endif
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/string.h>

#include "internal/string.h"


DECLHIDDEN(size_t) rtStrFormatKernelAddress(char *pszBuf, size_t cbBuf, RTR0INTPTR uPtr, signed int cchWidth,
                                            signed int cchPrecision, unsigned int fFlags)
{
#if !defined(DEBUG) && LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 38)
    bool fRestrict = true;
#if 0
    if (kptr_restrict > 1)
        fRestrict = true;
    else if (kptr_restrict == 1)
    {
        const struct cred *cred = current_cred();
        if (   !has_capability_noaudit(current, CAP_SYSLOG)
# if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
            || !uid_eq(cred->euid, cred->uid)
            || !gid_eq(cred->egid, cred->gid)
# endif
            )
            fRestrict = true;
    }
#endif

    if (fRestrict)
    {
        RT_NOREF(uPtr, cchWidth, cchPrecision);
# if R0_ARCH_BITS == 64
        static const char s_szObfuscated[] = "0xXXXXXXXXXXXXXXXX";
# else
        static const char s_szObfuscated[] = "0xXXXXXXXX";
# endif
        size_t      cbSrc  = sizeof(s_szObfuscated);
        const char *pszSrc = s_szObfuscated;
        if (!(fFlags & RTSTR_F_SPECIAL))
        {
            pszSrc += 2;
            cbSrc  -= 2;
        }
        if (cbSrc <= cbBuf)
        {
            memcpy(pszBuf, pszSrc, cbSrc);
            return cbSrc;
        }
        AssertFailed();
        memcpy(pszBuf, pszSrc, cbBuf);
        pszBuf[cbBuf - 1] = '\0';
        return cbBuf - 1;
    }
#endif  /* DEBUG && LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 38) */
    Assert(cbBuf >= 64);
    return RTStrFormatNumber(pszBuf, uPtr, 16, cchWidth, cchPrecision, fFlags);
}
