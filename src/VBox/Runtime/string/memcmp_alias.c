/* $Id$ */
/** @file
 * InnoTek Portable Runtime - No-CRT memcmp() alias for gcc.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/nocrt/string.h>
#undef memcmp

#if defined(__DARWIN__) || defined(__WIN__)
# ifndef __MINGW32__
#  pragma weak memcmp
# endif

/* No alias support here (yet in the ming case). */
extern int (memcmp)(const void *pv1, const void *pv2, size_t cb)
{
    return RT_NOCRT(memcmp)(pv1, pv2, cb);
}

#elif __GNUC__ >= 4
/* create a weak alias. */
__asm__(".weak memcmp\t\n"
        " .set memcmp," RT_NOCRT_STR(memcmp) "\t\n");
#else
/* create a weak alias. */
extern __typeof(RT_NOCRT(memcmp)) memcmp __attribute__((weak, alias(RT_NOCRT_STR(memcmp))));
#endif

