/* $Id$ */
/** @file
 * IPRT - Relocatable Object Module Format (OMF).
 *
 * @remarks For a more details description, see specification from Tools
 *          Interface Standards (TIS), version 1.1 dated May 2015.
 *          Typically named found as OMF_v1.1.pdf.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
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

#ifndef ___iprt_formats_omf_h
#define ___iprt_formats_omf_h

#include <iprt/stdint.h>


/** @defgroup grp_rt_formats_omf     Relocatable Object Module Format (OMF) structures and definitions
 * @ingroup grp_rt_formats
 * @{
 */

/**
 * OMF record header.
 */
#pragma pack(1)
typedef struct OMFRECHDR
{
    /** The record type. */
    uint8_t         bType;
    /** The record length, excluding the this header. */
    uint16_t        cbLen;
} OMFRECHDR;
#pragma pack()
/** Pointer to an OMF header. */
typedef OMFRECHDR *POMFRECHDR;
/** Pointer to a const OMF header. */
typedef OMFRECHDR *PCOMFRECHDR;

/** The max OMF record length, including the header. */
#define OMF_MAX_RECORD_LENGTH  UINT16_C(1024)

/** The max OMF record payload, including CRC byte. */
#define OMF_MAX_RECORD_PAYLOAD UINT16_C(1021)


/** @name OMF Record Types (OMFRECHDR::bType).
 * @{  */
/** Record type flag indicating 32-bit record. */
#define OMF_REC32           UINT8_C(0x01)
/** Object file header record.
 * Is followed by a length prefixed string */
#define OMF_THEADR          UINT8_C(0x80)
/** Comment record.
 * Is followed by a comment type byte and a commen class byte, thereafter comes
 * type specific byte sequence. */
#define OMF_COMENT          UINT8_C(0x88)
/** Local name table referenced by segment and group defintions.
 * Array of length prefixed strings. Multi record. */
#define OMF_LNAMES          UINT8_C(0x96)
/** 16-bit segment definition.
 * Complicated, see TIS docs. */
#define OMF_SEGDEF16        UINT8_C(0x98)
/** 32-bit segment definition.
 * Complicated, see TIS docs. */
#define OMF_SEGDEF32        UINT8_C(0x99)
/** Segment group definition.
 * Starts with an LNAMES index (one or two bytes) of the group name. Followed
 * by an array which entries consists of a 0xff byte and a segment
 * defintion index (one or two bytes). */
#define OMF_GRPDEF          UINT8_C(0x9a)
/** External symbol defintions.
 * Array where each entry is a length prefixed symbol name string followed by a
 * one or two byte type number. */
#define OMF_EXTDEF          UINT8_C(0x8c)
/** 16-but public symbol definitions.
 * Starts with a group index (one or two bytes) and a segment index (ditto)
 * which indicates which group/segment the symbols belong to.
 * Is followed by an array with entries consiting of a length prefixed symbol
 * name string, a two byte segment offset, and a one or two byte type index. */
#define OMF_PUBDEF16        UINT8_C(0x90)
/** 32-but public symbol definitions.
 * Identical to #OMF_PUBDEF16 except that the symbol offset field is four
 * byte. */
#define OMF_PUBDEF32        UINT8_C(0x91)
/** 16-bit local symbol definitions.
 * Same format as #OMF_PUBDEF16.  */
#define OMF_LPUBDEF16       UINT8_C(0xb6)
/** 16-bit local symbol definitions.
 * Same format as #OMF_PUBDEF32.  */
#define OMF_LPUBDEF32       UINT8_C(0xb7)
/** Logical enumerated data record (a chunk of raw segment bits).
 * Starts with the index of the segment it contributes to (one or two bytes) and
 * is followed by the offset into the segment of the bytes (two bytes).
 * After that comes the raw data bytes. */
#define OMF_LEDATA16        UINT8_C(0xa0)
/** Logical enumerated data record (a chunk of raw segment bits).
 * Identical to #OMF_LEDATA16 except that is has a the segment offset field is
 * four bytes. */
#define OMF_LEDATA32        UINT8_C(0xa1)
/** 16-bit fixup record.
 * Complicated, see TIS docs. */
#define OMF_FIXUPP16        UINT8_C(0x9c)
/** 32-bit fixup record.
 * Complicated, see TIS docs. */
#define OMF_FIXUPP32        UINT8_C(0x9d)
/** 16-bit line numbers record. */
#define OMF_LINNUM16        UINT8_C(0x94)
/** 32-bit line numbers record. */
#define OMF_LINNUM32        UINT8_C(0x95)
/** 16-bit object file end record.
 * Duh! wrong bitfield order.
 *
 * Starts with a byte bitfield indicating module type: bit 0 is set if this is a
 * main program module; bit 1 is set if this is a start address is available;
 * bits 2 thru 6 are reserved and must be zero; bit 7 is set to indicate
 * a non-absolute start address.
 *
 * When bit 1 is set what follow is: A FIXUPP byte, one or two byte frame datum,
 * one or two byte target datum, and a 2 byte target displacement. */
#define OMF_MODEND16        UINT8_C(0x8a)
/** 32-bit object file end record.
 * Identical to #OMF_MODEND16 except that is has a 4 byte target
 * displacement field. */
#define OMF_MODEND32        UINT8_C(0x8a)
/** @} */

/** @name OMF COMENT Type Flags
 * @{ */
/** Comment type: Don't remove comment when object is manipulated. */
#define OMF_CTYP_NO_PURGE       UINT8_C(0x80)
/** Comment type: Don't include in object listing. */
#define OMF_CTYP_NO_LIST        UINT8_C(0x40)
/** @} */

/** @name OMF COMENT Classes
 * @{ */
/** Comment class: Dependency file.
 * Is followed by a dword timestamp (1980 based?) and a length prefix
 * filename string. */
#define OMF_CCLS_DEP_FILE       UINT8_C(0x88)
/** Comment class: Link pass separator.
 * Contains a byte with the value 01 to indicate the linker can stop pass 1
 * processing now. */
#define OMF_CCLS_LINK_PASS_SEP  UINT8_C(0xa2)
/** @} */


/** @name OMF FIXUPP Locations.
 * @{ */
#define OMF_FIX_LOC_8BIT_LOW_BYTE    UINT8_C(0)  /**< FIXUP location: low byte (offset or displacement). */
#define OMF_FIX_LOC_16BIT_OFFSET     UINT8_C(1)  /**< FIXUP location: 16-bit offset. */
#define OMF_FIX_LOC_16BIT_SEGMENT    UINT8_C(2)  /**< FIXUP location: 16-bit segment. */
#define OMF_FIX_LOC_1616FAR          UINT8_C(3)  /**< FIXUP location: 16:16 far pointer. */
#define OMF_FIX_LOC_8BIT_HIGH_BYTE   UINT8_C(4)  /**< FIXUP location: high byte (offset). Not supported by MS/IBM. */
#define OMF_FIX_LOC_16BIT_OFFSET_LDR UINT8_C(5)  /**< FIXUP location: 16-bit loader resolved offset, same a 1 for linker. PharLab conflict. */
#define OMF_FIX_LOC_RESERVED_FAR1632 UINT8_C(6)  /**< FIXUP location: PharLab 16:32 far pointers, not defined by MS/IBM. */
#define OMF_FIX_LOC_RESERVED_7       UINT8_C(7)  /**< FIXUP location: Not defined. */
#define OMF_FIX_LOC_RESERVED_8       UINT8_C(8)  /**< FIXUP location: Not defined. */
#define OMF_FIX_LOC_32BIT_OFFSET     UINT8_C(9)  /**< FIXUP location: 32-bit offset. */
#define OMF_FIX_LOC_RESERVED_10      UINT8_C(10) /**< FIXUP location: Not defined. */
#define OMF_FIX_LOC_1632FAR          UINT8_C(11) /**< FIXUP location: 16:32 far pointer. */
#define OMF_FIX_LOC_RESERVED_12      UINT8_C(12) /**< FIXUP location: Not defined. */
#define OMF_FIX_LOC_32BIT_OFFSET_LDR UINT8_C(13) /**< FIXUP location: 32-bit loader resolved offset, same as 9 for linker. */
/** @} */
/** @name OMF FIXUPP Targets
 * @{ */
#define OMF_FIX_T_SEGDEF             UINT8_C(0)  /**< FIXUP target: SEGDEF index. */
#define OMF_FIX_T_GRPDEF             UINT8_C(1)  /**< FIXUP target: GRPDEF index. */
#define OMF_FIX_T_EXTDEF             UINT8_C(2)  /**< FIXUP target: EXTDEF index. */
#define OMF_FIX_T_FRAME_NO           UINT8_C(3)  /**< FIXUP target: Explicit frame number, not supported by MS/IBM. */
#define OMF_FIX_T_SEGDEF_NO_DISP     UINT8_C(4)  /**< FIXUP target: SEGDEF index only, displacement take as 0. */
#define OMF_FIX_T_GRPDEF_NO_DISP     UINT8_C(5)  /**< FIXUP target: GRPDEF index only, displacement take as 0. */
#define OMF_FIX_T_EXTDEF_NO_DISP     UINT8_C(6)  /**< FIXUP target: EXTDEF index only, displacement take as 0. */
/** @} */
/** @name OMF FIXUPP Frames
 * @{ */
#define OMF_FIX_F_SEGDEF             UINT8_C(0)  /**< FIXUP frame: SEGDEF index. */
#define OMF_FIX_F_GRPDEF             UINT8_C(1)  /**< FIXUP frame: GRPDEF index. */
#define OMF_FIX_F_EXTDEF             UINT8_C(2)  /**< FIXUP frame: EXTDEF index. */
#define OMF_FIX_F_FRAME_NO           UINT8_C(3)  /**< FIXUP frame: Explicit frame number, not supported by any linkers. */
#define OMF_FIX_F_LXDATA_SEG         UINT8_C(4)  /**< FIXUP frame: Determined from the data being fixed up. (No index field.) */
#define OMF_FIX_F_TARGET_SEG         UINT8_C(5)  /**< FIXUP frame: Determined from the target. (No index field.) */
#define OMF_FIX_F_RESERVED_6         UINT8_C(6)  /**< FIXUP frame: Reserved. */
/** @} */


/** @} */
#endif

