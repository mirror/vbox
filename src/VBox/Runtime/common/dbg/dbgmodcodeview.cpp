/* $Id$ */
/** @file
 * IPRT - Debug Module Reader For Microsoft CodeView and COFF.
 *
 * Based on the following documentation (plus guess work and googling):
 *
 *  - "Tools Interface Standard (TIS) Formats Specification for Windows",
 *    dated February 1993, version 1.0.
 *
 *  - "Visual C++ 5.0 Symbolic Debug Information Specification" chapter of
 *     SPECS.CHM from MSDN Library October 2001.
 *
 *  - "High Level Languages Debug Table Documentation", aka HLLDBG.HTML, aka
 *     IBMHLL.HTML, last changed 1996-07-08.
 *
 * Testcases using RTLdrFlt:
 *     - VBoxPcBios.sym at 0xf0000.
 *     - NT4 kernel PE image (coff syms).
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
#define LOG_GROUP RTLOGGROUP_DBG
#include <iprt/dbg.h>
#include "internal/iprt.h"

#include <iprt/alloca.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/strcache.h>
#include "internal/dbgmod.h"
#include "internal/ldrPE.h"
#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * CodeView Header.  There are two of this, base header at the start of the debug
 * information and a trailing header at the end.
 */
typedef struct RTCVHDR
{
    /** The magic ('NBxx'), see RTCVHDR_MAGIC_XXX. */
    uint32_t    u32Magic;
    /**
     * Base header: Subsection directory offset relative to this header (start).
     * Trailing header: Offset of the base header relative to the end of the file.
     *
     * Called lfoBase, lfaBase, lfoDirectory, lfoDir and probably other things in
     * the various specs/docs available. */
    uint32_t    off;
} RTCVHDR;
/** Pointer to a CodeView header. */
typedef RTCVHDR *PRTCVHDR;

/** @name CodeView magic values (RTCVHDR::u32Magic).
 * @{  */
/** CodeView from Visual C++ 5.0.  Specified in the 2001 MSDN specs.chm file. */
#define RTCVHDR_MAGIC_NB11  RT_MAKE_U32_FROM_U8('N', 'B', '1', '1')
/** External PDB reference (often referred to as PDB 2.0). */
#define RTCVHDR_MAGIC_NB10  RT_MAKE_U32_FROM_U8('N', 'B', '1', '0')
/** CodeView v4.10, packed. Specified in the TIS document. */
#define RTCVHDR_MAGIC_NB09  RT_MAKE_U32_FROM_U8('N', 'B', '0', '9')
/** CodeView v4.00 thru v4.05.  Specified in the TIS document?  */
#define RTCVHDR_MAGIC_NB08  RT_MAKE_U32_FROM_U8('N', 'B', '0', '8')
/** Quick C for Windows 1.0 debug info. */
#define RTCVHDR_MAGIC_NB07  RT_MAKE_U32_FROM_U8('N', 'B', '0', '7')
/** Emitted by ILINK indicating incremental link. Comparable to NB05?  */
#define RTCVHDR_MAGIC_NB06  RT_MAKE_U32_FROM_U8('N', 'B', '0', '6')
/** Emitted by LINK version 5.20 and later before packing. */
#define RTCVHDR_MAGIC_NB05  RT_MAKE_U32_FROM_U8('N', 'B', '0', '5')
/** Emitted by IBM ILINK for HLL (similar to NB02 in many ways). */
#define RTCVHDR_MAGIC_NB04  RT_MAKE_U32_FROM_U8('N', 'B', '0', '4')
/** Emitted by LINK version 5.10 (or similar OMF linkers), as shipped with
 * Microsoft C v6.0 for example.  More or less entirely 16-bit. */
#define RTCVHDR_MAGIC_NB02  RT_MAKE_U32_FROM_U8('N', 'B', '0', '2')
/* No idea what NB03 might have been. */
/** AIX debugger format according to "IBM OS/2 16/32-bit Object Module Format
 *  (OMF) and Linear eXecutable Module Format (LX)" revision 10 (LXOMF.PDF). */
#define RTCVHDR_MAGIC_NB01  RT_MAKE_U32_FROM_U8('N', 'B', '0', '1')
/** Ancient CodeView format according to LXOMF.PDF. */
#define RTCVHDR_MAGIC_NB00  RT_MAKE_U32_FROM_U8('N', 'B', '0', '0')
/** @} */


/** @name CV directory headers.
 * @{ */

/**
 * Really old CV directory header used with NB00 and NB02.
 *
 * Uses 16-bit directory entires (RTCVDIRENT16).
 */
typedef struct RTCVDIRHDR16
{
    /** The number of directory entries. */
    uint16_t        cEntries;
} RTCVDIRHDR16;
/** Pointer to a old CV directory header. */
typedef RTCVDIRHDR16 *PRTCVDIRHDR16;

/**
 * Simple 32-bit CV directory base header, used by NB04 (aka IBM HLL).
 */
typedef struct RTCVDIRHDR32
{
    /** The number of bytes of this header structure. */
    uint16_t        cbHdr;
    /** The number of bytes per entry. */
    uint16_t        cbEntry;
    /** The number of directory entries. */
    uint32_t        cEntries;
} RTCVDIRHDR32;
/** Pointer to a 32-bit CV directory header. */
typedef RTCVDIRHDR32 *PRTCVDIRHDR32;

/**
 * Extended 32-bit CV directory header as specified in the TIS doc.
 * The two extra fields seems to never have been assigned any official purpose.
 */
typedef struct RTCVDIRHDR32EX
{
    /** This starts the same way as the NB04 header. */
    RTCVDIRHDR32    Core;
    /** Tentatively decleared as the offset to the next directory generated by
     * the incremental linker.  Haven't seen this used yet. */
    uint32_t        offNextDir;
    /** Flags, non defined apparently, so MBZ. */
    uint32_t        fFlags;
} RTCVDIRHDR32EX;
/** Pointer to an extended 32-bit CV directory header. */
typedef RTCVDIRHDR32EX *PRTCVDIRHDR32EX;

/** @} */


/**
 * 16-bit CV directory entry used with NB00 and NB02.
 */
typedef struct RTCVDIRENT16
{
    /** Subsection type (RTCVSST). */
    uint16_t        uSubSectType;
    /** Which module (1-based, 0xffff is special). */
    uint16_t        iMod;
    /** The lowe offset of this subsection relative to the base CV header. */
    uint16_t        offLow;
    /** The high part of the subsection offset. */
    uint16_t        offHigh;
    /** The size of the subsection. */
    uint16_t        cb;
} RTCVDIRENT16;
AssertCompileSize(RTCVDIRENT16, 10);
/** Pointer to a 16-bit CV directory entry. */
typedef RTCVDIRENT16 *PRTCVDIRENT16;


/**
 * 32-bit CV directory entry used starting with NB04.
 */
typedef struct RTCVDIRENT32
{
    /** Subsection type (RTCVSST). */
    uint16_t        uSubSectType;
    /** Which module (1-based, 0xffff is special). */
    uint16_t        iMod;
    /** The offset of this subsection relative to the base CV header. */
    uint32_t        off;
    /** The size of the subsection. */
    uint32_t        cb;
} RTCVDIRENT32;
AssertCompileSize(RTCVDIRENT32, 12);
/** Pointer to a 32-bit CV directory entry. */
typedef RTCVDIRENT32 *PRTCVDIRENT32;
/** Pointer to a const 32-bit CV directory entry. */
typedef RTCVDIRENT32 const *PCRTCVDIRENT32;


/**
 * CodeView subsection types.
 */
typedef enum RTCVSST
{
    /** @name NB00, NB02 and NB04 subsection types.
     * The actual format of each subsection varies between NB04 and the others,
     * and it may further vary in NB04 depending on the module type.
     * @{ */
    kCvSst_OldModule    = 0x101,
    kCvSst_OldPublic,
    kCvSst_OldTypes,
    kCvSst_OldSymbols,
    kCvSst_OldSrcLines,
    kCvSst_OldLibraries,
    kCvSst_OldImports,
    kCvSst_OldCompacted,
    kCvSst_OldSrcLnSeg = 0x109,
    kCvSst_OldSrcLines3 = 0x10b,
    /** @} */

    /** @name NB09, NB11 (and possibly NB05, NB06, NB07, and NB08) subsection types.
     * @{ */
    kCvSst_Module    = 0x120,
    kCvSst_Types,
    kCvSst_Public,
    kCvSst_PublicSym,
    kCvSst_Symbols,
    kCvSst_AlignSym,
    kCvSst_SrcLnSeg,
    kCvSst_SrcModule,
    kCvSst_Libraries,
    kCvSst_GlobalSym,
    kCvSst_GlobalPub,
    kCvSst_GlobalTypes,
    kCvSst_MPC,
    kCvSst_SegMap,
    kCvSst_SegName,
    kCvSst_PreComp,
    kCvSst_PreCompMap,
    kCvSst_OffsetMap16,
    kCvSst_OffsetMap32,
    kCvSst_FileIndex = 0x133,
    kCvSst_StaticSym
    /** @} */
} RTCVSST;
/** Pointer to a CV subsection type value.  */
typedef RTCVSST *PRTCVSST;
/** Pointer to a const CV subsection type value.  */
typedef RTCVSST const *PCRTCVSST;


/**
 * CV4 module segment info.
 */
typedef struct RTCVMODSEGINFO32
{
    /** The segment number. */
    uint16_t        iSeg;
    /** Explicit padding. */
    uint16_t        u16Padding;
    /** Offset into the segment. */
    uint32_t        off;
    /** The size of the contribution. */
    uint32_t        cb;
} RTCVMODSEGINFO32;
typedef RTCVMODSEGINFO32 *PRTCVMODSEGINFO32;
typedef RTCVMODSEGINFO32 const *PCRTCVMODSEGINFO32;


/**
 * CV4 segment map header.
 */
typedef struct RTCVSEGMAPHDR
{
    /** Number of segments descriptors in the table. */
    uint16_t        cSegs;
    /** Number of logical segment descriptors. */
    uint16_t        cLogSegs;
} RTCVSEGMAPHDR;
/** Pointer to a CV4 segment map header. */
typedef RTCVSEGMAPHDR *PRTCVSEGMAPHDR;
/** Pointer to a const CV4 segment map header. */
typedef RTCVSEGMAPHDR const *PCRTCVSEGMAPHDR;

/**
 * CV4 Segment map descriptor entry.
 */
typedef struct RTCVSEGMAPDESC
{
    /** Segment flags. */
    uint16_t        fFlags;
    /** The overlay number. */
    uint16_t        iOverlay;
    /** Group index into this segment descriptor array. 0 if not relevant.
     * The group descriptors are found in the second half of the table.  */
    uint16_t        iGroup;
    /** Complicated. */
    uint16_t        iFrame;
    /** Offset (byte) into the kCvSst_SegName table of the segment name, or
     * 0xffff. */
    uint16_t        offSegName;
    /** Offset (byte) into the kCvSst_SegName table of the class name, or 0xffff. */
    uint16_t        offClassName;
    /** Offset into the physical segment. */
    uint32_t        off;
    /** Size of segment. */
    uint32_t        cb;
} RTCVSEGMAPDESC;
/** Pointer to a segment map descriptor entry. */
typedef RTCVSEGMAPDESC *PRTCVSEGMAPDESC;
/** Pointer to a const segment map descriptor entry. */
typedef RTCVSEGMAPDESC const *PCRTCVSEGMAPDESC;

/** @name RTCVSEGMAPDESC_F_XXX - RTCVSEGMAPDESC::fFlags values.
 * @{ */
#define RTCVSEGMAPDESC_F_READ       UINT16_C(0x0001)
#define RTCVSEGMAPDESC_F_WRITE      UINT16_C(0x0002)
#define RTCVSEGMAPDESC_F_EXECUTE    UINT16_C(0x0004)
#define RTCVSEGMAPDESC_F_32BIT      UINT16_C(0x0008)
#define RTCVSEGMAPDESC_F_SEL        UINT16_C(0x0100)
#define RTCVSEGMAPDESC_F_ABS        UINT16_C(0x0200)
#define RTCVSEGMAPDESC_F_GROUP      UINT16_C(0x1000)
#define RTCVSEGMAPDESC_F_RESERVED   UINT16_C(0xecf0)
/** @} */

/**
 * CV4 segment map subsection.
 */
typedef struct RTCVSEGMAP
{
    /** The header. */
    RTCVSEGMAPHDR   Hdr;
    /** Descriptor array. */
    RTCVSEGMAPDESC  aDescs[1];
} RTCVSEGMAP;
/** Pointer to a segment map subsection. */
typedef RTCVSEGMAP *PRTCVSEGMAP;
/** Pointer to a const segment map subsection. */
typedef RTCVSEGMAP const *PCRTCVSEGMAP;


/**
 * Global symbol table header, used by kCvSst_GlobalSym and kCvSst_GlobalPub.
 */
typedef struct RTCVGLOBALSYMTABHDR
{
    /** The symbol hash function. */
    uint16_t        uSymHash;
    /** The address hash function. */
    uint16_t        uAddrHash;
    /** The amount of symbol information following immediately after the header. */
    uint32_t        cbSymbols;
    /** The amount of symbol hash tables following the symbols. */
    uint32_t        cbSymHash;
    /** The amount of address hash tables following the symbol hash tables. */
    uint32_t        cbAddrHash;
} RTCVGLOBALSYMTABHDR;
/** Pointer to a global symbol table header. */
typedef RTCVGLOBALSYMTABHDR *PRTCVGLOBALSYMTABHDR;
/** Pointer to a const global symbol table header. */
typedef RTCVGLOBALSYMTABHDR const *PCRTCVGLOBALSYMTABHDR;


typedef enum RTCVSYMTYPE
{
    /** @name Symbols that doesn't change with compilation model or target machine.
     * @{ */
    kCvSymType_Compile = 0x0001,
    kCvSymType_Register,
    kCvSymType_Constant,
    kCvSymType_UDT,
    kCvSymType_SSearch,
    kCvSymType_End,
    kCvSymType_Skip,
    kCvSymType_CVReserve,
    kCvSymType_ObjName,
    kCvSymType_EndArg,
    kCvSymType_CobolUDT,
    kCvSymType_ManyReg,
    kCvSymType_Return,
    kCvSymType_EntryThis,
    /** @}  */

    /** @name Symbols with 16:16 addresses.
     * @{ */
    kCvSymType_BpRel16 = 0x0100,
    kCvSymType_LData16,
    kCvSymType_GData16,
    kCvSymType_Pub16,
    kCvSymType_LProc16,
    kCvSymType_GProc16,
    kCvSymType_Thunk16,
    kCvSymType_BLock16,
    kCvSymType_With16,
    kCvSymType_Label16,
    kCvSymType_CExModel16,
    kCvSymType_VftPath16,
    kCvSymType_RegRel16,
    /** @}  */

    /** @name Symbols with 16:32 addresses.
     * @{ */
    kCvSymType_BpRel32 = 0x0200,
    kCvSymType_LData32,
    kCvSymType_GData32,
    kCvSymType_Pub32,
    kCvSymType_LProc32,
    kCvSymType_GProc32,
    kCvSymType_Thunk32,
    kCvSymType_Block32,
    kCvSymType_With32,
    kCvSymType_Label32,
    kCvSymType_CExModel32,
    kCvSymType_VftPath32,
    kCvSymType_RegRel32,
    kCvSymType_LThread32,
    kCvSymType_GThread32,
    /** @}  */

    /** @name Symbols for MIPS.
     * @{ */
    kCvSymType_LProcMips = 0x0300,
    kCvSymType_GProcMips,
    /** @} */

    /** @name Symbols for Microsoft CodeView.
     * @{ */
    kCvSymType_ProcRef,
    kCvSymType_DataRef,
    kCvSymType_Align
    /** @} */
} RTCVSYMTYPE;
typedef RTCVSYMTYPE *PRTCVSYMTYPE;
typedef RTCVSYMTYPE const *PCRTCVSYMTYPE;


/** The $$SYMBOL table signature for CV4. */
#define RTCVSYMBOLS_SIGNATURE_CV4   UINT32_C(0x00000001)


/**
 * Directory sorting order.
 */
typedef enum RTCVDIRORDER
{
    RTCVDIRORDER_INVALID = 0,
    /** Ordered by module. */
    RTCVDIRORDER_BY_MOD,
    /** Ordered by module, but 0 modules at the end. */
    RTCVDIRORDER_BY_MOD_0,
    /** Ordered by section, with global modules at the end. */
    RTCVDIRORDER_BY_SST_MOD
} RTCVDIRORDER;


/**
 * File type.
 */
typedef enum RTCVFILETYPE
{
    RTCVFILETYPE_INVALID = 0,
    /** Executable image. */
    RTCVFILETYPE_IMAGE,
    /** A DBG-file with a IMAGE_SEPARATE_DEBUG_HEADER. */
    RTCVFILETYPE_DBG,
    /** A PDB file. */
    RTCVFILETYPE_PDB,
    /** Some other kind of file with CV at the end. */
    RTCVFILETYPE_OTHER_AT_END,
    /** The end of the valid values. */
    RTCVFILETYPE_END,
    /** Type blowup. */
    RTCVFILETYPE_32BIT_HACK = 0x7fffffff
} RTCVFILETYPE;


/**
 * CodeView debug info reader instance.
 */
typedef struct RTDBGMODCV
{
    /** Using a container for managing the debug info. */
    RTDBGMOD        hCnt;

    /** @name Codeview details
     * @{ */
    /** The code view magic (used as format indicator). */
    uint32_t        u32CvMagic;
    /** The offset of the CV debug info in the file. */
    uint32_t        offBase;
    /** The size of the CV debug info. */
    uint32_t        cbDbgInfo;
    /** The offset of the subsection directory (relative to offBase). */
    uint32_t        offDir;
    /** The directory order. */
    RTCVDIRORDER    enmDirOrder;
    /** @}  */

    /** @name COFF details.
     * @{ */
    /** Offset of the COFF header. */
    uint32_t        offCoffDbgInfo;
    /** The size of the COFF debug info. */
    uint32_t        cbCoffDbgInfo;
    /** The COFF debug info header. */
    IMAGE_COFF_SYMBOLS_HEADER CoffHdr;
    /** @} */

    /** The file type. */
    RTCVFILETYPE    enmType;
    /** The file handle (if external).  */
    RTFILE          hFile;
    /** Pointer to the module (no reference retained). */
    PRTDBGMODINT    pMod;

    /** The image size, if we know it. This is 0 if we don't know it. */
    uint32_t        cbImage;

    /** Indicates that we've loaded segments intot he container already. */
    bool            fHaveLoadedSegments;
    /** Alternative address translation method for DOS frames. */
    bool            fHaveDosFrames;

    /** @name Codeview Parsing state.
     * @{ */
    /** Number of directory entries. */
    uint32_t        cDirEnts;
    /** The directory (converted to 32-bit). */
    PRTCVDIRENT32   paDirEnts;
    /** Current debugging style when parsing modules. */
    uint16_t        uCurStyle;
    /** Current debugging style version (HLL only). */
    uint16_t        uCurStyleVer;

    /** The segment map (if present). */
    PRTCVSEGMAP     pSegMap;
    /** Segment names. */
    char           *pszzSegNames;
    /** The size of the segment names. */
    uint32_t        cbSegNames;

    /** @}  */

} RTDBGMODCV;
/** Pointer to a codeview debug info reader instance. */
typedef RTDBGMODCV *PRTDBGMODCV;
/** Pointer to a const codeview debug info reader instance. */
typedef RTDBGMODCV *PCRTDBGMODCV;



/**
 * Subsection callback.
 *
 * @returns IPRT status code.
 * @param   pThis           The CodeView debug info reader instance.
 * @param   pvSubSect       Pointer to the subsection data.
 * @param   cbSubSect       The size of the subsection data.
 * @param   pDirEnt         The directory entry.
 */
typedef DECLCALLBACK(int) FNDBGMODCVSUBSECTCALLBACK(PRTDBGMODCV pThis, void const *pvSubSect, size_t cbSubSect,
                                                    PCRTCVDIRENT32 pDirEnt);
/** Pointer to a subsection callback. */
typedef FNDBGMODCVSUBSECTCALLBACK *PFNDBGMODCVSUBSECTCALLBACK;



/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Light weight assert + return w/ fixed status code. */
#define RTDBGMODCV_CHECK_RET_BF(a_Expr, a_LogArgs) \
    do { \
        if (!(a_Expr)) \
        { \
            Log(("RTDbgCv: Check failed on line %d: " #a_Expr "\n", __LINE__)); \
            Log(a_LogArgs); \
            /*return VERR_CV_BAD_FORMAT;*/ \
        } \
    } while (0)


/** Light weight assert + return w/ fixed status code. */
#define RTDBGMODCV_CHECK_NOMSG_RET_BF(a_Expr) \
    do { \
        if (!(a_Expr)) \
        { \
            Log(("RTDbgCv: Check failed on line %d: " #a_Expr "\n", __LINE__)); \
            /*return VERR_CV_BAD_FORMAT;*/ \
        } \
    } while (0)





/**
 * Reads CodeView information.
 *
 * @returns IPRT status.
 * @param   pThis               The CodeView reader instance.
 * @param   off                 The offset to start reading at, relative to the
 *                              CodeView base header.
 * @param   pvBuf               The buffer to read into.
 * @param   cb                  How many bytes to read.
 */
static int rtDbgModCvReadAt(PRTDBGMODCV pThis, uint32_t off, void *pvBuf, size_t cb)
{
    int rc;
    if (pThis->hFile == NIL_RTFILE)
        rc = pThis->pMod->pImgVt->pfnReadAt(pThis->pMod, UINT32_MAX, off + pThis->offBase, pvBuf, cb);
    else
        rc = RTFileReadAt(pThis->hFile, off + pThis->offBase, pvBuf, cb, NULL);
    return rc;
}


/**
 * Reads CodeView information into an allocated buffer.
 *
 * @returns IPRT status.
 * @param   pThis               The CodeView reader instance.
 * @param   off                 The offset to start reading at, relative to the
 *                              CodeView base header.
 * @param   ppvBuf              Where to return the allocated buffer on success.
 * @param   cb                  How many bytes to read.
 */
static int rtDbgModCvReadAtAlloc(PRTDBGMODCV pThis, uint32_t off, void **ppvBuf, size_t cb)
{
    int   rc;
    void *pvBuf = *ppvBuf = RTMemAlloc(cb);
    if (pvBuf)
    {
        if (pThis->hFile == NIL_RTFILE)
            rc = pThis->pMod->pImgVt->pfnReadAt(pThis->pMod, UINT32_MAX, off + pThis->offBase, pvBuf, cb);
        else
            rc = RTFileReadAt(pThis->hFile, off + pThis->offBase, pvBuf, cb, NULL);
        if (RT_SUCCESS(rc))
            return VINF_SUCCESS;

        RTMemFree(pvBuf);
        *ppvBuf = NULL;
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


/**
 * Gets a name string for a subsection type.
 *
 * @returns Section name (read only).
 * @param   uSubSectType    The subsection type.
 */
static const char *rtDbgModCvGetSubSectionName(uint16_t uSubSectType)
{
    switch (uSubSectType)
    {
        case kCvSst_OldModule:      return "sstOldModule";
        case kCvSst_OldPublic:      return "sstOldPublic";
        case kCvSst_OldTypes:       return "sstOldTypes";
        case kCvSst_OldSymbols:     return "sstOldSymbols";
        case kCvSst_OldSrcLines:    return "sstOldSrcLines";
        case kCvSst_OldLibraries:   return "sstOldLibraries";
        case kCvSst_OldImports:     return "sstOldImports";
        case kCvSst_OldCompacted:   return "sstOldCompacted";
        case kCvSst_OldSrcLnSeg:    return "sstOldSrcLnSeg";
        case kCvSst_OldSrcLines3:   return "sstOldSrcLines3";

        case kCvSst_Module:         return "sstModule";
        case kCvSst_Types:          return "sstTypes";
        case kCvSst_Public:         return "sstPublic";
        case kCvSst_PublicSym:      return "sstPublicSym";
        case kCvSst_Symbols:        return "sstSymbols";
        case kCvSst_AlignSym:       return "sstAlignSym";
        case kCvSst_SrcLnSeg:       return "sstSrcLnSeg";
        case kCvSst_SrcModule:      return "sstSrcModule";
        case kCvSst_Libraries:      return "sstLibraries";
        case kCvSst_GlobalSym:      return "sstGlobalSym";
        case kCvSst_GlobalPub:      return "sstGlobalPub";
        case kCvSst_GlobalTypes:    return "sstGlobalTypes";
        case kCvSst_MPC:            return "sstMPC";
        case kCvSst_SegMap:         return "sstSegMap";
        case kCvSst_SegName:        return "sstSegName";
        case kCvSst_PreComp:        return "sstPreComp";
        case kCvSst_PreCompMap:     return "sstPreCompMap";
        case kCvSst_OffsetMap16:    return "sstOffsetMap16";
        case kCvSst_OffsetMap32:    return "sstOffsetMap32";
        case kCvSst_FileIndex:      return "sstFileIndex";
        case kCvSst_StaticSym:      return "sstStaticSym";
    }
    static char s_sz[32];
    RTStrPrintf(s_sz, sizeof(s_sz), "Unknown%#x", uSubSectType);
    return s_sz;
}


/**
 * Adds a symbol to the container.
 *
 * @returns IPRT status code
 * @param   pThis               The CodeView debug info reader instance.
 * @param   iSeg                Segment number.
 * @param   off                 Offset into the segment
 * @param   pchName             The symbol name (not necessarily terminated).
 * @param   cchName             The symbol name length.
 * @param   fFlags              Flags reserved for future exploits, MBZ.
 */
static int rtDbgModCvAddSymbol(PRTDBGMODCV pThis, uint32_t iSeg, uint64_t off, const char *pchName,
                               uint8_t cchName, uint32_t fFlags)
{
    const char *pszName = RTStrCacheEnterN(g_hDbgModStrCache, pchName, cchName);
    if (!pszName)
        return VERR_NO_STR_MEMORY;
#if 1
    Log2(("CV Sym: %04x:%08x %.*s\n", iSeg, off, cchName, pchName));
    if (iSeg == 0)
        iSeg = RTDBGSEGIDX_ABS;
    else if (pThis->pSegMap)
    {
        if (pThis->fHaveDosFrames)
        {
            if (   iSeg > pThis->pSegMap->Hdr.cSegs
                || iSeg == 0)
            {
                Log(("Invalid segment index/offset %#06x:%08x for symbol %.*s\n", iSeg, off, cchName, pchName));
                return VERR_CV_BAD_FORMAT;
            }
            if (off <= pThis->pSegMap->aDescs[iSeg - 1].cb + pThis->pSegMap->aDescs[iSeg - 1].off)
                off -= pThis->pSegMap->aDescs[iSeg - 1].off;
            else
            {
                /* Workaround for VGABIOS where _DATA symbols like vgafont8 are
                   reported in the VGAROM segment. */
                uint64_t uAddrSym = off + ((uint32_t)pThis->pSegMap->aDescs[iSeg - 1].iFrame << 4);
                uint16_t j = pThis->pSegMap->Hdr.cSegs;
                while (j-- > 0)
                {
                    uint64_t uAddrFirst = (uint64_t)pThis->pSegMap->aDescs[j].off
                                        + ((uint32_t)pThis->pSegMap->aDescs[j].iFrame << 4);
                    if (uAddrSym - uAddrFirst < pThis->pSegMap->aDescs[j].cb)
                    {
                        Log(("CV addr fix: %04x:%08x -> %04x:%08x\n", iSeg, off, j + 1, uAddrSym - uAddrFirst));
                        off  = uAddrSym - uAddrFirst;
                        iSeg = j + 1;
                        break;
                    }
                }
                if (j == UINT16_MAX)
                {
                    Log(("Invalid segment index/offset %#06x:%08x for symbol %.*s [2]\n", iSeg, off, cchName, pchName));
                    return VERR_CV_BAD_FORMAT;
                }
            }
        }
        else
        {
            if (   iSeg > pThis->pSegMap->Hdr.cSegs
                || iSeg == 0
                || off > pThis->pSegMap->aDescs[iSeg - 1].cb)
            {
                Log(("Invalid segment index/offset %#06x:%08x for symbol %.*s\n", iSeg, off, cchName, pchName));
                return VERR_CV_BAD_FORMAT;
            }
            off += pThis->pSegMap->aDescs[iSeg - 1].off;
        }
        if (pThis->pSegMap->aDescs[iSeg - 1].fFlags & RTCVSEGMAPDESC_F_ABS)
            iSeg = RTDBGSEGIDX_ABS;
        else
            iSeg = pThis->pSegMap->aDescs[iSeg - 1].iGroup;
    }

    int rc = RTDbgModSymbolAdd(pThis->hCnt, pszName, iSeg, off, 0, 0 /*fFlags*/, NULL);
    Log(("Symbol: %04x:%08x %.*s [%Rrc]\n", iSeg, off, cchName, pchName, rc));
    if (rc == VERR_DBG_ADDRESS_CONFLICT || rc == VERR_DBG_DUPLICATE_SYMBOL)
        rc = VINF_SUCCESS;
    RTStrCacheRelease(g_hDbgModStrCache, pszName);
    return rc;
#else
    Log(("Symbol: %04x:%08x %.*s\n", iSeg, off, cchName, pchName));
    return VINF_SUCCESS;
#endif
}


/**
 * Parses a CV4 symbol table, adding symbols to the container.
 *
 * @returns IPRT status code
 * @param   pThis               The CodeView debug info reader instance.
 * @param   pbSymTab            The symbol table.
 * @param   cbSymTab            The size of the symbol table.
 * @param   fFlags              Flags reserved for future exploits, MBZ.
 */
static int rtDbgModCvSsProcessV4SymTab(PRTDBGMODCV pThis, void const *pvSymTab, size_t cbSymTab, uint32_t fFlags)
{
    int         rc = VINF_SUCCESS;
    RTCPTRUNION uCursor;
    uCursor.pv = pvSymTab;

    while (cbSymTab > 0 && RT_SUCCESS(rc))
    {
        uint8_t const * const pbRecStart = uCursor.pu8;
        uint16_t cbRec = *uCursor.pu16++;
        if (cbRec >= 2)
        {
            uint16_t uSymType = *uCursor.pu16++;

            Log3(("    %p: uSymType=%#06x LB %#x\n", pbRecStart - (uint8_t *)pvSymTab, uSymType, cbRec));
            RTDBGMODCV_CHECK_RET_BF(cbRec >= 2 && cbRec <= cbSymTab, ("cbRec=%#x cbSymTab=%#x\n", cbRec, cbSymTab));

            switch (uSymType)
            {
                case kCvSymType_LData16:
                case kCvSymType_GData16:
                case kCvSymType_Pub16:
                {
                    RTDBGMODCV_CHECK_NOMSG_RET_BF(cbRec > 2 + 2+2+2+1);
                    uint16_t off     = *uCursor.pu16++;
                    uint16_t iSeg    = *uCursor.pu16++;
                    /*uint16_t iType   =*/ *uCursor.pu16++;
                    uint8_t  cchName = *uCursor.pu8++;
                    RTDBGMODCV_CHECK_NOMSG_RET_BF(cchName > 0);
                    RTDBGMODCV_CHECK_NOMSG_RET_BF(cbRec >= 2 + 2+2+2+1 + cchName);

                    rc = rtDbgModCvAddSymbol(pThis, iSeg, off, uCursor.pch, cchName, 0);
                    break;
                }

                case kCvSymType_LData32:
                case kCvSymType_GData32:
                case kCvSymType_Pub32:
                {
                    RTDBGMODCV_CHECK_NOMSG_RET_BF(cbRec > 2 + 4+2+2+1);
                    uint32_t off     = *uCursor.pu32++;
                    uint16_t iSeg    = *uCursor.pu16++;
                    /*uint16_t iType   =*/ *uCursor.pu16++;
                    uint8_t  cchName = *uCursor.pu8++;
                    RTDBGMODCV_CHECK_NOMSG_RET_BF(cchName > 0);
                    RTDBGMODCV_CHECK_NOMSG_RET_BF(cbRec >= 2 + 4+2+2+1 + cchName);

                    rc = rtDbgModCvAddSymbol(pThis, iSeg, off, uCursor.pch, cchName, 0);
                    break;
                }

                /** @todo add GProc and LProc so we can gather sizes as well as just symbols. */
            }
        }
        /*else: shorter records can be used for alignment, I guess. */

        /* next */
        uCursor.pu8 = pbRecStart + cbRec + 2;
        cbSymTab   -= cbRec + 2;
    }
    return rc;
}


/** @callback_method_impl{FNDBGMODCVSUBSECTCALLBACK,
 * Parses kCvSst_GlobalPub, kCvSst_GlobalSym and kCvSst_StaticSym subsections,
 * adding symbols it finds to the container.} */
static DECLCALLBACK(int)
rtDbgModCvSs_GlobalPub_GlobalSym_StaticSym(PRTDBGMODCV pThis, void const *pvSubSect, size_t cbSubSect, PCRTCVDIRENT32 pDirEnt)
{
    PCRTCVGLOBALSYMTABHDR pHdr = (PCRTCVGLOBALSYMTABHDR)pvSubSect;

    /*
     * Quick data validation.
     */
    Log2(("RTDbgModCv: %s: uSymHash=%#x uAddrHash=%#x cbSymbols=%#x cbSymHash=%#x cbAddrHash=%#x\n",
          rtDbgModCvGetSubSectionName(pDirEnt->uSubSectType), pHdr->uSymHash,
          pHdr->uAddrHash, pHdr->cbSymbols, pHdr->cbSymHash, pHdr->cbAddrHash));
    RTDBGMODCV_CHECK_NOMSG_RET_BF(cbSubSect >= sizeof(RTCVGLOBALSYMTABHDR));
    RTDBGMODCV_CHECK_NOMSG_RET_BF((uint64_t)pHdr->cbSymbols + pHdr->cbSymHash + pHdr->cbAddrHash <= cbSubSect - sizeof(*pHdr));
    RTDBGMODCV_CHECK_NOMSG_RET_BF(pHdr->uSymHash  < 0x20);
    RTDBGMODCV_CHECK_NOMSG_RET_BF(pHdr->uAddrHash < 0x20);
    if (!pHdr->cbSymbols)
        return VINF_SUCCESS;

    /*
     * Parse the symbols.
     */
    return rtDbgModCvSsProcessV4SymTab(pThis, pHdr + 1, pHdr->cbSymbols, 0);
}


/** @callback_method_impl{FNDBGMODCVSUBSECTCALLBACK,
 * Parses kCvSst_Module subsection, storing the debugging style in pThis.} */
static DECLCALLBACK(int)
rtDbgModCvSs_Module(PRTDBGMODCV pThis, void const *pvSubSect, size_t cbSubSect, PCRTCVDIRENT32 pDirEnt)
{
    RTCPTRUNION uCursor;
    uCursor.pv = pvSubSect;
    RTDBGMODCV_CHECK_NOMSG_RET_BF(cbSubSect >= 2 + 2 + 2 + 2 + 0 + 1);
    uint16_t iOverlay = *uCursor.pu16++;
    uint16_t iLib     = *uCursor.pu16++;
    uint16_t cSegs    = *uCursor.pu16++;
    pThis->uCurStyle  = *uCursor.pu16++;
    if (pThis->uCurStyle == 0)
        pThis->uCurStyle = RT_MAKE_U16('C', 'V');
    pThis->uCurStyleVer = 0;
    uint8_t cchName   = uCursor.pu8[cSegs * 12];
    RTDBGMODCV_CHECK_NOMSG_RET_BF(cbSubSect >= 2 + 2 + 2 + 2 + cSegs * 12U + 1 + cchName);

    const char *pchName = (const char *)&uCursor.pu8[cSegs * 12 + 1];
    Log2(("RTDbgModCv: Module: iOverlay=%#x iLib=%#x cSegs=%#x Style=%c%c (%#x) %.*s\n", iOverlay, iLib, cSegs,
          RT_BYTE1(pThis->uCurStyle), RT_BYTE2(pThis->uCurStyle), pThis->uCurStyle, cchName, pchName));
    RTDBGMODCV_CHECK_NOMSG_RET_BF(pThis->uCurStyle == RT_MAKE_U16('C', 'V'));

    PCRTCVMODSEGINFO32 paSegs = (PCRTCVMODSEGINFO32)uCursor.pv;
    for (uint16_t iSeg = 0; iSeg < cSegs; iSeg++)
        Log2(("    #%02u: %04x:%08x LB %08x\n", iSeg, paSegs[iSeg].iSeg, paSegs[iSeg].off, paSegs[iSeg].cb));

    return VINF_SUCCESS;
}


/** @callback_method_impl{FNDBGMODCVSUBSECTCALLBACK,
 * Parses kCvSst_Symbols, kCvSst_PublicSym and kCvSst_AlignSym subsections,
 * adding symbols it finds to the container.} */
static DECLCALLBACK(int)
rtDbgModCvSs_Symbols_PublicSym_AlignSym(PRTDBGMODCV pThis, void const *pvSubSect, size_t cbSubSect, PCRTCVDIRENT32 pDirEnt)
{
    RTDBGMODCV_CHECK_NOMSG_RET_BF(pThis->uCurStyle == RT_MAKE_U16('C', 'V'));
    RTDBGMODCV_CHECK_NOMSG_RET_BF(cbSubSect >= 8);

    uint32_t u32Signature = *(uint32_t const *)pvSubSect;
    RTDBGMODCV_CHECK_RET_BF(u32Signature == RTCVSYMBOLS_SIGNATURE_CV4,
                            ("%#x, expected %#x\n", u32Signature, RTCVSYMBOLS_SIGNATURE_CV4));

    return rtDbgModCvSsProcessV4SymTab(pThis, (uint8_t const *)pvSubSect + 4, cbSubSect - 4, 0);
}


static int rtDbgModCvLoadSegmentMap(PRTDBGMODCV pThis)
{
    /*
     * Search for the segment map and segment names. They will be at the end of the directory.
     */
    uint32_t iSegMap   = UINT32_MAX;
    uint32_t iSegNames = UINT32_MAX;
    uint32_t i = pThis->cDirEnts;
    while (i-- > 0)
    {
        if (   pThis->paDirEnts[i].iMod != 0xffff
            && pThis->paDirEnts[i].iMod != 0x0000)
            break;
        if (pThis->paDirEnts[i].uSubSectType == kCvSst_SegMap)
            iSegMap = i;
        else if (pThis->paDirEnts[i].uSubSectType == kCvSst_SegName)
            iSegNames = i;
    }
    if (iSegMap == UINT32_MAX)
    {
        Log(("RTDbgModCv: No segment map present, using segment indexes as is then...\n"));
        return VINF_SUCCESS;
    }
    RTDBGMODCV_CHECK_RET_BF(pThis->paDirEnts[iSegMap].cb >= sizeof(RTCVSEGMAPHDR),
                            ("Bad sstSegMap entry: cb=%#x\n", pThis->paDirEnts[iSegMap].cb));
    RTDBGMODCV_CHECK_NOMSG_RET_BF(iSegNames == UINT32_MAX || pThis->paDirEnts[iSegNames].cb > 0);

    /*
     * Read them into memory.
     */
    int rc = rtDbgModCvReadAtAlloc(pThis, pThis->paDirEnts[iSegMap].off, (void **)&pThis->pSegMap,
                                   pThis->paDirEnts[iSegMap].cb);
    if (iSegNames != UINT32_MAX && RT_SUCCESS(rc))
    {
        pThis->cbSegNames = pThis->paDirEnts[iSegNames].cb;
        rc = rtDbgModCvReadAtAlloc(pThis, pThis->paDirEnts[iSegNames].off, (void **)&pThis->pszzSegNames,
                                   pThis->paDirEnts[iSegNames].cb);
    }
    if (RT_FAILURE(rc))
        return rc;
    RTDBGMODCV_CHECK_NOMSG_RET_BF(!pThis->pszzSegNames || !pThis->pszzSegNames[pThis->cbSegNames - 1]); /* must be terminated */

    /* Use local pointers to avoid lots of indirection and typing. */
    PCRTCVSEGMAPHDR  pHdr    = &pThis->pSegMap->Hdr;
    PRTCVSEGMAPDESC  paDescs = &pThis->pSegMap->aDescs[0];

    /*
     * If there are only logical segments, assume a direct mapping.
     * PE images, like the NT4 kernel, does it like this.
     */
    bool const fNoGroups = pHdr->cSegs == pHdr->cLogSegs;

    /*
     * The PE image has an extra section/segment for the headers, the others
     * doesn't.  PE images doesn't have DOS frames. So, figure the image type now.
     */
    RTLDRFMT enmImgFmt = RTLDRFMT_INVALID;
    if (pThis->pMod->pImgVt)
        enmImgFmt = pThis->pMod->pImgVt->pfnGetFormat(pThis->pMod);

    /*
     * Validate and display it all.
     */
    Log2(("RTDbgModCv: SegMap: cSegs=%#x cLogSegs=%#x (cbSegNames=%#x)\n", pHdr->cSegs, pHdr->cLogSegs, pThis->cbSegNames));
    RTDBGMODCV_CHECK_RET_BF(pThis->paDirEnts[iSegMap].cb >= sizeof(*pHdr) + pHdr->cSegs * sizeof(paDescs[0]),
                            ("SegMap is out of bounds: cbSubSect=%#x cSegs=%#x\n", pThis->paDirEnts[iSegMap].cb, pHdr->cSegs));
    RTDBGMODCV_CHECK_NOMSG_RET_BF(pHdr->cSegs >= pHdr->cLogSegs);

    Log2(("Logical segment descriptors: %u\n", pHdr->cLogSegs));

    bool fHaveDosFrames = false;
    for (i = 0; i < pHdr->cSegs; i++)
    {
        if (i == pHdr->cLogSegs)
            Log2(("Group/Physical descriptors: %u\n", pHdr->cSegs - pHdr->cLogSegs));
        uint16_t idx = i < pHdr->cLogSegs ? i : i - pHdr->cLogSegs;
        char szFlags[16];
        memset(szFlags, '-', sizeof(szFlags));
        if (paDescs[i].fFlags & RTCVSEGMAPDESC_F_READ)
            szFlags[0] = 'R';
        if (paDescs[i].fFlags & RTCVSEGMAPDESC_F_WRITE)
            szFlags[1] = 'W';
        if (paDescs[i].fFlags & RTCVSEGMAPDESC_F_EXECUTE)
            szFlags[2] = 'X';
        if (paDescs[i].fFlags & RTCVSEGMAPDESC_F_32BIT)
            szFlags[3] = '3', szFlags[4]  = '2';
        if (paDescs[i].fFlags & RTCVSEGMAPDESC_F_SEL)
            szFlags[5] = 'S';
        if (paDescs[i].fFlags & RTCVSEGMAPDESC_F_ABS)
            szFlags[6] = 'A';
        if (paDescs[i].fFlags & RTCVSEGMAPDESC_F_GROUP)
            szFlags[7] = 'G';
        szFlags[8] = '\0';
        if (paDescs[i].fFlags & RTCVSEGMAPDESC_F_RESERVED)
            szFlags[8]  = '!', szFlags[9] = '\0';
        Log2(("    #%02u: %#010x LB %#010x flags=%#06x ovl=%#06x group=%#06x frame=%#06x iSegName=%#06x iClassName=%#06x %s\n",
              idx, paDescs[i].off, paDescs[i].cb, paDescs[i].fFlags, paDescs[i].iOverlay, paDescs[i].iGroup,
              paDescs[i].iFrame, paDescs[i].offSegName, paDescs[i].offClassName, szFlags));

        RTDBGMODCV_CHECK_NOMSG_RET_BF(paDescs[i].offSegName == UINT16_MAX || paDescs[i].offSegName < pThis->cbSegNames);
        RTDBGMODCV_CHECK_NOMSG_RET_BF(paDescs[i].offClassName == UINT16_MAX || paDescs[i].offClassName < pThis->cbSegNames);
        const char *pszName  = paDescs[i].offSegName   != UINT16_MAX
                             ? pThis->pszzSegNames + paDescs[i].offSegName
                             : NULL;
        const char *pszClass = paDescs[i].offClassName != UINT16_MAX
                             ? pThis->pszzSegNames + paDescs[i].offClassName
                             : NULL;
        if (pszName || pszClass)
            Log2(("              pszName=%s pszClass=%s\n", pszName, pszClass));

        /* Validate the group link. */
        RTDBGMODCV_CHECK_NOMSG_RET_BF(paDescs[i].iGroup == 0 || !(paDescs[i].fFlags & RTCVSEGMAPDESC_F_GROUP));
        RTDBGMODCV_CHECK_NOMSG_RET_BF(   paDescs[i].iGroup == 0
                                      || (   paDescs[i].iGroup >= pHdr->cLogSegs
                                          && paDescs[i].iGroup <  pHdr->cSegs));
        RTDBGMODCV_CHECK_NOMSG_RET_BF(   paDescs[i].iGroup == 0
                                      || (paDescs[paDescs[i].iGroup].fFlags & RTCVSEGMAPDESC_F_GROUP));
        RTDBGMODCV_CHECK_NOMSG_RET_BF(!(paDescs[i].fFlags & RTCVSEGMAPDESC_F_GROUP) || paDescs[i].off == 0); /* assumed below */

        if (fNoGroups)
        {
            RTDBGMODCV_CHECK_NOMSG_RET_BF(paDescs[i].iGroup == 0);
            if (   !fHaveDosFrames
                && paDescs[i].iFrame != 0
                && (paDescs[i].fFlags & (RTCVSEGMAPDESC_F_SEL | RTCVSEGMAPDESC_F_ABS))
                && paDescs[i].iOverlay == 0
                && enmImgFmt != RTLDRFMT_PE
                && pThis->enmType != RTCVFILETYPE_DBG)
                fHaveDosFrames = true; /* BIOS, only groups with frames. */
        }
    }

    /*
     * Further valiations based on fHaveDosFrames or not.
     */
    if (fNoGroups)
    {
        if (fHaveDosFrames)
            for (i = 0; i < pHdr->cSegs; i++)
            {
                RTDBGMODCV_CHECK_NOMSG_RET_BF(paDescs[i].iOverlay == 0);
                RTDBGMODCV_CHECK_NOMSG_RET_BF(      (paDescs[i].fFlags & (RTCVSEGMAPDESC_F_SEL | RTCVSEGMAPDESC_F_ABS))
                                                 == RTCVSEGMAPDESC_F_SEL
                                              ||    (paDescs[i].fFlags & (RTCVSEGMAPDESC_F_SEL | RTCVSEGMAPDESC_F_ABS))
                                                 == RTCVSEGMAPDESC_F_ABS);
                RTDBGMODCV_CHECK_NOMSG_RET_BF(!(paDescs[i].fFlags & RTCVSEGMAPDESC_F_ABS));
            }
        else
            for (i = 0; i < pHdr->cSegs; i++)
                RTDBGMODCV_CHECK_NOMSG_RET_BF(paDescs[i].off == 0);
    }

    /*
     * Modify the groups index to be the loader segment index instead, also
     * add the segments to the container if we haven't done that already.
     */

    /* Guess work: Group can be implicit if used.  Observed Visual C++ v1.5,
       omitting the CODE group. */
    const char *pszGroup0 = NULL;
    uint64_t    cbGroup0  = 0;
    if (!fNoGroups && !fHaveDosFrames)
    {
        for (i = 0; i < pHdr->cSegs; i++)
            if (   !(paDescs[i].fFlags & (RTCVSEGMAPDESC_F_GROUP | RTCVSEGMAPDESC_F_ABS))
                && paDescs[i].iGroup == 0)
            {
                if (pszGroup0 == NULL && paDescs[i].offClassName != UINT16_MAX)
                    pszGroup0 = pThis->pszzSegNames + paDescs[i].offClassName;
                uint64_t offEnd = (uint64_t)paDescs[i].off + paDescs[i].cb;
                if (offEnd > cbGroup0)
                    cbGroup0 = offEnd;
            }
    }

    /* Add the segments.
       Note! The RVAs derived from this exercise are all wrong. :-/
       Note! We don't have an image loader, so we cannot add any fake sections. */
    /** @todo Try see if we can figure something out from the frame value later. */
    if (!pThis->fHaveLoadedSegments)
    {
        uint16_t iSeg = 0;
        if (!fHaveDosFrames)
        {
            Assert(!pThis->pMod->pImgVt); Assert(pThis->enmType != RTCVFILETYPE_DBG);
            uint64_t uRva = 0;
            if (cbGroup0 && !fNoGroups)
            {
                rc = RTDbgModSegmentAdd(pThis->hCnt, 0, cbGroup0, pszGroup0 ? pszGroup0 : "Seg00", 0 /*fFlags*/, NULL);
                uRva += cbGroup0;
                iSeg++;
            }

            for (i = 0; RT_SUCCESS(rc) && i < pHdr->cSegs; i++)
                if ((paDescs[i].fFlags & RTCVSEGMAPDESC_F_GROUP) || fNoGroups)
                {
                    char szName[16];
                    char *pszName = szName;
                    if (paDescs[i].offSegName != UINT16_MAX)
                        pszName = pThis->pszzSegNames + paDescs[i].offSegName;
                    else
                        RTStrPrintf(szName, sizeof(szName), "Seg%02u", iSeg);
                    rc = RTDbgModSegmentAdd(pThis->hCnt, uRva, paDescs[i].cb, pszName, 0 /*fFlags*/, NULL);
                    uRva += paDescs[i].cb;
                    iSeg++;
                }
        }
        else
        {
            /* The map is not sorted by RVA, very annoying, but I'm countering
               by being lazy and slow about it. :-) Btw. this is the BIOS case. */
            Assert(fNoGroups);
#if 1 /** @todo need more inputs */

            /* Figure image base address. */
            uint64_t uImageBase = UINT64_MAX;
            for (i = 0; RT_SUCCESS(rc) && i < pHdr->cSegs; i++)
            {
                uint64_t uAddr = (uint64_t)paDescs[i].off + ((uint32_t)paDescs[i].iFrame << 4);
                if (uAddr < uImageBase)
                    uImageBase = uAddr;
            }

            /* Add the segments. */
            uint64_t uMinAddr = uImageBase;
            for (i = 0; RT_SUCCESS(rc) && i < pHdr->cSegs; i++)
            {
                /* Figure out the next one. */
                uint16_t cOverlaps = 0;
                uint16_t iBest     = UINT16_MAX;
                uint64_t uBestAddr = UINT64_MAX;
                for (uint16_t j = 0; j < pHdr->cSegs; j++)
                {
                    uint64_t uAddr = (uint64_t)paDescs[j].off + ((uint32_t)paDescs[j].iFrame << 4);
                    if (uAddr >= uMinAddr && uAddr < uBestAddr)
                    {
                        uBestAddr = uAddr;
                        iBest     = j;
                    }
                    else if (uAddr == uBestAddr)
                    {
                        cOverlaps++;
                        if (paDescs[j].cb > paDescs[iBest].cb)
                        {
                            uBestAddr = uAddr;
                            iBest     = j;
                        }
                    }
                }
                if (iBest == UINT16_MAX && RT_SUCCESS(rc))
                {
                    rc = VERR_CV_IPE;
                    break;
                }

                /* Add it. */
                char szName[16];
                char *pszName = szName;
                if (paDescs[iBest].offSegName != UINT16_MAX)
                    pszName = pThis->pszzSegNames + paDescs[iBest].offSegName;
                else
                    RTStrPrintf(szName, sizeof(szName), "Seg%02u", iSeg);
                Log(("CV: %#010x LB %#010x %s uRVA=%#010x iBest=%u cOverlaps=%u\n",
                     uBestAddr, paDescs[iBest].cb, szName, uBestAddr - uImageBase, iBest, cOverlaps));
                rc = RTDbgModSegmentAdd(pThis->hCnt, uBestAddr - uImageBase, paDescs[iBest].cb, pszName, 0 /*fFlags*/, NULL);

                /* Update translations. */
                paDescs[iBest].iGroup = iSeg;
                if (cOverlaps > 0)
                {
                    for (uint16_t j = 0; j < pHdr->cSegs; j++)
                        if ((uint64_t)paDescs[j].off + ((uint32_t)paDescs[j].iFrame << 4) == uBestAddr)
                            paDescs[iBest].iGroup = iSeg;
                    i += cOverlaps;
                }

                /* Advance. */
                uMinAddr = uBestAddr + 1;
                iSeg++;
            }

            pThis->fHaveDosFrames = true;
#else
            uint32_t iFrameFirst = UINT32_MAX;
            uint16_t iSeg        = 0;
            uint32_t iFrameMin   = 0;
            do
            {
                /* Find next frame. */
                uint32_t iFrame = UINT32_MAX;
                for (uint16_t j = 0; j < pHdr->cSegs; j++)
                    if (paDescs[j].iFrame >= iFrameMin && paDescs[j].iFrame < iFrame)
                        iFrame = paDescs[j].iFrame;
                if (iFrame == UINT32_MAX)
                    break;

                /* Figure the frame span. */
                uint32_t offFirst = UINT32_MAX;
                uint64_t offEnd   = 0;
                for (uint16_t j = 0; j < pHdr->cSegs; j++)
                    if (paDescs[j].iFrame == iFrame)
                    {
                        uint64_t offThisEnd = paDescs[j].off + paDescs[j].cb;
                        if (offThisEnd > offEnd)
                            offEnd   = offThisEnd;
                        if (paDescs[j].off < offFirst)
                            offFirst = paDescs[j].off;
                    }

                if (offFirst < offEnd)
                {
                    /* Add it. */
                    char szName[16];
                    RTStrPrintf(szName, sizeof(szName), "Frame_%04x", iFrame);
                    Log(("CV: %s offEnd=%#x offFirst=%#x\n", szName, offEnd, offFirst));
                    if (iFrameFirst == UINT32_MAX)
                        iFrameFirst = iFrame;
                    rc = RTDbgModSegmentAdd(pThis->hCnt, (iFrame - iFrameFirst) << 4, offEnd, szName, 0 /*fFlags*/, NULL);

                    /* Translation updates. */
                    for (uint16_t j = 0; j < pHdr->cSegs; j++)
                        if (paDescs[j].iFrame == iFrame)
                        {
                            paDescs[j].iGroup = iSeg;
                            paDescs[j].off    = 0;
                            paDescs[j].cb     = offEnd > UINT32_MAX ? UINT32_MAX : (uint32_t)offEnd;
                        }

                    iSeg++;
                }

                iFrameMin = iFrame + 1;
            } while (RT_SUCCESS(rc));
#endif
        }

        if (RT_FAILURE(rc))
        {
            Log(("RTDbgModCv: %Rrc while adding segments from SegMap\n", rc));
            return rc;
        }

        pThis->fHaveLoadedSegments = true;

        /* Skip the stuff below if we have DOS frames since we did it all above. */
        if (fHaveDosFrames)
            return VINF_SUCCESS;
    }

    /* Pass one: Fixate the group segment indexes. */
    uint16_t iSeg0 = enmImgFmt == RTLDRFMT_PE || pThis->enmType == RTCVFILETYPE_DBG ? 1 : 0;
    uint16_t iSeg = iSeg0 + (cbGroup0 > 0); /** @todo probably wrong... */
    for (i = 0; i < pHdr->cSegs; i++)
        if (paDescs[i].fFlags & RTCVSEGMAPDESC_F_ABS)
            paDescs[i].iGroup = (uint16_t)RTDBGSEGIDX_ABS;
        else if ((paDescs[i].fFlags & RTCVSEGMAPDESC_F_GROUP) || fNoGroups)
            paDescs[i].iGroup = iSeg++;

    /* Pass two: Resolve group references in to segment indexes. */
    Log2(("Mapped segments (both kinds):\n"));
    for (i = 0; i < pHdr->cSegs; i++)
    {
        if (!fNoGroups && !(paDescs[i].fFlags & (RTCVSEGMAPDESC_F_GROUP | RTCVSEGMAPDESC_F_ABS)))
            paDescs[i].iGroup = paDescs[i].iGroup == 0 ? iSeg0 : paDescs[paDescs[i].iGroup].iGroup;

        Log2(("    #%02u: %#010x LB %#010x -> %#06x (flags=%#06x ovl=%#06x frame=%#06x)\n",
              i, paDescs[i].off, paDescs[i].cb, paDescs[i].iGroup,
              paDescs[i].fFlags, paDescs[i].iOverlay, paDescs[i].iFrame));
    }

    return VINF_SUCCESS;
}


/**
 * Loads the directory into memory (RTDBGMODCV::paDirEnts and
 * RTDBGMODCV::cDirEnts).
 *
 * Converting old format version into the newer format to simplifying the code
 * using the directory.
 *
 *
 * @returns IPRT status code. (May leave with paDirEnts allocated on failure.)
 * @param   pThis               The CV reader instance.
 */
static int rtDbgModCvLoadDirectory(PRTDBGMODCV pThis)
{
    /*
     * Read in the CV directory.
     */
    int rc;
    if (   pThis->u32CvMagic == RTCVHDR_MAGIC_NB00
        || pThis->u32CvMagic == RTCVHDR_MAGIC_NB02)
    {
        /*
         * 16-bit type.
         */
        RTCVDIRHDR16 DirHdr;
        rc = rtDbgModCvReadAt(pThis, pThis->offDir, &DirHdr, sizeof(DirHdr));
        if (RT_SUCCESS(rc))
        {
            if (DirHdr.cEntries > 2 && DirHdr.cEntries < _64K - 32U)
            {
                pThis->cDirEnts  = DirHdr.cEntries;
                pThis->paDirEnts = (PRTCVDIRENT32)RTMemAlloc(DirHdr.cEntries * sizeof(pThis->paDirEnts[0]));
                if (pThis->paDirEnts)
                {
                    rc = rtDbgModCvReadAt(pThis, pThis->offDir + sizeof(DirHdr),
                                          pThis->paDirEnts, DirHdr.cEntries * sizeof(RTCVDIRENT16));
                    if (RT_SUCCESS(rc))
                    {
                        /* Convert the entries (from the end). */
                        uint32_t               cLeft = DirHdr.cEntries;
                        RTCVDIRENT32 volatile *pDst  = pThis->paDirEnts + cLeft;
                        RTCVDIRENT16 volatile *pSrc  = (RTCVDIRENT16 volatile *)pThis->paDirEnts + cLeft;
                        while (cLeft--)
                        {
                            pDst--;
                            pSrc--;

                            pDst->cb           = pSrc->cb;
                            pDst->off          = RT_MAKE_U32(pSrc->offLow, pSrc->offHigh);
                            pDst->iMod         = pSrc->iMod;
                            pDst->uSubSectType = pSrc->uSubSectType;
                        }
                    }
                }
                else
                    rc = VERR_NO_MEMORY;
            }
            else
            {
                Log(("Old CV directory count is out of considered valid range: %#x\n", DirHdr.cEntries));
                rc = VERR_CV_BAD_FORMAT;
            }
        }
    }
    else
    {
        /*
         * 32-bit type (reading too much for NB04 is no problem).
         */
        RTCVDIRHDR32EX DirHdr;
        rc = rtDbgModCvReadAt(pThis, pThis->offDir, &DirHdr, sizeof(DirHdr));
        if (RT_SUCCESS(rc))
        {
            if (   DirHdr.Core.cbHdr != sizeof(DirHdr.Core)
                && DirHdr.Core.cbHdr != sizeof(DirHdr))
            {
                Log(("Unexpected CV directory size: %#x\n", DirHdr.Core.cbHdr));
                rc = VERR_CV_BAD_FORMAT;
            }
            if (   DirHdr.Core.cbHdr == sizeof(DirHdr)
                && (   DirHdr.offNextDir != 0
                    || DirHdr.fFlags     != 0) )
            {
                Log(("Extended CV directory headers fields are not zero: fFlags=%#x offNextDir=%#x\n",
                     DirHdr.fFlags, DirHdr.offNextDir));
                rc = VERR_CV_BAD_FORMAT;
            }
            if (DirHdr.Core.cbEntry != sizeof(RTCVDIRENT32))
            {
                Log(("Unexpected CV directory entry size: %#x (expected %#x)\n", DirHdr.Core.cbEntry, sizeof(RTCVDIRENT32)));
                rc = VERR_CV_BAD_FORMAT;
            }
            if (DirHdr.Core.cEntries < 2 || DirHdr.Core.cEntries >= _512K)
            {
                Log(("CV directory count is out of considered valid range: %#x\n", DirHdr.Core.cEntries));
                rc = VERR_CV_BAD_FORMAT;
            }
            if (RT_SUCCESS(rc))
            {
                pThis->cDirEnts  = DirHdr.Core.cEntries;
                pThis->paDirEnts = (PRTCVDIRENT32)RTMemAlloc(DirHdr.Core.cEntries * sizeof(pThis->paDirEnts[0]));
                if (pThis->paDirEnts)
                    rc = rtDbgModCvReadAt(pThis, pThis->offDir + DirHdr.Core.cbHdr,
                                          pThis->paDirEnts, DirHdr.Core.cEntries * sizeof(RTCVDIRENT32));
                else
                    rc = VERR_NO_MEMORY;
            }
        }
    }

    if (RT_SUCCESS(rc))
    {
        /*
         * Basic info validation and determining the directory ordering.
         */
        bool           fWatcom      = 0;
        uint16_t       cGlobalMods  = 0;
        uint16_t       cNormalMods  = 0;
        uint16_t       iModLast     = 0;
        uint32_t const cbDbgInfo    = pThis->cbDbgInfo;
        uint32_t const cDirEnts     = pThis->cDirEnts;
        Log2(("RTDbgModCv: %u (%#x) directory entries:\n", cDirEnts, cDirEnts));
        for (uint32_t i = 0; i < cDirEnts; i++)
        {
            PCRTCVDIRENT32 pDirEnt = &pThis->paDirEnts[i];
            Log2(("    #%04u mod=%#06x sst=%#06x at %#010x LB %#07x %s\n",
                  i, pDirEnt->iMod, pDirEnt->uSubSectType, pDirEnt->off, pDirEnt->cb,
                  rtDbgModCvGetSubSectionName(pDirEnt->uSubSectType)));

            if (   pDirEnt->off >= cbDbgInfo
                || pDirEnt->cb  >= cbDbgInfo
                || pDirEnt->off + pDirEnt->cb > cbDbgInfo)
            {
                Log(("CV directory entry #%u is out of bounds: %#x LB %#x, max %#x\n", i, pDirEnt->off, pDirEnt->cb, cbDbgInfo));
                rc = VERR_CV_BAD_FORMAT;
            }
            if (   pDirEnt->iMod == 0
                && pThis->u32CvMagic != RTCVHDR_MAGIC_NB04
                && pThis->u32CvMagic != RTCVHDR_MAGIC_NB02
                && pThis->u32CvMagic != RTCVHDR_MAGIC_NB00)
            {
                Log(("CV directory entry #%u uses module index 0 (uSubSectType=%#x)\n", i, pDirEnt->iMod, pDirEnt->uSubSectType));
                rc = VERR_CV_BAD_FORMAT;
            }
            if (pDirEnt->iMod == 0 || pDirEnt->iMod == 0xffff)
                cGlobalMods++;
            else
            {
                if (pDirEnt->iMod > iModLast)
                {
                    if (   pDirEnt->uSubSectType != kCvSst_Module
                        && pDirEnt->uSubSectType != kCvSst_OldModule)
                    {
                        Log(("CV directory entry #%u: expected module subsection first, found %s (%#x)\n",
                             i, rtDbgModCvGetSubSectionName(pDirEnt->uSubSectType), pDirEnt->uSubSectType));
                        rc = VERR_CV_BAD_FORMAT;
                    }
                    if (pDirEnt->iMod != iModLast + 1)
                    {
                        Log(("CV directory entry #%u: skips from mod %#x to %#x modules\n", iModLast, pDirEnt->iMod));
                        rc = VERR_CV_BAD_FORMAT;
                    }
                    iModLast = pDirEnt->iMod;
                }
                else if (pDirEnt->iMod < iModLast)
                    fWatcom = true;
                cNormalMods++;
            }
        }
        if (cGlobalMods == 0)
        {
            Log(("CV directory contains no global modules\n"));
            rc = VERR_CV_BAD_FORMAT;
        }
        if (RT_SUCCESS(rc))
        {
            if (fWatcom)
                pThis->enmDirOrder = RTCVDIRORDER_BY_SST_MOD;
            else if (pThis->paDirEnts[0].iMod == 0)
                pThis->enmDirOrder = RTCVDIRORDER_BY_MOD_0;
            else
                pThis->enmDirOrder = RTCVDIRORDER_BY_MOD;
            Log(("CV dir stats: %u total, %u normal, %u special, iModLast=%#x (%u), enmDirOrder=%d\n",
                 cDirEnts, cNormalMods, cGlobalMods, iModLast, iModLast, pThis->enmDirOrder));


            /*
             * Validate the directory ordering.
             */
            uint16_t i = 0;

            /* Old style with special modules up front. */
            if (pThis->enmDirOrder == RTCVDIRORDER_BY_MOD_0)
                while (i < cGlobalMods)
                {
                    if (pThis->paDirEnts[i].iMod != 0)
                    {
                        Log(("CV directory entry #%u: Expected iMod=%x instead of %x\n", i, 0, pThis->paDirEnts[i].iMod));
                        rc = VERR_CV_BAD_FORMAT;
                    }
                    i++;
                }

            /* Normal modules. */
            if (pThis->enmDirOrder != RTCVDIRORDER_BY_SST_MOD)
            {
                uint16_t iEndNormalMods = cNormalMods + (pThis->enmDirOrder == RTCVDIRORDER_BY_MOD_0 ? cGlobalMods : 0);
                while (i < iEndNormalMods)
                {
                    if (pThis->paDirEnts[i].iMod == 0 || pThis->paDirEnts[i].iMod == 0xffff)
                    {
                        Log(("CV directory entry #%u: Unexpected global module entry.\n", i));
                        rc = VERR_CV_BAD_FORMAT;
                    }
                    i++;
                }
            }
            else
            {
                uint32_t fSeen = RT_BIT_32(kCvSst_Module      - kCvSst_Module)
                               | RT_BIT_32(kCvSst_Libraries   - kCvSst_Module)
                               | RT_BIT_32(kCvSst_GlobalSym   - kCvSst_Module)
                               | RT_BIT_32(kCvSst_GlobalPub   - kCvSst_Module)
                               | RT_BIT_32(kCvSst_GlobalTypes - kCvSst_Module)
                               | RT_BIT_32(kCvSst_SegName     - kCvSst_Module)
                               | RT_BIT_32(kCvSst_SegMap      - kCvSst_Module)
                               | RT_BIT_32(kCvSst_StaticSym   - kCvSst_Module)
                               | RT_BIT_32(kCvSst_FileIndex   - kCvSst_Module)
                               | RT_BIT_32(kCvSst_MPC         - kCvSst_Module);
                uint16_t iMod = 0;
                uint16_t uSst = kCvSst_Module;
                while (i < cNormalMods)
                {
                    PCRTCVDIRENT32 pDirEnt = &pThis->paDirEnts[i];
                    if (pDirEnt->iMod > iMod)
                    {
                        if (pDirEnt->uSubSectType != uSst)
                        {
                            Log(("CV directory entry #%u: Expected %s (%#x), found %s (%#x).\n",
                                 i, rtDbgModCvGetSubSectionName(uSst), uSst,
                                 rtDbgModCvGetSubSectionName(pDirEnt->uSubSectType), pDirEnt->uSubSectType));
                            rc = VERR_CV_BAD_FORMAT;
                        }
                    }
                    else
                    {
                        uint32_t iBit = pDirEnt->uSubSectType - kCvSst_Module;
                        if (iBit >= 32U || (fSeen & RT_BIT_32(iBit)))
                        {
                            Log(("CV directory entry #%u: SST %s (%#x) has already been seen or is for globals.\n",
                                 i, rtDbgModCvGetSubSectionName(pDirEnt->uSubSectType), pDirEnt->uSubSectType));
                            rc = VERR_CV_BAD_FORMAT;
                        }
                        fSeen |= RT_BIT_32(iBit);
                    }

                    uSst = pDirEnt->uSubSectType;
                    iMod = pDirEnt->iMod;
                    i++;
                }
            }

            /* New style with special modules at the end. */
            if (pThis->enmDirOrder != RTCVDIRORDER_BY_MOD_0)
                while (i < cDirEnts)
                {
                    if (pThis->paDirEnts[i].iMod != 0 && pThis->paDirEnts[i].iMod != 0xffff)
                    {
                        Log(("CV directory entry #%u: Expected global module entry, not %#x.\n", i,
                             pThis->paDirEnts[i].iMod));
                        rc = VERR_CV_BAD_FORMAT;
                    }
                    i++;
                }
        }

    }

    return rc;
}


static int rtDbgModCvLoadCodeViewInfo(PRTDBGMODCV pThis)
{
    /*
     * Load the directory, the segment map (if any) and then scan for segments
     * if necessary.
     */
    int rc = rtDbgModCvLoadDirectory(pThis);
    if (RT_SUCCESS(rc))
        rc = rtDbgModCvLoadSegmentMap(pThis);
    if (RT_SUCCESS(rc) && !pThis->fHaveLoadedSegments)
    {
        rc = VERR_CV_TODO; /** @todo Scan anything containing address, in particular sstSegMap and sstModule,
                            * and reconstruct the segments from that information. */
        pThis->cbImage = 0x1000;
        rc = VINF_SUCCESS;
    }

    /*
     * Process the directory.
     */
    for (uint32_t i = 0; RT_SUCCESS(rc) && i < pThis->cDirEnts; i++)
    {
        PCRTCVDIRENT32              pDirEnt     = &pThis->paDirEnts[i];
        Log3(("Processing subsection %#u %s\n", i, rtDbgModCvGetSubSectionName(pDirEnt->uSubSectType)));
        PFNDBGMODCVSUBSECTCALLBACK  pfnCallback = NULL;
        switch (pDirEnt->uSubSectType)
        {
            case kCvSst_GlobalPub:
            case kCvSst_GlobalSym:
            case kCvSst_StaticSym:
                pfnCallback = rtDbgModCvSs_GlobalPub_GlobalSym_StaticSym;
                break;
            case kCvSst_Module:
                pfnCallback = rtDbgModCvSs_Module;
                break;
            case kCvSst_PublicSym:
            case kCvSst_Symbols:
            case kCvSst_AlignSym:
                pfnCallback = rtDbgModCvSs_Symbols_PublicSym_AlignSym;
                break;

            case kCvSst_OldModule:
            case kCvSst_OldPublic:
            case kCvSst_OldTypes:
            case kCvSst_OldSymbols:
            case kCvSst_OldSrcLines:
            case kCvSst_OldLibraries:
            case kCvSst_OldImports:
            case kCvSst_OldCompacted:
            case kCvSst_OldSrcLnSeg:
            case kCvSst_OldSrcLines3:

            case kCvSst_Types:
            case kCvSst_Public:
            case kCvSst_SrcLnSeg:
            case kCvSst_SrcModule:
            case kCvSst_Libraries:
            case kCvSst_GlobalTypes:
            case kCvSst_MPC:
            case kCvSst_PreComp:
            case kCvSst_PreCompMap:
            case kCvSst_OffsetMap16:
            case kCvSst_OffsetMap32:
            case kCvSst_FileIndex:

            default:
                /** @todo implement more. */
                break;

            /* Skip because we've already processed them: */
            case kCvSst_SegMap:
            case kCvSst_SegName:
                pfnCallback = NULL;
                break;
        }

        if (pfnCallback)
        {
            void *pvSubSect;
            rc = rtDbgModCvReadAtAlloc(pThis, pDirEnt->off, &pvSubSect, pDirEnt->cb);
            if (RT_SUCCESS(rc))
            {
                rc = pfnCallback(pThis, pvSubSect, pDirEnt->cb, pDirEnt);
                RTMemFree(pvSubSect);
            }
        }
    }

    return rc;
}


/*
 *
 * COFF Debug Info Parsing.
 * COFF Debug Info Parsing.
 * COFF Debug Info Parsing.
 *
 */

static const char *rtDbgModCvGetCoffStorageClassName(uint8_t bStorageClass)
{
    switch (bStorageClass)
    {
        case IMAGE_SYM_CLASS_END_OF_FUNCTION:   return "END_OF_FUNCTION";
        case IMAGE_SYM_CLASS_NULL:              return "NULL";
        case IMAGE_SYM_CLASS_AUTOMATIC:         return "AUTOMATIC";
        case IMAGE_SYM_CLASS_EXTERNAL:          return "EXTERNAL";
        case IMAGE_SYM_CLASS_STATIC:            return "STATIC";
        case IMAGE_SYM_CLASS_REGISTER:          return "REGISTER";
        case IMAGE_SYM_CLASS_EXTERNAL_DEF:      return "EXTERNAL_DEF";
        case IMAGE_SYM_CLASS_LABEL:             return "LABEL";
        case IMAGE_SYM_CLASS_UNDEFINED_LABEL:   return "UNDEFINED_LABEL";
        case IMAGE_SYM_CLASS_MEMBER_OF_STRUCT:  return "MEMBER_OF_STRUCT";
        case IMAGE_SYM_CLASS_ARGUMENT:          return "ARGUMENT";
        case IMAGE_SYM_CLASS_STRUCT_TAG:        return "STRUCT_TAG";
        case IMAGE_SYM_CLASS_MEMBER_OF_UNION:   return "MEMBER_OF_UNION";
        case IMAGE_SYM_CLASS_UNION_TAG:         return "UNION_TAG";
        case IMAGE_SYM_CLASS_TYPE_DEFINITION:   return "TYPE_DEFINITION";
        case IMAGE_SYM_CLASS_UNDEFINED_STATIC:  return "UNDEFINED_STATIC";
        case IMAGE_SYM_CLASS_ENUM_TAG:          return "ENUM_TAG";
        case IMAGE_SYM_CLASS_MEMBER_OF_ENUM:    return "MEMBER_OF_ENUM";
        case IMAGE_SYM_CLASS_REGISTER_PARAM:    return "REGISTER_PARAM";
        case IMAGE_SYM_CLASS_BIT_FIELD:         return "BIT_FIELD";
        case IMAGE_SYM_CLASS_FAR_EXTERNAL:      return "FAR_EXTERNAL";
        case IMAGE_SYM_CLASS_BLOCK:             return "BLOCK";
        case IMAGE_SYM_CLASS_FUNCTION:          return "FUNCTION";
        case IMAGE_SYM_CLASS_END_OF_STRUCT:     return "END_OF_STRUCT";
        case IMAGE_SYM_CLASS_FILE:              return "FILE";
        case IMAGE_SYM_CLASS_SECTION:           return "SECTION";
        case IMAGE_SYM_CLASS_WEAK_EXTERNAL:     return "WEAK_EXTERNAL";
        case IMAGE_SYM_CLASS_CLR_TOKEN:         return "CLR_TOKEN";
    }

    static char s_szName[32];
    RTStrPrintf(s_szName, sizeof(s_szName), "Unknown%#04x", bStorageClass);
    return s_szName;
}


/**
 * Adds a chunk of COFF line numbers.
 *
 * @param   pThis               The COFF/CodeView reader instance.
 * @param   pszFile             The source file name.
 * @param   iSection            The section number.
 * @param   paLines             Pointer to the first line number table entry.
 * @param   cLines              The number of line number table entries to add.
 */
static void rtDbgModCvAddCoffLineNumbers(PRTDBGMODCV pThis, const char *pszFile, uint32_t iSection,
                                         PCIMAGE_LINENUMBER paLines, uint32_t cLines)
{
    Log4(("Adding %u line numbers in section #%u  for %s\n", cLines, iSection, pszFile));
    PCIMAGE_LINENUMBER pCur = paLines;
    while (cLines-- > 0)
    {
        if (pCur->Linenumber)
        {
            int rc = RTDbgModLineAdd(pThis->hCnt, pszFile, pCur->Linenumber, RTDBGSEGIDX_RVA, pCur->Type.VirtualAddress, NULL);
            Log4(("    %#010x: %u  [%Rrc]\n", pCur->Type.VirtualAddress, pCur->Linenumber, rc));
        }
        pCur++;
    }
}


/**
 * Adds a COFF symbol.
 *
 * @returns IPRT status (ignored)
 * @param   pThis               The COFF/CodeView reader instance.
 * @param   idxSeg              IPRT RVA or ABS segment index indicator.
 * @param   uValue              The symbol value.
 * @param   pszName             The symbol name.
 */
static int rtDbgModCvAddCoffSymbol(PRTDBGMODCV pThis, uint32_t idxSeg, uint32_t uValue, const char *pszName)
{
    int rc = RTDbgModSymbolAdd(pThis->hCnt, pszName, idxSeg, uValue, 0, 0 /*fFlags*/, NULL);
    Log(("Symbol: %s:%08x %s [%Rrc]\n", idxSeg == RTDBGSEGIDX_RVA ? "rva" : "abs", uValue, pszName, rc));
    if (rc == VERR_DBG_ADDRESS_CONFLICT || rc == VERR_DBG_DUPLICATE_SYMBOL)
        rc = VINF_SUCCESS;
    return rc;
}


/**
 * Processes the COFF symbol table.
 *
 * @returns IPRT status code
 * @param   pThis               The COFF/CodeView reader instance.
 * @param   paSymbols           Pointer to the symbol table.
 * @param   cSymbols            The number of entries in the symbol table.
 * @param   paLines             Pointer to the line number table.
 * @param   cLines              The number of entires in the line number table.
 * @param   pszzStrTab          Pointer to the string table.
 * @param   cbStrTab            Size of the string table.
 */
static int rtDbgModCvProcessCoffSymbolTable(PRTDBGMODCV pThis,
                                            PCIMAGE_SYMBOL      paSymbols,  uint32_t cSymbols,
                                            PCIMAGE_LINENUMBER  paLines,    uint32_t cLines,
                                            const char         *pszzStrTab, uint32_t cbStrTab)
{
    Log3(("Processing COFF symbol table with %#x symbols\n", cSymbols));

    /*
     * Making some bold assumption that the line numbers for the section in
     * the file are allocated sequentially, we do multiple passes until we've
     * gathered them all.
     */
    int      rc        = VINF_SUCCESS;
    uint32_t cSections = 1;
    uint32_t iLineSect = 1;
    uint32_t iLine     = 0;
    do
    {
        /*
         * Process the symbols.
         */
        char        szShort[9];
        char        szFile[RTPATH_MAX];
        uint32_t    iSymbol  = 0;
        szFile[0] = '\0';
        szShort[8] = '\0'; /* avoid having to terminate it all the time. */

        while (iSymbol < cSymbols && RT_SUCCESS(rc))
        {
            /* Copy the symbol in and hope it works around the misalignment
               issues everywhere. */
            IMAGE_SYMBOL Sym;
            memcpy(&Sym, &paSymbols[iSymbol], sizeof(Sym));
            RTDBGMODCV_CHECK_NOMSG_RET_BF(Sym.NumberOfAuxSymbols < cSymbols);

            /* Calc a zero terminated symbol name. */
            const char  *pszName;
            if (Sym.N.Name.Short)
                pszName = (const char *)memcpy(szShort, &Sym.N, 8);
            else
            {
                RTDBGMODCV_CHECK_NOMSG_RET_BF(Sym.N.Name.Long < cbStrTab);
                pszName = pszzStrTab + Sym.N.Name.Long;
            }

            /* Only log stuff and count sections the in the first pass.*/
            if (iLineSect == 1)
            {
                Log3(("%04x: s=%#06x v=%#010x t=%#06x a=%#04x c=%#04x (%s) name='%s'\n",
                      iSymbol, Sym.SectionNumber, Sym.Value, Sym.Type, Sym.NumberOfAuxSymbols,
                      Sym.StorageClass, rtDbgModCvGetCoffStorageClassName(Sym.StorageClass), pszName));
                if ((int16_t)cSections <= Sym.SectionNumber && Sym.SectionNumber > 0)
                    cSections = Sym.SectionNumber + 1;
            }

            /*
             * Use storage class to pick what we need (which isn't much because,
             * MS only provides a very restricted set of symbols).
             */
            IMAGE_AUX_SYMBOL Aux;
            switch (Sym.StorageClass)
            {
                case IMAGE_SYM_CLASS_NULL:
                    /* a NOP */
                    break;

                case IMAGE_SYM_CLASS_FILE:
                {
                    /* Change the current file name (for line numbers). Pretend
                       ANSI and ISO-8859-1 are similar enough for out purposes... */
                    RTDBGMODCV_CHECK_NOMSG_RET_BF(Sym.NumberOfAuxSymbols > 0);
                    const char *pszFile = (const char *)&paSymbols[iSymbol + 1];
                    char *pszDst = szFile;
                    rc = RTLatin1ToUtf8Ex(pszFile, Sym.NumberOfAuxSymbols * sizeof(IMAGE_SYMBOL), &pszDst, sizeof(szFile), NULL);
                    if (RT_FAILURE(rc))
                        Log(("Error converting COFF filename: %Rrc\n", rc));
                    else if (iLineSect == 1)
                        Log3(("    filename='%s'\n", szFile));
                    break;
                }

                case IMAGE_SYM_CLASS_STATIC:
                    if (   Sym.NumberOfAuxSymbols == 1
                        && (   iLineSect == 1
                            || Sym.SectionNumber == (int32_t)iLineSect) )
                    {
                        memcpy(&Aux, &paSymbols[iSymbol + 1], sizeof(Aux));
                        if (iLineSect == 1)
                            Log3(("    section: cb=%#010x #relocs=%#06x #lines=%#06x csum=%#x num=%#x sel=%x rvd=%u\n",
                                  Aux.Section.Length, Aux.Section.NumberOfRelocations,
                                  Aux.Section.NumberOfLinenumbers,
                                  Aux.Section.CheckSum,
                                  RT_MAKE_U32(Aux.Section.Number, Aux.Section.HighNumber),
                                  Aux.Section.Selection,
                                  Aux.Section.bReserved));
                        if (   Sym.SectionNumber == (int32_t)iLineSect
                            && Aux.Section.NumberOfLinenumbers > 0)
                        {
                            uint32_t cLinesToAdd = RT_MIN(Aux.Section.NumberOfLinenumbers, cLines - iLine);
                            if (iLine < cLines && szFile[0])
                                rtDbgModCvAddCoffLineNumbers(pThis, szFile, iLineSect, &paLines[iLine], cLinesToAdd);
                            iLine += cLinesToAdd;
                        }
                    }
                    /* Not so sure about the quality here, but might be useful. */
                    else if (   iLineSect == 1
                             && Sym.NumberOfAuxSymbols == 0
                             && Sym.SectionNumber != IMAGE_SYM_UNDEFINED
                             && Sym.SectionNumber != IMAGE_SYM_ABSOLUTE
                             && Sym.SectionNumber != IMAGE_SYM_DEBUG
                             && Sym.Value > 0
                             && *pszName)
                        rtDbgModCvAddCoffSymbol(pThis, RTDBGSEGIDX_RVA, Sym.Value, pszName);
                    break;

                case IMAGE_SYM_CLASS_EXTERNAL:
                    /* Add functions (first pass only). */
                    if (   iLineSect == 1
                        && (ISFCN(Sym.Type) || Sym.Type == 0)
                        && Sym.NumberOfAuxSymbols == 0
                        && *pszName )
                    {
                        if (Sym.SectionNumber == IMAGE_SYM_ABSOLUTE)
                            rtDbgModCvAddCoffSymbol(pThis, RTDBGSEGIDX_ABS, Sym.Value, pszName);
                        else if (   Sym.SectionNumber != IMAGE_SYM_UNDEFINED
                                 && Sym.SectionNumber != IMAGE_SYM_DEBUG)
                            rtDbgModCvAddCoffSymbol(pThis, RTDBGSEGIDX_RVA, Sym.Value, pszName);
                    }
                    break;

                case IMAGE_SYM_CLASS_FUNCTION:
                    /* Not sure this is really used. */
                    break;

                case IMAGE_SYM_CLASS_END_OF_FUNCTION:
                case IMAGE_SYM_CLASS_AUTOMATIC:
                case IMAGE_SYM_CLASS_REGISTER:
                case IMAGE_SYM_CLASS_EXTERNAL_DEF:
                case IMAGE_SYM_CLASS_LABEL:
                case IMAGE_SYM_CLASS_UNDEFINED_LABEL:
                case IMAGE_SYM_CLASS_MEMBER_OF_STRUCT:
                case IMAGE_SYM_CLASS_ARGUMENT:
                case IMAGE_SYM_CLASS_STRUCT_TAG:
                case IMAGE_SYM_CLASS_MEMBER_OF_UNION:
                case IMAGE_SYM_CLASS_UNION_TAG:
                case IMAGE_SYM_CLASS_TYPE_DEFINITION:
                case IMAGE_SYM_CLASS_UNDEFINED_STATIC:
                case IMAGE_SYM_CLASS_ENUM_TAG:
                case IMAGE_SYM_CLASS_MEMBER_OF_ENUM:
                case IMAGE_SYM_CLASS_REGISTER_PARAM:
                case IMAGE_SYM_CLASS_BIT_FIELD:
                case IMAGE_SYM_CLASS_FAR_EXTERNAL:
                case IMAGE_SYM_CLASS_BLOCK:
                case IMAGE_SYM_CLASS_END_OF_STRUCT:
                case IMAGE_SYM_CLASS_SECTION:
                case IMAGE_SYM_CLASS_WEAK_EXTERNAL:
                case IMAGE_SYM_CLASS_CLR_TOKEN:
                    /* Not used by MS, I think. */
                    break;

                default:
                    Log(("RTDbgCv: Unexpected COFF storage class %#x (%u)\n", Sym.StorageClass, Sym.StorageClass));
                    break;
            }

            /* next symbol */
            iSymbol += 1 + Sym.NumberOfAuxSymbols;
        }

        /* Next section with line numbers. */
        iLineSect++;
    } while (iLine < cLines && iLineSect < cSections && RT_SUCCESS(rc));

    return rc;
}


/**
 * Loads COFF debug information into the container.
 *
 * @returns IPRT status code.
 * @param   pThis               The COFF/CodeView debug reader instance.
 */
static int rtDbgModCvLoadCoffInfo(PRTDBGMODCV pThis)
{
    /*
     * Read the whole section into memory.
     * Note! Cannot use rtDbgModCvReadAt or rtDbgModCvReadAtAlloc here.
     */
    int rc;
    uint8_t *pbDbgSect = (uint8_t *)RTMemAlloc(pThis->cbCoffDbgInfo);
    if (pbDbgSect)
    {
        if (pThis->hFile == NIL_RTFILE)
            rc = pThis->pMod->pImgVt->pfnReadAt(pThis->pMod, UINT32_MAX, pThis->offCoffDbgInfo, pbDbgSect, pThis->cbCoffDbgInfo);
        else
            rc = RTFileReadAt(pThis->hFile, pThis->offCoffDbgInfo, pbDbgSect, pThis->cbCoffDbgInfo, NULL);
        if (RT_SUCCESS(rc))
        {
            /* The string table follows after the symbol table. */
            const char *pszzStrTab = (const char *)(  pbDbgSect
                                                    + pThis->CoffHdr.LvaToFirstSymbol
                                                    + pThis->CoffHdr.NumberOfSymbols * sizeof(IMAGE_SYMBOL));
            uint32_t    cbStrTab = (uint32_t)((uintptr_t)(pbDbgSect + pThis->cbCoffDbgInfo) - (uintptr_t)pszzStrTab);
            /** @todo The symbol table starts with a size. Read it and checking. Also verify
             *        that the symtab ends with a terminator character. */

            rc = rtDbgModCvProcessCoffSymbolTable(pThis,
                                                  (PCIMAGE_SYMBOL)(pbDbgSect + pThis->CoffHdr.LvaToFirstSymbol),
                                                  pThis->CoffHdr.NumberOfSymbols,
                                                  (PCIMAGE_LINENUMBER)(pbDbgSect + pThis->CoffHdr.LvaToFirstLinenumber),
                                                  pThis->CoffHdr.NumberOfLinenumbers,
                                                  pszzStrTab, cbStrTab);
        }
        RTMemFree(pbDbgSect);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}






/*
 *
 * CodeView Debug module implementation.
 * CodeView Debug module implementation.
 * CodeView Debug module implementation.
 *
 */


/** @interface_method_impl{RTDBGMODVTDBG,pfnLineByAddr} */
static DECLCALLBACK(int) rtDbgModCv_LineByAddr(PRTDBGMODINT pMod, RTDBGSEGIDX iSeg, RTUINTPTR off,
                                                  PRTINTPTR poffDisp, PRTDBGLINE pLineInfo)
{
    PRTDBGMODCV pThis = (PRTDBGMODCV)pMod->pvDbgPriv;
    return RTDbgModLineByAddr(pThis->hCnt, iSeg, off, poffDisp, pLineInfo);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnLineByOrdinal} */
static DECLCALLBACK(int) rtDbgModCv_LineByOrdinal(PRTDBGMODINT pMod, uint32_t iOrdinal, PRTDBGLINE pLineInfo)
{
    PRTDBGMODCV pThis = (PRTDBGMODCV)pMod->pvDbgPriv;
    return RTDbgModLineByOrdinal(pThis->hCnt, iOrdinal, pLineInfo);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnLineCount} */
static DECLCALLBACK(uint32_t) rtDbgModCv_LineCount(PRTDBGMODINT pMod)
{
    PRTDBGMODCV pThis = (PRTDBGMODCV)pMod->pvDbgPriv;
    return RTDbgModLineCount(pThis->hCnt);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnLineAdd} */
static DECLCALLBACK(int) rtDbgModCv_LineAdd(PRTDBGMODINT pMod, const char *pszFile, size_t cchFile, uint32_t uLineNo,
                                               uint32_t iSeg, RTUINTPTR off, uint32_t *piOrdinal)
{
    PRTDBGMODCV pThis = (PRTDBGMODCV)pMod->pvDbgPriv;
    Assert(!pszFile[cchFile]); NOREF(cchFile);
    return RTDbgModLineAdd(pThis->hCnt, pszFile, uLineNo, iSeg, off, piOrdinal);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnSymbolByAddr} */
static DECLCALLBACK(int) rtDbgModCv_SymbolByAddr(PRTDBGMODINT pMod, RTDBGSEGIDX iSeg, RTUINTPTR off, uint32_t fFlags,
                                                    PRTINTPTR poffDisp, PRTDBGSYMBOL pSymInfo)
{
    PRTDBGMODCV pThis = (PRTDBGMODCV)pMod->pvDbgPriv;
    return RTDbgModSymbolByAddr(pThis->hCnt, iSeg, off, fFlags, poffDisp, pSymInfo);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnSymbolByName} */
static DECLCALLBACK(int) rtDbgModCv_SymbolByName(PRTDBGMODINT pMod, const char *pszSymbol, size_t cchSymbol,
                                                    PRTDBGSYMBOL pSymInfo)
{
    PRTDBGMODCV pThis = (PRTDBGMODCV)pMod->pvDbgPriv;
    Assert(!pszSymbol[cchSymbol]);
    return RTDbgModSymbolByName(pThis->hCnt, pszSymbol/*, cchSymbol*/, pSymInfo);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnSymbolByOrdinal} */
static DECLCALLBACK(int) rtDbgModCv_SymbolByOrdinal(PRTDBGMODINT pMod, uint32_t iOrdinal, PRTDBGSYMBOL pSymInfo)
{
    PRTDBGMODCV pThis = (PRTDBGMODCV)pMod->pvDbgPriv;
    return RTDbgModSymbolByOrdinal(pThis->hCnt, iOrdinal, pSymInfo);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnSymbolCount} */
static DECLCALLBACK(uint32_t) rtDbgModCv_SymbolCount(PRTDBGMODINT pMod)
{
    PRTDBGMODCV pThis = (PRTDBGMODCV)pMod->pvDbgPriv;
    return RTDbgModSymbolCount(pThis->hCnt);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnSymbolAdd} */
static DECLCALLBACK(int) rtDbgModCv_SymbolAdd(PRTDBGMODINT pMod, const char *pszSymbol, size_t cchSymbol,
                                                 RTDBGSEGIDX iSeg, RTUINTPTR off, RTUINTPTR cb, uint32_t fFlags,
                                                 uint32_t *piOrdinal)
{
    PRTDBGMODCV pThis = (PRTDBGMODCV)pMod->pvDbgPriv;
    Assert(!pszSymbol[cchSymbol]); NOREF(cchSymbol);
    return RTDbgModSymbolAdd(pThis->hCnt, pszSymbol, iSeg, off, cb, fFlags, piOrdinal);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnSegmentByIndex} */
static DECLCALLBACK(int) rtDbgModCv_SegmentByIndex(PRTDBGMODINT pMod, RTDBGSEGIDX iSeg, PRTDBGSEGMENT pSegInfo)
{
    PRTDBGMODCV pThis = (PRTDBGMODCV)pMod->pvDbgPriv;
    return RTDbgModSegmentByIndex(pThis->hCnt, iSeg, pSegInfo);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnSegmentCount} */
static DECLCALLBACK(RTDBGSEGIDX) rtDbgModCv_SegmentCount(PRTDBGMODINT pMod)
{
    PRTDBGMODCV pThis = (PRTDBGMODCV)pMod->pvDbgPriv;
    return RTDbgModSegmentCount(pThis->hCnt);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnSegmentAdd} */
static DECLCALLBACK(int) rtDbgModCv_SegmentAdd(PRTDBGMODINT pMod, RTUINTPTR uRva, RTUINTPTR cb, const char *pszName, size_t cchName,
                                                  uint32_t fFlags, PRTDBGSEGIDX piSeg)
{
    PRTDBGMODCV pThis = (PRTDBGMODCV)pMod->pvDbgPriv;
    Assert(!pszName[cchName]); NOREF(cchName);
    return RTDbgModSegmentAdd(pThis->hCnt, uRva, cb, pszName, fFlags, piSeg);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnImageSize} */
static DECLCALLBACK(RTUINTPTR) rtDbgModCv_ImageSize(PRTDBGMODINT pMod)
{
    PRTDBGMODCV pThis = (PRTDBGMODCV)pMod->pvDbgPriv;
    if (pThis->cbImage)
        return pThis->cbImage;
    return RTDbgModImageSize(pThis->hCnt);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnRvaToSegOff} */
static DECLCALLBACK(RTDBGSEGIDX) rtDbgModCv_RvaToSegOff(PRTDBGMODINT pMod, RTUINTPTR uRva, PRTUINTPTR poffSeg)
{
    PRTDBGMODCV pThis = (PRTDBGMODCV)pMod->pvDbgPriv;
    return RTDbgModRvaToSegOff(pThis->hCnt, uRva, poffSeg);
}


/** @interface_method_impl{RTDBGMODVTDBG,pfnClose} */
static DECLCALLBACK(int) rtDbgModCv_Close(PRTDBGMODINT pMod)
{
    PRTDBGMODCV pThis = (PRTDBGMODCV)pMod->pvDbgPriv;

    RTDbgModRelease(pThis->hCnt);
    if (pThis->hFile != NIL_RTFILE)
        RTFileClose(pThis->hFile);
    RTMemFree(pThis->paDirEnts);
    RTMemFree(pThis);

    pMod->pvDbgPriv = NULL; /* for internal use */
    return VINF_SUCCESS;
}


/*
 *
 * Probing code used by rtDbgModCv_TryOpen.
 * Probing code used by rtDbgModCv_TryOpen.
 *
 */



/** @callback_method_impl{FNRTLDRENUMSEGS,
 * Used to add segments from the image.}  */
static DECLCALLBACK(int) rtDbgModCvAddSegmentsCallback(RTLDRMOD hLdrMod, PCRTLDRSEG pSeg, void *pvUser)
{
    PRTDBGMODCV pThis = (PRTDBGMODCV)pvUser;
    Log(("Segment %s: LinkAddress=%#llx RVA=%#llx cb=%#llx\n",
         pSeg->pszName, (uint64_t)pSeg->LinkAddress, (uint64_t)pSeg->RVA, pSeg->cb));
    NOREF(hLdrMod);

    /* If the segment doesn't have a mapping, just add a dummy so the indexing
       works out correctly (same as for the image). */
    if (pSeg->RVA == NIL_RTLDRADDR)
        return RTDbgModSegmentAdd(pThis->hCnt, 0, 0, pSeg->pszName, 0 /*fFlags*/, NULL);

    RTLDRADDR cb = RT_MAX(pSeg->cb, pSeg->cbMapped);
    return RTDbgModSegmentAdd(pThis->hCnt, pSeg->RVA, cb, pSeg->pszName, 0 /*fFlags*/, NULL);
}


/**
 * Copies the sections over from the DBG file.
 *
 * Called if we don't have an associated executable image.
 *
 * @returns IPRT status code.
 * @param   pThis               The CV module instance.
 * @param   pDbgHdr             The DBG file header.
 * @param   pszFilename         The filename (for logging).
 */
static int rtDbgModCvAddSegmentsFromDbg(PRTDBGMODCV pThis, PCIMAGE_SEPARATE_DEBUG_HEADER pDbgHdr, const char *pszFilename)
{
    /*
     * Validate the header fields a little.
     */
    if (   pDbgHdr->NumberOfSections < 1
        || pDbgHdr->NumberOfSections > 4096)
    {
        Log(("RTDbgModCv: Bad NumberOfSections: %d\n", pDbgHdr->NumberOfSections));
        return VERR_CV_BAD_FORMAT;
    }
    if (!RT_IS_POWER_OF_TWO(pDbgHdr->SectionAlignment))
    {
        Log(("RTDbgModCv: Bad SectionAlignment: %#x\n", pDbgHdr->SectionAlignment));
        return VERR_CV_BAD_FORMAT;
    }

    /*
     * Read the section table.
     */
    size_t cbShs = pDbgHdr->NumberOfSections * sizeof(IMAGE_SECTION_HEADER);
    PIMAGE_SECTION_HEADER paShs = (PIMAGE_SECTION_HEADER)RTMemAlloc(cbShs);
    if (!paShs)
        return VERR_NO_MEMORY;
    int rc = RTFileReadAt(pThis->hFile, sizeof(*pDbgHdr), paShs, cbShs, NULL);
    if (RT_SUCCESS(rc))
    {
        /*
         * Do some basic validation.
         */
        uint32_t cbHeaders = 0;
        uint32_t uRvaPrev = 0;
        for (uint32_t i = 0; i < pDbgHdr->NumberOfSections; i++)
        {
            Log3(("RTDbgModCv: Section #%02u %#010x LB %#010x %.*s\n",
                  i, paShs[i].VirtualAddress, paShs[i].Misc.VirtualSize, sizeof(paShs[i].Name), paShs[i].Name));

            if (paShs[i].Characteristics & IMAGE_SCN_TYPE_NOLOAD)
                continue;

            if (paShs[i].VirtualAddress < uRvaPrev)
            {
                Log(("RTDbgModCv: %s: Overlap or soring error, VirtualAddress=%#x uRvaPrev=%#x - section #%d '%.*s'!!!\n",
                     pszFilename, paShs[i].VirtualAddress, uRvaPrev, i, sizeof(paShs[i].Name), paShs[i].Name));
                rc = VERR_CV_BAD_FORMAT;
            }
            else if (   paShs[i].VirtualAddress > pDbgHdr->SizeOfImage
                     || paShs[i].Misc.VirtualSize > pDbgHdr->SizeOfImage
                     || paShs[i].VirtualAddress + paShs[i].Misc.VirtualSize > pDbgHdr->SizeOfImage)
            {
                Log(("RTDbgModCv: %s: VirtualAddress=%#x VirtualSize=%#x (total %x) - beyond image size (%#x) - section #%d '%.*s'!!!\n",
                     pszFilename, paShs[i].VirtualAddress, paShs[i].Misc.VirtualSize,
                     paShs[i].VirtualAddress + paShs[i].Misc.VirtualSize,
                     pThis->cbImage, i, sizeof(paShs[i].Name), paShs[i].Name));
                rc = VERR_CV_BAD_FORMAT;
            }
            else if (paShs[i].VirtualAddress & (pDbgHdr->SectionAlignment - 1))
            {
                Log(("RTDbgModCv: %s: VirtualAddress=%#x misaligned (%#x) - section #%d '%.*s'!!!\n",
                     pszFilename, paShs[i].VirtualAddress, pDbgHdr->SectionAlignment, i, sizeof(paShs[i].Name), paShs[i].Name));
                rc = VERR_CV_BAD_FORMAT;
            }
            else
            {
                if (uRvaPrev == 0)
                    cbHeaders = paShs[i].VirtualAddress;
                uRvaPrev = paShs[i].VirtualAddress + paShs[i].Misc.VirtualSize;
                continue;
            }
        }
        if (RT_SUCCESS(rc) && uRvaPrev == 0)
        {
            Log(("RTDbgModCv: %s: No loadable sections.\n", pszFilename));
            rc = VERR_CV_BAD_FORMAT;
        }
        if (RT_SUCCESS(rc) && cbHeaders == 0)
        {
            Log(("RTDbgModCv: %s: No space for PE headers.\n", pszFilename));
            rc = VERR_CV_BAD_FORMAT;
        }
        if (RT_SUCCESS(rc))
        {
            /*
             * Add sections.
             */
            rc = RTDbgModSegmentAdd(pThis->hCnt, 0, cbHeaders, "NtHdrs", 0 /*fFlags*/, NULL);
            for (uint32_t i = 0; RT_SUCCESS(rc) && i < pDbgHdr->NumberOfSections; i++)
            {
                char szName[sizeof(paShs[i].Name) + 1];
                memcpy(szName, paShs[i].Name, sizeof(paShs[i].Name));
                szName[sizeof(szName) - 1] = '\0';

                if (paShs[i].Characteristics & IMAGE_SCN_TYPE_NOLOAD)
                    rc = RTDbgModSegmentAdd(pThis->hCnt, 0, 0, szName, 0 /*fFlags*/, NULL);
                else
                    rc = RTDbgModSegmentAdd(pThis->hCnt, paShs[i].VirtualAddress, paShs[i].Misc.VirtualSize, szName,
                                            0 /*fFlags*/, NULL);
            }
            if (RT_SUCCESS(rc))
                pThis->fHaveLoadedSegments = true;
        }
    }

    RTMemFree(paShs);
    return rc;
}


/**
 * Instantiates the CV/COFF reader.
 *
 * @returns IPRT status code
 * @param   pDbgMod             The debug module instance.
 * @param   enmFileType         The type of input file.
 * @param   hFile               The file handle, NIL_RTFILE of image.
 * @param   ppThis              Where to return the reader instance.
 */
static int rtDbgModCvCreateInstance(PRTDBGMODINT pDbgMod, RTCVFILETYPE enmFileType, RTFILE hFile, PRTDBGMODCV *ppThis)
{
    /*
     * Do we already have an instance?  Happens if we find multiple debug
     * formats we support.
     */
    PRTDBGMODCV pThis = (PRTDBGMODCV)pDbgMod->pvDbgPriv;
    if (pThis)
    {
        Assert(pThis->enmType == enmFileType);
        Assert(pThis->hFile == hFile);
        Assert(pThis->pMod == pDbgMod);
        *ppThis = pThis;
        return VINF_SUCCESS;
    }

    /*
     * Create a new instance.
     */
    pThis = (PRTDBGMODCV)RTMemAllocZ(sizeof(RTDBGMODCV));
    if (!pThis)
        return VERR_NO_MEMORY;
    int rc = RTDbgModCreate(&pThis->hCnt, pDbgMod->pszName, 0 /*cbSeg*/, 0 /*fFlags*/);
    if (RT_SUCCESS(rc))
    {
        pDbgMod->pvDbgPriv = pThis;
        pThis->enmType          = enmFileType;
        pThis->hFile            = hFile;
        pThis->pMod             = pDbgMod;
        pThis->offBase          = UINT32_MAX;
        pThis->offCoffDbgInfo   = UINT32_MAX;
        *ppThis = pThis;
        return VINF_SUCCESS;
    }
    RTMemFree(pThis);
    return rc;
}


/**
 * Common part of the COFF probing.
 *
 * @returns status code.
 * @param   pDbgMod             The debug module instance.  On success pvDbgPriv
 *                              will point to a valid RTDBGMODCV.
 * @param   hFile               The file with debug info in it.
 * @param   off                 The offset where to expect CV debug info.
 * @param   cb                  The number of bytes of debug info.
 * @param   enmArch             The desired image architecture.
 * @param   pszFilename         The path to the file (for logging).
 */
static int rtDbgModCvProbeCoff(PRTDBGMODINT pDbgMod, RTCVFILETYPE enmFileType, RTFILE hFile,
                               uint32_t off, uint32_t cb, const char *pszFilename)
{
    /*
     * Check that there is sufficient data for a header, then read it.
     */
    if (cb < sizeof(IMAGE_COFF_SYMBOLS_HEADER))
    {
        Log(("RTDbgModCv: Not enough room for COFF header.\n"));
        return VERR_BAD_EXE_FORMAT;
    }
    if (cb >= UINT32_C(128) * _1M)
    {
        Log(("RTDbgModCv: COFF debug information is to large (%'u bytes), max is 128MB\n", cb));
        return VERR_BAD_EXE_FORMAT;
    }

    int rc;
    IMAGE_COFF_SYMBOLS_HEADER Hdr;
    if (hFile == NIL_RTFILE)
        rc = pDbgMod->pImgVt->pfnReadAt(pDbgMod, UINT32_MAX, off, &Hdr, sizeof(Hdr));
    else
        rc = RTFileReadAt(hFile, off, &Hdr, sizeof(Hdr), NULL);
    if (RT_FAILURE(rc))
    {
        Log(("RTDbgModCv: Error reading COFF header: %Rrc\n", rc));
        return rc;
    }

    Log2(("RTDbgModCv: Found COFF debug info header at %#x (LB %#x) in %s\n", off, cb, pszFilename));
    Log2(("    NumberOfSymbols      = %#010x\n", Hdr.NumberOfSymbols));
    Log2(("    LvaToFirstSymbol     = %#010x\n", Hdr.LvaToFirstSymbol));
    Log2(("    NumberOfLinenumbers  = %#010x\n", Hdr.NumberOfLinenumbers));
    Log2(("    LvaToFirstLinenumber = %#010x\n", Hdr.LvaToFirstLinenumber));
    Log2(("    RvaToFirstByteOfCode = %#010x\n", Hdr.RvaToFirstByteOfCode));
    Log2(("    RvaToLastByteOfCode  = %#010x\n", Hdr.RvaToLastByteOfCode));
    Log2(("    RvaToFirstByteOfData = %#010x\n", Hdr.RvaToFirstByteOfData));
    Log2(("    RvaToLastByteOfData  = %#010x\n", Hdr.RvaToLastByteOfData));

    /*
     * Validate the COFF header.
     */
    if (   (uint64_t)Hdr.LvaToFirstSymbol + (uint64_t)Hdr.NumberOfSymbols * sizeof(IMAGE_SYMBOL) > cb
        || (Hdr.LvaToFirstSymbol < sizeof(Hdr) && Hdr.NumberOfSymbols > 0))
    {
        Log(("RTDbgModCv: Bad COFF symbol count or/and offset: LvaToFirstSymbol=%#x, NumberOfSymbols=%#x cbCoff=%#x\n",
             Hdr.LvaToFirstSymbol, Hdr.NumberOfSymbols, cb));
        return VERR_BAD_EXE_FORMAT;
    }
    if (   (uint64_t)Hdr.LvaToFirstLinenumber + (uint64_t)Hdr.NumberOfLinenumbers * sizeof(IMAGE_LINENUMBER) > cb
        || (Hdr.LvaToFirstLinenumber < sizeof(Hdr) && Hdr.NumberOfLinenumbers > 0))
    {
        Log(("RTDbgModCv: Bad COFF symbol count or/and offset: LvaToFirstSymbol=%#x, NumberOfSymbols=%#x cbCoff=%#x\n",
             Hdr.LvaToFirstSymbol, Hdr.NumberOfSymbols, cb));
        return VERR_BAD_EXE_FORMAT;
    }
    if (Hdr.NumberOfSymbols < 2)
    {
        Log(("RTDbgModCv: The COFF symbol table is too short to be of any worth... (%u syms)\n", Hdr.NumberOfSymbols));
        return VERR_NO_DATA;
    }

    /*
     * What we care about looks fine, use it.
     */
    PRTDBGMODCV pThis;
    rc = rtDbgModCvCreateInstance(pDbgMod, enmFileType, hFile, &pThis);
    if (RT_SUCCESS(rc))
    {
        pThis->offCoffDbgInfo = off;
        pThis->cbCoffDbgInfo  = cb;
        pThis->CoffHdr        = Hdr;
    }

    return rc;
}


/**
 * Common part of the CodeView probing.
 *
 * @returns status code.
 * @param   pDbgMod             The debug module instance.  On success pvDbgPriv
 *                              will point to a valid RTDBGMODCV.
 * @param   pCvHdr              The CodeView base header.
 * @param   enmFileType         The kind of file this is we're probing.
 * @param   hFile               The file with debug info in it.
 * @param   off                 The offset where to expect CV debug info.
 * @param   cb                  The number of bytes of debug info.
 * @param   pszFilename         The path to the file (for logging).
 */
static int rtDbgModCvProbeCommon(PRTDBGMODINT pDbgMod, PRTCVHDR pCvHdr, RTCVFILETYPE enmFileType, RTFILE hFile,
                                 uint32_t off, uint32_t cb, RTLDRARCH enmArch, const char *pszFilename)
{
    int rc = VERR_DBG_NO_MATCHING_INTERPRETER;

    /* Is a codeview format we (wish to) support? */
    if (   pCvHdr->u32Magic == RTCVHDR_MAGIC_NB11
        || pCvHdr->u32Magic == RTCVHDR_MAGIC_NB09
        || pCvHdr->u32Magic == RTCVHDR_MAGIC_NB08
        || pCvHdr->u32Magic == RTCVHDR_MAGIC_NB05
        || pCvHdr->u32Magic == RTCVHDR_MAGIC_NB04
        || pCvHdr->u32Magic == RTCVHDR_MAGIC_NB02
        || pCvHdr->u32Magic == RTCVHDR_MAGIC_NB00
       )
    {
        /* We're assuming it's a base header, so the offset must be within
           the area defined by the debug info we got from the loader. */
        if (pCvHdr->off < cb && pCvHdr->off >= sizeof(*pCvHdr))
        {
            Log(("RTDbgModCv: Found %c%c%c%c at %#RTfoff - size %#x, directory at %#x. file type %d\n",
                 RT_BYTE1(pCvHdr->u32Magic), RT_BYTE2(pCvHdr->u32Magic), RT_BYTE3(pCvHdr->u32Magic), RT_BYTE4(pCvHdr->u32Magic),
                 off, cb, pCvHdr->off, enmFileType));

            /*
             * Create a module instance, if not already done.
             */
            PRTDBGMODCV pThis;
            rc = rtDbgModCvCreateInstance(pDbgMod, enmFileType, hFile, &pThis);
            if (RT_SUCCESS(rc))
            {
                pThis->u32CvMagic = pCvHdr->u32Magic;
                pThis->offBase    = off;
                pThis->cbDbgInfo  = cb;
                pThis->offDir     = pCvHdr->off;
                return VINF_SUCCESS;
            }
        }
    }

    return rc;
}


/** @callback_method_impl{FNRTLDRENUMDBG} */
static DECLCALLBACK(int) rtDbgModCvEnumCallback(RTLDRMOD hLdrMod, PCRTLDRDBGINFO pDbgInfo, void *pvUser)
{
    PRTDBGMODINT pDbgMod = (PRTDBGMODINT)pvUser;
    Assert(!pDbgMod->pvDbgPriv);

    /* Skip external files, RTDbgMod will deal with those
       via RTDBGMODINT::pszDbgFile. */
    if (pDbgInfo->pszExtFile)
        return VINF_SUCCESS;

    /* We only handle the codeview sections. */
    if (pDbgInfo->enmType == RTLDRDBGINFOTYPE_CODEVIEW)
    {
        /* Read the specified header and check if we like it. */
        RTCVHDR CvHdr;
        int rc = pDbgMod->pImgVt->pfnReadAt(pDbgMod, pDbgInfo->iDbgInfo, pDbgInfo->offFile, &CvHdr, sizeof(CvHdr));
        if (RT_SUCCESS(rc))
            rc = rtDbgModCvProbeCommon(pDbgMod, &CvHdr, RTCVFILETYPE_IMAGE, NIL_RTFILE, pDbgInfo->offFile, pDbgInfo->cb,
                                       pDbgMod->pImgVt->pfnGetArch(pDbgMod), pDbgMod->pszImgFile);
    }
    else if (pDbgInfo->enmType == RTLDRDBGINFOTYPE_COFF)
    {
        /* Join paths with the DBG code. */
        rtDbgModCvProbeCoff(pDbgMod, RTCVFILETYPE_IMAGE, NIL_RTFILE, pDbgInfo->offFile, pDbgInfo->cb, pDbgMod->pszImgFile);
    }

    return VINF_SUCCESS;
}


/**
 * Part two of the external file probing.
 *
 * @returns status code.
 * @param   pDbgMod             The debug module instance.  On success pvDbgPriv
 *                              will point to a valid RTDBGMODCV.
 * @param   enmFileType         The kind of file this is we're probing.
 * @param   hFile               The file with debug info in it.
 * @param   off                 The offset where to expect CV debug info.
 * @param   cb                  The number of bytes of debug info.
 * @param   enmArch             The desired image architecture.
 * @param   pszFilename         The path to the file (for logging).
 */
static int rtDbgModCvProbeFile2(PRTDBGMODINT pThis, RTCVFILETYPE enmFileType, RTFILE hFile, uint32_t off, uint32_t cb,
                                RTLDRARCH enmArch, const char *pszFilename)
{
    RTCVHDR CvHdr;
    int rc = RTFileReadAt(hFile, off, &CvHdr, sizeof(CvHdr), NULL);
    if (RT_SUCCESS(rc))
        rc = rtDbgModCvProbeCommon(pThis, &CvHdr, enmFileType, hFile, off, cb, enmArch, pszFilename);
    return rc;
}


/**
 * Probes an external file for CodeView information.
 *
 * @returns status code.
 * @param   pDbgMod             The debug module instance.  On success pvDbgPriv
 *                              will point to a valid RTDBGMODCV.
 * @param   pszFilename         The path to the file to probe.
 * @param   enmArch             The desired image architecture.
 */
static int rtDbgModCvProbeFile(PRTDBGMODINT pDbgMod, const char *pszFilename, RTLDRARCH enmArch)
{
    RTFILE hFile;
    int rc = RTFileOpen(&hFile, pszFilename, RTFILE_O_READ | RTFILE_O_DENY_WRITE | RTFILE_O_OPEN);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Check for .DBG file
     */
    IMAGE_SEPARATE_DEBUG_HEADER DbgHdr;
    rc = RTFileReadAt(hFile, 0, &DbgHdr, sizeof(DbgHdr), NULL);
    if (   RT_SUCCESS(rc)
        && DbgHdr.Signature == IMAGE_SEPARATE_DEBUG_SIGNATURE)
    {
        Log2(("RTDbgModCv: Found separate debug header in %s:\n", pszFilename));
        Log2(("    Flags              = %#x\n", DbgHdr.Flags));
        Log2(("    Machine            = %#x\n", DbgHdr.Machine));
        Log2(("    Characteristics    = %#x\n", DbgHdr.Characteristics));
        Log2(("    TimeDateStamp      = %#x\n", DbgHdr.TimeDateStamp));
        Log2(("    CheckSum           = %#x\n", DbgHdr.CheckSum));
        Log2(("    ImageBase          = %#x\n", DbgHdr.ImageBase));
        Log2(("    SizeOfImage        = %#x\n", DbgHdr.SizeOfImage));
        Log2(("    NumberOfSections   = %#x\n", DbgHdr.NumberOfSections));
        Log2(("    ExportedNamesSize  = %#x\n", DbgHdr.ExportedNamesSize));
        Log2(("    DebugDirectorySize = %#x\n", DbgHdr.DebugDirectorySize));
        Log2(("    SectionAlignment   = %#x\n", DbgHdr.SectionAlignment));

        /*
         * Match up the architecture if specified.
         */
        switch (enmArch)
        {
            case RTLDRARCH_X86_32:
                if (DbgHdr.Machine != IMAGE_FILE_MACHINE_I386)
                    rc = VERR_LDR_ARCH_MISMATCH;
                break;
            case RTLDRARCH_AMD64:
                if (DbgHdr.Machine != IMAGE_FILE_MACHINE_AMD64)
                    rc = VERR_LDR_ARCH_MISMATCH;
                break;

            default:
            case RTLDRARCH_HOST:
                AssertFailed();
            case RTLDRARCH_WHATEVER:
                break;
        }
        if (RT_FAILURE(rc))
        {
            RTFileClose(hFile);
            return rc;
        }

        /*
         * Probe for readable debug info in the debug directory.
         */
        uint32_t offDbgDir = sizeof(DbgHdr)
                           + DbgHdr.NumberOfSections * sizeof(IMAGE_SECTION_HEADER)
                           + DbgHdr.ExportedNamesSize;

        uint32_t cEntries = DbgHdr.DebugDirectorySize / sizeof(IMAGE_DEBUG_DIRECTORY);
        for (uint32_t i = 0; i < cEntries; i++, offDbgDir += sizeof(IMAGE_DEBUG_DIRECTORY))
        {
            IMAGE_DEBUG_DIRECTORY DbgDir;
            rc = RTFileReadAt(hFile, offDbgDir, &DbgDir, sizeof(DbgDir), NULL);
            if (RT_FAILURE(rc))
                break;
            if (DbgDir.Type == IMAGE_DEBUG_TYPE_CODEVIEW)
                rc = rtDbgModCvProbeFile2(pDbgMod, RTCVFILETYPE_DBG, hFile,
                                          DbgDir.PointerToRawData, DbgDir.SizeOfData,
                                          enmArch, pszFilename);
            else if (DbgDir.Type == IMAGE_DEBUG_TYPE_COFF)
                rc = rtDbgModCvProbeCoff(pDbgMod, RTCVFILETYPE_DBG, hFile,
                                         DbgDir.PointerToRawData, DbgDir.SizeOfData, pszFilename);
        }

        /*
         * If we get down here with an instance, it prooves that we've found
         * something, regardless of any errors.  Add the sections and such.
         */
        PRTDBGMODCV pThis = (PRTDBGMODCV)pDbgMod->pvDbgPriv;
        if (pThis)
        {
            pThis->cbImage = DbgHdr.SizeOfImage;
            if (pDbgMod->pImgVt)
                rc = VINF_SUCCESS;
            else
            {
                rc = rtDbgModCvAddSegmentsFromDbg(pThis, &DbgHdr, pszFilename);
                if (RT_FAILURE(rc))
                    rtDbgModCv_Close(pDbgMod);
            }
            return rc;
        }

        /* Failed to find CV or smth, look at the end of the file just to be sure... */
    }

    /*
     * Look for CV tail header.
     */
    uint64_t cbFile;
    rc = RTFileSeek(hFile, -(RTFOFF)sizeof(RTCVHDR), RTFILE_SEEK_END, &cbFile);
    if (RT_SUCCESS(rc))
    {
        cbFile += sizeof(RTCVHDR);
        RTCVHDR CvHdr;
        rc = RTFileRead(hFile, &CvHdr, sizeof(CvHdr), NULL);
        if (RT_SUCCESS(rc))
            rc = rtDbgModCvProbeFile2(pDbgMod, RTCVFILETYPE_OTHER_AT_END, hFile,
                                      cbFile - CvHdr.off, CvHdr.off, enmArch, pszFilename);
    }

    if (RT_FAILURE(rc))
        RTFileClose(hFile);
    return rc;
}



/** @interface_method_impl{RTDBGMODVTDBG,pfnTryOpen} */
static DECLCALLBACK(int) rtDbgModCv_TryOpen(PRTDBGMODINT pMod, RTLDRARCH enmArch)
{
    /*
     * Look for debug info.
     */
    int rc = VERR_DBG_NO_MATCHING_INTERPRETER;
    if (pMod->pszDbgFile)
        rc = rtDbgModCvProbeFile(pMod, pMod->pszDbgFile, enmArch);

    if (!pMod->pvDbgPriv && pMod->pImgVt)
    {
        int rc2 = pMod->pImgVt->pfnEnumDbgInfo(pMod, rtDbgModCvEnumCallback, pMod);
        if (RT_FAILURE(rc2))
            rc = rc2;

        if (!pMod->pvDbgPriv)
        {
            /* Try the executable in case it has a NBxx tail header. */
            rc2 = rtDbgModCvProbeFile(pMod, pMod->pszImgFile, enmArch);
            if (RT_FAILURE(rc2) && (RT_SUCCESS(rc) || rc == VERR_DBG_NO_MATCHING_INTERPRETER))
                rc = rc2;
        }
    }

    PRTDBGMODCV pThis = (PRTDBGMODCV)pMod->pvDbgPriv;
    if (!pThis)
        return RT_SUCCESS_NP(rc) ? VERR_DBG_NO_MATCHING_INTERPRETER : rc;
    Assert(pThis->offBase  != UINT32_MAX || pThis->offCoffDbgInfo != UINT32_MAX);

    /*
     * Load the debug info.
     */
    if (pMod->pImgVt)
    {
        rc = pMod->pImgVt->pfnEnumSegments(pMod, rtDbgModCvAddSegmentsCallback, pThis);
        pThis->fHaveLoadedSegments = true;
    }
    if (RT_SUCCESS(rc) && pThis->offBase != UINT32_MAX)
        rc = rtDbgModCvLoadCodeViewInfo(pThis);
    if (RT_SUCCESS(rc) && pThis->offCoffDbgInfo != UINT32_MAX)
        rc = rtDbgModCvLoadCoffInfo(pThis);
    if (RT_SUCCESS(rc))
    {
        Log(("RTDbgCv: Successfully loaded debug info\n"));
        return VINF_SUCCESS;
    }

    Log(("RTDbgCv: Debug info load error %Rrc\n", rc));
    rtDbgModCv_Close(pMod);
    return rc;
}





/** Virtual function table for the CodeView debug info reader. */
DECL_HIDDEN_CONST(RTDBGMODVTDBG) const g_rtDbgModVtDbgCodeView =
{
    /*.u32Magic = */            RTDBGMODVTDBG_MAGIC,
    /*.fSupports = */           RT_DBGTYPE_CODEVIEW,
    /*.pszName = */             "codeview",
    /*.pfnTryOpen = */          rtDbgModCv_TryOpen,
    /*.pfnClose = */            rtDbgModCv_Close,

    /*.pfnRvaToSegOff = */      rtDbgModCv_RvaToSegOff,
    /*.pfnImageSize = */        rtDbgModCv_ImageSize,

    /*.pfnSegmentAdd = */       rtDbgModCv_SegmentAdd,
    /*.pfnSegmentCount = */     rtDbgModCv_SegmentCount,
    /*.pfnSegmentByIndex = */   rtDbgModCv_SegmentByIndex,

    /*.pfnSymbolAdd = */        rtDbgModCv_SymbolAdd,
    /*.pfnSymbolCount = */      rtDbgModCv_SymbolCount,
    /*.pfnSymbolByOrdinal = */  rtDbgModCv_SymbolByOrdinal,
    /*.pfnSymbolByName = */     rtDbgModCv_SymbolByName,
    /*.pfnSymbolByAddr = */     rtDbgModCv_SymbolByAddr,

    /*.pfnLineAdd = */          rtDbgModCv_LineAdd,
    /*.pfnLineCount = */        rtDbgModCv_LineCount,
    /*.pfnLineByOrdinal = */    rtDbgModCv_LineByOrdinal,
    /*.pfnLineByAddr = */       rtDbgModCv_LineByAddr,

    /*.u32EndMagic = */         RTDBGMODVTDBG_MAGIC
};

