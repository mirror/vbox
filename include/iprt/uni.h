/** @file
 * Incredibly Portable Runtime - Unicode Code Points.
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

#ifndef ___iprt_uni_h
#define ___iprt_uni_h

/** @defgroup grp_rt_uni    RTUniCp - Unicode Code Points
 * @ingroup grp_rt
 * @{
 */

/** @def RTUNI_USE_WCTYPE
 * Define RTUNI_USE_WCTYPE to not use the IPRT unicode data but the
 * data which the C runtime library provides. */
#ifdef DOXYGEN_RUNNING
# define RTUNI_USE_WCTYPE
#endif

#include <iprt/types.h>
#ifdef RTUNI_USE_WCTYPE
# include <wctype.h>
#endif

__BEGIN_DECLS


/** Max value a RTUNICP type can hold. */
#define RTUNICP_MAX         ( ~(RTUNICP)0 )

/** Invalid code point.
 * This is returned when encountered invalid encodings or invalid
 * unicode code points. */
#define RTUNICP_INVALID     ( 0xfffffffe )



#ifndef RTUNI_USE_WCTYPE
/**
 * A unicode flags range.
 * @internal
 */
typedef struct RTUNIFLAGSRANGE
{
    /** The first code point of the range. */
    RTUNICP         BeginCP;
    /** The last + 1 code point of the range. */
    RTUNICP         EndCP;
    /** Pointer to the array of case folded code points. */
    const uint8_t  *pafFlags;
} RTUNIFLAGSRANGE;
/** Pointer to a flags range.
 * @internal */
typedef RTUNIFLAGSRANGE *PRTUNIFLAGSRANGE;
/** Pointer to a const flags range.
 * @internal */
typedef const RTUNIFLAGSRANGE *PCRTUNIFLAGSRANGE;

/**
 * A unicode case folded range.
 * @internal
 */
typedef struct RTUNICASERANGE
{
    /** The first code point of the range. */
    RTUNICP         BeginCP;
    /** The last + 1 code point of the range. */
    RTUNICP         EndCP;
    /** Pointer to the array of case folded code points. */
    PCRTUNICP       paFoldedCPs;
} RTUNICASERANGE;
/** Pointer to a case folded range.
 * @internal */
typedef RTUNICASERANGE *PRTUNICASERANGE;
/** Pointer to a const case folded range.
 * @internal */
typedef const RTUNICASERANGE *PCRTUNICASERANGE;

/** @name Unicode Code Point Flags.
 * @internal
 * @{ */
#define RTUNI_UPPER  RT_BIT(0)
#define RTUNI_LOWER  RT_BIT(1)
#define RTUNI_ALPHA  RT_BIT(2)
#define RTUNI_XDIGIT RT_BIT(3)
#define RTUNI_DDIGIT RT_BIT(4)
#define RTUNI_WSPACE RT_BIT(5)
/*#define RTUNI_BSPACE RT_BIT(6) - later */
/** @} */


/**
 * Array of flags ranges.
 * @internal
 */
extern RTDATADECL(const RTUNIFLAGSRANGE) g_aRTUniFlagsRanges[];

/**
 * Gets the flags for a unicode code point.
 *
 * @returns The flag mask. (RTUNI_*)
 * @param   CodePoint       The unicode code point.
 * @internal
 */
DECLINLINE(RTUNICP) rtUniCpFlags(RTUNICP CodePoint)
{
    PCRTUNIFLAGSRANGE pCur = &g_aRTUniFlagsRanges[0];
    do
    {
        if (pCur->EndCP > CodePoint)
        {
            if (pCur->BeginCP <= CodePoint)
                CodePoint = pCur->pafFlags[CodePoint - pCur->BeginCP];
            break;
        }
        pCur++;
    } while (pCur->EndCP != RTUNICP_MAX);
    return CodePoint;
}


/**
 * Checks if a unicode code point is upper case.
 *
 * @returns true if it is.
 * @returns false if it isn't.
 * @param   CodePoint       The code point.
 */
DECLINLINE(bool) RTUniCpIsUpper(RTUNICP CodePoint)
{
    return (rtUniCpFlags(CodePoint) & RTUNI_UPPER) != 0;
}


/**
 * Checks if a unicode code point is lower case.
 *
 * @returns true if it is.
 * @returns false if it isn't.
 * @param   CodePoint       The code point.
 */
DECLINLINE(bool) RTUniCpIsLower(RTUNICP CodePoint)
{
    return (rtUniCpFlags(CodePoint) & RTUNI_LOWER) != 0;
}


/**
 * Checks if a unicode code point is alphabetic.
 *
 * @returns true if it is.
 * @returns false if it isn't.
 * @param   CodePoint       The code point.
 */
DECLINLINE(bool) RTUniCpIsAlphabetic(RTUNICP CodePoint)
{
    return (rtUniCpFlags(CodePoint) & RTUNI_ALPHA) != 0;
}


/**
 * Checks if a unicode code point is a decimal digit.
 *
 * @returns true if it is.
 * @returns false if it isn't.
 * @param   CodePoint       The code point.
 */
DECLINLINE(bool) RTUniCpIsDecDigit(RTUNICP CodePoint)
{
    return (rtUniCpFlags(CodePoint) & RTUNI_DDIGIT) != 0;
}


/**
 * Checks if a unicode code point is a hexadecimal digit.
 *
 * @returns true if it is.
 * @returns false if it isn't.
 * @param   CodePoint       The code point.
 */
DECLINLINE(bool) RTUniCpIsHexDigit(RTUNICP CodePoint)
{
    return (rtUniCpFlags(CodePoint) & RTUNI_XDIGIT) != 0;
}


/**
 * Checks if a unicode code point is white space.
 *
 * @returns true if it is.
 * @returns false if it isn't.
 * @param   CodePoint       The code point.
 */
DECLINLINE(bool) RTUniCpIsSpace(RTUNICP CodePoint)
{
    return (rtUniCpFlags(CodePoint) & RTUNI_WSPACE) != 0;
}



/**
 * Array of uppercase ranges.
 * @internal
 */
extern RTDATADECL(const RTUNICASERANGE) g_aRTUniUpperRanges[];

/**
 * Array of lowercase ranges.
 * @internal
 */
extern RTDATADECL(const RTUNICASERANGE) g_aRTUniLowerRanges[];


/**
 * Folds a unicode code point using the specified range array.
 *
 * @returns FOlded code point.
 * @param   CodePoint       The unicode code point to fold.
 * @param   pCur            The case folding range to use.
 */
DECLINLINE(RTUNICP) rtUniCpFold(RTUNICP CodePoint, PCRTUNICASERANGE pCur)
{
    do
    {
        if (pCur->EndCP > CodePoint)
        {
            if (pCur->BeginCP <= CodePoint)
                CodePoint = pCur->paFoldedCPs[CodePoint - pCur->BeginCP];
            break;
        }
        pCur++;
    } while (pCur->EndCP != RTUNICP_MAX);
    return CodePoint;
}


/**
 * Folds a unicode code point to upper case.
 *
 * @returns Folded code point.
 * @param   CodePoint       The unicode code point to fold.
 */
DECLINLINE(RTUNICP) RTUniCpToUpper(RTUNICP CodePoint)
{
    return rtUniCpFold(CodePoint, &g_aRTUniUpperRanges[0]);
}


/**
 * Folds a unicode code point to lower case.
 *
 * @returns Folded code point.
 * @param   CodePoint       The unicode code point to fold.
 */
DECLINLINE(RTUNICP) RTUniCpToLower(RTUNICP CodePoint)
{
    return rtUniCpFold(CodePoint, &g_aRTUniLowerRanges[0]);
}


#else /* RTUNI_USE_WCTYPE */


/**
 * Checks if a unicode code point is upper case.
 *
 * @returns true if it is.
 * @returns false if it isn't.
 * @param   CodePoint       The code point.
 */
DECLINLINE(bool) RTUniCpIsUpper(RTUNICP CodePoint)
{
    return !!iswupper(CodePoint);
}


/**
 * Checks if a unicode code point is lower case.
 *
 * @returns true if it is.
 * @returns false if it isn't.
 * @param   CodePoint       The code point.
 */
DECLINLINE(bool) RTUniCpIsLower(RTUNICP CodePoint)
{
    return !!iswlower(CodePoint);
}


/**
 * Checks if a unicode code point is alphabetic.
 *
 * @returns true if it is.
 * @returns false if it isn't.
 * @param   CodePoint       The code point.
 */
DECLINLINE(bool) RTUniCpIsAlphabetic(RTUNICP CodePoint)
{
    return !!iswalpha(CodePoint);
}


/**
 * Checks if a unicode code point is a decimal digit.
 *
 * @returns true if it is.
 * @returns false if it isn't.
 * @param   CodePoint       The code point.
 */
DECLINLINE(bool) RTUniCpIsDecDigit(RTUNICP CodePoint)
{
    return !!iswdigit(CodePoint);
}


/**
 * Checks if a unicode code point is a hexadecimal digit.
 *
 * @returns true if it is.
 * @returns false if it isn't.
 * @param   CodePoint       The code point.
 */
DECLINLINE(bool) RTUniCpIsHexDigit(RTUNICP CodePoint)
{
    return !!iswxdigit(CodePoint);
}


/**
 * Checks if a unicode code point is white space.
 *
 * @returns true if it is.
 * @returns false if it isn't.
 * @param   CodePoint       The code point.
 */
DECLINLINE(bool) RTUniCpIsSpace(RTUNICP CodePoint)
{
    return !!iswspace(CodePoint);
}


/**
 * Folds a unicode code point to upper case.
 *
 * @returns Folded code point.
 * @param   CodePoint       The unicode code point to fold.
 */
DECLINLINE(RTUNICP) RTUniCpToUpper(RTUNICP CodePoint)
{
    return towupper(CodePoint);
}


/**
 * Folds a unicode code point to lower case.
 *
 * @returns Folded code point.
 * @param   CodePoint       The unicode code point to fold.
 */
DECLINLINE(RTUNICP) RTUniCpToLower(RTUNICP CodePoint)
{
    return towlower(CodePoint);
}


#endif /* RTUNI_USE_WCTYPE */


/**
 * Frees a unicode string.
 *
 * @param   pusz        The string to free.
 */
RTDECL(void) RTUniFree(PRTUNICP pusz);


__END_DECLS
/** @} */


#endif

