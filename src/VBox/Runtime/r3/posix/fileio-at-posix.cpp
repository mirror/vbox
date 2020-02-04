/* $Id$ */
/** @file
 * IPRT - File I/O, RTFileReadAt and RTFileWriteAt, posix.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include "internal/iprt.h"
#include <iprt/file.h>

#include <iprt/err.h>
#include <iprt/log.h>



RTDECL(int)  RTFileReadAt(RTFILE hFile, RTFOFF off, void *pvBuf, size_t cbToRead, size_t *pcbRead)
{
    ssize_t cbRead = pread(RTFileToNative(hFile), pvBuf, cbToRead, off);
    if (cbRead >= 0)
    {
        if (pcbRead)
            /* caller can handle partial read. */
            *pcbRead = cbRead;
        else
        {
            /* Caller expects all to be read. */
            while ((ssize_t)cbToRead > cbRead)
            {
                ssize_t cbReadPart = pread(RTFileToNative(hFile), (char*)pvBuf + cbRead, cbToRead - cbRead, off + cbRead);
                if (cbReadPart <= 0)
                {
                    if (cbReadPart == 0)
                        return VERR_EOF;
                    return RTErrConvertFromErrno(errno);
                }
                cbRead += cbReadPart;
            }
        }
        return VINF_SUCCESS;
    }

    return RTErrConvertFromErrno(errno);
}


RTDECL(int)  RTFileWriteAt(RTFILE hFile, RTFOFF off, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten)
{
    ssize_t cbWritten = pwrite(RTFileToNative(hFile), pvBuf, cbToWrite, off);
    if (cbWritten >= 0)
    {
        if (pcbWritten)
            /* caller can handle partial write. */
            *pcbWritten = cbWritten;
        else
        {
            /* Caller expects all to be write. */
            while ((ssize_t)cbToWrite > cbWritten)
            {
                ssize_t cbWrittenPart = pwrite(RTFileToNative(hFile), (const char *)pvBuf + cbWritten, cbToWrite - cbWritten,
                                               off + cbWritten);
                if (cbWrittenPart < 0)
                    return cbWrittenPart < 0 ? RTErrConvertFromErrno(errno) : VERR_TRY_AGAIN;
                cbWritten += cbWrittenPart;
            }
        }
        return VINF_SUCCESS;
    }
    return RTErrConvertFromErrno(errno);
}

