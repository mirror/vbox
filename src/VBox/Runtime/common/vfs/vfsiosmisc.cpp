/* $Id$ */
/** @file
 * IPRT - Virtual File System, Misc I/O Stream Operations.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/vfs.h>
#include <iprt/vfslowlevel.h>

#include <iprt/err.h>
#include <iprt/string.h>



RTDECL(int) RTVfsIoStrmValidateUtf8Encoding(RTVFSIOSTREAM hVfsIos, uint32_t fFlags, PRTFOFF poffError)
{
    /*
     * Validate input.
     */
    if (poffError)
    {
        AssertPtrReturn(poffError, VINF_SUCCESS);
        *poffError = 0;
    }
    AssertReturn(!(fFlags & ~RTVFS_VALIDATE_UTF8_VALID_MASK), VERR_INVALID_PARAMETER);

    /*
     * The loop.
     */
    char    achBuf[1024 + 1];
    size_t  cbUsed = 0;
    int     rc;
    for (;;)
    {
        /*
         * Fill the buffer
         */
        size_t cbRead = 0;
        rc = RTVfsIoStrmRead(hVfsIos, &achBuf[cbUsed], sizeof(achBuf) - cbUsed - 1, true /*fBlocking*/, &cbRead);
        if (RT_FAILURE(rc))
            break;
        cbUsed += cbRead;
        if (!cbUsed)
        {
            Assert(rc == VINF_EOF);
            break;
        }
        achBuf[sizeof(achBuf) - 1] = '\0';

        /*
         * Process the data in the buffer, maybe leaving the final chars till
         * the next round.
         */
        const char *pszCur = achBuf;
        size_t      offEnd = rc == VINF_EOF
                           ? cbUsed
                           : cbUsed >= 7
                           ? cbUsed - 7
                           : 0;
        size_t      off;
        while ((off = (pszCur - &achBuf[0])) < offEnd)
        {
            RTUNICP uc;
            rc = RTStrGetCpEx(&pszCur, &uc);
            if (RT_FAILURE(rc))
                break;
            if (!uc)
            {
                if (fFlags & RTVFS_VALIDATE_UTF8_NO_NULL)
                {
                    rc = VERR_INVALID_UTF8_ENCODING;
                    break;
                }
            }
            else if (uc > 0x10ffff)
            {
                if (fFlags & RTVFS_VALIDATE_UTF8_BY_RTC_3629)
                {
                    rc = VERR_INVALID_UTF8_ENCODING;
                    break;
                }
            }
        }

        if (off < cbUsed)
        {
            cbUsed -= off;
            memmove(achBuf, pszCur, cbUsed);
        }
    }

    /*
     * Set the offset on failure.
     */
    if (poffError && RT_FAILURE(rc))
    {
    }

    return rc == VINF_EOF ? VINF_SUCCESS : rc;
}

