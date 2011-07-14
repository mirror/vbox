/* $Id$ */
/** @file
 * IEM - Instruction Implementation in Assembly, portable C variant.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "IEMInternal.h"
#include <VBox/vmm/vm.h>
#include <iprt/x86.h>

#if 0


IEM_DECL_IMPL_DEF(void, iemImpl_add_u8,(uint8_t  *pu8Dst,  uint8_t  u8Src,  uint32_t *pEFlags))
{
    /* incorrect sketch (testing fastcall + gcc) */
    uint8_t u8Dst = *pu8Dst;
    uint8_t u8Res = u8Dst + u8Src;
    *pu8Dst = u8Res;

    if (u8Res)
        *pEFlags &= X86_EFL_ZF;
    else
        *pEFlags |= X86_EFL_ZF;
}

IEM_DECL_IMPL_DEF(void, iemImpl_add_u8_locked,(uint8_t  *pu8Dst,  uint8_t  u8Src,  uint32_t *pEFlags))
{
    iemImpl_add_u8(pu8Dst, u8Src, pEFlags);
}


#endif

