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
#include <iprt/dbg.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/err.h>
#include <iprt/ctype.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include "internal/dbgmod.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @name Standard DWARF Line Number Opcodes
 * @{ */
#define DW_LNS_extended             UINT8_C(0)
#define DW_LNS_copy                 UINT8_C(1)
#define DW_LNS_advance_pc           UINT8_C(2)
#define DW_LNS_advance_line         UINT8_C(3)
#define DW_LNS_set_file             UINT8_C(4)
#define DW_LNS_set_column           UINT8_C(5)
#define DW_LNS_negate_stmt          UINT8_C(6)
#define DW_LNS_set_basic_block      UINT8_C(7)
#define DW_LNS_const_add_pc         UINT8_C(8)
#define DW_LNS_fixed_advance_pc     UINT8_C(9)
#define DW_LNS_set_prologue_end     UINT8_C(10)
#define DW_LNS_set_epilogue_begin   UINT8_C(11)
#define DW_LNS_set_isa              UINT8_C(12)
/** @} */


/** @name Extended DWARF Line Number Opcodes
 * @{ */
#define DW_LNE_end_sequence         UINT8_C(1)
#define DW_LNE_set_address          UINT8_C(2)
#define DW_LNE_define_file          UINT8_C(3)
#define DW_LNE_set_descriminator    UINT8_C(4)
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
 * The instance data of the DWARF reader.
 */
typedef struct RTDBGMODDWARF
{
    /** The debug container containing doing the real work. */
    RTDBGMOD            hCnt;
    /** Pointer to back to the debug info module (no reference ofc). */
    PRTDBGMODINT        pMod;

    /** DWARF debug info sections. */
    struct
    {
        /** The file offset of the part. */
        RTFOFF          offFile;
        /** The size of the part. */
        size_t          cb;
        /** The memory mapping of the part. */
        void const     *pv;
        /** Set if present. */
        bool            fPresent;
    } aSections[krtDbgModDwarfSect_End];
} RTDBGMODDWARF;
/** Pointer to instance data of the DWARF reader. */
typedef RTDBGMODDWARF *PRTDBGMODDWARF;

/**
 * Section reader instance.
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
} RTDWARFCURSOR;
/** Pointer to a DWARF section reader. */
typedef RTDWARFCURSOR *PRTDWARFCURSOR;


#define VERR_DWARF_BAD_LINE_NUMBER_HEADER   -55555
#define VERR_DWARF_UNEXPECTED_END           -55554
#define VERR_DWARF_LEB_OVERFLOW             -55553
#define VERR_DWARF_BAD_LNE                  -55552


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
                                               PRTDBGSEGIDX piSeg, PRTUINTPTR poffSeg)
{
    /** @todo The image should be doing this conversion, not we. */
    *piSeg   = RTDBGSEGIDX_RVA;
    *poffSeg = LinkAddress;
    return VINF_SUCCESS;
}


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
 * Gets a unsigned LEB128 encoded number.
 *
 * @returns unsigned number.
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
 * Gets a signed LEB128 encoded number.
 *
 * @returns signed number.
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
        u64Ret |= ~RT_BIT_64(cBits - 1);

    return (int64_t)u64Ret;
}


/**
 * Gets a unsigned LEB128 encoded number, max 32-bit width.
 *
 * @returns unsigned number.
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
 * Gets a signed LEB128 encoded number, max 32-bit width.
 *
 * @returns unsigned number.
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


static int rtDwarfCursor_SkipLeb128(PRTDWARFCURSOR pCursor)
{
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
            return pszErrValue;
        pCursor->cbUnitLeft--;
        pCursor->cbLeft--;
        if (!*pCursor->pb++)
            break;
    }
    return pszRet;
}



static uint16_t rtDwarfCursor_GetUHalf(PRTDWARFCURSOR pCursor, uint16_t uErrValue)
{
    return rtDwarfCursor_GetU16(pCursor, uErrValue);
}


static uint8_t rtDwarfCursor_GetUByte(PRTDWARFCURSOR pCursor, uint8_t uErrValue)
{
    return rtDwarfCursor_GetU8(pCursor, uErrValue);
}


static int8_t rtDwarfCursor_GetSByte(PRTDWARFCURSOR pCursor, int8_t iErrValue)
{
    return (int8_t)rtDwarfCursor_GetU8(pCursor, (uint8_t)iErrValue);
}


static uint64_t rtDwarfCursor_GetUOff(PRTDWARFCURSOR pCursor, uint64_t uErrValue)
{
    if (pCursor->f64bitDwarf)
        return rtDwarfCursor_GetU64(pCursor, uErrValue);
    return rtDwarfCursor_GetU32(pCursor, (uint32_t)uErrValue);
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


static bool rtDwarfCursor_IsAtEndOfUnit(PRTDWARFCURSOR pCursor)
{
    return pCursor->cbUnitLeft > 0;
}


static int rtDwarfCursor_SkipUnit(PRTDWARFCURSOR pCursor)
{
    pCursor->pb        += pCursor->cbUnitLeft;
    pCursor->cbLeft    -= pCursor->cbUnitLeft;
    pCursor->cbUnitLeft = 0;
    return VINF_SUCCESS;
}


static bool rtDwarfCursor_IsAtEnd(PRTDWARFCURSOR pCursor)
{
    return pCursor->cbLeft > 0;
}


/**
 * Initialize a section reader.
 *
 * @returns IPRT status code.
 * @param   pCursor            The section reader.
 * @param   pThis               The dwarf module.
 * @param   enmSect             .
 */
static int rtDwarfCursor_Init(PRTDWARFCURSOR pCursor, PRTDBGMODDWARF pThis, krtDbgModDwarfSect enmSect)
{
    int rc = rtDbgModDwarfLoadSection(pThis, enmSect);
    if (RT_FAILURE(rc))
        return rc;

    pCursor->pb           = (uint8_t const *)pThis->aSections[enmSect].pv;
    pCursor->cbLeft       = pThis->aSections[enmSect].cb;
    pCursor->cbUnitLeft   = pCursor->cbLeft;
    pCursor->pDwarfMod    = pThis;
    pCursor->fNativEndian = true; /** @todo endian */
    pCursor->f64bitDwarf  = false;
    pCursor->cbNativeAddr = 4;
    pCursor->rc           = VINF_SUCCESS;

    /** @todo ask the image about the endian used as well as the address
     *        width. */

    return VINF_SUCCESS;
}


/**
 * Deletes a section reader initialized by rtDwarfCursor_Init.
 *
 * @param   pCursor            The section reader.
 */
static void rtDwarfCursor_Delete(PRTDWARFCURSOR pCursor)
{
    /* ... and a drop of poison. */
    pCursor->pb         = NULL;
    pCursor->cbLeft     = ~(size_t)0;
    pCursor->cbUnitLeft = ~(size_t)0;
    pCursor->pDwarfMod  = NULL;
    pCursor->rc         = VERR_INTERNAL_ERROR_4;
}


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
    RTMemFree(pThis);

    return VINF_SUCCESS;
}


/**
 * DWARF line number program state.
 */
typedef struct RTDWARFLINESTATE
{
    /** Registers */
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

    /** @name Header
     * @{ */
    uint32_t        uVer;
    uint64_t        cbSkipAfterHdr;
    uint8_t         cbMinInstr;
    uint8_t         cMaxOpsPerInstr;
    uint8_t         u8DefIsStmt;
    int8_t          s8LineBase;
    uint8_t         u8LineRange;
    uint8_t         u8OpcodeBase;
    uint8_t const  *pacStdOperands;
    /** @} */

    /** @name Include Path Table (0-based)
     * @{ */
    const char    **papszIncPaths;
    uint32_t        cIncPaths;
    /** @} */

    /** @name File Name Table (1-based)
     * @{ */
    char          **papszFileNames;
    uint32_t        cFileNames;
    /** @} */

    /** The DWARF debug info reader instance. */
    PRTDBGMODDWARF  pDwarfMod;
} RTDWARFLINESTATE;
/** Pointer to a DWARF line number program state. */
typedef RTDWARFLINESTATE *PRTDWARFLINESTATE;


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
    return rtDbgModDwarfStringToUtf8(pLnState->pDwarfMod, &pLnState->papszFileNames[iFileName]);
}


static int rtDwarfLine_StdOp_Copy(PRTDWARFLINESTATE pLnState)
{
    const char *pszFile = pLnState->Regs.iFile < pLnState->cFileNames
                        ? pLnState->papszFileNames[pLnState->Regs.iFile]
                        : "<bad file name index>";
    RTDBGSEGIDX iSeg;
    RTUINTPTR   offSeg;
    int rc = rtDbgModDwarfLinkAddressToSegOffset(pLnState->pDwarfMod, pLnState->Regs.uAddress, &iSeg, &offSeg);
    if (RT_SUCCESS(rc))
        rc = RTDbgModLineAdd(pLnState->pDwarfMod->hCnt, pszFile, pLnState->Regs.uLine, iSeg, offSeg, NULL);

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
    pLnState->Regs.fIsStatement     = RT_BOOL(pLnState->u8DefIsStmt);
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
    int rc = VINF_SUCCESS;
    rtDwarfLine_ResetState(pLnState);
    do
    {
        uint8_t bOpCode = rtDwarfCursor_GetUByte(pCursor, 0);
        if (bOpCode > pLnState->u8OpcodeBase)
        {
            /*
             * Special opcode.
             */
            bOpCode -= pLnState->u8OpcodeBase;
            pLnState->Regs.uLine    += bOpCode % pLnState->u8LineRange + (int32_t)pLnState->s8LineBase;
            bOpCode /= pLnState->u8LineRange;
            pLnState->Regs.uAddress += (pLnState->Regs.idxOp + bOpCode) / pLnState->cMaxOpsPerInstr
                                     * pLnState->cbMinInstr;
            pLnState->Regs.idxOp    += (pLnState->Regs.idxOp + bOpCode) % pLnState->cMaxOpsPerInstr;

            rc = rtDwarfLine_StdOp_Copy(pLnState);
        }
        else
        {
            switch (bOpCode)
            {
                /*
                 * Standard opcode.
                 */
                case DW_LNS_copy:
                    rtDwarfLine_StdOp_Copy(pLnState);
                    break;

                case DW_LNS_advance_pc:
                {
                    uint64_t u64Adv = rtDwarfCursor_GetULeb128(pCursor, 0);
                    pLnState->Regs.uAddress += (pLnState->Regs.idxOp + u64Adv) / pLnState->cMaxOpsPerInstr
                                             * pLnState->cbMinInstr;
                    pLnState->Regs.idxOp    += (pLnState->Regs.idxOp + u64Adv) % pLnState->cMaxOpsPerInstr;
                    break;
                }

                case DW_LNS_advance_line:
                    pLnState->Regs.uLine += rtDwarfCursor_GetSLeb128AsS32(pCursor, 0);
                    break;

                case DW_LNS_set_file:
                    pLnState->Regs.iFile = rtDwarfCursor_GetULeb128AsU32(pCursor, 0);
                    break;

                case DW_LNS_set_column:
                    pLnState->Regs.uColumn = rtDwarfCursor_GetULeb128AsU32(pCursor, 0);
                    break;

                case DW_LNS_negate_stmt:
                    pLnState->Regs.fIsStatement = !pLnState->Regs.fIsStatement;
                    break;

                case DW_LNS_set_basic_block:
                    pLnState->Regs.fBasicBlock = true;
                    break;

                case DW_LNS_const_add_pc:
                    pLnState->Regs.uAddress += (pLnState->Regs.idxOp + 255) / pLnState->cMaxOpsPerInstr
                                             * pLnState->cbMinInstr;
                    pLnState->Regs.idxOp    += (pLnState->Regs.idxOp + 255) % pLnState->cMaxOpsPerInstr;
                    break;

                case DW_LNS_fixed_advance_pc:
                    pLnState->Regs.uAddress += rtDwarfCursor_GetUHalf(pCursor, 0);
                    pLnState->Regs.idxOp     = 0;
                    break;

                case DW_LNS_set_prologue_end:
                    pLnState->Regs.fPrologueEnd = true;
                    break;

                case DW_LNS_set_epilogue_begin:
                    pLnState->Regs.fEpilogueBegin = true;
                    break;

                case DW_LNS_set_isa:
                    pLnState->Regs.uIsa = rtDwarfCursor_GetULeb128AsU32(pCursor, 0);
                    break;

                default:
                {
                    unsigned cOpsToSkip = pLnState->pacStdOperands[bOpCode - 1];
                    while (cOpsToSkip-- > 0)
                        rc = rtDwarfCursor_SkipLeb128(pCursor);
                    break;
                }

                /*
                 * Extended opcode.
                 */
                case DW_LNS_extended:
                {
                    uint64_t cbInstr = rtDwarfCursor_GetULeb128(pCursor, UINT64_MAX);
                    if (cbInstr > pCursor->cbUnitLeft)
                        return VERR_DWARF_BAD_LNE;
                    /** @todo continue here. */
                    bOpCode = rtDwarfCursor_GetUByte(pCursor, 0);
                    switch (bOpCode)
                    {
                        case DW_LNE_end_sequence:
                        case DW_LNE_set_address:
                        case DW_LNE_define_file:
                        case DW_LNE_set_descriminator:
                            break;
                    }
                    break;
                }
            }
        }
    } while (RT_SUCCESS(rc));
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
    for (;;)
    {
        const char *psz = rtDwarfCursor_GetSZ(pCursor, NULL);
        if (!*psz)
            break;

        uint64_t idxInc = rtDwarfCursor_GetULeb128(pCursor, UINT64_MAX);
        rtDwarfCursor_SkipLeb128(pCursor); /* st_mtime */
        rtDwarfCursor_SkipLeb128(pCursor); /* st_size */

        int rc = rtDwarfLine_DefineFileName(pLnState, psz, idxInc);
        if (RT_FAILURE(rc))
            return rc;
    }
    return VINF_SUCCESS;
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
        pLnState->papszIncPaths[pLnState->cIncPaths] = psz;
        pLnState->cIncPaths++;

        psz = rtDwarfCursor_GetSZ(pCursor, NULL);
        if (!*psz)
            break;
    }

    return VINF_SUCCESS;
}


static int rtDbgModDwarfExplodeLineNumbersForUnit(PRTDBGMODDWARF pThis, PRTDWARFCURSOR pCursor)
{
    RTDWARFLINESTATE LnState;
    RT_ZERO(LnState);
    LnState.pDwarfMod = pThis;

    /*
     * Parse the header.
     */
    LnState.uVer           = rtDwarfCursor_GetUHalf(pCursor, 0);
    if (LnState.uVer >= 2 && LnState.uVer <= 4)
        return rtDwarfCursor_SkipUnit(pCursor);
    LnState.cbSkipAfterHdr = rtDwarfCursor_GetUOff(pCursor, 0);
    LnState.cbMinInstr     = rtDwarfCursor_GetUByte(pCursor, 0);
    LnState.cMaxOpsPerInstr= rtDwarfCursor_GetUByte(pCursor, 0);
    LnState.u8DefIsStmt    = rtDwarfCursor_GetUByte(pCursor, 0);
    LnState.s8LineBase     = rtDwarfCursor_GetSByte(pCursor, 0);
    LnState.u8LineRange    = rtDwarfCursor_GetUByte(pCursor, 0);
    LnState.u8OpcodeBase   = rtDwarfCursor_GetUByte(pCursor, 0);
    if (!LnState.u8OpcodeBase)
        return VERR_DWARF_BAD_LINE_NUMBER_HEADER;

    LnState.pacStdOperands = pCursor->pb;
    for (uint8_t iStdOpcode = 1; iStdOpcode < LnState.u8OpcodeBase; iStdOpcode++)
        rtDwarfCursor_GetUByte(pCursor, 0);

    int rc = rtDwarfLine_ReadIncludePaths(&LnState, pCursor);
    if (RT_SUCCESS(rc))
        rc = rtDwarfLine_ReadFileNames(&LnState, pCursor);

    /*
     * Run the program....
     */
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
static int rtDbgModDwarfExplodeLineNumbers(PRTDBGMODDWARF pThis)
{
    if (!pThis->aSections[krtDbgModDwarfSect_line].fPresent)
        return VINF_SUCCESS;

    RTDWARFCURSOR Cursor;
    int rc = rtDwarfCursor_Init(&Cursor, pThis, krtDbgModDwarfSect_line);
    if (RT_FAILURE(rc))
        return rc;

    while (   rtDwarfCursor_IsAtEnd(&Cursor)
           && RT_SUCCESS(rc))
        rc = rtDbgModDwarfExplodeLineNumbersForUnit(pThis, &Cursor);


    rtDwarfCursor_Delete(&Cursor);
    return rc;
}


/**
 * Extracts the symbols.
 *
 * The symbols are insered into the debug info container.
 *
 * @returns IPRT status code
 * @param   pThis               The DWARF instance.
 */
static int rtDbgModDwarfExtractSymbols(PRTDBGMODDWARF pThis)
{
    int rc = rtDbgModDwarfLoadSection(pThis, krtDbgModDwarfSect_info);
    if (RT_FAILURE(rc))
        return rc;

    /** @todo  */

    rtDbgModDwarfUnloadSection(pThis, krtDbgModDwarfSect_info);
    return rc;
}


/**
 * Loads the abbreviations used to parse the info section.
 *
 * @returns IPRT status code
 * @param   pThis               The DWARF instance.
 */
static int rtDbgModDwarfLoadAbbreviations(PRTDBGMODDWARF pThis)
{
    int rc = rtDbgModDwarfLoadSection(pThis, krtDbgModDwarfSect_abbrev);
    if (RT_FAILURE(rc))
        return rc;

#if 0 /** @todo  */
    size_t          cbLeft = pThis->aSections[krtDbgModDwarfSect_abbrev].cb;
    uint8_t const  *pb     = (uint8_t const *)pThis->aSections[krtDbgModDwarfSect_abbrev].pv;
    while (cbLeft > 0)
    {

    }
#endif

    rtDbgModDwarfUnloadSection(pThis, krtDbgModDwarfSect_abbrev);
    return rc;
}


/** @callback_method_impl{FNRTLDRENUMSEGS} */
static DECLCALLBACK(int) rtDbgModHlpAddSegmentCallback(RTLDRMOD hLdrMod, PCRTLDRSEG pSeg, void *pvUser)
{
    PRTDBGMODINT pMod = (PRTDBGMODINT)pvUser;
    return pMod->pDbgVt->pfnSegmentAdd(pMod, pSeg->RVA, pSeg->cb, pSeg->pchName, pSeg->cchName, 0 /*fFlags*/, NULL);
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
                    rc = rtDbgModDwarfLoadAbbreviations(pThis);
                if (RT_SUCCESS(rc))
                    rc = rtDbgModDwarfExtractSymbols(pThis);
                if (RT_SUCCESS(rc))
                    rc = rtDbgModDwarfExplodeLineNumbers(pThis);
                if (RT_SUCCESS(rc))
                {
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

