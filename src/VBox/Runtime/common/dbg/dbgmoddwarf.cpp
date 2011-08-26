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
#include <iprt/stream.h>
#include <iprt/string.h>
#include "internal/dbgmod.h"


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
    /** The DWARF debug info reader instance. */
    PRTDBGMODDWARF          pDwarfMod;
    /** The section we're reading.  */
    krtDbgModDwarfSect      enmSect;
    /** The current position. */
    uint8_t const          *pb;
    /** The number of bytes left to read. */
    size_t                  cbLeft;
    /** Set if this is 64-bit DWARF, clear if 32-bit. */
    bool                    f64bit;
    /** Set if the format endian is native, clear if endian needs to be
     * inverted. */
    bool                    fNativEndian;
} RTDWARFSECTRDR;
/** Pointer to a DWARF section reader. */
typedef RTDWARFSECTRDR *PRTDWARFSECTRDR;



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
 * Corrects the endianness of a 32-bit unsigned value.
 *
 * @returns The corrected value.
 *
 * @param   pThis       The DWARF instance.
 * @param   pu32        The variable to correct.
 */
DECLINLINE(uint32_t) rtDbgModDwarfEndianU32(PRTDBGMODDWARF pThis, uint32_t *pu32)
{
    return *pu32;
}


/**
 * Corrects the endianness of a 64-bit unsigned value.
 *
 * @returns The corrected value.
 *
 * @param   pThis       The DWARF instance.
 * @param   pu64        The variable to correct.
 */
DECLINLINE(uint64_t) rtDbgModDwarfEndianU64(PRTDBGMODDWARF pThis, uint64_t *pu64)
{
    return *pu64;
}


static uint8_t rtDwarfSectRdrGetU8(PRTDWARFSECTRDR pSectRdr, uint8_t uErrValue)
{
    if (pSectRdr->cbLeft < 1)
    {
        pSectRdr->pb    += pSectRdr->cbLeft;
        pSectRdr->cbLeft = 0;
        return uErrValue;
    }

    uint8_t u8 = pSectRdr->pb[0];
    pSectRdr->pb      += 1;
    pSectRdr->cbLeft  -= 1;
    return u8;
}


static uint16_t rtDwarfSectRdrGetU16(PRTDWARFSECTRDR pSectRdr, uint16_t uErrValue)
{
    if (pSectRdr->cbLeft < 2)
    {
        pSectRdr->pb    += pSectRdr->cbLeft;
        pSectRdr->cbLeft = 0;
        return uErrValue;
    }

    uint16_t u16 = RT_MAKE_U16(pSectRdr->pb[0], pSectRdr->pb[1]);
    pSectRdr->pb      += 2;
    pSectRdr->cbLeft  -= 2;
    if (!pSectRdr->fNativEndian)
        u16 = RT_BSWAP_U16(u16);
    return u16;
}


static uint32_t rtDwarfSectRdrGetU32(PRTDWARFSECTRDR pSectRdr, uint32_t uErrValue)
{
    if (pSectRdr->cbLeft < 4)
    {
        pSectRdr->pb    += pSectRdr->cbLeft;
        pSectRdr->cbLeft = 0;
        return uErrValue;
    }

    uint32_t u32 = RT_MAKE_U32_FROM_U8(pSectRdr->pb[0], pSectRdr->pb[1], pSectRdr->pb[2], pSectRdr->pb[3]);
    pSectRdr->pb      += 4;
    pSectRdr->cbLeft  -= 4;
    if (!pSectRdr->fNativEndian)
        u32 = RT_BSWAP_U32(u32);
    return u32;
}


static uint64_t rtDwarfSectRdrGetU64(PRTDWARFSECTRDR pSectRdr, uint64_t uErrValue)
{
    if (pSectRdr->cbLeft < 8)
    {
        pSectRdr->pb    += pSectRdr->cbLeft;
        pSectRdr->cbLeft = uErrValue;
        return 0;
    }

    uint64_t u64 = RT_MAKE_U64_FROM_U8(pSectRdr->pb[0], pSectRdr->pb[1], pSectRdr->pb[2], pSectRdr->pb[3],
                                       pSectRdr->pb[4], pSectRdr->pb[5], pSectRdr->pb[6], pSectRdr->pb[7]);
    pSectRdr->pb      += 8;
    pSectRdr->cbLeft  -= 8;
    if (!pSectRdr->fNativEndian)
        u64 = RT_BSWAP_U64(u64);
    return u64;
}


static uint16_t rtDwarfSectRdrGetUHalf(PRTDWARFSECTRDR pSectRdr, uint16_t uErrValue)
{
    return rtDwarfSectRdrGetU16(pSectRdr, uErrValue);
}


static uint8_t rtDwarfSectRdrGetUByte(PRTDWARFSECTRDR pSectRdr, uint8_t uErrValue)
{
    return rtDwarfSectRdrGetU8(pSectRdr, uErrValue);
}


static int8_t rtDwarfSectRdrGetSByte(PRTDWARFSECTRDR pSectRdr, int8_t iErrValue)
{
    return (int8_t)rtDwarfSectRdrGetU8(pSectRdr, (uint8_t)iErrValue);
}


static uint64_t rtDwarfSectRdrGetUOff(PRTDWARFSECTRDR pSectRdr, uint64_t uErrValue)
{
    if (pSectRdr->f64bit)
        return rtDwarfSectRdrGetU64(pSectRdr, uErrValue);
    return rtDwarfSectRdrGetU32(pSectRdr, (uint32_t)uErrValue);
}


/**
 * Initialize a section reader.
 *
 * @returns IPRT status code.
 * @param   pSectRdr            The section reader.
 * @param   pThis               The dwarf module.
 * @param   enmSect             .
 */
static int rtDwarfSectRdrInit(PRTDWARFSECTRDR pSectRdr, PRTDBGMODDWARF pThis, krtDbgModDwarfSect enmSect)
{
    int rc = rtDbgModDwarfLoadSection(pThis, enmSect);
    if (RT_FAILURE(rc))
        return rc;

    pSectRdr->pDwarfMod    = pThis;
    pSectRdr->enmSect      = enmSect;
    pSectRdr->pb           = (uint8_t const *)pThis->aSections[krtDbgModDwarfSect_line].pv;
    pSectRdr->cbLeft       = pThis->aSections[krtDbgModDwarfSect_line].cb;
    pSectRdr->fNativEndian = true; /** @todo endian */
    pSectRdr->f64bit       = false;

    /*
     * Read the initial length.
     */
    uint64_t cbUnit = rtDwarfSectRdrGetU32(pSectRdr, 0);
    if (cbUnit == UINT32_C(0xffffffff))
    {
        pSectRdr->f64bit = true;
        cbUnit = rtDwarfSectRdrGetU64(pSectRdr, 0);
    }
    if (cbUnit < pSectRdr->cbLeft)
        pSectRdr->cbLeft = (size_t)cbUnit;

    return VINF_SUCCESS;
}


/**
 * Deletes a section reader initialized by rtDwarfSectRdrInit.
 *
 * @param   pSectRdr            The section reader.
 */
static void rtDwarfSectRdrDelete(PRTDWARFSECTRDR pSectRdr)
{
    rtDbgModDwarfUnloadSection(pSectRdr->pDwarfMod, pSectRdr->enmSect);

    /* ... and a drop of poison. */
    pSectRdr->pb = NULL;
    pSectRdr->cbLeft = ~(size_t)0;
    pSectRdr->enmSect = krtDbgModDwarfSect_End;
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

    RTDWARFSECTRDR SectRdr;
    int rc = rtDwarfSectRdrInit(&SectRdr, pThis, krtDbgModDwarfSect_line);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Parse the header.
     */
    uint32_t const uVer           = rtDwarfSectRdrGetUHalf(&SectRdr, 0);
    uint64_t const cbSkipAfterHdr = rtDwarfSectRdrGetUOff(&SectRdr, 0);
    uint8_t  const cbMinInstr     = rtDwarfSectRdrGetUByte(&SectRdr, 0);
    uint8_t  const cbMaxInstr     = rtDwarfSectRdrGetUByte(&SectRdr, 0);
    uint8_t  const fDefIsStmt     = rtDwarfSectRdrGetUByte(&SectRdr, 0);
    int8_t   const i8LineBase     = rtDwarfSectRdrGetSByte(&SectRdr, 0);
    uint8_t  const u8LineRange    = rtDwarfSectRdrGetUByte(&SectRdr, 0);
    uint8_t  const u8OpcodeBase   = rtDwarfSectRdrGetUByte(&SectRdr, 0);
    /*...*/

    if (uVer >= 2 && uVer <= 4)
    {
        /*
         * Run the program....
         */

    }

    rtDwarfSectRdrDelete(&SectRdr);
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

