/* $Id$ */
/** @file
 * Recording internals header.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_RecordingInternals_h
#define MAIN_INCLUDED_RecordingInternals_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <list>

#include <iprt/assert.h>
#include <iprt/types.h> /* drag in stdint.h before vpx does it. */

#ifdef VBOX_WITH_LIBVPX
# define VPX_CODEC_DISABLE_COMPAT 1
# include "vpx/vp8cx.h"
# include "vpx/vpx_image.h"
# include "vpx/vpx_encoder.h"
#endif /* VBOX_WITH_LIBVPX */

/**
 * Structure for keeping specific recording video codec data.
 */
typedef struct RECORDINGVIDEOCODEC
{
#ifdef VBOX_WITH_LIBVPX
    union
    {
        struct
        {
            /** VPX codec context. */
            vpx_codec_ctx_t     Ctx;
            /** VPX codec configuration. */
            vpx_codec_enc_cfg_t Cfg;
            /** VPX image context. */
            vpx_image_t         RawImage;
            /** Pointer to the codec's internal YUV buffer. */
            uint8_t            *pu8YuvBuf;
            /** The encoder's deadline (in ms).
             *  The more time the encoder is allowed to spend encoding, the better the encoded
             *  result, in exchange for higher CPU usage and time spent encoding. */
            unsigned int        uEncoderDeadline;
        } VPX;
    };
#endif /* VBOX_WITH_LIBVPX */
} RECORDINGVIDEOCODEC, *PRECORDINGVIDEOCODEC;

/**
 * Enumeration for supported pixel formats.
 */
enum RECORDINGPIXELFMT
{
    /** Unknown pixel format. */
    RECORDINGPIXELFMT_UNKNOWN    = 0,
    /** RGB 24. */
    RECORDINGPIXELFMT_RGB24      = 1,
    /** RGB 24. */
    RECORDINGPIXELFMT_RGB32      = 2,
    /** RGB 565. */
    RECORDINGPIXELFMT_RGB565     = 3,
    /** The usual 32-bit hack. */
    RECORDINGPIXELFMT_32BIT_HACK = 0x7fffffff
};

/**
 * Structure for keeping a single recording video frame.
 */
typedef struct RECORDINGVIDEOFRAME
{
    /** X resolution of this frame. */
    uint32_t            uWidth;
    /** Y resolution of this frame. */
    uint32_t            uHeight;
    /** Pixel format of this frame. */
    uint32_t            uPixelFormat;
    /** RGB buffer containing the unmodified frame buffer data from Main's display. */
    uint8_t            *pu8RGBBuf;
    /** Size (in bytes) of the RGB buffer. */
    size_t              cbRGBBuf;
} RECORDINGVIDEOFRAME, *PRECORDINGVIDEOFRAME;

#ifdef VBOX_WITH_AUDIO_RECORDING
/**
 * Structure for keeping a single recording audio frame.
 */
typedef struct RECORDINGAUDIOFRAME
{
    /** Pointer to audio data. */
    uint8_t            *pvBuf;
    /** Size (in bytes) of audio data. */
    size_t              cbBuf;
} RECORDINGAUDIOFRAME, *PRECORDINGAUDIOFRAME;
#endif

/**
 * Enumeration for specifying a video recording block type.
 */
typedef enum RECORDINGBLOCKTYPE
{
    /** Uknown block type, do not use. */
    RECORDINGBLOCKTYPE_UNKNOWN = 0,
    /** The block is a video frame. */
    RECORDINGBLOCKTYPE_VIDEO,
#ifdef VBOX_WITH_AUDIO_RECORDING
    /** The block is an audio frame. */
    RECORDINGBLOCKTYPE_AUDIO
#endif
} RECORDINGBLOCKTYPE;

#ifdef VBOX_WITH_AUDIO_RECORDING
void RecordingAudioFrameFree(PRECORDINGAUDIOFRAME pFrame);
#endif
void RecordingVideoFrameFree(PRECORDINGVIDEOFRAME pFrame);

/**
 * Generic structure for keeping a single video recording (data) block.
 */
struct RecordingBlock
{
    RecordingBlock()
        : enmType(RECORDINGBLOCKTYPE_UNKNOWN)
        , cRefs(0)
        , pvData(NULL)
        , cbData(0) { }

    virtual ~RecordingBlock()
    {
        Reset();
    }

    void Reset(void)
    {
        switch (enmType)
        {
            case RECORDINGBLOCKTYPE_UNKNOWN:
                break;

            case RECORDINGBLOCKTYPE_VIDEO:
                RecordingVideoFrameFree((PRECORDINGVIDEOFRAME)pvData);
                break;

#ifdef VBOX_WITH_AUDIO_RECORDING
            case RECORDINGBLOCKTYPE_AUDIO:
                RecordingAudioFrameFree((PRECORDINGAUDIOFRAME)pvData);
                break;
#endif
            default:
                AssertFailed();
                break;
        }

        enmType = RECORDINGBLOCKTYPE_UNKNOWN;
        cRefs   = 0;
        pvData  = NULL;
        cbData  = 0;
    }

    /** The block's type. */
    RECORDINGBLOCKTYPE enmType;
    /** Number of references held of this block. */
    uint16_t           cRefs;
    /** The (absolute) timestamp (in ms, PTS) of this block. */
    uint64_t           msTimestamp;
    /** Opaque data block to the actual block data, depending on the block's type. */
    void              *pvData;
    /** Size (in bytes) of the (opaque) data block. */
    size_t             cbData;
};

/** List for keeping video recording (data) blocks. */
typedef std::list<RecordingBlock *> RecordingBlockList;

#endif /* !MAIN_INCLUDED_RecordingInternals_h */

