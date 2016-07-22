/* $Id$ */
/** @file
 * IEM - Instruction Decoding and Emulation.
 */

/*
 * Copyright (C) 2011-2015 Oracle Corporation
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
*   Global Variables                                                           *
*******************************************************************************/
extern const PFNIEMOP g_apfnOneByteMap[256]; /* not static since we need to forward declare it. */

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4702) /* Unreachable code like return in iemOp_Grp6_lldt. */
#endif


/**
 * Common worker for instructions like ADD, AND, OR, ++ with a byte
 * memory/register as the destination.
 *
 * @param   pImpl       Pointer to the instruction implementation (assembly).
 */
FNIEMOP_DEF_1(iemOpHlpBinaryOperator_rm_r8, PCIEMOPBINSIZES, pImpl)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(3, 0);
        IEM_MC_ARG(uint8_t *,  pu8Dst,  0);
        IEM_MC_ARG(uint8_t,    u8Src,   1);
        IEM_MC_ARG(uint32_t *, pEFlags, 2);

        IEM_MC_FETCH_GREG_U8(u8Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_REF_GREG_U8(pu8Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        IEM_MC_REF_EFLAGS(pEFlags);
        IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU8, pu8Dst, u8Src, pEFlags);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /*
         * We're accessing memory.
         * Note! We're putting the eflags on the stack here so we can commit them
         *       after the memory.
         */
        uint32_t const fAccess = pImpl->pfnLockedU8 ? IEM_ACCESS_DATA_RW : IEM_ACCESS_DATA_R; /* CMP,TEST */
        IEM_MC_BEGIN(3, 2);
        IEM_MC_ARG(uint8_t *,  pu8Dst,           0);
        IEM_MC_ARG(uint8_t,    u8Src,            1);
        IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 2);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        if (!pImpl->pfnLockedU8)
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_MEM_MAP(pu8Dst, fAccess, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
        IEM_MC_FETCH_GREG_U8(u8Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_FETCH_EFLAGS(EFlags);
        if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
            IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU8, pu8Dst, u8Src, pEFlags);
        else
            IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnLockedU8, pu8Dst, u8Src, pEFlags);

        IEM_MC_MEM_COMMIT_AND_UNMAP(pu8Dst, fAccess);
        IEM_MC_COMMIT_EFLAGS(EFlags);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/**
 * Common worker for word/dword/qword instructions like ADD, AND, OR, ++ with
 * memory/register as the destination.
 *
 * @param   pImpl       Pointer to the instruction implementation (assembly).
 */
FNIEMOP_DEF_1(iemOpHlpBinaryOperator_rm_rv, PCIEMOPBINSIZES, pImpl)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint16_t *, pu16Dst, 0);
                IEM_MC_ARG(uint16_t,   u16Src,  1);
                IEM_MC_ARG(uint32_t *, pEFlags, 2);

                IEM_MC_FETCH_GREG_U16(u16Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU16, pu16Dst, u16Src, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint32_t *, pu32Dst, 0);
                IEM_MC_ARG(uint32_t,   u32Src,  1);
                IEM_MC_ARG(uint32_t *, pEFlags, 2);

                IEM_MC_FETCH_GREG_U32(u32Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_GREG_U32(pu32Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, u32Src, pEFlags);

                if (pImpl != &g_iemAImpl_test)
                    IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32Dst);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint64_t *, pu64Dst, 0);
                IEM_MC_ARG(uint64_t,   u64Src,  1);
                IEM_MC_ARG(uint32_t *, pEFlags, 2);

                IEM_MC_FETCH_GREG_U64(u64Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_GREG_U64(pu64Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU64, pu64Dst, u64Src, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;
        }
    }
    else
    {
        /*
         * We're accessing memory.
         * Note! We're putting the eflags on the stack here so we can commit them
         *       after the memory.
         */
        uint32_t const fAccess = pImpl->pfnLockedU8 ? IEM_ACCESS_DATA_RW : IEM_ACCESS_DATA_R /* CMP,TEST */;
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint16_t *, pu16Dst,          0);
                IEM_MC_ARG(uint16_t,   u16Src,           1);
                IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                if (!pImpl->pfnLockedU16)
                    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MEM_MAP(pu16Dst, fAccess, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_GREG_U16(u16Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_FETCH_EFLAGS(EFlags);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU16, pu16Dst, u16Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnLockedU16, pu16Dst, u16Src, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu16Dst, fAccess);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint32_t *, pu32Dst,          0);
                IEM_MC_ARG(uint32_t,   u32Src,           1);
                IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                if (!pImpl->pfnLockedU32)
                    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MEM_MAP(pu32Dst, fAccess, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_GREG_U32(u32Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_FETCH_EFLAGS(EFlags);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, u32Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnLockedU32, pu32Dst, u32Src, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu32Dst, fAccess);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint64_t *, pu64Dst,          0);
                IEM_MC_ARG(uint64_t,   u64Src,           1);
                IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                if (!pImpl->pfnLockedU64)
                    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MEM_MAP(pu64Dst, fAccess, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_GREG_U64(u64Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_FETCH_EFLAGS(EFlags);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU64, pu64Dst, u64Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnLockedU64, pu64Dst, u64Src, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu64Dst, fAccess);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;
        }
    }
    return VINF_SUCCESS;
}


/**
 * Common worker for byte instructions like ADD, AND, OR, ++ with a register as
 * the destination.
 *
 * @param   pImpl       Pointer to the instruction implementation (assembly).
 */
FNIEMOP_DEF_1(iemOpHlpBinaryOperator_r8_rm, PCIEMOPBINSIZES, pImpl)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(3, 0);
        IEM_MC_ARG(uint8_t *,  pu8Dst,  0);
        IEM_MC_ARG(uint8_t,    u8Src,   1);
        IEM_MC_ARG(uint32_t *, pEFlags, 2);

        IEM_MC_FETCH_GREG_U8(u8Src, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        IEM_MC_REF_GREG_U8(pu8Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_REF_EFLAGS(pEFlags);
        IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU8, pu8Dst, u8Src, pEFlags);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /*
         * We're accessing memory.
         */
        IEM_MC_BEGIN(3, 1);
        IEM_MC_ARG(uint8_t *,  pu8Dst,  0);
        IEM_MC_ARG(uint8_t,    u8Src,   1);
        IEM_MC_ARG(uint32_t *, pEFlags, 2);
        IEM_MC_LOCAL(RTGCPTR,  GCPtrEffDst);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_FETCH_MEM_U8(u8Src, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
        IEM_MC_REF_GREG_U8(pu8Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_REF_EFLAGS(pEFlags);
        IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU8, pu8Dst, u8Src, pEFlags);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/**
 * Common worker for word/dword/qword instructions like ADD, AND, OR, ++ with a
 * register as the destination.
 *
 * @param   pImpl       Pointer to the instruction implementation (assembly).
 */
FNIEMOP_DEF_1(iemOpHlpBinaryOperator_rv_rm, PCIEMOPBINSIZES, pImpl)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint16_t *, pu16Dst, 0);
                IEM_MC_ARG(uint16_t,   u16Src,  1);
                IEM_MC_ARG(uint32_t *, pEFlags, 2);

                IEM_MC_FETCH_GREG_U16(u16Src, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_GREG_U16(pu16Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU16, pu16Dst, u16Src, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint32_t *, pu32Dst, 0);
                IEM_MC_ARG(uint32_t,   u32Src,  1);
                IEM_MC_ARG(uint32_t *, pEFlags, 2);

                IEM_MC_FETCH_GREG_U32(u32Src, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_GREG_U32(pu32Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, u32Src, pEFlags);

                IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32Dst);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint64_t *, pu64Dst, 0);
                IEM_MC_ARG(uint64_t,   u64Src,  1);
                IEM_MC_ARG(uint32_t *, pEFlags, 2);

                IEM_MC_FETCH_GREG_U64(u64Src, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_GREG_U64(pu64Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU64, pu64Dst, u64Src, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;
        }
    }
    else
    {
        /*
         * We're accessing memory.
         */
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 1);
                IEM_MC_ARG(uint16_t *, pu16Dst, 0);
                IEM_MC_ARG(uint16_t,   u16Src,  1);
                IEM_MC_ARG(uint32_t *, pEFlags, 2);
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U16(u16Src, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
                IEM_MC_REF_GREG_U16(pu16Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU16, pu16Dst, u16Src, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(3, 1);
                IEM_MC_ARG(uint32_t *, pu32Dst, 0);
                IEM_MC_ARG(uint32_t,   u32Src,  1);
                IEM_MC_ARG(uint32_t *, pEFlags, 2);
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U32(u32Src, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
                IEM_MC_REF_GREG_U32(pu32Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, u32Src, pEFlags);

                IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32Dst);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 1);
                IEM_MC_ARG(uint64_t *, pu64Dst, 0);
                IEM_MC_ARG(uint64_t,   u64Src,  1);
                IEM_MC_ARG(uint32_t *, pEFlags, 2);
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U64(u64Src, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
                IEM_MC_REF_GREG_U64(pu64Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU64, pu64Dst, u64Src, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;
        }
    }
    return VINF_SUCCESS;
}


/**
 * Common worker for instructions like ADD, AND, OR, ++ with working on AL with
 * a byte immediate.
 *
 * @param   pImpl       Pointer to the instruction implementation (assembly).
 */
FNIEMOP_DEF_1(iemOpHlpBinaryOperator_AL_Ib, PCIEMOPBINSIZES, pImpl)
{
    uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(3, 0);
    IEM_MC_ARG(uint8_t *,       pu8Dst,             0);
    IEM_MC_ARG_CONST(uint8_t,   u8Src,/*=*/ u8Imm,  1);
    IEM_MC_ARG(uint32_t *,      pEFlags,            2);

    IEM_MC_REF_GREG_U8(pu8Dst, X86_GREG_xAX);
    IEM_MC_REF_EFLAGS(pEFlags);
    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU8, pu8Dst, u8Src, pEFlags);

    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/**
 * Common worker for instructions like ADD, AND, OR, ++ with working on
 * AX/EAX/RAX with a word/dword immediate.
 *
 * @param   pImpl       Pointer to the instruction implementation (assembly).
 */
FNIEMOP_DEF_1(iemOpHlpBinaryOperator_rAX_Iz, PCIEMOPBINSIZES, pImpl)
{
    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
        {
            uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

            IEM_MC_BEGIN(3, 0);
            IEM_MC_ARG(uint16_t *,      pu16Dst,                0);
            IEM_MC_ARG_CONST(uint16_t,  u16Src,/*=*/ u16Imm,    1);
            IEM_MC_ARG(uint32_t *,      pEFlags,                2);

            IEM_MC_REF_GREG_U16(pu16Dst, X86_GREG_xAX);
            IEM_MC_REF_EFLAGS(pEFlags);
            IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU16, pu16Dst, u16Src, pEFlags);

            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;
        }

        case IEMMODE_32BIT:
        {
            uint32_t u32Imm; IEM_OPCODE_GET_NEXT_U32(&u32Imm);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

            IEM_MC_BEGIN(3, 0);
            IEM_MC_ARG(uint32_t *,      pu32Dst,                0);
            IEM_MC_ARG_CONST(uint32_t,  u32Src,/*=*/ u32Imm,    1);
            IEM_MC_ARG(uint32_t *,      pEFlags,                2);

            IEM_MC_REF_GREG_U32(pu32Dst, X86_GREG_xAX);
            IEM_MC_REF_EFLAGS(pEFlags);
            IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, u32Src, pEFlags);

            if (pImpl != &g_iemAImpl_test)
                IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32Dst);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;
        }

        case IEMMODE_64BIT:
        {
            uint64_t u64Imm; IEM_OPCODE_GET_NEXT_S32_SX_U64(&u64Imm);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

            IEM_MC_BEGIN(3, 0);
            IEM_MC_ARG(uint64_t *,      pu64Dst,                0);
            IEM_MC_ARG_CONST(uint64_t,  u64Src,/*=*/ u64Imm,    1);
            IEM_MC_ARG(uint32_t *,      pEFlags,                2);

            IEM_MC_REF_GREG_U64(pu64Dst, X86_GREG_xAX);
            IEM_MC_REF_EFLAGS(pEFlags);
            IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU64, pu64Dst, u64Src, pEFlags);

            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;
        }

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/** Opcodes 0xf1, 0xd6. */
FNIEMOP_DEF(iemOp_Invalid)
{
    IEMOP_MNEMONIC("Invalid");
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Invalid with RM byte . */
FNIEMOPRM_DEF(iemOp_InvalidWithRM)
{
    IEMOP_MNEMONIC("InvalidWithRM");
    return IEMOP_RAISE_INVALID_OPCODE();
}



/** @name ..... opcodes.
 *
 * @{
 */

/** @}  */


/** @name Two byte opcodes (first byte 0x0f).
 *
 * @{
 */

/** Opcode 0x0f 0x00 /0. */
FNIEMOPRM_DEF(iemOp_Grp6_sldt)
{
    IEMOP_MNEMONIC("sldt Rv/Mw");
    IEMOP_HLP_MIN_286();
    IEMOP_HLP_NO_REAL_OR_V86_MODE();

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DECODED_NL_1(OP_SLDT, IEMOPFORM_M_REG, OP_PARM_Ew, DISOPTYPE_DANGEROUS | DISOPTYPE_PRIVILEGED_NOTRAP);
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint16_t, u16Ldtr);
                IEM_MC_FETCH_LDTR_U16(u16Ldtr);
                IEM_MC_STORE_GREG_U16((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u16Ldtr);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint32_t, u32Ldtr);
                IEM_MC_FETCH_LDTR_U32(u32Ldtr);
                IEM_MC_STORE_GREG_U32((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u32Ldtr);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint64_t, u64Ldtr);
                IEM_MC_FETCH_LDTR_U64(u64Ldtr);
                IEM_MC_STORE_GREG_U64((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u64Ldtr);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint16_t, u16Ldtr);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DECODED_NL_1(OP_SLDT, IEMOPFORM_M_MEM, OP_PARM_Ew, DISOPTYPE_DANGEROUS | DISOPTYPE_PRIVILEGED_NOTRAP);
        IEM_MC_FETCH_LDTR_U16(u16Ldtr);
        IEM_MC_STORE_MEM_U16(pVCpu->iem.s.iEffSeg, GCPtrEffDst, u16Ldtr);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x00 /1. */
FNIEMOPRM_DEF(iemOp_Grp6_str)
{
    IEMOP_MNEMONIC("str Rv/Mw");
    IEMOP_HLP_MIN_286();
    IEMOP_HLP_NO_REAL_OR_V86_MODE();

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DECODED_NL_1(OP_STR, IEMOPFORM_M_REG, OP_PARM_Ew, DISOPTYPE_DANGEROUS | DISOPTYPE_PRIVILEGED_NOTRAP);
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint16_t, u16Tr);
                IEM_MC_FETCH_TR_U16(u16Tr);
                IEM_MC_STORE_GREG_U16((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u16Tr);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint32_t, u32Tr);
                IEM_MC_FETCH_TR_U32(u32Tr);
                IEM_MC_STORE_GREG_U32((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u32Tr);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint64_t, u64Tr);
                IEM_MC_FETCH_TR_U64(u64Tr);
                IEM_MC_STORE_GREG_U64((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u64Tr);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint16_t, u16Tr);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DECODED_NL_1(OP_STR, IEMOPFORM_M_MEM, OP_PARM_Ew, DISOPTYPE_DANGEROUS | DISOPTYPE_PRIVILEGED_NOTRAP);
        IEM_MC_FETCH_TR_U16(u16Tr);
        IEM_MC_STORE_MEM_U16(pVCpu->iem.s.iEffSeg, GCPtrEffDst, u16Tr);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x00 /2. */
FNIEMOPRM_DEF(iemOp_Grp6_lldt)
{
    IEMOP_MNEMONIC("lldt Ew");
    IEMOP_HLP_MIN_286();
    IEMOP_HLP_NO_REAL_OR_V86_MODE();

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DECODED_NL_1(OP_LLDT, IEMOPFORM_M_REG, OP_PARM_Ew, DISOPTYPE_DANGEROUS);
        IEM_MC_BEGIN(1, 0);
        IEM_MC_ARG(uint16_t, u16Sel, 0);
        IEM_MC_FETCH_GREG_U16(u16Sel, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        IEM_MC_CALL_CIMPL_1(iemCImpl_lldt, u16Sel);
        IEM_MC_END();
    }
    else
    {
        IEM_MC_BEGIN(1, 1);
        IEM_MC_ARG(uint16_t, u16Sel, 0);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DECODED_NL_1(OP_LLDT, IEMOPFORM_M_MEM, OP_PARM_Ew, DISOPTYPE_DANGEROUS);
        IEM_MC_RAISE_GP0_IF_CPL_NOT_ZERO(); /** @todo test order */
        IEM_MC_FETCH_MEM_U16(u16Sel, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
        IEM_MC_CALL_CIMPL_1(iemCImpl_lldt, u16Sel);
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x00 /3. */
FNIEMOPRM_DEF(iemOp_Grp6_ltr)
{
    IEMOP_MNEMONIC("ltr Ew");
    IEMOP_HLP_MIN_286();
    IEMOP_HLP_NO_REAL_OR_V86_MODE();

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(1, 0);
        IEM_MC_ARG(uint16_t, u16Sel, 0);
        IEM_MC_FETCH_GREG_U16(u16Sel, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        IEM_MC_CALL_CIMPL_1(iemCImpl_ltr, u16Sel);
        IEM_MC_END();
    }
    else
    {
        IEM_MC_BEGIN(1, 1);
        IEM_MC_ARG(uint16_t, u16Sel, 0);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_RAISE_GP0_IF_CPL_NOT_ZERO(); /** @todo test ordre */
        IEM_MC_FETCH_MEM_U16(u16Sel, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
        IEM_MC_CALL_CIMPL_1(iemCImpl_ltr, u16Sel);
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x00 /3. */
FNIEMOP_DEF_2(iemOpCommonGrp6VerX, uint8_t, bRm, bool, fWrite)
{
    IEMOP_HLP_MIN_286();
    IEMOP_HLP_NO_REAL_OR_V86_MODE();

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DECODED_NL_1(fWrite ? OP_VERW : OP_VERR, IEMOPFORM_M_MEM, OP_PARM_Ew, DISOPTYPE_DANGEROUS | DISOPTYPE_PRIVILEGED_NOTRAP);
        IEM_MC_BEGIN(2, 0);
        IEM_MC_ARG(uint16_t,    u16Sel,            0);
        IEM_MC_ARG_CONST(bool,  fWriteArg, fWrite, 1);
        IEM_MC_FETCH_GREG_U16(u16Sel, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        IEM_MC_CALL_CIMPL_2(iemCImpl_VerX, u16Sel, fWriteArg);
        IEM_MC_END();
    }
    else
    {
        IEM_MC_BEGIN(2, 1);
        IEM_MC_ARG(uint16_t,    u16Sel,            0);
        IEM_MC_ARG_CONST(bool,  fWriteArg, fWrite, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DECODED_NL_1(fWrite ? OP_VERW : OP_VERR, IEMOPFORM_M_MEM, OP_PARM_Ew, DISOPTYPE_DANGEROUS | DISOPTYPE_PRIVILEGED_NOTRAP);
        IEM_MC_FETCH_MEM_U16(u16Sel, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
        IEM_MC_CALL_CIMPL_2(iemCImpl_VerX, u16Sel, fWriteArg);
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x00 /4. */
FNIEMOPRM_DEF(iemOp_Grp6_verr)
{
    IEMOP_MNEMONIC("verr Ew");
    IEMOP_HLP_MIN_286();
    return FNIEMOP_CALL_2(iemOpCommonGrp6VerX, bRm, false);
}


/** Opcode 0x0f 0x00 /5. */
FNIEMOPRM_DEF(iemOp_Grp6_verw)
{
    IEMOP_MNEMONIC("verr Ew");
    IEMOP_HLP_MIN_286();
    return FNIEMOP_CALL_2(iemOpCommonGrp6VerX, bRm, true);
}


/**
 * Group 6 jump table.
 */
IEM_STATIC const PFNIEMOPRM g_apfnGroup6[8] =
{
    iemOp_Grp6_sldt,
    iemOp_Grp6_str,
    iemOp_Grp6_lldt,
    iemOp_Grp6_ltr,
    iemOp_Grp6_verr,
    iemOp_Grp6_verw,
    iemOp_InvalidWithRM,
    iemOp_InvalidWithRM
};

/** Opcode 0x0f 0x00. */
FNIEMOP_DEF(iemOp_Grp6)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    return FNIEMOP_CALL_1(g_apfnGroup6[(bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK], bRm);
}


/** Opcode 0x0f 0x01 /0. */
FNIEMOP_DEF_1(iemOp_Grp7_sgdt, uint8_t, bRm)
{
    IEMOP_MNEMONIC("sgdt Ms");
    IEMOP_HLP_MIN_286();
    IEMOP_HLP_64BIT_OP_SIZE();
    IEM_MC_BEGIN(2, 1);
    IEM_MC_ARG(uint8_t,         iEffSeg,                                    0);
    IEM_MC_ARG(RTGCPTR,         GCPtrEffSrc,                                1);
    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_ASSIGN(iEffSeg, pVCpu->iem.s.iEffSeg);
    IEM_MC_CALL_CIMPL_2(iemCImpl_sgdt, iEffSeg, GCPtrEffSrc);
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x01 /0. */
FNIEMOP_DEF(iemOp_Grp7_vmcall)
{
    IEMOP_BITCH_ABOUT_STUB();
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Opcode 0x0f 0x01 /0. */
FNIEMOP_DEF(iemOp_Grp7_vmlaunch)
{
    IEMOP_BITCH_ABOUT_STUB();
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Opcode 0x0f 0x01 /0. */
FNIEMOP_DEF(iemOp_Grp7_vmresume)
{
    IEMOP_BITCH_ABOUT_STUB();
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Opcode 0x0f 0x01 /0. */
FNIEMOP_DEF(iemOp_Grp7_vmxoff)
{
    IEMOP_BITCH_ABOUT_STUB();
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Opcode 0x0f 0x01 /1. */
FNIEMOP_DEF_1(iemOp_Grp7_sidt, uint8_t, bRm)
{
    IEMOP_MNEMONIC("sidt Ms");
    IEMOP_HLP_MIN_286();
    IEMOP_HLP_64BIT_OP_SIZE();
    IEM_MC_BEGIN(2, 1);
    IEM_MC_ARG(uint8_t,         iEffSeg,                                    0);
    IEM_MC_ARG(RTGCPTR,         GCPtrEffSrc,                                1);
    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_ASSIGN(iEffSeg, pVCpu->iem.s.iEffSeg);
    IEM_MC_CALL_CIMPL_2(iemCImpl_sidt, iEffSeg, GCPtrEffSrc);
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x01 /1. */
FNIEMOP_DEF(iemOp_Grp7_monitor)
{
    IEMOP_MNEMONIC("monitor");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX(); /** @todo Verify that monitor is allergic to lock prefixes. */
    return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_monitor, pVCpu->iem.s.iEffSeg);
}


/** Opcode 0x0f 0x01 /1. */
FNIEMOP_DEF(iemOp_Grp7_mwait)
{
    IEMOP_MNEMONIC("mwait"); /** @todo Verify that mwait is allergic to lock prefixes. */
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_mwait);
}


/** Opcode 0x0f 0x01 /2. */
FNIEMOP_DEF_1(iemOp_Grp7_lgdt, uint8_t, bRm)
{
    IEMOP_MNEMONIC("lgdt");
    IEMOP_HLP_64BIT_OP_SIZE();
    IEM_MC_BEGIN(3, 1);
    IEM_MC_ARG(uint8_t,         iEffSeg,                                    0);
    IEM_MC_ARG(RTGCPTR,         GCPtrEffSrc,                                1);
    IEM_MC_ARG_CONST(IEMMODE,   enmEffOpSizeArg,/*=*/pVCpu->iem.s.enmEffOpSize, 2);
    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_ASSIGN(iEffSeg, pVCpu->iem.s.iEffSeg);
    IEM_MC_CALL_CIMPL_3(iemCImpl_lgdt, iEffSeg, GCPtrEffSrc, enmEffOpSizeArg);
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x01 0xd0. */
FNIEMOP_DEF(iemOp_Grp7_xgetbv)
{
    IEMOP_MNEMONIC("xgetbv");
    if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fXSaveRstor)
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_REPZ_OR_REPNZ_PREFIXES();
        return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_xgetbv);
    }
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Opcode 0x0f 0x01 0xd1. */
FNIEMOP_DEF(iemOp_Grp7_xsetbv)
{
    IEMOP_MNEMONIC("xsetbv");
    if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fXSaveRstor)
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_REPZ_OR_REPNZ_PREFIXES();
        return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_xsetbv);
    }
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Opcode 0x0f 0x01 /3. */
FNIEMOP_DEF_1(iemOp_Grp7_lidt, uint8_t, bRm)
{
    IEMMODE enmEffOpSize = pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT
                         ? IEMMODE_64BIT
                         : pVCpu->iem.s.enmEffOpSize;
    IEM_MC_BEGIN(3, 1);
    IEM_MC_ARG(uint8_t,         iEffSeg,                            0);
    IEM_MC_ARG(RTGCPTR,         GCPtrEffSrc,                        1);
    IEM_MC_ARG_CONST(IEMMODE,   enmEffOpSizeArg,/*=*/enmEffOpSize,  2);
    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_ASSIGN(iEffSeg, pVCpu->iem.s.iEffSeg);
    IEM_MC_CALL_CIMPL_3(iemCImpl_lidt, iEffSeg, GCPtrEffSrc, enmEffOpSizeArg);
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x01 0xd8. */
FNIEMOP_UD_STUB(iemOp_Grp7_Amd_vmrun);

/** Opcode 0x0f 0x01 0xd9. */
FNIEMOP_UD_STUB(iemOp_Grp7_Amd_vmmcall);

/** Opcode 0x0f 0x01 0xda. */
FNIEMOP_UD_STUB(iemOp_Grp7_Amd_vmload);

/** Opcode 0x0f 0x01 0xdb. */
FNIEMOP_UD_STUB(iemOp_Grp7_Amd_vmsave);

/** Opcode 0x0f 0x01 0xdc. */
FNIEMOP_UD_STUB(iemOp_Grp7_Amd_stgi);

/** Opcode 0x0f 0x01 0xdd. */
FNIEMOP_UD_STUB(iemOp_Grp7_Amd_clgi);

/** Opcode 0x0f 0x01 0xde. */
FNIEMOP_UD_STUB(iemOp_Grp7_Amd_skinit);

/** Opcode 0x0f 0x01 0xdf. */
FNIEMOP_UD_STUB(iemOp_Grp7_Amd_invlpga);

/** Opcode 0x0f 0x01 /4. */
FNIEMOP_DEF_1(iemOp_Grp7_smsw, uint8_t, bRm)
{
    IEMOP_MNEMONIC("smsw");
    IEMOP_HLP_MIN_286();
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint16_t, u16Tmp);
                IEM_MC_FETCH_CR0_U16(u16Tmp);
                if (IEM_GET_TARGET_CPU(pVCpu) > IEMTARGETCPU_386)
                { /* likely */ }
                else if (IEM_GET_TARGET_CPU(pVCpu) >= IEMTARGETCPU_386)
                    IEM_MC_OR_LOCAL_U16(u16Tmp, 0xffe0);
                else
                    IEM_MC_OR_LOCAL_U16(u16Tmp, 0xfff0);
                IEM_MC_STORE_GREG_U16((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u16Tmp);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint32_t, u32Tmp);
                IEM_MC_FETCH_CR0_U32(u32Tmp);
                IEM_MC_STORE_GREG_U32((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u32Tmp);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint64_t, u64Tmp);
                IEM_MC_FETCH_CR0_U64(u64Tmp);
                IEM_MC_STORE_GREG_U64((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u64Tmp);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        /* Ignore operand size here, memory refs are always 16-bit. */
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint16_t, u16Tmp);
        IEM_MC_LOCAL(RTGCPTR,  GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_FETCH_CR0_U16(u16Tmp);
        if (IEM_GET_TARGET_CPU(pVCpu) > IEMTARGETCPU_386)
        { /* likely */ }
        else if (pVCpu->iem.s.uTargetCpu >= IEMTARGETCPU_386)
            IEM_MC_OR_LOCAL_U16(u16Tmp, 0xffe0);
        else
            IEM_MC_OR_LOCAL_U16(u16Tmp, 0xfff0);
        IEM_MC_STORE_MEM_U16(pVCpu->iem.s.iEffSeg, GCPtrEffDst, u16Tmp);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
        return VINF_SUCCESS;
    }
}


/** Opcode 0x0f 0x01 /6. */
FNIEMOP_DEF_1(iemOp_Grp7_lmsw, uint8_t, bRm)
{
    /* The operand size is effectively ignored, all is 16-bit and only the
       lower 3-bits are used. */
    IEMOP_MNEMONIC("lmsw");
    IEMOP_HLP_MIN_286();
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(1, 0);
        IEM_MC_ARG(uint16_t, u16Tmp, 0);
        IEM_MC_FETCH_GREG_U16(u16Tmp, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        IEM_MC_CALL_CIMPL_1(iemCImpl_lmsw, u16Tmp);
        IEM_MC_END();
    }
    else
    {
        IEM_MC_BEGIN(1, 1);
        IEM_MC_ARG(uint16_t, u16Tmp, 0);
        IEM_MC_LOCAL(RTGCPTR,  GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_FETCH_MEM_U16(u16Tmp, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
        IEM_MC_CALL_CIMPL_1(iemCImpl_lmsw, u16Tmp);
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x01 /7. */
FNIEMOP_DEF_1(iemOp_Grp7_invlpg, uint8_t, bRm)
{
    IEMOP_MNEMONIC("invlpg");
    IEMOP_HLP_MIN_486();
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_BEGIN(1, 1);
    IEM_MC_ARG(RTGCPTR, GCPtrEffDst, 0);
    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
    IEM_MC_CALL_CIMPL_1(iemCImpl_invlpg, GCPtrEffDst);
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x01 /7. */
FNIEMOP_DEF(iemOp_Grp7_swapgs)
{
    IEMOP_MNEMONIC("swapgs");
    IEMOP_HLP_ONLY_64BIT();
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_swapgs);
}


/** Opcode 0x0f 0x01 /7. */
FNIEMOP_DEF(iemOp_Grp7_rdtscp)
{
    NOREF(pVCpu);
    IEMOP_BITCH_ABOUT_STUB();
    return VERR_IEM_INSTR_NOT_IMPLEMENTED;
}


/** Opcode 0x0f 0x01. */
FNIEMOP_DEF(iemOp_Grp7)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
    {
        case 0:
            if ((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT))
                return FNIEMOP_CALL_1(iemOp_Grp7_sgdt, bRm);
            switch (bRm & X86_MODRM_RM_MASK)
            {
                case 1: return FNIEMOP_CALL(iemOp_Grp7_vmcall);
                case 2: return FNIEMOP_CALL(iemOp_Grp7_vmlaunch);
                case 3: return FNIEMOP_CALL(iemOp_Grp7_vmresume);
                case 4: return FNIEMOP_CALL(iemOp_Grp7_vmxoff);
            }
            return IEMOP_RAISE_INVALID_OPCODE();

        case 1:
            if ((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT))
                return FNIEMOP_CALL_1(iemOp_Grp7_sidt, bRm);
            switch (bRm & X86_MODRM_RM_MASK)
            {
                case 0: return FNIEMOP_CALL(iemOp_Grp7_monitor);
                case 1: return FNIEMOP_CALL(iemOp_Grp7_mwait);
            }
            return IEMOP_RAISE_INVALID_OPCODE();

        case 2:
            if ((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT))
                return FNIEMOP_CALL_1(iemOp_Grp7_lgdt, bRm);
            switch (bRm & X86_MODRM_RM_MASK)
            {
                case 0: return FNIEMOP_CALL(iemOp_Grp7_xgetbv);
                case 1: return FNIEMOP_CALL(iemOp_Grp7_xsetbv);
            }
            return IEMOP_RAISE_INVALID_OPCODE();

        case 3:
            if ((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT))
                return FNIEMOP_CALL_1(iemOp_Grp7_lidt, bRm);
            switch (bRm & X86_MODRM_RM_MASK)
            {
                case 0: return FNIEMOP_CALL(iemOp_Grp7_Amd_vmrun);
                case 1: return FNIEMOP_CALL(iemOp_Grp7_Amd_vmmcall);
                case 2: return FNIEMOP_CALL(iemOp_Grp7_Amd_vmload);
                case 3: return FNIEMOP_CALL(iemOp_Grp7_Amd_vmsave);
                case 4: return FNIEMOP_CALL(iemOp_Grp7_Amd_stgi);
                case 5: return FNIEMOP_CALL(iemOp_Grp7_Amd_clgi);
                case 6: return FNIEMOP_CALL(iemOp_Grp7_Amd_skinit);
                case 7: return FNIEMOP_CALL(iemOp_Grp7_Amd_invlpga);
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }

        case 4:
            return FNIEMOP_CALL_1(iemOp_Grp7_smsw, bRm);

        case 5:
            return IEMOP_RAISE_INVALID_OPCODE();

        case 6:
            return FNIEMOP_CALL_1(iemOp_Grp7_lmsw, bRm);

        case 7:
            if ((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT))
                return FNIEMOP_CALL_1(iemOp_Grp7_invlpg, bRm);
            switch (bRm & X86_MODRM_RM_MASK)
            {
                case 0: return FNIEMOP_CALL(iemOp_Grp7_swapgs);
                case 1: return FNIEMOP_CALL(iemOp_Grp7_rdtscp);
            }
            return IEMOP_RAISE_INVALID_OPCODE();

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}

/** Opcode 0x0f 0x00 /3. */
FNIEMOP_DEF_1(iemOpCommonLarLsl_Gv_Ew, bool, fIsLar)
{
    IEMOP_HLP_NO_REAL_OR_V86_MODE();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DECODED_NL_2(fIsLar ? OP_LAR : OP_LSL, IEMOPFORM_RM_REG, OP_PARM_Gv, OP_PARM_Ew, DISOPTYPE_DANGEROUS | DISOPTYPE_PRIVILEGED_NOTRAP);
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
            {
                IEM_MC_BEGIN(4, 0);
                IEM_MC_ARG(uint16_t *,  pu16Dst,           0);
                IEM_MC_ARG(uint16_t,    u16Sel,            1);
                IEM_MC_ARG(uint32_t *,  pEFlags,           2);
                IEM_MC_ARG_CONST(bool,  fIsLarArg, fIsLar, 3);

                IEM_MC_REF_GREG_U16(pu16Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_FETCH_GREG_U16(u16Sel, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_CIMPL_4(iemCImpl_LarLsl_u16, pu16Dst, u16Sel, pEFlags, fIsLarArg);

                IEM_MC_END();
                return VINF_SUCCESS;
            }

            case IEMMODE_32BIT:
            case IEMMODE_64BIT:
            {
                IEM_MC_BEGIN(4, 0);
                IEM_MC_ARG(uint64_t *,  pu64Dst,           0);
                IEM_MC_ARG(uint16_t,    u16Sel,            1);
                IEM_MC_ARG(uint32_t *,  pEFlags,           2);
                IEM_MC_ARG_CONST(bool,  fIsLarArg, fIsLar, 3);

                IEM_MC_REF_GREG_U64(pu64Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_FETCH_GREG_U16(u16Sel, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_CIMPL_4(iemCImpl_LarLsl_u64, pu64Dst, u16Sel, pEFlags, fIsLarArg);

                IEM_MC_END();
                return VINF_SUCCESS;
            }

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
            {
                IEM_MC_BEGIN(4, 1);
                IEM_MC_ARG(uint16_t *,  pu16Dst,           0);
                IEM_MC_ARG(uint16_t,    u16Sel,            1);
                IEM_MC_ARG(uint32_t *,  pEFlags,           2);
                IEM_MC_ARG_CONST(bool,  fIsLarArg, fIsLar, 3);
                IEM_MC_LOCAL(RTGCPTR,   GCPtrEffSrc);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                IEMOP_HLP_DECODED_NL_2(fIsLar ? OP_LAR : OP_LSL, IEMOPFORM_RM_MEM, OP_PARM_Gv, OP_PARM_Ew, DISOPTYPE_DANGEROUS | DISOPTYPE_PRIVILEGED_NOTRAP);

                IEM_MC_FETCH_MEM_U16(u16Sel, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
                IEM_MC_REF_GREG_U16(pu16Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_CIMPL_4(iemCImpl_LarLsl_u16, pu16Dst, u16Sel, pEFlags, fIsLarArg);

                IEM_MC_END();
                return VINF_SUCCESS;
            }

            case IEMMODE_32BIT:
            case IEMMODE_64BIT:
            {
                IEM_MC_BEGIN(4, 1);
                IEM_MC_ARG(uint64_t *,  pu64Dst,           0);
                IEM_MC_ARG(uint16_t,    u16Sel,            1);
                IEM_MC_ARG(uint32_t *,  pEFlags,           2);
                IEM_MC_ARG_CONST(bool,  fIsLarArg, fIsLar, 3);
                IEM_MC_LOCAL(RTGCPTR,   GCPtrEffSrc);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                IEMOP_HLP_DECODED_NL_2(fIsLar ? OP_LAR : OP_LSL, IEMOPFORM_RM_MEM, OP_PARM_Gv, OP_PARM_Ew, DISOPTYPE_DANGEROUS | DISOPTYPE_PRIVILEGED_NOTRAP);
/** @todo testcase: make sure it's a 16-bit read. */

                IEM_MC_FETCH_MEM_U16(u16Sel, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
                IEM_MC_REF_GREG_U64(pu64Dst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_CIMPL_4(iemCImpl_LarLsl_u64, pu64Dst, u16Sel, pEFlags, fIsLarArg);

                IEM_MC_END();
                return VINF_SUCCESS;
            }

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}



/** Opcode 0x0f 0x02. */
FNIEMOP_DEF(iemOp_lar_Gv_Ew)
{
    IEMOP_MNEMONIC("lar Gv,Ew");
    return FNIEMOP_CALL_1(iemOpCommonLarLsl_Gv_Ew, true);
}


/** Opcode 0x0f 0x03. */
FNIEMOP_DEF(iemOp_lsl_Gv_Ew)
{
    IEMOP_MNEMONIC("lsl Gv,Ew");
    return FNIEMOP_CALL_1(iemOpCommonLarLsl_Gv_Ew, false);
}


/** Opcode 0x0f 0x05. */
FNIEMOP_DEF(iemOp_syscall)
{
    IEMOP_MNEMONIC("syscall"); /** @todo 286 LOADALL   */
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_syscall);
}


/** Opcode 0x0f 0x06. */
FNIEMOP_DEF(iemOp_clts)
{
    IEMOP_MNEMONIC("clts");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_clts);
}


/** Opcode 0x0f 0x07. */
FNIEMOP_DEF(iemOp_sysret)
{
    IEMOP_MNEMONIC("sysret");  /** @todo 386 LOADALL   */
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_sysret);
}


/** Opcode 0x0f 0x08. */
FNIEMOP_STUB(iemOp_invd);
// IEMOP_HLP_MIN_486();


/** Opcode 0x0f 0x09. */
FNIEMOP_DEF(iemOp_wbinvd)
{
    IEMOP_MNEMONIC("wbinvd");
    IEMOP_HLP_MIN_486();
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_BEGIN(0, 0);
    IEM_MC_RAISE_GP0_IF_CPL_NOT_ZERO();
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS; /* ignore for now */
}


/** Opcode 0x0f 0x0b. */
FNIEMOP_DEF(iemOp_ud2)
{
    IEMOP_MNEMONIC("ud2");
    return IEMOP_RAISE_INVALID_OPCODE();
}

/** Opcode 0x0f 0x0d. */
FNIEMOP_DEF(iemOp_nop_Ev_GrpP)
{
    /* AMD prefetch group, Intel implements this as NOP Ev (and so do we). */
    if (!IEM_GET_GUEST_CPU_FEATURES(pVCpu)->f3DNowPrefetch)
    {
        IEMOP_MNEMONIC("GrpP");
        return IEMOP_RAISE_INVALID_OPCODE();
    }

    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_MNEMONIC("GrpP");
        return IEMOP_RAISE_INVALID_OPCODE();
    }

    switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
    {
        case 2: /* Aliased to /0 for the time being. */
        case 4: /* Aliased to /0 for the time being. */
        case 5: /* Aliased to /0 for the time being. */
        case 6: /* Aliased to /0 for the time being. */
        case 7: /* Aliased to /0 for the time being. */
        case 0: IEMOP_MNEMONIC("prefetch"); break;
        case 1: IEMOP_MNEMONIC("prefetchw"); break;
        case 3: IEMOP_MNEMONIC("prefetchw"); break;
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }

    IEM_MC_BEGIN(0, 1);
    IEM_MC_LOCAL(RTGCPTR,  GCPtrEffSrc);
    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    /* Currently a NOP. */
    NOREF(GCPtrEffSrc);
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x0e. */
FNIEMOP_STUB(iemOp_femms);


/** Opcode 0x0f 0x0f 0x0c. */
FNIEMOP_STUB(iemOp_3Dnow_pi2fw_Pq_Qq);

/** Opcode 0x0f 0x0f 0x0d. */
FNIEMOP_STUB(iemOp_3Dnow_pi2fd_Pq_Qq);

/** Opcode 0x0f 0x0f 0x1c. */
FNIEMOP_STUB(iemOp_3Dnow_pf2fw_Pq_Qq);

/** Opcode 0x0f 0x0f 0x1d. */
FNIEMOP_STUB(iemOp_3Dnow_pf2fd_Pq_Qq);

/** Opcode 0x0f 0x0f 0x8a. */
FNIEMOP_STUB(iemOp_3Dnow_pfnacc_Pq_Qq);

/** Opcode 0x0f 0x0f 0x8e. */
FNIEMOP_STUB(iemOp_3Dnow_pfpnacc_Pq_Qq);

/** Opcode 0x0f 0x0f 0x90. */
FNIEMOP_STUB(iemOp_3Dnow_pfcmpge_Pq_Qq);

/** Opcode 0x0f 0x0f 0x94. */
FNIEMOP_STUB(iemOp_3Dnow_pfmin_Pq_Qq);

/** Opcode 0x0f 0x0f 0x96. */
FNIEMOP_STUB(iemOp_3Dnow_pfrcp_Pq_Qq);

/** Opcode 0x0f 0x0f 0x97. */
FNIEMOP_STUB(iemOp_3Dnow_pfrsqrt_Pq_Qq);

/** Opcode 0x0f 0x0f 0x9a. */
FNIEMOP_STUB(iemOp_3Dnow_pfsub_Pq_Qq);

/** Opcode 0x0f 0x0f 0x9e. */
FNIEMOP_STUB(iemOp_3Dnow_pfadd_PQ_Qq);

/** Opcode 0x0f 0x0f 0xa0. */
FNIEMOP_STUB(iemOp_3Dnow_pfcmpgt_Pq_Qq);

/** Opcode 0x0f 0x0f 0xa4. */
FNIEMOP_STUB(iemOp_3Dnow_pfmax_Pq_Qq);

/** Opcode 0x0f 0x0f 0xa6. */
FNIEMOP_STUB(iemOp_3Dnow_pfrcpit1_Pq_Qq);

/** Opcode 0x0f 0x0f 0xa7. */
FNIEMOP_STUB(iemOp_3Dnow_pfrsqit1_Pq_Qq);

/** Opcode 0x0f 0x0f 0xaa. */
FNIEMOP_STUB(iemOp_3Dnow_pfsubr_Pq_Qq);

/** Opcode 0x0f 0x0f 0xae. */
FNIEMOP_STUB(iemOp_3Dnow_pfacc_PQ_Qq);

/** Opcode 0x0f 0x0f 0xb0. */
FNIEMOP_STUB(iemOp_3Dnow_pfcmpeq_Pq_Qq);

/** Opcode 0x0f 0x0f 0xb4. */
FNIEMOP_STUB(iemOp_3Dnow_pfmul_Pq_Qq);

/** Opcode 0x0f 0x0f 0xb6. */
FNIEMOP_STUB(iemOp_3Dnow_pfrcpit2_Pq_Qq);

/** Opcode 0x0f 0x0f 0xb7. */
FNIEMOP_STUB(iemOp_3Dnow_pmulhrw_Pq_Qq);

/** Opcode 0x0f 0x0f 0xbb. */
FNIEMOP_STUB(iemOp_3Dnow_pswapd_Pq_Qq);

/** Opcode 0x0f 0x0f 0xbf. */
FNIEMOP_STUB(iemOp_3Dnow_pavgusb_PQ_Qq);


/** Opcode 0x0f 0x0f. */
FNIEMOP_DEF(iemOp_3Dnow)
{
    if (!IEM_GET_GUEST_CPU_FEATURES(pVCpu)->f3DNow)
    {
        IEMOP_MNEMONIC("3Dnow");
        return IEMOP_RAISE_INVALID_OPCODE();
    }

    /* This is pretty sparse, use switch instead of table. */
    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    switch (b)
    {
        case 0x0c: return FNIEMOP_CALL(iemOp_3Dnow_pi2fw_Pq_Qq);
        case 0x0d: return FNIEMOP_CALL(iemOp_3Dnow_pi2fd_Pq_Qq);
        case 0x1c: return FNIEMOP_CALL(iemOp_3Dnow_pf2fw_Pq_Qq);
        case 0x1d: return FNIEMOP_CALL(iemOp_3Dnow_pf2fd_Pq_Qq);
        case 0x8a: return FNIEMOP_CALL(iemOp_3Dnow_pfnacc_Pq_Qq);
        case 0x8e: return FNIEMOP_CALL(iemOp_3Dnow_pfpnacc_Pq_Qq);
        case 0x90: return FNIEMOP_CALL(iemOp_3Dnow_pfcmpge_Pq_Qq);
        case 0x94: return FNIEMOP_CALL(iemOp_3Dnow_pfmin_Pq_Qq);
        case 0x96: return FNIEMOP_CALL(iemOp_3Dnow_pfrcp_Pq_Qq);
        case 0x97: return FNIEMOP_CALL(iemOp_3Dnow_pfrsqrt_Pq_Qq);
        case 0x9a: return FNIEMOP_CALL(iemOp_3Dnow_pfsub_Pq_Qq);
        case 0x9e: return FNIEMOP_CALL(iemOp_3Dnow_pfadd_PQ_Qq);
        case 0xa0: return FNIEMOP_CALL(iemOp_3Dnow_pfcmpgt_Pq_Qq);
        case 0xa4: return FNIEMOP_CALL(iemOp_3Dnow_pfmax_Pq_Qq);
        case 0xa6: return FNIEMOP_CALL(iemOp_3Dnow_pfrcpit1_Pq_Qq);
        case 0xa7: return FNIEMOP_CALL(iemOp_3Dnow_pfrsqit1_Pq_Qq);
        case 0xaa: return FNIEMOP_CALL(iemOp_3Dnow_pfsubr_Pq_Qq);
        case 0xae: return FNIEMOP_CALL(iemOp_3Dnow_pfacc_PQ_Qq);
        case 0xb0: return FNIEMOP_CALL(iemOp_3Dnow_pfcmpeq_Pq_Qq);
        case 0xb4: return FNIEMOP_CALL(iemOp_3Dnow_pfmul_Pq_Qq);
        case 0xb6: return FNIEMOP_CALL(iemOp_3Dnow_pfrcpit2_Pq_Qq);
        case 0xb7: return FNIEMOP_CALL(iemOp_3Dnow_pmulhrw_Pq_Qq);
        case 0xbb: return FNIEMOP_CALL(iemOp_3Dnow_pswapd_Pq_Qq);
        case 0xbf: return FNIEMOP_CALL(iemOp_3Dnow_pavgusb_PQ_Qq);
        default:
            return IEMOP_RAISE_INVALID_OPCODE();
    }
}


/** Opcode 0x0f 0x10. */
FNIEMOP_STUB(iemOp_movups_Vps_Wps__movupd_Vpd_Wpd__movss_Vss_Wss__movsd_Vsd_Wsd);


/** Opcode 0x0f 0x11. */
FNIEMOP_DEF(iemOp_movups_Wps_Vps__movupd_Wpd_Vpd__movss_Wss_Vss__movsd_Vsd_Wsd)
{
    /* Quick hack. Need to restructure all of this later some time. */
    uint32_t const fRelevantPrefix = pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ);
    if (fRelevantPrefix == 0)
    {
        IEMOP_MNEMONIC("movups Wps,Vps");
        uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
        if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
        {
            /*
             * Register, register.
             */
            IEMOP_HLP_DONE_DECODING_NO_LOCK_REPZ_OR_REPNZ_PREFIXES();
            IEM_MC_BEGIN(0, 0);
            IEM_MC_MAYBE_RAISE_SSE_RELATED_XCPT();
            IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
            IEM_MC_COPY_XREG_U128((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB,
                                  ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
        else
        {
            /*
             * Memory, register.
             */
            IEM_MC_BEGIN(0, 2);
            IEM_MC_LOCAL(uint128_t,                 uSrc); /** @todo optimize this one day... */
            IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_REPZ_OR_REPNZ_PREFIXES(); /** @todo check if this is delayed this long for REPZ/NZ - yes it generally is! */
            IEM_MC_MAYBE_RAISE_SSE_RELATED_XCPT();
            IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();

            IEM_MC_FETCH_XREG_U128(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
            IEM_MC_STORE_MEM_U128(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);

            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
    }
    else if (fRelevantPrefix == IEM_OP_PRF_REPNZ)
    {
        IEMOP_MNEMONIC("movsd Wsd,Vsd");
        uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
        if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
        {
            /*
             * Register, register.
             */
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint64_t,                  uSrc);

            IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
            IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
            IEM_MC_FETCH_XREG_U64(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
            IEM_MC_STORE_XREG_U64((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, uSrc);

            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
        else
        {
            /*
             * Memory, register.
             */
            IEM_MC_BEGIN(0, 2);
            IEM_MC_LOCAL(uint64_t,                  uSrc);
            IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
            IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
            IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();

            IEM_MC_FETCH_XREG_U64(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
            IEM_MC_STORE_MEM_U64(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);

            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
    }
    else
    {
        IEMOP_BITCH_ABOUT_STUB();
        return VERR_IEM_INSTR_NOT_IMPLEMENTED;
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x12. */
FNIEMOP_STUB(iemOp_movlps_Vq_Mq__movhlps_Vq_Uq__movlpd_Vq_Mq__movsldup_Vq_Wq__movddup_Vq_Wq); //NEXT


/** Opcode 0x0f 0x13. */
FNIEMOP_DEF(iemOp_movlps_Mq_Vq__movlpd_Mq_Vq)
{
    /* Quick hack. Need to restructure all of this later some time. */
    if (pVCpu->iem.s.fPrefixes == IEM_OP_PRF_SIZE_OP)
    {
        IEMOP_MNEMONIC("movlpd Mq,Vq");
        uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
        if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
        {
#if 0
            /*
             * Register, register.
             */
            IEMOP_HLP_DONE_DECODING_NO_LOCK_REPZ_OR_REPNZ_PREFIXES();
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint64_t,                  uSrc);
            IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
            IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
            IEM_MC_FETCH_XREG_U64(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
            IEM_MC_STORE_XREG_U64((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, uSrc);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
#else
            return IEMOP_RAISE_INVALID_OPCODE();
#endif
        }
        else
        {
            /*
             * Memory, register.
             */
            IEM_MC_BEGIN(0, 2);
            IEM_MC_LOCAL(uint64_t,                  uSrc);
            IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_REPZ_OR_REPNZ_PREFIXES(); /** @todo check if this is delayed this long for REPZ/NZ - yes it generally is! */
            IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
            IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();

            IEM_MC_FETCH_XREG_U64(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
            IEM_MC_STORE_MEM_U64(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);

            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
        return VINF_SUCCESS;
    }

    IEMOP_BITCH_ABOUT_STUB();
    return VERR_IEM_INSTR_NOT_IMPLEMENTED;
}


/** Opcode 0x0f 0x14. */
FNIEMOP_STUB(iemOp_unpckhlps_Vps_Wq__unpcklpd_Vpd_Wq);
/** Opcode 0x0f 0x15. */
FNIEMOP_STUB(iemOp_unpckhps_Vps_Wq__unpckhpd_Vpd_Wq);
/** Opcode 0x0f 0x16. */
FNIEMOP_STUB(iemOp_movhps_Vq_Mq__movlhps_Vq_Uq__movhpd_Vq_Mq__movshdup_Vq_Wq); //NEXT
/** Opcode 0x0f 0x17. */
FNIEMOP_STUB(iemOp_movhps_Mq_Vq__movhpd_Mq_Vq); //NEXT


/** Opcode 0x0f 0x18. */
FNIEMOP_DEF(iemOp_prefetch_Grp16)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT))
    {
        switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
        {
            case 4: /* Aliased to /0 for the time being according to AMD. */
            case 5: /* Aliased to /0 for the time being according to AMD. */
            case 6: /* Aliased to /0 for the time being according to AMD. */
            case 7: /* Aliased to /0 for the time being according to AMD. */
            case 0: IEMOP_MNEMONIC("prefetchNTA m8"); break;
            case 1: IEMOP_MNEMONIC("prefetchT0  m8"); break;
            case 2: IEMOP_MNEMONIC("prefetchT1  m8"); break;
            case 3: IEMOP_MNEMONIC("prefetchT2  m8"); break;
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }

        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR,  GCPtrEffSrc);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        /* Currently a NOP. */
        NOREF(GCPtrEffSrc);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
        return VINF_SUCCESS;
    }

    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Opcode 0x0f 0x19..0x1f. */
FNIEMOP_DEF(iemOp_nop_Ev)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(0, 0);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        /* Currently a NOP. */
        NOREF(GCPtrEffSrc);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x20. */
FNIEMOP_DEF(iemOp_mov_Rd_Cd)
{
    /* mod is ignored, as is operand size overrides. */
    IEMOP_MNEMONIC("mov Rd,Cd");
    IEMOP_HLP_MIN_386();
    if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
        pVCpu->iem.s.enmEffOpSize = pVCpu->iem.s.enmDefOpSize = IEMMODE_64BIT;
    else
        pVCpu->iem.s.enmEffOpSize = pVCpu->iem.s.enmDefOpSize = IEMMODE_32BIT;

    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    uint8_t iCrReg = ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg;
    if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK)
    {
        /* The lock prefix can be used to encode CR8 accesses on some CPUs. */
        if (!IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fMovCr8In32Bit)
            return IEMOP_RAISE_INVALID_OPCODE(); /* #UD takes precedence over #GP(), see test. */
        iCrReg |= 8;
    }
    switch (iCrReg)
    {
        case 0: case 2: case 3: case 4: case 8:
            break;
        default:
            return IEMOP_RAISE_INVALID_OPCODE();
    }
    IEMOP_HLP_DONE_DECODING();

    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_mov_Rd_Cd, (X86_MODRM_RM_MASK & bRm) | pVCpu->iem.s.uRexB, iCrReg);
}


/** Opcode 0x0f 0x21. */
FNIEMOP_DEF(iemOp_mov_Rd_Dd)
{
    IEMOP_MNEMONIC("mov Rd,Dd");
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_REX_R)
        return IEMOP_RAISE_INVALID_OPCODE();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_mov_Rd_Dd,
                                   (X86_MODRM_RM_MASK & bRm) | pVCpu->iem.s.uRexB,
                                   ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK));
}


/** Opcode 0x0f 0x22. */
FNIEMOP_DEF(iemOp_mov_Cd_Rd)
{
    /* mod is ignored, as is operand size overrides. */
    IEMOP_MNEMONIC("mov Cd,Rd");
    IEMOP_HLP_MIN_386();
    if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
        pVCpu->iem.s.enmEffOpSize = pVCpu->iem.s.enmDefOpSize = IEMMODE_64BIT;
    else
        pVCpu->iem.s.enmEffOpSize = pVCpu->iem.s.enmDefOpSize = IEMMODE_32BIT;

    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    uint8_t iCrReg = ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg;
    if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK)
    {
        /* The lock prefix can be used to encode CR8 accesses on some CPUs. */
        if (!IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fMovCr8In32Bit)
            return IEMOP_RAISE_INVALID_OPCODE(); /* #UD takes precedence over #GP(), see test. */
        iCrReg |= 8;
    }
    switch (iCrReg)
    {
        case 0: case 2: case 3: case 4: case 8:
            break;
        default:
            return IEMOP_RAISE_INVALID_OPCODE();
    }
    IEMOP_HLP_DONE_DECODING();

    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_mov_Cd_Rd, iCrReg, (X86_MODRM_RM_MASK & bRm) | pVCpu->iem.s.uRexB);
}


/** Opcode 0x0f 0x23. */
FNIEMOP_DEF(iemOp_mov_Dd_Rd)
{
    IEMOP_MNEMONIC("mov Dd,Rd");
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_REX_R)
        return IEMOP_RAISE_INVALID_OPCODE();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_mov_Dd_Rd,
                                   ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK),
                                   (X86_MODRM_RM_MASK & bRm) | pVCpu->iem.s.uRexB);
}


/** Opcode 0x0f 0x24. */
FNIEMOP_DEF(iemOp_mov_Rd_Td)
{
    IEMOP_MNEMONIC("mov Rd,Td");
    /** @todo works on 386 and 486. */
    /* The RM byte is not considered, see testcase. */
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Opcode 0x0f 0x26. */
FNIEMOP_DEF(iemOp_mov_Td_Rd)
{
    IEMOP_MNEMONIC("mov Td,Rd");
    /** @todo works on 386 and 486. */
    /* The RM byte is not considered, see testcase. */
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Opcode 0x0f 0x28. */
FNIEMOP_DEF(iemOp_movaps_Vps_Wps__movapd_Vpd_Wpd)
{
    IEMOP_MNEMONIC(!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_OP) ? "movaps r,mr" : "movapd r,mr");
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /*
         * Register, register.
         */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_REPZ_OR_REPNZ_PREFIXES();
        IEM_MC_BEGIN(0, 0);
        if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_OP))
            IEM_MC_MAYBE_RAISE_SSE_RELATED_XCPT();
        else
            IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
        IEM_MC_COPY_XREG_U128(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg,
                              (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /*
         * Register, memory.
         */
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint128_t,                 uSrc); /** @todo optimize this one day... */
        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_REPZ_OR_REPNZ_PREFIXES(); /** @todo check if this is delayed this long for REPZ/NZ */
        if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_OP))
            IEM_MC_MAYBE_RAISE_SSE_RELATED_XCPT();
        else
            IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();

        IEM_MC_FETCH_MEM_U128_ALIGN_SSE(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
        IEM_MC_STORE_XREG_U128(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x29. */
FNIEMOP_DEF(iemOp_movaps_Wps_Vps__movapd_Wpd_Vpd)
{
    IEMOP_MNEMONIC(!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_OP) ? "movaps mr,r" : "movapd mr,r");
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /*
         * Register, register.
         */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_REPZ_OR_REPNZ_PREFIXES();
        IEM_MC_BEGIN(0, 0);
        if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_OP))
            IEM_MC_MAYBE_RAISE_SSE_RELATED_XCPT();
        else
            IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
        IEM_MC_COPY_XREG_U128((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB,
                              ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /*
         * Memory, register.
         */
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint128_t,                 uSrc); /** @todo optimize this one day... */
        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_REPZ_OR_REPNZ_PREFIXES(); /** @todo check if this is delayed this long for REPZ/NZ */
        if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_OP))
            IEM_MC_MAYBE_RAISE_SSE_RELATED_XCPT();
        else
            IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
        IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();

        IEM_MC_FETCH_XREG_U128(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_STORE_MEM_U128_ALIGN_SSE(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x2a. */
FNIEMOP_STUB(iemOp_cvtpi2ps_Vps_Qpi__cvtpi2pd_Vpd_Qpi__cvtsi2ss_Vss_Ey__cvtsi2sd_Vsd_Ey); //NEXT


/** Opcode 0x0f 0x2b. */
FNIEMOP_DEF(iemOp_movntps_Mps_Vps__movntpd_Mpd_Vpd)
{
    IEMOP_MNEMONIC(!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_OP) ? "movntps mr,r" : "movntpd mr,r");
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT))
    {
        /*
         * memory, register.
         */
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint128_t,                 uSrc); /** @todo optimize this one day... */
        IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_REPZ_OR_REPNZ_PREFIXES(); /** @todo check if this is delayed this long for REPZ/NZ */
        if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_OP))
            IEM_MC_MAYBE_RAISE_SSE_RELATED_XCPT();
        else
            IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
        IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();

        IEM_MC_FETCH_XREG_U128(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_STORE_MEM_U128_ALIGN_SSE(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    /* The register, register encoding is invalid. */
    else
        return IEMOP_RAISE_INVALID_OPCODE();
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x2c. */
FNIEMOP_STUB(iemOp_cvttps2pi_Ppi_Wps__cvttpd2pi_Ppi_Wpd__cvttss2si_Gy_Wss__cvttsd2si_Yu_Wsd); //NEXT
/** Opcode 0x0f 0x2d. */
FNIEMOP_STUB(iemOp_cvtps2pi_Ppi_Wps__cvtpd2pi_QpiWpd__cvtss2si_Gy_Wss__cvtsd2si_Gy_Wsd);
/** Opcode 0x0f 0x2e. */
FNIEMOP_STUB(iemOp_ucomiss_Vss_Wss__ucomisd_Vsd_Wsd); //NEXT
/** Opcode 0x0f 0x2f. */
FNIEMOP_STUB(iemOp_comiss_Vss_Wss__comisd_Vsd_Wsd);


/** Opcode 0x0f 0x30. */
FNIEMOP_DEF(iemOp_wrmsr)
{
    IEMOP_MNEMONIC("wrmsr");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_wrmsr);
}


/** Opcode 0x0f 0x31. */
FNIEMOP_DEF(iemOp_rdtsc)
{
    IEMOP_MNEMONIC("rdtsc");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_rdtsc);
}


/** Opcode 0x0f 0x33. */
FNIEMOP_DEF(iemOp_rdmsr)
{
    IEMOP_MNEMONIC("rdmsr");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_rdmsr);
}


/** Opcode 0x0f 0x34. */
FNIEMOP_STUB(iemOp_rdpmc);
/** Opcode 0x0f 0x34. */
FNIEMOP_STUB(iemOp_sysenter);
/** Opcode 0x0f 0x35. */
FNIEMOP_STUB(iemOp_sysexit);
/** Opcode 0x0f 0x37. */
FNIEMOP_STUB(iemOp_getsec);
/** Opcode 0x0f 0x38. */
FNIEMOP_UD_STUB(iemOp_3byte_Esc_A4); /* Here there be dragons... */
/** Opcode 0x0f 0x3a. */
FNIEMOP_UD_STUB(iemOp_3byte_Esc_A5); /* Here there be dragons... */


/**
 * Implements a conditional move.
 *
 * Wish there was an obvious way to do this where we could share and reduce
 * code bloat.
 *
 * @param   a_Cnd       The conditional "microcode" operation.
 */
#define CMOV_X(a_Cnd) \
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm); \
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT)) \
    { \
        switch (pVCpu->iem.s.enmEffOpSize) \
        { \
            case IEMMODE_16BIT: \
                IEM_MC_BEGIN(0, 1); \
                IEM_MC_LOCAL(uint16_t, u16Tmp); \
                a_Cnd { \
                    IEM_MC_FETCH_GREG_U16(u16Tmp, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB); \
                    IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u16Tmp); \
                } IEM_MC_ENDIF(); \
                IEM_MC_ADVANCE_RIP(); \
                IEM_MC_END(); \
                return VINF_SUCCESS; \
    \
            case IEMMODE_32BIT: \
                IEM_MC_BEGIN(0, 1); \
                IEM_MC_LOCAL(uint32_t, u32Tmp); \
                a_Cnd { \
                    IEM_MC_FETCH_GREG_U32(u32Tmp, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB); \
                    IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u32Tmp); \
                } IEM_MC_ELSE() { \
                    IEM_MC_CLEAR_HIGH_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg); \
                } IEM_MC_ENDIF(); \
                IEM_MC_ADVANCE_RIP(); \
                IEM_MC_END(); \
                return VINF_SUCCESS; \
    \
            case IEMMODE_64BIT: \
                IEM_MC_BEGIN(0, 1); \
                IEM_MC_LOCAL(uint64_t, u64Tmp); \
                a_Cnd { \
                    IEM_MC_FETCH_GREG_U64(u64Tmp, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB); \
                    IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u64Tmp); \
                } IEM_MC_ENDIF(); \
                IEM_MC_ADVANCE_RIP(); \
                IEM_MC_END(); \
                return VINF_SUCCESS; \
    \
            IEM_NOT_REACHED_DEFAULT_CASE_RET(); \
        } \
    } \
    else \
    { \
        switch (pVCpu->iem.s.enmEffOpSize) \
        { \
            case IEMMODE_16BIT: \
                IEM_MC_BEGIN(0, 2); \
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffSrc); \
                IEM_MC_LOCAL(uint16_t, u16Tmp); \
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0); \
                IEM_MC_FETCH_MEM_U16(u16Tmp, pVCpu->iem.s.iEffSeg, GCPtrEffSrc); \
                a_Cnd { \
                    IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u16Tmp); \
                } IEM_MC_ENDIF(); \
                IEM_MC_ADVANCE_RIP(); \
                IEM_MC_END(); \
                return VINF_SUCCESS; \
    \
            case IEMMODE_32BIT: \
                IEM_MC_BEGIN(0, 2); \
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffSrc); \
                IEM_MC_LOCAL(uint32_t, u32Tmp); \
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0); \
                IEM_MC_FETCH_MEM_U32(u32Tmp, pVCpu->iem.s.iEffSeg, GCPtrEffSrc); \
                a_Cnd { \
                    IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u32Tmp); \
                } IEM_MC_ELSE() { \
                    IEM_MC_CLEAR_HIGH_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg); \
                } IEM_MC_ENDIF(); \
                IEM_MC_ADVANCE_RIP(); \
                IEM_MC_END(); \
                return VINF_SUCCESS; \
    \
            case IEMMODE_64BIT: \
                IEM_MC_BEGIN(0, 2); \
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffSrc); \
                IEM_MC_LOCAL(uint64_t, u64Tmp); \
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0); \
                IEM_MC_FETCH_MEM_U64(u64Tmp, pVCpu->iem.s.iEffSeg, GCPtrEffSrc); \
                a_Cnd { \
                    IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u64Tmp); \
                } IEM_MC_ENDIF(); \
                IEM_MC_ADVANCE_RIP(); \
                IEM_MC_END(); \
                return VINF_SUCCESS; \
    \
            IEM_NOT_REACHED_DEFAULT_CASE_RET(); \
        } \
    } do {} while (0)



/** Opcode 0x0f 0x40. */
FNIEMOP_DEF(iemOp_cmovo_Gv_Ev)
{
    IEMOP_MNEMONIC("cmovo Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BIT_SET(X86_EFL_OF));
}


/** Opcode 0x0f 0x41. */
FNIEMOP_DEF(iemOp_cmovno_Gv_Ev)
{
    IEMOP_MNEMONIC("cmovno Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BIT_NOT_SET(X86_EFL_OF));
}


/** Opcode 0x0f 0x42. */
FNIEMOP_DEF(iemOp_cmovc_Gv_Ev)
{
    IEMOP_MNEMONIC("cmovc Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BIT_SET(X86_EFL_CF));
}


/** Opcode 0x0f 0x43. */
FNIEMOP_DEF(iemOp_cmovnc_Gv_Ev)
{
    IEMOP_MNEMONIC("cmovnc Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BIT_NOT_SET(X86_EFL_CF));
}


/** Opcode 0x0f 0x44. */
FNIEMOP_DEF(iemOp_cmove_Gv_Ev)
{
    IEMOP_MNEMONIC("cmove Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BIT_SET(X86_EFL_ZF));
}


/** Opcode 0x0f 0x45. */
FNIEMOP_DEF(iemOp_cmovne_Gv_Ev)
{
    IEMOP_MNEMONIC("cmovne Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BIT_NOT_SET(X86_EFL_ZF));
}


/** Opcode 0x0f 0x46. */
FNIEMOP_DEF(iemOp_cmovbe_Gv_Ev)
{
    IEMOP_MNEMONIC("cmovbe Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_ANY_BITS_SET(X86_EFL_CF | X86_EFL_ZF));
}


/** Opcode 0x0f 0x47. */
FNIEMOP_DEF(iemOp_cmovnbe_Gv_Ev)
{
    IEMOP_MNEMONIC("cmovnbe Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_NO_BITS_SET(X86_EFL_CF | X86_EFL_ZF));
}


/** Opcode 0x0f 0x48. */
FNIEMOP_DEF(iemOp_cmovs_Gv_Ev)
{
    IEMOP_MNEMONIC("cmovs Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BIT_SET(X86_EFL_SF));
}


/** Opcode 0x0f 0x49. */
FNIEMOP_DEF(iemOp_cmovns_Gv_Ev)
{
    IEMOP_MNEMONIC("cmovns Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BIT_NOT_SET(X86_EFL_SF));
}


/** Opcode 0x0f 0x4a. */
FNIEMOP_DEF(iemOp_cmovp_Gv_Ev)
{
    IEMOP_MNEMONIC("cmovp Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BIT_SET(X86_EFL_PF));
}


/** Opcode 0x0f 0x4b. */
FNIEMOP_DEF(iemOp_cmovnp_Gv_Ev)
{
    IEMOP_MNEMONIC("cmovnp Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BIT_NOT_SET(X86_EFL_PF));
}


/** Opcode 0x0f 0x4c. */
FNIEMOP_DEF(iemOp_cmovl_Gv_Ev)
{
    IEMOP_MNEMONIC("cmovl Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BITS_NE(X86_EFL_SF, X86_EFL_OF));
}


/** Opcode 0x0f 0x4d. */
FNIEMOP_DEF(iemOp_cmovnl_Gv_Ev)
{
    IEMOP_MNEMONIC("cmovnl Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BITS_EQ(X86_EFL_SF, X86_EFL_OF));
}


/** Opcode 0x0f 0x4e. */
FNIEMOP_DEF(iemOp_cmovle_Gv_Ev)
{
    IEMOP_MNEMONIC("cmovle Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BIT_SET_OR_BITS_NE(X86_EFL_ZF, X86_EFL_SF, X86_EFL_OF));
}


/** Opcode 0x0f 0x4f. */
FNIEMOP_DEF(iemOp_cmovnle_Gv_Ev)
{
    IEMOP_MNEMONIC("cmovnle Gv,Ev");
    CMOV_X(IEM_MC_IF_EFL_BIT_NOT_SET_AND_BITS_EQ(X86_EFL_ZF, X86_EFL_SF, X86_EFL_OF));
}

#undef CMOV_X

/** Opcode 0x0f 0x50. */
FNIEMOP_STUB(iemOp_movmskps_Gy_Ups__movmskpd_Gy_Upd);
/** Opcode 0x0f 0x51. */
FNIEMOP_STUB(iemOp_sqrtps_Wps_Vps__sqrtpd_Wpd_Vpd__sqrtss_Vss_Wss__sqrtsd_Vsd_Wsd);
/** Opcode 0x0f 0x52. */
FNIEMOP_STUB(iemOp_rsqrtps_Wps_Vps__rsqrtss_Vss_Wss);
/** Opcode 0x0f 0x53. */
FNIEMOP_STUB(iemOp_rcpps_Wps_Vps__rcpss_Vs_Wss);
/** Opcode 0x0f 0x54. */
FNIEMOP_STUB(iemOp_andps_Vps_Wps__andpd_Wpd_Vpd);
/** Opcode 0x0f 0x55. */
FNIEMOP_STUB(iemOp_andnps_Vps_Wps__andnpd_Wpd_Vpd);
/** Opcode 0x0f 0x56. */
FNIEMOP_STUB(iemOp_orps_Wpd_Vpd__orpd_Wpd_Vpd);
/** Opcode 0x0f 0x57. */
FNIEMOP_STUB(iemOp_xorps_Vps_Wps__xorpd_Wpd_Vpd);
/** Opcode 0x0f 0x58. */
FNIEMOP_STUB(iemOp_addps_Vps_Wps__addpd_Vpd_Wpd__addss_Vss_Wss__addsd_Vsd_Wsd); //NEXT
/** Opcode 0x0f 0x59. */
FNIEMOP_STUB(iemOp_mulps_Vps_Wps__mulpd_Vpd_Wpd__mulss_Vss__Wss__mulsd_Vsd_Wsd);//NEXT
/** Opcode 0x0f 0x5a. */
FNIEMOP_STUB(iemOp_cvtps2pd_Vpd_Wps__cvtpd2ps_Vps_Wpd__cvtss2sd_Vsd_Wss__cvtsd2ss_Vss_Wsd);
/** Opcode 0x0f 0x5b. */
FNIEMOP_STUB(iemOp_cvtdq2ps_Vps_Wdq__cvtps2dq_Vdq_Wps__cvtps2dq_Vdq_Wps);
/** Opcode 0x0f 0x5c. */
FNIEMOP_STUB(iemOp_subps_Vps_Wps__subpd_Vps_Wdp__subss_Vss_Wss__subsd_Vsd_Wsd);
/** Opcode 0x0f 0x5d. */
FNIEMOP_STUB(iemOp_minps_Vps_Wps__minpd_Vpd_Wpd__minss_Vss_Wss__minsd_Vsd_Wsd);
/** Opcode 0x0f 0x5e. */
FNIEMOP_STUB(iemOp_divps_Vps_Wps__divpd_Vpd_Wpd__divss_Vss_Wss__divsd_Vsd_Wsd);
/** Opcode 0x0f 0x5f. */
FNIEMOP_STUB(iemOp_maxps_Vps_Wps__maxpd_Vpd_Wpd__maxss_Vss_Wss__maxsd_Vsd_Wsd);


/**
 * Common worker for SSE2 and MMX instructions on the forms:
 *      pxxxx xmm1, xmm2/mem128
 *      pxxxx mm1, mm2/mem32
 *
 * The 2nd operand is the first half of a register, which in the memory case
 * means a 32-bit memory access for MMX and 128-bit aligned 64-bit or 128-bit
 * memory accessed for MMX.
 *
 * Exceptions type 4.
 */
FNIEMOP_DEF_1(iemOpCommonMmxSse_LowLow_To_Full, PCIEMOPMEDIAF1L1, pImpl)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        case IEM_OP_PRF_SIZE_OP: /* SSE */
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /*
                 * Register, register.
                 */
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(2, 0);
                IEM_MC_ARG(uint128_t *,          pDst, 0);
                IEM_MC_ARG(uint64_t const *,     pSrc, 1);
                IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
                IEM_MC_PREPARE_SSE_USAGE();
                IEM_MC_REF_XREG_U128(pDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_XREG_U64_CONST(pSrc, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_CALL_SSE_AIMPL_2(pImpl->pfnU128, pDst, pSrc);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /*
                 * Register, memory.
                 */
                IEM_MC_BEGIN(2, 2);
                IEM_MC_ARG(uint128_t *,                 pDst,       0);
                IEM_MC_LOCAL(uint64_t,                  uSrc);
                IEM_MC_ARG_LOCAL_REF(uint64_t const *,  pSrc, uSrc, 1);
                IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
                IEM_MC_FETCH_MEM_U64_ALIGN_U128(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);

                IEM_MC_PREPARE_SSE_USAGE();
                IEM_MC_REF_XREG_U128(pDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_CALL_SSE_AIMPL_2(pImpl->pfnU128, pDst, pSrc);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;

        case 0: /* MMX */
            if (!pImpl->pfnU64)
                return IEMOP_RAISE_INVALID_OPCODE();
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /*
                 * Register, register.
                 */
                /** @todo testcase: REX.B / REX.R and MMX register indexing. Ignored? */
                /** @todo testcase: REX.B / REX.R and segment register indexing. Ignored? */
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(2, 0);
                IEM_MC_ARG(uint64_t *,          pDst, 0);
                IEM_MC_ARG(uint32_t const *,    pSrc, 1);
                IEM_MC_MAYBE_RAISE_MMX_RELATED_XCPT();
                IEM_MC_PREPARE_FPU_USAGE();
                IEM_MC_REF_MREG_U64(pDst, (bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK);
                IEM_MC_REF_MREG_U32_CONST(pSrc, bRm & X86_MODRM_RM_MASK);
                IEM_MC_CALL_MMX_AIMPL_2(pImpl->pfnU64, pDst, pSrc);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /*
                 * Register, memory.
                 */
                IEM_MC_BEGIN(2, 2);
                IEM_MC_ARG(uint64_t *,                  pDst,       0);
                IEM_MC_LOCAL(uint32_t,                  uSrc);
                IEM_MC_ARG_LOCAL_REF(uint32_t const *,  pSrc, uSrc, 1);
                IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MAYBE_RAISE_MMX_RELATED_XCPT();
                IEM_MC_FETCH_MEM_U32(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);

                IEM_MC_PREPARE_FPU_USAGE();
                IEM_MC_REF_MREG_U64(pDst, (bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK);
                IEM_MC_CALL_MMX_AIMPL_2(pImpl->pfnU64, pDst, pSrc);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;

        default:
            return IEMOP_RAISE_INVALID_OPCODE();
    }
}


/** Opcode 0x0f 0x60. */
FNIEMOP_DEF(iemOp_punpcklbw_Pq_Qd__punpcklbw_Vdq_Wdq)
{
    IEMOP_MNEMONIC("punpcklbw");
    return FNIEMOP_CALL_1(iemOpCommonMmxSse_LowLow_To_Full, &g_iemAImpl_punpcklbw);
}


/** Opcode 0x0f 0x61. */
FNIEMOP_DEF(iemOp_punpcklwd_Pq_Qd__punpcklwd_Vdq_Wdq)
{
    IEMOP_MNEMONIC("punpcklwd"); /** @todo AMD mark the MMX version as 3DNow!. Intel says MMX CPUID req. */
    return FNIEMOP_CALL_1(iemOpCommonMmxSse_LowLow_To_Full, &g_iemAImpl_punpcklwd);
}


/** Opcode 0x0f 0x62. */
FNIEMOP_DEF(iemOp_punpckldq_Pq_Qd__punpckldq_Vdq_Wdq)
{
    IEMOP_MNEMONIC("punpckldq");
    return FNIEMOP_CALL_1(iemOpCommonMmxSse_LowLow_To_Full, &g_iemAImpl_punpckldq);
}


/** Opcode 0x0f 0x63. */
FNIEMOP_STUB(iemOp_packsswb_Pq_Qq__packsswb_Vdq_Wdq);
/** Opcode 0x0f 0x64. */
FNIEMOP_STUB(iemOp_pcmpgtb_Pq_Qq__pcmpgtb_Vdq_Wdq);
/** Opcode 0x0f 0x65. */
FNIEMOP_STUB(iemOp_pcmpgtw_Pq_Qq__pcmpgtw_Vdq_Wdq);
/** Opcode 0x0f 0x66. */
FNIEMOP_STUB(iemOp_pcmpgtd_Pq_Qq__pcmpgtd_Vdq_Wdq);
/** Opcode 0x0f 0x67. */
FNIEMOP_STUB(iemOp_packuswb_Pq_Qq__packuswb_Vdq_Wdq);


/**
 * Common worker for SSE2 and MMX instructions on the forms:
 *      pxxxx xmm1, xmm2/mem128
 *      pxxxx mm1, mm2/mem64
 *
 * The 2nd operand is the second half of a register, which in the memory case
 * means a 64-bit memory access for MMX, and for MMX a 128-bit aligned access
 * where it may read the full 128 bits or only the upper 64 bits.
 *
 * Exceptions type 4.
 */
FNIEMOP_DEF_1(iemOpCommonMmxSse_HighHigh_To_Full, PCIEMOPMEDIAF1H1, pImpl)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        case IEM_OP_PRF_SIZE_OP: /* SSE */
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /*
                 * Register, register.
                 */
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(2, 0);
                IEM_MC_ARG(uint128_t *,          pDst, 0);
                IEM_MC_ARG(uint128_t const *,    pSrc, 1);
                IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
                IEM_MC_PREPARE_SSE_USAGE();
                IEM_MC_REF_XREG_U128(pDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_XREG_U128_CONST(pSrc, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_CALL_SSE_AIMPL_2(pImpl->pfnU128, pDst, pSrc);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /*
                 * Register, memory.
                 */
                IEM_MC_BEGIN(2, 2);
                IEM_MC_ARG(uint128_t *,                 pDst,       0);
                IEM_MC_LOCAL(uint128_t,                 uSrc);
                IEM_MC_ARG_LOCAL_REF(uint128_t const *, pSrc, uSrc, 1);
                IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
                IEM_MC_FETCH_MEM_U128_ALIGN_SSE(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc); /* Most CPUs probably only right high qword */

                IEM_MC_PREPARE_SSE_USAGE();
                IEM_MC_REF_XREG_U128(pDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_CALL_SSE_AIMPL_2(pImpl->pfnU128, pDst, pSrc);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;

        case 0: /* MMX */
            if (!pImpl->pfnU64)
                return IEMOP_RAISE_INVALID_OPCODE();
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /*
                 * Register, register.
                 */
                /** @todo testcase: REX.B / REX.R and MMX register indexing. Ignored? */
                /** @todo testcase: REX.B / REX.R and segment register indexing. Ignored? */
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(2, 0);
                IEM_MC_ARG(uint64_t *,          pDst, 0);
                IEM_MC_ARG(uint64_t const *,    pSrc, 1);
                IEM_MC_MAYBE_RAISE_MMX_RELATED_XCPT();
                IEM_MC_PREPARE_FPU_USAGE();
                IEM_MC_REF_MREG_U64(pDst, (bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK);
                IEM_MC_REF_MREG_U64_CONST(pSrc, bRm & X86_MODRM_RM_MASK);
                IEM_MC_CALL_MMX_AIMPL_2(pImpl->pfnU64, pDst, pSrc);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /*
                 * Register, memory.
                 */
                IEM_MC_BEGIN(2, 2);
                IEM_MC_ARG(uint64_t *,                  pDst,       0);
                IEM_MC_LOCAL(uint64_t,                  uSrc);
                IEM_MC_ARG_LOCAL_REF(uint64_t const *,  pSrc, uSrc, 1);
                IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MAYBE_RAISE_MMX_RELATED_XCPT();
                IEM_MC_FETCH_MEM_U64(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);

                IEM_MC_PREPARE_FPU_USAGE();
                IEM_MC_REF_MREG_U64(pDst, (bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK);
                IEM_MC_CALL_MMX_AIMPL_2(pImpl->pfnU64, pDst, pSrc);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;

        default:
            return IEMOP_RAISE_INVALID_OPCODE();
    }
}


/** Opcode 0x0f 0x68. */
FNIEMOP_DEF(iemOp_punpckhbw_Pq_Qq__punpckhbw_Vdq_Wdq)
{
    IEMOP_MNEMONIC("punpckhbw");
    return FNIEMOP_CALL_1(iemOpCommonMmxSse_HighHigh_To_Full, &g_iemAImpl_punpckhbw);
}


/** Opcode 0x0f 0x69. */
FNIEMOP_DEF(iemOp_punpckhwd_Pq_Qd__punpckhwd_Vdq_Wdq)
{
    IEMOP_MNEMONIC("punpckhwd");
    return FNIEMOP_CALL_1(iemOpCommonMmxSse_HighHigh_To_Full, &g_iemAImpl_punpckhwd);
}


/** Opcode 0x0f 0x6a. */
FNIEMOP_DEF(iemOp_punpckhdq_Pq_Qd__punpckhdq_Vdq_Wdq)
{
    IEMOP_MNEMONIC("punpckhdq");
    return FNIEMOP_CALL_1(iemOpCommonMmxSse_HighHigh_To_Full, &g_iemAImpl_punpckhdq);
}

/** Opcode 0x0f 0x6b. */
FNIEMOP_STUB(iemOp_packssdw_Pq_Qd__packssdq_Vdq_Wdq);


/** Opcode 0x0f 0x6c. */
FNIEMOP_DEF(iemOp_punpcklqdq_Vdq_Wdq)
{
    IEMOP_MNEMONIC("punpcklqdq");
    return FNIEMOP_CALL_1(iemOpCommonMmxSse_LowLow_To_Full, &g_iemAImpl_punpcklqdq);
}


/** Opcode 0x0f 0x6d. */
FNIEMOP_DEF(iemOp_punpckhqdq_Vdq_Wdq)
{
    IEMOP_MNEMONIC("punpckhqdq");
    return FNIEMOP_CALL_1(iemOpCommonMmxSse_HighHigh_To_Full, &g_iemAImpl_punpckhqdq);
}


/** Opcode 0x0f 0x6e. */
FNIEMOP_DEF(iemOp_movd_q_Pd_Ey__movd_q_Vy_Ey)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        case IEM_OP_PRF_SIZE_OP: /* SSE */
            IEMOP_MNEMONIC("movd/q Wd/q,Ed/q");
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /* XMM, greg*/
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(0, 1);
                IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
                IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
                if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_REX_W)
                {
                    IEM_MC_LOCAL(uint64_t, u64Tmp);
                    IEM_MC_FETCH_GREG_U64(u64Tmp, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                    IEM_MC_STORE_XREG_U64_ZX_U128(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u64Tmp);
                }
                else
                {
                    IEM_MC_LOCAL(uint32_t, u32Tmp);
                    IEM_MC_FETCH_GREG_U32(u32Tmp, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                    IEM_MC_STORE_XREG_U32_ZX_U128(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u32Tmp);
                }
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /* XMM, [mem] */
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
                IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT(); /** @todo order */
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 1);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
                if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_REX_W)
                {
                    IEM_MC_LOCAL(uint64_t, u64Tmp);
                    IEM_MC_FETCH_MEM_U64(u64Tmp, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
                    IEM_MC_STORE_XREG_U64_ZX_U128(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u64Tmp);
                }
                else
                {
                    IEM_MC_LOCAL(uint32_t, u32Tmp);
                    IEM_MC_FETCH_MEM_U32(u32Tmp, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
                    IEM_MC_STORE_XREG_U32_ZX_U128(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u32Tmp);
                }
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;

        case 0: /* MMX */
            IEMOP_MNEMONIC("movq/d Pd/q,Ed/q");
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /* MMX, greg */
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(0, 1);
                IEM_MC_MAYBE_RAISE_MMX_RELATED_XCPT();
                IEM_MC_ACTUALIZE_FPU_STATE_FOR_CHANGE();
                IEM_MC_LOCAL(uint64_t, u64Tmp);
                if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_REX_W)
                    IEM_MC_FETCH_GREG_U64(u64Tmp, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                else
                    IEM_MC_FETCH_GREG_U32_ZX_U64(u64Tmp, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_STORE_MREG_U64((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK, u64Tmp);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /* MMX, [mem] */
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
                IEM_MC_MAYBE_RAISE_MMX_RELATED_XCPT();
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 1);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_ACTUALIZE_FPU_STATE_FOR_CHANGE();
                if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_REX_W)
                {
                    IEM_MC_LOCAL(uint64_t, u64Tmp);
                    IEM_MC_FETCH_MEM_U64(u64Tmp, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
                    IEM_MC_STORE_MREG_U64((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK, u64Tmp);
                }
                else
                {
                    IEM_MC_LOCAL(uint32_t, u32Tmp);
                    IEM_MC_FETCH_MEM_U32(u32Tmp, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
                    IEM_MC_STORE_MREG_U32_ZX_U64((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK, u32Tmp);
                }
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;

        default:
            return IEMOP_RAISE_INVALID_OPCODE();
    }
}


/** Opcode 0x0f 0x6f. */
FNIEMOP_DEF(iemOp_movq_Pq_Qq__movdqa_Vdq_Wdq__movdqu_Vdq_Wdq)
{
    bool fAligned = false;
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        case IEM_OP_PRF_SIZE_OP: /* SSE aligned */
            fAligned = true;
        case IEM_OP_PRF_REPZ: /* SSE unaligned */
            if (fAligned)
                IEMOP_MNEMONIC("movdqa Vdq,Wdq");
            else
                IEMOP_MNEMONIC("movdqu Vdq,Wdq");
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /*
                 * Register, register.
                 */
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(0, 0);
                IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
                IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
                IEM_MC_COPY_XREG_U128(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg,
                                      (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /*
                 * Register, memory.
                 */
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint128_t,  u128Tmp);
                IEM_MC_LOCAL(RTGCPTR,    GCPtrEffSrc);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
                IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
                if (fAligned)
                    IEM_MC_FETCH_MEM_U128_ALIGN_SSE(u128Tmp, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
                else
                    IEM_MC_FETCH_MEM_U128(u128Tmp, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
                IEM_MC_STORE_XREG_U128(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u128Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;

        case 0: /* MMX */
            IEMOP_MNEMONIC("movq Pq,Qq");
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /*
                 * Register, register.
                 */
                /** @todo testcase: REX.B / REX.R and MMX register indexing. Ignored? */
                /** @todo testcase: REX.B / REX.R and segment register indexing. Ignored? */
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint64_t, u64Tmp);
                IEM_MC_MAYBE_RAISE_MMX_RELATED_XCPT();
                IEM_MC_ACTUALIZE_FPU_STATE_FOR_CHANGE();
                IEM_MC_FETCH_MREG_U64(u64Tmp, bRm & X86_MODRM_RM_MASK);
                IEM_MC_STORE_MREG_U64((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK, u64Tmp);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /*
                 * Register, memory.
                 */
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint64_t, u64Tmp);
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffSrc);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MAYBE_RAISE_MMX_RELATED_XCPT();
                IEM_MC_ACTUALIZE_FPU_STATE_FOR_CHANGE();
                IEM_MC_FETCH_MEM_U64(u64Tmp, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
                IEM_MC_STORE_MREG_U64((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK, u64Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;

        default:
            return IEMOP_RAISE_INVALID_OPCODE();
    }
}


/** Opcode 0x0f 0x70. The immediate here is evil! */
FNIEMOP_DEF(iemOp_pshufw_Pq_Qq_Ib__pshufd_Vdq_Wdq_Ib__pshufhw_Vdq_Wdq_Ib__pshuflq_Vdq_Wdq_Ib)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        case IEM_OP_PRF_SIZE_OP: /* SSE */
        case IEM_OP_PRF_REPNZ:   /* SSE */
        case IEM_OP_PRF_REPZ:    /* SSE */
        {
            PFNIEMAIMPLMEDIAPSHUF pfnAImpl;
            switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
            {
                case IEM_OP_PRF_SIZE_OP:
                    IEMOP_MNEMONIC("pshufd Vdq,Wdq,Ib");
                    pfnAImpl = iemAImpl_pshufd;
                    break;
                case IEM_OP_PRF_REPNZ:
                    IEMOP_MNEMONIC("pshuflw Vdq,Wdq,Ib");
                    pfnAImpl = iemAImpl_pshuflw;
                    break;
                case IEM_OP_PRF_REPZ:
                    IEMOP_MNEMONIC("pshufhw Vdq,Wdq,Ib");
                    pfnAImpl = iemAImpl_pshufhw;
                    break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /*
                 * Register, register.
                 */
                uint8_t bEvil; IEM_OPCODE_GET_NEXT_U8(&bEvil);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint128_t *,         pDst, 0);
                IEM_MC_ARG(uint128_t const *,   pSrc, 1);
                IEM_MC_ARG_CONST(uint8_t,       bEvilArg, /*=*/ bEvil, 2);
                IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
                IEM_MC_PREPARE_SSE_USAGE();
                IEM_MC_REF_XREG_U128(pDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_XREG_U128_CONST(pSrc, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_CALL_SSE_AIMPL_3(pfnAImpl, pDst, pSrc, bEvilArg);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /*
                 * Register, memory.
                 */
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint128_t *,                 pDst,       0);
                IEM_MC_LOCAL(uint128_t,                 uSrc);
                IEM_MC_ARG_LOCAL_REF(uint128_t const *, pSrc, uSrc, 1);
                IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                uint8_t bEvil; IEM_OPCODE_GET_NEXT_U8(&bEvil);
                IEM_MC_ARG_CONST(uint8_t,               bEvilArg, /*=*/ bEvil, 2);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();

                IEM_MC_FETCH_MEM_U128_ALIGN_SSE(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
                IEM_MC_PREPARE_SSE_USAGE();
                IEM_MC_REF_XREG_U128(pDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_CALL_SSE_AIMPL_3(pfnAImpl, pDst, pSrc, bEvilArg);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;
        }

        case 0: /* MMX Extension */
            IEMOP_MNEMONIC("pshufw Pq,Qq,Ib");
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /*
                 * Register, register.
                 */
                uint8_t bEvil; IEM_OPCODE_GET_NEXT_U8(&bEvil);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint64_t *,          pDst, 0);
                IEM_MC_ARG(uint64_t const *,    pSrc, 1);
                IEM_MC_ARG_CONST(uint8_t,       bEvilArg, /*=*/ bEvil, 2);
                IEM_MC_MAYBE_RAISE_MMX_RELATED_XCPT_CHECK_SSE_OR_MMXEXT();
                IEM_MC_PREPARE_FPU_USAGE();
                IEM_MC_REF_MREG_U64(pDst, (bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK);
                IEM_MC_REF_MREG_U64_CONST(pSrc, bRm & X86_MODRM_RM_MASK);
                IEM_MC_CALL_MMX_AIMPL_3(iemAImpl_pshufw, pDst, pSrc, bEvilArg);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /*
                 * Register, memory.
                 */
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint64_t *,                  pDst,       0);
                IEM_MC_LOCAL(uint64_t,                  uSrc);
                IEM_MC_ARG_LOCAL_REF(uint64_t const *,  pSrc, uSrc, 1);
                IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                uint8_t bEvil; IEM_OPCODE_GET_NEXT_U8(&bEvil);
                IEM_MC_ARG_CONST(uint8_t,               bEvilArg, /*=*/ bEvil, 2);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MAYBE_RAISE_MMX_RELATED_XCPT_CHECK_SSE_OR_MMXEXT();

                IEM_MC_FETCH_MEM_U64(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
                IEM_MC_PREPARE_FPU_USAGE();
                IEM_MC_REF_MREG_U64(pDst, (bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK);
                IEM_MC_CALL_MMX_AIMPL_3(iemAImpl_pshufw, pDst, pSrc, bEvilArg);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;

        default:
            return IEMOP_RAISE_INVALID_OPCODE();
    }
}


/** Opcode 0x0f 0x71 11/2. */
FNIEMOP_STUB_1(iemOp_Grp12_psrlw_Nq_Ib,  uint8_t, bRm);

/** Opcode 0x66 0x0f 0x71 11/2. */
FNIEMOP_STUB_1(iemOp_Grp12_psrlw_Udq_Ib, uint8_t, bRm);

/** Opcode 0x0f 0x71 11/4. */
FNIEMOP_STUB_1(iemOp_Grp12_psraw_Nq_Ib,  uint8_t, bRm);

/** Opcode 0x66 0x0f 0x71 11/4. */
FNIEMOP_STUB_1(iemOp_Grp12_psraw_Udq_Ib, uint8_t, bRm);

/** Opcode 0x0f 0x71 11/6. */
FNIEMOP_STUB_1(iemOp_Grp12_psllw_Nq_Ib,  uint8_t, bRm);

/** Opcode 0x66 0x0f 0x71 11/6. */
FNIEMOP_STUB_1(iemOp_Grp12_psllw_Udq_Ib, uint8_t, bRm);


/** Opcode 0x0f 0x71. */
FNIEMOP_DEF(iemOp_Grp12)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT))
        return IEMOP_RAISE_INVALID_OPCODE();
    switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
    {
        case 0: case 1: case 3: case 5: case 7:
            return IEMOP_RAISE_INVALID_OPCODE();
        case 2:
            switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPZ | IEM_OP_PRF_REPNZ))
            {
                case 0:                     return FNIEMOP_CALL_1(iemOp_Grp12_psrlw_Nq_Ib, bRm);
                case IEM_OP_PRF_SIZE_OP:    return FNIEMOP_CALL_1(iemOp_Grp12_psrlw_Udq_Ib, bRm);
                default:                    return IEMOP_RAISE_INVALID_OPCODE();
            }
        case 4:
            switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPZ | IEM_OP_PRF_REPNZ))
            {
                case 0:                     return FNIEMOP_CALL_1(iemOp_Grp12_psraw_Nq_Ib, bRm);
                case IEM_OP_PRF_SIZE_OP:    return FNIEMOP_CALL_1(iemOp_Grp12_psraw_Udq_Ib, bRm);
                default:                    return IEMOP_RAISE_INVALID_OPCODE();
            }
        case 6:
            switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPZ | IEM_OP_PRF_REPNZ))
            {
                case 0:                     return FNIEMOP_CALL_1(iemOp_Grp12_psllw_Nq_Ib, bRm);
                case IEM_OP_PRF_SIZE_OP:    return FNIEMOP_CALL_1(iemOp_Grp12_psllw_Udq_Ib, bRm);
                default:                    return IEMOP_RAISE_INVALID_OPCODE();
            }
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/** Opcode 0x0f 0x72 11/2. */
FNIEMOP_STUB_1(iemOp_Grp13_psrld_Nq_Ib,  uint8_t, bRm);

/** Opcode 0x66 0x0f 0x72 11/2. */
FNIEMOP_STUB_1(iemOp_Grp13_psrld_Udq_Ib, uint8_t, bRm);

/** Opcode 0x0f 0x72 11/4. */
FNIEMOP_STUB_1(iemOp_Grp13_psrad_Nq_Ib,  uint8_t, bRm);

/** Opcode 0x66 0x0f 0x72 11/4. */
FNIEMOP_STUB_1(iemOp_Grp13_psrad_Udq_Ib, uint8_t, bRm);

/** Opcode 0x0f 0x72 11/6. */
FNIEMOP_STUB_1(iemOp_Grp13_pslld_Nq_Ib,  uint8_t, bRm);

/** Opcode 0x66 0x0f 0x72 11/6. */
FNIEMOP_STUB_1(iemOp_Grp13_pslld_Udq_Ib, uint8_t, bRm);


/** Opcode 0x0f 0x72. */
FNIEMOP_DEF(iemOp_Grp13)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT))
        return IEMOP_RAISE_INVALID_OPCODE();
    switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
    {
        case 0: case 1: case 3: case 5: case 7:
            return IEMOP_RAISE_INVALID_OPCODE();
        case 2:
            switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPZ | IEM_OP_PRF_REPNZ))
            {
                case 0:                     return FNIEMOP_CALL_1(iemOp_Grp13_psrld_Nq_Ib, bRm);
                case IEM_OP_PRF_SIZE_OP:    return FNIEMOP_CALL_1(iemOp_Grp13_psrld_Udq_Ib, bRm);
                default:                    return IEMOP_RAISE_INVALID_OPCODE();
            }
        case 4:
            switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPZ | IEM_OP_PRF_REPNZ))
            {
                case 0:                     return FNIEMOP_CALL_1(iemOp_Grp13_psrad_Nq_Ib, bRm);
                case IEM_OP_PRF_SIZE_OP:    return FNIEMOP_CALL_1(iemOp_Grp13_psrad_Udq_Ib, bRm);
                default:                    return IEMOP_RAISE_INVALID_OPCODE();
            }
        case 6:
            switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPZ | IEM_OP_PRF_REPNZ))
            {
                case 0:                     return FNIEMOP_CALL_1(iemOp_Grp13_pslld_Nq_Ib, bRm);
                case IEM_OP_PRF_SIZE_OP:    return FNIEMOP_CALL_1(iemOp_Grp13_pslld_Udq_Ib, bRm);
                default:                    return IEMOP_RAISE_INVALID_OPCODE();
            }
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/** Opcode 0x0f 0x73 11/2. */
FNIEMOP_STUB_1(iemOp_Grp14_psrlq_Nq_Ib,   uint8_t, bRm);

/** Opcode 0x66 0x0f 0x73 11/2. */
FNIEMOP_STUB_1(iemOp_Grp14_psrlq_Udq_Ib,  uint8_t, bRm);

/** Opcode 0x66 0x0f 0x73 11/3. */
FNIEMOP_STUB_1(iemOp_Grp14_psrldq_Udq_Ib, uint8_t, bRm); //NEXT

/** Opcode 0x0f 0x73 11/6. */
FNIEMOP_STUB_1(iemOp_Grp14_psllq_Nq_Ib,   uint8_t, bRm);

/** Opcode 0x66 0x0f 0x73 11/6. */
FNIEMOP_STUB_1(iemOp_Grp14_psllq_Udq_Ib,  uint8_t, bRm);

/** Opcode 0x66 0x0f 0x73 11/7. */
FNIEMOP_STUB_1(iemOp_Grp14_pslldq_Udq_Ib, uint8_t, bRm); //NEXT


/** Opcode 0x0f 0x73. */
FNIEMOP_DEF(iemOp_Grp14)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT))
        return IEMOP_RAISE_INVALID_OPCODE();
    switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
    {
        case 0: case 1: case 4: case 5:
            return IEMOP_RAISE_INVALID_OPCODE();
        case 2:
            switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPZ | IEM_OP_PRF_REPNZ))
            {
                case 0:                     return FNIEMOP_CALL_1(iemOp_Grp14_psrlq_Nq_Ib, bRm);
                case IEM_OP_PRF_SIZE_OP:    return FNIEMOP_CALL_1(iemOp_Grp14_psrlq_Udq_Ib, bRm);
                default:                    return IEMOP_RAISE_INVALID_OPCODE();
            }
        case 3:
            switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPZ | IEM_OP_PRF_REPNZ))
            {
                case IEM_OP_PRF_SIZE_OP:    return FNIEMOP_CALL_1(iemOp_Grp14_psrldq_Udq_Ib, bRm);
                default:                    return IEMOP_RAISE_INVALID_OPCODE();
            }
        case 6:
            switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPZ | IEM_OP_PRF_REPNZ))
            {
                case 0:                     return FNIEMOP_CALL_1(iemOp_Grp14_psllq_Nq_Ib, bRm);
                case IEM_OP_PRF_SIZE_OP:    return FNIEMOP_CALL_1(iemOp_Grp14_psllq_Udq_Ib, bRm);
                default:                    return IEMOP_RAISE_INVALID_OPCODE();
            }
        case 7:
            switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPZ | IEM_OP_PRF_REPNZ))
            {
                case IEM_OP_PRF_SIZE_OP:    return FNIEMOP_CALL_1(iemOp_Grp14_pslldq_Udq_Ib, bRm);
                default:                    return IEMOP_RAISE_INVALID_OPCODE();
            }
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/**
 * Common worker for SSE2 and MMX instructions on the forms:
 *      pxxx    mm1, mm2/mem64
 *      pxxx    xmm1, xmm2/mem128
 *
 * Proper alignment of the 128-bit operand is enforced.
 * Exceptions type 4. SSE2 and MMX cpuid checks.
 */
FNIEMOP_DEF_1(iemOpCommonMmxSse2_FullFull_To_Full, PCIEMOPMEDIAF2, pImpl)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        case IEM_OP_PRF_SIZE_OP: /* SSE */
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /*
                 * Register, register.
                 */
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(2, 0);
                IEM_MC_ARG(uint128_t *,          pDst, 0);
                IEM_MC_ARG(uint128_t const *,    pSrc, 1);
                IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
                IEM_MC_PREPARE_SSE_USAGE();
                IEM_MC_REF_XREG_U128(pDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_XREG_U128_CONST(pSrc, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_CALL_SSE_AIMPL_2(pImpl->pfnU128, pDst, pSrc);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /*
                 * Register, memory.
                 */
                IEM_MC_BEGIN(2, 2);
                IEM_MC_ARG(uint128_t *,                 pDst,       0);
                IEM_MC_LOCAL(uint128_t,                 uSrc);
                IEM_MC_ARG_LOCAL_REF(uint128_t const *, pSrc, uSrc, 1);
                IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
                IEM_MC_FETCH_MEM_U128_ALIGN_SSE(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);

                IEM_MC_PREPARE_SSE_USAGE();
                IEM_MC_REF_XREG_U128(pDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_CALL_SSE_AIMPL_2(pImpl->pfnU128, pDst, pSrc);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;

        case 0: /* MMX */
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /*
                 * Register, register.
                 */
                /** @todo testcase: REX.B / REX.R and MMX register indexing. Ignored? */
                /** @todo testcase: REX.B / REX.R and segment register indexing. Ignored? */
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(2, 0);
                IEM_MC_ARG(uint64_t *,          pDst, 0);
                IEM_MC_ARG(uint64_t const *,    pSrc, 1);
                IEM_MC_MAYBE_RAISE_MMX_RELATED_XCPT();
                IEM_MC_PREPARE_FPU_USAGE();
                IEM_MC_REF_MREG_U64(pDst, (bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK);
                IEM_MC_REF_MREG_U64_CONST(pSrc, bRm & X86_MODRM_RM_MASK);
                IEM_MC_CALL_MMX_AIMPL_2(pImpl->pfnU64, pDst, pSrc);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /*
                 * Register, memory.
                 */
                IEM_MC_BEGIN(2, 2);
                IEM_MC_ARG(uint64_t *,                  pDst,       0);
                IEM_MC_LOCAL(uint64_t,                  uSrc);
                IEM_MC_ARG_LOCAL_REF(uint64_t const *,  pSrc, uSrc, 1);
                IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MAYBE_RAISE_MMX_RELATED_XCPT();
                IEM_MC_FETCH_MEM_U64(uSrc, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);

                IEM_MC_PREPARE_FPU_USAGE();
                IEM_MC_REF_MREG_U64(pDst, (bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK);
                IEM_MC_CALL_MMX_AIMPL_2(pImpl->pfnU64, pDst, pSrc);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;

        default:
            return IEMOP_RAISE_INVALID_OPCODE();
    }
}


/** Opcode 0x0f 0x74. */
FNIEMOP_DEF(iemOp_pcmpeqb_Pq_Qq__pcmpeqb_Vdq_Wdq)
{
    IEMOP_MNEMONIC("pcmpeqb");
    return FNIEMOP_CALL_1(iemOpCommonMmxSse2_FullFull_To_Full, &g_iemAImpl_pcmpeqb);
}


/** Opcode 0x0f 0x75. */
FNIEMOP_DEF(iemOp_pcmpeqw_Pq_Qq__pcmpeqw_Vdq_Wdq)
{
    IEMOP_MNEMONIC("pcmpeqw");
    return FNIEMOP_CALL_1(iemOpCommonMmxSse2_FullFull_To_Full, &g_iemAImpl_pcmpeqw);
}


/** Opcode 0x0f 0x76. */
FNIEMOP_DEF(iemOp_pcmped_Pq_Qq__pcmpeqd_Vdq_Wdq)
{
    IEMOP_MNEMONIC("pcmpeqd");
    return FNIEMOP_CALL_1(iemOpCommonMmxSse2_FullFull_To_Full, &g_iemAImpl_pcmpeqd);
}


/** Opcode 0x0f 0x77. */
FNIEMOP_STUB(iemOp_emms);
/** Opcode 0x0f 0x78. */
FNIEMOP_UD_STUB(iemOp_vmread_AmdGrp17);
/** Opcode 0x0f 0x79. */
FNIEMOP_UD_STUB(iemOp_vmwrite);
/** Opcode 0x0f 0x7c. */
FNIEMOP_STUB(iemOp_haddpd_Vdp_Wpd__haddps_Vps_Wps);
/** Opcode 0x0f 0x7d. */
FNIEMOP_STUB(iemOp_hsubpd_Vpd_Wpd__hsubps_Vps_Wps);


/** Opcode 0x0f 0x7e. */
FNIEMOP_DEF(iemOp_movd_q_Ey_Pd__movd_q_Ey_Vy__movq_Vq_Wq)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        case IEM_OP_PRF_SIZE_OP: /* SSE */
            IEMOP_MNEMONIC("movd/q Ed/q,Wd/q");
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /* greg, XMM */
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(0, 1);
                IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
                IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();
                if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_REX_W)
                {
                    IEM_MC_LOCAL(uint64_t, u64Tmp);
                    IEM_MC_FETCH_XREG_U64(u64Tmp, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                    IEM_MC_STORE_GREG_U64((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u64Tmp);
                }
                else
                {
                    IEM_MC_LOCAL(uint32_t, u32Tmp);
                    IEM_MC_FETCH_XREG_U32(u32Tmp, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                    IEM_MC_STORE_GREG_U32((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u32Tmp);
                }
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /* [mem], XMM */
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
                IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 1);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();
                if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_REX_W)
                {
                    IEM_MC_LOCAL(uint64_t, u64Tmp);
                    IEM_MC_FETCH_XREG_U64(u64Tmp, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                    IEM_MC_STORE_MEM_U64(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, u64Tmp);
                }
                else
                {
                    IEM_MC_LOCAL(uint32_t, u32Tmp);
                    IEM_MC_FETCH_XREG_U32(u32Tmp, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                    IEM_MC_STORE_MEM_U32(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, u32Tmp);
                }
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;

        case 0: /* MMX */
            IEMOP_MNEMONIC("movq/d Ed/q,Pd/q");
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /* greg, MMX */
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(0, 1);
                IEM_MC_MAYBE_RAISE_MMX_RELATED_XCPT();
                IEM_MC_ACTUALIZE_FPU_STATE_FOR_READ();
                if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_REX_W)
                {
                    IEM_MC_LOCAL(uint64_t, u64Tmp);
                    IEM_MC_FETCH_MREG_U64(u64Tmp, (bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK);
                    IEM_MC_STORE_GREG_U64((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u64Tmp);
                }
                else
                {
                    IEM_MC_LOCAL(uint32_t, u32Tmp);
                    IEM_MC_FETCH_MREG_U32(u32Tmp, (bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK);
                    IEM_MC_STORE_GREG_U32((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u32Tmp);
                }
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /* [mem], MMX */
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
                IEM_MC_MAYBE_RAISE_MMX_RELATED_XCPT();
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 1);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_ACTUALIZE_FPU_STATE_FOR_READ();
                if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_REX_W)
                {
                    IEM_MC_LOCAL(uint64_t, u64Tmp);
                    IEM_MC_FETCH_MREG_U64(u64Tmp, (bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK);
                    IEM_MC_STORE_MEM_U64(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, u64Tmp);
                }
                else
                {
                    IEM_MC_LOCAL(uint32_t, u32Tmp);
                    IEM_MC_FETCH_MREG_U32(u32Tmp, (bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK);
                    IEM_MC_STORE_MEM_U32(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, u32Tmp);
                }
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;

        default:
            return IEMOP_RAISE_INVALID_OPCODE();
    }
}


/** Opcode 0x0f 0x7f. */
FNIEMOP_DEF(iemOp_movq_Qq_Pq__movq_movdqa_Wdq_Vdq__movdqu_Wdq_Vdq)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    bool fAligned = false;
    switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        case IEM_OP_PRF_SIZE_OP: /* SSE aligned */
            fAligned = true;
        case IEM_OP_PRF_REPZ: /* SSE unaligned */
            if (fAligned)
                IEMOP_MNEMONIC("movdqa Wdq,Vdq");
            else
                IEMOP_MNEMONIC("movdqu Wdq,Vdq");
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /*
                 * Register, register.
                 */
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(0, 0);
                IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
                IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE();
                IEM_MC_COPY_XREG_U128((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB,
                                      ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /*
                 * Register, memory.
                 */
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint128_t,  u128Tmp);
                IEM_MC_LOCAL(RTGCPTR,    GCPtrEffSrc);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
                IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();

                IEM_MC_FETCH_XREG_U128(u128Tmp, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                if (fAligned)
                    IEM_MC_STORE_MEM_U128_ALIGN_SSE(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, u128Tmp);
                else
                    IEM_MC_STORE_MEM_U128(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, u128Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;

        case 0: /* MMX */
            IEMOP_MNEMONIC("movq Qq,Pq");

            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /*
                 * Register, register.
                 */
                /** @todo testcase: REX.B / REX.R and MMX register indexing. Ignored? */
                /** @todo testcase: REX.B / REX.R and segment register indexing. Ignored? */
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint64_t, u64Tmp);
                IEM_MC_MAYBE_RAISE_MMX_RELATED_XCPT();
                IEM_MC_ACTUALIZE_FPU_STATE_FOR_CHANGE();
                IEM_MC_FETCH_MREG_U64(u64Tmp, (bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK);
                IEM_MC_STORE_MREG_U64(bRm & X86_MODRM_RM_MASK, u64Tmp);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /*
                 * Register, memory.
                 */
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint64_t, u64Tmp);
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffSrc);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MAYBE_RAISE_MMX_RELATED_XCPT();
                IEM_MC_ACTUALIZE_FPU_STATE_FOR_READ();

                IEM_MC_FETCH_MREG_U64(u64Tmp, (bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK);
                IEM_MC_STORE_MEM_U64(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, u64Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;

        default:
            return IEMOP_RAISE_INVALID_OPCODE();
    }
}



/** Opcode 0x0f 0x80. */
FNIEMOP_DEF(iemOp_jo_Jv)
{
    IEMOP_MNEMONIC("jo  Jv");
    IEMOP_HLP_MIN_386();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pVCpu->iem.s.enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_OF) {
            IEM_MC_REL_JMP_S16(i16Imm);
        } IEM_MC_ELSE() {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    else
    {
        int32_t i32Imm; IEM_OPCODE_GET_NEXT_S32(&i32Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_OF) {
            IEM_MC_REL_JMP_S32(i32Imm);
        } IEM_MC_ELSE() {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x81. */
FNIEMOP_DEF(iemOp_jno_Jv)
{
    IEMOP_MNEMONIC("jno Jv");
    IEMOP_HLP_MIN_386();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pVCpu->iem.s.enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_OF) {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ELSE() {
            IEM_MC_REL_JMP_S16(i16Imm);
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    else
    {
        int32_t i32Imm; IEM_OPCODE_GET_NEXT_S32(&i32Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_OF) {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ELSE() {
            IEM_MC_REL_JMP_S32(i32Imm);
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x82. */
FNIEMOP_DEF(iemOp_jc_Jv)
{
    IEMOP_MNEMONIC("jc/jb/jnae Jv");
    IEMOP_HLP_MIN_386();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pVCpu->iem.s.enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_CF) {
            IEM_MC_REL_JMP_S16(i16Imm);
        } IEM_MC_ELSE() {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    else
    {
        int32_t i32Imm; IEM_OPCODE_GET_NEXT_S32(&i32Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_CF) {
            IEM_MC_REL_JMP_S32(i32Imm);
        } IEM_MC_ELSE() {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x83. */
FNIEMOP_DEF(iemOp_jnc_Jv)
{
    IEMOP_MNEMONIC("jnc/jnb/jae Jv");
    IEMOP_HLP_MIN_386();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pVCpu->iem.s.enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_CF) {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ELSE() {
            IEM_MC_REL_JMP_S16(i16Imm);
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    else
    {
        int32_t i32Imm; IEM_OPCODE_GET_NEXT_S32(&i32Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_CF) {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ELSE() {
            IEM_MC_REL_JMP_S32(i32Imm);
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x84. */
FNIEMOP_DEF(iemOp_je_Jv)
{
    IEMOP_MNEMONIC("je/jz Jv");
    IEMOP_HLP_MIN_386();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pVCpu->iem.s.enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_ZF) {
            IEM_MC_REL_JMP_S16(i16Imm);
        } IEM_MC_ELSE() {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    else
    {
        int32_t i32Imm; IEM_OPCODE_GET_NEXT_S32(&i32Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_ZF) {
            IEM_MC_REL_JMP_S32(i32Imm);
        } IEM_MC_ELSE() {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x85. */
FNIEMOP_DEF(iemOp_jne_Jv)
{
    IEMOP_MNEMONIC("jne/jnz Jv");
    IEMOP_HLP_MIN_386();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pVCpu->iem.s.enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_ZF) {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ELSE() {
            IEM_MC_REL_JMP_S16(i16Imm);
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    else
    {
        int32_t i32Imm; IEM_OPCODE_GET_NEXT_S32(&i32Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_ZF) {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ELSE() {
            IEM_MC_REL_JMP_S32(i32Imm);
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x86. */
FNIEMOP_DEF(iemOp_jbe_Jv)
{
    IEMOP_MNEMONIC("jbe/jna Jv");
    IEMOP_HLP_MIN_386();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pVCpu->iem.s.enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_ANY_BITS_SET(X86_EFL_CF | X86_EFL_ZF) {
            IEM_MC_REL_JMP_S16(i16Imm);
        } IEM_MC_ELSE() {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    else
    {
        int32_t i32Imm; IEM_OPCODE_GET_NEXT_S32(&i32Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_ANY_BITS_SET(X86_EFL_CF | X86_EFL_ZF) {
            IEM_MC_REL_JMP_S32(i32Imm);
        } IEM_MC_ELSE() {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x87. */
FNIEMOP_DEF(iemOp_jnbe_Jv)
{
    IEMOP_MNEMONIC("jnbe/ja Jv");
    IEMOP_HLP_MIN_386();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pVCpu->iem.s.enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_ANY_BITS_SET(X86_EFL_CF | X86_EFL_ZF) {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ELSE() {
            IEM_MC_REL_JMP_S16(i16Imm);
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    else
    {
        int32_t i32Imm; IEM_OPCODE_GET_NEXT_S32(&i32Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_ANY_BITS_SET(X86_EFL_CF | X86_EFL_ZF) {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ELSE() {
            IEM_MC_REL_JMP_S32(i32Imm);
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x88. */
FNIEMOP_DEF(iemOp_js_Jv)
{
    IEMOP_MNEMONIC("js  Jv");
    IEMOP_HLP_MIN_386();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pVCpu->iem.s.enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_SF) {
            IEM_MC_REL_JMP_S16(i16Imm);
        } IEM_MC_ELSE() {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    else
    {
        int32_t i32Imm; IEM_OPCODE_GET_NEXT_S32(&i32Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_SF) {
            IEM_MC_REL_JMP_S32(i32Imm);
        } IEM_MC_ELSE() {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x89. */
FNIEMOP_DEF(iemOp_jns_Jv)
{
    IEMOP_MNEMONIC("jns Jv");
    IEMOP_HLP_MIN_386();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pVCpu->iem.s.enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_SF) {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ELSE() {
            IEM_MC_REL_JMP_S16(i16Imm);
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    else
    {
        int32_t i32Imm; IEM_OPCODE_GET_NEXT_S32(&i32Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_SF) {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ELSE() {
            IEM_MC_REL_JMP_S32(i32Imm);
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x8a. */
FNIEMOP_DEF(iemOp_jp_Jv)
{
    IEMOP_MNEMONIC("jp  Jv");
    IEMOP_HLP_MIN_386();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pVCpu->iem.s.enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_PF) {
            IEM_MC_REL_JMP_S16(i16Imm);
        } IEM_MC_ELSE() {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    else
    {
        int32_t i32Imm; IEM_OPCODE_GET_NEXT_S32(&i32Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_PF) {
            IEM_MC_REL_JMP_S32(i32Imm);
        } IEM_MC_ELSE() {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x8b. */
FNIEMOP_DEF(iemOp_jnp_Jv)
{
    IEMOP_MNEMONIC("jo  Jv");
    IEMOP_HLP_MIN_386();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pVCpu->iem.s.enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_PF) {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ELSE() {
            IEM_MC_REL_JMP_S16(i16Imm);
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    else
    {
        int32_t i32Imm; IEM_OPCODE_GET_NEXT_S32(&i32Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_PF) {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ELSE() {
            IEM_MC_REL_JMP_S32(i32Imm);
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x8c. */
FNIEMOP_DEF(iemOp_jl_Jv)
{
    IEMOP_MNEMONIC("jl/jnge Jv");
    IEMOP_HLP_MIN_386();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pVCpu->iem.s.enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BITS_NE(X86_EFL_SF, X86_EFL_OF) {
            IEM_MC_REL_JMP_S16(i16Imm);
        } IEM_MC_ELSE() {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    else
    {
        int32_t i32Imm; IEM_OPCODE_GET_NEXT_S32(&i32Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BITS_NE(X86_EFL_SF, X86_EFL_OF) {
            IEM_MC_REL_JMP_S32(i32Imm);
        } IEM_MC_ELSE() {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x8d. */
FNIEMOP_DEF(iemOp_jnl_Jv)
{
    IEMOP_MNEMONIC("jnl/jge Jv");
    IEMOP_HLP_MIN_386();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pVCpu->iem.s.enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BITS_NE(X86_EFL_SF, X86_EFL_OF) {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ELSE() {
            IEM_MC_REL_JMP_S16(i16Imm);
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    else
    {
        int32_t i32Imm; IEM_OPCODE_GET_NEXT_S32(&i32Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BITS_NE(X86_EFL_SF, X86_EFL_OF) {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ELSE() {
            IEM_MC_REL_JMP_S32(i32Imm);
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x8e. */
FNIEMOP_DEF(iemOp_jle_Jv)
{
    IEMOP_MNEMONIC("jle/jng Jv");
    IEMOP_HLP_MIN_386();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pVCpu->iem.s.enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET_OR_BITS_NE(X86_EFL_ZF, X86_EFL_SF, X86_EFL_OF) {
            IEM_MC_REL_JMP_S16(i16Imm);
        } IEM_MC_ELSE() {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    else
    {
        int32_t i32Imm; IEM_OPCODE_GET_NEXT_S32(&i32Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET_OR_BITS_NE(X86_EFL_ZF, X86_EFL_SF, X86_EFL_OF) {
            IEM_MC_REL_JMP_S32(i32Imm);
        } IEM_MC_ELSE() {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x8f. */
FNIEMOP_DEF(iemOp_jnle_Jv)
{
    IEMOP_MNEMONIC("jnle/jg Jv");
    IEMOP_HLP_MIN_386();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    if (pVCpu->iem.s.enmEffOpSize == IEMMODE_16BIT)
    {
        int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET_OR_BITS_NE(X86_EFL_ZF, X86_EFL_SF, X86_EFL_OF) {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ELSE() {
            IEM_MC_REL_JMP_S16(i16Imm);
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    else
    {
        int32_t i32Imm; IEM_OPCODE_GET_NEXT_S32(&i32Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET_OR_BITS_NE(X86_EFL_ZF, X86_EFL_SF, X86_EFL_OF) {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ELSE() {
            IEM_MC_REL_JMP_S32(i32Imm);
        } IEM_MC_ENDIF();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x90. */
FNIEMOP_DEF(iemOp_seto_Eb)
{
    IEMOP_MNEMONIC("seto Eb");
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_OF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 0);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_OF) {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x91. */
FNIEMOP_DEF(iemOp_setno_Eb)
{
    IEMOP_MNEMONIC("setno Eb");
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_OF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 1);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_OF) {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x92. */
FNIEMOP_DEF(iemOp_setc_Eb)
{
    IEMOP_MNEMONIC("setc Eb");
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_CF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 0);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_CF) {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x93. */
FNIEMOP_DEF(iemOp_setnc_Eb)
{
    IEMOP_MNEMONIC("setnc Eb");
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_CF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 1);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_CF) {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x94. */
FNIEMOP_DEF(iemOp_sete_Eb)
{
    IEMOP_MNEMONIC("sete Eb");
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_ZF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 0);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_ZF) {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x95. */
FNIEMOP_DEF(iemOp_setne_Eb)
{
    IEMOP_MNEMONIC("setne Eb");
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_ZF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 1);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_ZF) {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x96. */
FNIEMOP_DEF(iemOp_setbe_Eb)
{
    IEMOP_MNEMONIC("setbe Eb");
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_ANY_BITS_SET(X86_EFL_CF | X86_EFL_ZF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 0);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_IF_EFL_ANY_BITS_SET(X86_EFL_CF | X86_EFL_ZF) {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x97. */
FNIEMOP_DEF(iemOp_setnbe_Eb)
{
    IEMOP_MNEMONIC("setnbe Eb");
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_ANY_BITS_SET(X86_EFL_CF | X86_EFL_ZF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 1);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_IF_EFL_ANY_BITS_SET(X86_EFL_CF | X86_EFL_ZF) {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x98. */
FNIEMOP_DEF(iemOp_sets_Eb)
{
    IEMOP_MNEMONIC("sets Eb");
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_SF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 0);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_SF) {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x99. */
FNIEMOP_DEF(iemOp_setns_Eb)
{
    IEMOP_MNEMONIC("setns Eb");
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_SF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 1);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_SF) {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x9a. */
FNIEMOP_DEF(iemOp_setp_Eb)
{
    IEMOP_MNEMONIC("setnp Eb");
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_PF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 0);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_PF) {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x9b. */
FNIEMOP_DEF(iemOp_setnp_Eb)
{
    IEMOP_MNEMONIC("setnp Eb");
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_PF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 1);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_PF) {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x9c. */
FNIEMOP_DEF(iemOp_setl_Eb)
{
    IEMOP_MNEMONIC("setl Eb");
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BITS_NE(X86_EFL_SF, X86_EFL_OF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 0);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_IF_EFL_BITS_NE(X86_EFL_SF, X86_EFL_OF) {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x9d. */
FNIEMOP_DEF(iemOp_setnl_Eb)
{
    IEMOP_MNEMONIC("setnl Eb");
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BITS_NE(X86_EFL_SF, X86_EFL_OF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 1);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_IF_EFL_BITS_NE(X86_EFL_SF, X86_EFL_OF) {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x9e. */
FNIEMOP_DEF(iemOp_setle_Eb)
{
    IEMOP_MNEMONIC("setle Eb");
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET_OR_BITS_NE(X86_EFL_ZF, X86_EFL_SF, X86_EFL_OF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 0);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_IF_EFL_BIT_SET_OR_BITS_NE(X86_EFL_ZF, X86_EFL_SF, X86_EFL_OF) {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0x9f. */
FNIEMOP_DEF(iemOp_setnle_Eb)
{
    IEMOP_MNEMONIC("setnle Eb");
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /** @todo Encoding test: Check if the 'reg' field is ignored or decoded in
     *        any way. AMD says it's "unused", whatever that means.  We're
     *        ignoring for now. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(0, 0);
        IEM_MC_IF_EFL_BIT_SET_OR_BITS_NE(X86_EFL_ZF, X86_EFL_SF, X86_EFL_OF) {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_GREG_U8_CONST((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, 1);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_IF_EFL_BIT_SET_OR_BITS_NE(X86_EFL_ZF, X86_EFL_SF, X86_EFL_OF) {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
        } IEM_MC_ELSE() {
            IEM_MC_STORE_MEM_U8_CONST(pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1);
        } IEM_MC_ENDIF();
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/**
 * Common 'push segment-register' helper.
 */
FNIEMOP_DEF_1(iemOpCommonPushSReg, uint8_t, iReg)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    if (iReg < X86_SREG_FS)
        IEMOP_HLP_NO_64BIT();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint16_t, u16Value);
            IEM_MC_FETCH_SREG_U16(u16Value, iReg);
            IEM_MC_PUSH_U16(u16Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            break;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint32_t, u32Value);
            IEM_MC_FETCH_SREG_ZX_U32(u32Value, iReg);
            IEM_MC_PUSH_U32_SREG(u32Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            break;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint64_t, u64Value);
            IEM_MC_FETCH_SREG_ZX_U64(u64Value, iReg);
            IEM_MC_PUSH_U64(u64Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            break;
    }

    return VINF_SUCCESS;
}


/** Opcode 0x0f 0xa0. */
FNIEMOP_DEF(iemOp_push_fs)
{
    IEMOP_MNEMONIC("push fs");
    IEMOP_HLP_MIN_386();
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return FNIEMOP_CALL_1(iemOpCommonPushSReg, X86_SREG_FS);
}


/** Opcode 0x0f 0xa1. */
FNIEMOP_DEF(iemOp_pop_fs)
{
    IEMOP_MNEMONIC("pop fs");
    IEMOP_HLP_MIN_386();
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_pop_Sreg, X86_SREG_FS, pVCpu->iem.s.enmEffOpSize);
}


/** Opcode 0x0f 0xa2. */
FNIEMOP_DEF(iemOp_cpuid)
{
    IEMOP_MNEMONIC("cpuid");
    IEMOP_HLP_MIN_486(); /* not all 486es. */
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_cpuid);
}


/**
 * Common worker for iemOp_bt_Ev_Gv, iemOp_btc_Ev_Gv, iemOp_btr_Ev_Gv and
 * iemOp_bts_Ev_Gv.
 */
FNIEMOP_DEF_1(iemOpCommonBit_Ev_Gv, PCIEMOPBINSIZES, pImpl)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register destination. */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint16_t *,      pu16Dst,                0);
                IEM_MC_ARG(uint16_t,        u16Src,                 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                2);

                IEM_MC_FETCH_GREG_U16(u16Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_AND_LOCAL_U16(u16Src, 0xf);
                IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU16, pu16Dst, u16Src, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint32_t *,      pu32Dst,                0);
                IEM_MC_ARG(uint32_t,        u32Src,                 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                2);

                IEM_MC_FETCH_GREG_U32(u32Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_AND_LOCAL_U32(u32Src, 0x1f);
                IEM_MC_REF_GREG_U32(pu32Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, u32Src, pEFlags);

                IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32Dst);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint64_t *,      pu64Dst,                0);
                IEM_MC_ARG(uint64_t,        u64Src,                 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                2);

                IEM_MC_FETCH_GREG_U64(u64Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_AND_LOCAL_U64(u64Src, 0x3f);
                IEM_MC_REF_GREG_U64(pu64Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU64, pu64Dst, u64Src, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        /* memory destination. */

        uint32_t fAccess;
        if (pImpl->pfnLockedU16)
            fAccess = IEM_ACCESS_DATA_RW;
        else /* BT */
            fAccess = IEM_ACCESS_DATA_R;

        NOREF(fAccess);

        /** @todo test negative bit offsets! */
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint16_t *,              pu16Dst,                0);
                IEM_MC_ARG(uint16_t,                u16Src,                 1);
                IEM_MC_ARG_LOCAL_EFLAGS(            pEFlags, EFlags,        2);
                IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);
                IEM_MC_LOCAL(int16_t,               i16AddrAdj);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                if (pImpl->pfnLockedU16)
                    IEMOP_HLP_DONE_DECODING();
                else
                    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_GREG_U16(u16Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_ASSIGN(i16AddrAdj, u16Src);
                IEM_MC_AND_ARG_U16(u16Src, 0x0f);
                IEM_MC_SAR_LOCAL_S16(i16AddrAdj, 4);
                IEM_MC_SAR_LOCAL_S16(i16AddrAdj, 1);
                IEM_MC_ADD_LOCAL_S16_TO_EFF_ADDR(GCPtrEffDst, i16AddrAdj);
                IEM_MC_FETCH_EFLAGS(EFlags);

                IEM_MC_MEM_MAP(pu16Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU16, pu16Dst, u16Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnLockedU16, pu16Dst, u16Src, pEFlags);
                IEM_MC_MEM_COMMIT_AND_UNMAP(pu16Dst, IEM_ACCESS_DATA_RW);

                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint32_t *,              pu32Dst,                0);
                IEM_MC_ARG(uint32_t,                u32Src,                 1);
                IEM_MC_ARG_LOCAL_EFLAGS(            pEFlags, EFlags,        2);
                IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);
                IEM_MC_LOCAL(int32_t,               i32AddrAdj);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                if (pImpl->pfnLockedU16)
                    IEMOP_HLP_DONE_DECODING();
                else
                    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_GREG_U32(u32Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_ASSIGN(i32AddrAdj, u32Src);
                IEM_MC_AND_ARG_U32(u32Src, 0x1f);
                IEM_MC_SAR_LOCAL_S32(i32AddrAdj, 5);
                IEM_MC_SHL_LOCAL_S32(i32AddrAdj, 2);
                IEM_MC_ADD_LOCAL_S32_TO_EFF_ADDR(GCPtrEffDst, i32AddrAdj);
                IEM_MC_FETCH_EFLAGS(EFlags);

                IEM_MC_MEM_MAP(pu32Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, u32Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnLockedU32, pu32Dst, u32Src, pEFlags);
                IEM_MC_MEM_COMMIT_AND_UNMAP(pu32Dst, IEM_ACCESS_DATA_RW);

                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint64_t *,              pu64Dst,                0);
                IEM_MC_ARG(uint64_t,                u64Src,                 1);
                IEM_MC_ARG_LOCAL_EFLAGS(            pEFlags, EFlags,        2);
                IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);
                IEM_MC_LOCAL(int64_t,               i64AddrAdj);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                if (pImpl->pfnLockedU16)
                    IEMOP_HLP_DONE_DECODING();
                else
                    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_GREG_U64(u64Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_ASSIGN(i64AddrAdj, u64Src);
                IEM_MC_AND_ARG_U64(u64Src, 0x3f);
                IEM_MC_SAR_LOCAL_S64(i64AddrAdj, 6);
                IEM_MC_SHL_LOCAL_S64(i64AddrAdj, 3);
                IEM_MC_ADD_LOCAL_S64_TO_EFF_ADDR(GCPtrEffDst, i64AddrAdj);
                IEM_MC_FETCH_EFLAGS(EFlags);

                IEM_MC_MEM_MAP(pu64Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU64, pu64Dst, u64Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnLockedU64, pu64Dst, u64Src, pEFlags);
                IEM_MC_MEM_COMMIT_AND_UNMAP(pu64Dst, IEM_ACCESS_DATA_RW);

                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


/** Opcode 0x0f 0xa3. */
FNIEMOP_DEF(iemOp_bt_Ev_Gv)
{
    IEMOP_MNEMONIC("bt  Gv,Gv");
    IEMOP_HLP_MIN_386();
    return FNIEMOP_CALL_1(iemOpCommonBit_Ev_Gv, &g_iemAImpl_bt);
}


/**
 * Common worker for iemOp_shrd_Ev_Gv_Ib and iemOp_shld_Ev_Gv_Ib.
 */
FNIEMOP_DEF_1(iemOpCommonShldShrd_Ib, PCIEMOPSHIFTDBLSIZES, pImpl)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF | X86_EFL_OF);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        uint8_t cShift; IEM_OPCODE_GET_NEXT_U8(&cShift);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(4, 0);
                IEM_MC_ARG(uint16_t *,      pu16Dst,                0);
                IEM_MC_ARG(uint16_t,        u16Src,                 1);
                IEM_MC_ARG_CONST(uint8_t,   cShiftArg, /*=*/cShift, 2);
                IEM_MC_ARG(uint32_t *,      pEFlags,                3);

                IEM_MC_FETCH_GREG_U16(u16Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_4(pImpl->pfnNormalU16, pu16Dst, u16Src, cShiftArg, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(4, 0);
                IEM_MC_ARG(uint32_t *,      pu32Dst,                0);
                IEM_MC_ARG(uint32_t,        u32Src,                 1);
                IEM_MC_ARG_CONST(uint8_t,   cShiftArg, /*=*/cShift, 2);
                IEM_MC_ARG(uint32_t *,      pEFlags,                3);

                IEM_MC_FETCH_GREG_U32(u32Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_GREG_U32(pu32Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_4(pImpl->pfnNormalU32, pu32Dst, u32Src, cShiftArg, pEFlags);

                IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32Dst);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(4, 0);
                IEM_MC_ARG(uint64_t *,      pu64Dst,                0);
                IEM_MC_ARG(uint64_t,        u64Src,                 1);
                IEM_MC_ARG_CONST(uint8_t,   cShiftArg, /*=*/cShift, 2);
                IEM_MC_ARG(uint32_t *,      pEFlags,                3);

                IEM_MC_FETCH_GREG_U64(u64Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_GREG_U64(pu64Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_4(pImpl->pfnNormalU64, pu64Dst, u64Src, cShiftArg, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(4, 2);
                IEM_MC_ARG(uint16_t *,              pu16Dst,                0);
                IEM_MC_ARG(uint16_t,                u16Src,                 1);
                IEM_MC_ARG(uint8_t,                 cShiftArg,              2);
                IEM_MC_ARG_LOCAL_EFLAGS(            pEFlags, EFlags,        3);
                IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 1);
                uint8_t cShift; IEM_OPCODE_GET_NEXT_U8(&cShift);
                IEM_MC_ASSIGN(cShiftArg, cShift);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_GREG_U16(u16Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_MEM_MAP(pu16Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
                IEM_MC_CALL_VOID_AIMPL_4(pImpl->pfnNormalU16, pu16Dst, u16Src, cShiftArg, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu16Dst, IEM_ACCESS_DATA_RW);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(4, 2);
                IEM_MC_ARG(uint32_t *,              pu32Dst,                0);
                IEM_MC_ARG(uint32_t,                u32Src,                 1);
                IEM_MC_ARG(uint8_t,                 cShiftArg,              2);
                IEM_MC_ARG_LOCAL_EFLAGS(            pEFlags, EFlags,        3);
                IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 1);
                uint8_t cShift; IEM_OPCODE_GET_NEXT_U8(&cShift);
                IEM_MC_ASSIGN(cShiftArg, cShift);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_GREG_U32(u32Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_MEM_MAP(pu32Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
                IEM_MC_CALL_VOID_AIMPL_4(pImpl->pfnNormalU32, pu32Dst, u32Src, cShiftArg, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu32Dst, IEM_ACCESS_DATA_RW);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(4, 2);
                IEM_MC_ARG(uint64_t *,              pu64Dst,                0);
                IEM_MC_ARG(uint64_t,                u64Src,                 1);
                IEM_MC_ARG(uint8_t,                 cShiftArg,              2);
                IEM_MC_ARG_LOCAL_EFLAGS(            pEFlags, EFlags,        3);
                IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 1);
                uint8_t cShift; IEM_OPCODE_GET_NEXT_U8(&cShift);
                IEM_MC_ASSIGN(cShiftArg, cShift);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_GREG_U64(u64Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_MEM_MAP(pu64Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
                IEM_MC_CALL_VOID_AIMPL_4(pImpl->pfnNormalU64, pu64Dst, u64Src, cShiftArg, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu64Dst, IEM_ACCESS_DATA_RW);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


/**
 * Common worker for iemOp_shrd_Ev_Gv_CL and iemOp_shld_Ev_Gv_CL.
 */
FNIEMOP_DEF_1(iemOpCommonShldShrd_CL, PCIEMOPSHIFTDBLSIZES, pImpl)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF | X86_EFL_OF);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(4, 0);
                IEM_MC_ARG(uint16_t *,      pu16Dst,                0);
                IEM_MC_ARG(uint16_t,        u16Src,                 1);
                IEM_MC_ARG(uint8_t,         cShiftArg,              2);
                IEM_MC_ARG(uint32_t *,      pEFlags,                3);

                IEM_MC_FETCH_GREG_U16(u16Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_FETCH_GREG_U8(cShiftArg, X86_GREG_xCX);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_4(pImpl->pfnNormalU16, pu16Dst, u16Src, cShiftArg, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(4, 0);
                IEM_MC_ARG(uint32_t *,      pu32Dst,                0);
                IEM_MC_ARG(uint32_t,        u32Src,                 1);
                IEM_MC_ARG(uint8_t,         cShiftArg,              2);
                IEM_MC_ARG(uint32_t *,      pEFlags,                3);

                IEM_MC_FETCH_GREG_U32(u32Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_GREG_U32(pu32Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_FETCH_GREG_U8(cShiftArg, X86_GREG_xCX);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_4(pImpl->pfnNormalU32, pu32Dst, u32Src, cShiftArg, pEFlags);

                IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32Dst);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(4, 0);
                IEM_MC_ARG(uint64_t *,      pu64Dst,                0);
                IEM_MC_ARG(uint64_t,        u64Src,                 1);
                IEM_MC_ARG(uint8_t,         cShiftArg,              2);
                IEM_MC_ARG(uint32_t *,      pEFlags,                3);

                IEM_MC_FETCH_GREG_U64(u64Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_GREG_U64(pu64Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_FETCH_GREG_U8(cShiftArg, X86_GREG_xCX);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_4(pImpl->pfnNormalU64, pu64Dst, u64Src, cShiftArg, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(4, 2);
                IEM_MC_ARG(uint16_t *,              pu16Dst,                0);
                IEM_MC_ARG(uint16_t,                u16Src,                 1);
                IEM_MC_ARG(uint8_t,                 cShiftArg,              2);
                IEM_MC_ARG_LOCAL_EFLAGS(            pEFlags, EFlags,        3);
                IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_GREG_U16(u16Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_FETCH_GREG_U8(cShiftArg, X86_GREG_xCX);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_MEM_MAP(pu16Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
                IEM_MC_CALL_VOID_AIMPL_4(pImpl->pfnNormalU16, pu16Dst, u16Src, cShiftArg, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu16Dst, IEM_ACCESS_DATA_RW);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(4, 2);
                IEM_MC_ARG(uint32_t *,              pu32Dst,                0);
                IEM_MC_ARG(uint32_t,                u32Src,                 1);
                IEM_MC_ARG(uint8_t,                 cShiftArg,              2);
                IEM_MC_ARG_LOCAL_EFLAGS(            pEFlags, EFlags,        3);
                IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_GREG_U32(u32Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_FETCH_GREG_U8(cShiftArg, X86_GREG_xCX);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_MEM_MAP(pu32Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
                IEM_MC_CALL_VOID_AIMPL_4(pImpl->pfnNormalU32, pu32Dst, u32Src, cShiftArg, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu32Dst, IEM_ACCESS_DATA_RW);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(4, 2);
                IEM_MC_ARG(uint64_t *,              pu64Dst,                0);
                IEM_MC_ARG(uint64_t,                u64Src,                 1);
                IEM_MC_ARG(uint8_t,                 cShiftArg,              2);
                IEM_MC_ARG_LOCAL_EFLAGS(            pEFlags, EFlags,        3);
                IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_GREG_U64(u64Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_FETCH_GREG_U8(cShiftArg, X86_GREG_xCX);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_MEM_MAP(pu64Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
                IEM_MC_CALL_VOID_AIMPL_4(pImpl->pfnNormalU64, pu64Dst, u64Src, cShiftArg, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu64Dst, IEM_ACCESS_DATA_RW);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}



/** Opcode 0x0f 0xa4. */
FNIEMOP_DEF(iemOp_shld_Ev_Gv_Ib)
{
    IEMOP_MNEMONIC("shld Ev,Gv,Ib");
    IEMOP_HLP_MIN_386();
    return FNIEMOP_CALL_1(iemOpCommonShldShrd_Ib, &g_iemAImpl_shld);
}


/** Opcode 0x0f 0xa5. */
FNIEMOP_DEF(iemOp_shld_Ev_Gv_CL)
{
    IEMOP_MNEMONIC("shld Ev,Gv,CL");
    IEMOP_HLP_MIN_386();
    return FNIEMOP_CALL_1(iemOpCommonShldShrd_CL, &g_iemAImpl_shld);
}


/** Opcode 0x0f 0xa8. */
FNIEMOP_DEF(iemOp_push_gs)
{
    IEMOP_MNEMONIC("push gs");
    IEMOP_HLP_MIN_386();
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return FNIEMOP_CALL_1(iemOpCommonPushSReg, X86_SREG_GS);
}


/** Opcode 0x0f 0xa9. */
FNIEMOP_DEF(iemOp_pop_gs)
{
    IEMOP_MNEMONIC("pop gs");
    IEMOP_HLP_MIN_386();
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_pop_Sreg, X86_SREG_GS, pVCpu->iem.s.enmEffOpSize);
}


/** Opcode 0x0f 0xaa. */
FNIEMOP_STUB(iemOp_rsm);
//IEMOP_HLP_MIN_386();


/** Opcode 0x0f 0xab. */
FNIEMOP_DEF(iemOp_bts_Ev_Gv)
{
    IEMOP_MNEMONIC("bts Ev,Gv");
    IEMOP_HLP_MIN_386();
    return FNIEMOP_CALL_1(iemOpCommonBit_Ev_Gv, &g_iemAImpl_bts);
}


/** Opcode 0x0f 0xac. */
FNIEMOP_DEF(iemOp_shrd_Ev_Gv_Ib)
{
    IEMOP_MNEMONIC("shrd Ev,Gv,Ib");
    IEMOP_HLP_MIN_386();
    return FNIEMOP_CALL_1(iemOpCommonShldShrd_Ib, &g_iemAImpl_shrd);
}


/** Opcode 0x0f 0xad. */
FNIEMOP_DEF(iemOp_shrd_Ev_Gv_CL)
{
    IEMOP_MNEMONIC("shrd Ev,Gv,CL");
    IEMOP_HLP_MIN_386();
    return FNIEMOP_CALL_1(iemOpCommonShldShrd_CL, &g_iemAImpl_shrd);
}


/** Opcode 0x0f 0xae mem/0. */
FNIEMOP_DEF_1(iemOp_Grp15_fxsave,   uint8_t, bRm)
{
    IEMOP_MNEMONIC("fxsave m512");
    if (!IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fFxSaveRstor)
        return IEMOP_RAISE_INVALID_OPCODE();

    IEM_MC_BEGIN(3, 1);
    IEM_MC_ARG(uint8_t,         iEffSeg,                                 0);
    IEM_MC_ARG(RTGCPTR,         GCPtrEff,                                1);
    IEM_MC_ARG_CONST(IEMMODE,   enmEffOpSize,/*=*/pVCpu->iem.s.enmEffOpSize, 2);
    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEff, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_ASSIGN(iEffSeg, pVCpu->iem.s.iEffSeg);
    IEM_MC_CALL_CIMPL_3(iemCImpl_fxsave, iEffSeg, GCPtrEff, enmEffOpSize);
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0xae mem/1. */
FNIEMOP_DEF_1(iemOp_Grp15_fxrstor,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fxrstor m512");
    if (!IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fFxSaveRstor)
        return IEMOP_RAISE_INVALID_OPCODE();

    IEM_MC_BEGIN(3, 1);
    IEM_MC_ARG(uint8_t,         iEffSeg,                                 0);
    IEM_MC_ARG(RTGCPTR,         GCPtrEff,                                1);
    IEM_MC_ARG_CONST(IEMMODE,   enmEffOpSize,/*=*/pVCpu->iem.s.enmEffOpSize, 2);
    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEff, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_ASSIGN(iEffSeg, pVCpu->iem.s.iEffSeg);
    IEM_MC_CALL_CIMPL_3(iemCImpl_fxrstor, iEffSeg, GCPtrEff, enmEffOpSize);
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0xae mem/2. */
FNIEMOP_STUB_1(iemOp_Grp15_ldmxcsr,  uint8_t, bRm);

/** Opcode 0x0f 0xae mem/3. */
FNIEMOP_STUB_1(iemOp_Grp15_stmxcsr,  uint8_t, bRm);

/** Opcode 0x0f 0xae mem/4. */
FNIEMOP_UD_STUB_1(iemOp_Grp15_xsave,    uint8_t, bRm);

/** Opcode 0x0f 0xae mem/5. */
FNIEMOP_UD_STUB_1(iemOp_Grp15_xrstor,   uint8_t, bRm);

/** Opcode 0x0f 0xae mem/6. */
FNIEMOP_UD_STUB_1(iemOp_Grp15_xsaveopt, uint8_t, bRm);

/** Opcode 0x0f 0xae mem/7. */
FNIEMOP_STUB_1(iemOp_Grp15_clflush,  uint8_t, bRm);


/** Opcode 0x0f 0xae 11b/5. */
FNIEMOP_DEF_1(iemOp_Grp15_lfence,   uint8_t, bRm)
{
    IEMOP_MNEMONIC("lfence");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    if (!IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fSse2)
        return IEMOP_RAISE_INVALID_OPCODE();

    IEM_MC_BEGIN(0, 0);
    if (IEM_GET_HOST_CPU_FEATURES(pVCpu)->fSse2)
        IEM_MC_CALL_VOID_AIMPL_0(iemAImpl_lfence);
    else
        IEM_MC_CALL_VOID_AIMPL_0(iemAImpl_alt_mem_fence);
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0xae 11b/6. */
FNIEMOP_DEF_1(iemOp_Grp15_mfence,   uint8_t, bRm)
{
    IEMOP_MNEMONIC("mfence");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    if (!IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fSse2)
        return IEMOP_RAISE_INVALID_OPCODE();

    IEM_MC_BEGIN(0, 0);
    if (IEM_GET_HOST_CPU_FEATURES(pVCpu)->fSse2)
        IEM_MC_CALL_VOID_AIMPL_0(iemAImpl_mfence);
    else
        IEM_MC_CALL_VOID_AIMPL_0(iemAImpl_alt_mem_fence);
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0xae 11b/7. */
FNIEMOP_DEF_1(iemOp_Grp15_sfence,   uint8_t, bRm)
{
    IEMOP_MNEMONIC("sfence");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    if (!IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fSse2)
        return IEMOP_RAISE_INVALID_OPCODE();

    IEM_MC_BEGIN(0, 0);
    if (IEM_GET_HOST_CPU_FEATURES(pVCpu)->fSse2)
        IEM_MC_CALL_VOID_AIMPL_0(iemAImpl_sfence);
    else
        IEM_MC_CALL_VOID_AIMPL_0(iemAImpl_alt_mem_fence);
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xf3 0x0f 0xae 11b/0. */
FNIEMOP_UD_STUB_1(iemOp_Grp15_rdfsbase, uint8_t, bRm);

/** Opcode 0xf3 0x0f 0xae 11b/1. */
FNIEMOP_UD_STUB_1(iemOp_Grp15_rdgsbase, uint8_t, bRm);

/** Opcode 0xf3 0x0f 0xae 11b/2. */
FNIEMOP_UD_STUB_1(iemOp_Grp15_wrfsbase, uint8_t, bRm);

/** Opcode 0xf3 0x0f 0xae 11b/3. */
FNIEMOP_UD_STUB_1(iemOp_Grp15_wrgsbase, uint8_t, bRm);


/** Opcode 0x0f 0xae. */
FNIEMOP_DEF(iemOp_Grp15)
{
    IEMOP_HLP_MIN_586(); /* Not entirely accurate nor needed, but useful for debugging 286 code. */
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT))
    {
        switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
        {
            case 0: return FNIEMOP_CALL_1(iemOp_Grp15_fxsave,  bRm);
            case 1: return FNIEMOP_CALL_1(iemOp_Grp15_fxrstor, bRm);
            case 2: return FNIEMOP_CALL_1(iemOp_Grp15_ldmxcsr, bRm);
            case 3: return FNIEMOP_CALL_1(iemOp_Grp15_stmxcsr, bRm);
            case 4: return FNIEMOP_CALL_1(iemOp_Grp15_xsave,   bRm);
            case 5: return FNIEMOP_CALL_1(iemOp_Grp15_xrstor,  bRm);
            case 6: return FNIEMOP_CALL_1(iemOp_Grp15_xsaveopt,bRm);
            case 7: return FNIEMOP_CALL_1(iemOp_Grp15_clflush, bRm);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_REPZ | IEM_OP_PRF_REPNZ | IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_LOCK))
        {
            case 0:
                switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
                {
                    case 0: return IEMOP_RAISE_INVALID_OPCODE();
                    case 1: return IEMOP_RAISE_INVALID_OPCODE();
                    case 2: return IEMOP_RAISE_INVALID_OPCODE();
                    case 3: return IEMOP_RAISE_INVALID_OPCODE();
                    case 4: return IEMOP_RAISE_INVALID_OPCODE();
                    case 5: return FNIEMOP_CALL_1(iemOp_Grp15_lfence, bRm);
                    case 6: return FNIEMOP_CALL_1(iemOp_Grp15_mfence, bRm);
                    case 7: return FNIEMOP_CALL_1(iemOp_Grp15_sfence, bRm);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;

            case IEM_OP_PRF_REPZ:
                switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
                {
                    case 0: return FNIEMOP_CALL_1(iemOp_Grp15_rdfsbase, bRm);
                    case 1: return FNIEMOP_CALL_1(iemOp_Grp15_rdgsbase, bRm);
                    case 2: return FNIEMOP_CALL_1(iemOp_Grp15_wrfsbase, bRm);
                    case 3: return FNIEMOP_CALL_1(iemOp_Grp15_wrgsbase, bRm);
                    case 4: return IEMOP_RAISE_INVALID_OPCODE();
                    case 5: return IEMOP_RAISE_INVALID_OPCODE();
                    case 6: return IEMOP_RAISE_INVALID_OPCODE();
                    case 7: return IEMOP_RAISE_INVALID_OPCODE();
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;

            default:
                return IEMOP_RAISE_INVALID_OPCODE();
        }
    }
}


/** Opcode 0x0f 0xaf. */
FNIEMOP_DEF(iemOp_imul_Gv_Ev)
{
    IEMOP_MNEMONIC("imul Gv,Ev");
    IEMOP_HLP_MIN_386();
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rv_rm, &g_iemAImpl_imul_two);
}


/** Opcode 0x0f 0xb0. */
FNIEMOP_DEF(iemOp_cmpxchg_Eb_Gb)
{
    IEMOP_MNEMONIC("cmpxchg Eb,Gb");
    IEMOP_HLP_MIN_486();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING();
        IEM_MC_BEGIN(4, 0);
        IEM_MC_ARG(uint8_t *,       pu8Dst,                 0);
        IEM_MC_ARG(uint8_t *,       pu8Al,                  1);
        IEM_MC_ARG(uint8_t,         u8Src,                  2);
        IEM_MC_ARG(uint32_t *,      pEFlags,                3);

        IEM_MC_FETCH_GREG_U8(u8Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_REF_GREG_U8(pu8Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        IEM_MC_REF_GREG_U8(pu8Al, X86_GREG_xAX);
        IEM_MC_REF_EFLAGS(pEFlags);
        if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
            IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg_u8, pu8Dst, pu8Al, u8Src, pEFlags);
        else
            IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg_u8_locked, pu8Dst, pu8Al, u8Src, pEFlags);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        IEM_MC_BEGIN(4, 3);
        IEM_MC_ARG(uint8_t *,       pu8Dst,                 0);
        IEM_MC_ARG(uint8_t *,       pu8Al,                  1);
        IEM_MC_ARG(uint8_t,         u8Src,                  2);
        IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags,        3);
        IEM_MC_LOCAL(RTGCPTR,       GCPtrEffDst);
        IEM_MC_LOCAL(uint8_t,       u8Al);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING();
        IEM_MC_MEM_MAP(pu8Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
        IEM_MC_FETCH_GREG_U8(u8Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_FETCH_GREG_U8(u8Al, X86_GREG_xAX);
        IEM_MC_FETCH_EFLAGS(EFlags);
        IEM_MC_REF_LOCAL(pu8Al, u8Al);
        if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
            IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg_u8, pu8Dst, pu8Al, u8Src, pEFlags);
        else
            IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg_u8_locked, pu8Dst, pu8Al, u8Src, pEFlags);

        IEM_MC_MEM_COMMIT_AND_UNMAP(pu8Dst, IEM_ACCESS_DATA_RW);
        IEM_MC_COMMIT_EFLAGS(EFlags);
        IEM_MC_STORE_GREG_U8(X86_GREG_xAX, u8Al);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}

/** Opcode 0x0f 0xb1. */
FNIEMOP_DEF(iemOp_cmpxchg_Ev_Gv)
{
    IEMOP_MNEMONIC("cmpxchg Ev,Gv");
    IEMOP_HLP_MIN_486();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING();
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(4, 0);
                IEM_MC_ARG(uint16_t *,      pu16Dst,                0);
                IEM_MC_ARG(uint16_t *,      pu16Ax,                 1);
                IEM_MC_ARG(uint16_t,        u16Src,                 2);
                IEM_MC_ARG(uint32_t *,      pEFlags,                3);

                IEM_MC_FETCH_GREG_U16(u16Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_GREG_U16(pu16Ax, X86_GREG_xAX);
                IEM_MC_REF_EFLAGS(pEFlags);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg_u16, pu16Dst, pu16Ax, u16Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg_u16_locked, pu16Dst, pu16Ax, u16Src, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(4, 0);
                IEM_MC_ARG(uint32_t *,      pu32Dst,                0);
                IEM_MC_ARG(uint32_t *,      pu32Eax,                1);
                IEM_MC_ARG(uint32_t,        u32Src,                 2);
                IEM_MC_ARG(uint32_t *,      pEFlags,                3);

                IEM_MC_FETCH_GREG_U32(u32Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_GREG_U32(pu32Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_GREG_U32(pu32Eax, X86_GREG_xAX);
                IEM_MC_REF_EFLAGS(pEFlags);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg_u32, pu32Dst, pu32Eax, u32Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg_u32_locked, pu32Dst, pu32Eax, u32Src, pEFlags);

                IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32Eax);
                IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32Dst);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(4, 0);
                IEM_MC_ARG(uint64_t *,      pu64Dst,                0);
                IEM_MC_ARG(uint64_t *,      pu64Rax,                1);
#ifdef RT_ARCH_X86
                IEM_MC_ARG(uint64_t *,      pu64Src,                2);
#else
                IEM_MC_ARG(uint64_t,        u64Src,                 2);
#endif
                IEM_MC_ARG(uint32_t *,      pEFlags,                3);

                IEM_MC_REF_GREG_U64(pu64Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_GREG_U64(pu64Rax, X86_GREG_xAX);
                IEM_MC_REF_EFLAGS(pEFlags);
#ifdef RT_ARCH_X86
                IEM_MC_REF_GREG_U64(pu64Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg_u64, pu64Dst, pu64Rax, pu64Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg_u64_locked, pu64Dst, pu64Rax, pu64Src, pEFlags);
#else
                IEM_MC_FETCH_GREG_U64(u64Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg_u64, pu64Dst, pu64Rax, u64Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg_u64_locked, pu64Dst, pu64Rax, u64Src, pEFlags);
#endif

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(4, 3);
                IEM_MC_ARG(uint16_t *,      pu16Dst,                0);
                IEM_MC_ARG(uint16_t *,      pu16Ax,                 1);
                IEM_MC_ARG(uint16_t,        u16Src,                 2);
                IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags,        3);
                IEM_MC_LOCAL(RTGCPTR,       GCPtrEffDst);
                IEM_MC_LOCAL(uint16_t,      u16Ax);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING();
                IEM_MC_MEM_MAP(pu16Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
                IEM_MC_FETCH_GREG_U16(u16Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_FETCH_GREG_U16(u16Ax, X86_GREG_xAX);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_REF_LOCAL(pu16Ax, u16Ax);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg_u16, pu16Dst, pu16Ax, u16Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg_u16_locked, pu16Dst, pu16Ax, u16Src, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu16Dst, IEM_ACCESS_DATA_RW);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_STORE_GREG_U16(X86_GREG_xAX, u16Ax);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(4, 3);
                IEM_MC_ARG(uint32_t *,      pu32Dst,                0);
                IEM_MC_ARG(uint32_t *,      pu32Eax,                 1);
                IEM_MC_ARG(uint32_t,        u32Src,                 2);
                IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags,        3);
                IEM_MC_LOCAL(RTGCPTR,       GCPtrEffDst);
                IEM_MC_LOCAL(uint32_t,      u32Eax);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING();
                IEM_MC_MEM_MAP(pu32Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
                IEM_MC_FETCH_GREG_U32(u32Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_FETCH_GREG_U32(u32Eax, X86_GREG_xAX);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_REF_LOCAL(pu32Eax, u32Eax);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg_u32, pu32Dst, pu32Eax, u32Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg_u32_locked, pu32Dst, pu32Eax, u32Src, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu32Dst, IEM_ACCESS_DATA_RW);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_STORE_GREG_U32(X86_GREG_xAX, u32Eax);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(4, 3);
                IEM_MC_ARG(uint64_t *,      pu64Dst,                0);
                IEM_MC_ARG(uint64_t *,      pu64Rax,                1);
#ifdef RT_ARCH_X86
                IEM_MC_ARG(uint64_t *,      pu64Src,                2);
#else
                IEM_MC_ARG(uint64_t,        u64Src,                 2);
#endif
                IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags,        3);
                IEM_MC_LOCAL(RTGCPTR,       GCPtrEffDst);
                IEM_MC_LOCAL(uint64_t,      u64Rax);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING();
                IEM_MC_MEM_MAP(pu64Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
                IEM_MC_FETCH_GREG_U64(u64Rax, X86_GREG_xAX);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_REF_LOCAL(pu64Rax, u64Rax);
#ifdef RT_ARCH_X86
                IEM_MC_REF_GREG_U64(pu64Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg_u64, pu64Dst, pu64Rax, pu64Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg_u64_locked, pu64Dst, pu64Rax, pu64Src, pEFlags);
#else
                IEM_MC_FETCH_GREG_U64(u64Src, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg_u64, pu64Dst, pu64Rax, u64Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg_u64_locked, pu64Dst, pu64Rax, u64Src, pEFlags);
#endif

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu64Dst, IEM_ACCESS_DATA_RW);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_STORE_GREG_U64(X86_GREG_xAX, u64Rax);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


FNIEMOP_DEF_2(iemOpCommonLoadSRegAndGreg, uint8_t, iSegReg, uint8_t, bRm)
{
    Assert((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT)); /* Caller checks this */
    uint8_t const iGReg = ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg;

    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(5, 1);
            IEM_MC_ARG(uint16_t,        uSel,                                    0);
            IEM_MC_ARG(uint16_t,        offSeg,                                  1);
            IEM_MC_ARG_CONST(uint8_t,   iSegRegArg,/*=*/iSegReg,                 2);
            IEM_MC_ARG_CONST(uint8_t,   iGRegArg,  /*=*/iGReg,                   3);
            IEM_MC_ARG_CONST(IEMMODE,   enmEffOpSize,/*=*/pVCpu->iem.s.enmEffOpSize, 4);
            IEM_MC_LOCAL(RTGCPTR,       GCPtrEff);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEff, bRm, 0);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
            IEM_MC_FETCH_MEM_U16(offSeg, pVCpu->iem.s.iEffSeg, GCPtrEff);
            IEM_MC_FETCH_MEM_U16_DISP(uSel, pVCpu->iem.s.iEffSeg, GCPtrEff, 2);
            IEM_MC_CALL_CIMPL_5(iemCImpl_load_SReg_Greg, uSel, offSeg, iSegRegArg, iGRegArg, enmEffOpSize);
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(5, 1);
            IEM_MC_ARG(uint16_t,        uSel,                                    0);
            IEM_MC_ARG(uint32_t,        offSeg,                                  1);
            IEM_MC_ARG_CONST(uint8_t,   iSegRegArg,/*=*/iSegReg,                 2);
            IEM_MC_ARG_CONST(uint8_t,   iGRegArg,  /*=*/iGReg,                   3);
            IEM_MC_ARG_CONST(IEMMODE,   enmEffOpSize,/*=*/pVCpu->iem.s.enmEffOpSize, 4);
            IEM_MC_LOCAL(RTGCPTR,       GCPtrEff);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEff, bRm, 0);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
            IEM_MC_FETCH_MEM_U32(offSeg, pVCpu->iem.s.iEffSeg, GCPtrEff);
            IEM_MC_FETCH_MEM_U16_DISP(uSel, pVCpu->iem.s.iEffSeg, GCPtrEff, 4);
            IEM_MC_CALL_CIMPL_5(iemCImpl_load_SReg_Greg, uSel, offSeg, iSegRegArg, iGRegArg, enmEffOpSize);
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(5, 1);
            IEM_MC_ARG(uint16_t,        uSel,                                    0);
            IEM_MC_ARG(uint64_t,        offSeg,                                  1);
            IEM_MC_ARG_CONST(uint8_t,   iSegRegArg,/*=*/iSegReg,                 2);
            IEM_MC_ARG_CONST(uint8_t,   iGRegArg,  /*=*/iGReg,                   3);
            IEM_MC_ARG_CONST(IEMMODE,   enmEffOpSize,/*=*/pVCpu->iem.s.enmEffOpSize, 4);
            IEM_MC_LOCAL(RTGCPTR,       GCPtrEff);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEff, bRm, 0);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
            if (IEM_IS_GUEST_CPU_AMD(pVCpu)) /** @todo testcase: rev 3.15 of the amd manuals claims it only loads a 32-bit greg. */
                IEM_MC_FETCH_MEM_U32_SX_U64(offSeg, pVCpu->iem.s.iEffSeg, GCPtrEff);
            else
                IEM_MC_FETCH_MEM_U64(offSeg, pVCpu->iem.s.iEffSeg, GCPtrEff);
            IEM_MC_FETCH_MEM_U16_DISP(uSel, pVCpu->iem.s.iEffSeg, GCPtrEff, 8);
            IEM_MC_CALL_CIMPL_5(iemCImpl_load_SReg_Greg, uSel, offSeg, iSegRegArg, iGRegArg, enmEffOpSize);
            IEM_MC_END();
            return VINF_SUCCESS;

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/** Opcode 0x0f 0xb2. */
FNIEMOP_DEF(iemOp_lss_Gv_Mp)
{
    IEMOP_MNEMONIC("lss Gv,Mp");
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
        return IEMOP_RAISE_INVALID_OPCODE();
    return FNIEMOP_CALL_2(iemOpCommonLoadSRegAndGreg, X86_SREG_SS, bRm);
}


/** Opcode 0x0f 0xb3. */
FNIEMOP_DEF(iemOp_btr_Ev_Gv)
{
    IEMOP_MNEMONIC("btr Ev,Gv");
    return FNIEMOP_CALL_1(iemOpCommonBit_Ev_Gv, &g_iemAImpl_btr);
}


/** Opcode 0x0f 0xb4. */
FNIEMOP_DEF(iemOp_lfs_Gv_Mp)
{
    IEMOP_MNEMONIC("lfs Gv,Mp");
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
        return IEMOP_RAISE_INVALID_OPCODE();
    return FNIEMOP_CALL_2(iemOpCommonLoadSRegAndGreg, X86_SREG_FS, bRm);
}


/** Opcode 0x0f 0xb5. */
FNIEMOP_DEF(iemOp_lgs_Gv_Mp)
{
    IEMOP_MNEMONIC("lgs Gv,Mp");
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
        return IEMOP_RAISE_INVALID_OPCODE();
    return FNIEMOP_CALL_2(iemOpCommonLoadSRegAndGreg, X86_SREG_GS, bRm);
}


/** Opcode 0x0f 0xb6. */
FNIEMOP_DEF(iemOp_movzx_Gv_Eb)
{
    IEMOP_MNEMONIC("movzx Gv,Eb");
    IEMOP_HLP_MIN_386();

    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint16_t, u16Value);
                IEM_MC_FETCH_GREG_U8_ZX_U16(u16Value, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u16Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint32_t, u32Value);
                IEM_MC_FETCH_GREG_U8_ZX_U32(u32Value, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u32Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint64_t, u64Value);
                IEM_MC_FETCH_GREG_U8_ZX_U64(u64Value, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u64Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        /*
         * We're loading a register from memory.
         */
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint16_t, u16Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U8_ZX_U16(u16Value, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
                IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u16Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint32_t, u32Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U8_ZX_U32(u32Value, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
                IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u32Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint64_t, u64Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U8_ZX_U64(u64Value, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
                IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u64Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


/** Opcode 0x0f 0xb7. */
FNIEMOP_DEF(iemOp_movzx_Gv_Ew)
{
    IEMOP_MNEMONIC("movzx Gv,Ew");
    IEMOP_HLP_MIN_386();

    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /** @todo Not entirely sure how the operand size prefix is handled here,
     *        assuming that it will be ignored. Would be nice to have a few
     *        test for this. */
    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        if (pVCpu->iem.s.enmEffOpSize != IEMMODE_64BIT)
        {
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint32_t, u32Value);
            IEM_MC_FETCH_GREG_U16_ZX_U32(u32Value, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
            IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u32Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
        else
        {
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint64_t, u64Value);
            IEM_MC_FETCH_GREG_U16_ZX_U64(u64Value, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
            IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u64Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
    }
    else
    {
        /*
         * We're loading a register from memory.
         */
        if (pVCpu->iem.s.enmEffOpSize != IEMMODE_64BIT)
        {
            IEM_MC_BEGIN(0, 2);
            IEM_MC_LOCAL(uint32_t, u32Value);
            IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
            IEM_MC_FETCH_MEM_U16_ZX_U32(u32Value, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
            IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u32Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
        else
        {
            IEM_MC_BEGIN(0, 2);
            IEM_MC_LOCAL(uint64_t, u64Value);
            IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
            IEM_MC_FETCH_MEM_U16_ZX_U64(u64Value, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
            IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u64Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0xb8. */
FNIEMOP_STUB(iemOp_popcnt_Gv_Ev_jmpe);


/** Opcode 0x0f 0xb9. */
FNIEMOP_DEF(iemOp_Grp10)
{
    Log(("iemOp_Grp10 -> #UD\n"));
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Opcode 0x0f 0xba. */
FNIEMOP_DEF(iemOp_Grp8)
{
    IEMOP_HLP_MIN_386();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    PCIEMOPBINSIZES pImpl;
    switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
    {
        case 0: case 1: case 2: case 3:
            return IEMOP_RAISE_INVALID_OPCODE();
        case 4: pImpl = &g_iemAImpl_bt;  IEMOP_MNEMONIC("bt  Ev,Ib"); break;
        case 5: pImpl = &g_iemAImpl_bts; IEMOP_MNEMONIC("bts Ev,Ib"); break;
        case 6: pImpl = &g_iemAImpl_btr; IEMOP_MNEMONIC("btr Ev,Ib"); break;
        case 7: pImpl = &g_iemAImpl_btc; IEMOP_MNEMONIC("btc Ev,Ib"); break;
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register destination. */
        uint8_t u8Bit; IEM_OPCODE_GET_NEXT_U8(&u8Bit);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint16_t *,      pu16Dst,                    0);
                IEM_MC_ARG_CONST(uint16_t,  u16Src, /*=*/ u8Bit & 0x0f, 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                    2);

                IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU16, pu16Dst, u16Src, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint32_t *,      pu32Dst,                    0);
                IEM_MC_ARG_CONST(uint32_t,  u32Src, /*=*/ u8Bit & 0x1f, 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                    2);

                IEM_MC_REF_GREG_U32(pu32Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, u32Src, pEFlags);

                IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32Dst);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint64_t *,      pu64Dst,                    0);
                IEM_MC_ARG_CONST(uint64_t,  u64Src, /*=*/ u8Bit & 0x3f, 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                    2);

                IEM_MC_REF_GREG_U64(pu64Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU64, pu64Dst, u64Src, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        /* memory destination. */

        uint32_t fAccess;
        if (pImpl->pfnLockedU16)
            fAccess = IEM_ACCESS_DATA_RW;
        else /* BT */
            fAccess = IEM_ACCESS_DATA_R;

        /** @todo test negative bit offsets! */
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 1);
                IEM_MC_ARG(uint16_t *,              pu16Dst,                0);
                IEM_MC_ARG(uint16_t,                u16Src,                 1);
                IEM_MC_ARG_LOCAL_EFLAGS(            pEFlags, EFlags,        2);
                IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 1);
                uint8_t u8Bit; IEM_OPCODE_GET_NEXT_U8(&u8Bit);
                IEM_MC_ASSIGN(u16Src, u8Bit & 0x0f);
                if (pImpl->pfnLockedU16)
                    IEMOP_HLP_DONE_DECODING();
                else
                    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_MEM_MAP(pu16Dst, fAccess, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU16, pu16Dst, u16Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnLockedU16, pu16Dst, u16Src, pEFlags);
                IEM_MC_MEM_COMMIT_AND_UNMAP(pu16Dst, fAccess);

                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(3, 1);
                IEM_MC_ARG(uint32_t *,              pu32Dst,                0);
                IEM_MC_ARG(uint32_t,                u32Src,                 1);
                IEM_MC_ARG_LOCAL_EFLAGS(            pEFlags, EFlags,        2);
                IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 1);
                uint8_t u8Bit; IEM_OPCODE_GET_NEXT_U8(&u8Bit);
                IEM_MC_ASSIGN(u32Src, u8Bit & 0x1f);
                if (pImpl->pfnLockedU16)
                    IEMOP_HLP_DONE_DECODING();
                else
                    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_MEM_MAP(pu32Dst, fAccess, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, u32Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnLockedU32, pu32Dst, u32Src, pEFlags);
                IEM_MC_MEM_COMMIT_AND_UNMAP(pu32Dst, fAccess);

                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 1);
                IEM_MC_ARG(uint64_t *,              pu64Dst,                0);
                IEM_MC_ARG(uint64_t,                u64Src,                 1);
                IEM_MC_ARG_LOCAL_EFLAGS(            pEFlags, EFlags,        2);
                IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 1);
                uint8_t u8Bit; IEM_OPCODE_GET_NEXT_U8(&u8Bit);
                IEM_MC_ASSIGN(u64Src, u8Bit & 0x3f);
                if (pImpl->pfnLockedU16)
                    IEMOP_HLP_DONE_DECODING();
                else
                    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_MEM_MAP(pu64Dst, fAccess, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU64, pu64Dst, u64Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnLockedU64, pu64Dst, u64Src, pEFlags);
                IEM_MC_MEM_COMMIT_AND_UNMAP(pu64Dst, fAccess);

                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }

}


/** Opcode 0x0f 0xbb. */
FNIEMOP_DEF(iemOp_btc_Ev_Gv)
{
    IEMOP_MNEMONIC("btc Ev,Gv");
    IEMOP_HLP_MIN_386();
    return FNIEMOP_CALL_1(iemOpCommonBit_Ev_Gv, &g_iemAImpl_btc);
}


/** Opcode 0x0f 0xbc. */
FNIEMOP_DEF(iemOp_bsf_Gv_Ev)
{
    IEMOP_MNEMONIC("bsf Gv,Ev");
    IEMOP_HLP_MIN_386();
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_OF | X86_EFL_SF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rv_rm, &g_iemAImpl_bsf);
}


/** Opcode 0x0f 0xbd. */
FNIEMOP_DEF(iemOp_bsr_Gv_Ev)
{
    IEMOP_MNEMONIC("bsr Gv,Ev");
    IEMOP_HLP_MIN_386();
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_OF | X86_EFL_SF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rv_rm, &g_iemAImpl_bsr);
}


/** Opcode 0x0f 0xbe. */
FNIEMOP_DEF(iemOp_movsx_Gv_Eb)
{
    IEMOP_MNEMONIC("movsx Gv,Eb");
    IEMOP_HLP_MIN_386();

    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint16_t, u16Value);
                IEM_MC_FETCH_GREG_U8_SX_U16(u16Value, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u16Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint32_t, u32Value);
                IEM_MC_FETCH_GREG_U8_SX_U32(u32Value, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u32Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint64_t, u64Value);
                IEM_MC_FETCH_GREG_U8_SX_U64(u64Value, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u64Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        /*
         * We're loading a register from memory.
         */
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint16_t, u16Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U8_SX_U16(u16Value, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
                IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u16Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint32_t, u32Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U8_SX_U32(u32Value, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
                IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u32Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint64_t, u64Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U8_SX_U64(u64Value, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
                IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u64Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


/** Opcode 0x0f 0xbf. */
FNIEMOP_DEF(iemOp_movsx_Gv_Ew)
{
    IEMOP_MNEMONIC("movsx Gv,Ew");
    IEMOP_HLP_MIN_386();

    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /** @todo Not entirely sure how the operand size prefix is handled here,
     *        assuming that it will be ignored. Would be nice to have a few
     *        test for this. */
    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        if (pVCpu->iem.s.enmEffOpSize != IEMMODE_64BIT)
        {
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint32_t, u32Value);
            IEM_MC_FETCH_GREG_U16_SX_U32(u32Value, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
            IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u32Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
        else
        {
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint64_t, u64Value);
            IEM_MC_FETCH_GREG_U16_SX_U64(u64Value, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
            IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u64Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
    }
    else
    {
        /*
         * We're loading a register from memory.
         */
        if (pVCpu->iem.s.enmEffOpSize != IEMMODE_64BIT)
        {
            IEM_MC_BEGIN(0, 2);
            IEM_MC_LOCAL(uint32_t, u32Value);
            IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
            IEM_MC_FETCH_MEM_U16_SX_U32(u32Value, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
            IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u32Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
        else
        {
            IEM_MC_BEGIN(0, 2);
            IEM_MC_LOCAL(uint64_t, u64Value);
            IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
            IEM_MC_FETCH_MEM_U16_SX_U64(u64Value, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
            IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u64Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
        }
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0xc0. */
FNIEMOP_DEF(iemOp_xadd_Eb_Gb)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_HLP_MIN_486();
    IEMOP_MNEMONIC("xadd Eb,Gb");

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(3, 0);
        IEM_MC_ARG(uint8_t *,  pu8Dst,  0);
        IEM_MC_ARG(uint8_t *,  pu8Reg,  1);
        IEM_MC_ARG(uint32_t *, pEFlags, 2);

        IEM_MC_REF_GREG_U8(pu8Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        IEM_MC_REF_GREG_U8(pu8Reg, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_REF_EFLAGS(pEFlags);
        IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_xadd_u8, pu8Dst, pu8Reg, pEFlags);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /*
         * We're accessing memory.
         */
        IEM_MC_BEGIN(3, 3);
        IEM_MC_ARG(uint8_t *,   pu8Dst,          0);
        IEM_MC_ARG(uint8_t *,   pu8Reg,          1);
        IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 2);
        IEM_MC_LOCAL(uint8_t,  u8RegCopy);
        IEM_MC_LOCAL(RTGCPTR,  GCPtrEffDst);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEM_MC_MEM_MAP(pu8Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
        IEM_MC_FETCH_GREG_U8(u8RegCopy, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_REF_LOCAL(pu8Reg, u8RegCopy);
        IEM_MC_FETCH_EFLAGS(EFlags);
        if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
            IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_xadd_u8, pu8Dst, pu8Reg, pEFlags);
        else
            IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_xadd_u8_locked, pu8Dst, pu8Reg, pEFlags);

        IEM_MC_MEM_COMMIT_AND_UNMAP(pu8Dst, IEM_ACCESS_DATA_RW);
        IEM_MC_COMMIT_EFLAGS(EFlags);
        IEM_MC_STORE_GREG_U8(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u8RegCopy);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
        return VINF_SUCCESS;
    }
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0xc1. */
FNIEMOP_DEF(iemOp_xadd_Ev_Gv)
{
    IEMOP_MNEMONIC("xadd Ev,Gv");
    IEMOP_HLP_MIN_486();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint16_t *, pu16Dst,  0);
                IEM_MC_ARG(uint16_t *, pu16Reg,  1);
                IEM_MC_ARG(uint32_t *, pEFlags, 2);

                IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_GREG_U16(pu16Reg, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_xadd_u16, pu16Dst, pu16Reg, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint32_t *, pu32Dst,  0);
                IEM_MC_ARG(uint32_t *, pu32Reg,  1);
                IEM_MC_ARG(uint32_t *, pEFlags, 2);

                IEM_MC_REF_GREG_U32(pu32Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_GREG_U32(pu32Reg, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_xadd_u32, pu32Dst, pu32Reg, pEFlags);

                IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32Dst);
                IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32Reg);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint64_t *, pu64Dst,  0);
                IEM_MC_ARG(uint64_t *, pu64Reg,  1);
                IEM_MC_ARG(uint32_t *, pEFlags, 2);

                IEM_MC_REF_GREG_U64(pu64Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_GREG_U64(pu64Reg, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_xadd_u64, pu64Dst, pu64Reg, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        /*
         * We're accessing memory.
         */
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 3);
                IEM_MC_ARG(uint16_t *,  pu16Dst,         0);
                IEM_MC_ARG(uint16_t *,  pu16Reg,         1);
                IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 2);
                IEM_MC_LOCAL(uint16_t,  u16RegCopy);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEM_MC_MEM_MAP(pu16Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_GREG_U16(u16RegCopy, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_LOCAL(pu16Reg, u16RegCopy);
                IEM_MC_FETCH_EFLAGS(EFlags);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_xadd_u16, pu16Dst, pu16Reg, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_xadd_u16_locked, pu16Dst, pu16Reg, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu16Dst, IEM_ACCESS_DATA_RW);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u16RegCopy);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(3, 3);
                IEM_MC_ARG(uint32_t *,  pu32Dst,         0);
                IEM_MC_ARG(uint32_t *,  pu32Reg,         1);
                IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 2);
                IEM_MC_LOCAL(uint32_t,  u32RegCopy);
                IEM_MC_LOCAL(RTGCPTR,   GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEM_MC_MEM_MAP(pu32Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_GREG_U32(u32RegCopy, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_LOCAL(pu32Reg, u32RegCopy);
                IEM_MC_FETCH_EFLAGS(EFlags);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_xadd_u32, pu32Dst, pu32Reg, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_xadd_u32_locked, pu32Dst, pu32Reg, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu32Dst, IEM_ACCESS_DATA_RW);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u32RegCopy);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 3);
                IEM_MC_ARG(uint64_t *,  pu64Dst,         0);
                IEM_MC_ARG(uint64_t *,  pu64Reg,         1);
                IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 2);
                IEM_MC_LOCAL(uint64_t,  u64RegCopy);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEM_MC_MEM_MAP(pu64Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_GREG_U64(u64RegCopy, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_REF_LOCAL(pu64Reg, u64RegCopy);
                IEM_MC_FETCH_EFLAGS(EFlags);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_xadd_u64, pu64Dst, pu64Reg, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_xadd_u64_locked, pu64Dst, pu64Reg, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu64Dst, IEM_ACCESS_DATA_RW);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u64RegCopy);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}

/** Opcode 0x0f 0xc2. */
FNIEMOP_STUB(iemOp_cmpps_Vps_Wps_Ib__cmppd_Vpd_Wpd_Ib__cmpss_Vss_Wss_Ib__cmpsd_Vsd_Wsd_Ib);


/** Opcode 0x0f 0xc3. */
FNIEMOP_DEF(iemOp_movnti_My_Gy)
{
    IEMOP_MNEMONIC("movnti My,Gy");

    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /* Only the register -> memory form makes sense, assuming #UD for the other form. */
    if ((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT))
    {
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint32_t, u32Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                if (!IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fSse2)
                    return IEMOP_RAISE_INVALID_OPCODE();

                IEM_MC_FETCH_GREG_U32(u32Value, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_STORE_MEM_U32(pVCpu->iem.s.iEffSeg, GCPtrEffDst, u32Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint64_t, u64Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                if (!IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fSse2)
                    return IEMOP_RAISE_INVALID_OPCODE();

                IEM_MC_FETCH_GREG_U64(u64Value, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_STORE_MEM_U64(pVCpu->iem.s.iEffSeg, GCPtrEffDst, u64Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_16BIT:
                /** @todo check this form.   */
                return IEMOP_RAISE_INVALID_OPCODE();
        }
    }
    else
        return IEMOP_RAISE_INVALID_OPCODE();
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0xc4. */
FNIEMOP_STUB(iemOp_pinsrw_Pq_Ry_Mw_Ib__pinsrw_Vdq_Ry_Mw_Ib);

/** Opcode 0x0f 0xc5. */
FNIEMOP_STUB(iemOp_pextrw_Gd_Nq_Ib__pextrw_Gd_Udq_Ib);

/** Opcode 0x0f 0xc6. */
FNIEMOP_STUB(iemOp_shufps_Vps_Wps_Ib__shufdp_Vpd_Wpd_Ib);


/** Opcode 0x0f 0xc7 !11/1. */
FNIEMOP_DEF_1(iemOp_Grp9_cmpxchg8b_Mq, uint8_t, bRm)
{
    IEMOP_MNEMONIC("cmpxchg8b Mq");

    IEM_MC_BEGIN(4, 3);
    IEM_MC_ARG(uint64_t *, pu64MemDst,     0);
    IEM_MC_ARG(PRTUINT64U, pu64EaxEdx,     1);
    IEM_MC_ARG(PRTUINT64U, pu64EbxEcx,     2);
    IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 3);
    IEM_MC_LOCAL(RTUINT64U, u64EaxEdx);
    IEM_MC_LOCAL(RTUINT64U, u64EbxEcx);
    IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
    IEMOP_HLP_DONE_DECODING();
    IEM_MC_MEM_MAP(pu64MemDst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);

    IEM_MC_FETCH_GREG_U32(u64EaxEdx.s.Lo, X86_GREG_xAX);
    IEM_MC_FETCH_GREG_U32(u64EaxEdx.s.Hi, X86_GREG_xDX);
    IEM_MC_REF_LOCAL(pu64EaxEdx, u64EaxEdx);

    IEM_MC_FETCH_GREG_U32(u64EbxEcx.s.Lo, X86_GREG_xBX);
    IEM_MC_FETCH_GREG_U32(u64EbxEcx.s.Hi, X86_GREG_xCX);
    IEM_MC_REF_LOCAL(pu64EbxEcx, u64EbxEcx);

    IEM_MC_FETCH_EFLAGS(EFlags);
    if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
        IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg8b, pu64MemDst, pu64EaxEdx, pu64EbxEcx, pEFlags);
    else
        IEM_MC_CALL_VOID_AIMPL_4(iemAImpl_cmpxchg8b_locked, pu64MemDst, pu64EaxEdx, pu64EbxEcx, pEFlags);

    IEM_MC_MEM_COMMIT_AND_UNMAP(pu64MemDst, IEM_ACCESS_DATA_RW);
    IEM_MC_COMMIT_EFLAGS(EFlags);
    IEM_MC_IF_EFL_BIT_NOT_SET(X86_EFL_ZF)
        /** @todo Testcase: Check effect of cmpxchg8b on bits 63:32 in rax and rdx. */
        IEM_MC_STORE_GREG_U32(X86_GREG_xAX, u64EaxEdx.s.Lo);
        IEM_MC_STORE_GREG_U32(X86_GREG_xDX, u64EaxEdx.s.Hi);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode REX.W 0x0f 0xc7 !11/1. */
FNIEMOP_UD_STUB_1(iemOp_Grp9_cmpxchg16b_Mdq, uint8_t, bRm);

/** Opcode 0x0f 0xc7 11/6. */
FNIEMOP_UD_STUB_1(iemOp_Grp9_rdrand_Rv, uint8_t, bRm);

/** Opcode 0x0f 0xc7 !11/6. */
FNIEMOP_UD_STUB_1(iemOp_Grp9_vmptrld_Mq, uint8_t, bRm);

/** Opcode 0x66 0x0f 0xc7 !11/6. */
FNIEMOP_UD_STUB_1(iemOp_Grp9_vmclear_Mq, uint8_t, bRm);

/** Opcode 0xf3 0x0f 0xc7 !11/6. */
FNIEMOP_UD_STUB_1(iemOp_Grp9_vmxon_Mq, uint8_t, bRm);

/** Opcode [0xf3] 0x0f 0xc7 !11/7. */
FNIEMOP_UD_STUB_1(iemOp_Grp9_vmptrst_Mq, uint8_t, bRm);


/** Opcode 0x0f 0xc7. */
FNIEMOP_DEF(iemOp_Grp9)
{
    /** @todo Testcase: Check mixing 0x66 and 0xf3. Check the effect of 0xf2. */
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
    {
        case 0: case 2: case 3: case 4: case 5:
            return IEMOP_RAISE_INVALID_OPCODE();
        case 1:
            /** @todo Testcase: Check prefix effects on cmpxchg8b/16b. */
            if (   (bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT)
                || (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPZ))) /** @todo Testcase: AMD seems to express a different idea here wrt prefixes. */
                return IEMOP_RAISE_INVALID_OPCODE();
            if (bRm & IEM_OP_PRF_SIZE_REX_W)
                return FNIEMOP_CALL_1(iemOp_Grp9_cmpxchg16b_Mdq, bRm);
            return FNIEMOP_CALL_1(iemOp_Grp9_cmpxchg8b_Mq, bRm);
        case 6:
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
                return FNIEMOP_CALL_1(iemOp_Grp9_rdrand_Rv, bRm);
            switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPZ))
            {
                case 0:
                    return FNIEMOP_CALL_1(iemOp_Grp9_vmptrld_Mq, bRm);
                case IEM_OP_PRF_SIZE_OP:
                    return FNIEMOP_CALL_1(iemOp_Grp9_vmclear_Mq, bRm);
                case IEM_OP_PRF_REPZ:
                    return FNIEMOP_CALL_1(iemOp_Grp9_vmxon_Mq, bRm);
                default:
                    return IEMOP_RAISE_INVALID_OPCODE();
            }
        case 7:
            switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPZ))
            {
                case 0:
                case IEM_OP_PRF_REPZ:
                    return FNIEMOP_CALL_1(iemOp_Grp9_vmptrst_Mq, bRm);
                default:
                    return IEMOP_RAISE_INVALID_OPCODE();
            }
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/**
 * Common 'bswap register' helper.
 */
FNIEMOP_DEF_1(iemOpCommonBswapGReg, uint8_t, iReg)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(1, 0);
            IEM_MC_ARG(uint32_t *,  pu32Dst, 0);
            IEM_MC_REF_GREG_U32(pu32Dst, iReg);     /* Don't clear the high dword! */
            IEM_MC_CALL_VOID_AIMPL_1(iemAImpl_bswap_u16, pu32Dst);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(1, 0);
            IEM_MC_ARG(uint32_t *,  pu32Dst, 0);
            IEM_MC_REF_GREG_U32(pu32Dst, iReg);
            IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32Dst);
            IEM_MC_CALL_VOID_AIMPL_1(iemAImpl_bswap_u32, pu32Dst);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(1, 0);
            IEM_MC_ARG(uint64_t *,  pu64Dst, 0);
            IEM_MC_REF_GREG_U64(pu64Dst, iReg);
            IEM_MC_CALL_VOID_AIMPL_1(iemAImpl_bswap_u64, pu64Dst);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/** Opcode 0x0f 0xc8. */
FNIEMOP_DEF(iemOp_bswap_rAX_r8)
{
    IEMOP_MNEMONIC("bswap rAX/r8");
    /* Note! Intel manuals states that R8-R15 can be accessed by using a REX.X
             prefix.  REX.B is the correct prefix it appears.  For a parallel
             case, see iemOp_mov_AL_Ib and iemOp_mov_eAX_Iv. */
    IEMOP_HLP_MIN_486();
    return FNIEMOP_CALL_1(iemOpCommonBswapGReg, X86_GREG_xAX | pVCpu->iem.s.uRexB);
}


/** Opcode 0x0f 0xc9. */
FNIEMOP_DEF(iemOp_bswap_rCX_r9)
{
    IEMOP_MNEMONIC("bswap rCX/r9");
    IEMOP_HLP_MIN_486();
    return FNIEMOP_CALL_1(iemOpCommonBswapGReg, X86_GREG_xCX | pVCpu->iem.s.uRexB);
}


/** Opcode 0x0f 0xca. */
FNIEMOP_DEF(iemOp_bswap_rDX_r10)
{
    IEMOP_MNEMONIC("bswap rDX/r9");
    IEMOP_HLP_MIN_486();
    return FNIEMOP_CALL_1(iemOpCommonBswapGReg, X86_GREG_xDX | pVCpu->iem.s.uRexB);
}


/** Opcode 0x0f 0xcb. */
FNIEMOP_DEF(iemOp_bswap_rBX_r11)
{
    IEMOP_MNEMONIC("bswap rBX/r9");
    IEMOP_HLP_MIN_486();
    return FNIEMOP_CALL_1(iemOpCommonBswapGReg, X86_GREG_xBX | pVCpu->iem.s.uRexB);
}


/** Opcode 0x0f 0xcc. */
FNIEMOP_DEF(iemOp_bswap_rSP_r12)
{
    IEMOP_MNEMONIC("bswap rSP/r12");
    IEMOP_HLP_MIN_486();
    return FNIEMOP_CALL_1(iemOpCommonBswapGReg, X86_GREG_xSP | pVCpu->iem.s.uRexB);
}


/** Opcode 0x0f 0xcd. */
FNIEMOP_DEF(iemOp_bswap_rBP_r13)
{
    IEMOP_MNEMONIC("bswap rBP/r13");
    IEMOP_HLP_MIN_486();
    return FNIEMOP_CALL_1(iemOpCommonBswapGReg, X86_GREG_xBP | pVCpu->iem.s.uRexB);
}


/** Opcode 0x0f 0xce. */
FNIEMOP_DEF(iemOp_bswap_rSI_r14)
{
    IEMOP_MNEMONIC("bswap rSI/r14");
    IEMOP_HLP_MIN_486();
    return FNIEMOP_CALL_1(iemOpCommonBswapGReg, X86_GREG_xSI | pVCpu->iem.s.uRexB);
}


/** Opcode 0x0f 0xcf. */
FNIEMOP_DEF(iemOp_bswap_rDI_r15)
{
    IEMOP_MNEMONIC("bswap rDI/r15");
    IEMOP_HLP_MIN_486();
    return FNIEMOP_CALL_1(iemOpCommonBswapGReg, X86_GREG_xDI | pVCpu->iem.s.uRexB);
}



/** Opcode 0x0f 0xd0. */
FNIEMOP_STUB(iemOp_addsubpd_Vpd_Wpd__addsubps_Vps_Wps);
/** Opcode 0x0f 0xd1. */
FNIEMOP_STUB(iemOp_psrlw_Pp_Qp__psrlw_Vdp_Wdq);
/** Opcode 0x0f 0xd2. */
FNIEMOP_STUB(iemOp_psrld_Pq_Qq__psrld_Vdq_Wdq);
/** Opcode 0x0f 0xd3. */
FNIEMOP_STUB(iemOp_psrlq_Pq_Qq__psrlq_Vdq_Wdq);
/** Opcode 0x0f 0xd4. */
FNIEMOP_STUB(iemOp_paddq_Pq_Qq__paddq_Vdq_Wdq);
/** Opcode 0x0f 0xd5. */
FNIEMOP_STUB(iemOp_pmulq_Pq_Qq__pmullw_Vdq_Wdq);
/** Opcode 0x0f 0xd6. */
FNIEMOP_STUB(iemOp_movq_Wq_Vq__movq2dq_Vdq_Nq__movdq2q_Pq_Uq);


/** Opcode 0x0f 0xd7. */
FNIEMOP_DEF(iemOp_pmovmskb_Gd_Nq__pmovmskb_Gd_Udq)
{
    /* Docs says register only. */
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT)) /** @todo test that this is registers only. */
        return IEMOP_RAISE_INVALID_OPCODE();

    /* Note! Taking the lazy approch here wrt the high 32-bits of the GREG. */
    /** @todo testcase: Check that the instruction implicitly clears the high
     *        bits in 64-bit mode.  The REX.W is first necessary when VLMAX > 256
     *        and opcode modifications are made to work with the whole width (not
     *        just 128). */
    switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        case IEM_OP_PRF_SIZE_OP: /* SSE */
            IEMOP_MNEMONIC("pmovmskb Gd,Nq");
            IEMOP_HLP_DECODED_NL_2(OP_PMOVMSKB, IEMOPFORM_RM_REG, OP_PARM_Gd, OP_PARM_Vdq, DISOPTYPE_SSE | DISOPTYPE_HARMLESS);
            IEM_MC_BEGIN(2, 0);
            IEM_MC_ARG(uint64_t *,           pDst, 0);
            IEM_MC_ARG(uint128_t const *,    pSrc, 1);
            IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
            IEM_MC_PREPARE_SSE_USAGE();
            IEM_MC_REF_GREG_U64(pDst, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
            IEM_MC_REF_XREG_U128_CONST(pSrc, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
            IEM_MC_CALL_SSE_AIMPL_2(iemAImpl_pmovmskb_u128, pDst, pSrc);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case 0: /* MMX */
            IEMOP_MNEMONIC("pmovmskb Gd,Udq");
            IEMOP_HLP_DECODED_NL_2(OP_PMOVMSKB, IEMOPFORM_RM_REG, OP_PARM_Gd, OP_PARM_Vdq, DISOPTYPE_MMX | DISOPTYPE_HARMLESS);
            IEM_MC_BEGIN(2, 0);
            IEM_MC_ARG(uint64_t *,          pDst, 0);
            IEM_MC_ARG(uint64_t const *,    pSrc, 1);
            IEM_MC_MAYBE_RAISE_MMX_RELATED_XCPT_CHECK_SSE_OR_MMXEXT();
            IEM_MC_PREPARE_FPU_USAGE();
            IEM_MC_REF_GREG_U64(pDst, (bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK);
            IEM_MC_REF_MREG_U64_CONST(pSrc, bRm & X86_MODRM_RM_MASK);
            IEM_MC_CALL_MMX_AIMPL_2(iemAImpl_pmovmskb_u64, pDst, pSrc);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        default:
            return IEMOP_RAISE_INVALID_OPCODE();
    }
}


/** Opcode 0x0f 0xd8. */
FNIEMOP_STUB(iemOp_psubusb_Pq_Qq__psubusb_Vdq_Wdq);
/** Opcode 0x0f 0xd9. */
FNIEMOP_STUB(iemOp_psubusw_Pq_Qq__psubusw_Vdq_Wdq);
/** Opcode 0x0f 0xda. */
FNIEMOP_STUB(iemOp_pminub_Pq_Qq__pminub_Vdq_Wdq);
/** Opcode 0x0f 0xdb. */
FNIEMOP_STUB(iemOp_pand_Pq_Qq__pand_Vdq_Wdq);
/** Opcode 0x0f 0xdc. */
FNIEMOP_STUB(iemOp_paddusb_Pq_Qq__paddusb_Vdq_Wdq);
/** Opcode 0x0f 0xdd. */
FNIEMOP_STUB(iemOp_paddusw_Pq_Qq__paddusw_Vdq_Wdq);
/** Opcode 0x0f 0xde. */
FNIEMOP_STUB(iemOp_pmaxub_Pq_Qq__pamxub_Vdq_Wdq);
/** Opcode 0x0f 0xdf. */
FNIEMOP_STUB(iemOp_pandn_Pq_Qq__pandn_Vdq_Wdq);
/** Opcode 0x0f 0xe0. */
FNIEMOP_STUB(iemOp_pavgb_Pq_Qq__pavgb_Vdq_Wdq);
/** Opcode 0x0f 0xe1. */
FNIEMOP_STUB(iemOp_psraw_Pq_Qq__psraw_Vdq_Wdq);
/** Opcode 0x0f 0xe2. */
FNIEMOP_STUB(iemOp_psrad_Pq_Qq__psrad_Vdq_Wdq);
/** Opcode 0x0f 0xe3. */
FNIEMOP_STUB(iemOp_pavgw_Pq_Qq__pavgw_Vdq_Wdq);
/** Opcode 0x0f 0xe4. */
FNIEMOP_STUB(iemOp_pmulhuw_Pq_Qq__pmulhuw_Vdq_Wdq);
/** Opcode 0x0f 0xe5. */
FNIEMOP_STUB(iemOp_pmulhw_Pq_Qq__pmulhw_Vdq_Wdq);
/** Opcode 0x0f 0xe6. */
FNIEMOP_STUB(iemOp_cvttpd2dq_Vdq_Wdp__cvtdq2pd_Vdq_Wpd__cvtpd2dq_Vdq_Wpd);


/** Opcode 0x0f 0xe7. */
FNIEMOP_DEF(iemOp_movntq_Mq_Pq__movntdq_Mdq_Vdq)
{
    IEMOP_MNEMONIC(!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_OP) ? "movntq mr,r" : "movntdq mr,r");
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT))
    {
        /*
         * Register, memory.
         */
/** @todo check when the REPNZ/Z bits kick in. Same as lock, probably... */
        switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
        {

            case IEM_OP_PRF_SIZE_OP: /* SSE */
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint128_t,                 uSrc);
                IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT();
                IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ();

                IEM_MC_FETCH_XREG_U128(uSrc, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_STORE_MEM_U128_ALIGN_SSE(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case 0: /* MMX */
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint64_t,                  uSrc);
                IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MAYBE_RAISE_MMX_RELATED_XCPT();
                IEM_MC_ACTUALIZE_FPU_STATE_FOR_READ();

                IEM_MC_FETCH_MREG_U64(uSrc, (bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK);
                IEM_MC_STORE_MEM_U64(pVCpu->iem.s.iEffSeg, GCPtrEffSrc, uSrc);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            default:
                return IEMOP_RAISE_INVALID_OPCODE();
        }
    }
    /* The register, register encoding is invalid. */
    else
        return IEMOP_RAISE_INVALID_OPCODE();
    return VINF_SUCCESS;
}


/** Opcode 0x0f 0xe8. */
FNIEMOP_STUB(iemOp_psubsb_Pq_Qq__psubsb_Vdq_Wdq);
/** Opcode 0x0f 0xe9. */
FNIEMOP_STUB(iemOp_psubsw_Pq_Qq__psubsw_Vdq_Wdq);
/** Opcode 0x0f 0xea. */
FNIEMOP_STUB(iemOp_pminsw_Pq_Qq__pminsw_Vdq_Wdq);
/** Opcode 0x0f 0xeb. */
FNIEMOP_STUB(iemOp_por_Pq_Qq__por_Vdq_Wdq);
/** Opcode 0x0f 0xec. */
FNIEMOP_STUB(iemOp_paddsb_Pq_Qq__paddsb_Vdq_Wdq);
/** Opcode 0x0f 0xed. */
FNIEMOP_STUB(iemOp_paddsw_Pq_Qq__paddsw_Vdq_Wdq);
/** Opcode 0x0f 0xee. */
FNIEMOP_STUB(iemOp_pmaxsw_Pq_Qq__pmaxsw_Vdq_Wdq);


/** Opcode 0x0f 0xef. */
FNIEMOP_DEF(iemOp_pxor_Pq_Qq__pxor_Vdq_Wdq)
{
    IEMOP_MNEMONIC("pxor");
    return FNIEMOP_CALL_1(iemOpCommonMmxSse2_FullFull_To_Full, &g_iemAImpl_pxor);
}


/** Opcode 0x0f 0xf0. */
FNIEMOP_STUB(iemOp_lddqu_Vdq_Mdq);
/** Opcode 0x0f 0xf1. */
FNIEMOP_STUB(iemOp_psllw_Pq_Qq__pslw_Vdq_Wdq);
/** Opcode 0x0f 0xf2. */
FNIEMOP_STUB(iemOp_psld_Pq_Qq__pslld_Vdq_Wdq);
/** Opcode 0x0f 0xf3. */
FNIEMOP_STUB(iemOp_psllq_Pq_Qq__pslq_Vdq_Wdq);
/** Opcode 0x0f 0xf4. */
FNIEMOP_STUB(iemOp_pmuludq_Pq_Qq__pmuludq_Vdq_Wdq);
/** Opcode 0x0f 0xf5. */
FNIEMOP_STUB(iemOp_pmaddwd_Pq_Qq__pmaddwd_Vdq_Wdq);
/** Opcode 0x0f 0xf6. */
FNIEMOP_STUB(iemOp_psadbw_Pq_Qq__psadbw_Vdq_Wdq);
/** Opcode 0x0f 0xf7. */
FNIEMOP_STUB(iemOp_maskmovq_Pq_Nq__maskmovdqu_Vdq_Udq);
/** Opcode 0x0f 0xf8. */
FNIEMOP_STUB(iemOp_psubb_Pq_Qq_psubb_Vdq_Wdq); //NEXT
/** Opcode 0x0f 0xf9. */
FNIEMOP_STUB(iemOp_psubw_Pq_Qq__psubw_Vdq_Wdq);
/** Opcode 0x0f 0xfa. */
FNIEMOP_STUB(iemOp_psubd_Pq_Qq__psubd_Vdq_Wdq);
/** Opcode 0x0f 0xfb. */
FNIEMOP_STUB(iemOp_psubq_Pq_Qq__psbuq_Vdq_Wdq);
/** Opcode 0x0f 0xfc. */
FNIEMOP_STUB(iemOp_paddb_Pq_Qq__paddb_Vdq_Wdq);
/** Opcode 0x0f 0xfd. */
FNIEMOP_STUB(iemOp_paddw_Pq_Qq__paddw_Vdq_Wdq);
/** Opcode 0x0f 0xfe. */
FNIEMOP_STUB(iemOp_paddd_Pq_Qq__paddd_Vdq_Wdq);


IEM_STATIC const PFNIEMOP g_apfnTwoByteMap[256] =
{
    /* 0x00 */  iemOp_Grp6,
    /* 0x01 */  iemOp_Grp7,
    /* 0x02 */  iemOp_lar_Gv_Ew,
    /* 0x03 */  iemOp_lsl_Gv_Ew,
    /* 0x04 */  iemOp_Invalid,
    /* 0x05 */  iemOp_syscall,
    /* 0x06 */  iemOp_clts,
    /* 0x07 */  iemOp_sysret,
    /* 0x08 */  iemOp_invd,
    /* 0x09 */  iemOp_wbinvd,
    /* 0x0a */  iemOp_Invalid,
    /* 0x0b */  iemOp_ud2,
    /* 0x0c */  iemOp_Invalid,
    /* 0x0d */  iemOp_nop_Ev_GrpP,
    /* 0x0e */  iemOp_femms,
    /* 0x0f */  iemOp_3Dnow,
    /* 0x10 */  iemOp_movups_Vps_Wps__movupd_Vpd_Wpd__movss_Vss_Wss__movsd_Vsd_Wsd,
    /* 0x11 */  iemOp_movups_Wps_Vps__movupd_Wpd_Vpd__movss_Wss_Vss__movsd_Vsd_Wsd,
    /* 0x12 */  iemOp_movlps_Vq_Mq__movhlps_Vq_Uq__movlpd_Vq_Mq__movsldup_Vq_Wq__movddup_Vq_Wq,
    /* 0x13 */  iemOp_movlps_Mq_Vq__movlpd_Mq_Vq,
    /* 0x14 */  iemOp_unpckhlps_Vps_Wq__unpcklpd_Vpd_Wq,
    /* 0x15 */  iemOp_unpckhps_Vps_Wq__unpckhpd_Vpd_Wq,
    /* 0x16 */  iemOp_movhps_Vq_Mq__movlhps_Vq_Uq__movhpd_Vq_Mq__movshdup_Vq_Wq,
    /* 0x17 */  iemOp_movhps_Mq_Vq__movhpd_Mq_Vq,
    /* 0x18 */  iemOp_prefetch_Grp16,
    /* 0x19 */  iemOp_nop_Ev,
    /* 0x1a */  iemOp_nop_Ev,
    /* 0x1b */  iemOp_nop_Ev,
    /* 0x1c */  iemOp_nop_Ev,
    /* 0x1d */  iemOp_nop_Ev,
    /* 0x1e */  iemOp_nop_Ev,
    /* 0x1f */  iemOp_nop_Ev,
    /* 0x20 */  iemOp_mov_Rd_Cd,
    /* 0x21 */  iemOp_mov_Rd_Dd,
    /* 0x22 */  iemOp_mov_Cd_Rd,
    /* 0x23 */  iemOp_mov_Dd_Rd,
    /* 0x24 */  iemOp_mov_Rd_Td,
    /* 0x25 */  iemOp_Invalid,
    /* 0x26 */  iemOp_mov_Td_Rd,
    /* 0x27 */  iemOp_Invalid,
    /* 0x28 */  iemOp_movaps_Vps_Wps__movapd_Vpd_Wpd,
    /* 0x29 */  iemOp_movaps_Wps_Vps__movapd_Wpd_Vpd,
    /* 0x2a */  iemOp_cvtpi2ps_Vps_Qpi__cvtpi2pd_Vpd_Qpi__cvtsi2ss_Vss_Ey__cvtsi2sd_Vsd_Ey,
    /* 0x2b */  iemOp_movntps_Mps_Vps__movntpd_Mpd_Vpd,
    /* 0x2c */  iemOp_cvttps2pi_Ppi_Wps__cvttpd2pi_Ppi_Wpd__cvttss2si_Gy_Wss__cvttsd2si_Yu_Wsd,
    /* 0x2d */  iemOp_cvtps2pi_Ppi_Wps__cvtpd2pi_QpiWpd__cvtss2si_Gy_Wss__cvtsd2si_Gy_Wsd,
    /* 0x2e */  iemOp_ucomiss_Vss_Wss__ucomisd_Vsd_Wsd,
    /* 0x2f */  iemOp_comiss_Vss_Wss__comisd_Vsd_Wsd,
    /* 0x30 */  iemOp_wrmsr,
    /* 0x31 */  iemOp_rdtsc,
    /* 0x32 */  iemOp_rdmsr,
    /* 0x33 */  iemOp_rdpmc,
    /* 0x34 */  iemOp_sysenter,
    /* 0x35 */  iemOp_sysexit,
    /* 0x36 */  iemOp_Invalid,
    /* 0x37 */  iemOp_getsec,
    /* 0x38 */  iemOp_3byte_Esc_A4,
    /* 0x39 */  iemOp_Invalid,
    /* 0x3a */  iemOp_3byte_Esc_A5,
    /* 0x3b */  iemOp_Invalid,
    /* 0x3c */  iemOp_Invalid,
    /* 0x3d */  iemOp_Invalid,
    /* 0x3e */  iemOp_Invalid,
    /* 0x3f */  iemOp_Invalid,
    /* 0x40 */  iemOp_cmovo_Gv_Ev,
    /* 0x41 */  iemOp_cmovno_Gv_Ev,
    /* 0x42 */  iemOp_cmovc_Gv_Ev,
    /* 0x43 */  iemOp_cmovnc_Gv_Ev,
    /* 0x44 */  iemOp_cmove_Gv_Ev,
    /* 0x45 */  iemOp_cmovne_Gv_Ev,
    /* 0x46 */  iemOp_cmovbe_Gv_Ev,
    /* 0x47 */  iemOp_cmovnbe_Gv_Ev,
    /* 0x48 */  iemOp_cmovs_Gv_Ev,
    /* 0x49 */  iemOp_cmovns_Gv_Ev,
    /* 0x4a */  iemOp_cmovp_Gv_Ev,
    /* 0x4b */  iemOp_cmovnp_Gv_Ev,
    /* 0x4c */  iemOp_cmovl_Gv_Ev,
    /* 0x4d */  iemOp_cmovnl_Gv_Ev,
    /* 0x4e */  iemOp_cmovle_Gv_Ev,
    /* 0x4f */  iemOp_cmovnle_Gv_Ev,
    /* 0x50 */  iemOp_movmskps_Gy_Ups__movmskpd_Gy_Upd,
    /* 0x51 */  iemOp_sqrtps_Wps_Vps__sqrtpd_Wpd_Vpd__sqrtss_Vss_Wss__sqrtsd_Vsd_Wsd,
    /* 0x52 */  iemOp_rsqrtps_Wps_Vps__rsqrtss_Vss_Wss,
    /* 0x53 */  iemOp_rcpps_Wps_Vps__rcpss_Vs_Wss,
    /* 0x54 */  iemOp_andps_Vps_Wps__andpd_Wpd_Vpd,
    /* 0x55 */  iemOp_andnps_Vps_Wps__andnpd_Wpd_Vpd,
    /* 0x56 */  iemOp_orps_Wpd_Vpd__orpd_Wpd_Vpd,
    /* 0x57 */  iemOp_xorps_Vps_Wps__xorpd_Wpd_Vpd,
    /* 0x58 */  iemOp_addps_Vps_Wps__addpd_Vpd_Wpd__addss_Vss_Wss__addsd_Vsd_Wsd,
    /* 0x59 */  iemOp_mulps_Vps_Wps__mulpd_Vpd_Wpd__mulss_Vss__Wss__mulsd_Vsd_Wsd,
    /* 0x5a */  iemOp_cvtps2pd_Vpd_Wps__cvtpd2ps_Vps_Wpd__cvtss2sd_Vsd_Wss__cvtsd2ss_Vss_Wsd,
    /* 0x5b */  iemOp_cvtdq2ps_Vps_Wdq__cvtps2dq_Vdq_Wps__cvtps2dq_Vdq_Wps,
    /* 0x5c */  iemOp_subps_Vps_Wps__subpd_Vps_Wdp__subss_Vss_Wss__subsd_Vsd_Wsd,
    /* 0x5d */  iemOp_minps_Vps_Wps__minpd_Vpd_Wpd__minss_Vss_Wss__minsd_Vsd_Wsd,
    /* 0x5e */  iemOp_divps_Vps_Wps__divpd_Vpd_Wpd__divss_Vss_Wss__divsd_Vsd_Wsd,
    /* 0x5f */  iemOp_maxps_Vps_Wps__maxpd_Vpd_Wpd__maxss_Vss_Wss__maxsd_Vsd_Wsd,
    /* 0x60 */  iemOp_punpcklbw_Pq_Qd__punpcklbw_Vdq_Wdq,
    /* 0x61 */  iemOp_punpcklwd_Pq_Qd__punpcklwd_Vdq_Wdq,
    /* 0x62 */  iemOp_punpckldq_Pq_Qd__punpckldq_Vdq_Wdq,
    /* 0x63 */  iemOp_packsswb_Pq_Qq__packsswb_Vdq_Wdq,
    /* 0x64 */  iemOp_pcmpgtb_Pq_Qq__pcmpgtb_Vdq_Wdq,
    /* 0x65 */  iemOp_pcmpgtw_Pq_Qq__pcmpgtw_Vdq_Wdq,
    /* 0x66 */  iemOp_pcmpgtd_Pq_Qq__pcmpgtd_Vdq_Wdq,
    /* 0x67 */  iemOp_packuswb_Pq_Qq__packuswb_Vdq_Wdq,
    /* 0x68 */  iemOp_punpckhbw_Pq_Qq__punpckhbw_Vdq_Wdq,
    /* 0x69 */  iemOp_punpckhwd_Pq_Qd__punpckhwd_Vdq_Wdq,
    /* 0x6a */  iemOp_punpckhdq_Pq_Qd__punpckhdq_Vdq_Wdq,
    /* 0x6b */  iemOp_packssdw_Pq_Qd__packssdq_Vdq_Wdq,
    /* 0x6c */  iemOp_punpcklqdq_Vdq_Wdq,
    /* 0x6d */  iemOp_punpckhqdq_Vdq_Wdq,
    /* 0x6e */  iemOp_movd_q_Pd_Ey__movd_q_Vy_Ey,
    /* 0x6f */  iemOp_movq_Pq_Qq__movdqa_Vdq_Wdq__movdqu_Vdq_Wdq,
    /* 0x70 */  iemOp_pshufw_Pq_Qq_Ib__pshufd_Vdq_Wdq_Ib__pshufhw_Vdq_Wdq_Ib__pshuflq_Vdq_Wdq_Ib,
    /* 0x71 */  iemOp_Grp12,
    /* 0x72 */  iemOp_Grp13,
    /* 0x73 */  iemOp_Grp14,
    /* 0x74 */  iemOp_pcmpeqb_Pq_Qq__pcmpeqb_Vdq_Wdq,
    /* 0x75 */  iemOp_pcmpeqw_Pq_Qq__pcmpeqw_Vdq_Wdq,
    /* 0x76 */  iemOp_pcmped_Pq_Qq__pcmpeqd_Vdq_Wdq,
    /* 0x77 */  iemOp_emms,
    /* 0x78 */  iemOp_vmread_AmdGrp17,
    /* 0x79 */  iemOp_vmwrite,
    /* 0x7a */  iemOp_Invalid,
    /* 0x7b */  iemOp_Invalid,
    /* 0x7c */  iemOp_haddpd_Vdp_Wpd__haddps_Vps_Wps,
    /* 0x7d */  iemOp_hsubpd_Vpd_Wpd__hsubps_Vps_Wps,
    /* 0x7e */  iemOp_movd_q_Ey_Pd__movd_q_Ey_Vy__movq_Vq_Wq,
    /* 0x7f */  iemOp_movq_Qq_Pq__movq_movdqa_Wdq_Vdq__movdqu_Wdq_Vdq,
    /* 0x80 */  iemOp_jo_Jv,
    /* 0x81 */  iemOp_jno_Jv,
    /* 0x82 */  iemOp_jc_Jv,
    /* 0x83 */  iemOp_jnc_Jv,
    /* 0x84 */  iemOp_je_Jv,
    /* 0x85 */  iemOp_jne_Jv,
    /* 0x86 */  iemOp_jbe_Jv,
    /* 0x87 */  iemOp_jnbe_Jv,
    /* 0x88 */  iemOp_js_Jv,
    /* 0x89 */  iemOp_jns_Jv,
    /* 0x8a */  iemOp_jp_Jv,
    /* 0x8b */  iemOp_jnp_Jv,
    /* 0x8c */  iemOp_jl_Jv,
    /* 0x8d */  iemOp_jnl_Jv,
    /* 0x8e */  iemOp_jle_Jv,
    /* 0x8f */  iemOp_jnle_Jv,
    /* 0x90 */  iemOp_seto_Eb,
    /* 0x91 */  iemOp_setno_Eb,
    /* 0x92 */  iemOp_setc_Eb,
    /* 0x93 */  iemOp_setnc_Eb,
    /* 0x94 */  iemOp_sete_Eb,
    /* 0x95 */  iemOp_setne_Eb,
    /* 0x96 */  iemOp_setbe_Eb,
    /* 0x97 */  iemOp_setnbe_Eb,
    /* 0x98 */  iemOp_sets_Eb,
    /* 0x99 */  iemOp_setns_Eb,
    /* 0x9a */  iemOp_setp_Eb,
    /* 0x9b */  iemOp_setnp_Eb,
    /* 0x9c */  iemOp_setl_Eb,
    /* 0x9d */  iemOp_setnl_Eb,
    /* 0x9e */  iemOp_setle_Eb,
    /* 0x9f */  iemOp_setnle_Eb,
    /* 0xa0 */  iemOp_push_fs,
    /* 0xa1 */  iemOp_pop_fs,
    /* 0xa2 */  iemOp_cpuid,
    /* 0xa3 */  iemOp_bt_Ev_Gv,
    /* 0xa4 */  iemOp_shld_Ev_Gv_Ib,
    /* 0xa5 */  iemOp_shld_Ev_Gv_CL,
    /* 0xa6 */  iemOp_Invalid,
    /* 0xa7 */  iemOp_Invalid,
    /* 0xa8 */  iemOp_push_gs,
    /* 0xa9 */  iemOp_pop_gs,
    /* 0xaa */  iemOp_rsm,
    /* 0xab */  iemOp_bts_Ev_Gv,
    /* 0xac */  iemOp_shrd_Ev_Gv_Ib,
    /* 0xad */  iemOp_shrd_Ev_Gv_CL,
    /* 0xae */  iemOp_Grp15,
    /* 0xaf */  iemOp_imul_Gv_Ev,
    /* 0xb0 */  iemOp_cmpxchg_Eb_Gb,
    /* 0xb1 */  iemOp_cmpxchg_Ev_Gv,
    /* 0xb2 */  iemOp_lss_Gv_Mp,
    /* 0xb3 */  iemOp_btr_Ev_Gv,
    /* 0xb4 */  iemOp_lfs_Gv_Mp,
    /* 0xb5 */  iemOp_lgs_Gv_Mp,
    /* 0xb6 */  iemOp_movzx_Gv_Eb,
    /* 0xb7 */  iemOp_movzx_Gv_Ew,
    /* 0xb8 */  iemOp_popcnt_Gv_Ev_jmpe,
    /* 0xb9 */  iemOp_Grp10,
    /* 0xba */  iemOp_Grp8,
    /* 0xbd */  iemOp_btc_Ev_Gv,
    /* 0xbc */  iemOp_bsf_Gv_Ev,
    /* 0xbd */  iemOp_bsr_Gv_Ev,
    /* 0xbe */  iemOp_movsx_Gv_Eb,
    /* 0xbf */  iemOp_movsx_Gv_Ew,
    /* 0xc0 */  iemOp_xadd_Eb_Gb,
    /* 0xc1 */  iemOp_xadd_Ev_Gv,
    /* 0xc2 */  iemOp_cmpps_Vps_Wps_Ib__cmppd_Vpd_Wpd_Ib__cmpss_Vss_Wss_Ib__cmpsd_Vsd_Wsd_Ib,
    /* 0xc3 */  iemOp_movnti_My_Gy,
    /* 0xc4 */  iemOp_pinsrw_Pq_Ry_Mw_Ib__pinsrw_Vdq_Ry_Mw_Ib,
    /* 0xc5 */  iemOp_pextrw_Gd_Nq_Ib__pextrw_Gd_Udq_Ib,
    /* 0xc6 */  iemOp_shufps_Vps_Wps_Ib__shufdp_Vpd_Wpd_Ib,
    /* 0xc7 */  iemOp_Grp9,
    /* 0xc8 */  iemOp_bswap_rAX_r8,
    /* 0xc9 */  iemOp_bswap_rCX_r9,
    /* 0xca */  iemOp_bswap_rDX_r10,
    /* 0xcb */  iemOp_bswap_rBX_r11,
    /* 0xcc */  iemOp_bswap_rSP_r12,
    /* 0xcd */  iemOp_bswap_rBP_r13,
    /* 0xce */  iemOp_bswap_rSI_r14,
    /* 0xcf */  iemOp_bswap_rDI_r15,
    /* 0xd0 */  iemOp_addsubpd_Vpd_Wpd__addsubps_Vps_Wps,
    /* 0xd1 */  iemOp_psrlw_Pp_Qp__psrlw_Vdp_Wdq,
    /* 0xd2 */  iemOp_psrld_Pq_Qq__psrld_Vdq_Wdq,
    /* 0xd3 */  iemOp_psrlq_Pq_Qq__psrlq_Vdq_Wdq,
    /* 0xd4 */  iemOp_paddq_Pq_Qq__paddq_Vdq_Wdq,
    /* 0xd5 */  iemOp_pmulq_Pq_Qq__pmullw_Vdq_Wdq,
    /* 0xd6 */  iemOp_movq_Wq_Vq__movq2dq_Vdq_Nq__movdq2q_Pq_Uq,
    /* 0xd7 */  iemOp_pmovmskb_Gd_Nq__pmovmskb_Gd_Udq,
    /* 0xd8 */  iemOp_psubusb_Pq_Qq__psubusb_Vdq_Wdq,
    /* 0xd9 */  iemOp_psubusw_Pq_Qq__psubusw_Vdq_Wdq,
    /* 0xda */  iemOp_pminub_Pq_Qq__pminub_Vdq_Wdq,
    /* 0xdb */  iemOp_pand_Pq_Qq__pand_Vdq_Wdq,
    /* 0xdc */  iemOp_paddusb_Pq_Qq__paddusb_Vdq_Wdq,
    /* 0xdd */  iemOp_paddusw_Pq_Qq__paddusw_Vdq_Wdq,
    /* 0xde */  iemOp_pmaxub_Pq_Qq__pamxub_Vdq_Wdq,
    /* 0xdf */  iemOp_pandn_Pq_Qq__pandn_Vdq_Wdq,
    /* 0xe0 */  iemOp_pavgb_Pq_Qq__pavgb_Vdq_Wdq,
    /* 0xe1 */  iemOp_psraw_Pq_Qq__psraw_Vdq_Wdq,
    /* 0xe2 */  iemOp_psrad_Pq_Qq__psrad_Vdq_Wdq,
    /* 0xe3 */  iemOp_pavgw_Pq_Qq__pavgw_Vdq_Wdq,
    /* 0xe4 */  iemOp_pmulhuw_Pq_Qq__pmulhuw_Vdq_Wdq,
    /* 0xe5 */  iemOp_pmulhw_Pq_Qq__pmulhw_Vdq_Wdq,
    /* 0xe6 */  iemOp_cvttpd2dq_Vdq_Wdp__cvtdq2pd_Vdq_Wpd__cvtpd2dq_Vdq_Wpd,
    /* 0xe7 */  iemOp_movntq_Mq_Pq__movntdq_Mdq_Vdq,
    /* 0xe8 */  iemOp_psubsb_Pq_Qq__psubsb_Vdq_Wdq,
    /* 0xe9 */  iemOp_psubsw_Pq_Qq__psubsw_Vdq_Wdq,
    /* 0xea */  iemOp_pminsw_Pq_Qq__pminsw_Vdq_Wdq,
    /* 0xeb */  iemOp_por_Pq_Qq__por_Vdq_Wdq,
    /* 0xec */  iemOp_paddsb_Pq_Qq__paddsb_Vdq_Wdq,
    /* 0xed */  iemOp_paddsw_Pq_Qq__paddsw_Vdq_Wdq,
    /* 0xee */  iemOp_pmaxsw_Pq_Qq__pmaxsw_Vdq_Wdq,
    /* 0xef */  iemOp_pxor_Pq_Qq__pxor_Vdq_Wdq,
    /* 0xf0 */  iemOp_lddqu_Vdq_Mdq,
    /* 0xf1 */  iemOp_psllw_Pq_Qq__pslw_Vdq_Wdq,
    /* 0xf2 */  iemOp_psld_Pq_Qq__pslld_Vdq_Wdq,
    /* 0xf3 */  iemOp_psllq_Pq_Qq__pslq_Vdq_Wdq,
    /* 0xf4 */  iemOp_pmuludq_Pq_Qq__pmuludq_Vdq_Wdq,
    /* 0xf5 */  iemOp_pmaddwd_Pq_Qq__pmaddwd_Vdq_Wdq,
    /* 0xf6 */  iemOp_psadbw_Pq_Qq__psadbw_Vdq_Wdq,
    /* 0xf7 */  iemOp_maskmovq_Pq_Nq__maskmovdqu_Vdq_Udq,
    /* 0xf8 */  iemOp_psubb_Pq_Qq_psubb_Vdq_Wdq,
    /* 0xf9 */  iemOp_psubw_Pq_Qq__psubw_Vdq_Wdq,
    /* 0xfa */  iemOp_psubd_Pq_Qq__psubd_Vdq_Wdq,
    /* 0xfb */  iemOp_psubq_Pq_Qq__psbuq_Vdq_Wdq,
    /* 0xfc */  iemOp_paddb_Pq_Qq__paddb_Vdq_Wdq,
    /* 0xfd */  iemOp_paddw_Pq_Qq__paddw_Vdq_Wdq,
    /* 0xfe */  iemOp_paddd_Pq_Qq__paddd_Vdq_Wdq,
    /* 0xff */  iemOp_Invalid
};

/** @}  */


/** @name One byte opcodes.
 *
 * @{
 */

/** Opcode 0x00. */
FNIEMOP_DEF(iemOp_add_Eb_Gb)
{
    IEMOP_MNEMONIC("add Eb,Gb");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rm_r8, &g_iemAImpl_add);
}


/** Opcode 0x01. */
FNIEMOP_DEF(iemOp_add_Ev_Gv)
{
    IEMOP_MNEMONIC("add Ev,Gv");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rm_rv, &g_iemAImpl_add);
}


/** Opcode 0x02. */
FNIEMOP_DEF(iemOp_add_Gb_Eb)
{
    IEMOP_MNEMONIC("add Gb,Eb");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_r8_rm, &g_iemAImpl_add);
}


/** Opcode 0x03. */
FNIEMOP_DEF(iemOp_add_Gv_Ev)
{
    IEMOP_MNEMONIC("add Gv,Ev");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rv_rm, &g_iemAImpl_add);
}


/** Opcode 0x04. */
FNIEMOP_DEF(iemOp_add_Al_Ib)
{
    IEMOP_MNEMONIC("add al,Ib");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_AL_Ib, &g_iemAImpl_add);
}


/** Opcode 0x05. */
FNIEMOP_DEF(iemOp_add_eAX_Iz)
{
    IEMOP_MNEMONIC("add rAX,Iz");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rAX_Iz, &g_iemAImpl_add);
}


/** Opcode 0x06. */
FNIEMOP_DEF(iemOp_push_ES)
{
    IEMOP_MNEMONIC("push es");
    return FNIEMOP_CALL_1(iemOpCommonPushSReg, X86_SREG_ES);
}


/** Opcode 0x07. */
FNIEMOP_DEF(iemOp_pop_ES)
{
    IEMOP_MNEMONIC("pop es");
    IEMOP_HLP_NO_64BIT();
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_pop_Sreg, X86_SREG_ES, pVCpu->iem.s.enmEffOpSize);
}


/** Opcode 0x08. */
FNIEMOP_DEF(iemOp_or_Eb_Gb)
{
    IEMOP_MNEMONIC("or  Eb,Gb");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rm_r8, &g_iemAImpl_or);
}


/** Opcode 0x09. */
FNIEMOP_DEF(iemOp_or_Ev_Gv)
{
    IEMOP_MNEMONIC("or  Ev,Gv ");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rm_rv, &g_iemAImpl_or);
}


/** Opcode 0x0a. */
FNIEMOP_DEF(iemOp_or_Gb_Eb)
{
    IEMOP_MNEMONIC("or  Gb,Eb");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_r8_rm, &g_iemAImpl_or);
}


/** Opcode 0x0b. */
FNIEMOP_DEF(iemOp_or_Gv_Ev)
{
    IEMOP_MNEMONIC("or  Gv,Ev");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rv_rm, &g_iemAImpl_or);
}


/** Opcode 0x0c. */
FNIEMOP_DEF(iemOp_or_Al_Ib)
{
    IEMOP_MNEMONIC("or  al,Ib");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_AL_Ib, &g_iemAImpl_or);
}


/** Opcode 0x0d. */
FNIEMOP_DEF(iemOp_or_eAX_Iz)
{
    IEMOP_MNEMONIC("or  rAX,Iz");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rAX_Iz, &g_iemAImpl_or);
}


/** Opcode 0x0e. */
FNIEMOP_DEF(iemOp_push_CS)
{
    IEMOP_MNEMONIC("push cs");
    return FNIEMOP_CALL_1(iemOpCommonPushSReg, X86_SREG_CS);
}


/** Opcode 0x0f. */
FNIEMOP_DEF(iemOp_2byteEscape)
{
    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    /** @todo PUSH CS on 8086, undefined on 80186. */
    IEMOP_HLP_MIN_286();
    return FNIEMOP_CALL(g_apfnTwoByteMap[b]);
}

/** Opcode 0x10. */
FNIEMOP_DEF(iemOp_adc_Eb_Gb)
{
    IEMOP_MNEMONIC("adc Eb,Gb");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rm_r8, &g_iemAImpl_adc);
}


/** Opcode 0x11. */
FNIEMOP_DEF(iemOp_adc_Ev_Gv)
{
    IEMOP_MNEMONIC("adc Ev,Gv");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rm_rv, &g_iemAImpl_adc);
}


/** Opcode 0x12. */
FNIEMOP_DEF(iemOp_adc_Gb_Eb)
{
    IEMOP_MNEMONIC("adc Gb,Eb");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_r8_rm, &g_iemAImpl_adc);
}


/** Opcode 0x13. */
FNIEMOP_DEF(iemOp_adc_Gv_Ev)
{
    IEMOP_MNEMONIC("adc Gv,Ev");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rv_rm, &g_iemAImpl_adc);
}


/** Opcode 0x14. */
FNIEMOP_DEF(iemOp_adc_Al_Ib)
{
    IEMOP_MNEMONIC("adc al,Ib");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_AL_Ib, &g_iemAImpl_adc);
}


/** Opcode 0x15. */
FNIEMOP_DEF(iemOp_adc_eAX_Iz)
{
    IEMOP_MNEMONIC("adc rAX,Iz");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rAX_Iz, &g_iemAImpl_adc);
}


/** Opcode 0x16. */
FNIEMOP_DEF(iemOp_push_SS)
{
    IEMOP_MNEMONIC("push ss");
    return FNIEMOP_CALL_1(iemOpCommonPushSReg, X86_SREG_SS);
}


/** Opcode 0x17. */
FNIEMOP_DEF(iemOp_pop_SS)
{
    IEMOP_MNEMONIC("pop ss"); /** @todo implies instruction fusing? */
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_NO_64BIT();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_pop_Sreg, X86_SREG_SS, pVCpu->iem.s.enmEffOpSize);
}


/** Opcode 0x18. */
FNIEMOP_DEF(iemOp_sbb_Eb_Gb)
{
    IEMOP_MNEMONIC("sbb Eb,Gb");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rm_r8, &g_iemAImpl_sbb);
}


/** Opcode 0x19. */
FNIEMOP_DEF(iemOp_sbb_Ev_Gv)
{
    IEMOP_MNEMONIC("sbb Ev,Gv");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rm_rv, &g_iemAImpl_sbb);
}


/** Opcode 0x1a. */
FNIEMOP_DEF(iemOp_sbb_Gb_Eb)
{
    IEMOP_MNEMONIC("sbb Gb,Eb");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_r8_rm, &g_iemAImpl_sbb);
}


/** Opcode 0x1b. */
FNIEMOP_DEF(iemOp_sbb_Gv_Ev)
{
    IEMOP_MNEMONIC("sbb Gv,Ev");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rv_rm, &g_iemAImpl_sbb);
}


/** Opcode 0x1c. */
FNIEMOP_DEF(iemOp_sbb_Al_Ib)
{
    IEMOP_MNEMONIC("sbb al,Ib");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_AL_Ib, &g_iemAImpl_sbb);
}


/** Opcode 0x1d. */
FNIEMOP_DEF(iemOp_sbb_eAX_Iz)
{
    IEMOP_MNEMONIC("sbb rAX,Iz");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rAX_Iz, &g_iemAImpl_sbb);
}


/** Opcode 0x1e. */
FNIEMOP_DEF(iemOp_push_DS)
{
    IEMOP_MNEMONIC("push ds");
    return FNIEMOP_CALL_1(iemOpCommonPushSReg, X86_SREG_DS);
}


/** Opcode 0x1f. */
FNIEMOP_DEF(iemOp_pop_DS)
{
    IEMOP_MNEMONIC("pop ds");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_NO_64BIT();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_pop_Sreg, X86_SREG_DS, pVCpu->iem.s.enmEffOpSize);
}


/** Opcode 0x20. */
FNIEMOP_DEF(iemOp_and_Eb_Gb)
{
    IEMOP_MNEMONIC("and Eb,Gb");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rm_r8, &g_iemAImpl_and);
}


/** Opcode 0x21. */
FNIEMOP_DEF(iemOp_and_Ev_Gv)
{
    IEMOP_MNEMONIC("and Ev,Gv");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rm_rv, &g_iemAImpl_and);
}


/** Opcode 0x22. */
FNIEMOP_DEF(iemOp_and_Gb_Eb)
{
    IEMOP_MNEMONIC("and Gb,Eb");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_r8_rm, &g_iemAImpl_and);
}


/** Opcode 0x23. */
FNIEMOP_DEF(iemOp_and_Gv_Ev)
{
    IEMOP_MNEMONIC("and Gv,Ev");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rv_rm, &g_iemAImpl_and);
}


/** Opcode 0x24. */
FNIEMOP_DEF(iemOp_and_Al_Ib)
{
    IEMOP_MNEMONIC("and al,Ib");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_AL_Ib, &g_iemAImpl_and);
}


/** Opcode 0x25. */
FNIEMOP_DEF(iemOp_and_eAX_Iz)
{
    IEMOP_MNEMONIC("and rAX,Iz");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rAX_Iz, &g_iemAImpl_and);
}


/** Opcode 0x26. */
FNIEMOP_DEF(iemOp_seg_ES)
{
    IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("seg es");
    pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_SEG_ES;
    pVCpu->iem.s.iEffSeg    = X86_SREG_ES;

    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    return FNIEMOP_CALL(g_apfnOneByteMap[b]);
}


/** Opcode 0x27. */
FNIEMOP_DEF(iemOp_daa)
{
    IEMOP_MNEMONIC("daa AL");
    IEMOP_HLP_NO_64BIT();
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_OF);
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_daa);
}


/** Opcode 0x28. */
FNIEMOP_DEF(iemOp_sub_Eb_Gb)
{
    IEMOP_MNEMONIC("sub Eb,Gb");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rm_r8, &g_iemAImpl_sub);
}


/** Opcode 0x29. */
FNIEMOP_DEF(iemOp_sub_Ev_Gv)
{
    IEMOP_MNEMONIC("sub Ev,Gv");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rm_rv, &g_iemAImpl_sub);
}


/** Opcode 0x2a. */
FNIEMOP_DEF(iemOp_sub_Gb_Eb)
{
    IEMOP_MNEMONIC("sub Gb,Eb");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_r8_rm, &g_iemAImpl_sub);
}


/** Opcode 0x2b. */
FNIEMOP_DEF(iemOp_sub_Gv_Ev)
{
    IEMOP_MNEMONIC("sub Gv,Ev");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rv_rm, &g_iemAImpl_sub);
}


/** Opcode 0x2c. */
FNIEMOP_DEF(iemOp_sub_Al_Ib)
{
    IEMOP_MNEMONIC("sub al,Ib");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_AL_Ib, &g_iemAImpl_sub);
}


/** Opcode 0x2d. */
FNIEMOP_DEF(iemOp_sub_eAX_Iz)
{
    IEMOP_MNEMONIC("sub rAX,Iz");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rAX_Iz, &g_iemAImpl_sub);
}


/** Opcode 0x2e. */
FNIEMOP_DEF(iemOp_seg_CS)
{
    IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("seg cs");
    pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_SEG_CS;
    pVCpu->iem.s.iEffSeg    = X86_SREG_CS;

    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    return FNIEMOP_CALL(g_apfnOneByteMap[b]);
}


/** Opcode 0x2f. */
FNIEMOP_DEF(iemOp_das)
{
    IEMOP_MNEMONIC("das AL");
    IEMOP_HLP_NO_64BIT();
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_OF);
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_das);
}


/** Opcode 0x30. */
FNIEMOP_DEF(iemOp_xor_Eb_Gb)
{
    IEMOP_MNEMONIC("xor Eb,Gb");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rm_r8, &g_iemAImpl_xor);
}


/** Opcode 0x31. */
FNIEMOP_DEF(iemOp_xor_Ev_Gv)
{
    IEMOP_MNEMONIC("xor Ev,Gv");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rm_rv, &g_iemAImpl_xor);
}


/** Opcode 0x32. */
FNIEMOP_DEF(iemOp_xor_Gb_Eb)
{
    IEMOP_MNEMONIC("xor Gb,Eb");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_r8_rm, &g_iemAImpl_xor);
}


/** Opcode 0x33. */
FNIEMOP_DEF(iemOp_xor_Gv_Ev)
{
    IEMOP_MNEMONIC("xor Gv,Ev");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rv_rm, &g_iemAImpl_xor);
}


/** Opcode 0x34. */
FNIEMOP_DEF(iemOp_xor_Al_Ib)
{
    IEMOP_MNEMONIC("xor al,Ib");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_AL_Ib, &g_iemAImpl_xor);
}


/** Opcode 0x35. */
FNIEMOP_DEF(iemOp_xor_eAX_Iz)
{
    IEMOP_MNEMONIC("xor rAX,Iz");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rAX_Iz, &g_iemAImpl_xor);
}


/** Opcode 0x36. */
FNIEMOP_DEF(iemOp_seg_SS)
{
    IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("seg ss");
    pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_SEG_SS;
    pVCpu->iem.s.iEffSeg    = X86_SREG_SS;

    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    return FNIEMOP_CALL(g_apfnOneByteMap[b]);
}


/** Opcode 0x37. */
FNIEMOP_STUB(iemOp_aaa);


/** Opcode 0x38. */
FNIEMOP_DEF(iemOp_cmp_Eb_Gb)
{
    IEMOP_MNEMONIC("cmp Eb,Gb");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rm_r8, &g_iemAImpl_cmp);
}


/** Opcode 0x39. */
FNIEMOP_DEF(iemOp_cmp_Ev_Gv)
{
    IEMOP_MNEMONIC("cmp Ev,Gv");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rm_rv, &g_iemAImpl_cmp);
}


/** Opcode 0x3a. */
FNIEMOP_DEF(iemOp_cmp_Gb_Eb)
{
    IEMOP_MNEMONIC("cmp Gb,Eb");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_r8_rm, &g_iemAImpl_cmp);
}


/** Opcode 0x3b. */
FNIEMOP_DEF(iemOp_cmp_Gv_Ev)
{
    IEMOP_MNEMONIC("cmp Gv,Ev");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rv_rm, &g_iemAImpl_cmp);
}


/** Opcode 0x3c. */
FNIEMOP_DEF(iemOp_cmp_Al_Ib)
{
    IEMOP_MNEMONIC("cmp al,Ib");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_AL_Ib, &g_iemAImpl_cmp);
}


/** Opcode 0x3d. */
FNIEMOP_DEF(iemOp_cmp_eAX_Iz)
{
    IEMOP_MNEMONIC("cmp rAX,Iz");
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rAX_Iz, &g_iemAImpl_cmp);
}


/** Opcode 0x3e. */
FNIEMOP_DEF(iemOp_seg_DS)
{
    IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("seg ds");
    pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_SEG_DS;
    pVCpu->iem.s.iEffSeg    = X86_SREG_DS;

    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    return FNIEMOP_CALL(g_apfnOneByteMap[b]);
}


/** Opcode 0x3f. */
FNIEMOP_STUB(iemOp_aas);

/**
 * Common 'inc/dec/not/neg register' helper.
 */
FNIEMOP_DEF_2(iemOpCommonUnaryGReg, PCIEMOPUNARYSIZES, pImpl, uint8_t, iReg)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(2, 0);
            IEM_MC_ARG(uint16_t *,  pu16Dst, 0);
            IEM_MC_ARG(uint32_t *,  pEFlags, 1);
            IEM_MC_REF_GREG_U16(pu16Dst, iReg);
            IEM_MC_REF_EFLAGS(pEFlags);
            IEM_MC_CALL_VOID_AIMPL_2(pImpl->pfnNormalU16, pu16Dst, pEFlags);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(2, 0);
            IEM_MC_ARG(uint32_t *,  pu32Dst, 0);
            IEM_MC_ARG(uint32_t *,  pEFlags, 1);
            IEM_MC_REF_GREG_U32(pu32Dst, iReg);
            IEM_MC_REF_EFLAGS(pEFlags);
            IEM_MC_CALL_VOID_AIMPL_2(pImpl->pfnNormalU32, pu32Dst, pEFlags);
            IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32Dst);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(2, 0);
            IEM_MC_ARG(uint64_t *,  pu64Dst, 0);
            IEM_MC_ARG(uint32_t *,  pEFlags, 1);
            IEM_MC_REF_GREG_U64(pu64Dst, iReg);
            IEM_MC_REF_EFLAGS(pEFlags);
            IEM_MC_CALL_VOID_AIMPL_2(pImpl->pfnNormalU64, pu64Dst, pEFlags);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;
    }
    return VINF_SUCCESS;
}


/** Opcode 0x40. */
FNIEMOP_DEF(iemOp_inc_eAX)
{
    /*
     * This is a REX prefix in 64-bit mode.
     */
    if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
    {
        IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("rex");
        pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_REX;

        uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
        return FNIEMOP_CALL(g_apfnOneByteMap[b]);
    }

    IEMOP_MNEMONIC("inc eAX");
    return FNIEMOP_CALL_2(iemOpCommonUnaryGReg, &g_iemAImpl_inc, X86_GREG_xAX);
}


/** Opcode 0x41. */
FNIEMOP_DEF(iemOp_inc_eCX)
{
    /*
     * This is a REX prefix in 64-bit mode.
     */
    if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
    {
        IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("rex.b");
        pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_B;
        pVCpu->iem.s.uRexB     = 1 << 3;

        uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
        return FNIEMOP_CALL(g_apfnOneByteMap[b]);
    }

    IEMOP_MNEMONIC("inc eCX");
    return FNIEMOP_CALL_2(iemOpCommonUnaryGReg, &g_iemAImpl_inc, X86_GREG_xCX);
}


/** Opcode 0x42. */
FNIEMOP_DEF(iemOp_inc_eDX)
{
    /*
     * This is a REX prefix in 64-bit mode.
     */
    if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
    {
        IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("rex.x");
        pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_X;
        pVCpu->iem.s.uRexIndex = 1 << 3;

        uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
        return FNIEMOP_CALL(g_apfnOneByteMap[b]);
    }

    IEMOP_MNEMONIC("inc eDX");
    return FNIEMOP_CALL_2(iemOpCommonUnaryGReg, &g_iemAImpl_inc, X86_GREG_xDX);
}



/** Opcode 0x43. */
FNIEMOP_DEF(iemOp_inc_eBX)
{
    /*
     * This is a REX prefix in 64-bit mode.
     */
    if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
    {
        IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("rex.bx");
        pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_B | IEM_OP_PRF_REX_X;
        pVCpu->iem.s.uRexB     = 1 << 3;
        pVCpu->iem.s.uRexIndex = 1 << 3;

        uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
        return FNIEMOP_CALL(g_apfnOneByteMap[b]);
    }

    IEMOP_MNEMONIC("inc eBX");
    return FNIEMOP_CALL_2(iemOpCommonUnaryGReg, &g_iemAImpl_inc, X86_GREG_xBX);
}


/** Opcode 0x44. */
FNIEMOP_DEF(iemOp_inc_eSP)
{
    /*
     * This is a REX prefix in 64-bit mode.
     */
    if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
    {
        IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("rex.r");
        pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_R;
        pVCpu->iem.s.uRexReg   = 1 << 3;

        uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
        return FNIEMOP_CALL(g_apfnOneByteMap[b]);
    }

    IEMOP_MNEMONIC("inc eSP");
    return FNIEMOP_CALL_2(iemOpCommonUnaryGReg, &g_iemAImpl_inc, X86_GREG_xSP);
}


/** Opcode 0x45. */
FNIEMOP_DEF(iemOp_inc_eBP)
{
    /*
     * This is a REX prefix in 64-bit mode.
     */
    if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
    {
        IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("rex.rb");
        pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_R | IEM_OP_PRF_REX_B;
        pVCpu->iem.s.uRexReg   = 1 << 3;
        pVCpu->iem.s.uRexB     = 1 << 3;

        uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
        return FNIEMOP_CALL(g_apfnOneByteMap[b]);
    }

    IEMOP_MNEMONIC("inc eBP");
    return FNIEMOP_CALL_2(iemOpCommonUnaryGReg, &g_iemAImpl_inc, X86_GREG_xBP);
}


/** Opcode 0x46. */
FNIEMOP_DEF(iemOp_inc_eSI)
{
    /*
     * This is a REX prefix in 64-bit mode.
     */
    if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
    {
        IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("rex.rx");
        pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_R | IEM_OP_PRF_REX_X;
        pVCpu->iem.s.uRexReg   = 1 << 3;
        pVCpu->iem.s.uRexIndex = 1 << 3;

        uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
        return FNIEMOP_CALL(g_apfnOneByteMap[b]);
    }

    IEMOP_MNEMONIC("inc eSI");
    return FNIEMOP_CALL_2(iemOpCommonUnaryGReg, &g_iemAImpl_inc, X86_GREG_xSI);
}


/** Opcode 0x47. */
FNIEMOP_DEF(iemOp_inc_eDI)
{
    /*
     * This is a REX prefix in 64-bit mode.
     */
    if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
    {
        IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("rex.rbx");
        pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_R | IEM_OP_PRF_REX_B | IEM_OP_PRF_REX_X;
        pVCpu->iem.s.uRexReg   = 1 << 3;
        pVCpu->iem.s.uRexB     = 1 << 3;
        pVCpu->iem.s.uRexIndex = 1 << 3;

        uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
        return FNIEMOP_CALL(g_apfnOneByteMap[b]);
    }

    IEMOP_MNEMONIC("inc eDI");
    return FNIEMOP_CALL_2(iemOpCommonUnaryGReg, &g_iemAImpl_inc, X86_GREG_xDI);
}


/** Opcode 0x48. */
FNIEMOP_DEF(iemOp_dec_eAX)
{
    /*
     * This is a REX prefix in 64-bit mode.
     */
    if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
    {
        IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("rex.w");
        pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_SIZE_REX_W;
        iemRecalEffOpSize(pVCpu);

        uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
        return FNIEMOP_CALL(g_apfnOneByteMap[b]);
    }

    IEMOP_MNEMONIC("dec eAX");
    return FNIEMOP_CALL_2(iemOpCommonUnaryGReg, &g_iemAImpl_dec, X86_GREG_xAX);
}


/** Opcode 0x49. */
FNIEMOP_DEF(iemOp_dec_eCX)
{
    /*
     * This is a REX prefix in 64-bit mode.
     */
    if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
    {
        IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("rex.bw");
        pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_B | IEM_OP_PRF_SIZE_REX_W;
        pVCpu->iem.s.uRexB     = 1 << 3;
        iemRecalEffOpSize(pVCpu);

        uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
        return FNIEMOP_CALL(g_apfnOneByteMap[b]);
    }

    IEMOP_MNEMONIC("dec eCX");
    return FNIEMOP_CALL_2(iemOpCommonUnaryGReg, &g_iemAImpl_dec, X86_GREG_xCX);
}


/** Opcode 0x4a. */
FNIEMOP_DEF(iemOp_dec_eDX)
{
    /*
     * This is a REX prefix in 64-bit mode.
     */
    if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
    {
        IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("rex.xw");
        pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_X | IEM_OP_PRF_SIZE_REX_W;
        pVCpu->iem.s.uRexIndex = 1 << 3;
        iemRecalEffOpSize(pVCpu);

        uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
        return FNIEMOP_CALL(g_apfnOneByteMap[b]);
    }

    IEMOP_MNEMONIC("dec eDX");
    return FNIEMOP_CALL_2(iemOpCommonUnaryGReg, &g_iemAImpl_dec, X86_GREG_xDX);
}


/** Opcode 0x4b. */
FNIEMOP_DEF(iemOp_dec_eBX)
{
    /*
     * This is a REX prefix in 64-bit mode.
     */
    if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
    {
        IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("rex.bxw");
        pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_B | IEM_OP_PRF_REX_X | IEM_OP_PRF_SIZE_REX_W;
        pVCpu->iem.s.uRexB     = 1 << 3;
        pVCpu->iem.s.uRexIndex = 1 << 3;
        iemRecalEffOpSize(pVCpu);

        uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
        return FNIEMOP_CALL(g_apfnOneByteMap[b]);
    }

    IEMOP_MNEMONIC("dec eBX");
    return FNIEMOP_CALL_2(iemOpCommonUnaryGReg, &g_iemAImpl_dec, X86_GREG_xBX);
}


/** Opcode 0x4c. */
FNIEMOP_DEF(iemOp_dec_eSP)
{
    /*
     * This is a REX prefix in 64-bit mode.
     */
    if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
    {
        IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("rex.rw");
        pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_R | IEM_OP_PRF_SIZE_REX_W;
        pVCpu->iem.s.uRexReg   = 1 << 3;
        iemRecalEffOpSize(pVCpu);

        uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
        return FNIEMOP_CALL(g_apfnOneByteMap[b]);
    }

    IEMOP_MNEMONIC("dec eSP");
    return FNIEMOP_CALL_2(iemOpCommonUnaryGReg, &g_iemAImpl_dec, X86_GREG_xSP);
}


/** Opcode 0x4d. */
FNIEMOP_DEF(iemOp_dec_eBP)
{
    /*
     * This is a REX prefix in 64-bit mode.
     */
    if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
    {
        IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("rex.rbw");
        pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_R | IEM_OP_PRF_REX_B | IEM_OP_PRF_SIZE_REX_W;
        pVCpu->iem.s.uRexReg   = 1 << 3;
        pVCpu->iem.s.uRexB     = 1 << 3;
        iemRecalEffOpSize(pVCpu);

        uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
        return FNIEMOP_CALL(g_apfnOneByteMap[b]);
    }

    IEMOP_MNEMONIC("dec eBP");
    return FNIEMOP_CALL_2(iemOpCommonUnaryGReg, &g_iemAImpl_dec, X86_GREG_xBP);
}


/** Opcode 0x4e. */
FNIEMOP_DEF(iemOp_dec_eSI)
{
    /*
     * This is a REX prefix in 64-bit mode.
     */
    if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
    {
        IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("rex.rxw");
        pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_R | IEM_OP_PRF_REX_X | IEM_OP_PRF_SIZE_REX_W;
        pVCpu->iem.s.uRexReg   = 1 << 3;
        pVCpu->iem.s.uRexIndex = 1 << 3;
        iemRecalEffOpSize(pVCpu);

        uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
        return FNIEMOP_CALL(g_apfnOneByteMap[b]);
    }

    IEMOP_MNEMONIC("dec eSI");
    return FNIEMOP_CALL_2(iemOpCommonUnaryGReg, &g_iemAImpl_dec, X86_GREG_xSI);
}


/** Opcode 0x4f. */
FNIEMOP_DEF(iemOp_dec_eDI)
{
    /*
     * This is a REX prefix in 64-bit mode.
     */
    if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
    {
        IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("rex.rbxw");
        pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_REX | IEM_OP_PRF_REX_R | IEM_OP_PRF_REX_B | IEM_OP_PRF_REX_X | IEM_OP_PRF_SIZE_REX_W;
        pVCpu->iem.s.uRexReg   = 1 << 3;
        pVCpu->iem.s.uRexB     = 1 << 3;
        pVCpu->iem.s.uRexIndex = 1 << 3;
        iemRecalEffOpSize(pVCpu);

        uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
        return FNIEMOP_CALL(g_apfnOneByteMap[b]);
    }

    IEMOP_MNEMONIC("dec eDI");
    return FNIEMOP_CALL_2(iemOpCommonUnaryGReg, &g_iemAImpl_dec, X86_GREG_xDI);
}


/**
 * Common 'push register' helper.
 */
FNIEMOP_DEF_1(iemOpCommonPushGReg, uint8_t, iReg)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
    {
        iReg |= pVCpu->iem.s.uRexB;
        pVCpu->iem.s.enmDefOpSize = IEMMODE_64BIT;
        pVCpu->iem.s.enmEffOpSize = !(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_OP) ? IEMMODE_64BIT : IEMMODE_16BIT;
    }

    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint16_t, u16Value);
            IEM_MC_FETCH_GREG_U16(u16Value, iReg);
            IEM_MC_PUSH_U16(u16Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            break;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint32_t, u32Value);
            IEM_MC_FETCH_GREG_U32(u32Value, iReg);
            IEM_MC_PUSH_U32(u32Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            break;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint64_t, u64Value);
            IEM_MC_FETCH_GREG_U64(u64Value, iReg);
            IEM_MC_PUSH_U64(u64Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            break;
    }

    return VINF_SUCCESS;
}


/** Opcode 0x50. */
FNIEMOP_DEF(iemOp_push_eAX)
{
    IEMOP_MNEMONIC("push rAX");
    return FNIEMOP_CALL_1(iemOpCommonPushGReg, X86_GREG_xAX);
}


/** Opcode 0x51. */
FNIEMOP_DEF(iemOp_push_eCX)
{
    IEMOP_MNEMONIC("push rCX");
    return FNIEMOP_CALL_1(iemOpCommonPushGReg, X86_GREG_xCX);
}


/** Opcode 0x52. */
FNIEMOP_DEF(iemOp_push_eDX)
{
    IEMOP_MNEMONIC("push rDX");
    return FNIEMOP_CALL_1(iemOpCommonPushGReg, X86_GREG_xDX);
}


/** Opcode 0x53. */
FNIEMOP_DEF(iemOp_push_eBX)
{
    IEMOP_MNEMONIC("push rBX");
    return FNIEMOP_CALL_1(iemOpCommonPushGReg, X86_GREG_xBX);
}


/** Opcode 0x54. */
FNIEMOP_DEF(iemOp_push_eSP)
{
    IEMOP_MNEMONIC("push rSP");
    if (IEM_GET_TARGET_CPU(pVCpu) == IEMTARGETCPU_8086)
    {
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(uint16_t, u16Value);
        IEM_MC_FETCH_GREG_U16(u16Value, X86_GREG_xSP);
        IEM_MC_SUB_LOCAL_U16(u16Value, 2);
        IEM_MC_PUSH_U16(u16Value);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return FNIEMOP_CALL_1(iemOpCommonPushGReg, X86_GREG_xSP);
}


/** Opcode 0x55. */
FNIEMOP_DEF(iemOp_push_eBP)
{
    IEMOP_MNEMONIC("push rBP");
    return FNIEMOP_CALL_1(iemOpCommonPushGReg, X86_GREG_xBP);
}


/** Opcode 0x56. */
FNIEMOP_DEF(iemOp_push_eSI)
{
    IEMOP_MNEMONIC("push rSI");
    return FNIEMOP_CALL_1(iemOpCommonPushGReg, X86_GREG_xSI);
}


/** Opcode 0x57. */
FNIEMOP_DEF(iemOp_push_eDI)
{
    IEMOP_MNEMONIC("push rDI");
    return FNIEMOP_CALL_1(iemOpCommonPushGReg, X86_GREG_xDI);
}


/**
 * Common 'pop register' helper.
 */
FNIEMOP_DEF_1(iemOpCommonPopGReg, uint8_t, iReg)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
    {
        iReg |= pVCpu->iem.s.uRexB;
        pVCpu->iem.s.enmDefOpSize = IEMMODE_64BIT;
        pVCpu->iem.s.enmEffOpSize = !(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_OP) ? IEMMODE_64BIT : IEMMODE_16BIT;
    }

    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint16_t *, pu16Dst);
            IEM_MC_REF_GREG_U16(pu16Dst, iReg);
            IEM_MC_POP_U16(pu16Dst);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            break;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint32_t *, pu32Dst);
            IEM_MC_REF_GREG_U32(pu32Dst, iReg);
            IEM_MC_POP_U32(pu32Dst);
            IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32Dst); /** @todo testcase*/
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            break;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint64_t *, pu64Dst);
            IEM_MC_REF_GREG_U64(pu64Dst, iReg);
            IEM_MC_POP_U64(pu64Dst);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            break;
    }

    return VINF_SUCCESS;
}


/** Opcode 0x58. */
FNIEMOP_DEF(iemOp_pop_eAX)
{
    IEMOP_MNEMONIC("pop rAX");
    return FNIEMOP_CALL_1(iemOpCommonPopGReg, X86_GREG_xAX);
}


/** Opcode 0x59. */
FNIEMOP_DEF(iemOp_pop_eCX)
{
    IEMOP_MNEMONIC("pop rCX");
    return FNIEMOP_CALL_1(iemOpCommonPopGReg, X86_GREG_xCX);
}


/** Opcode 0x5a. */
FNIEMOP_DEF(iemOp_pop_eDX)
{
    IEMOP_MNEMONIC("pop rDX");
    return FNIEMOP_CALL_1(iemOpCommonPopGReg, X86_GREG_xDX);
}


/** Opcode 0x5b. */
FNIEMOP_DEF(iemOp_pop_eBX)
{
    IEMOP_MNEMONIC("pop rBX");
    return FNIEMOP_CALL_1(iemOpCommonPopGReg, X86_GREG_xBX);
}


/** Opcode 0x5c. */
FNIEMOP_DEF(iemOp_pop_eSP)
{
    IEMOP_MNEMONIC("pop rSP");
    if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
    {
        if (pVCpu->iem.s.uRexB)
            return FNIEMOP_CALL_1(iemOpCommonPopGReg, X86_GREG_xSP);
        pVCpu->iem.s.enmDefOpSize = IEMMODE_64BIT;
        pVCpu->iem.s.enmEffOpSize = !(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_OP) ? IEMMODE_64BIT : IEMMODE_16BIT;
    }

    IEMOP_HLP_DECODED_NL_1(OP_POP, IEMOPFORM_FIXED, OP_PARM_REG_ESP,
                           DISOPTYPE_HARMLESS | DISOPTYPE_DEFAULT_64_OP_SIZE | DISOPTYPE_REXB_EXTENDS_OPREG);
    /** @todo add testcase for this instruction. */
    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint16_t, u16Dst);
            IEM_MC_POP_U16(&u16Dst); /** @todo not correct MC, fix later. */
            IEM_MC_STORE_GREG_U16(X86_GREG_xSP, u16Dst);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            break;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint32_t, u32Dst);
            IEM_MC_POP_U32(&u32Dst);
            IEM_MC_STORE_GREG_U32(X86_GREG_xSP, u32Dst);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            break;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(uint64_t, u64Dst);
            IEM_MC_POP_U64(&u64Dst);
            IEM_MC_STORE_GREG_U64(X86_GREG_xSP, u64Dst);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            break;
    }

    return VINF_SUCCESS;
}


/** Opcode 0x5d. */
FNIEMOP_DEF(iemOp_pop_eBP)
{
    IEMOP_MNEMONIC("pop rBP");
    return FNIEMOP_CALL_1(iemOpCommonPopGReg, X86_GREG_xBP);
}


/** Opcode 0x5e. */
FNIEMOP_DEF(iemOp_pop_eSI)
{
    IEMOP_MNEMONIC("pop rSI");
    return FNIEMOP_CALL_1(iemOpCommonPopGReg, X86_GREG_xSI);
}


/** Opcode 0x5f. */
FNIEMOP_DEF(iemOp_pop_eDI)
{
    IEMOP_MNEMONIC("pop rDI");
    return FNIEMOP_CALL_1(iemOpCommonPopGReg, X86_GREG_xDI);
}


/** Opcode 0x60. */
FNIEMOP_DEF(iemOp_pusha)
{
    IEMOP_MNEMONIC("pusha");
    IEMOP_HLP_MIN_186();
    IEMOP_HLP_NO_64BIT();
    if (pVCpu->iem.s.enmEffOpSize == IEMMODE_16BIT)
        return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_pusha_16);
    Assert(pVCpu->iem.s.enmEffOpSize == IEMMODE_32BIT);
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_pusha_32);
}


/** Opcode 0x61. */
FNIEMOP_DEF(iemOp_popa)
{
    IEMOP_MNEMONIC("popa");
    IEMOP_HLP_MIN_186();
    IEMOP_HLP_NO_64BIT();
    if (pVCpu->iem.s.enmEffOpSize == IEMMODE_16BIT)
        return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_popa_16);
    Assert(pVCpu->iem.s.enmEffOpSize == IEMMODE_32BIT);
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_popa_32);
}


/** Opcode 0x62. */
FNIEMOP_STUB(iemOp_bound_Gv_Ma_evex);
//    IEMOP_HLP_MIN_186();


/** Opcode 0x63 - non-64-bit modes. */
FNIEMOP_DEF(iemOp_arpl_Ew_Gw)
{
    IEMOP_MNEMONIC("arpl Ew,Gw");
    IEMOP_HLP_MIN_286();
    IEMOP_HLP_NO_REAL_OR_V86_MODE();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* Register */
        IEMOP_HLP_DECODED_NL_2(OP_ARPL, IEMOPFORM_MR_REG, OP_PARM_Ew, OP_PARM_Gw, DISOPTYPE_HARMLESS);
        IEM_MC_BEGIN(3, 0);
        IEM_MC_ARG(uint16_t *,      pu16Dst,    0);
        IEM_MC_ARG(uint16_t,        u16Src,     1);
        IEM_MC_ARG(uint32_t *,      pEFlags,    2);

        IEM_MC_FETCH_GREG_U16(u16Src, (bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK);
        IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK));
        IEM_MC_REF_EFLAGS(pEFlags);
        IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_arpl, pu16Dst, u16Src, pEFlags);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* Memory */
        IEM_MC_BEGIN(3, 2);
        IEM_MC_ARG(uint16_t *, pu16Dst,          0);
        IEM_MC_ARG(uint16_t,   u16Src,           1);
        IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 2);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DECODED_NL_2(OP_ARPL, IEMOPFORM_MR_REG, OP_PARM_Ew, OP_PARM_Gw, DISOPTYPE_HARMLESS);
        IEM_MC_MEM_MAP(pu16Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
        IEM_MC_FETCH_GREG_U16(u16Src, (bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK);
        IEM_MC_FETCH_EFLAGS(EFlags);
        IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_arpl, pu16Dst, u16Src, pEFlags);

        IEM_MC_MEM_COMMIT_AND_UNMAP(pu16Dst, IEM_ACCESS_DATA_RW);
        IEM_MC_COMMIT_EFLAGS(EFlags);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;

}


/** Opcode 0x63.
 * @note This is a weird one. It works like a regular move instruction if
 *       REX.W isn't set, at least according to AMD docs (rev 3.15, 2009-11).
 * @todo This definitely needs a testcase to verify the odd cases.  */
FNIEMOP_DEF(iemOp_movsxd_Gv_Ev)
{
    Assert(pVCpu->iem.s.enmEffOpSize == IEMMODE_64BIT); /* Caller branched already . */

    IEMOP_MNEMONIC("movsxd Gv,Ev");
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /*
         * Register to register.
         */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(uint64_t, u64Value);
        IEM_MC_FETCH_GREG_U32_SX_U64(u64Value, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u64Value);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /*
         * We're loading a register from memory.
         */
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint64_t, u64Value);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_FETCH_MEM_U32_SX_U64(u64Value, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
        IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u64Value);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x64. */
FNIEMOP_DEF(iemOp_seg_FS)
{
    IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("seg fs");
    IEMOP_HLP_MIN_386();

    pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_SEG_FS;
    pVCpu->iem.s.iEffSeg    = X86_SREG_FS;

    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    return FNIEMOP_CALL(g_apfnOneByteMap[b]);
}


/** Opcode 0x65. */
FNIEMOP_DEF(iemOp_seg_GS)
{
    IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("seg gs");
    IEMOP_HLP_MIN_386();

    pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_SEG_GS;
    pVCpu->iem.s.iEffSeg    = X86_SREG_GS;

    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    return FNIEMOP_CALL(g_apfnOneByteMap[b]);
}


/** Opcode 0x66. */
FNIEMOP_DEF(iemOp_op_size)
{
    IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("op size");
    IEMOP_HLP_MIN_386();

    pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_SIZE_OP;
    iemRecalEffOpSize(pVCpu);

    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    return FNIEMOP_CALL(g_apfnOneByteMap[b]);
}


/** Opcode 0x67. */
FNIEMOP_DEF(iemOp_addr_size)
{
    IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("addr size");
    IEMOP_HLP_MIN_386();

    pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_SIZE_ADDR;
    switch (pVCpu->iem.s.enmDefAddrMode)
    {
        case IEMMODE_16BIT: pVCpu->iem.s.enmEffAddrMode = IEMMODE_32BIT; break;
        case IEMMODE_32BIT: pVCpu->iem.s.enmEffAddrMode = IEMMODE_16BIT; break;
        case IEMMODE_64BIT: pVCpu->iem.s.enmEffAddrMode = IEMMODE_32BIT; break;
        default: AssertFailed();
    }

    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    return FNIEMOP_CALL(g_apfnOneByteMap[b]);
}


/** Opcode 0x68. */
FNIEMOP_DEF(iemOp_push_Iz)
{
    IEMOP_MNEMONIC("push Iz");
    IEMOP_HLP_MIN_186();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
        {
            uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
            IEM_MC_BEGIN(0,0);
            IEM_MC_PUSH_U16(u16Imm);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;
        }

        case IEMMODE_32BIT:
        {
            uint32_t u32Imm; IEM_OPCODE_GET_NEXT_U32(&u32Imm);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
            IEM_MC_BEGIN(0,0);
            IEM_MC_PUSH_U32(u32Imm);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;
        }

        case IEMMODE_64BIT:
        {
            uint64_t u64Imm; IEM_OPCODE_GET_NEXT_S32_SX_U64(&u64Imm);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
            IEM_MC_BEGIN(0,0);
            IEM_MC_PUSH_U64(u64Imm);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;
        }

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/** Opcode 0x69. */
FNIEMOP_DEF(iemOp_imul_Gv_Ev_Iz)
{
    IEMOP_MNEMONIC("imul Gv,Ev,Iz"); /* Gv = Ev * Iz; */
    IEMOP_HLP_MIN_186();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF);

    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
        {
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /* register operand */
                uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

                IEM_MC_BEGIN(3, 1);
                IEM_MC_ARG(uint16_t *,      pu16Dst,            0);
                IEM_MC_ARG_CONST(uint16_t,  u16Src,/*=*/ u16Imm,1);
                IEM_MC_ARG(uint32_t *,      pEFlags,            2);
                IEM_MC_LOCAL(uint16_t,      u16Tmp);

                IEM_MC_FETCH_GREG_U16(u16Tmp, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_LOCAL(pu16Dst, u16Tmp);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_imul_two_u16, pu16Dst, u16Src, pEFlags);
                IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u16Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /* memory operand */
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint16_t *,      pu16Dst,            0);
                IEM_MC_ARG(uint16_t,        u16Src,             1);
                IEM_MC_ARG(uint32_t *,      pEFlags,            2);
                IEM_MC_LOCAL(uint16_t,      u16Tmp);
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 2);
                uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
                IEM_MC_ASSIGN(u16Src, u16Imm);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U16(u16Tmp, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
                IEM_MC_REF_LOCAL(pu16Dst, u16Tmp);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_imul_two_u16, pu16Dst, u16Src, pEFlags);
                IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u16Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;
        }

        case IEMMODE_32BIT:
        {
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /* register operand */
                uint32_t u32Imm; IEM_OPCODE_GET_NEXT_U32(&u32Imm);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

                IEM_MC_BEGIN(3, 1);
                IEM_MC_ARG(uint32_t *,      pu32Dst,            0);
                IEM_MC_ARG_CONST(uint32_t,  u32Src,/*=*/ u32Imm,1);
                IEM_MC_ARG(uint32_t *,      pEFlags,            2);
                IEM_MC_LOCAL(uint32_t,      u32Tmp);

                IEM_MC_FETCH_GREG_U32(u32Tmp, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_LOCAL(pu32Dst, u32Tmp);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_imul_two_u32, pu32Dst, u32Src, pEFlags);
                IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u32Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /* memory operand */
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint32_t *,      pu32Dst,            0);
                IEM_MC_ARG(uint32_t,        u32Src,             1);
                IEM_MC_ARG(uint32_t *,      pEFlags,            2);
                IEM_MC_LOCAL(uint32_t,      u32Tmp);
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 4);
                uint32_t u32Imm; IEM_OPCODE_GET_NEXT_U32(&u32Imm);
                IEM_MC_ASSIGN(u32Src, u32Imm);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U32(u32Tmp, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
                IEM_MC_REF_LOCAL(pu32Dst, u32Tmp);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_imul_two_u32, pu32Dst, u32Src, pEFlags);
                IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u32Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;
        }

        case IEMMODE_64BIT:
        {
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /* register operand */
                uint64_t u64Imm; IEM_OPCODE_GET_NEXT_S32_SX_U64(&u64Imm);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

                IEM_MC_BEGIN(3, 1);
                IEM_MC_ARG(uint64_t *,      pu64Dst,            0);
                IEM_MC_ARG_CONST(uint64_t,  u64Src,/*=*/ u64Imm,1);
                IEM_MC_ARG(uint32_t *,      pEFlags,            2);
                IEM_MC_LOCAL(uint64_t,      u64Tmp);

                IEM_MC_FETCH_GREG_U64(u64Tmp, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_LOCAL(pu64Dst, u64Tmp);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_imul_two_u64, pu64Dst, u64Src, pEFlags);
                IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u64Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /* memory operand */
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint64_t *,      pu64Dst,            0);
                IEM_MC_ARG(uint64_t,        u64Src,             1);
                IEM_MC_ARG(uint32_t *,      pEFlags,            2);
                IEM_MC_LOCAL(uint64_t,      u64Tmp);
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 4);
                uint64_t u64Imm; IEM_OPCODE_GET_NEXT_S32_SX_U64(&u64Imm);
                IEM_MC_ASSIGN(u64Src, u64Imm);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U64(u64Tmp, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
                IEM_MC_REF_LOCAL(pu64Dst, u64Tmp);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_imul_two_u64, pu64Dst, u64Src, pEFlags);
                IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u64Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;
        }
    }
    AssertFailedReturn(VERR_IEM_IPE_9);
}


/** Opcode 0x6a. */
FNIEMOP_DEF(iemOp_push_Ib)
{
    IEMOP_MNEMONIC("push Ib");
    IEMOP_HLP_MIN_186();
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    IEM_MC_BEGIN(0,0);
    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEM_MC_PUSH_U16(i8Imm);
            break;
        case IEMMODE_32BIT:
            IEM_MC_PUSH_U32(i8Imm);
            break;
        case IEMMODE_64BIT:
            IEM_MC_PUSH_U64(i8Imm);
            break;
    }
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x6b. */
FNIEMOP_DEF(iemOp_imul_Gv_Ev_Ib)
{
    IEMOP_MNEMONIC("imul Gv,Ev,Ib"); /* Gv = Ev * Iz; */
    IEMOP_HLP_MIN_186();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF);

    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /* register operand */
                uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

                IEM_MC_BEGIN(3, 1);
                IEM_MC_ARG(uint16_t *,      pu16Dst,                    0);
                IEM_MC_ARG_CONST(uint16_t,  u16Src,/*=*/ (int8_t)u8Imm, 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                    2);
                IEM_MC_LOCAL(uint16_t,      u16Tmp);

                IEM_MC_FETCH_GREG_U16(u16Tmp, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_LOCAL(pu16Dst, u16Tmp);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_imul_two_u16, pu16Dst, u16Src, pEFlags);
                IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u16Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /* memory operand */
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint16_t *,      pu16Dst,                    0);
                IEM_MC_ARG(uint16_t,        u16Src,                     1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                    2);
                IEM_MC_LOCAL(uint16_t,      u16Tmp);
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 1);
                uint16_t u16Imm; IEM_OPCODE_GET_NEXT_S8_SX_U16(&u16Imm);
                IEM_MC_ASSIGN(u16Src, u16Imm);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U16(u16Tmp, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
                IEM_MC_REF_LOCAL(pu16Dst, u16Tmp);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_imul_two_u16, pu16Dst, u16Src, pEFlags);
                IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u16Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /* register operand */
                uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

                IEM_MC_BEGIN(3, 1);
                IEM_MC_ARG(uint32_t *,      pu32Dst,                    0);
                IEM_MC_ARG_CONST(uint32_t,  u32Src,/*=*/ (int8_t)u8Imm, 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                    2);
                IEM_MC_LOCAL(uint32_t,      u32Tmp);

                IEM_MC_FETCH_GREG_U32(u32Tmp, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_LOCAL(pu32Dst, u32Tmp);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_imul_two_u32, pu32Dst, u32Src, pEFlags);
                IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u32Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /* memory operand */
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint32_t *,      pu32Dst,                    0);
                IEM_MC_ARG(uint32_t,        u32Src,                     1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                    2);
                IEM_MC_LOCAL(uint32_t,      u32Tmp);
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 1);
                uint32_t u32Imm; IEM_OPCODE_GET_NEXT_S8_SX_U32(&u32Imm);
                IEM_MC_ASSIGN(u32Src, u32Imm);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U32(u32Tmp, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
                IEM_MC_REF_LOCAL(pu32Dst, u32Tmp);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_imul_two_u32, pu32Dst, u32Src, pEFlags);
                IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u32Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /* register operand */
                uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

                IEM_MC_BEGIN(3, 1);
                IEM_MC_ARG(uint64_t *,      pu64Dst,                    0);
                IEM_MC_ARG_CONST(uint64_t,  u64Src,/*=*/ (int8_t)u8Imm, 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                    2);
                IEM_MC_LOCAL(uint64_t,      u64Tmp);

                IEM_MC_FETCH_GREG_U64(u64Tmp, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_LOCAL(pu64Dst, u64Tmp);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_imul_two_u64, pu64Dst, u64Src, pEFlags);
                IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u64Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /* memory operand */
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint64_t *,      pu64Dst,                    0);
                IEM_MC_ARG(uint64_t,        u64Src,                     1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                    2);
                IEM_MC_LOCAL(uint64_t,      u64Tmp);
                IEM_MC_LOCAL(RTGCPTR,  GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 1);
                uint64_t u64Imm; IEM_OPCODE_GET_NEXT_S8_SX_U64(&u64Imm);
                IEM_MC_ASSIGN(u64Src, u64Imm);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U64(u64Tmp, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
                IEM_MC_REF_LOCAL(pu64Dst, u64Tmp);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_imul_two_u64, pu64Dst, u64Src, pEFlags);
                IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u64Tmp);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            return VINF_SUCCESS;
    }
    AssertFailedReturn(VERR_IEM_IPE_8);
}


/** Opcode 0x6c. */
FNIEMOP_DEF(iemOp_insb_Yb_DX)
{
    IEMOP_HLP_MIN_186();
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    if (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        IEMOP_MNEMONIC("rep ins Yb,DX");
        switch (pVCpu->iem.s.enmEffAddrMode)
        {
            case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_ins_op8_addr16, false);
            case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_ins_op8_addr32, false);
            case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_ins_op8_addr64, false);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        IEMOP_MNEMONIC("ins Yb,DX");
        switch (pVCpu->iem.s.enmEffAddrMode)
        {
            case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_ins_op8_addr16, false);
            case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_ins_op8_addr32, false);
            case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_ins_op8_addr64, false);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


/** Opcode 0x6d. */
FNIEMOP_DEF(iemOp_inswd_Yv_DX)
{
    IEMOP_HLP_MIN_186();
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    if (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_REPZ | IEM_OP_PRF_REPNZ))
    {
        IEMOP_MNEMONIC("rep ins Yv,DX");
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_ins_op16_addr16, false);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_ins_op16_addr32, false);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_ins_op16_addr64, false);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            case IEMMODE_64BIT:
            case IEMMODE_32BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_ins_op32_addr16, false);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_ins_op32_addr32, false);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_ins_op32_addr64, false);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        IEMOP_MNEMONIC("ins Yv,DX");
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_ins_op16_addr16, false);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_ins_op16_addr32, false);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_ins_op16_addr64, false);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            case IEMMODE_64BIT:
            case IEMMODE_32BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_ins_op32_addr16, false);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_ins_op32_addr32, false);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_ins_op32_addr64, false);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


/** Opcode 0x6e. */
FNIEMOP_DEF(iemOp_outsb_Yb_DX)
{
    IEMOP_HLP_MIN_186();
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    if (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        IEMOP_MNEMONIC("rep outs DX,Yb");
        switch (pVCpu->iem.s.enmEffAddrMode)
        {
            case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_rep_outs_op8_addr16, pVCpu->iem.s.iEffSeg, false);
            case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_rep_outs_op8_addr32, pVCpu->iem.s.iEffSeg, false);
            case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_rep_outs_op8_addr64, pVCpu->iem.s.iEffSeg, false);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        IEMOP_MNEMONIC("outs DX,Yb");
        switch (pVCpu->iem.s.enmEffAddrMode)
        {
            case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_outs_op8_addr16, pVCpu->iem.s.iEffSeg, false);
            case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_outs_op8_addr32, pVCpu->iem.s.iEffSeg, false);
            case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_outs_op8_addr64, pVCpu->iem.s.iEffSeg, false);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


/** Opcode 0x6f. */
FNIEMOP_DEF(iemOp_outswd_Yv_DX)
{
    IEMOP_HLP_MIN_186();
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    if (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_REPZ | IEM_OP_PRF_REPNZ))
    {
        IEMOP_MNEMONIC("rep outs DX,Yv");
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_rep_outs_op16_addr16, pVCpu->iem.s.iEffSeg, false);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_rep_outs_op16_addr32, pVCpu->iem.s.iEffSeg, false);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_rep_outs_op16_addr64, pVCpu->iem.s.iEffSeg, false);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            case IEMMODE_64BIT:
            case IEMMODE_32BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_rep_outs_op32_addr16, pVCpu->iem.s.iEffSeg, false);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_rep_outs_op32_addr32, pVCpu->iem.s.iEffSeg, false);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_rep_outs_op32_addr64, pVCpu->iem.s.iEffSeg, false);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        IEMOP_MNEMONIC("outs DX,Yv");
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_outs_op16_addr16, pVCpu->iem.s.iEffSeg, false);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_outs_op16_addr32, pVCpu->iem.s.iEffSeg, false);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_outs_op16_addr64, pVCpu->iem.s.iEffSeg, false);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            case IEMMODE_64BIT:
            case IEMMODE_32BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_outs_op32_addr16, pVCpu->iem.s.iEffSeg, false);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_outs_op32_addr32, pVCpu->iem.s.iEffSeg, false);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_outs_op32_addr64, pVCpu->iem.s.iEffSeg, false);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


/** Opcode 0x70. */
FNIEMOP_DEF(iemOp_jo_Jb)
{
    IEMOP_MNEMONIC("jo  Jb");
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    IEM_MC_BEGIN(0, 0);
    IEM_MC_IF_EFL_BIT_SET(X86_EFL_OF) {
        IEM_MC_REL_JMP_S8(i8Imm);
    } IEM_MC_ELSE() {
        IEM_MC_ADVANCE_RIP();
    } IEM_MC_ENDIF();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x71. */
FNIEMOP_DEF(iemOp_jno_Jb)
{
    IEMOP_MNEMONIC("jno Jb");
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    IEM_MC_BEGIN(0, 0);
    IEM_MC_IF_EFL_BIT_SET(X86_EFL_OF) {
        IEM_MC_ADVANCE_RIP();
    } IEM_MC_ELSE() {
        IEM_MC_REL_JMP_S8(i8Imm);
    } IEM_MC_ENDIF();
    IEM_MC_END();
    return VINF_SUCCESS;
}

/** Opcode 0x72. */
FNIEMOP_DEF(iemOp_jc_Jb)
{
    IEMOP_MNEMONIC("jc/jnae Jb");
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    IEM_MC_BEGIN(0, 0);
    IEM_MC_IF_EFL_BIT_SET(X86_EFL_CF) {
        IEM_MC_REL_JMP_S8(i8Imm);
    } IEM_MC_ELSE() {
        IEM_MC_ADVANCE_RIP();
    } IEM_MC_ENDIF();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x73. */
FNIEMOP_DEF(iemOp_jnc_Jb)
{
    IEMOP_MNEMONIC("jnc/jnb Jb");
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    IEM_MC_BEGIN(0, 0);
    IEM_MC_IF_EFL_BIT_SET(X86_EFL_CF) {
        IEM_MC_ADVANCE_RIP();
    } IEM_MC_ELSE() {
        IEM_MC_REL_JMP_S8(i8Imm);
    } IEM_MC_ENDIF();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x74. */
FNIEMOP_DEF(iemOp_je_Jb)
{
    IEMOP_MNEMONIC("je/jz   Jb");
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    IEM_MC_BEGIN(0, 0);
    IEM_MC_IF_EFL_BIT_SET(X86_EFL_ZF) {
        IEM_MC_REL_JMP_S8(i8Imm);
    } IEM_MC_ELSE() {
        IEM_MC_ADVANCE_RIP();
    } IEM_MC_ENDIF();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x75. */
FNIEMOP_DEF(iemOp_jne_Jb)
{
    IEMOP_MNEMONIC("jne/jnz Jb");
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    IEM_MC_BEGIN(0, 0);
    IEM_MC_IF_EFL_BIT_SET(X86_EFL_ZF) {
        IEM_MC_ADVANCE_RIP();
    } IEM_MC_ELSE() {
        IEM_MC_REL_JMP_S8(i8Imm);
    } IEM_MC_ENDIF();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x76. */
FNIEMOP_DEF(iemOp_jbe_Jb)
{
    IEMOP_MNEMONIC("jbe/jna Jb");
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    IEM_MC_BEGIN(0, 0);
    IEM_MC_IF_EFL_ANY_BITS_SET(X86_EFL_CF | X86_EFL_ZF) {
        IEM_MC_REL_JMP_S8(i8Imm);
    } IEM_MC_ELSE() {
        IEM_MC_ADVANCE_RIP();
    } IEM_MC_ENDIF();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x77. */
FNIEMOP_DEF(iemOp_jnbe_Jb)
{
    IEMOP_MNEMONIC("jnbe/ja Jb");
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    IEM_MC_BEGIN(0, 0);
    IEM_MC_IF_EFL_ANY_BITS_SET(X86_EFL_CF | X86_EFL_ZF) {
        IEM_MC_ADVANCE_RIP();
    } IEM_MC_ELSE() {
        IEM_MC_REL_JMP_S8(i8Imm);
    } IEM_MC_ENDIF();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x78. */
FNIEMOP_DEF(iemOp_js_Jb)
{
    IEMOP_MNEMONIC("js  Jb");
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    IEM_MC_BEGIN(0, 0);
    IEM_MC_IF_EFL_BIT_SET(X86_EFL_SF) {
        IEM_MC_REL_JMP_S8(i8Imm);
    } IEM_MC_ELSE() {
        IEM_MC_ADVANCE_RIP();
    } IEM_MC_ENDIF();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x79. */
FNIEMOP_DEF(iemOp_jns_Jb)
{
    IEMOP_MNEMONIC("jns Jb");
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    IEM_MC_BEGIN(0, 0);
    IEM_MC_IF_EFL_BIT_SET(X86_EFL_SF) {
        IEM_MC_ADVANCE_RIP();
    } IEM_MC_ELSE() {
        IEM_MC_REL_JMP_S8(i8Imm);
    } IEM_MC_ENDIF();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x7a. */
FNIEMOP_DEF(iemOp_jp_Jb)
{
    IEMOP_MNEMONIC("jp  Jb");
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    IEM_MC_BEGIN(0, 0);
    IEM_MC_IF_EFL_BIT_SET(X86_EFL_PF) {
        IEM_MC_REL_JMP_S8(i8Imm);
    } IEM_MC_ELSE() {
        IEM_MC_ADVANCE_RIP();
    } IEM_MC_ENDIF();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x7b. */
FNIEMOP_DEF(iemOp_jnp_Jb)
{
    IEMOP_MNEMONIC("jnp Jb");
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    IEM_MC_BEGIN(0, 0);
    IEM_MC_IF_EFL_BIT_SET(X86_EFL_PF) {
        IEM_MC_ADVANCE_RIP();
    } IEM_MC_ELSE() {
        IEM_MC_REL_JMP_S8(i8Imm);
    } IEM_MC_ENDIF();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x7c. */
FNIEMOP_DEF(iemOp_jl_Jb)
{
    IEMOP_MNEMONIC("jl/jnge Jb");
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    IEM_MC_BEGIN(0, 0);
    IEM_MC_IF_EFL_BITS_NE(X86_EFL_SF, X86_EFL_OF) {
        IEM_MC_REL_JMP_S8(i8Imm);
    } IEM_MC_ELSE() {
        IEM_MC_ADVANCE_RIP();
    } IEM_MC_ENDIF();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x7d. */
FNIEMOP_DEF(iemOp_jnl_Jb)
{
    IEMOP_MNEMONIC("jnl/jge Jb");
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    IEM_MC_BEGIN(0, 0);
    IEM_MC_IF_EFL_BITS_NE(X86_EFL_SF, X86_EFL_OF) {
        IEM_MC_ADVANCE_RIP();
    } IEM_MC_ELSE() {
        IEM_MC_REL_JMP_S8(i8Imm);
    } IEM_MC_ENDIF();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x7e. */
FNIEMOP_DEF(iemOp_jle_Jb)
{
    IEMOP_MNEMONIC("jle/jng Jb");
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    IEM_MC_BEGIN(0, 0);
    IEM_MC_IF_EFL_BIT_SET_OR_BITS_NE(X86_EFL_ZF, X86_EFL_SF, X86_EFL_OF) {
        IEM_MC_REL_JMP_S8(i8Imm);
    } IEM_MC_ELSE() {
        IEM_MC_ADVANCE_RIP();
    } IEM_MC_ENDIF();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x7f. */
FNIEMOP_DEF(iemOp_jnle_Jb)
{
    IEMOP_MNEMONIC("jnle/jg Jb");
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    IEM_MC_BEGIN(0, 0);
    IEM_MC_IF_EFL_BIT_SET_OR_BITS_NE(X86_EFL_ZF, X86_EFL_SF, X86_EFL_OF) {
        IEM_MC_ADVANCE_RIP();
    } IEM_MC_ELSE() {
        IEM_MC_REL_JMP_S8(i8Imm);
    } IEM_MC_ENDIF();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x80. */
FNIEMOP_DEF(iemOp_Grp1_Eb_Ib_80)
{
    uint8_t bRm;   IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_MNEMONIC2("add\0or\0\0adc\0sbb\0and\0sub\0xor\0cmp" + ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)*4, "Eb,Ib");
    PCIEMOPBINSIZES pImpl = g_apIemImplGrp1[(bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK];

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register target */
        uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(3, 0);
        IEM_MC_ARG(uint8_t *,       pu8Dst,                 0);
        IEM_MC_ARG_CONST(uint8_t,   u8Src, /*=*/ u8Imm,     1);
        IEM_MC_ARG(uint32_t *,      pEFlags,                2);

        IEM_MC_REF_GREG_U8(pu8Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        IEM_MC_REF_EFLAGS(pEFlags);
        IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU8, pu8Dst, u8Src, pEFlags);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory target */
        uint32_t fAccess;
        if (pImpl->pfnLockedU8)
            fAccess = IEM_ACCESS_DATA_RW;
        else /* CMP */
            fAccess = IEM_ACCESS_DATA_R;
        IEM_MC_BEGIN(3, 2);
        IEM_MC_ARG(uint8_t *,       pu8Dst,                 0);
        IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags,        2);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 1);
        uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
        IEM_MC_ARG_CONST(uint8_t,   u8Src, /*=*/ u8Imm,     1);
        if (pImpl->pfnLockedU8)
            IEMOP_HLP_DONE_DECODING();
        else
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_MEM_MAP(pu8Dst, fAccess, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
        IEM_MC_FETCH_EFLAGS(EFlags);
        if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
            IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU8, pu8Dst, u8Src, pEFlags);
        else
            IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnLockedU8, pu8Dst, u8Src, pEFlags);

        IEM_MC_MEM_COMMIT_AND_UNMAP(pu8Dst, fAccess);
        IEM_MC_COMMIT_EFLAGS(EFlags);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x81. */
FNIEMOP_DEF(iemOp_Grp1_Ev_Iz)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_MNEMONIC2("add\0or\0\0adc\0sbb\0and\0sub\0xor\0cmp" + ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)*4, "Ev,Iz");
    PCIEMOPBINSIZES pImpl = g_apIemImplGrp1[(bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK];

    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
        {
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /* register target */
                uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint16_t *,      pu16Dst,                0);
                IEM_MC_ARG_CONST(uint16_t,  u16Src, /*=*/ u16Imm,   1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                2);

                IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU16, pu16Dst, u16Src, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /* memory target */
                uint32_t fAccess;
                if (pImpl->pfnLockedU16)
                    fAccess = IEM_ACCESS_DATA_RW;
                else /* CMP, TEST */
                    fAccess = IEM_ACCESS_DATA_R;
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint16_t *,      pu16Dst,                0);
                IEM_MC_ARG(uint16_t,        u16Src,                 1);
                IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags,        2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 2);
                uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
                IEM_MC_ASSIGN(u16Src, u16Imm);
                if (pImpl->pfnLockedU16)
                    IEMOP_HLP_DONE_DECODING();
                else
                    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MEM_MAP(pu16Dst, fAccess, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_EFLAGS(EFlags);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU16, pu16Dst, u16Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnLockedU16, pu16Dst, u16Src, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu16Dst, fAccess);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            break;
        }

        case IEMMODE_32BIT:
        {
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /* register target */
                uint32_t u32Imm; IEM_OPCODE_GET_NEXT_U32(&u32Imm);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint32_t *,      pu32Dst,                0);
                IEM_MC_ARG_CONST(uint32_t,  u32Src, /*=*/ u32Imm,   1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                2);

                IEM_MC_REF_GREG_U32(pu32Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, u32Src, pEFlags);
                IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32Dst);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /* memory target */
                uint32_t fAccess;
                if (pImpl->pfnLockedU32)
                    fAccess = IEM_ACCESS_DATA_RW;
                else /* CMP, TEST */
                    fAccess = IEM_ACCESS_DATA_R;
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint32_t *,      pu32Dst,                0);
                IEM_MC_ARG(uint32_t,        u32Src,                 1);
                IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags,        2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 4);
                uint32_t u32Imm; IEM_OPCODE_GET_NEXT_U32(&u32Imm);
                IEM_MC_ASSIGN(u32Src, u32Imm);
                if (pImpl->pfnLockedU32)
                    IEMOP_HLP_DONE_DECODING();
                else
                    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MEM_MAP(pu32Dst, fAccess, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_EFLAGS(EFlags);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, u32Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnLockedU32, pu32Dst, u32Src, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu32Dst, fAccess);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            break;
        }

        case IEMMODE_64BIT:
        {
            if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
            {
                /* register target */
                uint64_t u64Imm; IEM_OPCODE_GET_NEXT_S32_SX_U64(&u64Imm);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint64_t *,      pu64Dst,                0);
                IEM_MC_ARG_CONST(uint64_t,  u64Src, /*=*/ u64Imm,   1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                2);

                IEM_MC_REF_GREG_U64(pu64Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU64, pu64Dst, u64Src, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            else
            {
                /* memory target */
                uint32_t fAccess;
                if (pImpl->pfnLockedU64)
                    fAccess = IEM_ACCESS_DATA_RW;
                else /* CMP */
                    fAccess = IEM_ACCESS_DATA_R;
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint64_t *,      pu64Dst,                0);
                IEM_MC_ARG(uint64_t,        u64Src,                 1);
                IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags,        2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 4);
                uint64_t u64Imm; IEM_OPCODE_GET_NEXT_S32_SX_U64(&u64Imm);
                if (pImpl->pfnLockedU64)
                    IEMOP_HLP_DONE_DECODING();
                else
                    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_ASSIGN(u64Src, u64Imm);
                IEM_MC_MEM_MAP(pu64Dst, fAccess, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_EFLAGS(EFlags);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU64, pu64Dst, u64Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnLockedU64, pu64Dst, u64Src, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu64Dst, fAccess);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
            }
            break;
        }
    }
    return VINF_SUCCESS;
}


/** Opcode 0x82. */
FNIEMOP_DEF(iemOp_Grp1_Eb_Ib_82)
{
    IEMOP_HLP_NO_64BIT(); /** @todo do we need to decode the whole instruction or is this ok? */
    return FNIEMOP_CALL(iemOp_Grp1_Eb_Ib_80);
}


/** Opcode 0x83. */
FNIEMOP_DEF(iemOp_Grp1_Ev_Ib)
{
    uint8_t bRm;   IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_MNEMONIC2("add\0or\0\0adc\0sbb\0and\0sub\0xor\0cmp" + ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)*4, "Ev,Ib");
    /* Note! Seems the OR, AND, and XOR instructions are present on CPUs prior
             to the 386 even if absent in the intel reference manuals and some
             3rd party opcode listings. */
    PCIEMOPBINSIZES pImpl = g_apIemImplGrp1[(bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK];

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /*
         * Register target
         */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
            {
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint16_t *,      pu16Dst,                    0);
                IEM_MC_ARG_CONST(uint16_t,  u16Src, /*=*/ (int8_t)u8Imm,1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                    2);

                IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU16, pu16Dst, u16Src, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;
            }

            case IEMMODE_32BIT:
            {
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint32_t *,      pu32Dst,                    0);
                IEM_MC_ARG_CONST(uint32_t,  u32Src, /*=*/ (int8_t)u8Imm,1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                    2);

                IEM_MC_REF_GREG_U32(pu32Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, u32Src, pEFlags);
                IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32Dst);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;
            }

            case IEMMODE_64BIT:
            {
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint64_t *,      pu64Dst,                    0);
                IEM_MC_ARG_CONST(uint64_t,  u64Src, /*=*/ (int8_t)u8Imm,1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                    2);

                IEM_MC_REF_GREG_U64(pu64Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU64, pu64Dst, u64Src, pEFlags);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;
            }
        }
    }
    else
    {
        /*
         * Memory target.
         */
        uint32_t fAccess;
        if (pImpl->pfnLockedU16)
            fAccess = IEM_ACCESS_DATA_RW;
        else /* CMP */
            fAccess = IEM_ACCESS_DATA_R;

        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
            {
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint16_t *,      pu16Dst,                    0);
                IEM_MC_ARG(uint16_t,        u16Src,                     1);
                IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags,            2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 1);
                uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
                IEM_MC_ASSIGN(u16Src, (int8_t)u8Imm);
                if (pImpl->pfnLockedU16)
                    IEMOP_HLP_DONE_DECODING();
                else
                    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MEM_MAP(pu16Dst, fAccess, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_EFLAGS(EFlags);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU16, pu16Dst, u16Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnLockedU16, pu16Dst, u16Src, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu16Dst, fAccess);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;
            }

            case IEMMODE_32BIT:
            {
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint32_t *,      pu32Dst,                    0);
                IEM_MC_ARG(uint32_t,        u32Src,                     1);
                IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags,            2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 1);
                uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
                IEM_MC_ASSIGN(u32Src, (int8_t)u8Imm);
                if (pImpl->pfnLockedU32)
                    IEMOP_HLP_DONE_DECODING();
                else
                    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MEM_MAP(pu32Dst, fAccess, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_EFLAGS(EFlags);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, u32Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnLockedU32, pu32Dst, u32Src, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu32Dst, fAccess);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;
            }

            case IEMMODE_64BIT:
            {
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint64_t *,      pu64Dst,                    0);
                IEM_MC_ARG(uint64_t,        u64Src,                     1);
                IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags,            2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 1);
                uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
                IEM_MC_ASSIGN(u64Src, (int8_t)u8Imm);
                if (pImpl->pfnLockedU64)
                    IEMOP_HLP_DONE_DECODING();
                else
                    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MEM_MAP(pu64Dst, fAccess, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_EFLAGS(EFlags);
                if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU64, pu64Dst, u64Src, pEFlags);
                else
                    IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnLockedU64, pu64Dst, u64Src, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu64Dst, fAccess);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;
            }
        }
    }
    return VINF_SUCCESS;
}


/** Opcode 0x84. */
FNIEMOP_DEF(iemOp_test_Eb_Gb)
{
    IEMOP_MNEMONIC("test Eb,Gb");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rm_r8, &g_iemAImpl_test);
}


/** Opcode 0x85. */
FNIEMOP_DEF(iemOp_test_Ev_Gv)
{
    IEMOP_MNEMONIC("test Ev,Gv");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rm_rv, &g_iemAImpl_test);
}


/** Opcode 0x86. */
FNIEMOP_DEF(iemOp_xchg_Eb_Gb)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    IEMOP_MNEMONIC("xchg Eb,Gb");

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint8_t, uTmp1);
        IEM_MC_LOCAL(uint8_t, uTmp2);

        IEM_MC_FETCH_GREG_U8(uTmp1, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_FETCH_GREG_U8(uTmp2, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        IEM_MC_STORE_GREG_U8((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB,                              uTmp1);
        IEM_MC_STORE_GREG_U8(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, uTmp2);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /*
         * We're accessing memory.
         */
/** @todo the register must be committed separately! */
        IEM_MC_BEGIN(2, 2);
        IEM_MC_ARG(uint8_t *,  pu8Mem,           0);
        IEM_MC_ARG(uint8_t *,  pu8Reg,           1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEM_MC_MEM_MAP(pu8Mem, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
        IEM_MC_REF_GREG_U8(pu8Reg, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_CALL_VOID_AIMPL_2(iemAImpl_xchg_u8, pu8Mem, pu8Reg);
        IEM_MC_MEM_COMMIT_AND_UNMAP(pu8Mem, IEM_ACCESS_DATA_RW);

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x87. */
FNIEMOP_DEF(iemOp_xchg_Ev_Gv)
{
    IEMOP_MNEMONIC("xchg Ev,Gv");
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint16_t, uTmp1);
                IEM_MC_LOCAL(uint16_t, uTmp2);

                IEM_MC_FETCH_GREG_U16(uTmp1, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_FETCH_GREG_U16(uTmp2, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_STORE_GREG_U16((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB,                              uTmp1);
                IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, uTmp2);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint32_t, uTmp1);
                IEM_MC_LOCAL(uint32_t, uTmp2);

                IEM_MC_FETCH_GREG_U32(uTmp1, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_FETCH_GREG_U32(uTmp2, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_STORE_GREG_U32((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB,                              uTmp1);
                IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, uTmp2);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint64_t, uTmp1);
                IEM_MC_LOCAL(uint64_t, uTmp2);

                IEM_MC_FETCH_GREG_U64(uTmp1, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_FETCH_GREG_U64(uTmp2, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_STORE_GREG_U64((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB,                              uTmp1);
                IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, uTmp2);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        /*
         * We're accessing memory.
         */
        switch (pVCpu->iem.s.enmEffOpSize)
        {
/** @todo the register must be committed separately! */
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(2, 2);
                IEM_MC_ARG(uint16_t *,  pu16Mem, 0);
                IEM_MC_ARG(uint16_t *,  pu16Reg, 1);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEM_MC_MEM_MAP(pu16Mem, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_REF_GREG_U16(pu16Reg, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_CALL_VOID_AIMPL_2(iemAImpl_xchg_u16, pu16Mem, pu16Reg);
                IEM_MC_MEM_COMMIT_AND_UNMAP(pu16Mem, IEM_ACCESS_DATA_RW);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(2, 2);
                IEM_MC_ARG(uint32_t *,  pu32Mem, 0);
                IEM_MC_ARG(uint32_t *,  pu32Reg, 1);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEM_MC_MEM_MAP(pu32Mem, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_REF_GREG_U32(pu32Reg, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_CALL_VOID_AIMPL_2(iemAImpl_xchg_u32, pu32Mem, pu32Reg);
                IEM_MC_MEM_COMMIT_AND_UNMAP(pu32Mem, IEM_ACCESS_DATA_RW);

                IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32Reg);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(2, 2);
                IEM_MC_ARG(uint64_t *,  pu64Mem, 0);
                IEM_MC_ARG(uint64_t *,  pu64Reg, 1);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEM_MC_MEM_MAP(pu64Mem, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_REF_GREG_U64(pu64Reg, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_CALL_VOID_AIMPL_2(iemAImpl_xchg_u64, pu64Mem, pu64Reg);
                IEM_MC_MEM_COMMIT_AND_UNMAP(pu64Mem, IEM_ACCESS_DATA_RW);

                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


/** Opcode 0x88. */
FNIEMOP_DEF(iemOp_mov_Eb_Gb)
{
    IEMOP_MNEMONIC("mov Eb,Gb");

    uint8_t bRm;
    IEM_OPCODE_GET_NEXT_U8(&bRm);

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(uint8_t, u8Value);
        IEM_MC_FETCH_GREG_U8(u8Value, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_STORE_GREG_U8((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u8Value);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /*
         * We're writing a register to memory.
         */
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint8_t, u8Value);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_FETCH_GREG_U8(u8Value, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
        IEM_MC_STORE_MEM_U8(pVCpu->iem.s.iEffSeg, GCPtrEffDst, u8Value);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;

}


/** Opcode 0x89. */
FNIEMOP_DEF(iemOp_mov_Ev_Gv)
{
    IEMOP_MNEMONIC("mov Ev,Gv");

    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint16_t, u16Value);
                IEM_MC_FETCH_GREG_U16(u16Value, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_STORE_GREG_U16((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u16Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint32_t, u32Value);
                IEM_MC_FETCH_GREG_U32(u32Value, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_STORE_GREG_U32((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u32Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint64_t, u64Value);
                IEM_MC_FETCH_GREG_U64(u64Value, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_STORE_GREG_U64((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u64Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;
        }
    }
    else
    {
        /*
         * We're writing a register to memory.
         */
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint16_t, u16Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_GREG_U16(u16Value, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_STORE_MEM_U16(pVCpu->iem.s.iEffSeg, GCPtrEffDst, u16Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint32_t, u32Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_GREG_U32(u32Value, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_STORE_MEM_U32(pVCpu->iem.s.iEffSeg, GCPtrEffDst, u32Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint64_t, u64Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_GREG_U64(u64Value, ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg);
                IEM_MC_STORE_MEM_U64(pVCpu->iem.s.iEffSeg, GCPtrEffDst, u64Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;
        }
    }
    return VINF_SUCCESS;
}


/** Opcode 0x8a. */
FNIEMOP_DEF(iemOp_mov_Gb_Eb)
{
    IEMOP_MNEMONIC("mov Gb,Eb");

    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(uint8_t, u8Value);
        IEM_MC_FETCH_GREG_U8(u8Value, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        IEM_MC_STORE_GREG_U8(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u8Value);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /*
         * We're loading a register from memory.
         */
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint8_t, u8Value);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_FETCH_MEM_U8(u8Value, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
        IEM_MC_STORE_GREG_U8(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u8Value);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x8b. */
FNIEMOP_DEF(iemOp_mov_Gv_Ev)
{
    IEMOP_MNEMONIC("mov Gv,Ev");

    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint16_t, u16Value);
                IEM_MC_FETCH_GREG_U16(u16Value, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u16Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint32_t, u32Value);
                IEM_MC_FETCH_GREG_U32(u32Value, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u32Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint64_t, u64Value);
                IEM_MC_FETCH_GREG_U64(u64Value, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u64Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;
        }
    }
    else
    {
        /*
         * We're loading a register from memory.
         */
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint16_t, u16Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U16(u16Value, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
                IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u16Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint32_t, u32Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U32(u32Value, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
                IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u32Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint64_t, u64Value);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U64(u64Value, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
                IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u64Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;
        }
    }
    return VINF_SUCCESS;
}


/** Opcode 0x63. */
FNIEMOP_DEF(iemOp_arpl_Ew_Gw_movsx_Gv_Ev)
{
    if (pVCpu->iem.s.enmCpuMode != IEMMODE_64BIT)
        return FNIEMOP_CALL(iemOp_arpl_Ew_Gw);
    if (pVCpu->iem.s.enmEffOpSize != IEMMODE_64BIT)
        return FNIEMOP_CALL(iemOp_mov_Gv_Ev);
    return FNIEMOP_CALL(iemOp_movsxd_Gv_Ev);
}


/** Opcode 0x8c. */
FNIEMOP_DEF(iemOp_mov_Ev_Sw)
{
    IEMOP_MNEMONIC("mov Ev,Sw");

    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /*
     * Check that the destination register exists. The REX.R prefix is ignored.
     */
    uint8_t const iSegReg = ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK);
    if (   iSegReg > X86_SREG_GS)
        return IEMOP_RAISE_INVALID_OPCODE(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */

    /*
     * If rm is denoting a register, no more instruction bytes.
     * In that case, the operand size is respected and the upper bits are
     * cleared (starting with some pentium).
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint16_t, u16Value);
                IEM_MC_FETCH_SREG_U16(u16Value, iSegReg);
                IEM_MC_STORE_GREG_U16((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u16Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint32_t, u32Value);
                IEM_MC_FETCH_SREG_ZX_U32(u32Value, iSegReg);
                IEM_MC_STORE_GREG_U32((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u32Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint64_t, u64Value);
                IEM_MC_FETCH_SREG_ZX_U64(u64Value, iSegReg);
                IEM_MC_STORE_GREG_U64((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u64Value);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                break;
        }
    }
    else
    {
        /*
         * We're saving the register to memory.  The access is word sized
         * regardless of operand size prefixes.
         */
#if 0 /* not necessary */
        pVCpu->iem.s.enmEffOpSize = pVCpu->iem.s.enmDefOpSize = IEMMODE_16BIT;
#endif
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(uint16_t,  u16Value);
        IEM_MC_LOCAL(RTGCPTR,   GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_FETCH_SREG_U16(u16Value, iSegReg);
        IEM_MC_STORE_MEM_U16(pVCpu->iem.s.iEffSeg, GCPtrEffDst, u16Value);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}




/** Opcode 0x8d. */
FNIEMOP_DEF(iemOp_lea_Gv_M)
{
    IEMOP_MNEMONIC("lea Gv,M");
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
        return IEMOP_RAISE_INVALID_OPCODE(); /* no register form */

    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(0, 2);
            IEM_MC_LOCAL(RTGCPTR,  GCPtrEffSrc);
            IEM_MC_LOCAL(uint16_t, u16Cast);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
            IEM_MC_ASSIGN_TO_SMALLER(u16Cast, GCPtrEffSrc);
            IEM_MC_STORE_GREG_U16(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u16Cast);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(0, 2);
            IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
            IEM_MC_LOCAL(uint32_t, u32Cast);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
            IEM_MC_ASSIGN_TO_SMALLER(u32Cast, GCPtrEffSrc);
            IEM_MC_STORE_GREG_U32(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, u32Cast);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
            IEM_MC_STORE_GREG_U64(((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg, GCPtrEffSrc);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;
    }
    AssertFailedReturn(VERR_IEM_IPE_7);
}


/** Opcode 0x8e. */
FNIEMOP_DEF(iemOp_mov_Sw_Ev)
{
    IEMOP_MNEMONIC("mov Sw,Ev");

    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);

    /*
     * The practical operand size is 16-bit.
     */
#if 0 /* not necessary */
    pVCpu->iem.s.enmEffOpSize = pVCpu->iem.s.enmDefOpSize = IEMMODE_16BIT;
#endif

    /*
     * Check that the destination register exists and can be used with this
     * instruction.  The REX.R prefix is ignored.
     */
    uint8_t const iSegReg = ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK);
    if (   iSegReg == X86_SREG_CS
        || iSegReg > X86_SREG_GS)
        return IEMOP_RAISE_INVALID_OPCODE(); /** @todo should probably not be raised until we've fetched all the opcode bytes? */

    /*
     * If rm is denoting a register, no more instruction bytes.
     */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(2, 0);
        IEM_MC_ARG_CONST(uint8_t, iSRegArg, iSegReg, 0);
        IEM_MC_ARG(uint16_t,      u16Value,          1);
        IEM_MC_FETCH_GREG_U16(u16Value, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        IEM_MC_CALL_CIMPL_2(iemCImpl_load_SReg, iSRegArg, u16Value);
        IEM_MC_END();
    }
    else
    {
        /*
         * We're loading the register from memory.  The access is word sized
         * regardless of operand size prefixes.
         */
        IEM_MC_BEGIN(2, 1);
        IEM_MC_ARG_CONST(uint8_t, iSRegArg, iSegReg, 0);
        IEM_MC_ARG(uint16_t,      u16Value,          1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_FETCH_MEM_U16(u16Value, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
        IEM_MC_CALL_CIMPL_2(iemCImpl_load_SReg, iSRegArg, u16Value);
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0x8f /0. */
FNIEMOP_DEF_1(iemOp_pop_Ev, uint8_t, bRm)
{
    /* This bugger is rather annoying as it requires rSP to be updated before
       doing the effective address calculations.  Will eventually require a
       split between the R/M+SIB decoding and the effective address
       calculation - which is something that is required for any attempt at
       reusing this code for a recompiler.  It may also be good to have if we
       need to delay #UD exception caused by invalid lock prefixes.

       For now, we'll do a mostly safe interpreter-only implementation here. */
    /** @todo What's the deal with the 'reg' field and pop Ev?  Ignorning it for
     *        now until tests show it's checked.. */
    IEMOP_MNEMONIC("pop Ev");

    /* Register access is relatively easy and can share code. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
        return FNIEMOP_CALL_1(iemOpCommonPopGReg, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);

    /*
     * Memory target.
     *
     * Intel says that RSP is incremented before it's used in any effective
     * address calcuations.  This means some serious extra annoyance here since
     * we decode and calculate the effective address in one step and like to
     * delay committing registers till everything is done.
     *
     * So, we'll decode and calculate the effective address twice.  This will
     * require some recoding if turned into a recompiler.
     */
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE(); /* The common code does this differently. */

#ifndef TST_IEM_CHECK_MC
    /* Calc effective address with modified ESP. */
/** @todo testcase */
    PCPUMCTX        pCtx     = IEM_GET_CTX(pVCpu);
    RTGCPTR         GCPtrEff;
    VBOXSTRICTRC    rcStrict;
    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT: rcStrict = iemOpHlpCalcRmEffAddrEx(pVCpu, bRm, 0, &GCPtrEff, 2); break;
        case IEMMODE_32BIT: rcStrict = iemOpHlpCalcRmEffAddrEx(pVCpu, bRm, 0, &GCPtrEff, 4); break;
        case IEMMODE_64BIT: rcStrict = iemOpHlpCalcRmEffAddrEx(pVCpu, bRm, 0, &GCPtrEff, 8); break;
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    /* Perform the operation - this should be CImpl. */
    RTUINT64U TmpRsp;
    TmpRsp.u = pCtx->rsp;
    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
        {
            uint16_t u16Value;
            rcStrict = iemMemStackPopU16Ex(pVCpu, &u16Value, &TmpRsp);
            if (rcStrict == VINF_SUCCESS)
                rcStrict = iemMemStoreDataU16(pVCpu, pVCpu->iem.s.iEffSeg, GCPtrEff, u16Value);
            break;
        }

        case IEMMODE_32BIT:
        {
            uint32_t u32Value;
            rcStrict = iemMemStackPopU32Ex(pVCpu, &u32Value, &TmpRsp);
            if (rcStrict == VINF_SUCCESS)
                rcStrict = iemMemStoreDataU32(pVCpu, pVCpu->iem.s.iEffSeg, GCPtrEff, u32Value);
            break;
        }

        case IEMMODE_64BIT:
        {
            uint64_t u64Value;
            rcStrict = iemMemStackPopU64Ex(pVCpu, &u64Value, &TmpRsp);
            if (rcStrict == VINF_SUCCESS)
                rcStrict = iemMemStoreDataU64(pVCpu, pVCpu->iem.s.iEffSeg, GCPtrEff, u64Value);
            break;
        }

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
    if (rcStrict == VINF_SUCCESS)
    {
        pCtx->rsp = TmpRsp.u;
        iemRegUpdateRipAndClearRF(pVCpu);
    }
    return rcStrict;

#else
    return VERR_IEM_IPE_2;
#endif
}


/** Opcode 0x8f. */
FNIEMOP_DEF(iemOp_Grp1A)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_REG_MASK) == (0 << X86_MODRM_REG_SHIFT)) /* /0 */
        return FNIEMOP_CALL_1(iemOp_pop_Ev, bRm);

    /* AMD has defined /1 thru /7 as XOP prefix (similar to three byte VEX). */
    /** @todo XOP decoding. */
    IEMOP_MNEMONIC("3-byte-xop");
    return IEMOP_RAISE_INVALID_OPCODE();
}


/**
 * Common 'xchg reg,rAX' helper.
 */
FNIEMOP_DEF_1(iemOpCommonXchgGRegRax, uint8_t, iReg)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    iReg |= pVCpu->iem.s.uRexB;
    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(0, 2);
            IEM_MC_LOCAL(uint16_t, u16Tmp1);
            IEM_MC_LOCAL(uint16_t, u16Tmp2);
            IEM_MC_FETCH_GREG_U16(u16Tmp1, iReg);
            IEM_MC_FETCH_GREG_U16(u16Tmp2, X86_GREG_xAX);
            IEM_MC_STORE_GREG_U16(X86_GREG_xAX, u16Tmp1);
            IEM_MC_STORE_GREG_U16(iReg,         u16Tmp2);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(0, 2);
            IEM_MC_LOCAL(uint32_t, u32Tmp1);
            IEM_MC_LOCAL(uint32_t, u32Tmp2);
            IEM_MC_FETCH_GREG_U32(u32Tmp1, iReg);
            IEM_MC_FETCH_GREG_U32(u32Tmp2, X86_GREG_xAX);
            IEM_MC_STORE_GREG_U32(X86_GREG_xAX, u32Tmp1);
            IEM_MC_STORE_GREG_U32(iReg,         u32Tmp2);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(0, 2);
            IEM_MC_LOCAL(uint64_t, u64Tmp1);
            IEM_MC_LOCAL(uint64_t, u64Tmp2);
            IEM_MC_FETCH_GREG_U64(u64Tmp1, iReg);
            IEM_MC_FETCH_GREG_U64(u64Tmp2, X86_GREG_xAX);
            IEM_MC_STORE_GREG_U64(X86_GREG_xAX, u64Tmp1);
            IEM_MC_STORE_GREG_U64(iReg,         u64Tmp2);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/** Opcode 0x90. */
FNIEMOP_DEF(iemOp_nop)
{
    /* R8/R8D and RAX/EAX can be exchanged. */
    if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_REX_B)
    {
        IEMOP_MNEMONIC("xchg r8,rAX");
        return FNIEMOP_CALL_1(iemOpCommonXchgGRegRax, X86_GREG_xAX);
    }

    if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK)
        IEMOP_MNEMONIC("pause");
    else
        IEMOP_MNEMONIC("nop");
    IEM_MC_BEGIN(0, 0);
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x91. */
FNIEMOP_DEF(iemOp_xchg_eCX_eAX)
{
    IEMOP_MNEMONIC("xchg rCX,rAX");
    return FNIEMOP_CALL_1(iemOpCommonXchgGRegRax, X86_GREG_xCX);
}


/** Opcode 0x92. */
FNIEMOP_DEF(iemOp_xchg_eDX_eAX)
{
    IEMOP_MNEMONIC("xchg rDX,rAX");
    return FNIEMOP_CALL_1(iemOpCommonXchgGRegRax, X86_GREG_xDX);
}


/** Opcode 0x93. */
FNIEMOP_DEF(iemOp_xchg_eBX_eAX)
{
    IEMOP_MNEMONIC("xchg rBX,rAX");
    return FNIEMOP_CALL_1(iemOpCommonXchgGRegRax, X86_GREG_xBX);
}


/** Opcode 0x94. */
FNIEMOP_DEF(iemOp_xchg_eSP_eAX)
{
    IEMOP_MNEMONIC("xchg rSX,rAX");
    return FNIEMOP_CALL_1(iemOpCommonXchgGRegRax, X86_GREG_xSP);
}


/** Opcode 0x95. */
FNIEMOP_DEF(iemOp_xchg_eBP_eAX)
{
    IEMOP_MNEMONIC("xchg rBP,rAX");
    return FNIEMOP_CALL_1(iemOpCommonXchgGRegRax, X86_GREG_xBP);
}


/** Opcode 0x96. */
FNIEMOP_DEF(iemOp_xchg_eSI_eAX)
{
    IEMOP_MNEMONIC("xchg rSI,rAX");
    return FNIEMOP_CALL_1(iemOpCommonXchgGRegRax, X86_GREG_xSI);
}


/** Opcode 0x97. */
FNIEMOP_DEF(iemOp_xchg_eDI_eAX)
{
    IEMOP_MNEMONIC("xchg rDI,rAX");
    return FNIEMOP_CALL_1(iemOpCommonXchgGRegRax, X86_GREG_xDI);
}


/** Opcode 0x98. */
FNIEMOP_DEF(iemOp_cbw)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEMOP_MNEMONIC("cbw");
            IEM_MC_BEGIN(0, 1);
            IEM_MC_IF_GREG_BIT_SET(X86_GREG_xAX, 7) {
                IEM_MC_OR_GREG_U16(X86_GREG_xAX, UINT16_C(0xff00));
            } IEM_MC_ELSE() {
                IEM_MC_AND_GREG_U16(X86_GREG_xAX, UINT16_C(0x00ff));
            } IEM_MC_ENDIF();
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            IEMOP_MNEMONIC("cwde");
            IEM_MC_BEGIN(0, 1);
            IEM_MC_IF_GREG_BIT_SET(X86_GREG_xAX, 15) {
                IEM_MC_OR_GREG_U32(X86_GREG_xAX, UINT32_C(0xffff0000));
            } IEM_MC_ELSE() {
                IEM_MC_AND_GREG_U32(X86_GREG_xAX, UINT32_C(0x0000ffff));
            } IEM_MC_ENDIF();
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            IEMOP_MNEMONIC("cdqe");
            IEM_MC_BEGIN(0, 1);
            IEM_MC_IF_GREG_BIT_SET(X86_GREG_xAX, 31) {
                IEM_MC_OR_GREG_U64(X86_GREG_xAX, UINT64_C(0xffffffff00000000));
            } IEM_MC_ELSE() {
                IEM_MC_AND_GREG_U64(X86_GREG_xAX, UINT64_C(0x00000000ffffffff));
            } IEM_MC_ENDIF();
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/** Opcode 0x99. */
FNIEMOP_DEF(iemOp_cwd)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEMOP_MNEMONIC("cwd");
            IEM_MC_BEGIN(0, 1);
            IEM_MC_IF_GREG_BIT_SET(X86_GREG_xAX, 15) {
                IEM_MC_STORE_GREG_U16_CONST(X86_GREG_xDX, UINT16_C(0xffff));
            } IEM_MC_ELSE() {
                IEM_MC_STORE_GREG_U16_CONST(X86_GREG_xDX, 0);
            } IEM_MC_ENDIF();
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            IEMOP_MNEMONIC("cdq");
            IEM_MC_BEGIN(0, 1);
            IEM_MC_IF_GREG_BIT_SET(X86_GREG_xAX, 31) {
                IEM_MC_STORE_GREG_U32_CONST(X86_GREG_xDX, UINT32_C(0xffffffff));
            } IEM_MC_ELSE() {
                IEM_MC_STORE_GREG_U32_CONST(X86_GREG_xDX, 0);
            } IEM_MC_ENDIF();
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            IEMOP_MNEMONIC("cqo");
            IEM_MC_BEGIN(0, 1);
            IEM_MC_IF_GREG_BIT_SET(X86_GREG_xAX, 63) {
                IEM_MC_STORE_GREG_U64_CONST(X86_GREG_xDX, UINT64_C(0xffffffffffffffff));
            } IEM_MC_ELSE() {
                IEM_MC_STORE_GREG_U64_CONST(X86_GREG_xDX, 0);
            } IEM_MC_ENDIF();
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/** Opcode 0x9a. */
FNIEMOP_DEF(iemOp_call_Ap)
{
    IEMOP_MNEMONIC("call Ap");
    IEMOP_HLP_NO_64BIT();

    /* Decode the far pointer address and pass it on to the far call C implementation. */
    uint32_t offSeg;
    if (pVCpu->iem.s.enmEffOpSize != IEMMODE_16BIT)
        IEM_OPCODE_GET_NEXT_U32(&offSeg);
    else
        IEM_OPCODE_GET_NEXT_U16_ZX_U32(&offSeg);
    uint16_t uSel;  IEM_OPCODE_GET_NEXT_U16(&uSel);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_3(iemCImpl_callf, uSel, offSeg, pVCpu->iem.s.enmEffOpSize);
}


/** Opcode 0x9b. (aka fwait) */
FNIEMOP_DEF(iemOp_wait)
{
    IEMOP_MNEMONIC("wait");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(0, 0);
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x9c. */
FNIEMOP_DEF(iemOp_pushf_Fv)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_pushf, pVCpu->iem.s.enmEffOpSize);
}


/** Opcode 0x9d. */
FNIEMOP_DEF(iemOp_popf_Fv)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_popf, pVCpu->iem.s.enmEffOpSize);
}


/** Opcode 0x9e. */
FNIEMOP_DEF(iemOp_sahf)
{
    IEMOP_MNEMONIC("sahf");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    if (   pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT
        && !IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLahfSahf)
        return IEMOP_RAISE_INVALID_OPCODE();
    IEM_MC_BEGIN(0, 2);
    IEM_MC_LOCAL(uint32_t, u32Flags);
    IEM_MC_LOCAL(uint32_t, EFlags);
    IEM_MC_FETCH_EFLAGS(EFlags);
    IEM_MC_FETCH_GREG_U8_ZX_U32(u32Flags, X86_GREG_xSP/*=AH*/);
    IEM_MC_AND_LOCAL_U32(u32Flags, X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF);
    IEM_MC_AND_LOCAL_U32(EFlags, UINT32_C(0xffffff00));
    IEM_MC_OR_LOCAL_U32(u32Flags, X86_EFL_1);
    IEM_MC_OR_2LOCS_U32(EFlags, u32Flags);
    IEM_MC_COMMIT_EFLAGS(EFlags);
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0x9f. */
FNIEMOP_DEF(iemOp_lahf)
{
    IEMOP_MNEMONIC("lahf");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    if (   pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT
        && !IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLahfSahf)
        return IEMOP_RAISE_INVALID_OPCODE();
    IEM_MC_BEGIN(0, 1);
    IEM_MC_LOCAL(uint8_t, u8Flags);
    IEM_MC_FETCH_EFLAGS_U8(u8Flags);
    IEM_MC_STORE_GREG_U8(X86_GREG_xSP/*=AH*/, u8Flags);
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/**
 * Macro used by iemOp_mov_Al_Ob, iemOp_mov_rAX_Ov, iemOp_mov_Ob_AL and
 * iemOp_mov_Ov_rAX to fetch the moffsXX bit of the opcode and fend of lock
 * prefixes.  Will return on failures.
 * @param   a_GCPtrMemOff   The variable to store the offset in.
 */
#define IEMOP_FETCH_MOFFS_XX(a_GCPtrMemOff) \
    do \
    { \
        switch (pVCpu->iem.s.enmEffAddrMode) \
        { \
            case IEMMODE_16BIT: \
                IEM_OPCODE_GET_NEXT_U16_ZX_U64(&(a_GCPtrMemOff)); \
                break; \
            case IEMMODE_32BIT: \
                IEM_OPCODE_GET_NEXT_U32_ZX_U64(&(a_GCPtrMemOff)); \
                break; \
            case IEMMODE_64BIT: \
                IEM_OPCODE_GET_NEXT_U64(&(a_GCPtrMemOff)); \
                break; \
            IEM_NOT_REACHED_DEFAULT_CASE_RET(); \
        } \
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX(); \
    } while (0)

/** Opcode 0xa0. */
FNIEMOP_DEF(iemOp_mov_Al_Ob)
{
    /*
     * Get the offset and fend of lock prefixes.
     */
    RTGCPTR GCPtrMemOff;
    IEMOP_FETCH_MOFFS_XX(GCPtrMemOff);

    /*
     * Fetch AL.
     */
    IEM_MC_BEGIN(0,1);
    IEM_MC_LOCAL(uint8_t, u8Tmp);
    IEM_MC_FETCH_MEM_U8(u8Tmp, pVCpu->iem.s.iEffSeg, GCPtrMemOff);
    IEM_MC_STORE_GREG_U8(X86_GREG_xAX, u8Tmp);
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xa1. */
FNIEMOP_DEF(iemOp_mov_rAX_Ov)
{
    /*
     * Get the offset and fend of lock prefixes.
     */
    IEMOP_MNEMONIC("mov rAX,Ov");
    RTGCPTR GCPtrMemOff;
    IEMOP_FETCH_MOFFS_XX(GCPtrMemOff);

    /*
     * Fetch rAX.
     */
    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(0,1);
            IEM_MC_LOCAL(uint16_t, u16Tmp);
            IEM_MC_FETCH_MEM_U16(u16Tmp, pVCpu->iem.s.iEffSeg, GCPtrMemOff);
            IEM_MC_STORE_GREG_U16(X86_GREG_xAX, u16Tmp);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(0,1);
            IEM_MC_LOCAL(uint32_t, u32Tmp);
            IEM_MC_FETCH_MEM_U32(u32Tmp, pVCpu->iem.s.iEffSeg, GCPtrMemOff);
            IEM_MC_STORE_GREG_U32(X86_GREG_xAX, u32Tmp);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(0,1);
            IEM_MC_LOCAL(uint64_t, u64Tmp);
            IEM_MC_FETCH_MEM_U64(u64Tmp, pVCpu->iem.s.iEffSeg, GCPtrMemOff);
            IEM_MC_STORE_GREG_U64(X86_GREG_xAX, u64Tmp);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/** Opcode 0xa2. */
FNIEMOP_DEF(iemOp_mov_Ob_AL)
{
    /*
     * Get the offset and fend of lock prefixes.
     */
    RTGCPTR GCPtrMemOff;
    IEMOP_FETCH_MOFFS_XX(GCPtrMemOff);

    /*
     * Store AL.
     */
    IEM_MC_BEGIN(0,1);
    IEM_MC_LOCAL(uint8_t, u8Tmp);
    IEM_MC_FETCH_GREG_U8(u8Tmp, X86_GREG_xAX);
    IEM_MC_STORE_MEM_U8(pVCpu->iem.s.iEffSeg, GCPtrMemOff, u8Tmp);
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xa3. */
FNIEMOP_DEF(iemOp_mov_Ov_rAX)
{
    /*
     * Get the offset and fend of lock prefixes.
     */
    RTGCPTR GCPtrMemOff;
    IEMOP_FETCH_MOFFS_XX(GCPtrMemOff);

    /*
     * Store rAX.
     */
    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(0,1);
            IEM_MC_LOCAL(uint16_t, u16Tmp);
            IEM_MC_FETCH_GREG_U16(u16Tmp, X86_GREG_xAX);
            IEM_MC_STORE_MEM_U16(pVCpu->iem.s.iEffSeg, GCPtrMemOff, u16Tmp);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(0,1);
            IEM_MC_LOCAL(uint32_t, u32Tmp);
            IEM_MC_FETCH_GREG_U32(u32Tmp, X86_GREG_xAX);
            IEM_MC_STORE_MEM_U32(pVCpu->iem.s.iEffSeg, GCPtrMemOff, u32Tmp);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(0,1);
            IEM_MC_LOCAL(uint64_t, u64Tmp);
            IEM_MC_FETCH_GREG_U64(u64Tmp, X86_GREG_xAX);
            IEM_MC_STORE_MEM_U64(pVCpu->iem.s.iEffSeg, GCPtrMemOff, u64Tmp);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}

/** Macro used by iemOp_movsb_Xb_Yb and iemOp_movswd_Xv_Yv */
#define IEM_MOVS_CASE(ValBits, AddrBits) \
        IEM_MC_BEGIN(0, 2); \
        IEM_MC_LOCAL(uint##ValBits##_t, uValue); \
        IEM_MC_LOCAL(RTGCPTR,           uAddr); \
        IEM_MC_FETCH_GREG_U##AddrBits##_ZX_U64(uAddr, X86_GREG_xSI); \
        IEM_MC_FETCH_MEM_U##ValBits(uValue, pVCpu->iem.s.iEffSeg, uAddr); \
        IEM_MC_FETCH_GREG_U##AddrBits##_ZX_U64(uAddr, X86_GREG_xDI); \
        IEM_MC_STORE_MEM_U##ValBits(X86_SREG_ES, uAddr, uValue); \
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_DF) { \
            IEM_MC_SUB_GREG_U##AddrBits(X86_GREG_xDI, ValBits / 8); \
            IEM_MC_SUB_GREG_U##AddrBits(X86_GREG_xSI, ValBits / 8); \
        } IEM_MC_ELSE() { \
            IEM_MC_ADD_GREG_U##AddrBits(X86_GREG_xDI, ValBits / 8); \
            IEM_MC_ADD_GREG_U##AddrBits(X86_GREG_xSI, ValBits / 8); \
        } IEM_MC_ENDIF(); \
        IEM_MC_ADVANCE_RIP(); \
        IEM_MC_END();

/** Opcode 0xa4. */
FNIEMOP_DEF(iemOp_movsb_Xb_Yb)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    /*
     * Use the C implementation if a repeat prefix is encountered.
     */
    if (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        IEMOP_MNEMONIC("rep movsb Xb,Yb");
        switch (pVCpu->iem.s.enmEffAddrMode)
        {
            case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_movs_op8_addr16, pVCpu->iem.s.iEffSeg);
            case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_movs_op8_addr32, pVCpu->iem.s.iEffSeg);
            case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_movs_op8_addr64, pVCpu->iem.s.iEffSeg);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    IEMOP_MNEMONIC("movsb Xb,Yb");

    /*
     * Sharing case implementation with movs[wdq] below.
     */
    switch (pVCpu->iem.s.enmEffAddrMode)
    {
        case IEMMODE_16BIT: IEM_MOVS_CASE(8, 16); break;
        case IEMMODE_32BIT: IEM_MOVS_CASE(8, 32); break;
        case IEMMODE_64BIT: IEM_MOVS_CASE(8, 64); break;
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
    return VINF_SUCCESS;
}


/** Opcode 0xa5. */
FNIEMOP_DEF(iemOp_movswd_Xv_Yv)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    /*
     * Use the C implementation if a repeat prefix is encountered.
     */
    if (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        IEMOP_MNEMONIC("rep movs Xv,Yv");
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_movs_op16_addr16, pVCpu->iem.s.iEffSeg);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_movs_op16_addr32, pVCpu->iem.s.iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_movs_op16_addr64, pVCpu->iem.s.iEffSeg);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            case IEMMODE_32BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_movs_op32_addr16, pVCpu->iem.s.iEffSeg);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_movs_op32_addr32, pVCpu->iem.s.iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_movs_op32_addr64, pVCpu->iem.s.iEffSeg);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
            case IEMMODE_64BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: AssertFailedReturn(VERR_IEM_IPE_6);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_movs_op64_addr32, pVCpu->iem.s.iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_rep_movs_op64_addr64, pVCpu->iem.s.iEffSeg);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    IEMOP_MNEMONIC("movs Xv,Yv");

    /*
     * Annoying double switch here.
     * Using ugly macro for implementing the cases, sharing it with movsb.
     */
    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
            switch (pVCpu->iem.s.enmEffAddrMode)
            {
                case IEMMODE_16BIT: IEM_MOVS_CASE(16, 16); break;
                case IEMMODE_32BIT: IEM_MOVS_CASE(16, 32); break;
                case IEMMODE_64BIT: IEM_MOVS_CASE(16, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;

        case IEMMODE_32BIT:
            switch (pVCpu->iem.s.enmEffAddrMode)
            {
                case IEMMODE_16BIT: IEM_MOVS_CASE(32, 16); break;
                case IEMMODE_32BIT: IEM_MOVS_CASE(32, 32); break;
                case IEMMODE_64BIT: IEM_MOVS_CASE(32, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;

        case IEMMODE_64BIT:
            switch (pVCpu->iem.s.enmEffAddrMode)
            {
                case IEMMODE_16BIT: AssertFailedReturn(VERR_IEM_IPE_1); /* cannot be encoded */ break;
                case IEMMODE_32BIT: IEM_MOVS_CASE(64, 32); break;
                case IEMMODE_64BIT: IEM_MOVS_CASE(64, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
    return VINF_SUCCESS;
}

#undef IEM_MOVS_CASE

/** Macro used by iemOp_cmpsb_Xb_Yb and iemOp_cmpswd_Xv_Yv */
#define IEM_CMPS_CASE(ValBits, AddrBits) \
        IEM_MC_BEGIN(3, 3); \
        IEM_MC_ARG(uint##ValBits##_t *, puValue1, 0); \
        IEM_MC_ARG(uint##ValBits##_t,   uValue2,  1); \
        IEM_MC_ARG(uint32_t *,          pEFlags,  2); \
        IEM_MC_LOCAL(uint##ValBits##_t, uValue1); \
        IEM_MC_LOCAL(RTGCPTR,           uAddr); \
        \
        IEM_MC_FETCH_GREG_U##AddrBits##_ZX_U64(uAddr, X86_GREG_xSI); \
        IEM_MC_FETCH_MEM_U##ValBits(uValue1, pVCpu->iem.s.iEffSeg, uAddr); \
        IEM_MC_FETCH_GREG_U##AddrBits##_ZX_U64(uAddr, X86_GREG_xDI); \
        IEM_MC_FETCH_MEM_U##ValBits(uValue2, X86_SREG_ES, uAddr); \
        IEM_MC_REF_LOCAL(puValue1, uValue1); \
        IEM_MC_REF_EFLAGS(pEFlags); \
        IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_cmp_u##ValBits, puValue1, uValue2, pEFlags); \
        \
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_DF) { \
            IEM_MC_SUB_GREG_U##AddrBits(X86_GREG_xDI, ValBits / 8); \
            IEM_MC_SUB_GREG_U##AddrBits(X86_GREG_xSI, ValBits / 8); \
        } IEM_MC_ELSE() { \
            IEM_MC_ADD_GREG_U##AddrBits(X86_GREG_xDI, ValBits / 8); \
            IEM_MC_ADD_GREG_U##AddrBits(X86_GREG_xSI, ValBits / 8); \
        } IEM_MC_ENDIF(); \
        IEM_MC_ADVANCE_RIP(); \
        IEM_MC_END(); \

/** Opcode 0xa6. */
FNIEMOP_DEF(iemOp_cmpsb_Xb_Yb)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    /*
     * Use the C implementation if a repeat prefix is encountered.
     */
    if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_REPZ)
    {
        IEMOP_MNEMONIC("repe cmps Xb,Yb");
        switch (pVCpu->iem.s.enmEffAddrMode)
        {
            case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repe_cmps_op8_addr16, pVCpu->iem.s.iEffSeg);
            case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repe_cmps_op8_addr32, pVCpu->iem.s.iEffSeg);
            case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repe_cmps_op8_addr64, pVCpu->iem.s.iEffSeg);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_REPNZ)
    {
        IEMOP_MNEMONIC("repe cmps Xb,Yb");
        switch (pVCpu->iem.s.enmEffAddrMode)
        {
            case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repne_cmps_op8_addr16, pVCpu->iem.s.iEffSeg);
            case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repne_cmps_op8_addr32, pVCpu->iem.s.iEffSeg);
            case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repne_cmps_op8_addr64, pVCpu->iem.s.iEffSeg);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    IEMOP_MNEMONIC("cmps Xb,Yb");

    /*
     * Sharing case implementation with cmps[wdq] below.
     */
    switch (pVCpu->iem.s.enmEffAddrMode)
    {
        case IEMMODE_16BIT: IEM_CMPS_CASE(8, 16); break;
        case IEMMODE_32BIT: IEM_CMPS_CASE(8, 32); break;
        case IEMMODE_64BIT: IEM_CMPS_CASE(8, 64); break;
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
    return VINF_SUCCESS;

}


/** Opcode 0xa7. */
FNIEMOP_DEF(iemOp_cmpswd_Xv_Yv)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    /*
     * Use the C implementation if a repeat prefix is encountered.
     */
    if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_REPZ)
    {
        IEMOP_MNEMONIC("repe cmps Xv,Yv");
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repe_cmps_op16_addr16, pVCpu->iem.s.iEffSeg);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repe_cmps_op16_addr32, pVCpu->iem.s.iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repe_cmps_op16_addr64, pVCpu->iem.s.iEffSeg);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            case IEMMODE_32BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repe_cmps_op32_addr16, pVCpu->iem.s.iEffSeg);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repe_cmps_op32_addr32, pVCpu->iem.s.iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repe_cmps_op32_addr64, pVCpu->iem.s.iEffSeg);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
            case IEMMODE_64BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: AssertFailedReturn(VERR_IEM_IPE_4);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repe_cmps_op64_addr32, pVCpu->iem.s.iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repe_cmps_op64_addr64, pVCpu->iem.s.iEffSeg);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }

    if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_REPNZ)
    {
        IEMOP_MNEMONIC("repne cmps Xv,Yv");
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repne_cmps_op16_addr16, pVCpu->iem.s.iEffSeg);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repne_cmps_op16_addr32, pVCpu->iem.s.iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repne_cmps_op16_addr64, pVCpu->iem.s.iEffSeg);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            case IEMMODE_32BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repne_cmps_op32_addr16, pVCpu->iem.s.iEffSeg);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repne_cmps_op32_addr32, pVCpu->iem.s.iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repne_cmps_op32_addr64, pVCpu->iem.s.iEffSeg);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
            case IEMMODE_64BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: AssertFailedReturn(VERR_IEM_IPE_2);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repne_cmps_op64_addr32, pVCpu->iem.s.iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_repne_cmps_op64_addr64, pVCpu->iem.s.iEffSeg);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }

    IEMOP_MNEMONIC("cmps Xv,Yv");

    /*
     * Annoying double switch here.
     * Using ugly macro for implementing the cases, sharing it with cmpsb.
     */
    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
            switch (pVCpu->iem.s.enmEffAddrMode)
            {
                case IEMMODE_16BIT: IEM_CMPS_CASE(16, 16); break;
                case IEMMODE_32BIT: IEM_CMPS_CASE(16, 32); break;
                case IEMMODE_64BIT: IEM_CMPS_CASE(16, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;

        case IEMMODE_32BIT:
            switch (pVCpu->iem.s.enmEffAddrMode)
            {
                case IEMMODE_16BIT: IEM_CMPS_CASE(32, 16); break;
                case IEMMODE_32BIT: IEM_CMPS_CASE(32, 32); break;
                case IEMMODE_64BIT: IEM_CMPS_CASE(32, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;

        case IEMMODE_64BIT:
            switch (pVCpu->iem.s.enmEffAddrMode)
            {
                case IEMMODE_16BIT: AssertFailedReturn(VERR_IEM_IPE_1); /* cannot be encoded */ break;
                case IEMMODE_32BIT: IEM_CMPS_CASE(64, 32); break;
                case IEMMODE_64BIT: IEM_CMPS_CASE(64, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
    return VINF_SUCCESS;

}

#undef IEM_CMPS_CASE

/** Opcode 0xa8. */
FNIEMOP_DEF(iemOp_test_AL_Ib)
{
    IEMOP_MNEMONIC("test al,Ib");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_AL_Ib, &g_iemAImpl_test);
}


/** Opcode 0xa9. */
FNIEMOP_DEF(iemOp_test_eAX_Iz)
{
    IEMOP_MNEMONIC("test rAX,Iz");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);
    return FNIEMOP_CALL_1(iemOpHlpBinaryOperator_rAX_Iz, &g_iemAImpl_test);
}


/** Macro used by iemOp_stosb_Yb_AL and iemOp_stoswd_Yv_eAX */
#define IEM_STOS_CASE(ValBits, AddrBits) \
        IEM_MC_BEGIN(0, 2); \
        IEM_MC_LOCAL(uint##ValBits##_t, uValue); \
        IEM_MC_LOCAL(RTGCPTR, uAddr); \
        IEM_MC_FETCH_GREG_U##ValBits(uValue, X86_GREG_xAX); \
        IEM_MC_FETCH_GREG_U##AddrBits##_ZX_U64(uAddr,  X86_GREG_xDI); \
        IEM_MC_STORE_MEM_U##ValBits(X86_SREG_ES, uAddr, uValue); \
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_DF) { \
            IEM_MC_SUB_GREG_U##AddrBits(X86_GREG_xDI, ValBits / 8); \
        } IEM_MC_ELSE() { \
            IEM_MC_ADD_GREG_U##AddrBits(X86_GREG_xDI, ValBits / 8); \
        } IEM_MC_ENDIF(); \
        IEM_MC_ADVANCE_RIP(); \
        IEM_MC_END(); \

/** Opcode 0xaa. */
FNIEMOP_DEF(iemOp_stosb_Yb_AL)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    /*
     * Use the C implementation if a repeat prefix is encountered.
     */
    if (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        IEMOP_MNEMONIC("rep stos Yb,al");
        switch (pVCpu->iem.s.enmEffAddrMode)
        {
            case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_stos_al_m16);
            case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_stos_al_m32);
            case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_stos_al_m64);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    IEMOP_MNEMONIC("stos Yb,al");

    /*
     * Sharing case implementation with stos[wdq] below.
     */
    switch (pVCpu->iem.s.enmEffAddrMode)
    {
        case IEMMODE_16BIT: IEM_STOS_CASE(8, 16); break;
        case IEMMODE_32BIT: IEM_STOS_CASE(8, 32); break;
        case IEMMODE_64BIT: IEM_STOS_CASE(8, 64); break;
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
    return VINF_SUCCESS;
}


/** Opcode 0xab. */
FNIEMOP_DEF(iemOp_stoswd_Yv_eAX)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    /*
     * Use the C implementation if a repeat prefix is encountered.
     */
    if (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        IEMOP_MNEMONIC("rep stos Yv,rAX");
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_stos_ax_m16);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_stos_ax_m32);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_stos_ax_m64);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            case IEMMODE_32BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_stos_eax_m16);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_stos_eax_m32);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_stos_eax_m64);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
            case IEMMODE_64BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: AssertFailedReturn(VERR_IEM_IPE_9);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_stos_rax_m32);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_stos_rax_m64);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    IEMOP_MNEMONIC("stos Yv,rAX");

    /*
     * Annoying double switch here.
     * Using ugly macro for implementing the cases, sharing it with stosb.
     */
    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
            switch (pVCpu->iem.s.enmEffAddrMode)
            {
                case IEMMODE_16BIT: IEM_STOS_CASE(16, 16); break;
                case IEMMODE_32BIT: IEM_STOS_CASE(16, 32); break;
                case IEMMODE_64BIT: IEM_STOS_CASE(16, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;

        case IEMMODE_32BIT:
            switch (pVCpu->iem.s.enmEffAddrMode)
            {
                case IEMMODE_16BIT: IEM_STOS_CASE(32, 16); break;
                case IEMMODE_32BIT: IEM_STOS_CASE(32, 32); break;
                case IEMMODE_64BIT: IEM_STOS_CASE(32, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;

        case IEMMODE_64BIT:
            switch (pVCpu->iem.s.enmEffAddrMode)
            {
                case IEMMODE_16BIT: AssertFailedReturn(VERR_IEM_IPE_1); /* cannot be encoded */ break;
                case IEMMODE_32BIT: IEM_STOS_CASE(64, 32); break;
                case IEMMODE_64BIT: IEM_STOS_CASE(64, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
    return VINF_SUCCESS;
}

#undef IEM_STOS_CASE

/** Macro used by iemOp_lodsb_AL_Xb and iemOp_lodswd_eAX_Xv */
#define IEM_LODS_CASE(ValBits, AddrBits) \
        IEM_MC_BEGIN(0, 2); \
        IEM_MC_LOCAL(uint##ValBits##_t, uValue); \
        IEM_MC_LOCAL(RTGCPTR, uAddr); \
        IEM_MC_FETCH_GREG_U##AddrBits##_ZX_U64(uAddr, X86_GREG_xSI); \
        IEM_MC_FETCH_MEM_U##ValBits(uValue, pVCpu->iem.s.iEffSeg, uAddr); \
        IEM_MC_STORE_GREG_U##ValBits(X86_GREG_xAX, uValue); \
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_DF) { \
            IEM_MC_SUB_GREG_U##AddrBits(X86_GREG_xSI, ValBits / 8); \
        } IEM_MC_ELSE() { \
            IEM_MC_ADD_GREG_U##AddrBits(X86_GREG_xSI, ValBits / 8); \
        } IEM_MC_ENDIF(); \
        IEM_MC_ADVANCE_RIP(); \
        IEM_MC_END();

/** Opcode 0xac. */
FNIEMOP_DEF(iemOp_lodsb_AL_Xb)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    /*
     * Use the C implementation if a repeat prefix is encountered.
     */
    if (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        IEMOP_MNEMONIC("rep lodsb al,Xb");
        switch (pVCpu->iem.s.enmEffAddrMode)
        {
            case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_lods_al_m16, pVCpu->iem.s.iEffSeg);
            case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_lods_al_m32, pVCpu->iem.s.iEffSeg);
            case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_lods_al_m64, pVCpu->iem.s.iEffSeg);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    IEMOP_MNEMONIC("lodsb al,Xb");

    /*
     * Sharing case implementation with stos[wdq] below.
     */
    switch (pVCpu->iem.s.enmEffAddrMode)
    {
        case IEMMODE_16BIT: IEM_LODS_CASE(8, 16); break;
        case IEMMODE_32BIT: IEM_LODS_CASE(8, 32); break;
        case IEMMODE_64BIT: IEM_LODS_CASE(8, 64); break;
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
    return VINF_SUCCESS;
}


/** Opcode 0xad. */
FNIEMOP_DEF(iemOp_lodswd_eAX_Xv)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    /*
     * Use the C implementation if a repeat prefix is encountered.
     */
    if (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ))
    {
        IEMOP_MNEMONIC("rep lods rAX,Xv");
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_lods_ax_m16, pVCpu->iem.s.iEffSeg);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_lods_ax_m32, pVCpu->iem.s.iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_lods_ax_m64, pVCpu->iem.s.iEffSeg);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            case IEMMODE_32BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_lods_eax_m16, pVCpu->iem.s.iEffSeg);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_lods_eax_m32, pVCpu->iem.s.iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_lods_eax_m64, pVCpu->iem.s.iEffSeg);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
            case IEMMODE_64BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: AssertFailedReturn(VERR_IEM_IPE_7);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_lods_rax_m32, pVCpu->iem.s.iEffSeg);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_lods_rax_m64, pVCpu->iem.s.iEffSeg);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    IEMOP_MNEMONIC("lods rAX,Xv");

    /*
     * Annoying double switch here.
     * Using ugly macro for implementing the cases, sharing it with lodsb.
     */
    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
            switch (pVCpu->iem.s.enmEffAddrMode)
            {
                case IEMMODE_16BIT: IEM_LODS_CASE(16, 16); break;
                case IEMMODE_32BIT: IEM_LODS_CASE(16, 32); break;
                case IEMMODE_64BIT: IEM_LODS_CASE(16, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;

        case IEMMODE_32BIT:
            switch (pVCpu->iem.s.enmEffAddrMode)
            {
                case IEMMODE_16BIT: IEM_LODS_CASE(32, 16); break;
                case IEMMODE_32BIT: IEM_LODS_CASE(32, 32); break;
                case IEMMODE_64BIT: IEM_LODS_CASE(32, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;

        case IEMMODE_64BIT:
            switch (pVCpu->iem.s.enmEffAddrMode)
            {
                case IEMMODE_16BIT: AssertFailedReturn(VERR_IEM_IPE_1); /* cannot be encoded */ break;
                case IEMMODE_32BIT: IEM_LODS_CASE(64, 32); break;
                case IEMMODE_64BIT: IEM_LODS_CASE(64, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
    return VINF_SUCCESS;
}

#undef IEM_LODS_CASE

/** Macro used by iemOp_scasb_AL_Xb and iemOp_scaswd_eAX_Xv */
#define IEM_SCAS_CASE(ValBits, AddrBits) \
        IEM_MC_BEGIN(3, 2); \
        IEM_MC_ARG(uint##ValBits##_t *, puRax,   0); \
        IEM_MC_ARG(uint##ValBits##_t,   uValue,  1); \
        IEM_MC_ARG(uint32_t *,          pEFlags, 2); \
        IEM_MC_LOCAL(RTGCPTR,           uAddr); \
        \
        IEM_MC_FETCH_GREG_U##AddrBits##_ZX_U64(uAddr, X86_GREG_xDI); \
        IEM_MC_FETCH_MEM_U##ValBits(uValue, X86_SREG_ES, uAddr); \
        IEM_MC_REF_GREG_U##ValBits(puRax, X86_GREG_xAX); \
        IEM_MC_REF_EFLAGS(pEFlags); \
        IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_cmp_u##ValBits, puRax, uValue, pEFlags); \
        \
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_DF) { \
            IEM_MC_SUB_GREG_U##AddrBits(X86_GREG_xDI, ValBits / 8); \
        } IEM_MC_ELSE() { \
            IEM_MC_ADD_GREG_U##AddrBits(X86_GREG_xDI, ValBits / 8); \
        } IEM_MC_ENDIF(); \
        IEM_MC_ADVANCE_RIP(); \
        IEM_MC_END();

/** Opcode 0xae. */
FNIEMOP_DEF(iemOp_scasb_AL_Xb)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    /*
     * Use the C implementation if a repeat prefix is encountered.
     */
    if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_REPZ)
    {
        IEMOP_MNEMONIC("repe scasb al,Xb");
        switch (pVCpu->iem.s.enmEffAddrMode)
        {
            case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_al_m16);
            case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_al_m32);
            case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_al_m64);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_REPNZ)
    {
        IEMOP_MNEMONIC("repne scasb al,Xb");
        switch (pVCpu->iem.s.enmEffAddrMode)
        {
            case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repne_scas_al_m16);
            case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repne_scas_al_m32);
            case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repne_scas_al_m64);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    IEMOP_MNEMONIC("scasb al,Xb");

    /*
     * Sharing case implementation with stos[wdq] below.
     */
    switch (pVCpu->iem.s.enmEffAddrMode)
    {
        case IEMMODE_16BIT: IEM_SCAS_CASE(8, 16); break;
        case IEMMODE_32BIT: IEM_SCAS_CASE(8, 32); break;
        case IEMMODE_64BIT: IEM_SCAS_CASE(8, 64); break;
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
    return VINF_SUCCESS;
}


/** Opcode 0xaf. */
FNIEMOP_DEF(iemOp_scaswd_eAX_Xv)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    /*
     * Use the C implementation if a repeat prefix is encountered.
     */
    if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_REPZ)
    {
        IEMOP_MNEMONIC("repe scas rAX,Xv");
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_ax_m16);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_ax_m32);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_ax_m64);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            case IEMMODE_32BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_eax_m16);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_eax_m32);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_eax_m64);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
            case IEMMODE_64BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: AssertFailedReturn(VERR_IEM_IPE_6); /** @todo It's this wrong, we can do 16-bit addressing in 64-bit mode, but not 32-bit. right? */
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_rax_m32);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repe_scas_rax_m64);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    if (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_REPNZ)
    {
        IEMOP_MNEMONIC("repne scas rAX,Xv");
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repne_scas_ax_m16);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repne_scas_ax_m32);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repne_scas_ax_m64);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            case IEMMODE_32BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repne_scas_eax_m16);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repne_scas_eax_m32);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repne_scas_eax_m64);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
            case IEMMODE_64BIT:
                switch (pVCpu->iem.s.enmEffAddrMode)
                {
                    case IEMMODE_16BIT: AssertFailedReturn(VERR_IEM_IPE_5);
                    case IEMMODE_32BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repne_scas_rax_m32);
                    case IEMMODE_64BIT: return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_repne_scas_rax_m64);
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    IEMOP_MNEMONIC("scas rAX,Xv");

    /*
     * Annoying double switch here.
     * Using ugly macro for implementing the cases, sharing it with scasb.
     */
    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
            switch (pVCpu->iem.s.enmEffAddrMode)
            {
                case IEMMODE_16BIT: IEM_SCAS_CASE(16, 16); break;
                case IEMMODE_32BIT: IEM_SCAS_CASE(16, 32); break;
                case IEMMODE_64BIT: IEM_SCAS_CASE(16, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;

        case IEMMODE_32BIT:
            switch (pVCpu->iem.s.enmEffAddrMode)
            {
                case IEMMODE_16BIT: IEM_SCAS_CASE(32, 16); break;
                case IEMMODE_32BIT: IEM_SCAS_CASE(32, 32); break;
                case IEMMODE_64BIT: IEM_SCAS_CASE(32, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;

        case IEMMODE_64BIT:
            switch (pVCpu->iem.s.enmEffAddrMode)
            {
                case IEMMODE_16BIT: AssertFailedReturn(VERR_IEM_IPE_1); /* cannot be encoded */ break;
                case IEMMODE_32BIT: IEM_SCAS_CASE(64, 32); break;
                case IEMMODE_64BIT: IEM_SCAS_CASE(64, 64); break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }
            break;
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
    return VINF_SUCCESS;
}

#undef IEM_SCAS_CASE

/**
 * Common 'mov r8, imm8' helper.
 */
FNIEMOP_DEF_1(iemOpCommonMov_r8_Ib, uint8_t, iReg)
{
    uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(0, 1);
    IEM_MC_LOCAL_CONST(uint8_t, u8Value,/*=*/ u8Imm);
    IEM_MC_STORE_GREG_U8(iReg, u8Value);
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();

    return VINF_SUCCESS;
}


/** Opcode 0xb0. */
FNIEMOP_DEF(iemOp_mov_AL_Ib)
{
    IEMOP_MNEMONIC("mov AL,Ib");
    return FNIEMOP_CALL_1(iemOpCommonMov_r8_Ib, X86_GREG_xAX | pVCpu->iem.s.uRexB);
}


/** Opcode 0xb1. */
FNIEMOP_DEF(iemOp_CL_Ib)
{
    IEMOP_MNEMONIC("mov CL,Ib");
    return FNIEMOP_CALL_1(iemOpCommonMov_r8_Ib, X86_GREG_xCX | pVCpu->iem.s.uRexB);
}


/** Opcode 0xb2. */
FNIEMOP_DEF(iemOp_DL_Ib)
{
    IEMOP_MNEMONIC("mov DL,Ib");
    return FNIEMOP_CALL_1(iemOpCommonMov_r8_Ib, X86_GREG_xDX | pVCpu->iem.s.uRexB);
}


/** Opcode 0xb3. */
FNIEMOP_DEF(iemOp_BL_Ib)
{
    IEMOP_MNEMONIC("mov BL,Ib");
    return FNIEMOP_CALL_1(iemOpCommonMov_r8_Ib, X86_GREG_xBX | pVCpu->iem.s.uRexB);
}


/** Opcode 0xb4. */
FNIEMOP_DEF(iemOp_mov_AH_Ib)
{
    IEMOP_MNEMONIC("mov AH,Ib");
    return FNIEMOP_CALL_1(iemOpCommonMov_r8_Ib, X86_GREG_xSP | pVCpu->iem.s.uRexB);
}


/** Opcode 0xb5. */
FNIEMOP_DEF(iemOp_CH_Ib)
{
    IEMOP_MNEMONIC("mov CH,Ib");
    return FNIEMOP_CALL_1(iemOpCommonMov_r8_Ib, X86_GREG_xBP | pVCpu->iem.s.uRexB);
}


/** Opcode 0xb6. */
FNIEMOP_DEF(iemOp_DH_Ib)
{
    IEMOP_MNEMONIC("mov DH,Ib");
    return FNIEMOP_CALL_1(iemOpCommonMov_r8_Ib, X86_GREG_xSI | pVCpu->iem.s.uRexB);
}


/** Opcode 0xb7. */
FNIEMOP_DEF(iemOp_BH_Ib)
{
    IEMOP_MNEMONIC("mov BH,Ib");
    return FNIEMOP_CALL_1(iemOpCommonMov_r8_Ib, X86_GREG_xDI | pVCpu->iem.s.uRexB);
}


/**
 * Common 'mov regX,immX' helper.
 */
FNIEMOP_DEF_1(iemOpCommonMov_Rv_Iv, uint8_t, iReg)
{
    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
        {
            uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL_CONST(uint16_t, u16Value,/*=*/ u16Imm);
            IEM_MC_STORE_GREG_U16(iReg, u16Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            break;
        }

        case IEMMODE_32BIT:
        {
            uint32_t u32Imm; IEM_OPCODE_GET_NEXT_U32(&u32Imm);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL_CONST(uint32_t, u32Value,/*=*/ u32Imm);
            IEM_MC_STORE_GREG_U32(iReg, u32Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            break;
        }
        case IEMMODE_64BIT:
        {
            uint64_t u64Imm; IEM_OPCODE_GET_NEXT_U64(&u64Imm); /* 64-bit immediate! */
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

            IEM_MC_BEGIN(0, 1);
            IEM_MC_LOCAL_CONST(uint64_t, u64Value,/*=*/ u64Imm);
            IEM_MC_STORE_GREG_U64(iReg, u64Value);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            break;
        }
    }

    return VINF_SUCCESS;
}


/** Opcode 0xb8. */
FNIEMOP_DEF(iemOp_eAX_Iv)
{
    IEMOP_MNEMONIC("mov rAX,IV");
    return FNIEMOP_CALL_1(iemOpCommonMov_Rv_Iv, X86_GREG_xAX | pVCpu->iem.s.uRexB);
}


/** Opcode 0xb9. */
FNIEMOP_DEF(iemOp_eCX_Iv)
{
    IEMOP_MNEMONIC("mov rCX,IV");
    return FNIEMOP_CALL_1(iemOpCommonMov_Rv_Iv, X86_GREG_xCX | pVCpu->iem.s.uRexB);
}


/** Opcode 0xba. */
FNIEMOP_DEF(iemOp_eDX_Iv)
{
    IEMOP_MNEMONIC("mov rDX,IV");
    return FNIEMOP_CALL_1(iemOpCommonMov_Rv_Iv, X86_GREG_xDX | pVCpu->iem.s.uRexB);
}


/** Opcode 0xbb. */
FNIEMOP_DEF(iemOp_eBX_Iv)
{
    IEMOP_MNEMONIC("mov rBX,IV");
    return FNIEMOP_CALL_1(iemOpCommonMov_Rv_Iv, X86_GREG_xBX | pVCpu->iem.s.uRexB);
}


/** Opcode 0xbc. */
FNIEMOP_DEF(iemOp_eSP_Iv)
{
    IEMOP_MNEMONIC("mov rSP,IV");
    return FNIEMOP_CALL_1(iemOpCommonMov_Rv_Iv, X86_GREG_xSP | pVCpu->iem.s.uRexB);
}


/** Opcode 0xbd. */
FNIEMOP_DEF(iemOp_eBP_Iv)
{
    IEMOP_MNEMONIC("mov rBP,IV");
    return FNIEMOP_CALL_1(iemOpCommonMov_Rv_Iv, X86_GREG_xBP | pVCpu->iem.s.uRexB);
}


/** Opcode 0xbe. */
FNIEMOP_DEF(iemOp_eSI_Iv)
{
    IEMOP_MNEMONIC("mov rSI,IV");
    return FNIEMOP_CALL_1(iemOpCommonMov_Rv_Iv, X86_GREG_xSI | pVCpu->iem.s.uRexB);
}


/** Opcode 0xbf. */
FNIEMOP_DEF(iemOp_eDI_Iv)
{
    IEMOP_MNEMONIC("mov rDI,IV");
    return FNIEMOP_CALL_1(iemOpCommonMov_Rv_Iv, X86_GREG_xDI | pVCpu->iem.s.uRexB);
}


/** Opcode 0xc0. */
FNIEMOP_DEF(iemOp_Grp2_Eb_Ib)
{
    IEMOP_HLP_MIN_186();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    PCIEMOPSHIFTSIZES pImpl;
    switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
    {
        case 0: pImpl = &g_iemAImpl_rol; IEMOP_MNEMONIC("rol Eb,Ib"); break;
        case 1: pImpl = &g_iemAImpl_ror; IEMOP_MNEMONIC("ror Eb,Ib"); break;
        case 2: pImpl = &g_iemAImpl_rcl; IEMOP_MNEMONIC("rcl Eb,Ib"); break;
        case 3: pImpl = &g_iemAImpl_rcr; IEMOP_MNEMONIC("rcr Eb,Ib"); break;
        case 4: pImpl = &g_iemAImpl_shl; IEMOP_MNEMONIC("shl Eb,Ib"); break;
        case 5: pImpl = &g_iemAImpl_shr; IEMOP_MNEMONIC("shr Eb,Ib"); break;
        case 7: pImpl = &g_iemAImpl_sar; IEMOP_MNEMONIC("sar Eb,Ib"); break;
        case 6: return IEMOP_RAISE_INVALID_OPCODE();
        IEM_NOT_REACHED_DEFAULT_CASE_RET(); /* gcc maybe stupid */
    }
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_OF | X86_EFL_AF);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register */
        uint8_t cShift; IEM_OPCODE_GET_NEXT_U8(&cShift);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(3, 0);
        IEM_MC_ARG(uint8_t *,       pu8Dst,            0);
        IEM_MC_ARG_CONST(uint8_t,   cShiftArg, cShift, 1);
        IEM_MC_ARG(uint32_t *,      pEFlags,           2);
        IEM_MC_REF_GREG_U8(pu8Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        IEM_MC_REF_EFLAGS(pEFlags);
        IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU8, pu8Dst, cShiftArg, pEFlags);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory */
        IEM_MC_BEGIN(3, 2);
        IEM_MC_ARG(uint8_t *,   pu8Dst,    0);
        IEM_MC_ARG(uint8_t,     cShiftArg,  1);
        IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 2);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 1);
        uint8_t cShift; IEM_OPCODE_GET_NEXT_U8(&cShift);
        IEM_MC_ASSIGN(cShiftArg, cShift);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_MEM_MAP(pu8Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
        IEM_MC_FETCH_EFLAGS(EFlags);
        IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU8, pu8Dst, cShiftArg, pEFlags);

        IEM_MC_MEM_COMMIT_AND_UNMAP(pu8Dst, IEM_ACCESS_DATA_RW);
        IEM_MC_COMMIT_EFLAGS(EFlags);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0xc1. */
FNIEMOP_DEF(iemOp_Grp2_Ev_Ib)
{
    IEMOP_HLP_MIN_186();
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    PCIEMOPSHIFTSIZES pImpl;
    switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
    {
        case 0: pImpl = &g_iemAImpl_rol; IEMOP_MNEMONIC("rol Ev,Ib"); break;
        case 1: pImpl = &g_iemAImpl_ror; IEMOP_MNEMONIC("ror Ev,Ib"); break;
        case 2: pImpl = &g_iemAImpl_rcl; IEMOP_MNEMONIC("rcl Ev,Ib"); break;
        case 3: pImpl = &g_iemAImpl_rcr; IEMOP_MNEMONIC("rcr Ev,Ib"); break;
        case 4: pImpl = &g_iemAImpl_shl; IEMOP_MNEMONIC("shl Ev,Ib"); break;
        case 5: pImpl = &g_iemAImpl_shr; IEMOP_MNEMONIC("shr Ev,Ib"); break;
        case 7: pImpl = &g_iemAImpl_sar; IEMOP_MNEMONIC("sar Ev,Ib"); break;
        case 6: return IEMOP_RAISE_INVALID_OPCODE();
        IEM_NOT_REACHED_DEFAULT_CASE_RET(); /* gcc maybe stupid */
    }
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_OF | X86_EFL_AF);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register */
        uint8_t cShift; IEM_OPCODE_GET_NEXT_U8(&cShift);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint16_t *,      pu16Dst,           0);
                IEM_MC_ARG_CONST(uint8_t,   cShiftArg, cShift, 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,           2);
                IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU16, pu16Dst, cShiftArg, pEFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint32_t *,      pu32Dst,           0);
                IEM_MC_ARG_CONST(uint8_t,   cShiftArg, cShift, 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,           2);
                IEM_MC_REF_GREG_U32(pu32Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, cShiftArg, pEFlags);
                IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32Dst);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint64_t *,      pu64Dst,           0);
                IEM_MC_ARG_CONST(uint8_t,   cShiftArg, cShift, 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,           2);
                IEM_MC_REF_GREG_U64(pu64Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU64, pu64Dst, cShiftArg, pEFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        /* memory */
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint16_t *,  pu16Dst,    0);
                IEM_MC_ARG(uint8_t,     cShiftArg,  1);
                IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 1);
                uint8_t cShift; IEM_OPCODE_GET_NEXT_U8(&cShift);
                IEM_MC_ASSIGN(cShiftArg, cShift);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MEM_MAP(pu16Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU16, pu16Dst, cShiftArg, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu16Dst, IEM_ACCESS_DATA_RW);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint32_t *,  pu32Dst,    0);
                IEM_MC_ARG(uint8_t,     cShiftArg,  1);
                IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 1);
                uint8_t cShift; IEM_OPCODE_GET_NEXT_U8(&cShift);
                IEM_MC_ASSIGN(cShiftArg, cShift);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MEM_MAP(pu32Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, cShiftArg, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu32Dst, IEM_ACCESS_DATA_RW);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint64_t *,  pu64Dst,    0);
                IEM_MC_ARG(uint8_t,     cShiftArg,  1);
                IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 1);
                uint8_t cShift; IEM_OPCODE_GET_NEXT_U8(&cShift);
                IEM_MC_ASSIGN(cShiftArg, cShift);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MEM_MAP(pu64Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU64, pu64Dst, cShiftArg, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu64Dst, IEM_ACCESS_DATA_RW);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


/** Opcode 0xc2. */
FNIEMOP_DEF(iemOp_retn_Iw)
{
    IEMOP_MNEMONIC("retn Iw");
    uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_retn, pVCpu->iem.s.enmEffOpSize, u16Imm);
}


/** Opcode 0xc3. */
FNIEMOP_DEF(iemOp_retn)
{
    IEMOP_MNEMONIC("retn");
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_retn, pVCpu->iem.s.enmEffOpSize, 0);
}


/** Opcode 0xc4. */
FNIEMOP_DEF(iemOp_les_Gv_Mp_vex2)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if (   pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT
        || (bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        IEMOP_MNEMONIC("2-byte-vex");
        /* The LES instruction is invalid 64-bit mode. In legacy and
           compatability mode it is invalid with MOD=3.
           The use as a VEX prefix is made possible by assigning the inverted
           REX.R to the top MOD bit, and the top bit in the inverted register
           specifier to the bottom MOD bit, thereby effectively limiting 32-bit
           to accessing registers 0..7 in this VEX form. */
        /** @todo VEX: Just use new tables for it. */
        return IEMOP_RAISE_INVALID_OPCODE();
    }
    IEMOP_MNEMONIC("les Gv,Mp");
    return FNIEMOP_CALL_2(iemOpCommonLoadSRegAndGreg, X86_SREG_ES, bRm);
}


/** Opcode 0xc5. */
FNIEMOP_DEF(iemOp_lds_Gv_Mp_vex3)
{
    /* The LDS instruction is invalid 64-bit mode. In legacy and
       compatability mode it is invalid with MOD=3.
       The use as a VEX prefix is made possible by assigning the inverted
       REX.R and REX.X to the two MOD bits, since the REX bits are ignored
       outside of 64-bit mode.  VEX is not available in real or v86 mode. */
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if (pVCpu->iem.s.enmCpuMode != IEMMODE_64BIT)
    {
        if ((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT))
        {
            IEMOP_MNEMONIC("lds Gv,Mp");
            return FNIEMOP_CALL_2(iemOpCommonLoadSRegAndGreg, X86_SREG_DS, bRm);
        }
        IEMOP_HLP_NO_REAL_OR_V86_MODE();
    }

    IEMOP_MNEMONIC("3-byte-vex");
    /** @todo Test when exctly the VEX conformance checks kick in during
     * instruction decoding and fetching (using \#PF). */
    uint8_t bVex1;   IEM_OPCODE_GET_NEXT_U8(&bVex1);
    uint8_t bVex2;   IEM_OPCODE_GET_NEXT_U8(&bVex2);
    uint8_t bOpcode; IEM_OPCODE_GET_NEXT_U8(&bOpcode);
#if 0 /* will make sense of this next week... */
    if (   !(pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ | IEM_OP_PRF_REPZ | IEM_OP_PRF_SIZE_OP | IEM_OP_PRF_REX))
        &&
        )
    {

    }
#endif

    /** @todo VEX: Just use new tables for it. */
    return IEMOP_RAISE_INVALID_OPCODE();
}


/** Opcode 0xc6. */
FNIEMOP_DEF(iemOp_Grp11_Eb_Ib)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_REG_MASK) != (0 << X86_MODRM_REG_SHIFT)) /* only mov Eb,Ib in this group. */
        return IEMOP_RAISE_INVALID_OPCODE();
    IEMOP_MNEMONIC("mov Eb,Ib");

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register access */
        uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(0, 0);
        IEM_MC_STORE_GREG_U8((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u8Imm);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory access. */
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 1);
        uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_STORE_MEM_U8(pVCpu->iem.s.iEffSeg, GCPtrEffDst, u8Imm);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0xc7. */
FNIEMOP_DEF(iemOp_Grp11_Ev_Iz)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_REG_MASK) != (0 << X86_MODRM_REG_SHIFT)) /* only mov Eb,Ib in this group. */
        return IEMOP_RAISE_INVALID_OPCODE();
    IEMOP_MNEMONIC("mov Ev,Iz");

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register access */
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 0);
                uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_STORE_GREG_U16((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u16Imm);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 0);
                uint32_t u32Imm; IEM_OPCODE_GET_NEXT_U32(&u32Imm);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_STORE_GREG_U32((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u32Imm);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 0);
                uint64_t u64Imm; IEM_OPCODE_GET_NEXT_S32_SX_U64(&u64Imm);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_STORE_GREG_U64((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB, u64Imm);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        /* memory access. */
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 2);
                uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_STORE_MEM_U16(pVCpu->iem.s.iEffSeg, GCPtrEffDst, u16Imm);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 4);
                uint32_t u32Imm; IEM_OPCODE_GET_NEXT_U32(&u32Imm);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_STORE_MEM_U32(pVCpu->iem.s.iEffSeg, GCPtrEffDst, u32Imm);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 4);
                uint64_t u64Imm; IEM_OPCODE_GET_NEXT_S32_SX_U64(&u64Imm);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_STORE_MEM_U64(pVCpu->iem.s.iEffSeg, GCPtrEffDst, u64Imm);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}




/** Opcode 0xc8. */
FNIEMOP_DEF(iemOp_enter_Iw_Ib)
{
    IEMOP_MNEMONIC("enter Iw,Ib");
    IEMOP_HLP_MIN_186();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    uint16_t cbFrame;        IEM_OPCODE_GET_NEXT_U16(&cbFrame);
    uint8_t  u8NestingLevel; IEM_OPCODE_GET_NEXT_U8(&u8NestingLevel);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_3(iemCImpl_enter, pVCpu->iem.s.enmEffOpSize, cbFrame, u8NestingLevel);
}


/** Opcode 0xc9. */
FNIEMOP_DEF(iemOp_leave)
{
    IEMOP_MNEMONIC("retn");
    IEMOP_HLP_MIN_186();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_leave, pVCpu->iem.s.enmEffOpSize);
}


/** Opcode 0xca. */
FNIEMOP_DEF(iemOp_retf_Iw)
{
    IEMOP_MNEMONIC("retf Iw");
    uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_retf, pVCpu->iem.s.enmEffOpSize, u16Imm);
}


/** Opcode 0xcb. */
FNIEMOP_DEF(iemOp_retf)
{
    IEMOP_MNEMONIC("retf");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_retf, pVCpu->iem.s.enmEffOpSize, 0);
}


/** Opcode 0xcc. */
FNIEMOP_DEF(iemOp_int_3)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_int, X86_XCPT_BP, true /*fIsBpInstr*/);
}


/** Opcode 0xcd. */
FNIEMOP_DEF(iemOp_int_Ib)
{
    uint8_t u8Int; IEM_OPCODE_GET_NEXT_U8(&u8Int);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_int, u8Int, false /*fIsBpInstr*/);
}


/** Opcode 0xce. */
FNIEMOP_DEF(iemOp_into)
{
    IEMOP_MNEMONIC("into");
    IEMOP_HLP_NO_64BIT();

    IEM_MC_BEGIN(2, 0);
    IEM_MC_ARG_CONST(uint8_t,   u8Int,      /*=*/ X86_XCPT_OF, 0);
    IEM_MC_ARG_CONST(bool,      fIsBpInstr, /*=*/ false, 1);
    IEM_MC_CALL_CIMPL_2(iemCImpl_int, u8Int, fIsBpInstr);
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xcf. */
FNIEMOP_DEF(iemOp_iret)
{
    IEMOP_MNEMONIC("iret");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_iret, pVCpu->iem.s.enmEffOpSize);
}


/** Opcode 0xd0. */
FNIEMOP_DEF(iemOp_Grp2_Eb_1)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    PCIEMOPSHIFTSIZES pImpl;
    switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
    {
        case 0: pImpl = &g_iemAImpl_rol; IEMOP_MNEMONIC("rol Eb,1"); break;
        case 1: pImpl = &g_iemAImpl_ror; IEMOP_MNEMONIC("ror Eb,1"); break;
        case 2: pImpl = &g_iemAImpl_rcl; IEMOP_MNEMONIC("rcl Eb,1"); break;
        case 3: pImpl = &g_iemAImpl_rcr; IEMOP_MNEMONIC("rcr Eb,1"); break;
        case 4: pImpl = &g_iemAImpl_shl; IEMOP_MNEMONIC("shl Eb,1"); break;
        case 5: pImpl = &g_iemAImpl_shr; IEMOP_MNEMONIC("shr Eb,1"); break;
        case 7: pImpl = &g_iemAImpl_sar; IEMOP_MNEMONIC("sar Eb,1"); break;
        case 6: return IEMOP_RAISE_INVALID_OPCODE();
        IEM_NOT_REACHED_DEFAULT_CASE_RET(); /* gcc maybe, well... */
    }
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_OF | X86_EFL_AF);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(3, 0);
        IEM_MC_ARG(uint8_t *,       pu8Dst,             0);
        IEM_MC_ARG_CONST(uint8_t,   cShiftArg,/*=*/1,   1);
        IEM_MC_ARG(uint32_t *,      pEFlags,            2);
        IEM_MC_REF_GREG_U8(pu8Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        IEM_MC_REF_EFLAGS(pEFlags);
        IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU8, pu8Dst, cShiftArg, pEFlags);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory */
        IEM_MC_BEGIN(3, 2);
        IEM_MC_ARG(uint8_t *,       pu8Dst,             0);
        IEM_MC_ARG_CONST(uint8_t,   cShiftArg,/*=*/1,   1);
        IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags,        2);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_MEM_MAP(pu8Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
        IEM_MC_FETCH_EFLAGS(EFlags);
        IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU8, pu8Dst, cShiftArg, pEFlags);

        IEM_MC_MEM_COMMIT_AND_UNMAP(pu8Dst, IEM_ACCESS_DATA_RW);
        IEM_MC_COMMIT_EFLAGS(EFlags);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}



/** Opcode 0xd1. */
FNIEMOP_DEF(iemOp_Grp2_Ev_1)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    PCIEMOPSHIFTSIZES pImpl;
    switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
    {
        case 0: pImpl = &g_iemAImpl_rol; IEMOP_MNEMONIC("rol Ev,1"); break;
        case 1: pImpl = &g_iemAImpl_ror; IEMOP_MNEMONIC("ror Ev,1"); break;
        case 2: pImpl = &g_iemAImpl_rcl; IEMOP_MNEMONIC("rcl Ev,1"); break;
        case 3: pImpl = &g_iemAImpl_rcr; IEMOP_MNEMONIC("rcr Ev,1"); break;
        case 4: pImpl = &g_iemAImpl_shl; IEMOP_MNEMONIC("shl Ev,1"); break;
        case 5: pImpl = &g_iemAImpl_shr; IEMOP_MNEMONIC("shr Ev,1"); break;
        case 7: pImpl = &g_iemAImpl_sar; IEMOP_MNEMONIC("sar Ev,1"); break;
        case 6: return IEMOP_RAISE_INVALID_OPCODE();
        IEM_NOT_REACHED_DEFAULT_CASE_RET(); /* gcc maybe, well... */
    }
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_OF | X86_EFL_AF);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint16_t *,      pu16Dst,           0);
                IEM_MC_ARG_CONST(uint8_t,   cShiftArg,/*=1*/1, 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,           2);
                IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU16, pu16Dst, cShiftArg, pEFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint32_t *,      pu32Dst,           0);
                IEM_MC_ARG_CONST(uint8_t,   cShiftArg,/*=1*/1, 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,           2);
                IEM_MC_REF_GREG_U32(pu32Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, cShiftArg, pEFlags);
                IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32Dst);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint64_t *,      pu64Dst,           0);
                IEM_MC_ARG_CONST(uint8_t,   cShiftArg,/*=1*/1, 1);
                IEM_MC_ARG(uint32_t *,      pEFlags,           2);
                IEM_MC_REF_GREG_U64(pu64Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU64, pu64Dst, cShiftArg, pEFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        /* memory */
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint16_t *,      pu16Dst,            0);
                IEM_MC_ARG_CONST(uint8_t,   cShiftArg,/*=1*/1,  1);
                IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags,        2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MEM_MAP(pu16Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU16, pu16Dst, cShiftArg, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu16Dst, IEM_ACCESS_DATA_RW);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint32_t *,      pu32Dst,            0);
                IEM_MC_ARG_CONST(uint8_t,   cShiftArg,/*=1*/1,  1);
                IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags,        2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MEM_MAP(pu32Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, cShiftArg, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu32Dst, IEM_ACCESS_DATA_RW);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint64_t *,      pu64Dst,            0);
                IEM_MC_ARG_CONST(uint8_t,   cShiftArg,/*=1*/1,  1);
                IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags,        2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MEM_MAP(pu64Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU64, pu64Dst, cShiftArg, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu64Dst, IEM_ACCESS_DATA_RW);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


/** Opcode 0xd2. */
FNIEMOP_DEF(iemOp_Grp2_Eb_CL)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    PCIEMOPSHIFTSIZES pImpl;
    switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
    {
        case 0: pImpl = &g_iemAImpl_rol; IEMOP_MNEMONIC("rol Eb,CL"); break;
        case 1: pImpl = &g_iemAImpl_ror; IEMOP_MNEMONIC("ror Eb,CL"); break;
        case 2: pImpl = &g_iemAImpl_rcl; IEMOP_MNEMONIC("rcl Eb,CL"); break;
        case 3: pImpl = &g_iemAImpl_rcr; IEMOP_MNEMONIC("rcr Eb,CL"); break;
        case 4: pImpl = &g_iemAImpl_shl; IEMOP_MNEMONIC("shl Eb,CL"); break;
        case 5: pImpl = &g_iemAImpl_shr; IEMOP_MNEMONIC("shr Eb,CL"); break;
        case 7: pImpl = &g_iemAImpl_sar; IEMOP_MNEMONIC("sar Eb,CL"); break;
        case 6: return IEMOP_RAISE_INVALID_OPCODE();
        IEM_NOT_REACHED_DEFAULT_CASE_RET(); /* gcc, grr. */
    }
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_OF | X86_EFL_AF);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(3, 0);
        IEM_MC_ARG(uint8_t *,   pu8Dst,     0);
        IEM_MC_ARG(uint8_t,     cShiftArg,  1);
        IEM_MC_ARG(uint32_t *,  pEFlags,    2);
        IEM_MC_REF_GREG_U8(pu8Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        IEM_MC_FETCH_GREG_U8(cShiftArg, X86_GREG_xCX);
        IEM_MC_REF_EFLAGS(pEFlags);
        IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU8, pu8Dst, cShiftArg, pEFlags);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory */
        IEM_MC_BEGIN(3, 2);
        IEM_MC_ARG(uint8_t *,   pu8Dst,          0);
        IEM_MC_ARG(uint8_t,     cShiftArg,       1);
        IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 2);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_FETCH_GREG_U8(cShiftArg, X86_GREG_xCX);
        IEM_MC_MEM_MAP(pu8Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
        IEM_MC_FETCH_EFLAGS(EFlags);
        IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU8, pu8Dst, cShiftArg, pEFlags);

        IEM_MC_MEM_COMMIT_AND_UNMAP(pu8Dst, IEM_ACCESS_DATA_RW);
        IEM_MC_COMMIT_EFLAGS(EFlags);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0xd3. */
FNIEMOP_DEF(iemOp_Grp2_Ev_CL)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    PCIEMOPSHIFTSIZES pImpl;
    switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
    {
        case 0: pImpl = &g_iemAImpl_rol; IEMOP_MNEMONIC("rol Ev,CL"); break;
        case 1: pImpl = &g_iemAImpl_ror; IEMOP_MNEMONIC("ror Ev,CL"); break;
        case 2: pImpl = &g_iemAImpl_rcl; IEMOP_MNEMONIC("rcl Ev,CL"); break;
        case 3: pImpl = &g_iemAImpl_rcr; IEMOP_MNEMONIC("rcr Ev,CL"); break;
        case 4: pImpl = &g_iemAImpl_shl; IEMOP_MNEMONIC("shl Ev,CL"); break;
        case 5: pImpl = &g_iemAImpl_shr; IEMOP_MNEMONIC("shr Ev,CL"); break;
        case 7: pImpl = &g_iemAImpl_sar; IEMOP_MNEMONIC("sar Ev,CL"); break;
        case 6: return IEMOP_RAISE_INVALID_OPCODE();
        IEM_NOT_REACHED_DEFAULT_CASE_RET(); /* gcc maybe stupid */
    }
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_OF | X86_EFL_AF);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint16_t *,      pu16Dst,    0);
                IEM_MC_ARG(uint8_t,         cShiftArg,  1);
                IEM_MC_ARG(uint32_t *,      pEFlags,    2);
                IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_FETCH_GREG_U8(cShiftArg, X86_GREG_xCX);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU16, pu16Dst, cShiftArg, pEFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint32_t *,      pu32Dst,    0);
                IEM_MC_ARG(uint8_t,         cShiftArg,  1);
                IEM_MC_ARG(uint32_t *,      pEFlags,    2);
                IEM_MC_REF_GREG_U32(pu32Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_FETCH_GREG_U8(cShiftArg, X86_GREG_xCX);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, cShiftArg, pEFlags);
                IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32Dst);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint64_t *,      pu64Dst,    0);
                IEM_MC_ARG(uint8_t,         cShiftArg,  1);
                IEM_MC_ARG(uint32_t *,      pEFlags,    2);
                IEM_MC_REF_GREG_U64(pu64Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_FETCH_GREG_U8(cShiftArg, X86_GREG_xCX);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU64, pu64Dst, cShiftArg, pEFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        /* memory */
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint16_t *,  pu16Dst,    0);
                IEM_MC_ARG(uint8_t,     cShiftArg,  1);
                IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_GREG_U8(cShiftArg, X86_GREG_xCX);
                IEM_MC_MEM_MAP(pu16Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU16, pu16Dst, cShiftArg, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu16Dst, IEM_ACCESS_DATA_RW);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint32_t *,  pu32Dst,    0);
                IEM_MC_ARG(uint8_t,     cShiftArg,  1);
                IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_GREG_U8(cShiftArg, X86_GREG_xCX);
                IEM_MC_MEM_MAP(pu32Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU32, pu32Dst, cShiftArg, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu32Dst, IEM_ACCESS_DATA_RW);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint64_t *,  pu64Dst,    0);
                IEM_MC_ARG(uint8_t,     cShiftArg,  1);
                IEM_MC_ARG_LOCAL_EFLAGS(pEFlags, EFlags, 2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_GREG_U8(cShiftArg, X86_GREG_xCX);
                IEM_MC_MEM_MAP(pu64Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_CALL_VOID_AIMPL_3(pImpl->pfnNormalU64, pu64Dst, cShiftArg, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu64Dst, IEM_ACCESS_DATA_RW);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}

/** Opcode 0xd4. */
FNIEMOP_DEF(iemOp_aam_Ib)
{
    IEMOP_MNEMONIC("aam Ib");
    uint8_t bImm; IEM_OPCODE_GET_NEXT_U8(&bImm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_NO_64BIT();
    if (!bImm)
        return IEMOP_RAISE_DIVIDE_ERROR();
    return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_aam, bImm);
}


/** Opcode 0xd5. */
FNIEMOP_DEF(iemOp_aad_Ib)
{
    IEMOP_MNEMONIC("aad Ib");
    uint8_t bImm; IEM_OPCODE_GET_NEXT_U8(&bImm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_NO_64BIT();
    return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_aad, bImm);
}


/** Opcode 0xd6. */
FNIEMOP_DEF(iemOp_salc)
{
    IEMOP_MNEMONIC("salc");
    IEMOP_HLP_MIN_286(); /* (undocument at the time) */
    uint8_t bImm; IEM_OPCODE_GET_NEXT_U8(&bImm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_NO_64BIT();

    IEM_MC_BEGIN(0, 0);
    IEM_MC_IF_EFL_BIT_SET(X86_EFL_CF) {
        IEM_MC_STORE_GREG_U8_CONST(X86_GREG_xAX, 0xff);
    } IEM_MC_ELSE() {
        IEM_MC_STORE_GREG_U8_CONST(X86_GREG_xAX, 0x00);
    } IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xd7. */
FNIEMOP_DEF(iemOp_xlat)
{
    IEMOP_MNEMONIC("xlat");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    switch (pVCpu->iem.s.enmEffAddrMode)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(2, 0);
            IEM_MC_LOCAL(uint8_t,  u8Tmp);
            IEM_MC_LOCAL(uint16_t, u16Addr);
            IEM_MC_FETCH_GREG_U8_ZX_U16(u16Addr, X86_GREG_xAX);
            IEM_MC_ADD_GREG_U16_TO_LOCAL(u16Addr, X86_GREG_xBX);
            IEM_MC_FETCH_MEM16_U8(u8Tmp, pVCpu->iem.s.iEffSeg, u16Addr);
            IEM_MC_STORE_GREG_U8(X86_GREG_xAX, u8Tmp);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(2, 0);
            IEM_MC_LOCAL(uint8_t,  u8Tmp);
            IEM_MC_LOCAL(uint32_t, u32Addr);
            IEM_MC_FETCH_GREG_U8_ZX_U32(u32Addr, X86_GREG_xAX);
            IEM_MC_ADD_GREG_U32_TO_LOCAL(u32Addr, X86_GREG_xBX);
            IEM_MC_FETCH_MEM32_U8(u8Tmp, pVCpu->iem.s.iEffSeg, u32Addr);
            IEM_MC_STORE_GREG_U8(X86_GREG_xAX, u8Tmp);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(2, 0);
            IEM_MC_LOCAL(uint8_t,  u8Tmp);
            IEM_MC_LOCAL(uint64_t, u64Addr);
            IEM_MC_FETCH_GREG_U8_ZX_U64(u64Addr, X86_GREG_xAX);
            IEM_MC_ADD_GREG_U64_TO_LOCAL(u64Addr, X86_GREG_xBX);
            IEM_MC_FETCH_MEM_U8(u8Tmp, pVCpu->iem.s.iEffSeg, u64Addr);
            IEM_MC_STORE_GREG_U8(X86_GREG_xAX, u8Tmp);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

         IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/**
 * Common worker for FPU instructions working on ST0 and STn, and storing the
 * result in ST0.
 *
 * @param   pfnAImpl    Pointer to the instruction implementation (assembly).
 */
FNIEMOP_DEF_2(iemOpHlpFpu_st0_stN, uint8_t, bRm, PFNIEMAIMPLFPUR80, pfnAImpl)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(3, 1);
    IEM_MC_LOCAL(IEMFPURESULT,          FpuRes);
    IEM_MC_ARG_LOCAL_REF(PIEMFPURESULT, pFpuRes,        FpuRes,     0);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value1,                 1);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value2,                 2);

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_TWO_FPUREGS_NOT_EMPTY_REF_R80(pr80Value1, 0, pr80Value2, bRm & X86_MODRM_RM_MASK)
        IEM_MC_CALL_FPU_AIMPL_3(pfnAImpl, pFpuRes, pr80Value1, pr80Value2);
        IEM_MC_STORE_FPU_RESULT(FpuRes, 0);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW(0);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/**
 * Common worker for FPU instructions working on ST0 and STn, and only affecting
 * flags.
 *
 * @param   pfnAImpl    Pointer to the instruction implementation (assembly).
 */
FNIEMOP_DEF_2(iemOpHlpFpuNoStore_st0_stN, uint8_t, bRm, PFNIEMAIMPLFPUR80FSW, pfnAImpl)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(3, 1);
    IEM_MC_LOCAL(uint16_t,              u16Fsw);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,    pu16Fsw,        u16Fsw,     0);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value1,                 1);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value2,                 2);

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_TWO_FPUREGS_NOT_EMPTY_REF_R80(pr80Value1, 0, pr80Value2, bRm & X86_MODRM_RM_MASK)
        IEM_MC_CALL_FPU_AIMPL_3(pfnAImpl, pu16Fsw, pr80Value1, pr80Value2);
        IEM_MC_UPDATE_FSW(u16Fsw);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW(UINT8_MAX);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/**
 * Common worker for FPU instructions working on ST0 and STn, only affecting
 * flags, and popping when done.
 *
 * @param   pfnAImpl    Pointer to the instruction implementation (assembly).
 */
FNIEMOP_DEF_2(iemOpHlpFpuNoStore_st0_stN_pop, uint8_t, bRm, PFNIEMAIMPLFPUR80FSW, pfnAImpl)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(3, 1);
    IEM_MC_LOCAL(uint16_t,              u16Fsw);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,    pu16Fsw,        u16Fsw,     0);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value1,                 1);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value2,                 2);

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_TWO_FPUREGS_NOT_EMPTY_REF_R80(pr80Value1, 0, pr80Value2, bRm & X86_MODRM_RM_MASK)
        IEM_MC_CALL_FPU_AIMPL_3(pfnAImpl, pu16Fsw, pr80Value1, pr80Value2);
        IEM_MC_UPDATE_FSW_THEN_POP(u16Fsw);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW_THEN_POP(UINT8_MAX);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xd8 11/0. */
FNIEMOP_DEF_1(iemOp_fadd_stN,   uint8_t, bRm)
{
    IEMOP_MNEMONIC("fadd st0,stN");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_stN, bRm, iemAImpl_fadd_r80_by_r80);
}


/** Opcode 0xd8 11/1. */
FNIEMOP_DEF_1(iemOp_fmul_stN,   uint8_t, bRm)
{
    IEMOP_MNEMONIC("fmul st0,stN");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_stN, bRm, iemAImpl_fmul_r80_by_r80);
}


/** Opcode 0xd8 11/2. */
FNIEMOP_DEF_1(iemOp_fcom_stN,   uint8_t, bRm)
{
    IEMOP_MNEMONIC("fcom st0,stN");
    return FNIEMOP_CALL_2(iemOpHlpFpuNoStore_st0_stN, bRm, iemAImpl_fcom_r80_by_r80);
}


/** Opcode 0xd8 11/3. */
FNIEMOP_DEF_1(iemOp_fcomp_stN,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fcomp st0,stN");
    return FNIEMOP_CALL_2(iemOpHlpFpuNoStore_st0_stN_pop, bRm, iemAImpl_fcom_r80_by_r80);
}


/** Opcode 0xd8 11/4. */
FNIEMOP_DEF_1(iemOp_fsub_stN,   uint8_t, bRm)
{
    IEMOP_MNEMONIC("fsub st0,stN");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_stN, bRm, iemAImpl_fsub_r80_by_r80);
}


/** Opcode 0xd8 11/5. */
FNIEMOP_DEF_1(iemOp_fsubr_stN,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fsubr st0,stN");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_stN, bRm, iemAImpl_fsubr_r80_by_r80);
}


/** Opcode 0xd8 11/6. */
FNIEMOP_DEF_1(iemOp_fdiv_stN,   uint8_t, bRm)
{
    IEMOP_MNEMONIC("fdiv st0,stN");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_stN, bRm, iemAImpl_fdiv_r80_by_r80);
}


/** Opcode 0xd8 11/7. */
FNIEMOP_DEF_1(iemOp_fdivr_stN,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fdivr st0,stN");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_stN, bRm, iemAImpl_fdivr_r80_by_r80);
}


/**
 * Common worker for FPU instructions working on ST0 and an m32r, and storing
 * the result in ST0.
 *
 * @param   pfnAImpl    Pointer to the instruction implementation (assembly).
 */
FNIEMOP_DEF_2(iemOpHlpFpu_st0_m32r, uint8_t, bRm, PFNIEMAIMPLFPUR32, pfnAImpl)
{
    IEM_MC_BEGIN(3, 3);
    IEM_MC_LOCAL(RTGCPTR,               GCPtrEffSrc);
    IEM_MC_LOCAL(IEMFPURESULT,          FpuRes);
    IEM_MC_LOCAL(RTFLOAT32U,            r32Val2);
    IEM_MC_ARG_LOCAL_REF(PIEMFPURESULT, pFpuRes,        FpuRes,     0);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value1,                 1);
    IEM_MC_ARG_LOCAL_REF(PCRTFLOAT32U,  pr32Val2,       r32Val2,    2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_FETCH_MEM_R32(r32Val2, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value1, 0)
        IEM_MC_CALL_FPU_AIMPL_3(pfnAImpl, pFpuRes, pr80Value1, pr32Val2);
        IEM_MC_STORE_FPU_RESULT(FpuRes, 0);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW(0);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xd8 !11/0. */
FNIEMOP_DEF_1(iemOp_fadd_m32r,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fadd st0,m32r");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_m32r, bRm, iemAImpl_fadd_r80_by_r32);
}


/** Opcode 0xd8 !11/1. */
FNIEMOP_DEF_1(iemOp_fmul_m32r,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fmul st0,m32r");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_m32r, bRm, iemAImpl_fmul_r80_by_r32);
}


/** Opcode 0xd8 !11/2. */
FNIEMOP_DEF_1(iemOp_fcom_m32r,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fcom st0,m32r");

    IEM_MC_BEGIN(3, 3);
    IEM_MC_LOCAL(RTGCPTR,               GCPtrEffSrc);
    IEM_MC_LOCAL(uint16_t,              u16Fsw);
    IEM_MC_LOCAL(RTFLOAT32U,            r32Val2);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,    pu16Fsw,        u16Fsw,     0);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value1,                 1);
    IEM_MC_ARG_LOCAL_REF(PCRTFLOAT32U,  pr32Val2,       r32Val2,    2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_FETCH_MEM_R32(r32Val2, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value1, 0)
        IEM_MC_CALL_FPU_AIMPL_3(iemAImpl_fcom_r80_by_r32, pu16Fsw, pr80Value1, pr32Val2);
        IEM_MC_UPDATE_FSW_WITH_MEM_OP(u16Fsw, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP(UINT8_MAX, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xd8 !11/3. */
FNIEMOP_DEF_1(iemOp_fcomp_m32r, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fcomp st0,m32r");

    IEM_MC_BEGIN(3, 3);
    IEM_MC_LOCAL(RTGCPTR,               GCPtrEffSrc);
    IEM_MC_LOCAL(uint16_t,              u16Fsw);
    IEM_MC_LOCAL(RTFLOAT32U,            r32Val2);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,    pu16Fsw,        u16Fsw,     0);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value1,                 1);
    IEM_MC_ARG_LOCAL_REF(PCRTFLOAT32U,  pr32Val2,       r32Val2,    2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_FETCH_MEM_R32(r32Val2, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value1, 0)
        IEM_MC_CALL_FPU_AIMPL_3(iemAImpl_fcom_r80_by_r32, pu16Fsw, pr80Value1, pr32Val2);
        IEM_MC_UPDATE_FSW_WITH_MEM_OP_THEN_POP(u16Fsw, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP_THEN_POP(UINT8_MAX, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xd8 !11/4. */
FNIEMOP_DEF_1(iemOp_fsub_m32r,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fsub st0,m32r");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_m32r, bRm, iemAImpl_fsub_r80_by_r32);
}


/** Opcode 0xd8 !11/5. */
FNIEMOP_DEF_1(iemOp_fsubr_m32r, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fsubr st0,m32r");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_m32r, bRm, iemAImpl_fsubr_r80_by_r32);
}


/** Opcode 0xd8 !11/6. */
FNIEMOP_DEF_1(iemOp_fdiv_m32r,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fdiv st0,m32r");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_m32r, bRm, iemAImpl_fdiv_r80_by_r32);
}


/** Opcode 0xd8 !11/7. */
FNIEMOP_DEF_1(iemOp_fdivr_m32r, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fdivr st0,m32r");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_m32r, bRm, iemAImpl_fdivr_r80_by_r32);
}


/** Opcode 0xd8. */
FNIEMOP_DEF(iemOp_EscF0)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    pVCpu->iem.s.uFpuOpcode = RT_MAKE_U16(bRm, 0xd8 & 0x7);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
        {
            case 0: return FNIEMOP_CALL_1(iemOp_fadd_stN,  bRm);
            case 1: return FNIEMOP_CALL_1(iemOp_fmul_stN,  bRm);
            case 2: return FNIEMOP_CALL_1(iemOp_fcom_stN,  bRm);
            case 3: return FNIEMOP_CALL_1(iemOp_fcomp_stN, bRm);
            case 4: return FNIEMOP_CALL_1(iemOp_fsub_stN,  bRm);
            case 5: return FNIEMOP_CALL_1(iemOp_fsubr_stN, bRm);
            case 6: return FNIEMOP_CALL_1(iemOp_fdiv_stN,  bRm);
            case 7: return FNIEMOP_CALL_1(iemOp_fdivr_stN, bRm);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
        {
            case 0: return FNIEMOP_CALL_1(iemOp_fadd_m32r,  bRm);
            case 1: return FNIEMOP_CALL_1(iemOp_fmul_m32r,  bRm);
            case 2: return FNIEMOP_CALL_1(iemOp_fcom_m32r,  bRm);
            case 3: return FNIEMOP_CALL_1(iemOp_fcomp_m32r, bRm);
            case 4: return FNIEMOP_CALL_1(iemOp_fsub_m32r,  bRm);
            case 5: return FNIEMOP_CALL_1(iemOp_fsubr_m32r, bRm);
            case 6: return FNIEMOP_CALL_1(iemOp_fdiv_m32r,  bRm);
            case 7: return FNIEMOP_CALL_1(iemOp_fdivr_m32r, bRm);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


/** Opcode 0xd9 /0 mem32real
 * @sa  iemOp_fld_m64r */
FNIEMOP_DEF_1(iemOp_fld_m32r, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fld m32r");

    IEM_MC_BEGIN(2, 3);
    IEM_MC_LOCAL(RTGCPTR,               GCPtrEffSrc);
    IEM_MC_LOCAL(IEMFPURESULT,          FpuRes);
    IEM_MC_LOCAL(RTFLOAT32U,            r32Val);
    IEM_MC_ARG_LOCAL_REF(PIEMFPURESULT, pFpuRes,    FpuRes, 0);
    IEM_MC_ARG_LOCAL_REF(PCRTFLOAT32U,  pr32Val,    r32Val, 1);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_FETCH_MEM_R32(r32Val, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_IS_EMPTY(7)
        IEM_MC_CALL_FPU_AIMPL_2(iemAImpl_fld_r32_to_r80, pFpuRes, pr32Val);
        IEM_MC_PUSH_FPU_RESULT_MEM_OP(FpuRes, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_PUSH_OVERFLOW_MEM_OP(pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xd9 !11/2 mem32real */
FNIEMOP_DEF_1(iemOp_fst_m32r, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fst m32r");
    IEM_MC_BEGIN(3, 2);
    IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);
    IEM_MC_LOCAL(uint16_t,              u16Fsw);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,    pu16Fsw,    u16Fsw, 0);
    IEM_MC_ARG(PRTFLOAT32U,             pr32Dst,            1);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value,          2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_MEM_MAP(pr32Dst, IEM_ACCESS_DATA_W, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1 /*arg*/);
    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value, 0)
        IEM_MC_CALL_FPU_AIMPL_3(iemAImpl_fst_r80_to_r32, pu16Fsw, pr32Dst, pr80Value);
        IEM_MC_MEM_COMMIT_AND_UNMAP_FOR_FPU_STORE(pr32Dst, IEM_ACCESS_DATA_W, u16Fsw);
        IEM_MC_UPDATE_FSW_WITH_MEM_OP(u16Fsw, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ELSE()
        IEM_MC_IF_FCW_IM()
            IEM_MC_STORE_MEM_NEG_QNAN_R32_BY_REF(pr32Dst);
            IEM_MC_MEM_COMMIT_AND_UNMAP(pr32Dst, IEM_ACCESS_DATA_W);
        IEM_MC_ENDIF();
        IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP(UINT8_MAX, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xd9 !11/3 */
FNIEMOP_DEF_1(iemOp_fstp_m32r, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fstp m32r");
    IEM_MC_BEGIN(3, 2);
    IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);
    IEM_MC_LOCAL(uint16_t,              u16Fsw);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,    pu16Fsw,    u16Fsw, 0);
    IEM_MC_ARG(PRTFLOAT32U,             pr32Dst,            1);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value,          2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_MEM_MAP(pr32Dst, IEM_ACCESS_DATA_W, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1 /*arg*/);
    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value, 0)
        IEM_MC_CALL_FPU_AIMPL_3(iemAImpl_fst_r80_to_r32, pu16Fsw, pr32Dst, pr80Value);
        IEM_MC_MEM_COMMIT_AND_UNMAP_FOR_FPU_STORE(pr32Dst, IEM_ACCESS_DATA_W, u16Fsw);
        IEM_MC_UPDATE_FSW_WITH_MEM_OP_THEN_POP(u16Fsw, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ELSE()
        IEM_MC_IF_FCW_IM()
            IEM_MC_STORE_MEM_NEG_QNAN_R32_BY_REF(pr32Dst);
            IEM_MC_MEM_COMMIT_AND_UNMAP(pr32Dst, IEM_ACCESS_DATA_W);
        IEM_MC_ENDIF();
        IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP_THEN_POP(UINT8_MAX, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xd9 !11/4 */
FNIEMOP_DEF_1(iemOp_fldenv, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fldenv m14/28byte");
    IEM_MC_BEGIN(3, 0);
    IEM_MC_ARG_CONST(IEMMODE,           enmEffOpSize, /*=*/ pVCpu->iem.s.enmEffOpSize,  0);
    IEM_MC_ARG(uint8_t,                 iEffSeg,                                    1);
    IEM_MC_ARG(RTGCPTR,                 GCPtrEffSrc,                                2);
    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_ACTUALIZE_FPU_STATE_FOR_CHANGE();
    IEM_MC_ASSIGN(iEffSeg, pVCpu->iem.s.iEffSeg);
    IEM_MC_CALL_CIMPL_3(iemCImpl_fldenv, enmEffOpSize, iEffSeg, GCPtrEffSrc);
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xd9 !11/5 */
FNIEMOP_DEF_1(iemOp_fldcw, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fldcw m2byte");
    IEM_MC_BEGIN(1, 1);
    IEM_MC_LOCAL(RTGCPTR,               GCPtrEffSrc);
    IEM_MC_ARG(uint16_t,                u16Fsw,                                     0);
    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_ACTUALIZE_FPU_STATE_FOR_CHANGE();
    IEM_MC_FETCH_MEM_U16(u16Fsw, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_CALL_CIMPL_1(iemCImpl_fldcw, u16Fsw);
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xd9 !11/6 */
FNIEMOP_DEF_1(iemOp_fnstenv, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fstenv m14/m28byte");
    IEM_MC_BEGIN(3, 0);
    IEM_MC_ARG_CONST(IEMMODE,           enmEffOpSize, /*=*/ pVCpu->iem.s.enmEffOpSize,  0);
    IEM_MC_ARG(uint8_t,                 iEffSeg,                                    1);
    IEM_MC_ARG(RTGCPTR,                 GCPtrEffDst,                                2);
    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_ACTUALIZE_FPU_STATE_FOR_READ();
    IEM_MC_ASSIGN(iEffSeg, pVCpu->iem.s.iEffSeg);
    IEM_MC_CALL_CIMPL_3(iemCImpl_fnstenv, enmEffOpSize, iEffSeg, GCPtrEffDst);
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xd9 !11/7 */
FNIEMOP_DEF_1(iemOp_fnstcw, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fnstcw m2byte");
    IEM_MC_BEGIN(2, 0);
    IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);
    IEM_MC_LOCAL(uint16_t,              u16Fcw);
    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_ACTUALIZE_FPU_STATE_FOR_READ();
    IEM_MC_FETCH_FCW(u16Fcw);
    IEM_MC_STORE_MEM_U16(pVCpu->iem.s.iEffSeg, GCPtrEffDst, u16Fcw);
    IEM_MC_ADVANCE_RIP(); /* C0-C3 are documented as undefined, we leave them unmodified. */
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xd9 0xd0, 0xd9 0xd8-0xdf, ++?.  */
FNIEMOP_DEF(iemOp_fnop)
{
    IEMOP_MNEMONIC("fnop");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(0, 0);
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_ACTUALIZE_FPU_STATE_FOR_CHANGE();
    /** @todo Testcase: looks like FNOP leaves FOP alone but updates FPUIP. Could be
     *        intel optimizations. Investigate. */
    IEM_MC_UPDATE_FPU_OPCODE_IP();
    IEM_MC_ADVANCE_RIP(); /* C0-C3 are documented as undefined, we leave them unmodified. */
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xd9 11/0 stN */
FNIEMOP_DEF_1(iemOp_fld_stN, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fld stN");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    /** @todo Testcase: Check if this raises \#MF?  Intel mentioned it not. AMD
     *        indicates that it does. */
    IEM_MC_BEGIN(0, 2);
    IEM_MC_LOCAL(PCRTFLOAT80U,          pr80Value);
    IEM_MC_LOCAL(IEMFPURESULT,          FpuRes);
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value, bRm & X86_MODRM_RM_MASK)
        IEM_MC_SET_FPU_RESULT(FpuRes, 0 /*FSW*/, pr80Value);
        IEM_MC_PUSH_FPU_RESULT(FpuRes);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_PUSH_UNDERFLOW();
    IEM_MC_ENDIF();

    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();

    return VINF_SUCCESS;
}


/** Opcode 0xd9 11/3 stN */
FNIEMOP_DEF_1(iemOp_fxch_stN, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fxch stN");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    /** @todo Testcase: Check if this raises \#MF?  Intel mentioned it not. AMD
     *        indicates that it does. */
    IEM_MC_BEGIN(1, 3);
    IEM_MC_LOCAL(PCRTFLOAT80U,          pr80Value1);
    IEM_MC_LOCAL(PCRTFLOAT80U,          pr80Value2);
    IEM_MC_LOCAL(IEMFPURESULT,          FpuRes);
    IEM_MC_ARG_CONST(uint8_t,           iStReg, /*=*/ bRm & X86_MODRM_RM_MASK, 0);
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_TWO_FPUREGS_NOT_EMPTY_REF_R80(pr80Value1, 0, pr80Value2, bRm & X86_MODRM_RM_MASK)
        IEM_MC_SET_FPU_RESULT(FpuRes, X86_FSW_C1, pr80Value2);
        IEM_MC_STORE_FPUREG_R80_SRC_REF(bRm & X86_MODRM_RM_MASK, pr80Value1);
        IEM_MC_STORE_FPU_RESULT(FpuRes, 0);
    IEM_MC_ELSE()
        IEM_MC_CALL_CIMPL_1(iemCImpl_fxch_underflow, iStReg);
    IEM_MC_ENDIF();

    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();

    return VINF_SUCCESS;
}


/** Opcode 0xd9 11/4, 0xdd 11/2. */
FNIEMOP_DEF_1(iemOp_fstp_stN, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fstp st0,stN");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    /* fstp st0, st0 is frequendly used as an official 'ffreep st0' sequence. */
    uint8_t const iDstReg = bRm & X86_MODRM_RM_MASK;
    if (!iDstReg)
    {
        IEM_MC_BEGIN(0, 1);
        IEM_MC_LOCAL_CONST(uint16_t,        u16Fsw, /*=*/ 0);
        IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
        IEM_MC_MAYBE_RAISE_FPU_XCPT();

        IEM_MC_PREPARE_FPU_USAGE();
        IEM_MC_IF_FPUREG_NOT_EMPTY(0)
            IEM_MC_UPDATE_FSW_THEN_POP(u16Fsw);
        IEM_MC_ELSE()
            IEM_MC_FPU_STACK_UNDERFLOW_THEN_POP(0);
        IEM_MC_ENDIF();

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        IEM_MC_BEGIN(0, 2);
        IEM_MC_LOCAL(PCRTFLOAT80U,          pr80Value);
        IEM_MC_LOCAL(IEMFPURESULT,          FpuRes);
        IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
        IEM_MC_MAYBE_RAISE_FPU_XCPT();

        IEM_MC_PREPARE_FPU_USAGE();
        IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value, 0)
            IEM_MC_SET_FPU_RESULT(FpuRes, 0 /*FSW*/, pr80Value);
            IEM_MC_STORE_FPU_RESULT_THEN_POP(FpuRes, iDstReg);
        IEM_MC_ELSE()
            IEM_MC_FPU_STACK_UNDERFLOW_THEN_POP(iDstReg);
        IEM_MC_ENDIF();

        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/**
 * Common worker for FPU instructions working on ST0 and replaces it with the
 * result, i.e. unary operators.
 *
 * @param   pfnAImpl    Pointer to the instruction implementation (assembly).
 */
FNIEMOP_DEF_1(iemOpHlpFpu_st0, PFNIEMAIMPLFPUR80UNARY, pfnAImpl)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(2, 1);
    IEM_MC_LOCAL(IEMFPURESULT,          FpuRes);
    IEM_MC_ARG_LOCAL_REF(PIEMFPURESULT, pFpuRes,    FpuRes, 0);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value,          1);

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value, 0)
        IEM_MC_CALL_FPU_AIMPL_2(pfnAImpl, pFpuRes, pr80Value);
        IEM_MC_STORE_FPU_RESULT(FpuRes, 0);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW(0);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xd9 0xe0. */
FNIEMOP_DEF(iemOp_fchs)
{
    IEMOP_MNEMONIC("fchs st0");
    return FNIEMOP_CALL_1(iemOpHlpFpu_st0, iemAImpl_fchs_r80);
}


/** Opcode 0xd9 0xe1. */
FNIEMOP_DEF(iemOp_fabs)
{
    IEMOP_MNEMONIC("fabs st0");
    return FNIEMOP_CALL_1(iemOpHlpFpu_st0, iemAImpl_fabs_r80);
}


/**
 * Common worker for FPU instructions working on ST0 and only returns FSW.
 *
 * @param   pfnAImpl    Pointer to the instruction implementation (assembly).
 */
FNIEMOP_DEF_1(iemOpHlpFpuNoStore_st0, PFNIEMAIMPLFPUR80UNARYFSW, pfnAImpl)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(2, 1);
    IEM_MC_LOCAL(uint16_t,              u16Fsw);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,    pu16Fsw,    u16Fsw, 0);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value,          1);

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value, 0)
        IEM_MC_CALL_FPU_AIMPL_2(pfnAImpl, pu16Fsw, pr80Value);
        IEM_MC_UPDATE_FSW(u16Fsw);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW(UINT8_MAX);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xd9 0xe4. */
FNIEMOP_DEF(iemOp_ftst)
{
    IEMOP_MNEMONIC("ftst st0");
    return FNIEMOP_CALL_1(iemOpHlpFpuNoStore_st0, iemAImpl_ftst_r80);
}


/** Opcode 0xd9 0xe5. */
FNIEMOP_DEF(iemOp_fxam)
{
    IEMOP_MNEMONIC("fxam st0");
    return FNIEMOP_CALL_1(iemOpHlpFpuNoStore_st0, iemAImpl_fxam_r80);
}


/**
 * Common worker for FPU instructions pushing a constant onto the FPU stack.
 *
 * @param   pfnAImpl    Pointer to the instruction implementation (assembly).
 */
FNIEMOP_DEF_1(iemOpHlpFpuPushConstant, PFNIEMAIMPLFPUR80LDCONST, pfnAImpl)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(1, 1);
    IEM_MC_LOCAL(IEMFPURESULT,          FpuRes);
    IEM_MC_ARG_LOCAL_REF(PIEMFPURESULT, pFpuRes,    FpuRes, 0);

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_IS_EMPTY(7)
        IEM_MC_CALL_FPU_AIMPL_1(pfnAImpl, pFpuRes);
        IEM_MC_PUSH_FPU_RESULT(FpuRes);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_PUSH_OVERFLOW();
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xd9 0xe8. */
FNIEMOP_DEF(iemOp_fld1)
{
    IEMOP_MNEMONIC("fld1");
    return FNIEMOP_CALL_1(iemOpHlpFpuPushConstant, iemAImpl_fld1);
}


/** Opcode 0xd9 0xe9. */
FNIEMOP_DEF(iemOp_fldl2t)
{
    IEMOP_MNEMONIC("fldl2t");
    return FNIEMOP_CALL_1(iemOpHlpFpuPushConstant, iemAImpl_fldl2t);
}


/** Opcode 0xd9 0xea. */
FNIEMOP_DEF(iemOp_fldl2e)
{
    IEMOP_MNEMONIC("fldl2e");
    return FNIEMOP_CALL_1(iemOpHlpFpuPushConstant, iemAImpl_fldl2e);
}

/** Opcode 0xd9 0xeb. */
FNIEMOP_DEF(iemOp_fldpi)
{
    IEMOP_MNEMONIC("fldpi");
    return FNIEMOP_CALL_1(iemOpHlpFpuPushConstant, iemAImpl_fldpi);
}


/** Opcode 0xd9 0xec. */
FNIEMOP_DEF(iemOp_fldlg2)
{
    IEMOP_MNEMONIC("fldlg2");
    return FNIEMOP_CALL_1(iemOpHlpFpuPushConstant, iemAImpl_fldlg2);
}

/** Opcode 0xd9 0xed. */
FNIEMOP_DEF(iemOp_fldln2)
{
    IEMOP_MNEMONIC("fldln2");
    return FNIEMOP_CALL_1(iemOpHlpFpuPushConstant, iemAImpl_fldln2);
}


/** Opcode 0xd9 0xee. */
FNIEMOP_DEF(iemOp_fldz)
{
    IEMOP_MNEMONIC("fldz");
    return FNIEMOP_CALL_1(iemOpHlpFpuPushConstant, iemAImpl_fldz);
}


/** Opcode 0xd9 0xf0. */
FNIEMOP_DEF(iemOp_f2xm1)
{
    IEMOP_MNEMONIC("f2xm1 st0");
    return FNIEMOP_CALL_1(iemOpHlpFpu_st0, iemAImpl_f2xm1_r80);
}


/** Opcode 0xd9 0xf1. */
FNIEMOP_DEF(iemOp_fylx2)
{
    IEMOP_MNEMONIC("fylx2 st0");
    return FNIEMOP_CALL_1(iemOpHlpFpu_st0, iemAImpl_fyl2x_r80);
}


/**
 * Common worker for FPU instructions working on ST0 and having two outputs, one
 * replacing ST0 and one pushed onto the stack.
 *
 * @param   pfnAImpl    Pointer to the instruction implementation (assembly).
 */
FNIEMOP_DEF_1(iemOpHlpFpuReplace_st0_push, PFNIEMAIMPLFPUR80UNARYTWO, pfnAImpl)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(2, 1);
    IEM_MC_LOCAL(IEMFPURESULTTWO,           FpuResTwo);
    IEM_MC_ARG_LOCAL_REF(PIEMFPURESULTTWO,  pFpuResTwo, FpuResTwo,  0);
    IEM_MC_ARG(PCRTFLOAT80U,                pr80Value,              1);

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value, 0)
        IEM_MC_CALL_FPU_AIMPL_2(pfnAImpl, pFpuResTwo, pr80Value);
        IEM_MC_PUSH_FPU_RESULT_TWO(FpuResTwo);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_PUSH_UNDERFLOW_TWO();
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xd9 0xf2. */
FNIEMOP_DEF(iemOp_fptan)
{
    IEMOP_MNEMONIC("fptan st0");
    return FNIEMOP_CALL_1(iemOpHlpFpuReplace_st0_push, iemAImpl_fptan_r80_r80);
}


/**
 * Common worker for FPU instructions working on STn and ST0, storing the result
 * in STn, and popping the stack unless IE, DE or ZE was raised.
 *
 * @param   pfnAImpl    Pointer to the instruction implementation (assembly).
 */
FNIEMOP_DEF_2(iemOpHlpFpu_stN_st0_pop, uint8_t, bRm, PFNIEMAIMPLFPUR80, pfnAImpl)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(3, 1);
    IEM_MC_LOCAL(IEMFPURESULT,          FpuRes);
    IEM_MC_ARG_LOCAL_REF(PIEMFPURESULT, pFpuRes,        FpuRes,     0);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value1,                 1);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value2,                 2);

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_TWO_FPUREGS_NOT_EMPTY_REF_R80(pr80Value1, bRm & X86_MODRM_RM_MASK, pr80Value2, 0)
        IEM_MC_CALL_FPU_AIMPL_3(pfnAImpl, pFpuRes, pr80Value1, pr80Value2);
        IEM_MC_STORE_FPU_RESULT_THEN_POP(FpuRes, bRm & X86_MODRM_RM_MASK);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW_THEN_POP(bRm & X86_MODRM_RM_MASK);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xd9 0xf3. */
FNIEMOP_DEF(iemOp_fpatan)
{
    IEMOP_MNEMONIC("fpatan st1,st0");
    return FNIEMOP_CALL_2(iemOpHlpFpu_stN_st0_pop, 1, iemAImpl_fpatan_r80_by_r80);
}


/** Opcode 0xd9 0xf4. */
FNIEMOP_DEF(iemOp_fxtract)
{
    IEMOP_MNEMONIC("fxtract st0");
    return FNIEMOP_CALL_1(iemOpHlpFpuReplace_st0_push, iemAImpl_fxtract_r80_r80);
}


/** Opcode 0xd9 0xf5. */
FNIEMOP_DEF(iemOp_fprem1)
{
    IEMOP_MNEMONIC("fprem1 st0, st1");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_stN, 1, iemAImpl_fprem1_r80_by_r80);
}


/** Opcode 0xd9 0xf6. */
FNIEMOP_DEF(iemOp_fdecstp)
{
    IEMOP_MNEMONIC("fdecstp");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    /* Note! C0, C2 and C3 are documented as undefined, we clear them. */
    /** @todo Testcase: Check whether FOP, FPUIP and FPUCS are affected by
     *        FINCSTP and FDECSTP. */

    IEM_MC_BEGIN(0,0);

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_ACTUALIZE_FPU_STATE_FOR_CHANGE();
    IEM_MC_FPU_STACK_DEC_TOP();
    IEM_MC_UPDATE_FSW_CONST(0);

    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xd9 0xf7. */
FNIEMOP_DEF(iemOp_fincstp)
{
    IEMOP_MNEMONIC("fincstp");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    /* Note! C0, C2 and C3 are documented as undefined, we clear them. */
    /** @todo Testcase: Check whether FOP, FPUIP and FPUCS are affected by
     *        FINCSTP and FDECSTP. */

    IEM_MC_BEGIN(0,0);

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_ACTUALIZE_FPU_STATE_FOR_CHANGE();
    IEM_MC_FPU_STACK_INC_TOP();
    IEM_MC_UPDATE_FSW_CONST(0);

    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xd9 0xf8. */
FNIEMOP_DEF(iemOp_fprem)
{
    IEMOP_MNEMONIC("fprem st0, st1");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_stN, 1, iemAImpl_fprem_r80_by_r80);
}


/** Opcode 0xd9 0xf9. */
FNIEMOP_DEF(iemOp_fyl2xp1)
{
    IEMOP_MNEMONIC("fyl2xp1 st1,st0");
    return FNIEMOP_CALL_2(iemOpHlpFpu_stN_st0_pop, 1, iemAImpl_fyl2xp1_r80_by_r80);
}


/** Opcode 0xd9 0xfa. */
FNIEMOP_DEF(iemOp_fsqrt)
{
    IEMOP_MNEMONIC("fsqrt st0");
    return FNIEMOP_CALL_1(iemOpHlpFpu_st0, iemAImpl_fsqrt_r80);
}


/** Opcode 0xd9 0xfb. */
FNIEMOP_DEF(iemOp_fsincos)
{
    IEMOP_MNEMONIC("fsincos st0");
    return FNIEMOP_CALL_1(iemOpHlpFpuReplace_st0_push, iemAImpl_fsincos_r80_r80);
}


/** Opcode 0xd9 0xfc. */
FNIEMOP_DEF(iemOp_frndint)
{
    IEMOP_MNEMONIC("frndint st0");
    return FNIEMOP_CALL_1(iemOpHlpFpu_st0, iemAImpl_frndint_r80);
}


/** Opcode 0xd9 0xfd. */
FNIEMOP_DEF(iemOp_fscale)
{
    IEMOP_MNEMONIC("fscale st0, st1");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_stN, 1, iemAImpl_fscale_r80_by_r80);
}


/** Opcode 0xd9 0xfe. */
FNIEMOP_DEF(iemOp_fsin)
{
    IEMOP_MNEMONIC("fsin st0");
    return FNIEMOP_CALL_1(iemOpHlpFpu_st0, iemAImpl_fsin_r80);
}


/** Opcode 0xd9 0xff. */
FNIEMOP_DEF(iemOp_fcos)
{
    IEMOP_MNEMONIC("fcos st0");
    return FNIEMOP_CALL_1(iemOpHlpFpu_st0, iemAImpl_fcos_r80);
}


/** Used by iemOp_EscF1. */
IEM_STATIC const PFNIEMOP g_apfnEscF1_E0toFF[32] =
{
    /* 0xe0 */  iemOp_fchs,
    /* 0xe1 */  iemOp_fabs,
    /* 0xe2 */  iemOp_Invalid,
    /* 0xe3 */  iemOp_Invalid,
    /* 0xe4 */  iemOp_ftst,
    /* 0xe5 */  iemOp_fxam,
    /* 0xe6 */  iemOp_Invalid,
    /* 0xe7 */  iemOp_Invalid,
    /* 0xe8 */  iemOp_fld1,
    /* 0xe9 */  iemOp_fldl2t,
    /* 0xea */  iemOp_fldl2e,
    /* 0xeb */  iemOp_fldpi,
    /* 0xec */  iemOp_fldlg2,
    /* 0xed */  iemOp_fldln2,
    /* 0xee */  iemOp_fldz,
    /* 0xef */  iemOp_Invalid,
    /* 0xf0 */  iemOp_f2xm1,
    /* 0xf1 */  iemOp_fylx2,
    /* 0xf2 */  iemOp_fptan,
    /* 0xf3 */  iemOp_fpatan,
    /* 0xf4 */  iemOp_fxtract,
    /* 0xf5 */  iemOp_fprem1,
    /* 0xf6 */  iemOp_fdecstp,
    /* 0xf7 */  iemOp_fincstp,
    /* 0xf8 */  iemOp_fprem,
    /* 0xf9 */  iemOp_fyl2xp1,
    /* 0xfa */  iemOp_fsqrt,
    /* 0xfb */  iemOp_fsincos,
    /* 0xfc */  iemOp_frndint,
    /* 0xfd */  iemOp_fscale,
    /* 0xfe */  iemOp_fsin,
    /* 0xff */  iemOp_fcos
};


/** Opcode 0xd9. */
FNIEMOP_DEF(iemOp_EscF1)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    pVCpu->iem.s.uFpuOpcode = RT_MAKE_U16(bRm, 0xd9 & 0x7);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
        {
            case 0: return FNIEMOP_CALL_1(iemOp_fld_stN, bRm);
            case 1: return FNIEMOP_CALL_1(iemOp_fxch_stN, bRm);
            case 2:
                if (bRm == 0xd0)
                    return FNIEMOP_CALL(iemOp_fnop);
                return IEMOP_RAISE_INVALID_OPCODE();
            case 3: return FNIEMOP_CALL_1(iemOp_fstp_stN, bRm); /* Reserved. Intel behavior seems to be FSTP ST(i) though. */
            case 4:
            case 5:
            case 6:
            case 7:
                Assert((unsigned)bRm - 0xe0U < RT_ELEMENTS(g_apfnEscF1_E0toFF));
                return FNIEMOP_CALL(g_apfnEscF1_E0toFF[bRm - 0xe0]);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
        {
            case 0: return FNIEMOP_CALL_1(iemOp_fld_m32r,  bRm);
            case 1: return IEMOP_RAISE_INVALID_OPCODE();
            case 2: return FNIEMOP_CALL_1(iemOp_fst_m32r,  bRm);
            case 3: return FNIEMOP_CALL_1(iemOp_fstp_m32r, bRm);
            case 4: return FNIEMOP_CALL_1(iemOp_fldenv,    bRm);
            case 5: return FNIEMOP_CALL_1(iemOp_fldcw,     bRm);
            case 6: return FNIEMOP_CALL_1(iemOp_fnstenv,    bRm);
            case 7: return FNIEMOP_CALL_1(iemOp_fnstcw,     bRm);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


/** Opcode 0xda 11/0. */
FNIEMOP_DEF_1(iemOp_fcmovb_stN,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fcmovb st0,stN");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(0, 1);
    IEM_MC_LOCAL(PCRTFLOAT80U,      pr80ValueN);

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_TWO_FPUREGS_NOT_EMPTY_REF_R80_FIRST(pr80ValueN, bRm & X86_MODRM_RM_MASK, 0)
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_CF)
            IEM_MC_STORE_FPUREG_R80_SRC_REF(0, pr80ValueN);
        IEM_MC_ENDIF();
        IEM_MC_UPDATE_FPU_OPCODE_IP();
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW(0);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xda 11/1. */
FNIEMOP_DEF_1(iemOp_fcmove_stN,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fcmove st0,stN");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(0, 1);
    IEM_MC_LOCAL(PCRTFLOAT80U,      pr80ValueN);

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_TWO_FPUREGS_NOT_EMPTY_REF_R80_FIRST(pr80ValueN, bRm & X86_MODRM_RM_MASK, 0)
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_ZF)
            IEM_MC_STORE_FPUREG_R80_SRC_REF(0, pr80ValueN);
        IEM_MC_ENDIF();
        IEM_MC_UPDATE_FPU_OPCODE_IP();
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW(0);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xda 11/2. */
FNIEMOP_DEF_1(iemOp_fcmovbe_stN, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fcmovbe st0,stN");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(0, 1);
    IEM_MC_LOCAL(PCRTFLOAT80U,      pr80ValueN);

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_TWO_FPUREGS_NOT_EMPTY_REF_R80_FIRST(pr80ValueN, bRm & X86_MODRM_RM_MASK, 0)
        IEM_MC_IF_EFL_ANY_BITS_SET(X86_EFL_CF | X86_EFL_ZF)
            IEM_MC_STORE_FPUREG_R80_SRC_REF(0, pr80ValueN);
        IEM_MC_ENDIF();
        IEM_MC_UPDATE_FPU_OPCODE_IP();
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW(0);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xda 11/3. */
FNIEMOP_DEF_1(iemOp_fcmovu_stN,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fcmovu st0,stN");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(0, 1);
    IEM_MC_LOCAL(PCRTFLOAT80U,      pr80ValueN);

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_TWO_FPUREGS_NOT_EMPTY_REF_R80_FIRST(pr80ValueN, bRm & X86_MODRM_RM_MASK, 0)
        IEM_MC_IF_EFL_BIT_SET(X86_EFL_PF)
            IEM_MC_STORE_FPUREG_R80_SRC_REF(0, pr80ValueN);
        IEM_MC_ENDIF();
        IEM_MC_UPDATE_FPU_OPCODE_IP();
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW(0);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/**
 * Common worker for FPU instructions working on ST0 and STn, only affecting
 * flags, and popping twice when done.
 *
 * @param   pfnAImpl    Pointer to the instruction implementation (assembly).
 */
FNIEMOP_DEF_1(iemOpHlpFpuNoStore_st0_stN_pop_pop, PFNIEMAIMPLFPUR80FSW, pfnAImpl)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(3, 1);
    IEM_MC_LOCAL(uint16_t,              u16Fsw);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,    pu16Fsw,        u16Fsw,     0);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value1,                 1);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value2,                 2);

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_TWO_FPUREGS_NOT_EMPTY_REF_R80(pr80Value1, 0, pr80Value2, 1)
        IEM_MC_CALL_FPU_AIMPL_3(pfnAImpl, pu16Fsw, pr80Value1, pr80Value2);
        IEM_MC_UPDATE_FSW_THEN_POP_POP(u16Fsw);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW_THEN_POP_POP();
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xda 0xe9. */
FNIEMOP_DEF(iemOp_fucompp)
{
    IEMOP_MNEMONIC("fucompp st0,stN");
    return FNIEMOP_CALL_1(iemOpHlpFpuNoStore_st0_stN_pop_pop, iemAImpl_fucom_r80_by_r80);
}


/**
 * Common worker for FPU instructions working on ST0 and an m32i, and storing
 * the result in ST0.
 *
 * @param   pfnAImpl    Pointer to the instruction implementation (assembly).
 */
FNIEMOP_DEF_2(iemOpHlpFpu_st0_m32i, uint8_t, bRm, PFNIEMAIMPLFPUI32, pfnAImpl)
{
    IEM_MC_BEGIN(3, 3);
    IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
    IEM_MC_LOCAL(IEMFPURESULT,              FpuRes);
    IEM_MC_LOCAL(int32_t,                   i32Val2);
    IEM_MC_ARG_LOCAL_REF(PIEMFPURESULT,     pFpuRes,        FpuRes,     0);
    IEM_MC_ARG(PCRTFLOAT80U,                pr80Value1,                 1);
    IEM_MC_ARG_LOCAL_REF(int32_t const *,   pi32Val2,       i32Val2,    2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_FETCH_MEM_I32(i32Val2, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value1, 0)
        IEM_MC_CALL_FPU_AIMPL_3(pfnAImpl, pFpuRes, pr80Value1, pi32Val2);
        IEM_MC_STORE_FPU_RESULT(FpuRes, 0);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW(0);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xda !11/0. */
FNIEMOP_DEF_1(iemOp_fiadd_m32i,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fiadd m32i");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_m32i, bRm, iemAImpl_fiadd_r80_by_i32);
}


/** Opcode 0xda !11/1. */
FNIEMOP_DEF_1(iemOp_fimul_m32i,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fimul m32i");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_m32i, bRm, iemAImpl_fimul_r80_by_i32);
}


/** Opcode 0xda !11/2. */
FNIEMOP_DEF_1(iemOp_ficom_m32i,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("ficom st0,m32i");

    IEM_MC_BEGIN(3, 3);
    IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
    IEM_MC_LOCAL(uint16_t,                  u16Fsw);
    IEM_MC_LOCAL(int32_t,                   i32Val2);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,        pu16Fsw,        u16Fsw,     0);
    IEM_MC_ARG(PCRTFLOAT80U,                pr80Value1,                 1);
    IEM_MC_ARG_LOCAL_REF(int32_t const *,   pi32Val2,       i32Val2,    2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_FETCH_MEM_I32(i32Val2, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value1, 0)
        IEM_MC_CALL_FPU_AIMPL_3(iemAImpl_ficom_r80_by_i32, pu16Fsw, pr80Value1, pi32Val2);
        IEM_MC_UPDATE_FSW_WITH_MEM_OP(u16Fsw, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP(UINT8_MAX, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xda !11/3. */
FNIEMOP_DEF_1(iemOp_ficomp_m32i, uint8_t, bRm)
{
    IEMOP_MNEMONIC("ficomp st0,m32i");

    IEM_MC_BEGIN(3, 3);
    IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
    IEM_MC_LOCAL(uint16_t,                  u16Fsw);
    IEM_MC_LOCAL(int32_t,                   i32Val2);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,        pu16Fsw,        u16Fsw,     0);
    IEM_MC_ARG(PCRTFLOAT80U,                pr80Value1,                 1);
    IEM_MC_ARG_LOCAL_REF(int32_t const *,   pi32Val2,       i32Val2,    2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_FETCH_MEM_I32(i32Val2, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value1, 0)
        IEM_MC_CALL_FPU_AIMPL_3(iemAImpl_ficom_r80_by_i32, pu16Fsw, pr80Value1, pi32Val2);
        IEM_MC_UPDATE_FSW_WITH_MEM_OP_THEN_POP(u16Fsw, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP_THEN_POP(UINT8_MAX, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xda !11/4. */
FNIEMOP_DEF_1(iemOp_fisub_m32i,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fisub m32i");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_m32i, bRm, iemAImpl_fisub_r80_by_i32);
}


/** Opcode 0xda !11/5. */
FNIEMOP_DEF_1(iemOp_fisubr_m32i, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fisubr m32i");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_m32i, bRm, iemAImpl_fisubr_r80_by_i32);
}


/** Opcode 0xda !11/6. */
FNIEMOP_DEF_1(iemOp_fidiv_m32i,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fidiv m32i");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_m32i, bRm, iemAImpl_fidiv_r80_by_i32);
}


/** Opcode 0xda !11/7. */
FNIEMOP_DEF_1(iemOp_fidivr_m32i, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fidivr m32i");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_m32i, bRm, iemAImpl_fidivr_r80_by_i32);
}


/** Opcode 0xda. */
FNIEMOP_DEF(iemOp_EscF2)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    pVCpu->iem.s.uFpuOpcode = RT_MAKE_U16(bRm, 0xda & 0x7);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
        {
            case 0: return FNIEMOP_CALL_1(iemOp_fcmovb_stN, bRm);
            case 1: return FNIEMOP_CALL_1(iemOp_fcmove_stN, bRm);
            case 2: return FNIEMOP_CALL_1(iemOp_fcmovbe_stN, bRm);
            case 3: return FNIEMOP_CALL_1(iemOp_fcmovu_stN, bRm);
            case 4: return IEMOP_RAISE_INVALID_OPCODE();
            case 5:
                if (bRm == 0xe9)
                    return FNIEMOP_CALL(iemOp_fucompp);
                return IEMOP_RAISE_INVALID_OPCODE();
            case 6: return IEMOP_RAISE_INVALID_OPCODE();
            case 7: return IEMOP_RAISE_INVALID_OPCODE();
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
        {
            case 0: return FNIEMOP_CALL_1(iemOp_fiadd_m32i,  bRm);
            case 1: return FNIEMOP_CALL_1(iemOp_fimul_m32i,  bRm);
            case 2: return FNIEMOP_CALL_1(iemOp_ficom_m32i,  bRm);
            case 3: return FNIEMOP_CALL_1(iemOp_ficomp_m32i, bRm);
            case 4: return FNIEMOP_CALL_1(iemOp_fisub_m32i,  bRm);
            case 5: return FNIEMOP_CALL_1(iemOp_fisubr_m32i, bRm);
            case 6: return FNIEMOP_CALL_1(iemOp_fidiv_m32i,  bRm);
            case 7: return FNIEMOP_CALL_1(iemOp_fidivr_m32i, bRm);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


/** Opcode 0xdb !11/0. */
FNIEMOP_DEF_1(iemOp_fild_m32i, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fild m32i");

    IEM_MC_BEGIN(2, 3);
    IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
    IEM_MC_LOCAL(IEMFPURESULT,              FpuRes);
    IEM_MC_LOCAL(int32_t,                   i32Val);
    IEM_MC_ARG_LOCAL_REF(PIEMFPURESULT,     pFpuRes,    FpuRes, 0);
    IEM_MC_ARG_LOCAL_REF(int32_t const *,   pi32Val,    i32Val, 1);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_FETCH_MEM_I32(i32Val, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_IS_EMPTY(7)
        IEM_MC_CALL_FPU_AIMPL_2(iemAImpl_fild_i32_to_r80, pFpuRes, pi32Val);
        IEM_MC_PUSH_FPU_RESULT_MEM_OP(FpuRes, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_PUSH_OVERFLOW_MEM_OP(pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdb !11/1. */
FNIEMOP_DEF_1(iemOp_fisttp_m32i, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fisttp m32i");
    IEM_MC_BEGIN(3, 2);
    IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);
    IEM_MC_LOCAL(uint16_t,              u16Fsw);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,    pu16Fsw,    u16Fsw, 0);
    IEM_MC_ARG(int32_t *,               pi32Dst,            1);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value,          2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_MEM_MAP(pi32Dst, IEM_ACCESS_DATA_W, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1 /*arg*/);
    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value, 0)
        IEM_MC_CALL_FPU_AIMPL_3(iemAImpl_fistt_r80_to_i32, pu16Fsw, pi32Dst, pr80Value);
        IEM_MC_MEM_COMMIT_AND_UNMAP_FOR_FPU_STORE(pi32Dst, IEM_ACCESS_DATA_W, u16Fsw);
        IEM_MC_UPDATE_FSW_WITH_MEM_OP_THEN_POP(u16Fsw, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ELSE()
        IEM_MC_IF_FCW_IM()
            IEM_MC_STORE_MEM_I32_CONST_BY_REF(pi32Dst, INT32_MIN /* (integer indefinite) */);
            IEM_MC_MEM_COMMIT_AND_UNMAP(pi32Dst, IEM_ACCESS_DATA_W);
        IEM_MC_ENDIF();
        IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP_THEN_POP(UINT8_MAX, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdb !11/2. */
FNIEMOP_DEF_1(iemOp_fist_m32i, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fist m32i");
    IEM_MC_BEGIN(3, 2);
    IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);
    IEM_MC_LOCAL(uint16_t,              u16Fsw);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,    pu16Fsw,    u16Fsw, 0);
    IEM_MC_ARG(int32_t *,               pi32Dst,            1);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value,          2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_MEM_MAP(pi32Dst, IEM_ACCESS_DATA_W, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1 /*arg*/);
    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value, 0)
        IEM_MC_CALL_FPU_AIMPL_3(iemAImpl_fist_r80_to_i32, pu16Fsw, pi32Dst, pr80Value);
        IEM_MC_MEM_COMMIT_AND_UNMAP_FOR_FPU_STORE(pi32Dst, IEM_ACCESS_DATA_W, u16Fsw);
        IEM_MC_UPDATE_FSW_WITH_MEM_OP(u16Fsw, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ELSE()
        IEM_MC_IF_FCW_IM()
            IEM_MC_STORE_MEM_I32_CONST_BY_REF(pi32Dst, INT32_MIN /* (integer indefinite) */);
            IEM_MC_MEM_COMMIT_AND_UNMAP(pi32Dst, IEM_ACCESS_DATA_W);
        IEM_MC_ENDIF();
        IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP(UINT8_MAX, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdb !11/3. */
FNIEMOP_DEF_1(iemOp_fistp_m32i, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fisttp m32i");
    IEM_MC_BEGIN(3, 2);
    IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);
    IEM_MC_LOCAL(uint16_t,              u16Fsw);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,    pu16Fsw,    u16Fsw, 0);
    IEM_MC_ARG(int32_t *,               pi32Dst,            1);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value,          2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_MEM_MAP(pi32Dst, IEM_ACCESS_DATA_W, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1 /*arg*/);
    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value, 0)
        IEM_MC_CALL_FPU_AIMPL_3(iemAImpl_fist_r80_to_i32, pu16Fsw, pi32Dst, pr80Value);
        IEM_MC_MEM_COMMIT_AND_UNMAP_FOR_FPU_STORE(pi32Dst, IEM_ACCESS_DATA_W, u16Fsw);
        IEM_MC_UPDATE_FSW_WITH_MEM_OP_THEN_POP(u16Fsw, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ELSE()
        IEM_MC_IF_FCW_IM()
            IEM_MC_STORE_MEM_I32_CONST_BY_REF(pi32Dst, INT32_MIN /* (integer indefinite) */);
            IEM_MC_MEM_COMMIT_AND_UNMAP(pi32Dst, IEM_ACCESS_DATA_W);
        IEM_MC_ENDIF();
        IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP_THEN_POP(UINT8_MAX, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdb !11/5. */
FNIEMOP_DEF_1(iemOp_fld_m80r, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fld m80r");

    IEM_MC_BEGIN(2, 3);
    IEM_MC_LOCAL(RTGCPTR,               GCPtrEffSrc);
    IEM_MC_LOCAL(IEMFPURESULT,          FpuRes);
    IEM_MC_LOCAL(RTFLOAT80U,            r80Val);
    IEM_MC_ARG_LOCAL_REF(PIEMFPURESULT, pFpuRes,    FpuRes, 0);
    IEM_MC_ARG_LOCAL_REF(PCRTFLOAT80U,  pr80Val,    r80Val, 1);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_FETCH_MEM_R80(r80Val, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_IS_EMPTY(7)
        IEM_MC_CALL_FPU_AIMPL_2(iemAImpl_fld_r80_from_r80, pFpuRes, pr80Val);
        IEM_MC_PUSH_FPU_RESULT_MEM_OP(FpuRes, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_PUSH_OVERFLOW_MEM_OP(pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdb !11/7. */
FNIEMOP_DEF_1(iemOp_fstp_m80r, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fstp m80r");
    IEM_MC_BEGIN(3, 2);
    IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);
    IEM_MC_LOCAL(uint16_t,              u16Fsw);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,    pu16Fsw,    u16Fsw, 0);
    IEM_MC_ARG(PRTFLOAT80U,             pr80Dst,            1);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value,          2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_MEM_MAP(pr80Dst, IEM_ACCESS_DATA_W, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1 /*arg*/);
    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value, 0)
        IEM_MC_CALL_FPU_AIMPL_3(iemAImpl_fst_r80_to_r80, pu16Fsw, pr80Dst, pr80Value);
        IEM_MC_MEM_COMMIT_AND_UNMAP_FOR_FPU_STORE(pr80Dst, IEM_ACCESS_DATA_W, u16Fsw);
        IEM_MC_UPDATE_FSW_WITH_MEM_OP_THEN_POP(u16Fsw, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ELSE()
        IEM_MC_IF_FCW_IM()
            IEM_MC_STORE_MEM_NEG_QNAN_R80_BY_REF(pr80Dst);
            IEM_MC_MEM_COMMIT_AND_UNMAP(pr80Dst, IEM_ACCESS_DATA_W);
        IEM_MC_ENDIF();
        IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP_THEN_POP(UINT8_MAX, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdb 11/0. */
FNIEMOP_DEF_1(iemOp_fcmovnb_stN,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fcmovnb st0,stN");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(0, 1);
    IEM_MC_LOCAL(PCRTFLOAT80U,      pr80ValueN);

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_TWO_FPUREGS_NOT_EMPTY_REF_R80_FIRST(pr80ValueN, bRm & X86_MODRM_RM_MASK, 0)
        IEM_MC_IF_EFL_BIT_NOT_SET(X86_EFL_CF)
            IEM_MC_STORE_FPUREG_R80_SRC_REF(0, pr80ValueN);
        IEM_MC_ENDIF();
        IEM_MC_UPDATE_FPU_OPCODE_IP();
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW(0);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdb 11/1. */
FNIEMOP_DEF_1(iemOp_fcmovne_stN,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fcmovne st0,stN");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(0, 1);
    IEM_MC_LOCAL(PCRTFLOAT80U,      pr80ValueN);

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_TWO_FPUREGS_NOT_EMPTY_REF_R80_FIRST(pr80ValueN, bRm & X86_MODRM_RM_MASK, 0)
        IEM_MC_IF_EFL_BIT_NOT_SET(X86_EFL_ZF)
            IEM_MC_STORE_FPUREG_R80_SRC_REF(0, pr80ValueN);
        IEM_MC_ENDIF();
        IEM_MC_UPDATE_FPU_OPCODE_IP();
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW(0);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdb 11/2. */
FNIEMOP_DEF_1(iemOp_fcmovnbe_stN, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fcmovnbe st0,stN");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(0, 1);
    IEM_MC_LOCAL(PCRTFLOAT80U,      pr80ValueN);

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_TWO_FPUREGS_NOT_EMPTY_REF_R80_FIRST(pr80ValueN, bRm & X86_MODRM_RM_MASK, 0)
        IEM_MC_IF_EFL_NO_BITS_SET(X86_EFL_CF | X86_EFL_ZF)
            IEM_MC_STORE_FPUREG_R80_SRC_REF(0, pr80ValueN);
        IEM_MC_ENDIF();
        IEM_MC_UPDATE_FPU_OPCODE_IP();
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW(0);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdb 11/3. */
FNIEMOP_DEF_1(iemOp_fcmovnnu_stN, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fcmovnnu st0,stN");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(0, 1);
    IEM_MC_LOCAL(PCRTFLOAT80U,      pr80ValueN);

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_TWO_FPUREGS_NOT_EMPTY_REF_R80_FIRST(pr80ValueN, bRm & X86_MODRM_RM_MASK, 0)
        IEM_MC_IF_EFL_BIT_NOT_SET(X86_EFL_PF)
            IEM_MC_STORE_FPUREG_R80_SRC_REF(0, pr80ValueN);
        IEM_MC_ENDIF();
        IEM_MC_UPDATE_FPU_OPCODE_IP();
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW(0);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdb 0xe0. */
FNIEMOP_DEF(iemOp_fneni)
{
    IEMOP_MNEMONIC("fneni (8087/ign)");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_BEGIN(0,0);
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdb 0xe1. */
FNIEMOP_DEF(iemOp_fndisi)
{
    IEMOP_MNEMONIC("fndisi (8087/ign)");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_BEGIN(0,0);
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdb 0xe2. */
FNIEMOP_DEF(iemOp_fnclex)
{
    IEMOP_MNEMONIC("fnclex");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(0,0);
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_ACTUALIZE_FPU_STATE_FOR_CHANGE();
    IEM_MC_CLEAR_FSW_EX();
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdb 0xe3. */
FNIEMOP_DEF(iemOp_fninit)
{
    IEMOP_MNEMONIC("fninit");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_finit, false /*fCheckXcpts*/);
}


/** Opcode 0xdb 0xe4. */
FNIEMOP_DEF(iemOp_fnsetpm)
{
    IEMOP_MNEMONIC("fnsetpm (80287/ign)");   /* set protected mode on fpu. */
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_BEGIN(0,0);
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdb 0xe5. */
FNIEMOP_DEF(iemOp_frstpm)
{
    IEMOP_MNEMONIC("frstpm (80287XL/ign)"); /* reset pm, back to real mode. */
#if 0 /* #UDs on newer CPUs */
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_BEGIN(0,0);
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
#else
    return IEMOP_RAISE_INVALID_OPCODE();
#endif
}


/** Opcode 0xdb 11/5. */
FNIEMOP_DEF_1(iemOp_fucomi_stN, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fucomi st0,stN");
    return IEM_MC_DEFER_TO_CIMPL_3(iemCImpl_fcomi_fucomi, bRm & X86_MODRM_RM_MASK, iemAImpl_fucomi_r80_by_r80, false /*fPop*/);
}


/** Opcode 0xdb 11/6. */
FNIEMOP_DEF_1(iemOp_fcomi_stN,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fcomi st0,stN");
    return IEM_MC_DEFER_TO_CIMPL_3(iemCImpl_fcomi_fucomi, bRm & X86_MODRM_RM_MASK, iemAImpl_fcomi_r80_by_r80, false /*fPop*/);
}


/** Opcode 0xdb. */
FNIEMOP_DEF(iemOp_EscF3)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    pVCpu->iem.s.uFpuOpcode = RT_MAKE_U16(bRm, 0xdb & 0x7);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
        {
            case 0: return FNIEMOP_CALL_1(iemOp_fcmovnb_stN,  bRm);
            case 1: return FNIEMOP_CALL_1(iemOp_fcmovne_stN,  bRm);
            case 2: return FNIEMOP_CALL_1(iemOp_fcmovnbe_stN, bRm);
            case 3: return FNIEMOP_CALL_1(iemOp_fcmovnnu_stN, bRm);
            case 4:
                switch (bRm)
                {
                    case 0xe0:  return FNIEMOP_CALL(iemOp_fneni);
                    case 0xe1:  return FNIEMOP_CALL(iemOp_fndisi);
                    case 0xe2:  return FNIEMOP_CALL(iemOp_fnclex);
                    case 0xe3:  return FNIEMOP_CALL(iemOp_fninit);
                    case 0xe4:  return FNIEMOP_CALL(iemOp_fnsetpm);
                    case 0xe5:  return FNIEMOP_CALL(iemOp_frstpm);
                    case 0xe6:  return IEMOP_RAISE_INVALID_OPCODE();
                    case 0xe7:  return IEMOP_RAISE_INVALID_OPCODE();
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }
                break;
            case 5: return FNIEMOP_CALL_1(iemOp_fucomi_stN, bRm);
            case 6: return FNIEMOP_CALL_1(iemOp_fcomi_stN,  bRm);
            case 7: return IEMOP_RAISE_INVALID_OPCODE();
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
        {
            case 0: return FNIEMOP_CALL_1(iemOp_fild_m32i,  bRm);
            case 1: return FNIEMOP_CALL_1(iemOp_fisttp_m32i,bRm);
            case 2: return FNIEMOP_CALL_1(iemOp_fist_m32i,  bRm);
            case 3: return FNIEMOP_CALL_1(iemOp_fistp_m32i, bRm);
            case 4: return IEMOP_RAISE_INVALID_OPCODE();
            case 5: return FNIEMOP_CALL_1(iemOp_fld_m80r,   bRm);
            case 6: return IEMOP_RAISE_INVALID_OPCODE();
            case 7: return FNIEMOP_CALL_1(iemOp_fstp_m80r,  bRm);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


/**
 * Common worker for FPU instructions working on STn and ST0, and storing the
 * result in STn unless IE, DE or ZE was raised.
 *
 * @param   pfnAImpl    Pointer to the instruction implementation (assembly).
 */
FNIEMOP_DEF_2(iemOpHlpFpu_stN_st0, uint8_t, bRm, PFNIEMAIMPLFPUR80, pfnAImpl)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(3, 1);
    IEM_MC_LOCAL(IEMFPURESULT,          FpuRes);
    IEM_MC_ARG_LOCAL_REF(PIEMFPURESULT, pFpuRes,        FpuRes,     0);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value1,                 1);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value2,                 2);

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_TWO_FPUREGS_NOT_EMPTY_REF_R80(pr80Value1, bRm & X86_MODRM_RM_MASK, pr80Value2, 0)
        IEM_MC_CALL_FPU_AIMPL_3(pfnAImpl, pFpuRes, pr80Value1, pr80Value2);
        IEM_MC_STORE_FPU_RESULT(FpuRes, bRm & X86_MODRM_RM_MASK);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW(bRm & X86_MODRM_RM_MASK);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdc 11/0. */
FNIEMOP_DEF_1(iemOp_fadd_stN_st0,   uint8_t, bRm)
{
    IEMOP_MNEMONIC("fadd stN,st0");
    return FNIEMOP_CALL_2(iemOpHlpFpu_stN_st0, bRm, iemAImpl_fadd_r80_by_r80);
}


/** Opcode 0xdc 11/1. */
FNIEMOP_DEF_1(iemOp_fmul_stN_st0,   uint8_t, bRm)
{
    IEMOP_MNEMONIC("fmul stN,st0");
    return FNIEMOP_CALL_2(iemOpHlpFpu_stN_st0, bRm, iemAImpl_fmul_r80_by_r80);
}


/** Opcode 0xdc 11/4. */
FNIEMOP_DEF_1(iemOp_fsubr_stN_st0,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fsubr stN,st0");
    return FNIEMOP_CALL_2(iemOpHlpFpu_stN_st0, bRm, iemAImpl_fsubr_r80_by_r80);
}


/** Opcode 0xdc 11/5. */
FNIEMOP_DEF_1(iemOp_fsub_stN_st0,   uint8_t, bRm)
{
    IEMOP_MNEMONIC("fsub stN,st0");
    return FNIEMOP_CALL_2(iemOpHlpFpu_stN_st0, bRm, iemAImpl_fsub_r80_by_r80);
}


/** Opcode 0xdc 11/6. */
FNIEMOP_DEF_1(iemOp_fdivr_stN_st0,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fdivr stN,st0");
    return FNIEMOP_CALL_2(iemOpHlpFpu_stN_st0, bRm, iemAImpl_fdivr_r80_by_r80);
}


/** Opcode 0xdc 11/7. */
FNIEMOP_DEF_1(iemOp_fdiv_stN_st0,   uint8_t, bRm)
{
    IEMOP_MNEMONIC("fdiv stN,st0");
    return FNIEMOP_CALL_2(iemOpHlpFpu_stN_st0, bRm, iemAImpl_fdiv_r80_by_r80);
}


/**
 * Common worker for FPU instructions working on ST0 and a 64-bit floating point
 * memory operand, and storing the result in ST0.
 *
 * @param   pfnAImpl    Pointer to the instruction implementation (assembly).
 */
FNIEMOP_DEF_2(iemOpHlpFpu_ST0_m64r, uint8_t, bRm, PFNIEMAIMPLFPUR64, pfnImpl)
{
    IEM_MC_BEGIN(3, 3);
    IEM_MC_LOCAL(RTGCPTR,               GCPtrEffSrc);
    IEM_MC_LOCAL(IEMFPURESULT,          FpuRes);
    IEM_MC_LOCAL(RTFLOAT64U,            r64Factor2);
    IEM_MC_ARG_LOCAL_REF(PIEMFPURESULT, pFpuRes,        FpuRes,     0);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Factor1,                1);
    IEM_MC_ARG_LOCAL_REF(PRTFLOAT64U,   pr64Factor2,    r64Factor2, 2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_FETCH_MEM_R64(r64Factor2, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Factor1, 0)
        IEM_MC_CALL_FPU_AIMPL_3(pfnImpl, pFpuRes, pr80Factor1, pr64Factor2);
        IEM_MC_STORE_FPU_RESULT_MEM_OP(FpuRes, 0, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP(0, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdc !11/0. */
FNIEMOP_DEF_1(iemOp_fadd_m64r,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fadd m64r");
    return FNIEMOP_CALL_2(iemOpHlpFpu_ST0_m64r, bRm, iemAImpl_fadd_r80_by_r64);
}


/** Opcode 0xdc !11/1. */
FNIEMOP_DEF_1(iemOp_fmul_m64r,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fmul m64r");
    return FNIEMOP_CALL_2(iemOpHlpFpu_ST0_m64r, bRm, iemAImpl_fmul_r80_by_r64);
}


/** Opcode 0xdc !11/2. */
FNIEMOP_DEF_1(iemOp_fcom_m64r,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fcom st0,m64r");

    IEM_MC_BEGIN(3, 3);
    IEM_MC_LOCAL(RTGCPTR,               GCPtrEffSrc);
    IEM_MC_LOCAL(uint16_t,              u16Fsw);
    IEM_MC_LOCAL(RTFLOAT64U,            r64Val2);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,    pu16Fsw,        u16Fsw,     0);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value1,                 1);
    IEM_MC_ARG_LOCAL_REF(PCRTFLOAT64U,  pr64Val2,       r64Val2,    2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_FETCH_MEM_R64(r64Val2, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value1, 0)
        IEM_MC_CALL_FPU_AIMPL_3(iemAImpl_fcom_r80_by_r64, pu16Fsw, pr80Value1, pr64Val2);
        IEM_MC_UPDATE_FSW_WITH_MEM_OP(u16Fsw, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP(UINT8_MAX, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdc !11/3. */
FNIEMOP_DEF_1(iemOp_fcomp_m64r, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fcomp st0,m64r");

    IEM_MC_BEGIN(3, 3);
    IEM_MC_LOCAL(RTGCPTR,               GCPtrEffSrc);
    IEM_MC_LOCAL(uint16_t,              u16Fsw);
    IEM_MC_LOCAL(RTFLOAT64U,            r64Val2);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,    pu16Fsw,        u16Fsw,     0);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value1,                 1);
    IEM_MC_ARG_LOCAL_REF(PCRTFLOAT64U,  pr64Val2,       r64Val2,    2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_FETCH_MEM_R64(r64Val2, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value1, 0)
        IEM_MC_CALL_FPU_AIMPL_3(iemAImpl_fcom_r80_by_r64, pu16Fsw, pr80Value1, pr64Val2);
        IEM_MC_UPDATE_FSW_WITH_MEM_OP_THEN_POP(u16Fsw, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP_THEN_POP(UINT8_MAX, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdc !11/4. */
FNIEMOP_DEF_1(iemOp_fsub_m64r,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fsub m64r");
    return FNIEMOP_CALL_2(iemOpHlpFpu_ST0_m64r, bRm, iemAImpl_fsub_r80_by_r64);
}


/** Opcode 0xdc !11/5. */
FNIEMOP_DEF_1(iemOp_fsubr_m64r, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fsubr m64r");
    return FNIEMOP_CALL_2(iemOpHlpFpu_ST0_m64r, bRm, iemAImpl_fsubr_r80_by_r64);
}


/** Opcode 0xdc !11/6. */
FNIEMOP_DEF_1(iemOp_fdiv_m64r,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fdiv m64r");
    return FNIEMOP_CALL_2(iemOpHlpFpu_ST0_m64r, bRm, iemAImpl_fdiv_r80_by_r64);
}


/** Opcode 0xdc !11/7. */
FNIEMOP_DEF_1(iemOp_fdivr_m64r, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fdivr m64r");
    return FNIEMOP_CALL_2(iemOpHlpFpu_ST0_m64r, bRm, iemAImpl_fdivr_r80_by_r64);
}


/** Opcode 0xdc. */
FNIEMOP_DEF(iemOp_EscF4)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    pVCpu->iem.s.uFpuOpcode = RT_MAKE_U16(bRm, 0xdc & 0x7);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
        {
            case 0: return FNIEMOP_CALL_1(iemOp_fadd_stN_st0,  bRm);
            case 1: return FNIEMOP_CALL_1(iemOp_fmul_stN_st0,  bRm);
            case 2: return FNIEMOP_CALL_1(iemOp_fcom_stN,      bRm); /* Marked reserved, intel behavior is that of FCOM ST(i). */
            case 3: return FNIEMOP_CALL_1(iemOp_fcomp_stN,     bRm); /* Marked reserved, intel behavior is that of FCOMP ST(i). */
            case 4: return FNIEMOP_CALL_1(iemOp_fsubr_stN_st0, bRm);
            case 5: return FNIEMOP_CALL_1(iemOp_fsub_stN_st0,  bRm);
            case 6: return FNIEMOP_CALL_1(iemOp_fdivr_stN_st0, bRm);
            case 7: return FNIEMOP_CALL_1(iemOp_fdiv_stN_st0,  bRm);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
        {
            case 0: return FNIEMOP_CALL_1(iemOp_fadd_m64r,  bRm);
            case 1: return FNIEMOP_CALL_1(iemOp_fmul_m64r,  bRm);
            case 2: return FNIEMOP_CALL_1(iemOp_fcom_m64r,  bRm);
            case 3: return FNIEMOP_CALL_1(iemOp_fcomp_m64r, bRm);
            case 4: return FNIEMOP_CALL_1(iemOp_fsub_m64r,  bRm);
            case 5: return FNIEMOP_CALL_1(iemOp_fsubr_m64r, bRm);
            case 6: return FNIEMOP_CALL_1(iemOp_fdiv_m64r,  bRm);
            case 7: return FNIEMOP_CALL_1(iemOp_fdivr_m64r, bRm);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


/** Opcode 0xdd !11/0.
 * @sa iemOp_fld_m32r */
FNIEMOP_DEF_1(iemOp_fld_m64r,    uint8_t, bRm)
{
    IEMOP_MNEMONIC("fld m64r");

    IEM_MC_BEGIN(2, 3);
    IEM_MC_LOCAL(RTGCPTR,               GCPtrEffSrc);
    IEM_MC_LOCAL(IEMFPURESULT,          FpuRes);
    IEM_MC_LOCAL(RTFLOAT64U,            r64Val);
    IEM_MC_ARG_LOCAL_REF(PIEMFPURESULT, pFpuRes,    FpuRes, 0);
    IEM_MC_ARG_LOCAL_REF(PCRTFLOAT64U,  pr64Val,    r64Val, 1);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_FETCH_MEM_R64(r64Val, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_IS_EMPTY(7)
        IEM_MC_CALL_FPU_AIMPL_2(iemAImpl_fld_r64_to_r80, pFpuRes, pr64Val);
        IEM_MC_PUSH_FPU_RESULT_MEM_OP(FpuRes, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_PUSH_OVERFLOW_MEM_OP(pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdd !11/0. */
FNIEMOP_DEF_1(iemOp_fisttp_m64i, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fisttp m64i");
    IEM_MC_BEGIN(3, 2);
    IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);
    IEM_MC_LOCAL(uint16_t,              u16Fsw);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,    pu16Fsw,    u16Fsw, 0);
    IEM_MC_ARG(int64_t *,               pi64Dst,            1);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value,          2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_MEM_MAP(pi64Dst, IEM_ACCESS_DATA_W, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1 /*arg*/);
    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value, 0)
        IEM_MC_CALL_FPU_AIMPL_3(iemAImpl_fistt_r80_to_i64, pu16Fsw, pi64Dst, pr80Value);
        IEM_MC_MEM_COMMIT_AND_UNMAP_FOR_FPU_STORE(pi64Dst, IEM_ACCESS_DATA_W, u16Fsw);
        IEM_MC_UPDATE_FSW_WITH_MEM_OP_THEN_POP(u16Fsw, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ELSE()
        IEM_MC_IF_FCW_IM()
            IEM_MC_STORE_MEM_I64_CONST_BY_REF(pi64Dst, INT64_MIN /* (integer indefinite) */);
            IEM_MC_MEM_COMMIT_AND_UNMAP(pi64Dst, IEM_ACCESS_DATA_W);
        IEM_MC_ENDIF();
        IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP_THEN_POP(UINT8_MAX, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdd !11/0. */
FNIEMOP_DEF_1(iemOp_fst_m64r,    uint8_t, bRm)
{
    IEMOP_MNEMONIC("fst m64r");
    IEM_MC_BEGIN(3, 2);
    IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);
    IEM_MC_LOCAL(uint16_t,              u16Fsw);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,    pu16Fsw,    u16Fsw, 0);
    IEM_MC_ARG(PRTFLOAT64U,             pr64Dst,            1);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value,          2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_MEM_MAP(pr64Dst, IEM_ACCESS_DATA_W, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1 /*arg*/);
    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value, 0)
        IEM_MC_CALL_FPU_AIMPL_3(iemAImpl_fst_r80_to_r64, pu16Fsw, pr64Dst, pr80Value);
        IEM_MC_MEM_COMMIT_AND_UNMAP_FOR_FPU_STORE(pr64Dst, IEM_ACCESS_DATA_W, u16Fsw);
        IEM_MC_UPDATE_FSW_WITH_MEM_OP(u16Fsw, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ELSE()
        IEM_MC_IF_FCW_IM()
            IEM_MC_STORE_MEM_NEG_QNAN_R64_BY_REF(pr64Dst);
            IEM_MC_MEM_COMMIT_AND_UNMAP(pr64Dst, IEM_ACCESS_DATA_W);
        IEM_MC_ENDIF();
        IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP(UINT8_MAX, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}




/** Opcode 0xdd !11/0. */
FNIEMOP_DEF_1(iemOp_fstp_m64r,   uint8_t, bRm)
{
    IEMOP_MNEMONIC("fstp m64r");
    IEM_MC_BEGIN(3, 2);
    IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);
    IEM_MC_LOCAL(uint16_t,              u16Fsw);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,    pu16Fsw,    u16Fsw, 0);
    IEM_MC_ARG(PRTFLOAT64U,             pr64Dst,            1);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value,          2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_MEM_MAP(pr64Dst, IEM_ACCESS_DATA_W, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1 /*arg*/);
    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value, 0)
        IEM_MC_CALL_FPU_AIMPL_3(iemAImpl_fst_r80_to_r64, pu16Fsw, pr64Dst, pr80Value);
        IEM_MC_MEM_COMMIT_AND_UNMAP_FOR_FPU_STORE(pr64Dst, IEM_ACCESS_DATA_W, u16Fsw);
        IEM_MC_UPDATE_FSW_WITH_MEM_OP_THEN_POP(u16Fsw, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ELSE()
        IEM_MC_IF_FCW_IM()
            IEM_MC_STORE_MEM_NEG_QNAN_R64_BY_REF(pr64Dst);
            IEM_MC_MEM_COMMIT_AND_UNMAP(pr64Dst, IEM_ACCESS_DATA_W);
        IEM_MC_ENDIF();
        IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP_THEN_POP(UINT8_MAX, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdd !11/0. */
FNIEMOP_DEF_1(iemOp_frstor,      uint8_t, bRm)
{
    IEMOP_MNEMONIC("frstor m94/108byte");
    IEM_MC_BEGIN(3, 0);
    IEM_MC_ARG_CONST(IEMMODE,           enmEffOpSize, /*=*/ pVCpu->iem.s.enmEffOpSize,  0);
    IEM_MC_ARG(uint8_t,                 iEffSeg,                                    1);
    IEM_MC_ARG(RTGCPTR,                 GCPtrEffSrc,                                2);
    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_ACTUALIZE_FPU_STATE_FOR_CHANGE();
    IEM_MC_ASSIGN(iEffSeg, pVCpu->iem.s.iEffSeg);
    IEM_MC_CALL_CIMPL_3(iemCImpl_frstor, enmEffOpSize, iEffSeg, GCPtrEffSrc);
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdd !11/0. */
FNIEMOP_DEF_1(iemOp_fnsave,      uint8_t, bRm)
{
    IEMOP_MNEMONIC("fnsave m94/108byte");
    IEM_MC_BEGIN(3, 0);
    IEM_MC_ARG_CONST(IEMMODE,           enmEffOpSize, /*=*/ pVCpu->iem.s.enmEffOpSize,  0);
    IEM_MC_ARG(uint8_t,                 iEffSeg,                                    1);
    IEM_MC_ARG(RTGCPTR,                 GCPtrEffDst,                                2);
    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_ACTUALIZE_FPU_STATE_FOR_READ();
    IEM_MC_ASSIGN(iEffSeg, pVCpu->iem.s.iEffSeg);
    IEM_MC_CALL_CIMPL_3(iemCImpl_fnsave, enmEffOpSize, iEffSeg, GCPtrEffDst);
    IEM_MC_END();
    return VINF_SUCCESS;

}

/** Opcode 0xdd !11/0. */
FNIEMOP_DEF_1(iemOp_fnstsw,      uint8_t, bRm)
{
    IEMOP_MNEMONIC("fnstsw m16");

    IEM_MC_BEGIN(0, 2);
    IEM_MC_LOCAL(uint16_t, u16Tmp);
    IEM_MC_LOCAL(RTGCPTR,  GCPtrEffDst);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();

    IEM_MC_ACTUALIZE_FPU_STATE_FOR_READ();
    IEM_MC_FETCH_FSW(u16Tmp);
    IEM_MC_STORE_MEM_U16(pVCpu->iem.s.iEffSeg, GCPtrEffDst, u16Tmp);
    IEM_MC_ADVANCE_RIP();

/** @todo Debug / drop a hint to the verifier that things may differ
 * from REM. Seen 0x4020 (iem) vs 0x4000 (rem) at 0008:801c6b88 booting
 * NT4SP1. (X86_FSW_PE) */
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdd 11/0. */
FNIEMOP_DEF_1(iemOp_ffree_stN,   uint8_t, bRm)
{
    IEMOP_MNEMONIC("ffree stN");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    /* Note! C0, C1, C2 and C3 are documented as undefined, we leave the
             unmodified. */

    IEM_MC_BEGIN(0, 0);

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_ACTUALIZE_FPU_STATE_FOR_CHANGE();
    IEM_MC_FPU_STACK_FREE(bRm & X86_MODRM_RM_MASK);
    IEM_MC_UPDATE_FPU_OPCODE_IP();

    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdd 11/1. */
FNIEMOP_DEF_1(iemOp_fst_stN,     uint8_t, bRm)
{
    IEMOP_MNEMONIC("fst st0,stN");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(0, 2);
    IEM_MC_LOCAL(PCRTFLOAT80U,          pr80Value);
    IEM_MC_LOCAL(IEMFPURESULT,          FpuRes);
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value, 0)
        IEM_MC_SET_FPU_RESULT(FpuRes, 0 /*FSW*/, pr80Value);
        IEM_MC_STORE_FPU_RESULT(FpuRes, bRm & X86_MODRM_RM_MASK);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW(bRm & X86_MODRM_RM_MASK);
    IEM_MC_ENDIF();

    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdd 11/3. */
FNIEMOP_DEF_1(iemOp_fucom_stN_st0, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fcom st0,stN");
    return FNIEMOP_CALL_2(iemOpHlpFpuNoStore_st0_stN, bRm, iemAImpl_fucom_r80_by_r80);
}


/** Opcode 0xdd 11/4. */
FNIEMOP_DEF_1(iemOp_fucomp_stN,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fcomp st0,stN");
    return FNIEMOP_CALL_2(iemOpHlpFpuNoStore_st0_stN_pop, bRm, iemAImpl_fucom_r80_by_r80);
}


/** Opcode 0xdd. */
FNIEMOP_DEF(iemOp_EscF5)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    pVCpu->iem.s.uFpuOpcode = RT_MAKE_U16(bRm, 0xdd & 0x7);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
        {
            case 0: return FNIEMOP_CALL_1(iemOp_ffree_stN,   bRm);
            case 1: return FNIEMOP_CALL_1(iemOp_fxch_stN,    bRm); /* Reserved, intel behavior is that of XCHG ST(i). */
            case 2: return FNIEMOP_CALL_1(iemOp_fst_stN,     bRm);
            case 3: return FNIEMOP_CALL_1(iemOp_fstp_stN,    bRm);
            case 4: return FNIEMOP_CALL_1(iemOp_fucom_stN_st0,bRm);
            case 5: return FNIEMOP_CALL_1(iemOp_fucomp_stN,  bRm);
            case 6: return IEMOP_RAISE_INVALID_OPCODE();
            case 7: return IEMOP_RAISE_INVALID_OPCODE();
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
        {
            case 0: return FNIEMOP_CALL_1(iemOp_fld_m64r,    bRm);
            case 1: return FNIEMOP_CALL_1(iemOp_fisttp_m64i, bRm);
            case 2: return FNIEMOP_CALL_1(iemOp_fst_m64r,    bRm);
            case 3: return FNIEMOP_CALL_1(iemOp_fstp_m64r,   bRm);
            case 4: return FNIEMOP_CALL_1(iemOp_frstor,      bRm);
            case 5: return IEMOP_RAISE_INVALID_OPCODE();
            case 6: return FNIEMOP_CALL_1(iemOp_fnsave,      bRm);
            case 7: return FNIEMOP_CALL_1(iemOp_fnstsw,      bRm);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


/** Opcode 0xde 11/0. */
FNIEMOP_DEF_1(iemOp_faddp_stN_st0, uint8_t, bRm)
{
    IEMOP_MNEMONIC("faddp stN,st0");
    return FNIEMOP_CALL_2(iemOpHlpFpu_stN_st0_pop, bRm, iemAImpl_fadd_r80_by_r80);
}


/** Opcode 0xde 11/0. */
FNIEMOP_DEF_1(iemOp_fmulp_stN_st0, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fmulp stN,st0");
    return FNIEMOP_CALL_2(iemOpHlpFpu_stN_st0_pop, bRm, iemAImpl_fmul_r80_by_r80);
}


/** Opcode 0xde 0xd9. */
FNIEMOP_DEF(iemOp_fcompp)
{
    IEMOP_MNEMONIC("fucompp st0,stN");
    return FNIEMOP_CALL_1(iemOpHlpFpuNoStore_st0_stN_pop_pop, iemAImpl_fcom_r80_by_r80);
}


/** Opcode 0xde 11/4. */
FNIEMOP_DEF_1(iemOp_fsubrp_stN_st0, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fsubrp stN,st0");
    return FNIEMOP_CALL_2(iemOpHlpFpu_stN_st0_pop, bRm, iemAImpl_fsubr_r80_by_r80);
}


/** Opcode 0xde 11/5. */
FNIEMOP_DEF_1(iemOp_fsubp_stN_st0, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fsubp stN,st0");
    return FNIEMOP_CALL_2(iemOpHlpFpu_stN_st0_pop, bRm, iemAImpl_fsub_r80_by_r80);
}


/** Opcode 0xde 11/6. */
FNIEMOP_DEF_1(iemOp_fdivrp_stN_st0, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fdivrp stN,st0");
    return FNIEMOP_CALL_2(iemOpHlpFpu_stN_st0_pop, bRm, iemAImpl_fdivr_r80_by_r80);
}


/** Opcode 0xde 11/7. */
FNIEMOP_DEF_1(iemOp_fdivp_stN_st0, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fdivp stN,st0");
    return FNIEMOP_CALL_2(iemOpHlpFpu_stN_st0_pop, bRm, iemAImpl_fdiv_r80_by_r80);
}


/**
 * Common worker for FPU instructions working on ST0 and an m16i, and storing
 * the result in ST0.
 *
 * @param   pfnAImpl    Pointer to the instruction implementation (assembly).
 */
FNIEMOP_DEF_2(iemOpHlpFpu_st0_m16i, uint8_t, bRm, PFNIEMAIMPLFPUI16, pfnAImpl)
{
    IEM_MC_BEGIN(3, 3);
    IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
    IEM_MC_LOCAL(IEMFPURESULT,              FpuRes);
    IEM_MC_LOCAL(int16_t,                   i16Val2);
    IEM_MC_ARG_LOCAL_REF(PIEMFPURESULT,     pFpuRes,        FpuRes,     0);
    IEM_MC_ARG(PCRTFLOAT80U,                pr80Value1,                 1);
    IEM_MC_ARG_LOCAL_REF(int16_t const *,   pi16Val2,       i16Val2,    2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_FETCH_MEM_I16(i16Val2, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value1, 0)
        IEM_MC_CALL_FPU_AIMPL_3(pfnAImpl, pFpuRes, pr80Value1, pi16Val2);
        IEM_MC_STORE_FPU_RESULT(FpuRes, 0);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW(0);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xde !11/0. */
FNIEMOP_DEF_1(iemOp_fiadd_m16i,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fiadd m16i");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_m16i, bRm, iemAImpl_fiadd_r80_by_i16);
}


/** Opcode 0xde !11/1. */
FNIEMOP_DEF_1(iemOp_fimul_m16i,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fimul m16i");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_m16i, bRm, iemAImpl_fimul_r80_by_i16);
}


/** Opcode 0xde !11/2. */
FNIEMOP_DEF_1(iemOp_ficom_m16i,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("ficom st0,m16i");

    IEM_MC_BEGIN(3, 3);
    IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
    IEM_MC_LOCAL(uint16_t,                  u16Fsw);
    IEM_MC_LOCAL(int16_t,                   i16Val2);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,        pu16Fsw,        u16Fsw,     0);
    IEM_MC_ARG(PCRTFLOAT80U,                pr80Value1,                 1);
    IEM_MC_ARG_LOCAL_REF(int16_t const *,   pi16Val2,       i16Val2,    2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_FETCH_MEM_I16(i16Val2, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value1, 0)
        IEM_MC_CALL_FPU_AIMPL_3(iemAImpl_ficom_r80_by_i16, pu16Fsw, pr80Value1, pi16Val2);
        IEM_MC_UPDATE_FSW_WITH_MEM_OP(u16Fsw, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP(UINT8_MAX, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xde !11/3. */
FNIEMOP_DEF_1(iemOp_ficomp_m16i, uint8_t, bRm)
{
    IEMOP_MNEMONIC("ficomp st0,m16i");

    IEM_MC_BEGIN(3, 3);
    IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
    IEM_MC_LOCAL(uint16_t,                  u16Fsw);
    IEM_MC_LOCAL(int16_t,                   i16Val2);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,        pu16Fsw,        u16Fsw,     0);
    IEM_MC_ARG(PCRTFLOAT80U,                pr80Value1,                 1);
    IEM_MC_ARG_LOCAL_REF(int16_t const *,   pi16Val2,       i16Val2,    2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_FETCH_MEM_I16(i16Val2, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value1, 0)
        IEM_MC_CALL_FPU_AIMPL_3(iemAImpl_ficom_r80_by_i16, pu16Fsw, pr80Value1, pi16Val2);
        IEM_MC_UPDATE_FSW_WITH_MEM_OP_THEN_POP(u16Fsw, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP_THEN_POP(UINT8_MAX, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xde !11/4. */
FNIEMOP_DEF_1(iemOp_fisub_m16i,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fisub m16i");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_m16i, bRm, iemAImpl_fisub_r80_by_i16);
}


/** Opcode 0xde !11/5. */
FNIEMOP_DEF_1(iemOp_fisubr_m16i, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fisubr m16i");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_m16i, bRm, iemAImpl_fisubr_r80_by_i16);
}


/** Opcode 0xde !11/6. */
FNIEMOP_DEF_1(iemOp_fidiv_m16i,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fiadd m16i");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_m16i, bRm, iemAImpl_fidiv_r80_by_i16);
}


/** Opcode 0xde !11/7. */
FNIEMOP_DEF_1(iemOp_fidivr_m16i, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fiadd m16i");
    return FNIEMOP_CALL_2(iemOpHlpFpu_st0_m16i, bRm, iemAImpl_fidivr_r80_by_i16);
}


/** Opcode 0xde. */
FNIEMOP_DEF(iemOp_EscF6)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    pVCpu->iem.s.uFpuOpcode = RT_MAKE_U16(bRm, 0xde & 0x7);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
        {
            case 0: return FNIEMOP_CALL_1(iemOp_faddp_stN_st0, bRm);
            case 1: return FNIEMOP_CALL_1(iemOp_fmulp_stN_st0, bRm);
            case 2: return FNIEMOP_CALL_1(iemOp_fcomp_stN, bRm);
            case 3: if (bRm == 0xd9)
                        return FNIEMOP_CALL(iemOp_fcompp);
                    return IEMOP_RAISE_INVALID_OPCODE();
            case 4: return FNIEMOP_CALL_1(iemOp_fsubrp_stN_st0, bRm);
            case 5: return FNIEMOP_CALL_1(iemOp_fsubp_stN_st0, bRm);
            case 6: return FNIEMOP_CALL_1(iemOp_fdivrp_stN_st0, bRm);
            case 7: return FNIEMOP_CALL_1(iemOp_fdivp_stN_st0, bRm);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
        {
            case 0: return FNIEMOP_CALL_1(iemOp_fiadd_m16i,  bRm);
            case 1: return FNIEMOP_CALL_1(iemOp_fimul_m16i,  bRm);
            case 2: return FNIEMOP_CALL_1(iemOp_ficom_m16i,  bRm);
            case 3: return FNIEMOP_CALL_1(iemOp_ficomp_m16i, bRm);
            case 4: return FNIEMOP_CALL_1(iemOp_fisub_m16i,  bRm);
            case 5: return FNIEMOP_CALL_1(iemOp_fisubr_m16i, bRm);
            case 6: return FNIEMOP_CALL_1(iemOp_fidiv_m16i,  bRm);
            case 7: return FNIEMOP_CALL_1(iemOp_fidivr_m16i, bRm);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


/** Opcode 0xdf 11/0.
 * Undocument instruction, assumed to work like ffree + fincstp.  */
FNIEMOP_DEF_1(iemOp_ffreep_stN, uint8_t, bRm)
{
    IEMOP_MNEMONIC("ffreep stN");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(0, 0);

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_ACTUALIZE_FPU_STATE_FOR_CHANGE();
    IEM_MC_FPU_STACK_FREE(bRm & X86_MODRM_RM_MASK);
    IEM_MC_FPU_STACK_INC_TOP();
    IEM_MC_UPDATE_FPU_OPCODE_IP();

    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdf 0xe0. */
FNIEMOP_DEF(iemOp_fnstsw_ax)
{
    IEMOP_MNEMONIC("fnstsw ax");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_BEGIN(0, 1);
    IEM_MC_LOCAL(uint16_t, u16Tmp);
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_ACTUALIZE_FPU_STATE_FOR_READ();
    IEM_MC_FETCH_FSW(u16Tmp);
    IEM_MC_STORE_GREG_U16(X86_GREG_xAX, u16Tmp);
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdf 11/5. */
FNIEMOP_DEF_1(iemOp_fucomip_st0_stN, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fcomip st0,stN");
    return IEM_MC_DEFER_TO_CIMPL_3(iemCImpl_fcomi_fucomi, bRm & X86_MODRM_RM_MASK, iemAImpl_fcomi_r80_by_r80, true /*fPop*/);
}


/** Opcode 0xdf 11/6. */
FNIEMOP_DEF_1(iemOp_fcomip_st0_stN,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fcomip st0,stN");
    return IEM_MC_DEFER_TO_CIMPL_3(iemCImpl_fcomi_fucomi, bRm & X86_MODRM_RM_MASK, iemAImpl_fcomi_r80_by_r80, true /*fPop*/);
}


/** Opcode 0xdf !11/0. */
FNIEMOP_DEF_1(iemOp_fild_m16i, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fild m16i");

    IEM_MC_BEGIN(2, 3);
    IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
    IEM_MC_LOCAL(IEMFPURESULT,              FpuRes);
    IEM_MC_LOCAL(int16_t,                   i16Val);
    IEM_MC_ARG_LOCAL_REF(PIEMFPURESULT,     pFpuRes,    FpuRes, 0);
    IEM_MC_ARG_LOCAL_REF(int16_t const *,   pi16Val,    i16Val, 1);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_FETCH_MEM_I16(i16Val, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_IS_EMPTY(7)
        IEM_MC_CALL_FPU_AIMPL_2(iemAImpl_fild_i16_to_r80, pFpuRes, pi16Val);
        IEM_MC_PUSH_FPU_RESULT_MEM_OP(FpuRes, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_PUSH_OVERFLOW_MEM_OP(pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdf !11/1. */
FNIEMOP_DEF_1(iemOp_fisttp_m16i, uint8_t, bRm)
{
    IEMOP_MNEMONIC("fisttp m16i");
    IEM_MC_BEGIN(3, 2);
    IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);
    IEM_MC_LOCAL(uint16_t,              u16Fsw);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,    pu16Fsw,    u16Fsw, 0);
    IEM_MC_ARG(int16_t *,               pi16Dst,            1);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value,          2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_MEM_MAP(pi16Dst, IEM_ACCESS_DATA_W, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1 /*arg*/);
    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value, 0)
        IEM_MC_CALL_FPU_AIMPL_3(iemAImpl_fistt_r80_to_i16, pu16Fsw, pi16Dst, pr80Value);
        IEM_MC_MEM_COMMIT_AND_UNMAP_FOR_FPU_STORE(pi16Dst, IEM_ACCESS_DATA_W, u16Fsw);
        IEM_MC_UPDATE_FSW_WITH_MEM_OP_THEN_POP(u16Fsw, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ELSE()
        IEM_MC_IF_FCW_IM()
            IEM_MC_STORE_MEM_I16_CONST_BY_REF(pi16Dst, INT16_MIN /* (integer indefinite) */);
            IEM_MC_MEM_COMMIT_AND_UNMAP(pi16Dst, IEM_ACCESS_DATA_W);
        IEM_MC_ENDIF();
        IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP_THEN_POP(UINT8_MAX, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdf !11/2. */
FNIEMOP_DEF_1(iemOp_fist_m16i,   uint8_t, bRm)
{
    IEMOP_MNEMONIC("fistp m16i");
    IEM_MC_BEGIN(3, 2);
    IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);
    IEM_MC_LOCAL(uint16_t,              u16Fsw);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,    pu16Fsw,    u16Fsw, 0);
    IEM_MC_ARG(int16_t *,               pi16Dst,            1);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value,          2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_MEM_MAP(pi16Dst, IEM_ACCESS_DATA_W, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1 /*arg*/);
    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value, 0)
        IEM_MC_CALL_FPU_AIMPL_3(iemAImpl_fist_r80_to_i16, pu16Fsw, pi16Dst, pr80Value);
        IEM_MC_MEM_COMMIT_AND_UNMAP_FOR_FPU_STORE(pi16Dst, IEM_ACCESS_DATA_W, u16Fsw);
        IEM_MC_UPDATE_FSW_WITH_MEM_OP(u16Fsw, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ELSE()
        IEM_MC_IF_FCW_IM()
            IEM_MC_STORE_MEM_I16_CONST_BY_REF(pi16Dst, INT16_MIN /* (integer indefinite) */);
            IEM_MC_MEM_COMMIT_AND_UNMAP(pi16Dst, IEM_ACCESS_DATA_W);
        IEM_MC_ENDIF();
        IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP(UINT8_MAX, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdf !11/3. */
FNIEMOP_DEF_1(iemOp_fistp_m16i,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fistp m16i");
    IEM_MC_BEGIN(3, 2);
    IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);
    IEM_MC_LOCAL(uint16_t,              u16Fsw);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,    pu16Fsw,    u16Fsw, 0);
    IEM_MC_ARG(int16_t *,               pi16Dst,            1);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value,          2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_MEM_MAP(pi16Dst, IEM_ACCESS_DATA_W, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1 /*arg*/);
    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value, 0)
        IEM_MC_CALL_FPU_AIMPL_3(iemAImpl_fist_r80_to_i16, pu16Fsw, pi16Dst, pr80Value);
        IEM_MC_MEM_COMMIT_AND_UNMAP_FOR_FPU_STORE(pi16Dst, IEM_ACCESS_DATA_W, u16Fsw);
        IEM_MC_UPDATE_FSW_WITH_MEM_OP_THEN_POP(u16Fsw, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ELSE()
        IEM_MC_IF_FCW_IM()
            IEM_MC_STORE_MEM_I16_CONST_BY_REF(pi16Dst, INT16_MIN /* (integer indefinite) */);
            IEM_MC_MEM_COMMIT_AND_UNMAP(pi16Dst, IEM_ACCESS_DATA_W);
        IEM_MC_ENDIF();
        IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP_THEN_POP(UINT8_MAX, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdf !11/4. */
FNIEMOP_STUB_1(iemOp_fbld_m80d,   uint8_t, bRm);


/** Opcode 0xdf !11/5. */
FNIEMOP_DEF_1(iemOp_fild_m64i,   uint8_t, bRm)
{
    IEMOP_MNEMONIC("fild m64i");

    IEM_MC_BEGIN(2, 3);
    IEM_MC_LOCAL(RTGCPTR,                   GCPtrEffSrc);
    IEM_MC_LOCAL(IEMFPURESULT,              FpuRes);
    IEM_MC_LOCAL(int64_t,                   i64Val);
    IEM_MC_ARG_LOCAL_REF(PIEMFPURESULT,     pFpuRes,    FpuRes, 0);
    IEM_MC_ARG_LOCAL_REF(int64_t const *,   pi64Val,    i64Val, 1);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();
    IEM_MC_FETCH_MEM_I64(i64Val, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);

    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_IS_EMPTY(7)
        IEM_MC_CALL_FPU_AIMPL_2(iemAImpl_fild_i64_to_r80, pFpuRes, pi64Val);
        IEM_MC_PUSH_FPU_RESULT_MEM_OP(FpuRes, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ELSE()
        IEM_MC_FPU_STACK_PUSH_OVERFLOW_MEM_OP(pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdf !11/6. */
FNIEMOP_STUB_1(iemOp_fbstp_m80d,  uint8_t, bRm);


/** Opcode 0xdf !11/7. */
FNIEMOP_DEF_1(iemOp_fistp_m64i,  uint8_t, bRm)
{
    IEMOP_MNEMONIC("fistp m64i");
    IEM_MC_BEGIN(3, 2);
    IEM_MC_LOCAL(RTGCPTR,               GCPtrEffDst);
    IEM_MC_LOCAL(uint16_t,              u16Fsw);
    IEM_MC_ARG_LOCAL_REF(uint16_t *,    pu16Fsw,    u16Fsw, 0);
    IEM_MC_ARG(int64_t *,               pi64Dst,            1);
    IEM_MC_ARG(PCRTFLOAT80U,            pr80Value,          2);

    IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE();
    IEM_MC_MAYBE_RAISE_FPU_XCPT();

    IEM_MC_MEM_MAP(pi64Dst, IEM_ACCESS_DATA_W, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 1 /*arg*/);
    IEM_MC_PREPARE_FPU_USAGE();
    IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(pr80Value, 0)
        IEM_MC_CALL_FPU_AIMPL_3(iemAImpl_fist_r80_to_i64, pu16Fsw, pi64Dst, pr80Value);
        IEM_MC_MEM_COMMIT_AND_UNMAP_FOR_FPU_STORE(pi64Dst, IEM_ACCESS_DATA_W, u16Fsw);
        IEM_MC_UPDATE_FSW_WITH_MEM_OP_THEN_POP(u16Fsw, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ELSE()
        IEM_MC_IF_FCW_IM()
            IEM_MC_STORE_MEM_I64_CONST_BY_REF(pi64Dst, INT64_MIN /* (integer indefinite) */);
            IEM_MC_MEM_COMMIT_AND_UNMAP(pi64Dst, IEM_ACCESS_DATA_W);
        IEM_MC_ENDIF();
        IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP_THEN_POP(UINT8_MAX, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
    IEM_MC_ENDIF();
    IEM_MC_ADVANCE_RIP();

    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xdf. */
FNIEMOP_DEF(iemOp_EscF7)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
        {
            case 0: return FNIEMOP_CALL_1(iemOp_ffreep_stN, bRm); /* ffree + pop afterwards, since forever according to AMD. */
            case 1: return FNIEMOP_CALL_1(iemOp_fxch_stN,   bRm); /* Reserved, behaves like FXCH ST(i) on intel. */
            case 2: return FNIEMOP_CALL_1(iemOp_fstp_stN,   bRm); /* Reserved, behaves like FSTP ST(i) on intel. */
            case 3: return FNIEMOP_CALL_1(iemOp_fstp_stN,   bRm); /* Reserved, behaves like FSTP ST(i) on intel. */
            case 4: if (bRm == 0xe0)
                        return FNIEMOP_CALL(iemOp_fnstsw_ax);
                    return IEMOP_RAISE_INVALID_OPCODE();
            case 5: return FNIEMOP_CALL_1(iemOp_fucomip_st0_stN, bRm);
            case 6: return FNIEMOP_CALL_1(iemOp_fcomip_st0_stN,  bRm);
            case 7: return IEMOP_RAISE_INVALID_OPCODE();
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
        {
            case 0: return FNIEMOP_CALL_1(iemOp_fild_m16i,   bRm);
            case 1: return FNIEMOP_CALL_1(iemOp_fisttp_m16i, bRm);
            case 2: return FNIEMOP_CALL_1(iemOp_fist_m16i,   bRm);
            case 3: return FNIEMOP_CALL_1(iemOp_fistp_m16i,  bRm);
            case 4: return FNIEMOP_CALL_1(iemOp_fbld_m80d,   bRm);
            case 5: return FNIEMOP_CALL_1(iemOp_fild_m64i,   bRm);
            case 6: return FNIEMOP_CALL_1(iemOp_fbstp_m80d,  bRm);
            case 7: return FNIEMOP_CALL_1(iemOp_fistp_m64i,  bRm);
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


/** Opcode 0xe0. */
FNIEMOP_DEF(iemOp_loopne_Jb)
{
    IEMOP_MNEMONIC("loopne Jb");
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    switch (pVCpu->iem.s.enmEffAddrMode)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(0,0);
            IEM_MC_SUB_GREG_U16(X86_GREG_xCX, 1);
            IEM_MC_IF_CX_IS_NZ_AND_EFL_BIT_NOT_SET(X86_EFL_ZF) {
                IEM_MC_REL_JMP_S8(i8Imm);
            } IEM_MC_ELSE() {
                IEM_MC_ADVANCE_RIP();
            } IEM_MC_ENDIF();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(0,0);
            IEM_MC_SUB_GREG_U32(X86_GREG_xCX, 1);
            IEM_MC_IF_ECX_IS_NZ_AND_EFL_BIT_NOT_SET(X86_EFL_ZF) {
                IEM_MC_REL_JMP_S8(i8Imm);
            } IEM_MC_ELSE() {
                IEM_MC_ADVANCE_RIP();
            } IEM_MC_ENDIF();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(0,0);
            IEM_MC_SUB_GREG_U64(X86_GREG_xCX, 1);
            IEM_MC_IF_RCX_IS_NZ_AND_EFL_BIT_NOT_SET(X86_EFL_ZF) {
                IEM_MC_REL_JMP_S8(i8Imm);
            } IEM_MC_ELSE() {
                IEM_MC_ADVANCE_RIP();
            } IEM_MC_ENDIF();
            IEM_MC_END();
            return VINF_SUCCESS;

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/** Opcode 0xe1. */
FNIEMOP_DEF(iemOp_loope_Jb)
{
    IEMOP_MNEMONIC("loope Jb");
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    switch (pVCpu->iem.s.enmEffAddrMode)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(0,0);
            IEM_MC_SUB_GREG_U16(X86_GREG_xCX, 1);
            IEM_MC_IF_CX_IS_NZ_AND_EFL_BIT_SET(X86_EFL_ZF) {
                IEM_MC_REL_JMP_S8(i8Imm);
            } IEM_MC_ELSE() {
                IEM_MC_ADVANCE_RIP();
            } IEM_MC_ENDIF();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(0,0);
            IEM_MC_SUB_GREG_U32(X86_GREG_xCX, 1);
            IEM_MC_IF_ECX_IS_NZ_AND_EFL_BIT_SET(X86_EFL_ZF) {
                IEM_MC_REL_JMP_S8(i8Imm);
            } IEM_MC_ELSE() {
                IEM_MC_ADVANCE_RIP();
            } IEM_MC_ENDIF();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(0,0);
            IEM_MC_SUB_GREG_U64(X86_GREG_xCX, 1);
            IEM_MC_IF_RCX_IS_NZ_AND_EFL_BIT_SET(X86_EFL_ZF) {
                IEM_MC_REL_JMP_S8(i8Imm);
            } IEM_MC_ELSE() {
                IEM_MC_ADVANCE_RIP();
            } IEM_MC_ENDIF();
            IEM_MC_END();
            return VINF_SUCCESS;

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/** Opcode 0xe2. */
FNIEMOP_DEF(iemOp_loop_Jb)
{
    IEMOP_MNEMONIC("loop Jb");
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    /** @todo Check out the #GP case if EIP < CS.Base or EIP > CS.Limit when
     * using the 32-bit operand size override.  How can that be restarted?  See
     * weird pseudo code in intel manual. */
    switch (pVCpu->iem.s.enmEffAddrMode)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(0,0);
            if (-(int8_t)IEM_GET_INSTR_LEN(pVCpu) != i8Imm)
            {
                IEM_MC_SUB_GREG_U16(X86_GREG_xCX, 1);
                IEM_MC_IF_CX_IS_NZ() {
                    IEM_MC_REL_JMP_S8(i8Imm);
                } IEM_MC_ELSE() {
                    IEM_MC_ADVANCE_RIP();
                } IEM_MC_ENDIF();
            }
            else
            {
                IEM_MC_STORE_GREG_U16_CONST(X86_GREG_xCX, 0);
                IEM_MC_ADVANCE_RIP();
            }
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(0,0);
            if (-(int8_t)IEM_GET_INSTR_LEN(pVCpu) != i8Imm)
            {
                IEM_MC_SUB_GREG_U32(X86_GREG_xCX, 1);
                IEM_MC_IF_ECX_IS_NZ() {
                    IEM_MC_REL_JMP_S8(i8Imm);
                } IEM_MC_ELSE() {
                    IEM_MC_ADVANCE_RIP();
                } IEM_MC_ENDIF();
            }
            else
            {
                IEM_MC_STORE_GREG_U32_CONST(X86_GREG_xCX, 0);
                IEM_MC_ADVANCE_RIP();
            }
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(0,0);
            if (-(int8_t)IEM_GET_INSTR_LEN(pVCpu) != i8Imm)
            {
                IEM_MC_SUB_GREG_U64(X86_GREG_xCX, 1);
                IEM_MC_IF_RCX_IS_NZ() {
                    IEM_MC_REL_JMP_S8(i8Imm);
                } IEM_MC_ELSE() {
                    IEM_MC_ADVANCE_RIP();
                } IEM_MC_ENDIF();
            }
            else
            {
                IEM_MC_STORE_GREG_U64_CONST(X86_GREG_xCX, 0);
                IEM_MC_ADVANCE_RIP();
            }
            IEM_MC_END();
            return VINF_SUCCESS;

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/** Opcode 0xe3. */
FNIEMOP_DEF(iemOp_jecxz_Jb)
{
    IEMOP_MNEMONIC("jecxz Jb");
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    switch (pVCpu->iem.s.enmEffAddrMode)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(0,0);
            IEM_MC_IF_CX_IS_NZ() {
                IEM_MC_ADVANCE_RIP();
            } IEM_MC_ELSE() {
                IEM_MC_REL_JMP_S8(i8Imm);
            } IEM_MC_ENDIF();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(0,0);
            IEM_MC_IF_ECX_IS_NZ() {
                IEM_MC_ADVANCE_RIP();
            } IEM_MC_ELSE() {
                IEM_MC_REL_JMP_S8(i8Imm);
            } IEM_MC_ENDIF();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(0,0);
            IEM_MC_IF_RCX_IS_NZ() {
                IEM_MC_ADVANCE_RIP();
            } IEM_MC_ELSE() {
                IEM_MC_REL_JMP_S8(i8Imm);
            } IEM_MC_ENDIF();
            IEM_MC_END();
            return VINF_SUCCESS;

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/** Opcode 0xe4 */
FNIEMOP_DEF(iemOp_in_AL_Ib)
{
    IEMOP_MNEMONIC("in eAX,Ib");
    uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_in, u8Imm, 1);
}


/** Opcode 0xe5 */
FNIEMOP_DEF(iemOp_in_eAX_Ib)
{
    IEMOP_MNEMONIC("in eAX,Ib");
    uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_in, u8Imm, pVCpu->iem.s.enmEffOpSize == IEMMODE_16BIT ? 2 : 4);
}


/** Opcode 0xe6 */
FNIEMOP_DEF(iemOp_out_Ib_AL)
{
    IEMOP_MNEMONIC("out Ib,AL");
    uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_out, u8Imm, 1);
}


/** Opcode 0xe7 */
FNIEMOP_DEF(iemOp_out_Ib_eAX)
{
    IEMOP_MNEMONIC("out Ib,eAX");
    uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_out, u8Imm, pVCpu->iem.s.enmEffOpSize == IEMMODE_16BIT ? 2 : 4);
}


/** Opcode 0xe8. */
FNIEMOP_DEF(iemOp_call_Jv)
{
    IEMOP_MNEMONIC("call Jv");
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
        {
            uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
            return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_call_rel_16, (int16_t)u16Imm);
        }

        case IEMMODE_32BIT:
        {
            uint32_t u32Imm; IEM_OPCODE_GET_NEXT_U32(&u32Imm);
            return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_call_rel_32, (int32_t)u32Imm);
        }

        case IEMMODE_64BIT:
        {
            uint64_t u64Imm; IEM_OPCODE_GET_NEXT_S32_SX_U64(&u64Imm);
            return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_call_rel_64, u64Imm);
        }

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/** Opcode 0xe9. */
FNIEMOP_DEF(iemOp_jmp_Jv)
{
    IEMOP_MNEMONIC("jmp Jv");
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
        {
            int16_t i16Imm; IEM_OPCODE_GET_NEXT_S16(&i16Imm);
            IEM_MC_BEGIN(0, 0);
            IEM_MC_REL_JMP_S16(i16Imm);
            IEM_MC_END();
            return VINF_SUCCESS;
        }

        case IEMMODE_64BIT:
        case IEMMODE_32BIT:
        {
            int32_t i32Imm; IEM_OPCODE_GET_NEXT_S32(&i32Imm);
            IEM_MC_BEGIN(0, 0);
            IEM_MC_REL_JMP_S32(i32Imm);
            IEM_MC_END();
            return VINF_SUCCESS;
        }

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/** Opcode 0xea. */
FNIEMOP_DEF(iemOp_jmp_Ap)
{
    IEMOP_MNEMONIC("jmp Ap");
    IEMOP_HLP_NO_64BIT();

    /* Decode the far pointer address and pass it on to the far call C implementation. */
    uint32_t offSeg;
    if (pVCpu->iem.s.enmEffOpSize != IEMMODE_16BIT)
        IEM_OPCODE_GET_NEXT_U32(&offSeg);
    else
        IEM_OPCODE_GET_NEXT_U16_ZX_U32(&offSeg);
    uint16_t uSel;  IEM_OPCODE_GET_NEXT_U16(&uSel);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_3(iemCImpl_FarJmp, uSel, offSeg, pVCpu->iem.s.enmEffOpSize);
}


/** Opcode 0xeb. */
FNIEMOP_DEF(iemOp_jmp_Jb)
{
    IEMOP_MNEMONIC("jmp Jb");
    int8_t i8Imm; IEM_OPCODE_GET_NEXT_S8(&i8Imm);
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    IEM_MC_BEGIN(0, 0);
    IEM_MC_REL_JMP_S8(i8Imm);
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xec */
FNIEMOP_DEF(iemOp_in_AL_DX)
{
    IEMOP_MNEMONIC("in  AL,DX");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_in_eAX_DX, 1);
}


/** Opcode 0xed */
FNIEMOP_DEF(iemOp_eAX_DX)
{
    IEMOP_MNEMONIC("in  eAX,DX");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_in_eAX_DX, pVCpu->iem.s.enmEffOpSize == IEMMODE_16BIT ? 2 : 4);
}


/** Opcode 0xee */
FNIEMOP_DEF(iemOp_out_DX_AL)
{
    IEMOP_MNEMONIC("out DX,AL");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_out_DX_eAX, 1);
}


/** Opcode 0xef */
FNIEMOP_DEF(iemOp_out_DX_eAX)
{
    IEMOP_MNEMONIC("out DX,eAX");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_1(iemCImpl_out_DX_eAX, pVCpu->iem.s.enmEffOpSize == IEMMODE_16BIT ? 2 : 4);
}


/** Opcode 0xf0. */
FNIEMOP_DEF(iemOp_lock)
{
    IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("lock");
    pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_LOCK;

    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    return FNIEMOP_CALL(g_apfnOneByteMap[b]);
}


/** Opcode 0xf1. */
FNIEMOP_DEF(iemOp_int_1)
{
    IEMOP_MNEMONIC("int1"); /* icebp */
    IEMOP_HLP_MIN_386(); /** @todo does not generate #UD on 286, or so they say... */
    /** @todo testcase! */
    return IEM_MC_DEFER_TO_CIMPL_2(iemCImpl_int, X86_XCPT_DB, false /*fIsBpInstr*/);
}


/** Opcode 0xf2. */
FNIEMOP_DEF(iemOp_repne)
{
    /* This overrides any previous REPE prefix. */
    pVCpu->iem.s.fPrefixes &= ~IEM_OP_PRF_REPZ;
    IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("repne");
    pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_REPNZ;

    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    return FNIEMOP_CALL(g_apfnOneByteMap[b]);
}


/** Opcode 0xf3. */
FNIEMOP_DEF(iemOp_repe)
{
    /* This overrides any previous REPNE prefix. */
    pVCpu->iem.s.fPrefixes &= ~IEM_OP_PRF_REPNZ;
    IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE("repe");
    pVCpu->iem.s.fPrefixes |= IEM_OP_PRF_REPZ;

    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    return FNIEMOP_CALL(g_apfnOneByteMap[b]);
}


/** Opcode 0xf4. */
FNIEMOP_DEF(iemOp_hlt)
{
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_hlt);
}


/** Opcode 0xf5. */
FNIEMOP_DEF(iemOp_cmc)
{
    IEMOP_MNEMONIC("cmc");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_BEGIN(0, 0);
    IEM_MC_FLIP_EFL_BIT(X86_EFL_CF);
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/**
 * Common implementation of 'inc/dec/not/neg Eb'.
 *
 * @param   bRm             The RM byte.
 * @param   pImpl           The instruction implementation.
 */
FNIEMOP_DEF_2(iemOpCommonUnaryEb, uint8_t, bRm, PCIEMOPUNARYSIZES, pImpl)
{
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register access */
        IEM_MC_BEGIN(2, 0);
        IEM_MC_ARG(uint8_t *,   pu8Dst, 0);
        IEM_MC_ARG(uint32_t *,  pEFlags, 1);
        IEM_MC_REF_GREG_U8(pu8Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        IEM_MC_REF_EFLAGS(pEFlags);
        IEM_MC_CALL_VOID_AIMPL_2(pImpl->pfnNormalU8, pu8Dst, pEFlags);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory access. */
        IEM_MC_BEGIN(2, 2);
        IEM_MC_ARG(uint8_t *,       pu8Dst,          0);
        IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags, 1);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEM_MC_MEM_MAP(pu8Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
        IEM_MC_FETCH_EFLAGS(EFlags);
        if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
            IEM_MC_CALL_VOID_AIMPL_2(pImpl->pfnNormalU8, pu8Dst, pEFlags);
        else
            IEM_MC_CALL_VOID_AIMPL_2(pImpl->pfnLockedU8, pu8Dst, pEFlags);

        IEM_MC_MEM_COMMIT_AND_UNMAP(pu8Dst, IEM_ACCESS_DATA_RW);
        IEM_MC_COMMIT_EFLAGS(EFlags);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/**
 * Common implementation of 'inc/dec/not/neg Ev'.
 *
 * @param   bRm             The RM byte.
 * @param   pImpl           The instruction implementation.
 */
FNIEMOP_DEF_2(iemOpCommonUnaryEv, uint8_t, bRm, PCIEMOPUNARYSIZES, pImpl)
{
    /* Registers are handled by a common worker. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
        return FNIEMOP_CALL_2(iemOpCommonUnaryGReg, pImpl, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);

    /* Memory we do here. */
    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(2, 2);
            IEM_MC_ARG(uint16_t *,      pu16Dst,         0);
            IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags, 1);
            IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
            IEM_MC_MEM_MAP(pu16Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
            IEM_MC_FETCH_EFLAGS(EFlags);
            if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                IEM_MC_CALL_VOID_AIMPL_2(pImpl->pfnNormalU16, pu16Dst, pEFlags);
            else
                IEM_MC_CALL_VOID_AIMPL_2(pImpl->pfnLockedU16, pu16Dst, pEFlags);

            IEM_MC_MEM_COMMIT_AND_UNMAP(pu16Dst, IEM_ACCESS_DATA_RW);
            IEM_MC_COMMIT_EFLAGS(EFlags);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(2, 2);
            IEM_MC_ARG(uint32_t *,      pu32Dst,         0);
            IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags, 1);
            IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
            IEM_MC_MEM_MAP(pu32Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
            IEM_MC_FETCH_EFLAGS(EFlags);
            if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                IEM_MC_CALL_VOID_AIMPL_2(pImpl->pfnNormalU32, pu32Dst, pEFlags);
            else
                IEM_MC_CALL_VOID_AIMPL_2(pImpl->pfnLockedU32, pu32Dst, pEFlags);

            IEM_MC_MEM_COMMIT_AND_UNMAP(pu32Dst, IEM_ACCESS_DATA_RW);
            IEM_MC_COMMIT_EFLAGS(EFlags);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(2, 2);
            IEM_MC_ARG(uint64_t *,      pu64Dst,         0);
            IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags, 1);
            IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
            IEM_MC_MEM_MAP(pu64Dst, IEM_ACCESS_DATA_RW, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
            IEM_MC_FETCH_EFLAGS(EFlags);
            if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_LOCK))
                IEM_MC_CALL_VOID_AIMPL_2(pImpl->pfnNormalU64, pu64Dst, pEFlags);
            else
                IEM_MC_CALL_VOID_AIMPL_2(pImpl->pfnLockedU64, pu64Dst, pEFlags);

            IEM_MC_MEM_COMMIT_AND_UNMAP(pu64Dst, IEM_ACCESS_DATA_RW);
            IEM_MC_COMMIT_EFLAGS(EFlags);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/** Opcode 0xf6 /0. */
FNIEMOP_DEF_1(iemOp_grp3_test_Eb, uint8_t, bRm)
{
    IEMOP_MNEMONIC("test Eb,Ib");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register access */
        uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();

        IEM_MC_BEGIN(3, 0);
        IEM_MC_ARG(uint8_t *,       pu8Dst,             0);
        IEM_MC_ARG_CONST(uint8_t,   u8Src,/*=*/u8Imm,   1);
        IEM_MC_ARG(uint32_t *,      pEFlags,            2);
        IEM_MC_REF_GREG_U8(pu8Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        IEM_MC_REF_EFLAGS(pEFlags);
        IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_test_u8, pu8Dst, u8Src, pEFlags);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    else
    {
        /* memory access. */
        IEM_MC_BEGIN(3, 2);
        IEM_MC_ARG(uint8_t *,       pu8Dst,             0);
        IEM_MC_ARG(uint8_t,         u8Src,              1);
        IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags,    2);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 1);
        uint8_t u8Imm; IEM_OPCODE_GET_NEXT_U8(&u8Imm);
        IEM_MC_ASSIGN(u8Src, u8Imm);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_MEM_MAP(pu8Dst, IEM_ACCESS_DATA_R, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
        IEM_MC_FETCH_EFLAGS(EFlags);
        IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_test_u8, pu8Dst, u8Src, pEFlags);

        IEM_MC_MEM_COMMIT_AND_UNMAP(pu8Dst, IEM_ACCESS_DATA_R);
        IEM_MC_COMMIT_EFLAGS(EFlags);
        IEM_MC_ADVANCE_RIP();
        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0xf7 /0. */
FNIEMOP_DEF_1(iemOp_grp3_test_Ev, uint8_t, bRm)
{
    IEMOP_MNEMONIC("test Ev,Iv");
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_AF);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register access */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
            {
                uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint16_t *,      pu16Dst,                0);
                IEM_MC_ARG_CONST(uint16_t,  u16Src,/*=*/u16Imm,     1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                2);
                IEM_MC_REF_GREG_U16(pu16Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_test_u16, pu16Dst, u16Src, pEFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;
            }

            case IEMMODE_32BIT:
            {
                uint32_t u32Imm; IEM_OPCODE_GET_NEXT_U32(&u32Imm);
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint32_t *,      pu32Dst,                0);
                IEM_MC_ARG_CONST(uint32_t,  u32Src,/*=*/u32Imm,     1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                2);
                IEM_MC_REF_GREG_U32(pu32Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_test_u32, pu32Dst, u32Src, pEFlags);
                /* No clearing the high dword here - test doesn't write back the result. */
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;
            }

            case IEMMODE_64BIT:
            {
                uint64_t u64Imm; IEM_OPCODE_GET_NEXT_S32_SX_U64(&u64Imm);
                IEM_MC_BEGIN(3, 0);
                IEM_MC_ARG(uint64_t *,      pu64Dst,                0);
                IEM_MC_ARG_CONST(uint64_t,  u64Src,/*=*/u64Imm,     1);
                IEM_MC_ARG(uint32_t *,      pEFlags,                2);
                IEM_MC_REF_GREG_U64(pu64Dst, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_test_u64, pu64Dst, u64Src, pEFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;
            }

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        /* memory access. */
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
            {
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint16_t *,      pu16Dst,            0);
                IEM_MC_ARG(uint16_t,        u16Src,             1);
                IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags,    2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 2);
                uint16_t u16Imm; IEM_OPCODE_GET_NEXT_U16(&u16Imm);
                IEM_MC_ASSIGN(u16Src, u16Imm);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MEM_MAP(pu16Dst, IEM_ACCESS_DATA_R, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_test_u16, pu16Dst, u16Src, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu16Dst, IEM_ACCESS_DATA_R);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;
            }

            case IEMMODE_32BIT:
            {
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint32_t *,      pu32Dst,            0);
                IEM_MC_ARG(uint32_t,        u32Src,             1);
                IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags,    2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 4);
                uint32_t u32Imm; IEM_OPCODE_GET_NEXT_U32(&u32Imm);
                IEM_MC_ASSIGN(u32Src, u32Imm);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MEM_MAP(pu32Dst, IEM_ACCESS_DATA_R, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_test_u32, pu32Dst, u32Src, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu32Dst, IEM_ACCESS_DATA_R);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;
            }

            case IEMMODE_64BIT:
            {
                IEM_MC_BEGIN(3, 2);
                IEM_MC_ARG(uint64_t *,      pu64Dst,            0);
                IEM_MC_ARG(uint64_t,        u64Src,             1);
                IEM_MC_ARG_LOCAL_EFLAGS(    pEFlags, EFlags,    2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 4);
                uint64_t u64Imm; IEM_OPCODE_GET_NEXT_S32_SX_U64(&u64Imm);
                IEM_MC_ASSIGN(u64Src, u64Imm);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_MEM_MAP(pu64Dst, IEM_ACCESS_DATA_R, pVCpu->iem.s.iEffSeg, GCPtrEffDst, 0 /*arg*/);
                IEM_MC_FETCH_EFLAGS(EFlags);
                IEM_MC_CALL_VOID_AIMPL_3(iemAImpl_test_u64, pu64Dst, u64Src, pEFlags);

                IEM_MC_MEM_COMMIT_AND_UNMAP(pu64Dst, IEM_ACCESS_DATA_R);
                IEM_MC_COMMIT_EFLAGS(EFlags);
                IEM_MC_ADVANCE_RIP();
                IEM_MC_END();
                return VINF_SUCCESS;
            }

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


/** Opcode 0xf6 /4, /5, /6 and /7. */
FNIEMOP_DEF_2(iemOpCommonGrp3MulDivEb, uint8_t, bRm, PFNIEMAIMPLMULDIVU8, pfnU8)
{
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register access */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_BEGIN(3, 1);
        IEM_MC_ARG(uint16_t *,      pu16AX,     0);
        IEM_MC_ARG(uint8_t,         u8Value,    1);
        IEM_MC_ARG(uint32_t *,      pEFlags,    2);
        IEM_MC_LOCAL(int32_t,       rc);

        IEM_MC_FETCH_GREG_U8(u8Value, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
        IEM_MC_REF_GREG_U16(pu16AX, X86_GREG_xAX);
        IEM_MC_REF_EFLAGS(pEFlags);
        IEM_MC_CALL_AIMPL_3(rc, pfnU8, pu16AX, u8Value, pEFlags);
        IEM_MC_IF_LOCAL_IS_Z(rc) {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ELSE() {
            IEM_MC_RAISE_DIVIDE_ERROR();
        } IEM_MC_ENDIF();

        IEM_MC_END();
    }
    else
    {
        /* memory access. */
        IEM_MC_BEGIN(3, 2);
        IEM_MC_ARG(uint16_t *,      pu16AX,     0);
        IEM_MC_ARG(uint8_t,         u8Value,    1);
        IEM_MC_ARG(uint32_t *,      pEFlags,    2);
        IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
        IEM_MC_LOCAL(int32_t,       rc);

        IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        IEM_MC_FETCH_MEM_U8(u8Value, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
        IEM_MC_REF_GREG_U16(pu16AX, X86_GREG_xAX);
        IEM_MC_REF_EFLAGS(pEFlags);
        IEM_MC_CALL_AIMPL_3(rc, pfnU8, pu16AX, u8Value, pEFlags);
        IEM_MC_IF_LOCAL_IS_Z(rc) {
            IEM_MC_ADVANCE_RIP();
        } IEM_MC_ELSE() {
            IEM_MC_RAISE_DIVIDE_ERROR();
        } IEM_MC_ENDIF();

        IEM_MC_END();
    }
    return VINF_SUCCESS;
}


/** Opcode 0xf7 /4, /5, /6 and /7. */
FNIEMOP_DEF_2(iemOpCommonGrp3MulDivEv, uint8_t, bRm, PCIEMOPMULDIVSIZES, pImpl)
{
    IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF);

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* register access */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
            {
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(4, 1);
                IEM_MC_ARG(uint16_t *,      pu16AX,     0);
                IEM_MC_ARG(uint16_t *,      pu16DX,     1);
                IEM_MC_ARG(uint16_t,        u16Value,   2);
                IEM_MC_ARG(uint32_t *,      pEFlags,    3);
                IEM_MC_LOCAL(int32_t,       rc);

                IEM_MC_FETCH_GREG_U16(u16Value, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_GREG_U16(pu16AX, X86_GREG_xAX);
                IEM_MC_REF_GREG_U16(pu16DX, X86_GREG_xDX);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_AIMPL_4(rc, pImpl->pfnU16, pu16AX, pu16DX, u16Value, pEFlags);
                IEM_MC_IF_LOCAL_IS_Z(rc) {
                    IEM_MC_ADVANCE_RIP();
                } IEM_MC_ELSE() {
                    IEM_MC_RAISE_DIVIDE_ERROR();
                } IEM_MC_ENDIF();

                IEM_MC_END();
                return VINF_SUCCESS;
            }

            case IEMMODE_32BIT:
            {
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(4, 1);
                IEM_MC_ARG(uint32_t *,      pu32AX,     0);
                IEM_MC_ARG(uint32_t *,      pu32DX,     1);
                IEM_MC_ARG(uint32_t,        u32Value,   2);
                IEM_MC_ARG(uint32_t *,      pEFlags,    3);
                IEM_MC_LOCAL(int32_t,       rc);

                IEM_MC_FETCH_GREG_U32(u32Value, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_GREG_U32(pu32AX, X86_GREG_xAX);
                IEM_MC_REF_GREG_U32(pu32DX, X86_GREG_xDX);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_AIMPL_4(rc, pImpl->pfnU32, pu32AX, pu32DX, u32Value, pEFlags);
                IEM_MC_IF_LOCAL_IS_Z(rc) {
                    IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32AX);
                    IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32DX);
                    IEM_MC_ADVANCE_RIP();
                } IEM_MC_ELSE() {
                    IEM_MC_RAISE_DIVIDE_ERROR();
                } IEM_MC_ENDIF();

                IEM_MC_END();
                return VINF_SUCCESS;
            }

            case IEMMODE_64BIT:
            {
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_BEGIN(4, 1);
                IEM_MC_ARG(uint64_t *,      pu64AX,     0);
                IEM_MC_ARG(uint64_t *,      pu64DX,     1);
                IEM_MC_ARG(uint64_t,        u64Value,   2);
                IEM_MC_ARG(uint32_t *,      pEFlags,    3);
                IEM_MC_LOCAL(int32_t,       rc);

                IEM_MC_FETCH_GREG_U64(u64Value, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_REF_GREG_U64(pu64AX, X86_GREG_xAX);
                IEM_MC_REF_GREG_U64(pu64DX, X86_GREG_xDX);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_AIMPL_4(rc, pImpl->pfnU64, pu64AX, pu64DX, u64Value, pEFlags);
                IEM_MC_IF_LOCAL_IS_Z(rc) {
                    IEM_MC_ADVANCE_RIP();
                } IEM_MC_ELSE() {
                    IEM_MC_RAISE_DIVIDE_ERROR();
                } IEM_MC_ENDIF();

                IEM_MC_END();
                return VINF_SUCCESS;
            }

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        /* memory access. */
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
            {
                IEM_MC_BEGIN(4, 2);
                IEM_MC_ARG(uint16_t *,      pu16AX,     0);
                IEM_MC_ARG(uint16_t *,      pu16DX,     1);
                IEM_MC_ARG(uint16_t,        u16Value,   2);
                IEM_MC_ARG(uint32_t *,      pEFlags,    3);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_LOCAL(int32_t,       rc);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U16(u16Value, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
                IEM_MC_REF_GREG_U16(pu16AX, X86_GREG_xAX);
                IEM_MC_REF_GREG_U16(pu16DX, X86_GREG_xDX);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_AIMPL_4(rc, pImpl->pfnU16, pu16AX, pu16DX, u16Value, pEFlags);
                IEM_MC_IF_LOCAL_IS_Z(rc) {
                    IEM_MC_ADVANCE_RIP();
                } IEM_MC_ELSE() {
                    IEM_MC_RAISE_DIVIDE_ERROR();
                } IEM_MC_ENDIF();

                IEM_MC_END();
                return VINF_SUCCESS;
            }

            case IEMMODE_32BIT:
            {
                IEM_MC_BEGIN(4, 2);
                IEM_MC_ARG(uint32_t *,      pu32AX,     0);
                IEM_MC_ARG(uint32_t *,      pu32DX,     1);
                IEM_MC_ARG(uint32_t,        u32Value,   2);
                IEM_MC_ARG(uint32_t *,      pEFlags,    3);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_LOCAL(int32_t,       rc);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U32(u32Value, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
                IEM_MC_REF_GREG_U32(pu32AX, X86_GREG_xAX);
                IEM_MC_REF_GREG_U32(pu32DX, X86_GREG_xDX);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_AIMPL_4(rc, pImpl->pfnU32, pu32AX, pu32DX, u32Value, pEFlags);
                IEM_MC_IF_LOCAL_IS_Z(rc) {
                    IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32AX);
                    IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(pu32DX);
                    IEM_MC_ADVANCE_RIP();
                } IEM_MC_ELSE() {
                    IEM_MC_RAISE_DIVIDE_ERROR();
                } IEM_MC_ENDIF();

                IEM_MC_END();
                return VINF_SUCCESS;
            }

            case IEMMODE_64BIT:
            {
                IEM_MC_BEGIN(4, 2);
                IEM_MC_ARG(uint64_t *,      pu64AX,     0);
                IEM_MC_ARG(uint64_t *,      pu64DX,     1);
                IEM_MC_ARG(uint64_t,        u64Value,   2);
                IEM_MC_ARG(uint32_t *,      pEFlags,    3);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffDst);
                IEM_MC_LOCAL(int32_t,       rc);

                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffDst, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U64(u64Value, pVCpu->iem.s.iEffSeg, GCPtrEffDst);
                IEM_MC_REF_GREG_U64(pu64AX, X86_GREG_xAX);
                IEM_MC_REF_GREG_U64(pu64DX, X86_GREG_xDX);
                IEM_MC_REF_EFLAGS(pEFlags);
                IEM_MC_CALL_AIMPL_4(rc, pImpl->pfnU64, pu64AX, pu64DX, u64Value, pEFlags);
                IEM_MC_IF_LOCAL_IS_Z(rc) {
                    IEM_MC_ADVANCE_RIP();
                } IEM_MC_ELSE() {
                    IEM_MC_RAISE_DIVIDE_ERROR();
                } IEM_MC_ENDIF();

                IEM_MC_END();
                return VINF_SUCCESS;
            }

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}

/** Opcode 0xf6. */
FNIEMOP_DEF(iemOp_Grp3_Eb)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
    {
        case 0:
            return FNIEMOP_CALL_1(iemOp_grp3_test_Eb, bRm);
        case 1:
/** @todo testcase: Present on <=386, most 486 (not early), Pentiums, and current CPUs too. CPUUNDOC.EXE */
            return IEMOP_RAISE_INVALID_OPCODE();
        case 2:
            IEMOP_MNEMONIC("not Eb");
            return FNIEMOP_CALL_2(iemOpCommonUnaryEb, bRm, &g_iemAImpl_not);
        case 3:
            IEMOP_MNEMONIC("neg Eb");
            return FNIEMOP_CALL_2(iemOpCommonUnaryEb, bRm, &g_iemAImpl_neg);
        case 4:
            IEMOP_MNEMONIC("mul Eb");
            IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF);
            return FNIEMOP_CALL_2(iemOpCommonGrp3MulDivEb, bRm, iemAImpl_mul_u8);
        case 5:
            IEMOP_MNEMONIC("imul Eb");
            IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF);
            return FNIEMOP_CALL_2(iemOpCommonGrp3MulDivEb, bRm, iemAImpl_imul_u8);
        case 6:
            IEMOP_MNEMONIC("div Eb");
            IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_OF | X86_EFL_CF);
            return FNIEMOP_CALL_2(iemOpCommonGrp3MulDivEb, bRm, iemAImpl_div_u8);
        case 7:
            IEMOP_MNEMONIC("idiv Eb");
            IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_OF | X86_EFL_CF);
            return FNIEMOP_CALL_2(iemOpCommonGrp3MulDivEb, bRm, iemAImpl_idiv_u8);
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/** Opcode 0xf7. */
FNIEMOP_DEF(iemOp_Grp3_Ev)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
    {
        case 0:
            return FNIEMOP_CALL_1(iemOp_grp3_test_Ev, bRm);
        case 1:
/** @todo testcase: Present on <=386, most 486 (not early), Pentiums, and current CPUs too. CPUUNDOC.EXE */
            return IEMOP_RAISE_INVALID_OPCODE();
        case 2:
            IEMOP_MNEMONIC("not Ev");
            return FNIEMOP_CALL_2(iemOpCommonUnaryEv, bRm, &g_iemAImpl_not);
        case 3:
            IEMOP_MNEMONIC("neg Ev");
            return FNIEMOP_CALL_2(iemOpCommonUnaryEv, bRm, &g_iemAImpl_neg);
        case 4:
            IEMOP_MNEMONIC("mul Ev");
            IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF);
            return FNIEMOP_CALL_2(iemOpCommonGrp3MulDivEv, bRm, &g_iemAImpl_mul);
        case 5:
            IEMOP_MNEMONIC("imul Ev");
            IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF);
            return FNIEMOP_CALL_2(iemOpCommonGrp3MulDivEv, bRm, &g_iemAImpl_imul);
        case 6:
            IEMOP_MNEMONIC("div Ev");
            IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_OF | X86_EFL_CF);
            return FNIEMOP_CALL_2(iemOpCommonGrp3MulDivEv, bRm, &g_iemAImpl_div);
        case 7:
            IEMOP_MNEMONIC("idiv Ev");
            IEMOP_VERIFICATION_UNDEFINED_EFLAGS(X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_OF | X86_EFL_CF);
            return FNIEMOP_CALL_2(iemOpCommonGrp3MulDivEv, bRm, &g_iemAImpl_idiv);
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/** Opcode 0xf8. */
FNIEMOP_DEF(iemOp_clc)
{
    IEMOP_MNEMONIC("clc");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_BEGIN(0, 0);
    IEM_MC_CLEAR_EFL_BIT(X86_EFL_CF);
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xf9. */
FNIEMOP_DEF(iemOp_stc)
{
    IEMOP_MNEMONIC("stc");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_BEGIN(0, 0);
    IEM_MC_SET_EFL_BIT(X86_EFL_CF);
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xfa. */
FNIEMOP_DEF(iemOp_cli)
{
    IEMOP_MNEMONIC("cli");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_cli);
}


FNIEMOP_DEF(iemOp_sti)
{
    IEMOP_MNEMONIC("sti");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    return IEM_MC_DEFER_TO_CIMPL_0(iemCImpl_sti);
}


/** Opcode 0xfc. */
FNIEMOP_DEF(iemOp_cld)
{
    IEMOP_MNEMONIC("cld");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_BEGIN(0, 0);
    IEM_MC_CLEAR_EFL_BIT(X86_EFL_DF);
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xfd. */
FNIEMOP_DEF(iemOp_std)
{
    IEMOP_MNEMONIC("std");
    IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
    IEM_MC_BEGIN(0, 0);
    IEM_MC_SET_EFL_BIT(X86_EFL_DF);
    IEM_MC_ADVANCE_RIP();
    IEM_MC_END();
    return VINF_SUCCESS;
}


/** Opcode 0xfe. */
FNIEMOP_DEF(iemOp_Grp4)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
    {
        case 0:
            IEMOP_MNEMONIC("inc Ev");
            return FNIEMOP_CALL_2(iemOpCommonUnaryEb, bRm, &g_iemAImpl_inc);
        case 1:
            IEMOP_MNEMONIC("dec Ev");
            return FNIEMOP_CALL_2(iemOpCommonUnaryEb, bRm, &g_iemAImpl_dec);
        default:
            IEMOP_MNEMONIC("grp4-ud");
            return IEMOP_RAISE_INVALID_OPCODE();
    }
}


/**
 * Opcode 0xff /2.
 * @param   bRm             The RM byte.
 */
FNIEMOP_DEF_1(iemOp_Grp5_calln_Ev, uint8_t, bRm)
{
    IEMOP_MNEMONIC("calln Ev");
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* The new RIP is taken from a register. */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(1, 0);
                IEM_MC_ARG(uint16_t, u16Target, 0);
                IEM_MC_FETCH_GREG_U16(u16Target, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_CALL_CIMPL_1(iemCImpl_call_16, u16Target);
                IEM_MC_END()
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(1, 0);
                IEM_MC_ARG(uint32_t, u32Target, 0);
                IEM_MC_FETCH_GREG_U32(u32Target, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_CALL_CIMPL_1(iemCImpl_call_32, u32Target);
                IEM_MC_END()
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(1, 0);
                IEM_MC_ARG(uint64_t, u64Target, 0);
                IEM_MC_FETCH_GREG_U64(u64Target, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_CALL_CIMPL_1(iemCImpl_call_64, u64Target);
                IEM_MC_END()
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        /* The new RIP is taken from a register. */
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(1, 1);
                IEM_MC_ARG(uint16_t,  u16Target, 0);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U16(u16Target, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
                IEM_MC_CALL_CIMPL_1(iemCImpl_call_16, u16Target);
                IEM_MC_END()
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(1, 1);
                IEM_MC_ARG(uint32_t,  u32Target, 0);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U32(u32Target, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
                IEM_MC_CALL_CIMPL_1(iemCImpl_call_32, u32Target);
                IEM_MC_END()
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(1, 1);
                IEM_MC_ARG(uint64_t,  u64Target, 0);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U64(u64Target, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
                IEM_MC_CALL_CIMPL_1(iemCImpl_call_64, u64Target);
                IEM_MC_END()
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}

typedef IEM_CIMPL_DECL_TYPE_3(FNIEMCIMPLFARBRANCH, uint16_t, uSel, uint64_t, offSeg, IEMMODE, enmOpSize);

FNIEMOP_DEF_2(iemOpHlp_Grp5_far_Ep, uint8_t, bRm, FNIEMCIMPLFARBRANCH *, pfnCImpl)
{
    /* Registers? How?? */
    if (RT_LIKELY((bRm & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT)))
    { /* likely */ }
    else
        return IEMOP_RAISE_INVALID_OPCODE(); /* callf eax is not legal */

    /* Far pointer loaded from memory. */
    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(3, 1);
            IEM_MC_ARG(uint16_t,        u16Sel,                         0);
            IEM_MC_ARG(uint16_t,        offSeg,                         1);
            IEM_MC_ARG_CONST(IEMMODE,   enmEffOpSize, IEMMODE_16BIT,    2);
            IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
            IEM_MC_FETCH_MEM_U16(offSeg, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
            IEM_MC_FETCH_MEM_U16_DISP(u16Sel, pVCpu->iem.s.iEffSeg, GCPtrEffSrc, 2);
            IEM_MC_CALL_CIMPL_3(pfnCImpl, u16Sel, offSeg, enmEffOpSize);
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            /** @todo testcase: AMD does not seem to believe in the case (see bs-cpu-xcpt-1)
             *        and will apparently ignore REX.W, at least for the jmp far qword [rsp]
             *        and call far qword [rsp] encodings. */
            if (!IEM_IS_GUEST_CPU_AMD(pVCpu))
            {
                IEM_MC_BEGIN(3, 1);
                IEM_MC_ARG(uint16_t,        u16Sel,                         0);
                IEM_MC_ARG(uint64_t,        offSeg,                         1);
                IEM_MC_ARG_CONST(IEMMODE,   enmEffOpSize, IEMMODE_16BIT,    2);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U64(offSeg, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
                IEM_MC_FETCH_MEM_U16_DISP(u16Sel, pVCpu->iem.s.iEffSeg, GCPtrEffSrc, 8);
                IEM_MC_CALL_CIMPL_3(pfnCImpl, u16Sel, offSeg, enmEffOpSize);
                IEM_MC_END();
                return VINF_SUCCESS;
            }
            /* AMD falls thru. */

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(3, 1);
            IEM_MC_ARG(uint16_t,        u16Sel,                         0);
            IEM_MC_ARG(uint32_t,        offSeg,                         1);
            IEM_MC_ARG_CONST(IEMMODE,   enmEffOpSize, IEMMODE_32BIT,    2);
            IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
            IEM_MC_FETCH_MEM_U32(offSeg, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
            IEM_MC_FETCH_MEM_U16_DISP(u16Sel, pVCpu->iem.s.iEffSeg, GCPtrEffSrc, 4);
            IEM_MC_CALL_CIMPL_3(pfnCImpl, u16Sel, offSeg, enmEffOpSize);
            IEM_MC_END();
            return VINF_SUCCESS;

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/**
 * Opcode 0xff /3.
 * @param   bRm             The RM byte.
 */
FNIEMOP_DEF_1(iemOp_Grp5_callf_Ep, uint8_t, bRm)
{
    IEMOP_MNEMONIC("callf Ep");
    return FNIEMOP_CALL_2(iemOpHlp_Grp5_far_Ep, bRm, iemCImpl_callf);
}


/**
 * Opcode 0xff /4.
 * @param   bRm             The RM byte.
 */
FNIEMOP_DEF_1(iemOp_Grp5_jmpn_Ev, uint8_t, bRm)
{
    IEMOP_MNEMONIC("jmpn Ev");
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();

    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /* The new RIP is taken from a register. */
        IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint16_t, u16Target);
                IEM_MC_FETCH_GREG_U16(u16Target, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_SET_RIP_U16(u16Target);
                IEM_MC_END()
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint32_t, u32Target);
                IEM_MC_FETCH_GREG_U32(u32Target, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_SET_RIP_U32(u32Target);
                IEM_MC_END()
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 1);
                IEM_MC_LOCAL(uint64_t, u64Target);
                IEM_MC_FETCH_GREG_U64(u64Target, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);
                IEM_MC_SET_RIP_U64(u64Target);
                IEM_MC_END()
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
    else
    {
        /* The new RIP is taken from a memory location. */
        switch (pVCpu->iem.s.enmEffOpSize)
        {
            case IEMMODE_16BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint16_t, u16Target);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U16(u16Target, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
                IEM_MC_SET_RIP_U16(u16Target);
                IEM_MC_END()
                return VINF_SUCCESS;

            case IEMMODE_32BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint32_t, u32Target);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U32(u32Target, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
                IEM_MC_SET_RIP_U32(u32Target);
                IEM_MC_END()
                return VINF_SUCCESS;

            case IEMMODE_64BIT:
                IEM_MC_BEGIN(0, 2);
                IEM_MC_LOCAL(uint64_t, u64Target);
                IEM_MC_LOCAL(RTGCPTR, GCPtrEffSrc);
                IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
                IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
                IEM_MC_FETCH_MEM_U64(u64Target, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
                IEM_MC_SET_RIP_U64(u64Target);
                IEM_MC_END()
                return VINF_SUCCESS;

            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }
}


/**
 * Opcode 0xff /5.
 * @param   bRm             The RM byte.
 */
FNIEMOP_DEF_1(iemOp_Grp5_jmpf_Ep, uint8_t, bRm)
{
    IEMOP_MNEMONIC("jmpf Ep");
    return FNIEMOP_CALL_2(iemOpHlp_Grp5_far_Ep, bRm, iemCImpl_FarJmp);
}


/**
 * Opcode 0xff /6.
 * @param   bRm             The RM byte.
 */
FNIEMOP_DEF_1(iemOp_Grp5_push_Ev, uint8_t, bRm)
{
    IEMOP_MNEMONIC("push Ev");

    /* Registers are handled by a common worker. */
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
        return FNIEMOP_CALL_1(iemOpCommonPushGReg, (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB);

    /* Memory we do here. */
    IEMOP_HLP_DEFAULT_64BIT_OP_SIZE();
    switch (pVCpu->iem.s.enmEffOpSize)
    {
        case IEMMODE_16BIT:
            IEM_MC_BEGIN(0, 2);
            IEM_MC_LOCAL(uint16_t,  u16Src);
            IEM_MC_LOCAL(RTGCPTR,   GCPtrEffSrc);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
            IEM_MC_FETCH_MEM_U16(u16Src, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
            IEM_MC_PUSH_U16(u16Src);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_32BIT:
            IEM_MC_BEGIN(0, 2);
            IEM_MC_LOCAL(uint32_t,  u32Src);
            IEM_MC_LOCAL(RTGCPTR,   GCPtrEffSrc);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
            IEM_MC_FETCH_MEM_U32(u32Src, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
            IEM_MC_PUSH_U32(u32Src);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        case IEMMODE_64BIT:
            IEM_MC_BEGIN(0, 2);
            IEM_MC_LOCAL(uint64_t,  u64Src);
            IEM_MC_LOCAL(RTGCPTR,   GCPtrEffSrc);
            IEM_MC_CALC_RM_EFF_ADDR(GCPtrEffSrc, bRm, 0);
            IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX();
            IEM_MC_FETCH_MEM_U64(u64Src, pVCpu->iem.s.iEffSeg, GCPtrEffSrc);
            IEM_MC_PUSH_U64(u64Src);
            IEM_MC_ADVANCE_RIP();
            IEM_MC_END();
            return VINF_SUCCESS;

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
}


/** Opcode 0xff. */
FNIEMOP_DEF(iemOp_Grp5)
{
    uint8_t bRm; IEM_OPCODE_GET_NEXT_U8(&bRm);
    switch ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK)
    {
        case 0:
            IEMOP_MNEMONIC("inc Ev");
            return FNIEMOP_CALL_2(iemOpCommonUnaryEv, bRm, &g_iemAImpl_inc);
        case 1:
            IEMOP_MNEMONIC("dec Ev");
            return FNIEMOP_CALL_2(iemOpCommonUnaryEv, bRm, &g_iemAImpl_dec);
        case 2:
            return FNIEMOP_CALL_1(iemOp_Grp5_calln_Ev, bRm);
        case 3:
            return FNIEMOP_CALL_1(iemOp_Grp5_callf_Ep, bRm);
        case 4:
            return FNIEMOP_CALL_1(iemOp_Grp5_jmpn_Ev, bRm);
        case 5:
            return FNIEMOP_CALL_1(iemOp_Grp5_jmpf_Ep, bRm);
        case 6:
            return FNIEMOP_CALL_1(iemOp_Grp5_push_Ev, bRm);
        case 7:
            IEMOP_MNEMONIC("grp5-ud");
            return IEMOP_RAISE_INVALID_OPCODE();
    }
    AssertFailedReturn(VERR_IEM_IPE_3);
}



const PFNIEMOP g_apfnOneByteMap[256] =
{
    /* 0x00 */  iemOp_add_Eb_Gb,        iemOp_add_Ev_Gv,        iemOp_add_Gb_Eb,        iemOp_add_Gv_Ev,
    /* 0x04 */  iemOp_add_Al_Ib,        iemOp_add_eAX_Iz,       iemOp_push_ES,          iemOp_pop_ES,
    /* 0x08 */  iemOp_or_Eb_Gb,         iemOp_or_Ev_Gv,         iemOp_or_Gb_Eb,         iemOp_or_Gv_Ev,
    /* 0x0c */  iemOp_or_Al_Ib,         iemOp_or_eAX_Iz,        iemOp_push_CS,          iemOp_2byteEscape,
    /* 0x10 */  iemOp_adc_Eb_Gb,        iemOp_adc_Ev_Gv,        iemOp_adc_Gb_Eb,        iemOp_adc_Gv_Ev,
    /* 0x14 */  iemOp_adc_Al_Ib,        iemOp_adc_eAX_Iz,       iemOp_push_SS,          iemOp_pop_SS,
    /* 0x18 */  iemOp_sbb_Eb_Gb,        iemOp_sbb_Ev_Gv,        iemOp_sbb_Gb_Eb,        iemOp_sbb_Gv_Ev,
    /* 0x1c */  iemOp_sbb_Al_Ib,        iemOp_sbb_eAX_Iz,       iemOp_push_DS,          iemOp_pop_DS,
    /* 0x20 */  iemOp_and_Eb_Gb,        iemOp_and_Ev_Gv,        iemOp_and_Gb_Eb,        iemOp_and_Gv_Ev,
    /* 0x24 */  iemOp_and_Al_Ib,        iemOp_and_eAX_Iz,       iemOp_seg_ES,           iemOp_daa,
    /* 0x28 */  iemOp_sub_Eb_Gb,        iemOp_sub_Ev_Gv,        iemOp_sub_Gb_Eb,        iemOp_sub_Gv_Ev,
    /* 0x2c */  iemOp_sub_Al_Ib,        iemOp_sub_eAX_Iz,       iemOp_seg_CS,           iemOp_das,
    /* 0x30 */  iemOp_xor_Eb_Gb,        iemOp_xor_Ev_Gv,        iemOp_xor_Gb_Eb,        iemOp_xor_Gv_Ev,
    /* 0x34 */  iemOp_xor_Al_Ib,        iemOp_xor_eAX_Iz,       iemOp_seg_SS,           iemOp_aaa,
    /* 0x38 */  iemOp_cmp_Eb_Gb,        iemOp_cmp_Ev_Gv,        iemOp_cmp_Gb_Eb,        iemOp_cmp_Gv_Ev,
    /* 0x3c */  iemOp_cmp_Al_Ib,        iemOp_cmp_eAX_Iz,       iemOp_seg_DS,           iemOp_aas,
    /* 0x40 */  iemOp_inc_eAX,          iemOp_inc_eCX,          iemOp_inc_eDX,          iemOp_inc_eBX,
    /* 0x44 */  iemOp_inc_eSP,          iemOp_inc_eBP,          iemOp_inc_eSI,          iemOp_inc_eDI,
    /* 0x48 */  iemOp_dec_eAX,          iemOp_dec_eCX,          iemOp_dec_eDX,          iemOp_dec_eBX,
    /* 0x4c */  iemOp_dec_eSP,          iemOp_dec_eBP,          iemOp_dec_eSI,          iemOp_dec_eDI,
    /* 0x50 */  iemOp_push_eAX,         iemOp_push_eCX,         iemOp_push_eDX,         iemOp_push_eBX,
    /* 0x54 */  iemOp_push_eSP,         iemOp_push_eBP,         iemOp_push_eSI,         iemOp_push_eDI,
    /* 0x58 */  iemOp_pop_eAX,          iemOp_pop_eCX,          iemOp_pop_eDX,          iemOp_pop_eBX,
    /* 0x5c */  iemOp_pop_eSP,          iemOp_pop_eBP,          iemOp_pop_eSI,          iemOp_pop_eDI,
    /* 0x60 */  iemOp_pusha,            iemOp_popa,             iemOp_bound_Gv_Ma_evex, iemOp_arpl_Ew_Gw_movsx_Gv_Ev,
    /* 0x64 */  iemOp_seg_FS,           iemOp_seg_GS,           iemOp_op_size,          iemOp_addr_size,
    /* 0x68 */  iemOp_push_Iz,          iemOp_imul_Gv_Ev_Iz,    iemOp_push_Ib,          iemOp_imul_Gv_Ev_Ib,
    /* 0x6c */  iemOp_insb_Yb_DX,       iemOp_inswd_Yv_DX,      iemOp_outsb_Yb_DX,      iemOp_outswd_Yv_DX,
    /* 0x70 */  iemOp_jo_Jb,            iemOp_jno_Jb,           iemOp_jc_Jb,            iemOp_jnc_Jb,
    /* 0x74 */  iemOp_je_Jb,            iemOp_jne_Jb,           iemOp_jbe_Jb,           iemOp_jnbe_Jb,
    /* 0x78 */  iemOp_js_Jb,            iemOp_jns_Jb,           iemOp_jp_Jb,            iemOp_jnp_Jb,
    /* 0x7c */  iemOp_jl_Jb,            iemOp_jnl_Jb,           iemOp_jle_Jb,           iemOp_jnle_Jb,
    /* 0x80 */  iemOp_Grp1_Eb_Ib_80,    iemOp_Grp1_Ev_Iz,       iemOp_Grp1_Eb_Ib_82,    iemOp_Grp1_Ev_Ib,
    /* 0x84 */  iemOp_test_Eb_Gb,       iemOp_test_Ev_Gv,       iemOp_xchg_Eb_Gb,       iemOp_xchg_Ev_Gv,
    /* 0x88 */  iemOp_mov_Eb_Gb,        iemOp_mov_Ev_Gv,        iemOp_mov_Gb_Eb,        iemOp_mov_Gv_Ev,
    /* 0x8c */  iemOp_mov_Ev_Sw,        iemOp_lea_Gv_M,         iemOp_mov_Sw_Ev,        iemOp_Grp1A,
    /* 0x90 */  iemOp_nop,              iemOp_xchg_eCX_eAX,     iemOp_xchg_eDX_eAX,     iemOp_xchg_eBX_eAX,
    /* 0x94 */  iemOp_xchg_eSP_eAX,     iemOp_xchg_eBP_eAX,     iemOp_xchg_eSI_eAX,     iemOp_xchg_eDI_eAX,
    /* 0x98 */  iemOp_cbw,              iemOp_cwd,              iemOp_call_Ap,          iemOp_wait,
    /* 0x9c */  iemOp_pushf_Fv,         iemOp_popf_Fv,          iemOp_sahf,             iemOp_lahf,
    /* 0xa0 */  iemOp_mov_Al_Ob,        iemOp_mov_rAX_Ov,       iemOp_mov_Ob_AL,        iemOp_mov_Ov_rAX,
    /* 0xa4 */  iemOp_movsb_Xb_Yb,      iemOp_movswd_Xv_Yv,     iemOp_cmpsb_Xb_Yb,      iemOp_cmpswd_Xv_Yv,
    /* 0xa8 */  iemOp_test_AL_Ib,       iemOp_test_eAX_Iz,      iemOp_stosb_Yb_AL,      iemOp_stoswd_Yv_eAX,
    /* 0xac */  iemOp_lodsb_AL_Xb,      iemOp_lodswd_eAX_Xv,    iemOp_scasb_AL_Xb,      iemOp_scaswd_eAX_Xv,
    /* 0xb0 */  iemOp_mov_AL_Ib,        iemOp_CL_Ib,            iemOp_DL_Ib,            iemOp_BL_Ib,
    /* 0xb4 */  iemOp_mov_AH_Ib,        iemOp_CH_Ib,            iemOp_DH_Ib,            iemOp_BH_Ib,
    /* 0xb8 */  iemOp_eAX_Iv,           iemOp_eCX_Iv,           iemOp_eDX_Iv,           iemOp_eBX_Iv,
    /* 0xbc */  iemOp_eSP_Iv,           iemOp_eBP_Iv,           iemOp_eSI_Iv,           iemOp_eDI_Iv,
    /* 0xc0 */  iemOp_Grp2_Eb_Ib,       iemOp_Grp2_Ev_Ib,       iemOp_retn_Iw,          iemOp_retn,
    /* 0xc4 */  iemOp_les_Gv_Mp_vex2,   iemOp_lds_Gv_Mp_vex3,   iemOp_Grp11_Eb_Ib,      iemOp_Grp11_Ev_Iz,
    /* 0xc8 */  iemOp_enter_Iw_Ib,      iemOp_leave,            iemOp_retf_Iw,          iemOp_retf,
    /* 0xcc */  iemOp_int_3,            iemOp_int_Ib,           iemOp_into,             iemOp_iret,
    /* 0xd0 */  iemOp_Grp2_Eb_1,        iemOp_Grp2_Ev_1,        iemOp_Grp2_Eb_CL,       iemOp_Grp2_Ev_CL,
    /* 0xd4 */  iemOp_aam_Ib,           iemOp_aad_Ib,           iemOp_salc,             iemOp_xlat,
    /* 0xd8 */  iemOp_EscF0,            iemOp_EscF1,            iemOp_EscF2,            iemOp_EscF3,
    /* 0xdc */  iemOp_EscF4,            iemOp_EscF5,            iemOp_EscF6,            iemOp_EscF7,
    /* 0xe0 */  iemOp_loopne_Jb,        iemOp_loope_Jb,         iemOp_loop_Jb,          iemOp_jecxz_Jb,
    /* 0xe4 */  iemOp_in_AL_Ib,         iemOp_in_eAX_Ib,        iemOp_out_Ib_AL,        iemOp_out_Ib_eAX,
    /* 0xe8 */  iemOp_call_Jv,          iemOp_jmp_Jv,           iemOp_jmp_Ap,           iemOp_jmp_Jb,
    /* 0xec */  iemOp_in_AL_DX,         iemOp_eAX_DX,           iemOp_out_DX_AL,        iemOp_out_DX_eAX,
    /* 0xf0 */  iemOp_lock,             iemOp_int_1,            iemOp_repne,            iemOp_repe,
    /* 0xf4 */  iemOp_hlt,              iemOp_cmc,              iemOp_Grp3_Eb,          iemOp_Grp3_Ev,
    /* 0xf8 */  iemOp_clc,              iemOp_stc,              iemOp_cli,              iemOp_sti,
    /* 0xfc */  iemOp_cld,              iemOp_std,              iemOp_Grp4,             iemOp_Grp5,
};


/** @} */

#ifdef _MSC_VER
# pragma warning(pop)
#endif
