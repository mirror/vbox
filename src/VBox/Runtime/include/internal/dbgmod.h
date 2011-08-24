/* $Id$ */
/** @file
 * IPRT - Internal Header for RTDbgMod and the associated interpreters.
 */

/*
 * Copyright (C) 2008-2011 Oracle Corporation
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

#ifndef ___internal_dbgmod_h
#define ___internal_dbgmod_h

#include <iprt/types.h>
#include <iprt/critsect.h>
#include <iprt/ldr.h> /* for PFNRTLDRENUMDBG */
#include "internal/magics.h"

RT_C_DECLS_BEGIN

/** @addtogroup grp_rt_dbgmod
 * @internal
 * @{
 */


/** Pointer to the internal module structure. */
typedef struct RTDBGMODINT *PRTDBGMODINT;

/**
 * Virtual method table for executable image interpreters.
 */
typedef struct RTDBGMODVTIMG
{
    /** Magic number (RTDBGMODVTIMG_MAGIC). */
    uint32_t    u32Magic;
    /** Reserved. */
    uint32_t    fReserved;
    /** The name of the interpreter. */
    const char *pszName;

    /**
     * Try open the image.
     *
     * This combines probing and opening.
     *
     * @returns IPRT status code. No informational returns defined.
     *
     * @param   pMod        Pointer to the module that is being opened.
     *
     *                      The RTDBGMOD::pszDbgFile member will point to
     *                      the filename of any debug info we're aware of
     *                      on input. Also, or alternatively, it is expected
     *                      that the interpreter will look for debug info in
     *                      the executable image file when present and that it
     *                      may ask the image interpreter for this when it's
     *                      around.
     *
     *                      Upon successful return the method is expected to
     *                      initialize pImgOps and pvImgPriv.
     */
    DECLCALLBACKMEMBER(int, pfnTryOpen)(PRTDBGMODINT pMod);

    /**
     * Close the interpreter, freeing all associated resources.
     *
     * The caller sets the pDbgOps and pvDbgPriv RTDBGMOD members
     * to NULL upon return.
     *
     * @param   pMod        Pointer to the module structure.
     */
    DECLCALLBACKMEMBER(int, pfnClose)(PRTDBGMODINT pMod);

    /**
     * Enumerate the debug info contained in the executable image.
     *
     * Identical to RTLdrEnumDbgInfo.
     *
     * @returns IPRT status code or whatever pfnCallback returns.
     *
     * @param   hLdrMod         The module handle.
     * @param   pvBits          Optional pointer to bits returned by
     *                          RTLdrGetBits().  This can be used by some module
     *                          interpreters to reduce memory consumption.
     * @param   pfnCallback     The callback function.  Ignore the module
     *                          handle argument!
     * @param   pvUser          The user argument.
     */
    DECLCALLBACKMEMBER(int, pfnEnumDbgInfo)(PRTDBGMODINT pMod, PFNRTLDRENUMDBG pfnCallback, void *pvUser);

    /** For catching initialization errors (RTDBGMODVTIMG_MAGIC). */
    uint32_t    u32EndMagic;
} RTDBGMODVTIMG;
/** Pointer to a const RTDBGMODVTIMG. */
typedef RTDBGMODVTIMG const *PCRTDBGMODVTIMG;


/**
 * Virtual method table for debug info interpreters.
 */
typedef struct RTDBGMODVTDBG
{
    /** Magic number (RTDBGMODVTDBG_MAGIC). */
    uint32_t    u32Magic;
    /** Mask of supported debug info types, see grp_rt_dbg_type.
     * Used to speed up the search for a suitable interpreter. */
    uint32_t    fSupports;
    /** The name of the interpreter. */
    const char *pszName;

    /**
     * Try open the image.
     *
     * This combines probing and opening.
     *
     * @returns IPRT status code. No informational returns defined.
     *
     * @param   pMod        Pointer to the module that is being opened.
     *
     *                      The RTDBGMOD::pszDbgFile member will point to
     *                      the filename of any debug info we're aware of
     *                      on input. Also, or alternatively, it is expected
     *                      that the interpreter will look for debug info in
     *                      the executable image file when present and that it
     *                      may ask the image interpreter for this when it's
     *                      around.
     *
     *                      Upon successful return the method is expected to
     *                      initialize pDbgOps and pvDbgPriv.
     */
    DECLCALLBACKMEMBER(int, pfnTryOpen)(PRTDBGMODINT pMod);

    /**
     * Close the interpreter, freeing all associated resources.
     *
     * The caller sets the pDbgOps and pvDbgPriv RTDBGMOD members
     * to NULL upon return.
     *
     * @param   pMod        Pointer to the module structure.
     */
    DECLCALLBACKMEMBER(int, pfnClose)(PRTDBGMODINT pMod);



    /**
     * Converts an image relative virtual address address to a segmented address.
     *
     * @returns Segment index on success, NIL_RTDBGSEGIDX on failure.
     * @param   pMod        Pointer to the module structure.
     * @param   uRva        The image relative address to convert.
     * @param   poffSeg     Where to return the segment offset. Optional.
     */
    DECLCALLBACKMEMBER(RTDBGSEGIDX, pfnRvaToSegOff)(PRTDBGMODINT pMod, RTUINTPTR uRva, PRTUINTPTR poffSeg);

    /**
     * Image size when mapped if segments are mapped adjacently.
     *
     * For ELF, PE, and Mach-O images this is (usually) a natural query, for LX and
     * NE and such it's a bit odder and the answer may not make much sense for them.
     *
     * @returns Image mapped size.
     * @param   pMod        Pointer to the module structure.
     */
    DECLCALLBACKMEMBER(RTUINTPTR, pfnImageSize)(PRTDBGMODINT pMod);



    /**
     * Adds a segment to the module (optional).
     *
     * @returns IPRT status code.
     * @retval  VERR_NOT_SUPPORTED if the interpreter doesn't support this feature.
     * @retval  VERR_DBG_SEGMENT_INDEX_CONFLICT if the segment index exists already.
     *
     * @param   pMod        Pointer to the module structure.
     * @param   uRva        The segment image relative address.
     * @param   cb          The segment size.
     * @param   pszName     The segment name.
     * @param   cchName     The length of the segment name.
     * @param   fFlags      Segment flags.
     * @param   piSeg       The segment index or NIL_RTDBGSEGIDX on input.
     *                      The assigned segment index on successful return.
     *                      Optional.
     */
    DECLCALLBACKMEMBER(int, pfnSegmentAdd)(PRTDBGMODINT pMod, RTUINTPTR uRva, RTUINTPTR cb, const char *pszName, size_t cchName,
                                           uint32_t fFlags, PRTDBGSEGIDX piSeg);

    /**
     * Gets the segment count.
     *
     * @returns Number of segments.
     * @retval  NIL_RTDBGSEGIDX if unknown.
     *
     * @param   pMod        Pointer to the module structure.
     */
    DECLCALLBACKMEMBER(RTDBGSEGIDX, pfnSegmentCount)(PRTDBGMODINT pMod);

    /**
     * Gets information about a segment.
     *
     * @returns IPRT status code.
     * @retval  VERR_DBG_INVALID_SEGMENT_INDEX if iSeg is too high.
     *
     * @param   pMod        Pointer to the module structure.
     * @param   iSeg        The segment.
     * @param   pSegInfo    Where to store the segment information.
     */
    DECLCALLBACKMEMBER(int, pfnSegmentByIndex)(PRTDBGMODINT pMod, RTDBGSEGIDX iSeg, PRTDBGSEGMENT pSegInfo);



    /**
     * Adds a symbol to the module (optional).
     *
     * @returns IPRT  code.
     * @retval  VERR_NOT_SUPPORTED if the interpreter doesn't support this feature.
     *
     * @param   pMod        Pointer to the module structure.
     * @param   pszSymbol   The symbol name.
     * @param   cchSymbol   The length for the symbol name.
     * @param   iSeg        The segment number (0-based). RTDBGMOD_SEG_RVA can be used.
     * @param   off         The offset into the segment.
     * @param   cb          The area covered by the symbol. 0 is fine.
     * @param   fFlags      Flags.
     * @param   piOrdinal   Where to return the symbol ordinal on success. If the
     *                      interpreter doesn't do ordinals, this will be set to
     *                      UINT32_MAX. Optional
     */
    DECLCALLBACKMEMBER(int, pfnSymbolAdd)(PRTDBGMODINT pMod, const char *pszSymbol, size_t cchSymbol,
                                          uint32_t iSeg, RTUINTPTR off, RTUINTPTR cb, uint32_t fFlags,
                                          uint32_t *piOrdinal);

    /**
     * Gets the number of symbols in the module.
     *
     * This is used for figuring out the max value to pass to pfnSymbolByIndex among
     * other things.
     *
     * @returns The number of symbols, UINT32_MAX if not known/supported.
     *
     * @param   pMod        Pointer to the module structure.
     */
    DECLCALLBACKMEMBER(uint32_t, pfnSymbolCount)(PRTDBGMODINT pMod);

    /**
     * Queries symbol information by ordinal number.
     *
     * @returns IPRT status code.
     * @retval  VINF_SUCCESS on success, no informational status code.
     * @retval  VERR_DBG_NO_SYMBOLS if there aren't any symbols.
     * @retval  VERR_NOT_SUPPORTED if lookup by ordinal is not supported.
     * @retval  VERR_SYMBOL_NOT_FOUND if there is no symbol at that index.
     *
     * @param   pMod        Pointer to the module structure.
     * @param   iOrdinal    The symbol ordinal number.
     * @param   pSymInfo    Where to store the symbol information.
     */
    DECLCALLBACKMEMBER(int, pfnSymbolByOrdinal)(PRTDBGMODINT pMod, uint32_t iOrdinal, PRTDBGSYMBOL pSymInfo);

    /**
     * Queries symbol information by symbol name.
     *
     * @returns IPRT status code.
     * @retval  VINF_SUCCESS on success, no informational status code.
     * @retval  VERR_DBG_NO_SYMBOLS if there aren't any symbols.
     * @retval  VERR_SYMBOL_NOT_FOUND if no suitable symbol was found.
     *
     * @param   pMod        Pointer to the module structure.
     * @param   pszSymbol   The symbol name.
     * @param   cchSymbol   The length of the symbol name.
     * @param   pSymInfo    Where to store the symbol information.
     */
    DECLCALLBACKMEMBER(int, pfnSymbolByName)(PRTDBGMODINT pMod, const char *pszSymbol, size_t cchSymbol, PRTDBGSYMBOL pSymInfo);

    /**
     * Queries symbol information by address.
     *
     * The returned symbol is what the debug info interpreter considers the symbol
     * most applicable to the specified address. This usually means a symbol with an
     * address equal or lower than the requested.
     *
     * @returns IPRT status code.
     * @retval  VINF_SUCCESS on success, no informational status code.
     * @retval  VERR_DBG_NO_SYMBOLS if there aren't any symbols.
     * @retval  VERR_SYMBOL_NOT_FOUND if no suitable symbol was found.
     *
     * @param   pMod        Pointer to the module structure.
     * @param   iSeg        The segment number (0-based) or RTDBGSEGIDX_ABS.
     * @param   off         The offset into the segment.
     * @param   poffDisp    Where to store the distance between the specified address
     *                      and the returned symbol. Optional.
     * @param   pSymInfo    Where to store the symbol information.
     */
    DECLCALLBACKMEMBER(int, pfnSymbolByAddr)(PRTDBGMODINT pMod, uint32_t iSeg, RTUINTPTR off, PRTINTPTR poffDisp, PRTDBGSYMBOL pSymInfo);



    /**
     * Adds a line number to the module (optional).
     *
     * @returns IPRT status code.
     * @retval  VERR_NOT_SUPPORTED if the interpreter doesn't support this feature.
     *
     * @param   pMod        Pointer to the module structure.
     * @param   pszFile     The filename.
     * @param   cchFile     The length of the filename.
     * @param   uLineNo     The line number.
     * @param   iSeg        The segment number (0-based).
     * @param   off         The offset into the segment.
     * @param   piOrdinal   Where to return the line number ordinal on success. If
     *                      the interpreter doesn't do ordinals, this will be set to
     *                      UINT32_MAX. Optional
     */
    DECLCALLBACKMEMBER(int, pfnLineAdd)(PRTDBGMODINT pMod, const char *pszFile, size_t cchFile, uint32_t uLineNo,
                                        uint32_t iSeg, RTUINTPTR off, uint32_t *piOrdinal);

    /**
     * Gets the number of line numbers in the module.
     *
     * @returns The number or UINT32_MAX if not known/supported.
     *
     * @param   pMod        Pointer to the module structure.
     */
    DECLCALLBACKMEMBER(uint32_t, pfnLineCount)(PRTDBGMODINT pMod);

    /**
     * Queries line number information by ordinal number.
     *
     * @returns IPRT status code.
     * @retval  VINF_SUCCESS on success, no informational status code.
     * @retval  VERR_DBG_NO_LINE_NUMBERS if there aren't any line numbers.
     * @retval  VERR_DBG_LINE_NOT_FOUND if there is no line number with that
     *          ordinal.
     *
     * @param   pMod        Pointer to the module structure.
     * @param   iOrdinal    The line number ordinal number.
     * @param   pLineInfo   Where to store the information about the line number.
     */
    DECLCALLBACKMEMBER(int, pfnLineByOrdinal)(PRTDBGMODINT pMod, uint32_t iOrdinal, PRTDBGLINE pLineInfo);

    /**
     * Queries line number information by address.
     *
     * @returns IPRT status code.
     * @retval  VINF_SUCCESS on success, no informational status code.
     * @retval  VERR_DBG_NO_LINE_NUMBERS if there aren't any line numbers.
     * @retval  VERR_DBG_LINE_NOT_FOUND if no suitable line number was found.
     *
     * @param   pMod        Pointer to the module structure.
     * @param   iSeg        The segment number (0-based) or RTDBGSEGIDX_ABS.
     * @param   off         The offset into the segment.
     * @param   poffDisp    Where to store the distance between the specified address
     *                      and the returned line number. Optional.
     * @param   pLineInfo   Where to store the information about the closest line
     *                      number.
     */
    DECLCALLBACKMEMBER(int, pfnLineByAddr)(PRTDBGMODINT pMod, uint32_t iSeg, RTUINTPTR off, PRTINTPTR poffDisp, PRTDBGLINE pLineInfo);


    /** For catching initialization errors (RTDBGMODVTDBG_MAGIC). */
    uint32_t    u32EndMagic;
} RTDBGMODVTDBG;
/** Pointer to a const RTDBGMODVTDBG. */
typedef RTDBGMODVTDBG const *PCRTDBGMODVTDBG;


/**
 * Debug module structure.
 */
typedef struct RTDBGMODINT
{
    /** Magic value (RTDBGMOD_MAGIC). */
    uint32_t            u32Magic;
    /** The number of reference there are to this module.
     * This is used to perform automatic cleanup and sharing. */
    uint32_t volatile   cRefs;
    /** The module tag. */
    uint64_t            uTag;
    /** The module name (short). */
    char const         *pszName;
    /** The module filename. Can be NULL. */
    char const         *pszImgFile;
    /** The debug info file (if external). Can be NULL. */
    char const         *pszDbgFile;

    /** Critical section serializing access to the module. */
    RTCRITSECT          CritSect;

    /** The method table for the executable image interpreter. */
    PCRTDBGMODVTIMG     pImgVt;
    /** Pointer to the private data of the executable image interpreter. */
    void               *pvImgPriv;

    /** The method table for the debug info interpreter. */
    PCRTDBGMODVTDBG     pDbgVt;
    /** Pointer to the private data of the debug info interpreter. */
    void               *pvDbgPriv;

} RTDBGMODINT;
/** Pointer to an debug module structure.  */
typedef RTDBGMODINT *PRTDBGMODINT;


extern DECLHIDDEN(RTSTRCACHE)           g_hDbgModStrCache;
extern DECLHIDDEN(RTDBGMODVTDBG const)  g_rtDbgModVtDbgDwarf;
extern DECLHIDDEN(RTDBGMODVTDBG const)  g_rtDbgModVtDbgNm;
extern DECLHIDDEN(RTDBGMODVTIMG const)  g_rtDbgModVtImgLdr;

int rtDbgModContainerCreate(PRTDBGMODINT pMod, RTUINTPTR cbSeg);

/** @} */

RT_C_DECLS_END

#endif

