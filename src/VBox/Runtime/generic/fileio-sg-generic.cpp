/* $Id$ */
/** @file
 * IPRT - File I/O, RTFileSgRead & RTFileSgWrite, generic.
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
#include "internal/iprt.h"
#include <iprt/file.h>

#include <iprt/assert.h>
#include <iprt/err.h>


RTDECL(int)  RTFileSgRead(RTFILE hFile, PRTSGBUF pSgBuf, size_t cbToRead, size_t *pcbRead)
{
    int rc = VINF_SUCCESS;
    size_t cbRead = 0;
    while (cbToRead)
    {
        size_t cbBuf = cbToRead;
        void  *pvBuf = RTSgBufGetNextSegment(pSgBuf, &cbBuf); /** @todo this is wrong as it may advance the buffer past what's actually read. */

        size_t cbThisRead = cbBuf;
        rc = RTFileRead(hFile, pvBuf, cbBuf, pcbRead ? &cbThisRead : NULL);
        if (RT_SUCCESS(rc))
            cbRead += cbThisRead;
        else
            break;
        if (cbThisRead < cbBuf)
        {
            AssertStmt(pcbRead, rc = VERR_INTERNAL_ERROR_2);
            break;
        }
        Assert(cbBuf == cbThisRead);

        cbToRead -= cbBuf;
    }

    if (pcbRead)
        *pcbRead = cbRead;

    return rc;
}


RTDECL(int)  RTFileSgWrite(RTFILE hFile, PRTSGBUF pSgBuf, size_t cbToWrite, size_t *pcbWritten)
{
    int rc = VINF_SUCCESS;
    size_t cbWritten = 0;
    while (cbToWrite)
    {
        size_t cbBuf = cbToWrite;
        void  *pvBuf = RTSgBufGetNextSegment(pSgBuf, &cbBuf); /** @todo this is wrong as it may advance the buffer past what's actually read. */

        size_t cbThisWritten = cbBuf;
        rc = RTFileWrite(hFile, pvBuf, cbBuf, pcbWritten ? &cbThisWritten : NULL);
        if (RT_SUCCESS(rc))
            cbWritten += cbThisWritten;
        else
            break;
        if (cbThisWritten < cbBuf)
        {
            AssertStmt(pcbWritten, rc = VERR_INTERNAL_ERROR_2);
            break;
        }

        Assert(cbBuf == cbThisWritten);
        cbToWrite -= cbBuf;
    }

    if (pcbWritten)
        *pcbWritten = cbWritten;

    return rc;
}

