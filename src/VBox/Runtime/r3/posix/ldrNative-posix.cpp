/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Binary Image Loader, POSIX native.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP RTLOGGROUP_LDR
#include <dlfcn.h>

#include <iprt/ldr.h>
#include <iprt/assert.h>
#include <iprt/path.h>
#include <iprt/alloca.h>
#include <iprt/string.h>
#include <iprt/err.h>
#include <iprt/log.h>
#include "internal/ldr.h"


int rtldrNativeLoad(const char *pszFilename, uintptr_t *phHandle)
{
    /*
     * Do we need to add an extension?
     */
    if (!RTPathHaveExt(pszFilename))
    {
#if defined(__OS2__) || defined(__WIN__)
        static const char s_szSuff[] = ".DLL";
#elif defined(__L4__)
        static const char s_szSuff[] = ".s.so";
#elif defined(__DARWIN__)
        static const char s_szSuff[] = ".dylib";
#else
        static const char s_szSuff[] = ".so";
#endif
        size_t cch = strlen(pszFilename);
        char *psz = (char *)alloca(cch + sizeof(s_szSuff));
        if (!psz)
            return VERR_NO_MEMORY;
        memcpy(psz, pszFilename, cch);
        memcpy(psz + cch, s_szSuff, sizeof(s_szSuff));
        pszFilename = psz;
    }

    /*
     * Attempt load.
     */

    void *pvMod = dlopen(pszFilename, RTLD_NOW | RTLD_LOCAL);
    if (pvMod)
    {
        *phHandle = (uintptr_t)pvMod;
        return VINF_SUCCESS;
    }
    Log(("rtldrNativeLoad: dlopen('%s', RTLD_NOW | RTLD_LOCAL) failed: %s\n", pszFilename, dlerror()));
    return VERR_FILE_NOT_FOUND;
}


DECLCALLBACK(int) rtldrNativeGetSymbol(PRTLDRMODINTERNAL pMod, const char *pszSymbol, void **ppvValue)
{
    PRTLDRMODNATIVE pModNative = (PRTLDRMODNATIVE)pMod;
#ifdef __OS2__
    /* Prefix the symbol with an underscore (assuming __cdecl/gcc-default). */
    size_t cch = strlen(pszSymbol);
    char *psz = (char *)alloca(cch + 2);
    psz[0] = '_';
    memcpy(psz + 1, pszSymbol, cch + 1);
    pszSymbol = psz;
#endif
    *ppvValue = dlsym((void *)pModNative->hNative, pszSymbol);
    if (*ppvValue)
        return VINF_SUCCESS;
    return VERR_SYMBOL_NOT_FOUND;
}


DECLCALLBACK(int) rtldrNativeClose(PRTLDRMODINTERNAL pMod)
{
    PRTLDRMODNATIVE pModNative = (PRTLDRMODNATIVE)pMod;
    if (!dlclose((void *)pModNative->hNative))
    {
        pModNative->hNative = (uintptr_t)0;
        return VINF_SUCCESS;
    }
    Log(("rtldrNativeFree: dlclose(%p) failed: %s\n", pModNative->hNative, dlerror()));
    return VERR_GENERAL_FAILURE;
}

