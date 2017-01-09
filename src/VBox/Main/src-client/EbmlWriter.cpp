/* $Id$ */
/** @file
 * EbmlWriter.cpp - EBML writer + WebM container
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


#include <list>
#include <stack>
#include <iprt/string.h>
#include <iprt/file.h>
#include <iprt/asm.h>
#include <iprt/cdefs.h>
#include <iprt/err.h>
#include <VBox/log.h>
#include "EbmlWriter.h"
#include "EbmlIDs.h"


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

    /** Creates EBML output file. */
    inline int create(const char *a_pszFilename, uint64_t fOpen)
    {
        return RTFileOpen(&m_File, a_pszFilename, fOpen);
    }

    /** Returns file size. */
    inline uint64_t getFileSize()
    {
        return RTFileTell(m_File);
    }

    /** Get reference to file descriptor */
    inline const RTFILE &getFile()
    {
        return m_File;
    }

    /** Returns available space on storage. */
    inline uint64_t getAvailableSpace()
    {
        RTFOFF pcbFree;
        int rc = RTFileQueryFsSizes(m_File, NULL, &pcbFree, 0, 0);
        return (RT_SUCCESS(rc)? (uint64_t)pcbFree : UINT64_MAX);
    }

    /** Closes the file. */
    inline void close()
    {
        RTFileClose(m_File);
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
        /* Class ID on the top of the stack should match the class ID passed
         * to the function. Otherwise it may mean that we have a bug in the code.
         */
        if(m_Elements.empty() || m_Elements.top().classId != classId) throw VERR_INTERNAL_ERROR;

        uint64_t uPos = RTFileTell(m_File);
        uint64_t uSize = uPos - m_Elements.top().offset - 8;
        RTFileSeek(m_File, m_Elements.top().offset, RTFILE_SEEK_BEGIN, NULL);

        /* make sure that size will be serialized as uint64 */
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
    inline Ebml &serializeFloat(EbmlClassId classId, double value)
    {
        writeClassId(classId);
        writeSize(sizeof(double));
        writeUnsignedInteger(*reinterpret_cast<uint64_t*>(&value));
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
    inline void write(const void *data, size_t size)
    {
        int rc = RTFileWrite(m_File, data, size, NULL);
        if (!RT_SUCCESS(rc)) throw rc;
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

class WebMWriter_Impl
{
    struct CueEntry
    {
        uint32_t    time;
        uint64_t    loc;
        CueEntry(uint32_t t, uint64_t l) : time(t), loc(l) {}
    };

#ifdef VBOX_WITH_AUDIO_VIDEOREC
# pragma pack(push)
# pragma pack(1)
    /** Opus codec private data.
     *  Taken from: https://wiki.xiph.org/MatroskaOpus */
    struct OpusPrivData
    {
        uint8_t  au8Head[8]       = { 0x4f, 0x70, 0x75, 0x73, 0x48, 0x65, 0x61, 0x64 };
        uint8_t  u8Version        = 1;
        uint8_t  c8Channels;
        uint16_t u16PreSkip       = 0;
        uint32_t u32SampleRate;
        uint16_t u16Gain          = 0;
        uint8_t  u8Mapping_family = 0;
    };
# pragma pack(pop)
#endif /* VBOX_WITH_AUDIO_VIDEOREC */

    /** Operation mode. */
    WebMWriter::Mode       m_enmMode;
    /** Audio codec to use. */
    WebMWriter::AudioCodec m_enmAudioCodec;
    /** Video codec to use. */
    WebMWriter::VideoCodec m_enmVideoCodec;

    bool                   m_fDebug;

    /** Timestamp of initial PTS (Presentation Time Stamp). */
    int64_t                m_tsInitialPtsMs;
    /** Timestamp of last written PTS (Presentation Time Stamp). */
    int64_t                m_tsLastPtsMs;

    vpx_rational_t         m_Framerate;

    /** Start offset (in bytes) of current segment. */
    uint64_t               m_offSegCurStart;

    /** Start offset (in bytes) of seeking info segment. */
    uint64_t               m_offSegSeekInfoStart;
    /** Offset (in bytes) for current seek info element. */
    uint64_t               m_offSeekInfo;

    /** Start offset (in bytes) of tracks segment. */
    uint64_t               m_offSegTracksStart;

    /** Absolute position of cue segment. */
    uint64_t               m_uCuePos;
    /** List of cue points. Needed for seeking table. */
    std::list<CueEntry>    m_lstCue;

    uint64_t               m_uTrackIdPos;

    /** Timestamp (in ms) when the current cluster has been opened. */
    uint32_t               m_tsClusterOpenMs;
    /** Whether we're currently in an opened cluster segment. */
    bool                   m_fClusterOpen;
    /** Absolute position (in bytes) of current cluster within file.
     *  Needed for seeking info table. */
    uint64_t               m_offSegClusterStart;

    Ebml                   m_Ebml;

public:

    WebMWriter_Impl() :
          m_enmMode(WebMWriter::Mode_Unknown)
        , m_fDebug(false)
        , m_tsInitialPtsMs(-1)
        , m_tsLastPtsMs(-1)
        , m_Framerate()
        , m_offSegCurStart(0)
        , m_offSegSeekInfoStart(0)
        , m_offSeekInfo(0)
        , m_offSegTracksStart(0)
        , m_uCuePos(0)
        , m_uTrackIdPos(0)
        , m_tsClusterOpenMs(0)
        , m_fClusterOpen(false)
        , m_offSegClusterStart(0) {}

    void writeHeader(const vpx_codec_enc_cfg_t *a_pCfg, const struct vpx_rational *a_pFps)
    {
        m_Ebml.subStart(EBML)
              .serializeUnsignedInteger(EBMLVersion, 1)
              .serializeUnsignedInteger(EBMLReadVersion, 1)
              .serializeUnsignedInteger(EBMLMaxIDLength, 4)
              .serializeUnsignedInteger(EBMLMaxSizeLength, 8)
              .serializeString(DocType, "webm")
              .serializeUnsignedInteger(DocTypeVersion, 2)
              .serializeUnsignedInteger(DocTypeReadVersion, 2)
              .subEnd(EBML);

        m_Ebml.subStart(Segment);

        m_Framerate = *a_pFps;

        /* Save offset of current segment. */
        m_offSegCurStart = RTFileTell(m_Ebml.getFile());

        writeSeekInfo();

        /* Save offset of upcoming tracks segment. */
        m_offSegTracksStart = RTFileTell(m_Ebml.getFile());

        m_Ebml.subStart(Tracks);

        /* Write video? */
        if (   m_enmMode == WebMWriter::Mode_Video
            || m_enmMode == WebMWriter::Mode_AudioVideo)
        {
            /*
             * Video track.
             */
            m_Ebml.subStart(TrackEntry);
            m_Ebml.serializeUnsignedInteger(TrackNumber, 1);

            m_uTrackIdPos = RTFileTell(m_Ebml.getFile());

            m_Ebml.serializeUnsignedInteger(TrackUID, 0 /* UID */, 4)
                  .serializeUnsignedInteger(TrackType, 1 /* Video */)
                  .serializeString(CodecID, "V_VP8")
                  .subStart(Video)
                  .serializeUnsignedInteger(PixelWidth, a_pCfg->g_w)
                  .serializeUnsignedInteger(PixelHeight, a_pCfg->g_h)
                  .serializeFloat(FrameRate, (double) a_pFps->num / a_pFps->den)
                  .subEnd(Video)
                  .subEnd(TrackEntry);
        }

#ifdef VBOX_WITH_AUDIO_VIDEOREC
        if (   m_enmMode == WebMWriter::Mode_Audio
            || m_enmMode == WebMWriter::Mode_AudioVideo)
        {
            /*
             * Audio track.
             */
            m_Ebml.subStart(TrackEntry);
            m_Ebml.serializeUnsignedInteger(TrackNumber, 2);
            /** @todo Implement track's "Language" property? Currently this defaults to English ("eng"). */

            OpusPrivData opusPrivData;

            m_Ebml.serializeUnsignedInteger(TrackUID, 1 /* UID */, 4)
                  .serializeUnsignedInteger(TrackType, 2 /* Audio */)
                  .serializeString(CodecID, "A_OPUS")
                  .serializeData(CodecPrivate, &opusPrivData, sizeof(opusPrivData))
                  .subStart(Audio)
                  .serializeFloat(SamplingFrequency, 44100.0)
                  .serializeFloat(OutputSamplingFrequency, 44100.0)
                  .serializeUnsignedInteger(Channels, 2)
                  .serializeUnsignedInteger(BitDepth, 16)
                  .subEnd(Audio)
                  .subEnd(TrackEntry);
        }
#endif

        m_Ebml.subEnd(Tracks);
    }

    void writeBlock(const vpx_codec_enc_cfg_t *a_pCfg, const vpx_codec_cx_pkt_t *a_pPkt)
    {
        /* Calculate the PTS of this frame in milliseconds. */
        int64_t iPtsMs = a_pPkt->data.frame.pts * 1000
                       * (uint64_t) a_pCfg->g_timebase.num / a_pCfg->g_timebase.den;

        if (iPtsMs <= m_tsLastPtsMs)
            iPtsMs = m_tsLastPtsMs + 1;

        m_tsLastPtsMs = iPtsMs;

        if (m_tsInitialPtsMs < 0)
          m_tsInitialPtsMs = m_tsLastPtsMs;

        /* Calculate the relative time of this block. */
        uint16_t uBlockTimecode = 0;
        bool     fClusterStart  = false;

        if (iPtsMs - m_tsClusterOpenMs > 65536)
            fClusterStart = true;
        else
            uBlockTimecode = static_cast<uint16_t>(iPtsMs - m_tsClusterOpenMs);

        bool fKeyframe = RT_BOOL(a_pPkt->data.frame.flags & VPX_FRAME_IS_KEY);

        if (   fClusterStart
            || fKeyframe)
        {
            if (m_fClusterOpen)
                m_Ebml.subEnd(Cluster);

            /* Open a new cluster. */
            uBlockTimecode = 0;
            m_fClusterOpen = true;
            m_tsClusterOpenMs = (uint32_t)iPtsMs;
            m_offSegClusterStart = RTFileTell(m_Ebml.getFile());
            m_Ebml.subStart(Cluster)
                  .serializeUnsignedInteger(Timecode, m_tsClusterOpenMs);

            /* Save a cue point if this is a keyframe. */
            if (fKeyframe)
            {
                CueEntry cue(m_tsClusterOpenMs, m_offSegClusterStart);
                m_lstCue.push_back(cue);
            }
        }

        /* Write a "Simple Block". */
        m_Ebml.writeClassId(SimpleBlock);
        m_Ebml.writeUnsignedInteger(0x10000000u | (4 + a_pPkt->data.frame.sz), 4);
        m_Ebml.writeSize(1);
        m_Ebml.writeUnsignedInteger(uBlockTimecode, 2);
        m_Ebml.writeUnsignedInteger((fKeyframe ? 0x80 : 0) | (a_pPkt->data.frame.flags & VPX_FRAME_IS_INVISIBLE ? 0x08 : 0), 1);
        m_Ebml.write(a_pPkt->data.frame.buf, a_pPkt->data.frame.sz);
    }

    void writeFooter(uint32_t a_u64Hash)
    {
        if (m_fClusterOpen)
            m_Ebml.subEnd(Cluster);

        m_uCuePos = RTFileTell(m_Ebml.getFile());
        m_Ebml.subStart(Cues);
        for (std::list<CueEntry>::iterator it = m_lstCue.begin(); it != m_lstCue.end(); ++it)
        {
          m_Ebml.subStart(CuePoint)
                .serializeUnsignedInteger(CueTime, it->time)
                .subStart(CueTrackPositions)
                .serializeUnsignedInteger(CueTrack, 1)
                .serializeUnsignedInteger(CueClusterPosition, it->loc - m_offSegCurStart, 8)
                .subEnd(CueTrackPositions)
                .subEnd(CuePoint);
        }

        m_Ebml.subEnd(Cues)
              .subEnd(Segment);

        writeSeekInfo();

        int rc = RTFileSeek(m_Ebml.getFile(), m_uTrackIdPos, RTFILE_SEEK_BEGIN, NULL);
        if (!RT_SUCCESS(rc)) throw rc;

        m_Ebml.serializeUnsignedInteger(TrackUID, (m_fDebug ? 0xDEADBEEF : a_u64Hash), 4);

        rc = RTFileSeek(m_Ebml.getFile(), 0, RTFILE_SEEK_END, NULL);
        if (!RT_SUCCESS(rc)) throw rc;
    }

    friend class WebMWriter;

private:

    void writeSeekInfo(void)
    {
        uint64_t uPos = RTFileTell(m_Ebml.getFile());
        if (m_offSegSeekInfoStart)
            RTFileSeek(m_Ebml.getFile(), m_offSegSeekInfoStart, RTFILE_SEEK_BEGIN, NULL);
        else
            m_offSegSeekInfoStart = uPos;

        m_Ebml.subStart(SeekHead)

              .subStart(Seek)
              .serializeUnsignedInteger(SeekID, Tracks)
              .serializeUnsignedInteger(SeekPosition, m_offSegTracksStart - m_offSegCurStart, 8)
              .subEnd(Seek)

              .subStart(Seek)
              .serializeUnsignedInteger(SeekID, Cues)
              .serializeUnsignedInteger(SeekPosition, m_uCuePos - m_offSegCurStart, 8)
              .subEnd(Seek)

              .subStart(Seek)
              .serializeUnsignedInteger(SeekID, Info)
              .serializeUnsignedInteger(SeekPosition, m_offSeekInfo - m_offSegCurStart, 8)
              .subEnd(Seek)

        .subEnd(SeekHead);

        int64_t iFrameTime = (int64_t)1000 * m_Framerate.den / m_Framerate.num;
        m_offSeekInfo = RTFileTell(m_Ebml.getFile());

        char szVersion[64];
        RTStrPrintf(szVersion, sizeof(szVersion), "vpxenc%s",
                    m_fDebug ? "" : vpx_codec_version_str());

        m_Ebml.subStart(Info)
              .serializeUnsignedInteger(TimecodeScale, 1000000)
              .serializeFloat(Segment_Duration, m_tsLastPtsMs + iFrameTime - m_tsInitialPtsMs)
              .serializeString(MuxingApp, szVersion)
              .serializeString(WritingApp, szVersion)
              .subEnd(Info);
    }
};

WebMWriter::WebMWriter(void) : m_pImpl(new WebMWriter_Impl()) {}

WebMWriter::~WebMWriter(void)
{
    if (m_pImpl)
        delete m_pImpl;
}

int WebMWriter::create(const char *a_pszFilename, uint64_t a_fOpen, WebMWriter::Mode a_enmMode,
                       WebMWriter::AudioCodec a_enmAudioCodec, WebMWriter::VideoCodec a_enmVideoCodec)
{
    m_pImpl->m_enmMode       = a_enmMode;
    m_pImpl->m_enmAudioCodec = a_enmAudioCodec;
    m_pImpl->m_enmVideoCodec = a_enmVideoCodec;

    return m_pImpl->m_Ebml.create(a_pszFilename, a_fOpen);
}

void WebMWriter::close(void)
{
    m_pImpl->m_Ebml.close();
}

int WebMWriter::writeHeader(const vpx_codec_enc_cfg_t *a_pCfg, const vpx_rational *a_pFps)
{
    try
    {
        m_pImpl->writeHeader(a_pCfg, a_pFps);
    }
    catch(int rc)
    {
        return rc;
    }
    return VINF_SUCCESS;
}

int WebMWriter::writeBlock(const vpx_codec_enc_cfg_t *a_pCfg, const vpx_codec_cx_pkt_t *a_pPkt)
{
    try
    {
        m_pImpl->writeBlock(a_pCfg, a_pPkt);
    }
    catch(int rc)
    {
        return rc;
    }
    return VINF_SUCCESS;
}

int WebMWriter::writeFooter(uint32_t a_u64Hash)
{
    try
    {
        m_pImpl->writeFooter(a_u64Hash);
    }
    catch(int rc)
    {
        return rc;
    }
    return VINF_SUCCESS;
}

uint64_t WebMWriter::getFileSize()
{
    return m_pImpl->m_Ebml.getFileSize();
}

uint64_t WebMWriter::getAvailableSpace()
{
    return m_pImpl->m_Ebml.getAvailableSpace();
}

