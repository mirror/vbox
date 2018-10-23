/* $Id$ */
/** @file
 * Video recording internals header.
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

#ifndef ____H_VIDEOREC_INTERNALS
#define ____H_VIDEOREC_INTERNALS

#include <list>

#ifdef VBOX_WITH_LIBVPX
# define VPX_CODEC_DISABLE_COMPAT 1
# include "vpx/vp8cx.h"
# include "vpx/vpx_image.h"
# include "vpx/vpx_encoder.h"
#endif /* VBOX_WITH_LIBVPX */

/**
 * Enumeration for video recording destinations.
 */
typedef enum VIDEORECDEST
{
    /** Invalid destination, do not use. */
    VIDEORECDEST_INVALID = 0,
    /** Write to a file. */
    VIDEORECDEST_FILE    = 1
} VIDEORECDEST;

/**
 * Structure for keeping specific video recording codec data.
 */
typedef struct VIDEORECVIDEOCODEC
{
    union
    {
#ifdef VBOX_WITH_LIBVPX
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
        } VPX;
#endif /* VBOX_WITH_LIBVPX */
    };
} VIDEORECVIDEOCODEC, *PVIDEORECVIDEOCODEC;

/**
 * Enumeration for supported pixel formats.
 */
enum VIDEORECPIXELFMT
{
    /** Unknown pixel format. */
    VIDEORECPIXELFMT_UNKNOWN    = 0,
    /** RGB 24. */
    VIDEORECPIXELFMT_RGB24      = 1,
    /** RGB 24. */
    VIDEORECPIXELFMT_RGB32      = 2,
    /** RGB 565. */
    VIDEORECPIXELFMT_RGB565     = 3,
    /** The usual 32-bit hack. */
    VIDEORECPIXELFMT_32BIT_HACK = 0x7fffffff
};

/**
 * Structure for keeping a single video recording video frame.
 */
typedef struct VIDEORECVIDEOFRAME
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
} VIDEORECVIDEOFRAME, *PVIDEORECVIDEOFRAME;

#ifdef VBOX_WITH_AUDIO_VIDEOREC
/**
 * Structure for keeping a single video recording audio frame.
 */
typedef struct VIDEORECAUDIOFRAME
{
    /** Pointer to audio data. */
    uint8_t            *pvBuf;
    /** Size (in bytes) of audio data. */
    size_t              cbBuf;
} VIDEORECAUDIOFRAME, *PVIDEORECAUDIOFRAME;
#endif

/**
 * Enumeration for specifying a video recording block type.
 */
typedef enum VIDEORECBLOCKTYPE
{
    /** Uknown block type, do not use. */
    VIDEORECBLOCKTYPE_UNKNOWN = 0,
    /** The block is a video frame. */
    VIDEORECBLOCKTYPE_VIDEO,
#ifdef VBOX_WITH_AUDIO_VIDEOREC
    /** The block is an audio frame. */
    VIDEORECBLOCKTYPE_AUDIO
#endif
} VIDEORECBLOCKTYPE;

/**
 * Generic structure for keeping a single video recording (data) block.
 */
typedef struct VIDEORECBLOCK
{
    /** The block's type. */
    VIDEORECBLOCKTYPE  enmType;
    /** Number of references held of this block. */
    uint16_t           cRefs;
    /** The (absolute) time stamp (in ms, PTS) of this block. */
    uint64_t           uTimeStampMs;
    /** Opaque data block to the actual block data, depending on the block's type. */
    void              *pvData;
    /** Size (in bytes) of the (opaque) data block. */
    size_t             cbData;
} VIDEORECBLOCK, *PVIDEORECBLOCK;

/** List for keeping video recording (data) blocks. */
typedef std::list<PVIDEORECBLOCK> VideoRecBlockList;

void VideoRecBlockFree(PVIDEORECBLOCK pBlock);
#ifdef VBOX_WITH_AUDIO_VIDEOREC
void VideoRecAudioFrameFree(PVIDEORECAUDIOFRAME pFrame);
#endif
void VideoRecVideoFrameFree(PVIDEORECVIDEOFRAME pFrame);

#endif /* ____H_VIDEOREC_INTERNALS */
