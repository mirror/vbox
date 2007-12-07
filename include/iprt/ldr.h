/** @file
 * innotek Portable Runtime - Loader.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

#ifndef ___iprt_ldr_h
#define ___iprt_ldr_h

#include <iprt/cdefs.h>
#include <iprt/types.h>


/** @defgroup grp_ldr       RTLdr - Loader
 * @ingroup grp_rt
 * @{
 */

__BEGIN_DECLS


/**
 * Loads a dynamic load library (/shared object) image file using native
 * OS facilities.
 *
 * The filename will be appended the default DLL/SO extension of
 * the platform if it have been omitted. This means that it's not
 * possible to load DLLs/SOs with no extension using this interface,
 * but that's not a bad tradeoff.
 *
 * If no path is specified in the filename, the OS will usually search it's library
 * path to find the image file.
 *
 * @returns iprt status code.
 * @param   pszFilename Image filename.
 * @param   phLdrMod    Where to store the handle to the loader module.
 */
RTDECL(int) RTLdrLoad(const char *pszFilename, PRTLDRMOD phLdrMod);

/**
 * Open a binary image file.
 *
 * @returns iprt status code.
 * @param   pszFilename Image filename.
 * @param   phLdrMod    Where to store the handle to the loader module.
 */
RTDECL(int) RTLdrOpen(const char *pszFilename, PRTLDRMOD phLdrMod);

/**
 * Opens a binary image file using kLdr.
 * 
 * @returns iprt status code.
 * @param   pszFilename     Image filename.
 * @param   phLdrMod        Where to store the handle to the loaded module.
 * @remark  Primarily for testing the loader.
 */
RTDECL(int) RTLdrOpenkLdr(const char *pszFilename, PRTLDRMOD phLdrMod);

/**
 * What to expect and do with the bits passed to RTLdrOpenBits().
 */
typedef enum RTLDROPENBITS
{
    /** The usual invalid 0 entry. */
    RTLDROPENBITS_INVALID = 0,
    /** The bits are readonly and will never be changed. */
    RTLDROPENBITS_READONLY,
    /** The bits are going to be changed and the loader will have to duplicate them
     * when opening the image. */
    RTLDROPENBITS_WRITABLE,
    /** The bits are both the source and destination for the loader operation.
     * This means that the loader may have to duplicate them prior to changing them. */
    RTLDROPENBITS_SRC_AND_DST,
    /** The end of the valid enums. This entry marks the
     * first invalid entry.. */
    RTLDROPENBITS_END,
    RTLDROPENBITS_32BIT_HACK = 0x7fffffff
} RTLDROPENBITS;

/**
 * Open a binary image from in-memory bits.
 *
 * @returns iprt status code.
 * @param   pvBits      The start of the raw-image.
 * @param   cbBits      The size of the raw-image.
 * @param   enmBits     What to expect from the pvBits.
 * @param   pszLogName  What to call the raw-image when logging.
 *                      For RTLdrLoad and RTLdrOpen the filename is used for this.
 * @param   phLdrMod    Where to store the handle to the loader module.
 */
RTDECL(int) RTLdrOpenBits(const void *pvBits, size_t cbBits, RTLDROPENBITS enmBits, const char *pszLogName, PRTLDRMOD phLdrMod);

/**
 * Closes a loader module handle.
 *
 * The handle can be obtained using any of the RTLdrLoad(), RTLdrOpen()
 * and RTLdrOpenBits() functions.
 *
 * @returns iprt status code.
 * @param   hLdrMod         The loader module handle.
 */
RTDECL(int) RTLdrClose(RTLDRMOD hLdrMod);

/**
 * Gets the address of a named exported symbol.
 *
 * @returns iprt status code.
 * @param   hLdrMod         The loader module handle.
 * @param   pszSymbol       Symbol name.
 * @param   ppvValue        Where to store the symbol value. Note that this is restricted to the
 *                          pointer size used on the host!
 */
RTDECL(int) RTLdrGetSymbol(RTLDRMOD hLdrMod, const char *pszSymbol, void **ppvValue);

/**
 * Gets the address of a named exported symbol.
 *
 * This function differs from the plain one in that it can deal with
 * both GC and HC address sizes, and that it can calculate the symbol
 * value relative to any given base address.
 *
 * @returns iprt status code.
 * @param   hLdrMod         The loader module handle.
 * @param   pvBits          Optional pointer to the loaded image.
 *                          Set this to NULL if no RTLdrGetBits() processed image bits are available.
 *                          Not supported for RTLdrLoad() images.
 * @param   BaseAddress     Image load address.
 *                          Not supported for RTLdrLoad() images.
 * @param   pszSymbol       Symbol name.
 * @param   pValue          Where to store the symbol value.
 */
RTDECL(int) RTLdrGetSymbolEx(RTLDRMOD hLdrMod, const void *pvBits, RTUINTPTR BaseAddress, const char *pszSymbol, RTUINTPTR *pValue);

/**
 * Gets the size of the loaded image.
 * This is only supported for modules which has been opened using RTLdrOpen() and RTLdrOpenBits().
 *
 * @returns image size (in bytes).
 * @returns ~(size_t)0 on if not opened by RTLdrOpen().
 * @param   hLdrMod     Handle to the loader module.
 * @remark  Not supported for RTLdrLoad() images.
 */
RTDECL(size_t) RTLdrSize(RTLDRMOD hLdrMod);

/**
 * Resolve an external symbol during RTLdrGetBits().
 *
 * @returns iprt status code.
 * @param   hLdrMod         The loader module handle.
 * @param   pszModule       Module name.
 * @param   pszSymbol       Symbol name, NULL if uSymbol should be used.
 * @param   uSymbol         Symbol ordinal, ~0 if pszSymbol should be used.
 * @param   pValue          Where to store the symbol value (address).
 * @param   pvUser          User argument.
 */
typedef DECLCALLBACK(int) RTLDRIMPORT(RTLDRMOD hLdrMod, const char *pszModule, const char *pszSymbol, unsigned uSymbol, RTUINTPTR *pValue, void *pvUser);
/** Pointer to a FNRTLDRIMPORT() callback function. */
typedef RTLDRIMPORT *PFNRTLDRIMPORT;

/**
 * Loads the image into a buffer provided by the user and applies fixups
 * for the given base address.
 *
 * @returns iprt status code.
 * @param   hLdrMod         The load module handle.
 * @param   pvBits          Where to put the bits.
 *                          Must be as large as RTLdrSize() suggests.
 * @param   BaseAddress     The base address.
 * @param   pfnGetImport    Callback function for resolving imports one by one.
 * @param   pvUser          User argument for the callback.
 * @remark  Not supported for RTLdrLoad() images.
 */
RTDECL(int) RTLdrGetBits(RTLDRMOD hLdrMod, void *pvBits, RTUINTPTR BaseAddress, PFNRTLDRIMPORT pfnGetImport, void *pvUser);

/**
 * Relocates bits after getting them.
 * Useful for code which moves around a bit.
 *
 * @returns iprt status code.
 * @param   hLdrMod             The loader module handle.
 * @param   pvBits              Where the image bits are.
 *                              Must've been passed to RTLdrGetBits().
 * @param   NewBaseAddress      The new base address.
 * @param   OldBaseAddress      The old base address.
 * @param   pfnGetImport        Callback function for resolving imports one by one.
 * @param   pvUser              User argument for the callback.
 * @remark  Not supported for RTLdrLoad() images.
 */
RTDECL(int) RTLdrRelocate(RTLDRMOD hLdrMod, void *pvBits, RTUINTPTR NewBaseAddress, RTUINTPTR OldBaseAddress,
                          PFNRTLDRIMPORT pfnGetImport, void *pvUser);

/**
 * Enumeration callback function used by RTLdrEnumSymbols().
 *
 * @returns iprt status code. Failure will stop the enumeration.
 * @param   hLdrMod         The loader module handle.
 * @param   pszSymbol       Symbol name. NULL if ordinal only.
 * @param   uSymbol         Symbol ordinal, ~0 if not used.
 * @param   Value           Symbol value.
 * @param   pvUser          The user argument specified to RTLdrEnumSymbols().
 */
typedef DECLCALLBACK(int) RTLDRENUMSYMS(RTLDRMOD hLdrMod, const char *pszSymbol, unsigned uSymbol, RTUINTPTR Value, void *pvUser);
/** Pointer to a RTLDRENUMSYMS() callback function. */
typedef RTLDRENUMSYMS *PFNRTLDRENUMSYMS;

/**
 * Enumerates all symbols in a module.
 *
 * @returns iprt status code.
 * @param   hLdrMod         The loader module handle.
 * @param   fFlags          Flags indicating what to return and such.
 * @param   pvBits          Optional pointer to the loaded image. (RTLDR_ENUM_SYMBOL_FLAGS_*)
 *                          Set this to NULL if no RTLdrGetBits() processed image bits are available.
 * @param   BaseAddress     Image load address.
 * @param   pfnCallback     Callback function.
 * @param   pvUser          User argument for the callback.
 * @remark  Not supported for RTLdrLoad() images.
 */
RTDECL(int) RTLdrEnumSymbols(RTLDRMOD hLdrMod, unsigned fFlags, const void *pvBits, RTUINTPTR BaseAddress, PFNRTLDRENUMSYMS pfnCallback, void *pvUser);

/** @name RTLdrEnumSymbols flags.
 * @{ */
/** Returns ALL kinds of symbols. The default is to only return public/exported symbols. */
#define RTLDR_ENUM_SYMBOL_FLAGS_ALL    RT_BIT(1)
/** @} */

__END_DECLS

/** @} */

#endif

