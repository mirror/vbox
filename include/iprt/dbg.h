/* $Id$ */
/** @file
 * IPRT - Debugging Routines.
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

#ifndef ___iprt_dbg_h
#define ___iprt_dbg_h

#include <iprt/types.h>

__BEGIN_DECLS

/** @defgroup grp_rt_dbg    RTDbg - Debugging Routines
 * @ingroup grp_rt
 * @{
 */


/** Debug segment index. */
typedef uint32_t            RTDBGSEGIDX;
/** Pointer to a debug segment index. */
typedef RTDBGSEGIDX        *PRTDBGSEGIDX;
/** Pointer to a const debug segment index. */
typedef RTDBGSEGIDX const  *PCRTDBGSEGIDX;
/** NIL debug segment index. */
#define NIL_RTDBGSEGIDX     UINT32_C(0xffffffff)
/** Special segment index that indicates that the offset is a relative
 * virtual address (RVA). I.e. an offset from the start of the module. */
#define RTDBGSEGIDX_RVA     UINT32_C(0xfffffff0)
/** Special segment index that indicates that the offset is a absolute. */
#define RTDBGSEGIDX_ABS     UINT32_C(0xfffffff1)

/** Max length (including '\\0') of a symbol name. */
#define RTDBG_SYMBOL_NAME_LENGTH   (512 - 8 - 4 - 4 - 4)

/**
 * Debug symbol.
 */
typedef struct RTDBGSYMBOL
{
    /** Symbol value (address). */
    RTUINTPTR           Value;
    /** Segment number when applicable or NIL_RTDBGSEGIDX. */
    RTDBGSEGIDX         iSeg;
    /** Symbol size. */
    uint32_t            cb;
    /** Symbol Flags. (reserved). */
    uint32_t            fFlags;
    /** Symbol name. */
    char                szName[RTDBG_SYMBOL_NAME_LENGTH];
} RTDBGSYMBOL;
/** Pointer to debug symbol. */
typedef RTDBGSYMBOL *PRTDBGSYMBOL;
/** Pointer to const debug symbol. */
typedef const RTDBGSYMBOL *PCRTDBGSYMBOL;

RTDECL(PRTDBGSYMBOL) RTDbgSymbolAlloc(void);
RTDECL(PRTDBGSYMBOL) RTDbgSymbolDup(PCRTDBGSYMBOL pSymbol);
RTDECL(void)         RTDbgSymbolFree(PRTDBGSYMBOL pSymbol);


/**
 * Debug line number information.
 */
typedef struct RTDBGLINE
{
    /** Address. */
    RTUINTPTR           Address;
    /** Segment number when applicable or NIL_RTDBGSEGIDX. */
    RTDBGSEGIDX         iSeg;
    /** Line number. */
    uint32_t            uLineNo;
    /** Filename. */
    char                szFilename[260];
} RTDBGLINE;
/** Pointer to debug line number. */
typedef RTDBGLINE *PRTDBGLINE;
/** Pointer to const debug line number. */
typedef const RTDBGLINE *PCRTDBGLINE;

RTDECL(PRTDBGLINE)   RTDbgLineAlloc(void);
RTDECL(PRTDBGLINE)   RTDbgLineDup(PCRTDBGLINE pLine);
RTDECL(void)         RTDbgLineFree(PRTDBGLINE pLine);


/** @defgroup grp_rt_dbgas      RTDbgAs - Debug Address Space
 * @{
 */

/**
 * Creates an empty address space.
 *
 * @returns IPRT status code.
 *
 * @param   phDbgAs         Where to store the address space handle on success.
 * @param   FirstAddr       The first address in the address space.
 * @param   LastAddr        The last address in the address space.
 * @param   pszName         The name of the address space.
 */
RTDECL(int) RTDbgAsCreate(PRTDBGAS phDbgAs, RTUINTPTR FirstAddr, RTUINTPTR LastAddr, const char *pszName);

/**
 * Variant of RTDbgAsCreate that takes a name format string.
 *
 * @returns IPRT status code.
 *
 * @param   phDbgAs         Where to store the address space handle on success.
 * @param   FirstAddr       The first address in the address space.
 * @param   LastAddr        The last address in the address space.
 * @param   pszNameFmt      The name format of the address space.
 * @param   va              Format arguments.
 */
RTDECL(int) RTDbgAsCreateV(PRTDBGAS phDbgAs, RTUINTPTR FirstAddr, RTUINTPTR LastAddr, const char *pszNameFmt, va_list va);

/**
 * Variant of RTDbgAsCreate that takes a name format string.
 *
 * @returns IPRT status code.
 *
 * @param   phDbgAs         Where to store the address space handle on success.
 * @param   FirstAddr       The first address in the address space.
 * @param   LastAddr        The last address in the address space.
 * @param   pszNameFmt      The name format of the address space.
 * @param   ...             Format arguments.
 */
RTDECL(int) RTDbgAsCreateF(PRTDBGAS phDbgAs, RTUINTPTR FirstAddr, RTUINTPTR LastAddr, const char *pszNameFmt, ...);

/**
 * Destroys the address space.
 *
 * This means unlinking all the modules it currently contains, potentially
 * causing some or all of them to be destroyed as they are managed by
 * reference counting.
 *
 * @returns IPRT status code.
 *
 * @param   hDbgAs          The address space handle. A NIL handle will
 *                          be quietly ignored.
 */
RTDECL(int) RTDbgAsDestroy(RTDBGAS hDbgAs);

/**
 * Gets the name of an address space.
 *
 * @returns read only address space name.
 *          NULL if hDbgAs is invalid.
 *
 * @param   hDbgAs          The address space handle.
 */
RTDECL(const char *) RTDbgAsName(RTDBGAS hDbgAs);

/**
 * Gets the first address in an address space.
 *
 * @returns The address.
 *          0 if hDbgAs is invalid.
 *
 * @param   hDbgAs          The address space handle.
 */
RTDECL(RTUINTPTR) RTDbgAsFirstAddr(RTDBGAS hDbgAs);

/**
 * Gets the last address in an address space.
 *
 * @returns The address.
 *          0 if hDbgAs is invalid.
 *
 * @param   hDbgAs          The address space handle.
 */
RTDECL(RTUINTPTR) RTDbgAsLastAddr(RTDBGAS hDbgAs);

/**
 * Gets the number of modules in the address space.
 *
 * This can be used together with RTDbgAsModuleByIndex
 * to enumerate the modules.
 *
 * @returns The number of modules.
 *
 * @param   hDbgAs          The address space handle.
 */
RTDECL(uint32_t) RTDbgAsModuleCount(RTDBGAS hDbgAs);

/**
 * Links a module into the address space at the give address.
 *
 * The size of the mapping is determined using RTDbgModImageSize().
 *
 * @returns IPRT status code.
 * @retval  VERR_OUT_OF_RANGE if the specified address will put the module
 *          outside the address space.
 * @retval  VERR_ADDRESS_CONFLICT if the mapping clashes with existing mappings.
 *
 * @param   hDbgAs          The address space handle.
 * @param   hDbgMod         The module handle of the module to be linked in.
 * @param   ImageAddr       The address to link the module at.
 */
RTDECL(int) RTDbgAsModuleLink(RTDBGAS hDbgAs, RTDBGMOD hDbgMod, RTUINTPTR ImageAddr);

/**
 * Links a segment into the address space at the give address.
 *
 * The size of the mapping is determined using RTDbgModSegmentSize().
 *
 * @returns IPRT status code.
 * @retval  VERR_OUT_OF_RANGE if the specified address will put the module
 *          outside the address space.
 * @retval  VERR_ADDRESS_CONFLICT if the mapping clashes with existing mappings.
 *
 * @param   hDbgAs          The address space handle.
 * @param   hDbgMod         The module handle.
 * @param   iSeg            The segment number (0-based) of the segment to be
 *                          linked in.
 * @param   SegAddr         The address to link the segment at.
 */
RTDECL(int) RTDbgAsModuleLinkSeg(RTDBGAS hDbgAs, RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg, RTUINTPTR SegAddr);

/**
 * Unlinks all the mappings of a module from the address space.
 *
 * @returns IPRT status code.
 * @retval  VERR_NOT_FOUND if the module wasn't found.
 *
 * @param   hDbgAs          The address space handle.
 * @param   hDbgMod         The module handle of the module to be unlinked.
 */
RTDECL(int) RTDbgAsModuleUnlink(RTDBGAS hDbgAs, RTDBGMOD hDbgMod);

/**
 * Unlinks the mapping at the specified address.
 *
 * @returns IPRT status code.
 * @retval  VERR_NOT_FOUND if no module or segment is mapped at that address.
 *
 * @param   hDbgAs          The address space handle.
 * @param   Addr            The address within the mapping to be unlinked.
 */
RTDECL(int) RTDbgAsModuleUnlinkByAddr(RTDBGAS hDbgAs, RTUINTPTR Addr);

/**
 * Get a the handle of a module in the address space by is index.
 *
 * @returns A retained handle to the specified module. The caller must release
 *          the returned reference.
 *          NIL_RTDBGMOD if invalid index or handle.
 *
 * @param   hDbgAs          The address space handle.
 * @param   iModule         The index of the module to get.
 *
 * @remarks The module indexes may change after calls to RTDbgAsModuleLink,
 *          RTDbgAsModuleLinkSeg, RTDbgAsModuleUnlink and
 *          RTDbgAsModuleUnlinkByAddr.
 */
RTDECL(RTDBGMOD) RTDbgAsModuleByIndex(RTDBGAS hDbgAs, uint32_t iModule);

/**
 * Queries mapping module information by handle.
 *
 * @returns IPRT status code.
 * @retval  VERR_NOT_FOUND if no mapping was found at the specified address.
 *
 * @param   hDbgAs          The address space handle.
 * @param   Addr            Address within the mapping of the module or segment.
 * @param   phMod           Where to the return the retained module handle.
 *                          Optional.
 * @param   pAddr           Where to return the base address of the mapping.
 *                          Optional.
 * @param   piSeg           Where to return the segment index. This is set to
 *                          NIL if the entire module is mapped as a single
 *                          mapping. Optional.
 */
RTDECL(int) RTDbgAsModuleByAddr(RTDBGAS hDbgAs, RTUINTPTR Addr, PRTDBGMOD phMod, PRTUINTPTR pAddr, PRTDBGSEGIDX piSeg);

/**
 * Queries mapping module information by name.
 *
 * @returns IPRT status code.
 * @retval  VERR_NOT_FOUND if no mapping was found at the specified address.
 * @retval  VERR_OUT_OF_RANGE if the name index was out of range.
 *
 * @param   hDbgAs          The address space handle.
 * @param   pszName         The module name.
 * @param   iName           There can be more than one module by the same name
 *                          in an address space. This argument indicates which
 *                          is ment. (0 based)
 * @param   phMod           Where to the return the retained module handle.
 */
RTDECL(int) RTDbgAsModuleByName(RTDBGAS hDbgAs, const char *pszName, uint32_t iName, PRTDBGMOD phMod);

/**
 * Adds a symbol to a module in the address space.
 *
 * @returns IPRT status code. See RTDbgModSymbolAdd for more specific ones.
 * @retval  VERR_INVALID_HANDLE if hDbgAs is invalid.
 * @retval  VERR_NOT_FOUND if no module was found at the specified address.
 *
 * @param   hDbgAs          The address space handle.
 * @param   pszSymbol       The symbol name.
 * @param   Addr            The address of the symbol.
 * @param   cb              The size of the symbol.
 */
RTDECL(int) RTDbgAsSymbolAdd(RTDBGAS hDbgAs, const char *pszSymbol, RTUINTPTR Addr, uint32_t cb);

/**
 * Query a symbol by address.
 *
 * @returns IPRT status code. See RTDbgModSymbolAddr for more specific ones.
 * @retval  VERR_INVALID_HANDLE if hDbgAs is invalid.
 * @retval  VERR_NOT_FOUND if the address couldn't be mapped to a module.
 *
 * @param   hDbgAs          The address space handle.
 * @param   Addr            The address which closest symbol is requested.
 * @param   poffDisp        Where to return the distance between the symbol
 *                          and address. Optional.
 * @param   pSymbol         Where to return the symbol info.
 */
RTDECL(int) RTDbgAsSymbolByAddr(RTDBGAS hDbgAs, RTUINTPTR Addr, PRTINTPTR poffDisp, PRTDBGSYMBOL pSymbol);

/**
 * Query a symbol by address.
 *
 * @returns IPRT status code. See RTDbgModSymbolAddrA for more specific ones.
 * @retval  VERR_INVALID_HANDLE if hDbgAs is invalid.
 * @retval  VERR_NOT_FOUND if the address couldn't be mapped to a module.
 *
 * @param   hDbgAs          The address space handle.
 * @param   Addr            The address which closest symbol is requested.
 * @param   poffDisp        Where to return the distance between the symbol
 *                          and address. Optional.
 * @param   ppSymbol        Where to return the pointer to the allocated
 *                          symbol info. Always set. Free with RTDbgSymbolFree.
 */
RTDECL(int) RTDbgAsSymbolByAddrA(RTDBGAS hDbgAs, RTUINTPTR Addr, PRTINTPTR poffDisp, PRTDBGSYMBOL *ppSymbol);

/**
 * Query a symbol by name.
 *
 * @returns IPRT status code.
 * @retval  VERR_SYMBOL_NOT_FOUND if not found.
 *
 * @param   hDbgAs          The address space handle.
 * @param   pszSymbol       The symbol name.
 * @param   pSymbol         Where to return the symbol info.
 */
RTDECL(int) RTDbgAsSymbolByName(RTDBGAS hDbgAs, const char *pszSymbol, PRTDBGSYMBOL pSymbol);

/**
 * Query a symbol by name.
 *
 * @returns IPRT status code.
 * @retval  VERR_SYMBOL_NOT_FOUND if not found.
 *
 * @param   hDbgAs          The address space handle.
 * @param   pszSymbol       The symbol name.
 * @param   ppSymbol        Where to return the pointer to the allocated
 *                          symbol info. Always set. Free with RTDbgSymbolFree.
 */
RTDECL(int) RTDbgAsSymbolByNameA(RTDBGAS hDbgAs, const char *pszSymbol, PRTDBGSYMBOL *ppSymbol);

/**
 * Query a line number by address.
 *
 * @returns IPRT status code. See RTDbgModSymbolAddrA for more specific ones.
 * @retval  VERR_INVALID_HANDLE if hDbgAs is invalid.
 * @retval  VERR_NOT_FOUND if the address couldn't be mapped to a module.
 *
 * @param   hDbgAs          The address space handle.
 * @param   Addr            The address which closest symbol is requested.
 * @param   poffDisp        Where to return the distance between the line
 *                          number and address.
 * @param   pLine           Where to return the line number information.
 */
RTDECL(int) RTDbgAs(RTDBGAS hDbgAs, RTUINTPTR Addr, PRTINTPTR poffDisp, PRTDBGLINE pLine);

/**
 * Query a line number by address.
 *
 * @returns IPRT status code. See RTDbgModSymbolAddrA for more specific ones.
 * @retval  VERR_INVALID_HANDLE if hDbgAs is invalid.
 * @retval  VERR_NOT_FOUND if the address couldn't be mapped to a module.
 *
 * @param   hDbgAs          The address space handle.
 * @param   Addr            The address which closest symbol is requested.
 * @param   poffDisp        Where to return the distance between the line
 *                          number and address.
 * @param   ppLine          Where to return the pointer to the allocated line
 *                          number info. Always set. Free with RTDbgLineFree.
 */
RTDECL(int) RTDbgAsLineByAddrA(RTDBGAS hDbgAs, RTUINTPTR Addr, PRTINTPTR poffDisp, PRTDBGLINE *ppLine);

/** @todo Missing some bits here. */

/** @} */


/** @defgroup grp_rt_dbgmod     RTDbgMod - Debug Module Interpreter
 * @{
 */
RTDECL(int)         RTDbgModCreate(PRTDBGMOD phDbgMod, const char *pszName, const char *pszImgFile, const char *pszDbgFile);
RTDECL(int)         RTDbgModDestroy(RTDBGMOD hDbgMod);
RTDECL(uint32_t)    RTDbgModRetain(RTDBGMOD hDbgMod);
RTDECL(uint32_t)    RTDbgModRelease(RTDBGMOD hDbgMod);
RTDECL(const char *) RTDbgModName(RTDBGMOD hDbgMod);
RTDECL(RTUINTPTR)   RTDbgModImageSize(RTDBGMOD hDbgMod);
RTDECL(RTUINTPTR)   RTDbgModSegmentSize(RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg);
RTDECL(RTDBGSEGIDX) RTDbgModSegmentCount(RTDBGMOD hDbgMod);

RTDECL(int)         RTDbgModSymbolAdd(RTDBGMOD hDbgMod, const char *pszSymbol, RTDBGSEGIDX iSeg, RTUINTPTR off, uint32_t cb);
RTDECL(uint32_t)    RTDbgModSymbolCount(RTDBGMOD hDbgMod);
RTDECL(int)         RTDbgModSymbolByIndex(RTDBGMOD hDbgMod, uint32_t iSymbol, PRTDBGSYMBOL pSymbol);
RTDECL(int)         RTDbgModSymbolByAddr(RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg, RTUINTPTR off, PRTINTPTR poffDisp, PRTDBGSYMBOL pSymbol);
RTDECL(int)         RTDbgModSymbolByAddrA(RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg, RTUINTPTR off, PRTINTPTR poffDisp, PRTDBGSYMBOL *ppSymbol);
RTDECL(int)         RTDbgModSymbolByName(RTDBGMOD hDbgMod, const char *pszSymbol, PRTDBGSYMBOL pSymbol);
RTDECL(int)         RTDbgModSymbolByNameA(RTDBGMOD hDbgMod, const char *pszSymbol, PRTDBGSYMBOL *ppSymbol);

RTDECL(int)         RTDbgModLineByAddr(RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg, RTUINTPTR off, PRTINTPTR poffDisp, PRTDBGLINE pLine);
RTDECL(int)         RTDbgModLineByAddrA(RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg, RTUINTPTR off, PRTINTPTR poffDisp, PRTDBGLINE *ppLine);
/** @} */

/** @} */

__END_DECLS

#endif

