/* $Id$ */
/** @file
 * IPRT - Debug Module Interpreter.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/dbg.h>
#include <iprt/asm.h>
#include <iprt/avl.h>
#include <iprt/string.h>

#include <iprt/mem.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#include <iprt/param.h>
#include "internal/dbgmod.h"
#include "internal/magics.h"


RTDECL(int)         RTDbgModCreate(PRTDBGMOD phDbgMod, const char *pszName, RTUINTPTR cb, uint32_t fFlags)
{
    return VERR_NOT_IMPLEMENTED;
}

RTDECL(int)         RTDbgModCreateFromImage(PRTDBGMOD phDbgMod, const char *pszFilename, const char *pszName, uint32_t fFlags)
{
    return VERR_NOT_IMPLEMENTED;
}

RTDECL(int)         RTDbgModCreateFromMap(PRTDBGMOD phDbgMod, const char *pszFilename, const char *pszName, RTUINTPTR uSubtrahend, uint32_t fFlags)
{
    return VERR_NOT_IMPLEMENTED;
}

RTDECL(int)         RTDbgModDestroy(RTDBGMOD hDbgMod)
{
    return VERR_NOT_IMPLEMENTED;
}

RTDECL(uint32_t)    RTDbgModRetain(RTDBGMOD hDbgMod)
{
    return 0;
}

RTDECL(uint32_t)    RTDbgModRelease(RTDBGMOD hDbgMod)
{
    return 0;
}

RTDECL(const char *) RTDbgModName(RTDBGMOD hDbgMod)
{
    return NULL;
}

RTDECL(RTUINTPTR)   RTDbgModImageSize(RTDBGMOD hDbgMod)
{
    return 1;
}

RTDECL(RTUINTPTR)   RTDbgModSegmentSize(RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg)
{
    return 1;
}

RTDECL(RTDBGSEGIDX) RTDbgModSegmentCount(RTDBGMOD hDbgMod)
{
    return 1;
}

RTDECL(int)         RTDbgModSymbolAdd(RTDBGMOD hDbgMod, const char *pszSymbol, RTDBGSEGIDX iSeg, RTUINTPTR off, uint32_t cb)
{
    return VERR_NOT_IMPLEMENTED;
}

RTDECL(uint32_t)    RTDbgModSymbolCount(RTDBGMOD hDbgMod)
{
    return 1;
}

RTDECL(int)         RTDbgModSymbolByIndex(RTDBGMOD hDbgMod, uint32_t iSymbol, PRTDBGSYMBOL pSymbol)
{
    return VERR_NOT_IMPLEMENTED;
}

RTDECL(int)         RTDbgModSymbolByAddr(RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg, RTUINTPTR off, PRTINTPTR poffDisp, PRTDBGSYMBOL pSymbol)
{
    return VERR_NOT_IMPLEMENTED;
}

RTDECL(int)         RTDbgModSymbolByAddrA(RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg, RTUINTPTR off, PRTINTPTR poffDisp, PRTDBGSYMBOL *ppSymbol)
{
    return VERR_NOT_IMPLEMENTED;
}

RTDECL(int)         RTDbgModSymbolByName(RTDBGMOD hDbgMod, const char *pszSymbol, PRTDBGSYMBOL pSymbol)
{
    return VERR_NOT_IMPLEMENTED;
}

RTDECL(int)         RTDbgModSymbolByNameA(RTDBGMOD hDbgMod, const char *pszSymbol, PRTDBGSYMBOL *ppSymbol)
{
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int)         RTDbgModLineByAddr(RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg, RTUINTPTR off, PRTINTPTR poffDisp, PRTDBGLINE pLine)
{
    return VERR_NOT_IMPLEMENTED;
}

RTDECL(int)         RTDbgModLineByAddrA(RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg, RTUINTPTR off, PRTINTPTR poffDisp, PRTDBGLINE *ppLine)
{
    return VERR_NOT_IMPLEMENTED;
}

