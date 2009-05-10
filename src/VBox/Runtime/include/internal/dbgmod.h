/* $Id$ */
/** @file
 * IPRT - Internal Header for RTDbgMod and the associated interpreters.
 */

/*
 * Copyright (C) 2008-2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___internal_dbgmod_h
#define ___internal_dbgmod_h

#include <iprt/types.h>
#include "internal/magics.h"

__BEGIN_DECLS

/** @defgroup grp_rt_dbgmod     RTDbgMod - Debug Module Interperter
 * @ingroup grp_rt
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
    /** Mask of supported executable image types, see grp_rt_exe_img_type.
     * Used to speed up the search for a suitable interpreter. */
    uint32_t    fSupports;
    /** The name of the interpreter. */
    const char *pszName;

    /**
     * Try open the image.
     *
     * This combines probing and opening.
     *
     * @returns VBox status code. No informational returns defined.
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
     * @returns VBox status code. No informational returns defined.
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
     * Queries symbol information by symbol name.
     *
     * @returns VBox status code.
     * @retval  VINF_SUCCESS on success, no informational status code.
     * @retval  VERR_RTDBGMOD_NO_SYMBOLS if there aren't any symbols.
     * @retval  VERR_SYMBOL_NOT_FOUND if no suitable symbol was found.
     *
     * @param   pMod        Pointer to the module structure.
     * @param   pszSymbol   The symbol name.
     * @para    pSymbol     Where to store the symbol information.
     */
    DECLCALLBACKMEMBER(int, pfnSymbolByName)(PRTDBGMODINT pMod, const char *pszSymbol, PRTDBGSYMBOL pSymbol);

    /**
     * Queries symbol information by address.
     *
     * The returned symbol is what the debug info interpreter consideres the symbol
     * most applicable to the specified address. This usually means a symbol with an
     * address equal or lower than the requested.
     *
     * @returns VBox status code.
     * @retval  VINF_SUCCESS on success, no informational status code.
     * @retval  VERR_RTDBGMOD_NO_SYMBOLS if there aren't any symbols.
     * @retval  VERR_SYMBOL_NOT_FOUND if no suitable symbol was found.
     *
     * @param   pMod        Pointer to the module structure.
     * @param   iSeg        The segment number (0-based). RTDBGMOD_SEG_RVA can be used.
     * @param   off         The offset into the segment.
     * @param   poffDisp    Where to store the distance between the specified address
     *                      and the returned symbol. Optional.
     * @param   pSymbol     Where to store the symbol information.
     */
    DECLCALLBACKMEMBER(int, pfnSymbolByAddr)(PRTDBGMODINT pMod, uint32_t iSeg, RTGCUINTPTR off, PRTGCINTPTR poffDisp, PRTDBGSYMBOL pSymbol);

    /**
     * Queries line number information by address.
     *
     * @returns VBox status code.
     * @retval  VINF_SUCCESS on success, no informational status code.
     * @retval  VERR_RTDBGMOD_NO_LINE_NUMBERS if there aren't any line numbers.
     * @retval  VERR_RTDBGMOD_LINE_NOT_FOUND if no suitable line number was found.
     *
     * @param   pMod        Pointer to the module structure.
     * @param   iSeg        The segment number (0-based). RTDBGMOD_SEG_RVA can be used.
     * @param   off         The offset into the segment.
     * @param   poffDisp    Where to store the distance between the specified address
     *                      and the returned line number. Optional.
     * @param   pLine       Where to store the information about the closest line number.
     */
    DECLCALLBACKMEMBER(int, pfnLineByAddr)(PRTDBGMODINT pMod, uint32_t iSeg, RTGCUINTPTR off, PRTGCINTPTR poffDisp, PRTDBGLINE pLine);

    /**
     * Adds a symbol to the module (optional).
     *
     * This method is used to implement DBGFR3SymbolAdd.
     *
     * @returns VBox status code.
     * @retval  VERR_NOT_SUPPORTED if the interpreter doesn't support this feature.
     *
     * @param   pMod        Pointer to the module structure.
     * @param   pszSymbol   The symbol name.
     * @param   iSeg        The segment number (0-based). RTDBGMOD_SEG_RVA can be used.
     * @param   off         The offset into the segment.
     * @param   cbSymbol    The area covered by the symbol. 0 is fine.
     */
    DECLCALLBACKMEMBER(int, pfnSymbolAdd)(PRTDBGMODINT pMod, const char *pszSymbol, uint32_t iSeg, RTGCUINTPTR off, RTUINT cbSymbol);

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
    uint32_t        u32Magic;
    /** The number of address spaces this module is currently linked into.
     * This is used to perform automatic cleanup and sharing. */
    uint32_t        cLinks;
    /** The module name (short). */
    const char     *pszName;
    /** The module filename. Can be NULL. */
    const char     *pszImgFile;
    /** The debug info file (if external). Can be NULL. */
    const char     *pszDbgFile;

    /** The method table for the executable image interpreter. */
    PCRTDBGMODVTIMG pImgVt;
    /** Pointer to the private data of the executable image interpreter. */
    void           *pvImgPriv;

    /** The method table for the debug info interpreter. */
    PCRTDBGMODVTDBG pDbgVt;
    /** Pointer to the private data of the debug info interpreter. */
    void           *pvDbgPriv;

} RTDBGMODINT;


/** @} */

__END_DECLS

#endif


