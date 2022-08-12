/* $Id$ */
/** @file
 * Recording internals header.
 */

/*
 * Copyright (C) 2012-2022 Oracle Corporation
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

#include "VBox/com/VirtualBox.h"
#include "VBox/settings.h"
#include <VBox/vmm/pdmaudioifs.h>

#ifdef VBOX_WITH_LIBVPX
# define VPX_CODEC_DISABLE_COMPAT 1
# include "vpx/vp8cx.h"
# include "vpx/vpx_image.h"
# include "vpx/vpx_encoder.h"
#endif /* VBOX_WITH_LIBVPX */

#ifdef VBOX_WITH_LIBOPUS
# include <opus.h>
#endif

#ifdef VBOX_WITH_LIBVORBIS
# include "vorbis/vorbisenc.h"
#endif


/*********************************************************************************************************************************
*   Defines                                                                                                                      *
*********************************************************************************************************************************/
#define VBOX_RECORDING_OPUS_HZ_MAX               48000   /**< Maximum sample rate (in Hz) Opus can handle. */
#define VBOX_RECORDING_OPUS_FRAME_MS_DEFAULT     20      /**< Default Opus frame size (in ms). */

#define VBOX_RECORDING_VORBIS_HZ_MAX             48000   /**< Maximum sample rate (in Hz) Vorbis can handle. */
#define VBOX_RECORDING_VORBIS_FRAME_MS_DEFAULT   20      /**< Default Vorbis frame size (in ms). */


/*********************************************************************************************************************************
*   Prototypes                                                                                                                   *
*********************************************************************************************************************************/
struct RECORDINGCODEC;
typedef RECORDINGCODEC *PRECORDINGCODEC;

struct RECORDINGFRAME;
typedef RECORDINGFRAME *PRECORDINGFRAME;


/*********************************************************************************************************************************
*   Internal structures, defines and APIs                                                                                        *
*********************************************************************************************************************************/

/**
 * Enumeration for specifying a (generic) codec type.
 */
typedef enum RECORDINGCODECTYPE
{
    /** Invalid codec type. Do not use. */
    RECORDINGCODECTYPE_INVALID = 0,
    /** Video codec. */
    RECORDINGCODECTYPE_VIDEO,
    /** Audio codec. */
    RECORDINGCODECTYPE_AUDIO
} RECORDINGCODECTYPE;

/**
 * Structure for keeping a codec operations table.
 */
typedef struct RECORDINGCODECOPS
{
    /**
     * Initializes a codec.
     *
     * @returns VBox status code.
     * @param   pCodec              Codec instance to initialize.
     */
    DECLCALLBACKMEMBER(int, pfnInit,         (PRECORDINGCODEC pCodec));

    /**
     * Destroys a codec.
     *
     * @returns VBox status code.
     * @param   pCodec              Codec instance to destroy.
     */
    DECLCALLBACKMEMBER(int, pfnDestroy,      (PRECORDINGCODEC pCodec));

    /**
     * Parses an options string to configure advanced / hidden / experimental features of a recording stream.
     * Unknown values will be skipped. Optional.
     *
     * @returns VBox status code.
     * @param   pCodec              Codec instance to parse options for.
     * @param   strOptions          Options string to parse.
     */
    DECLCALLBACKMEMBER(int, pfnParseOptions, (PRECORDINGCODEC pCodec, const Utf8Str &strOptions));

    /**
     * Feeds the codec encoder with data to encode.
     *
     * @returns VBox status code.
     * @param   pCodec              Codec instance to use.
     * @param   pFrame              Pointer to frame data to encode.
     * @param   pvDst               Where to store the encoded data on success.
     * @param   cbDst               Size (in bytes) of \a pvDst.
     * @param   pcEncoded           Where to return the number of encoded blocks in \a pvDst on success. Optional.
     * @param   pcbEncoded          Where to return the number of encoded bytes in \a pvDst on success. Optional.
     */
    DECLCALLBACKMEMBER(int, pfnEncode,       (PRECORDINGCODEC pCodec, const PRECORDINGFRAME pFrame, void *pvDst, size_t cbDst, size_t *pcEncoded, size_t *pcbEncoded));

    /**
     * Tells the codec to finalize the current stream. Optional.
     *
     * @returns VBox status code.
     * @param   pCodec              Codec instance to finalize stream for.
     */
    DECLCALLBACKMEMBER(int, pfnFinalize,     (PRECORDINGCODEC pCodec));
} RECORDINGCODECOPS, *PRECORDINGCODECOPS;

/**
 * Structure for keeping a codec callback table.
 */
typedef struct RECORDINGCODECCALLBACKS
{
    DECLCALLBACKMEMBER(int, pfnWriteData, (PRECORDINGCODEC pCodec, const void *pvData, size_t cbData, void *pvUser));
    /** User-supplied data pointer. */
    void                       *pvUser;
} RECORDINGCODECCALLBACKS, *PRECORDINGCODECCALLBACKS;

/**
 * Structure for keeping generic codec parameters.
 */
typedef struct RECORDINGCODECPARMS
{
    /** The generic codec type. */
    RECORDINGCODECTYPE          enmType;
    /** The specific codec type, based on \a enmType. */
    union
    {
        /** The container's video codec to use. */
        RecordingVideoCodec_T   enmVideoCodec;
        /** The container's audio codec to use. */
        RecordingAudioCodec_T   enmAudioCodec;
    };
    union
    {
        struct
        {
            /** Frames per second. */
            uint8_t             uFPS;
            /** Target width (in pixels) of encoded video image. */
            uint16_t            uWidth;
            /** Target height (in pixels) of encoded video image. */
            uint16_t            uHeight;
            /** Minimal delay (in ms) between two video frames.
             *  This value is based on the configured FPS rate. */
            uint32_t            uDelayMs;
        } Video;
        struct
        {
            /** The codec's used PCM properties. */
            PDMAUDIOPCMPROPS    PCMProps;
        } Audio;
    };
    /** Desired (average) bitrate (in kbps) to use, for codecs which support bitrate management.
     *  Set to 0 to use a variable bit rate (VBR) (if available, otherwise fall back to CBR). */
    uint32_t                    uBitrate;
    /** Time (in ms) an (encoded) frame takes.
     *
     *  For Opus, valid frame sizes are:
     *  ms           Frame size
     *  2.5          120
     *  5            240
     *  10           480
     *  20 (Default) 960
     *  40           1920
     *  60           2880
     */
    uint32_t                    msFrame;
    /** The frame size in bytes (based on msFrame). */
    uint32_t                    cbFrame;
    /** The frame size in samples per frame (based on msFrame). */
    uint32_t                    csFrame;
} RECORDINGCODECPARMS, *PRECORDINGCODECPARMS;

#ifdef VBOX_WITH_LIBVPX
/**
 * VPX encoder state (needs libvpx).
 */
typedef struct RECORDINGCODECVPX
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
} RECORDINGCODECVPX;
/** Pointer to a VPX encoder state. */
typedef RECORDINGCODECVPX *PRECORDINGCODECVPX;
#endif /* VBOX_WITH_LIBVPX */

#ifdef VBOX_WITH_LIBOPUS
/**
 * Opus encoder state (needs libvorbis).
 */
typedef struct RECORDINGCODECOPUS
{
    /** Encoder we're going to use. */
    OpusEncoder    *pEnc;
} RECORDINGCODECOPUS;
/** Pointer to an Opus encoder state. */
typedef RECORDINGCODECOPUS *PRECORDINGCODECOPUS;
#endif /* VBOX_WITH_LIBOPUS */

#ifdef VBOX_WITH_LIBVORBIS
/**
 * Vorbis encoder state (needs libvorbis + libogg).
 */
typedef struct RECORDINGCODECVORBIS
{
    /** Basic information about the audio in a Vorbis bitstream. */
    vorbis_info      info;
    /** Encoder state. */
    vorbis_dsp_state dsp_state;
    /** Current block being worked on. */
    vorbis_block     block_cur;
} RECORDINGCODECVORBIS;
/** Pointer to a Vorbis encoder state. */
typedef RECORDINGCODECVORBIS *PRECORDINGCODECVORBIS;
#endif /* VBOX_WITH_LIBVORBIS */

/**
 * Structure for keeping codec-specific data.
 */
typedef struct RECORDINGCODEC
{
    /** Callback table for codec operations. */
    RECORDINGCODECOPS           Ops;
    /** Table for user-supplied callbacks. */
    RECORDINGCODECCALLBACKS     Callbacks;
    /** Generic codec parameters. */
    RECORDINGCODECPARMS         Parms;

#ifdef VBOX_WITH_LIBVPX
    union
    {
        RECORDINGCODECVPX       VPX;
    } Video;
#endif

#ifdef VBOX_WITH_AUDIO_RECORDING
    union
    {
# ifdef VBOX_WITH_LIBOPUS
        RECORDINGCODECOPUS      Opus;
# endif /* VBOX_WITH_LIBOPUS */
# ifdef VBOX_WITH_LIBVORBIS
        RECORDINGCODECVORBIS    Vorbis;
# endif /* VBOX_WITH_LIBVORBIS */
    } Audio;
#endif /* VBOX_WITH_AUDIO_RECORDING */

    /** Timestamp (in ms) of the last frame was encoded. */
    uint64_t            uLastTimeStampMs;

#ifdef VBOX_WITH_STATISTICS /** @todo Register these values with STAM. */
    struct
    {
        /** Number of encoding errors. */
        uint64_t        cEncErrors;
        /** Number of frames encoded. */
        uint64_t        cEncBlocks;
        /** Total time (in ms) of already encoded audio data. */
        uint64_t        msEncTotal;
    } Stats;
#endif
} RECORDINGCODEC, *PRECORDINGCODEC;

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
#endif /* VBOX_WITH_AUDIO_RECORDING */

/**
 * Structure for keeping a single recording audio frame.
 */
typedef struct RECORDINGFRAME
{
    uint64_t                msTimestamp;
    union
    {
#ifdef VBOX_WITH_AUDIO_RECORDING
        RECORDINGAUDIOFRAME  Audio;
#endif
        RECORDINGVIDEOFRAME  Video;
        RECORDINGVIDEOFRAME *VideoPtr;
    };
} RECORDINGFRAME, *PRECORDINGFRAME;

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

#ifdef VBOX_WITH_AUDIO_RECORDING
int recordingCodecCreateAudio(PRECORDINGCODEC pCodec, RecordingAudioCodec_T enmAudioCodec);
#endif
int recordingCodecCreateVideo(PRECORDINGCODEC pCodec, RecordingVideoCodec_T enmVideoCodec);
int recordingCodecInit(const PRECORDINGCODEC pCodec, const PRECORDINGCODECCALLBACKS pCallbacks, const settings::RecordingScreenSettings &Settings);
int recordingCodecDestroy(PRECORDINGCODEC pCodec);
int recordingCodecEncode(PRECORDINGCODEC pCodec, const PRECORDINGFRAME pFrame, void *pvDst, size_t cbDst, size_t *pcEncoded, size_t *pcbEncoded);
int recordingCodecFinalize(PRECORDINGCODEC pCodec);
#endif /* !MAIN_INCLUDED_RecordingInternals_h */

