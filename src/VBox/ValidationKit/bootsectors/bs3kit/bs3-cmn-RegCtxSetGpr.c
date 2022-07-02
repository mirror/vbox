/* $Id$ */
/** @file
 * BS3Kit - Bs3RegCtxSetGpr
 */

/*
 * Copyright (C) 2007-2022 Oracle Corporation
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
#include "bs3kit-template-header.h"


#undef Bs3RegCtxSetGpr
BS3_CMN_DEF(bool, Bs3RegCtxSetGpr,(PBS3REGCTX pRegCtx, uint8_t iGpr, uint64_t uValue, uint8_t cb))
{
    if (iGpr < 16)
    {
        PBS3REG pGpr = &pRegCtx->rax + iGpr;
        switch (cb)
        {
            case 1: pGpr->u8  = (uint8_t)uValue; break;
            case 2: pGpr->u16 = (uint16_t)uValue; break;
            case 4: pGpr->u32 = (uint32_t)uValue; break;
            case 8: pGpr->u64 = uValue; break;
            default:
                BS3_ASSERT(false);
                return false;
        }
        return true;
    }
    return false;
}

