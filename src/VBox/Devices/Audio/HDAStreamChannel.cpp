/* $Id$ */
/** @file
 * HDAStreamChannel.cpp - Stream channel functions for HD Audio.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_HDA
#include <VBox/log.h>

#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmaudioifs.h>

#include "HDAStreamChannel.h"

int hdaStreamChannelDataInit(PPDMAUDIOSTREAMCHANNELDATA pChanData, uint32_t fFlags)
{
    int rc = RTCircBufCreate(&pChanData->pCircBuf, 256); /** @todo Make this configurable? */
    if (RT_SUCCESS(rc))
    {
        pChanData->fFlags = fFlags;
    }

    return rc;
}

/**
 * Frees a stream channel data block again.
 *
 * @param   pChanData           Pointer to channel data to free.
 */
void hdaStreamChannelDataDestroy(PPDMAUDIOSTREAMCHANNELDATA pChanData)
{
    if (!pChanData)
        return;

    if (pChanData->pCircBuf)
    {
        RTCircBufDestroy(pChanData->pCircBuf);
        pChanData->pCircBuf = NULL;
    }

    pChanData->fFlags = PDMAUDIOSTREAMCHANNELDATA_FLAG_NONE;
}

int hdaStreamChannelExtract(PPDMAUDIOSTREAMCHANNEL pChan, const void *pvBuf, size_t cbBuf)
{
    AssertPtrReturn(pChan, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf,    VERR_INVALID_PARAMETER);

    AssertRelease(pChan->cbOff <= cbBuf);

    const uint8_t *pu8Buf = (const uint8_t *)pvBuf;

    size_t         cbSrc = cbBuf - pChan->cbOff;
    const uint8_t *pvSrc = &pu8Buf[pChan->cbOff];

    size_t         cbDst;
    uint8_t       *pvDst;
    RTCircBufAcquireWriteBlock(pChan->Data.pCircBuf, cbBuf, (void **)&pvDst, &cbDst);

    cbSrc = RT_MIN(cbSrc, cbDst);

    while (cbSrc)
    {
        AssertBreak(cbDst >= cbSrc);

        /* Enough data for at least one next frame? */
        if (cbSrc < pChan->cbFrame)
            break;

        memcpy(pvDst, pvSrc, pChan->cbFrame);

        /* Advance to next channel frame in stream. */
        pvSrc        += pChan->cbStep;
        Assert(cbSrc >= pChan->cbStep);
        cbSrc        -= pChan->cbStep;

        /* Advance destination by one frame. */
        pvDst        += pChan->cbFrame;
        Assert(cbDst >= pChan->cbFrame);
        cbDst        -= pChan->cbFrame;

        /* Adjust offset. */
        pChan->cbOff += pChan->cbFrame;
    }

    RTCircBufReleaseWriteBlock(pChan->Data.pCircBuf, cbDst);

    return VINF_SUCCESS;
}

int hdaStreamChannelAdvance(PPDMAUDIOSTREAMCHANNEL pChan, size_t cbAdv)
{
    AssertPtrReturn(pChan, VERR_INVALID_POINTER);

    if (!cbAdv)
        return VINF_SUCCESS;

    return VINF_SUCCESS;
}

int hdaStreamChannelAcquireData(PPDMAUDIOSTREAMCHANNELDATA pChanData, void *pvData, size_t *pcbData)
{
    AssertPtrReturn(pChanData, VERR_INVALID_POINTER);
    AssertPtrReturn(pvData,    VERR_INVALID_POINTER);
    AssertPtrReturn(pcbData,   VERR_INVALID_POINTER);

    RTCircBufAcquireReadBlock(pChanData->pCircBuf, 256 /** @todo Make this configurarble? */, &pvData, &pChanData->cbAcq);

    *pcbData = pChanData->cbAcq;
    return VINF_SUCCESS;
}

int hdaStreamChannelReleaseData(PPDMAUDIOSTREAMCHANNELDATA pChanData)
{
    AssertPtrReturn(pChanData, VERR_INVALID_POINTER);
    RTCircBufReleaseReadBlock(pChanData->pCircBuf, pChanData->cbAcq);

    return VINF_SUCCESS;
}

