/* $Id$ */
/** @file
 * innotek Portable Runtime - Memory Allocation.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/string.h>


/**
 * Duplicates a chunk of memory into a new heap block.
 *
 * @returns New heap block with the duplicate data.
 * @returns NULL if we're out of memory.
 * @param   pvSrc   The memory to duplicate.
 * @param   cb      The amount of memory to duplicate.
 */
RTDECL(void *) RTMemDup(const void *pvSrc, size_t cb)
{
    void *pvDst = RTMemAlloc(cb);
    if (pvDst)
        memcpy(pvDst, pvSrc, cb);
    return pvDst;
}


/**
 * Duplicates a chunk of memory into a new heap block with some
 * additional zeroed memory.
 *
 * @returns New heap block with the duplicate data.
 * @returns NULL if we're out of memory.
 * @param   pvSrc   The memory to duplicate.
 * @param   cbSrc   The amount of memory to duplicate.
 * @param   cbExtra The amount of extra memory to allocate and zero.
 */
RTDECL(void *) RTMemDupEx(const void *pvSrc, size_t cbSrc, size_t cbExtra)
{
    void *pvDst = RTMemAlloc(cbSrc + cbExtra);
    if (pvDst)
    {
        memcpy(pvDst, pvSrc, cbSrc);
        memset((uint8_t *)pvDst + cbSrc, 0, cbExtra);
    }
    return pvDst;
}


