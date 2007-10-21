/* $Id$ */
/** @file
 * innotek Portable Runtime - CRT Strings, memset().
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
 * Fill a memory block with specific byte.
 *
 * @returns pvDst.
 * @param   pvDst      Pointer to the block.
 * @param   ch      The filler char.
 * @param   cb      The size of the block.
 */
#ifdef _MSC_VER
# if _MSC_VER >= 1400
void *  __cdecl memset(__out_bcount_full_opt(_Size) void *pvDst, __in int ch, __in size_t cb)
# else
void *memset(void *pvDst, int ch, size_t cb)
# endif
#else
void *memset(void *pvDst, int ch, size_t cb)
#endif
{
    register union
    {
        uint8_t  *pu8;
        uint32_t *pu32;
        void     *pvDst;
    } u;
    u.pvDst = pvDst;

    /* 32-bit word moves. */
    register uint32_t u32 = ch | (ch << 8);
    u32 |= u32 << 16;
    register size_t c = cb >> 2;
    while (c-- > 0)
        *u.pu32++ = u32;

    /* Remaining byte moves. */
    c = cb & 3;
    while (c-- > 0)
        *u.pu8++ = (uint8_t)u32;

    return pvDst;
}

