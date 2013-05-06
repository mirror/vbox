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
#include <iprt/mem.h>
#include <iprt/string.h>
#include <VBox/log.h>


int Ebml_Write(EbmlGlobal *glob, const void *buffer_in, uint64_t len)
{
    return RTFileWrite(glob->file, buffer_in, len, NULL);
}

int Ebml_Serialize(EbmlGlobal *glob, const void *buffer_in, uint64_t len)
{
    const unsigned char *q = (const unsigned char *)buffer_in + len - 1;

    for (; len; len--)
    {
        int rc = Ebml_Write(glob, q--, 1);
        if (RT_FAILURE(rc))
            return rc;
    }
    return VINF_SUCCESS;
}

int Ebml_SerializeUnsigned32(EbmlGlobal *glob, uint32_t class_id, uint64_t ui)
{
    unsigned char sizeSerialized = 4 | 0x80;
    int rc = Ebml_WriteID(glob, class_id);
    rc = Ebml_Serialize(glob, &sizeSerialized, 1);
    if (RT_SUCCESS(rc))
        rc = Ebml_Serialize(glob, &ui, 4);
    return rc;
}

int Ebml_StartSubElement(EbmlGlobal *glob, uint64_t *ebmlLoc, uint32_t class_id)
{
    // todo this is always taking 8 bytes, this may need later optimization
    // this is a key that says lenght unknown
    uint64_t unknownLen =  UINT64_C(0x01FFFFFFFFFFFFFF);

    Ebml_WriteID(glob, class_id);
    *ebmlLoc = RTFileTell(glob->file);
    return Ebml_Serialize(glob, &unknownLen, 8);
}

int Ebml_EndSubElement(EbmlGlobal *glob, uint64_t *ebmlLoc)
{
    /* Save the current file pointer */
    uint64_t pos = RTFileTell(glob->file);

    /* Calculate the size of this element */
    uint64_t size  = pos - *ebmlLoc - 8;
    size |= UINT64_C(0x0100000000000000);

    /* Seek back to the beginning of the element and write the new size */
    RTFileSeek(glob->file, *ebmlLoc, RTFILE_SEEK_BEGIN, NULL);
    int rc = Ebml_Serialize(glob, &size, 8);

    /* Reset the file pointer */
    RTFileSeek(glob->file, pos, RTFILE_SEEK_BEGIN, NULL);

    return rc;
}

int Ebml_WriteLen(EbmlGlobal *glob, uint64_t val)
{
    //TODO check and make sure we are not > than 0x0100000000000000LLU
    unsigned char size = 8; //size in bytes to output
    uint64_t minVal = UINT64_C(0x00000000000000ff); //mask to compare for byte size

    for (size = 1; size < 8; size ++)
    {
        if (val < minVal)
            break;

        minVal = (minVal << 7);
    }

    val |= (UINT64_C(0x000000000000080) << ((size - 1) * 7));

    return Ebml_Serialize(glob, (void *) &val, size);
}

int Ebml_WriteString(EbmlGlobal *glob, const char *str)
{
    const size_t size_ = strlen(str);
    const uint64_t size = size_;
    int rc = Ebml_WriteLen(glob, size);
    //TODO: it's not clear from the spec whether the nul terminator
    //should be serialized too.  For now we omit the null terminator.
    if (RT_SUCCESS(rc))
        rc = Ebml_Write(glob, str, size);
    return rc;
}

int Ebml_WriteID(EbmlGlobal *glob, uint32_t class_id)
{
    int rc;
    if (class_id >= 0x01000000)
        rc = Ebml_Serialize(glob, (void *)&class_id, 4);
    else if (class_id >= 0x00010000)
        rc = Ebml_Serialize(glob, (void *)&class_id, 3);
    else if (class_id >= 0x00000100)
        rc = Ebml_Serialize(glob, (void *)&class_id, 2);
    else
        rc = Ebml_Serialize(glob, (void *)&class_id, 1);
    return rc;
}
int Ebml_SerializeUnsigned64(EbmlGlobal *glob, uint32_t class_id, uint64_t ui)
{
    unsigned char sizeSerialized = 8 | 0x80;
    int rc = Ebml_WriteID(glob, class_id);
    if (RT_SUCCESS(rc))
        rc = Ebml_Serialize(glob, &sizeSerialized, 1);
    if (RT_SUCCESS(rc))
        rc = Ebml_Serialize(glob, &ui, 8);
    return rc;
}

int Ebml_SerializeUnsigned(EbmlGlobal *glob, uint32_t class_id, uint32_t ui)
{
    unsigned char size = 8; //size in bytes to output
    unsigned char sizeSerialized = 0;
    uint32_t minVal;

    int rc = Ebml_WriteID(glob, class_id);
    if (RT_FAILURE(rc))
        return rc;
    minVal = 0x7fLU; //mask to compare for byte size

    for (size = 1; size < 4; size ++)
    {
        if (ui < minVal)
            break;

        minVal <<= 7;
    }

    sizeSerialized = 0x80 | size;
    rc = Ebml_Serialize(glob, &sizeSerialized, 1);
    if (RT_SUCCESS(rc))
        rc = Ebml_Serialize(glob, &ui, size);
    return rc;
}

//TODO: perhaps this is a poor name for this id serializer helper function
int Ebml_SerializeBinary(EbmlGlobal *glob, uint32_t class_id, uint32_t bin)
{
    int size;
    for (size = 4; size > 1; size--)
    {
        if (bin & 0x000000ff << ((size-1) * 8))
            break;
    }
    int rc = Ebml_WriteID(glob, class_id);
    if (RT_SUCCESS(rc))
        rc = Ebml_WriteLen(glob, size);
    if (RT_SUCCESS(rc))
        rc = Ebml_WriteID(glob, bin);
    return rc;
}

int Ebml_SerializeFloat(EbmlGlobal *glob, uint32_t class_id, double d)
{
    unsigned char len = 0x88;

    int rc = Ebml_WriteID(glob, class_id);
    if (RT_SUCCESS(rc))
        rc = Ebml_Serialize(glob, &len, 1);
    if (RT_SUCCESS(rc))
        rc = Ebml_Serialize(glob,  &d, 8);
    return rc;
}

int Ebml_SerializeString(EbmlGlobal *glob, uint32_t class_id, const char *s)
{
    int rc = Ebml_WriteID(glob, class_id);
    if (RT_SUCCESS(rc))
        rc = Ebml_WriteString(glob, s);
    return rc;
}

int Ebml_WriteWebMSeekElement(EbmlGlobal *ebml, uint32_t id, uint64_t pos)
{
    uint64_t offset = pos - ebml->position_reference;
    uint64_t start;
    int rc = Ebml_StartSubElement(ebml, &start, Seek);
    if (RT_SUCCESS(rc))
        rc = Ebml_SerializeBinary(ebml, SeekID, id);
    if (RT_SUCCESS(rc))
        rc = Ebml_SerializeUnsigned64(ebml, SeekPosition, offset);
    if (RT_SUCCESS(rc))
        rc = Ebml_EndSubElement(ebml, &start);
    return rc;
}

int Ebml_WriteWebMSeekInfo(EbmlGlobal *ebml)
{
    uint64_t pos;
    int rc = VINF_SUCCESS;

    /* Save the current file pointer */
    pos = RTFileTell(ebml->file);

    if (ebml->seek_info_pos)
        rc = RTFileSeek(ebml->file, ebml->seek_info_pos, RTFILE_SEEK_BEGIN, NULL);
    else
        ebml->seek_info_pos = pos;

    {
        uint64_t start;

        if (RT_SUCCESS(rc))
            rc = Ebml_StartSubElement(ebml, &start, SeekHead);
        if (RT_SUCCESS(rc))
            rc = Ebml_WriteWebMSeekElement(ebml, Tracks, ebml->track_pos);
        if (RT_SUCCESS(rc))
            rc = Ebml_WriteWebMSeekElement(ebml, Cues,   ebml->cue_pos);
        if (RT_SUCCESS(rc))
            rc = Ebml_WriteWebMSeekElement(ebml, Info,   ebml->segment_info_pos);
        if (RT_SUCCESS(rc))
            rc = Ebml_EndSubElement(ebml, &start);
    }
    {
        //segment info
        uint64_t startInfo;
        uint64_t frame_time;

        frame_time = (uint64_t)1000 * ebml->framerate.den
                     / ebml->framerate.num;
        ebml->segment_info_pos = RTFileTell(ebml->file);
        if (RT_SUCCESS(rc))
            rc = Ebml_StartSubElement(ebml, &startInfo, Info);
        if (RT_SUCCESS(rc))
            rc = Ebml_SerializeUnsigned(ebml, TimecodeScale, 1000000);
        if (RT_SUCCESS(rc))
            rc = Ebml_SerializeFloat(ebml, Segment_Duration,
                                     (double)(ebml->last_pts_ms + frame_time));
        char szVersion[64];
        RTStrPrintf(szVersion, sizeof(szVersion), "vpxenc%",
                    ebml->debug ? vpx_codec_version_str() : "");
        if (RT_SUCCESS(rc))
            rc = Ebml_SerializeString(ebml, 0x4D80, szVersion);
        if (RT_SUCCESS(rc))
            rc = Ebml_SerializeString(ebml, 0x5741, szVersion);
        if (RT_SUCCESS(rc))
            rc = Ebml_EndSubElement(ebml, &startInfo);
    }
    return rc;
}

int Ebml_WriteWebMFileHeader(EbmlGlobal                *glob,
                             const vpx_codec_enc_cfg_t *cfg,
                             const struct vpx_rational *fps)
{
    int rc = VINF_SUCCESS;
    {
        uint64_t start;
        rc = Ebml_StartSubElement(glob, &start, EBML);
        if (RT_SUCCESS(rc))
            rc = Ebml_SerializeUnsigned(glob, EBMLVersion, 1);
        if (RT_SUCCESS(rc))
            rc = Ebml_SerializeUnsigned(glob, EBMLReadVersion, 1); //EBML Read Version
        if (RT_SUCCESS(rc))
            rc = Ebml_SerializeUnsigned(glob, EBMLMaxIDLength, 4); //EBML Max ID Length
        if (RT_SUCCESS(rc))
            rc = Ebml_SerializeUnsigned(glob, EBMLMaxSizeLength, 8); //EBML Max Size Length
        if (RT_SUCCESS(rc))
            rc = Ebml_SerializeString(glob, DocType, "webm"); //Doc Type
        if (RT_SUCCESS(rc))
            rc = Ebml_SerializeUnsigned(glob, DocTypeVersion, 2); //Doc Type Version
        if (RT_SUCCESS(rc))
            rc = Ebml_SerializeUnsigned(glob, DocTypeReadVersion, 2); //Doc Type Read Version
        if (RT_SUCCESS(rc))
            rc = Ebml_EndSubElement(glob, &start);
    }
    {
        if (RT_SUCCESS(rc))
            rc = Ebml_StartSubElement(glob, &glob->startSegment, Segment); //segment
        glob->position_reference = RTFileTell(glob->file);
        glob->framerate = *fps;
        if (RT_SUCCESS(rc))
            rc = Ebml_WriteWebMSeekInfo(glob);
        {
            uint64_t trackStart;
            glob->track_pos = RTFileTell(glob->file);
            if (RT_SUCCESS(rc))
                rc = Ebml_StartSubElement(glob, &trackStart, Tracks);
            {
                unsigned int trackNumber = 1;
                uint64_t     trackID = 0;

                uint64_t start;
                if (RT_SUCCESS(rc))
                    rc = Ebml_StartSubElement(glob, &start, TrackEntry);
                if (RT_SUCCESS(rc))
                    rc = Ebml_SerializeUnsigned(glob, TrackNumber, trackNumber);
                glob->track_id_pos = RTFileTell(glob->file);
                Ebml_SerializeUnsigned32(glob, TrackUID, trackID);
                Ebml_SerializeUnsigned(glob, TrackType, 1); //video is always 1
                Ebml_SerializeString(glob, CodecID, "V_VP8");
                {
                    unsigned int pixelWidth = cfg->g_w;
                    unsigned int pixelHeight = cfg->g_h;
                    double       frameRate   = (double)fps->num/(double)fps->den;

                    uint64_t videoStart;
                    if (RT_SUCCESS(rc))
                        rc = Ebml_StartSubElement(glob, &videoStart, Video);
                    if (RT_SUCCESS(rc))
                        rc = Ebml_SerializeUnsigned(glob, PixelWidth, pixelWidth);
                    if (RT_SUCCESS(rc))
                        rc = Ebml_SerializeUnsigned(glob, PixelHeight, pixelHeight);
                    if (RT_SUCCESS(rc))
                        rc = Ebml_SerializeFloat(glob, FrameRate, frameRate);
                    if (RT_SUCCESS(rc))
                        rc = Ebml_EndSubElement(glob, &videoStart); //Video
                }
                if (RT_SUCCESS(rc))
                    rc = Ebml_EndSubElement(glob, &start); //Track Entry
            }
            if (RT_SUCCESS(rc))
                rc = Ebml_EndSubElement(glob, &trackStart);
        }
        // segment element is open
    }
    return rc;
}

int Ebml_WriteWebMBlock(EbmlGlobal                *glob,
                        const vpx_codec_enc_cfg_t *cfg,
                        const vpx_codec_cx_pkt_t  *pkt)
{
    uint64_t       block_length;
    unsigned char  track_number;
    unsigned short block_timecode = 0;
    unsigned char  flags;
    int64_t        pts_ms;
    int            start_cluster = 0, is_keyframe;
    int            rc = VINF_SUCCESS;

    /* Calculate the PTS of this frame in milliseconds */
    pts_ms = pkt->data.frame.pts * 1000
             * (uint64_t)cfg->g_timebase.num / (uint64_t)cfg->g_timebase.den;
    if (pts_ms <= glob->last_pts_ms)
        pts_ms = glob->last_pts_ms + 1;
    glob->last_pts_ms = pts_ms;

    /* Calculate the relative time of this block */
    if (pts_ms - glob->cluster_timecode > 65536)
        start_cluster = 1;
    else
        block_timecode = (unsigned short)(pts_ms - glob->cluster_timecode);

    is_keyframe = (pkt->data.frame.flags & VPX_FRAME_IS_KEY);
    if (start_cluster || is_keyframe)
    {
        if (glob->cluster_open)
            rc = Ebml_EndSubElement(glob, &glob->startCluster);

        /* Open the new cluster */
        block_timecode = 0;
        glob->cluster_open = 1;
        glob->cluster_timecode = (uint32_t)pts_ms;
        glob->cluster_pos = RTFileTell(glob->file);
        if (RT_SUCCESS(rc))
            rc = Ebml_StartSubElement(glob, &glob->startCluster, Cluster); //cluster
        if (RT_SUCCESS(rc))
            rc = Ebml_SerializeUnsigned(glob, Timecode, glob->cluster_timecode);

        /* Save a cue point if this is a keyframe. */
        if (is_keyframe)
        {
            struct cue_entry *cue;

            glob->cue_list = (cue_entry*)RTMemRealloc(glob->cue_list, (glob->cues+1) * sizeof(cue_entry));
            cue = &glob->cue_list[glob->cues];
            cue->time = glob->cluster_timecode;
            cue->loc = glob->cluster_pos;
            glob->cues++;
        }
    }

    /* Write the Simple Block */
    if (RT_SUCCESS(rc))
        rc = Ebml_WriteID(glob, SimpleBlock);

    block_length = pkt->data.frame.sz + 4;
    block_length |= 0x10000000;
    if (RT_SUCCESS(rc))
        rc = Ebml_Serialize(glob, &block_length, 4);

    track_number = 1;
    track_number |= 0x80;
    if (RT_SUCCESS(rc))
        rc = Ebml_Write(glob, &track_number, 1);

    if (RT_SUCCESS(rc))
        rc = Ebml_Serialize(glob, &block_timecode, 2);

    flags = 0;
    if (is_keyframe)
        flags |= 0x80;
    if (pkt->data.frame.flags & VPX_FRAME_IS_INVISIBLE)
        flags |= 0x08;
    if (RT_SUCCESS(rc))
        rc = Ebml_Write(glob, &flags, 1);

    if (RT_SUCCESS(rc))
        rc = Ebml_Write(glob, pkt->data.frame.buf, pkt->data.frame.sz);

    return rc;
}


int Ebml_WriteWebMFileFooter(EbmlGlobal *glob, long hash)
{
    int rc = VINF_SUCCESS;
    if (glob->cluster_open)
        rc = Ebml_EndSubElement(glob, &glob->startCluster);

    {
        uint64_t start;
        unsigned int i;

        glob->cue_pos = RTFileTell(glob->file);
        if (RT_SUCCESS(rc))
            rc = Ebml_StartSubElement(glob, &start, Cues);
        for (i=0; i<glob->cues; i++)
        {
            struct cue_entry *cue = &glob->cue_list[i];
            uint64_t startSub;

            if (RT_SUCCESS(rc))
                rc = Ebml_StartSubElement(glob, &startSub, CuePoint);
            {
                uint64_t startSubsub;

                if (RT_SUCCESS(rc))
                    rc = Ebml_SerializeUnsigned(glob, CueTime, cue->time);
                if (RT_SUCCESS(rc))
                    rc = Ebml_StartSubElement(glob, &startSubsub, CueTrackPositions);
                if (RT_SUCCESS(rc))
                    rc = Ebml_SerializeUnsigned(glob, CueTrack, 1);
                if (RT_SUCCESS(rc))
                    rc = Ebml_SerializeUnsigned64(glob, CueClusterPosition,
                                                  cue->loc - glob->position_reference);
                //Ebml_SerializeUnsigned(glob, CueBlockNumber, cue->blockNumber);
                if (RT_SUCCESS(rc))
                    rc = Ebml_EndSubElement(glob, &startSubsub);
            }
            if (RT_SUCCESS(rc))
                rc = Ebml_EndSubElement(glob, &startSub);
        }
        if (RT_SUCCESS(rc))
            rc = Ebml_EndSubElement(glob, &start);
    }

    if (RT_SUCCESS(rc))
        rc = Ebml_EndSubElement(glob, &glob->startSegment);

    /* Patch up the seek info block */
    if (RT_SUCCESS(rc))
        rc = Ebml_WriteWebMSeekInfo(glob);

    /* Patch up the track id */
    if (RT_SUCCESS(rc))
        rc = RTFileSeek(glob->file, glob->track_id_pos, RTFILE_SEEK_BEGIN, NULL);
    if (RT_SUCCESS(rc))
        rc  = Ebml_SerializeUnsigned32(glob, TrackUID, glob->debug ? 0xDEADBEEF : hash);

    if (RT_SUCCESS(rc))
        rc = RTFileSeek(glob->file, 0, RTFILE_SEEK_END, NULL);
    return rc;
}
