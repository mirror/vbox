/* $Id$ */
/** @file
 * Virtual SCSI driver: S/G list handling
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */
#define LOG_GROUP LOG_GROUP_VSCSI
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/string.h>

#include "VSCSIInternal.h"


void vscsiIoMemCtxInit(PVSCSIIOMEMCTX pIoMemCtx, PCPDMDATASEG paDataSeg, size_t cSegments)
{
    if (RT_UNLIKELY(!cSegments))
    {
        pIoMemCtx->paDataSeg = NULL;
        pIoMemCtx->cSegments = 0;
        pIoMemCtx->iSegIdx   = 0;
        pIoMemCtx->pbBuf     = NULL;
        pIoMemCtx->cbBufLeft = 0;
    }
    else
    {
        pIoMemCtx->paDataSeg = paDataSeg;
        pIoMemCtx->cSegments = cSegments;
        pIoMemCtx->iSegIdx   = 0;
        pIoMemCtx->pbBuf     = (uint8_t *)paDataSeg[0].pvSeg;
        pIoMemCtx->cbBufLeft = paDataSeg[0].cbSeg;
    }
}


uint8_t *vscsiIoMemCtxGetBuffer(PVSCSIIOMEMCTX pIoMemCtx, size_t *pcbData)
{
    size_t cbData = RT_MIN(*pcbData, pIoMemCtx->cbBufLeft);
    uint8_t *pbBuf = pIoMemCtx->pbBuf;

    pIoMemCtx->cbBufLeft -= cbData;

    /* Advance to the next segment if required. */
    if (!pIoMemCtx->cbBufLeft)
    {
        pIoMemCtx->iSegIdx++;

        if (RT_UNLIKELY(pIoMemCtx->iSegIdx == pIoMemCtx->cSegments))
        {
            pIoMemCtx->cbBufLeft = 0;
            pIoMemCtx->pbBuf     = NULL;
        }
        else
        {
            pIoMemCtx->pbBuf     = (uint8_t *)pIoMemCtx->paDataSeg[pIoMemCtx->iSegIdx].pvSeg;
            pIoMemCtx->cbBufLeft = pIoMemCtx->paDataSeg[pIoMemCtx->iSegIdx].cbSeg;
        }

        *pcbData = cbData;
    }
    else
        pIoMemCtx->pbBuf += cbData;

    return pbBuf;
}


size_t vscsiCopyToIoMemCtx(PVSCSIIOMEMCTX pIoMemCtx, uint8_t *pbData, size_t cbData)
{
    size_t cbLeft = cbData;

    while (cbLeft)
    {
        size_t cbCopy = cbLeft;
        uint8_t *pbBuf = vscsiIoMemCtxGetBuffer(pIoMemCtx, &cbCopy);

        if (!cbCopy)
            break;

        memcpy(pbBuf, pbData, cbCopy);

        cbLeft -= cbCopy;
        pbData += cbCopy;
    }

    return cbData - cbLeft;
}


size_t vscsiCopyFromIoMemCtx(PVSCSIIOMEMCTX pIoMemCtx, uint8_t *pbData, size_t cbData)
{
    size_t cbLeft = cbData;

    while (cbLeft)
    {
        size_t cbCopy = cbData;
        uint8_t *pbBuf = vscsiIoMemCtxGetBuffer(pIoMemCtx, &cbCopy);

        if (!cbCopy)
            break;

        memcpy(pbData, pbBuf, cbCopy);

        cbData -= cbCopy;
        pbData += cbCopy;
    }

    return cbData - cbLeft;
}


