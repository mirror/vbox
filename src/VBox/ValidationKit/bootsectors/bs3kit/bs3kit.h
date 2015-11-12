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

/**
 * Template for createing a pointer union type.
 * @param   a_BaseName      The base type name.
 * @param   a_Modifier      The type modifier.
 */
#define BS3_PTR_UNION_TEMPLATE(a_BaseName, a_Modifiers) \
    typedef union a_BaseName \
    { \
        /** Pointer into the void. */ \
        a_Modifiers void BS3_FAR                  *pv; \
        /** As a signed integer. */ \
        intptr_t                                   i; \
        /** As an unsigned integer. */ \
        uintptr_t                                  u; \
        /** Pointer to char value. */ \
        a_Modifiers char BS3_FAR                   *pch; \
        /** Pointer to char value. */ \
        a_Modifiers unsigned char BS3_FAR          *puch; \
        /** Pointer to a int value. */ \
        a_Modifiers int BS3_FAR                    *pi; \
        /** Pointer to a unsigned int value. */ \
        a_Modifiers unsigned int BS3_FAR           *pu; \
        /** Pointer to a long value. */ \
        a_Modifiers long BS3_FAR                   *pl; \
        /** Pointer to a long value. */ \
        a_Modifiers unsigned long BS3_FAR          *pul; \
        /** Pointer to a memory size value. */ \
        a_Modifiers size_t BS3_FAR                 *pcb; \
        /** Pointer to a byte value. */ \
        a_Modifiers uint8_t BS3_FAR                *pb; \
        /** Pointer to a 8-bit unsigned value. */ \
        a_Modifiers uint8_t BS3_FAR                *pu8; \
        /** Pointer to a 16-bit unsigned value. */ \
        a_Modifiers uint16_t BS3_FAR               *pu16; \
        /** Pointer to a 32-bit unsigned value. */ \
        a_Modifiers uint32_t BS3_FAR               *pu32; \
        /** Pointer to a 64-bit unsigned value. */ \
        a_Modifiers uint64_t BS3_FAR               *pu64; \
        /** Pointer to a UTF-16 character. */ \
        a_Modifiers RTUTF16 BS3_FAR                *pwc; \
        /** Pointer to a UUID character. */ \
        a_Modifiers RTUUID BS3_FAR                 *pUuid; \
    } a_BaseName; \
    /** Pointer to a pointer union. */ \
    typedef a_BaseName *RT_CONCAT(P,a_BaseName)
BS3_PTR_UNION_TEMPLATE(BS3PTRUNION, RT_NOTHING);
BS3_PTR_UNION_TEMPLATE(BS3CPTRUNION, const);
BS3_PTR_UNION_TEMPLATE(BS3VPTRUNION, volatile);
BS3_PTR_UNION_TEMPLATE(BS3CVPTRUNION, const volatile);


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
BS3_DECL(void) Bs3Panic_c32(void); /**< @copydoc Bs3Panic_c16  */
BS3_DECL(void) Bs3Panic_c64(void); /**< @copydoc Bs3Panic_c16  */
#define Bs3Panic BS3_CMN_NM(Bs3Panic) /**< Selects #Bs3Panic_c16, #Bs3Panic_c32 or #Bs3Panic_c64. */

/**
 * Shutdown the system, never returns.
 *
 * This currently only works for VMs.  When running on real systems it will
 * just halt the CPU.
 */
BS3_DECL(void) Bs3Shutdown_c16(void);
BS3_DECL(void) Bs3Shutdown_c32(void); /**< @copydoc Bs3Shutdown_c16 */
BS3_DECL(void) Bs3Shutdown_c64(void); /**< @copydoc Bs3Shutdown_c16 */
#define Bs3Shutdown BS3_CMN_NM(Bs3Shutdown) /**< Selects #Bs3Shutdown_c16, #Bs3Shutdown_c32 or #Bs3Shutdown_c64. */

/**
 * Prints a 32-bit unsigned value as hex to the screen.
 *
 * @param   uValue      The 32-bit value.
 */
BS3_DECL(void) Bs3PrintU32_c16(uint32_t uValue); /**< @copydoc Bs3PrintU32_c16 */
BS3_DECL(void) Bs3PrintU32_c32(uint32_t uValue); /**< @copydoc Bs3PrintU32_c16 */
BS3_DECL(void) Bs3PrintU32_c64(uint32_t uValue); /**< @copydoc Bs3PrintU32_c16 */
#define Bs3PrintU32 BS3_CMN_NM(Bs3PrintU32) /**< Selects #Bs3PrintU32_c16, #Bs3PrintU32_c32 or #Bs3PrintU32_c64. */

/**
 * Finds the length of a zero terminated string.
 *
 * @returns String length in chars/bytes.
 * @param   pszString       The string to examine.
 */
BS3_DECL(size_t) Bs3StrLen_c16(const char BS3_FAR *pszString);
BS3_DECL(size_t) Bs3StrLen_c32(const char BS3_FAR *pszString); /** @copydoc Bs3StrLen_c16 */
BS3_DECL(size_t) Bs3StrLen_c64(const char BS3_FAR *pszString); /** @copydoc Bs3StrLen_c16 */
#define Bs3StrLen BS3_CMN_NM(Bs3StrLen) /**< Selects #Bs3StrLen_c16, #Bs3StrLen_c32 or #Bs3StrLen_c64. */

/**
 * Finds the length of a zero terminated string, but with a max length.
 *
 * @returns String length in chars/bytes, or @a cchMax if no zero-terminator
 *           was found before we reached the limit.
 * @param   pszString       The string to examine.
 * @param   cchMax          The max length to examine.
 */
BS3_DECL(size_t) Bs3StrNLen_c16(const char BS3_FAR *pszString, size_t cchMax);
BS3_DECL(size_t) Bs3StrNLen_c32(const char BS3_FAR *pszString, size_t cchMax); /** @copydoc Bs3StrNLen_c16 */
BS3_DECL(size_t) Bs3StrNLen_c64(const char BS3_FAR *pszString, size_t cchMax); /** @copydoc Bs3StrNLen_c16 */
#define Bs3StrNLen BS3_CMN_NM(Bs3StrNLen) /**< Selects #Bs3StrNLen_c16, #Bs3StrNLen_c32 or #Bs3StrNLen_c64. */

/**
 * CRT style unsafe strcpy.
 *
 * @returns pszDst.
 * @param   pszDst          The destination buffer.  Must be large enough to
 *                          hold the source string.
 * @param   pszSrc          The source string.
 */
BS3_DECL(char BS3_FAR *) Bs3StrCpy_c16(char BS3_FAR *pszDst, const char BS3_FAR *pszSrc);
BS3_DECL(char BS3_FAR *) Bs3StrCpy_c32(char BS3_FAR *pszDst, const char BS3_FAR *pszSrc); /** @copydoc Bs3StrCpy_c16 */
BS3_DECL(char BS3_FAR *) Bs3StrCpy_c64(char BS3_FAR *pszDst, const char BS3_FAR *pszSrc); /** @copydoc Bs3StrCpy_c16 */
#define Bs3StrCpy BS3_CMN_NM(Bs3StrCpy) /**< Selects #Bs3StrCpy_c16, #Bs3StrCpy_c32 or #Bs3StrCpy_c64. */

/**
 * CRT style memcpy.
 *
 * @returns pvDst
 * @param   pvDst           The destination buffer.
 * @param   pvSrc           The source buffer.
 * @param   cbCopy          The number of bytes to copy.
 */
BS3_DECL(void BS3_FAR *) Bs3MemCpy_c16(void BS3_FAR *pvDst, const void BS3_FAR *pvSrc, size_t cbToCopy);
BS3_DECL(void BS3_FAR *) Bs3MemCpy_c32(void BS3_FAR *pvDst, const void BS3_FAR *pvSrc, size_t cbToCopy); /** @copydoc Bs3MemCpy_c16 */
BS3_DECL(void BS3_FAR *) Bs3MemCpy_c64(void BS3_FAR *pvDst, const void BS3_FAR *pvSrc, size_t cbToCopy); /** @copydoc Bs3MemCpy_c16 */
#define Bs3MemCpy BS3_CMN_NM(Bs3MemCpy) /**< Selects #Bs3MemCpy_c16, #Bs3MemCpy_c32 or #Bs3MemCpy_c64. */

/**
 * GNU (?) style mempcpy.
 *
 * @returns pvDst + cbCopy
 * @param   pvDst           The destination buffer.
 * @param   pvSrc           The source buffer.
 * @param   cbCopy          The number of bytes to copy.
 */
BS3_DECL(void BS3_FAR *) Bs3MemPCpy_c16(void BS3_FAR *pvDst, const void BS3_FAR *pvSrc, size_t cbToCopy);
BS3_DECL(void BS3_FAR *) Bs3MemPCpy_c32(void BS3_FAR *pvDst, const void BS3_FAR *pvSrc, size_t cbToCopy); /** @copydoc Bs3MemPCpy_c16 */
BS3_DECL(void BS3_FAR *) Bs3MemPCpy_c64(void BS3_FAR *pvDst, const void BS3_FAR *pvSrc, size_t cbToCopy); /** @copydoc Bs3MemPCpy_c16 */
#define Bs3MemPCpy BS3_CMN_NM(Bs3MemPCpy) /**< Selects #Bs3MemPCpy_c16, #Bs3MemPCpy_c32 or #Bs3MemPCpy_c64. */

/**
 * CRT style memmove (overlapping buffers is fine).
 *
 * @returns pvDst
 * @param   pvDst           The destination buffer.
 * @param   pvSrc           The source buffer.
 * @param   cbCopy          The number of bytes to copy.
 */
BS3_DECL(void BS3_FAR *) Bs3MemMove_c16(void BS3_FAR *pvDst, const void BS3_FAR *pvSrc, size_t cbToCopy);
BS3_DECL(void BS3_FAR *) Bs3MemMove_c32(void BS3_FAR *pvDst, const void BS3_FAR *pvSrc, size_t cbToCopy); /** @copydoc Bs3MemMove_c16 */
BS3_DECL(void BS3_FAR *) Bs3MemMove_c64(void BS3_FAR *pvDst, const void BS3_FAR *pvSrc, size_t cbToCopy); /** @copydoc Bs3MemMove_c16 */
#define Bs3MemMove BS3_CMN_NM(Bs3MemMove) /**< Selects #Bs3MemMove_c16, #Bs3MemMove_c32 or #Bs3MemMove_c64. */


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

