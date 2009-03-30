/** @file
 * IPRT - String Manipluation.
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

#ifndef ___iprt_string_h
#define ___iprt_string_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/stdarg.h>
#include <iprt/err.h> /* for VINF_SUCCESS */
#if defined(RT_OS_LINUX) && defined(__KERNEL__)
# include <linux/string.h>
#elif defined(RT_OS_FREEBSD) && defined(_KERNEL)
  /*
   * Kludge for the FreeBSD kernel:
   *  Some of the string.h stuff clashes with sys/libkern.h, so just wrap
   *  it up while including string.h to keep things quiet. It's nothing
   *  important that's clashing, after all.
   */
# define strdup strdup_string_h
# include <string.h>
# undef strdup
#elif defined(RT_OS_SOLARIS) && defined(_KERNEL)
  /*
   * Same case as with FreeBSD kernel:
   * The string.h stuff clashes with sys/system.h
   * ffs = find first set bit.
   */
# define ffs ffs_string_h
# include <string.h>
# undef ffs
# undef strpbrk
#else
# include <string.h>
#endif

/*
 * Supply prototypes for standard string functions provided by
 * IPRT instead of the operating environment.
 */
#if defined(RT_OS_DARWIN) && defined(KERNEL)
__BEGIN_DECLS
void *memchr(const void *pv, int ch, size_t cb);
char *strpbrk(const char *pszStr, const char *pszChars);
__END_DECLS
#endif

/**
 * Byte zero the specified object.
 *
 * This will use sizeof(Obj) to figure the size and will call memset, bzero
 * or some compiler intrinsic to perform the actual zeroing.
 *
 * @param   Obj     The object to zero. Make sure to dereference pointers.
 *
 * @remarks Because the macro may use memset it has been placed in string.h
 *          instead of cdefs.h to avoid build issues because someone forgot
 *          to include this header.
 *
 * @ingroup grp_rt_cdefs
 */
#define RT_ZERO(Obj)        RT_BZERO(&(Obj), sizeof(Obj))

/**
 * Byte zero the specified memory area.
 *
 * This will call memset, bzero or some compiler intrinsic to clear the
 * specified bytes of memory.
 *
 * @param   pv          Pointer to the memory.
 * @param   cb          The number of bytes to clear. Please, don't pass 0.
 *
 * @remarks Because the macro may use memset it has been placed in string.h
 *          instead of cdefs.h to avoid build issues because someone forgot
 *          to include this header.
 *
 * @ingroup grp_rt_cdefs
 */
#define RT_BZERO(pv, cb)    do { memset((pv), 0, cb); } while (0)


/** @defgroup grp_rt_str    RTStr - String Manipulation
 * Mostly UTF-8 related helpers where the standard string functions won't do.
 * @ingroup grp_rt
 * @{
 */

__BEGIN_DECLS


/**
 * The maximum string length.
 */
#define RTSTR_MAX       (~(size_t)0)


#ifdef IN_RING3

/**
 * Allocates tmp buffer, translates pszString from UTF8 to current codepage.
 *
 * @returns iprt status code.
 * @param   ppszString      Receives pointer of allocated native CP string.
 *                          The returned pointer must be freed using RTStrFree().
 * @param   pszString       UTF-8 string to convert.
 */
RTR3DECL(int)  RTStrUtf8ToCurrentCP(char **ppszString, const char *pszString);

/**
 * Allocates tmp buffer, translates pszString from current codepage to UTF-8.
 *
 * @returns iprt status code.
 * @param   ppszString      Receives pointer of allocated UTF-8 string.
 *                          The returned pointer must be freed using RTStrFree().
 * @param   pszString       Native string to convert.
 */
RTR3DECL(int)  RTStrCurrentCPToUtf8(char **ppszString, const char *pszString);

#endif

/**
 * Free string allocated by any of the non-UCS-2 string functions.
 *
 * @returns iprt status code.
 * @param   pszString      Pointer to buffer with string to free.
 *                         NULL is accepted.
 */
RTDECL(void)  RTStrFree(char *pszString);

/**
 * Allocates a new copy of the given UTF-8 string.
 *
 * @returns Pointer to the allocated UTF-8 string.
 * @param   pszString       UTF-8 string to duplicate.
 */
RTDECL(char *) RTStrDup(const char *pszString);

/**
 * Allocates a new copy of the given UTF-8 string.
 *
 * @returns iprt status code.
 * @param   ppszString      Receives pointer of the allocated UTF-8 string.
 *                          The returned pointer must be freed using RTStrFree().
 * @param   pszString       UTF-8 string to duplicate.
 */
RTDECL(int)  RTStrDupEx(char **ppszString, const char *pszString);

/**
 * Validates the UTF-8 encoding of the string.
 *
 * @returns iprt status code.
 * @param   psz         The string.
 */
RTDECL(int) RTStrValidateEncoding(const char *psz);

/** @name Flags for RTStrValidateEncodingEx
 */
/** Check that the string is zero terminated within the given size.
 * VERR_BUFFER_OVERFLOW will be returned if the check fails. */
#define RTSTR_VALIDATE_ENCODING_ZERO_TERMINATED     RT_BIT_32(0)
/** @} */

/**
 * Validates the UTF-8 encoding of the string.
 *
 * @returns iprt status code.
 * @param   psz         The string.
 * @param   cch         The max string length. Use RTSTR_MAX to process the entire string.
 * @param   fFlags      Reserved for future. Pass 0.
 */
RTDECL(int) RTStrValidateEncodingEx(const char *psz, size_t cch, uint32_t fFlags);

/**
 * Checks if the UTF-8 encoding is valid.
 *
 * @returns true / false.
 * @param   psz         The string.
 */
RTDECL(bool) RTStrIsValidEncoding(const char *psz);

/**
 * Gets the number of code points the string is made up of, excluding
 * the terminator.
 *
 *
 * @returns Number of code points (RTUNICP).
 * @returns 0 if the string was incorrectly encoded.
 * @param   psz         The string.
 */
RTDECL(size_t) RTStrUniLen(const char *psz);

/**
 * Gets the number of code points the string is made up of, excluding
 * the terminator.
 *
 * This function will validate the string, and incorrectly encoded UTF-8
 * strings will be rejected.
 *
 * @returns iprt status code.
 * @param   psz         The string.
 * @param   cch         The max string length. Use RTSTR_MAX to process the entire string.
 * @param   pcuc        Where to store the code point count.
 *                      This is undefined on failure.
 */
RTDECL(int) RTStrUniLenEx(const char *psz, size_t cch, size_t *pcuc);

/**
 * Translate a UTF-8 string into an unicode string (i.e. RTUNICPs), allocating the string buffer.
 *
 * @returns iprt status code.
 * @param   pszString       UTF-8 string to convert.
 * @param   ppUniString     Receives pointer to the allocated unicode string.
 *                          The returned string must be freed using RTUniFree().
 */
RTDECL(int) RTStrToUni(const char *pszString, PRTUNICP *ppUniString);

/**
 * Translates pszString from UTF-8 to an array of code points, allocating the result
 * array if requested.
 *
 * @returns iprt status code.
 * @param   pszString       UTF-8 string to convert.
 * @param   cchString       The maximum size in chars (the type) to convert. The conversion stop
 *                          when it reaches cchString or the string terminator ('\\0').
 *                          Use RTSTR_MAX to translate the entire string.
 * @param   ppaCps          If cCps is non-zero, this must either be pointing to pointer to
 *                          a buffer of the specified size, or pointer to a NULL pointer.
 *                          If *ppusz is NULL or cCps is zero a buffer of at least cCps items
 *                          will be allocated to hold the translated string.
 *                          If a buffer was requested it must be freed using RTUtf16Free().
 * @param   cCps            The number of code points in the unicode string. This includes the terminator.
 * @param   pcCps           Where to store the length of the translated string. (Optional)
 *                          This field will be updated even on failure, however the value is only
 *                          specified for the following two error codes. On VERR_BUFFER_OVERFLOW
 *                          and VERR_NO_STR_MEMORY it contains the required buffer space.
 */
RTDECL(int)  RTStrToUniEx(const char *pszString, size_t cchString, PRTUNICP *ppaCps, size_t cCps, size_t *pcCps);

/**
 * Calculates the length of the string in RTUTF16 items.
 *
 * This function will validate the string, and incorrectly encoded UTF-8
 * strings will be rejected. The primary purpose of this function is to
 * help allocate buffers for RTStrToUtf16Ex of the correct size. For most
 * other purposes RTStrCalcUtf16LenEx() should be used.
 *
 * @returns Number of RTUTF16 items.
 * @returns 0 if the string was incorrectly encoded.
 * @param   psz         The string.
 */
RTDECL(size_t) RTStrCalcUtf16Len(const char *psz);

/**
 * Calculates the length of the string in RTUTF16 items.
 *
 * This function will validate the string, and incorrectly encoded UTF-8
 * strings will be rejected.
 *
 * @returns iprt status code.
 * @param   psz         The string.
 * @param   cch         The max string length. Use RTSTR_MAX to process the entire string.
 * @param   pcwc        Where to store the string length. Optional.
 *                      This is undefined on failure.
 */
RTDECL(int) RTStrCalcUtf16LenEx(const char *psz, size_t cch, size_t *pcwc);

/**
 * Translate a UTF-8 string into a UTF-16 allocating the result buffer.
 *
 * @returns iprt status code.
 * @param   pszString       UTF-8 string to convert.
 * @param   ppwszString     Receives pointer to the allocated UTF-16 string.
 *                          The returned string must be freed using RTUtf16Free().
 */
RTDECL(int) RTStrToUtf16(const char *pszString, PRTUTF16 *ppwszString);

/**
 * Translates pszString from UTF-8 to UTF-16, allocating the result buffer if requested.
 *
 * @returns iprt status code.
 * @param   pszString       UTF-8 string to convert.
 * @param   cchString       The maximum size in chars (the type) to convert. The conversion stop
 *                          when it reaches cchString or the string terminator ('\\0').
 *                          Use RTSTR_MAX to translate the entire string.
 * @param   ppwsz           If cwc is non-zero, this must either be pointing to pointer to
 *                          a buffer of the specified size, or pointer to a NULL pointer.
 *                          If *ppwsz is NULL or cwc is zero a buffer of at least cwc items
 *                          will be allocated to hold the translated string.
 *                          If a buffer was requested it must be freed using RTUtf16Free().
 * @param   cwc             The buffer size in RTUTF16s. This includes the terminator.
 * @param   pcwc            Where to store the length of the translated string. (Optional)
 *                          This field will be updated even on failure, however the value is only
 *                          specified for the following two error codes. On VERR_BUFFER_OVERFLOW
 *                          and VERR_NO_STR_MEMORY it contains the required buffer space.
 */
RTDECL(int)  RTStrToUtf16Ex(const char *pszString, size_t cchString, PRTUTF16 *ppwsz, size_t cwc, size_t *pcwc);


/**
 * Get the unicode code point at the given string position.
 *
 * @returns unicode code point.
 * @returns RTUNICP_INVALID if the encoding is invalid.
 * @param   psz         The string.
 */
RTDECL(RTUNICP) RTStrGetCpInternal(const char *psz);

/**
 * Get the unicode code point at the given string position.
 *
 * @returns iprt status code
 * @returns VERR_INVALID_UTF8_ENCODING if the encoding is invalid.
 * @param   ppsz        The string.
 * @param   pCp         Where to store the unicode code point.
 *                      Stores RTUNICP_INVALID if the encoding is invalid.
 */
RTDECL(int) RTStrGetCpExInternal(const char **ppsz, PRTUNICP pCp);

/**
 * Get the unicode code point at the given string position for a string of a
 * given length.
 *
 * @returns iprt status code
 * @retval  VERR_INVALID_UTF8_ENCODING if the encoding is invalid.
 * @retval  VERR_END_OF_STRING if *pcch is 0. *pCp is set to RTUNICP_INVALID.
 *
 * @param   ppsz        The string.
 * @param   pcch        Pointer to the length of the string.  This will be
 *                      decremented by the size of the code point.
 * @param   pCp         Where to store the unicode code point.
 *                      Stores RTUNICP_INVALID if the encoding is invalid.
 */
RTDECL(int) RTStrGetCpNExInternal(const char **ppsz, size_t *pcch, PRTUNICP pCp);

/**
 * Put the unicode code point at the given string position
 * and return the pointer to the char following it.
 *
 * This function will not consider anything at or following the
 * buffer area pointed to by psz. It is therefore not suitable for
 * inserting code points into a string, only appending/overwriting.
 *
 * @returns pointer to the char following the written code point.
 * @param   psz         The string.
 * @param   CodePoint   The code point to write.
 *                      This should not be RTUNICP_INVALID or any other
 *                      character out of the UTF-8 range.
 *
 * @remark  This is a worker function for RTStrPutCp().
 *
 */
RTDECL(char *) RTStrPutCpInternal(char *psz, RTUNICP CodePoint);

/**
 * Get the unicode code point at the given string position.
 *
 * @returns unicode code point.
 * @returns RTUNICP_INVALID if the encoding is invalid.
 * @param   psz         The string.
 *
 * @remark  We optimize this operation by using an inline function for
 *          the most frequent and simplest sequence, the rest is
 *          handled by RTStrGetCpInternal().
 */
DECLINLINE(RTUNICP) RTStrGetCp(const char *psz)
{
    const unsigned char uch = *(const unsigned char *)psz;
    if (!(uch & RT_BIT(7)))
        return uch;
    return RTStrGetCpInternal(psz);
}

/**
 * Get the unicode code point at the given string position.
 *
 * @returns iprt status code.
 * @param   ppsz        Pointer to the string pointer. This will be updated to
 *                      point to the char following the current code point.
 * @param   pCp         Where to store the code point.
 *                      RTUNICP_INVALID is stored here on failure.
 *
 * @remark  We optimize this operation by using an inline function for
 *          the most frequent and simplest sequence, the rest is
 *          handled by RTStrGetCpExInternal().
 */
DECLINLINE(int) RTStrGetCpEx(const char **ppsz, PRTUNICP pCp)
{
    const unsigned char uch = **(const unsigned char **)ppsz;
    if (!(uch & RT_BIT(7)))
    {
        (*ppsz)++;
        *pCp = uch;
        return VINF_SUCCESS;
    }
    return RTStrGetCpExInternal(ppsz, pCp);
}

/**
 * Get the unicode code point at the given string position for a string of a
 * given maximum length.
 *
 * @returns iprt status code.
 * @retval  VERR_INVALID_UTF8_ENCODING if the encoding is invalid.
 * @retval  VERR_END_OF_STRING if *pcch is 0. *pCp is set to RTUNICP_INVALID.
 *
 * @param   ppsz        Pointer to the string pointer. This will be updated to
 *                      point to the char following the current code point.
 * @param   pcch        Pointer to the maximum string length.  This will be
 *                      decremented by the size of the code point found.
 * @param   pCp         Where to store the code point.
 *                      RTUNICP_INVALID is stored here on failure.
 *
 * @remark  We optimize this operation by using an inline function for
 *          the most frequent and simplest sequence, the rest is
 *          handled by RTStrGetCpNExInternal().
 */
DECLINLINE(int) RTStrGetCpNEx(const char **ppsz, size_t *pcch, PRTUNICP pCp)
{
    if (RT_LIKELY(*pcch != 0))
    {
        const unsigned char uch = **(const unsigned char **)ppsz;
        if (!(uch & RT_BIT(7)))
        {
            (*ppsz)++;
            (*pcch)--;
            *pCp = uch;
            return VINF_SUCCESS;
        }
    }
    return RTStrGetCpNExInternal(ppsz, pcch, pCp);
}

/**
 * Put the unicode code point at the given string position
 * and return the pointer to the char following it.
 *
 * This function will not consider anything at or following the
 * buffer area pointed to by psz. It is therefore not suitable for
 * inserting code points into a string, only appending/overwriting.
 *
 * @returns pointer to the char following the written code point.
 * @param   psz         The string.
 * @param   CodePoint   The code point to write.
 *                      This should not be RTUNICP_INVALID or any other
 *                      character out of the UTF-8 range.
 *
 * @remark  We optimize this operation by using an inline function for
 *          the most frequent and simplest sequence, the rest is
 *          handled by RTStrPutCpInternal().
 */
DECLINLINE(char *) RTStrPutCp(char *psz, RTUNICP CodePoint)
{
    if (CodePoint < 0x80)
    {
        *psz++ = (unsigned char)CodePoint;
        return psz;
    }
    return RTStrPutCpInternal(psz, CodePoint);
}

/**
 * Skips ahead, past the current code point.
 *
 * @returns Pointer to the char after the current code point.
 * @param   psz     Pointer to the current code point.
 * @remark  This will not move the next valid code point, only past the current one.
 */
DECLINLINE(char *) RTStrNextCp(const char *psz)
{
    RTUNICP Cp;
    RTStrGetCpEx(&psz, &Cp);
    return (char *)psz;
}

/**
 * Skips back to the previous code point.
 *
 * @returns Pointer to the char before the current code point.
 * @returns pszStart on failure.
 * @param   pszStart    Pointer to the start of the string.
 * @param   psz         Pointer to the current code point.
 */
RTDECL(char *) RTStrPrevCp(const char *pszStart, const char *psz);



#ifndef DECLARED_FNRTSTROUTPUT          /* duplicated in iprt/log.h */
#define DECLARED_FNRTSTROUTPUT
/**
 * Output callback.
 *
 * @returns number of bytes written.
 * @param   pvArg       User argument.
 * @param   pachChars   Pointer to an array of utf-8 characters.
 * @param   cbChars     Number of bytes in the character array pointed to by pachChars.
 */
typedef DECLCALLBACK(size_t) FNRTSTROUTPUT(void *pvArg, const char *pachChars, size_t cbChars);
/** Pointer to callback function. */
typedef FNRTSTROUTPUT *PFNRTSTROUTPUT;
#endif

/** Format flag.
 * These are used by RTStrFormat extensions and RTStrFormatNumber, mind
 * that not all flags makes sense to both of the functions.
 * @{ */
#define RTSTR_F_CAPITAL    0x0001
#define RTSTR_F_LEFT       0x0002
#define RTSTR_F_ZEROPAD    0x0004
#define RTSTR_F_SPECIAL    0x0008
#define RTSTR_F_VALSIGNED  0x0010
#define RTSTR_F_PLUS       0x0020
#define RTSTR_F_BLANK      0x0040
#define RTSTR_F_WIDTH      0x0080
#define RTSTR_F_PRECISION  0x0100

#define RTSTR_F_BIT_MASK   0xf800
#define RTSTR_F_8BIT       0x0800
#define RTSTR_F_16BIT      0x1000
#define RTSTR_F_32BIT      0x2000
#define RTSTR_F_64BIT      0x4000
#define RTSTR_F_128BIT     0x8000
/** @} */

/** @def RTSTR_GET_BIT_FLAG
 * Gets the bit flag for the specified type.
 */
#define RTSTR_GET_BIT_FLAG(type) \
    ( sizeof(type) == 32 ? RTSTR_F_32BIT \
    : sizeof(type) == 64 ? RTSTR_F_64BIT \
    : sizeof(type) == 16 ? RTSTR_F_16BIT \
    : sizeof(type) == 8  ? RTSTR_F_8BIT \
    : sizeof(type) == 128? RTSTR_F_128BIT \
    : 0)


/**
 * Callback to format non-standard format specifiers.
 *
 * @returns The number of bytes formatted.
 * @param   pvArg           Formatter argument.
 * @param   pfnOutput       Pointer to output function.
 * @param   pvArgOutput     Argument for the output function.
 * @param   ppszFormat      Pointer to the format string pointer. Advance this till the char
 *                          after the format specifier.
 * @param   pArgs           Pointer to the argument list. Use this to fetch the arguments.
 * @param   cchWidth        Format Width. -1 if not specified.
 * @param   cchPrecision    Format Precision. -1 if not specified.
 * @param   fFlags          Flags (RTSTR_NTFS_*).
 * @param   chArgSize       The argument size specifier, 'l' or 'L'.
 */
typedef DECLCALLBACK(size_t) FNSTRFORMAT(void *pvArg, PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                         const char **ppszFormat, va_list *pArgs, int cchWidth,
                                         int cchPrecision, unsigned fFlags, char chArgSize);
/** Pointer to a FNSTRFORMAT() function. */
typedef FNSTRFORMAT *PFNSTRFORMAT;


/**
 * Partial implementation of a printf like formatter.
 * It doesn't do everything correct, and there is no floating point support.
 * However, it supports custom formats by the means of a format callback.
 *
 * @returns number of bytes formatted.
 * @param   pfnOutput   Output worker.
 *                      Called in two ways. Normally with a string and its length.
 *                      For termination, it's called with NULL for string, 0 for length.
 * @param   pvArgOutput Argument to the output worker.
 * @param   pfnFormat   Custom format worker.
 * @param   pvArgFormat Argument to the format worker.
 * @param   pszFormat   Format string pointer.
 * @param   InArgs      Argument list.
 */
RTDECL(size_t) RTStrFormatV(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput, PFNSTRFORMAT pfnFormat, void *pvArgFormat, const char *pszFormat, va_list InArgs);

/**
 * Partial implementation of a printf like formatter.
 * It doesn't do everything correct, and there is no floating point support.
 * However, it supports custom formats by the means of a format callback.
 *
 * @returns number of bytes formatted.
 * @param   pfnOutput   Output worker.
 *                      Called in two ways. Normally with a string and its length.
 *                      For termination, it's called with NULL for string, 0 for length.
 * @param   pvArgOutput Argument to the output worker.
 * @param   pfnFormat   Custom format worker.
 * @param   pvArgFormat Argument to the format worker.
 * @param   pszFormat   Format string.
 * @param   ...         Argument list.
 */
RTDECL(size_t) RTStrFormat(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput, PFNSTRFORMAT pfnFormat, void *pvArgFormat, const char *pszFormat, ...);

/**
 * Formats an integer number according to the parameters.
 *
 * @returns Length of the formatted number.
 * @param   psz            Pointer to output string buffer of sufficient size.
 * @param   u64Value       Value to format.
 * @param   uiBase         Number representation base.
 * @param   cchWidth       Width.
 * @param   cchPrecision   Precision.
 * @param   fFlags         Flags (NTFS_*).
 */
RTDECL(int) RTStrFormatNumber(char *psz, uint64_t u64Value, unsigned int uiBase, signed int cchWidth, signed int cchPrecision, unsigned int fFlags);


/**
 * Callback for formatting a type.
 *
 * This is registered using the RTStrFormatTypeRegister function and will
 * be called during string formatting to handle the specified %R[type].
 * The argument for this format type is assumed to be a pointer and it's
 * passed in the @a pvValue argument.
 *
 * @returns Length of the formatted output.
 * @param   pfnOutput       Output worker.
 * @param   pvArgOutput     Argument to the output worker.
 * @param   pszType         The type name.
 * @param   pvValue         The argument value.
 * @param   cchWidth        Width.
 * @param   cchPrecision    Precision.
 * @param   fFlags          Flags (NTFS_*).
 * @param   pvUser          The user argument.
 */
typedef DECLCALLBACK(size_t) FNRTSTRFORMATTYPE(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                               const char *pszType, void const *pvValue,
                                               int cchWidth, int cchPrecision, unsigned fFlags,
                                               void *pvUser);
/** Pointer to a FNRTSTRFORMATTYPE. */
typedef FNRTSTRFORMATTYPE *PFNRTSTRFORMATTYPE;


/**
 * Register a format handler for a type.
 *
 * The format handler is used to handle '%R[type]' format types, where the argument
 * in the vector is a pointer value (a bit restrictive, but keeps it simple).
 *
 * The caller must ensure that no other thread will be making use of any of
 * the dynamic formatting type facilities simultaneously with this call.
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_ALREADY_EXISTS if the type has already been registered.
 * @retval  VERR_TOO_MANY_OPEN_FILES if all the type slots has been allocated already.
 *
 * @param   pszType         The type name.
 * @param   pfnHandler      The handler address. See FNRTSTRFORMATTYPE for details.
 * @param   pvUser          The user argument to pass to the handler. See RTStrFormatTypeSetUser
 *                          for how to update this later.
 */
RTDECL(int) RTStrFormatTypeRegister(const char *pszType, PFNRTSTRFORMATTYPE pfnHandler, void *pvUser);

/**
 * Deregisters a format type.
 *
 * The caller must ensure that no other thread will be making use of any of
 * the dynamic formatting type facilities simultaneously with this call.
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_FILE_NOT_FOUND if not found.
 *
 * @param   pszType     The type to deregister.
 */
RTDECL(int) RTStrFormatTypeDeregister(const char *pszType);

/**
 * Sets the user argument for a type.
 *
 * This can be used if a user argument needs relocating in GC.
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_FILE_NOT_FOUND if not found.
 *
 * @param   pszType     The type to update.
 * @param   pvUser      The new user argument value.
 */
RTDECL(int) RTStrFormatTypeSetUser(const char *pszType, void *pvUser);


/**
 * String printf.
 *
 * @returns The length of the returned string (in pszBuffer).
 * @param   pszBuffer   Output buffer.
 * @param   cchBuffer   Size of the output buffer.
 * @param   pszFormat   The format string.
 * @param   args        The format argument.
 */
RTDECL(size_t) RTStrPrintfV(char *pszBuffer, size_t cchBuffer, const char *pszFormat, va_list args);

/**
 * String printf.
 *
 * @returns The length of the returned string (in pszBuffer).
 * @param   pszBuffer   Output buffer.
 * @param   cchBuffer   Size of the output buffer.
 * @param   pszFormat   The format string.
 * @param   ...         The format argument.
 */
RTDECL(size_t) RTStrPrintf(char *pszBuffer, size_t cchBuffer, const char *pszFormat, ...);


/**
 * String printf with custom formatting.
 *
 * @returns The length of the returned string (in pszBuffer).
 * @param   pfnFormat   Pointer to handler function for the custom formats.
 * @param   pvArg       Argument to the pfnFormat function.
 * @param   pszBuffer   Output buffer.
 * @param   cchBuffer   Size of the output buffer.
 * @param   pszFormat   The format string.
 * @param   args        The format argument.
 */
RTDECL(size_t) RTStrPrintfExV(PFNSTRFORMAT pfnFormat, void *pvArg, char *pszBuffer, size_t cchBuffer, const char *pszFormat, va_list args);

/**
 * String printf with custom formatting.
 *
 * @returns The length of the returned string (in pszBuffer).
 * @param   pfnFormat   Pointer to handler function for the custom formats.
 * @param   pvArg       Argument to the pfnFormat function.
 * @param   pszBuffer   Output buffer.
 * @param   cchBuffer   Size of the output buffer.
 * @param   pszFormat   The format string.
 * @param   ...         The format argument.
 */
RTDECL(size_t) RTStrPrintfEx(PFNSTRFORMAT pfnFormat, void *pvArg, char *pszBuffer, size_t cchBuffer, const char *pszFormat, ...);


/**
 * Allocating string printf.
 *
 * @returns The length of the string in the returned *ppszBuffer.
 * @returns -1 on failure.
 * @param   ppszBuffer  Where to store the pointer to the allocated output buffer.
 *                      The buffer should be freed using RTStrFree().
 *                      On failure *ppszBuffer will be set to NULL.
 * @param   pszFormat   The format string.
 * @param   args        The format argument.
 */
RTDECL(int) RTStrAPrintfV(char **ppszBuffer, const char *pszFormat, va_list args);

/**
 * Allocating string printf.
 *
 * @returns The length of the string in the returned *ppszBuffer.
 * @returns -1 on failure.
 * @param   ppszBuffer  Where to store the pointer to the allocated output buffer.
 *                      The buffer should be freed using RTStrFree().
 *                      On failure *ppszBuffer will be set to NULL.
 * @param   pszFormat   The format string.
 * @param   ...         The format argument.
 */
RTDECL(int) RTStrAPrintf(char **ppszBuffer, const char *pszFormat, ...);


/**
 * Strips blankspaces from both ends of the string.
 *
 * @returns Pointer to first non-blank char in the string.
 * @param   psz     The string to strip.
 */
RTDECL(char *) RTStrStrip(char *psz);

/**
 * Strips blankspaces from the start of the string.
 *
 * @returns Pointer to first non-blank char in the string.
 * @param   psz     The string to strip.
 */
RTDECL(char *) RTStrStripL(const char *psz);

/**
 * Strips blankspaces from the end of the string.
 *
 * @returns psz.
 * @param   psz     The string to strip.
 */
RTDECL(char *) RTStrStripR(char *psz);

/**
 * Performs a case sensitive string compare between two UTF-8 strings.
 *
 * Encoding errors are ignored by the current implementation. So, the only
 * difference between this and the CRT strcmp function is the handling of
 * NULL arguments.
 *
 * @returns < 0 if the first string less than the second string.
 * @returns 0 if the first string identical to the second string.
 * @returns > 0 if the first string greater than the second string.
 * @param   psz1        First UTF-8 string. Null is allowed.
 * @param   psz2        Second UTF-8 string. Null is allowed.
 */
RTDECL(int) RTStrCmp(const char *psz1, const char *psz2);

/**
 * Performs a case sensitive string compare between two UTF-8 strings, given
 * a maximum string length.
 *
 * Encoding errors are ignored by the current implementation. So, the only
 * difference between this and the CRT strncmp function is the handling of
 * NULL arguments.
 *
 * @returns < 0 if the first string less than the second string.
 * @returns 0 if the first string identical to the second string.
 * @returns > 0 if the first string greater than the second string.
 * @param   psz1        First UTF-8 string. Null is allowed.
 * @param   psz2        Second UTF-8 string. Null is allowed.
 * @param   cchMax      The maximum string length
 */
RTDECL(int) RTStrNCmp(const char *psz1, const char *psz2, size_t cchMax);

/**
 * Performs a case insensitive string compare between two UTF-8 strings.
 *
 * This is a simplified compare, as only the simplified lower/upper case folding
 * specified by the unicode specs are used. It does not consider character pairs
 * as they are used in some languages, just simple upper & lower case compares.
 *
 * The result is the difference between the mismatching codepoints after they
 * both have been lower cased.
 *
 * If the string encoding is invalid the function will assert (strict builds)
 * and use RTStrCmp for the remainder of the string.
 *
 * @returns < 0 if the first string less than the second string.
 * @returns 0 if the first string identical to the second string.
 * @returns > 0 if the first string greater than the second string.
 * @param   psz1        First UTF-8 string. Null is allowed.
 * @param   psz2        Second UTF-8 string. Null is allowed.
 */
RTDECL(int) RTStrICmp(const char *psz1, const char *psz2);

/**
 * Performs a case insensitive string compare between two UTF-8 strings, given a
 * maximum string length.
 *
 * This is a simplified compare, as only the simplified lower/upper case folding
 * specified by the unicode specs are used. It does not consider character pairs
 * as they are used in some languages, just simple upper & lower case compares.
 *
 * The result is the difference between the mismatching codepoints after they
 * both have been lower cased.
 *
 * If the string encoding is invalid the function will assert (strict builds)
 * and use RTStrCmp for the remainder of the string.
 *
 * @returns < 0 if the first string less than the second string.
 * @returns 0 if the first string identical to the second string.
 * @returns > 0 if the first string greater than the second string.
 * @param   psz1        First UTF-8 string. Null is allowed.
 * @param   psz2        Second UTF-8 string. Null is allowed.
 * @param   cchMax      Maximum string length
 */
RTDECL(int) RTStrNICmp(const char *psz1, const char *psz2, size_t cchMax);

/**
 * Find the length of a zero-terminated byte string, given
 * a max string length.
 *
 * See also RTStrNLenEx.
 *
 * @returns The string length or cbMax. The returned length does not include
 *          the zero terminator if it was found.
 *
 * @param   pszString   The string.
 * @param   cchMax      The max string length.
 */
RTDECL(size_t) RTStrNLen(const char *pszString, size_t cchMax);

/**
 * Find the length of a zero-terminated byte string, given
 * a max string length.
 *
 * See also RTStrNLen.
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS if the string has a length less than cchMax.
 * @retval  VERR_BUFFER_OVERFLOW if the end of the string wasn't found
 *          before cchMax was reached.
 *
 * @param   pszString   The string.
 * @param   cchMax      The max string length.
 * @param   pcch        Where to store the string length excluding the
 *                      terminator. This is set to cchMax if the terminator
 *                      isn't found.
 */
RTDECL(int) RTStrNLenEx(const char *pszString, size_t cchMax, size_t *pcch);

/**
 * Matches a simple string pattern.
 *
 * @returns true if the string matches the pattern, otherwise false.
 *
 * @param  pszPattern   The pattern. Special chars are '*' and '?', where the
 *                      asterisk matches zero or more characters and question
 *                      mark matches exactly one character.
 * @param  pszString    The string to match against the pattern.
 */
RTDECL(bool) RTStrSimplePatternMatch(const char *pszPattern, const char *pszString);

/**
 * Matches a simple string pattern, neither which needs to be zero terminated.
 *
 * This is identical to RTStrSimplePatternMatch except that you can optionally
 * specify the length of both the pattern and the string.  The function will
 * stop when it hits a string terminator or either of the lengths.
 *
 * @returns true if the string matches the pattern, otherwise false.
 *
 * @param  pszPattern   The pattern. Special chars are '*' and '?', where the
 *                      asterisk matches zero or more characters and question
 *                      mark matches exactly one character.
 * @param  cchPattern   The pattern length. Pass RTSTR_MAX if you don't know the
 *                      length and wish to stop at the string terminator.
 * @param  pszString    The string to match against the pattern.
 * @param  cchString    The string length. Pass RTSTR_MAX if you don't know the
 *                      length and wish to match up to the string terminator.
 */
RTDECL(bool) RTStrSimplePatternNMatch(const char *pszPattern, size_t cchPattern,
                                      const char *pszString, size_t cchString);

/**
 * Matches multiple patterns against a string.
 *
 * The patterns are separated by the pipe character (|).
 *
 * @returns true if the string matches the pattern, otherwise false.
 *
 * @param   pszPatterns The patterns.
 * @param   cchPatterns The lengths of the patterns to use. Pass RTSTR_MAX to
 *                      stop at the terminator.
 * @param   pszString   The string to match against the pattern.
 * @param   cchString   The string length. Pass RTSTR_MAX stop stop at the
 *                      terminator.
 * @param   poffPattern Offset into the patterns string of the patttern that
 *                      matched. If no match, this will be set to RTSTR_MAX.
 *                      This is optional, NULL is fine.
 */
RTDECL(bool) RTStrSimplePatternMultiMatch(const char *pszPatterns, size_t cchPatterns,
                                          const char *pszString, size_t cchString,
                                          size_t *poffPattern);

/**
 * Converts the string to lower case.
 *
 * @returns Pointer to the converted string.
 * @param   psz     The string to convert.
 */
RTDECL(char *) RTStrToLower(char *psz);

/**
 * Converts the string to upper case.
 *
 * @returns Pointer to the converted string.
 * @param   psz     The string to convert.
 */
RTDECL(char *) RTStrToUpper(char *psz);


/** @defgroup rt_str_conv   String To/From Number Conversions
 * @ingroup grp_rt_str
 * @{ */

/**
 * Converts a string representation of a number to a 64-bit unsigned number.
 *
 * @returns iprt status code.
 *          Warnings are used to indicate conversion problems.
 * @retval  VWRN_NUMBER_TOO_BIG
 * @retval  VWRN_NEGATIVE_UNSIGNED
 * @retval  VWRN_TRAILING_CHARS
 * @retval  VWRN_TRAILING_SPACES
 * @retval  VINF_SUCCESS
 * @retval  VERR_NO_DIGITS
 *
 * @param   pszValue    Pointer to the string value.
 * @param   ppszNext    Where to store the pointer to the first char following the number. (Optional)
 * @param   uBase       The base of the representation used.
 *                      If 0 the function will look for known prefixes before defaulting to 10.
 * @param   pu64        Where to store the converted number. (optional)
 */
RTDECL(int) RTStrToUInt64Ex(const char *pszValue, char **ppszNext, unsigned uBase, uint64_t *pu64);

/**
 * Converts a string representation of a number to a 64-bit unsigned number,
 * making sure the full string is converted.
 *
 * @returns iprt status code.
 *          Warnings are used to indicate conversion problems.
 * @retval  VWRN_NUMBER_TOO_BIG
 * @retval  VWRN_NEGATIVE_UNSIGNED
 * @retval  VINF_SUCCESS
 * @retval  VERR_NO_DIGITS
 * @retval  VERR_TRAILING_SPACES
 * @retval  VERR_TRAILING_CHARS
 *
 * @param   pszValue    Pointer to the string value.
 * @param   uBase       The base of the representation used.
 *                      If 0 the function will look for known prefixes before defaulting to 10.
 * @param   pu64        Where to store the converted number. (optional)
 */
RTDECL(int) RTStrToUInt64Full(const char *pszValue, unsigned uBase, uint64_t *pu64);

/**
 * Converts a string representation of a number to a 64-bit unsigned number.
 * The base is guessed.
 *
 * @returns 64-bit unsigned number on success.
 * @returns 0 on failure.
 * @param   pszValue    Pointer to the string value.
 */
RTDECL(uint64_t) RTStrToUInt64(const char *pszValue);

/**
 * Converts a string representation of a number to a 32-bit unsigned number.
 *
 * @returns iprt status code.
 *          Warnings are used to indicate conversion problems.
 * @retval  VWRN_NUMBER_TOO_BIG
 * @retval  VWRN_NEGATIVE_UNSIGNED
 * @retval  VWRN_TRAILING_CHARS
 * @retval  VWRN_TRAILING_SPACES
 * @retval  VINF_SUCCESS
 * @retval  VERR_NO_DIGITS
 *
 * @param   pszValue    Pointer to the string value.
 * @param   ppszNext    Where to store the pointer to the first char following the number. (Optional)
 * @param   uBase       The base of the representation used.
 *                      If 0 the function will look for known prefixes before defaulting to 10.
 * @param   pu32        Where to store the converted number. (optional)
 */
RTDECL(int) RTStrToUInt32Ex(const char *pszValue, char **ppszNext, unsigned uBase, uint32_t *pu32);

/**
 * Converts a string representation of a number to a 32-bit unsigned number,
 * making sure the full string is converted.
 *
 * @returns iprt status code.
 *          Warnings are used to indicate conversion problems.
 * @retval  VWRN_NUMBER_TOO_BIG
 * @retval  VWRN_NEGATIVE_UNSIGNED
 * @retval  VINF_SUCCESS
 * @retval  VERR_NO_DIGITS
 * @retval  VERR_TRAILING_SPACES
 * @retval  VERR_TRAILING_CHARS
 *
 * @param   pszValue    Pointer to the string value.
 * @param   uBase       The base of the representation used.
 *                      If 0 the function will look for known prefixes before defaulting to 10.
 * @param   pu32        Where to store the converted number. (optional)
 */
RTDECL(int) RTStrToUInt32Full(const char *pszValue, unsigned uBase, uint32_t *pu32);

/**
 * Converts a string representation of a number to a 64-bit unsigned number.
 * The base is guessed.
 *
 * @returns 32-bit unsigned number on success.
 * @returns 0 on failure.
 * @param   pszValue    Pointer to the string value.
 */
RTDECL(uint32_t) RTStrToUInt32(const char *pszValue);

/**
 * Converts a string representation of a number to a 16-bit unsigned number.
 *
 * @returns iprt status code.
 *          Warnings are used to indicate conversion problems.
 * @retval  VWRN_NUMBER_TOO_BIG
 * @retval  VWRN_NEGATIVE_UNSIGNED
 * @retval  VWRN_TRAILING_CHARS
 * @retval  VWRN_TRAILING_SPACES
 * @retval  VINF_SUCCESS
 * @retval  VERR_NO_DIGITS
 *
 * @param   pszValue    Pointer to the string value.
 * @param   ppszNext    Where to store the pointer to the first char following the number. (Optional)
 * @param   uBase       The base of the representation used.
 *                      If 0 the function will look for known prefixes before defaulting to 10.
 * @param   pu16        Where to store the converted number. (optional)
 */
RTDECL(int) RTStrToUInt16Ex(const char *pszValue, char **ppszNext, unsigned uBase, uint16_t *pu16);

/**
 * Converts a string representation of a number to a 16-bit unsigned number,
 * making sure the full string is converted.
 *
 * @returns iprt status code.
 *          Warnings are used to indicate conversion problems.
 * @retval  VWRN_NUMBER_TOO_BIG
 * @retval  VWRN_NEGATIVE_UNSIGNED
 * @retval  VINF_SUCCESS
 * @retval  VERR_NO_DIGITS
 * @retval  VERR_TRAILING_SPACES
 * @retval  VERR_TRAILING_CHARS
 *
 * @param   pszValue    Pointer to the string value.
 * @param   uBase       The base of the representation used.
 *                      If 0 the function will look for known prefixes before defaulting to 10.
 * @param   pu16        Where to store the converted number. (optional)
 */
RTDECL(int) RTStrToUInt16Full(const char *pszValue, unsigned uBase, uint16_t *pu16);

/**
 * Converts a string representation of a number to a 16-bit unsigned number.
 * The base is guessed.
 *
 * @returns 16-bit unsigned number on success.
 * @returns 0 on failure.
 * @param   pszValue    Pointer to the string value.
 */
RTDECL(uint16_t) RTStrToUInt16(const char *pszValue);

/**
 * Converts a string representation of a number to a 8-bit unsigned number.
 *
 * @returns iprt status code.
 *          Warnings are used to indicate conversion problems.
 * @retval  VWRN_NUMBER_TOO_BIG
 * @retval  VWRN_NEGATIVE_UNSIGNED
 * @retval  VWRN_TRAILING_CHARS
 * @retval  VWRN_TRAILING_SPACES
 * @retval  VINF_SUCCESS
 * @retval  VERR_NO_DIGITS
 *
 * @param   pszValue    Pointer to the string value.
 * @param   ppszNext    Where to store the pointer to the first char following the number. (Optional)
 * @param   uBase       The base of the representation used.
 *                      If 0 the function will look for known prefixes before defaulting to 10.
 * @param   pu8         Where to store the converted number. (optional)
 */
RTDECL(int) RTStrToUInt8Ex(const char *pszValue, char **ppszNext, unsigned uBase, uint8_t *pu8);

/**
 * Converts a string representation of a number to a 8-bit unsigned number,
 * making sure the full string is converted.
 *
 * @returns iprt status code.
 *          Warnings are used to indicate conversion problems.
 * @retval  VWRN_NUMBER_TOO_BIG
 * @retval  VWRN_NEGATIVE_UNSIGNED
 * @retval  VINF_SUCCESS
 * @retval  VERR_NO_DIGITS
 * @retval  VERR_TRAILING_SPACES
 * @retval  VERR_TRAILING_CHARS
 *
 * @param   pszValue    Pointer to the string value.
 * @param   uBase       The base of the representation used.
 *                      If 0 the function will look for known prefixes before defaulting to 10.
 * @param   pu8         Where to store the converted number. (optional)
 */
RTDECL(int) RTStrToUInt8Full(const char *pszValue, unsigned uBase, uint8_t *pu8);

/**
 * Converts a string representation of a number to a 8-bit unsigned number.
 * The base is guessed.
 *
 * @returns 8-bit unsigned number on success.
 * @returns 0 on failure.
 * @param   pszValue    Pointer to the string value.
 */
RTDECL(uint8_t) RTStrToUInt8(const char *pszValue);

/**
 * Converts a string representation of a number to a 64-bit signed number.
 *
 * @returns iprt status code.
 *          Warnings are used to indicate conversion problems.
 * @retval  VWRN_NUMBER_TOO_BIG
 * @retval  VWRN_TRAILING_CHARS
 * @retval  VWRN_TRAILING_SPACES
 * @retval  VINF_SUCCESS
 * @retval  VERR_NO_DIGITS
 *
 * @param   pszValue    Pointer to the string value.
 * @param   ppszNext    Where to store the pointer to the first char following the number. (Optional)
 * @param   uBase       The base of the representation used.
 *                      If 0 the function will look for known prefixes before defaulting to 10.
 * @param   pi64        Where to store the converted number. (optional)
 */
RTDECL(int) RTStrToInt64Ex(const char *pszValue, char **ppszNext, unsigned uBase, int64_t *pi64);

/**
 * Converts a string representation of a number to a 64-bit signed number,
 * making sure the full string is converted.
 *
 * @returns iprt status code.
 *          Warnings are used to indicate conversion problems.
 * @retval  VWRN_NUMBER_TOO_BIG
 * @retval  VINF_SUCCESS
 * @retval  VERR_TRAILING_CHARS
 * @retval  VERR_TRAILING_SPACES
 * @retval  VERR_NO_DIGITS
 *
 * @param   pszValue    Pointer to the string value.
 * @param   uBase       The base of the representation used.
 *                      If 0 the function will look for known prefixes before defaulting to 10.
 * @param   pi64        Where to store the converted number. (optional)
 */
RTDECL(int) RTStrToInt64Full(const char *pszValue, unsigned uBase, int64_t *pi64);

/**
 * Converts a string representation of a number to a 64-bit signed number.
 * The base is guessed.
 *
 * @returns 64-bit signed number on success.
 * @returns 0 on failure.
 * @param   pszValue    Pointer to the string value.
 */
RTDECL(int64_t) RTStrToInt64(const char *pszValue);

/**
 * Converts a string representation of a number to a 32-bit signed number.
 *
 * @returns iprt status code.
 *          Warnings are used to indicate conversion problems.
 * @retval  VWRN_NUMBER_TOO_BIG
 * @retval  VWRN_TRAILING_CHARS
 * @retval  VWRN_TRAILING_SPACES
 * @retval  VINF_SUCCESS
 * @retval  VERR_NO_DIGITS
 *
 * @param   pszValue    Pointer to the string value.
 * @param   ppszNext    Where to store the pointer to the first char following the number. (Optional)
 * @param   uBase       The base of the representation used.
 *                      If 0 the function will look for known prefixes before defaulting to 10.
 * @param   pi32        Where to store the converted number. (optional)
 */
RTDECL(int) RTStrToInt32Ex(const char *pszValue, char **ppszNext, unsigned uBase, int32_t *pi32);

/**
 * Converts a string representation of a number to a 32-bit signed number,
 * making sure the full string is converted.
 *
 * @returns iprt status code.
 *          Warnings are used to indicate conversion problems.
 * @retval  VWRN_NUMBER_TOO_BIG
 * @retval  VINF_SUCCESS
 * @retval  VERR_TRAILING_CHARS
 * @retval  VERR_TRAILING_SPACES
 * @retval  VERR_NO_DIGITS
 *
 * @param   pszValue    Pointer to the string value.
 * @param   uBase       The base of the representation used.
 *                      If 0 the function will look for known prefixes before defaulting to 10.
 * @param   pi32        Where to store the converted number. (optional)
 */
RTDECL(int) RTStrToInt32Full(const char *pszValue, unsigned uBase, int32_t *pi32);

/**
 * Converts a string representation of a number to a 32-bit signed number.
 * The base is guessed.
 *
 * @returns 32-bit signed number on success.
 * @returns 0 on failure.
 * @param   pszValue    Pointer to the string value.
 */
RTDECL(int32_t) RTStrToInt32(const char *pszValue);

/**
 * Converts a string representation of a number to a 16-bit signed number.
 *
 * @returns iprt status code.
 *          Warnings are used to indicate conversion problems.
 * @retval  VWRN_NUMBER_TOO_BIG
 * @retval  VWRN_TRAILING_CHARS
 * @retval  VWRN_TRAILING_SPACES
 * @retval  VINF_SUCCESS
 * @retval  VERR_NO_DIGITS
 *
 * @param   pszValue    Pointer to the string value.
 * @param   ppszNext    Where to store the pointer to the first char following the number. (Optional)
 * @param   uBase       The base of the representation used.
 *                      If 0 the function will look for known prefixes before defaulting to 10.
 * @param   pi16        Where to store the converted number. (optional)
 */
RTDECL(int) RTStrToInt16Ex(const char *pszValue, char **ppszNext, unsigned uBase, int16_t *pi16);

/**
 * Converts a string representation of a number to a 16-bit signed number,
 * making sure the full string is converted.
 *
 * @returns iprt status code.
 *          Warnings are used to indicate conversion problems.
 * @retval  VWRN_NUMBER_TOO_BIG
 * @retval  VINF_SUCCESS
 * @retval  VERR_TRAILING_CHARS
 * @retval  VERR_TRAILING_SPACES
 * @retval  VERR_NO_DIGITS
 *
 * @param   pszValue    Pointer to the string value.
 * @param   uBase       The base of the representation used.
 *                      If 0 the function will look for known prefixes before defaulting to 10.
 * @param   pi16        Where to store the converted number. (optional)
 */
RTDECL(int) RTStrToInt16Full(const char *pszValue, unsigned uBase, int16_t *pi16);

/**
 * Converts a string representation of a number to a 16-bit signed number.
 * The base is guessed.
 *
 * @returns 16-bit signed number on success.
 * @returns 0 on failure.
 * @param   pszValue    Pointer to the string value.
 */
RTDECL(int16_t) RTStrToInt16(const char *pszValue);

/**
 * Converts a string representation of a number to a 8-bit signed number.
 *
 * @returns iprt status code.
 *          Warnings are used to indicate conversion problems.
 * @retval  VWRN_NUMBER_TOO_BIG
 * @retval  VWRN_TRAILING_CHARS
 * @retval  VWRN_TRAILING_SPACES
 * @retval  VINF_SUCCESS
 * @retval  VERR_NO_DIGITS
 *
 * @param   pszValue    Pointer to the string value.
 * @param   ppszNext    Where to store the pointer to the first char following the number. (Optional)
 * @param   uBase       The base of the representation used.
 *                      If 0 the function will look for known prefixes before defaulting to 10.
 * @param   pi8        Where to store the converted number. (optional)
 */
RTDECL(int) RTStrToInt8Ex(const char *pszValue, char **ppszNext, unsigned uBase, int8_t *pi8);

/**
 * Converts a string representation of a number to a 8-bit signed number,
 * making sure the full string is converted.
 *
 * @returns iprt status code.
 *          Warnings are used to indicate conversion problems.
 * @retval  VWRN_NUMBER_TOO_BIG
 * @retval  VINF_SUCCESS
 * @retval  VERR_TRAILING_CHARS
 * @retval  VERR_TRAILING_SPACES
 * @retval  VERR_NO_DIGITS
 *
 * @param   pszValue    Pointer to the string value.
 * @param   uBase       The base of the representation used.
 *                      If 0 the function will look for known prefixes before defaulting to 10.
 * @param   pi8         Where to store the converted number. (optional)
 */
RTDECL(int) RTStrToInt8Full(const char *pszValue, unsigned uBase, int8_t *pi8);

/**
 * Converts a string representation of a number to a 8-bit signed number.
 * The base is guessed.
 *
 * @returns 8-bit signed number on success.
 * @returns 0 on failure.
 * @param   pszValue    Pointer to the string value.
 */
RTDECL(int8_t) RTStrToInt8(const char *pszValue);

/** @} */


/** @defgroup rt_str_space  Unique String Space
 * @ingroup grp_rt_str
 * @{
 */

/** Pointer to a string name space container node core. */
typedef struct RTSTRSPACECORE *PRTSTRSPACECORE;
/** Pointer to a pointer to a string name space container node core. */
typedef PRTSTRSPACECORE *PPRTSTRSPACECORE;

/**
 * String name space container node core.
 */
typedef struct RTSTRSPACECORE
{
    /** Hash key. Don't touch. */
    uint32_t        Key;
    /** Pointer to the left leaf node. Don't touch. */
    PRTSTRSPACECORE pLeft;
    /** Pointer to the left rigth node. Don't touch. */
    PRTSTRSPACECORE pRight;
    /** Pointer to the list of string with the same key. Don't touch. */
    PRTSTRSPACECORE pList;
    /** Height of this tree: max(heigth(left), heigth(right)) + 1. Don't touch */
    unsigned char   uchHeight;
    /** The string length. Read only! */
    size_t          cchString;
    /** Pointer to the string. Read only! */
    const char *    pszString;
} RTSTRSPACECORE;

/** String space. (Initialize with NULL.) */
typedef PRTSTRSPACECORE     RTSTRSPACE;
/** Pointer to a string space. */
typedef PPRTSTRSPACECORE    PRTSTRSPACE;


/**
 * Inserts a string into a unique string space.
 *
 * @returns true on success.
 * @returns false if the string collided with an existing string.
 * @param   pStrSpace       The space to insert it into.
 * @param   pStr            The string node.
 */
RTDECL(bool) RTStrSpaceInsert(PRTSTRSPACE pStrSpace, PRTSTRSPACECORE pStr);

/**
 * Removes a string from a unique string space.
 *
 * @returns Pointer to the removed string node.
 * @returns NULL if the string was not found in the string space.
 * @param   pStrSpace       The space to insert it into.
 * @param   pszString       The string to remove.
 */
RTDECL(PRTSTRSPACECORE) RTStrSpaceRemove(PRTSTRSPACE pStrSpace, const char *pszString);

/**
 * Gets a string from a unique string space.
 *
 * @returns Pointer to the string node.
 * @returns NULL if the string was not found in the string space.
 * @param   pStrSpace       The space to insert it into.
 * @param   pszString       The string to get.
 */
RTDECL(PRTSTRSPACECORE) RTStrSpaceGet(PRTSTRSPACE pStrSpace, const char *pszString);

/**
 * Callback function for RTStrSpaceEnumerate() and RTStrSpaceDestroy().
 *
 * @returns 0 on continue.
 * @returns Non-zero to aborts the operation.
 * @param   pStr        The string node
 * @param   pvUser      The user specified argument.
 */
typedef DECLCALLBACK(int)   FNRTSTRSPACECALLBACK(PRTSTRSPACECORE pStr, void *pvUser);
/** Pointer to callback function for RTStrSpaceEnumerate() and RTStrSpaceDestroy(). */
typedef FNRTSTRSPACECALLBACK *PFNRTSTRSPACECALLBACK;

/**
 * Destroys the string space.
 * The caller supplies a callback which will be called for each of
 * the string nodes in for freeing their memory and other resources.
 *
 * @returns 0 or what ever non-zero return value pfnCallback returned
 *          when aborting the destruction.
 * @param   pStrSpace       The space to insert it into.
 * @param   pfnCallback     The callback.
 * @param   pvUser          The user argument.
 */
RTDECL(int) RTStrSpaceDestroy(PRTSTRSPACE pStrSpace, PFNRTSTRSPACECALLBACK pfnCallback, void *pvUser);

/**
 * Enumerates the string space.
 * The caller supplies a callback which will be called for each of
 * the string nodes.
 *
 * @returns 0 or what ever non-zero return value pfnCallback returned
 *          when aborting the destruction.
 * @param   pStrSpace       The space to insert it into.
 * @param   pfnCallback     The callback.
 * @param   pvUser          The user argument.
 */
RTDECL(int) RTStrSpaceEnumerate(PRTSTRSPACE pStrSpace, PFNRTSTRSPACECALLBACK pfnCallback, void *pvUser);

/** @} */


/** @defgroup rt_str_utf16      UTF-16 String Manipulation
 * @ingroup grp_rt_str
 * @{
 */

/**
 * Free a UTF-16 string allocated by RTStrUtf8ToUtf16(), RTStrUtf8ToUtf16Ex(),
 * RTUtf16Dup() or RTUtf16DupEx().
 *
 * @returns iprt status code.
 * @param   pwszString      The UTF-16 string to free. NULL is accepted.
 */
RTDECL(void)  RTUtf16Free(PRTUTF16 pwszString);

/**
 * Allocates a new copy of the specified UTF-16 string.
 *
 * @returns Pointer to the allocated string copy. Use RTUtf16Free() to free it.
 * @returns NULL when out of memory.
 * @param   pwszString      UTF-16 string to duplicate.
 * @remark  This function will not make any attempt to validate the encoding.
 */
RTDECL(PRTUTF16) RTUtf16Dup(PCRTUTF16 pwszString);

/**
 * Allocates a new copy of the specified UTF-16 string.
 *
 * @returns iprt status code.
 * @param   ppwszString     Receives pointer of the allocated UTF-16 string.
 *                          The returned pointer must be freed using RTUtf16Free().
 * @param   pwszString      UTF-16 string to duplicate.
 * @param   cwcExtra        Number of extra RTUTF16 items to allocate.
 * @remark  This function will not make any attempt to validate the encoding.
 */
RTDECL(int) RTUtf16DupEx(PRTUTF16 *ppwszString, PCRTUTF16 pwszString, size_t cwcExtra);

/**
 * Returns the length of a UTF-16 string in UTF-16 characters
 * without trailing '\\0'.
 *
 * Surrogate pairs counts as two UTF-16 characters here. Use RTUtf16CpCnt()
 * to get the exact number of code points in the string.
 *
 * @returns The number of RTUTF16 items in the string.
 * @param   pwszString  Pointer the UTF-16 string.
 * @remark  This function will not make any attempt to validate the encoding.
 */
RTDECL(size_t) RTUtf16Len(PCRTUTF16 pwszString);

/**
 * Performs a case sensitive string compare between two UTF-16 strings.
 *
 * @returns < 0 if the first string less than the second string.s
 * @returns 0 if the first string identical to the second string.
 * @returns > 0 if the first string greater than the second string.
 * @param   pwsz1       First UTF-16 string. Null is allowed.
 * @param   pwsz2       Second UTF-16 string. Null is allowed.
 * @remark  This function will not make any attempt to validate the encoding.
 */
RTDECL(int) RTUtf16Cmp(register PCRTUTF16 pwsz1, register PCRTUTF16 pwsz2);

/**
 * Performs a case insensitive string compare between two UTF-16 strings.
 *
 * This is a simplified compare, as only the simplified lower/upper case folding
 * specified by the unicode specs are used. It does not consider character pairs
 * as they are used in some languages, just simple upper & lower case compares.
 *
 * @returns < 0 if the first string less than the second string.
 * @returns 0 if the first string identical to the second string.
 * @returns > 0 if the first string greater than the second string.
 * @param   pwsz1       First UTF-16 string. Null is allowed.
 * @param   pwsz2       Second UTF-16 string. Null is allowed.
 */
RTDECL(int) RTUtf16ICmp(PCRTUTF16 pwsz1, PCRTUTF16 pwsz2);

/**
 * Performs a case insensitive string compare between two UTF-16 strings
 * using the current locale of the process (if applicable).
 *
 * This differs from RTUtf16ICmp() in that it will try, if a locale with the
 * required data is available, to do a correct case-insensitive compare. It
 * follows that it is more complex and thereby likely to be more expensive.
 *
 * @returns < 0 if the first string less than the second string.
 * @returns 0 if the first string identical to the second string.
 * @returns > 0 if the first string greater than the second string.
 * @param   pwsz1       First UTF-16 string. Null is allowed.
 * @param   pwsz2       Second UTF-16 string. Null is allowed.
 */
RTDECL(int) RTUtf16LocaleICmp(PCRTUTF16 pwsz1, PCRTUTF16 pwsz2);

/**
 * Folds a UTF-16 string to lowercase.
 *
 * This is a very simple folding; is uses the simple lowercase
 * code point, it is not related to any locale just the most common
 * lowercase codepoint setup by the unicode specs, and it will not
 * create new surrogate pairs or remove existing ones.
 *
 * @returns Pointer to the passed in string.
 * @param   pwsz        The string to fold.
 */
RTDECL(PRTUTF16) RTUtf16ToLower(PRTUTF16 pwsz);

/**
 * Folds a UTF-16 string to uppercase.
 *
 * This is a very simple folding; is uses the simple uppercase
 * code point, it is not related to any locale just the most common
 * uppercase codepoint setup by the unicode specs, and it will not
 * create new surrogate pairs or remove existing ones.
 *
 * @returns Pointer to the passed in string.
 * @param   pwsz        The string to fold.
 */
RTDECL(PRTUTF16) RTUtf16ToUpper(PRTUTF16 pwsz);

/**
 * Translate a UTF-16 string into a UTF-8 allocating the result buffer.
 *
 * @returns iprt status code.
 * @param   pwszString      UTF-16 string to convert.
 * @param   ppszString      Receives pointer of allocated UTF-8 string on
 *                          success, and is always set to NULL on failure.
 *                          The returned pointer must be freed using RTStrFree().
 */
RTDECL(int)  RTUtf16ToUtf8(PCRTUTF16 pwszString, char **ppszString);

/**
 * Translates UTF-16 to UTF-8 using buffer provided by the caller or
 * a fittingly sized buffer allocated by the function.
 *
 * @returns iprt status code.
 * @param   pwszString      The UTF-16 string to convert.
 * @param   cwcString       The number of RTUTF16 items to translate from pwszString.
 *                          The translation will stop when reaching cwcString or the terminator ('\\0').
 *                          Use RTSTR_MAX to translate the entire string.
 * @param   ppsz            If cch is non-zero, this must either be pointing to a pointer to
 *                          a buffer of the specified size, or pointer to a NULL pointer.
 *                          If *ppsz is NULL or cch is zero a buffer of at least cch chars
 *                          will be allocated to hold the translated string.
 *                          If a buffer was requested it must be freed using RTUtf16Free().
 * @param   cch             The buffer size in chars (the type). This includes the terminator.
 * @param   pcch            Where to store the length of the translated string. (Optional)
 *                          This field will be updated even on failure, however the value is only
 *                          specified for the following two error codes. On VERR_BUFFER_OVERFLOW
 *                          and VERR_NO_STR_MEMORY it contains the required buffer space.
 */
RTDECL(int)  RTUtf16ToUtf8Ex(PCRTUTF16 pwszString, size_t cwcString, char **ppsz, size_t cch, size_t *pcch);

/**
 * Calculates the length of the UTF-16 string in UTF-8 chars (bytes).
 *
 * This function will validate the string, and incorrectly encoded UTF-16
 * strings will be rejected. The primary purpose of this function is to
 * help allocate buffers for RTUtf16ToUtf8() of the correct size. For most
 * other purposes RTUtf16ToUtf8Ex() should be used.
 *
 * @returns Number of char (bytes).
 * @returns 0 if the string was incorrectly encoded.
 * @param   pwsz        The UTF-16 string.
 */
RTDECL(size_t) RTUtf16CalcUtf8Len(PCRTUTF16 pwsz);

/**
 * Calculates the length of the UTF-16 string in UTF-8 chars (bytes).
 *
 * This function will validate the string, and incorrectly encoded UTF-16
 * strings will be rejected.
 *
 * @returns iprt status code.
 * @param   pwsz        The string.
 * @param   cwc         The max string length. Use RTSTR_MAX to process the entire string.
 * @param   pcch        Where to store the string length (in bytes). Optional.
 *                      This is undefined on failure.
 */
RTDECL(int) RTUtf16CalcUtf8LenEx(PCRTUTF16 pwsz, size_t cwc, size_t *pcch);

/**
 * Get the unicode code point at the given string position.
 *
 * @returns unicode code point.
 * @returns RTUNICP_INVALID if the encoding is invalid.
 * @param   pwsz        The string.
 *
 * @remark  This is an internal worker for RTUtf16GetCp().
 */
RTDECL(RTUNICP) RTUtf16GetCpInternal(PCRTUTF16 pwsz);

/**
 * Get the unicode code point at the given string position.
 *
 * @returns iprt status code.
 * @param   ppwsz       Pointer to the string pointer. This will be updated to
 *                      point to the char following the current code point.
 * @param   pCp         Where to store the code point.
 *                      RTUNICP_INVALID is stored here on failure.
 *
 * @remark  This is an internal worker for RTUtf16GetCpEx().
 */
RTDECL(int) RTUtf16GetCpExInternal(PCRTUTF16 *ppwsz, PRTUNICP pCp);

/**
 * Put the unicode code point at the given string position
 * and return the pointer to the char following it.
 *
 * This function will not consider anything at or following the
 * buffer area pointed to by pwsz. It is therefore not suitable for
 * inserting code points into a string, only appending/overwriting.
 *
 * @returns pointer to the char following the written code point.
 * @param   pwsz        The string.
 * @param   CodePoint   The code point to write.
 *                      This should not be RTUNICP_INVALID or any other
 *                      character out of the UTF-16 range.
 *
 * @remark  This is an internal worker for RTUtf16GetCpEx().
 */
RTDECL(PRTUTF16) RTUtf16PutCpInternal(PRTUTF16 pwsz, RTUNICP CodePoint);

/**
 * Get the unicode code point at the given string position.
 *
 * @returns unicode code point.
 * @returns RTUNICP_INVALID if the encoding is invalid.
 * @param   pwsz        The string.
 *
 * @remark  We optimize this operation by using an inline function for
 *          everything which isn't a surrogate pair or an endian indicator.
 */
DECLINLINE(RTUNICP) RTUtf16GetCp(PCRTUTF16 pwsz)
{
    const RTUTF16 wc = *pwsz;
    if (wc < 0xd800 || (wc > 0xdfff && wc < 0xfffe))
        return wc;
    return RTUtf16GetCpInternal(pwsz);
}

/**
 * Get the unicode code point at the given string position.
 *
 * @returns iprt status code.
 * @param   ppwsz       Pointer to the string pointer. This will be updated to
 *                      point to the char following the current code point.
 * @param   pCp         Where to store the code point.
 *                      RTUNICP_INVALID is stored here on failure.
 *
 * @remark  We optimize this operation by using an inline function for
 *          everything which isn't a surrogate pair or and endian indicator.
 */
DECLINLINE(int) RTUtf16GetCpEx(PCRTUTF16 *ppwsz, PRTUNICP pCp)
{
    const RTUTF16 wc = **ppwsz;
    if (wc < 0xd800 || (wc > 0xdfff && wc < 0xfffe))
    {
        (*ppwsz)++;
        *pCp = wc;
        return VINF_SUCCESS;
    }
    return RTUtf16GetCpExInternal(ppwsz, pCp);
}

/**
 * Put the unicode code point at the given string position
 * and return the pointer to the char following it.
 *
 * This function will not consider anything at or following the
 * buffer area pointed to by pwsz. It is therefore not suitable for
 * inserting code points into a string, only appending/overwriting.
 *
 * @returns pointer to the char following the written code point.
 * @param   pwsz        The string.
 * @param   CodePoint   The code point to write.
 *                      This should not be RTUNICP_INVALID or any other
 *                      character out of the UTF-16 range.
 *
 * @remark  We optimize this operation by using an inline function for
 *          everything which isn't a surrogate pair or and endian indicator.
 */
DECLINLINE(PRTUTF16) RTUtf16PutCp(PRTUTF16 pwsz, RTUNICP CodePoint)
{
    if (CodePoint < 0xd800 || (CodePoint > 0xd800 && CodePoint < 0xfffe))
    {
        *pwsz++ = (RTUTF16)CodePoint;
        return pwsz;
    }
    return RTUtf16PutCpInternal(pwsz, CodePoint);
}

/**
 * Skips ahead, past the current code point.
 *
 * @returns Pointer to the char after the current code point.
 * @param   pwsz    Pointer to the current code point.
 * @remark  This will not move the next valid code point, only past the current one.
 */
DECLINLINE(PRTUTF16) RTUtf16NextCp(PCRTUTF16 pwsz)
{
    RTUNICP Cp;
    RTUtf16GetCpEx(&pwsz, &Cp);
    return (PRTUTF16)pwsz;
}

/**
 * Skips backwards, to the previous code point.
 *
 * @returns Pointer to the char after the current code point.
 * @param   pwszStart   Pointer to the start of the string.
 * @param   pwsz        Pointer to the current code point.
 */
RTDECL(PRTUTF16) RTUtf16PrevCp(PCRTUTF16 pwszStart, PCRTUTF16 pwsz);


/**
 * Checks if the UTF-16 char is the high surrogate char (i.e.
 * the 1st char in the pair).
 *
 * @returns true if it is.
 * @returns false if it isn't.
 * @param   wc      The character to investigate.
 */
DECLINLINE(bool) RTUtf16IsHighSurrogate(RTUTF16 wc)
{
    return wc >= 0xd800 && wc <= 0xdbff;
}

/**
 * Checks if the UTF-16 char is the low surrogate char (i.e.
 * the 2nd char in the pair).
 *
 * @returns true if it is.
 * @returns false if it isn't.
 * @param   wc      The character to investigate.
 */
DECLINLINE(bool) RTUtf16IsLowSurrogate(RTUTF16 wc)
{
    return wc >= 0xdc00 && wc <= 0xdfff;
}


/**
 * Checks if the two UTF-16 chars form a valid surrogate pair.
 *
 * @returns true if they do.
 * @returns false if they doesn't.
 * @param   wcHigh      The high (1st) character.
 * @param   wcLow       The low (2nd) character.
 */
DECLINLINE(bool) RTUtf16IsSurrogatePair(RTUTF16 wcHigh, RTUTF16 wcLow)
{
    return RTUtf16IsHighSurrogate(wcHigh)
        && RTUtf16IsLowSurrogate(wcLow);
}

/** @} */

__END_DECLS

/** @} */

#endif

