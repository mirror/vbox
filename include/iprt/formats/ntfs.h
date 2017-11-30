/* $Id$ */
/** @file
 * IPRT, NT File System (NTFS).
 */

/*
 * Copyright (C) 2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___iprt_formats_ntfs_h
#define ___iprt_formats_ntfs_h

#include <iprt/formats/fat.h>


/** @defgroup grp_rt_formats_ntfs    NT File System (NTFS) structures and definitions
 * @ingroup grp_rt_formats
 * @{
 */

/** Value of the FATBOOTSECTOR::achOemName for an NTFS file system. */
#define NTFS_OEM_ID_MAGIC       "NTFS    "


/** @name NTFS_MFT_IDX_XXX - Predefined MFT indexes.
 * @{ */
#define NTFS_MFT_IDX_MFT            0   /**< The MFT itself. */
#define NTFS_MFT_IDX_MFT_MIRROR     1   /**< Mirror MFT (partial?). */
#define NTFS_MFT_IDX_LOG_FILE       2   /**< Journalling log. */
#define NTFS_MFT_IDX_VOLUME         3   /**< Volume attributes. */
#define NTFS_MFT_IDX_ATTRIB_DEF     4   /**< Attribute definitions. */
#define NTFS_MFT_IDX_ROOT           5   /**< The root directory. */
#define NTFS_MFT_IDX_BITMAP         6   /**< Allocation bitmap. */
#define NTFS_MFT_IDX_BOOT           7   /**< The boot sector. */
#define NTFS_MFT_IDX_BAD_CLUSTER    8   /**< Bad cluster table. */
#define NTFS_MFT_IDX_SECURITY       9   /**< Shared security descriptors (w2k and later). */
#define NTFS_MFT_IDX_UP_CASE        10  /**< Unicode upper case table. */
#define NTFS_MFT_IDX_EXTEND         11  /**< Directory containing further system files. */
#define NTFS_MFT_IDX_FIRST_USER     16  /**< The first user file. */
/** @} */

/**
 * NTFS MFT record reference.
 */
typedef union NTFSMFTREF
{
    /** unsigned 64-bit view. */
    uint64_t        u64;
    /** unsigned 32-bit view. */
    uint32_t        au32[2];
    /** unsigned 16-bit view. */
    uint16_t        au16[4];

    /** Structured view. */
    struct
    {
        /** Index of the master file table record. */
        uint64_t        idxMft : 48;
        /** MFT record reuse sequence number (for catching dangling references). */
        uint64_t        uRecReuseSeqNo : 16;
    } s;
} NTFSMFTREF;
AssertCompileSize(NTFSMFTREF, 8);
/** Pointer to a NTFS MFT record reference. */
typedef NTFSMFTREF *PNTFSMFTREF;
/** Pointer to a const NTFS MFT record reference. */
typedef NTFSMFTREF const *PCNTFSMFTREF;

/** @name NTFSMFTREF_GET_IDX
 * Gets the MFT index number from a MFT reference. */
/** @name NTFSMFTREF_GET_SEQ
 * Gets the MFT reuse sequence number from a MFT reference. */
/** @name NTFSMFTREF_SET_IDX
 * Sets the MFT index number of a MFT reference. */
/** @name NTFSMFTREF_SET_SEQ
 * Sets the MFT reuse sequence number of a MFT reference. */
/** @name NTFSMFTREF_SET
 * Sets the values of a MFT reference. */
#ifdef RT_LITTLE_ENDIAN
# define NTFSMFTREF_GET_IDX(a_pMftRef)              ((a_pMftRef)->s.idxMft)
# define NTFSMFTREF_GET_SEQ(a_pMftRef)              ((a_pMftRef)->s.uRecReuseSeqNo)
# define NTFSMFTREF_SET_SEQ(a_pMftRef, a_uValue)    do { (a_pMftRef)->s.uRecReuseSeqNo = (a_uValue); } while (0)
# define NTFSMFTREF_SET_IDX(a_pMftRef, a_uValue)    do { (a_pMftRef)->s.idxMft         = (a_uValue); } while (0)
# define NTFSMFTREF_SET(a_pMftRef, a_idx, a_uSeq)  \
    do { \
        (a_pMftRef)->s.idxMft         = (a_idx); \
        (a_pMftRef)->s.uRecReuseSeqNo = (a_uSeq); \
    } while (0)
#else
# define NTFSMFTREF_GET_IDX(a_pMftRef)              (RT_LE2H_U64((a_pMftRef)->u64) & UINT64_C(0x0000ffffffffffff))
# define NTFSMFTREF_GET_SEQ(a_pMftRef)              RT_LE2H_U16((uint16_t)(a_pMftRef)->u64)
# define NTFSMFTREF_SET_SEQ(a_pMftRef, a_uValue)    do { (a_pMftRef)->au16[3] = RT_H2LE_U16(a_uValue); } while (0)
# define NTFSMFTREF_SET_IDX(a_pMftRef, a_uValue)  \
    do { \
        (a_pMftRef)->au32[0] = RT_H2LE_U32((uint32_t)(a_uValue)); \
        (a_pMftRef)->au16[2] = RT_H2LE_U16((uint16_t)((a_uValue) >> 32)); \
    } while (0)
# define NTFSMFTREF_SET(a_pMftRef, a_idx, a_uSeq)  \
    do { \
        (a_pMftRef)->au32[0] = RT_H2LE_U32((uint32_t)(a_idx)); \
        (a_pMftRef)->au16[2] = RT_H2LE_U16((uint16_t)((a_idx) >> 32)); \
        (a_pMftRef)->au16[3] = RT_H2LE_U16((uint16_t)(a_uSeq)); \
    } while (0)
#endif


/**
 * NTFS record header.
 */
typedef struct NTFSRECHDR
{
    /** Magic number (usually ASCII). */
    uint32_t        uMagic;
    /** Offset of the update sequence array from the start of the record. */
    uint16_t        offUpdateSeqArray;
    /** Number of entries in the update sequence array. (uint16_t sized entries) */
    uint16_t        cUpdateSeqEntries;
} NTFSRECHDR;
AssertCompileSize(NTFSRECHDR, 8);
/** Pointer to a NTFS record header. */
typedef NTFSRECHDR *PNTFSRECHDR;
/** Pointer to a const NTFS record header. */
typedef NTFSRECHDR const *PCNTFSRECHDR;


/**
 * NTFS file record (in the MFT).
 */
typedef struct NTFSRECFILE
{
    /** 0x00: Header with NTFSREC_MAGIC_FILE. */
    NTFSRECHDR          Hdr;
    /** 0x08: Log file sequence number. */
    uint64_t            uLsn;
    /** 0x10: MFT record reuse sequence number (for dangling MFT references). */
    uint16_t            uRecReuseSeqNo;
    /** 0x12: Number of hard links. */
    uint16_t            cLinks;
    /** 0x14: Offset of the first attribute (relative to start of record). */
    uint16_t            offFirstAttrib;
    /** 0x16: Record flags (NTFSRECFILE_F_XXX). */
    uint16_t            fFlags;
    /** 0x18: Number of byte in use in this MFT record. */
    uint32_t            cbRecUsed;
    /** 0x1c: The MFT record size. */
    uint32_t            cbRecSize;
    /** 0x20: Reference to the base MFT record. */
    NTFSMFTREF          BaseMftRec;
    /** 0x28: Next attribute instance number. */
    uint16_t            idNextAttrib;
    /** 0x2a: Padding if NTFS 3.1+, update sequence array if older. */
    uint16_t            uPaddingOrUsa;
    /** 0x2c: MFT index of this record. */
    uint32_t            idxMftSelf;
} NTFSRECFILE;
AssertCompileSize(NTFSRECFILE, 0x30);
/** Pointer to a NTFS file record. */
typedef NTFSRECFILE *PNTFSRECFILE;
/** Pointer to a const NTFS file record. */
typedef NTFSRECFILE const *PCNTFSRECFILE;


/** NTFS 'FILE' record magic value. */
#define NTFSREC_MAGIC_FILE      RT_H2LE_U32_C(UINT32_C(0x454c4946))

/** @name NTFSRECFILE_F_XXX - NTFSRECFILE::fFlags.
 * @{ */
/** MFT record is in use. */
#define NTFSRECFILE_F_IN_USE        RT_H2LE_U16_C(UINT16_C(0x0001))
/** Directory record. */
#define NTFSRECFILE_F_DIRECTORY     RT_H2LE_U16_C(UINT16_C(0x0002))
/** @} */


/** @name NTFS_AT_XXX - Attribute types
 * @{ */
#define NTFS_AT_UNUSED                      RT_H2LE_U32_C(UINT32_C(0x00000000))
/** NTFSATSTDINFO */
#define NTFS_AT_STANDARD_INFORMATION        RT_H2LE_U32_C(UINT32_C(0x00000010))
/** NTFSATLISTENTRY */
#define NTFS_AT_ATTRIBUTE_LIST              RT_H2LE_U32_C(UINT32_C(0x00000020))
/** NTFSATFILENAME */
#define NTFS_AT_FILENAME                    RT_H2LE_U32_C(UINT32_C(0x00000030))
#define NTFS_AT_OBJECT_ID                   RT_H2LE_U32_C(UINT32_C(0x00000040))
#define NTFS_AT_SECURITY_DESCRIPTOR         RT_H2LE_U32_C(UINT32_C(0x00000050))
#define NTFS_AT_VOLUME_NAME                 RT_H2LE_U32_C(UINT32_C(0x00000060))
#define NTFS_AT_VOLUME_INFORMATION          RT_H2LE_U32_C(UINT32_C(0x00000070))
#define NTFS_AT_DATA                        RT_H2LE_U32_C(UINT32_C(0x00000080))
#define NTFS_AT_INDEX_ROOT                  RT_H2LE_U32_C(UINT32_C(0x00000090))
#define NTFS_AT_INDEX_ALLOCATION            RT_H2LE_U32_C(UINT32_C(0x000000a0))
#define NTFS_AT_BITMAP                      RT_H2LE_U32_C(UINT32_C(0x000000b0))
#define NTFS_AT_REPARSE_POINT               RT_H2LE_U32_C(UINT32_C(0x000000c0))
#define NTFS_AT_EA_INFORMATION              RT_H2LE_U32_C(UINT32_C(0x000000d0))
#define NTFS_AT_EA                          RT_H2LE_U32_C(UINT32_C(0x000000e0))
#define NTFS_AT_PROPERTY_SET                RT_H2LE_U32_C(UINT32_C(0x000000f0))
#define NTFS_AT_LOGGED_UTILITY_STREAM       RT_H2LE_U32_C(UINT32_C(0x00000100))
#define NTFS_AT_FIRST_USER_DEFINED          RT_H2LE_U32_C(UINT32_C(0x00001000))
#define NTFS_AT_END                         RT_H2LE_U32_C(UINT32_C(0xffffffff))
/** @} */

/** @name NTFS_AF_XXX - Attribute flags.
 * @{ */
#define NTFS_AF_COMPR_FMT_NONE              UINT16_C(0x0000)
#define NTFS_AF_COMPR_FMT_LZNT1             UINT16_C(0x0001)    /**< See RtlCompressBuffer / COMPRESSION_FORMAT_LZNT1. */
#define NTFS_AF_COMPR_FMT_XPRESS            UINT16_C(0x0002)    /**< See RtlCompressBuffer / COMPRESSION_FORMAT_XPRESS_HUFF. */
#define NTFS_AF_COMPR_FMT_XPRESS_HUFF       UINT16_C(0x0003)    /**< See RtlCompressBuffer / COMPRESSION_FORMAT_XPRESS_HUFF. */
#define NTFS_AF_COMPR_FMT_MASK              UINT16_C(0x00ff)
#define NTFS_AF_ENCRYPTED                   UINT16_C(0x4000)
#define NTFS_AF_SPARSE                      UINT16_C(0x8000)
/** @} */

/**
 * NTFS attribute header.
 *
 * This has three forms:
 *      - Resident
 *      - Non-resident, no compression
 *      - Non-resident, compressed.
 *
 * Each form translates to a different header size.
 */
typedef struct NTFSATTRIBHDR
{
    /** 0x00: Attribute type (NTFS_AT_XXX). */
    uint32_t                uAttrType;
    /** 0x04: Length of this attribute (resident part). */
    uint32_t                cbAttrib;
    /** 0x08: Set (1) if non-resident attribute, 0 if resident. */
    uint8_t                 fNonResident;
    /** 0x09: Attribute name length (can be zero). */
    uint8_t                 cwcName;
    /** 0x0a: Offset of the name string (relative to the start of this header). */
    uint16_t                offName;
    /** 0x0c: NTFS_AF_XXX. */
    uint16_t                fFlags;
    /** 0x0e: Attribute instance number.  Unique within the MFT record. */
    uint16_t                idAttrib;
    /** 0x10: Data depending on the fNonResident member value. */
    union
    {
        /** Resident attributes. */
        struct
        {
            /** 0x10: Attribute value length. */
            uint32_t        cbValue;
            /** 0x14: Offset of the value (relative to the start of this header). */
            uint16_t        offValue;
            /** 0x16: NTFS_RES_AF_XXX. */
            uint8_t         fFlags;
            /** 0x17: Reserved. */
            uint8_t         bReserved;
        } Res;

        /** Non-resident attributes. */
        struct
        {
            /** 0x10: The first virtual cluster containing data.
             *
             * This is mainly for internal checking when the run list doesn't fit in one
             * MFT record.  It can also be used to avoid recording a sparse run at the
             * beginning of the data covered by this attribute record. */
            int64_t         iVcnFirst;
            /** 0x18: The last virtual cluster containing data (inclusive). */
            int64_t         iVcnLast;
            /** 0x20: Offset of the mapping pair program.  This program gives us a mapping
             * between VNC and LCN for the attribute value. */
            uint16_t        offMappingPairs;
            /** 0x22: Power of two compression unit size in clusters (cbCluster << uCompessionUnit).
             * Zero means uncompressed.  */
            uint8_t         uCompressionUnit;
            /** 0x23: Reserved */
            uint8_t         abReserved[5];
            /** 0x28: Allocated size (rouneded to cluster).
             * @note Only set in the first attribute record (iVcnFirst == 0). */
            int64_t         cbAllocated;
            /** 0x30: The exact length of the data.
             * @note Only set in the first attribute record (iVcnFirst == 0). */
            int64_t         cbData;
            /** 0x38: The length of the initialized data (rounded to cluster).
             * @note Only set in the first attribute record (iVcnFirst == 0). */
            int64_t         cbInitialized;
            /** 0x40: Compressed size if compressed, otherwise absent. */
            int64_t         cbCompressed;
        } NonRes;
    } u;
} NTFSATTRIBHDR;
AssertCompileSize(NTFSATTRIBHDR, 0x48);
AssertCompileMemberOffset(NTFSATTRIBHDR, u.Res, 0x10);
AssertCompileMemberOffset(NTFSATTRIBHDR, u.Res.bReserved, 0x17);
AssertCompileMemberOffset(NTFSATTRIBHDR, u.NonRes, 0x10);
AssertCompileMemberOffset(NTFSATTRIBHDR, u.NonRes.cbCompressed, 0x40);
/** Pointer to a NTFS attribute header. */
typedef NTFSATTRIBHDR *PNTFSATTRIBHDR;
/** Pointer to a const NTFS attribute header. */
typedef NTFSATTRIBHDR const *PCNTFSATTRIBHDR;

/** @name NTFSATTRIBHDR_SIZE_XXX - Attribute header sizes.
 * @{ */
/** Attribute header size for resident values. */
#define NTFSATTRIBHDR_SIZE_RESIDENT                 (0x18)
/** Attribute header size for uncompressed non-resident values. */
#define NTFSATTRIBHDR_SIZE_NONRES_UNCOMPRESSED      (0x40)
/** Attribute header size for compressed non-resident values. */
#define NTFSATTRIBHDR_SIZE_NONRES_COMPRESSED        (0x48)
/** @} */


/**
 * Attribute list entry (NTFS_AT_ATTRIBUTE_LIST).
 *
 * This is used to deal with a file having attributes in more than one MFT
 * record.  A prominent example is an fragment file (unnamed data attribute)
 * which mapping pairs doesn't fit in a single MFT record.
 *
 * This attribute can be non-resident, however it's mapping pair program must
 * fit in the base MFT record.
 */
typedef struct NTFSATLISTENTRY
{
    /** 0x00: Attribute type (NTFS_AT_XXX). */
    uint32_t            uAttrType;
    /** 0x04: Length of this entry. */
    uint16_t            cbEntry;
    /** 0x06: Attribute name length (zero if none). */
    uint8_t             cwcName;
    /** 0x07: Name offset. */
    uint8_t             offName;
    /** 0x08: The first VNC for this part of the attribute value. */
    int64_t             iVcnFirst;
    /** 0x10: The MFT record holding the actual attribute. */
    NTFSMFTREF          InMftRec;
    /** 0x18: Attribute instance number.  Unique within the MFT record. */
    uint16_t            idAttrib;
    /** 0x1a: Maybe where the attribute name starts. */
    RTUTF16             wszName[RT_FLEXIBLE_ARRAY];
} NTFSATLISTENTRY;
AssertCompileMemberOffset(NTFSATLISTENTRY, idAttrib, 0x18);
/** Pointer to a NTFS attribute list entry. */
typedef NTFSATLISTENTRY *PNTFSATLISTENTRY;
/** Pointer to a const NTFS attribute list entry. */
typedef NTFSATLISTENTRY const *PCNTFSATLISTENTRY;

/** Unaligned minimum entry size (no name). */
#define NTFSATLISTENTRY_SIZE_MINIMAL        0x1a



/**
 * NTFS standard file info attribute (NTFS_AT_STANDARD_INFORMATION).
 */
typedef struct NTFSATSTDINFO
{
    /** 0x00: Creation timestamp. */
    int64_t             iCreationTime;
    /** 0x08: Last data modification timestamp. */
    int64_t             iLastDataModTime;
    /** 0x10: Last MFT record modification timestamp. */
    int64_t             iLastMftModTime;
    /** 0x18: Last access timestamp. */
    int64_t             iLastAccessTime;
    /** 0x20: File attributes. */
    uint32_t            fFileAttribs;
    /** 0x24: Maximum number of file versions allowed.
     * @note NTFS 3.x, padding in 1.2  */
    uint32_t            cMaxFileVersions;
    /** 0x28: Current file version number.
     * @note NTFS 3.x, padding in 1.2  */
    uint32_t            uFileVersion;
    /** 0x2c: Class ID (whatever that is).
     * @note NTFS 3.x, padding in 1.2  */
    uint32_t            idClass;
    /** 0x30: Owner ID.
     * Translated via $Q index in NTFS_MFT_IDX_EXTENDED/$Quota.
     * @note NTFS 3.x, not present in 1.2  */
    uint32_t            idOwner;
    /** 0x34: Security ID. Translated via $SII index and $SDS data stream in
     *  NTFS_MFT_IDX_SECURITY.
     * @note NTFS 3.x, not present in 1.2  */
    uint32_t            idSecurity;
    /** 0x38: Total quota charged for this file.
     * @note NTFS 3.x, not present in 1.2  */
    uint64_t            cbQuotaChared;
    /** 0x40: Last update sequence number, index into $UsnJrnl.
     * @note NTFS 3.x, not present in 1.2  */
    uint64_t            idxUpdateSequence;
} NTFSATSTDINFO;
AssertCompileSize(NTFSATSTDINFO, 0x48);
/** Pointer to NTFS standard file info. */
typedef NTFSATSTDINFO *PNTFSATSTDINFO;
/** Pointer to const NTFS standard file info. */
typedef NTFSATSTDINFO const *PCNTFSATSTDINFO;

/** The size of NTFSATSTDINFO in NTFS v1.2 and earlier. */
#define NTFSATSTDINFO_SIZE_NTFS_V12     (0x30)

/** @name NTFS_FA_XXX - NTFS file attributes.
 * @{ */
#define NTFS_FA_READONLY                            UINT32_C(0x00000001)
#define NTFS_FA_HIDDEN                              UINT32_C(0x00000002)
#define NTFS_FA_SYSTEM                              UINT32_C(0x00000004)
#define NTFS_FA_DIRECTORY                           UINT32_C(0x00000010)
#define NTFS_FA_ARCHIVE                             UINT32_C(0x00000020)
#define NTFS_FA_DEVICE                              UINT32_C(0x00000040)
#define NTFS_FA_NORMAL                              UINT32_C(0x00000080)
#define NTFS_FA_TEMPORARY                           UINT32_C(0x00000100)
#define NTFS_FA_SPARSE_FILE                         UINT32_C(0x00000200)
#define NTFS_FA_REPARSE_POINT                       UINT32_C(0x00000400)
#define NTFS_FA_COMPRESSED                          UINT32_C(0x00000800)
#define NTFS_FA_OFFLINE                             UINT32_C(0x00001000)
#define NTFS_FA_NOT_CONTENT_INDEXED                 UINT32_C(0x00002000)
#define NTFS_FA_ENCRYPTED                           UINT32_C(0x00004000)
#define NTFS_FA_VALID_FLAGS                         UINT32_C(0x00007fb7)
#define NTFS_FA_VALID_SET_FLAGS                     UINT32_C(0x000031a7)
#define NTFS_FA_DUP_FILE_NAME_INDEX_PRESENT         UINT32_C(0x10000000) /**< ?? */
#define NTFS_FA_DUP_VIEW_INDEX_PRESENT              UINT32_C(0x20000000) /**< ?? */
/** @} */



/**
 * NTFS filename attribute (NTFS_AT_FILENAME).
 */
typedef struct NTFSATFILENAME
{
    /** 0x00: The parent directory MFT record. */
    NTFSMFTREF          ParentDirMftRec;
    /** 0x08: Creation timestamp. */
    int64_t             iCreationTime;
    /** 0x10: Last data modification timestamp. */
    int64_t             iLastDataModTime;
    /** 0x18: Last MFT record modification timestamp. */
    int64_t             iLastMftModTime;
    /** 0x20: Last access timestamp. */
    int64_t             iLastAccessTime;
    /** 0x28: Allocated disk space for the unnamed data attribute. */
    int64_t             cbAllocated;
    /** 0x30: Actual size of unnamed data attribute. */
    int64_t             cbData;
    /** 0x38: File attributes. */
    uint32_t            fFileAttribs;
    union
    {
        /** 0x3c: Packed EA length. */
        uint16_t        cbPackedEas;
        /** 0x3c: Reparse tag, if no EAs. */
        uint32_t        uReparseTag;
    } u;
    /** 0x40: Filename length in unicode chars. */
    uint8_t             cwcFilename;
    /** 0x41: Filename type (NTFS_FILENAME_T_XXX). */
    uint8_t             fFilenameType;
    /** 0x42: The filename. */
    RTUTF16             wszFilename[RT_FLEXIBLE_ARRAY];
} NTFSATFILENAME;
AssertCompileMemberOffset(NTFSATFILENAME, cbData, 0x30);
AssertCompileMemberOffset(NTFSATFILENAME, u.cbPackedEas, 0x3c);
AssertCompileMemberOffset(NTFSATFILENAME, u.uReparseTag, 0x3c);
AssertCompileMemberOffset(NTFSATFILENAME, wszFilename, 0x42);
/** Pointer to a NTFS filename attribute. */
typedef NTFSATFILENAME *PNTFSATFILENAME;
/** Pointer to a const NTFS filename attribute. */
typedef NTFSATFILENAME const *PCNTFSATFILENAME;

/** @name NTFS_FILENAME_T_XXX - filename types
 * @{ */
#define NTFS_FILENAME_T_POSIX           0
#define NTFS_FILENAME_T_WINDOWS         1
#define NTFS_FILENAME_T_DOS             2
#define NTFS_FILENAME_T_WINDOWS_AND_DSO 3
/** @} */

/** @} */

#endif

