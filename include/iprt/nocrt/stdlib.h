/** @file
 * IPRT / No-CRT - Our minimal stdlib.h.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
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

#ifndef IPRT_INCLUDED_nocrt_stdlib_h
#define IPRT_INCLUDED_nocrt_stdlib_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>
#include <iprt/mem.h>

RT_C_DECLS_BEGIN

typedef void FNRTNOCRTATEXITCALLBACK(void) /*RT_NOEXCEPT*/;
typedef FNRTNOCRTATEXITCALLBACK *PFNRTNOCRTATEXITCALLBACK;
#if defined(_MSC_VER) && defined(RT_WITHOUT_NOCRT_WRAPPERS) /* Clashes with compiler internal prototype or smth. */
int          rtnocrt_atexit(PFNRTNOCRTATEXITCALLBACK) RT_NOEXCEPT;
# define     atexit rtnocrt_atexit
#else
int          RT_NOCRT(atexit)(PFNRTNOCRTATEXITCALLBACK) RT_NOEXCEPT;
#endif

#if !defined(RT_WITHOUT_NOCRT_WRAPPERS) && !defined(RT_WITHOUT_NOCRT_WRAPPER_ALIASES)
# define atexit     RT_NOCRT(atexit)
#endif


#ifdef IPRT_NO_CRT_FOR_3RD_PARTY
/*
 * Only for external libraries and such.
 */

DECLINLINE(void *) RT_NOCRT(malloc)(size_t cb)
{
    return RTMemAlloc(cb);
}

DECLINLINE(void *) RT_NOCRT(calloc)(size_t cItems, size_t cbItem)
{
    return RTMemAllocZ(cItems * cbItem); /* caller responsible for overflow issues. */
}

DECLINLINE(void *) RT_NOCRT(realloc)(void *pvOld, size_t cbNew)
{
    return RTMemRealloc(pvOld, cbNew);
}

DECLINLINE(void) RT_NOCRT(free)(void *pv)
{
    RTMemFree(pv);
}

# if !defined(RT_WITHOUT_NOCRT_WRAPPERS) && !defined(RT_WITHOUT_NOCRT_WRAPPER_ALIASES)
#  define malloc    RT_NOCRT(malloc)
#  define calloc    RT_NOCRT(calloc)
#  define realloc   RT_NOCRT(realloc)
#  define free      RT_NOCRT(free)
# endif

#endif /* IPRT_NO_CRT_FOR_3RD_PARTY */


RT_C_DECLS_END

#endif /* !IPRT_INCLUDED_nocrt_stdlib_h */
