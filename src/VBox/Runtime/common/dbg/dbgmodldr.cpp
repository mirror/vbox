/* $Id$ */
/** @file
 * IPRT - Debug Module Image Interpretation by RTLdr.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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
#include <iprt/dbg.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/ldr.h>
#include <iprt/mem.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include "internal/dbgmod.h"
#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The instance data of the RTLdr based image reader.
 */
typedef struct RTDBGMODLDR
{
    /** The loader handle. */
    RTLDRMOD        hLdrMod;
    /** File handle for the image. */
    RTFILE          hFile;
} RTDBGMODLDR;
/** Pointer to instance data NM map reader. */
typedef RTDBGMODLDR *PRTDBGMODLDR;


/** @interface_method_impl{RTDBGMODVTIMG,pfnUnmapPart} */
static DECLCALLBACK(int) rtDbgModLdr_UnmapPart(PRTDBGMODINT pMod, size_t cb, void const **ppvMap)
{
    RTMemFree((void *)*ppvMap);
    *ppvMap = NULL;
    return VINF_SUCCESS;
}


/** @interface_method_impl{RTDBGMODVTIMG,pfnMapPart} */
static DECLCALLBACK(int) rtDbgModLdr_MapPart(PRTDBGMODINT pMod, RTFOFF off, size_t cb, void const **ppvMap)
{
    PRTDBGMODLDR pThis = (PRTDBGMODLDR)pMod->pvImgPriv;

    void *pvMap = RTMemAlloc(cb);
    if (!pvMap)
        return VERR_NO_MEMORY;

    int rc = RTFileReadAt(pThis->hFile, off, pvMap, cb, NULL);
    if (RT_SUCCESS(rc))
        *ppvMap = pvMap;
    else
    {
        RTMemFree(pvMap);
        *ppvMap = NULL;
    }
    return rc;
}


/** @interface_method_impl{RTDBGMODVTIMG,pfnGetLoadedSize} */
static DECLCALLBACK(RTUINTPTR) rtDbgModLdr_GetLoadedSize(PRTDBGMODINT pMod)
{
    PRTDBGMODLDR pThis = (PRTDBGMODLDR)pMod->pvImgPriv;
    return RTLdrSize(pThis->hLdrMod);
}


/** @interface_method_impl{RTDBGMODVTIMG,pfnEnumSegments} */
static DECLCALLBACK(int) rtDbgModLdr_EnumSegments(PRTDBGMODINT pMod, PFNRTLDRENUMSEGS pfnCallback, void *pvUser)
{
    PRTDBGMODLDR pThis = (PRTDBGMODLDR)pMod->pvImgPriv;
    return RTLdrEnumSegments(pThis->hLdrMod, pfnCallback, pvUser);
}


/** @interface_method_impl{RTDBGMODVTIMG,pfnEnumDbgInfo} */
static DECLCALLBACK(int) rtDbgModLdr_EnumDbgInfo(PRTDBGMODINT pMod, PFNRTLDRENUMDBG pfnCallback, void *pvUser)
{
    PRTDBGMODLDR pThis = (PRTDBGMODLDR)pMod->pvImgPriv;
    return RTLdrEnumDbgInfo(pThis->hLdrMod, NULL, pfnCallback, pvUser);
}


/** @interface_method_impl{RTDBGMODVTIMG,pfnClose} */
static DECLCALLBACK(int) rtDbgModLdr_Close(PRTDBGMODINT pMod)
{
    PRTDBGMODLDR pThis = (PRTDBGMODLDR)pMod->pvImgPriv;
    AssertPtr(pThis);

    int rc = RTLdrClose(pThis->hLdrMod); AssertRC(rc);
    pThis->hLdrMod = NIL_RTLDRMOD;

    rc = RTFileClose(pThis->hFile); AssertRC(rc);
    pThis->hFile = NIL_RTFILE;

    RTMemFree(pThis);

    return VINF_SUCCESS;
}


/** @interface_method_impl{RTDBGMODVTIMG,pfnTryOpen} */
static DECLCALLBACK(int) rtDbgModLdr_TryOpen(PRTDBGMODINT pMod)
{
    RTFILE hFile;
    int rc = RTFileOpen(&hFile, pMod->pszImgFile, RTFILE_O_READ | RTFILE_O_DENY_WRITE | RTFILE_O_OPEN);
    if (RT_SUCCESS(rc))
    {
        RTLDRMOD hLdrMod;
        rc = RTLdrOpen(pMod->pszImgFile, 0 /*fFlags*/, RTLDRARCH_WHATEVER, &hLdrMod);
        if (RT_SUCCESS(rc))
        {
            PRTDBGMODLDR pThis = (PRTDBGMODLDR)RTMemAllocZ(sizeof(RTDBGMODLDR));
            if (pThis)
            {
                pThis->hLdrMod  = hLdrMod;
                pThis->hFile    = hFile;
                pMod->pvImgPriv = pThis;
                return VINF_SUCCESS;
            }

            rc = VERR_NO_MEMORY;
            RTLdrClose(hLdrMod);
        }

        RTFileClose(hFile);
    }
    return rc;
}


/** Virtual function table for the RTLdr based image reader. */
DECL_HIDDEN_CONST(RTDBGMODVTIMG) const g_rtDbgModVtImgLdr =
{
    /*.u32Magic = */            RTDBGMODVTIMG_MAGIC,
    /*.fReserved = */           0,
    /*.pszName = */             "RTLdr",
    /*.pfnTryOpen = */          rtDbgModLdr_TryOpen,
    /*.pfnClose = */            rtDbgModLdr_Close,
    /*.pfnEnumDbgInfo = */      rtDbgModLdr_EnumDbgInfo,
    /*.pfnEnumSegments = */     rtDbgModLdr_EnumSegments,
    /*.pfnGetLoadedSize = */    rtDbgModLdr_GetLoadedSize,
    /*.pfnMapPart = */          rtDbgModLdr_MapPart,
    /*.pfnUnmapPart = */        rtDbgModLdr_UnmapPart,

    /*.u32EndMagic = */         RTDBGMODVTIMG_MAGIC
};


