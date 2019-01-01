/* $Id$ */
/** @file
 * Recording internals code.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "RecordingInternals.h"

#include <iprt/assert.h>
#include <iprt/mem.h>

#ifdef VBOX_WITH_AUDIO_RECORDING
/**
 * Frees a previously allocated recording audio frame.
 *
 * @param   pFrame              Audio frame to free. The pointer will be invalid after return.
 */
void RecordingAudioFrameFree(PRECORDINGAUDIOFRAME pFrame)
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
 * Frees a recording video frame.
 *
 * @returns IPRT status code.
 * @param   pFrame              Pointer to video frame to free. The pointer will be invalid after return.
 */
void RecordingVideoFrameFree(PRECORDINGVIDEOFRAME pFrame)
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

