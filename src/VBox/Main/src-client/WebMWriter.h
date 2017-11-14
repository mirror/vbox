/* $Id$ */
/** @file
 * WebMWriter.h - WebM container handling.
 */

/*
 * Copyright (C) 2013-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____WEBMWRITER
#define ____WEBMWRITER

#include "EBMLWriter.h"

#ifdef VBOX_WITH_LIBVPX
# ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4668) /* vpx_codec.h(64) : warning C4668: '__GNUC__' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif' */
#  include <vpx/vpx_encoder.h>
#  pragma warning(pop)
# else
#  include <vpx/vpx_encoder.h>
# endif
#endif /* VBOX_WITH_LIBVPX */

/** No flags specified. */
#define VBOX_WEBM_BLOCK_FLAG_NONE           0
/** Invisible block which can be skipped. */
#define VBOX_WEBM_BLOCK_FLAG_INVISIBLE      0x08
/** The block marks a key frame. */
#define VBOX_WEBM_BLOCK_FLAG_KEY_FRAME      0x80

/** The default timecode scale factor for WebM -- all timecodes in the segments are expressed in ms.
 *  This allows every cluster to have blocks with positive values up to 32.767 seconds. */
#define VBOX_WEBM_TIMECODESCALE_FACTOR_MS   1000000

/** Maximum time (in ms) a cluster can store. */
#define VBOX_WEBM_CLUSTER_MAX_LEN_MS        INT16_MAX

/** Maximum time a block can store.
 *  With signed 16-bit timecodes and a default timecode scale of 1ms per unit this makes 65536ms. */
#define VBOX_WEBM_BLOCK_MAX_LEN_MS          UINT16_MAX

#ifdef VBOX_WITH_LIBOPUS
# pragma pack(push)
# pragma pack(1)
    /** Opus codec private data within the MKV (WEBM) container.
     *  Taken from: https://wiki.xiph.org/MatroskaOpus */
    typedef struct WEBMOPUSPRIVDATA
    {
        WEBMOPUSPRIVDATA(uint32_t a_u32SampleRate, uint8_t a_u8Channels)
        {
            au64Head        = RT_MAKE_U64_FROM_U8('O', 'p', 'u', 's', 'H', 'e', 'a', 'd');
            u8Version       = 1;
            u8Channels      = a_u8Channels;
            u16PreSkip      = 0;
            u32SampleRate   = a_u32SampleRate;
            u16Gain         = 0;
            u8MappingFamily = 0;
        }

        uint64_t au64Head;          /**< Defaults to "OpusHead". */
        uint8_t  u8Version;         /**< Must be set to 1. */
        uint8_t  u8Channels;
        uint16_t u16PreSkip;
        /** Sample rate *before* encoding to Opus.
         *  Note: This rate has nothing to do with the playback rate later! */
        uint32_t u32SampleRate;
        uint16_t u16Gain;
        /** Must stay 0 -- otherwise a mapping table must be appended
         *  right after this header. */
        uint8_t  u8MappingFamily;
    } WEBMOPUSPRIVDATA, *PWEBMOPUSPRIVDATA;
    AssertCompileSize(WEBMOPUSPRIVDATA, 19);
# pragma pack(pop)
#endif /* VBOX_WITH_LIBOPUS */

using namespace com;

class WebMWriter : public EBMLWriter
{

public:

    /** Defines a WebM timecode. */
    typedef uint16_t WebMTimecode;

    /** Defines the WebM block flags data type. */
    typedef uint8_t  WebMBlockFlags;

    /**
     * Supported audio codecs.
     */
    enum AudioCodec
    {
        /** No audio codec specified. */
        AudioCodec_None = 0,
        /** Opus. */
        AudioCodec_Opus = 1
    };

    /**
     * Supported video codecs.
     */
    enum VideoCodec
    {
        /** No video codec specified. */
        VideoCodec_None = 0,
        /** VP8. */
        VideoCodec_VP8  = 1
    };

    /**
     * Track type enumeration.
     */
    enum WebMTrackType
    {
        /** Unknown / invalid type. */
        WebMTrackType_Invalid     = 0,
        /** Only writes audio. */
        WebMTrackType_Audio       = 1,
        /** Only writes video. */
        WebMTrackType_Video       = 2
    };

    struct WebMTrack;

    /**
     * Structure for defining a WebM simple block.
     */
    struct WebMSimpleBlock
    {
        WebMSimpleBlock(WebMTrack *a_pTrack,
                        WebMTimecode a_tcPTSMs, const void *a_pvData, size_t a_cbData, WebMBlockFlags a_fFlags)
            : pTrack(a_pTrack)
        {
            Data.tcPTSMs = a_tcPTSMs;
            Data.cb      = a_cbData;
            Data.fFlags  = a_fFlags;

            if (Data.cb)
            {
                Data.pv = RTMemDup(a_pvData, a_cbData);
                if (!Data.pv)
                    throw;
            }
        }

        virtual ~WebMSimpleBlock()
        {
            if (Data.pv)
            {
                Assert(Data.cb);
                RTMemFree(Data.pv);
            }
        }

        WebMTrack    *pTrack;

        /** Actual simple block data. */
        struct
        {
            WebMTimecode   tcPTSMs;
            WebMTimecode   tcRelToClusterMs;
            void          *pv;
            size_t         cb;
            WebMBlockFlags fFlags;
        } Data;
    };

    /** A simple block queue.*/
    typedef std::queue<WebMSimpleBlock *> WebMSimpleBlockQueue;

    /** Structure for queuing all simple blocks bound to a single timecode.
     *  This can happen if multiple tracks are being involved. */
    struct WebMTimecodeBlocks
    {
        WebMTimecodeBlocks(void)
            : fClusterNeeded(false)
            , fClusterStarted(false) { }

        /** The actual block queue for this timecode. */
        WebMSimpleBlockQueue Queue;
        /** Whether a new cluster is needed for this timecode or not. */
        bool                 fClusterNeeded;
        /** Whether a new cluster already has been started for this timecode or not. */
        bool                 fClusterStarted;

        /**
         * Enqueues a simple block into the internal queue.
         *
         * @param   a_pBlock    Block to enqueue and take ownership of.
         */
        void Enqueue(WebMSimpleBlock *a_pBlock)
        {
            Queue.push(a_pBlock);

            if (a_pBlock->Data.fFlags & VBOX_WEBM_BLOCK_FLAG_KEY_FRAME)
                fClusterNeeded = true;
        }
    };

    /** A block map containing all currently queued blocks.
     *  The key specifies a unique timecode, whereas the value
     *  is a queue of blocks which all correlate to the key (timecode). */
    typedef std::map<WebMTimecode, WebMTimecodeBlocks> WebMBlockMap;

    /**
     * Structure for defining a WebM (encoding) queue.
     */
    struct WebMQueue
    {
        WebMQueue(void)
            : tcLastBlockWrittenMs(0)
            , tslastProcessedMs(0) { }

        /** Blocks as FIFO (queue). */
        WebMBlockMap Map;
        /** Timecode (in ms) of last written block to queue. */
        WebMTimecode tcLastBlockWrittenMs;
        /** Time stamp (in ms) of when the queue was processed last. */
        uint64_t     tslastProcessedMs;
    };

    /**
     * Structure for keeping a WebM track entry.
     */
    struct WebMTrack
    {
        WebMTrack(WebMTrackType a_enmType, uint8_t a_uTrack, uint64_t a_offID)
            : enmType(a_enmType)
            , uTrack(a_uTrack)
            , offUUID(a_offID)
            , cTotalBlocks(0)
            , tcLastWrittenMs(0)
        {
            uUUID = RTRandU32();
        }

        /** The type of this track. */
        WebMTrackType enmType;
        /** Track parameters. */
        union
        {
            struct
            {
                /** Sample rate of input data. */
                uint32_t uHz;
                /** Duration of the frame in samples (per channel).
                 *  Valid frame size are:
                 *
                 *  ms           Frame size
                 *  2.5          120
                 *  5            240
                 *  10           480
                 *  20 (Default) 960
                 *  40           1920
                 *  60           2880
                 */
                uint16_t framesPerBlock;
                /** How many milliseconds (ms) one written (simple) block represents. */
                uint16_t msPerBlock;
            } Audio;
        };
        /** This track's track number. Also used as key in track map. */
        uint8_t             uTrack;
        /** The track's "UUID".
         *  Needed in case this track gets mux'ed with tracks from other files. Not really unique though. */
        uint32_t            uUUID;
        /** Absolute offset in file of track UUID.
         *  Needed to write the hash sum within the footer. */
        uint64_t            offUUID;
        /** Total number of blocks. */
        uint64_t            cTotalBlocks;
        /** Timecode (in ms) of last write. */
        WebMTimecode        tcLastWrittenMs;
    };

    /**
     * Structure for keeping a cue point.
     */
    struct WebMCuePoint
    {
        WebMCuePoint(WebMTrack *a_pTrack, uint64_t a_offCluster, WebMTimecode a_tcAbs)
            : pTrack(a_pTrack)
            , offCluster(a_offCluster), tcAbs(a_tcAbs) { }

        /** Associated track. */
        WebMTrack   *pTrack;
        /** Offset (in bytes) of the related cluster containing the given position. */
        uint64_t     offCluster;
        /** Time code (absolute) of this cue point. */
        WebMTimecode tcAbs;
    };

    /**
     * Structure for keeping a WebM cluster entry.
     */
    struct WebMCluster
    {
        WebMCluster(void)
            : uID(0)
            , offStart(0)
            , fOpen(false)
            , tcStartMs(0)
            , cBlocks(0) { }

        /** This cluster's ID. */
        uint64_t      uID;
        /** Absolute offset (in bytes) of this cluster.
         *  Needed for seeking info table. */
        uint64_t      offStart;
        /** Whether this cluster element is opened currently. */
        bool          fOpen;
        /** Timecode (in ms) when this cluster starts. */
        WebMTimecode  tcStartMs;
        /** Number of (simple) blocks in this cluster. */
        uint64_t      cBlocks;
    };

    /**
     * Structure for keeping a WebM segment entry.
     *
     * Current we're only using one segment.
     */
    struct WebMSegment
    {
        WebMSegment(void)
            : tcStartMs(0)
            , tcLastWrittenMs(0)
            , offStart(0)
            , offInfo(0)
            , offSeekInfo(0)
            , offTracks(0)
            , offCues(0)
        {
            uTimecodeScaleFactor = VBOX_WEBM_TIMECODESCALE_FACTOR_MS;

            LogFunc(("Default timecode scale is: %RU64ns\n", uTimecodeScaleFactor));
        }

        int init(void)
        {
            return RTCritSectInit(&CritSect);
        }

        void destroy(void)
        {
            RTCritSectDelete(&CritSect);
        }

        /** Critical section for serializing access to this segment. */
        RTCRITSECT                      CritSect;

        /** The timecode scale factor of this segment. */
        uint64_t                        uTimecodeScaleFactor;

        /** Timecode (in ms) when starting this segment. */
        WebMTimecode                    tcStartMs;
        /** Timecode (in ms) of last write. */
        WebMTimecode                    tcLastWrittenMs;

        /** Absolute offset (in bytes) of CurSeg. */
        uint64_t                        offStart;
        /** Absolute offset (in bytes) of general info. */
        uint64_t                        offInfo;
        /** Absolute offset (in bytes) of seeking info. */
        uint64_t                        offSeekInfo;
        /** Absolute offset (in bytes) of tracks. */
        uint64_t                        offTracks;
        /** Absolute offset (in bytes) of cues table. */
        uint64_t                        offCues;
        /** List of cue points. Needed for seeking table. */
        std::list<WebMCuePoint>         lstCues;

        /** Total number of clusters. */
        uint64_t                        cClusters;

        /** Map of tracks.
         *  The key marks the track number (*not* the UUID!). */
        std::map <uint8_t, WebMTrack *> mapTracks;

        /** Current cluster which is being handled.
         *
         *  Note that we don't need (and shouldn't need, as this can be a *lot* of data!) a
         *  list of all clusters. */
        WebMCluster                     CurCluster;

        WebMQueue                       queueBlocks;

    } CurSeg;

    /** Audio codec to use. */
    WebMWriter::AudioCodec      m_enmAudioCodec;
    /** Video codec to use. */
    WebMWriter::VideoCodec      m_enmVideoCodec;

    /** Whether we're currently in the tracks section. */
    bool                        m_fInTracksSection;

    /** Size of timecodes in bytes. */
    size_t                      m_cbTimecode;
    /** Maximum value a timecode can have. */
    uint32_t                    m_uTimecodeMax;

#ifdef VBOX_WITH_LIBVPX
    /**
     * Block data for VP8-encoded video data.
     */
    struct BlockData_VP8
    {
        const vpx_codec_enc_cfg_t *pCfg;
        const vpx_codec_cx_pkt_t  *pPkt;
    };
#endif /* VBOX_WITH_LIBVPX */

#ifdef VBOX_WITH_LIBOPUS
    /**
     * Block data for Opus-encoded audio data.
     */
    struct BlockData_Opus
    {
        /** Pointer to encoded Opus audio data. */
        const void *pvData;
        /** Size (in bytes) of encoded Opus audio data. */
        size_t      cbData;
        /** PTS (in ms) of encoded Opus audio data. */
        uint64_t    uPTSMs;
    };
#endif /* VBOX_WITH_LIBOPUS */

public:

    WebMWriter();

    virtual ~WebMWriter();

public:

    int OpenEx(const char *a_pszFilename, PRTFILE a_phFile,
               WebMWriter::AudioCodec a_enmAudioCodec, WebMWriter::VideoCodec a_enmVideoCodec);

    int Open(const char *a_pszFilename, uint64_t a_fOpen,
             WebMWriter::AudioCodec a_enmAudioCodec, WebMWriter::VideoCodec a_enmVideoCodec);

    int Close(void);

    int AddAudioTrack(uint16_t uHz, uint8_t cChannels, uint8_t cBits, uint8_t *puTrack);

    int AddVideoTrack(uint16_t uWidth, uint16_t uHeight, double dbFPS, uint8_t *puTrack);

    int WriteBlock(uint8_t uTrack, const void *pvData, size_t cbData);

    const Utf8Str& GetFileName(void);

    uint64_t GetFileSize(void);

    uint64_t GetAvailableSpace(void);

protected:

    int init(void);

    void destroy(void);

    int writeHeader(void);

    void writeSegSeekInfo(void);

    int writeFooter(void);

    int writeSimpleBlockEBML(WebMTrack *a_pTrack, WebMSimpleBlock *a_pBlock);

    int writeSimpleBlockQueued(WebMTrack *a_pTrack, WebMSimpleBlock *a_pBlock);

#ifdef VBOX_WITH_LIBVPX
    int writeSimpleBlockVP8(WebMTrack *a_pTrack, const vpx_codec_enc_cfg_t *a_pCfg, const vpx_codec_cx_pkt_t *a_pPkt);
#endif

#ifdef VBOX_WITH_LIBOPUS
    int writeSimpleBlockOpus(WebMTrack *a_pTrack, const void *pvData, size_t cbData, WebMTimecode uPTSMs);
#endif

    int processQueues(WebMQueue *pQueue, bool fForce);

protected:

    typedef std::map <uint8_t, WebMTrack *> WebMTracks;
};

#endif /* !____WEBMWRITER */
