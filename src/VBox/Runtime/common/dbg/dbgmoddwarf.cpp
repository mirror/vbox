/* $Id$ */
/** @file
 * IPRT - Debug Info Reader For DWARF.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP   RTLOGGROUP_DBG_DWARF
#include <iprt/dbg.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include "internal/dbgmod.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @name Standard DWARF Line Number Opcodes
 * @{ */
#define DW_LNS_extended                    UINT8_C(0x00)
#define DW_LNS_copy                        UINT8_C(0x01)
#define DW_LNS_advance_pc                  UINT8_C(0x02)
#define DW_LNS_advance_line                UINT8_C(0x03)
#define DW_LNS_set_file                    UINT8_C(0x04)
#define DW_LNS_set_column                  UINT8_C(0x05)
#define DW_LNS_negate_stmt                 UINT8_C(0x06)
#define DW_LNS_set_basic_block             UINT8_C(0x07)
#define DW_LNS_const_add_pc                UINT8_C(0x08)
#define DW_LNS_fixed_advance_pc            UINT8_C(0x09)
#define DW_LNS_set_prologue_end            UINT8_C(0x0a)
#define DW_LNS_set_epilogue_begin          UINT8_C(0x0b)
#define DW_LNS_set_isa                     UINT8_C(0x0c)
#define DW_LNS_what_question_mark          UINT8_C(0x0d)
/** @} */


/** @name Extended DWARF Line Number Opcodes
 * @{ */
#define DW_LNE_end_sequence                 UINT8_C(1)
#define DW_LNE_set_address                  UINT8_C(2)
#define DW_LNE_define_file                  UINT8_C(3)
#define DW_LNE_set_descriminator            UINT8_C(4)
/** @} */

/** @name DIE Tags.
 * @{ */
#define DW_TAG_array_type                   UINT16_C(0x0001)
#define DW_TAG_class_type                   UINT16_C(0x0002)
#define DW_TAG_entry_point                  UINT16_C(0x0003)
#define DW_TAG_enumeration_type             UINT16_C(0x0004)
#define DW_TAG_formal_parameter             UINT16_C(0x0005)
#define DW_TAG_imported_declaration         UINT16_C(0x0008)
#define DW_TAG_label                        UINT16_C(0x000a)
#define DW_TAG_lexical_block                UINT16_C(0x000b)
#define DW_TAG_member                       UINT16_C(0x000d)
#define DW_TAG_pointer_type                 UINT16_C(0x000f)
#define DW_TAG_reference_type               UINT16_C(0x0010)
#define DW_TAG_compile_unit                 UINT16_C(0x0011)
#define DW_TAG_string_type                  UINT16_C(0x0012)
#define DW_TAG_structure_type               UINT16_C(0x0013)
#define DW_TAG_subroutine_type              UINT16_C(0x0015)
#define DW_TAG_typedef                      UINT16_C(0x0016)
#define DW_TAG_union_type                   UINT16_C(0x0017)
#define DW_TAG_unspecified_parameters       UINT16_C(0x0018)
#define DW_TAG_variant                      UINT16_C(0x0019)
#define DW_TAG_common_block                 UINT16_C(0x001a)
#define DW_TAG_common_inclusion             UINT16_C(0x001b)
#define DW_TAG_inheritance                  UINT16_C(0x001c)
#define DW_TAG_inlined_subroutine           UINT16_C(0x001d)
#define DW_TAG_module                       UINT16_C(0x001e)
#define DW_TAG_ptr_to_member_type           UINT16_C(0x001f)
#define DW_TAG_set_type                     UINT16_C(0x0020)
#define DW_TAG_subrange_type                UINT16_C(0x0021)
#define DW_TAG_with_stmt                    UINT16_C(0x0022)
#define DW_TAG_access_declaration           UINT16_C(0x0023)
#define DW_TAG_base_type                    UINT16_C(0x0024)
#define DW_TAG_catch_block                  UINT16_C(0x0025)
#define DW_TAG_const_type                   UINT16_C(0x0026)
#define DW_TAG_constant                     UINT16_C(0x0027)
#define DW_TAG_enumerator                   UINT16_C(0x0028)
#define DW_TAG_file_type                    UINT16_C(0x0029)
#define DW_TAG_friend                       UINT16_C(0x002a)
#define DW_TAG_namelist                     UINT16_C(0x002b)
#define DW_TAG_namelist_item                UINT16_C(0x002c)
#define DW_TAG_packed_type                  UINT16_C(0x002d)
#define DW_TAG_subprogram                   UINT16_C(0x002e)
#define DW_TAG_template_type_parameter      UINT16_C(0x002f)
#define DW_TAG_template_value_parameter     UINT16_C(0x0030)
#define DW_TAG_thrown_type                  UINT16_C(0x0031)
#define DW_TAG_try_block                    UINT16_C(0x0032)
#define DW_TAG_variant_part                 UINT16_C(0x0033)
#define DW_TAG_variable                     UINT16_C(0x0034)
#define DW_TAG_volatile_type                UINT16_C(0x0035)
#define DW_TAG_dwarf_procedure              UINT16_C(0x0036)
#define DW_TAG_restrict_type                UINT16_C(0x0037)
#define DW_TAG_interface_type               UINT16_C(0x0038)
#define DW_TAG_namespace                    UINT16_C(0x0039)
#define DW_TAG_imported_module              UINT16_C(0x003a)
#define DW_TAG_unspecified_type             UINT16_C(0x003b)
#define DW_TAG_partial_unit                 UINT16_C(0x003c)
#define DW_TAG_imported_unit                UINT16_C(0x003d)
#define DW_TAG_condition                    UINT16_C(0x003f)
#define DW_TAG_shared_type                  UINT16_C(0x0040)
#define DW_TAG_type_unit                    UINT16_C(0x0041)
#define DW_TAG_rvalue_reference_type        UINT16_C(0x0042)
#define DW_TAG_template_alias               UINT16_C(0x0043)
#define DW_TAG_lo_user                      UINT16_C(0x4080)
#define DW_TAG_hi_user                      UINT16_C(0xffff)
/** @} */


/** @name DIE Attributes.
 * @{ */
#define DW_AT_sibling                       UINT16_C(0x0001)
#define DW_AT_location                      UINT16_C(0x0002)
#define DW_AT_name                          UINT16_C(0x0003)
#define DW_AT_ordering                      UINT16_C(0x0009)
#define DW_AT_byte_size                     UINT16_C(0x000b)
#define DW_AT_bit_offset                    UINT16_C(0x000c)
#define DW_AT_bit_size                      UINT16_C(0x000d)
#define DW_AT_stmt_list                     UINT16_C(0x0010)
#define DW_AT_low_pc                        UINT16_C(0x0011)
#define DW_AT_high_pc                       UINT16_C(0x0012)
#define DW_AT_language                      UINT16_C(0x0013)
#define DW_AT_discr                         UINT16_C(0x0015)
#define DW_AT_discr_value                   UINT16_C(0x0016)
#define DW_AT_visibility                    UINT16_C(0x0017)
#define DW_AT_import                        UINT16_C(0x0018)
#define DW_AT_string_length                 UINT16_C(0x0019)
#define DW_AT_common_reference              UINT16_C(0x001a)
#define DW_AT_comp_dir                      UINT16_C(0x001b)
#define DW_AT_const_value                   UINT16_C(0x001c)
#define DW_AT_containing_type               UINT16_C(0x001d)
#define DW_AT_default_value                 UINT16_C(0x001e)
#define DW_AT_inline                        UINT16_C(0x0020)
#define DW_AT_is_optional                   UINT16_C(0x0021)
#define DW_AT_lower_bound                   UINT16_C(0x0022)
#define DW_AT_producer                      UINT16_C(0x0025)
#define DW_AT_prototyped                    UINT16_C(0x0027)
#define DW_AT_return_addr                   UINT16_C(0x002a)
#define DW_AT_start_scope                   UINT16_C(0x002c)
#define DW_AT_bit_stride                    UINT16_C(0x002e)
#define DW_AT_upper_bound                   UINT16_C(0x002f)
#define DW_AT_abstract_origin               UINT16_C(0x0031)
#define DW_AT_accessibility                 UINT16_C(0x0032)
#define DW_AT_address_class                 UINT16_C(0x0033)
#define DW_AT_artificial                    UINT16_C(0x0034)
#define DW_AT_base_types                    UINT16_C(0x0035)
#define DW_AT_calling_convention            UINT16_C(0x0036)
#define DW_AT_count                         UINT16_C(0x0037)
#define DW_AT_data_member_location          UINT16_C(0x0038)
#define DW_AT_decl_column                   UINT16_C(0x0039)
#define DW_AT_decl_file                     UINT16_C(0x003a)
#define DW_AT_decl_line                     UINT16_C(0x003b)
#define DW_AT_declaration                   UINT16_C(0x003c)
#define DW_AT_discr_list                    UINT16_C(0x003d)
#define DW_AT_encoding                      UINT16_C(0x003e)
#define DW_AT_external                      UINT16_C(0x003f)
#define DW_AT_frame_base                    UINT16_C(0x0040)
#define DW_AT_friend                        UINT16_C(0x0041)
#define DW_AT_identifier_case               UINT16_C(0x0042)
#define DW_AT_macro_info                    UINT16_C(0x0043)
#define DW_AT_namelist_item                 UINT16_C(0x0044)
#define DW_AT_priority                      UINT16_C(0x0045)
#define DW_AT_segment                       UINT16_C(0x0046)
#define DW_AT_specification                 UINT16_C(0x0047)
#define DW_AT_static_link                   UINT16_C(0x0048)
#define DW_AT_type                          UINT16_C(0x0049)
#define DW_AT_use_location                  UINT16_C(0x004a)
#define DW_AT_variable_parameter            UINT16_C(0x004b)
#define DW_AT_virtuality                    UINT16_C(0x004c)
#define DW_AT_vtable_elem_location          UINT16_C(0x004d)
#define DW_AT_allocated                     UINT16_C(0x004e)
#define DW_AT_associated                    UINT16_C(0x004f)
#define DW_AT_data_location                 UINT16_C(0x0050)
#define DW_AT_byte_stride                   UINT16_C(0x0051)
#define DW_AT_entry_pc                      UINT16_C(0x0052)
#define DW_AT_use_UTF8                      UINT16_C(0x0053)
#define DW_AT_extension                     UINT16_C(0x0054)
#define DW_AT_ranges                        UINT16_C(0x0055)
#define DW_AT_trampoline                    UINT16_C(0x0056)
#define DW_AT_call_column                   UINT16_C(0x0057)
#define DW_AT_call_file                     UINT16_C(0x0058)
#define DW_AT_call_line                     UINT16_C(0x0059)
#define DW_AT_description                   UINT16_C(0x005a)
#define DW_AT_binary_scale                  UINT16_C(0x005b)
#define DW_AT_decimal_scale                 UINT16_C(0x005c)
#define DW_AT_small                         UINT16_C(0x005d)
#define DW_AT_decimal_sign                  UINT16_C(0x005e)
#define DW_AT_digit_count                   UINT16_C(0x005f)
#define DW_AT_picture_string                UINT16_C(0x0060)
#define DW_AT_mutable                       UINT16_C(0x0061)
#define DW_AT_threads_scaled                UINT16_C(0x0062)
#define DW_AT_explicit                      UINT16_C(0x0063)
#define DW_AT_object_pointer                UINT16_C(0x0064)
#define DW_AT_endianity                     UINT16_C(0x0065)
#define DW_AT_elemental                     UINT16_C(0x0066)
#define DW_AT_pure                          UINT16_C(0x0067)
#define DW_AT_recursive                     UINT16_C(0x0068)
#define DW_AT_signature                     UINT16_C(0x0069)
#define DW_AT_main_subprogram               UINT16_C(0x006a)
#define DW_AT_data_bit_offset               UINT16_C(0x006b)
#define DW_AT_const_expr                    UINT16_C(0x006c)
#define DW_AT_enum_class                    UINT16_C(0x006d)
#define DW_AT_linkage_name                  UINT16_C(0x006e)
#define DW_AT_lo_user                       UINT16_C(0x2000)
#define DW_AT_hi_user                       UINT16_C(0x3fff)
/** @} */

/** @name DIE Forms.
 * @{ */
#define DW_FORM_addr                        UINT16_C(0x01)
/* What was 0x02? */
#define DW_FORM_block2                      UINT16_C(0x03)
#define DW_FORM_block4                      UINT16_C(0x04)
#define DW_FORM_data2                       UINT16_C(0x05)
#define DW_FORM_data4                       UINT16_C(0x06)
#define DW_FORM_data8                       UINT16_C(0x07)
#define DW_FORM_string                      UINT16_C(0x08)
#define DW_FORM_block                       UINT16_C(0x09)
#define DW_FORM_block1                      UINT16_C(0x0a)
#define DW_FORM_data1                       UINT16_C(0x0b)
#define DW_FORM_flag                        UINT16_C(0x0c)
#define DW_FORM_sdata                       UINT16_C(0x0d)
#define DW_FORM_strp                        UINT16_C(0x0e)
#define DW_FORM_udata                       UINT16_C(0x0f)
#define DW_FORM_ref_addr                    UINT16_C(0x10)
#define DW_FORM_ref1                        UINT16_C(0x11)
#define DW_FORM_ref2                        UINT16_C(0x12)
#define DW_FORM_ref4                        UINT16_C(0x13)
#define DW_FORM_ref8                        UINT16_C(0x14)
#define DW_FORM_ref_udata                   UINT16_C(0x15)
#define DW_FORM_indirect                    UINT16_C(0x16)
#define DW_FORM_sec_offset                  UINT16_C(0x17)
#define DW_FORM_exprloc                     UINT16_C(0x18)
#define DW_FORM_flag_present                UINT16_C(0x19)
#define DW_FORM_ref_sig8                    UINT16_C(0x20)
/** @} */


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * DWARF sections.
 */
typedef enum krtDbgModDwarfSect
{
    krtDbgModDwarfSect_abbrev = 0,
    krtDbgModDwarfSect_aranges,
    krtDbgModDwarfSect_frame,
    krtDbgModDwarfSect_info,
    krtDbgModDwarfSect_inlined,
    krtDbgModDwarfSect_line,
    krtDbgModDwarfSect_loc,
    krtDbgModDwarfSect_macinfo,
    krtDbgModDwarfSect_pubnames,
    krtDbgModDwarfSect_pubtypes,
    krtDbgModDwarfSect_ranges,
    krtDbgModDwarfSect_str,
    krtDbgModDwarfSect_types,
    /** End of valid parts (exclusive). */
    krtDbgModDwarfSect_End
} krtDbgModDwarfSect;

/**
 * Abbreviation cache entry.
 */
typedef struct RTDWARFABBREV
{
    /** Whether this entry is filled in or not. */
    bool                fFilled;
    /** Whether there are children or not. */
    bool                fChildren;
    /** The tag.  */
    uint16_t            uTag;
    /** Offset into the abbrev section of the specification pairs. */
    uint32_t            offSpec;
} RTDWARFABBREV;
/** Pointer to an abbreviation cache entry. */
typedef RTDWARFABBREV *PRTDWARFABBREV;
/** Pointer to a const abbreviation cache entry. */
typedef RTDWARFABBREV const *PCRTDWARFABBREV;


/**
 * The instance data of the DWARF reader.
 */
typedef struct RTDBGMODDWARF
{
    /** The debug container containing doing the real work. */
    RTDBGMOD                hCnt;
    /** Pointer to back to the debug info module (no reference ofc). */
    PRTDBGMODINT            pMod;

    /** DWARF debug info sections. */
    struct
    {
        /** The file offset of the part. */
        RTFOFF              offFile;
        /** The size of the part. */
        size_t              cb;
        /** The memory mapping of the part. */
        void const         *pv;
        /** Set if present. */
        bool                fPresent;
    } aSections[krtDbgModDwarfSect_End];

    /** The offset into the abbreviation section of the current cache. */
    uint32_t                offCachedAbbrev;
    /** The number of cached abbreviations we've allocated space for. */
    uint32_t                cCachedAbbrevsAlloced;
    /** Used for range checking cache lookups. */
    uint32_t                cCachedAbbrevs;
    /** Array of cached abbreviations, indexed by code. */
    PRTDWARFABBREV           paCachedAbbrevs;
    /** Used by rtDwarfAbbrev_Lookup when the result is uncachable. */
    RTDWARFABBREV            LookupAbbrev;
} RTDBGMODDWARF;
/** Pointer to instance data of the DWARF reader. */
typedef RTDBGMODDWARF *PRTDBGMODDWARF;

/**
 * DWARF cursor for reading byte data.
 */
typedef struct RTDWARFSECTRDR
{
    /** The current position. */
    uint8_t const          *pb;
    /** The number of bytes left to read. */
    size_t                  cbLeft;
    /** The number of bytes left to read in the current unit. */
    size_t                  cbUnitLeft;
    /** The DWARF debug info reader instance. */
    PRTDBGMODDWARF          pDwarfMod;
    /** Set if this is 64-bit DWARF, clear if 32-bit. */
    bool                    f64bitDwarf;
    /** Set if the format endian is native, clear if endian needs to be
     * inverted. */
    bool                    fNativEndian;
    /** The size of a native address. */
    uint8_t                 cbNativeAddr;
    /** The cursor status code.  This is VINF_SUCCESS until some error
     *  occurs. */
    int                     rc;
    /** The start of the area covered by the cursor.
     * Used for repositioning the cursor relative to the start of a section. */
    uint8_t const          *pbStart;
    /** The section. */
    krtDbgModDwarfSect      enmSect;
} RTDWARFCURSOR;
/** Pointer to a DWARF section reader. */
typedef RTDWARFCURSOR *PRTDWARFCURSOR;


/**
 * DWARF line number program state.
 */
typedef struct RTDWARFLINESTATE
{
    /** Virtual Line Number Machine Registers. */
    struct
    {
        uint64_t        uAddress;
        uint64_t        idxOp;
        uint32_t        iFile;
        uint32_t        uLine;
        uint32_t        uColumn;
        bool            fIsStatement;
        bool            fBasicBlock;
        bool            fEndSequence;
        bool            fPrologueEnd;
        bool            fEpilogueBegin;
        uint32_t        uIsa;
        uint32_t        uDiscriminator;
    } Regs;
    /** @} */

    /** Header. */
    struct
    {
        uint32_t        uVer;
        uint64_t        offFirstOpcode;
        uint8_t         cbMinInstr;
        uint8_t         cMaxOpsPerInstr;
        uint8_t         u8DefIsStmt;
        int8_t          s8LineBase;
        uint8_t         u8LineRange;
        uint8_t         u8OpcodeBase;
        uint8_t const  *pacStdOperands;
    } Hdr;

    /** @name Include Path Table (0-based)
     * @{ */
    const char    **papszIncPaths;
    uint32_t        cIncPaths;
    /** @} */

    /** @name File Name Table (0-based, dummy zero entry)
     * @{ */
    char          **papszFileNames;
    uint32_t        cFileNames;
    /** @} */

    /** The DWARF debug info reader instance. */
    PRTDBGMODDWARF  pDwarfMod;
} RTDWARFLINESTATE;
/** Pointer to a DWARF line number program state. */
typedef RTDWARFLINESTATE *PRTDWARFLINESTATE;


/** @callback_method_impl{FNRTLDRENUMSEGS} */
static DECLCALLBACK(int) rtDbgModHlpAddSegmentCallback(RTLDRMOD hLdrMod, PCRTLDRSEG pSeg, void *pvUser)
{
    PRTDBGMODINT pMod = (PRTDBGMODINT)pvUser;
    Log(("Segment %.*s: LinkAddress=%#llx RVA=%#llx cb=%#llx\n",
         pSeg->cchName, pSeg->pchName, (uint64_t)pSeg->LinkAddress, (uint64_t)pSeg->RVA, pSeg->cb));
    RTLDRADDR cb = RT_MAX(pSeg->cb, pSeg->cbMapped);
#if 1
    return pMod->pDbgVt->pfnSegmentAdd(pMod, pSeg->RVA, cb, pSeg->pchName, pSeg->cchName, 0 /*fFlags*/, NULL);
#else
    return pMod->pDbgVt->pfnSegmentAdd(pMod, pSeg->LinkAddress, cb, pSeg->pchName, pSeg->cchName, 0 /*fFlags*/, NULL);
#endif
}


/**
 * Calls pfnSegmentAdd for each segment in the executable image.
 *
 * @returns IPRT status code.
 * @param   pMod                The debug module.
 */
DECLHIDDEN(int) rtDbgModHlpAddSegmentsFromImage(PRTDBGMODINT pMod)
{
    AssertReturn(pMod->pImgVt, VERR_INTERNAL_ERROR_2);
    return pMod->pImgVt->pfnEnumSegments(pMod, rtDbgModHlpAddSegmentCallback, pMod);
}




/**
 * Loads a DWARF section from the image file.
 *
 * @returns IPRT status code.
 * @param   pThis               The DWARF instance.
 * @param   enmSect             The section to load.
 */
static int rtDbgModDwarfLoadSection(PRTDBGMODDWARF pThis, krtDbgModDwarfSect enmSect)
{
    /*
     * Don't load stuff twice.
     */
    if (pThis->aSections[enmSect].pv)
        return VINF_SUCCESS;

    /*
     * Sections that are not present cannot be loaded, treat them like they
     * are empty
     */
    if (!pThis->aSections[enmSect].fPresent)
    {
        Assert(pThis->aSections[enmSect].cb);
        return VINF_SUCCESS;
    }
    if (!pThis->aSections[enmSect].cb)
        return VINF_SUCCESS;

    /*
     * Sections must be readable with the current image interface.
     */
    if (pThis->aSections[enmSect].offFile < 0)
        return VERR_OUT_OF_RANGE;

    /*
     * Do the job.
     */
    return pThis->pMod->pImgVt->pfnMapPart(pThis->pMod, pThis->aSections[enmSect].offFile, pThis->aSections[enmSect].cb,
                                           &pThis->aSections[enmSect].pv);
}


/**
 * Unloads a DWARF section previously mapped by rtDbgModDwarfLoadSection.
 *
 * @returns IPRT status code.
 * @param   pThis               The DWARF instance.
 * @param   enmSect             The section to unload.
 */
static int rtDbgModDwarfUnloadSection(PRTDBGMODDWARF pThis, krtDbgModDwarfSect enmSect)
{
    if (!pThis->aSections[enmSect].pv)
        return VINF_SUCCESS;

    int rc = pThis->pMod->pImgVt->pfnUnmapPart(pThis->pMod, pThis->aSections[enmSect].cb, &pThis->aSections[enmSect].pv);
    AssertRC(rc);
    return rc;
}


/**
 * Converts to UTF-8 or otherwise makes sure it's valid UTF-8.
 *
 * @returns IPRT status code.
 * @param   pThis               The DWARF instance.
 * @param   ppsz                Pointer to the string pointer.  May be
 *                              reallocated (RTStr*).
 */
static int rtDbgModDwarfStringToUtf8(PRTDBGMODDWARF pThis, char **ppsz)
{
    RTStrPurgeEncoding(*ppsz);
    return VINF_SUCCESS;
}


/**
 * Convers a link address into a segment+offset or RVA.
 *
 * @returns IPRT status code.
 * @param   pThis           The DWARF instance.
 * @param   LinkAddress     The address to convert..
 * @param   piSeg           The segment index.
 * @param   poffSeg         Where to return the segment offset.
 */
static int rtDbgModDwarfLinkAddressToSegOffset(PRTDBGMODDWARF pThis, uint64_t LinkAddress,
                                               PRTDBGSEGIDX piSeg, PRTLDRADDR poffSeg)
{
    return pThis->pMod->pImgVt->pfnLinkAddressToSegOffset(pThis->pMod, LinkAddress, piSeg, poffSeg);
}


/*
 *
 * DWARF Cursor.
 * DWARF Cursor.
 * DWARF Cursor.
 *
 */


/**
 * Reads a 8-bit unsigned integer and advances the cursor.
 *
 * @returns 8-bit unsigned integer. On error RTDWARFCURSOR::rc is set and @a
 *          uErrValue is returned.
 * @param   pCursor             The cursor.
 * @param   uErrValue           What to return on read error.
 */
static uint8_t rtDwarfCursor_GetU8(PRTDWARFCURSOR pCursor, uint8_t uErrValue)
{
    if (pCursor->cbUnitLeft < 1)
    {
        pCursor->rc = VERR_DWARF_UNEXPECTED_END;
        return uErrValue;
    }

    uint8_t u8 = pCursor->pb[0];
    pCursor->pb         += 1;
    pCursor->cbUnitLeft -= 1;
    pCursor->cbLeft     -= 1;
    return u8;
}


/**
 * Reads a 16-bit unsigned integer and advances the cursor.
 *
 * @returns 16-bit unsigned integer. On error RTDWARFCURSOR::rc is set and @a
 *          uErrValue is returned.
 * @param   pCursor             The cursor.
 * @param   uErrValue           What to return on read error.
 */
static uint16_t rtDwarfCursor_GetU16(PRTDWARFCURSOR pCursor, uint16_t uErrValue)
{
    if (pCursor->cbUnitLeft < 2)
    {
        pCursor->pb         += pCursor->cbUnitLeft;
        pCursor->cbLeft     -= pCursor->cbUnitLeft;
        pCursor->cbUnitLeft  = 0;
        pCursor->rc          = VERR_DWARF_UNEXPECTED_END;
        return uErrValue;
    }

    uint16_t u16 = RT_MAKE_U16(pCursor->pb[0], pCursor->pb[1]);
    pCursor->pb         += 2;
    pCursor->cbUnitLeft -= 2;
    pCursor->cbLeft     -= 2;
    if (!pCursor->fNativEndian)
        u16 = RT_BSWAP_U16(u16);
    return u16;
}


/**
 * Reads a 32-bit unsigned integer and advances the cursor.
 *
 * @returns 32-bit unsigned integer. On error RTDWARFCURSOR::rc is set and @a
 *          uErrValue is returned.
 * @param   pCursor             The cursor.
 * @param   uErrValue           What to return on read error.
 */
static uint32_t rtDwarfCursor_GetU32(PRTDWARFCURSOR pCursor, uint32_t uErrValue)
{
    if (pCursor->cbUnitLeft < 4)
    {
        pCursor->pb         += pCursor->cbUnitLeft;
        pCursor->cbLeft     -= pCursor->cbUnitLeft;
        pCursor->cbUnitLeft  = 0;
        pCursor->rc          = VERR_DWARF_UNEXPECTED_END;
        return uErrValue;
    }

    uint32_t u32 = RT_MAKE_U32_FROM_U8(pCursor->pb[0], pCursor->pb[1], pCursor->pb[2], pCursor->pb[3]);
    pCursor->pb         += 4;
    pCursor->cbUnitLeft -= 4;
    pCursor->cbLeft     -= 4;
    if (!pCursor->fNativEndian)
        u32 = RT_BSWAP_U32(u32);
    return u32;
}


/**
 * Reads a 64-bit unsigned integer and advances the cursor.
 *
 * @returns 64-bit unsigned integer. On error RTDWARFCURSOR::rc is set and @a
 *          uErrValue is returned.
 * @param   pCursor             The cursor.
 * @param   uErrValue           What to return on read error.
 */
static uint64_t rtDwarfCursor_GetU64(PRTDWARFCURSOR pCursor, uint64_t uErrValue)
{
    if (pCursor->cbUnitLeft < 8)
    {
        pCursor->pb         += pCursor->cbUnitLeft;
        pCursor->cbLeft     -= pCursor->cbUnitLeft;
        pCursor->cbUnitLeft  = 0;
        pCursor->rc          = VERR_DWARF_UNEXPECTED_END;
        return uErrValue;
    }

    uint64_t u64 = RT_MAKE_U64_FROM_U8(pCursor->pb[0], pCursor->pb[1], pCursor->pb[2], pCursor->pb[3],
                                       pCursor->pb[4], pCursor->pb[5], pCursor->pb[6], pCursor->pb[7]);
    pCursor->pb         += 8;
    pCursor->cbUnitLeft -= 8;
    pCursor->cbLeft     -= 8;
    if (!pCursor->fNativEndian)
        u64 = RT_BSWAP_U64(u64);
    return u64;
}


/**
 * Reads an unsigned LEB128 encoded number.
 *
 * @returns unsigned 64-bit number. On error RTDWARFCURSOR::rc is set and @a
 *          uErrValue is returned.
 * @param   pCursor             The cursor.
 * @param   uErrValue           The value to return on error.
 */
static uint64_t rtDwarfCursor_GetULeb128(PRTDWARFCURSOR pCursor, uint64_t uErrValue)
{
    if (pCursor->cbUnitLeft < 1)
    {
        pCursor->rc = VERR_DWARF_UNEXPECTED_END;
        return uErrValue;
    }

    /*
     * Special case - single byte.
     */
    uint8_t b = pCursor->pb[0];
    if (!(b & 0x80))
    {
        pCursor->pb         += 1;
        pCursor->cbUnitLeft -= 1;
        pCursor->cbLeft     -= 1;
        return b;
    }

    /*
     * Generic case.
     */
    /* Decode. */
    uint32_t off    = 1;
    uint64_t u64Ret = b & 0x7f;
    do
    {
        if (off == pCursor->cbUnitLeft)
        {
            pCursor->rc = VERR_DWARF_UNEXPECTED_END;
            u64Ret = uErrValue;
            break;
        }
        b = pCursor->pb[off];
        u64Ret |= (b & 0x7f) << off * 7;
        off++;
    } while (b & 0x80);

    /* Update the cursor. */
    pCursor->pb         += off;
    pCursor->cbUnitLeft -= off;
    pCursor->cbLeft     -= off;

    /* Check the range. */
    uint32_t cBits = off * 7;
    if (cBits > 64)
    {
        pCursor->rc = VERR_DWARF_LEB_OVERFLOW;
        u64Ret = uErrValue;
    }

    return u64Ret;
}


/**
 * Reads a signed LEB128 encoded number.
 *
 * @returns signed 64-bit number. On error RTDWARFCURSOR::rc is set and @a
 *          uErrValue is returned.
 * @param   pCursor             The cursor.
 * @param   sErrValue           The value to return on error.
 */
static int64_t rtDwarfCursor_GetSLeb128(PRTDWARFCURSOR pCursor, int64_t sErrValue)
{
    if (pCursor->cbUnitLeft < 1)
    {
        pCursor->rc = VERR_DWARF_UNEXPECTED_END;
        return sErrValue;
    }

    /*
     * Special case - single byte.
     */
    uint8_t b = pCursor->pb[0];
    if (!(b & 0x80))
    {
        pCursor->pb         += 1;
        pCursor->cbUnitLeft -= 1;
        pCursor->cbLeft     -= 1;
        if (b & 0x40)
            b |= 0x80;
        return (int8_t)b;
    }

    /*
     * Generic case.
     */
    /* Decode it. */
    uint32_t off    = 1;
    uint64_t u64Ret = b & 0x7f;
    do
    {
        if (off == pCursor->cbUnitLeft)
        {
            pCursor->rc = VERR_DWARF_UNEXPECTED_END;
            u64Ret = (uint64_t)sErrValue;
            break;
        }
        b = pCursor->pb[off];
        u64Ret |= (b & 0x7f) << off * 7;
        off++;
    } while (b & 0x80);

    /* Update cursor. */
    pCursor->pb         += off;
    pCursor->cbUnitLeft -= off;
    pCursor->cbLeft     -= off;

    /* Check the range. */
    uint32_t cBits = off * 7;
    if (cBits > 64)
    {
        pCursor->rc = VERR_DWARF_LEB_OVERFLOW;
        u64Ret = (uint64_t)sErrValue;
    }
    /* Sign extend the value. */
    else if (u64Ret & RT_BIT_64(cBits - 1))
        u64Ret |= ~(RT_BIT_64(cBits - 1) - 1);

    return (int64_t)u64Ret;
}


/**
 * Reads an unsigned LEB128 encoded number, max 32-bit width.
 *
 * @returns unsigned 32-bit number. On error RTDWARFCURSOR::rc is set and @a
 *          uErrValue is returned.
 * @param   pCursor             The cursor.
 * @param   uErrValue           The value to return on error.
 */
static uint32_t rtDwarfCursor_GetULeb128AsU32(PRTDWARFCURSOR pCursor, uint32_t uErrValue)
{
    uint64_t u64 = rtDwarfCursor_GetULeb128(pCursor, uErrValue);
    if (u64 > UINT32_MAX)
    {
        pCursor->rc = VERR_DWARF_LEB_OVERFLOW;
        return uErrValue;
    }
    return (uint32_t)u64;
}


/**
 * Reads a signed LEB128 encoded number, max 32-bit width.
 *
 * @returns signed 32-bit number. On error RTDWARFCURSOR::rc is set and @a
 *          uErrValue is returned.
 * @param   pCursor             The cursor.
 * @param   sErrValue           The value to return on error.
 */
static int32_t rtDwarfCursor_GetSLeb128AsS32(PRTDWARFCURSOR pCursor, int32_t sErrValue)
{
    int64_t s64 = rtDwarfCursor_GetSLeb128(pCursor, sErrValue);
    if (s64 > INT32_MAX || s64 < INT32_MIN)
    {
        pCursor->rc = VERR_DWARF_LEB_OVERFLOW;
        return sErrValue;
    }
    return (int32_t)s64;
}


/**
 * Skips a LEB128 encoded number.
 *
 * @returns IPRT status code.
 * @param   pCursor             The cursor.
 */
static int rtDwarfCursor_SkipLeb128(PRTDWARFCURSOR pCursor)
{
    if (RT_FAILURE(pCursor->rc))
        return pCursor->rc;

    if (pCursor->cbUnitLeft < 1)
        return pCursor->rc = VERR_DWARF_UNEXPECTED_END;

    uint32_t offSkip = 1;
    if (pCursor->pb[0] & 0x80)
        do
        {
            if (offSkip == pCursor->cbUnitLeft)
            {
                pCursor->rc = VERR_DWARF_UNEXPECTED_END;
                break;
            }
        } while (pCursor->pb[offSkip++] & 0x80);

    pCursor->pb         += offSkip;
    pCursor->cbUnitLeft -= offSkip;
    pCursor->cbLeft     -= offSkip;
    return pCursor->rc;
}


/**
 * Advances the cursor a given number of bytes.
 *
 * @returns IPRT status code.
 * @param   pCursor             The cursor.
 * @param   offSkip             The number of bytes to advance.
 */
static int rtDwarfCursor_SkipBytes(PRTDWARFCURSOR pCursor, uint64_t offSkip)
{
    if (RT_FAILURE(pCursor->rc))
        return pCursor->rc;
    if (pCursor->cbUnitLeft < offSkip)
        return pCursor->rc = VERR_DWARF_UNEXPECTED_END;

    size_t const offSkipSizeT = (size_t)offSkip;
    pCursor->cbUnitLeft -= offSkipSizeT;
    pCursor->cbLeft     -= offSkipSizeT;
    pCursor->pb         += offSkipSizeT;

    return VINF_SUCCESS;
}


/**
 * Reads a zero terminated string, advancing the cursor beyond the terminator.
 *
 * @returns Pointer to the string.
 * @param   pCursor             The cursor.
 * @param   pszErrValue         What to return if the string isn't terminated
 *                              before the end of the unit.
 */
static const char *rtDwarfCursor_GetSZ(PRTDWARFCURSOR pCursor, const char *pszErrValue)
{
    const char *pszRet = (const char *)pCursor->pb;
    for (;;)
    {
        if (!pCursor->cbUnitLeft)
        {
            pCursor->rc = VERR_DWARF_BAD_STRING;
            return pszErrValue;
        }
        pCursor->cbUnitLeft--;
        pCursor->cbLeft--;
        if (!*pCursor->pb++)
            break;
    }
    return pszRet;
}


/**
 * Reads a 1, 2, 4 or 8 byte unsgined value.
 *
 * @returns 64-bit unsigned value.
 * @param   pCursor             The cursor.
 * @param   cbValue             The value size.
 * @param   uErrValue           The error value.
 */
static uint64_t rtDwarfCursor_GetVarSizedU(PRTDWARFCURSOR pCursor, size_t cbValue, uint64_t uErrValue)
{
    uint64_t u64Ret;
    switch (cbValue)
    {
        case 1: u64Ret = rtDwarfCursor_GetU8( pCursor, UINT8_MAX); break;
        case 2: u64Ret = rtDwarfCursor_GetU16(pCursor, UINT16_MAX); break;
        case 4: u64Ret = rtDwarfCursor_GetU32(pCursor, UINT32_MAX); break;
        case 8: u64Ret = rtDwarfCursor_GetU64(pCursor, UINT64_MAX); break;
        default:
            pCursor->rc = VERR_DWARF_BAD_INFO;
            return uErrValue;
    }
    if (RT_FAILURE(pCursor->rc))
        return uErrValue;
    return u64Ret;
}


/**
 * Reads an unsigned DWARF half number.
 *
 * @returns The number. On error RTDWARFCURSOR::rc is set and @a
 *          uErrValue is returned.
 * @param   pCursor             The cursor.
 * @param   uErrValue           What to return on error.
 */
static uint16_t rtDwarfCursor_GetUHalf(PRTDWARFCURSOR pCursor, uint16_t uErrValue)
{
    return rtDwarfCursor_GetU16(pCursor, uErrValue);
}


/**
 * Reads an unsigned DWARF byte number.
 *
 * @returns The number. On error RTDWARFCURSOR::rc is set and @a
 *          uErrValue is returned.
 * @param   pCursor             The cursor.
 * @param   uErrValue           What to return on error.
 */
static uint8_t rtDwarfCursor_GetUByte(PRTDWARFCURSOR pCursor, uint8_t uErrValue)
{
    return rtDwarfCursor_GetU8(pCursor, uErrValue);
}


/**
 * Reads a signed DWARF byte number.
 *
 * @returns The number. On error RTDWARFCURSOR::rc is set and @a
 *          uErrValue is returned.
 * @param   pCursor             The cursor.
 * @param   uErrValue           What to return on error.
 */
static int8_t rtDwarfCursor_GetSByte(PRTDWARFCURSOR pCursor, int8_t iErrValue)
{
    return (int8_t)rtDwarfCursor_GetU8(pCursor, (uint8_t)iErrValue);
}


/**
 * Reads a unsigned DWARF offset value.
 *
 * @returns The value. On error RTDWARFCURSOR::rc is set and @a
 *          uErrValue is returned.
 * @param   pCursor             The cursor.
 * @param   uErrValue           What to return on error.
 */
static uint64_t rtDwarfCursor_GetUOff(PRTDWARFCURSOR pCursor, uint64_t uErrValue)
{
    if (pCursor->f64bitDwarf)
        return rtDwarfCursor_GetU64(pCursor, uErrValue);
    return rtDwarfCursor_GetU32(pCursor, (uint32_t)uErrValue);
}


/**
 * Reads a unsigned DWARF native offset value.
 *
 * @returns The value. On error RTDWARFCURSOR::rc is set and @a
 *          uErrValue is returned.
 * @param   pCursor             The cursor.
 * @param   uErrValue           What to return on error.
 */
static uint64_t rtDwarfCursor_GetNativeUOff(PRTDWARFCURSOR pCursor, uint64_t uErrValue)
{
    switch (pCursor->cbNativeAddr)
    {
        case 1: return rtDwarfCursor_GetU8(pCursor,  (uint8_t )uErrValue);
        case 2: return rtDwarfCursor_GetU16(pCursor, (uint16_t)uErrValue);
        case 4: return rtDwarfCursor_GetU32(pCursor, (uint32_t)uErrValue);
        case 8: return rtDwarfCursor_GetU64(pCursor, uErrValue);
        default:
            pCursor->rc = VERR_INTERNAL_ERROR_2;
            return uErrValue;
    }
}


/**
 * Gets the unit length, updating the unit length member and DWARF bitness
 * members of the cursor.
 *
 * @returns The unit length.
 * @param   pCursor             The cursor.
 */
static uint64_t rtDwarfCursor_GetInitalLength(PRTDWARFCURSOR pCursor)
{
    /*
     * Read the initial length.
     */
    pCursor->cbUnitLeft = pCursor->cbLeft;
    uint64_t cbUnit = rtDwarfCursor_GetU32(pCursor, 0);
    if (cbUnit != UINT32_C(0xffffffff))
        pCursor->f64bitDwarf = false;
    else
    {
        pCursor->f64bitDwarf = true;
        cbUnit = rtDwarfCursor_GetU64(pCursor, 0);
    }


    /*
     * Set the unit length, quitely fixing bad lengths.
     */
    pCursor->cbUnitLeft = (size_t)cbUnit;
    if (   pCursor->cbUnitLeft > pCursor->cbLeft
        || pCursor->cbUnitLeft != cbUnit)
        pCursor->cbUnitLeft = pCursor->cbLeft;

    return cbUnit;
}


/**
 * Calculates the section offset corresponding to the current cursor position.
 *
 * @returns 32-bit section offset. If out of range, RTDWARFCURSOR::rc will be
 *          set and UINT32_MAX returned.
 * @param   pCursor             The cursor.
 */
static uint32_t rtDwarfCursor_CalcSectOffsetU32(PRTDWARFCURSOR pCursor)
{
    size_t off = pCursor->pb - (uint8_t const *)pCursor->pDwarfMod->aSections[pCursor->enmSect].pv;
    uint32_t offRet = (uint32_t)off;
    if (offRet != off)
    {
        pCursor->rc = VERR_OUT_OF_RANGE;
        offRet = UINT32_MAX;
    }
    return offRet;
}


/**
 * Calculates an absolute cursor position from one relative to the current
 * cursor position.
 *
 * @returns The absolute cursor position.
 * @param   pCursor             The cursor.
 * @param   offRelative         The relative position.  Must be a positive
 *                              offset.
 */
static uint8_t const *rtDwarfCursor_CalcPos(PRTDWARFCURSOR pCursor, size_t offRelative)
{
    if (offRelative > pCursor->cbUnitLeft)
    {
        Log(("rtDwarfCursor_CalcPos: bad position %#zx, cbUnitLeft=%#zu\n", offRelative, pCursor->cbUnitLeft));
        pCursor->rc = VERR_DWARF_BAD_POS;
        return NULL;
    }
    return pCursor->pb + offRelative;
}


/**
 * Advances the cursor to the given position.
 *
 * @returns IPRT status code.
 * @param   pCursor             The cursor.
 * @param   pbNewPos            The new position - returned by
 *                              rtDwarfCursor_CalcPos().
 */
static int rtDwarfCursor_AdvanceToPos(PRTDWARFCURSOR pCursor, uint8_t const *pbNewPos)
{
    if (RT_FAILURE(pCursor->rc))
        return pCursor->rc;
    AssertPtr(pbNewPos);
    if ((uintptr_t)pbNewPos < (uintptr_t)pCursor->pb)
    {
        Log(("rtDwarfCursor_AdvanceToPos: bad position %p, current %p\n", pbNewPos, pCursor->pb));
        return pCursor->rc = VERR_DWARF_BAD_POS;
    }

    uintptr_t cbAdj = (uintptr_t)pbNewPos - (uintptr_t)pCursor->pb;
    if (RT_UNLIKELY(cbAdj > pCursor->cbUnitLeft))
    {
        AssertFailed();
        pCursor->rc = VERR_DWARF_BAD_POS;
        cbAdj = pCursor->cbUnitLeft;
    }

    pCursor->cbUnitLeft -= cbAdj;
    pCursor->cbLeft     -= cbAdj;
    pCursor->pb         += cbAdj;
    return pCursor->rc;
}


/**
 * Check if the cursor is at the end of the current DWARF unit.
 *
 * @retval  @c true if at the end or a cursor error is pending.
 * @retval  @c false if not.
 * @param   pCursor             The cursor.
 */
static bool rtDwarfCursor_IsAtEndOfUnit(PRTDWARFCURSOR pCursor)
{
    return !pCursor->cbUnitLeft || RT_FAILURE(pCursor->rc);
}


/**
 * Skips to the end of the current unit.
 *
 * @returns IPRT status code.
 * @param   pCursor             The cursor.
 */
static int rtDwarfCursor_SkipUnit(PRTDWARFCURSOR pCursor)
{
    pCursor->pb        += pCursor->cbUnitLeft;
    pCursor->cbLeft    -= pCursor->cbUnitLeft;
    pCursor->cbUnitLeft = 0;
    return pCursor->rc;
}


/**
 * Check if the cursor is at the end of the section (or whatever the cursor is
 * processing).
 *
 * @retval  @c true if at the end or a cursor error is pending.
 * @retval  @c false if not.
 * @param   pCursor             The cursor.
 */
static bool rtDwarfCursor_IsAtEnd(PRTDWARFCURSOR pCursor)
{
    return !pCursor->cbLeft || RT_FAILURE(pCursor->rc);
}


/**
 * Initialize a section reader cursor.
 *
 * @returns IPRT status code.
 * @param   pCursor             The cursor.
 * @param   pThis               The dwarf module.
 * @param   enmSect             The name of the section to read.
 */
static int rtDwarfCursor_Init(PRTDWARFCURSOR pCursor, PRTDBGMODDWARF pThis, krtDbgModDwarfSect enmSect)
{
    int rc = rtDbgModDwarfLoadSection(pThis, enmSect);
    if (RT_FAILURE(rc))
        return rc;

    pCursor->enmSect          = enmSect;
    pCursor->pbStart          = (uint8_t const *)pThis->aSections[enmSect].pv;
    pCursor->pb               = pCursor->pbStart;
    pCursor->cbLeft           = pThis->aSections[enmSect].cb;
    pCursor->cbUnitLeft       = pCursor->cbLeft;
    pCursor->pDwarfMod        = pThis;
    pCursor->f64bitDwarf      = false;
    /** @todo ask the image about the endian used as well as the address
     *        width. */
    pCursor->fNativEndian     = true;
    pCursor->cbNativeAddr     = 4;
    pCursor->rc               = VINF_SUCCESS;

    return VINF_SUCCESS;
}


/**
 * Initialize a section reader cursor with an offset.
 *
 * @returns IPRT status code.
 * @param   pCursor             The cursor.
 * @param   pThis               The dwarf module.
 * @param   enmSect             The name of the section to read.
 * @param   offSect             The offset into the section.
 */
static int rtDwarfCursor_InitWithOffset(PRTDWARFCURSOR pCursor, PRTDBGMODDWARF pThis,
                                        krtDbgModDwarfSect enmSect, uint32_t offSect)
{
    if (offSect > pThis->aSections[enmSect].cb)
    {
        Log(("rtDwarfCursor_InitWithOffset: offSect=%#x cb=%#x enmSect=%d\n", offSect, pThis->aSections[enmSect].cb, enmSect));
        return VERR_DWARF_BAD_POS;
    }

    int rc = rtDwarfCursor_Init(pCursor, pThis, enmSect);
    if (RT_SUCCESS(rc))
    {
        pCursor->pbStart    += offSect;
        pCursor->pb         += offSect;
        pCursor->cbLeft     -= offSect;
        pCursor->cbUnitLeft -= offSect;
    }

    return rc;
}


/**
 * Deletes a section reader initialized by rtDwarfCursor_Init.
 *
 * @returns @a rcOther or RTDWARCURSOR::rc.
 * @param   pCursor             The section reader.
 * @param   rcOther             Other error code to be returned if it indicates
 *                              error or if the cursor status is OK.
 */
static int rtDwarfCursor_Delete(PRTDWARFCURSOR pCursor, int rcOther)
{
    /* ... and a drop of poison. */
    pCursor->pb         = NULL;
    pCursor->cbLeft     = ~(size_t)0;
    pCursor->cbUnitLeft = ~(size_t)0;
    pCursor->pDwarfMod  = NULL;
    if (RT_FAILURE(pCursor->rc) && RT_SUCCESS(rcOther))
        rcOther = pCursor->rc;
    pCursor->rc         = VERR_INTERNAL_ERROR_4;
    return rcOther;
}


/*
 *
 * DWARF Line Numbers.
 * DWARF Line Numbers.
 * DWARF Line Numbers.
 *
 */


/**
 * Defines a file name.
 *
 * @returns IPRT status code.
 * @param   pLnState            The line number program state.
 * @param   pszFilename         The name of the file.
 * @param   idxInc              The include path index.
 */
static int rtDwarfLine_DefineFileName(PRTDWARFLINESTATE pLnState, const char *pszFilename, uint64_t idxInc)
{
    /*
     * Resize the array if necessary.
     */
    uint32_t iFileName = pLnState->cFileNames;
    if ((iFileName % 2) == 0)
    {
        void *pv = RTMemRealloc(pLnState->papszFileNames, sizeof(pLnState->papszFileNames[0]) * (iFileName + 2));
        if (!pv)
            return VERR_NO_MEMORY;
        pLnState->papszFileNames = (char **)pv;
    }

    /*
     * Add the file name.
     */
    if (   pszFilename[0] == '/'
        || pszFilename[0] == '\\'
        || (RT_C_IS_ALPHA(pszFilename[0]) && pszFilename[1] == ':') )
        pLnState->papszFileNames[iFileName] = RTStrDup(pszFilename);
    else if (idxInc < pLnState->cIncPaths)
        pLnState->papszFileNames[iFileName] = RTPathJoinA(pLnState->papszIncPaths[idxInc], pszFilename);
    else
        return VERR_DWARF_BAD_LINE_NUMBER_HEADER;
    if (!pLnState->papszFileNames[iFileName])
        return VERR_NO_STR_MEMORY;
    pLnState->cFileNames = iFileName + 1;

    /*
     * Sanitize the name.
     */
    int rc = rtDbgModDwarfStringToUtf8(pLnState->pDwarfMod, &pLnState->papszFileNames[iFileName]);
    Log(("  File #%02u = '%s'\n", iFileName, pLnState->papszFileNames[iFileName]));
    return rc;
}


/**
 * Adds a line to the table and resets parts of the state (DW_LNS_copy).
 *
 * @returns IPRT status code
 * @param   pLnState            The line number program state.
 * @param   offOpCode           The opcode offset (for logging
 *                              purposes).
 */
static int rtDwarfLine_AddLine(PRTDWARFLINESTATE pLnState, uint32_t offOpCode)
{
    const char *pszFile = pLnState->Regs.iFile < pLnState->cFileNames
                        ? pLnState->papszFileNames[pLnState->Regs.iFile]
                        : "<bad file name index>";
    RTDBGSEGIDX iSeg;
    RTUINTPTR   offSeg;
    int rc = rtDbgModDwarfLinkAddressToSegOffset(pLnState->pDwarfMod, pLnState->Regs.uAddress, &iSeg, &offSeg);
    if (RT_SUCCESS(rc))
    {
        Log2(("rtDwarfLine_AddLine: %x:%08llx (%#llx) %s(%d) [offOpCode=%08x]\n", iSeg, offSeg, pLnState->Regs.uAddress, pszFile, pLnState->Regs.uLine, offOpCode));
        rc = RTDbgModLineAdd(pLnState->pDwarfMod->hCnt, pszFile, pLnState->Regs.uLine, iSeg, offSeg, NULL);

        /* Ignore address conflicts for now. */
        if (rc == VERR_DBG_ADDRESS_CONFLICT)
            rc = VINF_SUCCESS;
    }

    pLnState->Regs.fBasicBlock    = false;
    pLnState->Regs.fPrologueEnd   = false;
    pLnState->Regs.fEpilogueBegin = false;
    pLnState->Regs.uDiscriminator = 0;
    return rc;
}


/**
 * Reset the program to the start-of-sequence state.
 *
 * @param   pLnState            The line number program state.
 */
static void rtDwarfLine_ResetState(PRTDWARFLINESTATE pLnState)
{
    pLnState->Regs.uAddress         = 0;
    pLnState->Regs.idxOp            = 0;
    pLnState->Regs.iFile            = 1;
    pLnState->Regs.uLine            = 1;
    pLnState->Regs.uColumn          = 0;
    pLnState->Regs.fIsStatement     = RT_BOOL(pLnState->Hdr.u8DefIsStmt);
    pLnState->Regs.fBasicBlock      = false;
    pLnState->Regs.fEndSequence     = false;
    pLnState->Regs.fPrologueEnd     = false;
    pLnState->Regs.fEpilogueBegin   = false;
    pLnState->Regs.uIsa             = 0;
    pLnState->Regs.uDiscriminator   = 0;
}


/**
 * Runs the line number program.
 *
 * @returns IPRT status code.
 * @param   pLnState            The line number program state.
 * @param   pCursor             The cursor.
 */
static int rtDwarfLine_RunProgram(PRTDWARFLINESTATE pLnState, PRTDWARFCURSOR pCursor)
{
    LogFlow(("rtDwarfLine_RunProgram: cbUnitLeft=%zu\n", pCursor->cbUnitLeft));

    int rc = VINF_SUCCESS;
    rtDwarfLine_ResetState(pLnState);

    while (!rtDwarfCursor_IsAtEndOfUnit(pCursor))
    {
#ifdef LOG_ENABLED
        uint32_t const offOpCode = rtDwarfCursor_CalcSectOffsetU32(pCursor);
#else
        uint32_t const offOpCode = 0;
#endif
        uint8_t        bOpCode   = rtDwarfCursor_GetUByte(pCursor, DW_LNS_extended);
        if (bOpCode >= pLnState->Hdr.u8OpcodeBase)
        {
            /*
             * Special opcode.
             */
            uint8_t const bLogOpCode = bOpCode; NOREF(bLogOpCode);
            bOpCode -= pLnState->Hdr.u8OpcodeBase;

            int32_t const cLineDelta = bOpCode % pLnState->Hdr.u8LineRange + (int32_t)pLnState->Hdr.s8LineBase;
            bOpCode /= pLnState->Hdr.u8LineRange;

            uint64_t uTmp = bOpCode + pLnState->Regs.idxOp + bOpCode;
            uint64_t const cAddressDelta = uTmp / pLnState->Hdr.cMaxOpsPerInstr * pLnState->Hdr.cbMinInstr;
            uint64_t const cOpIndexDelta = uTmp % pLnState->Hdr.cMaxOpsPerInstr;

            pLnState->Regs.uLine    += cLineDelta;
            pLnState->Regs.uAddress += cAddressDelta;
            pLnState->Regs.idxOp    += cOpIndexDelta;
            Log2(("%08x: DW Special Opcode %#04x: uLine + %d => %u; uAddress + %#llx => %#llx; idxOp + %#llx => %#llx\n",
                  offOpCode, bLogOpCode, cLineDelta, pLnState->Regs.uLine, cAddressDelta, pLnState->Regs.uAddress,
                  cOpIndexDelta, pLnState->Regs.idxOp));

            rc = rtDwarfLine_AddLine(pLnState, offOpCode);
        }
        else
        {
            switch (bOpCode)
            {
                /*
                 * Standard opcode.
                 */
                case DW_LNS_copy:
                    Log2(("%08x: DW_LNS_copy\n", offOpCode));
                    rc = rtDwarfLine_AddLine(pLnState, offOpCode);
                    break;

                case DW_LNS_advance_pc:
                {
                    uint64_t u64Adv = rtDwarfCursor_GetULeb128(pCursor, 0);
                    pLnState->Regs.uAddress += (pLnState->Regs.idxOp + u64Adv) / pLnState->Hdr.cMaxOpsPerInstr
                                             * pLnState->Hdr.cbMinInstr;
                    pLnState->Regs.idxOp    += (pLnState->Regs.idxOp + u64Adv) % pLnState->Hdr.cMaxOpsPerInstr;
                    Log2(("%08x: DW_LNS_advance_pc: u64Adv=%#llx (%lld) )\n", offOpCode, u64Adv, u64Adv));
                    break;
                }

                case DW_LNS_advance_line:
                {
                    int32_t cLineDelta = rtDwarfCursor_GetSLeb128AsS32(pCursor, 0);
                    pLnState->Regs.uLine += cLineDelta;
                    Log2(("%08x: DW_LNS_advance_line: uLine + %d => %u\n", offOpCode, cLineDelta, pLnState->Regs.uLine));
                    break;
                }

                case DW_LNS_set_file:
                    pLnState->Regs.iFile = rtDwarfCursor_GetULeb128AsU32(pCursor, 0);
                    Log2(("%08x: DW_LNS_set_file: iFile=%u\n", offOpCode, pLnState->Regs.iFile));
                    break;

                case DW_LNS_set_column:
                    pLnState->Regs.uColumn = rtDwarfCursor_GetULeb128AsU32(pCursor, 0);
                    Log2(("%08x: DW_LNS_set_column\n", offOpCode));
                    break;

                case DW_LNS_negate_stmt:
                    pLnState->Regs.fIsStatement = !pLnState->Regs.fIsStatement;
                    Log2(("%08x: DW_LNS_negate_stmt\n", offOpCode));
                    break;

                case DW_LNS_set_basic_block:
                    pLnState->Regs.fBasicBlock = true;
                    Log2(("%08x: DW_LNS_set_basic_block\n", offOpCode));
                    break;

                case DW_LNS_const_add_pc:
                    pLnState->Regs.uAddress += (pLnState->Regs.idxOp + 255) / pLnState->Hdr.cMaxOpsPerInstr
                                             * pLnState->Hdr.cbMinInstr;
                    pLnState->Regs.idxOp    += (pLnState->Regs.idxOp + 255) % pLnState->Hdr.cMaxOpsPerInstr;
                    Log2(("%08x: DW_LNS_const_add_pc\n", offOpCode));
                    break;

                case DW_LNS_fixed_advance_pc:
                    pLnState->Regs.uAddress += rtDwarfCursor_GetUHalf(pCursor, 0);
                    pLnState->Regs.idxOp     = 0;
                    Log2(("%08x: DW_LNS_fixed_advance_pc\n", offOpCode));
                    break;

                case DW_LNS_set_prologue_end:
                    pLnState->Regs.fPrologueEnd = true;
                    Log2(("%08x: DW_LNS_set_prologue_end\n", offOpCode));
                    break;

                case DW_LNS_set_epilogue_begin:
                    pLnState->Regs.fEpilogueBegin = true;
                    Log2(("%08x: DW_LNS_set_epilogue_begin\n", offOpCode));
                    break;

                case DW_LNS_set_isa:
                    pLnState->Regs.uIsa = rtDwarfCursor_GetULeb128AsU32(pCursor, 0);
                    Log2(("%08x: DW_LNS_set_isa %#x\n", offOpCode, pLnState->Regs.uIsa));
                    break;

                default:
                {
                    unsigned cOpsToSkip = pLnState->Hdr.pacStdOperands[bOpCode - 1];
                    Log(("rtDwarfLine_RunProgram: Unknown standard opcode %#x, %#x operands, at %08x.\n", bOpCode, cOpsToSkip, offOpCode));
                    while (cOpsToSkip-- > 0)
                        rc = rtDwarfCursor_SkipLeb128(pCursor);
                    break;
                }

                /*
                 * Extended opcode.
                 */
                case DW_LNS_extended:
                {
                    /* The instruction has a length prefix. */
                    uint64_t cbInstr = rtDwarfCursor_GetULeb128(pCursor, UINT64_MAX);
                    if (RT_FAILURE(pCursor->rc))
                        return pCursor->rc;
                    if (cbInstr > pCursor->cbUnitLeft)
                        return VERR_DWARF_BAD_LNE;
                    uint8_t const * const pbEndOfInstr = rtDwarfCursor_CalcPos(pCursor, cbInstr);

                    /* Get the opcode and deal with it if we know it. */
                    bOpCode = rtDwarfCursor_GetUByte(pCursor, 0);
                    switch (bOpCode)
                    {
                        case DW_LNE_end_sequence:
#if 0 /* No need for this, I think. */
                            pLnState->Regs.fEndSequence = true;
                            rc = rtDwarfLine_AddLine(pLnState, offOpCode);
#endif
                            rtDwarfLine_ResetState(pLnState);
                            Log2(("%08x: DW_LNE_end_sequence\n", offOpCode));
                            break;

                        case DW_LNE_set_address:
                            pLnState->Regs.uAddress = rtDwarfCursor_GetVarSizedU(pCursor, cbInstr - 1, UINT64_MAX);
                            pLnState->Regs.idxOp    = 0;
                            Log2(("%08x: DW_LNE_set_address: %#llx\n", offOpCode, pLnState->Regs.uAddress));
                            break;

                        case DW_LNE_define_file:
                        {
                            const char *pszFilename = rtDwarfCursor_GetSZ(pCursor, NULL);
                            uint32_t    idxInc      = rtDwarfCursor_GetULeb128AsU32(pCursor, UINT32_MAX);
                            rtDwarfCursor_SkipLeb128(pCursor); /* st_mtime */
                            rtDwarfCursor_SkipLeb128(pCursor); /* st_size */
                            Log2(("%08x: DW_LNE_define_file: {%d}/%s\n", offOpCode, idxInc, pszFilename));

                            rc = rtDwarfCursor_AdvanceToPos(pCursor, pbEndOfInstr);
                            if (RT_SUCCESS(rc))
                                rc = rtDwarfLine_DefineFileName(pLnState, pszFilename, idxInc);
                        }

                        /*
                         * Note! Was defined in DWARF 4.  But... Watcom used it
                         *       for setting the segment in DWARF 2, creating
                         *       an incompatibility with the newer standard.
                         */
                        case DW_LNE_set_descriminator:
                            if (pLnState->Hdr.uVer != 2)
                            {
                                Assert(pLnState->Hdr.uVer >= 4);
                                pLnState->Regs.uDiscriminator = rtDwarfCursor_GetULeb128AsU32(pCursor, UINT32_MAX);
                                Log2(("%08x: DW_LNE_set_descriminator: %u\n", offOpCode, pLnState->Regs.uDiscriminator));
                            }
                            else
                            {
                                uint64_t uSeg = rtDwarfCursor_GetVarSizedU(pCursor, cbInstr - 1, UINT64_MAX);
                                Log2(("%08x: DW_LNE_set_segment: %ll#x - Watcom Extension\n", offOpCode, uSeg));
                                /** @todo make use of this? */
                            }
                            break;

                        default:
                            Log(("rtDwarfLine_RunProgram: Unknown extended opcode %#x, length %#x at %08x\n", bOpCode, cbInstr, offOpCode));
                            break;
                    }

                    /* Advance the cursor to the end of the instruction . */
                    rtDwarfCursor_AdvanceToPos(pCursor, pbEndOfInstr);
                    break;
                }
            }
        }

        /*
         * Check the status before looping.
         */
        if (RT_FAILURE(rc))
            return rc;
        if (RT_FAILURE(pCursor->rc))
            return pCursor->rc;
    }
    return rc;
}


/**
 * Reads the include directories for a line number unit.
 *
 * @returns IPRT status code
 * @param   pLnState            The line number program state.
 * @param   pCursor             The cursor.
 */
static int rtDwarfLine_ReadFileNames(PRTDWARFLINESTATE pLnState, PRTDWARFCURSOR pCursor)
{
    int rc = rtDwarfLine_DefineFileName(pLnState, "/<bad-zero-file-name-entry>", 0);
    if (RT_FAILURE(rc))
        return rc;

    for (;;)
    {
        const char *psz = rtDwarfCursor_GetSZ(pCursor, NULL);
        if (!*psz)
            break;

        uint64_t idxInc = rtDwarfCursor_GetULeb128(pCursor, UINT64_MAX);
        rtDwarfCursor_SkipLeb128(pCursor); /* st_mtime */
        rtDwarfCursor_SkipLeb128(pCursor); /* st_size */

        rc = rtDwarfLine_DefineFileName(pLnState, psz, idxInc);
        if (RT_FAILURE(rc))
            return rc;
    }
    return pCursor->rc;
}


/**
 * Reads the include directories for a line number unit.
 *
 * @returns IPRT status code
 * @param   pLnState            The line number program state.
 * @param   pCursor             The cursor.
 */
static int rtDwarfLine_ReadIncludePaths(PRTDWARFLINESTATE pLnState, PRTDWARFCURSOR pCursor)
{
    const char *psz = "";   /* The zeroth is the unit dir. */
    for (;;)
    {
        if ((pLnState->cIncPaths % 2) == 0)
        {
            void *pv = RTMemRealloc(pLnState->papszIncPaths, sizeof(pLnState->papszIncPaths[0]) * (pLnState->cIncPaths + 2));
            if (!pv)
                return VERR_NO_MEMORY;
            pLnState->papszIncPaths = (const char **)pv;
        }
        Log(("  Path #%02u = '%s'\n", pLnState->cIncPaths, psz));
        pLnState->papszIncPaths[pLnState->cIncPaths] = psz;
        pLnState->cIncPaths++;

        psz = rtDwarfCursor_GetSZ(pCursor, NULL);
        if (!*psz)
            break;
    }

    return pCursor->rc;
}


/**
 * Explodes the line number table for a compilation unit.
 *
 * @returns IPRT status code
 * @param   pThis               The DWARF instance.
 * @param   pCursor             The cursor to read the line number information
 *                              via.
 */
static int rtDwarfLine_ExplodeUnit(PRTDBGMODDWARF pThis, PRTDWARFCURSOR pCursor)
{
    RTDWARFLINESTATE LnState;
    RT_ZERO(LnState);
    LnState.pDwarfMod = pThis;

    /*
     * Parse the header.
     */
    rtDwarfCursor_GetInitalLength(pCursor);
    LnState.Hdr.uVer           = rtDwarfCursor_GetUHalf(pCursor, 0);
    if (   LnState.Hdr.uVer < 2
        || LnState.Hdr.uVer > 4)
        return rtDwarfCursor_SkipUnit(pCursor);

    LnState.Hdr.offFirstOpcode = rtDwarfCursor_GetUOff(pCursor, 0);
    uint8_t const * const pbFirstOpcode = rtDwarfCursor_CalcPos(pCursor, LnState.Hdr.offFirstOpcode);

    LnState.Hdr.cbMinInstr     = rtDwarfCursor_GetUByte(pCursor, 0);
    if (LnState.Hdr.uVer >= 4)
        LnState.Hdr.cMaxOpsPerInstr = rtDwarfCursor_GetUByte(pCursor, 0);
    else
        LnState.Hdr.cMaxOpsPerInstr = 1;
    LnState.Hdr.u8DefIsStmt    = rtDwarfCursor_GetUByte(pCursor, 0);
    LnState.Hdr.s8LineBase     = rtDwarfCursor_GetSByte(pCursor, 0);
    LnState.Hdr.u8LineRange    = rtDwarfCursor_GetUByte(pCursor, 0);
    LnState.Hdr.u8OpcodeBase   = rtDwarfCursor_GetUByte(pCursor, 0);

    if (   !LnState.Hdr.u8OpcodeBase
        || !LnState.Hdr.cMaxOpsPerInstr
        || !LnState.Hdr.u8LineRange
        || LnState.Hdr.u8DefIsStmt > 1)
        return VERR_DWARF_BAD_LINE_NUMBER_HEADER;
    Log2(("DWARF Line number header:\n"
          "    uVer             %d\n"
          "    offFirstOpcode   %#llx\n"
          "    cbMinInstr       %u\n"
          "    cMaxOpsPerInstr  %u\n"
          "    u8DefIsStmt      %u\n"
          "    s8LineBase       %d\n"
          "    u8LineRange      %u\n"
          "    u8OpcodeBase     %u\n",
          LnState.Hdr.uVer,    LnState.Hdr.offFirstOpcode, LnState.Hdr.cbMinInstr,  LnState.Hdr.cMaxOpsPerInstr,
          LnState.Hdr.u8DefIsStmt, LnState.Hdr.s8LineBase, LnState.Hdr.u8LineRange, LnState.Hdr.u8OpcodeBase));

    LnState.Hdr.pacStdOperands = pCursor->pb;
    for (uint8_t iStdOpcode = 1; iStdOpcode < LnState.Hdr.u8OpcodeBase; iStdOpcode++)
        rtDwarfCursor_GetUByte(pCursor, 0);

    int rc = pCursor->rc;
    if (RT_SUCCESS(rc))
        rc = rtDwarfLine_ReadIncludePaths(&LnState, pCursor);
    if (RT_SUCCESS(rc))
        rc = rtDwarfLine_ReadFileNames(&LnState, pCursor);

    /*
     * Run the program....
     */
    if (RT_SUCCESS(rc))
        rc = rtDwarfCursor_AdvanceToPos(pCursor, pbFirstOpcode);
    if (RT_SUCCESS(rc))
        rc = rtDwarfLine_RunProgram(&LnState, pCursor);

    /*
     * Clean up.
     */
    size_t i = LnState.cFileNames;
    while (i-- > 0)
        RTStrFree(LnState.papszFileNames[i]);
    RTMemFree(LnState.papszFileNames);
    RTMemFree(LnState.papszIncPaths);

    Assert(rtDwarfCursor_IsAtEndOfUnit(pCursor) || RT_FAILURE(rc));
    return rc;
}


/**
 * Explodes the line number table.
 *
 * The line numbers are insered into the debug info container.
 *
 * @returns IPRT status code
 * @param   pThis               The DWARF instance.
 */
static int rtDwarfLine_ExplodeAll(PRTDBGMODDWARF pThis)
{
    if (!pThis->aSections[krtDbgModDwarfSect_line].fPresent)
        return VINF_SUCCESS;

    RTDWARFCURSOR Cursor;
    int rc = rtDwarfCursor_Init(&Cursor, pThis, krtDbgModDwarfSect_line);
    if (RT_FAILURE(rc))
        return rc;

    while (   !rtDwarfCursor_IsAtEnd(&Cursor)
           && RT_SUCCESS(rc))
        rc = rtDwarfLine_ExplodeUnit(pThis, &Cursor);

    return rtDwarfCursor_Delete(&Cursor, rc);
}


/*
 *
 * DWARF Abbreviations.
 * DWARF Abbreviations.
 * DWARF Abbreviations.
 *
 */

/**
 * Deals with a cache miss in rtDwarfAbbrev_Lookup.
 *
 * @returns Pointer to abbreviation cache entry (read only).  May be rendered
 *          invalid by subsequent calls to this function.
 * @param   pThis               The DWARF instance.
 * @param   uCode               The abbreviation code to lookup.
 */
static PCRTDWARFABBREV rtDwarfAbbrev_LookupMiss(PRTDBGMODDWARF pThis, uint32_t uCode)
{
    /*
     * There is no entry with code zero.
     */
    if (!uCode)
        return NULL;

    /*
     * Resize the cache array if the code is considered cachable.
     */
    bool fFillCache = true;
    if (pThis->cCachedAbbrevsAlloced < uCode)
    {
        if (uCode > _64K)
            fFillCache = false;
        else
        {
            uint32_t cNew = RT_ALIGN(uCode, 64);
            void *pv = RTMemRealloc(pThis->paCachedAbbrevs, sizeof(pThis->paCachedAbbrevs[0]) * cNew);
            if (!pv)
                fFillCache = false;
            else
            {
                pThis->cCachedAbbrevsAlloced = cNew;
                pThis->paCachedAbbrevs       = (PRTDWARFABBREV)pv;
            }
        }
    }

    /*
     * Walk the abbreviations till we find the desired code.
     */
    RTDWARFCURSOR Cursor;
    int rc = rtDwarfCursor_InitWithOffset(&Cursor, pThis, krtDbgModDwarfSect_abbrev, pThis->offCachedAbbrev);
    if (RT_FAILURE(rc))
        return NULL;

    PRTDWARFABBREV pRet = NULL;
    if (fFillCache)
    {
        /*
         * Search for the entry and fill the cache while doing so.
         */
        for (;;)
        {
            /* Read the 'header'. */
            uint32_t const uCurCode  = rtDwarfCursor_GetULeb128AsU32(&Cursor, 0);
            uint32_t const uCurTag   = rtDwarfCursor_GetULeb128AsU32(&Cursor, 0);
            uint8_t  const uChildren = rtDwarfCursor_GetU8(&Cursor, 0);
            if (RT_FAILURE(Cursor.rc))
                break;
            if (   uCurTag > 0xffff
                || uChildren > 1)
            {
                Cursor.rc = VERR_DWARF_BAD_ABBREV;
                break;
            }

            /* Cache it? */
            if (uCurCode <= pThis->cCachedAbbrevsAlloced)
            {
                PRTDWARFABBREV pEntry = &pThis->paCachedAbbrevs[uCurCode - 1];
                while (pThis->cCachedAbbrevs < uCurCode)
                {
                    pThis->paCachedAbbrevs[pThis->cCachedAbbrevs].fFilled = false;
                    pThis->cCachedAbbrevs++;
                }

                pEntry->fFilled   = true;
                pEntry->fChildren = RT_BOOL(uChildren);
                pEntry->uTag      = uCurTag;
                pEntry->offSpec   = rtDwarfCursor_CalcSectOffsetU32(&Cursor);

                if (uCurCode == uCode)
                {
                    pRet = pEntry;
                    if (uCurCode == pThis->cCachedAbbrevsAlloced)
                        break;
                }
            }

            /* Skip the specification. */
            uint32_t uAttr, uForm;
            do
            {
                uAttr = rtDwarfCursor_GetULeb128AsU32(&Cursor, 0);
                uForm = rtDwarfCursor_GetULeb128AsU32(&Cursor, 0);
            } while (uAttr != 0);
            if (RT_FAILURE(Cursor.rc))
                break;

            /* Done? (Maximize cache filling.) */
            if (   pRet != NULL
                && uCurCode >= pThis->cCachedAbbrevsAlloced)
                break;
        }
    }
    else
    {
        /*
         * Search for the entry with the desired code, no cache filling.
         */
        for (;;)
        {
            /* Read the 'header'. */
            uint32_t const uCurCode  = rtDwarfCursor_GetULeb128AsU32(&Cursor, 0);
            uint32_t const uCurTag   = rtDwarfCursor_GetULeb128AsU32(&Cursor, 0);
            uint8_t  const uChildren = rtDwarfCursor_GetU8(&Cursor, 0);
            if (RT_FAILURE(Cursor.rc))
                break;
            if (   uCurTag > 0xffff
                || uChildren > 1)
            {
                Cursor.rc = VERR_DWARF_BAD_ABBREV;
                break;
            }

            /* Do we have a match? */
            if (uCurCode == uCode)
            {
                pRet = &pThis->LookupAbbrev;
                pRet->fFilled   = true;
                pRet->fChildren = RT_BOOL(uChildren);
                pRet->uTag      = uCurTag;
                pRet->offSpec   = rtDwarfCursor_CalcSectOffsetU32(&Cursor);
                break;
            }

            /* Skip the specification. */
            uint32_t uAttr, uForm;
            do
            {
                uAttr = rtDwarfCursor_GetULeb128AsU32(&Cursor, 0);
                uForm = rtDwarfCursor_GetULeb128AsU32(&Cursor, 0);
            } while (uAttr != 0);
            if (RT_FAILURE(Cursor.rc))
                break;
        }
    }

    rtDwarfCursor_Delete(&Cursor, VINF_SUCCESS);
    return pRet;
}


/**
 * Looks up an abbreviation.
 *
 * @returns Pointer to abbreviation cache entry (read only).  May be rendered
 *          invalid by subsequent calls to this function.
 * @param   pThis               The DWARF instance.
 * @param   uCode               The abbreviation code to lookup.
 */
static PCRTDWARFABBREV rtDwarfAbbrev_Lookup(PRTDBGMODDWARF pThis, uint32_t uCode)
{
    if (   uCode - 1 >= pThis->cCachedAbbrevs
        || !pThis->paCachedAbbrevs[uCode - 1].fFilled)
        return rtDwarfAbbrev_LookupMiss(pThis, uCode);
    return &pThis->paCachedAbbrevs[uCode - 1];
}


/**
 * Sets the abbreviation offset of the current unit.
 *
 * This will flush the cached abbreviation entries if the offset differs from
 * the previous unit.
 *
 * @param   pThis               The DWARF instance.
 * @param   offAbbrev           The offset into the abbreviation section.
 */
static void rtDwarfAbbrev_SetUnitOffset(PRTDBGMODDWARF pThis, uint32_t offAbbrev)
{
    if (pThis->offCachedAbbrev != offAbbrev)
    {
        pThis->offCachedAbbrev = offAbbrev;
        pThis->cCachedAbbrevs  = 0;
    }
}


/*
 *
 * DWARF DIE stack (used by the debug_info parser).
 * DWARF DIE stack (used by the debug_info parser).
 * DWARF DIE stack (used by the debug_info parser).
 *
 */

/**
 * DWARF DIE stack entry.
 */
typedef struct RTDWARFDIESTACKENTRY
{
    /** The abbreviation code. */
    uint32_t        uAbbrCode;
    /** The tag. */
    uint16_t        uTag;
    /** The offset into the debug_info section of the data. */
    uint32_t        offData;
    /** The associated name. (Points into some dwarf section.) */
    const char     *pszName;
} RTDWARFDIESTACKENTRY;
/** Pointer to a DIE stack entry. */
typedef RTDWARFDIESTACKENTRY *PRTDWARFDIESTACKENTRY;

/**
 * DWARF DIE stack.
 */
typedef struct RTDWARFDIESTACK
{
    /** The number of items on the stack. */
    uint32_t                cOnStack;
    /** The number of stack entries we've allocated. */
    uint32_t                cAlloced;
    /** Pointer to the stack entries. */
    PRTDWARFDIESTACKENTRY   paEntries;
} RTDWARFDIESTACK;
/** Pointer to a DIE stack. */
typedef RTDWARFDIESTACK *PRTDWARFDIESTACK;
/** Pointer to a const DIE stack. */
typedef RTDWARFDIESTACK const *PCRTDWARFDIESTACK;


static int rtDwarfDieStack_SetName(PRTDWARFDIESTACK pStack, const char *pszName)
{
    uint32_t i = pStack->cOnStack;
    if (i == 0)
        return VERR_DWARF_BAD_INFO;
    i--;
    pStack->paEntries[i].pszName = pszName;
    return VINF_SUCCESS;
}


static int rtDwarfDieStack_Push(PRTDWARFDIESTACK pStack, uint32_t uAbbrCode, uint16_t uTag, uint32_t offData)
{
    uint32_t i = pStack->cOnStack;
    if (i == pStack->cAlloced)
    {
        uint32_t cNewSize = pStack->cAlloced ? pStack->cAlloced * 2 : 1 /*16*/;
        void *pv = RTMemRealloc(pStack->paEntries, sizeof(pStack->paEntries[0]) * cNewSize);
        if (!pv)
            return VERR_NO_MEMORY;
        pStack->paEntries = (PRTDWARFDIESTACKENTRY)pv;
        pStack->cAlloced  = cNewSize;
    }
    Assert(i < pStack->cAlloced);

    pStack->paEntries[i].uAbbrCode = uAbbrCode;
    pStack->paEntries[i].uTag      = uTag;
    pStack->paEntries[i].offData   = offData;
    pStack->paEntries[i].pszName   = NULL;
    pStack->cOnStack = i + 1;

    return VINF_SUCCESS;
}


static int rtDwarfDieStack_Pop(PRTDWARFDIESTACK pStack)
{
    if (pStack->cOnStack == 0)
        return VERR_DWARF_BAD_INFO;
    pStack->cOnStack--;
    return VINF_SUCCESS;
}


static void rtDwarfDieStack_Init(PRTDWARFDIESTACK pStack)
{
    pStack->cOnStack  = 0;
    pStack->cAlloced  = 0;
    pStack->paEntries = NULL;
}


static void rtDwarfDieStack_Delete(PRTDWARFDIESTACK pStack)
{
    RTMemFree(pStack->paEntries);
    pStack->paEntries = NULL;
}



/*
 *
 * DWARF debug_info parser
 * DWARF debug_info parser
 * DWARF debug_info parser
 *
 */


/**
 * Resolves a string section (debug_str) reference.
 *
 * @returns Pointer to the string (inside the string section).
 * @param   pThis               The DWARF instance.
 * @param   pCursor             The cursor.
 * @param   pszErrValue         What to return on failure (@a
 *                              pCursor->rc is set).
 */
static const char *rtDwarfForm_GetStrp(PRTDBGMODDWARF pThis, PRTDWARFCURSOR pCursor, const char *pszErrValue)
{
    uint64_t offDebugStr = rtDwarfCursor_GetUOff(pCursor, UINT64_MAX);
    if (RT_FAILURE(pCursor->rc))
        return pszErrValue;

    if (offDebugStr >= pThis->aSections[krtDbgModDwarfSect_str].cb)
    {
        /* Ugly: Exploit the cursor status field for reporting errors. */
        pCursor->rc = VERR_DWARF_BAD_INFO;
        return pszErrValue;
    }

    if (!pThis->aSections[krtDbgModDwarfSect_str].pv)
    {
        int rc = rtDbgModDwarfLoadSection(pThis, krtDbgModDwarfSect_str);
        if (RT_FAILURE(rc))
        {
            /* Ugly: Exploit the cursor status field for reporting errors. */
            pCursor->rc = rc;
            return pszErrValue;
        }
    }

    return (const char *)pThis->aSections[krtDbgModDwarfSect_str].pv + (size_t)offDebugStr;
}

static const char *rtDwarfForm_GetString(PRTDBGMODDWARF pThis, PRTDWARFCURSOR pCursor, uint16_t uForm, const char *pszErrValue)
{
    switch (uForm)
    {
        case DW_FORM_string:
            return rtDwarfCursor_GetSZ(pCursor, pszErrValue);

        case DW_FORM_strp:
            return rtDwarfForm_GetStrp(pThis, pCursor, pszErrValue);

        default:
            pCursor->rc = VERR_DWARF_UNEXPECTED_FORM;
            return pszErrValue;
    }
}


/**
 * Skips a form.
 * @returns IPRT status code
 * @param   pCursor             The cursor.
 * @param   uForm               The form to skip.
 */
static int rtDwarfInfo_SkipForm(PRTDWARFCURSOR pCursor, uint32_t uForm)
{
    switch (uForm)
    {
        case DW_FORM_addr:
            return rtDwarfCursor_SkipBytes(pCursor, pCursor->cbNativeAddr);

        case DW_FORM_block:
        case DW_FORM_exprloc:
            return rtDwarfCursor_SkipBytes(pCursor, rtDwarfCursor_GetULeb128(pCursor, 0));

        case DW_FORM_block1:
            return rtDwarfCursor_SkipBytes(pCursor, rtDwarfCursor_GetU8(pCursor, 0));

        case DW_FORM_block2:
            return rtDwarfCursor_SkipBytes(pCursor, rtDwarfCursor_GetU16(pCursor, 0));

        case DW_FORM_block4:
            return rtDwarfCursor_SkipBytes(pCursor, rtDwarfCursor_GetU32(pCursor, 0));

        case DW_FORM_data1:
        case DW_FORM_ref1:
        case DW_FORM_flag:
            return rtDwarfCursor_SkipBytes(pCursor, 1);

        case DW_FORM_data2:
        case DW_FORM_ref2:
            return rtDwarfCursor_SkipBytes(pCursor, 2);

        case DW_FORM_data4:
        case DW_FORM_ref4:
            return rtDwarfCursor_SkipBytes(pCursor, 4);

        case DW_FORM_data8:
        case DW_FORM_ref8:
        case DW_FORM_ref_sig8:
            return rtDwarfCursor_SkipBytes(pCursor, 8);

        case DW_FORM_udata:
        case DW_FORM_sdata:
        case DW_FORM_ref_udata:
            return rtDwarfCursor_SkipLeb128(pCursor);

        case DW_FORM_string:
            rtDwarfCursor_GetSZ(pCursor, NULL);
            return pCursor->rc;

        case DW_FORM_indirect:
            return rtDwarfInfo_SkipForm(pCursor, rtDwarfCursor_GetULeb128AsU32(pCursor, UINT32_MAX));

        case DW_FORM_strp:
        case DW_FORM_ref_addr:
        case DW_FORM_sec_offset:
            return rtDwarfCursor_SkipBytes(pCursor, pCursor->f64bitDwarf ? 8 : 4);

        case DW_FORM_flag_present:
            return pCursor->rc; /* no data */

        default:
            return VERR_DWARF_UNKNOWN_FORM;
    }
}



/**
 * Skips a DIE.
 *
 * @returns IPRT status code.
 * @param   pCursor         The cursor.
 * @param   pAbbrevCursor   The abbreviation cursor.
 */
static int rtDwarfInfo_SkipDie(PRTDWARFCURSOR pCursor, PRTDWARFCURSOR pAbbrevCursor)
{
    for (;;)
    {
        uint32_t uAttr = rtDwarfCursor_GetULeb128AsU32(pAbbrevCursor, 0);
        uint32_t uForm = rtDwarfCursor_GetULeb128AsU32(pAbbrevCursor, 0);
        if (uAttr == 0 && uForm == 0)
            break;

        int rc = rtDwarfInfo_SkipForm(pCursor, uForm);
        if (RT_FAILURE(rc))
            return rc;
    }
    return RT_FAILURE(pCursor->rc) ? pCursor->rc : pAbbrevCursor->rc;
}


/**
 * Parse a DIE.
 *
 * @returns IPRT status code.
 * @param   pThis               The DWARF instance.
 * @param   pCursor             The debug_info cursor.
 * @param   pAbbrev             The abbreviation entry.
 * @param   pAbbrevCursor       The abbreviation cursor.
 * @param   pStack              The DIE stack.
 */
static int rtDwarfInfo_ParseDie(PRTDBGMODDWARF pThis, PRTDWARFCURSOR pCursor,
                                PCRTDWARFABBREV pAbbrev, PRTDWARFCURSOR pAbbrevCursor,
                                PRTDWARFDIESTACK pStack)
{
    /*
     * Pick out DIEs which tag we find interesting.
     */
    switch (pAbbrev->uTag)
    {
        case DW_TAG_compile_unit:
        case DW_TAG_partial_unit:
            Log((" - DW_TAG_%s_unit:\n", pAbbrev->uTag == DW_TAG_compile_unit ? "compile" : "partial"));
            for (;;)
            {
                uint32_t uAttr = rtDwarfCursor_GetULeb128AsU32(pAbbrevCursor, 0);
                uint32_t uForm = rtDwarfCursor_GetULeb128AsU32(pAbbrevCursor, 0);
                switch (uAttr)
                {
                    case 0:
                        return VINF_SUCCESS;

                    case DW_AT_name:
                    {
                        const char *pszName = rtDwarfForm_GetString(pThis, pCursor, uForm, "<error>");
                        rtDwarfDieStack_SetName(pStack, pszName);
                        Log4(("%*s   - name=%s\n", pStack->cOnStack*2, "", pszName));
                        break;
                    }

                    case DW_AT_stmt_list:
                        /** @todo  */
                    default:
                        Log4(("%*s   - attr=%#x form=%#x\n", pStack->cOnStack*2, "", uAttr, uForm));
                        rtDwarfInfo_SkipForm(pCursor, uForm);
                }
                if (RT_FAILURE(pCursor->rc))
                    return pCursor->rc;
            }
            break; /* not reached. */

        case DW_TAG_subprogram:
            Log((" - DW_TAG_subprogram:\n"));
            for (;;)
            {
                uint32_t uAttr = rtDwarfCursor_GetULeb128AsU32(pAbbrevCursor, 0);
                uint32_t uForm = rtDwarfCursor_GetULeb128AsU32(pAbbrevCursor, 0);
                switch (uAttr)
                {
                    case 0:
                        return VINF_SUCCESS;

                    case DW_AT_name:
                    {
                        const char *pszName = rtDwarfForm_GetString(pThis, pCursor, uForm, "<error>");
                        rtDwarfDieStack_SetName(pStack, pszName);
                        Log4(("%*s   - name=%s\n", pStack->cOnStack*2, "", pszName));
                        break;
                    }

                    default:
                        Log4(("%*s   - attr=%#x form=%#x\n", pStack->cOnStack*2, "", uAttr, uForm));
                        rtDwarfInfo_SkipForm(pCursor, uForm);
                }
                if (RT_FAILURE(pCursor->rc))
                    return pCursor->rc;
            }
            break; /* not reached. */

    }

    return rtDwarfInfo_SkipDie(pCursor, pAbbrevCursor);
}



static int rtDwarfInfo_LoadUnit(PRTDBGMODDWARF pThis, PRTDWARFCURSOR pCursor)
{
    Log(("rtDwarfInfo_LoadUnit: %#x\n", rtDwarfCursor_CalcSectOffsetU32(pCursor)));

    /*
     * Read the compilation unit header.
     */
    rtDwarfCursor_GetInitalLength(pCursor);
    uint16_t const uVer = rtDwarfCursor_GetUHalf(pCursor, 0);
    if (   uVer < 2
        || uVer > 4)
        return rtDwarfCursor_SkipUnit(pCursor);
    uint64_t const offAbbrev    = rtDwarfCursor_GetUOff(pCursor, UINT64_MAX);
    uint8_t  const cbNativeAddr = rtDwarfCursor_GetU8(pCursor, UINT8_MAX);
    if (RT_FAILURE(pCursor->rc))
        return pCursor->rc;
    Log(("   uVer=%d  offAbbrev=%#llx cbNativeAddr=%d\n", uVer, offAbbrev, cbNativeAddr));

    /*
     * Set up the abbreviation cache and store the native address size in the cursor.
     */
    if (offAbbrev > UINT32_MAX)
        return VERR_DWARF_BAD_INFO;
    rtDwarfAbbrev_SetUnitOffset(pThis, offAbbrev);
    pCursor->cbNativeAddr = cbNativeAddr;

    /*
     * We want a DIE stack as well.
     */
    RTDWARFDIESTACK Stack;
    rtDwarfDieStack_Init(&Stack);

    /*
     * Parse DIEs.
     */
    int rc = VINF_SUCCESS;
    while (!rtDwarfCursor_IsAtEndOfUnit(pCursor))
    {
        uint32_t uAbbrCode = rtDwarfCursor_GetULeb128AsU32(pCursor, UINT32_MAX);
        if (!uAbbrCode)
        {
            /* End of siblings, pop the stack. */
            rc = rtDwarfDieStack_Pop(&Stack);
            if (RT_FAILURE(rc))
                break;
        }
        else
        {
            /*
             * Look up the abbreviation.
             */
            PCRTDWARFABBREV pAbbrev = rtDwarfAbbrev_Lookup(pThis, uAbbrCode);
            if (!pAbbrev)
            {
                rc = VERR_DWARF_ABBREV_NOT_FOUND;
                break;
            }
            Log4((" %*stag=%#x%s\n", Stack.cOnStack * 2, "", pAbbrev->uTag, pAbbrev->fChildren ? " has children" : ""));

            /*
             * If this DIE has children, push it onto the stack.
             */
            if (pAbbrev->fChildren)
            {
                rc = rtDwarfDieStack_Push(&Stack, uAbbrCode, pAbbrev->uTag, rtDwarfCursor_CalcSectOffsetU32(pCursor));
                if (RT_FAILURE(rc))
                    break;
            }

            /*
             * Parse it.
             */
            RTDWARFCURSOR AbbrevCursor;
            rc = rtDwarfCursor_InitWithOffset(&AbbrevCursor, pThis, krtDbgModDwarfSect_abbrev, pAbbrev->offSpec);
            if (RT_SUCCESS(rc))
            {
                rc = rtDwarfInfo_ParseDie(pThis, pCursor, pAbbrev, &AbbrevCursor, &Stack);
                rc = rtDwarfCursor_Delete(&AbbrevCursor, rc);
            }
            if (RT_FAILURE(rc))
                break;
        }
    }

    rtDwarfDieStack_Delete(&Stack);
    return RT_SUCCESS(rc) ? pCursor->rc : rc;
}


/**
 * Extracts the symbols.
 *
 * The symbols are insered into the debug info container.
 *
 * @returns IPRT status code
 * @param   pThis               The DWARF instance.
 */
static int rtDwarfInfo_LoadAll(PRTDBGMODDWARF pThis)
{
    RTDWARFCURSOR Cursor;
    int rc = rtDwarfCursor_Init(&Cursor, pThis, krtDbgModDwarfSect_info);
    if (RT_FAILURE(rc))
        return rc;

    while (   !rtDwarfCursor_IsAtEnd(&Cursor)
           && RT_SUCCESS(rc))
        rc = rtDwarfInfo_LoadUnit(pThis, &Cursor);

    return rtDwarfCursor_Delete(&Cursor, rc);
}




/*
 *
 * DWARF Debug module implementation.
 * DWARF Debug module implementation.
 * DWARF Debug module implementation.
 *
 */


/** @interface_method_impl{RTDBGMODVTDBG,pfnLineByAddr} */
static DECLCALLBACK(int) rtDbgModDwarf_LineByAddr(PRTDBGMODINT pMod, RTDBGSEGIDX iSeg, RTUINTPTR off,
                                                  PRTINTPTR poffDisp, PRTDBGLINE pLineInfo)
{
    PRTDBGMODDWARF pThis = (PRTDBGMODDWARF)pMod->pvDbgPriv;
    return RTDbgModLineByAddr(pThis->hCnt, iSeg, off, poffDisp, pLineInfo);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnLineByOrdinal} */
static DECLCALLBACK(int) rtDbgModDwarf_LineByOrdinal(PRTDBGMODINT pMod, uint32_t iOrdinal, PRTDBGLINE pLineInfo)
{
    PRTDBGMODDWARF pThis = (PRTDBGMODDWARF)pMod->pvDbgPriv;
    return RTDbgModLineByOrdinal(pThis->hCnt, iOrdinal, pLineInfo);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnLineCount} */
static DECLCALLBACK(uint32_t) rtDbgModDwarf_LineCount(PRTDBGMODINT pMod)
{
    PRTDBGMODDWARF pThis = (PRTDBGMODDWARF)pMod->pvDbgPriv;
    return RTDbgModLineCount(pThis->hCnt);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnLineAdd} */
static DECLCALLBACK(int) rtDbgModDwarf_LineAdd(PRTDBGMODINT pMod, const char *pszFile, size_t cchFile, uint32_t uLineNo,
                                               uint32_t iSeg, RTUINTPTR off, uint32_t *piOrdinal)
{
    PRTDBGMODDWARF pThis = (PRTDBGMODDWARF)pMod->pvDbgPriv;
    return RTDbgModLineAdd(pThis->hCnt, pszFile, uLineNo, iSeg, off, piOrdinal);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnSymbolByAddr} */
static DECLCALLBACK(int) rtDbgModDwarf_SymbolByAddr(PRTDBGMODINT pMod, RTDBGSEGIDX iSeg, RTUINTPTR off,
                                                    PRTINTPTR poffDisp, PRTDBGSYMBOL pSymInfo)
{
    PRTDBGMODDWARF pThis = (PRTDBGMODDWARF)pMod->pvDbgPriv;
    return RTDbgModSymbolByAddr(pThis->hCnt, iSeg, off, poffDisp, pSymInfo);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnSymbolByName} */
static DECLCALLBACK(int) rtDbgModDwarf_SymbolByName(PRTDBGMODINT pMod, const char *pszSymbol, size_t cchSymbol,
                                                    PRTDBGSYMBOL pSymInfo)
{
    PRTDBGMODDWARF pThis = (PRTDBGMODDWARF)pMod->pvDbgPriv;
    Assert(!pszSymbol[cchSymbol]);
    return RTDbgModSymbolByName(pThis->hCnt, pszSymbol/*, cchSymbol*/, pSymInfo);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnSymbolByOrdinal} */
static DECLCALLBACK(int) rtDbgModDwarf_SymbolByOrdinal(PRTDBGMODINT pMod, uint32_t iOrdinal, PRTDBGSYMBOL pSymInfo)
{
    PRTDBGMODDWARF pThis = (PRTDBGMODDWARF)pMod->pvDbgPriv;
    return RTDbgModSymbolByOrdinal(pThis->hCnt, iOrdinal, pSymInfo);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnSymbolCount} */
static DECLCALLBACK(uint32_t) rtDbgModDwarf_SymbolCount(PRTDBGMODINT pMod)
{
    PRTDBGMODDWARF pThis = (PRTDBGMODDWARF)pMod->pvDbgPriv;
    return RTDbgModSymbolCount(pThis->hCnt);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnSymbolAdd} */
static DECLCALLBACK(int) rtDbgModDwarf_SymbolAdd(PRTDBGMODINT pMod, const char *pszSymbol, size_t cchSymbol,
                                                 RTDBGSEGIDX iSeg, RTUINTPTR off, RTUINTPTR cb, uint32_t fFlags,
                                                 uint32_t *piOrdinal)
{
    PRTDBGMODDWARF pThis = (PRTDBGMODDWARF)pMod->pvDbgPriv;
    return RTDbgModSymbolAdd(pThis->hCnt, pszSymbol, iSeg, off, cb, fFlags, piOrdinal);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnSegmentByIndex} */
static DECLCALLBACK(int) rtDbgModDwarf_SegmentByIndex(PRTDBGMODINT pMod, RTDBGSEGIDX iSeg, PRTDBGSEGMENT pSegInfo)
{
    PRTDBGMODDWARF pThis = (PRTDBGMODDWARF)pMod->pvDbgPriv;
    return RTDbgModSegmentByIndex(pThis->hCnt, iSeg, pSegInfo);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnSegmentCount} */
static DECLCALLBACK(RTDBGSEGIDX) rtDbgModDwarf_SegmentCount(PRTDBGMODINT pMod)
{
    PRTDBGMODDWARF pThis = (PRTDBGMODDWARF)pMod->pvDbgPriv;
    return RTDbgModSegmentCount(pThis->hCnt);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnSegmentAdd} */
static DECLCALLBACK(int) rtDbgModDwarf_SegmentAdd(PRTDBGMODINT pMod, RTUINTPTR uRva, RTUINTPTR cb, const char *pszName, size_t cchName,
                                                  uint32_t fFlags, PRTDBGSEGIDX piSeg)
{
    PRTDBGMODDWARF pThis = (PRTDBGMODDWARF)pMod->pvDbgPriv;
    return RTDbgModSegmentAdd(pThis->hCnt, uRva, cb, pszName, fFlags, piSeg);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnImageSize} */
static DECLCALLBACK(RTUINTPTR) rtDbgModDwarf_ImageSize(PRTDBGMODINT pMod)
{
    PRTDBGMODDWARF pThis = (PRTDBGMODDWARF)pMod->pvDbgPriv;
    RTUINTPTR cb1 = RTDbgModImageSize(pThis->hCnt);
    RTUINTPTR cb2 = pMod->pImgVt->pfnImageSize(pMod);
    return RT_MAX(cb1, cb2);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnRvaToSegOff} */
static DECLCALLBACK(RTDBGSEGIDX) rtDbgModDwarf_RvaToSegOff(PRTDBGMODINT pMod, RTUINTPTR uRva, PRTUINTPTR poffSeg)
{
    PRTDBGMODDWARF pThis = (PRTDBGMODDWARF)pMod->pvDbgPriv;
    return RTDbgModRvaToSegOff(pThis->hCnt, uRva, poffSeg);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnClose} */
static DECLCALLBACK(int) rtDbgModDwarf_Close(PRTDBGMODINT pMod)
{
    PRTDBGMODDWARF pThis = (PRTDBGMODDWARF)pMod->pvDbgPriv;

    for (unsigned iSect = 0; iSect < RT_ELEMENTS(pThis->aSections); iSect++)
        if (pThis->aSections[iSect].pv)
            pThis->pMod->pImgVt->pfnUnmapPart(pThis->pMod, pThis->aSections[iSect].cb, &pThis->aSections[iSect].pv);

    RTDbgModRelease(pThis->hCnt);
    RTMemFree(pThis->paCachedAbbrevs);
    RTMemFree(pThis);

    return VINF_SUCCESS;
}


/** @callback_method_impl{FNRTLDRENUMDBG} */
static DECLCALLBACK(int) rtDbgModDwarfEnumCallback(RTLDRMOD hLdrMod, uint32_t iDbgInfo, RTLDRDBGINFOTYPE enmType,
                                                   uint16_t iMajorVer, uint16_t iMinorVer, const char *pszPartNm,
                                                   RTFOFF offFile, RTLDRADDR LinkAddress, RTLDRADDR cb,
                                                   const char *pszExtFile, void *pvUser)
{
    /*
     * Skip stuff we can't handle.
     */
    if (   enmType != RTLDRDBGINFOTYPE_DWARF
        || !pszPartNm
        || pszExtFile)
        return VINF_SUCCESS;

    /*
     * Must have a part name starting with debug_ and possibly prefixed by dots
     * or underscores.
     */
    if (!strncmp(pszPartNm, ".debug_", sizeof(".debug_") - 1))        /* ELF */
        pszPartNm += sizeof(".debug_") - 1;
    else if (!strncmp(pszPartNm, "__debug_", sizeof("__debug_") - 1)) /* Mach-O */
        pszPartNm += sizeof("__debug_") - 1;
    else
        AssertMsgFailedReturn(("%s\n", pszPartNm), VINF_SUCCESS /*ignore*/);

    /*
     * Figure out which part we're talking about.
     */
    krtDbgModDwarfSect enmSect;
    if (0) { /* dummy */ }
#define ELSE_IF_STRCMP_SET(a_Name) else if (!strcmp(pszPartNm, #a_Name))  enmSect = krtDbgModDwarfSect_ ## a_Name
    ELSE_IF_STRCMP_SET(abbrev);
    ELSE_IF_STRCMP_SET(aranges);
    ELSE_IF_STRCMP_SET(frame);
    ELSE_IF_STRCMP_SET(info);
    ELSE_IF_STRCMP_SET(inlined);
    ELSE_IF_STRCMP_SET(line);
    ELSE_IF_STRCMP_SET(loc);
    ELSE_IF_STRCMP_SET(macinfo);
    ELSE_IF_STRCMP_SET(pubnames);
    ELSE_IF_STRCMP_SET(pubtypes);
    ELSE_IF_STRCMP_SET(ranges);
    ELSE_IF_STRCMP_SET(str);
    ELSE_IF_STRCMP_SET(types);
#undef ELSE_IF_STRCMP_SET
    else
    {
        AssertMsgFailed(("%s\n", pszPartNm));
        return VINF_SUCCESS;
    }

    /*
     * Record the section.
     */
    PRTDBGMODDWARF pThis = (PRTDBGMODDWARF)pvUser;
    AssertMsgReturn(!pThis->aSections[enmSect].fPresent, ("duplicate %s\n", pszPartNm), VINF_SUCCESS /*ignore*/);

    pThis->aSections[enmSect].fPresent  = true;
    pThis->aSections[enmSect].offFile   = offFile;
    pThis->aSections[enmSect].pv        = NULL;
    pThis->aSections[enmSect].cb        = (size_t)cb;
    if (pThis->aSections[enmSect].cb != cb)
        pThis->aSections[enmSect].cb    = ~(size_t)0;

    return VINF_SUCCESS;
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnTryOpen} */
static DECLCALLBACK(int) rtDbgModDwarf_TryOpen(PRTDBGMODINT pMod)
{
    /*
     * DWARF is only supported when part of an image.
     */
    if (!pMod->pImgVt)
        return VERR_DBG_NO_MATCHING_INTERPRETER;

    /*
     * Enumerate the debug info in the module, looking for DWARF bits.
     */
    PRTDBGMODDWARF pThis = (PRTDBGMODDWARF)RTMemAllocZ(sizeof(*pThis));
    if (!pThis)
        return VERR_NO_MEMORY;
    pThis->pMod = pMod;

    int rc = pMod->pImgVt->pfnEnumDbgInfo(pMod, rtDbgModDwarfEnumCallback, pThis);
    if (RT_SUCCESS(rc))
    {
        if (pThis->aSections[krtDbgModDwarfSect_info].fPresent)
        {
            /*
             * Extract / explode the data we want (symbols and line numbers)
             * storing them in a container module.
             */
            rc = RTDbgModCreate(&pThis->hCnt, pMod->pszName, 0 /*cbSeg*/, 0 /*fFlags*/);
            if (RT_SUCCESS(rc))
            {
                pMod->pvDbgPriv = pThis;

                rc = rtDbgModHlpAddSegmentsFromImage(pMod);
                if (RT_SUCCESS(rc))
                    rc = rtDwarfInfo_LoadAll(pThis);
                if (RT_SUCCESS(rc))
                    rc = rtDwarfLine_ExplodeAll(pThis);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Free the cached abbreviations and unload all sections.
                     */
                    pThis->cCachedAbbrevs = pThis->cCachedAbbrevsAlloced = 0;
                    RTMemFree(pThis->paCachedAbbrevs);

                    for (unsigned iSect = 0; iSect < RT_ELEMENTS(pThis->aSections); iSect++)
                        if (pThis->aSections[iSect].pv)
                            pThis->pMod->pImgVt->pfnUnmapPart(pThis->pMod, pThis->aSections[iSect].cb,
                                                              &pThis->aSections[iSect].pv);


                    return VINF_SUCCESS;
                }

                /* bail out. */
                RTDbgModRelease(pThis->hCnt);
                pMod->pvDbgPriv = NULL;
            }
        }
        else
            rc = VERR_DBG_NO_MATCHING_INTERPRETER;
    }
    RTMemFree(pThis->paCachedAbbrevs);
    RTMemFree(pThis);

    return rc;
}



/** Virtual function table for the DWARF debug info reader. */
DECL_HIDDEN_CONST(RTDBGMODVTDBG) const g_rtDbgModVtDbgDwarf =
{
    /*.u32Magic = */            RTDBGMODVTDBG_MAGIC,
    /*.fSupports = */           RT_DBGTYPE_DWARF,
    /*.pszName = */             "dwarf",
    /*.pfnTryOpen = */          rtDbgModDwarf_TryOpen,
    /*.pfnClose = */            rtDbgModDwarf_Close,

    /*.pfnRvaToSegOff = */      rtDbgModDwarf_RvaToSegOff,
    /*.pfnImageSize = */        rtDbgModDwarf_ImageSize,

    /*.pfnSegmentAdd = */       rtDbgModDwarf_SegmentAdd,
    /*.pfnSegmentCount = */     rtDbgModDwarf_SegmentCount,
    /*.pfnSegmentByIndex = */   rtDbgModDwarf_SegmentByIndex,

    /*.pfnSymbolAdd = */        rtDbgModDwarf_SymbolAdd,
    /*.pfnSymbolCount = */      rtDbgModDwarf_SymbolCount,
    /*.pfnSymbolByOrdinal = */  rtDbgModDwarf_SymbolByOrdinal,
    /*.pfnSymbolByName = */     rtDbgModDwarf_SymbolByName,
    /*.pfnSymbolByAddr = */     rtDbgModDwarf_SymbolByAddr,

    /*.pfnLineAdd = */          rtDbgModDwarf_LineAdd,
    /*.pfnLineCount = */        rtDbgModDwarf_LineCount,
    /*.pfnLineByOrdinal = */    rtDbgModDwarf_LineByOrdinal,
    /*.pfnLineByAddr = */       rtDbgModDwarf_LineByAddr,

    /*.u32EndMagic = */         RTDBGMODVTDBG_MAGIC
};

