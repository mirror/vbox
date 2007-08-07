/* $Id$ */
/** @file
 * innotek Portable Runtime - No-CRT strchr() alias for gcc.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/nocrt/string.h>
#undef strchr

#if defined(RT_OS_DARWIN) || defined(RT_OS_WINDOWS)
# ifndef __MINGW32__
#  pragma weak strchr
# endif

/* No alias support here (yet in the ming case). */
extern char *(strchr)(const char *psz, int ch)
{
    return RT_NOCRT(strchr)(psz, ch);
}

#elif __GNUC__ >= 4
/* create a weak alias. */
__asm__(".weak strchr\t\n"
        " .set strchr," RT_NOCRT_STR(strchr) "\t\n");
#else
/* create a weak alias. */
extern __typeof(RT_NOCRT(strchr)) strchr __attribute__((weak, alias(RT_NOCRT_STR(strchr))));
#endif

