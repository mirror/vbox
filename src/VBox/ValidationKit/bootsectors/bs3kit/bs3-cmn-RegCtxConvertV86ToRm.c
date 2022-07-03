/* $Id$ */
/** @file
 * BS3Kit - Bs3RegCtxConvertV86ToRm
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


#undef Bs3RegCtxConvertV86ToRm
BS3_CMN_DEF(void, Bs3RegCtxConvertV86ToRm,(PBS3REGCTX pRegCtx))
{
    BS3_ASSERT(BS3_MODE_IS_V86(pRegCtx->bMode));

    pRegCtx->cr0.u32    &= ~(X86_CR0_PE | X86_CR0_PG);
    pRegCtx->rflags.u32 &= ~X86_EFL_VM;
    pRegCtx->fbFlags    |= BS3REG_CTX_F_NO_TR_LDTR | BS3REG_CTX_F_NO_AMD64;
    pRegCtx->bCpl        = 0;
    pRegCtx->bMode       = BS3_MODE_RM;
}

