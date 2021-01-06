/* $Id$ */
/** @file
 * IPRT - ASMMemZeroPage - generic C implementation.
 */

/*
 * Copyright (C) 2006-2021 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/asm.h>
#include "internal/iprt.h"


DECLASM(void *) ASMMemFirstMismatchingU8(void const RT_FAR *pv, size_t cb, uint8_t u8) RT_NOTHROW_DEF
{
    uint8_t const *pb = (uint8_t const RT_FAR *)pv;
    for (; cb; cb--, pb++)
        if (RT_LIKELY(*pb == u8))
        { /* likely */ }
        else
            return (void *)pb;
    return NULL;
}

