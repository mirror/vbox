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
#include <VBox/log.h>

uint64_t WebMWriter::getFileSize() 
{
    return RTFileTell(m_File);
}

uint64_t WebMWriter::getAvailableSpace()
{
    RTFOFF pcbFree;
    int rc = RTFileQueryFsSizes(m_File, NULL, &pcbFree, 0, 0);
    return (RT_SUCCESS(rc)? (uint64_t)pcbFree : UINT64_MAX);
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

void WebMWriter::writeSeekInfo()
{
    uint64_t uPos = RTFileTell(m_File);
    if (m_uSeekInfoPos)
        RTFileSeek(m_File, m_uSeekInfoPos, RTFILE_SEEK_BEGIN, NULL);
    else
        m_uSeekInfoPos = uPos;

    m_Ebml.subStart(SeekHead)

          .subStart(Seek)
          .serializeUnsignedInteger(SeekID, Tracks)
          .serializeUnsignedInteger(SeekPosition, m_uTrackPos - m_uPositionReference, 8)
          .subEnd(Seek)

          .subStart(Seek)
          .serializeUnsignedInteger(SeekID, Cues)
          .serializeUnsignedInteger(SeekPosition, m_uCuePos - m_uPositionReference, 8)
          .subEnd(Seek)

          .subStart(Seek)
          .serializeUnsignedInteger(SeekID, Info)
          .serializeUnsignedInteger(SeekPosition, m_uSegmentInfoPos - m_uPositionReference, 8)
          .subEnd(Seek)

          .subEnd(SeekHead);

    int64_t iFrameTime = (int64_t)1000 * m_Framerate.den / m_Framerate.num;
    m_uSegmentInfoPos = RTFileTell(m_File);

    char szVersion[64];
    RTStrPrintf(szVersion, sizeof(szVersion), "vpxenc%",
                m_bDebug ? vpx_codec_version_str() : "");

    m_Ebml.subStart(Info)
          .serializeUnsignedInteger(TimecodeScale, 1000000)
          .serializeFloat(Segment_Duration, m_iLastPtsMs + iFrameTime)
          .serializeString(MuxingApp, szVersion)
          .serializeString(WritingApp, szVersion)
          .subEnd(Info);
}

int WebMWriter::writeHeader(const vpx_codec_enc_cfg_t *a_pCfg,
                            const struct vpx_rational *a_pFps)
{
    try
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

        m_uPositionReference = RTFileTell(m_File);
        m_Framerate = *a_pFps;

        writeSeekInfo();

        m_uTrackPos = RTFileTell(m_File);

        m_Ebml.subStart(Tracks)
              .subStart(TrackEntry)
              .serializeUnsignedInteger(TrackNumber, 1);

        m_uTrackIdPos = RTFileTell(m_File);

        m_Ebml.serializeUnsignedInteger(TrackUID, 0, 4)
              .serializeUnsignedInteger(TrackType, 1)
              .serializeString(CodecID, "V_VP8")
              .subStart(Video)
              .serializeUnsignedInteger(PixelWidth, a_pCfg->g_w)
              .serializeUnsignedInteger(PixelHeight, a_pCfg->g_h)
              .serializeFloat(FrameRate, (double) a_pFps->num / a_pFps->den)
              .subEnd(Video)
              .subEnd(TrackEntry)
              .subEnd(Tracks);

    }
    catch(int rc)
    {
        return rc;
    }
    return VINF_SUCCESS;
}

int WebMWriter::writeBlock(const vpx_codec_enc_cfg_t *a_pCfg,
                            const vpx_codec_cx_pkt_t *a_pPkt)
{
  try 
    {
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
                m_Ebml.subEnd(Cluster);

            /* Open a new cluster */
            uBlockTimecode = 0;
            m_bClusterOpen = true;
            m_uClusterTimecode = (uint32_t)iPtsMs;
            m_uClusterPos = RTFileTell(m_File);
            m_Ebml.subStart(Cluster)
                  .serializeUnsignedInteger(Timecode, m_uClusterTimecode);

            /* Save a cue point if this is a keyframe. */
            if (fKeyframe)
            {
                CueEntry cue(m_uClusterTimecode, m_uClusterPos);
                m_CueList.push_back(cue);
            }
        }

        /* Write a Simple Block */
        m_Ebml.writeClassId(SimpleBlock);
        m_Ebml.writeUnsignedInteger(0x10000000u | (4 + a_pPkt->data.frame.sz), 4);
        m_Ebml.writeSize(1);
        m_Ebml.writeUnsignedInteger(uBlockTimecode, 2);
        m_Ebml.writeUnsignedInteger((fKeyframe ? 0x80 : 0) | (a_pPkt->data.frame.flags & VPX_FRAME_IS_INVISIBLE ? 0x08 : 0), 1);
        m_Ebml.write(a_pPkt->data.frame.buf, a_pPkt->data.frame.sz);
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
        if (m_bClusterOpen)
            m_Ebml.subEnd(Cluster);

        m_uCuePos = RTFileTell(m_File);
        m_Ebml.subStart(Cues);
        for (std::list<CueEntry>::iterator it = m_CueList.begin(); it != m_CueList.end(); ++it)
        {
          m_Ebml.subStart(CuePoint)
                .serializeUnsignedInteger(CueTime, it->time)
                .subStart(CueTrackPositions)
                .serializeUnsignedInteger(CueTrack, 1)
                .serializeUnsignedInteger(CueClusterPosition, it->loc - m_uPositionReference, 8)
                .subEnd(CueTrackPositions)
                .subEnd(CuePoint);   
        }

        m_Ebml.subEnd(Cues)
              .subEnd(Segment);

        writeSeekInfo();

        int rc = RTFileSeek(m_File, m_uTrackIdPos, RTFILE_SEEK_BEGIN, NULL);
        if (!RT_SUCCESS(rc)) throw rc;

        m_Ebml.serializeUnsignedInteger(TrackUID, (m_bDebug ? 0xDEADBEEF : a_u64Hash), 4);

        rc = RTFileSeek(m_File, 0, RTFILE_SEEK_END, NULL);
        if (!RT_SUCCESS(rc)) throw rc;
    }
    catch(int rc)
    {
        return rc;
    }
    return VINF_SUCCESS;
}
