/* $Id$ */
/** @file
 * innotek Portable Runtime - Internal RTRand header
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

#ifndef ___internal_rand_h
#define ___internal_rand_h

#include <iprt/types.h>

__BEGIN_DECLS

/**
 * Initialize OS facilities for generating random bytes.
 */
void rtRandLazyInitNative(void);

/**
 * Generate random bytes using OS facilities.
 *
 * @returns VINF_SUCCESS on success, some error status code on failure.
 * @param   pv      Where to store the random bytes.
 * @param   cb      How many random bytes to store.
 */
int rtRandGenBytesNative(void *pv, size_t cb);

void rtRandGenBytesFallback(void *pv, size_t cb);

__END_DECLS

#endif
