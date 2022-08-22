/* $Id$ */
/** @file
 * Recording internals code.
 */

/*
 * Copyright (C) 2012-2022 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
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

