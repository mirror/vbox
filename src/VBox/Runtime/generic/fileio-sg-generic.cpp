/* $Id$ */
/** @file
 * IPRT - File I/O, RTFileSgReadAt & RTFileSgWriteAt, generic.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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

#include <iprt/errcore.h>


/**
 * Read bytes from a file at a given offset into a S/G buffer.
 * This function may modify the file position.
 *
 * @returns iprt status code.
 * @param   hFile       Handle to the file.
 * @param   off         Where to read.
 * @param   pSgBuf      Pointer to the S/G buffer to read into.
 * @param   cbToRead    How much to read.
 * @param   pcbRead     How much we actually read.
 *                      If NULL an error will be returned for a partial read.
 */
RTDECL(int)  RTFileSgReadAt(RTFILE hFile, RTFOFF off, PRTSGBUF pSgBuf, size_t cbToRead, size_t *pcbRead)
{
    int rc = VINF_SUCCESS;
    size_t cbRead = 0;
    while (cbToRead)
    {
        size_t cbBuf = cbToRead;
        void  *pvBuf = RTSgBufGetNextSegment(pSgBuf, &cbBuf);

        size_t cbThisRead = 0;
        rc = RTFileReadAt(hFile, off, pvBuf, cbBuf, pcbRead ? &cbThisRead : NULL);
        if (RT_SUCCESS(rc))
            cbRead += cbThisRead;
        else
            break;
        if (   cbThisRead < cbBuf
            && pcbRead)
            break;

        cbToRead -= cbBuf;
        off += cbBuf;
    }

    if (pcbRead)
        *pcbRead = cbRead;

    return rc;
}


/**
 * Write bytes from a S/G buffer to a file at a given offset.
 * This function may modify the file position.
 *
 * @returns iprt status code.
 * @param   hFile       Handle to the file.
 * @param   off         Where to write.
 * @param   pSgBuf      What to write.
 * @param   cbToWrite   How much to write.
 * @param   pcbWritten  How much we actually wrote.
 *                      If NULL an error will be returned for a partial write.
 */
RTDECL(int)  RTFileSgWriteAt(RTFILE hFile, RTFOFF off, PRTSGBUF pSgBuf, size_t cbToWrite, size_t *pcbWritten)
{
    int rc = VINF_SUCCESS;
    size_t cbWritten = 0;
    while (cbToWrite)
    {
        size_t cbBuf = cbToWrite;
        void  *pvBuf = RTSgBufGetNextSegment(pSgBuf, &cbBuf);

        size_t cbThisWritten = 0;
        rc = RTFileWriteAt(hFile, off, pvBuf, cbBuf, pcbWritten ? &cbThisWritten : NULL);
        if (RT_SUCCESS(rc))
            cbWritten += cbThisWritten;
        else
            break;
        if (   cbThisWritten < cbBuf
            && pcbWritten)
            break;

        cbToWrite -= cbBuf;
        off += cbBuf;
    }

    if (pcbWritten)
        *pcbWritten = cbWritten;

    return rc;
}

