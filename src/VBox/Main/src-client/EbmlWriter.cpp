/* $Id$ */
/** @file
 * EbmlWriter.cpp - EBML writer + WebM container handling.
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

#define LOG_GROUP LOG_GROUP_MAIN_DISPLAY
#include "LoggingNew.h"

#include <list>
#include <map>
#include <stack>

#include <math.h> /* For lround.h. */

#include <iprt/asm.h>
#include <iprt/buildconfig.h>
#include <iprt/cdefs.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/rand.h>
#include <iprt/string.h>

#include <VBox/log.h>
#include <VBox/version.h>

#include "EbmlWriter.h"
#include "EbmlMkvIDs.h"


class Ebml
{
public:
    typedef uint32_t EbmlClassId;

private:

    struct EbmlSubElement
    {
        uint64_t offset;
        EbmlClassId classId;
        EbmlSubElement(uint64_t offs, EbmlClassId cid) : offset(offs), classId(cid) {}
    };

    std::stack<EbmlSubElement> m_Elements;
    RTFILE m_File;

public:

    Ebml(void)
        : m_File(NIL_RTFILE) { }

    virtual ~Ebml(void) { close(); }

public:

    /** Creates EBML output file. */
    inline int create(const char *a_pszFilename, uint64_t fOpen)
    {
        return RTFileOpen(&m_File, a_pszFilename, fOpen);
    }

    /** Returns file size. */
    inline uint64_t getFileSize(void)
    {
        return RTFileTell(m_File);
    }

    /** Get reference to file descriptor */
    inline const RTFILE &getFile(void)
    {
        return m_File;
    }

    /** Returns available space on storage. */
    inline uint64_t getAvailableSpace(void)
    {
        RTFOFF pcbFree;
        int rc = RTFileQueryFsSizes(m_File, NULL, &pcbFree, 0, 0);
        return (RT_SUCCESS(rc)? (uint64_t)pcbFree : UINT64_MAX);
    }

    /** Closes the file. */
    inline void close(void)
    {
        if (!isOpen())
            return;

        AssertMsg(m_Elements.size() == 0,
                  ("%zu elements are not closed yet (next element to close is 0x%x)\n",
                   m_Elements.size(), m_Elements.top().classId));

        RTFileClose(m_File);
        m_File = NIL_RTFILE;
    }

    /**
     * Returns whether the file is open or not.
     *
     * @returns True if open, false if not.
     */
    inline bool isOpen(void)
    {
        return RTFileIsValid(m_File);
    }

    /** Starts an EBML sub-element. */
    inline Ebml &subStart(EbmlClassId classId)
    {
        writeClassId(classId);
        /* store the current file offset. */
        m_Elements.push(EbmlSubElement(RTFileTell(m_File), classId));
        /* Indicates that size of the element
         * is unkown (as according to EBML specs).
         */
        writeUnsignedInteger(UINT64_C(0x01FFFFFFFFFFFFFF));
        return *this;
    }

    /** Ends an EBML sub-element. */
    inline Ebml &subEnd(EbmlClassId classId)
    {
#ifdef VBOX_STRICT
        /* Class ID on the top of the stack should match the class ID passed
         * to the function. Otherwise it may mean that we have a bug in the code.
         */
        AssertMsg(!m_Elements.empty(), ("No elements to close anymore\n"));
        AssertMsg(m_Elements.top().classId == classId,
                  ("Ending sub element 0x%x is in wrong order (next to close is 0x%x)\n", classId, m_Elements.top().classId));
#else
        RT_NOREF(classId);
#endif

        uint64_t uPos = RTFileTell(m_File);
        uint64_t uSize = uPos - m_Elements.top().offset - 8;
        RTFileSeek(m_File, m_Elements.top().offset, RTFILE_SEEK_BEGIN, NULL);

        /* Make sure that size will be serialized as uint64_t. */
        writeUnsignedInteger(uSize | UINT64_C(0x0100000000000000));
        RTFileSeek(m_File, uPos, RTFILE_SEEK_BEGIN, NULL);
        m_Elements.pop();
        return *this;
    }

    /** Serializes a null-terminated string. */
    inline Ebml &serializeString(EbmlClassId classId, const char *str)
    {
        writeClassId(classId);
        uint64_t size = strlen(str);
        writeSize(size);
        write(str, size);
        return *this;
    }

    /* Serializes an UNSIGNED integer
     * If size is zero then it will be detected automatically. */
    inline Ebml &serializeUnsignedInteger(EbmlClassId classId, uint64_t parm, size_t size = 0)
    {
        writeClassId(classId);
        if (!size) size = getSizeOfUInt(parm);
        writeSize(size);
        writeUnsignedInteger(parm, size);
        return *this;
    }

    /** Serializes a floating point value.
     *
     * Only 8-bytes double precision values are supported
     * by this function.
     */
    inline Ebml &serializeFloat(EbmlClassId classId, float value)
    {
        writeClassId(classId);
        Assert(sizeof(uint32_t) == sizeof(float));
        writeSize(sizeof(float));

        union
        {
            float   f;
            uint8_t u8[4];
        } u;

        u.f = value;

        for (int i = 3; i >= 0; i--) /* Converts values to big endian. */
            write(&u.u8[i], 1);

        return *this;
    }

    /** Serializes binary data. */
    inline Ebml &serializeData(EbmlClassId classId, const void *pvData, size_t cbData)
    {
        writeClassId(classId);
        writeSize(cbData);
        write(pvData, cbData);
        return *this;
    }

    /** Writes raw data to file. */
    inline int write(const void *data, size_t size)
    {
        return RTFileWrite(m_File, data, size, NULL);
    }

    /** Writes an unsigned integer of variable of fixed size. */
    inline void writeUnsignedInteger(uint64_t value, size_t size = sizeof(uint64_t))
    {
        /* convert to big-endian */
        value = RT_H2BE_U64(value);
        write(reinterpret_cast<uint8_t*>(&value) + sizeof(value) - size, size);
    }

    /** Writes EBML class ID to file.
     *
     * EBML ID already has a UTF8-like represenation
     * so getSizeOfUInt is used to determine
     * the number of its bytes.
     */
    inline void writeClassId(EbmlClassId parm)
    {
        writeUnsignedInteger(parm, getSizeOfUInt(parm));
    }

    /** Writes data size value. */
    inline void writeSize(uint64_t parm)
    {
        /* The following expression defines the size of the value that will be serialized
         * as an EBML UTF-8 like integer (with trailing bits represeting its size):
          1xxx xxxx                                                                              - value 0 to  2^7-2
          01xx xxxx  xxxx xxxx                                                                   - value 0 to 2^14-2
          001x xxxx  xxxx xxxx  xxxx xxxx                                                        - value 0 to 2^21-2
          0001 xxxx  xxxx xxxx  xxxx xxxx  xxxx xxxx                                             - value 0 to 2^28-2
          0000 1xxx  xxxx xxxx  xxxx xxxx  xxxx xxxx  xxxx xxxx                                  - value 0 to 2^35-2
          0000 01xx  xxxx xxxx  xxxx xxxx  xxxx xxxx  xxxx xxxx  xxxx xxxx                       - value 0 to 2^42-2
          0000 001x  xxxx xxxx  xxxx xxxx  xxxx xxxx  xxxx xxxx  xxxx xxxx  xxxx xxxx            - value 0 to 2^49-2
          0000 0001  xxxx xxxx  xxxx xxxx  xxxx xxxx  xxxx xxxx  xxxx xxxx  xxxx xxxx  xxxx xxxx - value 0 to 2^56-2
         */
        size_t size = 8 - ! (parm & (UINT64_MAX << 49)) - ! (parm & (UINT64_MAX << 42)) -
                          ! (parm & (UINT64_MAX << 35)) - ! (parm & (UINT64_MAX << 28)) -
                          ! (parm & (UINT64_MAX << 21)) - ! (parm & (UINT64_MAX << 14)) -
                          ! (parm & (UINT64_MAX << 7));
        /* One is subtracted in order to avoid loosing significant bit when size = 8. */
        uint64_t mask = RT_BIT_64(size * 8 - 1);
        writeUnsignedInteger((parm & (((mask << 1) - 1) >> size)) | (mask >> (size - 1)), size);
    }

    /** Size calculation for variable size UNSIGNED integer.
     *
     * The function defines the size of the number by trimming
     * consequent trailing zero bytes starting from the most significant.
     * The following statement is always true:
     * 1 <= getSizeOfUInt(arg) <= 8.
     *
     * Every !(arg & (UINT64_MAX << X)) expression gives one
     * if an only if all the bits from X to 63 are set to zero.
     */
    static inline size_t getSizeOfUInt(uint64_t arg)
    {
        return 8 - ! (arg & (UINT64_MAX << 56)) - ! (arg & (UINT64_MAX << 48)) -
                   ! (arg & (UINT64_MAX << 40)) - ! (arg & (UINT64_MAX << 32)) -
                   ! (arg & (UINT64_MAX << 24)) - ! (arg & (UINT64_MAX << 16)) -
                   ! (arg & (UINT64_MAX << 8));
    }

private:
    void operator=(const Ebml &);

};

/** No flags specified. */
#define VBOX_WEBM_BLOCK_FLAG_NONE           0
/** Invisible block which can be skipped. */
#define VBOX_WEBM_BLOCK_FLAG_INVISIBLE      0x08
/** The block marks a key frame. */
#define VBOX_WEBM_BLOCK_FLAG_KEY_FRAME      0x80

class WebMWriter_Impl
{
    /**
     * Structure for keeping a cue entry.
     */
    struct WebMCueEntry
    {
        WebMCueEntry(uint32_t t, uint64_t l)
            : time(t), loc(l) {}

        uint32_t    time;
        uint64_t    loc;
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

    /**
     * Structure for keeping a WebM track entry.
     */
    struct WebMTrack
    {
        WebMTrack(WebMTrackType a_enmType, uint8_t a_uTrack, uint64_t a_offID)
            : enmType(a_enmType)
            , uTrack(a_uTrack)
            , offUUID(a_offID)
            , cTotalClusters(0)
            , cTotalBlocks(0)
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
                uint16_t csFrame;
            } Audio;
        };
        /** This track's track number. Also used as key in track map. */
        uint8_t       uTrack;
        /** The track's "UUID".
         *  Needed in case this track gets mux'ed with tracks from other files. Not really unique though. */
        uint32_t      uUUID;
        /** Absolute offset in file of track UUID.
         *  Needed to write the hash sum within the footer. */
        uint64_t      offUUID;
        /** Total number of clusters. */
        uint64_t      cTotalClusters;
        /** Total number of blocks. */
        uint64_t      cTotalBlocks;
    };

    /**
     * Structure for keeping a WebM cluster entry.
     */
    struct WebMCluster
    {
        WebMCluster(void)
            : uID(0)
            , offCluster(0)
            , fOpen(false)
            , tcStart(0)
            , tcLast(0)
            , cBlocks(0)
            , cbData(0) { }

        /** This cluster's ID. */
        uint64_t      uID;
        /** Absolute offset (in bytes) of current cluster.
         *  Needed for seeking info table. */
        uint64_t      offCluster;
        /** Whether this cluster element is opened currently. */
        bool          fOpen;
        /** Timecode when starting this cluster. */
        uint64_t      tcStart;
        /** Timecode when this cluster was last touched. */
        uint64_t      tcLast;
        /** Number of (simple) blocks in this cluster. */
        uint64_t      cBlocks;
        /** Size (in bytes) of data already written. */
        uint64_t      cbData;
    };

    /**
     * Structure for keeping a WebM segment entry.
     *
     * Current we're only using one segment.
     */
    struct WebMSegment
    {
        WebMSegment(void)
            : tcStart(UINT64_MAX)
            , tcEnd(UINT64_MAX)
            , offStart(0)
            , offInfo(0)
            , offSeekInfo(0)
            , offTracks(0)
            , offCues(0)
        {
            /* This is the default for WebM -- all timecodes in the segments are expressed in ms.
             * This allows every cluster to have blocks with positive values up to 32.767 seconds. */
            uTimecodeScaleFactor = 1000000;
            //m_uTimecodeScaleFactor = lround(1000000000 /* ns */ / 48000);

            LogFunc(("Default timecode scale is: %RU64ns\n", uTimecodeScaleFactor));
        }

        /** The timecode scale factor of this segment.
         *  By default this is 1000000, which is 1ms per timecode unit. */
        uint64_t                        uTimecodeScaleFactor;

        /** Timecode when starting this segment. */
        uint64_t                        tcStart;
        /** Timecode when this segment ended. */
        uint64_t                        tcEnd;

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
        std::list<WebMCueEntry>         lstCues;

        /** Map of tracks.
         *  The key marks the track number (*not* the UUID!). */
        std::map <uint8_t, WebMTrack *> mapTracks;

        /** Current cluster which is being handled.
         *
         *  Note that we don't need (and shouldn't need, as this can be a *lot* of data!) a
         *  list of all clusters. */
        WebMCluster                     CurCluster;

    } CurSeg;

#ifdef VBOX_WITH_LIBOPUS
# pragma pack(push)
# pragma pack(1)
    /** Opus codec private data.
     *  Taken from: https://wiki.xiph.org/MatroskaOpus */
    struct OpusPrivData
    {
        OpusPrivData(uint32_t a_u32SampleRate, uint8_t a_u8Channels)
            : u8Channels(a_u8Channels)
            , u32SampleRate(a_u32SampleRate) { }

        /** "OpusHead". */
        uint8_t  au8Head[8]      = { 0x4f, 0x70, 0x75, 0x73, 0x48, 0x65, 0x61, 0x64 };
        /** Must set to 1. */
        uint8_t  u8Version       = 1;
        uint8_t  u8Channels      = 0;
        uint16_t u16PreSkip      = 0;
        /** Sample rate *before* encoding to Opus.
         *  Note: This rate has nothing to do with the playback rate later! */
        uint32_t u32SampleRate   = 0;
        uint16_t u16Gain         = 0;
        /** Must stay 0 -- otherwise a mapping table must be appended
         *  right after this header. */
        uint8_t  u8MappingFamily = 0;
    };
# pragma pack(pop)
#endif /* VBOX_WITH_LIBOPUS */

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

    Ebml                        m_Ebml;

public:

    typedef std::map <uint8_t, WebMTrack *> WebMTracks;

public:

    WebMWriter_Impl() :
        m_fInTracksSection(false)
    {
        /* Size (in bytes) of time code to write. We use 2 bytes (16 bit) by default. */
        m_cbTimecode   = 2;
        m_uTimecodeMax = UINT16_MAX;
    }

    virtual ~WebMWriter_Impl()
    {
        close();
    }

    /**
     * Adds an audio track.
     *
     * @returns IPRT status code.
     * @param   uHz             Input sampling rate.
     *                          Must be supported by the selected audio codec.
     * @param   cChannels       Number of input audio channels.
     * @param   cBits           Number of input bits per channel.
     * @param   puTrack         Track number on successful creation. Optional.
     */
    int AddAudioTrack(uint16_t uHz, uint8_t cChannels, uint8_t cBits, uint8_t *puTrack)
    {
#ifdef VBOX_WITH_LIBOPUS
        int rc;

        /*
         * Check if the requested codec rate is supported.
         *
         * Only the following values are supported by an Opus standard build
         * -- every other rate only is supported by a custom build.
         */
        switch (uHz)
        {
            case 48000:
            case 24000:
            case 16000:
            case 12000:
            case  8000:
                rc = VINF_SUCCESS;
                break;

            default:
                rc = VERR_NOT_SUPPORTED;
                break;
        }

        if (RT_FAILURE(rc))
            return rc;

        m_Ebml.subStart(MkvElem_TrackEntry);
        m_Ebml.serializeUnsignedInteger(MkvElem_TrackNumber, (uint8_t)CurSeg.mapTracks.size());
        /** @todo Implement track's "Language" property? Currently this defaults to English ("eng"). */

        uint8_t uTrack = (uint8_t)CurSeg.mapTracks.size();

        WebMTrack *pTrack = new WebMTrack(WebMTrackType_Audio, uTrack, RTFileTell(m_Ebml.getFile()));

        pTrack->Audio.uHz     = uHz;
        pTrack->Audio.csFrame = pTrack->Audio.uHz / 50; /** @todo 20 ms of audio data. Make this configurable? */

        OpusPrivData opusPrivData(uHz, cChannels);

        LogFunc(("Opus @ %RU16Hz (Frame size is %RU16 samples / channel))\n", pTrack->Audio.uHz, pTrack->Audio.csFrame));

        m_Ebml.serializeUnsignedInteger(MkvElem_TrackUID,     pTrack->uUUID, 4)
              .serializeUnsignedInteger(MkvElem_TrackType,    2 /* Audio */)
              .serializeString(MkvElem_CodecID,               "A_OPUS")
              .serializeData(MkvElem_CodecPrivate,            &opusPrivData, sizeof(opusPrivData))
              .serializeUnsignedInteger(MkvElem_CodecDelay,   0)
              .serializeUnsignedInteger(MkvElem_SeekPreRoll,  80000000) /* 80ms in ns. */
              .subStart(MkvElem_Audio)
                  .serializeFloat(MkvElem_SamplingFrequency,  (float)uHz)
                  .serializeUnsignedInteger(MkvElem_Channels, cChannels)
                  .serializeUnsignedInteger(MkvElem_BitDepth, cBits)
              .subEnd(MkvElem_Audio)
              .subEnd(MkvElem_TrackEntry);

        CurSeg.mapTracks[uTrack] = pTrack;

        if (puTrack)
            *puTrack = uTrack;

        return VINF_SUCCESS;
#else
        RT_NOREF(uHz, cChannels, cBits, puTrack);
        return VERR_NOT_SUPPORTED;
#endif
    }

    int AddVideoTrack(uint16_t uWidth, uint16_t uHeight, double dbFPS, uint8_t *puTrack)
    {
#ifdef VBOX_WITH_LIBVPX
        m_Ebml.subStart(MkvElem_TrackEntry);
        m_Ebml.serializeUnsignedInteger(MkvElem_TrackNumber, (uint8_t)CurSeg.mapTracks.size());

        uint8_t uTrack = (uint8_t)CurSeg.mapTracks.size();

        WebMTrack *pTrack = new WebMTrack(WebMTrackType_Video, uTrack, RTFileTell(m_Ebml.getFile()));

        /** @todo Resolve codec type. */
        m_Ebml.serializeUnsignedInteger(MkvElem_TrackUID,    pTrack->uUUID /* UID */, 4)
              .serializeUnsignedInteger(MkvElem_TrackType,   1 /* Video */)
              .serializeString(MkvElem_CodecID,              "V_VP8")
              .subStart(MkvElem_Video)
              .serializeUnsignedInteger(MkvElem_PixelWidth,  uWidth)
              .serializeUnsignedInteger(MkvElem_PixelHeight, uHeight)
              .serializeFloat(MkvElem_FrameRate,             dbFPS)
              .subEnd(MkvElem_Video)
              .subEnd(MkvElem_TrackEntry);

        CurSeg.mapTracks[uTrack] = pTrack;

        if (puTrack)
            *puTrack = uTrack;

        return VINF_SUCCESS;
#else
        RT_NOREF(uWidth, uHeight, dbFPS, puTrack);
        return VERR_NOT_SUPPORTED;
#endif
    }

    int writeHeader(void)
    {
        LogFunc(("Header @ %RU64\n", RTFileTell(m_Ebml.getFile())));

        m_Ebml.subStart(MkvElem_EBML)
              .serializeUnsignedInteger(MkvElem_EBMLVersion, 1)
              .serializeUnsignedInteger(MkvElem_EBMLReadVersion, 1)
              .serializeUnsignedInteger(MkvElem_EBMLMaxIDLength, 4)
              .serializeUnsignedInteger(MkvElem_EBMLMaxSizeLength, 8)
              .serializeString(MkvElem_DocType, "webm")
              .serializeUnsignedInteger(MkvElem_DocTypeVersion, 2)
              .serializeUnsignedInteger(MkvElem_DocTypeReadVersion, 2)
              .subEnd(MkvElem_EBML);

        m_Ebml.subStart(MkvElem_Segment);

        /* Save offset of current segment. */
        CurSeg.offStart = RTFileTell(m_Ebml.getFile());

        writeSegSeekInfo();

        /* Save offset of upcoming tracks segment. */
        CurSeg.offTracks = RTFileTell(m_Ebml.getFile());

        /* The tracks segment starts right after this header. */
        m_Ebml.subStart(MkvElem_Tracks);
        m_fInTracksSection = true;

        return VINF_SUCCESS;
    }

    int writeSimpleBlockInternal(WebMTrack *a_pTrack, uint64_t a_uTimecode,
                                 const void *a_pvData, size_t a_cbData, uint8_t a_fFlags)
    {
        LogFunc(("SimpleBlock @ %RU64 (T%RU8, TS=%RU64, %zu bytes)\n",
                 RTFileTell(m_Ebml.getFile()), a_pTrack->uTrack, a_uTimecode, a_cbData));

        /** @todo Mask out non-valid timecode bits, e.g. the upper 48 bits for a (default) 16-bit timecode. */
        Assert(a_uTimecode <= m_uTimecodeMax);

        /* Write a "Simple Block". */
        m_Ebml.writeClassId(MkvElem_SimpleBlock);
        /* Block size. */
        m_Ebml.writeUnsignedInteger(0x10000000u | (  1            /* Track number size. */
                                                   + m_cbTimecode /* Timecode size .*/
                                                   + 1            /* Flags size. */
                                                   + a_cbData     /* Actual frame data size. */),  4);
        /* Track number. */
        m_Ebml.writeSize(a_pTrack->uTrack);
        /* Timecode (relative to cluster opening timecode). */
        m_Ebml.writeUnsignedInteger(a_uTimecode, m_cbTimecode);
        /* Flags. */
        m_Ebml.writeUnsignedInteger(a_fFlags, 1);
        /* Frame data. */
        m_Ebml.write(a_pvData, a_cbData);

        a_pTrack->cTotalBlocks++;

        return VINF_SUCCESS;
    }

#ifdef VBOX_WITH_LIBVPX
    int writeBlockVP8(WebMTrack *a_pTrack, const vpx_codec_enc_cfg_t *a_pCfg, const vpx_codec_cx_pkt_t *a_pPkt)
    {
        RT_NOREF(a_pTrack);

        /* Calculate the PTS of this frame in milliseconds. */
        uint64_t tcPTS = a_pPkt->data.frame.pts * 1000
                         * (uint64_t) a_pCfg->g_timebase.num / a_pCfg->g_timebase.den;

        if (tcPTS <= CurSeg.CurCluster.tcLast)
            tcPTS = CurSeg.CurCluster.tcLast + 1;

        CurSeg.CurCluster.tcLast = tcPTS;

        if (CurSeg.CurCluster.tcStart == UINT64_MAX)
            CurSeg.CurCluster.tcStart = CurSeg.CurCluster.tcLast;

        /* Calculate the relative time of this block. */
        uint16_t tcBlock       = 0;
        bool     fClusterStart = false;

        /* Did we reach the maximum our timecode can hold? Use a new cluster then. */
        if (tcPTS - CurSeg.CurCluster.tcStart > m_uTimecodeMax)
            fClusterStart = true;
        else
        {
            /* Calculate the block's timecode, which is relative to the current cluster's starting timecode. */
            tcBlock = static_cast<uint16_t>(tcPTS - CurSeg.CurCluster.tcStart);
        }

        const bool fKeyframe = RT_BOOL(a_pPkt->data.frame.flags & VPX_FRAME_IS_KEY);

        if (   fClusterStart
            || fKeyframe)
        {
            WebMCluster &Cluster = CurSeg.CurCluster;

            a_pTrack->cTotalClusters++;

            if (Cluster.fOpen) /* Close current cluster first. */
            {
                m_Ebml.subEnd(MkvElem_Cluster);
                Cluster.fOpen = false;
            }

            tcBlock = 0;

            /* Open a new cluster. */
            Cluster.fOpen      = true;
            Cluster.tcStart    = tcPTS;
            Cluster.offCluster = RTFileTell(m_Ebml.getFile());
            Cluster.cBlocks    = 0;
            Cluster.cbData     = 0;

            LogFunc(("[C%RU64] Start @ tc=%RU64 off=%RU64\n", a_pTrack->cTotalClusters, Cluster.tcStart, Cluster.offCluster));

            m_Ebml.subStart(MkvElem_Cluster)
                  .serializeUnsignedInteger(MkvElem_Timecode, Cluster.tcStart);

            /* Save a cue point if this is a keyframe. */
            if (fKeyframe)
            {
                WebMCueEntry cue(Cluster.tcStart, Cluster.offCluster);
                CurSeg.lstCues.push_back(cue);
            }
        }

        LogFunc(("tcPTS=%RU64, s=%RU64, e=%RU64\n", tcPTS, CurSeg.CurCluster.tcStart, CurSeg.CurCluster.tcLast));

        uint8_t fFlags = 0;
        if (fKeyframe)
            fFlags |= VBOX_WEBM_BLOCK_FLAG_KEY_FRAME;
        if (a_pPkt->data.frame.flags & VPX_FRAME_IS_INVISIBLE)
            fFlags |= VBOX_WEBM_BLOCK_FLAG_INVISIBLE;

        return writeSimpleBlockInternal(a_pTrack, tcBlock, a_pPkt->data.frame.buf, a_pPkt->data.frame.sz, fFlags);
    }
#endif /* VBOX_WITH_LIBVPX */

#ifdef VBOX_WITH_LIBOPUS
    /* Audio blocks that have same absolute timecode as video blocks SHOULD be written before the video blocks. */
    int writeBlockOpus(WebMTrack *a_pTrack, const void *pvData, size_t cbData, uint64_t uTimeStampMs)
    {
        AssertPtrReturn(a_pTrack, VERR_INVALID_POINTER);
        AssertPtrReturn(pvData, VERR_INVALID_POINTER);
        AssertReturn(cbData, VERR_INVALID_PARAMETER);

        RT_NOREF(uTimeStampMs);

        WebMCluster &Cluster = CurSeg.CurCluster;

        /* Calculate the PTS. */
        /* Make sure to round the result. This is very important! */
        uint64_t tcPTSRaw = lround((CurSeg.uTimecodeScaleFactor * 1000 * Cluster.cbData) / a_pTrack->Audio.uHz);

        /* Calculate the absolute PTS. */
        uint64_t tcPTS = lround(tcPTSRaw / CurSeg.uTimecodeScaleFactor);

        if (Cluster.tcStart == UINT64_MAX)
            Cluster.tcStart = tcPTS;

        LogFunc(("tcPTSRaw=%RU64, tcPTS=%RU64, cbData=%RU64, uTimeStampMs=%RU64\n",
                 tcPTSRaw, tcPTS, Cluster.cbData, uTimeStampMs));

        Cluster.tcLast = tcPTS;

        uint16_t tcBlock;
        bool     fClusterStart = false;

        if (a_pTrack->cTotalBlocks == 0)
            fClusterStart = true;

        /* Did we reach the maximum our timecode can hold? Use a new cluster then. */
        if (tcPTS - Cluster.tcStart > m_uTimecodeMax)
        {
            tcBlock = 0;

            fClusterStart = true;
        }
        else
        {
            /* Calculate the block's timecode, which is relative to the Cluster timecode. */
            tcBlock = static_cast<uint16_t>(tcPTS - Cluster.tcStart);
        }

        if (fClusterStart)
        {
            if (Cluster.fOpen) /* Close current cluster first. */
            {
                m_Ebml.subEnd(MkvElem_Cluster);
                Cluster.fOpen = false;
            }

            tcBlock = 0;

            Cluster.fOpen      = true;
            Cluster.tcStart    = tcPTS;
            Cluster.offCluster = RTFileTell(m_Ebml.getFile());
            Cluster.cBlocks    = 0;
            Cluster.cbData     = 0;

            LogFunc(("[C%RU64] Start @ tc=%RU64 off=%RU64\n", a_pTrack->cTotalClusters, Cluster.tcStart, Cluster.offCluster));

            m_Ebml.subStart(MkvElem_Cluster)
                  .serializeUnsignedInteger(MkvElem_Timecode, Cluster.tcStart);

            a_pTrack->cTotalClusters++;
        }

        LogFunc(("tcPTS=%RU64, s=%RU64, e=%RU64\n", tcPTS, CurSeg.CurCluster.tcStart, CurSeg.CurCluster.tcLast));

        Cluster.cBlocks += 1;
        Cluster.cbData  += cbData;

        return writeSimpleBlockInternal(a_pTrack, tcBlock, pvData, cbData, VBOX_WEBM_BLOCK_FLAG_KEY_FRAME);
    }
#endif /* VBOX_WITH_LIBOPUS */

    int WriteBlock(uint8_t uTrack, const void *pvData, size_t cbData)
    {
        RT_NOREF(pvData, cbData); /* Only needed for assertions for now. */

        WebMTracks::iterator itTrack = CurSeg.mapTracks.find(uTrack);
        if (itTrack == CurSeg.mapTracks.end())
            return VERR_NOT_FOUND;

        WebMTrack *pTrack = itTrack->second;
        AssertPtr(pTrack);

        int rc;

        if (m_fInTracksSection)
        {
            m_Ebml.subEnd(MkvElem_Tracks);
            m_fInTracksSection = false;
        }

        switch (pTrack->enmType)
        {

            case WebMTrackType_Audio:
            {
#ifdef VBOX_WITH_LIBOPUS
                if (m_enmAudioCodec == WebMWriter::AudioCodec_Opus)
                {
                    Assert(cbData == sizeof(WebMWriter::BlockData_Opus));
                    WebMWriter::BlockData_Opus *pData = (WebMWriter::BlockData_Opus *)pvData;
                    rc = writeBlockOpus(pTrack, pData->pvData, pData->cbData, pData->uTimestampMs);
                }
                else
#endif /* VBOX_WITH_LIBOPUS */
                    rc = VERR_NOT_SUPPORTED;
                break;
            }

            case WebMTrackType_Video:
            {
#ifdef VBOX_WITH_LIBVPX
                if (m_enmVideoCodec == WebMWriter::VideoCodec_VP8)
                {
                    Assert(cbData == sizeof(WebMWriter::BlockData_VP8));
                    WebMWriter::BlockData_VP8 *pData = (WebMWriter::BlockData_VP8 *)pvData;
                    rc = writeBlockVP8(pTrack, pData->pCfg, pData->pPkt);
                }
                else
#endif /* VBOX_WITH_LIBVPX */
                    rc = VERR_NOT_SUPPORTED;
                break;
            }

            default:
                rc = VERR_NOT_SUPPORTED;
                break;
        }

        return rc;
    }

    int writeFooter(void)
    {
        if (m_fInTracksSection)
        {
            m_Ebml.subEnd(MkvElem_Tracks);
            m_fInTracksSection = false;
        }

        if (CurSeg.CurCluster.fOpen)
        {
            m_Ebml.subEnd(MkvElem_Cluster);
            CurSeg.CurCluster.fOpen = false;
        }

        /*
         * Write Cues element.
         */
        LogFunc(("Cues @ %RU64\n", RTFileTell(m_Ebml.getFile())));

        CurSeg.offCues = RTFileTell(m_Ebml.getFile());

        m_Ebml.subStart(MkvElem_Cues);

        std::list<WebMCueEntry>::iterator itCuePoint = CurSeg.lstCues.begin();
        while (itCuePoint != CurSeg.lstCues.end())
        {
            m_Ebml.subStart(MkvElem_CuePoint)
                  .serializeUnsignedInteger(MkvElem_CueTime, itCuePoint->time)
                  .subStart(MkvElem_CueTrackPositions)
                  .serializeUnsignedInteger(MkvElem_CueTrack, 1)
                  .serializeUnsignedInteger(MkvElem_CueClusterPosition, itCuePoint->loc - CurSeg.offStart, 8)
                  .subEnd(MkvElem_CueTrackPositions)
                  .subEnd(MkvElem_CuePoint);

            itCuePoint++;
        }

        m_Ebml.subEnd(MkvElem_Cues)
              .subEnd(MkvElem_Segment);

        /*
         * Re-Update SeekHead / Info elements.
         */

        writeSegSeekInfo();

        return RTFileSeek(m_Ebml.getFile(), 0, RTFILE_SEEK_END, NULL);
    }

    int close(void)
    {
        WebMTracks::iterator itTrack = CurSeg.mapTracks.begin();
        for (; itTrack != CurSeg.mapTracks.end(); ++itTrack)
        {
            delete itTrack->second;
            CurSeg.mapTracks.erase(itTrack);
        }

        Assert(CurSeg.mapTracks.size() == 0);

        m_Ebml.close();

        return VINF_SUCCESS;
    }

    friend class Ebml;
    friend class WebMWriter;

private:

    /**
     * Writes the segment's seek information and cue points.
     *
     * @returns IPRT status code.
     */
    void writeSegSeekInfo(void)
    {
        if (CurSeg.offSeekInfo)
            RTFileSeek(m_Ebml.getFile(), CurSeg.offSeekInfo, RTFILE_SEEK_BEGIN, NULL);
        else
            CurSeg.offSeekInfo = RTFileTell(m_Ebml.getFile());

        LogFunc(("SeekHead @ %RU64\n", CurSeg.offSeekInfo));

        m_Ebml.subStart(MkvElem_SeekHead)

              .subStart(MkvElem_Seek)
              .serializeUnsignedInteger(MkvElem_SeekID, MkvElem_Tracks)
              .serializeUnsignedInteger(MkvElem_SeekPosition, CurSeg.offTracks - CurSeg.offStart, 8)
              .subEnd(MkvElem_Seek)

              .subStart(MkvElem_Seek)
              .serializeUnsignedInteger(MkvElem_SeekID, MkvElem_Cues)
              .serializeUnsignedInteger(MkvElem_SeekPosition, CurSeg.offCues - CurSeg.offStart, 8)
              .subEnd(MkvElem_Seek)

              .subStart(MkvElem_Seek)
              .serializeUnsignedInteger(MkvElem_SeekID, MkvElem_Info)
              .serializeUnsignedInteger(MkvElem_SeekPosition, CurSeg.offInfo - CurSeg.offStart, 8)
              .subEnd(MkvElem_Seek)

        .subEnd(MkvElem_SeekHead);

        //int64_t iFrameTime = (int64_t)1000 * 1 / 25; //m_Framerate.den / m_Framerate.num; /** @todo Fix this! */
        CurSeg.offInfo = RTFileTell(m_Ebml.getFile());

        LogFunc(("Info @ %RU64\n", CurSeg.offInfo));

        char szMux[64];
        RTStrPrintf(szMux, sizeof(szMux),
#ifdef VBOX_WITH_LIBVPX
                     "vpxenc%s", vpx_codec_version_str());
#else
                     "unknown");
#endif
        char szApp[64];
        RTStrPrintf(szApp, sizeof(szApp), VBOX_PRODUCT " %sr%u", VBOX_VERSION_STRING, RTBldCfgRevision());

        LogFunc(("Duration=%RU64\n", CurSeg.tcEnd - CurSeg.tcStart));

        m_Ebml.subStart(MkvElem_Info)
              .serializeUnsignedInteger(MkvElem_TimecodeScale, CurSeg.uTimecodeScaleFactor)
              .serializeFloat(MkvElem_Segment_Duration, CurSeg.tcEnd /*+ iFrameTime*/ - CurSeg.tcStart)
              .serializeString(MkvElem_MuxingApp, szMux)
              .serializeString(MkvElem_WritingApp, szApp)
              .subEnd(MkvElem_Info);
    }
};

WebMWriter::WebMWriter(void) : m_pImpl(new WebMWriter_Impl()) {}

WebMWriter::~WebMWriter(void)
{
    if (m_pImpl)
    {
        Close();
        delete m_pImpl;
    }
}

int WebMWriter::Create(const char *a_pszFilename, uint64_t a_fOpen,
                       WebMWriter::AudioCodec a_enmAudioCodec, WebMWriter::VideoCodec a_enmVideoCodec)
{
    try
    {
        m_pImpl->m_enmAudioCodec = a_enmAudioCodec;
        m_pImpl->m_enmVideoCodec = a_enmVideoCodec;

        LogFunc(("Creating '%s'\n", a_pszFilename));

        int rc = m_pImpl->m_Ebml.create(a_pszFilename, a_fOpen);
        if (RT_SUCCESS(rc))
            rc = m_pImpl->writeHeader();
    }
    catch(int rc)
    {
        return rc;
    }
    return VINF_SUCCESS;
}

int WebMWriter::Close(void)
{
    if (!m_pImpl->m_Ebml.isOpen())
        return VINF_SUCCESS;

    int rc = m_pImpl->writeFooter();
    if (RT_SUCCESS(rc))
        m_pImpl->close();

    return rc;
}

int WebMWriter::AddAudioTrack(uint16_t uHz, uint8_t cChannels, uint8_t cBitDepth, uint8_t *puTrack)
{
    return m_pImpl->AddAudioTrack(uHz, cChannels, cBitDepth, puTrack);
}

int WebMWriter::AddVideoTrack(uint16_t uWidth, uint16_t uHeight, double dbFPS, uint8_t *puTrack)
{
    return m_pImpl->AddVideoTrack(uWidth, uHeight, dbFPS, puTrack);
}

int WebMWriter::WriteBlock(uint8_t uTrack, const void *pvData, size_t cbData)
{
    int rc;

    try
    {
        rc = m_pImpl->WriteBlock(uTrack, pvData, cbData);
    }
    catch(int rc2)
    {
        rc = rc2;
    }
    return rc;
}

uint64_t WebMWriter::GetFileSize(void)
{
    return m_pImpl->m_Ebml.getFileSize();
}

uint64_t WebMWriter::GetAvailableSpace(void)
{
    return m_pImpl->m_Ebml.getAvailableSpace();
}

