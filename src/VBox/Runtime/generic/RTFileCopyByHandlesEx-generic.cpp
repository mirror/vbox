/* $Id$ */
/** @file
 * IPRT - RTFileCopyByHandlesEx, generic implementation.
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

#include <iprt/alloca.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/errcore.h>


RTDECL(int) RTFileCopyByHandlesEx(RTFILE FileSrc, RTFILE FileDst, PFNRTPROGRESS pfnProgress, void *pvUser)
{
    /*
     * Validate input.
     */
    AssertMsgReturn(RTFileIsValid(FileSrc), ("FileSrc=%RTfile\n", FileSrc), VERR_INVALID_PARAMETER);
    AssertMsgReturn(RTFileIsValid(FileDst), ("FileDst=%RTfile\n", FileDst), VERR_INVALID_PARAMETER);
    AssertMsgReturn(!pfnProgress || VALID_PTR(pfnProgress), ("pfnProgress=%p\n", pfnProgress), VERR_INVALID_PARAMETER);

    /*
     * Save file offset.
     */
    RTFOFF offSrcSaved;
    int rc = RTFileSeek(FileSrc, 0, RTFILE_SEEK_CURRENT, (uint64_t *)&offSrcSaved);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Get the file size.
     */
    RTFOFF cbSrc;
    rc = RTFileSeek(FileSrc, 0, RTFILE_SEEK_END, (uint64_t *)&cbSrc);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Allocate buffer.
     */
    size_t      cbBuf;
    uint8_t    *pbBufFree = NULL;
    uint8_t    *pbBuf;
    if (cbSrc < _512K)
    {
        cbBuf = 8*_1K;
        pbBuf = (uint8_t *)alloca(cbBuf);
    }
    else
    {
        cbBuf = _128K;
        pbBuf = pbBufFree = (uint8_t *)RTMemTmpAlloc(cbBuf);
    }
    if (pbBuf)
    {
        /*
         * Seek to the start of each file
         * and set the size of the destination file.
         */
        rc = RTFileSeek(FileSrc, 0, RTFILE_SEEK_BEGIN, NULL);
        if (RT_SUCCESS(rc))
        {
            rc = RTFileSeek(FileDst, 0, RTFILE_SEEK_BEGIN, NULL);
            if (RT_SUCCESS(rc))
                rc = RTFileSetSize(FileDst, cbSrc);
            if (RT_SUCCESS(rc) && pfnProgress)
                rc = pfnProgress(0, pvUser);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Copy loop.
                 */
                unsigned    uPercentage = 0;
                RTFOFF      off = 0;
                RTFOFF      cbPercent = cbSrc / 100;
                RTFOFF      offNextPercent = cbPercent;
                while (off < cbSrc)
                {
                    /* copy block */
                    RTFOFF cbLeft = cbSrc - off;
                    size_t cbBlock = cbLeft >= (RTFOFF)cbBuf ? cbBuf : (size_t)cbLeft;
                    rc = RTFileRead(FileSrc, pbBuf, cbBlock, NULL);
                    if (RT_FAILURE(rc))
                        break;
                    rc = RTFileWrite(FileDst, pbBuf, cbBlock, NULL);
                    if (RT_FAILURE(rc))
                        break;

                    /* advance */
                    off += cbBlock;
                    if (pfnProgress && offNextPercent < off && uPercentage < 100)
                    {
                        do
                        {
                            uPercentage++;
                            offNextPercent += cbPercent;
                        } while (offNextPercent < off && uPercentage < 100);
                        rc = pfnProgress(uPercentage, pvUser);
                        if (RT_FAILURE(rc))
                            break;
                    }
                }

#if 0
                /*
                 * Copy OS specific data (EAs and stuff).
                 */
                rtFileCopyOSStuff(FileSrc, FileDst);
#endif

                /* 100% */
                if (pfnProgress && uPercentage < 100 && RT_SUCCESS(rc))
                    rc = pfnProgress(100, pvUser);
            }
        }
        RTMemTmpFree(pbBufFree);
    }
    else
        rc = VERR_NO_MEMORY;

    /*
     * Restore source position.
     */
    RTFileSeek(FileSrc, offSrcSaved, RTFILE_SEEK_BEGIN, NULL);

    return rc;
}

