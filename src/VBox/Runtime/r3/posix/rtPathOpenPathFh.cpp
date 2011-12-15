/* $Id$ */
/** @file
 * IPRT - rtPathOpenFd.cpp
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "internal/iprt.h"
#include "internal/path.h"
#include <iprt/err.h>
#include <iprt/path.h>
#include <iprt/string.h>


DECLHIDDEN(int) rtPathOpenPathNoFollowFh(const char *pszPath, int *pFh, const char **ppszName)
{
#if defined(RT_OS_LINUX) || defined(RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
    const char *psz = pszPath;
    const char *pszName = pszPath;
    int fh = -1;

    AssertPtrReturn(pFh, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszPath, VERR_INVALID_PARAMETER);
    /* must be an absolute path */
    AssertReturn(*pszPath == '/', VERR_INVALID_PARAMETER);

    for (;; psz++)
    {
        switch (*psz)
        {
            /* handle separators. */
            case '/':
            {
                int fhNew;
                if (fh == -1)
                {
                    /* root directory */
                    fhNew = open("/", O_RDONLY | O_NOFOLLOW);
                }
                else
                {
                    /* subdirectory */
                    char szTmpPath[RTPATH_MAX + 1];
                    RTStrCopyEx(szTmpPath, sizeof(szTmpPath), pszName, psz - pszName);
                    fhNew = openat(fh, szTmpPath, O_RDONLY | O_NOFOLLOW);
                    close(fh);
                }
                if (fhNew < 0)
                    return RTErrConvertFromErrno(errno);
                fh = fhNew;
                pszName = psz + 1;
                break;
            }

            /*
             * The end. Complete the results.
             */
            case '\0':
            {
                if (ppszName)
                    *ppszName = *pszName != '\0' ? pszName : NULL;
                *pFh = fh;
                return VINF_SUCCESS;
            }
        }
    }

    /* will never get here */
    return 0;
#else
    return VERR_NOT_IMPLEMENTED;
#endif
}
