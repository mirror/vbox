/* $Id$ */
/** @file
 * BS3Kit - structures, symbols, macros and stuff.
 */

/*
 * Copyright (C) 2007-2015 Oracle Corporation
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

#ifndef ___bs3kit_h
#define ___bs3kit_h

#ifndef DOXYGEN_RUNNING
# define IN_RING0
#endif
#include <iprt/cdefs.h>
#include <iprt/types.h>
#ifndef DOXYGEN_RUNNING
# undef  IN_RING0
#endif


/** @defgroup grp_bs3kit     BS3Kit
 * @{ */

/** @def BS3_FAR
 * For inidicating far pointers in 16-bit code.
 * Does nothing in 32-bit and 64-bit code. */
/** @def BS3_NEAR
 * For inidicating near pointers in 16-bit code.
 * Does nothing in 32-bit and 64-bit code. */
#ifdef M_I86
# define BS3_FAR            __far
# define BS3_NEAR           __near
#else
# define BS3_FAR
# define BS3_NEAR
#endif

/** @def BS3_CALL
 * The calling convension used by BS3 functions.  */
#if ARCH_BITS != 64
# define BS3_CALL           __cdecl
#elif !defined(_MSC_VER)
# define BS3_CALL           __attribute__((__ms_abi__))
#else
# define BS3_CALL
#endif

/** @def IN_BS3KIT
 * Indicates that we're in the same link job as the BS3Kit code. */
#ifdef DOXYGEN_RUNNING
# define IN_BS3KIT
#endif

/** @def BS3_DECL
 * Declares a BS3Kit function.
 * @param a_Type        The return type. */
#ifdef IN_BS3KIT
# define BS3_DECL(a_Type)   DECLEXPORT(a_Type) BS3_CALL
#else
# define BS3_DECL(a_Type)   DECLIMPORT(a_Type) BS3_CALL
#endif

/**
 * Constructs a common name.
 *
 * Example: BS3_CMN_NM(Bs3Shutdown)
 *
 * @param   a_Name      The name of the function or global variable.
 */
#define BS3_CMN_NM(a_Name)  RT_CONCAT3(a_Name,_c,ARCH_BITS)



/** @defgroup grp_bs3kit_cmn    Common Functions and Data
 *
 * The common functions comes in three variations: 16-bit, 32-bit and 64-bit.
 * Templated code uses the #BS3_CMN_NM macro to mangle the name according to the
 * desired
 *
 * @{
 */

/**
 * Panic, never return.
 *
 * The current implementation will only halt the CPU.
 */
BS3_DECL(void) Bs3Panic_c16(void);
BS3_DECL(void) Bs3Panic_c32(void);              /**< @copydoc Bs3Panic_c16  */
BS3_DECL(void) Bs3Panic_c64(void);              /**< @copydoc Bs3Panic_c16  */
#define Bs3Panic BS3_CMN_NM(Bs3Panic)           /**< Selects #Bs3Panic_c16, #Bs3Panic_c32 or #Bs3Panic_c64. */

/**
 * Shutdown the system, never returns.
 *
 * This currently only works for VMs.  When running on real systems it will
 * just halt the CPU.
 */
BS3_DECL(void) Bs3Shutdown_c16(void);
BS3_DECL(void) Bs3Shutdown_c32(void);           /**< @copydoc Bs3Shutdown_c16 */
BS3_DECL(void) Bs3Shutdown_c64(void);           /**< @copydoc Bs3Shutdown_c16 */
#define Bs3Shutdown BS3_CMN_NM(Bs3Shutdown)     /**< Selects #Bs3Shutdown_c16, #Bs3Shutdown_c32 or #Bs3Shutdown_c64. */

/**
 * Prints a 32-bit unsigned value as hex to the screen.
 *
 * @param   uValue      The 32-bit value.
 */
BS3_DECL(void) Bs3PrintU32_c16(uint32_t uValue); /**< @copydoc Bs3PrintU32_c16 */
BS3_DECL(void) Bs3PrintU32_c32(uint32_t uValue); /**< @copydoc Bs3PrintU32_c16 */
BS3_DECL(void) Bs3PrintU32_c64(uint32_t uValue); /**< @copydoc Bs3PrintU32_c16 */
#define Bs3PrintU32 BS3_CMN_NM(Bs3PrintU32)      /**< Selects #Bs3PrintU32_c16, #Bs3PrintU32_c32 or #Bs3PrintU32_c64. */

/** @} */



/** @defgroup grp_bs3kit_mode   Mode Specific Functions and Data
 *
 * The mode specific functions come in bit count variations and CPU mode
 * variations.  The bs3kit-template-header.h/mac defines the BS3_NM macro to
 * mangle a function or variable name according to the target CPU mode.  In
 * non-templated code, it's common to spell the name out in full.
 *
 * @{
 */


/** @} */

#endif

