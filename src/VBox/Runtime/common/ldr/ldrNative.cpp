/* $Id$ */
/** @file
 * innotek Portable Runtime - Binary Image Loader, Native interface.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP RTLOGGROUP_LDR
#include <iprt/ldr.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/log.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/err.h>
#include "internal/ldr.h"


/** @copydoc RTLDROPS::pfnEnumSymbols */
static DECLCALLBACK(int) rtldrNativeEnumSymbols(PRTLDRMODINTERNAL pMod, unsigned fFlags, const void *pvBits, RTUINTPTR BaseAddress,
                                                PFNRTLDRENUMSYMS pfnCallback, void *pvUser)
{
    return VERR_NOT_SUPPORTED;
}


/** @copydoc RTLDROPS::pfnDone */
static DECLCALLBACK(int) rtldrNativeDone(PRTLDRMODINTERNAL pMod)
{
    return VINF_SUCCESS;
}


/**
 * Operations for a native module.
 */
static const RTLDROPS s_rtldrNativeOps =
{
    "native",
    rtldrNativeClose,
    rtldrNativeGetSymbol,
    rtldrNativeDone,
    rtldrNativeEnumSymbols,
    /* ext: */
    NULL,
    NULL,
    NULL,
    NULL,
    42
};



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
 * @param   phLdrMod    Where to store the handle to the loaded module.
 */
RTDECL(int) RTLdrLoad(const char *pszFilename, PRTLDRMOD phLdrMod)
{
    LogFlow(("RTLdrLoad: pszFilename=%p:{%s} phLdrMod=%p\n", pszFilename, pszFilename, phLdrMod));

    /*
     * validate input.
     */
    AssertMsgReturn(VALID_PTR(pszFilename), ("pszFilename=%p\n", pszFilename), VERR_INVALID_PARAMETER);
    AssertMsgReturn(VALID_PTR(phLdrMod), ("phLdrMod=%p\n", phLdrMod), VERR_INVALID_PARAMETER);

    /*
     * Allocate and initialize module structure.
     */
    int rc = VERR_NO_MEMORY;
    PRTLDRMODNATIVE pMod = (PRTLDRMODNATIVE)RTMemAlloc(sizeof(*pMod));
    if (pMod)
    {
        pMod->Core.u32Magic = RTLDRMOD_MAGIC;
        pMod->Core.eState   = LDR_STATE_LOADED;
        pMod->Core.pOps     = &s_rtldrNativeOps;
        pMod->hNative       = ~(uintptr_t)0;

        /*
         * Attempt to open the module.
         */
        rc = rtldrNativeLoad(pszFilename, &pMod->hNative);
        if (RT_SUCCESS(rc))
        {
            *phLdrMod = &pMod->Core;
            LogFlow(("RTLdrLoad: returns %Rrc *phLdrMod=%RTldrm\n", rc, *phLdrMod));
            return rc;
        }
        RTMemFree(pMod);
    }
    *phLdrMod = NIL_RTLDRMOD;
    LogFlow(("RTLdrLoad: returns %Rrc\n", rc));
    return rc;
}


/**
 * Loads a dynamic load library (/shared object) image file using native
 * OS facilities.
 *
 * If the path is specified in the filename, only this path is used.
 * If only the image file name is specified, then try to load it from:
 *   - RTPathAppPrivateArch
 *   - RTPathSharedLibs (legacy)
 *
 * @returns iprt status code.
 * @param   pszFilename Image filename.
 * @param   phLdrMod    Where to store the handle to the loaded module.
 */
RTDECL(int) RTLdrLoadAppSharedLib(const char *pszFilename, PRTLDRMOD phLdrMod)
{
    LogFlow(("RTLdrLoadAppSharedLib: pszFilename=%p:{%s} phLdrMod=%p\n", pszFilename, pszFilename, phLdrMod));

    /*
     * Validate input.
     */
    AssertMsgReturn(VALID_PTR(pszFilename), ("pszFilename=%p\n", pszFilename), VERR_INVALID_PARAMETER);
    AssertMsgReturn(VALID_PTR(phLdrMod), ("phLdrMod=%p\n", phLdrMod), VERR_INVALID_PARAMETER);

    /*
     * If a path is specified, just load the file.
     */
    if (RTPathHavePath(pszFilename))
        return RTLdrLoad(pszFilename, phLdrMod);

    /*
     * By default nothing is found.
     */
    int rc = VERR_FILE_NOT_FOUND;
    *phLdrMod = NIL_RTLDRMOD;

    /*
     * Try default locations.
     */
    int i;
    for (i = 0;; i++)
    {
        /*
         * Get the appropriate base path.
         */
        char szBase[RTPATH_MAX];
        if (i == 0)
            rc = RTPathAppPrivateArch(szBase, sizeof (szBase));
        else if (i == 1)
            rc = RTPathSharedLibs(szBase, sizeof (szBase));
        else
            break;

        if (RT_SUCCESS(rc))
        {
            /*
             * Got the base path. Construct szPath = pszBase + pszFilename
             */
            char szPath[RTPATH_MAX];
            rc = RTPathAbsEx(szBase, pszFilename, szPath, sizeof (szPath));
            if (RT_SUCCESS(rc))
            {
                /*
                 * Finally try to load the image file.
                 */
                rc = RTLdrLoad(szPath, phLdrMod);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Successfully loaded the image file.
                     */
                    LogFlow(("Library loaded: [%s]\n", szPath));
                    break;
                }
            }
        }
    }
    LogFlow(("RTLdrLoadAppSharedLib: returns %Rrc\n", rc));
    return rc;
}
