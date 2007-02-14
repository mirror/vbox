/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Internal RTRand header
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

#ifndef __internal_rand_h__
#define __internal_rand_h__

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
