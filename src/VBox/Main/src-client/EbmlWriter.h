/* $Id$ */
/** @file
 * EbmlWriter.h - EBML writer + WebM container.
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
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ____EBMLWRITER
#define ____EBMLWRITER

#include <iprt/file.h>
#include <iprt/cdefs.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <vpx/vpx_encoder.h>

#include <stack>
#include <list>

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

    /* Initialize EBML object */
    inline void init(const RTFILE &a_File) { m_File = a_File; }

    /* Starts an EBML sub-element */
    inline Ebml &subStart(EbmlClassId classId)
    {
        writeClassId(classId);
        /* store the current file offset */
        m_Elements.push(EbmlSubElement(RTFileTell(m_File), classId));
        /* Indicates that size of the element
         * is unkown (as according to EBML specs)
         */
        writeUnsignedInteger(UINT64_C(0x01FFFFFFFFFFFFFF));
        return *this;
    }

    /* Ends an EBML sub-element */
    inline Ebml &subEnd(EbmlClassId classId)
    {
        /* Class ID on the top of the stack should match the class ID passed
         * to the function. Otherwise it may mean that we have a bug in the code
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

    /* Serializes a null-terminated string */
    inline Ebml &serializeString(EbmlClassId classId, const char *str)
    {
        writeClassId(classId);
        uint64_t size = strlen(str);
        writeSize(size);
        write(str, size);
        return *this;
    }

    /* Serializes an UNSIGNED integer 
     * If size is zero then it will be detected automatically */
    inline Ebml &serializeUnsignedInteger(EbmlClassId classId, uint64_t parm, size_t size = 0)
    {
        writeClassId(classId);
        if (!size) size = getSizeOfUInt(parm);
        writeSize(size);
        writeUnsignedInteger(parm, size);
        return *this;
    }

    /* Serializes a floating point value
     * Only 8-bytes double precision values are supported
     * by this function
     */
    inline Ebml &serializeFloat(EbmlClassId classId, double value)
    {
        writeClassId(classId);
        writeSize(sizeof(double));
        writeUnsignedInteger(*reinterpret_cast<uint64_t*>(&value));
        return *this;
    }

    /* Writes raw data to file */
    inline void write(const void *data, size_t size)
    {
        int rc = RTFileWrite(m_File, data, size, NULL);
        if (!RT_SUCCESS(rc)) throw rc;
    }

    /* Writes an unsigned integer of variable of fixed size */
    inline void writeUnsignedInteger(uint64_t value, size_t size = sizeof(uint64_t))
    {
        /* Convert to big-endian */
        value = RT_H2BE_U64(value);
        write(reinterpret_cast<uint8_t*>(&value) + sizeof(value) - size, size);
    }

    /* Writes EBML class ID to file
     * EBML ID already has a UTF8-like represenation
     * so getSizeOfUInt is used to determine
     * the number of its bytes
     */
    inline void writeClassId(EbmlClassId parm)
    {
        writeUnsignedInteger(parm, getSizeOfUInt(parm));
    }

    /* Writes data size value */
    inline void writeSize(uint64_t parm)
    {
        /* The following expression defines the size of the value that will be serialized
         * as an EBML UTF-8 like integer (with trailing bits represeting its size)
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
        /* One is subtracted in order to avoid loosing significant bit when size = 8 */
        uint64_t mask = RT_BIT_64(size * 8 - 1);
        writeUnsignedInteger((parm & (((mask << 1) - 1) >> size)) | (mask >> (size - 1)), size);
    }
    /* Size calculation for variable size UNSIGNED integer.
     * The function defines the size of the number by trimming
     * consequent trailing zero bytes starting from the most significant.
     * The following statement is always true:
     * 1 <= getSizeOfUInt(arg) <= 8
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

class WebMWriter
{
    /* Matroska EBML Class IDs supported by WebM */
    enum Mkv
    {
        EBML = 0x1A45DFA3,
        EBMLVersion = 0x4286,
        EBMLReadVersion = 0x42F7,
        EBMLMaxIDLength = 0x42F2,
        EBMLMaxSizeLength = 0x42F3,
        DocType = 0x4282,
        DocTypeVersion = 0x4287,
        DocTypeReadVersion = 0x4285,
    //  CRC_32 = 0xBF,
        Void = 0xEC,
        SignatureSlot = 0x1B538667,
        SignatureAlgo = 0x7E8A,
        SignatureHash = 0x7E9A,
        SignaturePublicKey = 0x7EA5,
        Signature = 0x7EB5,
        SignatureElements = 0x7E5B,
        SignatureElementList = 0x7E7B,
        SignedElement = 0x6532,
        //segment
        Segment = 0x18538067,
        //Meta Seek Information
        SeekHead = 0x114D9B74,
        Seek = 0x4DBB,
        SeekID = 0x53AB,
        SeekPosition = 0x53AC,
        //Segment Information
        Info = 0x1549A966,
    //  SegmentUID = 0x73A4,
    //  SegmentFilename = 0x7384,
    //  PrevUID = 0x3CB923,
    //  PrevFilename = 0x3C83AB,
    //  NextUID = 0x3EB923,
    //  NextFilename = 0x3E83BB,
    //  SegmentFamily = 0x4444,
    //  ChapterTranslate = 0x6924,
    //  ChapterTranslateEditionUID = 0x69FC,
    //  ChapterTranslateCodec = 0x69BF,
    //  ChapterTranslateID = 0x69A5,
        TimecodeScale = 0x2AD7B1,
        Segment_Duration = 0x4489,
        DateUTC = 0x4461,
    //  Title = 0x7BA9,
        MuxingApp = 0x4D80,
        WritingApp = 0x5741,
        //Cluster
        Cluster = 0x1F43B675,
        Timecode = 0xE7,
    //  SilentTracks = 0x5854,
    //  SilentTrackNumber = 0x58D7,
    //  Position = 0xA7,
        PrevSize = 0xAB,
        BlockGroup = 0xA0,
        Block = 0xA1,
    //  BlockVirtual = 0xA2,
    //  BlockAdditions = 0x75A1,
    //  BlockMore = 0xA6,
    //  BlockAddID = 0xEE,
    //  BlockAdditional = 0xA5,
        BlockDuration = 0x9B,
    //  ReferencePriority = 0xFA,
        ReferenceBlock = 0xFB,
    //  ReferenceVirtual = 0xFD,
    //  CodecState = 0xA4,
    //  Slices = 0x8E,
    //  TimeSlice = 0xE8,
        LaceNumber = 0xCC,
    //  FrameNumber = 0xCD,
    //  BlockAdditionID = 0xCB,
    //  MkvDelay = 0xCE,
    //  Cluster_Duration = 0xCF,
        SimpleBlock = 0xA3,
    //  EncryptedBlock = 0xAF,
        //Track
        Tracks = 0x1654AE6B,
        TrackEntry = 0xAE,
        TrackNumber = 0xD7,
        TrackUID = 0x73C5,
        TrackType = 0x83,
        FlagEnabled = 0xB9,
        FlagDefault = 0x88,
        FlagForced = 0x55AA,
        FlagLacing = 0x9C,
    //  MinCache = 0x6DE7,
    //  MaxCache = 0x6DF8,
        DefaultDuration = 0x23E383,
    //  TrackTimecodeScale = 0x23314F,
    //  TrackOffset = 0x537F,
    //  MaxBlockAdditionID = 0x55EE,
        Name = 0x536E,
        Language = 0x22B59C,
        CodecID = 0x86,
        CodecPrivate = 0x63A2,
        CodecName = 0x258688,
    //  AttachmentLink = 0x7446,
    //  CodecSettings = 0x3A9697,
    //  CodecInfoURL = 0x3B4040,
    //  CodecDownloadURL = 0x26B240,
    //  CodecDecodeAll = 0xAA,
    //  TrackOverlay = 0x6FAB,
    //  TrackTranslate = 0x6624,
    //  TrackTranslateEditionUID = 0x66FC,
    //  TrackTranslateCodec = 0x66BF,
    //  TrackTranslateTrackID = 0x66A5,
        //video
        Video = 0xE0,
        FlagInterlaced = 0x9A,
    //  StereoMode = 0x53B8,
        PixelWidth = 0xB0,
        PixelHeight = 0xBA,
        PixelCropBottom = 0x54AA,
        PixelCropTop = 0x54BB,
        PixelCropLeft = 0x54CC,
        PixelCropRight = 0x54DD,
        DisplayWidth = 0x54B0,
        DisplayHeight = 0x54BA,
        DisplayUnit = 0x54B2,
        AspectRatioType = 0x54B3,
    //  ColourSpace = 0x2EB524,
    //  GammaValue = 0x2FB523,
        FrameRate = 0x2383E3,
        //end video
        //audio
        Audio = 0xE1,
        SamplingFrequency = 0xB5,
        OutputSamplingFrequency = 0x78B5,
        Channels = 0x9F,
    //  ChannelPositions = 0x7D7B,
        BitDepth = 0x6264,
        //end audio
        //content encoding
    //  ContentEncodings = 0x6d80,
    //  ContentEncoding = 0x6240,
    //  ContentEncodingOrder = 0x5031,
    //  ContentEncodingScope = 0x5032,
    //  ContentEncodingType = 0x5033,
    //  ContentCompression = 0x5034,
    //  ContentCompAlgo = 0x4254,
    //  ContentCompSettings = 0x4255,
    //  ContentEncryption = 0x5035,
    //  ContentEncAlgo = 0x47e1,
    //  ContentEncKeyID = 0x47e2,
    //  ContentSignature = 0x47e3,
    //  ContentSigKeyID = 0x47e4,
    //  ContentSigAlgo = 0x47e5,
    //  ContentSigHashAlgo = 0x47e6,
        //end content encoding
        //Cueing Data
        Cues = 0x1C53BB6B,
        CuePoint = 0xBB,
        CueTime = 0xB3,
        CueTrackPositions = 0xB7,
        CueTrack = 0xF7,
        CueClusterPosition = 0xF1,
        CueBlockNumber = 0x5378
    //  CueCodecState = 0xEA,
    //  CueReference = 0xDB,
    //  CueRefTime = 0x96,
    //  CueRefCluster = 0x97,
    //  CueRefNumber = 0x535F,
    //  CueRefCodecState = 0xEB,
        //Attachment
    //  Attachments = 0x1941A469,
    //  AttachedFile = 0x61A7,
    //  FileDescription = 0x467E,
    //  FileName = 0x466E,
    //  FileMimeType = 0x4660,
    //  FileData = 0x465C,
    //  FileUID = 0x46AE,
    //  FileReferral = 0x4675,
        //Chapters
    //  Chapters = 0x1043A770,
    //  EditionEntry = 0x45B9,
    //  EditionUID = 0x45BC,
    //  EditionFlagHidden = 0x45BD,
    //  EditionFlagDefault = 0x45DB,
    //  EditionFlagOrdered = 0x45DD,
    //  ChapterAtom = 0xB6,
    //  ChapterUID = 0x73C4,
    //  ChapterTimeStart = 0x91,
    //  ChapterTimeEnd = 0x92,
    //  ChapterFlagHidden = 0x98,
    //  ChapterFlagEnabled = 0x4598,
    //  ChapterSegmentUID = 0x6E67,
    //  ChapterSegmentEditionUID = 0x6EBC,
    //  ChapterPhysicalEquiv = 0x63C3,
    //  ChapterTrack = 0x8F,
    //  ChapterTrackNumber = 0x89,
    //  ChapterDisplay = 0x80,
    //  ChapString = 0x85,
    //  ChapLanguage = 0x437C,
    //  ChapCountry = 0x437E,
    //  ChapProcess = 0x6944,
    //  ChapProcessCodecID = 0x6955,
    //  ChapProcessPrivate = 0x450D,
    //  ChapProcessCommand = 0x6911,
    //  ChapProcessTime = 0x6922,
    //  ChapProcessData = 0x6933,
        //Tagging
    //  Tags = 0x1254C367,
    //  Tag = 0x7373,
    //  Targets = 0x63C0,
    //  TargetTypeValue = 0x68CA,
    //  TargetType = 0x63CA,
    //  Tagging_TrackUID = 0x63C5,
    //  Tagging_EditionUID = 0x63C9,
    //  Tagging_ChapterUID = 0x63C4,
    //  AttachmentUID = 0x63C6,
    //  SimpleTag = 0x67C8,
    //  TagName = 0x45A3,
    //  TagLanguage = 0x447A,
    //  TagDefault = 0x4484,
    //  TagString = 0x4487,
    //  TagBinary = 0x4485,
    };

    struct CueEntry
    {
        uint32_t    time;
        uint64_t    loc;
        CueEntry(uint32_t t, uint64_t l) : time(t), loc(l) {}
    };

    bool            m_bDebug;
    int64_t         m_iLastPtsMs;
    int64_t         m_iInitialPtsMs;
    vpx_rational_t  m_Framerate;

    uint64_t        m_uPositionReference;
    uint64_t        m_uSeekInfoPos;
    uint64_t        m_uSegmentInfoPos;
    uint64_t        m_uTrackPos;
    uint64_t        m_uCuePos;
    uint64_t        m_uClusterPos;

    uint64_t        m_uTrackIdPos;

    uint64_t        m_uStartSegment;

    uint32_t        m_uClusterTimecode;
    bool            m_bClusterOpen;
    std::list<CueEntry> m_CueList;

    Ebml m_Ebml;
    RTFILE m_File;

public:
    WebMWriter();
    /* Creates output file */
    int create(const char *a_pszFilename);
    /* closes the file */
    void close();
    /* Writes WebM header to file
     * Should be called before any writeBlock call
     */
    int writeHeader(const vpx_codec_enc_cfg_t *a_pCfg, const vpx_rational *a_pFps);
    /* Writes a block of compressed data */
    int writeBlock(const vpx_codec_enc_cfg_t *a_pCfg, const vpx_codec_cx_pkt_t *a_pPkt);
    /* Writes WebM footer
     * No other write functions should be called after this one
     */
    int writeFooter(uint32_t a_u64Hash);
    /* Returns current output file size */
    uint64_t getFileSize();
    /* Returns current free storage space
     * available for the file
     */
    uint64_t getAvailableSpace();

private:
    void writeSeekInfo();
    void operator=(const WebMWriter &);
    WebMWriter(const WebMWriter &);
};

#endif
