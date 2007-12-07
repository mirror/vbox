/* $Id$ */
/** @file
 * innotek Portable Runtime - Loader Internals.
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

#ifndef ___internal_ldr_h
#define ___internal_ldr_h

#include <iprt/types.h>
#include "internal/magics.h"

__BEGIN_DECLS


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#ifdef __DOXYGEN__
/** @def LDR_WITH_NATIVE
 * Define this to get native support. */
# define LDR_WITH_NATIVE

/** @def LDR_WITH_ELF32
 * Define this to get 32-bit ELF support. */
# define LDR_WITH_ELF32

/** @def LDR_WITH_ELF64
 * Define this to get 64-bit ELF support. */
# define LDR_WITH_ELF64

/** @def LDR_WITH_PE
 * Define this to get 32-bit and 64-bit PE support. */
# define LDR_WITH_PE

/** @def LDR_WITH_LX
 * Define this to get LX support. */
# define LDR_WITH_LX

/** @def LDR_WITH_MACHO
 * Define this to get mach-o support (not implemented yet). */
# define LDR_WITH_MACHO
#endif /* __DOXYGEN__ */

#if defined(LDR_WITH_ELF32) || defined(LDR_WITH_ELF64)
/** @def LDR_WITH_ELF
 * This is defined if any of the ELF versions is requested.
 */
# define LDR_WITH_ELF
#endif

/* These two may clash with winnt.h. */
#undef IMAGE_DOS_SIGNATURE
#undef IMAGE_NT_SIGNATURE


/** Little endian uint32_t ELF signature ("\x7fELF"). */
#define IMAGE_ELF_SIGNATURE (0x7f | ('E' << 8) | ('L' << 16) | ('F' << 24))
/** Little endian uint32_t PE signature ("PE\0\0"). */
#define IMAGE_NT_SIGNATURE  0x00004550
/** Little endian uint16_t LX signature ("LX") */
#define IMAGE_LX_SIGNATURE  ('L' | ('X' << 8))
/** Little endian uint16_t LE signature ("LE") */
#define IMAGE_LE_SIGNATURE  ('L' | ('E' << 8))
/** Little endian uint16_t NE signature ("NE") */
#define IMAGE_NE_SIGNATURE  ('N' | ('E' << 8))
/** Little endian uint16_t MZ signature ("MZ"). */
#define IMAGE_DOS_SIGNATURE ('M' | ('Z' << 8))


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Loader state.
 */
typedef enum RTLDRSTATE
{
    /** Invalid. */
    LDR_STATE_INVALID = 0,
    /** Opened. */
    LDR_STATE_OPENED,
    /** The image can no longer be relocated. */
    LDR_STATE_DONE,
    /** The image was loaded, not opened. */
    LDR_STATE_LOADED,
    /** The usual 32-bit hack. */
    LDR_STATE_32BIT_HACK = 0x7fffffff
} RTLDRSTATE;


/** Pointer to a loader item. */
typedef struct RTLDRMODINTERNAL *PRTLDRMODINTERNAL;

/**
 * Loader module operations.
 */
typedef struct RTLDROPS
{
    /** The name of the executable format. */
    const char *pszName;

    /**
     * Release any resources attached to the module.
     * The caller will do RTMemFree on pMod on return.
     *
     * @returns iprt status code.
     * @param   pMod    Pointer to the loader module structure.
     * @remark  Compulsory entry point.
     */
    DECLCALLBACKMEMBER(int, pfnClose)(PRTLDRMODINTERNAL pMod);

    /**
     * Gets a simple symbol.
     * This entrypoint can be omitted if RTLDROPS::pfnGetSymbolEx() is provided.
     *
     * @returns iprt status code.
     * @param   pMod        Pointer to the loader module structure.
     * @param   pszSymbol   The symbol name.
     * @param   ppvValue    Where to store the symbol value.
     */
    DECLCALLBACKMEMBER(int, pfnGetSymbol)(PRTLDRMODINTERNAL pMod, const char *pszSymbol, void **ppvValue);

    /**
     * Called when we're done with getting bits and relocating them.
     * This is used to release resources used by the loader to support those actions.
     *
     * After this call none of the extended loader functions can be called.
     *
     * @returns iprt status code.
     * @param   pMod        Pointer to the loader module structure.
     * @remark  This is an optional entry point.
     */
    DECLCALLBACKMEMBER(int, pfnDone)(PRTLDRMODINTERNAL pMod);

    /**
     * Enumerates the symbols exported by the module.
     *
     * @returns iprt status code, which might have been returned by pfnCallback.
     * @param   pMod        Pointer to the loader module structure.
     * @param   fFlags      Flags indicating what to return and such.
     * @param   pvBits      Pointer to the bits returned by RTLDROPS::pfnGetBits(), optional.
     * @param   BaseAddress The image base addressto use when calculating the symbol values.
     * @param   pfnCallback The callback function which each symbol is to be feeded to.
     * @param   pvUser      User argument to pass to the enumerator.
     * @remark  This is an optional entry point.
     */
    DECLCALLBACKMEMBER(int, pfnEnumSymbols)(PRTLDRMODINTERNAL pMod, unsigned fFlags, const void *pvBits, RTUINTPTR BaseAddress,
                                            PFNRTLDRENUMSYMS pfnCallback, void *pvUser);


/* extended functions: */

    /**
     * Gets the size of the loaded image (i.e. in memory).
     *
     * @returns in memory size, in bytes.
     * @returns ~(size_t)0 if it's not an extended image.
     * @param   pMod    Pointer to the loader module structure.
     * @remark  Extended loader feature.
     */
    DECLCALLBACKMEMBER(size_t, pfnGetImageSize)(PRTLDRMODINTERNAL pMod);

    /**
     * Gets the image bits fixed up for a specified address.
     *
     * @returns iprt status code.
     * @param   pMod            Pointer to the loader module structure.
     * @param   pvBits          Where to store the bits. The size of this buffer is equal or
     *                          larger to the value returned by pfnGetImageSize().
     * @param   BaseAddress     The base address which the image should be fixed up to.
     * @param   pfnGetImport    The callback function to use to resolve imports (aka unresolved externals).
     * @param   pvUser          User argument to pass to the callback.
     * @remark  Extended loader feature.
     */
    DECLCALLBACKMEMBER(int, pfnGetBits)(PRTLDRMODINTERNAL pMod, void *pvBits, RTUINTPTR BaseAddress, PFNRTLDRIMPORT pfnGetImport, void *pvUser);

    /**
     * Relocate bits obtained using pfnGetBits to a new address.
     *
     * @returns iprt status code.
     * @param   pMod            Pointer to the loader module structure.
     * @param   pvBits          Where to store the bits. The size of this buffer is equal or
     *                          larger to the value returned by pfnGetImageSize().
     * @param   NewBaseAddress  The base address which the image should be fixed up to.
     * @param   OldBaseAddress  The base address which the image is currently fixed up to.
     * @param   pfnGetImport    The callback function to use to resolve imports (aka unresolved externals).
     * @param   pvUser          User argument to pass to the callback.
     * @remark  Extended loader feature.
     */
    DECLCALLBACKMEMBER(int, pfnRelocate)(PRTLDRMODINTERNAL pMod, void *pvBits, RTUINTPTR NewBaseAddress, RTUINTPTR OldBaseAddress, PFNRTLDRIMPORT pfnGetImport, void *pvUser);

    /**
     * Gets a symbol with special base address and stuff.
     * This entrypoint can be omitted if RTLDROPS::pfnGetSymbolEx() is provided and the special BaseAddress feature isn't supported.
     *
     * @returns iprt status code.
     * @param   pMod        Pointer to the loader module structure.
     * @param   pvBits      Pointer to bits returned by RTLDROPS::pfnGetBits(), optional.
     * @param   BaseAddress The image base address to use when calculating the symbol value.
     * @param   pszSymbol   The symbol name.
     * @param   pValue      Where to store the symbol value.
     * @remark  Extended loader feature.
     */
    DECLCALLBACKMEMBER(int, pfnGetSymbolEx)(PRTLDRMODINTERNAL pMod, const void *pvBits, RTUINTPTR BaseAddress, const char *pszSymbol, RTUINTPTR *pValue);

    /** Dummy entry to make sure we've initialized it all. */
    RTUINT uDummy;
} RTLDROPS;
typedef RTLDROPS *PRTLDROPS;
typedef const RTLDROPS *PCRTLDROPS;


/** Pointer to a loader reader instance. */
typedef struct RTLDRREADER *PRTLDRREADER;

/**
 * Loader image reader instance.
 * The reader will have extra data members following this structure.
 */
typedef struct RTLDRREADER
{
    /** The name of the image provider. */
    const char *pszName;

    /**
     * Reads bytes at a give place in the raw image.
     *
     * @returns iprt status code.
     * @param   pReader     Pointer to the reader instance.
     * @param   pvBuf       Where to store the bits.
     * @param   cb          Number of bytes to read.
     * @param   off         Where to start reading relative to the start of the raw image.
     */
    DECLCALLBACKMEMBER(int, pfnRead)(PRTLDRREADER pReader, void *pvBuf, size_t cb, RTFOFF off);

    /**
     * Tells end position of last read.
     *
     * @returns position relative to start of the raw image.
     * @param   pReader     Pointer to the reader instance.
     */
    DECLCALLBACKMEMBER(RTFOFF, pfnTell)(PRTLDRREADER pReader);

    /**
     * Gets the size of the raw image bits.
     *
     * @returns size of raw image bits in bytes.
     * @param   pReader     Pointer to the reader instance.
     */
    DECLCALLBACKMEMBER(RTFOFF, pfnSize)(PRTLDRREADER pReader);

    /**
     * Map the bits into memory.
     *
     * The mapping will be freed upon calling pfnDestroy() if not pfnUnmap()
     * is called before that. The mapping is read only.
     *
     * @returns iprt status code.
     * @param   pReader     Pointer to the reader instance.
     * @param   ppvBits     Where to store the address of the memory mapping on success.
     *                      The size of the mapping can be obtained by calling pfnSize().
     */
    DECLCALLBACKMEMBER(int, pfnMap)(PRTLDRREADER pReader, const void **ppvBits);

    /**
     * Unmap bits.
     *
     * @returns iprt status code.
     * @param   pReader     Pointer to the reader instance.
     * @param   pvBits      Memory pointer returned by pfnMap().
     */
    DECLCALLBACKMEMBER(int, pfnUnmap)(PRTLDRREADER pReader, const void *pvBits);

    /**
     * Gets the most appropriate log name.
     *
     * @returns Pointer to readonly log name.
     * @param   pReader     Pointer to the reader instance.
     */
    DECLCALLBACKMEMBER(const char *, pfnLogName)(PRTLDRREADER pReader);

    /**
     * Releases all resources associated with the reader instance.
     * The instance is invalid after this call returns.
     *
     * @returns iprt status code.
     * @param   pReader     Pointer to the reader instance.
     */
    DECLCALLBACKMEMBER(int, pfnDestroy)(PRTLDRREADER pReader);

} RTLDRREADER;


/**
 * Loader module core.
 */
typedef struct RTLDRMODINTERNAL
{
    /** The loader magic value (RTLDRMOD_MAGIC). */
    uint32_t                u32Magic;
    /** State. */
    RTLDRSTATE              eState;
    /** Loader ops. */
    PCRTLDROPS              pOps;
} RTLDRMODINTERNAL;


/**
 * Validates that a loader module handle is valid.
 *
 * @returns true if valid.
 * @returns false if invalid.
 * @param   hLdrMod     The loader module handle.
 */
DECLINLINE(bool) rtldrIsValid(RTLDRMOD hLdrMod)
{
    return VALID_PTR(hLdrMod)
        && ((PRTLDRMODINTERNAL)hLdrMod)->u32Magic == RTLDRMOD_MAGIC;
}

int rtldrOpenWithReader(PRTLDRREADER pReader, PRTLDRMOD phMod);


/**
 * Native loader module.
 */
typedef struct RTLDRMODNATIVE
{
    /** The core structure. */
    RTLDRMODINTERNAL    Core;
    /** The native handle. */
    uintptr_t           hNative;
} RTLDRMODNATIVE, *PRTLDRMODNATIVE;

/** @copydoc RTLDROPS::pfnGetSymbol */
DECLCALLBACK(int) rtldrNativeGetSymbol(PRTLDRMODINTERNAL pMod, const char *pszSymbol, void **ppvValue);
/** @copydoc RTLDROPS::pfnClose */
DECLCALLBACK(int) rtldrNativeClose(PRTLDRMODINTERNAL pMod);

/**
 * Load a native module using the native loader.
 *
 * @returns iprt status code.
 * @param   pszFilename     The image filename.
 * @param   phHandle        Where to store the module handle on success.
 */
int rtldrNativeLoad(const char *pszFilename, uintptr_t *phHandle);

int rtldrPEOpen(PRTLDRREADER pReader, RTFOFF offNtHdrs, PRTLDRMOD phLdrMod);
int rtldrELFOpen(PRTLDRREADER pReader, PRTLDRMOD phLdrMod);
int rtldrkLdrOpen(PRTLDRREADER pReader, PRTLDRMOD phLdrMod);
/*int rtldrLXOpen(PRTLDRREADER pReader, RTFOFF offLX, PRTLDRMOD phLdrMod);
int rtldrMachoOpen(PRTLDRREADER pReader, RTFOFF offSomething, PRTLDRMOD phLdrMod);*/


__END_DECLS

#endif

