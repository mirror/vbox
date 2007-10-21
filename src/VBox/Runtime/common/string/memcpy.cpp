/* $Id$ */
/** @file
 * innotek Portable Runtime - CRT Strings, memcpy().
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
#include <iprt/string.h>


/**
 * Copies a memory.
 *
 * @returns pvDst.
 * @param   pvDst       Pointer to the target block.
 * @param   pvSrc       Pointer to the source block.
 * @param   cb          The size of the block.
 */
#ifdef _MSC_VER
# if _MSC_VER >= 1400
_CRT_INSECURE_DEPRECATE_MEMORY(memcpy_s) void *  __cdecl memcpy(__out_bcount_full_opt(_Size) void * pvDst, __in_bcount_opt(_Size) const void * pvSrc, __in size_t cb)
# else
void *memcpy(void *pvDst, const void *pvSrc, size_t cb)
# endif
#else
void *memcpy(void *pvDst, const void *pvSrc, size_t cb)
#endif
{
    register union
    {
        uint8_t  *pu8;
        uint32_t *pu32;
        void     *pv;
    } uTrg;
    uTrg.pv = pvDst;

    register union
    {
        uint8_t const  *pu8;
        uint32_t const *pu32;
        void const     *pv;
    } uSrc;
    uSrc.pv = pvSrc;

    /* 32-bit word moves. */
    register size_t c = cb >> 2;
    while (c-- > 0)
        *uTrg.pu32++ = *uSrc.pu32++;

    /* Remaining byte moves. */
    c = cb & 3;
    while (c-- > 0)
        *uTrg.pu8++ = *uSrc.pu8++;

    return pvDst;
}

