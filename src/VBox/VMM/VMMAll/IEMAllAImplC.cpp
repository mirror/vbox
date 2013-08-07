/* $Id$ */
/** @file
 * IEM - Instruction Implementation in Assembly, portable C variant.
 */

/*
 * Copyright (C) 2011-2013 Oracle Corporation
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


#ifdef RT_ARCH_X86
/*
 * There are a few 64-bit on 32-bit things we'd rather do in C.
 */


IEM_DECL_IMPL_DEF(int, iemAImpl_mul_u64,(uint64_t *pu64RAX, uint64_t *pu64RDX, uint64_t u64Factor, uint32_t *pEFlags))
{
    AssertFailed();
    return -1;
}


IEM_DECL_IMPL_DEF(int, iemAImpl_imul_u64,(uint64_t *pu64RAX, uint64_t *pu64RDX, uint64_t u64Factor, uint32_t *pEFlags))
{
    AssertFailed();
    return -1;
}


IEM_DECL_IMPL_DEF(int, iemAImpl_div_u64,(uint64_t *pu64RAX, uint64_t *pu64RDX, uint64_t u64Divisor, uint32_t *pEFlags))
{
    AssertFailed();
    return -1;
}


IEM_DECL_IMPL_DEF(int, iemAImpl_idiv_u64,(uint64_t *pu64RAX, uint64_t *pu64RDX, uint64_t u64Divisor, uint32_t *pEFlags))
{
    AssertFailed();
    return -1;
}

#endif /* RT_ARCH_X86 */


IEM_DECL_IMPL_DEF(void, iemAImpl_arpl,(uint16_t *pu16Dst, uint16_t u16Src, uint32_t *pEFlags))
{
    if ((*pu16Dst & X86_SEL_RPL) < (u16Src & X86_SEL_RPL))
    {
        *pu16Dst &= X86_SEL_MASK_OFF_RPL;
        *pu16Dst |= u16Src & X86_SEL_RPL;

        *pEFlags |= X86_EFL_ZF;
    }
    else
        *pEFlags &= ~X86_EFL_ZF;
}

