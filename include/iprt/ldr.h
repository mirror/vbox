/** @file
 * IPRT - Loader.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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


RT_C_DECLS_BEGIN

/** Loader address (unsigned integer). */
typedef RTUINTPTR           RTLDRADDR;
/** Pointer to a loader address. */
typedef RTLDRADDR          *PRTLDRADDR;
/** Pointer to a const loader address. */
typedef RTLDRADDR const    *PCRTLDRADDR;
/** The max loader address value. */
#define RTLDRADDR_MAX       RTUINTPTR_MAX
/** NIL loader address value. */
#define NIL_RTLDRADDR       RTLDRADDR_MAX


/**
 * Gets the default file suffix for DLL/SO/DYLIB/whatever.
 *
 * @returns The stuff (readonly).
 */
RTDECL(const char *) RTLdrGetSuff(void);

/**
 * Checks if a library is loadable or not.
 *
 * This may attempt load and unload the library.
 *
 * @returns true/false accordingly.
 * @param   pszFilename     Image filename.
 */
RTDECL(bool) RTLdrIsLoadable(const char *pszFilename);

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
 * @param   fFlags      See RTLDRLOAD_FLAGS_XXX.
 * @param   pErrInfo    Where to return extended error information. Optional.
 */
RTDECL(int) RTLdrLoadEx(const char *pszFilename, PRTLDRMOD phLdrMod, uint32_t fFlags, PRTERRINFO pErrInfo);

/** @defgroup RTLDRLOAD_FLAGS_XXX RTLdrLoadEx flags.
 * @{ */
/** Symbols defined in this library are not made available to resolve
 * references in subsequently loaded libraries (default). */
#define RTLDRLOAD_FLAGS_LOCAL       UINT32_C(0)
/** Symbols defined in this library will be made available for symbol
 * resolution of subsequently loaded libraries. */
#define RTLDRLOAD_FLAGS_GLOBAL      RT_BIT_32(0)
/** The mask of valid flag bits. */
#define RTLDRLOAD_FLAGS_VALID_MASK  UINT32_C(0x00000001)
/** @} */

/**
 * Loads a dynamic load library (/shared object) image file residing in the
 * RTPathAppPrivateArch() directory.
 *
 * Suffix is not required.
 *
 * @returns iprt status code.
 * @param   pszFilename Image filename. No path.
 * @param   phLdrMod    Where to store the handle to the loaded module.
 */
RTDECL(int) RTLdrLoadAppPriv(const char *pszFilename, PRTLDRMOD phLdrMod);

/**
 * Image architecuture specifier for RTLdrOpenEx.
 */
typedef enum RTLDRARCH
{
    RTLDRARCH_INVALID = 0,
    /** Whatever. */
    RTLDRARCH_WHATEVER,
    /** The host architecture. */
    RTLDRARCH_HOST,
    /** 32-bit x86. */
    RTLDRARCH_X86_32,
    /** AMD64 (64-bit x86 if you like). */
    RTLDRARCH_AMD64,
    /** End of the valid values. */
    RTLDRARCH_END,
    /** Make sure the type is a full 32-bit. */
    RTLDRARCH_32BIT_HACK = 0x7fffffff
} RTLDRARCH;
/** Pointer to a RTLDRARCH. */
typedef RTLDRARCH *PRTLDRARCH;

/**
 * Open a binary image file, extended version.
 *
 * @returns iprt status code.
 * @param   pszFilename Image filename.
 * @param   fFlags      Reserved, MBZ.
 * @param   enmArch     CPU architecture specifier for the image to be loaded.
 * @param   phLdrMod    Where to store the handle to the loader module.
 */
RTDECL(int) RTLdrOpen(const char *pszFilename, uint32_t fFlags, RTLDRARCH enmArch, PRTLDRMOD phLdrMod);

/**
 * Opens a binary image file using kLdr.
 *
 * @returns iprt status code.
 * @param   pszFilename     Image filename.
 * @param   phLdrMod        Where to store the handle to the loaded module.
 * @param   fFlags      Reserved, MBZ.
 * @param   enmArch     CPU architecture specifier for the image to be loaded.
 * @remark  Primarily for testing the loader.
 */
RTDECL(int) RTLdrOpenkLdr(const char *pszFilename, uint32_t fFlags, RTLDRARCH enmArch, PRTLDRMOD phLdrMod);

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
RTDECL(int) RTLdrGetSymbolEx(RTLDRMOD hLdrMod, const void *pvBits, RTLDRADDR BaseAddress, const char *pszSymbol,
                             PRTLDRADDR pValue);

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
typedef DECLCALLBACK(int) RTLDRIMPORT(RTLDRMOD hLdrMod, const char *pszModule, const char *pszSymbol, unsigned uSymbol,
                                      PRTLDRADDR pValue, void *pvUser);
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
RTDECL(int) RTLdrGetBits(RTLDRMOD hLdrMod, void *pvBits, RTLDRADDR BaseAddress, PFNRTLDRIMPORT pfnGetImport, void *pvUser);

/**
 * Relocates bits after getting them.
 * Useful for code which moves around a bit.
 *
 * @returns iprt status code.
 * @param   hLdrMod             The loader module handle.
 * @param   pvBits              Where the image bits are.
 *                              Must have been passed to RTLdrGetBits().
 * @param   NewBaseAddress      The new base address.
 * @param   OldBaseAddress      The old base address.
 * @param   pfnGetImport        Callback function for resolving imports one by one.
 * @param   pvUser              User argument for the callback.
 * @remark  Not supported for RTLdrLoad() images.
 */
RTDECL(int) RTLdrRelocate(RTLDRMOD hLdrMod, void *pvBits, RTLDRADDR NewBaseAddress, RTLDRADDR OldBaseAddress,
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
typedef DECLCALLBACK(int) RTLDRENUMSYMS(RTLDRMOD hLdrMod, const char *pszSymbol, unsigned uSymbol, RTLDRADDR Value, void *pvUser);
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
RTDECL(int) RTLdrEnumSymbols(RTLDRMOD hLdrMod, unsigned fFlags, const void *pvBits, RTLDRADDR BaseAddress, PFNRTLDRENUMSYMS pfnCallback, void *pvUser);

/** @name RTLdrEnumSymbols flags.
 * @{ */
/** Returns ALL kinds of symbols. The default is to only return public/exported symbols. */
#define RTLDR_ENUM_SYMBOL_FLAGS_ALL    RT_BIT(1)
/** @} */


/**
 * Debug info type (as far the loader can tell).
 */
typedef enum RTLDRDBGINFOTYPE
{
    /** The invalid 0 value. */
    RTLDRDBGINFOTYPE_INVALID = 0,
    /** Unknown debug info format. */
    RTLDRDBGINFOTYPE_UNKNOWN,
    /** Stabs. */
    RTLDRDBGINFOTYPE_STABS,
    /** Debug With Arbitrary Record Format (DWARF). */
    RTLDRDBGINFOTYPE_DWARF,
    /** Microsoft Codeview debug info. */
    RTLDRDBGINFOTYPE_CODEVIEW,
    /** Watcom debug info. */
    RTLDRDBGINFOTYPE_WATCOM,
    /** IBM High Level Language debug info.. */
    RTLDRDBGINFOTYPE_HLL,
    /** The end of the valid debug info values (exclusive). */
    RTLDRDBGINFOTYPE_END,
    /** Blow the type up to 32-bits. */
    RTLDRDBGINFOTYPE_32BIT_HACK = 0x7fffffff
} RTLDRDBGINFOTYPE;

/**
 * Debug info enumerator callback.
 *
 * @returns VINF_SUCCESS to continue the enumeration.  Any other status code
 *          will cause RTLdrEnumDbgInfo to immediately return with that status.
 *
 * @param   hLdrMod         The module handle.
 * @param   iDbgInfo        The debug info ordinal number / id.
 * @param   enmType         The debug info type.
 * @param   iMajorVer       The major version number of the debug info format.
 *                          -1 if unknow - implies invalid iMinorVer.
 * @param   iMinorVer       The minor version number of the debug info format.
 *                          -1 when iMajorVer is -1.
 * @param   pszPartNm       The name of the debug info part, NULL if not
 *                          applicable.
 * @param   offFile         The file offset *if* this type has one specific
 *                          location in the executable image file. This is -1
 *                          if there isn't any specific file location.
 * @param   LinkAddress     The link address of the debug info if it's
 *                          loadable. NIL_RTLDRADDR if not loadable.
 * @param   cb              The size of the debug information. -1 is used if
 *                          this isn't applicable.
 * @param   pszExtFile      This points to the name of an external file
 *                          containing the debug info.  This is NULL if there
 *                          isn't any external file.
 * @param   pvUser          The user parameter specified to RTLdrEnumDbgInfo.
 */
typedef DECLCALLBACK(int) FNRTLDRENUMDBG(RTLDRMOD hLdrMod, uint32_t iDbgInfo, RTLDRDBGINFOTYPE enmType,
                                         uint16_t iMajorVer, uint16_t iMinorVer, const char *pszPartNm,
                                         RTFOFF offFile, RTLDRADDR LinkAddress, RTLDRADDR cb,
                                         const char *pszExtFile, void *pvUser);
/** Pointer to a debug info enumerator callback. */
typedef FNRTLDRENUMDBG *PFNRTLDRENUMDBG;

/**
 * Enumerate the debug info contained in the executable image.
 *
 * @returns IPRT status code or whatever pfnCallback returns.
 *
 * @param   hLdrMod         The module handle.
 * @param   pvBits          Optional pointer to bits returned by
 *                          RTLdrGetBits().  This can be used by some module
 *                          interpreters to reduce memory consumption.
 * @param   pfnCallback     The callback function.
 * @param   pvUser          The user argument.
 */
RTDECL(int) RTLdrEnumDbgInfo(RTLDRMOD hLdrMod, const void *pvBits, PFNRTLDRENUMDBG pfnCallback, void *pvUser);


/**
 * Loader segment.
 */
typedef struct RTLDRSEG
{
    /** The segment name. (Might not be zero terminated!) */
    const char     *pchName;
    /** The length of the segment name. */
    uint32_t        cchName;
    /** The flat selector to use for the segment (i.e. data/code).
     * Primarily a way for the user to specify selectors for the LX/LE and NE interpreters. */
    uint16_t        SelFlat;
    /** The 16-bit selector to use for the segment.
     * Primarily a way for the user to specify selectors for the LX/LE and NE interpreters. */
    uint16_t        Sel16bit;
    /** Segment flags. */
    uint32_t        fFlags;
    /** The segment protection (RTMEM_PROT_XXX). */
    uint32_t        fProt;
    /** The size of the segment. */
    RTLDRADDR       cb;
    /** The required segment alignment.
     * The to 0 if the segment isn't supposed to be mapped. */
    RTLDRADDR       Alignment;
    /** The link address.
     * Set to NIL_RTLDRADDR if the segment isn't supposed to be mapped or if
     * the image doesn't have link addresses. */
    RTLDRADDR       LinkAddress;
    /** File offset of the segment.
     * Set to -1 if no file backing (like BSS). */
    RTFOFF          offFile;
    /** Size of the file bits of the segment.
     * Set to -1 if no file backing (like BSS). */
    RTFOFF          cbFile;
    /** The relative virtual address when mapped.
     * Set to NIL_RTLDRADDR if the segment isn't supposed to be mapped. */
    RTLDRADDR       RVA;
    /** The size of the segment including the alignment gap up to the next segment when mapped.
     * This is set to NIL_RTLDRADDR if not implemented. */
    RTLDRADDR       cbMapped;
} RTLDRSEG;
/** Pointer to a loader segment. */
typedef RTLDRSEG *PRTLDRSEG;
/** Pointer to a read only loader segment. */
typedef RTLDRSEG const *PCRTLDRSEG;


/** @name Segment flags
 * @{ */
/** The segment is 16-bit. When not set the default of the target architecture is assumed. */
#define RTLDRSEG_FLAG_16BIT         UINT32_C(1)
/** The segment requires a 16-bit selector alias. (OS/2) */
#define RTLDRSEG_FLAG_OS2_ALIAS16   UINT32_C(2)
/** Conforming segment (x86 weirdness). (OS/2) */
#define RTLDRSEG_FLAG_OS2_CONFORM   UINT32_C(4)
/** IOPL (ring-2) segment. (OS/2) */
#define RTLDRSEG_FLAG_OS2_IOPL      UINT32_C(8)
/** @} */

/**
 * Segment enumerator callback.
 *
 * @returns VINF_SUCCESS to continue the enumeration.  Any other status code
 *          will cause RTLdrEnumSegments to immediately return with that
 *          status.
 *
 * @param   hLdrMod         The module handle.
 * @param   pSeg            The segment information.
 * @param   pvUser          The user parameter specified to RTLdrEnumSegments.
 */
typedef DECLCALLBACK(int) FNRTLDRENUMSEGS(RTLDRMOD hLdrMod, PCRTLDRSEG pSeg, void *pvUser);
/** Pointer to a segment enumerator callback. */
typedef FNRTLDRENUMSEGS *PFNRTLDRENUMSEGS;

/**
 * Enumerate the debug info contained in the executable image.
 *
 * @returns IPRT status code or whatever pfnCallback returns.
 *
 * @param   hLdrMod         The module handle.
 * @param   pfnCallback     The callback function.
 * @param   pvUser          The user argument.
 */
RTDECL(int) RTLdrEnumSegments(RTLDRMOD hLdrMod, PFNRTLDRENUMSEGS pfnCallback, void *pvUser);


RT_C_DECLS_END

/** @} */

#endif

