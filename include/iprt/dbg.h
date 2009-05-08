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
RTDECL(int)         RTDbgAsCreate(PRTDBGAS phDbgAs, const char *pszName, RTUINTPTR FirstAddr, RTUINTPTR LastAddr);
RTDECL(int)         RTDbgAsDestroy(PRTDBGAS phDbgAs);

RTDECL(int)         RTDbgAsModuleLink(PRTDBGAS phDbgAs, RTDBGMOD hDbgMod, RTUINTPTR ImageAddr);
RTDECL(int)         RTDbgAsModuleLinkSeg(PRTDBGAS phDbgAs, RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg, RTUINTPTR SegAddr);
RTDECL(int)         RTDbgAsModuleUnlink(PRTDBGAS phDbgAs, RTDBGMOD hDbgMod);
RTDECL(int)         RTDbgAsModuleUnlinkByAddr(PRTDBGAS phDbgAs, RTUINTPTR ImageAddr);
RTDECL(uint32_t)    RTDbgAsModuleCount(PRTDBGAS phDbgAs);
RTDECL(RTDBGMOD)    RTDbgAsModuleByIndex(PRTDBGAS phDbgAs, uint32_t iModule);
RTDECL(RTDBGMOD)    RTDbgAsModuleByName(PRTDBGAS phDbgAs, RTUINTPTR Addr);
RTDECL(RTDBGMOD)    RTDbgAsModuleByAddr(PRTDBGAS phDbgAs, const char *pszName);

RTDECL(int)         RTDbgAsSymbolAdd(RTDBGAS hDbgAs, const char *pszSymbol, RTUINTPTR Addr, uint32_t cb);
RTDECL(int)         RTDbgAsSymbolByName(RTDBGAS hDbgAs, const char *pszSymbol, PRTDBGSYMBOL pSymbol);
RTDECL(int)         RTDbgAsSymbolByNameA(RTDBGAS hDbgAs, const char *pszSymbol, PRTDBGSYMBOL *ppSymbol);
/** @} */


/** @defgroup grp_rt_dbgmod     RTDbgMod - Debug Module Interperter
 * @{
 */
RTDECL(int)         RTDbgModCreate(PRTDBGMOD phDbgMod, const char *pszName, const char *pszImgFile, const char *pszDbgFile);
RTDECL(int)         RTDbgModDestroy(RTDBGMOD hDbgMod);
RTDECL(int)         RTDbgModRetain(RTDBGMOD hDbgMod);
RTDECL(int)         RTDbgModRelease(RTDBGMOD hDbgMod);

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

