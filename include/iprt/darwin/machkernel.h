/* $Id$ */
/** @file
 * IPRT - mach_kernel symbol resolving hack, R0 Driver, Darwin.
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

#ifndef ___iprt_darwin_machkernel_h
#define ___iprt_darwin_machkernel_h

#include <iprt/types.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_darwin_machkernel  Mach Kernel Symbols (Ring-0 only)
 * @addtogroup grp_rt
 * @{
 */

/** Handle to the mach kernel symbol database (darwin/r0 only). */
typedef R3R0PTRTYPE(struct RTR0MACHKERNELINT *) RTR0MACHKERNEL;
/** Pointer to a mach kernel symbol database handle. */
typedef RTR0MACHKERNEL                         *PRTR0MACHKERNEL;
/** Nil mach kernel symbol database handle. */
#define NIL_RTR0MACHKERNEL                      ((RTR0MACHKERNEL)0)


/**
 * Opens symbol table of the mach_kernel.
 *
 * @returns IPRT status code.
 * @param   pszMachKernel   The path to the mach_kernel image.
 * @param   phKernel        Where to return a mach kernel symbol database
 *                          handle on success.  Call RTR0MachKernelClose on it
 *                          when done.
 */
RTDECL(int) RTR0MachKernelOpen(const char *pszMachKernel, PRTR0MACHKERNEL phKernel);

/**
 * Frees up the internal scratch data when done looking up symbols.
 *
 * @param   pThis               The internal scratch data.
 */
RTDECL(int) RTR0MachKernelClose(RTR0MACHKERNEL hKernel);

/**
 * Looks up a kernel symbol.
 *
 * @returns VINF_SUCCESS if found, *ppvValue set.
 * @retval  VERR_SYMBOL_NOT_FOUND if not found.
 * @retval  VERR_INVALID_HANDLE
 * @retval  VERR_INVALID_POINTER
 *
 * @param   hKernel             The mach kernel handle.
 * @param   pszSymbol           The name of the symbol to look up, omitting the
 *                              leading underscore.
 * @param   ppvValue            Where to store the symbol address (optional).
 */
RTDECL(int) RTR0MachKernelGetSymbol(RTR0MACHKERNEL hKernel, const char *pszSymbol, void **ppvValue);

/** @} */
RT_C_DECLS_END

#endif

