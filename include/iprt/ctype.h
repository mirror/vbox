/** @file
 * IPRT - ctype.h wrapper and C locale variants.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#ifndef ___iprt_ctype_h
#define ___iprt_ctype_h

#include <iprt/types.h>

#ifdef IN_RING3
# include <ctype.h>

# if defined(_MSC_VER) && !defined(isblank)
/* isblank for MSC */
#  define isblank(ch)  ( (ch) == ' ' || (ch) == '\t' )
# endif
#endif /* IN_RING3 */

/** @name C locale predicates and conversions.
 *
 * For most practical purposes, this can safely be used when parsing UTF-8
 * strings.  Just keep in mind that we only deal with the first 127 chars and
 * that full correctness is only archived using the non-existing RTLocIs* API.
 *
 * @remarks Use the marcros, not the inlined functions.
 *
 * @remarks ASSUMES the source code includes the basic ASCII chars. This is a
 *          general IPRT assumption.
 * @{ */
#define RT_C_IS_BLANK(ch)   RTLocCIsBlank((ch))
#define RT_C_IS_ALNUM(ch)   RTLocCIsAlNum((ch))
#define RT_C_IS_ALPHA(ch)   RTLocCIsAlpha((ch))
#define RT_C_IS_CNTRL(ch)   RTLocCIsCntrl((ch))
#define RT_C_IS_DIGIT(ch)   RTLocCIsDigit((ch))
#define RT_C_IS_LOWER(ch)   RTLocCIsLower((ch))
#define RT_C_IS_GRAPH(ch)   RTLocCIsGraph((ch))
#define RT_C_IS_ODIGIT(ch)  RTLocCIsODigit((ch))
#define RT_C_IS_PRINT(ch)   RTLocCIsPrint((ch))
#define RT_C_IS_PUNCT(ch)   RTLocCIsPunct((ch))
#define RT_C_IS_SPACE(ch)   RTLocCIsSpace((ch))
#define RT_C_IS_UPPER(ch)   RTLocCIsUpper((ch))
#define RT_C_IS_XDIGIT(ch)  RTLocCIsXDigit((ch))

#define RT_C_TO_LOWER(ch)   RTLocCToLower((ch))
#define RT_C_TO_UPPER(ch)   RTLocCToUpper((ch))

/**
 * Checks for a blank character.
 *
 * @returns true / false.
 * @param   ch      The character to test.
 */
DECL_FORCE_INLINE(bool) RTLocCIsBlank(int ch)
{
    return ch == ' ' || ch == '\t';
}

/**
 * Checks for a control character.
 *
 * @returns true / false.
 * @param   ch      The character to test.
 */
DECL_FORCE_INLINE(bool) RTLocCIsCntrl(int ch)
{
    return (ch) >= 0   && (ch) <  32;
}

/**
 * Checks for a decimal digit.
 *
 * @returns true / false.
 * @param   ch      The character to test.
 */
DECL_FORCE_INLINE(bool) RTLocCIsDigit(int ch)
{
    return (ch) >= '0' && (ch) <= '9';
}

/**
 * Checks for a lower case character.
 *
 * @returns true / false.
 * @param   ch      The character to test.
 */
DECL_FORCE_INLINE(bool) RTLocCIsLower(int ch)
{
    return (ch) >= 'a' && (ch) <= 'z';
}

/**
 * Checks for a octal digit.
 *
 * @returns true / false.
 * @param   ch      The character to test.
 */
DECL_FORCE_INLINE(bool) RTLocCIsODigit(int ch)
{
    return (ch) >= '0' && (ch) <= '7';
}

/**
 * Checks for a printable character (whitespace included).
 *
 * @returns true / false.
 * @param   ch      The character to test.
 */
DECL_FORCE_INLINE(bool) RTLocCIsPrint(int ch)
{
    /** @todo quite possibly incorrect */
    return (ch) >= 32  && (ch) < 127;
}

/**
 * Checks for punctuation (?).
 *
 * @returns true / false.
 * @param   ch      The character to test.
 */
DECL_FORCE_INLINE(bool) RTLocCIsPunct(int ch)
{
    /** @todo possibly incorrect */
    return (ch) == ',' || (ch) == '.'  || (ch) == ':'  || (ch) == ';'  || (ch) == '!'  || (ch) == '?';
}

/**
 * Checks for a white-space character.
 *
 * @returns true / false.
 * @param   ch      The character to test.
 */
DECL_FORCE_INLINE(bool) RTLocCIsSpace(int ch)
{
    return (ch) == ' ' || ((ch) >= '\t' && (ch) <= '\f');
}

/**
 * Checks for an upper case character.
 *
 * @returns true / false.
 * @param   ch      The character to test.
 */
DECL_FORCE_INLINE(bool) RTLocCIsUpper(int ch)
{
    return (ch) >= 'A' && (ch) <= 'Z';
}

/**
 * Checks for a hexadecimal digit.
 *
 * @returns true / false.
 * @param   ch      The character to test.
 */
DECL_FORCE_INLINE(bool) RTLocCIsXDigit(int ch)
{
    return RTLocCIsDigit(ch) || ((ch) >= 'a' && (ch) <= 'f') || ((ch) >= 'A' && (ch) <= 'F');
}

/**
 * Checks for an alphabetic character.
 *
 * @returns true / false.
 * @param   ch      The character to test.
 */
DECL_FORCE_INLINE(bool) RTLocCIsAlpha(int ch)
{
    return RTLocCIsLower(ch) || RTLocCIsUpper(ch);
}

/**
 * Checks for an alphanumerical character.
 *
 * @returns true / false.
 * @param   ch      The character to test.
 */
DECL_FORCE_INLINE(bool) RTLocCIsAlNum(int ch)
{
    return RTLocCIsDigit(ch) || RTLocCIsAlpha(ch);
}

/**
 * Checks for a printable character whitespace excluded.
 *
 * @returns true / false.
 * @param   ch      The character to test.
 */
DECL_FORCE_INLINE(bool) RTLocCIsGraph(int ch)
{
    return RTLocCIsPrint(ch) && !RTLocCIsBlank(ch);
}


/**
 * Converts the character to lower case if applictable.
 *
 * @returns lower cased character or ch.
 * @param   ch      The character to test.
 */
DECL_FORCE_INLINE(int) RTLocCToLower(int ch)
{
    return RTLocCIsUpper(ch) ? (ch) + ('a' - 'A') : (ch);
}

/**
 * Converts the character to upper case if applictable.
 *
 * @returns upper cased character or ch.
 * @param   ch      The character to test.
 */
DECL_FORCE_INLINE(int) RTLocCToUpper(int ch)
{
    return RTLocCIsLower(ch) ? (ch) - ('a' - 'A') : (ch);
}


/** @} */

#endif

