/* $Id$ */
/** @file
 * Video recording internals code.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VideoRecInternals.h"

#include <iprt/assert.h>
#include <iprt/mem.h>

#ifdef VBOX_WITH_AUDIO_VIDEOREC
/**
 * Frees a previously allocated video recording audio frame.
 *
 * @param   pFrame              Audio frame to free. The pointer will be invalid after return.
 */
void VideoRecAudioFrameFree(PVIDEORECAUDIOFRAME pFrame)
{
    if (!pFrame)
        return;

    if (pFrame->pvBuf)
    {
        Assert(pFrame->cbBuf);
        RTMemFree(pFrame->pvBuf);
    }
    RTMemFree(pFrame);
    pFrame = NULL;
}
#endif

/**
 * Frees a video recording video frame.
 *
 * @returns IPRT status code.
 * @param   pFrame              Pointer to video frame to free. The pointer will be invalid after return.
 */
void VideoRecVideoFrameFree(PVIDEORECVIDEOFRAME pFrame)
{
    if (!pFrame)
        return;

    if (pFrame->pu8RGBBuf)
    {
        Assert(pFrame->cbRGBBuf);
        RTMemFree(pFrame->pu8RGBBuf);
    }
    RTMemFree(pFrame);
}

/**
 * Frees a video recording (data) block.
 *
 * @returns IPRT status code.
 * @param   pBlock              Video recording (data) block to free. The pointer will be invalid after return.
 */
void VideoRecBlockFree(PVIDEORECBLOCK pBlock)
{
    if (!pBlock)
        return;

    switch (pBlock->enmType)
    {
        case VIDEORECBLOCKTYPE_VIDEO:
            VideoRecVideoFrameFree((PVIDEORECVIDEOFRAME)pBlock->pvData);
            break;

#ifdef VBOX_WITH_AUDIO_VIDEOREC
        case VIDEORECBLOCKTYPE_AUDIO:
            VideoRecAudioFrameFree((PVIDEORECAUDIOFRAME)pBlock->pvData);
            break;
#endif
        default:
            AssertFailed();
            break;
    }

    RTMemFree(pBlock);
    pBlock = NULL;
}

