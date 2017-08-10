/* $Id$ */
/** @file
 * IPRT, Universal Disk Format (UDF).
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

#ifndef ___iprt_formats_udf_h
#define ___iprt_formats_udf_h

#include <iprt/types.h>
#include <iprt/assert.h>
#include <iprt/formats/iso9660.h>


/** @defgroup grp_rt_formats_udf    Universal Disk Format (UDF) structures and definitions
 * @ingroup grp_rt_formats
 *
 * References:
 *  - https://www.ecma-international.org/publications/files/ECMA-ST/Ecma-167.pdf
 *  - http://www.osta.org/specs/pdf/udf260.pdf
 *  - http://wiki.osdev.org/UDF
 *  - https://sites.google.com/site/udfintro/
 *
 * @{
 */


/**
 * UDF extent allocation descriptor (AD) (@ecma167{3,7.1,42}).
 */
typedef struct UDFEXTENTAD
{
    /** Extent length in bytes. */
    uint32_t        cb;
    /** Extent offset (logical sector number).
     * If @a cb is zero, this is also zero. */
    uint32_t        off;
} UDFEXTENTAD;
AssertCompileSize(UDFEXTENTAD, 8);
/** Pointer to an UDF extent descriptor. */
typedef UDFEXTENTAD *PUDFEXTENTAD;
/** Pointer to a const UDF extent descriptor. */
typedef UDFEXTENTAD const *PCUDFEXTENTAD;


/**
 * UDF logical block address (@ecma167{4,7.1,73}).
 */
#pragma pack(2)
typedef struct UDFLBADDR
{
    /** Logical block number, relative to the start of the given partition. */
    uint32_t        off;
    /** Partition reference number. */
    uint16_t        uPartitionNo;
} UDFLBADDR;
#pragma pack()
AssertCompileSize(UDFLBADDR, 6);
/** Pointer to an UDF logical block address. */
typedef UDFLBADDR *PUDFLBADDR;
/** Pointer to a const UDF logical block address. */
typedef UDFLBADDR const *PCUDFLBADDR;


/** @name UDF_AD_TYPE_XXX - Allocation descriptor types.
 *
 * Used by UDFSHORTAD::uType, UDFLONGAD::uType and UDFEXTAD::uType.
 *
 * See @ecma167{4,14.14.1.1,116}.
 *
 * @{ */
/** Recorded and allocated.
 * Also used for zero length descriptors. */
#define UDF_AD_TYPE_RECORDED_AND_ALLOCATED          0
/** Allocated but not recorded. */
#define UDF_AD_TYPE_ONLY_ALLOCATED                  1
/** Not recorded nor allocated. */
#define UDF_AD_TYPE_FREE                            2
/** Go figure. */
#define UDF_AD_TYPE_NEXT                            3
/** @} */

/**
 * UDF short allocation descriptor (@ecma167{4,14.14.1,116}).
 */
typedef struct UDFSHORTAD
{
#ifdef RT_BIG_ENDIAN
    /** Extent type (UDF_AD_TYPE_XXX). */
    uint32_t        uType : 2;
    /** Extent length in bytes, top 2 bits . */
    uint32_t        cb : 30;
#else
    /** Extent length in bytes. */
    uint32_t        cb : 30;
    /** Extent type (UDF_AD_TYPE_XXX). */
    uint32_t        uType : 2;
#endif
    /** Extent offset (logical sector number). */
    uint32_t        off;
} UDFSHORTAD;
AssertCompileSize(UDFSHORTAD, 8);
/** Pointer to an UDF short allocation descriptor. */
typedef UDFSHORTAD *PUDFSHORTAD;
/** Pointer to a const UDF short allocation descriptor. */
typedef UDFSHORTAD const *PCUDFSHORTAD;

/**
 * UDF long allocation descriptor (@ecma167{4,14.14.2,116}).
 */
typedef struct UDFLONGAD
{
#ifdef RT_BIG_ENDIAN
    /** Extent type (UDF_AD_TYPE_XXX). */
    uint32_t        uType : 2;
    /** Extent length in bytes, top 2 bits . */
    uint32_t        cb : 30;
#else
    /** Extent length in bytes. */
    uint32_t        cb : 30;
    /** Extent type (UDF_AD_TYPE_XXX). */
    uint32_t        uType : 2;
#endif
    /** Extent location. */
    UDFLBADDR       Location;
    /** Implementation use area. */
    uint8_t         abImplementationUse[6];
} UDFLONGAD;
AssertCompileSize(UDFLONGAD, 16);
/** Pointer to an UDF long allocation descriptor. */
typedef UDFLONGAD *PUDFLONGAD;
/** Pointer to a const UDF long allocation descriptor. */
typedef UDFLONGAD const *PCUDFLONGAD;

/**
 * UDF extended allocation descriptor (@ecma167{4,14.14.3,117}).
 */
typedef struct UDFEXTAD
{
#ifdef RT_BIG_ENDIAN
    /** 0x00: Extent type (UDF_AD_TYPE_XXX). */
    uint32_t        uType : 2;
    /** 0x00: Extent length in bytes, top 2 bits . */
    uint32_t        cb : 30;
    /** 0x04: Reserved, MBZ. */
    uint32_t        uReserved : 2;
    /** 0x04: Number of bytes recorded. */
    uint32_t        cbRecorded : 30;
#else
    /** 0x00: Extent length in bytes. */
    uint32_t        cb : 30;
    /** 0x00: Extent type (UDF_AD_TYPE_XXX). */
    uint32_t        uType : 2;
    /** 0x04: Number of bytes recorded. */
    uint32_t        cbRecorded : 30;
    /** 0x04: Reserved, MBZ. */
    uint32_t        uReserved : 2;
#endif
    /** 0x08: Number of bytes of information (from first byte). */
    uint32_t        cbInformation;
    /** 0x0c: Extent location. */
    UDFLBADDR       Location;
    /** 0x12: Implementation use area. */
    uint8_t         abImplementationUse[2];
} UDFEXTAD;
AssertCompileSize(UDFEXTAD, 20);
/** Pointer to an UDF extended allocation descriptor. */
typedef UDFEXTAD *PUDFEXTAD;
/** Pointer to a const UDF extended allocation descriptor. */
typedef UDFEXTAD const *PCUDFEXTAD;


/**
 * UDF timestamp (@ecma167{1,7.3,25}, @udf260{2.1.4,19}).
 */
typedef struct UDFTIMESTAMP
{
    /** 0x00: Type and timezone. */
    uint16_t        uTypeAndZone;
    /** 0x02: The year. */
    int16_t         iYear;
    /** 0x04: Month of year (1-12). */
    uint8_t         uMonth;
    /** 0x05: Day of month (1-31). */
    uint8_t         uDay;
    /** 0x06: Hour of day (0-23). */
    uint8_t         uHour;
    /** 0x07: Minute of hour (0-59). */
    uint8_t         uMinute;
    /** 0x08: Second of minute (0-60 if type 2, otherwise 0-59). */
    uint8_t         uSecond;
    /** 0x09: Number of Centiseconds (0-99). */
    uint8_t         cCentiseconds;
    /** 0x0a: Number of hundreds of microseconds (0-99).  Unit is 100us. */
    uint8_t         cHundredsOfMicroseconds;
    /** 0x0b: Number of microseconds (0-99). */
    uint8_t         cMicroseconds;
} UDFTIMESTAMP;
AssertCompileSize(UDFTIMESTAMP, 12);
/** Pointer to an UDF timestamp. */
typedef UDFTIMESTAMP *PUDFTIMESTAMP;
/** Pointer to a const UDF timestamp. */
typedef UDFTIMESTAMP const *PCUDFTIMESTAMP;


/**
 * UDF character set specficiation (@ecma167{1,7.2.1,21}, @udf260{2.1.2,18}).
 */
typedef struct UDFCHARSPEC
{
    /** The character set type (UDF_CHAR_SET_TYPE_XXX) */
    uint8_t     uType;
    /** Character set information. */
    uint8_t     abInfo[63];
} UDFCHARSPEC;
AssertCompileSize(UDFCHARSPEC, 64);
/** Pointer to UDF character set specification. */
typedef UDFCHARSPEC *PUDFCHARSPEC;
/** Pointer to const UDF character set specification. */
typedef UDFCHARSPEC const *PCUDFCHARSPEC;

/** @name UDF_CHAR_SET_TYPE_XXX - Character set types.
 * @{ */
/** CS0: By agreement between the medium producer and consumer.
 * See UDF_CHAR_SET_OSTA_COMPRESSED_UNICODE. */
#define UDF_CHAR_SET_TYPE_BY_AGREEMENT  UINT8_C(0x00)
/** CS1: ASCII (ECMA-6) with all or part of the specified graphic characters. */
#define UDF_CHAR_SET_TYPE_ASCII         UINT8_C(0x01)
/** CS5: Latin-1 (ECMA-94) with all graphical characters. */
#define UDF_CHAR_SET_TYPE_LATIN_1       UINT8_C(0x05)
/* there are more defined here, but they are mostly useless, since UDF only uses CS0. */

/** The CS0 definition used by the UDF specification. */
#define UDF_CHAR_SET_OSTA_COMPRESSED_UNICODE        UDF_CHAR_SET_TYPE_BY_AGREEMENT
/** String to put in the UDFCHARSEPC::abInfo field for UDF CS0. */
#define UDF_CHAR_SET_OSTA_COMPRESSED_UNICODE_INFO   "OSTA Compressed Unicode"
/** @} */


/**
 * UDF entity identifier (@ecma167{1,7.4,26}, @udf260{2.1.5,20}).
 */
typedef struct UDFENTITYID
{
    /** 0x00: Flags (UDFENTITYID_FLAGS_XXX). */
    uint8_t         fFlags;
    /** 0x01: Identifier string (see UDF_ENTITY_ID_XXX). */
    char            achIdentifier[23];
    /** 0x18: Identifier suffix. */
    union
    {
        /** Domain ID suffix. */
        struct
        {
            uint16_t    uUdfRevision;
            uint8_t     fDomain;
            uint8_t     abReserved[5];
        } Domain;

        /** UDF ID suffix. */
        struct
        {
            uint16_t    uUdfRevision;
            uint8_t     bOsClass;
            uint8_t     idOS;
            uint8_t     abReserved[4];
        } Udf;


        /** Implementation ID suffix. */
        struct
        {
            uint8_t     bOsClass;
            uint8_t     idOS;
            uint8_t     achImplUse[6];
        } Implementation;

        /** Application ID suffix / generic. */
        uint8_t     abApplication[8];
    } Suffix;
} UDFENTITYID;
AssertCompileSize(UDFENTITYID, 32);
/** Pointer to UDF entity identifier. */
typedef UDFENTITYID *PUDFENTITYID;
/** Pointer to const UDF entity identifier. */
typedef UDFENTITYID const *PCUDFENTITYID;

/** @name UDF_ENTITY_ID_XXX - UDF identifier strings
 *
 * See @udf260{2.1.5.2,21}.
 *
 * @{ */
/** Primary volume descriptor, implementation ID field.
 * Implementation ID suffix. */
#define UDF_ENTITY_ID_PVD_IMPLEMENTATION        "*Developer ID"
/** Primary volume descriptor, application ID field.
 * Application ID suffix. */
#define UDF_ENTITY_ID_PVD_APPLICATION           "*Application ID"

/** Implementation use volume descriptor, implementation ID field.
 * UDF ID suffix. */
#define UDF_ENTITY_ID_IUVD_IMPLEMENTATION       "*UDF LV Info"
/** Implementation use volume descriptor, implementation ID field in Use field.
 * Implementation ID suffix. */
#define UDF_ENTITY_ID_IUVD_USE_IMPLEMENTATION   "*Developer ID"

/** Partition descriptor, implementation ID field.
 * Implementation ID suffix. */
#define UDF_ENTITY_ID_PD_IMPLEMENTATION         "*Developer ID"
/** Partition descriptor, partition contents field, set to indicate UDF
 * (ECMA-167 3rd edition). Application ID suffix. */
#define UDF_ENTITY_ID_PD_PARTITION_CONTENTS_UDF     "+NSR03"
/** Partition descriptor, partition contents field, set to indicate ISO-9660
 * (ECMA-119). Application ID suffix. */
#define UDF_ENTITY_ID_PD_PARTITION_CONTENTS_ISO9660 "+CD001"
/** Partition descriptor, partition contents field, set to indicate ECMA-168.
 * Application ID suffix. */
#define UDF_ENTITY_ID_PD_PARTITION_CONTENTS_CDW     "+CDW02"
/** Partition descriptor, partition contents field, set to indicate FAT
 * (ECMA-107). Application ID suffix. */
#define UDF_ENTITY_ID_PD_PARTITION_CONTENTS_FAT     "+FDC01"

/** Logical volume descriptor, implementation ID field.
 * Implementation ID suffix. */
#define UDF_ENTITY_ID_LVD_IMPLEMENTATION        "*Developer ID"
/** Logical volume descriptor, domain ID field.
 * Domain ID suffix. */
#define UDF_ENTITY_ID_LVD_DOMAIN                "*OSTA UDF Compliant"

/** File set descriptor, domain ID field.
 * Domain ID suffix. */
#define UDF_ENTITY_FSD_LVD_DOMAIN               "*OSTA UDF Compliant"

/** File identifier descriptor, implementation use field.
 * Implementation ID suffix. */
#define UDF_ENTITY_ID_FID_IMPLEMENTATION_USE    "*Developer ID"

/** File entry, implementation ID field.
 * Implementation ID suffix. */
#define UDF_ENTITY_ID_FE_IMPLEMENTATION         "*Developer ID"

/** Device specification extended attribute, implementation use field.
 * Implementation ID suffix. */
#define UDF_ENTITY_ID_DSEA_IMPLEMENTATION_USE   "*Developer ID"

/** UDF implementation use extended attribute, implementation ID field, set
 * to free EA space.  UDF ID suffix. */
#define UDF_ENTITY_ID_IUEA_FREE_EA_SPACE        "*UDF FreeEASpace"
/** UDF implementation use extended attribute, implementation ID field, set
 * to DVD copyright management information.  UDF ID suffix. */
#define UDF_ENTITY_ID_IUEA_DVD_CGMS_INFO        "*UDF DVD CGMS Info"
/** UDF implementation use extended attribute, implementation ID field, set
 * to OS/2 extended attribute length.  UDF ID suffix. */
#define UDF_ENTITY_ID_IUEA_OS2_EA_LENGTH        "*UDF OS/2 EALength"
/** UDF implementation use extended attribute, implementation ID field, set
 * to Machintosh OS volume information.  UDF ID suffix. */
#define UDF_ENTITY_ID_IUEA_MAC_VOLUME_INFO      "*UDF Mac VolumeInfo"
/** UDF implementation use extended attribute, implementation ID field, set
 * to Machintosh Finder Info.  UDF ID suffix. */
#define UDF_ENTITY_ID_IUEA_MAC_FINDER_INFO      "*UDF Mac FinderInfo"
/** UDF implementation use extended attribute, implementation ID field, set
 * to OS/400 extended directory information.  UDF ID suffix. */
#define UDF_ENTITY_ID_IUEA_OS400_DIR_INFO       "*UDF OS/400 DirInfo"

/** Non-UDF implementation use extended attribute, implementation ID field.
 * Implementation ID suffix. */
#define UDF_ENTITY_ID_NUIUSEA_IMPLEMENTATION    "*Developer ID"

/** UDF application use extended attribute, application ID field, set
 * to free application use EA space.  UDF ID suffix. */
#define UDF_ENTITY_ID_AUEA_FREE_EA_SPACE        "*UDF FreeAppEASpace"

/** Non-UDF application use extended attribute, implementation ID field.
 * Application ID suffix. */
#define UDF_ENTITY_ID_NUAUEA_IMPLEMENTATION     "*Application ID"

/** UDF unique ID mapping data, implementation ID field.
 * Implementation ID suffix. */
#define UDF_ENTITY_ID_UIMD_IMPLEMENTATION       "*Developer ID"

/** Power calibration table stream, implementation ID field.
 * Implementation ID suffix. */
#define UDF_ENTITY_ID_PCTS_IMPLEMENTATION       "*Developer ID"

/** Logical volume integrity descriptor, implementation ID field.
 * Implementation ID suffix. */
#define UDF_ENTITY_ID_LVID_IMPLEMENTATION       "*Developer ID"

/** Virtual partition map, partition type field.
 * UDF ID suffix. */
#define UDF_ENTITY_ID_VPM_PARTITION_TYPE        "*UDF Virtual Partition"

/** Virtual allocation table, implementation use field.
 * Implementation ID suffix. */
#define UDF_ENTITY_ID_VAT_IMPLEMENTATION_USE    "*Developer ID"

/** Sparable partition map, partition type field.
 * UDF ID suffix. */
#define UDF_ENTITY_ID_SPM_PARTITION_TYPE        "*UDF Sparable Partition"

/** Sparing table, sparing identifier field.
 * UDF ID suffix. */
#define UDF_ENTITY_ID_ST_SPARING                "*UDF Sparting Table"

/** Metadata partition map, partition type field.
 * UDF ID suffix. */
#define UDF_ENTITY_ID_MPM_PARTITION_TYPE        "*UDF Metadata Partition"

/** @} */


/**
 * UDF descriptor tag (@ecma167{3,7.2,42}, @udf260{2.2.1,26}).
 */
typedef struct UDFTAG
{
    /** Tag identifier (UDF_TAG_ID_XXX). */
    uint16_t        idTag;
    /** Descriptor version. */
    uint16_t        uVersion;
    /** Tag checksum.
     * Sum of each byte in the structure with this field as zero.  */
    uint8_t         uChecksum;
    /** Reserved, MBZ. */
    uint8_t         bReserved;
    /** Tag serial number. */
    uint16_t        uTagSerialNo;
    /** Descriptor CRC. */
    uint16_t        uDescriptorCrc;
    /** Descriptor CRC length. */
    uint16_t        cbDescriptorCrc;
    /** The tag location (logical sector number). */
    uint32_t        offTag;
} UDFTAG;
AssertCompileSize(UDFTAG, 16);
/** Pointer to an UDF descriptor tag. */
typedef UDFTAG *PUDFTAG;
/** Pointer to a const UDF descriptor tag. */
typedef UDFTAG const *PCUDFTAG;

/** @name UDF_TAG_ID_XXX - UDF descriptor tag IDs.
 * @{ */
#define UDF_TAG_ID_PRIMARY_VOL_DESC                 UINT16_C(0x0001)
#define UDF_TAG_ID_ANCHOR_VOLUME_DESC_PTR           UINT16_C(0x0002) /**< UDFANCHORVOLUMEDESCPTR */
#define UDF_TAG_ID_VOLUME_DESC_PTR                  UINT16_C(0x0003)
#define UDF_TAG_ID_IMPLEMENATION_USE_VOLUME_DESC    UINT16_C(0x0004)
#define UDF_TAG_ID_PARTITION_DESC                   UINT16_C(0x0005)
#define UDF_TAG_ID_LOGICAL_VOLUME_DESC              UINT16_C(0x0006)
#define UDF_TAG_ID_UNALLOCATED_SPACE_DESC           UINT16_C(0x0007)
#define UDF_TAG_ID_TERMINATING_DESC                 UINT16_C(0x0008)
#define UDF_TAG_ID_LOGICAL_VOLUME_INTEGRITY_DESC    UINT16_C(0x0009)
/** @} */


/**
 * UDF primary volume descriptor (PVD) (@ecma167{3,10.1,50},
 * @udf260{2.2.2,27}).
 */
typedef struct UDFPRIMARYVOLUMEDESC
{
    /** 0x000: The descriptor tag (UDF_TAG_ID_PRIMARY_VOL_DESC). */
    UDFTAG          Tag;
    /** 0x010: Volume descriptor sequence number. */
    uint32_t        uVolumeDescSeqNo;
    /** 0x014: Primary volume descriptor number. */
    uint32_t        uPrimaryVolumeDescNo;
    /** 0x018: Volume identifier (dstring). */
    char            achVolumeID[32];
    /** 0x038: Volume sequence number. */
    uint16_t        uVolumeSeqNo;
    /** 0x03a: Maximum volume sequence number. */
    uint16_t        uMaxVolumeSeqNo;
    /** 0x03c: Interchange level. */
    uint16_t        uInterchangeLevel;
    /** 0x03e: Maximum interchange level. */
    uint16_t        uMaxInterchangeLevel;
    /** 0x040: Character set bitmask (aka list).  Each bit correspond to a
     * character set number. */
    uint32_t        fCharacterSets;
    /** 0x044: Maximum character set bitmask (aka list). */
    uint32_t        fMaxCharacterSets;
    /** 0x048: Volume set identifier (dstring).  This starts with 16 unique
     *  characters, the first 8 being the hex represenation of a time value. */
    char            achVolumeSetID[128];
    /** 0x0c8: Descriptor character set.
     * For achVolumeSetID and achVolumeID. */
    UDFCHARSPEC     DescCharSet;
    /** 0x108: Explanatory character set.
     * For VolumeAbstract and VolumeCopyrightNotice data. */
    UDFCHARSPEC     ExplanatoryCharSet;
    /** 0x148: Volume abstract. */
    UDFEXTENTAD     VolumeAbstract;
    /** 0x150: Volume copyright notice. */
    UDFEXTENTAD     VolumeCopyrightNotice;
    /** 0x158: Application identifier (UDF_ENTITY_ID_PVD_APPLICATION). */
    UDFENTITYID     idApplication;
    /** 0x178: Recording date and time. */
    UDFTIMESTAMP    RecordingTimestamp;
    /** 0x184: Implementation identifier (UDF_ENTITY_ID_PVD_IMPLEMENTATION). */
    UDFENTITYID     idImplementation;
    /** 0x1a4: Implementation use. */
    uint8_t         abImplementationUse[64];
    /** 0x1e4: Predecessor volume descriptor sequence location. */
    uint32_t        offPredecessorVolDescSeq;
    /** 0x1e8: Flags (UDF_PVD_FLAGS_XXX). */
    uint16_t        fFlags;
    /** 0x1ea: Reserved. */
    uint8_t         abReserved[22];
} UDFPRIMARYVOLUMEDESC;
AssertCompileSize(UDFPRIMARYVOLUMEDESC, 512);
/** Pointer to a UDF primary volume descriptor. */
typedef UDFPRIMARYVOLUMEDESC *PUDFPRIMARYVOLUMEDESC;
/** Pointer to a const UDF primary volume descriptor. */
typedef UDFPRIMARYVOLUMEDESC const *PCUDFPRIMARYVOLUMEDESC;

/** @name UDF_PVD_FLAGS_XXX - Flags for UDFPRIMARYVOLUMEDESC::fFlags.
 * @{ */
/** Indicates that the volume set ID is common to all members of the set.  */
#define UDF_PVD_FLAGS_COMMON_VOLUME_SET_ID      UINT16_C(0x0001)
/** @} */


/**
 * UDF anchor volume descriptor pointer (AVDP) (@ecma167{3,10.2,53},
 * @udf260{2.2.3,29}).
 *
 * This is stored at least two of these locations:
 *      - logical sector 256
 *      - logical sector N - 256.
 *      - logical sector N.
 */
typedef struct UDFANCHORVOLUMEDESCPTR
{
    /** 0x00: The descriptor tag (UDF_TAG_ID_ANCHOR_VOLUME_DESC_PTR). */
    UDFTAG          Tag;
    /** 0x10: The extent descripting the main volume descriptor sequence. */
    UDFEXTENTAD     MainVolumeDescSeq;
    /** 0x18: Location of the backup descriptor sequence. */
    UDFEXTENTAD     ReserveVolumeDescSeq;
    /** 0x20: Reserved, probably must be zeros. */
    uint8_t         abReserved[0x1e0];
} UDFANCHORVOLUMEDESCPTR;
AssertCompileSize(UDFANCHORVOLUMEDESCPTR, 512);
/** Pointer to UDF anchor volume descriptor pointer. */
typedef UDFANCHORVOLUMEDESCPTR *PUDFANCHORVOLUMEDESCPTR;
/** Pointer to const UDF anchor volume descriptor pointer. */
typedef UDFANCHORVOLUMEDESCPTR const *PCUDFANCHORVOLUMEDESCPTR;


/**
 * UDF volume descriptor pointer (VDP) (@ecma167{3,10.3,53}).
 */
typedef struct UDFVOLUMEDESCPTR
{
    /** 0x00: The descriptor tag (UDF_TAG_ID_VOLUME_DESC_PTR). */
    UDFTAG          Tag;
    /** 0x10: Volume descriptor sequence number. */
    uint32_t        uVolumeDescSeqNo;
    /** 0x14: Location of the next volume descriptor sequence. */
    UDFEXTENTAD     NextVolumeDescSeq;
    /** 0x1c: Reserved, probably must be zeros. */
    uint8_t         abReserved[484];
} UDFVOLUMEDESCPTR;
AssertCompileSize(UDFVOLUMEDESCPTR, 512);
/** Pointer to UDF volume descriptor pointer. */
typedef UDFVOLUMEDESCPTR *PUDFVOLUMEDESCPTR;
/** Pointer to const UDF volume descriptor pointer. */
typedef UDFVOLUMEDESCPTR const *PCUDFVOLUMEDESCPTR;


/**
 * UDF implementation use volume descriptor (IUVD) (@ecma167{3,10.4,55},
 * @udf260{2.2.7,35}).
 */
typedef struct UDFIMPLEMENTATIONUSEVOLUMEDESC
{
    /** 0x00: The descriptor tag (UDF_TAG_ID_IMPLEMENATION_USE_VOLUME_DESC). */
    UDFTAG          Tag;
    /** 0x10: Volume descriptor sequence number. */
    uint32_t        uVolumeDescSeqNo;
    /** 0x14: The implementation identifier (UDF_ENTITY_ID_IUVD_IMPLEMENTATION). */
    UDFENTITYID     idImplementation;
    /** 0x34: The implementation use area. */
    union
    {
        /** Generic view. */
        uint8_t     ab[460];
        /** Logical volume information. */
        struct
        {
            /** 0x034: The character set used in this sub-structure. */
            UDFCHARSPEC     Charset;
            /** 0x074: Logical volume identifier. */
            char            achVolumeID[128];
            /** 0x0f4: Info string \#1. */
            char            achInfo1[36];
            /** 0x118: Info string \#2. */
            char            achInfo2[36];
            /** 0x13c: Info string \#3. */
            char            achInfo3[36];
            /** 0x160: The implementation identifier
             * (UDF_ENTITY_ID_IUVD_USE_IMPLEMENTATION). */
            UDFENTITYID     idImplementation;
            /** 0x180: Additional use bytes. */
            uint8_t         abUse[128];
        } Lvi;
    } ImplementationUse;
} UDFIMPLEMENTATIONUSEVOLUMEDESC;
AssertCompileSize(UDFIMPLEMENTATIONUSEVOLUMEDESC, 512);
AssertCompileMemberOffset(UDFIMPLEMENTATIONUSEVOLUMEDESC, ImplementationUse.Lvi.Charset,          0x034);
AssertCompileMemberOffset(UDFIMPLEMENTATIONUSEVOLUMEDESC, ImplementationUse.Lvi.achVolumeID,      0x074);
AssertCompileMemberOffset(UDFIMPLEMENTATIONUSEVOLUMEDESC, ImplementationUse.Lvi.achInfo1,         0x0f4);
AssertCompileMemberOffset(UDFIMPLEMENTATIONUSEVOLUMEDESC, ImplementationUse.Lvi.achInfo2,         0x118);
AssertCompileMemberOffset(UDFIMPLEMENTATIONUSEVOLUMEDESC, ImplementationUse.Lvi.achInfo3,         0x13c);
AssertCompileMemberOffset(UDFIMPLEMENTATIONUSEVOLUMEDESC, ImplementationUse.Lvi.idImplementation, 0x160);
/** Pointer to an UDF implementation use volume descriptor. */
typedef UDFIMPLEMENTATIONUSEVOLUMEDESC *PUDFIMPLEMENTATIONUSEVOLUMEDESC;
/** Pointer to a const UDF implementation use volume descriptor. */
typedef UDFIMPLEMENTATIONUSEVOLUMEDESC const *PCUDFIMPLEMENTATIONUSEVOLUMEDESC;


/**
 * UDF partition header descriptor (@ecma167{4,14.3,90}, @udf260{2.3.3,56}).
 *
 * This is found in UDFPARTITIONDESC::ContentsUse.
 */
typedef struct UDFPARTITIONHDRDESC
{
    /** 0x00: Unallocated space table location.  Zero length means no table. */
    UDFSHORTAD      UnallocatedSpaceTable;
    /** 0x08: Unallocated space bitmap location.  Zero length means no bitmap. */
    UDFSHORTAD      UnallocatedSpaceBitmap;
    /** 0x10: Partition integrity table location.  Zero length means no table. */
    UDFSHORTAD      PartitionIntegrityTable;
    /** 0x18: Freed space table location.  Zero length means no table. */
    UDFSHORTAD      FreedSpaceTable;
    /** 0x20: Freed space bitmap location.  Zero length means no bitmap. */
    UDFSHORTAD      FreedSpaceBitmap;
    /** 0x28: Reserved, MBZ. */
    uint8_t         abReserved[88];
} UDFPARTITIONHDRDESC;
AssertCompileSize(UDFPARTITIONHDRDESC, 128);
AssertCompileMemberOffset(UDFPARTITIONHDRDESC, PartitionIntegrityTable, 0x10);
AssertCompileMemberOffset(UDFPARTITIONHDRDESC, abReserved, 0x28);
/** Pointer to an UDF partition header descriptor. */
typedef UDFPARTITIONHDRDESC *PUDFPARTITIONHDRDESC;
/** Pointer to a const UDF partition header descriptor. */
typedef UDFPARTITIONHDRDESC const *PCUDFPARTITIONHDRDESC;


/**
 * UDF partition descriptor (PD) (@ecma167{3,10.5,55}, @udf260{2.2.14,51}).
 */
typedef struct UDFPARTITIONDESC
{
    /** 0x000: The descriptor tag (UDF_TAG_ID_PARTITION_DESC). */
    UDFTAG          Tag;
    /** 0x010: Volume descriptor sequence number. */
    uint32_t        uVolumeDescSeqNo;
    /** 0x014: The partition flags (UDF_PARTITION_FLAGS_XXX).   */
    uint16_t        fFlags;
    /** 0x016: The partition number. */
    uint16_t        uPartitionNo;
    /** 0x018: Partition contents (UDF_ENTITY_ID_PD_PARTITION_CONTENTS_XXX). */
    UDFENTITYID     PartitionContents;
    /** 0x038: partition contents use (depends on the PartitionContents field). */
    union
    {
        /** Generic view. */
        uint8_t     ab[128];
        /** UDF partition header descriptor (UDF_ENTITY_ID_PD_PARTITION_CONTENTS_UDF). */
        UDFPARTITIONHDRDESC Hdr;
    } ContentsUse;
    /** 0x0b8: Access type (UDF_PART_ACCESS_TYPE_XXX).  */
    uint32_t        uAccessType;
    /** 0x0bc: Partition starting location (logical sector number). */
    uint32_t        offLocation;
    /** 0x0c0: Partition length in sectors. */
    uint32_t        cSectors;
    /** 0x0c4: Implementation identifier (UDF_ENTITY_ID_PD_IMPLEMENTATION). */
    UDFENTITYID     idImplementation;
    /** 0x0e4: Implemenation use bytes. */
    union
    {
        /** Generic view. */
        uint8_t     ab[128];

    } ImplementationUse;
    /** 0x164: Reserved. */
    uint8_t         abReserved[156];
} UDFPARTITIONDESC;
AssertCompileSize(UDFPARTITIONDESC, 512);

/** @name UDF_PART_ACCESS_TYPE_XXX - UDF partition access types
 *
 * See @ecma167{3,10.5.7,57}, @udf260{2.2.14.2,51}.
 *
 * @{ */
/** Access not specified by this field. */
#define UDF_PART_ACCESS_TYPE_NOT_SPECIFIED  UINT32_C(0x00000000)
/** Read only: No writes. */
#define UDF_PART_ACCESS_TYPE_READ_ONLY      UINT32_C(0x00000001)
/** Write once: Sectors can only be written once. */
#define UDF_PART_ACCESS_TYPE_WRITE_ONCE     UINT32_C(0x00000002)
/** Rewritable: Logical sectors may require preprocessing before writing. */
#define UDF_PART_ACCESS_TYPE_REWRITABLE     UINT32_C(0x00000003)
/** Overwritable: No restrictions on writing. */
#define UDF_PART_ACCESS_TYPE_OVERWRITABLE   UINT32_C(0x00000004)
/** @} */


/**
 * Logical volume descriptor (LVD).
 */
typedef struct UDFLOGICALVOLUMEDESC
{
    /** 0x00: The descriptor tag (UDF_TAG_ID_LOGICAL_VOLUME_DESC). */
    UDFTAG          Tag;
    /** 0x10: Volume descriptor sequence number. */
    uint32_t        uVolumeDescSeqNo;
    /** 0x14: Character set used in the achLogicalVolumeID field.   */
    UDFCHARSPEC     DescriptorCharSet;
    /** 0x54: The logical volume ID (label). */
    char            achLogicalVolumeID[128];

    // continue here...
    // continue here...
    // continue here...

} UDFLOGICALVOLUMEDESC;

//#define UDF_TAG_ID_UNALLOCATED_SPACE_DESC           UINT16_C(0x0007)
//#define UDF_TAG_ID_TERMINATING_DESC                 UINT16_C(0x0008)
//#define UDF_TAG_ID_LOGICAL_VOLUME_INTEGRITY_DESC    UINT16_C(0x0009)



/** @name UDF Volume Recognition Sequence (VRS)
 *
 * The recognition sequence usually follows the CD001 descriptor sequence at
 * sector 16 and is there to indicate that the medium (also) contains a UDF file
 * system and which standards are involved.
 *
 * See @ecma167{2,8,31}, @ecma167{2,9,32}, @udf260{2.1.7,25}.
 *
 * @{ */

/** The type value used for all the extended UDF volume descriptors
 * (ISO9660VOLDESCHDR::bDescType). */
#define UDF_EXT_VOL_DESC_TYPE               0
/** The version value used for all the extended UDF volume descriptors
 *  (ISO9660VOLDESCHDR::bDescVersion). */
#define UDF_EXT_VOL_DESC_VERSION            1

/** Standard ID for UDFEXTVOLDESCBEGIN. */
#define UDF_EXT_VOL_DESC_STD_ID_BEGIN   "BEA01"
/** Standard ID for UDFEXTVOLDESCTERM. */
#define UDF_EXT_VOL_DESC_STD_ID_TERM    "TEA01"
/** Standard ID for UDFEXTVOLDESCNSR following ECMA-167 2nd edition. */
#define UDF_EXT_VOL_DESC_STD_ID_NSR_02  "NSR02"
/** Standard ID for UDFEXTVOLDESCNSR following ECMA-167 3rd edition. */
#define UDF_EXT_VOL_DESC_STD_ID_NSR_03  "NSR03"
/** Standard ID for UDFEXTVOLDESCBOOT. */
#define UDF_EXT_VOL_DESC_STD_ID_BOOT    "BOOT2"


/**
 * Begin UDF extended volume descriptor area (@ecma167{2,9.2,33}).
 */
typedef struct UDFEXTVOLDESCBEGIN
{
    /** The volume descriptor header.
     * The standard identifier is UDF_EXT_VOL_DESC_STD_ID_BEGIN. */
    ISO9660VOLDESCHDR   Hdr;
    /** Zero payload. */
    uint8_t             abZero[2041];
} UDFEXTVOLDESCBEGIN;
AssertCompileSize(UDFEXTVOLDESCBEGIN, 2048);
/** Pointer to an UDF extended volume descriptor indicating the start of the
 * extended descriptor area. */
typedef UDFEXTVOLDESCBEGIN *PUDFEXTVOLDESCBEGIN;
/** Pointer to a const UDF extended volume descriptor indicating the start of
 * the extended descriptor area. */
typedef UDFEXTVOLDESCBEGIN const *PCUDFEXTVOLDESCBEGIN;


/**
 * Terminate UDF extended volume descriptor area (@ecma167{2,9.3,33}).
 */
typedef struct UDFEXTVOLDESCTERM
{
    /** The volume descriptor header.
     * The standard identifier is UDF_EXT_VOL_DESC_STD_ID_TERM. */
    ISO9660VOLDESCHDR   Hdr;
    /** Zero payload. */
    uint8_t             abZero[2041];
} UDFEXTVOLDESCTERM;
AssertCompileSize(UDFEXTVOLDESCTERM, 2048);
/** Pointer to an UDF extended volume descriptor indicating the end of the
 * extended descriptor area. */
typedef UDFEXTVOLDESCTERM *PUDFEXTVOLDESCTERM;
/** Pointer to a const UDF extended volume descriptor indicating the end of
 * the extended descriptor area. */
typedef UDFEXTVOLDESCTERM const *PCUDFEXTVOLDESCTERM;


/**
 * UDF NSR extended volume descriptor (@ecma167{3,9.1,50}).
 *
 * This gives the ECMA standard version.
 */
typedef struct UDFEXTVOLDESCNSR
{
    /** The volume descriptor header.
     * The standard identifier is UDF_EXT_VOL_DESC_STD_ID_NSR_02, or
     * UDF_EXT_VOL_DESC_STD_ID_NSR_03. */
    ISO9660VOLDESCHDR   Hdr;
    /** Zero payload. */
    uint8_t             abZero[2041];
} UDFEXTVOLDESCNSR;
AssertCompileSize(UDFEXTVOLDESCNSR, 2048);
/** Pointer to an extended volume descriptor giving the UDF standard version. */
typedef UDFEXTVOLDESCNSR *PUDFEXTVOLDESCNSR;
/** Pointer to a const extended volume descriptor giving the UDF standard version. */
typedef UDFEXTVOLDESCNSR const *PCUDFEXTVOLDESCNSR;


/**
 * UDF boot extended volume descriptor (@ecma167{2,9.4,34}).
 *
 * @note Probably entirely unused.
 */
typedef struct UDFEXTVOLDESCBOOT
{
    /** 0x00: The volume descriptor header.
     * The standard identifier is UDF_EXT_VOL_DESC_STD_ID_BOOT. */
    ISO9660VOLDESCHDR   Hdr;
    /** 0x07: Reserved/alignment, MBZ. */
    uint8_t             bReserved1;
    /** 0x08: The architecture type. */
    UDFENTITYID         ArchType;
    /** 0x28: The boot identifier. */
    UDFENTITYID         idBoot;
    /** 0x48: Logical sector number of load the boot loader from. */
    uint32_t            offBootExtent;
    /** 0x4c: Number of bytes to load. */
    uint32_t            cbBootExtent;
    /** 0x50: The load address (in memory). */
    uint64_t            uLoadAddress;
    /** 0x58: The start address (in memory). */
    uint64_t            uStartAddress;
    /** 0x60: The descriptor creation timestamp. */
    UDFTIMESTAMP        CreationTimestamp;
    /** 0x6c: Flags. */
    uint16_t            fFlags;
    /** 0x6e: Reserved, MBZ. */
    uint8_t             abReserved2[32];
    /** 0x8e: Implementation use. */
    uint8_t             abBootUse[1906];
} UDFEXTVOLDESCBOOT;
AssertCompileSize(UDFEXTVOLDESCBOOT, 2048);
/** Pointer to a boot extended volume descriptor. */
typedef UDFEXTVOLDESCBOOT *PUDFEXTVOLDESCBOOT;
/** Pointer to a const boot extended volume descriptor. */
typedef UDFEXTVOLDESCBOOT const *PCUDFEXTVOLDESCBOOT;

/** @} */


/** @} */

#endif

