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
 * Search a memory block for a character.
 *
 * @returns Pointer to the first instance of ch in pv.
 * @returns NULL if ch wasn't found.
 * @param   pv          Pointer to the block to search.
 * @param   ch          The character to search for.
 * @param   cb          The size of the block.
 */
#ifdef _MSC_VER /* Silly 'safeness' from MS. */
# if _MSC_VER >= 1400
_CRTIMP __checkReturn _CONST_RETURN void *  __cdecl memchr( __in_bcount_opt(_MaxCount) const void * pv, __in int ch, __in size_t cb)
# else
void *memchr(const void *pv, int ch, size_t cb)
# endif
#else
void *memchr(const void *pv, int ch, size_t cb)
#endif
{
    register uint8_t const *pu8 = (uint8_t const *)pv;
    register size_t cb2 = cb;
    while (cb2-- > 0)
    {
        if (*pu8 == ch)
            return (void *)pu8;
        pu8++;
    }
    return NULL;
}

