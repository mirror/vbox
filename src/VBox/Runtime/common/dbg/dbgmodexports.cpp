/* $Id$ */
/** @file
 * IPRT - Debug Module Using Image Exports.
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
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/log.h>
#include <iprt/string.h>
#include "internal/dbgmod.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct RTDBGMODEXPORTARGS
{
    PRTDBGMODINT    pDbgMod;
    RTLDRADDR       uImageBase;
} RTDBGMODEXPORTARGS;
/** Pointer to an argument package. */
typedef RTDBGMODEXPORTARGS *PRTDBGMODEXPORTARGS;


/** @callback_method_impl{FNRTLDRENUMSYMS,
 *  Copies the symbols over into the container.} */
static DECLCALLBACK(int) rtDbgModExportsAddSymbolCallback(RTLDRMOD hLdrMod, const char *pszSymbol, unsigned uSymbol,
                                                          RTLDRADDR Value, void *pvUser)
{
    PRTDBGMODEXPORTARGS pArgs = (PRTDBGMODEXPORTARGS)pvUser;
    NOREF(hLdrMod);

    if (Value >= pArgs->uImageBase)
    {
        int rc = RTDbgModSymbolAdd(pArgs->pDbgMod, pszSymbol, RTDBGSEGIDX_RVA, Value - pArgs->uImageBase,
                                   0 /*cb*/, 0 /* fFlags */, NULL /*piOrdinal*/);
        Log(("Symbol #%05u %#018x %s [%Rrc]\n", uSymbol, Value, pszSymbol, rc)); NOREF(rc);
    }
    else
        Log(("Symbol #%05u %#018x %s [SKIPPED - INVALID ADDRESS]\n", uSymbol, Value, pszSymbol));
    return VINF_SUCCESS;
}


/** @callback_method_impl{FNRTLDRENUMSEGS,
 *  Copies the segments over into the container.} */
static DECLCALLBACK(int) rtDbgModExportsAddSegmentsCallback(RTLDRMOD hLdrMod, PCRTLDRSEG pSeg, void *pvUser)
{
    PRTDBGMODEXPORTARGS pArgs = (PRTDBGMODEXPORTARGS)pvUser;
    Log(("Segment %.*s: LinkAddress=%#llx RVA=%#llx cb=%#llx\n",
         pSeg->cchName, pSeg->pchName, (uint64_t)pSeg->LinkAddress, (uint64_t)pSeg->RVA, pSeg->cb));
    NOREF(hLdrMod);

    /* Find the best base address for the module. */
    if (    pSeg->LinkAddress != NIL_RTLDRADDR
        &&  (   !pArgs->uImageBase
             || pArgs->uImageBase > pSeg->LinkAddress))
        pArgs->uImageBase = pSeg->LinkAddress;

    /* Make sure the name is terminated before we add it. */
    char *pszName = (char *)pSeg->pchName;
    if (pszName[pSeg->cchName])
    {
        pszName = (char *)alloca(pSeg->cchName + 1);
        memcpy(pszName, pSeg->pchName, pSeg->cchName);
        pszName[pSeg->cchName] = '\0';
    }

    RTLDRADDR cb = RT_MAX(pSeg->cb, pSeg->cbMapped);
    return RTDbgModSegmentAdd(pArgs->pDbgMod, pSeg->RVA, cb, pszName, 0 /*fFlags*/, NULL);
}


/**
 * Creates the debug info side of affairs based on exports and segments found in
 * the image part.
 *
 * The image part must be successfully initialized prior to the call, while the
 * debug bits must not be present of course.
 *
 * @returns IPRT status code
 * @param   pDbgMod             The debug module structure.
 */
DECLHIDDEN(int) rtDbgModCreateForExports(PRTDBGMODINT pDbgMod)
{
    AssertReturn(!pDbgMod->pDbgVt, VERR_DBG_MOD_IPE);
    AssertReturn(pDbgMod->pImgVt, VERR_DBG_MOD_IPE);
    RTUINTPTR cbImage = pDbgMod->pImgVt->pfnImageSize(pDbgMod);
    AssertReturn(cbImage > 0, VERR_DBG_MOD_IPE);

    /*
     * We simply use a container type for this work.
     */
    int rc = rtDbgModContainerCreate(pDbgMod, 0);
    if (RT_FAILURE(rc))
        return rc;
    pDbgMod->fExports = true;

    /*
     * Copy the segments and symbols.
     */
    RTDBGMODEXPORTARGS Args;
    Args.pDbgMod    = pDbgMod;
    Args.uImageBase = 0;
    rc = pDbgMod->pImgVt->pfnEnumSegments(pDbgMod, rtDbgModExportsAddSegmentsCallback, &Args);
    if (RT_SUCCESS(rc))
    {
        rc = pDbgMod->pImgVt->pfnEnumSymbols(pDbgMod, RTLDR_ENUM_SYMBOL_FLAGS_ALL | RTLDR_ENUM_SYMBOL_FLAGS_NO_FWD,
                                             Args.uImageBase ? Args.uImageBase : 0x10000,
                                             rtDbgModExportsAddSymbolCallback, &Args);
        if (RT_FAILURE(rc))
            Log(("rtDbgModCreateForExports: Error during symbol enum: %Rrc\n", rc));
    }
    else
        Log(("rtDbgModCreateForExports: Error during segment enum: %Rrc\n", rc));

    /* This won't fail. */
    if (RT_SUCCESS(rc))
        rc = VINF_SUCCESS;
    else
        rc = -rc; /* Change it into a warning. */
    return rc;
}

