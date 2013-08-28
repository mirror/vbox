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
 * There are a few 64-bit on 32-bit things we'd rather do in C.  Actually, doing
 * it all in C is probably safer atm., optimize what's necessary later, maybe.
 */


/* Binary ops */

IEM_DECL_IMPL_DEF(void, iemAImpl_add_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    AssertFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_adc_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    AssertFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_sub_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    AssertFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_sbb_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    AssertFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_or_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    AssertFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_xor_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    AssertFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_and_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    AssertFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_cmp_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    AssertFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_test_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    AssertFailed();
}


/** 64-bit locked binary operand operation. */
# define DO_LOCKED_BIN_OP_U64(a_Mnemonic) \
    do { \
        uint64_t uOld = ASMAtomicReadU64(puDst); \
        uint64_t uTmp; \
        uint32_t fEflTmp; \
        do \
        { \
            uTmp = uOld; \
            fEflTmp = *pfEFlags; \
            iemAImpl_ ## a_Mnemonic ## _u64(&uTmp, uSrc, &fEflTmp); \
        } while (ASMAtomicCmpXchgExU64(puDst, uTmp, uOld, &uOld)); \
        *pfEFlags = fEflTmp; \
    } while (0)


IEM_DECL_IMPL_DEF(void, iemAImpl_add_u64_locked,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    DO_LOCKED_BIN_OP_U64(adc);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_adc_u64_locked,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    DO_LOCKED_BIN_OP_U64(adc);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_sub_u64_locked,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    DO_LOCKED_BIN_OP_U64(sub);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_sbb_u64_locked,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    DO_LOCKED_BIN_OP_U64(sbb);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_or_u64_locked,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    DO_LOCKED_BIN_OP_U64(or);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_xor_u64_locked,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    DO_LOCKED_BIN_OP_U64(xor);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_and_u64_locked,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    DO_LOCKED_BIN_OP_U64(and);
}


/* Bit operations (same signature as above). */

IEM_DECL_IMPL_DEF(void, iemAImpl_bt_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, ZF, AF, PF. */
    Assert(uSrc < 64);
    if (*puDst & RT_BIT_64(uSrc))
        *pfEFlags |= X86_EFL_CF;
    else
        *pfEFlags &= ~X86_EFL_CF;
}

IEM_DECL_IMPL_DEF(void, iemAImpl_btc_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, ZF, AF, PF. */
    Assert(uSrc < 64);
    uint64_t fMask = RT_BIT_64(uSrc);
    if (*puDst & fMask)
    {
        *puDst    &= ~fMask;
        *pfEFlags |= X86_EFL_CF;
    }
    else
    {
        *puDst    |= ~fMask;
        *pfEFlags &= ~X86_EFL_CF;
    }
}

IEM_DECL_IMPL_DEF(void, iemAImpl_btr_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, ZF, AF, PF. */
    uint64_t fMask = RT_BIT_64(uSrc);
    if (*puDst & fMask)
        *pfEFlags |= X86_EFL_CF;
    else
        *pfEFlags &= ~X86_EFL_CF;
    *puDst &= ~fMask;
}

IEM_DECL_IMPL_DEF(void, iemAImpl_bts_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, ZF, AF, PF. */
    uint64_t fMask = RT_BIT_64(uSrc);
    if (*puDst & fMask)
        *pfEFlags |= X86_EFL_CF;
    else
        *pfEFlags &= ~X86_EFL_CF;
    *puDst |= fMask;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_btc_u64_locked,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    DO_LOCKED_BIN_OP_U64(btc);
}

IEM_DECL_IMPL_DEF(void, iemAImpl_btr_u64_locked,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    DO_LOCKED_BIN_OP_U64(btr);
}

IEM_DECL_IMPL_DEF(void, iemAImpl_bts_u64_locked,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    DO_LOCKED_BIN_OP_U64(bts);
}


/* bit scan */

IEM_DECL_IMPL_DEF(void, iemAImpl_bsf_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, AF, PF, CF. */
    if (uSrc)
    {
        uint8_t  iBit;
        uint32_t u32Src;
        if (uSrc & UINT32_MAX)
        {
            iBit = 0;
            u32Src = uSrc;
        }
        else
        {
            iBit = 32;
            u32Src = uSrc >> 32;
        }
        if (!(u32Src & UINT16_MAX))
        {
            iBit += 16;
            u32Src >>= 16;
        }
        if (!(u32Src & UINT8_MAX))
        {
            iBit += 8;
            u32Src >>= 8;
        }
        if (!(u32Src & 0xf))
        {
            iBit += 4;
            u32Src >>= 4;
        }
        if (!(u32Src & 0x3))
        {
            iBit += 2;
            u32Src >>= 2;
        }
        if (!(u32Src & 1))
        {
            iBit += 1;
            Assert(u32Src & 2);
        }

        *puDst     = iBit;
        *pfEFlags &= ~X86_EFL_ZF;
    }
    else
        *pfEFlags |= X86_EFL_ZF;
}

IEM_DECL_IMPL_DEF(void, iemAImpl_bsr_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, AF, PF, CF. */
    if (uSrc)
    {
        uint8_t  iBit;
        uint32_t u32Src;
        if (uSrc & UINT64_C(0xffffffff00000000))
        {
            iBit = 64;
            u32Src = uSrc >> 32;
        }
        else
        {
            iBit = 32;
            u32Src = uSrc;
        }
        if (!(u32Src & UINT32_C(0xffff0000)))
        {
            iBit -= 16;
            u32Src <<= 16;
        }
        if (!(u32Src & UINT32_C(0xff000000)))
        {
            iBit -= 8;
            u32Src <<= 8;
        }
        if (!(u32Src & UINT32_C(0xf0000000)))
        {
            iBit -= 4;
            u32Src <<= 4;
        }
        if (!(u32Src & UINT32_C(0xc0000000)))
        {
            iBit -= 2;
            u32Src <<= 2;
        }
        if (!(u32Src & UINT32_C(0x10000000)))
        {
            iBit -= 1;
            u32Src <<= 1;
            Assert(u32Src & RT_BIT_64(63));
        }

        *puDst     = iBit;
        *pfEFlags &= ~X86_EFL_ZF;
    }
    else
        *pfEFlags |= X86_EFL_ZF;
}


/* Unary operands. */

IEM_DECL_IMPL_DEF(void, iemAImpl_inc_u64,(uint64_t  *puDst,  uint32_t *pEFlags))
{
    AssertFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_dec_u64,(uint64_t  *puDst,  uint32_t *pEFlags))
{
    AssertFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_not_u64,(uint64_t  *puDst,  uint32_t *pEFlags))
{
    AssertFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_neg_u64,(uint64_t  *puDst,  uint32_t *pEFlags))
{
    AssertFailed();
}


/** 64-bit locked unary operand operation. */
# define DO_LOCKED_UNARY_OP_U64(a_Mnemonic) \
    do { \
        uint64_t uOld = ASMAtomicReadU64(puDst); \
        uint64_t uTmp; \
        uint32_t fEflTmp; \
        do \
        { \
            uTmp = uOld; \
            fEflTmp = *pfEFlags; \
            iemAImpl_ ## a_Mnemonic ## _u64(&uTmp, &fEflTmp); \
        } while (ASMAtomicCmpXchgExU64(puDst, uTmp, uOld, &uOld)); \
        *pfEFlags = fEflTmp; \
    } while (0)

IEM_DECL_IMPL_DEF(void, iemAImpl_inc_u64_locked,(uint64_t  *puDst,  uint32_t *pfEFlags))
{
    DO_LOCKED_UNARY_OP_U64(inc);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_dec_u64_locked,(uint64_t  *puDst,  uint32_t *pfEFlags))
{
    DO_LOCKED_UNARY_OP_U64(dec);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_not_u64_locked,(uint64_t  *puDst,  uint32_t *pfEFlags))
{
    DO_LOCKED_UNARY_OP_U64(not);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_neg_u64_locked,(uint64_t  *puDst,  uint32_t *pfEFlags))
{
    DO_LOCKED_UNARY_OP_U64(neg);
}


/* Shift and rotate. */

IEM_DECL_IMPL_DEF(void, iemAImpl_rol_u64,(uint64_t *puDst, uint8_t cShift, uint32_t *pfEFlags))
{
    AssertFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_ror_u64,(uint64_t *puDst, uint8_t cShift, uint32_t *pfEFlags))
{
    AssertFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_rcl_u64,(uint64_t *puDst, uint8_t cShift, uint32_t *pfEFlags))
{
    AssertFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_rcr_u64,(uint64_t *puDst, uint8_t cShift, uint32_t *pfEFlags))
{
    AssertFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_shl_u64,(uint64_t *puDst, uint8_t cShift, uint32_t *pfEFlags))
{
    AssertFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_shr_u64,(uint64_t *puDst, uint8_t cShift, uint32_t *pfEFlags))
{
    AssertFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_sar_u64,(uint64_t *puDst, uint8_t cShift, uint32_t *pfEFlags))
{
    AssertFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_shld_u64,(uint64_t *puDst, uint64_t uSrc, uint8_t cShift, uint32_t *pfEFlags))
{
    AssertFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_shrd_u64,(uint64_t *puDst, uint64_t uSrc, uint8_t cShift, uint32_t *pfEFlags))
{
    AssertFailed();
}


/* multiplication and division */

IEM_DECL_IMPL_DEF(int, iemAImpl_mul_u64,(uint64_t *pu64RAX, uint64_t *pu64RDX, uint64_t u64Factor, uint32_t *pfEFlags))
{
    AssertFailed();
    return -1;
}


IEM_DECL_IMPL_DEF(int, iemAImpl_imul_u64,(uint64_t *pu64RAX, uint64_t *pu64RDX, uint64_t u64Factor, uint32_t *pfEFlags))
{
    AssertFailed();
    return -1;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_imul_two_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    AssertFailed();
}



IEM_DECL_IMPL_DEF(int, iemAImpl_div_u64,(uint64_t *pu64RAX, uint64_t *pu64RDX, uint64_t u64Divisor, uint32_t *pfEFlags))
{
    AssertFailed();
    return -1;
}


IEM_DECL_IMPL_DEF(int, iemAImpl_idiv_u64,(uint64_t *pu64RAX, uint64_t *pu64RDX, uint64_t u64Divisor, uint32_t *pfEFlags))
{
    AssertFailed();
    return -1;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u64,(uint64_t *puMem, uint64_t *puReg))
{
    /* XCHG implies LOCK. */
    uint64_t uOldMem = *puMem;
    while (!ASMAtomicCmpXchgExU64(puMem, *puReg, uOldMem, &uOldMem))
        ASMNopPause();
    *puReg = uOldMem;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u64,(uint64_t *puDst, uint64_t *puReg, uint32_t *pfEFlags))
{
    AssertFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u64_locked,(uint64_t *puDst, uint64_t *puReg, uint32_t *pfEFlags))
{
    uint64_t uOld = ASMAtomicReadU64(puDst);
    uint64_t uTmpDst;
    uint64_t uTmpReg;
    uint32_t fEflTmp;
    do
    {
        uTmpDst = uOld;
        uTmpReg = *puReg;
        fEflTmp = *pfEFlags;
        iemAImpl_xadd_u64(&uTmpDst, &uTmpReg, &fEflTmp);
    } while (ASMAtomicCmpXchgExU64(puDst, uTmpDst, uOld, &uOld));
    *puReg    = uTmpReg;
    *pfEFlags = fEflTmp;
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

