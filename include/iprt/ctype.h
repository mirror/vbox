/** @file
 * innotek Portable Runtime - ctype.h wrapper and C locale variants.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___iprt_ctype_h
#define ___iprt_ctype_h

#ifdef IN_RING3
# include <ctype.h>

# if defined(_MSC_VER) && !defined(isblank)
/* isblank for MSC */
#  define isblank(ch)  ( (ch) == ' ' || (ch) == '\t' )
# endif
#endif /* IN_RING3 */

/** @name C locale macros.
 *
 * @remarks The macros may reference the character more than once.
 * @remarks Assumes a charset similar to ASCII.
 * @remarks Can probably be optimized if someone has time.
 * @{ */
#define RT_C_IS_BLANK(ch)   ( (ch) == ' ' || (ch) == '\t' )
#define RT_C_IS_ALNUM(ch)   ( RT_C_IS_DIGIT(ch) || RT_C_IS_ALPHA(ch) )
#define RT_C_IS_ALPHA(ch)   ( RT_C_IS_LOWER(ch) || RT_C_IS_UPPER(ch) )
#define RT_C_IS_CNTRL(ch)   ( (ch) >= 0   && (ch) <  32 )
#define RT_C_IS_DIGIT(ch)   ( (ch) >= '0' && (ch) <= '9' )
#define RT_C_IS_LOWER(ch)   ( (ch) >= 'a' && (ch) <= 'z' )
#define RT_C_IS_GRAPH(ch)   ( RT_C_IS_PRINT(ch) && !RT_C_IS_BLANK(ch) )
#define RT_C_IS_PRINT(ch)   ( (ch) >= 32  && (ch) < 127 ) /**< @todo possibly incorrect */
#define RT_C_IS_PUNCT(ch)   ( (ch) == ',' || (ch) == '.'  || (ch) == ':'  || (ch) == ';'  || (ch) == '!'  || (ch) == '?' ) /**< @todo possibly incorrect */
#define RT_C_IS_SPACE(ch)   ( (ch) == ' ' || (ch) == '\t' || (ch) == '\n' || (ch) == '\r' || (ch) == '\f' || (ch) == '\v' )
#define RT_C_IS_UPPER(ch)   ( (ch) >= 'A' && (ch) <= 'Z' )
#define RT_C_IS_XDIGIT(ch)  ( RT_C_IS_DIGIT(ch) || ((ch) >= 'a' && (ch) <= 'f') || ((ch) >= 'A' && (ch) <= 'F') )

#define RT_C_TO_LOWER(ch)   ( RT_C_IS_UPPER(ch) ? (ch) + ('a' - 'A') : (ch) )
#define RT_C_TO_UPPER(ch)   ( RT_C_IS_LOWER(ch) ? (ch) - ('a' - 'A') : (ch) )
/** @} */

#endif

