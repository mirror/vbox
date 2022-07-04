/* $Id$ */
/** @file
 * BS3Kit - Bs3RegCtxSaveForMode
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


#undef Bs3RegCtxSaveForMode
BS3_CMN_DEF(void, Bs3RegCtxSaveForMode,(PBS3REGCTX pRegCtx, uint8_t bMode, uint16_t cbExtraStack))
{
    if (   bMode != BS3_MODE_RM
#if ARCH_BIT == 16
        || g_bBs3CurrentMode == BS3_MODE_RM
#endif
       )
    {
        BS3_ASSERT((bMode & BS3_MODE_SYS_MASK) == (g_bBs3CurrentMode & BS3_MODE_SYS_MASK));
        Bs3RegCtxSaveEx(pRegCtx, bMode, cbExtraStack);
    }
    else
    {
#if ARCH_BIT == 64
        BS3_ASSERT(0); /* No V86 mode in LM! */
#endif
        Bs3RegCtxSaveEx(pRegCtx, (bMode & ~BS3_MODE_CODE_MASK) | BS3_MODE_CODE_V86, cbExtraStack);
        Bs3RegCtxConvertV86ToRm(pRegCtx);
    }
}

