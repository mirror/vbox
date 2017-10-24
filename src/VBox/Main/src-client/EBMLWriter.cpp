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

/**
 * For more information, see:
 * - https://w3c.github.io/media-source/webm-byte-stream-format.html
 * - https://www.webmproject.org/docs/container/#muxer-guidelines
 */

#define LOG_GROUP LOG_GROUP_MAIN_DISPLAY
#include "LoggingNew.h"

#include <list>
#include <map>
#include <queue>
#include <stack>

#include <math.h> /* For lround.h. */

#include <iprt/asm.h>
#include <iprt/buildconfig.h>
#include <iprt/cdefs.h>
#include <iprt/critsect.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/rand.h>
#include <iprt/string.h>

#include <VBox/log.h>
#include <VBox/version.h>

#include "EBMLWriter.h"
#include "EBML_MKV.h"

/** No flags set. */
#define VBOX_EBMLWRITER_FLAG_NONE               0
/** The file handle was inherited. */
#define VBOX_EBMLWRITER_FLAG_HANDLE_INHERITED   RT_BIT(0)

class EBMLWriter
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

    /** Stack of EBML sub elements. */
    std::stack<EbmlSubElement> m_Elements;
    /** The file's handle. */
    RTFILE                     m_hFile;
    /** The file's name (path). */
    Utf8Str                    m_strFile;
    /** Flags. */
    uint32_t                   m_fFlags;

public:

    EBMLWriter(void)
        : m_hFile(NIL_RTFILE)
        , m_fFlags(VBOX_EBMLWRITER_FLAG_NONE) { }

    virtual ~EBMLWriter(void) { close(); }

public:

    /** Creates an EBML output file using an existing, open file handle. */
    inline int createEx(const char *a_pszFile, PRTFILE phFile)
    {
        AssertPtrReturn(phFile, VERR_INVALID_POINTER);

        m_hFile   = *phFile;
        m_fFlags |= VBOX_EBMLWRITER_FLAG_HANDLE_INHERITED;
        m_strFile = a_pszFile;

        return VINF_SUCCESS;
    }

    /** Creates an EBML output file using a file name. */
    inline int create(const char *a_pszFile, uint64_t fOpen)
    {
        int rc = RTFileOpen(&m_hFile, a_pszFile, fOpen);
        if (RT_SUCCESS(rc))
            m_strFile = a_pszFile;

        return rc;
    }

    /** Returns the file name. */
    inline const Utf8Str& getFileName(void)
    {
        return m_strFile;
    }

    /** Returns file size. */
    inline uint64_t getFileSize(void)
    {
        return RTFileTell(m_hFile);
    }

    /** Get reference to file descriptor */
    inline const RTFILE &getFile(void)
    {
        return m_hFile;
    }

    /** Returns available space on storage. */
    inline uint64_t getAvailableSpace(void)
    {
        RTFOFF pcbFree;
        int rc = RTFileQueryFsSizes(m_hFile, NULL, &pcbFree, 0, 0);
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

        if (!(m_fFlags & VBOX_EBMLWRITER_FLAG_HANDLE_INHERITED))
        {
            RTFileClose(m_hFile);
            m_hFile = NIL_RTFILE;
        }

        m_fFlags  = VBOX_EBMLWRITER_FLAG_NONE;
        m_strFile = "";
    }

    /**
     * Returns whether the file is open or not.
     *
     * @returns True if open, false if not.
     */
    inline bool isOpen(void)
    {
        return RTFileIsValid(m_hFile);
    }

    /** Starts an EBML sub-element. */
    inline EBMLWriter &subStart(EbmlClassId classId)
    {
        writeClassId(classId);
        /* store the current file offset. */
        m_Elements.push(EbmlSubElement(RTFileTell(m_hFile), classId));
        /* Indicates that size of the element
         * is unkown (as according to EBML specs).
         */
        writeUnsignedInteger(UINT64_C(0x01FFFFFFFFFFFFFF));
        return *this;
    }

    /** Ends an EBML sub-element. */
    inline EBMLWriter &subEnd(EbmlClassId classId)
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

        uint64_t uPos = RTFileTell(m_hFile);
        uint64_t uSize = uPos - m_Elements.top().offset - 8;
        RTFileSeek(m_hFile, m_Elements.top().offset, RTFILE_SEEK_BEGIN, NULL);

        /* Make sure that size will be serialized as uint64_t. */
        writeUnsignedInteger(uSize | UINT64_C(0x0100000000000000));
        RTFileSeek(m_hFile, uPos, RTFILE_SEEK_BEGIN, NULL);
        m_Elements.pop();
        return *this;
    }

    /** Serializes a null-terminated string. */
    inline EBMLWriter &serializeString(EbmlClassId classId, const char *str)
    {
        writeClassId(classId);
        uint64_t size = strlen(str);
        writeSize(size);
        write(str, size);
        return *this;
    }

    /* Serializes an UNSIGNED integer
     * If size is zero then it will be detected automatically. */
    inline EBMLWriter &serializeUnsignedInteger(EbmlClassId classId, uint64_t parm, size_t size = 0)
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
    inline EBMLWriter &serializeFloat(EbmlClassId classId, float value)
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
    inline EBMLWriter &serializeData(EbmlClassId classId, const void *pvData, size_t cbData)
    {
        writeClassId(classId);
        writeSize(cbData);
        write(pvData, cbData);
        return *this;
    }

    /** Writes raw data to file. */
    inline int write(const void *data, size_t size)
    {
        return RTFileWrite(m_hFile, data, size, NULL);
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
    void operator=(const EBMLWriter &);

};

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

class WebMWriter_Impl
{
    /** Defines a WebM timecode. */
    typedef uint16_t WebMTimecode;

    /** Defines the WebM block flags data type. */
    typedef uint8_t  WebMBlockFlags;

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

    EBMLWriter                  m_Ebml;

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

    int init(void)
    {
        return CurSeg.init();
    }

    void destroy(void)
    {
        CurSeg.destroy();
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

        const uint8_t uTrack = (uint8_t)CurSeg.mapTracks.size();

        m_Ebml.subStart(MkvElem_TrackEntry);

        m_Ebml.serializeUnsignedInteger(MkvElem_TrackNumber, (uint8_t)uTrack);
        m_Ebml.serializeString         (MkvElem_Language,    "und" /* "Undefined"; see ISO-639-2. */);
        m_Ebml.serializeUnsignedInteger(MkvElem_FlagLacing,  (uint8_t)0);

        WebMTrack *pTrack = new WebMTrack(WebMTrackType_Audio, uTrack, RTFileTell(m_Ebml.getFile()));

        pTrack->Audio.uHz            = uHz;
        pTrack->Audio.msPerBlock     = 20; /** Opus uses 20ms by default. Make this configurable? */
        pTrack->Audio.framesPerBlock = uHz / (1000 /* s in ms */ / pTrack->Audio.msPerBlock);

        WEBMOPUSPRIVDATA opusPrivData(uHz, cChannels);

        LogFunc(("Opus @ %RU16Hz (%RU16ms + %RU16 frames per block)\n",
                 pTrack->Audio.uHz, pTrack->Audio.msPerBlock, pTrack->Audio.framesPerBlock));

        m_Ebml.serializeUnsignedInteger(MkvElem_TrackUID,     pTrack->uUUID, 4)
              .serializeUnsignedInteger(MkvElem_TrackType,    2 /* Audio */)
              .serializeString(MkvElem_CodecID,               "A_OPUS")
              .serializeData(MkvElem_CodecPrivate,            &opusPrivData, sizeof(opusPrivData))
              .serializeUnsignedInteger(MkvElem_CodecDelay,   0)
              .serializeUnsignedInteger(MkvElem_SeekPreRoll,  80 * 1000000) /* 80ms in ns. */
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
        RT_NOREF(dbFPS);

        const uint8_t uTrack = (uint8_t)CurSeg.mapTracks.size();

        m_Ebml.subStart(MkvElem_TrackEntry);

        m_Ebml.serializeUnsignedInteger(MkvElem_TrackNumber, (uint8_t)uTrack);
        m_Ebml.serializeString         (MkvElem_Language,    "und" /* "Undefined"; see ISO-639-2. */);
        m_Ebml.serializeUnsignedInteger(MkvElem_FlagLacing,  (uint8_t)0);

        WebMTrack *pTrack = new WebMTrack(WebMTrackType_Video, uTrack, RTFileTell(m_Ebml.getFile()));

        /** @todo Resolve codec type. */
        m_Ebml.serializeUnsignedInteger(MkvElem_TrackUID,    pTrack->uUUID /* UID */, 4)
              .serializeUnsignedInteger(MkvElem_TrackType,   1 /* Video */)
              .serializeString(MkvElem_CodecID,              "V_VP8")
              .subStart(MkvElem_Video)
                  .serializeUnsignedInteger(MkvElem_PixelWidth,  uWidth)
                  .serializeUnsignedInteger(MkvElem_PixelHeight, uHeight)
              .subEnd(MkvElem_Video);

        m_Ebml.subEnd(MkvElem_TrackEntry);

        CurSeg.mapTracks[uTrack] = pTrack;

        if (puTrack)
            *puTrack = uTrack;

        return VINF_SUCCESS;
#else
        RT_NOREF(uWidth, uHeight, dbFPS, puTrack);
        return VERR_NOT_SUPPORTED;
#endif
    }

    /**
     * Writes the WebM file header.
     *
     * @returns IPRT status code.
     */
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

    /**
     * Writes a simple block into the EBML structure.
     *
     * @returns IPRT status code.
     * @param   a_pTrack        Track the simple block is assigned to.
     * @param   a_pBlock        Simple block to write.
     */
    int writeSimpleBlockEBML(WebMTrack *a_pTrack, WebMSimpleBlock *a_pBlock)
    {
#ifdef LOG_ENABLED
        WebMCluster &Cluster = CurSeg.CurCluster;

        Log3Func(("[T%RU8C%RU64] Off=%RU64, PTS=%RU16, RelToClusterMs=%RU16, %zu bytes\n",
                  a_pTrack->uTrack, Cluster.uID, RTFileTell(m_Ebml.getFile()),
                  a_pBlock->Data.tcPTSMs, a_pBlock->Data.tcRelToClusterMs, a_pBlock->Data.cb));
#endif
        /*
         * Write a "Simple Block".
         */
        m_Ebml.writeClassId(MkvElem_SimpleBlock);
        /* Block size. */
        m_Ebml.writeUnsignedInteger(0x10000000u | (  1                 /* Track number size. */
                                                   + m_cbTimecode      /* Timecode size .*/
                                                   + 1                 /* Flags size. */
                                                   + a_pBlock->Data.cb /* Actual frame data size. */),  4);
        /* Track number. */
        m_Ebml.writeSize(a_pTrack->uTrack);
        /* Timecode (relative to cluster opening timecode). */
        m_Ebml.writeUnsignedInteger(a_pBlock->Data.tcRelToClusterMs, m_cbTimecode);
        /* Flags. */
        m_Ebml.writeUnsignedInteger(a_pBlock->Data.fFlags, 1);
        /* Frame data. */
        m_Ebml.write(a_pBlock->Data.pv, a_pBlock->Data.cb);

        return VINF_SUCCESS;
    }

    /**
     * Writes a simple block and enqueues it into the segment's render queue.
     *
     * @returns IPRT status code.
     * @param   a_pTrack        Track the simple block is assigned to.
     * @param   a_pBlock        Simple block to write and enqueue.
     */
    int writeSimpleBlockQueued(WebMTrack *a_pTrack, WebMSimpleBlock *a_pBlock)
    {
        RT_NOREF(a_pTrack);

        int rc = VINF_SUCCESS;

        try
        {
            const WebMTimecode tcMap = a_pBlock->Data.tcPTSMs;

            /* See if we already have an entry for the specified timecode in our queue. */
            WebMBlockMap::iterator itQueue = CurSeg.queueBlocks.Map.find(tcMap);
            if (itQueue != CurSeg.queueBlocks.Map.end()) /* Use existing queue. */
            {
                WebMTimecodeBlocks &Blocks = itQueue->second;
                Blocks.Enqueue(a_pBlock);
            }
            else /* Create a new timecode entry. */
            {
                WebMTimecodeBlocks Blocks;
                Blocks.Enqueue(a_pBlock);

                CurSeg.queueBlocks.Map[tcMap] = Blocks;
            }

            processQueues(&CurSeg.queueBlocks, false /* fForce */);
        }
        catch(...)
        {
            delete a_pBlock;
            a_pBlock = NULL;

            rc = VERR_NO_MEMORY;
        }

        return rc;
    }

#ifdef VBOX_WITH_LIBVPX
    /**
     * Writes VPX (VP8 video) simple data block.
     *
     * @returns IPRT status code.
     * @param   a_pTrack        Track ID to write data to.
     * @param   a_pCfg          VPX encoder configuration to use.
     * @param   a_pPkt          VPX packet video data packet to write.
     */
    int writeSimpleBlockVP8(WebMTrack *a_pTrack, const vpx_codec_enc_cfg_t *a_pCfg, const vpx_codec_cx_pkt_t *a_pPkt)
    {
        RT_NOREF(a_pTrack);

        /* Calculate the PTS of this frame (in ms). */
        WebMTimecode tcPTSMs = a_pPkt->data.frame.pts * 1000
                             * (uint64_t) a_pCfg->g_timebase.num / a_pCfg->g_timebase.den;

        if (   tcPTSMs
            && tcPTSMs <= a_pTrack->tcLastWrittenMs)
        {
            tcPTSMs = a_pTrack->tcLastWrittenMs + 1;
        }

        const bool fKeyframe = RT_BOOL(a_pPkt->data.frame.flags & VPX_FRAME_IS_KEY);

        uint8_t fFlags = VBOX_WEBM_BLOCK_FLAG_NONE;
        if (fKeyframe)
            fFlags |= VBOX_WEBM_BLOCK_FLAG_KEY_FRAME;
        if (a_pPkt->data.frame.flags & VPX_FRAME_IS_INVISIBLE)
            fFlags |= VBOX_WEBM_BLOCK_FLAG_INVISIBLE;

        return writeSimpleBlockQueued(a_pTrack,
                                      new WebMSimpleBlock(a_pTrack,
                                                          tcPTSMs, a_pPkt->data.frame.buf, a_pPkt->data.frame.sz, fFlags));
    }
#endif /* VBOX_WITH_LIBVPX */

#ifdef VBOX_WITH_LIBOPUS
    /**
     * Writes an Opus (audio) simple data block.
     *
     * @returns IPRT status code.
     * @param   a_pTrack        Track ID to write data to.
     * @param   pvData          Pointer to simple data block to write.
     * @param   cbData          Size (in bytes) of simple data block to write.
     * @param   uPTSMs          PTS of simple data block.
     *
     * @remarks Audio blocks that have same absolute timecode as video blocks SHOULD be written before the video blocks.
     */
    int writeSimpleBlockOpus(WebMTrack *a_pTrack, const void *pvData, size_t cbData, WebMTimecode uPTSMs)
    {
        AssertPtrReturn(a_pTrack, VERR_INVALID_POINTER);
        AssertPtrReturn(pvData,   VERR_INVALID_POINTER);
        AssertReturn(cbData,      VERR_INVALID_PARAMETER);

        /* Every Opus frame is a key frame. */
        const uint8_t fFlags = VBOX_WEBM_BLOCK_FLAG_KEY_FRAME;

        return writeSimpleBlockQueued(a_pTrack,
                                      new WebMSimpleBlock(a_pTrack,
                                                          uPTSMs, pvData, cbData, fFlags));
    }
#endif /* VBOX_WITH_LIBOPUS */

    /**
     * Writes a data block to the specified track.
     *
     * @returns IPRT status code.
     * @param   uTrack          Track ID to write data to.
     * @param   pvData          Pointer to data block to write.
     * @param   cbData          Size (in bytes) of data block to write.
     */
    int WriteBlock(uint8_t uTrack, const void *pvData, size_t cbData)
    {
        RT_NOREF(cbData); /* Only needed for assertions for now. */

        int rc = RTCritSectEnter(&CurSeg.CritSect);
        AssertRC(rc);

        WebMTracks::iterator itTrack = CurSeg.mapTracks.find(uTrack);
        if (itTrack == CurSeg.mapTracks.end())
        {
            RTCritSectLeave(&CurSeg.CritSect);
            return VERR_NOT_FOUND;
        }

        WebMTrack *pTrack = itTrack->second;
        AssertPtr(pTrack);

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
                    rc = writeSimpleBlockOpus(pTrack, pData->pvData, pData->cbData, pData->uPTSMs);
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
                    rc = writeSimpleBlockVP8(pTrack, pData->pCfg, pData->pPkt);
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

        int rc2 = RTCritSectLeave(&CurSeg.CritSect);
        AssertRC(rc2);

        return rc;
    }

    /**
     * Processes a render queue.
     *
     * @returns IPRT status code.
     * @param   pQueue          Queue to process.
     * @param   fForce          Whether forcing to process the render queue or not.
     *                          Needed to drain the queues when terminating.
     */
    int processQueues(WebMQueue *pQueue, bool fForce)
    {
        if (pQueue->tslastProcessedMs == 0)
            pQueue->tslastProcessedMs = RTTimeMilliTS();

        if (!fForce)
        {
            /* Only process when we reached a certain threshold. */
            if (RTTimeMilliTS() - pQueue->tslastProcessedMs < 5000 /* ms */ /** @todo Make this configurable */)
                return VINF_SUCCESS;
        }

        WebMCluster &Cluster = CurSeg.CurCluster;

        /* Iterate through the block map. */
        WebMBlockMap::iterator it = pQueue->Map.begin();
        while (it != CurSeg.queueBlocks.Map.end())
        {
            WebMTimecode       mapTC     = it->first;
            RT_NOREF(mapTC);
            WebMTimecodeBlocks mapBlocks = it->second;

            /* Whether to start a new cluster or not. */
            bool fClusterStart = false;

            /* No blocks written yet? Start a new cluster. */
            if (Cluster.cBlocks == 0)
                fClusterStart = true;

            /* Did we reach the maximum a cluster can hold? Use a new cluster then. */
            if (mapTC - Cluster.tcStartMs > VBOX_WEBM_CLUSTER_MAX_LEN_MS)
                fClusterStart = true;

            /* If the block map indicates that a cluster is needed for this timecode, create one. */
            if (mapBlocks.fClusterNeeded)
                fClusterStart = true;

            if (   fClusterStart
                && !mapBlocks.fClusterStarted)
            {
                if (Cluster.fOpen) /* Close current cluster first. */
                {
                    /* Make sure that the current cluster contained some data.  */
                    Assert(Cluster.offStart);
                    Assert(Cluster.cBlocks);

                    m_Ebml.subEnd(MkvElem_Cluster);
                    Cluster.fOpen = false;
                }

                Cluster.fOpen     = true;
                Cluster.uID       = CurSeg.cClusters;
                Cluster.tcStartMs = mapTC;
                Cluster.offStart  = RTFileTell(m_Ebml.getFile());
                Cluster.cBlocks   = 0;

                if (CurSeg.cClusters)
                    AssertMsg(Cluster.tcStartMs, ("[C%RU64] @ %RU64 starting timecode is 0 which is invalid\n",
                                                  Cluster.uID, Cluster.offStart));

                Log2Func(("[C%RU64] Start @ %RU64ms (map TC is %RU64) / %RU64 bytes\n",
                          Cluster.uID, Cluster.tcStartMs, mapTC, Cluster.offStart));

                m_Ebml.subStart(MkvElem_Cluster)
                      .serializeUnsignedInteger(MkvElem_Timecode, Cluster.tcStartMs);

                CurSeg.cClusters++;

                mapBlocks.fClusterStarted = true;
            }

            /* Iterate through all blocks related to the current timecode. */
            while (!mapBlocks.Queue.empty())
            {
                WebMSimpleBlock *pBlock = mapBlocks.Queue.front();
                AssertPtr(pBlock);

                WebMTrack       *pTrack = pBlock->pTrack;
                AssertPtr(pTrack);

                /* Calculate the block's relative time code to the current cluster's starting time code. */
                Assert(pBlock->Data.tcPTSMs >= Cluster.tcStartMs);
                pBlock->Data.tcRelToClusterMs = pBlock->Data.tcPTSMs - Cluster.tcStartMs;

                int rc2 = writeSimpleBlockEBML(pTrack, pBlock);
                AssertRC(rc2);

                Cluster.cBlocks++;

                pTrack->cTotalBlocks++;
                pTrack->tcLastWrittenMs = pBlock->Data.tcPTSMs;

                if (CurSeg.tcLastWrittenMs < pTrack->tcLastWrittenMs)
                    CurSeg.tcLastWrittenMs = pTrack->tcLastWrittenMs;

                /* Save a cue point if this is a keyframe. */
                if (pBlock->Data.fFlags & VBOX_WEBM_BLOCK_FLAG_KEY_FRAME)
                {
                    WebMCuePoint cue(pBlock->pTrack, Cluster.offStart, pBlock->Data.tcPTSMs);
                    CurSeg.lstCues.push_back(cue);
                }

                delete pBlock;
                pBlock = NULL;

                mapBlocks.Queue.pop();
            }

            Assert(mapBlocks.Queue.empty());

            CurSeg.queueBlocks.Map.erase(it);

            it = CurSeg.queueBlocks.Map.begin();
        }

        Assert(CurSeg.queueBlocks.Map.empty());

        pQueue->tslastProcessedMs = RTTimeMilliTS();

        return VINF_SUCCESS;
    }

    /**
     * Writes the WebM footer.
     *
     * @returns IPRT status code.
     */
    int writeFooter(void)
    {
        AssertReturn(m_Ebml.isOpen(), VERR_WRONG_ORDER);

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

        std::list<WebMCuePoint>::iterator itCuePoint = CurSeg.lstCues.begin();
        while (itCuePoint != CurSeg.lstCues.end())
        {
            /* Sanity. */
            AssertPtr(itCuePoint->pTrack);

            LogFunc(("CuePoint @ %RU64: Track #%RU8 (Cluster @ %RU64, TC %RU64)\n",
                     RTFileTell(m_Ebml.getFile()), itCuePoint->pTrack->uTrack,
                     itCuePoint->offCluster, itCuePoint->tcAbs));

            m_Ebml.subStart(MkvElem_CuePoint)
                      .serializeUnsignedInteger(MkvElem_CueTime,                itCuePoint->tcAbs)
                      .subStart(MkvElem_CueTrackPositions)
                          .serializeUnsignedInteger(MkvElem_CueTrack,           itCuePoint->pTrack->uTrack)
                          .serializeUnsignedInteger(MkvElem_CueClusterPosition, itCuePoint->offCluster, 8)
                  .subEnd(MkvElem_CueTrackPositions)
                  .subEnd(MkvElem_CuePoint);

            itCuePoint++;
        }

        m_Ebml.subEnd(MkvElem_Cues);
        m_Ebml.subEnd(MkvElem_Segment);

        /*
         * Re-Update SeekHead / Info elements.
         */

        writeSegSeekInfo();

        return RTFileSeek(m_Ebml.getFile(), 0, RTFILE_SEEK_END, NULL);
    }

    /**
     * Closes the WebM file and drains all queues.
     *
     * @returns IPRT status code.
     */
    int close(void)
    {
        if (!m_Ebml.isOpen())
            return VINF_SUCCESS;

        LogFunc(("\n"));

        /* Make sure to drain all queues. */
        processQueues(&CurSeg.queueBlocks, true /* fForce */);

        writeFooter();

        WebMTracks::iterator itTrack = CurSeg.mapTracks.begin();
        for (; itTrack != CurSeg.mapTracks.end(); ++itTrack)
        {
            WebMTrack *pTrack = itTrack->second;

            delete pTrack;
            CurSeg.mapTracks.erase(itTrack);
        }

        Assert(CurSeg.queueBlocks.Map.size() == 0);
        Assert(CurSeg.mapTracks.size() == 0);

        m_Ebml.close();

        return VINF_SUCCESS;
    }

    friend class Ebml;
    friend class WebMWriter;

private:

    /**
     * Writes the segment's seek information and cue points.
     */
    void writeSegSeekInfo(void)
    {
        if (CurSeg.offSeekInfo)
            RTFileSeek(m_Ebml.getFile(), CurSeg.offSeekInfo, RTFILE_SEEK_BEGIN, NULL);
        else
            CurSeg.offSeekInfo = RTFileTell(m_Ebml.getFile());

        LogFunc(("SeekHead @ %RU64\n", CurSeg.offSeekInfo));

        m_Ebml.subStart(MkvElem_SeekHead);

        m_Ebml.subStart(MkvElem_Seek)
              .serializeUnsignedInteger(MkvElem_SeekID, MkvElem_Tracks)
              .serializeUnsignedInteger(MkvElem_SeekPosition, CurSeg.offTracks - CurSeg.offStart, 8)
              .subEnd(MkvElem_Seek);

        Assert(CurSeg.offCues - CurSeg.offStart > 0); /* Sanity. */

        m_Ebml.subStart(MkvElem_Seek)
              .serializeUnsignedInteger(MkvElem_SeekID, MkvElem_Cues)
              .serializeUnsignedInteger(MkvElem_SeekPosition, CurSeg.offCues - CurSeg.offStart, 8)
              .subEnd(MkvElem_Seek);

        m_Ebml.subStart(MkvElem_Seek)
              .serializeUnsignedInteger(MkvElem_SeekID, MkvElem_Info)
              .serializeUnsignedInteger(MkvElem_SeekPosition, CurSeg.offInfo - CurSeg.offStart, 8)
              .subEnd(MkvElem_Seek);

        m_Ebml.subEnd(MkvElem_SeekHead);

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

        const WebMTimecode tcDuration = CurSeg.tcLastWrittenMs - CurSeg.tcStartMs;

        if (!CurSeg.lstCues.empty())
        {
            LogFunc(("tcDuration=%RU64\n", tcDuration));
            AssertMsg(tcDuration, ("Segment seems to be empty\n"));
        }

        m_Ebml.subStart(MkvElem_Info)
              .serializeUnsignedInteger(MkvElem_TimecodeScale, CurSeg.uTimecodeScaleFactor)
              .serializeFloat(MkvElem_Segment_Duration, tcDuration)
              .serializeString(MkvElem_MuxingApp, szMux)
              .serializeString(MkvElem_WritingApp, szApp)
              .subEnd(MkvElem_Info);
    }
};

WebMWriter::WebMWriter(void)
    : m_pImpl(new WebMWriter_Impl()) {}

WebMWriter::~WebMWriter(void)
{
    if (m_pImpl)
    {
        m_pImpl->destroy();

        delete m_pImpl;
    }
}

int WebMWriter::OpenEx(const char *a_pszFilename, PRTFILE a_phFile,
                       WebMWriter::AudioCodec a_enmAudioCodec, WebMWriter::VideoCodec a_enmVideoCodec)
{
    try
    {
        m_pImpl->m_enmAudioCodec = a_enmAudioCodec;
        m_pImpl->m_enmVideoCodec = a_enmVideoCodec;

        LogFunc(("Creating '%s'\n", a_pszFilename));

        int rc = m_pImpl->m_Ebml.createEx(a_pszFilename, a_phFile);
        if (RT_SUCCESS(rc))
        {
            rc = m_pImpl->init();
            if (RT_SUCCESS(rc))
                rc = m_pImpl->writeHeader();
        }
    }
    catch(int rc)
    {
        return rc;
    }
    return VINF_SUCCESS;
}

int WebMWriter::Open(const char *a_pszFilename, uint64_t a_fOpen,
                     WebMWriter::AudioCodec a_enmAudioCodec, WebMWriter::VideoCodec a_enmVideoCodec)
{
    try
    {
        m_pImpl->m_enmAudioCodec = a_enmAudioCodec;
        m_pImpl->m_enmVideoCodec = a_enmVideoCodec;

        LogFunc(("Creating '%s'\n", a_pszFilename));

        int rc = m_pImpl->m_Ebml.create(a_pszFilename, a_fOpen);
        if (RT_SUCCESS(rc))
        {
            rc = m_pImpl->init();
            if (RT_SUCCESS(rc))
                rc = m_pImpl->writeHeader();
        }
    }
    catch(int rc)
    {
        return rc;
    }
    return VINF_SUCCESS;
}

int WebMWriter::Close(void)
{
    return m_pImpl->close();
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

const Utf8Str& WebMWriter::GetFileName(void)
{
    return m_pImpl->m_Ebml.getFileName();
}

uint64_t WebMWriter::GetFileSize(void)
{
    return m_pImpl->m_Ebml.getFileSize();
}

uint64_t WebMWriter::GetAvailableSpace(void)
{
    return m_pImpl->m_Ebml.getAvailableSpace();
}

