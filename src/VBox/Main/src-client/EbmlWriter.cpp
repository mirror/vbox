/* $Id$ */
/** @file
 * EbmlWriter.cpp - EBML writer + WebM container
 */

/*
 * Copyright (C) 2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*
 * This code is based on:
 *
 * Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "EbmlWriter.h"
#include <iprt/asm.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <VBox/log.h>

Ebml::Ebml() {}

void Ebml::init(const RTFILE &a_File)
{
    m_File = a_File;
}

void Ebml::write(const void *data, size_t size)
{
    int rc = RTFileWrite(m_File, data, size, NULL);
    if (!RT_SUCCESS(rc)) throw rc;
}

WebMWriter::WebMWriter() :
    m_bDebug(false),
    m_iLastPtsMs(-1),
    m_Framerate(),
    m_uPositionReference(0),
    m_uSeekInfoPos(0),
    m_uSegmentInfoPos(0),
    m_uTrackPos(0),
    m_uCuePos(0),
    m_uClusterPos(0),
    m_uTrackIdPos(0),
    m_uStartSegment(0),
    m_uClusterTimecode(0),
    m_bClusterOpen(false) {}

int WebMWriter::create(const char *a_pszFilename)
{
    int rc = RTFileOpen(&m_File, a_pszFilename, RTFILE_O_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
    if (RT_SUCCESS(rc))
    {
        m_Ebml.init(m_File);
    }
    return rc;
}

void WebMWriter::close()
{
    RTFileClose(m_File);
}

Ebml &operator<<(Ebml &a_Ebml, const WebMWriter::SimpleBlockData &a_Data)
{
    a_Ebml.serializeConst<WebMWriter::Mkv::SimpleBlock>();
    a_Ebml.write<uint32_t>(0x10000000u | (Ebml::getSizeOfUInt(a_Data.trackNumber) + 2 + 1 + a_Data.dataSize));
    a_Ebml.serializeInteger(a_Data.trackNumber);
    a_Ebml.write(a_Data.timeCode);
    a_Ebml.write(a_Data.flags);
    a_Ebml.write(a_Data.data, a_Data.dataSize);
    return a_Ebml;
}

void WebMWriter::writeSeekInfo()
{
    // Save the current file pointer
    uint64_t uPos = RTFileTell(m_File);
    if (m_uSeekInfoPos)
        RTFileSeek(m_File, m_uSeekInfoPos, RTFILE_SEEK_BEGIN, NULL);
    else
        m_uSeekInfoPos = uPos;

    m_Ebml << Ebml::SubStart<SeekHead>()

          << Ebml::SubStart<Seek>()
          << Ebml::Const<SeekID, Tracks>()
          << Ebml::Var<SeekPosition, uint64_t>(m_uTrackPos - m_uPositionReference)
          << Ebml::SubEnd<Seek>()

          << Ebml::SubStart<Seek>()
          << Ebml::Const<SeekID, Cues>()
          << Ebml::Var<SeekPosition, uint64_t>(m_uCuePos - m_uPositionReference)
          << Ebml::SubEnd<Seek>()

          << Ebml::SubStart<Seek>()
          << Ebml::Const<SeekID, Info>()
          << Ebml::Var<SeekPosition, uint64_t>(m_uSegmentInfoPos - m_uPositionReference)
          << Ebml::SubEnd<Seek>()

          << Ebml::SubEnd<SeekHead>();

    int64_t iFrameTime = (int64_t)1000 * m_Framerate.den / m_Framerate.num;
    m_uSegmentInfoPos = RTFileTell(m_File);

    char szVersion[64];
    RTStrPrintf(szVersion, sizeof(szVersion), "vpxenc%",
                m_bDebug ? vpx_codec_version_str() : "");

    m_Ebml << Ebml::SubStart<Info>()
           << Ebml::UnsignedInteger<TimecodeScale>(1000000)
           << Ebml::Float<Segment_Duration>(m_iLastPtsMs + iFrameTime)
           << Ebml::String<MuxingApp>(szVersion)
           << Ebml::String<WritingApp>(szVersion)
           << Ebml::SubEnd<Info>();
}

int WebMWriter::writeHeader(const vpx_codec_enc_cfg_t *a_pCfg,
                            const struct vpx_rational *a_pFps)
{
    try
    {
        m_Ebml << Ebml::SubStart<EBML>()
               << Ebml::UnsignedInteger<EBMLVersion>(1)
               << Ebml::UnsignedInteger<EBMLReadVersion>(1)
               << Ebml::UnsignedInteger<EBMLMaxIDLength>(4)
               << Ebml::UnsignedInteger<EBMLMaxSizeLength>(8)
               << Ebml::String<DocType>("webm")
               << Ebml::UnsignedInteger<DocTypeVersion>(2)
               << Ebml::UnsignedInteger<DocTypeReadVersion>(2)
               << Ebml::SubEnd<EBML>();

        m_Ebml << Ebml::SubStart<Segment>();

        m_uPositionReference = RTFileTell(m_File);
        m_Framerate = *a_pFps;

        writeSeekInfo();

        m_uTrackPos = RTFileTell(m_File);

        m_Ebml << Ebml::SubStart<Tracks>()
               << Ebml::SubStart<TrackEntry>()
               << Ebml::UnsignedInteger<TrackNumber>(1);

        m_uTrackIdPos = RTFileTell(m_File);

        m_Ebml << Ebml::Var<TrackUID, uint32_t>(0)
               << Ebml::UnsignedInteger<TrackType>(1)
               << Ebml::String<CodecID>("V_VP8")
               << Ebml::SubStart<Video>()
               << Ebml::UnsignedInteger<PixelWidth>(a_pCfg->g_w)
               << Ebml::UnsignedInteger<PixelHeight>(a_pCfg->g_h)
               << Ebml::Float<FrameRate>((double)a_pFps->num / a_pFps->den)
               << Ebml::SubEnd<Video>()
               << Ebml::SubEnd<TrackEntry>()
               << Ebml::SubEnd<Tracks>();
    }
    catch(int rc)
    {
        LogFlow(("WebMWriter::writeHeader catched"));
        return rc;
    }
    return VINF_SUCCESS;
}

int WebMWriter::writeBlock(const vpx_codec_enc_cfg_t *a_pCfg,
                            const vpx_codec_cx_pkt_t *a_pPkt)
{
    try {
        uint16_t uBlockTimecode = 0;
        int64_t  iPtsMs;
        bool     bStartCluster = false;

        /* Calculate the PTS of this frame in milliseconds */
        iPtsMs = a_pPkt->data.frame.pts * 1000
                 * (uint64_t) a_pCfg->g_timebase.num / a_pCfg->g_timebase.den;
        if (iPtsMs <= m_iLastPtsMs)
            iPtsMs = m_iLastPtsMs + 1;
        m_iLastPtsMs = iPtsMs;

        /* Calculate the relative time of this block */
        if (iPtsMs - m_uClusterTimecode > 65536)
            bStartCluster = 1;
        else
            uBlockTimecode = static_cast<uint16_t>(iPtsMs - m_uClusterTimecode);

        int fKeyframe = (a_pPkt->data.frame.flags & VPX_FRAME_IS_KEY);
        if (bStartCluster || fKeyframe)
        {
            if (m_bClusterOpen)
                m_Ebml << Ebml::SubEnd<Cluster>();

            /* Open a new cluster */
            uBlockTimecode = 0;
            m_bClusterOpen = true;
            m_uClusterTimecode = (uint32_t)iPtsMs;
            m_uClusterPos = RTFileTell(m_File);
            m_Ebml << Ebml::SubStart<Cluster>()
                   << Ebml::UnsignedInteger<Timecode>(m_uClusterTimecode);

            /* Save a cue point if this is a keyframe. */
            if (fKeyframe)
            {
                CueEntry cue(m_uClusterTimecode, m_uClusterPos);
                m_CueList.push_back(cue);
            }
        }

        // Write a Simple Block
        SimpleBlockData block(1, uBlockTimecode,
            (fKeyframe ? 0x80 : 0) | (a_pPkt->data.frame.flags & VPX_FRAME_IS_INVISIBLE ? 0x08 : 0),
            a_pPkt->data.frame.buf, a_pPkt->data.frame.sz);

        m_Ebml << block;
    }
    catch(int rc)
    {
        LogFlow(("WebMWriter::writeBlock catched"));
        return rc;
    }
    return VINF_SUCCESS;
}

int WebMWriter::writeFooter(uint32_t a_u64Hash)
{
    try {
        if (m_bClusterOpen)
            m_Ebml << Ebml::SubEnd<Cluster>();

        m_uCuePos = RTFileTell(m_File);
        m_Ebml << Ebml::SubStart<Cues>();
        for (std::list<CueEntry>::iterator it = m_CueList.begin(); it != m_CueList.end(); ++it)
        {
            m_Ebml << Ebml::SubStart<CuePoint>()
                   << Ebml::UnsignedInteger<CueTime>(it->time)
                   << Ebml::SubStart<CueTrackPositions>()
                   << Ebml::Const<CueTrack, 1>()
                   << Ebml::Var<CueClusterPosition, uint64_t>(it->loc - m_uPositionReference)
                   << Ebml::SubEnd<CueTrackPositions>()
                   << Ebml::SubEnd<CuePoint>();        
        }
        m_Ebml << Ebml::SubEnd<Cues>()
               << Ebml::SubEnd<Segment>();

        writeSeekInfo();

        int rc = RTFileSeek(m_File, m_uTrackIdPos, RTFILE_SEEK_BEGIN, NULL);
        if (!RT_SUCCESS(rc)) throw rc;

        m_Ebml << Ebml::Var<TrackUID, uint32_t>(m_bDebug ? 0xDEADBEEF : a_u64Hash);

        rc = RTFileSeek(m_File, 0, RTFILE_SEEK_END, NULL);
        if (!RT_SUCCESS(rc)) throw rc;
    }
    catch(int rc)
    {
        LogFlow(("WebMWriter::writeFooterException catched"));
        return rc;
    }
    return VINF_SUCCESS;
}