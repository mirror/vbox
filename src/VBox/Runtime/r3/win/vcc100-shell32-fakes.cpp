/* $Id$ */
/** @file
 * IPRT - Tricks to make the Visual C++ 2010 CRT work on NT4, W2K and XP.
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define RT_NO_STRICT /* Minimal deps so that it works on NT 3.51 too. */
#include <iprt/types.h>
#include <iprt/assert.h>
#include <iprt/string.h>

#ifndef RT_ARCH_X86
# error "This code is X86 only"
#endif

#define CommandLineToArgvW                      Ignore_CommandLineToArgvW

#include <iprt/nt/nt-and-windows.h>

#undef CommandLineToArgvW


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Dynamically resolves an kernel32 API.   */
#define RESOLVE_ME(ApiNm) \
    static decltype(ShellExecuteW) * volatile s_pfnInitialized = NULL; \
    static decltype(ApiNm) *s_pfnApi = NULL; \
    decltype(ApiNm)        *pfnApi; \
    if (s_pfnInitialized) \
        pfnApi = s_pfnApi; \
    else \
    { \
        pfnApi = (decltype(pfnApi))GetProcAddress(GetModuleHandleW(L"shell32"), #ApiNm); \
        s_pfnApi = pfnApi; \
        s_pfnInitialized = ShellExecuteW; /* ensures shell32 is loaded */ \
    } do {} while (0)


/** Declare a shell32 API.
 * @note We are not exporting them as that causes duplicate symbol troubles in
 *       the OpenGL bits. */
#define DECL_SHELL32(a_Type) extern "C" a_Type WINAPI


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/


DECL_SHELL32(LPWSTR *) CommandLineToArgvW(LPCWSTR pwszCmdLine, int *pcArgs)
{
    RESOLVE_ME(CommandLineToArgvW);
    if (pfnApi)
        return pfnApi(pwszCmdLine, pcArgs);

    *pcArgs = 0;
    return NULL;
}


/* Dummy to force dragging in this object in the link, so the linker
   won't accidentally use the symbols from shell32. */
extern "C" int vcc100_shell32_fakes_cpp(void)
{
    return 42;
}

