/** @file
 * IPRT / No-CRT - Our own assert.h header.
 */

/*
 * Copyright (C) 2022 Oracle Corporation
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

#ifndef IPRT_INCLUDED_nocrt_assert_h
#define IPRT_INCLUDED_nocrt_assert_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/assert.h>

DECL_FORCE_INLINE(void) rtCrtAssertPanic(void)
{
    RTAssertPanic();
}

/* Mesa uses assert() in such a way that we must not have any 'do {} while'
   wrappers in the expansion, so we partially cook our own assert here but
   using the standard iprt/assert.h building blocks. */
#define assert(a_Expr) (RT_LIKELY(!!(a_Expr)) ? (void)0 \
                        : RTAssertMsg1Weak((const char *)0, __LINE__, __FILE__, RT_GCC_EXTENSION __PRETTY_FUNCTION__), \
                          rtCrtAssertPanic(), (void)0 )

#endif /* !IPRT_INCLUDED_nocrt_assert_h */

