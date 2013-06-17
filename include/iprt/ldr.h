/** @file
 * IPRT - Loader.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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
 * Loader module format.
 */
typedef enum RTLDRFMT
{
    /** The usual invalid 0 format. */
    RTLDRFMT_INVALID = 0,
    /** The native OS loader. */
    RTLDRFMT_NATIVE,
    /** The AOUT loader. */
    RTLDRFMT_AOUT,
    /** The ELF loader. */
    RTLDRFMT_ELF,
    /** The LX loader. */
    RTLDRFMT_LX,
    /** The Mach-O loader. */
    RTLDRFMT_MACHO,
    /** The PE loader. */
    RTLDRFMT_PE,
    /** The end of the valid format values (exclusive). */
    RTLDRFMT_END,
    /** Hack to blow the type up to 32-bit. */
    RTLDRFMT_32BIT_HACK = 0x7fffffff
} RTLDRFMT;


/**
 * Loader module type.
 */
typedef enum RTLDRTYPE
{
    /** The usual invalid 0 type. */
    RTLDRTYPE_INVALID = 0,
    /** Object file. */
    RTLDRTYPE_OBJECT,
    /** Executable module, fixed load address. */
    RTLDRTYPE_EXECUTABLE_FIXED,
    /** Executable module, relocatable, non-fixed load address. */
    RTLDRTYPE_EXECUTABLE_RELOCATABLE,
    /** Executable module, position independent code, non-fixed load address. */
    RTLDRTYPE_EXECUTABLE_PIC,
    /** Shared library, fixed load address.
     * Typically a system library. */
    RTLDRTYPE_SHARED_LIBRARY_FIXED,
    /** Shared library, relocatable, non-fixed load address. */
    RTLDRTYPE_SHARED_LIBRARY_RELOCATABLE,
    /** Shared library, position independent code, non-fixed load address. */
    RTLDRTYPE_SHARED_LIBRARY_PIC,
    /** DLL that contains no code or data only imports and exports. (Chiefly OS/2.) */
    RTLDRTYPE_FORWARDER_DLL,
    /** Core or dump. */
    RTLDRTYPE_CORE,
    /** Debug module (debug info with empty code & data segments). */
    RTLDRTYPE_DEBUG_INFO,
    /** The end of the valid types values (exclusive). */
    RTLDRTYPE_END,
    /** Hack to blow the type up to 32-bit. */
    RTLDRTYPE_32BIT_HACK = 0x7fffffff
} RTLDRTYPE;


/**
 * Loader endian indicator.
 */
typedef enum RTLDRENDIAN
{
    /** The usual invalid endian. */
    RTLDRENDIAN_INVALID,
    /** Little endian. */
    RTLDRENDIAN_LITTLE,
    /** Bit endian. */
    RTLDRENDIAN_BIG,
    /** Endianness doesn't have a meaning in the context. */
    RTLDRENDIAN_NA,
    /** The end of the valid endian values (exclusive). */
    RTLDRENDIAN_END,
    /** Hack to blow the type up to 32-bit. */
    RTLDRENDIAN_32BIT_HACK = 0x7fffffff
} RTLDRENDIAN;


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
/** Do not unload the library upon RTLdrClose. (For system libs.) */
#define RTLDRLOAD_FLAGS_NO_UNLOAD   RT_BIT_32(1)
/** The mask of valid flag bits. */
#define RTLDRLOAD_FLAGS_VALID_MASK  UINT32_C(0x00000003)
/** @} */

/**
 * Loads a dynamic load library (/shared object) image file residing in one of
 * the default system library locations.
 *
 * Only the system library locations are searched. No suffix is required.
 *
 * @returns iprt status code.
 * @param   pszFilename Image filename. No path.
 * @param   fNoUnload   Do not unload the library when RTLdrClose is called.
 * @param   phLdrMod    Where to store the handle to the loaded module.
 */
RTDECL(int) RTLdrLoadSystem(const char *pszFilename, bool fNoUnload, PRTLDRMOD phLdrMod);

/**
 * Combines RTLdrLoadSystem and RTLdrGetSymbol, with fNoUnload set to true.
 *
 * @returns The symbol value, NULL on failure.  (If you care for a less boolean
 *          status, go thru the necessary API calls yourself.)
 * @param   pszFilename Image filename. No path.
 * @param   pszSymbol       Symbol name.
 */
RTDECL(void *) RTLdrGetSystemSymbol(const char *pszFilename, const char *pszSymbol);

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
 * Gets the native module handle for a module loaded by RTLdrLoad, RTLdrLoadEx,
 * RTLdrLoadSystem,  or RTLdrLoadAppPriv.
 *
 * @returns Native handle on success, ~(uintptr_t)0 on failure.
 * @param   hLdrMod     The loader module handle.
 */
RTDECL(uintptr_t) RTLdrGetNativeHandle(RTLDRMOD hLdrMod);


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

/** @name RTLDR_O_XXX - RTLdrOpen flags.
 * @{ */
/** Open for debugging or introspection reasons.
 * This will skip a few of the stricter validations when loading images. */
#define RTLDR_O_FOR_DEBUG       RT_BIT_32(0)
/** Mask of valid flags. */
#define RTLDR_O_VALID_MASK      UINT32_C(0x00000001)
/** @} */

/**
 * Open a binary image file, extended version.
 *
 * @returns iprt status code.
 * @param   pszFilename Image filename.
 * @param   fFlags      Valid RTLDR_O_XXX combination.
 * @param   enmArch     CPU architecture specifier for the image to be loaded.
 * @param   phLdrMod    Where to store the handle to the loader module.
 */
RTDECL(int) RTLdrOpen(const char *pszFilename, uint32_t fFlags, RTLDRARCH enmArch, PRTLDRMOD phLdrMod);

/**
 * Opens a binary image file using kLdr.
 *
 * @returns iprt status code.
 * @param   pszFilename Image filename.
 * @param   phLdrMod    Where to store the handle to the loaded module.
 * @param   fFlags      Valid RTLDR_O_XXX combination.
 * @param   enmArch     CPU architecture specifier for the image to be loaded.
 * @remark  Primarily for testing the loader.
 */
RTDECL(int) RTLdrOpenkLdr(const char *pszFilename, uint32_t fFlags, RTLDRARCH enmArch, PRTLDRMOD phLdrMod);


/**
 * Called to read @a cb bytes at @a off into @a pvBuf.
 *
 * @returns IPRT status code
 * @param   pvBuf       The output buffer.
 * @param   cb          The number of bytes to read.
 * @param   off         Where to start reading.
 * @param   pvUser      The user parameter.
 */
typedef DECLCALLBACK(int) FNRTLDRRDRMEMREAD(void *pvBuf, size_t cb, size_t off, void *pvUser);
/** Pointer to a RTLdrOpenInMemory reader callback. */
typedef FNRTLDRRDRMEMREAD *PFNRTLDRRDRMEMREAD;

/**
 * Called to when the module is unloaded (or done loading) to release resources
 * associated with it (@a pvUser).
 *
 * @returns IPRT status code
 * @param   pvUser      The user parameter.
 */
typedef DECLCALLBACK(void) FNRTLDRRDRMEMDTOR(void *pvUser);
/** Pointer to a RTLdrOpenInMemory destructor callback. */
typedef FNRTLDRRDRMEMDTOR *PFNRTLDRRDRMEMDTOR;

/**
 * Open a in-memory image or an image with a custom reader callback.
 *
 * @returns IPRT status code.
 * @param   pszName     The image name.
 * @param   fFlags      Valid RTLDR_O_XXX combination.
 * @param   enmArch     CPU architecture specifier for the image to be loaded.
 * @param   cbImage     The size of the image (fake file).
 * @param   pfnRead     The read function.  If NULL is passed in, a default
 *                      reader function is provided that assumes @a pvUser
 *                      points to the raw image bits, at least @a cbImage of
 *                      valid memory.
 * @param   pfnDtor     The destructor function.  If NULL is passed, a default
 *                      destructor will be provided that passes @a pvUser to
 *                      RTMemFree.
 * @param   pvUser      The user argument or, if any of the callbacks are NULL,
 *                      a pointer to a memory block.
 * @param   phLdrMod    Where to return the module handle.
 *
 * @remarks With the exception of invalid @a pfnDtor and/or @a pvUser
 *          parameters, the pfnDtor methods (or the default one if NULL) will
 *          always be invoked.  The destruction of pvUser is entirely in the
 *          hands of this method once it's called.
 */
RTDECL(int) RTLdrOpenInMemory(const char *pszName, uint32_t fFlags, RTLDRARCH enmArch, size_t cbImage,
                              PFNRTLDRRDRMEMREAD pfnRead, PFNRTLDRRDRMEMDTOR pfnDtor, void *pvUser,
                              PRTLDRMOD phLdrMod);

/**
 * Closes a loader module handle.
 *
 * The handle can be obtained using any of the RTLdrLoad(), RTLdrOpen()
 * and RTLdrOpenInMemory() functions.
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
 * Gets the address of a named exported function.
 *
 * Same as RTLdrGetSymbol, but skips the status code and pointer to return
 * variable stuff.
 *
 * @returns Pointer to the function if found, NULL if not.
 * @param   hLdrMod         The loader module handle.
 * @param   pszSymbol       Function name.
 */
RTDECL(PFNRT) RTLdrGetFunction(RTLDRMOD hLdrMod, const char *pszSymbol);

/**
 * Gets the size of the loaded image.
 *
 * This is not necessarily available for images that has been loaded using
 * RTLdrLoad().
 *
 * @returns image size (in bytes).
 * @returns ~(size_t)0 on if not available.
 * @param   hLdrMod     Handle to the loader module.
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
#define RTLDR_ENUM_SYMBOL_FLAGS_ALL     RT_BIT(1)
/** Ignore forwarders (for use with RTLDR_ENUM_SYMBOL_FLAGS_ALL). */
#define RTLDR_ENUM_SYMBOL_FLAGS_NO_FWD  RT_BIT(2)
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
    /** Debug With Arbitrary Record Format (DWARF), in external file (DWO). */
    RTLDRDBGINFOTYPE_DWARF_DWO,
    /** Microsoft Codeview debug info. */
    RTLDRDBGINFOTYPE_CODEVIEW,
    /** Microsoft Codeview debug info, in external v2.0+ program database (PDB). */
    RTLDRDBGINFOTYPE_CODEVIEW_PDB20,
    /** Microsoft Codeview debug info, in external v7.0+ program database (PDB). */
    RTLDRDBGINFOTYPE_CODEVIEW_PDB70,
    /** Microsoft Codeview debug info, in external file (DBG). */
    RTLDRDBGINFOTYPE_CODEVIEW_DBG,
    /** Microsoft COFF debug info. */
    RTLDRDBGINFOTYPE_COFF,
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
 * Debug info details for the enumeration callback.
 */
typedef struct RTLDRDBGINFO
{
    /** The kind of debug info. */
    RTLDRDBGINFOTYPE    enmType;
    /** The debug info ordinal number / id. */
    uint32_t            iDbgInfo;
    /** The file offset *if* this type has one specific location in the executable
     * image file. This is -1 if there isn't any specific file location. */
    RTFOFF              offFile;
    /** The link address of the debug info if it's loadable. NIL_RTLDRADDR if not
     * loadable*/
    RTLDRADDR           LinkAddress;
    /** The size of the debug information. -1 is used if this isn't applicable.*/
    RTLDRADDR           cb;
    /** This is set if the debug information is found in an external file.  NULL
     * if no external file involved.
     * @note Putting it outside the union to allow lazy callback implementation. */
    const char         *pszExtFile;
    /** Type (enmType) specific information. */
    union
    {
        /** RTLDRDBGINFOTYPE_DWARF */
        struct
        {
            /** The section name. */
            const char *pszSection;
        } Dwarf;

        /** RTLDRDBGINFOTYPE_DWARF_DWO */
        struct
        {
            /** The CRC32 of the external file. */
            uint32_t    uCrc32;
        } Dwo;

        /** RTLDRDBGINFOTYPE_CODEVIEW, RTLDRDBGINFOTYPE_COFF  */
        struct
        {
            /** The PE image size. */
            uint32_t    cbImage;
            /** The timestamp. */
            uint32_t    uTimestamp;
            /** The major version from the entry. */
            uint32_t    uMajorVer;
            /** The minor version from the entry. */
            uint32_t    uMinorVer;
        } Cv, Coff;

        /** RTLDRDBGINFOTYPE_CODEVIEW_DBG */
        struct
        {
            /** The PE image size. */
            uint32_t    cbImage;
            /** The timestamp. */
            uint32_t    uTimestamp;
        } Dbg;

        /** RTLDRDBGINFOTYPE_CODEVIEW_PDB20*/
        struct
        {
            /** The PE image size. */
            uint32_t    cbImage;
            /** The timestamp. */
            uint32_t    uTimestamp;
            /** The PDB age. */
            uint32_t    uAge;
        } Pdb20;

        /** RTLDRDBGINFOTYPE_CODEVIEW_PDB70 */
        struct
        {
            /** The PE image size. */
            uint32_t    cbImage;
            /** The PDB age. */
            uint32_t    uAge;
            /** The UUID. */
            RTUUID      Uuid;
        } Pdb70;
    } u;
} RTLDRDBGINFO;
/** Pointer to debug info details. */
typedef RTLDRDBGINFO *PRTLDRDBGINFO;
/** Pointer to read only debug info details. */
typedef RTLDRDBGINFO const *PCRTLDRDBGINFO;


/**
 * Debug info enumerator callback.
 *
 * @returns VINF_SUCCESS to continue the enumeration.  Any other status code
 *          will cause RTLdrEnumDbgInfo to immediately return with that status.
 *
 * @param   hLdrMod         The module handle.
 * @param   pDbgInfo        Pointer to a read only structure with the details.
 * @param   pvUser          The user parameter specified to RTLdrEnumDbgInfo.
 */
typedef DECLCALLBACK(int) FNRTLDRENUMDBG(RTLDRMOD hLdrMod, PCRTLDRDBGINFO pDbgInfo, void *pvUser);
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
    /** The segment name.  Always set to something. */
    const char     *pszName;
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

/**
 * Converts a link address to a segment:offset address.
 *
 * @returns IPRT status code.
 *
 * @param   hLdrMod         The module handle.
 * @param   LinkAddress     The link address to convert.
 * @param   piSeg           Where to return the segment index.
 * @param   poffSeg         Where to return the segment offset.
 */
RTDECL(int) RTLdrLinkAddressToSegOffset(RTLDRMOD hLdrMod, RTLDRADDR LinkAddress, uint32_t *piSeg, PRTLDRADDR poffSeg);

/**
 * Converts a link address to an image relative virtual address (RVA).
 *
 * @returns IPRT status code.
 *
 * @param   hLdrMod         The module handle.
 * @param   LinkAddress     The link address to convert.
 * @param   pRva            Where to return the RVA.
 */
RTDECL(int) RTLdrLinkAddressToRva(RTLDRMOD hLdrMod, RTLDRADDR LinkAddress, PRTLDRADDR pRva);

/**
 * Converts an image relative virtual address (RVA) to a segment:offset.
 *
 * @returns IPRT status code.
 *
 * @param   hLdrMod         The module handle.
 * @param   Rva             The link address to convert.
 * @param   piSeg           Where to return the segment index.
 * @param   poffSeg         Where to return the segment offset.
 */
RTDECL(int) RTLdrSegOffsetToRva(RTLDRMOD hLdrMod, uint32_t iSeg, RTLDRADDR offSeg, PRTLDRADDR pRva);

/**
 * Converts a segment:offset into an image relative virtual address (RVA).
 *
 * @returns IPRT status code.
 *
 * @param   hLdrMod         The module handle.
 * @param   iSeg            The segment index.
 * @param   offSeg          The segment offset.
 * @param   pRva            Where to return the RVA.
 */
RTDECL(int) RTLdrRvaToSegOffset(RTLDRMOD hLdrMod, RTLDRADDR Rva, uint32_t *piSeg, PRTLDRADDR poffSeg);

/**
 * Gets the image format.
 *
 * @returns Valid image format on success. RTLDRFMT_INVALID on invalid handle or
 *          other errors.
 * @param   hLdrMod         The module handle.
 */
RTDECL(RTLDRFMT) RTLdrGetFormat(RTLDRMOD hLdrMod);

/**
 * Gets the image type.
 *
 * @returns Valid image type value on success. RTLDRTYPE_INVALID on
 *          invalid handle or other errors.
 * @param   hLdrMod         The module handle.
 */
RTDECL(RTLDRTYPE) RTLdrGetType(RTLDRMOD hLdrMod);

/**
 * Gets the image endian-ness.
 *
 * @returns Valid image endian value on success. RTLDRENDIAN_INVALID on invalid
 *          handle or other errors.
 * @param   hLdrMod         The module handle.
 */
RTDECL(RTLDRENDIAN) RTLdrGetEndian(RTLDRMOD hLdrMod);

/**
 * Gets the image endian-ness.
 *
 * @returns Valid image architecture value on success.
 *          RTLDRARCH_INVALID on invalid handle or other errors.
 * @param   hLdrMod         The module handle.
 */
RTDECL(RTLDRARCH) RTLdrGetArch(RTLDRMOD hLdrMod);


RT_C_DECLS_END

/** @} */

#endif

