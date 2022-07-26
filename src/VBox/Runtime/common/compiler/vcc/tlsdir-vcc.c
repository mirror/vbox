/* $Id$ */
/** @file
 * IPRT - Visual C++ Compiler - PE/Windows TLS Directory.
 *
 * @note Doing this as a C file for same reasons as loadcfg-vcc.c.
 */

/*
 * Copyright (C) 2022 Oracle Corporation
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
#define IPRT_COMPILER_VCC_WITH_TLS_CALLBACK_SECTIONS
#define IPRT_COMPILER_VCC_WITH_TLS_DATA_SECTIONS
#include "internal/iprt.h"
#include <iprt/win/windows.h>

#include "internal/compiler-vcc.h"


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** @name TLS callback arrays.
 *
 * The important thing here are the section names, the linker is told to merge
 * and sort all the .CRT* sections into .rdata.
 *
 * @{ */
/** Start of the TLS callback array. */
__declspec(allocate(".CRT$XLA"))    PIMAGE_TLS_CALLBACK     g_apfnRTVccTlsCallbacks_Start[]   = { NULL, };
/** End of the TLS callback array (not actually used, but seems to be
 * traditional). */
__declspec(allocate(".CRT$XLZ"))    PIMAGE_TLS_CALLBACK     g_apfnRTVccTlsCallbacks_End[]     = { NULL, };

/* Tell the linker to merge the .CRT* sections into .rdata */
#pragma comment(linker, "/merge:.CRT=.rdata ")
/** @} */


/** @name TLS data arrays.
 *
 * @{ */
#pragma data_seg(".tls")

/** Start of the TLS data.
 * @note The linker has a reference to name '_tls_start' indicating a possible
 *       required naming convention here.
 * @note Not sure if the byte here is ignored or not...  In assembly we could
 *       optimize it out, I think... */
extern __declspec(allocate(".tls"))      char _tls_start = 0;

/** End of the TLS callback array.
 * @note The linker has a reference to name '_tls_end' indicating a possible
 *       required naming convention here. */
extern __declspec(allocate(".tls$ZZZ"))  char _tls_end   = 0;

#pragma data_seg ()
/** @} */


/** The TLS index for the module we're linked into.
 * The linker has a reference to the name '_tls_start', so this is probably
 * fixed in some way. */
extern ULONG _tls_index = 0;


/**
 * The TLS directory for the PE image.
 *
 * The name of this is dictated by the linker, as it looks for a _tls_used
 * symbol and puts it's address and (somehow) size in the TLS data dir entry.
 */
extern __declspec(".rdata$T") /* seems to be tranditional, doubt it is necessary */
const RT_CONCAT(IMAGE_TLS_DIRECTORY, ARCH_BITS) _tls_used =
{
    /* .StartAddressOfRawData   = */    (uintptr_t)&_tls_start,
    /* .EndAddressOfRawData     = */    (uintptr_t)&_tls_end,
    /* .AddressOfIndex          = */    (uintptr_t)&_tls_index,
    /* .AddressOfCallBacks      = */    (uintptr_t)&g_apfnRTVccTlsCallbacks_Start[1],
    /* .SizeOfZeroFill          = */    0,
    /* .Characteristics         = */    0,
};

