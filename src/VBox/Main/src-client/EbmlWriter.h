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
#include <iprt/mem.h>
#include <iprt/string.h>
#include <VBox/log.h>
#include <vpx/vpx_encoder.h>

#include <stack>
#include <list>
#include <limits>

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
    template<EbmlClassId, typename T>
    struct Var
    {
        T value;
        explicit Var(const T &v) : value(v) {}
    };

    template<EbmlClassId classId> struct SignedInteger : Var<classId, int64_t> { SignedInteger(int64_t v) : Var<classId, int64_t>(v) {} };
    template<EbmlClassId classId> struct UnsignedInteger : Var<classId, uint64_t> { UnsignedInteger(int64_t v) : Var<classId, uint64_t>(v) {} };
    template<EbmlClassId classId> struct Float : Var<classId, double> { Float(double v) : Var<classId, double>(v) {}};
    template<EbmlClassId classId> struct String : Var<classId, const char *> { String(const char *v) : Var<classId, const char *>(v) {}};
    /* Master-element block */
    template<EbmlClassId classId> struct SubStart {};
    /* End of master-element block, a pseudo type */
    template<EbmlClassId classId> struct SubEnd {};
    /* Not an EBML type, used for optimization purposes */
    template<EbmlClassId classId, EbmlClassId id> struct Const {};

    Ebml();

    void init(const RTFILE &a_File);

    template<EbmlClassId classId>
    inline Ebml &operator<<(const SubStart<classId> &dummy)
    {
        serializeConst<classId>();
        m_Elements.push(EbmlSubElement(RTFileTell(m_File), classId));
        write<uint64_t>(UINT64_C(0x01FFFFFFFFFFFFFF));
        return *this;
    }

    template<EbmlClassId classId>
    inline Ebml &operator<<(const SubEnd<classId> &dummy)
    {
        if(m_Elements.empty() || m_Elements.top().classId != classId) throw VERR_INTERNAL_ERROR;

        uint64_t uPos = RTFileTell(m_File);
        uint64_t uSize = uPos - m_Elements.top().offset - 8;
        RTFileSeek(m_File, m_Elements.top().offset, RTFILE_SEEK_BEGIN, NULL);

        // make sure that size will be serialized as uint64
        write<uint64_t>(uSize | UINT64_C(0x0100000000000000));
        RTFileSeek(m_File, uPos, RTFILE_SEEK_BEGIN, NULL);
        m_Elements.pop();
        return *this;
    }

    template<EbmlClassId classId, typename T>
    inline Ebml &operator<<(const Var<classId, T> &value)
    {
        serializeConst<classId>();
        serializeConstEbml<sizeof(T)>();
        write<T>(value.value);
        return *this;
    }

    template<EbmlClassId classId>
    inline Ebml &operator<<(const String<classId> &str)
    {
        serializeConst<classId>();
        uint64_t size = strlen(str.value);
        serializeInteger(size);
        write(str.value, size);
        return *this;
    }

    template<EbmlClassId classId>
    inline Ebml &operator<<(const SignedInteger<classId> &parm)
    {
        serializeConst<classId>();
        size_t size = getSizeOfInt(parm.value);
        serializeInteger(size);
        int64_t value = RT_H2BE_U64(parm.value);
        write(reinterpret_cast<uint8_t*>(&value) + 8 - size, size);
        return *this;
    }

    template<EbmlClassId classId>
    inline Ebml &operator<<(const UnsignedInteger<classId> &parm)
    {
        serializeConst<classId>();
        size_t size = getSizeOfUInt(parm.value);
        serializeInteger(size);
        uint64_t value = RT_H2BE_U64(parm.value);
        write(reinterpret_cast<uint8_t*>(&value) + 8 - size, size);
        return *this;
    }

    template<EbmlClassId classId>
    inline Ebml &operator<<(const Float<classId> &parm)
    {
        return operator<<(Var<classId, uint64_t>(*reinterpret_cast<const uint64_t *>(&parm.value)));
    }

    template<EbmlClassId classId, EbmlClassId id>
    inline Ebml &operator<<(const Const<classId, id> &)
    {
        serializeConst<classId>();
        serializeConstEbml<SizeOfConst<id>::value>();
        serializeInteger(id);
        serializeConst<id>();
        return *this;
    }

    void write(const void *data, size_t size);

    template <typename T>
    void write(const T &data)
    {
        if (std::numeric_limits<T>::is_integer)
        {
            T tmp;
            switch(sizeof(T))
            {
                case 2:
                tmp = RT_H2BE_U16(data);
                break;
                case 4:
                tmp = RT_H2BE_U32(data);
                break;
                case 8:
                tmp = RT_H2BE_U64(data);
                break;
                default:
                tmp = data;
            }
            write(&tmp, sizeof(T));
        }
        else write(&data, sizeof(T));
    }

    template<uint64_t parm>
    inline void serializeConst()
    {
        static const uint64_t mask = RT_BIT_64(SizeOfConst<parm>::value * 8 - 1);
        uint64_t value = RT_H2BE_U64((parm &
                                    (((mask << 1) - 1) >> SizeOfConst<parm>::value)) |
                                    (mask >> (SizeOfConst<parm>::value - 1)));

        write(reinterpret_cast<uint8_t *>(&value) + (8 - SizeOfConst<parm>::value),
                    SizeOfConst<parm>::value);
    }

    template<uint64_t parm>
    inline void serializeConstEbml()
    {
        static const uint64_t mask = RT_BIT_64(SizeOfConst<parm>::ebmlValue * 8 - 1);
        uint64_t value = RT_H2BE_U64((parm &
                                    (((mask << 1) - 1) >> SizeOfConst<parm>::ebmlValue)) |
                                    (mask >> (SizeOfConst<parm>::ebmlValue - 1)));

        write(reinterpret_cast<uint8_t *>(&value) + (8 - SizeOfConst<parm>::ebmlValue),
                    SizeOfConst<parm>::ebmlValue);
    }

    inline void serializeInteger(uint64_t parm)
    {
        const size_t size = 8 - ! (parm & (UINT64_MAX << 49)) - ! (parm & (UINT64_MAX << 42)) -
                          ! (parm & (UINT64_MAX << 35)) - ! (parm & (UINT64_MAX << 28)) -
                          ! (parm & (UINT64_MAX << 21)) - ! (parm & (UINT64_MAX << 14)) -
                          ! (parm & (UINT64_MAX << 7));

        uint64_t mask = RT_BIT_64(size * 8 - 1);
        uint64_t value = RT_H2BE_U64((parm &
                                    (((mask << 1) - 1) >> size)) |
                                    (mask >> (size - 1)));

        write(reinterpret_cast<uint8_t *>(&value) + (8 - size),
                    size);
    }

    /* runtime calculation */
    static inline size_t getSizeOfUInt(uint64_t arg)
    {
        return 8 - ! (arg & (UINT64_MAX << 56)) - ! (arg & (UINT64_MAX << 48)) -
                   ! (arg & (UINT64_MAX << 40)) - ! (arg & (UINT64_MAX << 32)) -
                   ! (arg & (UINT64_MAX << 24)) - ! (arg & (UINT64_MAX << 16)) -
                   ! (arg & (UINT64_MAX << 8));
    }

    static inline size_t getSizeOfInt(int64_t arg)
    {
        return 8 - ! (arg & (INT64_C(-1) << 56)) - ! (~arg & (INT64_C(-1) << 56)) - 
                   ! (arg & (INT64_C(-1) << 48)) - ! (~arg & (INT64_C(-1) << 48)) -
                   ! (arg & (INT64_C(-1) << 40)) - ! (~arg & (INT64_C(-1) << 40)) -
                   ! (arg & (INT64_C(-1) << 32)) - ! (~arg & (INT64_C(-1) << 32)) -
                   ! (arg & (INT64_C(-1) << 24)) - ! (~arg & (INT64_C(-1) << 24)) -
                   ! (arg & (INT64_C(-1) << 16)) - ! (~arg & (INT64_C(-1) << 16)) -
                   ! (arg & (INT64_C(-1) << 8))  - ! (~arg & (INT64_C(-1) << 8));
    }


    /* Compile-time calculation for constants */
    template<uint64_t arg>
    struct SizeOfConst
    {
        static const size_t value = 8 - 
                   ! (arg & (UINT64_MAX << 56)) - ! (arg & (UINT64_MAX << 48)) -
                   ! (arg & (UINT64_MAX << 40)) - ! (arg & (UINT64_MAX << 32)) -
                   ! (arg & (UINT64_MAX << 24)) - ! (arg & (UINT64_MAX << 16)) -
                   ! (arg & (UINT64_MAX << 8));

        static const size_t ebmlValue = 8 -
                   ! (arg & (UINT64_MAX << 49)) - ! (arg & (UINT64_MAX << 42)) -
                   ! (arg & (UINT64_MAX << 35)) - ! (arg & (UINT64_MAX << 28)) -
                   ! (arg & (UINT64_MAX << 21)) - ! (arg & (UINT64_MAX << 14)) -
                   ! (arg & (UINT64_MAX << 7));
    };

private:
    Ebml(const Ebml &);
    void operator=(const Ebml &);

};

class WebMWriter
{

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

    void writeSeekInfo();

    Ebml m_Ebml;
    RTFILE m_File;

public:
    WebMWriter();
    int create(const char *a_pszFilename);
    void close();
    int writeHeader(const vpx_codec_enc_cfg_t *a_pCfg, const vpx_rational *a_pFps);
    int writeBlock(const vpx_codec_enc_cfg_t *a_pCfg, const vpx_codec_cx_pkt_t *a_pPkt);
    int writeFooter(uint32_t a_u64Hash);
    uint64_t getFileSize();
    uint64_t getAvailableSpace();

    struct SimpleBlockData
    {
        uint32_t    trackNumber;
        int16_t     timeCode;
        uint8_t     flags;
        const void *data;
        size_t      dataSize;

        SimpleBlockData(uint32_t tn, int16_t tc, uint8_t f, const void *d, size_t ds) : 
            trackNumber(tn),
            timeCode(tc),
            flags(f),
            data(d),
            dataSize(ds) {}
    };

private:
    void operator=(const WebMWriter &);
    WebMWriter(const WebMWriter &);
    friend Ebml &operator<<(Ebml &a_Ebml, const SimpleBlockData &a_Data);
};

#endif
